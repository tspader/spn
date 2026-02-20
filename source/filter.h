#ifndef SPN_TARGET_FILTER_H
#define SPN_TARGET_FILTER_H

#include "sp.h"

#include "target.h"

typedef struct {
  sp_str_t name;
  struct {
    bool public;
    bool test;
  } disabled;
} spn_target_filter_t;

bool spn_target_filter_pass(spn_target_filter_t* filter, spn_target_t* target);
bool spn_is_visibility_linked(spn_visibility_t target, spn_visibility_t dep);

#endif
