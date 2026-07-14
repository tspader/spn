#include "dag/dag.h"
#include "dag/occ.h"
#include "sha256/sha256.h"
#include "error/types.h"
#include "sp.h"
#include "sp/io.h"
#include "sp/atomic_file.h"

spn_dag_t* spn_dag_new(sp_mem_t mem) {
  sp_mem_arena_t* arena = sp_mem_arena_new(mem);
  sp_mem_t a = sp_mem_arena_as_allocator(arena);
  spn_dag_t* g = sp_mem_allocator_alloc_type(a, spn_dag_t);
  g->arena = arena;
  g->mem = a;
  sp_da_init(a, g->artifacts);
  sp_da_init(a, g->actions);
  sp_str_ht_init(a, g->files);
  return g;
}

spn_dag_artifact_t* spn_dag_find_artifact(spn_dag_t* g, spn_dag_id_t id) {
  sp_assert(id.occupied);
  return g->artifacts + id.index;
}

spn_dag_action_t* spn_dag_find_action(spn_dag_t* g, spn_dag_id_t id) {
  sp_assert(id.occupied);
  return g->actions + id.index;
}

static spn_dag_id_t spn_dag_add_artifact(spn_dag_t* g, spn_dag_artifact_t artifact) {
  artifact.id = (spn_dag_id_t) {
    .index = (u32)sp_da_size(g->artifacts),
    .occupied = true
  };
  sp_da_init(g->mem, artifact.consumers);
  sp_da_push(g->artifacts, artifact);
  return artifact.id;
}

spn_dag_id_t spn_dag_add_value(spn_dag_t* g, const void* data, u64 len) {
  return spn_dag_add_artifact(g, (spn_dag_artifact_t) {
    .kind = SPN_DAG_ARTIFACT_KIND_VALUE,
    .digest = spn_dag_digest(data, len),
  });
}

spn_dag_id_t spn_dag_find_file(spn_dag_t* g, sp_str_t path) {
  spn_dag_id_t* existing = sp_ht_getp(g->files, path);
  return existing ? *existing : (spn_dag_id_t) sp_zero;
}

spn_dag_id_t spn_dag_add_file(spn_dag_t* g, sp_str_t path) {
  spn_dag_id_t existing = spn_dag_find_file(g, path);
  if (existing.occupied) {
    return existing;
  }

  sp_str_t copy = sp_str_copy(g->mem, path);
  spn_dag_id_t id = spn_dag_add_artifact(g, (spn_dag_artifact_t) {
    .kind = SPN_DAG_ARTIFACT_KIND_FILE,
    .path = copy,
  });
  sp_ht_insert(g->files, copy, id);
  return id;
}

spn_dag_id_t spn_dag_add_action(spn_dag_t* g, spn_dag_action_config_t config) {
  spn_dag_action_t action = {
    .id = {
      .index = (u32)sp_da_size(g->actions),
      .occupied = true
    },
    .identity = config.identity,
    .execute = config.execute,
    .discover = config.discover,
    .user_data = config.user_data,
  };
  sp_da_init(g->mem, action.consumes);
  sp_da_init(g->mem, action.produces);
  sp_da_push(g->actions, action);
  return action.id;
}

void spn_dag_action_add_input(spn_dag_t* g, spn_dag_id_t action_id, spn_dag_id_t artifact_id) {
  spn_dag_action_t* action = spn_dag_find_action(g, action_id);
  spn_dag_artifact_t* artifact = spn_dag_find_artifact(g, artifact_id);
  sp_da_push(action->consumes, artifact_id);
  sp_da_push(artifact->consumers, action_id);
}

spn_err_t spn_dag_action_add_output(spn_dag_t* g, spn_dag_id_t action_id, spn_dag_id_t artifact_id) {
  spn_dag_action_t* action = spn_dag_find_action(g, action_id);
  spn_dag_artifact_t* artifact = spn_dag_find_artifact(g, artifact_id);
  if (artifact->producer.occupied) {
    return SPN_ERROR;
  }
  artifact->producer = action_id;
  sp_da_push(action->produces, artifact_id);
  return SPN_OK;
}

static void spn_dag_hash_bytes(spn_sha256_ctx_t* ctx, const void* data, u64 len) {
  spn_sha256_update(ctx, (const u8*)data, len);
}

