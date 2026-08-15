#include "dag/dag.h"
#include "dag/types.h"
#include "core/core.h"
#include "paths/paths.h"
#include "thread_pool/thread_pool.h"
#include "sha256/sha256.h"
#include "sp.h"
#include "spn/core.h"
#include "sp/fs.h"
#include "sp/sp_glob.h"


static bool is_timespec_equal(sp_sys_timespec_t a, sp_sys_timespec_t b) {
  return a.tv_sec == b.tv_sec && a.tv_nsec == b.tv_nsec;
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
  if (meta.id.device && meta.id.device != sys.device) return false;
  if (meta.id.inode != sys.id) return false;
  if (!is_timespec_equal(meta.mtime, sys.mtime)) return false;
  return meta.size == sys.size;
}

void spn_dag_file_cache_init(spn_dag_file_cache_t* c, sp_mem_t mem, const spn_path_roots_t* roots) {
  c->arena = sp_mem_arena_new(mem);
  c->mem = sp_mem_arena_as_allocator(c->arena);
  c->roots = roots;
  sp_ht_init(c->mem, c->entries);
  sp_ht_init(c->mem, c->metadata);
  sp_ht_set_fns(c->metadata, spn_path_on_hash, spn_path_on_compare);
  sp_ht_init(c->mem, c->hints);
  sp_ht_set_fns(c->hints, spn_path_on_hash, spn_path_on_compare);
  sp_str_ht_init(c->mem, c->canonical);
}

sp_str_t spn_dag_file_cache_canonical(spn_dag_file_cache_t* c, sp_str_t path) {
  sp_mutex_lock(&c->mutex);
  sp_str_t* cached = sp_ht_getp(c->canonical, path);
  if (cached) {
    sp_str_t result = *cached;
    sp_mutex_unlock(&c->mutex);
    return result;
  }
  sp_mutex_unlock(&c->mutex);

  sp_str_t canonical = sp_fs_canonicalize_path(c->mem, path);

  sp_mutex_lock(&c->mutex);
  sp_ht_insert(c->canonical, sp_str_copy(c->mem, path), canonical);
  sp_mutex_unlock(&c->mutex);
  return canonical;
}

void spn_dag_file_cache_seed(spn_dag_file_cache_t* c, spn_dag_file_meta_t meta) {
  sp_mutex_lock(&c->mutex);
  sp_ht_insert(c->entries, meta.id, meta);
  sp_mutex_unlock(&c->mutex);
}

void spn_dag_file_cache_invalidate(spn_dag_file_cache_t* c, spn_path_t path) {
  sp_mutex_lock(&c->mutex);
  sp_ht_erase(c->metadata, path);
  sp_mutex_unlock(&c->mutex);
}

void spn_dag_file_cache_invalidate_dir(spn_dag_file_cache_t* c, spn_path_t dir) {
  sp_mem_arena_marker_t s = sp_mem_begin_scratch();
  sp_mutex_lock(&c->mutex);

  sp_da(spn_path_t) stale = sp_da_new(s.mem, spn_path_t);
  sp_ht_for_kv(c->metadata, it) {
    if (spn_path_within(dir, *it.key).within) {
      sp_da_push(stale, *it.key);
    }
  }
  sp_da_for(stale, it) {
    sp_ht_erase(c->metadata, stale[it]);
  }

  sp_mutex_unlock(&c->mutex);
  sp_mem_end_scratch(s);
}

void spn_dag_file_cache_invalidate_all(spn_dag_file_cache_t* c) {
  sp_mutex_lock(&c->mutex);
  sp_ht_clear(c->metadata);
  sp_mutex_unlock(&c->mutex);
}

