#include "sp.h"
#include "pkg/patch.h"

spn_pkg_patch_stamp_result_t spn_pkg_patch_stamp(sp_da(spn_pkg_patch_t) patches, sp_str_t qualified, spn_pkg_tree_t* source) {
  sp_da_for(patches, it) {
    if (!sp_str_equal(patches[it].qualified, qualified)) {
      continue;
    }
    if (source->kind != SPN_PKG_TREE_GIT) {
      return SPN_PKG_PATCH_STAMP_NOT_GIT;
    }
    source->git.patches = patches[it].set;
    return SPN_PKG_PATCH_STAMP_APPLIED;
  }
  return SPN_PKG_PATCH_STAMP_NONE;
}