static void spn_dag_hash_u8(spn_sha256_ctx_t* ctx, u8 value) {
  spn_dag_hash_bytes(ctx, &value, sizeof(value));
}

static void spn_dag_hash_u64(spn_sha256_ctx_t* ctx, u64 value) {
  spn_dag_hash_bytes(ctx, &value, sizeof(value));
}

static void spn_dag_hash_str(spn_sha256_ctx_t* ctx, sp_str_t str) {
  spn_dag_hash_u64(ctx, str.len);
  spn_dag_hash_bytes(ctx, str.data, str.len);
}

static void spn_dag_hash_digest(spn_sha256_ctx_t* ctx, spn_dag_digest_t digest) {
  spn_dag_hash_bytes(ctx, digest.bytes, sizeof(digest.bytes));
}

spn_dag_digest_t spn_dag_action_key(spn_dag_t* g, spn_dag_id_t action_id) {
  spn_dag_action_t* action = spn_dag_find_action(g, action_id);

  spn_sha256_ctx_t ctx = sp_zero;
  spn_sha256_init(&ctx);
  spn_dag_hash_str(&ctx, sp_str_lit("spn.dag.action.v2"));
  spn_dag_hash_digest(&ctx, action->identity);

  spn_dag_hash_u64(&ctx, sp_da_size(action->consumes));
  sp_da_for(action->consumes, it) {
    spn_dag_artifact_t* artifact = spn_dag_find_artifact(g, action->consumes[it]);
    sp_assert(spn_dag_digest_valid(artifact->digest));
    spn_dag_hash_u8(&ctx, (u8)artifact->kind);
    spn_dag_hash_digest(&ctx, artifact->digest);
  }

  spn_dag_hash_u64(&ctx, sp_da_size(action->produces));
  sp_da_for(action->produces, it) {
    spn_dag_artifact_t* artifact = spn_dag_find_artifact(g, action->produces[it]);
    spn_dag_hash_u8(&ctx, (u8)artifact->kind);
    spn_dag_hash_str(&ctx, artifact->path);
  }

  spn_dag_digest_t key = sp_zero;
  spn_sha256_final(&ctx, key.bytes);
  return key;
}

spn_dag_digest_t spn_dag_strong_key(spn_dag_digest_t prelim, const spn_dag_obs_t* obs, u32 count) {
  spn_sha256_ctx_t ctx = sp_zero;
  spn_sha256_init(&ctx);
  spn_dag_hash_str(&ctx, sp_str_lit("spn.dag.strong.v2"));
  spn_dag_hash_digest(&ctx, prelim);
  spn_dag_hash_u64(&ctx, count);
  sp_for(it, count) {
    spn_dag_hash_u8(&ctx, (u8)obs[it].kind);
    spn_dag_hash_str(&ctx, obs[it].path);
    spn_dag_hash_digest(&ctx, obs[it].meta.digest);
  }

  spn_dag_digest_t key = sp_zero;
  spn_sha256_final(&ctx, key.bytes);
  return key;
}

spn_dag_digest_t spn_dag_digest(const void* data, u64 len) {
  spn_dag_digest_t digest = sp_zero;
  spn_sha256(data, len, digest.bytes);
  return digest;
}

bool spn_dag_digest_equal(spn_dag_digest_t a, spn_dag_digest_t b) {
  return sp_mem_is_equal(a.bytes, b.bytes, sizeof(a.bytes));
}

bool spn_dag_digest_valid(spn_dag_digest_t digest) {
  sp_for(it, sizeof(digest.bytes)) {
    if (digest.bytes[it]) {
      return true;
    }
  }
  return false;
}

sp_str_t spn_dag_digest_hex(sp_mem_t mem, spn_dag_digest_t digest) {
  return spn_sha256_digest_hex(mem, digest.bytes);
}

void spn_dag_file_cache_init(spn_dag_file_cache_t* c, sp_mem_t mem) {
  c->arena = sp_mem_arena_new(mem);
  c->mem = sp_mem_arena_as_allocator(c->arena);
  sp_ht_init(c->mem, c->entries);
  sp_str_ht_init(c->mem, c->metadata);
}

void spn_dag_file_cache_refresh(spn_dag_file_cache_t* c) {
  sp_ht_clear(c->metadata);
}