spn_err_t spn_dag_file_cache_stat(spn_dag_file_cache_t* c, spn_path_t path, sp_sys_file_meta_t* meta) {
  sp_mutex_lock(&c->mutex);
  sp_sys_file_meta_t* cached = sp_ht_getp(c->metadata, path);
  if (cached) {
    *meta = *cached;
    sp_mutex_unlock(&c->mutex);
    return SPN_OK;
  }
  sp_mutex_unlock(&c->mutex);

  if (c->stats) {
    sp_atomic_u32_add(&c->stats->stats, 1, SP_ATOMIC_RELAXED);
  }
  sp_mem_arena_marker_t s = sp_mem_begin_scratch();
  sp_sys_file_meta_t sys = sp_zero;
  sp_err_t rc = sp_sys_get_path_metadata_s(sp_sys_get_root(0), spn_path_str(c->roots, s.mem, path), &sys);
  sp_mem_end_scratch(s);
  if (rc) {
    return SPN_ERR_DAG_STAT;
  }

  sp_mutex_lock(&c->mutex);
  sp_ht_insert(c->metadata, spn_path_copy(c->mem, path), sys);
  sp_mutex_unlock(&c->mutex);
  *meta = sys;
  return SPN_OK;
}

spn_err_t spn_dag_file_cache_digest(spn_dag_file_cache_t* c, spn_path_t path, spn_dag_digest_t* digest) {
  sp_sys_file_meta_t sys = sp_zero;
  spn_try(spn_dag_file_cache_stat(c, path, &sys));

  spn_dag_file_meta_t fresh = file_meta_from_sys(sys);
  sp_mutex_lock(&c->mutex);
  spn_dag_file_meta_t* cached = sp_ht_getp(c->entries, fresh.id);
  if (cached && file_meta_current(*cached, sys)) {
    *digest = cached->digest;
    sp_mutex_unlock(&c->mutex);
    return SPN_OK;
  }

  spn_dag_file_meta_t* hint = sp_ht_getp(c->hints, path);
  if (hint && file_meta_current(*hint, sys) && spn_dag_digest_valid(hint->digest)) {
    hint->id.device = sys.device;
    *digest = hint->digest;
    sp_ht_insert(c->entries, hint->id, *hint);
    sp_mutex_unlock(&c->mutex);
    return SPN_OK;
  }
  sp_mutex_unlock(&c->mutex);

  sp_mem_arena_marker_t s = sp_mem_begin_scratch();
  u64 size = 0;
  spn_err_t err = spn_sha256_file_digest(spn_path_str(c->roots, s.mem, path), digest->bytes, &size);
  sp_mem_end_scratch(s);
  spn_try(err);
  if (c->stats) {
    sp_atomic_u32_add(&c->stats->hashed_files, 1, SP_ATOMIC_RELAXED);
    sp_atomic_u64_add(&c->stats->hashed_bytes, size, SP_ATOMIC_RELAXED);
  }

  fresh.digest = *digest;
  sp_mutex_lock(&c->mutex);
  sp_ht_insert(c->entries, fresh.id, fresh);
  sp_ht_insert(c->hints, spn_path_copy(c->mem, path), fresh);
  c->hints_dirty = true;
  sp_mutex_unlock(&c->mutex);
  return SPN_OK;
}

static void diag_set(spn_dag_diag_t* diag, spn_err_t err, spn_dag_id_t action, sp_str_t path) {
  if (!diag || diag->err) {
    return;
  }
  diag->err = err;
  diag->action = action;
  diag->path = path;
}

static void trace_emit(spn_dag_env_t* env, spn_dag_trace_event_t event) {
  if (env->trace) {
    env->trace(&event, env->trace_data);
  }
}

static void trace_resolve(spn_dag_env_t* env, spn_dag_id_t action, bool hit) {
  trace_emit(env, (spn_dag_trace_event_t) {
    .kind = SPN_DAG_TRACE_RESOLVE,
    .action = action,
    .hit = hit
  });
}

static void progress_total(spn_dag_env_t* env, u64 total) {
  if (env->progress) {
    sp_atomic_s32_store(&env->progress->total, (s32)total, SP_ATOMIC_SEQ_CST);
    if (env->wake) {
      spn_wake_ring(env->wake);
    }
  }
}

static void progress_count(spn_dag_env_t* env, const spn_dag_action_t* action, bool hit) {
  if (action->uncacheable) sp_assert(!hit);
  if (!env->progress) {
    return;
  }
  if (!action->uncacheable) {
    sp_atomic_s32_add(hit ? &env->progress->hits : &env->progress->misses, 1, SP_ATOMIC_SEQ_CST);
  }
  sp_atomic_s32_add(&env->progress->completed, 1, SP_ATOMIC_SEQ_CST);
  if (env->wake) {
    spn_wake_ring(env->wake);
  }
}

