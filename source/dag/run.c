#include "dag/dag.h"
#include "dag/types.h"
#include "sha256/sha256.h"
#include "error/types.h"
#include "sp.h"
#include "sp/fs.h"
#include "sp/sp_glob.h"

#define try(expr) spn_try(expr)

static bool is_timespec_equal(sp_sys_timespec_t a, sp_sys_timespec_t b) {
  return a.tv_sec == b.tv_sec && a.tv_nsec == b.tv_nsec;
}

static bool is_path_within(sp_str_t path, sp_str_t dir) {
  if (path.len <= dir.len + 1 || !sp_str_starts_with(path, dir)) {
    return false;
  }
  return path.data[dir.len] == '/';
}

static sp_str_t parent_dir(sp_str_t path) {
  s32 index = sp_str_find_c8_reverse(path, '/');
  return index == SP_STR_NO_MATCH ? sp_str_lit("") : sp_str_prefix(path, index);
}

static spn_dag_file_meta_t file_meta_from_sys(sp_sys_file_meta_t sys) {
  return (spn_dag_file_meta_t) {
    .id = {
      .device = sys.device,
      .inode = sys.id
    },
    .mtime = sys.mtime,
    .size = sys.size,
  };
}

static bool file_meta_current(spn_dag_file_meta_t meta, sp_sys_file_meta_t sys) {
  if (meta.id.device != sys.device) return false;
  if (meta.id.inode != sys.id) return false;
  if (!is_timespec_equal(meta.mtime, sys.mtime)) return false;
  return meta.size == sys.size;
}

static bool file_meta_equal(spn_dag_file_meta_t a, spn_dag_file_meta_t b) {
  return a.id.device == b.id.device
    && a.id.inode == b.id.inode
    && is_timespec_equal(a.mtime, b.mtime)
    && a.size == b.size
    && spn_dag_digest_equal(a.digest, b.digest);
}

void spn_dag_file_cache_init(spn_dag_file_cache_t* c, sp_mem_t mem) {
  c->arena = sp_mem_arena_new(mem);
  c->mem = sp_mem_arena_as_allocator(c->arena);
  sp_ht_init(c->mem, c->entries);
  sp_str_ht_init(c->mem, c->metadata);
}

void spn_dag_file_cache_seed(spn_dag_file_cache_t* c, spn_dag_file_meta_t meta) {
  sp_ht_insert(c->entries, meta.id, meta);
}

void spn_dag_file_cache_invalidate(spn_dag_file_cache_t* c, sp_str_t path) {
  sp_ht_erase(c->metadata, path);
}

void spn_dag_file_cache_invalidate_dir(spn_dag_file_cache_t* c, sp_str_t dir) {
  sp_mem_arena_marker_t s = sp_mem_begin_scratch();

  sp_da(sp_str_t) stale = sp_da_new(s.mem, sp_str_t);
  sp_ht_for_kv(c->metadata, it) {
    if (sp_str_equal(*it.key, dir) || is_path_within(*it.key, dir)) {
      sp_da_push(stale, *it.key);
    }
  }
  sp_da_for(stale, it) {
    sp_ht_erase(c->metadata, stale[it]);
  }

  sp_mem_end_scratch(s);
}

void spn_dag_file_cache_invalidate_all(spn_dag_file_cache_t* c) {
  sp_ht_clear(c->metadata);
}

spn_err_t spn_dag_file_cache_stat(spn_dag_file_cache_t* c, sp_str_t path, sp_sys_file_meta_t* meta) {
  sp_sys_file_meta_t* cached = sp_ht_getp(c->metadata, path);
  if (cached) {
    *meta = *cached;
    return SPN_OK;
  }

  sp_sys_file_meta_t sys = sp_zero;
  spn_try_as(sp_sys_get_path_metadata_s(sp_sys_get_root(0), path, &sys), SPN_ERR_DAG_STAT);

  sp_ht_insert(c->metadata, sp_str_copy(c->mem, path), sys);
  *meta = sys;
  return SPN_OK;
}

spn_err_t spn_dag_file_cache_digest(spn_dag_file_cache_t* c, sp_str_t path, spn_dag_digest_t* digest) {
  sp_sys_file_meta_t sys = sp_zero;
  try(spn_dag_file_cache_stat(c, path, &sys));

  spn_dag_file_meta_t fresh = file_meta_from_sys(sys);
  spn_dag_file_meta_t* cached = sp_ht_getp(c->entries, fresh.id);
  if (cached && file_meta_current(*cached, sys)) {
    *digest = cached->digest;
    return SPN_OK;
  }

  try(spn_sha256_file_digest(path, digest->bytes));

  fresh.digest = *digest;
  sp_ht_insert(c->entries, fresh.id, fresh);
  return SPN_OK;
}