void spn_dag_file_cache_invalidate(spn_dag_file_cache_t* c, sp_str_t path) {
  if (sp_ht_getp(c->metadata, path)) {
    sp_ht_erase(c->metadata, path);
  }
}

void spn_dag_file_cache_seed(spn_dag_file_cache_t* c, spn_dag_file_meta_t meta) {
  sp_ht_insert(c->entries, meta.id, meta);
}

spn_err_t spn_dag_get_file_meta(spn_dag_file_cache_t* c, sp_str_t path, sp_sys_file_meta_t* meta) {
  sp_sys_file_meta_t* cached = sp_ht_getp(c->metadata, path);
  if (cached) {
    *meta = *cached;
    return SPN_OK;
  }

  sp_sys_file_meta_t st = sp_zero;
  spn_try_as(sp_sys_get_path_metadata_s(sp_sys_get_root(0), path, &st), SPN_ERROR);

  sp_ht_insert(c->metadata, sp_str_copy(c->mem, path), st);
  *meta = st;
  return SPN_OK;
}

static bool spn_dag_timespec_equal(sp_sys_timespec_t a, sp_sys_timespec_t b) {
  return a.tv_sec == b.tv_sec && a.tv_nsec == b.tv_nsec;
}

static bool is_file_clean(spn_dag_file_meta_t* cached, sp_sys_file_meta_t sys) {
  if (!cached) return false;
  if (!spn_dag_timespec_equal(sys.mtime, cached->mtime)) return false;
  if (cached->size != sys.size) return false;
  return true;
}

spn_err_t spn_dag_get_file_digest(spn_dag_file_cache_t* c, sp_str_t path, spn_dag_digest_t* digest) {
  sp_sys_file_meta_t sys = sp_zero;
  spn_try(spn_dag_get_file_meta(c, path, &sys));

  spn_dag_file_id_t id = {
    .device = sys.device,
    .id = sys.id
  };
  spn_dag_file_meta_t* cached = sp_ht_getp(c->entries, id);

  if (is_file_clean(cached, sys)) {
    *digest = cached->digest;
    return SPN_OK;
  }

  spn_try(spn_sha256_file_digest(path, digest->bytes));

  spn_dag_file_meta_t fresh = {
    .id = id,
    .mtime = sys.mtime,
    .size = sys.size,
    .digest = *digest
  };

  sp_ht_insert(c->entries, id, fresh);

  return SPN_OK;
}

static bool spn_dag_restore(spn_dag_t* g, spn_dag_action_t* action, const spn_dag_action_entry_t* entry, spn_dag_file_cache_t* files, spn_dag_store_t* store) {
  if (sp_da_size(entry->outputs) != sp_da_size(action->produces)) {
    return false;
  }

  sp_da_for(entry->outputs, it) {
    spn_dag_artifact_t* artifact = spn_dag_find_artifact(g, action->produces[it]);
    if (!sp_str_equal(entry->outputs[it].path, artifact->path)) {
      return false;
    }
    if (!spn_dag_store_has(store, entry->outputs[it].digest)) {
      return false;
    }
  }

  sp_da_for(entry->outputs, it) {
    spn_dag_action_output_t* out = &entry->outputs[it];
    if (spn_dag_store_materialize(store, out->digest, out->path)) {
      return false;
    }
    spn_dag_find_artifact(g, action->produces[it])->digest = out->digest;
    spn_dag_file_cache_invalidate(files, out->path);
  }

  return true;
}

static s32 spn_dag_obs_sort_kernel(const void* a, const void* b) {
  const spn_dag_obs_t* oa = (const spn_dag_obs_t*)a;
  const spn_dag_obs_t* ob = (const spn_dag_obs_t*)b;
  s32 order = sp_str_compare_alphabetical(oa->path, ob->path);
  if (order) {
    return order;
  }
  return (s32)oa->kind - (s32)ob->kind;
}

static void spn_dag_canonicalize(sp_da(spn_dag_obs_t) obs) {
  if (sp_da_empty(obs)) {
    return;
  }
  sp_da_sort(obs, spn_dag_obs_sort_kernel);
  u64 w = 1;
  for (u64 r = 1; r < sp_da_size(obs); r++) {
    if (!sp_str_equal(obs[r].path, obs[w - 1].path)) {
      obs[w++] = obs[r];
    }
  }
  sp_da_head(obs)->size = w;
}

