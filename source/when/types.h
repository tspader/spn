#ifndef SPN_WHEN_TYPES_H
#define SPN_WHEN_TYPES_H

#include "sp.h"

typedef enum {
  SPN_OPTION_VALUE_NONE,
  SPN_OPTION_VALUE_BOOL,
  SPN_OPTION_VALUE_STR,
} spn_option_value_kind_t;

typedef struct {
  spn_option_value_kind_t kind;
  union {
    bool b;
    sp_str_t str;
  };
} spn_option_value_t;

typedef struct {
  sp_str_t key;
  bool negated;
  spn_option_value_t value;
} spn_when_clause_t;

typedef struct {
  sp_da(spn_when_clause_t) clauses;
} spn_when_t;

typedef sp_str_ht(spn_option_value_t) spn_when_env_t;

typedef enum {
  SPN_OPTION_TYPE_NONE,
  SPN_OPTION_TYPE_BOOL,
  SPN_OPTION_TYPE_ENUM,
} spn_option_type_t;

typedef struct {
  spn_when_t when;
  spn_option_value_t value;
} spn_option_default_t;

typedef sp_da(spn_option_default_t) spn_option_defaults_t;

typedef struct {
  sp_str_t name;
  spn_option_type_t type;
  bool additive;
  bool public;
  sp_str_t define;
  sp_da(sp_str_t) values;
  spn_option_defaults_t defaults;
} spn_option_info_t;

// A manifest list entry that may carry a predicate; after option application
// every surviving entry has been folded into the plain list beside it
typedef struct {
  sp_str_t value;
  spn_when_t when;
} spn_gated_str_t;

typedef sp_da(spn_gated_str_t) spn_gated_list_t;

#endif