static void trace_emit(spn_dag_env_t* env, spn_dag_trace_event_t event) {
  if (env->trace) {
    env->trace(&event, env->trace_data);
  }
}

static void trace_resolve(spn_dag_env_t* env, spn_dag_id_t action, bool hit, bool changed) {
  trace_emit(env, (spn_dag_trace_event_t) {
    .kind = SPN_DAG_TRACE_RESOLVE,
    .action = action,
    .hit = hit,
    .changed = changed
  });
}

static void progress_total(spn_dag_env_t* env, u64 total) {
  if (env->progress) {
    sp_atomic_s32_set(&env->progress->total, (s32)total);
  }
}

static void progress_count(spn_dag_env_t* env, bool hit) {
  if (!env->progress) {
    return;
  }
  sp_atomic_s32_add(hit ? &env->progress->hits : &env->progress->misses, 1);
  sp_atomic_s32_add(&env->progress->completed, 1);
}

static bool is_file_settled(spn_dag_file_cache_t* files, sp_str_t path, spn_dag_digest_t digest) {
  spn_dag_digest_t existing = sp_zero;
  if (spn_dag_file_cache_digest(files, path, &existing)) {
    return false;
  }
  return spn_dag_digest_equal(existing, digest);
}

static bool is_tree_settled(spn_dag_artifact_t* artifact, spn_dag_env_t* env) {
  sp_mem_arena_marker_t s = sp_mem_begin_scratch();
  bool settled = false;

  if (!sp_fs_is_dir(artifact->target)) {
    goto done;
  }

  sp_da(spn_dag_action_output_t) entries = sp_zero;
  if (spn_dag_tree_entries(env->store, artifact->digest, s.mem, &entries)) {
    goto done;
  }

  u64 present = 0;
  sp_da(sp_fs_entry_t) files = sp_fs_collect_recursive(s.mem, artifact->target);
  sp_da_for(files, it) {
    if (files[it].kind != SP_FS_KIND_DIR) {
      present++;
    }
  }
  if (present != sp_da_size(entries)) {
    goto done;
  }

  sp_da_for(entries, it) {
    sp_str_t path = sp_fs_join_path(s.mem, artifact->target, entries[it].name);
    if (!is_file_settled(env->files, path, entries[it].digest)) {
      goto done;
    }
  }
  settled = true;

done:
  sp_mem_end_scratch(s);
  return settled;
}

static spn_err_t settle_tree(spn_dag_action_t* action, spn_dag_artifact_t* artifact, spn_dag_env_t* env) {
  bool settled = is_tree_settled(artifact, env);
  trace_emit(env, (spn_dag_trace_event_t) { .kind = SPN_DAG_TRACE_SETTLE, .action = action->id, .producer = artifact->id, .key = artifact->digest, .hit = settled });
  if (settled) {
    return SPN_OK;
  }
  action->wrote = true;
  spn_dag_file_cache_invalidate_dir(env->files, artifact->target);
  return spn_dag_store_materialize_tree(env->store, artifact->digest, artifact->target);
}

static spn_err_t settle(spn_dag_t* g, spn_dag_action_t* action, spn_dag_env_t* env) {
  action->wrote = false;
  sp_da_for(action->produces, it) {
    spn_dag_artifact_t* artifact = spn_dag_find_artifact(g, action->produces[it]);
    if (artifact->kind == SPN_DAG_ARTIFACT_KIND_TREE) {
      try(settle_tree(action, artifact, env));
      artifact->path = artifact->target;
      continue;
    }
    if (sp_str_empty(artifact->target)) {
      artifact->path = spn_dag_store_path(env->store, g->mem, artifact->digest, artifact->name);
      continue;
    }
    bool settled = is_file_settled(env->files, artifact->target, artifact->digest);
    trace_emit(env, (spn_dag_trace_event_t) { .kind = SPN_DAG_TRACE_SETTLE, .action = action->id, .producer = artifact->id, .key = artifact->digest, .hit = settled });
    if (!settled) {
      action->wrote = true;
      try(spn_dag_store_materialize(env->store, artifact->digest, artifact->name, artifact->target));
      spn_dag_file_cache_invalidate(env->files, artifact->target);
    }
    artifact->path = artifact->target;
  }
  return SPN_OK;
}

