#include "spn.h"
#include "target/types.h"
#include "filter/filter.h"

bool spn_target_filter_pass(spn_target_filter_t* filter, spn_target_info_t* target) {
  if (!sp_str_empty(filter->name)) {
    return sp_str_equal(filter->name, target->name);
  }

  if (filter->only.bin) return target->kind == SPN_TARGET_EXE;
  if (filter->only.lib) return target->kind == SPN_TARGET_LIB;
  if (filter->only.test) return target->kind == SPN_TARGET_TEST;
  if (filter->only.script) return target->kind == SPN_TARGET_SCRIPT;

  switch (target->kind) {
    case SPN_TARGET_EXE:
    case SPN_TARGET_LIB: return !filter->disabled.public;
    case SPN_TARGET_TEST: return !filter->disabled.test;
    case SPN_TARGET_SCRIPT: return !filter->disabled.script;
  }
  sp_unreachable_return(false);
}
