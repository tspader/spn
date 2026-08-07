#include "sp.h"
#include "spn/core.h"
#include "target/types.h"
#include "filter/filter.h"

static bool spn_target_rule_pass(const spn_target_rule_t* rule, const spn_target_info_t* target) {
  switch (rule->kind) {
    case SPN_TARGET_RULE_NONE: {
      return false;
    }
    case SPN_TARGET_RULE_ALL: {
      return true;
    }
    case SPN_TARGET_RULE_NAMED: {
      sp_da_for(rule->names, it) {
        if (sp_str_equal(rule->names[it], target->name)) {
          return true;
        }
      }
      return false;
    }
  }
  sp_unreachable_return(false);
}

static bool is_default(const spn_target_selection_t* selection) {
  return
    selection->bin.kind == SPN_TARGET_RULE_NONE &&
    selection->lib.kind == SPN_TARGET_RULE_NONE &&
    selection->test.kind == SPN_TARGET_RULE_NONE &&
    selection->script.kind == SPN_TARGET_RULE_NONE;
}

bool spn_target_selection_pass(const spn_target_selection_t* selection, const spn_target_info_t* target) {
  if (is_default(selection)) {
    switch (target->kind) {
      case SPN_TARGET_KIND_BUILD_METAPROGRAM:
      case SPN_TARGET_KIND_CONFIGURE_METAPROGRAM:
      case SPN_TARGET_KIND_SCRIPT: {
        return false;
      }
      case SPN_TARGET_KIND_LIB:
      case SPN_TARGET_KIND_TEST:
      case SPN_TARGET_KIND_EXE: {
        return true;
      }
    }
  }

  switch (target->kind) {
    case SPN_TARGET_KIND_EXE: {
      return spn_target_rule_pass(&selection->bin, target);
    }
    case SPN_TARGET_KIND_LIB: {
      return spn_target_rule_pass(&selection->lib, target);
    }
    case SPN_TARGET_KIND_TEST: {
      return spn_target_rule_pass(&selection->test, target);
    }
    case SPN_TARGET_KIND_SCRIPT: {
      return spn_target_rule_pass(&selection->script, target);
    }
    case SPN_TARGET_KIND_CONFIGURE_METAPROGRAM:
    case SPN_TARGET_KIND_BUILD_METAPROGRAM: {
      return false;
    }
  }
  sp_unreachable_return(false);
}