static bool is_file_settled(spn_dag_file_cache_t* files, spn_path_t path, spn_dag_digest_t digest) {
  spn_dag_digest_t existing = sp_zero;
  if (spn_dag_file_cache_digest(files, path, &existing)) {
    return false;
  }
  return spn_dag_digest_equal(existing, digest);
}

static void prime_materialized(spn_dag_env_t* env, spn_path_t target) {
  spn_dag_file_cache_invalidate(env->files, target);
  spn_dag_digest_t digest = sp_zero;
  spn_dag_file_cache_digest(env->files, target, &digest);
}

static bool is_tree_settled(spn_dag_t* g, spn_dag_artifact_t* artifact, spn_dag_env_t* env) {
  sp_mem_arena_marker_t s = sp_mem_begin_scratch();
  bool settled = false;

  sp_str_t dir = spn_path_str(g->roots, s.mem, artifact->path);
  if (!sp_fs_is_dir(dir)) {
    goto done;
  }

  sp_da(spn_dag_action_output_t) entries = sp_zero;
  if (spn_dag_tree_entries(env->store, artifact->digest, s.mem, &entries)) {
    goto done;
  }

  u64 present = 0;
  sp_da(sp_fs_entry_t) files = sp_fs_collect_recursive(s.mem, dir);
  sp_da_for(files, it) {
    if (files[it].kind != SP_FS_KIND_DIR) {
      present++;
    }
  }
  if (present != sp_da_size(entries)) {
    goto done;
  }

  sp_da_for(entries, it) {
    if (!is_file_settled(env->files, spn_path_join(s.mem, artifact->path, entries[it].name), entries[it].digest)) {
      goto done;
    }
  }
  settled = true;

done:
  sp_mem_end_scratch(s);
  return settled;
}

static void prime_tree(spn_dag_env_t* env, spn_dag_artifact_t* artifact) {
  sp_mem_arena_marker_t s = sp_mem_begin_scratch();
  sp_da(spn_dag_action_output_t) entries = sp_zero;
  if (!spn_dag_tree_entries(env->store, artifact->digest, s.mem, &entries)) {
    sp_da_for(entries, it) {
      prime_materialized(env, spn_path_join(s.mem, artifact->path, entries[it].name));
    }
  }
  sp_mem_end_scratch(s);
}

static sp_str_t artifact_render(spn_dag_t* g, spn_path_t path) {
  sp_mutex_lock(&g->mutex);
  sp_str_t rendered = spn_path_str(g->roots, g->mem, path);
  sp_mutex_unlock(&g->mutex);
  return rendered;
}

static sp_str_t artifact_location(spn_dag_t* g, spn_dag_artifact_t* artifact) {
  return spn_path_empty(artifact->path) ? artifact->name : artifact_render(g, artifact->path);
}

static spn_err_t settle_tree(spn_dag_t* g, spn_dag_action_t* action, spn_dag_artifact_t* artifact, spn_dag_env_t* env, spn_dag_diag_t* diag) {
  bool settled = is_tree_settled(g, artifact, env);
  trace_emit(env, (spn_dag_trace_event_t) { .kind = SPN_DAG_TRACE_SETTLE, .action = action->id, .producer = artifact->id, .key = artifact->digest, .hit = settled });
  if (settled) {
    return SPN_OK;
  }
  action->wrote = true;
  spn_dag_file_cache_invalidate_dir(env->files, artifact->path);

  sp_mem_arena_marker_t s = sp_mem_begin_scratch();
  spn_err_t err = spn_dag_store_materialize_tree(env->store, artifact->digest, spn_path_str(g->roots, s.mem, artifact->path));
  sp_mem_end_scratch(s);
  if (err) {
    diag_set(diag, err, action->id, artifact_render(g, artifact->path));
    return err;
  }
  prime_tree(env, artifact);
  return SPN_OK;
}

