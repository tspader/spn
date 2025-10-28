#ifndef SPN_BUILD_H
#define SPN_BUILD_H

#include "spn/types.h"

#define SPN_MAX_DEPS 16

typedef struct {
  void* data;
  u32 size;
} spn_opaque_options_t;

typedef struct {
  const c8* name;
  spn_dep_kind_t kind;
  spn_opaque_options_t options;
} spn_opaque_dep_t;

typedef struct {
  spn_opaque_dep_t* deps;
  u32 num_deps;
} spn_opaque_build_t;
#endif
