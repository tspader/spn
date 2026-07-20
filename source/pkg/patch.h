#ifndef SPN_PKG_PATCH_H
#define SPN_PKG_PATCH_H

#include "sp.h"

#include "pkg/types.h"

typedef enum {
  SPN_PKG_PATCH_STAMP_NONE,
  SPN_PKG_PATCH_STAMP_APPLIED,
  SPN_PKG_PATCH_STAMP_NOT_GIT,
} spn_pkg_patch_stamp_result_t;

spn_pkg_patch_stamp_result_t spn_pkg_patch_stamp(sp_da(spn_pkg_patch_t) patches, sp_str_t qualified, spn_pkg_tree_t* source);

#endif
