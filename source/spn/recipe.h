#ifndef SPN_RECIPE_H
#define SPN_RECIPE_H

#include "spn/types.h"

#define SPN_RECIPE_MAX_LIBS 8

typedef struct {
  const c8* name;
  spn_dep_kind_t kind;
  spn_dep_mode_t mode;
  void* options;

  const c8* commit;
  const c8* build_id;

  struct {
    const c8* log;
    const c8* stamp;
    const c8* source;
    const c8* work;
    const c8* store;
    const c8* include;
    const c8* lib;
    const c8* vendor;
  } paths;
} spn_dep_builder_t;

typedef struct {
  spn_cache_dir_kind_t dir;
  const c8* path;
} spn_recipe_path_t;

typedef struct {
  spn_recipe_path_t from;
  spn_recipe_path_t to;
} spn_recipe_copy_config_t;

typedef void (*spn_recipe_build_fn_t)(spn_dep_builder_t);
typedef void (*spn_recipe_configure_fn_t)(spn_dep_builder_t);
typedef void (*spn_recipe_package_fn_t)(spn_dep_builder_t);

typedef struct spn_recipe_config {
  const c8* name;
  const c8* git;
  const c8* branch;
  const c8* libs [SPN_RECIPE_MAX_LIBS];
  spn_dep_kind_t kinds [3];
  spn_recipe_configure_fn_t configure;
  spn_recipe_build_fn_t build;
  spn_recipe_package_fn_t package;
} spn_recipe_info_t;

typedef spn_recipe_info_t (*spn_recipe_fn_t)(void);

void spn_recipe_copy(spn_dep_builder_t* recipe, spn_cache_dir_kind_t from, const c8* pf, spn_cache_dir_kind_t to, const c8* dt);
void spn_recipe_copy_n(spn_dep_builder_t* recipe, spn_recipe_copy_config_t entries [16]);
#endif