static bool spn_dag_meta_current(spn_dag_file_meta_t meta, sp_sys_file_meta_t sys) {
  if (meta.id.device != sys.device || meta.id.id != sys.id) return false;
  if (!spn_dag_timespec_equal(meta.mtime, sys.mtime)) return false;
  if (meta.size != sys.size) return false;
  return spn_dag_digest_valid(meta.digest);
}

static spn_err_t spn_dag_resolve_obs(spn_dag_file_cache_t* files, spn_dag_obs_t* obs, u32 count) {
  sp_for(it, count) {
    spn_dag_obs_t* o = &obs[it];
    if (o->kind == SPN_DAG_OBS_ABSENT && !sp_fs_exists(o->path)) {
      o->meta = (spn_dag_file_meta_t) sp_zero;
      continue;
    }

    sp_sys_file_meta_t sys = sp_zero;
    spn_try(spn_dag_get_file_meta(files, o->path, &sys));

    if (spn_dag_meta_current(o->meta, sys)) {
      spn_dag_file_cache_seed(files, o->meta);
      continue;
    }

    spn_dag_file_meta_t fresh = {
      .id = { .device = sys.device, .id = sys.id },
      .mtime = sys.mtime,
      .size = sys.size,
    };
    spn_try(spn_dag_get_file_digest(files, o->path, &fresh.digest));
    o->meta = fresh;
  }
  return SPN_OK;
}

static spn_err_t spn_dag_publish(spn_dag_t* g, spn_dag_action_t* action, spn_dag_digest_t key, spn_dag_file_cache_t* files, spn_dag_action_cache_t* cache, spn_dag_store_t* store) {
  sp_mem_arena_marker_t s = sp_mem_begin_scratch();
  spn_err_t err = SPN_OK;

  sp_da(spn_dag_action_output_t) outputs = sp_da_new(s.mem, spn_dag_action_output_t);
  sp_da_for(action->produces, it) {
    spn_dag_artifact_t* artifact = spn_dag_find_artifact(g, action->produces[it]);
    spn_dag_file_cache_invalidate(files, artifact->path);
    if (!sp_fs_exists(artifact->path)) {
      err = SPN_ERROR;
      goto done;
    }
    if (spn_dag_store_put_file(store, artifact->path, &artifact->digest)) {
      err = SPN_ERROR;
      goto done;
    }
    sp_da_push(outputs, ((spn_dag_action_output_t) {
      .path = artifact->path,
      .digest = artifact->digest
    }));
  }

  spn_dag_action_cache_put(cache, key, outputs, (u32)sp_da_size(outputs));

done:
  sp_mem_end_scratch(s);
  return err;
}

static void spn_dag_clear_outputs(spn_dag_t* g, spn_dag_action_t* action, spn_dag_file_cache_t* files) {
  sp_da_for(action->produces, it) {
    spn_dag_artifact_t* artifact = spn_dag_find_artifact(g, action->produces[it]);
    sp_fs_remove_file(artifact->path);
    spn_dag_file_cache_invalidate(files, artifact->path);
  }
}

static spn_err_t spn_dag_exec_plain(spn_dag_t* g, spn_dag_action_t* action, spn_dag_file_cache_t* files, spn_dag_action_cache_t* cache, spn_dag_store_t* store) {
  spn_dag_digest_t key = spn_dag_action_key(g, action->id);

  const spn_dag_action_entry_t* entry = spn_dag_action_cache_get(cache, key);
  if (entry) {
    if (spn_dag_restore(g, action, entry, files, store)) {
      return SPN_OK;
    }
    spn_dag_action_cache_remove(cache, key);
  }

  if (action->execute) {
    spn_dag_clear_outputs(g, action, files);
    if (action->execute(action, action->user_data)) {
      return SPN_ERROR;
    }
  }

  return spn_dag_publish(g, action, key, files, cache, store);
}