static bool restore_entry(spn_dag_t* g, spn_dag_action_t* action, const spn_dag_action_entry_t* entry, spn_dag_env_t* env) {
  if (sp_da_size(entry->outputs) != sp_da_size(action->produces)) {
    return false;
  }

  sp_da_for(entry->outputs, it) {
    spn_dag_artifact_t* artifact = spn_dag_find_artifact(g, action->produces[it]);
    if (!sp_str_equal(entry->outputs[it].name, artifact->name)) {
      return false;
    }
    bool present = artifact->kind == SPN_DAG_ARTIFACT_KIND_TREE
      ? spn_dag_store_has_tree(env->store, entry->outputs[it].digest)
      : spn_dag_store_has(env->store, entry->outputs[it].digest, entry->outputs[it].name);
    if (!present) {
      return false;
    }
  }

  sp_da_for(entry->outputs, it) {
    spn_dag_find_artifact(g, action->produces[it])->digest = entry->outputs[it].digest;
  }

  return !settle(g, action, env);
}

static bool try_restore(spn_dag_t* g, spn_dag_action_t* action, spn_dag_digest_t key, spn_dag_env_t* env) {
  const spn_dag_action_entry_t* entry = spn_dag_action_cache_get(env->cache, key);
  bool hit = entry && restore_entry(g, action, entry, env);
  trace_emit(env, (spn_dag_trace_event_t) {
    .kind = SPN_DAG_TRACE_CACHE,
    .action = action->id,
    .key = key,
    .present = entry != SP_NULLPTR,
    .hit = hit,
  });
  if (entry && !hit) {
    spn_dag_action_cache_remove(env->cache, key);
  }
  return hit;
}

static s32 obs_order(const void* a, const void* b) {
  const spn_dag_obs_t* oa = (const spn_dag_obs_t*)a;
  const spn_dag_obs_t* ob = (const spn_dag_obs_t*)b;
  s32 order = sp_str_compare_alphabetical(oa->path, ob->path);
  if (order) {
    return order;
  }
  if (oa->kind != ob->kind) {
    return (s32)oa->kind - (s32)ob->kind;
  }
  return sp_str_compare_alphabetical(oa->filter, ob->filter);
}

static bool obs_equal(const spn_dag_obs_t* a, const spn_dag_obs_t* b) {
  return a->kind == b->kind && sp_str_equal(a->path, b->path) && sp_str_equal(a->filter, b->filter);
}

static void canonicalize_observations(sp_da(spn_dag_obs_t) obs) {
  if (sp_da_empty(obs)) {
    return;
  }
  sp_da_sort(obs, obs_order);
  u64 w = 1;
  for (u64 r = 1; r < sp_da_size(obs); r++) {
    if (!obs_equal(&obs[r], &obs[w - 1])) {
      obs[w++] = obs[r];
    }
  }
  sp_da_head(obs)->size = w;
}

static s32 member_order(const void* a, const void* b) {
  return sp_str_compare_alphabetical(((const sp_fs_entry_t*)a)->name, ((const sp_fs_entry_t*)b)->name);
}

static spn_err_t membership_digest(sp_str_t dir, sp_str_t filter, spn_dag_digest_t* digest) {
  sp_mem_arena_marker_t s = sp_mem_begin_scratch();
  spn_err_t err = SPN_OK;

  sp_glob_t* glob = SP_NULLPTR;
  if (!sp_str_empty(filter)) {
    glob = sp_glob_new_str(s.mem, filter);
    if (!glob) {
      err = SPN_ERR_DAG_GLOB;
      goto done;
    }
  }

  sp_da(sp_fs_entry_t) members = sp_da_new(s.mem, sp_fs_entry_t);
  sp_da(sp_fs_entry_t) entries = sp_fs_collect(s.mem, dir);
  sp_da_for(entries, it) {
    if (entries[it].kind != SP_FS_KIND_DIR && glob && !sp_glob_match(glob, entries[it].name)) {
      continue;
    }
    sp_da_push(members, entries[it]);
  }
  sp_da_sort(members, member_order);

  spn_sha256_ctx_t ctx = sp_zero;
  spn_sha256_init(&ctx);
  spn_dag_hash_str(&ctx, sp_str_lit("spn.dag.enum.v1"));
  spn_dag_hash_u64(&ctx, sp_da_size(members));
  sp_da_for(members, it) {
    spn_dag_hash_str(&ctx, members[it].name);
    spn_dag_hash_u8(&ctx, (u8)members[it].kind);
  }
  *digest = spn_dag_hash_final(&ctx);

done:
  sp_mem_end_scratch(s);
  return err;
}

