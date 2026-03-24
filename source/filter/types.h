#ifndef SPN_FILTER_TYPES_H
#define SPN_FILTER_TYPES_H

#include "sp.h"

typedef struct {
  sp_str_t name;
  struct {
    bool public;
    bool test;
    bool script;
  } disabled;
} spn_target_filter_t;

#endif
