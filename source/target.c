#include "target.h"

spn_target_kind_t spn_pkg_linkage_to_target_kind(spn_linkage_t kind) {
  switch (kind) {
    case SPN_LIB_KIND_SHARED: return SPN_TARGET_SHARED_LIB;
    case SPN_LIB_KIND_STATIC: return SPN_TARGET_STATIC_LIB;
    case SPN_LIB_KIND_SOURCE: {
      sp_unreachable_case();
    }
  }
  sp_unreachable_return(SPN_TARGET_EXE);
}

spn_linkage_t spn_target_kind_to_pkg_linkage(spn_target_kind_t kind) {
  switch (kind) {
    case SPN_TARGET_SHARED_LIB: return SPN_LIB_KIND_SHARED;
    case SPN_TARGET_STATIC_LIB: return SPN_LIB_KIND_STATIC;
    case SPN_TARGET_NONE:
    case SPN_TARGET_EXE:
    case SPN_TARGET_JIT:
    case SPN_TARGET_OBJECT: {
      sp_unreachable_case();
    }
  }
  sp_unreachable_return(SPN_LIB_KIND_SHARED);
}
