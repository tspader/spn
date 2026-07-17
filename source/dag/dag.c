#include "dag/dag.h"
#include "dag/types.h"
#include "sha256/sha256.h"
#include "error/types.h"
#include "sp.h"
#include "sp/sp_glob.h"

#define try(expr) spn_try(expr)

#define SPN_DAG_GLOB_DEPTH_MAX 64

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
  sp_assert(id.index < sp_da_size(g->artifacts));
  return g->artifacts + id.index;
}

spn_dag_action_t* spn_dag_find_action(spn_dag_t* g, spn_dag_id_t id) {
  sp_assert(id.occupied);
  sp_assert(id.index < sp_da_size(g->actions));
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

static spn_dag_id_t add_path(spn_dag_t* g, sp_str_t path, spn_dag_artifact_kind_t kind) {
  spn_dag_id_t* existing = sp_ht_getp(g->paths, path);
  if (existing) {
    sp_assert(spn_dag_find_artifact(g, *existing)->kind == kind);
    return *existing;
  }

  sp_str_t copy = sp_str_copy(g->mem, path);
  spn_dag_id_t id = add_artifact(g, (spn_dag_artifact_t) {
    .kind = kind,
    .path = copy,
  });
  sp_ht_insert(g->paths, copy, id);
  return id;
}

spn_dag_id_t spn_dag_add_value(spn_dag_t* g, const void* data, u64 len) {
  return add_artifact(g, (spn_dag_artifact_t) {
    .kind = SPN_DAG_ARTIFACT_KIND_VALUE,
    .digest = spn_dag_digest(data, len),
  });
}

spn_dag_id_t spn_dag_add_file(spn_dag_t* g, sp_str_t path) {
  return add_path(g, path, SPN_DAG_ARTIFACT_KIND_FILE);
}

spn_dag_id_t spn_dag_add_tree(spn_dag_t* g, sp_str_t path) {
  return add_path(g, path, SPN_DAG_ARTIFACT_KIND_TREE);
}

spn_dag_id_t spn_dag_add_output(spn_dag_t* g, sp_str_t name) {
  return add_artifact(g, (spn_dag_artifact_t) {
    .kind = SPN_DAG_ARTIFACT_KIND_FILE,
    .name = sp_str_copy(g->mem, name),
  });
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
    return SPN_ERR_DAG_DUPLICATE_OUTPUT;
  }
  if (sp_str_empty(artifact->name)) {
    artifact->name = sp_fs_get_name(artifact->path);
    artifact->target = artifact->path;
  }
  if (sp_str_empty(artifact->name)) {
    return SPN_ERR_DAG_OUTPUT_NAME;
  }
  if (sp_str_find_c8(artifact->name, '/') != SP_STR_NO_MATCH) {
    return SPN_ERR_DAG_OUTPUT_NAME;
  }
  sp_da_for(action->produces, it) {
    if (sp_str_equal(spn_dag_find_artifact(g, action->produces[it])->name, artifact->name)) {
      return SPN_ERR_DAG_DUPLICATE_OUTPUT;
    }
  }
  artifact->producer = action_id;
  sp_da_push(action->produces, artifact_id);
  return SPN_OK;
}

void spn_dag_hash_bytes(spn_sha256_ctx_t* ctx, const void* data, u64 len) {
  spn_sha256_update(ctx, (const u8*)data, len);
}

void spn_dag_hash_u8(spn_sha256_ctx_t* ctx, u8 value) {
  spn_dag_hash_bytes(ctx, &value, sizeof(value));
}

void spn_dag_hash_u64(spn_sha256_ctx_t* ctx, u64 value) {
  spn_dag_hash_bytes(ctx, &value, sizeof(value));
}

void spn_dag_hash_str(spn_sha256_ctx_t* ctx, sp_str_t str) {
  spn_dag_hash_u64(ctx, str.len);
  spn_dag_hash_bytes(ctx, str.data, str.len);
}

void spn_dag_hash_strs(spn_sha256_ctx_t* ctx, sp_da(sp_str_t) strs) {
  spn_dag_hash_u64(ctx, sp_da_size(strs));
  sp_da_for(strs, it) {
    spn_dag_hash_str(ctx, strs[it]);
  }
}

void spn_dag_hash_digest(spn_sha256_ctx_t* ctx, spn_dag_digest_t digest) {
  spn_dag_hash_bytes(ctx, digest.bytes, sizeof(digest.bytes));
}

spn_dag_digest_t spn_dag_hash_final(spn_sha256_ctx_t* ctx) {
  spn_dag_digest_t digest = sp_zero;
  spn_sha256_final(ctx, digest.bytes);
  return digest;
}

spn_dag_digest_t spn_dag_weak_key(spn_dag_t* g, spn_dag_id_t action_id) {
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
    spn_dag_hash_str(&ctx, artifact->name);
  }

  return spn_dag_hash_final(&ctx);
}

spn_dag_digest_t spn_dag_strong_key(spn_dag_digest_t weak, const spn_dag_obs_t* obs, u32 count) {
  spn_sha256_ctx_t ctx = sp_zero;
  spn_sha256_init(&ctx);
  spn_dag_hash_str(&ctx, sp_str_lit("spn.dag.strong.v3"));
  spn_dag_hash_digest(&ctx, weak);
  spn_dag_hash_u64(&ctx, count);
  sp_for(it, count) {
    spn_dag_hash_u8(&ctx, (u8)obs[it].kind);
    spn_dag_hash_str(&ctx, obs[it].path);
    spn_dag_hash_str(&ctx, obs[it].filter);
    spn_dag_hash_digest(&ctx, obs[it].meta.digest);
  }

  return spn_dag_hash_final(&ctx);
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

static spn_err_t glob_visit(spn_dag_glob_walk_t* w, sp_str_t dir, u32 depth) {
  if (depth > SPN_DAG_GLOB_DEPTH_MAX) {
    return SPN_ERR_DAG_GLOB;
  }
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
        try(glob_visit(w, entry->path, depth + 1));
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
  return SPN_OK;
}

spn_err_t spn_dag_glob(sp_mem_t mem, sp_str_t root, sp_str_t pattern, sp_da(spn_dag_obs_t)* obs, sp_da(spn_dag_match_t)* matches) {
  sp_glob_t* glob = sp_glob_new_str(mem, pattern);
  if (!glob) {
    return SPN_ERR_DAG_GLOB;
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
  try(glob_visit(&walk, sp_str_empty(prefix) ? root : sp_fs_join_path(mem, root, prefix), 0));

  if (matches) {
    sp_da_sort(*matches, compare_matches);
  }
  return SPN_OK;
}
