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
  return sp_str_compare_alphabetical(((const spn_dag_glob_match_t*)a)->path.sub, ((const spn_dag_glob_match_t*)b)->path.sub);
}

typedef struct {
  spn_path_t path;
  u32 depth;
} spn_dag_glob_dir_t;

typedef struct {
  sp_mem_t mem;
  const spn_path_roots_t* roots;
  sp_glob_t* glob;
  sp_str_t prefix;
  sp_str_t filter;
  bool recursive;
  spn_dag_glob_result_t* result;
} spn_dag_glob_walk_t;

static void walk_match(spn_dag_glob_walk_t* w, spn_path_t path) {
  sp_str_t rel = sp_str_suffix(path.sub, path.sub.len - w->prefix.len);
  sp_da_push(w->result->matches, ((spn_dag_glob_match_t) {
    .path = path,
    .rel = sp_str_strip_left(rel, sp_str_lit("/"))
  }));
}

static void walk_observe(spn_dag_glob_walk_t* w, spn_dag_obs_kind_t kind, spn_path_t path, sp_str_t filter) {
  sp_da_push(w->result->obs, ((spn_dag_obs_t) {
    .kind = kind,
    .path = path,
    .filter = filter
  }));
}

static spn_err_t glob_walk(spn_dag_glob_walk_t* w, spn_dag_glob_dir_t start) {
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch_for(w->mem);
  spn_err_t err = SPN_OK;

  sp_da(spn_dag_glob_dir_t) pending = sp_da_new(scratch.mem, spn_dag_glob_dir_t);
  sp_da_push(pending, start);

  for (u64 dt = 0; dt < sp_da_size(pending); dt++) {
    spn_dag_glob_dir_t visit = pending[dt];
    if (visit.depth > SPN_DAG_GLOB_DEPTH_MAX) {
      err = SPN_ERR_DAG_GLOB;
      break;
    }
    walk_observe(w, SPN_DAG_OBS_ENUMERATION, visit.path, w->filter);

    sp_da(sp_fs_entry_t) entries = sp_zero;
    sp_fs_collect(scratch.mem, spn_path_str(w->roots, scratch.mem, visit.path), &entries);
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
      walk_match(w, path);
    }
  }

  sp_mem_end_scratch(scratch);
  return err;
}

static bool literal_exists(spn_dag_glob_walk_t* w, spn_path_t pattern) {
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch_for(w->mem);
  bool exists = sp_fs_is_file(spn_path_str(w->roots, scratch.mem, pattern));
  sp_mem_end_scratch(scratch);
  return exists;
}

static spn_err_t glob_run(spn_dag_glob_walk_t* w, spn_path_t pattern) {
  w->glob = sp_glob_new_str(w->mem, pattern.sub);
  if (!w->glob) {
    return SPN_ERR_DAG_GLOB;
  }
  w->prefix = get_literal_dir(w->glob);

  if (w->glob->strategy == SP_GLOB_STRATEGY_LITERAL) {
    if (literal_exists(w, pattern)) {
      walk_observe(w, SPN_DAG_OBS_FILE, pattern, sp_str_lit(""));
      walk_match(w, pattern);
    } else {
      walk_observe(w, SPN_DAG_OBS_ABSENT, pattern, sp_str_lit(""));
    }
    return SPN_OK;
  }

  sp_str_t remainder = sp_str_sub(pattern.sub, w->prefix.len, pattern.sub.len - w->prefix.len);
  remainder = sp_str_strip_left(remainder, sp_str_lit("/"));

  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch_for(w->mem);
  w->filter = get_glob_filter(scratch.mem, pattern.sub);
  sp_mem_end_scratch(scratch);
  w->recursive = sp_str_contains(remainder, sp_str_lit("/")) || has_recursive_token(w->glob);

  spn_try(glob_walk(w, (spn_dag_glob_dir_t) {
    .path = { .root = pattern.root, .sub = w->prefix }
  }));

  sp_da_sort(w->result->matches, compare_matches);
  return SPN_OK;
}

spn_err_t spn_dag_glob(sp_mem_t mem, const spn_path_roots_t* roots, spn_path_t pattern, spn_dag_glob_result_t* result) {
  result->obs = sp_da_new(mem, spn_dag_obs_t);
  result->matches = sp_da_new(mem, spn_dag_glob_match_t);
  spn_dag_glob_walk_t walk = {
    .mem = mem,
    .roots = roots,
    .result = result
  };
  return glob_run(&walk, pattern);
}