static spn_err_t resolve_one(spn_dag_file_cache_t* files, spn_dag_obs_t* o) {
  switch (o->kind) {
    case SPN_DAG_OBS_ENUMERATION: {
      o->meta = (spn_dag_file_meta_t) sp_zero;
      return membership_digest(o->path, o->filter, &o->meta.digest);
    }
    case SPN_DAG_OBS_ABSENT: {
      if (!sp_fs_exists(o->path)) {
        o->meta = (spn_dag_file_meta_t) sp_zero;
        return SPN_OK;
      }
      break;
    }
    case SPN_DAG_OBS_FILE: {
      break;
    }
  }

  sp_sys_file_meta_t sys = sp_zero;
  try(spn_dag_file_cache_stat(files, o->path, &sys));

  if (sys.kind == SP_FS_KIND_DIR) {
    o->meta = (spn_dag_file_meta_t) sp_zero;
    return membership_digest(o->path, sp_str_lit(""), &o->meta.digest);
  }

  if (file_meta_current(o->meta, sys)) {
    if (spn_dag_digest_valid(o->meta.digest)) {
      spn_dag_file_cache_seed(files, o->meta);
      return SPN_OK;
    }
  }

  spn_dag_file_meta_t fresh = file_meta_from_sys(sys);
  try(spn_dag_file_cache_digest(files, o->path, &fresh.digest));
  o->meta = fresh;
  return SPN_OK;
}

static spn_err_t resolve_observations(spn_dag_file_cache_t* files, spn_dag_obs_t* obs, u32 count, bool* changed) {
  *changed = false;
  sp_for(it, count) {
    spn_dag_file_meta_t before = obs[it].meta;
    try(resolve_one(files, &obs[it]));
    if (!file_meta_equal(before, obs[it].meta)) {
      *changed = true;
    }
  }
  return SPN_OK;
}

static void record(spn_dag_t* g, spn_dag_action_t* action, spn_dag_digest_t key, spn_dag_env_t* env) {
  sp_mem_arena_marker_t s = sp_mem_begin_scratch();

  sp_da(spn_dag_action_output_t) outputs = sp_da_new(s.mem, spn_dag_action_output_t);
  sp_da_for(action->produces, it) {
    spn_dag_artifact_t* artifact = spn_dag_find_artifact(g, action->produces[it]);
    sp_da_push(outputs, ((spn_dag_action_output_t) {
      .name = artifact->name,
      .digest = artifact->digest
    }));
  }
  spn_dag_action_cache_put(env->cache, key, outputs, (u32)sp_da_size(outputs));

  sp_mem_end_scratch(s);
}

static sp_str_t begin_scratch(spn_dag_t* g, spn_dag_action_t* action, sp_str_t root) {
  sp_assert(!sp_str_empty(root));

  sp_str_t dir = sp_zero;
  if (sp_fs_staging_dir(g->mem, sp_fs_join_path(g->mem, root, sp_str_lit("scratch")), sp_str_lit("tmp"), &dir)) {
    return sp_str_lit("");
  }

  sp_da_for(action->produces, it) {
    spn_dag_artifact_t* artifact = spn_dag_find_artifact(g, action->produces[it]);
    artifact->path = sp_fs_join_path(g->mem, dir, artifact->name);
    if (artifact->kind == SPN_DAG_ARTIFACT_KIND_TREE) {
      sp_fs_create_dir(artifact->path);
    }
  }
  return dir;
}

static void end_scratch(sp_str_t dir) {
  if (!sp_str_empty(dir)) {
    sp_fs_remove_dir(dir);
  }
}

typedef struct {
  spn_dag_action_t* action;
  sp_mem_t mem;
  spn_dag_digest_t key;
  sp_str_t scratch;
  bool hit;
  sp_da(spn_dag_digest_t) digests;
  sp_da(spn_dag_obs_t) obs;
} spn_dag_attempt_t;

