#include "spn.h"
#include "filter/filter.h"

bool spn_target_filter_pass(spn_target_filter_t* filter, spn_target_t* target) {
  if (!sp_str_empty(filter->name)) {
    return sp_str_equal(filter->name, target->name);
  }

  bool has_only = filter->only.bin || filter->only.lib || filter->only.test || filter->only.script;
  if (has_only) {
    switch (target->visibility) {
      case SPN_VISIBILITY_PUBLIC: {
        switch (target->kind) {
          case SPN_TARGET_EXE:        return filter->only.bin;
          case SPN_TARGET_STATIC_LIB:
          case SPN_TARGET_SHARED_LIB: return filter->only.lib;
          default:                    return false;
        }
      }
      case SPN_VISIBILITY_TEST:   return filter->only.test;
      case SPN_VISIBILITY_SCRIPT: return filter->only.script;
      case SPN_VISIBILITY_BUILD:  return true;
    }
    sp_unreachable_return(false);
  }

  switch (target->visibility) {
    case SPN_VISIBILITY_PUBLIC: return !filter->disabled.public;
    case SPN_VISIBILITY_TEST: return !filter->disabled.test;
    case SPN_VISIBILITY_SCRIPT: return !filter->disabled.script;
    case SPN_VISIBILITY_BUILD: return true;
  }
  sp_unreachable_return(false);
}

bool spn_is_visibility_linked(spn_visibility_t target, spn_visibility_t dep) {
  switch (target) {
    case SPN_VISIBILITY_PUBLIC: {
      switch (dep) {
        case SPN_VISIBILITY_PUBLIC: return true;
        case SPN_VISIBILITY_TEST: return false;
        case SPN_VISIBILITY_SCRIPT: return false;
        case SPN_VISIBILITY_BUILD: return false;
      }
    }
    case SPN_VISIBILITY_TEST: {
      switch (dep) {
        case SPN_VISIBILITY_PUBLIC: return true;
        case SPN_VISIBILITY_TEST: return true;
        case SPN_VISIBILITY_SCRIPT: return false;
        case SPN_VISIBILITY_BUILD: return false;
      }
    }
    case SPN_VISIBILITY_SCRIPT: {
      switch (dep) {
        case SPN_VISIBILITY_PUBLIC: return true;
        case SPN_VISIBILITY_TEST: return true;
        case SPN_VISIBILITY_SCRIPT: return false;
        case SPN_VISIBILITY_BUILD: return false;
      }
    }
    case SPN_VISIBILITY_BUILD: {
      return false;
    }
  }
  SP_UNREACHABLE_RETURN(false);
}
