#ifdef SPN_BUILD
#include "spn/build.h"
#include "spn/recipes/sp.h"

#define SPN_DEPS() \
  SPN_DEP(sp)

#include "spn/gen.h"

spn_build_config_t spn_build();

spn_build_config_ex_t spn_build_ex() {
  spn_build_config_t user = spn_build();

  spn_build_config_ex_t spn = SP_ZERO_INITIALIZE();
  spn.name = sp_str_from_cstr(user.name);

  spn_opaque_dep_t sp = {
    .name = "sp",
    .kind = user.deps.sp.kind,
    .options = SP_ALLOC(spn_sp_options_t)
  };
  sp_os_copy_memory(&user.deps.sp, sp.options, sizeof(spn_sp_options_t));
  sp_dyn_array_push(spn.deps, sp);


  return spn;
}

spn_build_config_t spn_build() {
  return (spn_build_config_t) {
    .name = "sp",
    .deps = {
      .sp = {
        .backend = SP_BACKEND_NATIVE,
        .bar = 42
      }
    }
  };
}

int main(int n, char** args) {
  spn_build_config_ex_t build = spn_build_ex();
  spn_sp_options_t* sp = (spn_sp_options_t*)build.deps[0].options;
  return sp->bar;
}

#endif