static spn_err_t spn_dag_exec_discovery(spn_dag_t* g, spn_dag_action_t* action, spn_dag_file_cache_t* files, spn_dag_action_cache_t* cache, spn_dag_store_t* store, spn_dag_discovery_t* discovery) {
  spn_dag_digest_t prelim = spn_dag_action_key(g, action->id);

  sp_mem_arena_marker_t s = sp_mem_begin_scratch();
  spn_err_t err = SPN_OK;

  spn_dag_pathset_t* set = spn_dag_discovery_get(discovery, prelim);
  if (set) {
    u32 count = (u32)sp_da_size(set->obs);
    if (!spn_dag_resolve_obs(files, set->obs, count)) {
      spn_dag_digest_t key = spn_dag_strong_key(prelim, set->obs, count);
      const spn_dag_action_entry_t* entry = spn_dag_action_cache_get(cache, key);
      if (entry) {
        if (spn_dag_restore(g, action, entry, files, store)) {
          spn_dag_discovery_flush(discovery, prelim);
          goto done;
        }
        spn_dag_action_cache_remove(cache, key);
      }
    }
  }

  if (action->execute) {
    spn_dag_clear_outputs(g, action, files);
    if (action->execute(action, action->user_data)) {
      err = SPN_ERROR;
      goto done;
    }
  }

  sp_da(spn_dag_obs_t) obs = sp_da_new(s.mem, spn_dag_obs_t);
  if (action->discover(action, action->user_data, s.mem, &obs)) {
    err = SPN_ERROR;
    goto done;
  }
  spn_dag_canonicalize(obs);

  u32 count = (u32)sp_da_size(obs);
  bool resolved = !spn_dag_resolve_obs(files, obs, count);
  spn_dag_discovery_put(discovery, prelim, obs, count);
  if (!resolved) {
    goto done;
  }

  spn_dag_digest_t key = spn_dag_strong_key(prelim, obs, count);
  err = spn_dag_publish(g, action, key, files, cache, store);

done:
  sp_mem_end_scratch(s);
  return err;
}

spn_err_t spn_dag_execute(spn_dag_t* g, spn_dag_id_t action_id, spn_dag_file_cache_t* files, spn_dag_action_cache_t* cache, spn_dag_store_t* store) {
  spn_dag_action_t* action = spn_dag_find_action(g, action_id);
  sp_assert(!action->discover);
  return spn_dag_exec_plain(g, action, files, cache, store);
}

spn_err_t spn_dag_execute_discovered(spn_dag_t* g, spn_dag_id_t action_id, spn_dag_file_cache_t* files, spn_dag_action_cache_t* cache, spn_dag_store_t* store, spn_dag_discovery_t* discovery) {
  spn_dag_action_t* action = spn_dag_find_action(g, action_id);
  sp_assert(action->discover);
  sp_assert(discovery);
  return spn_dag_exec_discovery(g, action, files, cache, store, discovery);
}

typedef struct {
  u32 pending;
  u32 deferred;
  bool done;
  sp_da(u32) waiters;
} spn_dag_run_state_t;

static bool spn_dag_run_defer(spn_dag_t* g, spn_dag_action_t* action, spn_dag_discovery_t* discovery, spn_dag_run_state_t* states) {
  const spn_dag_pathset_t* set = spn_dag_discovery_get(discovery, spn_dag_action_key(g, action->id));
  if (!set) {
    return false;
  }

  spn_dag_run_state_t* state = &states[action->id.index];
  sp_da_for(set->obs, it) {
    if (set->obs[it].kind != SPN_DAG_OBS_FILE) {
      continue;
    }
    spn_dag_id_t found = spn_dag_find_file(g, set->obs[it].path);
    if (!found.occupied) {
      continue;
    }
    spn_dag_artifact_t* artifact = spn_dag_find_artifact(g, found);
    if (!artifact->producer.occupied || artifact->producer.index == action->id.index) {
      continue;
    }
    spn_dag_run_state_t* producer = &states[artifact->producer.index];
    if (producer->done) {
      continue;
    }
    sp_da_push(producer->waiters, action->id.index);
    state->deferred++;
  }

  return state->deferred > 0;
}