static spn_err_t lookup(spn_dag_t* g, spn_dag_action_t* action, spn_dag_env_t* env, sp_mem_t mem, spn_dag_attempt_t* attempt) {
  attempt->action = action;
  attempt->mem = mem;
  attempt->key = spn_dag_weak_key(g, action->id);
  sp_da_init(mem, attempt->digests);
  sp_da_init(mem, attempt->obs);
  trace_emit(env, (spn_dag_trace_event_t) { .kind = SPN_DAG_TRACE_KEY, .action = action->id, .key = attempt->key });

  if (action->discover) {
    spn_dag_pathset_t* set = spn_dag_discovery_get(env->discovery, attempt->key);
    trace_emit(env, (spn_dag_trace_event_t) { .kind = SPN_DAG_TRACE_DISCOVERY, .action = action->id, .key = attempt->key, .hit = set != SP_NULLPTR });
    if (set) {
      u32 count = (u32)sp_da_size(set->obs);
      bool changed = false;
      bool resolved = !resolve_observations(env->files, set->obs, count, &changed);
      trace_resolve(env, action->id, resolved, changed);
      if (resolved) {
        spn_dag_digest_t strong = spn_dag_strong_key(attempt->key, set->obs, count);
        trace_emit(env, (spn_dag_trace_event_t) { .kind = SPN_DAG_TRACE_STRONG, .action = action->id, .key = strong });
        if (try_restore(g, action, strong, env)) {
          if (changed) {
            spn_dag_discovery_flush(env->discovery, attempt->key);
          }
          attempt->hit = true;
          return SPN_OK;
        }
      }
    }
  } else if (try_restore(g, action, attempt->key, env)) {
    attempt->hit = true;
    return SPN_OK;
  }

  attempt->scratch = begin_scratch(g, action, env->scratch);
  return sp_str_empty(attempt->scratch) ? SPN_ERR_DAG_SCRATCH : SPN_OK;
}

static spn_err_t execute(spn_dag_t* g, spn_dag_attempt_t* attempt, spn_dag_env_t* env) {
  spn_dag_action_t* action = attempt->action;
  trace_emit(env, (spn_dag_trace_event_t) { .kind = SPN_DAG_TRACE_EXECUTE, .action = action->id, .key = attempt->key });

  if (action->execute) {
    if (action->execute(action, action->user_data)) {
      return SPN_ERR_DAG_ACTION;
    }
  }
  if (action->discover) {
    try(action->discover(action, action->user_data, attempt->mem, &attempt->obs));
    canonicalize_observations(attempt->obs);
  }

  sp_da_for(action->produces, it) {
    spn_dag_artifact_t* artifact = spn_dag_find_artifact(g, action->produces[it]);
    if (!sp_fs_exists(artifact->path)) {
      return SPN_ERR_DAG_MISSING_OUTPUT;
    }
    spn_dag_digest_t digest = sp_zero;
    spn_err_t put = artifact->kind == SPN_DAG_ARTIFACT_KIND_TREE
      ? spn_dag_store_put_tree(env->store, artifact->path, &digest)
      : spn_dag_store_put_file(env->store, artifact->path, artifact->name, &digest);
    try(put);
    sp_da_push(attempt->digests, digest);
  }

  return SPN_OK;
}

static spn_err_t commit(spn_dag_t* g, spn_dag_attempt_t* attempt, spn_dag_env_t* env) {
  spn_dag_action_t* action = attempt->action;
  sp_assert(!attempt->hit);
  sp_assert(sp_da_size(attempt->digests) == sp_da_size(action->produces));

  sp_da_for(action->produces, it) {
    spn_dag_find_artifact(g, action->produces[it])->digest = attempt->digests[it];
  }

  spn_dag_digest_t key = attempt->key;
  bool resolved = true;
  if (action->discover) {
    u32 count = (u32)sp_da_size(attempt->obs);
    bool changed = false;
    resolved = !resolve_observations(env->files, attempt->obs, count, &changed);
    trace_resolve(env, action->id, resolved, changed);
    spn_dag_discovery_put(env->discovery, attempt->key, attempt->obs, count);
    if (resolved) {
      key = spn_dag_strong_key(attempt->key, attempt->obs, count);
      trace_emit(env, (spn_dag_trace_event_t) { .kind = SPN_DAG_TRACE_STRONG, .action = action->id, .key = key });
    }
  }

  try(settle(g, action, env));
  if (resolved) {
    record(g, action, key, env);
  }
  trace_emit(env, (spn_dag_trace_event_t) { .kind = SPN_DAG_TRACE_COMMIT, .action = action->id, .key = key, .hit = resolved });
  return SPN_OK;
}

