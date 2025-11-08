#include "spn/spn.h"

#define SPN_PACKAGE tcc
#define SPN_OPTIONS()

#include "spn/gen/recipe.h"

void tcc_build(spn_dep_t* build) {
  spn_autoconf(build);
  spn_make(build);
}

void tcc_package(spn_dep_t* build) {
  spn_make_t* make = spn_make_new(build);
  spn_make_add_target(make, "install");
  spn_make_run(make);
}

void tcc(spn_recipe_t* recipe) {
  spn_recipe_set_name(recipe, "tcc");
  spn_recipe_set_git(recipe, "TinyCC/tinycc");
  spn_recipe_add_kind(recipe, SPN_DEP_BUILD_KIND_SHARED);
  spn_recipe_add_kind(recipe, SPN_DEP_BUILD_KIND_STATIC);
  spn_recipe_set_build_fn(recipe, tcc_build);
  spn_recipe_set_package_fn(recipe, tcc_package);

}
