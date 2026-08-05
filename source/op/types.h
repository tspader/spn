#ifndef SPN_OP_TYPES_H
#define SPN_OP_TYPES_H

#include "sp.h"

#include "semver/types.h"

typedef enum {
  SPN_ADD_DEP_PACKAGE,
  SPN_ADD_DEP_TEST,
  SPN_ADD_DEP_BUILD,
} spn_add_dep_t;

typedef struct {
  sp_str_t key;
  sp_str_t requested;
  spn_semver_range_t range;
  spn_add_dep_t dep;
} spn_add_request_t;

typedef struct {
  bool force;
  sp_str_t only;
} spn_index_refresh_t;

typedef struct {
  sp_str_t index;
  sp_str_t url;
  sp_str_t revision;
  bool allow_dirty;
} spn_publish_request_t;

#endif
