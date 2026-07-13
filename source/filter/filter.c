#include "sp.h"
#include "spn.h"
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

bool spn_target_selection_pass(const spn_target_selection_t* selection, const spn_target_info_t* target) {
  if (selection->kind == SPN_TARGET_SELECTION_DEFAULT) {
    switch (target->kind) {
      case SPN_TARGET_BUILD_METAPROGRAM:
      case SPN_TARGET_CONFIGURE_METAPROGRAM:
      case SPN_TARGET_SCRIPT: {
        return false;
      }
      case SPN_TARGET_LIB:
      case SPN_TARGET_TEST:
      case SPN_TARGET_EXE: {
        return true;
      }
    }
  }

  switch (target->kind) {
    case SPN_TARGET_EXE: {
      return spn_target_rule_pass(&selection->targets.bin, target);
    }
    case SPN_TARGET_LIB: {
      return spn_target_rule_pass(&selection->targets.lib, target);
    }
    case SPN_TARGET_TEST: {
      return spn_target_rule_pass(&selection->targets.test, target);
    }
    case SPN_TARGET_SCRIPT: {
      return spn_target_rule_pass(&selection->targets.script, target);
    }
    case SPN_TARGET_CONFIGURE_METAPROGRAM:
    case SPN_TARGET_BUILD_METAPROGRAM: {
      return false;
    }
  }
  sp_unreachable_return(false);
}