static spn_err_t exec_action(spn_dag_t* g, spn_dag_action_t* action, spn_dag_env_t* env) {
  sp_mem_arena_marker_t s = sp_mem_begin_scratch();

  spn_dag_attempt_t attempt = sp_zero;
  spn_err_t err = lookup(g, action, env, s.mem, &attempt);
  if (!err && !attempt.hit) {
    err = execute(g, &attempt, env);
    if (!err) {
      err = commit(g, &attempt, env);
    }
    end_scratch(attempt.scratch);
  }
  if (!err) {
    progress_count(env, attempt.hit);
  }

  sp_mem_end_scratch(s);
  return err;
}

spn_err_t spn_dag_execute(spn_dag_t* g, spn_dag_id_t action_id, spn_dag_env_t* env) {
  spn_dag_action_t* action = spn_dag_find_action(g, action_id);
  sp_assert(!action->discover);
  return exec_action(g, action, env);
}

spn_err_t spn_dag_execute_discovered(spn_dag_t* g, spn_dag_id_t action_id, spn_dag_env_t* env) {
  spn_dag_action_t* action = spn_dag_find_action(g, action_id);
  sp_assert(action->discover);
  sp_assert(env->discovery);
  return exec_action(g, action, env);
}

typedef struct {
  u32 producer;
  spn_dag_artifact_kind_t kind;
} spn_dag_target_t;

typedef struct {
  sp_ht(sp_str_t, spn_dag_target_t) by_path;
  sp_ht(sp_str_t, sp_da(u32)) below;
} spn_dag_targets_t;

static void targets_init(spn_dag_targets_t* targets, spn_dag_t* g, sp_mem_t mem) {
  sp_str_ht_init(mem, targets->by_path);
  sp_str_ht_init(mem, targets->below);

  sp_da_for(g->artifacts, it) {
    spn_dag_artifact_t* artifact = &g->artifacts[it];
    if (!artifact->producer.occupied || sp_str_empty(artifact->target)) {
      continue;
    }

    sp_assert(!sp_ht_getp(targets->by_path, artifact->target));
    sp_ht_insert(targets->by_path, artifact->target, ((spn_dag_target_t) {
      .producer = artifact->producer.index,
      .kind = artifact->kind
    }));

    for (sp_str_t dir = parent_dir(artifact->target); !sp_str_empty(dir); dir = parent_dir(dir)) {
      sp_da(u32)* below = sp_ht_getp(targets->below, dir);
      if (!below) {
        sp_ht_insert(targets->below, dir, sp_da_new(mem, u32));
        below = sp_ht_getp(targets->below, dir);
      }
      sp_da_push(*below, artifact->producer.index);
    }
  }
}

typedef struct spn_dag_flight_t spn_dag_flight_t;

typedef struct {
  u32 pending;
  u32 deferred;
  bool done;
  u64 done_epoch;
  sp_da(u32) waiters;
  spn_dag_flight_t* parked;
} spn_dag_run_state_t;

struct spn_dag_flight_t {
  spn_dag_t* g;
  spn_dag_env_t* env;
  sp_mem_arena_t* arena;
  u64 epoch;
  spn_err_t err;
  spn_dag_attempt_t attempt;
};

typedef struct {
  spn_dag_t* g;
  spn_dag_env_t* env;
  spn_dag_executor_t* ex;
  spn_dag_targets_t targets;
  spn_dag_run_state_t* states;
  sp_da(spn_dag_id_t) ready;
  u64 completed;
  u32 in_flight;
  spn_err_t err;
} spn_dag_run_t;

static void defer_producer(spn_dag_run_t* run, spn_dag_action_t* action, u32 producer_index, u64 epoch, bool* requeue) {
  if (producer_index == action->id.index) {
    return;
  }
  spn_dag_run_state_t* producer = &run->states[producer_index];
  if (producer->done) {
    if (producer->done_epoch > epoch && run->g->actions[producer_index].wrote) {
      trace_emit(run->env, (spn_dag_trace_event_t) { .kind = SPN_DAG_TRACE_REQUEUE, .action = action->id, .producer = run->g->actions[producer_index].id });
      *requeue = true;
    }
    return;
  }
  trace_emit(run->env, (spn_dag_trace_event_t) { .kind = SPN_DAG_TRACE_DEFER, .action = action->id, .producer = run->g->actions[producer_index].id });
  sp_da_push(producer->waiters, action->id.index);
  run->states[action->id.index].deferred++;
}

