#ifndef SPN_FILTER_TYPES_H
#define SPN_FILTER_TYPES_H

#include "sp.h"

typedef sp_da(sp_str_t) spn_target_names_t;

typedef enum {
  SPN_TARGET_RULE_NONE,
  SPN_TARGET_RULE_ALL,
  SPN_TARGET_RULE_NAMED,
} spn_target_rule_kind_t;

typedef struct {
  spn_target_rule_kind_t kind;
  spn_target_names_t names;
} spn_target_rule_t;

typedef enum {
  SPN_TARGET_SELECTION_DEFAULT,
  SPN_TARGET_SELECTION_EXPLICIT,
} spn_target_selection_kind_t;

typedef struct {
  spn_target_selection_kind_t kind;
  struct {
    spn_target_rule_t bin;
    spn_target_rule_t lib;
    spn_target_rule_t test;
    spn_target_rule_t script;
  } targets;
} spn_target_selection_t;

#endif
