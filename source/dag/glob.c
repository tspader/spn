#include "dag/dag.h"
#include "dag/types.h"
#include "error/types.h"
#include "sp.h"
#include "sp/sp_glob.h"

#define try(expr) spn_try(expr)

#define SPN_DAG_GLOB_DEPTH_MAX 64

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

typedef struct {
  sp_str_t dir;
  u32 depth;
} spn_dag_glob_dir_t;

static spn_err_t glob_walk(spn_dag_glob_walk_t* w, sp_str_t start) {
  sp_da(spn_dag_glob_dir_t) pending = sp_da_new(w->mem, spn_dag_glob_dir_t);
  sp_da_push(pending, ((spn_dag_glob_dir_t) { .dir = start }));

  for (u64 dt = 0; dt < sp_da_size(pending); dt++) {
    spn_dag_glob_dir_t visit = pending[dt];
    if (visit.depth > SPN_DAG_GLOB_DEPTH_MAX) {
      return SPN_ERR_DAG_GLOB;
    }
    if (w->obs) {
      sp_da_push(*w->obs, ((spn_dag_obs_t) {
        .kind = SPN_DAG_OBS_ENUMERATION,
        .path = visit.dir,
        .filter = w->filter
      }));
    }

    sp_da(sp_fs_entry_t) entries = sp_fs_collect(w->mem, visit.dir);
    sp_da_for(entries, it) {
      sp_fs_entry_t* entry = &entries[it];
      if (entry->kind == SP_FS_KIND_DIR) {
        if (w->recursive) {
          sp_da_push(pending, ((spn_dag_glob_dir_t) { .dir = entry->path, .depth = visit.depth + 1 }));
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
  try(glob_walk(&walk, sp_str_empty(prefix) ? root : sp_fs_join_path(mem, root, prefix)));

  if (matches) {
    sp_da_sort(*matches, compare_matches);
  }
  return SPN_OK;
}
