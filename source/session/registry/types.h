#ifndef SPN_SESSION_REGISTRY_TYPES_H
#define SPN_SESSION_REGISTRY_TYPES_H

#include "sp.h"
#include "spn.h"

#include "pkg/types.h"

typedef struct {
  spn_pkg_source_t source;
  spn_pkg_info_t* info;
  sp_str_t manifest;
} spn_registry_pkg_t;

typedef sp_str_ht(spn_registry_pkg_t) spn_pkg_registry_t;

typedef struct {
  sp_str_t manifest;
  sp_str_t error;
} spn_registry_err_t;

#endif