static bool defer_observations(spn_dag_run_t* run, spn_dag_action_t* action, const spn_dag_obs_t* obs, u64 count, u64 epoch, bool* requeue) {
  sp_for(it, count) {
    const spn_dag_obs_t* o = &obs[it];

    spn_dag_target_t* exact = sp_ht_getp(run->targets.by_path, o->path);
    if (exact) {
      defer_producer(run, action, exact->producer, epoch, requeue);
    }

    for (sp_str_t dir = parent_dir(o->path); !sp_str_empty(dir); dir = parent_dir(dir)) {
      spn_dag_target_t* tree = sp_ht_getp(run->targets.by_path, dir);
      if (tree && tree->kind == SPN_DAG_ARTIFACT_KIND_TREE) {
        defer_producer(run, action, tree->producer, epoch, requeue);
      }
    }

    if (o->kind == SPN_DAG_OBS_ENUMERATION) {
      sp_da(u32)* below = sp_ht_getp(run->targets.below, o->path);
      if (below) {
        sp_da_for(*below, bi) {
          defer_producer(run, action, (*below)[bi], epoch, requeue);
        }
      }
    }
  }

  return run->states[action->id.index].deferred > 0;
}

static bool defer_pathset(spn_dag_run_t* run, spn_dag_action_t* action, spn_dag_digest_t weak, u64 epoch, bool* requeue) {
  const spn_dag_pathset_t* set = spn_dag_discovery_get(run->env->discovery, weak);
  if (!set) {
    return false;
  }
  return defer_observations(run, action, set->obs, sp_da_size(set->obs), epoch, requeue);
}

static spn_err_t seed_sources(spn_dag_t* g, spn_dag_env_t* env) {
  sp_da_for(g->artifacts, it) {
    spn_dag_artifact_t* artifact = &g->artifacts[it];
    switch (artifact->kind) {
      case SPN_DAG_ARTIFACT_KIND_TREE: {
        sp_assert(artifact->producer.occupied);
        break;
      }
      case SPN_DAG_ARTIFACT_KIND_FILE: {
        if (!artifact->producer.occupied) {
          if (spn_dag_file_cache_digest(env->files, artifact->path, &artifact->digest)) {
            return SPN_ERR_DAG_MISSING_INPUT;
          }
        }
        break;
      }
      case SPN_DAG_ARTIFACT_KIND_VALUE: {
        break;
      }
    }
  }
  return SPN_OK;
}

static void seed_ready(spn_dag_run_t* run, sp_mem_t mem) {
  sp_da_for(run->g->actions, ai) {
    spn_dag_action_t* action = &run->g->actions[ai];
    sp_da_init(mem, run->states[ai].waiters);
    sp_da_for(action->consumes, ci) {
      if (!spn_dag_digest_valid(spn_dag_find_artifact(run->g, action->consumes[ci])->digest)) {
        run->states[ai].pending++;
      }
    }
    if (!run->states[ai].pending) {
      sp_da_push(run->ready, action->id);
    }
  }
}

static void finish_action(spn_dag_run_t* run, spn_dag_action_t* action) {
  spn_dag_run_state_t* state = &run->states[action->id.index];
  state->done = true;
  state->done_epoch = ++run->completed;

  sp_da_for(action->produces, pi) {
    spn_dag_artifact_t* produced = spn_dag_find_artifact(run->g, action->produces[pi]);
    sp_da_for(produced->consumers, cj) {
      spn_dag_run_state_t* consumer = &run->states[produced->consumers[cj].index];
      if (consumer->pending) {
        consumer->pending--;
        if (!consumer->pending) {
          sp_da_push(run->ready, produced->consumers[cj]);
        }
      }
    }
  }

  sp_da_for(state->waiters, wi) {
    u32 index = state->waiters[wi];
    spn_dag_run_state_t* waiter = &run->states[index];
    if (waiter->deferred) {
      waiter->deferred--;
      if (!waiter->deferred) {
        sp_da_push(run->ready, run->g->actions[index].id);
      }
    }
  }
}

static void flight_run(void* data) {
  spn_dag_flight_t* flight = (spn_dag_flight_t*)data;
  flight->err = execute(flight->g, &flight->attempt, flight->env);
}

static void flight_free(spn_dag_flight_t* flight) {
  end_scratch(flight->attempt.scratch);
  sp_mem_arena_destroy(flight->arena);
}

static void run_commit_flight(spn_dag_run_t* run, spn_dag_action_t* action, spn_dag_flight_t* flight) {
  run->err = commit(run->g, &flight->attempt, run->env);
  flight_free(flight);
  if (run->err) {
    return;
  }
  progress_count(run->env, false);
  finish_action(run, action);
}

