#ifndef SPN_BUILD_H
#define SPN_BUILD_H
#include "sp.h"

typedef enum {
  SPN_DEP_BUILD_MODE_DEBUG,
  SPN_DEP_BUILD_MODE_RELEASE,
} spn_dep_build_mode_t;

typedef enum {
  SPN_DEP_BUILD_KIND_SHARED = SP_OS_LIB_SHARED,
  SPN_DEP_BUILD_KIND_STATIC = SP_OS_LIB_STATIC,
  SPN_DEP_BUILD_KIND_SOURCE,
} spn_dep_build_kind_t;

typedef struct {
  const c8* name;

} spn_dep_config_t;

typedef struct {
  const c8* name;
} spn_build_config_t;

#define SPN_BUILD_FN() spn_build_config_t spn_build_fn()
#endif

