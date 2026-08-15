#ifndef SPN_PATHS_TYPES_H
#define SPN_PATHS_TYPES_H

#include "sp.h"
#include "spn/core.h"
#include "spn/types.h"

typedef struct {
  sp_str_t dirs [SPN_PATH_ROOT_COUNT];
  sp_str_t storage;
} spn_path_roots_t;

typedef struct {
  bool within;
  sp_str_t sub;
} spn_path_rel_t;

typedef struct {
  spn_path_t recipe;
  spn_path_t source;
} spn_tree_roots_t;

typedef struct {
  spn_tree_t tree;
  sp_str_t sub;
} spn_tree_rel_t;

typedef struct {
  sp_str_t prefix;
  spn_path_t path;
} spn_arg_t;

typedef struct {
  sp_str_t dir;
  sp_str_t checkouts;
} spn_git_cache_paths_t;

typedef struct {
  sp_str_t dir;
} spn_store_cache_paths_t;

typedef struct {
  sp_str_t dir;
} spn_build_cache_paths_t;

typedef struct {
  sp_str_t dir;
  spn_git_cache_paths_t git;
  spn_store_cache_paths_t store;
  spn_build_cache_paths_t build;
} spn_cache_paths_t;

typedef struct {
  sp_str_t cwd;
  sp_str_t project;
  sp_str_t index;
  sp_str_t runtime;
  sp_str_t version;
  sp_str_t toolchain;
  sp_str_t patches;
  sp_str_t storage;
  struct {
    sp_str_t dir;
    sp_str_t toml;
  } config;
  spn_cache_paths_t caches;
} spn_system_paths_t;

#endif
