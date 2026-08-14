#ifndef SPN_PROJECT_TYPES_H
#define SPN_PROJECT_TYPES_H

#include "sp.h"
#include "spn/core.h"

#include "lock/types.h"
#include "pkg/types.h"

struct spn_project_t {
  struct {
    sp_str_t root;
    sp_str_t manifest;
    sp_str_t lock;
  } paths;
  spn_pkg_info_t package;
  sp_opt(spn_lock_file_t) lock;
};

#endif