spn_err_t spn_dag_run(spn_dag_t* g, spn_dag_file_cache_t* files, spn_dag_action_cache_t* cache, spn_dag_store_t* store, spn_dag_discovery_t* discovery) {
  sp_mem_arena_marker_t s = sp_mem_begin_scratch();
  spn_err_t err = SPN_OK;

  sp_da_for(g->artifacts, it) {
    spn_dag_artifact_t* artifact = &g->artifacts[it];
    if (artifact->kind == SPN_DAG_ARTIFACT_KIND_FILE && !artifact->producer.occupied) {
      if (spn_dag_get_file_digest(files, artifact->path, &artifact->digest)) {
        err = SPN_ERROR;
        goto done;
      }
    }
  }

  u64 n = sp_da_size(g->actions);
  spn_dag_run_state_t* states = sp_alloc_n(s.mem, spn_dag_run_state_t, n ? n : 1);
  sp_mem_zero(states, n * sizeof(spn_dag_run_state_t));
  sp_da(spn_dag_id_t) ready = sp_da_new(s.mem, spn_dag_id_t);

  sp_da_for(g->actions, ai) {
    spn_dag_action_t* action = &g->actions[ai];
    sp_da_init(s.mem, states[ai].waiters);
    sp_da_for(action->consumes, ci) {
      if (!spn_dag_digest_valid(spn_dag_find_artifact(g, action->consumes[ci])->digest)) {
        states[ai].pending++;
      }
    }
    if (!states[ai].pending) {
      sp_da_push(ready, action->id);
    }
  }

  u64 completed = 0;
  while (!sp_da_empty(ready)) {
    spn_dag_id_t id = *sp_da_back(ready);
    sp_da_pop(ready);

    spn_dag_action_t* action = spn_dag_find_action(g, id);
    if (action->discover) {
      sp_assert(discovery);
      if (spn_dag_run_defer(g, action, discovery, states)) {
        continue;
      }
      if (spn_dag_exec_discovery(g, action, files, cache, store, discovery)) {
        err = SPN_ERROR;
        goto done;
      }
      if (spn_dag_run_defer(g, action, discovery, states)) {
        continue;
      }
    } else {
      if (spn_dag_exec_plain(g, action, files, cache, store)) {
        err = SPN_ERROR;
        goto done;
      }
    }

    spn_dag_run_state_t* state = &states[id.index];
    state->done = true;
    completed++;

    sp_da_for(action->produces, pi) {
      spn_dag_artifact_t* produced = spn_dag_find_artifact(g, action->produces[pi]);
      sp_da_for(produced->consumers, cj) {
        u32 index = produced->consumers[cj].index;
        if (states[index].pending && !--states[index].pending) {
          sp_da_push(ready, produced->consumers[cj]);
        }
      }
    }

    sp_da_for(state->waiters, wi) {
      u32 index = state->waiters[wi];
      if (states[index].deferred && !--states[index].deferred) {
        sp_da_push(ready, g->actions[index].id);
      }
    }
  }

  if (completed != n) {
    err = SPN_ERROR;
  }

done:
  sp_mem_end_scratch(s);
  return err;
}

spn_err_t spn_cc_deps_parse(sp_mem_t mem, sp_str_t content, sp_da(sp_str_t)* out) {
  occ_parser_t p = sp_zero;
  if (occ_init(&p, content)) {
    return SPN_ERROR;
  }

  sp_str_t prereq = sp_zero;
  while (occ_next(&p, &prereq)) {
    sp_da_push(*out, sp_str_copy(mem, prereq));
  }

  return p.err ? SPN_ERROR : SPN_OK;
}

static void spn_cc_probe_shadows(spn_cc_ctx_t* ctx, sp_str_t prereq, sp_mem_t mem, sp_da(spn_dag_obs_t)* out) {
  sp_da_for(ctx->search_dirs, it) {
    sp_str_t dir = ctx->search_dirs[it];
    if (prereq.len <= dir.len + 1) {
      continue;
    }
    if (!sp_str_starts_with(prereq, dir) || prereq.data[dir.len] != '/') {
      continue;
    }

    sp_str_t suffix = sp_str_sub(prereq, dir.len + 1, prereq.len - dir.len - 1);
    sp_for(shadow, it) {
      sp_da_push(*out, ((spn_dag_obs_t) {
        .kind = SPN_DAG_OBS_ABSENT,
        .path = sp_fs_join_path(mem, ctx->search_dirs[shadow], suffix)
      }));
    }
    return;
  }
}

