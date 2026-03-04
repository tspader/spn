#ifndef SPN_OPTION_TYPES_H
#define SPN_OPTION_TYPES_H

#include "sp.h"

typedef enum {
  SPN_DEP_OPTION_KIND_BOOL,
  SPN_DEP_OPTION_KIND_S64,
  SPN_DEP_OPTION_KIND_STR,
} spn_dep_option_kind_t;

typedef struct {
  sp_str_t name;
  spn_dep_option_kind_t kind;
  union {
    bool b;
    s64 s;
    sp_str_t str;
  };
} spn_dep_option_t;

typedef sp_ht(sp_str_t, spn_dep_option_t) spn_dep_options_t;

#endif
