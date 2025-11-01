#include "spn/spn.h"

void ggml(spn_recipe_t* recipe) {
  spn_recipe_set_name(recipe, "ggml");
  spn_recipe_set_git(recipe, "ggml-org/ggml");
  spn_recipe_add_kind(recipe, SPN_DEP_BUILD_KIND_SHARED);
  spn_recipe_add_kind(recipe, SPN_DEP_BUILD_KIND_STATIC);
}
