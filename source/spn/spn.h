#include "spn/types.h"
#include "spn/build.h"
#include "spn/recipe.h"
#include "spn/config.h"

#define SPN_COPY_N_MAX_ENTRIES 16

void spn_copy(spn_dep_builder_t* recipe, spn_cache_dir_kind_t from, const c8* from_path, spn_cache_dir_kind_t to, const c8* to_path);
void spn_copy_n(spn_dep_builder_t* recipe, spn_recipe_copy_config_t entries [SPN_COPY_N_MAX_ENTRIES]);

