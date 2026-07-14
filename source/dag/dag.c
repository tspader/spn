#include "dag/dag.h"
#include "sha256/sha256.h"
#include "error/types.h"
#include "sp.h"
#include "sp/io.h"
#include "sp/atomic_file.h"
#include "sp/sp_glob.h"

spn_dag_t* spn_dag_new(sp_mem_t mem) {
  sp_mem_arena_t* arena = sp_mem_arena_new(mem);
  sp_mem_t a = sp_mem_arena_as_allocator(arena);
  spn_dag_t* g = sp_mem_allocator_alloc_type(a, spn_dag_t);
  g->arena = arena;
  g->mem = a;
  sp_da_init(a, g->artifacts);
  sp_da_init(a, g->actions);
  sp_str_ht_init(a, g->paths);
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

static spn_dag_id_t add_artifact(spn_dag_t* g, spn_dag_artifact_t artifact) {
  artifact.id = (spn_dag_id_t) {
    .index = (u32)sp_da_size(g->artifacts),
    .occupied = true
  };
  sp_da_init(g->mem, artifact.consumers);
  sp_da_push(g->artifacts, artifact);
  return artifact.id;
}

spn_dag_id_t spn_dag_add_value(spn_dag_t* g, const void* data, u64 len) {
  return add_artifact(g, (spn_dag_artifact_t) {
    .kind = SPN_DAG_ARTIFACT_KIND_VALUE,
    .digest = spn_dag_digest(data, len),
  });
}

spn_dag_id_t spn_dag_find_file(spn_dag_t* g, sp_str_t path) {
  spn_dag_id_t* existing = sp_ht_getp(g->paths, path);
  return existing ? *existing : (spn_dag_id_t) sp_zero;
}

spn_dag_id_t spn_dag_add_file(spn_dag_t* g, sp_str_t path) {
  spn_dag_id_t existing = spn_dag_find_file(g, path);
  if (existing.occupied) {
    sp_assert(spn_dag_find_artifact(g, existing)->kind == SPN_DAG_ARTIFACT_KIND_FILE);
    return existing;
  }

  sp_str_t copy = sp_str_copy(g->mem, path);
  spn_dag_id_t id = add_artifact(g, (spn_dag_artifact_t) {
    .kind = SPN_DAG_ARTIFACT_KIND_FILE,
    .path = copy,
  });
  sp_ht_insert(g->paths, copy, id);
  return id;
}

spn_dag_id_t spn_dag_add_output(spn_dag_t* g, sp_str_t name) {
  return add_artifact(g, (spn_dag_artifact_t) {
    .kind = SPN_DAG_ARTIFACT_KIND_FILE,
    .name = sp_str_copy(g->mem, name),
  });
}

spn_dag_id_t spn_dag_add_tree(spn_dag_t* g, sp_str_t path) {
  spn_dag_id_t existing = spn_dag_find_file(g, path);
  if (existing.occupied) {
    sp_assert(spn_dag_find_artifact(g, existing)->kind == SPN_DAG_ARTIFACT_KIND_TREE);
    return existing;
  }

  sp_str_t copy = sp_str_copy(g->mem, path);
  spn_dag_id_t id = add_artifact(g, (spn_dag_artifact_t) {
    .kind = SPN_DAG_ARTIFACT_KIND_TREE,
    .path = copy,
  });
  sp_ht_insert(g->paths, copy, id);
  return id;
}

static bool has_recursive_token(sp_glob_t* glob) {
  sp_da_for(glob->tokens, it) {
    switch (glob->tokens[it].type) {
      case SP_GLOB_TOK_RECURSIVE_PREFIX:
      case SP_GLOB_TOK_RECURSIVE_SUFFIX:
      case SP_GLOB_TOK_RECURSIVE_ZERO_OR_MORE: {
        return true;
      }
      default: {
        continue;
      }
    }
  }
  return false;
}

static sp_str_t get_literal_dir(sp_glob_t* glob) {
  u32 cut = 0;
  sp_da_for(glob->tokens, it) {
    sp_glob_token_t* token = &glob->tokens[it];
    if (token->type != SP_GLOB_TOK_LITERAL) {
      break;
    }
    if (token->literal == '/') {
      cut = (u32)it;
    }
  }
  return sp_str_sub(glob->pattern, 0, cut);
}

static bool is_match_all(sp_glob_t* glob) {
  sp_da_for(glob->tokens, it) {
    switch (glob->tokens[it].type) {
      case SP_GLOB_TOK_ZERO_OR_MORE:
      case SP_GLOB_TOK_RECURSIVE_PREFIX:
      case SP_GLOB_TOK_RECURSIVE_SUFFIX:
      case SP_GLOB_TOK_RECURSIVE_ZERO_OR_MORE: {
        continue;
      }
      default: {
        return false;
      }
    }
  }
  return true;
}

static sp_str_t get_glob_filter(sp_mem_t mem, sp_str_t pattern) {
  sp_str_t segment = sp_fs_get_name(pattern);
  sp_glob_t* glob = sp_glob_new_str(mem, segment);
  if (!glob || is_match_all(glob)) {
    return sp_str_lit("");
  }
  return segment;
}

static s32 compare_matches(const void* a, const void* b) {
  return sp_str_compare_alphabetical(((const spn_dag_match_t*)a)->relative, ((const spn_dag_match_t*)b)->relative);
}

typedef struct {
  sp_mem_t mem;
  sp_str_t root;
  sp_glob_t* glob;
  sp_str_t filter;
  bool recursive;
  sp_da(spn_dag_obs_t)* obs;
  sp_da(spn_dag_match_t)* matches;
} spn_dag_glob_walk_t;

static void spn_dag_glob_visit(spn_dag_glob_walk_t* w, sp_str_t dir) {
  if (w->obs) {
    sp_da_push(*w->obs, ((spn_dag_obs_t) {
      .kind = SPN_DAG_OBS_ENUMERATION,
      .path = dir,
      .filter = w->filter
    }));
  }

  sp_da(sp_fs_entry_t) entries = sp_fs_collect(w->mem, dir);
  sp_da_for(entries, it) {
    sp_fs_entry_t* entry = &entries[it];
    if (entry->kind == SP_FS_KIND_DIR) {
      if (w->recursive) {
        spn_dag_glob_visit(w, entry->path);
      }
      continue;
    }

    sp_str_t relative = sp_str_strip_left(sp_str_strip_left(entry->path, w->root), sp_str_lit("/"));
    if (!sp_glob_match(w->glob, relative)) {
      continue;
    }
    if (w->obs) {
      sp_da_push(*w->obs, ((spn_dag_obs_t) {
        .kind = SPN_DAG_OBS_FILE,
        .path = entry->path
      }));
    }
    if (w->matches) {
      sp_da_push(*w->matches, ((spn_dag_match_t) {
        .path = entry->path,
        .relative = relative
      }));
    }
  }
}

spn_err_t spn_dag_glob(sp_mem_t mem, sp_str_t root, sp_str_t pattern, sp_da(spn_dag_obs_t)* obs, sp_da(spn_dag_match_t)* matches) {
  sp_glob_t* glob = sp_glob_new_str(mem, pattern);
  if (!glob) {
    return SPN_ERROR;
  }

  if (glob->strategy == SP_GLOB_STRATEGY_LITERAL) {
    sp_str_t path = sp_fs_join_path(mem, root, pattern);
    if (sp_fs_is_file(path)) {
      if (obs) {
        sp_da_push(*obs, ((spn_dag_obs_t) { .kind = SPN_DAG_OBS_FILE, .path = path }));
      }
      if (matches) {
        sp_da_push(*matches, ((spn_dag_match_t) { .path = path, .relative = pattern }));
      }
    } else if (obs) {
      sp_da_push(*obs, ((spn_dag_obs_t) { .kind = SPN_DAG_OBS_ABSENT, .path = path }));
    }
    return SPN_OK;
  }

  sp_str_t prefix = get_literal_dir(glob);
  sp_str_t remainder = sp_str_sub(pattern, prefix.len, pattern.len - prefix.len);
  remainder = sp_str_strip_left(remainder, sp_str_lit("/"));

  spn_dag_glob_walk_t walk = {
    .mem = mem,
    .root = root,
    .glob = glob,
    .filter = get_glob_filter(mem, pattern),
    .recursive = sp_str_contains(remainder, sp_str_lit("/")) || has_recursive_token(glob),
    .obs = obs,
    .matches = matches
  };
  spn_dag_glob_visit(&walk, sp_str_empty(prefix) ? root : sp_fs_join_path(mem, root, prefix));

  if (matches) {
    sp_da_sort(*matches, compare_matches);
  }
  return SPN_OK;
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
  if (sp_str_empty(artifact->name)) {
    artifact->name = sp_fs_get_name(artifact->path);
    artifact->target = artifact->path;
  }
  if (sp_str_empty(artifact->name) || sp_str_find_c8(artifact->name, '/') != SP_STR_NO_MATCH) {
    return SPN_ERROR;
  }
  sp_da_for(action->produces, it) {
    if (sp_str_equal(spn_dag_find_artifact(g, action->produces[it])->name, artifact->name)) {
      return SPN_ERROR;
    }
  }
  artifact->producer = action_id;
  sp_da_push(action->produces, artifact_id);
  return SPN_OK;
}

static void hash_bytes(spn_sha256_ctx_t* ctx, const void* data, u64 len) {
  spn_sha256_update(ctx, (const u8*)data, len);
}

static void hash_u8(spn_sha256_ctx_t* ctx, u8 value) {
  hash_bytes(ctx, &value, sizeof(value));
}

static void hash_u64(spn_sha256_ctx_t* ctx, u64 value) {
  hash_bytes(ctx, &value, sizeof(value));
}

static void hash_str(spn_sha256_ctx_t* ctx, sp_str_t str) {
  hash_u64(ctx, str.len);
  hash_bytes(ctx, str.data, str.len);
}

static void hash_digest(spn_sha256_ctx_t* ctx, spn_dag_digest_t digest) {
  hash_bytes(ctx, digest.bytes, sizeof(digest.bytes));
}

spn_dag_digest_t spn_dag_action_key(spn_dag_t* g, spn_dag_id_t action_id) {
  spn_dag_action_t* action = spn_dag_find_action(g, action_id);

  spn_sha256_ctx_t ctx = sp_zero;
  spn_sha256_init(&ctx);
  hash_str(&ctx, sp_str_lit("spn.dag.action.v2"));
  hash_digest(&ctx, action->identity);

  hash_u64(&ctx, sp_da_size(action->consumes));
  sp_da_for(action->consumes, it) {
    spn_dag_artifact_t* artifact = spn_dag_find_artifact(g, action->consumes[it]);
    sp_assert(spn_dag_digest_valid(artifact->digest));
    hash_u8(&ctx, (u8)artifact->kind);
    hash_digest(&ctx, artifact->digest);
  }

  hash_u64(&ctx, sp_da_size(action->produces));
  sp_da_for(action->produces, it) {
    spn_dag_artifact_t* artifact = spn_dag_find_artifact(g, action->produces[it]);
    hash_u8(&ctx, (u8)artifact->kind);
    hash_str(&ctx, artifact->name);
  }

  spn_dag_digest_t key = sp_zero;
  spn_sha256_final(&ctx, key.bytes);
  return key;
}

spn_dag_digest_t spn_dag_strong_key(spn_dag_digest_t prelim, const spn_dag_obs_t* obs, u32 count) {
  spn_sha256_ctx_t ctx = sp_zero;
  spn_sha256_init(&ctx);
  hash_str(&ctx, sp_str_lit("spn.dag.strong.v3"));
  hash_digest(&ctx, prelim);
  hash_u64(&ctx, count);
  sp_for(it, count) {
    hash_u8(&ctx, (u8)obs[it].kind);
    hash_str(&ctx, obs[it].path);
    hash_str(&ctx, obs[it].filter);
    hash_digest(&ctx, obs[it].meta.digest);
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

static bool is_path_within(sp_str_t path, sp_str_t dir) {
  if (path.len <= dir.len + 1 || !sp_str_starts_with(path, dir)) {
    return false;
  }
  return path.data[dir.len] == '/';
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

static bool is_timespec_equal(sp_sys_timespec_t a, sp_sys_timespec_t b) {
  return a.tv_sec == b.tv_sec && a.tv_nsec == b.tv_nsec;
}

static bool is_file_clean(spn_dag_file_meta_t* cached, sp_sys_file_meta_t sys) {
  if (!cached) return false;
  if (!is_timespec_equal(sys.mtime, cached->mtime)) return false;
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

static spn_err_t settle_tree(spn_dag_artifact_t* artifact, spn_dag_env_t* env) {
  spn_dag_file_cache_invalidate_dir(env->files, artifact->target);
  return spn_dag_store_materialize_tree(env->store, artifact->digest, artifact->target);
}

static spn_err_t settle(spn_dag_t* g, spn_dag_action_t* action, spn_dag_env_t* env) {
  sp_da_for(action->produces, it) {
    spn_dag_artifact_t* artifact = spn_dag_find_artifact(g, action->produces[it]);
    if (artifact->kind == SPN_DAG_ARTIFACT_KIND_TREE) {
      spn_try(settle_tree(artifact, env));
      artifact->path = artifact->target;
      continue;
    }
    if (!sp_str_empty(artifact->target)) {
      if (spn_dag_store_materialize(env->store, artifact->digest, artifact->target)) {
        return SPN_ERROR;
      }
      spn_dag_file_cache_invalidate(env->files, artifact->target);
      artifact->path = artifact->target;
    } else {
      artifact->path = spn_dag_store_path(env->store, g->mem, artifact->digest);
    }
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
      : spn_dag_store_has(env->store, entry->outputs[it].digest);
    if (!present) {
      return false;
    }
  }

  sp_da_for(entry->outputs, it) {
    spn_dag_find_artifact(g, action->produces[it])->digest = entry->outputs[it].digest;
  }

  return !settle(g, action, env);
}

static s32 spn_dag_obs_sort_kernel(const void* a, const void* b) {
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

static bool spn_dag_obs_equal(const spn_dag_obs_t* a, const spn_dag_obs_t* b) {
  return a->kind == b->kind && sp_str_equal(a->path, b->path) && sp_str_equal(a->filter, b->filter);
}

static void spn_dag_canonicalize(sp_da(spn_dag_obs_t) obs) {
  if (sp_da_empty(obs)) {
    return;
  }
  sp_da_sort(obs, spn_dag_obs_sort_kernel);
  u64 w = 1;
  for (u64 r = 1; r < sp_da_size(obs); r++) {
    if (!spn_dag_obs_equal(&obs[r], &obs[w - 1])) {
      obs[w++] = obs[r];
    }
  }
  sp_da_head(obs)->size = w;
}

static bool is_file_meta_current(spn_dag_file_meta_t meta, sp_sys_file_meta_t sys) {
  if (meta.id.device != sys.device || meta.id.id != sys.id) return false;
  if (!is_timespec_equal(meta.mtime, sys.mtime)) return false;
  if (meta.size != sys.size) return false;
  return spn_dag_digest_valid(meta.digest);
}

static s32 sort_member_kernel(const void* a, const void* b) {
  const sp_fs_entry_t* ea = (const sp_fs_entry_t*)a;
  const sp_fs_entry_t* eb = (const sp_fs_entry_t*)b;
  return sp_str_compare_alphabetical(ea->name, eb->name);
}

static spn_err_t spn_dag_membership(sp_str_t dir, sp_str_t filter, spn_dag_digest_t* digest) {
  sp_mem_arena_marker_t s = sp_mem_begin_scratch();
  spn_err_t err = SPN_ERROR;

  sp_glob_t* glob = SP_NULLPTR;
  if (!sp_str_empty(filter)) {
    glob = sp_glob_new_str(s.mem, filter);
    if (!glob) {
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
  sp_da_sort(members, sort_member_kernel);

  spn_sha256_ctx_t ctx = sp_zero;
  spn_sha256_init(&ctx);
  hash_str(&ctx, sp_str_lit("spn.dag.enum.v1"));
  hash_u64(&ctx, sp_da_size(members));
  sp_da_for(members, it) {
    hash_str(&ctx, members[it].name);
    hash_u8(&ctx, (u8)members[it].kind);
  }
  spn_sha256_final(&ctx, digest->bytes);
  err = SPN_OK;

done:
  sp_mem_end_scratch(s);
  return err;
}

static spn_err_t resolve_observation(spn_dag_file_cache_t* files, spn_dag_obs_t* obs, u32 count) {
  sp_for(it, count) {
    spn_dag_obs_t* o = &obs[it];
    if (o->kind == SPN_DAG_OBS_ENUMERATION) {
      o->meta = (spn_dag_file_meta_t) sp_zero;
      spn_try(spn_dag_membership(o->path, o->filter, &o->meta.digest));
      continue;
    }
    if (o->kind == SPN_DAG_OBS_ABSENT && !sp_fs_exists(o->path)) {
      o->meta = (spn_dag_file_meta_t) sp_zero;
      continue;
    }
    if (sp_fs_is_dir(o->path)) {
      o->meta = (spn_dag_file_meta_t) sp_zero;
      spn_try(spn_dag_membership(o->path, sp_str_lit(""), &o->meta.digest));
      continue;
    }

    sp_sys_file_meta_t sys = sp_zero;
    spn_try(spn_dag_get_file_meta(files, o->path, &sys));

    if (is_file_meta_current(o->meta, sys)) {
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

static sp_atomic_s32_t spn_dag_scratch_sequence;

static sp_str_t begin_scratch(spn_dag_t* g, spn_dag_action_t* action, sp_str_t root) {
  sp_assert(!sp_str_empty(root));

  sp_tm_epoch_t now = sp_tm_now_epoch();
  u64 stamp = ((u64)now.s << 20) ^ (u64)now.ns;
  u64 sequence = (u64)(u32)sp_atomic_s32_add(&spn_dag_scratch_sequence, 1);
  sp_str_t name = sp_fmt(g->mem, "{}.{}", sp_fmt_uint(stamp), sp_fmt_uint(sequence)).value;
  sp_str_t dir = sp_fs_join_path(g->mem, root, name);
  if (sp_fs_create_dir(dir)) {
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
} spn_dag_work_t;

static bool try_restore(spn_dag_t* g, spn_dag_action_t* action, spn_dag_digest_t key, spn_dag_env_t* env) {
  const spn_dag_action_entry_t* entry = spn_dag_action_cache_get(env->cache, key);
  if (!entry) {
    return false;
  }
  if (restore_entry(g, action, entry, env)) {
    return true;
  }
  spn_dag_action_cache_remove(env->cache, key);
  return false;
}

static spn_err_t lookup(spn_dag_t* g, spn_dag_action_t* action, spn_dag_env_t* env, sp_mem_t mem, spn_dag_work_t* work) {
  work->action = action;
  work->mem = mem;
  work->key = spn_dag_action_key(g, action->id);
  sp_da_init(mem, work->digests);
  sp_da_init(mem, work->obs);

  if (action->discover) {
    spn_dag_pathset_t* set = spn_dag_discovery_get(env->discovery, work->key);
    if (set) {
      u32 count = (u32)sp_da_size(set->obs);
      if (!resolve_observation(env->files, set->obs, count)) {
        if (try_restore(g, action, spn_dag_strong_key(work->key, set->obs, count), env)) {
          spn_dag_discovery_flush(env->discovery, work->key);
          work->hit = true;
          return SPN_OK;
        }
      }
    }
  } else if (try_restore(g, action, work->key, env)) {
    work->hit = true;
    return SPN_OK;
  }

  work->scratch = begin_scratch(g, action, env->scratch);
  return sp_str_empty(work->scratch) ? SPN_ERROR : SPN_OK;
}

static spn_err_t execute(spn_dag_t* g, spn_dag_work_t* work, spn_dag_env_t* env) {
  spn_dag_action_t* action = work->action;

  if (action->execute && action->execute(action, action->user_data)) {
    return SPN_ERROR;
  }
  if (action->discover) {
    spn_try(action->discover(action, action->user_data, work->mem, &work->obs));
    spn_dag_canonicalize(work->obs);
  }

  sp_da_for(action->produces, it) {
    spn_dag_artifact_t* artifact = spn_dag_find_artifact(g, action->produces[it]);
    if (!sp_fs_exists(artifact->path)) {
      return SPN_ERROR;
    }
    spn_dag_digest_t digest = sp_zero;
    spn_err_t put = artifact->kind == SPN_DAG_ARTIFACT_KIND_TREE
      ? spn_dag_store_put_tree(env->store, artifact->path, &digest)
      : spn_dag_store_put_file(env->store, artifact->path, &digest);
    spn_try(put);
    sp_da_push(work->digests, digest);
  }

  return SPN_OK;
}

static spn_err_t commit(spn_dag_t* g, spn_dag_work_t* work, spn_dag_env_t* env) {
  spn_dag_action_t* action = work->action;

  sp_da_for(action->produces, it) {
    spn_dag_find_artifact(g, action->produces[it])->digest = work->digests[it];
  }

  spn_dag_digest_t key = work->key;
  bool resolved = true;
  if (action->discover) {
    u32 count = (u32)sp_da_size(work->obs);
    resolved = !resolve_observation(env->files, work->obs, count);
    spn_dag_discovery_put(env->discovery, work->key, work->obs, count);
    if (resolved) {
      key = spn_dag_strong_key(work->key, work->obs, count);
    }
  }

  spn_try(settle(g, action, env));
  if (resolved) {
    record(g, action, key, env);
  }
  return SPN_OK;
}

static spn_err_t exec_action(spn_dag_t* g, spn_dag_action_t* action, spn_dag_env_t* env) {
  sp_mem_arena_marker_t s = sp_mem_begin_scratch();

  spn_dag_work_t work = sp_zero;
  spn_err_t err = lookup(g, action, env, s.mem, &work);
  if (!err && !work.hit) {
    err = execute(g, &work, env);
    if (!err) {
      err = commit(g, &work, env);
    }
    end_scratch(work.scratch);
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
  u32 pending;
  u32 deferred;
  bool done;
  sp_da(u32) waiters;
} spn_dag_run_state_t;

static bool is_artifact_covered(const spn_dag_obs_t* obs, spn_dag_artifact_t* artifact) {
  if (sp_str_empty(artifact->target)) {
    return false;
  }
  if (sp_str_equal(obs->path, artifact->target)) {
    return true;
  }
  if (artifact->kind == SPN_DAG_ARTIFACT_KIND_TREE && is_path_within(obs->path, artifact->target)) {
    return true;
  }
  return obs->kind == SPN_DAG_OBS_ENUMERATION && is_path_within(artifact->target, obs->path);
}

static bool spn_dag_run_defer(spn_dag_t* g, spn_dag_action_t* action, spn_dag_discovery_t* discovery, spn_dag_run_state_t* states) {
  const spn_dag_pathset_t* set = spn_dag_discovery_get(discovery, spn_dag_action_key(g, action->id));
  if (!set) {
    return false;
  }

  spn_dag_run_state_t* state = &states[action->id.index];
  sp_da_for(set->obs, it) {
    sp_da_for(g->artifacts, ai) {
      spn_dag_artifact_t* artifact = &g->artifacts[ai];
      if (!artifact->producer.occupied || artifact->producer.index == action->id.index) {
        continue;
      }
      if (!is_artifact_covered(&set->obs[it], artifact)) {
        continue;
      }
      spn_dag_run_state_t* producer = &states[artifact->producer.index];
      if (producer->done) {
        continue;
      }
      sp_da_push(producer->waiters, action->id.index);
      state->deferred++;
    }
  }

  return state->deferred > 0;
}

spn_err_t spn_dag_run(spn_dag_t* g, spn_dag_env_t* env) {
  sp_mem_arena_marker_t s = sp_mem_begin_scratch();
  spn_err_t err = SPN_OK;

  sp_da_for(g->artifacts, it) {
    spn_dag_artifact_t* artifact = &g->artifacts[it];
    if (artifact->kind == SPN_DAG_ARTIFACT_KIND_TREE) {
      sp_assert(artifact->producer.occupied);
      continue;
    }
    if (artifact->kind == SPN_DAG_ARTIFACT_KIND_FILE && !artifact->producer.occupied) {
      if (spn_dag_get_file_digest(env->files, artifact->path, &artifact->digest)) {
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
      sp_assert(env->discovery);
      if (spn_dag_run_defer(g, action, env->discovery, states)) {
        continue;
      }
      if (exec_action(g, action, env)) {
        err = SPN_ERROR;
        goto done;
      }
      if (spn_dag_run_defer(g, action, env->discovery, states)) {
        continue;
      }
    } else {
      if (exec_action(g, action, env)) {
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

static sp_str_t get_blob_dir(spn_dag_store_t* store, sp_mem_t mem, spn_dag_digest_t digest) {
  return sp_fs_join_path(mem, store->dir, spn_dag_digest_hex(mem, digest));
}

static sp_str_t get_blob_path(spn_dag_store_t* store, sp_mem_t mem, spn_dag_digest_t digest) {
  sp_da(sp_fs_entry_t) entries = sp_fs_collect(mem, get_blob_dir(store, mem, digest));
  sp_da_for(entries, it) {
    if (entries[it].kind == SP_FS_KIND_FILE) {
      return entries[it].path;
    }
  }
  return sp_str_lit("");
}

static spn_err_t link_from_store(sp_str_t from, sp_str_t to) {
  sp_fs_create_dir(sp_fs_parent_path(to));
  sp_fs_remove_file(to);
  if (!sp_fs_link(from, to, SP_FS_LINK_HARD)) {
    return SPN_OK;
  }
  sp_fs_copy(from, to);
  return sp_fs_is_file(to) ? SPN_OK : SPN_ERROR;
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
      if (sp_str_empty(get_blob_path(store, s.mem, *digest))) {
        sp_str_t dir = get_blob_dir(store, s.mem, *digest);
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
      if (sp_str_empty(get_blob_path(store, s.mem, *digest))) {
        sp_str_t dir = get_blob_dir(store, s.mem, *digest);
        sp_fs_create_dir(dir);
        sp_str_t stored = sp_fs_join_path(s.mem, dir, sp_fs_get_name(path));
        err = link_from_store(path, stored);
      }
      sp_mem_end_scratch(s);
      return err;
    }
  }

  SP_UNREACHABLE_RETURN(SPN_ERROR);
}

sp_str_t spn_dag_store_path(spn_dag_store_t* store, sp_mem_t mem, spn_dag_digest_t digest) {
  switch (store->kind) {
    case SPN_DAG_STORE_MEM: {
      return sp_str_lit("");
    }
    case SPN_DAG_STORE_FILESYSTEM: {
      return get_blob_path(store, mem, digest);
    }
  }

  SP_UNREACHABLE_RETURN(sp_str_lit(""));
}

bool spn_dag_store_has(spn_dag_store_t* store, spn_dag_digest_t digest) {
  switch (store->kind) {
    case SPN_DAG_STORE_MEM: {
      return sp_ht_getp(store->blobs, digest) != SP_NULLPTR;
    }
    case SPN_DAG_STORE_FILESYSTEM: {
      sp_mem_arena_marker_t s = sp_mem_begin_scratch();
      bool exists = !sp_str_empty(get_blob_path(store, s.mem, digest));
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
      sp_str_t stored = get_blob_path(store, s.mem, digest);
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
      sp_fs_create_dir(sp_fs_parent_path(path));
      return sp_fs_create_file_slice(path, *blob) ? SPN_ERROR : SPN_OK;
    }
    case SPN_DAG_STORE_FILESYSTEM: {
      sp_mem_arena_marker_t s = sp_mem_begin_scratch();
      sp_str_t stored = get_blob_path(store, s.mem, digest);
      spn_err_t err = SPN_ERROR;
      if (!sp_str_empty(stored)) {
        err = link_from_store(stored, path);
      }
      sp_mem_end_scratch(s);
      return err;
    }
  }

  SP_UNREACHABLE_RETURN(SPN_ERROR);
}
