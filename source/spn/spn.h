#ifndef SPN_SPN_H
#define SPN_SPN_H

#include "spn/types.h"
#include "spn/build.h"
#include "spn/recipe.h"
#include "spn/config.h"

#define _SP_MSTR(x) #x
#define SP_MSTR(x) _SP_MSTR(x)

#define _SP_MCAT(x, y) x##y
#define SP_MCAT(x, y) _SP_MCAT(x, y)

#define SPN_COPY_N_MAX_ENTRIES 16

#define SPN_DEP(expr) you_forgot_to_escape_a_dep_with_backslash spn_static_assert;

typedef struct {
  const c8* target;
  u32 jobs;
} spn_make_t;

typedef struct {
  int foo;
} spn_autoconf_t;

void spn_make(spn_dep_builder_t* build, spn_make_t config);
void spn_autoconf(spn_dep_builder_t* build, spn_autoconf_t config);

void spn_copy(spn_dep_builder_t* build, spn_cache_dir_kind_t from, const c8* from_path, spn_cache_dir_kind_t to, const c8* to_path);
void spn_copy_n(spn_dep_builder_t* build, spn_recipe_copy_config_t entries [SPN_COPY_N_MAX_ENTRIES]);

#endif
