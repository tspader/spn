#include "dag/dag.h"
#include "dag/types.h"
#include "paths/paths.h"
#include "sp.h"
#include "spn/core.h"
#include "sp/sp_glob.h"


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
  return sp_str_compare_alphabetical(((const spn_path_t*)a)->sub, ((const spn_path_t*)b)->sub);
}

typedef struct {
  spn_path_t path;
  u32 depth;
} spn_dag_glob_dir_t;

typedef struct {
  sp_mem_t mem;
  const spn_path_roots_t* roots;
  sp_glob_t* glob;
  sp_str_t filter;
  bool recursive;
  sp_da(spn_dag_obs_t)* obs;
  sp_da(spn_path_t)* matches;
} spn_dag_glob_walk_t;

static sp_str_t walk_str(spn_dag_glob_walk_t* w, spn_path_t path) {
  return spn_path_str(w->roots, w->mem, path);
}

static void walk_observe(spn_dag_glob_walk_t* w, spn_dag_obs_kind_t kind, spn_path_t path, sp_str_t filter) {
  sp_da_push(*w->obs, ((spn_dag_obs_t) {
    .kind = kind,
    .path = path,
    .filter = filter
  }));
}

static spn_err_t glob_walk(spn_dag_glob_walk_t* w, spn_dag_glob_dir_t start) {
  sp_da(spn_dag_glob_dir_t) pending = sp_da_new(w->mem, spn_dag_glob_dir_t);
  sp_da_push(pending, start);

  for (u64 dt = 0; dt < sp_da_size(pending); dt++) {
    spn_dag_glob_dir_t visit = pending[dt];
    if (visit.depth > SPN_DAG_GLOB_DEPTH_MAX) {
      return SPN_ERR_DAG_GLOB;
    }
    walk_observe(w, SPN_DAG_OBS_ENUMERATION, visit.path, w->filter);

    sp_da(sp_fs_entry_t) entries = sp_zero;
    sp_fs_collect(w->mem, walk_str(w, visit.path), &entries);
    sp_da_for(entries, it) {
      sp_fs_entry_t* entry = &entries[it];
      if (entry->kind == SP_FS_KIND_DIR) {
        if (w->recursive) {
          sp_da_push(pending, ((spn_dag_glob_dir_t) {
            .path = spn_path_join(w->mem, visit.path, entry->name),
            .depth = visit.depth + 1
          }));
        }
        continue;
      }

      spn_path_t path = spn_path_join(w->mem, visit.path, entry->name);
      if (!sp_glob_match(w->glob, path.sub)) {
        continue;
      }
      walk_observe(w, SPN_DAG_OBS_FILE, path, sp_str_lit(""));
      sp_da_push(*w->matches, path);
    }
  }
  return SPN_OK;
}

static spn_err_t glob_run(spn_dag_glob_walk_t* w, spn_path_t pattern) {
  w->glob = sp_glob_new_str(w->mem, pattern.sub);
  if (!w->glob) {
    return SPN_ERR_DAG_GLOB;
  }

  if (w->glob->strategy == SP_GLOB_STRATEGY_LITERAL) {
    if (sp_fs_is_file(walk_str(w, pattern))) {
      walk_observe(w, SPN_DAG_OBS_FILE, pattern, sp_str_lit(""));
      sp_da_push(*w->matches, pattern);
    } else {
      walk_observe(w, SPN_DAG_OBS_ABSENT, pattern, sp_str_lit(""));
    }
    return SPN_OK;
  }

  sp_str_t prefix = get_literal_dir(w->glob);
  sp_str_t remainder = sp_str_sub(pattern.sub, prefix.len, pattern.sub.len - prefix.len);
  remainder = sp_str_strip_left(remainder, sp_str_lit("/"));

  w->filter = get_glob_filter(w->mem, pattern.sub);
  w->recursive = sp_str_contains(remainder, sp_str_lit("/")) || has_recursive_token(w->glob);

  spn_try(glob_walk(w, (spn_dag_glob_dir_t) {
    .path = { .root = pattern.root, .sub = prefix }
  }));

  sp_da_sort(*w->matches, compare_matches);
  return SPN_OK;
}

spn_err_t spn_dag_glob(sp_mem_t mem, const spn_path_roots_t* roots, spn_path_t pattern, sp_da(spn_dag_obs_t)* obs, sp_da(spn_path_t)* matches) {
  sp_da(spn_dag_obs_t) discarded_obs;
  if (!obs) {
    discarded_obs = sp_da_new(mem, spn_dag_obs_t);
    obs = &discarded_obs;
  }
  sp_da(spn_path_t) discarded_matches;
  if (!matches) {
    discarded_matches = sp_da_new(mem, spn_path_t);
    matches = &discarded_matches;
  }
  spn_dag_glob_walk_t walk = {
    .mem = mem,
    .roots = roots,
    .obs = obs,
    .matches = matches
  };
  return glob_run(&walk, pattern);
}