static spn_err_t settle_file(spn_dag_t* g, spn_dag_action_t* action, spn_dag_artifact_t* artifact, spn_dag_env_t* env, spn_dag_diag_t* diag) {
  bool settled = is_file_settled(env->files, artifact->path, artifact->digest);
  trace_emit(env, (spn_dag_trace_event_t) { .kind = SPN_DAG_TRACE_SETTLE, .action = action->id, .producer = artifact->id, .key = artifact->digest, .hit = settled });
  if (settled) {
    return SPN_OK;
  }
  action->wrote = true;

  sp_mem_arena_marker_t s = sp_mem_begin_scratch();
  spn_err_t err = spn_dag_store_materialize(env->store, artifact->digest, artifact->name, spn_path_str(g->roots, s.mem, artifact->path));
  sp_mem_end_scratch(s);
  if (err) {
    diag_set(diag, err, action->id, artifact_render(g, artifact->path));
    return err;
  }
  prime_materialized(env, artifact->path);
  return SPN_OK;
}

static spn_err_t settle(spn_dag_t* g, spn_dag_action_t* action, spn_dag_env_t* env, spn_dag_diag_t* diag) {
  action->wrote = false;
  sp_da_for(action->produces, it) {
    spn_dag_artifact_t* artifact = spn_dag_find_artifact(g, action->produces[it]);
    if (spn_path_empty(artifact->path)) {
      sp_mutex_lock(&g->mutex);
      artifact->materialized = spn_dag_store_path(env->store, g->mem, artifact->digest, artifact->name);
      sp_mutex_unlock(&g->mutex);
      continue;
    }
    spn_try(artifact->kind == SPN_DAG_ARTIFACT_KIND_TREE
      ? settle_tree(g, action, artifact, env, diag)
      : settle_file(g, action, artifact, env, diag));
    artifact->materialized = artifact->path;
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

  return !settle(g, action, env, SP_NULLPTR);
}

static bool try_restore(spn_dag_t* g, spn_dag_action_t* action, spn_dag_digest_t key, spn_dag_env_t* env) {
  spn_dag_action_entry_t entry = sp_zero;
  bool present = spn_dag_action_cache_get(env->cache, key, &entry);
  bool hit = present && restore_entry(g, action, &entry, env);
  trace_emit(env, (spn_dag_trace_event_t) {
    .kind = SPN_DAG_TRACE_CACHE,
    .action = action->id,
    .key = key,
    .present = present,
    .hit = hit,
  });
  if (present && !hit) {
    spn_dag_action_cache_remove(env->cache, key);
  }
  return hit;
}

static s32 obs_order(const void* a, const void* b) {
  const spn_dag_obs_t* oa = (const spn_dag_obs_t*)a;
  const spn_dag_obs_t* ob = (const spn_dag_obs_t*)b;
  if (oa->path.root != ob->path.root) {
    return (s32)oa->path.root - (s32)ob->path.root;
  }
  s32 order = sp_str_compare_alphabetical(oa->path.sub, ob->path.sub);
  if (order) {
    return order;
  }
  if (oa->kind != ob->kind) {
    return (s32)oa->kind - (s32)ob->kind;
  }
  return sp_str_compare_alphabetical(oa->filter, ob->filter);
}

static bool obs_equal(const spn_dag_obs_t* a, const spn_dag_obs_t* b) {
  return a->kind == b->kind && spn_path_equal(a->path, b->path) && sp_str_equal(a->filter, b->filter);
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

static spn_err_t resolve_one(spn_dag_file_cache_t* files, spn_dag_obs_t* o, sp_mem_t mem) {
  switch (o->kind) {
    case SPN_DAG_OBS_ENUMERATION: {
      o->meta = (spn_dag_file_meta_t) sp_zero;
      return membership_digest(spn_path_str(files->roots, mem, o->path), o->filter, &o->meta.digest);
    }
    case SPN_DAG_OBS_ABSENT: {
      if (!sp_fs_exists(spn_path_str(files->roots, mem, o->path))) {
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
  spn_try(spn_dag_file_cache_stat(files, o->path, &sys));

  if (sys.kind == SP_FS_KIND_DIR) {
    o->meta = (spn_dag_file_meta_t) sp_zero;
    return membership_digest(spn_path_str(files->roots, mem, o->path), sp_str_lit(""), &o->meta.digest);
  }

  spn_dag_file_meta_t fresh = file_meta_from_sys(sys);
  spn_try(spn_dag_file_cache_digest(files, o->path, &fresh.digest));
  o->meta = fresh;
  return SPN_OK;
}

static spn_err_t resolve_observations(spn_dag_file_cache_t* files, spn_dag_obs_t* obs, u32 count) {
  sp_mem_arena_marker_t s = sp_mem_begin_scratch();
  spn_err_t err = SPN_OK;
  sp_for(it, count) {
    err = resolve_one(files, &obs[it], s.mem);
    if (err) {
      break;
    }
  }
  sp_mem_end_scratch(s);
  return err;
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

static spn_path_t begin_scratch(spn_dag_t* g, spn_dag_action_t* action, spn_path_t root) {
  sp_assert(!spn_path_empty(root));
  sp_mem_arena_marker_t s = sp_mem_begin_scratch();

  sp_str_t prefix = spn_path_str(g->roots, s.mem, spn_path_join(s.mem, root, sp_str_lit("scratch")));
  sp_str_t name = sp_zero;
  if (sp_fs_staging_dir_name(s.mem, prefix, sp_str_lit("tmp"), &name)) {
    sp_mem_end_scratch(s);
    return (spn_path_t) sp_zero;
  }

  sp_mutex_lock(&g->mutex);
  spn_path_t dir = spn_path_join(g->mem, root, name);
  sp_da_for(action->produces, it) {
    spn_dag_artifact_t* artifact = spn_dag_find_artifact(g, action->produces[it]);
    artifact->materialized = spn_path_join(g->mem, dir, artifact->name);
    if (artifact->kind == SPN_DAG_ARTIFACT_KIND_TREE) {
      sp_fs_create_dir(spn_path_str(g->roots, s.mem, artifact->materialized));
    }
  }
  sp_mutex_unlock(&g->mutex);

  sp_mem_end_scratch(s);
  return dir;
}

static void end_scratch(spn_dag_t* g, spn_path_t dir) {
  if (spn_path_empty(dir)) {
    return;
  }
  sp_mem_arena_marker_t s = sp_mem_begin_scratch();
  sp_fs_remove_dir(spn_path_str(g->roots, s.mem, dir));
  sp_mem_end_scratch(s);
}

typedef struct {
  spn_dag_action_t* action;
  sp_mem_t mem;
  spn_dag_digest_t key;
  spn_path_t scratch;
  bool hit;
  sp_da(spn_dag_digest_t) digests;
  sp_da(spn_dag_obs_t) obs;
  spn_dag_diag_t diag;
} spn_dag_attempt_t;

static void diag_flush(spn_dag_env_t* env, spn_dag_attempt_t* attempt, spn_err_t err) {
  if (!err) {
    return;
  }
  if (attempt->diag.err) {
    diag_set(&env->diag, attempt->diag.err, attempt->diag.action, attempt->diag.path);
  }
  else {
    spn_dag_id_t action = attempt->action ? attempt->action->id : (spn_dag_id_t) sp_zero;
    diag_set(&env->diag, err, action, sp_str_lit(""));
  }
}

static spn_err_t lookup(spn_dag_t* g, spn_dag_action_t* action, spn_dag_env_t* env, sp_mem_t mem, spn_dag_attempt_t* attempt) {
  attempt->action = action;
  attempt->mem = mem;
  sp_da_init(mem, attempt->digests);
  sp_da_init(mem, attempt->obs);

  if (action->uncacheable) {
    attempt->scratch = begin_scratch(g, action, env->scratch);
    return spn_path_empty(attempt->scratch) ? SPN_ERR_DAG_SCRATCH : SPN_OK;
  }

  attempt->key = spn_dag_weak_key(g, action->id);
  trace_emit(env, (spn_dag_trace_event_t) { .kind = SPN_DAG_TRACE_KEY, .action = action->id, .key = attempt->key });

  if (action->discover) {
    spn_dag_pathset_t set = sp_zero;
    bool present = spn_dag_obs_table_get(env->discovery, attempt->key, mem, &set);
    trace_emit(env, (spn_dag_trace_event_t) { .kind = SPN_DAG_TRACE_DISCOVERY, .action = action->id, .key = attempt->key, .hit = present });
    if (present) {
      u32 count = (u32)sp_da_size(set.obs);
      bool resolved = !resolve_observations(env->files, set.obs, count);
      trace_resolve(env, action->id, resolved);
      if (resolved) {
        spn_dag_digest_t strong = spn_dag_strong_key(attempt->key, set.obs, count);
        trace_emit(env, (spn_dag_trace_event_t) { .kind = SPN_DAG_TRACE_STRONG, .action = action->id, .key = strong });
        if (try_restore(g, action, strong, env)) {
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
  return spn_path_empty(attempt->scratch) ? SPN_ERR_DAG_SCRATCH : SPN_OK;
}

static spn_err_t store_produced(spn_dag_env_t* env, spn_dag_artifact_t* artifact, sp_str_t produced, spn_dag_digest_t* digest) {
  if (!sp_fs_exists(produced)) {
    return SPN_ERR_DAG_MISSING_OUTPUT;
  }
  return artifact->kind == SPN_DAG_ARTIFACT_KIND_TREE
    ? spn_dag_store_put_tree(env->store, produced, digest)
    : spn_dag_store_put_file(env->store, produced, artifact->name, digest);
}

static spn_err_t execute(spn_dag_t* g, spn_dag_attempt_t* attempt, spn_dag_env_t* env) {
  spn_dag_action_t* action = attempt->action;
  trace_emit(env, (spn_dag_trace_event_t) { .kind = SPN_DAG_TRACE_EXECUTE, .action = action->id, .key = attempt->key });

  if (action->execute) {
    if (action->execute(g, action, action->user_data)) {
      diag_set(&attempt->diag, SPN_ERR_DAG_ACTION, action->id, sp_str_lit(""));
      return SPN_ERR_DAG_ACTION;
    }
  }
  if (action->discover) {
    spn_err_t err = action->discover(g, action, action->user_data, env, attempt->mem, &attempt->obs);
    if (err) {
      diag_set(&attempt->diag, err, action->id, sp_str_lit(""));
      return err;
    }
    canonicalize_observations(attempt->obs);
  }

  sp_da_for(action->produces, it) {
    spn_dag_artifact_t* artifact = spn_dag_find_artifact(g, action->produces[it]);
    sp_mem_arena_marker_t s = sp_mem_begin_scratch();
    spn_dag_digest_t digest = sp_zero;
    spn_err_t put = store_produced(env, artifact, spn_path_str(g->roots, s.mem, artifact->materialized), &digest);
    sp_mem_end_scratch(s);
    if (put) {
      diag_set(&attempt->diag, put, action->id, artifact_location(g, artifact));
      return put;
    }
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

  if (action->uncacheable) {
    spn_try(settle(g, action, env, &attempt->diag));
    trace_emit(env, (spn_dag_trace_event_t) { .kind = SPN_DAG_TRACE_COMMIT, .action = action->id });
    return SPN_OK;
  }

  spn_dag_digest_t key = attempt->key;
  bool resolved = true;
  if (action->discover) {
    u32 count = (u32)sp_da_size(attempt->obs);
    resolved = !resolve_observations(env->files, attempt->obs, count);
    trace_resolve(env, action->id, resolved);
    spn_dag_obs_table_put(env->discovery, attempt->key, attempt->obs, count);
    if (resolved) {
      key = spn_dag_strong_key(attempt->key, attempt->obs, count);
      trace_emit(env, (spn_dag_trace_event_t) { .kind = SPN_DAG_TRACE_STRONG, .action = action->id, .key = key });
    }
  }

  spn_try(settle(g, action, env, &attempt->diag));
  if (resolved) {
    record(g, action, key, env);
  }
  trace_emit(env, (spn_dag_trace_event_t) { .kind = SPN_DAG_TRACE_COMMIT, .action = action->id, .key = key, .hit = resolved });
  return SPN_OK;
}

static spn_err_t exec_action(spn_dag_t* g, spn_dag_action_t* action, spn_dag_env_t* env) {
  sp_mem_arena_marker_t s = sp_mem_begin_scratch();
  env->diag = (spn_dag_diag_t) sp_zero;

  spn_dag_attempt_t attempt = sp_zero;
  spn_err_t err = lookup(g, action, env, s.mem, &attempt);
  if (!err && !attempt.hit) {
    err = execute(g, &attempt, env);
    if (!err) {
      err = commit(g, &attempt, env);
    }
    end_scratch(g, attempt.scratch);
  }
  diag_flush(env, &attempt, err);
  if (!err) {
    progress_count(env, action, attempt.hit);
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
  sp_ht(spn_path_t, spn_dag_target_t) by_path;
  sp_ht(spn_path_t, sp_da(u32)) below;
} spn_dag_targets_t;

static bool path_parent(spn_path_t* path) {
  if (sp_str_empty(path->sub)) {
    return false;
  }
  path->sub = parent_dir(path->sub);
  return !sp_str_empty(path->sub) || path->root != SPN_PATH_ROOT_NONE;
}

static void targets_init(spn_dag_targets_t* targets, spn_dag_t* g, sp_mem_t mem) {
  sp_ht_init(mem, targets->by_path);
  sp_ht_set_fns(targets->by_path, spn_path_on_hash, spn_path_on_compare);
  sp_ht_init(mem, targets->below);
  sp_ht_set_fns(targets->below, spn_path_on_hash, spn_path_on_compare);

  sp_da_for(g->artifacts, it) {
    spn_dag_artifact_t* artifact = &g->artifacts[it];
    if (!artifact->producer.occupied || spn_path_empty(artifact->path)) {
      continue;
    }

    sp_assert(!sp_ht_getp(targets->by_path, artifact->path));
    sp_ht_insert(targets->by_path, artifact->path, ((spn_dag_target_t) {
      .producer = artifact->producer.index,
      .kind = artifact->kind
    }));

    for (spn_path_t dir = artifact->path; path_parent(&dir);) {
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
  spn_dag_action_t* action;
  sp_mem_arena_t* arena;
  sp_mem_t mem;
  sp_atomic_s32_t* completed;
  u64 epoch;
  spn_err_t err;
  spn_dag_attempt_t attempt;
};

typedef struct {
  spn_dag_t* g;
  spn_dag_env_t* env;
  spn_thread_pool_executor_t* ex;
  spn_dag_targets_t targets;
  spn_dag_run_state_t* states;
  sp_da(spn_dag_id_t) ready;
  sp_atomic_s32_t completed;
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

static bool defer_observations(spn_dag_run_t* run, spn_dag_action_t* action, sp_da(spn_dag_obs_t) obs, u64 epoch, bool* requeue) {
  sp_da_for(obs, it) {
    const spn_dag_obs_t* o = &obs[it];

    spn_dag_target_t* exact = sp_ht_getp(run->targets.by_path, o->path);
    if (exact) {
      defer_producer(run, action, exact->producer, epoch, requeue);
    }

    for (spn_path_t dir = o->path; path_parent(&dir);) {
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
  sp_mem_arena_marker_t s = sp_mem_begin_scratch();
  spn_dag_pathset_t set = sp_zero;
  bool deferred = false;
  if (spn_dag_obs_table_get(run->env->discovery, weak, s.mem, &set)) {
    deferred = defer_observations(run, action, set.obs, epoch, requeue);
  }
  sp_mem_end_scratch(s);
  return deferred;
}

static spn_err_t seed_source(spn_dag_env_t* env, spn_dag_artifact_t* artifact) {
  artifact->materialized = artifact->path;
  sp_sys_file_meta_t sys = sp_zero;
  spn_try(spn_dag_file_cache_stat(env->files, artifact->materialized, &sys));
  if (sys.kind == SP_FS_KIND_DIR) {
    return SPN_ERR_DAG_MISSING_INPUT;
  }
  return spn_dag_file_cache_digest(env->files, artifact->materialized, &artifact->digest);
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
          if (seed_source(env, artifact)) {
            diag_set(&env->diag, SPN_ERR_DAG_MISSING_INPUT, (spn_dag_id_t) sp_zero, artifact_render(g, artifact->path));
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
  state->done_epoch = (u64)(sp_atomic_s32_add(&run->completed, 1, SP_ATOMIC_SEQ_CST) + 1);

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
  flight->epoch = (u64)sp_atomic_s32_load(flight->completed, SP_ATOMIC_SEQ_CST);
  flight->err = lookup(flight->g, flight->action, flight->env, flight->mem, &flight->attempt);
  if (!flight->err && !flight->attempt.hit) {
    flight->err = execute(flight->g, &flight->attempt, flight->env);
  }
}

static void flight_free(spn_dag_flight_t* flight) {
  end_scratch(flight->g, flight->attempt.scratch);
  sp_mem_arena_destroy(flight->arena);
}

static void run_commit_flight(spn_dag_run_t* run, spn_dag_action_t* action, spn_dag_flight_t* flight) {
  run->err = commit(run->g, &flight->attempt, run->env);
  diag_flush(run->env, &flight->attempt, run->err);
  flight_free(flight);
  if (run->err) {
    return;
  }
  progress_count(run->env, action, false);
  finish_action(run, action);
}

static void run_dispatch(spn_dag_run_t* run, spn_dag_id_t id) {
  spn_dag_action_t* action = spn_dag_find_action(run->g, id);
  spn_dag_run_state_t* state = &run->states[id.index];

  if (state->parked) {
    spn_dag_flight_t* flight = state->parked;
    bool requeue = false;
    if (defer_observations(run, action, flight->attempt.obs, flight->epoch, &requeue)) {
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
    if (defer_pathset(run, action, spn_dag_weak_key(run->g, action->id), (u64)sp_atomic_s32_load(&run->completed, SP_ATOMIC_SEQ_CST), &requeue)) {
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
    .action = action,
    .arena = arena,
    .mem = mem,
    .completed = &run->completed,
  };

  spn_thread_pool_submit(run->ex, (spn_thread_pool_job_t) { .fn = flight_run, .data = flight });
  run->in_flight++;
}

static void run_complete(spn_dag_run_t* run, spn_dag_flight_t* flight) {
  spn_dag_action_t* action = flight->action;

  if (run->err || flight->err) {
    diag_flush(run->env, &flight->attempt, flight->err);
    run->err = run->err ? run->err : flight->err;
    flight_free(flight);
    return;
  }

  if (flight->attempt.hit) {
    flight_free(flight);
    progress_count(run->env, action, true);
    finish_action(run, action);
    return;
  }

  if (action->discover) {
    bool requeue = false;
    if (defer_observations(run, action, flight->attempt.obs, flight->epoch, &requeue)) {
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

spn_err_t spn_dag_run_executor(spn_dag_t* g, spn_dag_env_t* env, spn_thread_pool_executor_t* ex) {
  sp_mem_arena_marker_t s = sp_mem_begin_scratch();
  env->diag = (spn_dag_diag_t) sp_zero;

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
    sp_fs_create_dir(spn_path_str(g->roots, s.mem, spn_path_join(s.mem, env->scratch, sp_str_lit("scratch"))));
    run.states = sp_alloc_n(s.mem, spn_dag_run_state_t, n ? n : 1);
    run.ready = sp_da_new(s.mem, spn_dag_id_t);
    seed_ready(&run, s.mem);
    targets_init(&run.targets, g, s.mem);

    u64 turns = 0;
    u64 turns_max = 4 * (n + 1) * (n + 1);
    while (true) {
      turns++;
      sp_assert(turns <= turns_max);

      if (!run.err && env->cancel && sp_atomic_s32_load(env->cancel, SP_ATOMIC_SEQ_CST)) {
        run.err = SPN_ERR_DAG_CANCELLED;
      }

      spn_thread_pool_job_t job = spn_thread_pool_try_poll(ex);
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
        job = spn_thread_pool_poll(ex);
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
    if (!run.err && (u64)sp_atomic_s32_load(&run.completed, SP_ATOMIC_SEQ_CST) != n) {
      run.err = SPN_ERR_DAG_STALLED;
      diag_set(&env->diag, SPN_ERR_DAG_STALLED, (spn_dag_id_t) sp_zero, sp_str_lit(""));
    }
  }

  sp_mem_end_scratch(s);
  return run.err;
}

spn_err_t spn_dag_run(spn_dag_t* g, spn_dag_env_t* env) {
  spn_thread_pool_t pool = sp_zero;
  spn_thread_pool_init(&pool, g->mem, (spn_thread_pool_config_t) sp_zero);
  spn_err_t err = spn_dag_run_executor(g, env, &pool.executor);
  spn_thread_pool_deinit(&pool);
  return err;
}