spn_err_t spn_cc_discover(spn_dag_action_t* action, void* user_data, sp_mem_t mem, sp_da(spn_dag_obs_t)* out) {
  spn_cc_ctx_t* ctx = (spn_cc_ctx_t*)user_data;

  sp_str_t dep_path = sp_zero;
  sp_da_for(action->produces, it) {
    spn_dag_artifact_t* artifact = spn_dag_find_artifact(ctx->g, action->produces[it]);
    if (sp_str_ends_with(artifact->path, sp_str_lit(".d"))) {
      dep_path = artifact->path;
      break;
    }
  }

  if (sp_str_empty(dep_path)) {
    return SPN_ERROR;
  }

  sp_str_t content = sp_zero;
  if (sp_io_read_file(mem, dep_path, &content)) {
    return SPN_ERROR;
  }

  sp_da(sp_str_t) prereqs = sp_da_new(mem, sp_str_t);
  spn_try(spn_cc_deps_parse(mem, content, &prereqs));

  sp_da_for(prereqs, it) {
    sp_da_push(*out, ((spn_dag_obs_t) {
      .kind = SPN_DAG_OBS_FILE,
      .path = prereqs[it]
    }));
    spn_cc_probe_shadows(ctx, prereqs[it], mem, out);
  }

  return SPN_OK;
}

static sp_str_t spn_dag_store_blob_dir(spn_dag_store_t* store, sp_mem_t mem, spn_dag_digest_t digest) {
  return sp_fs_join_path(mem, store->dir, spn_dag_digest_hex(mem, digest));
}

static sp_str_t spn_dag_store_find(spn_dag_store_t* store, sp_mem_t mem, spn_dag_digest_t digest) {
  sp_da(sp_fs_entry_t) entries = sp_fs_collect(mem, spn_dag_store_blob_dir(store, mem, digest));
  sp_da_for(entries, it) {
    if (entries[it].kind == SP_FS_KIND_FILE) {
      return entries[it].path;
    }
  }
  return sp_str_lit("");
}

static spn_err_t spn_dag_store_link(sp_str_t from, sp_str_t to) {
  sp_fs_remove_file(to);
  if (!sp_fs_link(from, to, SP_FS_LINK_HARD)) {
    return SPN_OK;
  }
  return sp_fs_copy(from, to) ? SPN_ERROR : SPN_OK;
}

void spn_dag_store_init(spn_dag_store_t* store, spn_dag_store_config_t config) {
  store->kind = config.kind;
  store->arena = sp_mem_arena_new(config.mem);
  store->mem = sp_mem_arena_as_allocator(store->arena);

  switch (store->kind) {
    case SPN_DAG_STORE_MEM: {
      sp_ht_init(store->mem, store->blobs);
      break;
    }
    case SPN_DAG_STORE_FILESYSTEM: {
      store->dir = sp_str_copy(store->mem, config.dir);
      if (!sp_fs_exists(store->dir)) {
        sp_fs_create_dir(store->dir);
      }
      break;
    }
  }
}

spn_err_t spn_dag_put(spn_dag_store_t* store, const void* data, u64 len, spn_dag_digest_t* digest) {
  *digest = spn_dag_digest(data, len);

  switch (store->kind) {
    case SPN_DAG_STORE_MEM: {
      if (sp_ht_getp(store->blobs, *digest)) {
        return SPN_OK;
      }
      sp_mem_slice_t blob = {
        .data = sp_alloc_n(store->mem, u8, len),
        .len = len
      };
      sp_mem_copy(blob.data, data, len);
      sp_ht_insert(store->blobs, *digest, blob);
      return SPN_OK;
    }
    case SPN_DAG_STORE_FILESYSTEM: {
      sp_mem_arena_marker_t s = sp_mem_begin_scratch();
      spn_err_t err = SPN_OK;
      if (sp_str_empty(spn_dag_store_find(store, s.mem, *digest))) {
        sp_str_t dir = spn_dag_store_blob_dir(store, s.mem, *digest);
        sp_fs_create_dir(dir);
        if (sp_fs_write_atomic_slice(sp_fs_join_path(s.mem, dir, sp_str_lit("blob")), sp_mem_slice((u8*)data, len))) {
          err = SPN_ERROR;
        }
      }
      sp_mem_end_scratch(s);
      return err;
    }
  }

  SP_UNREACHABLE_RETURN(SPN_ERROR);
}