static void run_dispatch(spn_dag_run_t* run, spn_dag_id_t id) {
  spn_dag_action_t* action = spn_dag_find_action(run->g, id);
  spn_dag_run_state_t* state = &run->states[id.index];

  if (state->parked) {
    spn_dag_flight_t* flight = state->parked;
    bool requeue = false;
    if (defer_observations(run, action, flight->attempt.obs, sp_da_size(flight->attempt.obs), flight->epoch, &requeue)) {
      return;
    }
    state->parked = SP_NULLPTR;
    if (!requeue) {
      run_commit_flight(run, action, flight);
      return;
    }
    flight_free(flight);
  }

  if (action->discover) {
    sp_assert(run->env->discovery);
    bool requeue = false;
    if (defer_pathset(run, action, spn_dag_weak_key(run->g, action->id), run->completed, &requeue)) {
      return;
    }
    sp_assert(!requeue);
  }

  sp_mem_arena_t* arena = sp_mem_arena_new(sp_mem_os_new());
  sp_mem_t mem = sp_mem_arena_as_allocator(arena);
  spn_dag_flight_t* flight = sp_mem_allocator_alloc_type(mem, spn_dag_flight_t);
  *flight = (spn_dag_flight_t) {
    .g = run->g,
    .env = run->env,
    .arena = arena,
    .epoch = run->completed,
  };

  run->err = lookup(run->g, action, run->env, mem, &flight->attempt);
  if (run->err) {
    flight_free(flight);
    return;
  }
  if (flight->attempt.hit) {
    flight_free(flight);
    progress_count(run->env, true);
    finish_action(run, action);
    return;
  }

  run->ex->submit(run->ex, (spn_dag_job_t) { .fn = flight_run, .data = flight });
  run->in_flight++;
}

static void run_complete(spn_dag_run_t* run, spn_dag_flight_t* flight) {
  spn_dag_action_t* action = flight->attempt.action;

  if (run->err || flight->err) {
    run->err = run->err ? run->err : flight->err;
    flight_free(flight);
    return;
  }

  if (action->discover) {
    bool requeue = false;
    if (defer_observations(run, action, flight->attempt.obs, sp_da_size(flight->attempt.obs), flight->epoch, &requeue)) {
      run->states[action->id.index].parked = flight;
      return;
    }
    if (requeue) {
      flight_free(flight);
      sp_da_push(run->ready, action->id);
      return;
    }
  }

  run_commit_flight(run, action, flight);
}

spn_err_t spn_dag_run_executor(spn_dag_t* g, spn_dag_env_t* env, spn_dag_executor_t* ex) {
  sp_mem_arena_marker_t s = sp_mem_begin_scratch();

  spn_dag_run_t run = {
    .g = g,
    .env = env,
    .ex = ex,
    .err = seed_sources(g, env),
  };

  u64 n = sp_da_size(g->actions);
  sp_assert(n < (1u << 30));
  if (!run.err) {
    progress_total(env, n);
    run.states = sp_alloc_n(s.mem, spn_dag_run_state_t, n ? n : 1);
    run.ready = sp_da_new(s.mem, spn_dag_id_t);
    seed_ready(&run, s.mem);
    targets_init(&run.targets, g, s.mem);

    u64 turns = 0;
    u64 turns_max = 4 * (n + 1) * (n + 1);
    while (true) {
      turns++;
      sp_assert(turns <= turns_max);

      spn_dag_job_t job = ex->try_poll(ex);
      if (job.fn) {
        run.in_flight--;
        run_complete(&run, (spn_dag_flight_t*)job.data);
        continue;
      }
      if (!run.err && !sp_da_empty(run.ready)) {
        spn_dag_id_t id = *sp_da_back(run.ready);
        sp_da_pop(run.ready);
        run_dispatch(&run, id);
        continue;
      }
      if (run.in_flight) {
        job = ex->poll(ex);
        run.in_flight--;
        run_complete(&run, (spn_dag_flight_t*)job.data);
        continue;
      }
      break;
    }

    sp_for(it, n) {
      if (run.states[it].parked) {
        flight_free(run.states[it].parked);
      }
    }
    if (!run.err && run.completed != n) {
      run.err = SPN_ERR_DAG_STALLED;
    }
  }

  sp_mem_end_scratch(s);
  return run.err;
}
