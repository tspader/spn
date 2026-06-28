#ifndef SPN_SESSION_REGISTRY_TYPES_H
#define SPN_SESSION_REGISTRY_TYPES_H

#include "sp.h"
#include "spn.h"

#include "pkg/types.h"

typedef struct {
  spn_pkg_source_t source;
  spn_pkg_info_t* info;
  struct {
    sp_str_t manifest;
    sp_str_t script;
    sp_str_t configure;
    sp_str_t build;
    sp_str_t source;
  } paths;
  u64 elapsed;
} spn_loaded_pkg_t;

typedef sp_str_ht(spn_loaded_pkg_t) spn_pkg_registry_t;

#endif
