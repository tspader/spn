#ifndef SPN_GEN_H
#define SPN_GEN_H

#define _SPN_OPTIONS_T(PACKAGE) spn_##PACKAGE##_options_t
#define SPN_OPTIONS_T(PACKAGE) _SPN_OPTIONS_T(PACKAGE)
#define SPN_OPTION(TYPE, NAME) TYPE NAME;

#define _SP_MSTR(x) #x
#define SP_MSTR(x) _SP_MSTR(x)

#endif



#ifdef SPN_OPTIONS
#include "spn/recipe.h"

typedef struct {
  spn_dep_kind_t kind;
  SPN_OPTIONS()
} SPN_OPTIONS_T(SPN_PACKAGE);

#undef SPN_PACKAGE
#undef SPN_OPTIONS
#endif


#ifdef SPN_DEPS
#include "spn/types.h"
#include "spn/build.h"
#include "spn/recipe.h"

#define SPN_IMPLEMENTATION
#include "spn/recipe.h"

#undef SPN_DEP
#define SPN_DEP(DEP) spn_##DEP##_options_t DEP;

typedef struct {
  const char* name;

  struct {
    SPN_DEPS()
  } deps;
} spn_build_t;

spn_build_t spn_build();

spn_opaque_build_t spn_build_opaque() {
  spn_build_t build = spn_build();

  spn_opaque_build_t spn = { 0 };
  spn.deps = calloc(sizeof(spn_opaque_dep_t), SPN_MAX_DEPS);
  spn.num_deps = 0;

  #undef SPN_DEP
  #define SPN_DEP(DEP) spn_opaque_dep_t DEP = { \
    .name = SP_MSTR(DEP), \
    .kind = build.deps.DEP.kind, \
  }; \
  DEP.options.size = sizeof(SPN_OPTIONS_T(DEP)); \
  DEP.options.data = calloc(DEP.options.size, 1); \
  memcpy(DEP.options.data, &build.deps.DEP, DEP.options.size); \


  #ifdef SPN_LOCKS
  #define SPN_LOCK(DEP, COMMIT) DEP.lock = COMMIT;
  SPN_LOCKS()
  // put array of { name, commit } onto opaque build
  // put on dep
  // all good
  #endif

  spn.deps[spn.num_deps++] = DEP;

  SPN_DEPS()
  return spn;
}

#undef SPN_DEPS
#endif