spn_err_t spn_dag_store_put_file(spn_dag_store_t* store, sp_str_t path, spn_dag_digest_t* digest) {
  switch (store->kind) {
    case SPN_DAG_STORE_MEM: {
      sp_mem_arena_marker_t s = sp_mem_begin_scratch();
      sp_mem_slice_t content = sp_zero;
      if (sp_io_read_file_slice(s.mem, path, &content)) {
        sp_mem_end_scratch(s);
        return SPN_ERROR;
      }
      spn_err_t err = spn_dag_put(store, content.data, content.len, digest);
      sp_mem_end_scratch(s);
      return err;
    }
    case SPN_DAG_STORE_FILESYSTEM: {
      if (spn_sha256_file_digest(path, digest->bytes)) {
        return SPN_ERROR;
      }

      sp_mem_arena_marker_t s = sp_mem_begin_scratch();
      spn_err_t err = SPN_OK;
      if (sp_str_empty(spn_dag_store_find(store, s.mem, *digest))) {
        sp_str_t dir = spn_dag_store_blob_dir(store, s.mem, *digest);
        sp_fs_create_dir(dir);
        sp_str_t stored = sp_fs_join_path(s.mem, dir, sp_fs_get_name(path));
        err = spn_dag_store_link(path, stored);
      }
      sp_mem_end_scratch(s);
      return err;
    }
  }

  SP_UNREACHABLE_RETURN(SPN_ERROR);
}

bool spn_dag_store_has(spn_dag_store_t* store, spn_dag_digest_t digest) {
  switch (store->kind) {
    case SPN_DAG_STORE_MEM: {
      return sp_ht_getp(store->blobs, digest) != SP_NULLPTR;
    }
    case SPN_DAG_STORE_FILESYSTEM: {
      sp_mem_arena_marker_t s = sp_mem_begin_scratch();
      bool exists = !sp_str_empty(spn_dag_store_find(store, s.mem, digest));
      sp_mem_end_scratch(s);
      return exists;
    }
  }

  SP_UNREACHABLE_RETURN(false);
}

spn_err_t spn_dag_store_get(spn_dag_store_t* store, spn_dag_digest_t digest, sp_mem_t mem, sp_mem_slice_t* data) {
  switch (store->kind) {
    case SPN_DAG_STORE_MEM: {
      sp_mem_slice_t* blob = sp_ht_getp(store->blobs, digest);
      if (!blob) {
        return SPN_ERROR;
      }
      data->data = sp_alloc_n(mem, u8, blob->len);
      data->len = blob->len;
      sp_mem_copy(data->data, blob->data, blob->len);
      return SPN_OK;
    }
    case SPN_DAG_STORE_FILESYSTEM: {
      sp_mem_arena_marker_t s = sp_mem_begin_scratch();
      sp_str_t stored = spn_dag_store_find(store, s.mem, digest);
      spn_err_t err = SPN_ERROR;
      if (!sp_str_empty(stored)) {
        err = sp_io_read_file_slice(mem, stored, data) ? SPN_ERROR : SPN_OK;
      }
      sp_mem_end_scratch(s);
      return err;
    }
  }

  SP_UNREACHABLE_RETURN(SPN_ERROR);
}

spn_err_t spn_dag_store_materialize(spn_dag_store_t* store, spn_dag_digest_t digest, sp_str_t path) {
  switch (store->kind) {
    case SPN_DAG_STORE_MEM: {
      sp_mem_slice_t* blob = sp_ht_getp(store->blobs, digest);
      if (!blob) {
        return SPN_ERROR;
      }
      return sp_fs_create_file_slice(path, *blob) ? SPN_ERROR : SPN_OK;
    }
    case SPN_DAG_STORE_FILESYSTEM: {
      sp_mem_arena_marker_t s = sp_mem_begin_scratch();
      sp_str_t stored = spn_dag_store_find(store, s.mem, digest);
      spn_err_t err = SPN_ERROR;
      if (!sp_str_empty(stored)) {
        err = spn_dag_store_link(stored, path);
      }
      sp_mem_end_scratch(s);
      return err;
    }
  }

  SP_UNREACHABLE_RETURN(SPN_ERROR);
}
