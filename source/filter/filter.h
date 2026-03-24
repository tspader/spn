#ifndef SPN_FILTER_FILTER_H
#define SPN_FILTER_FILTER_H

#include "filter/types.h"
#include "spn.h"
#include "target/types.h"

bool spn_target_filter_pass(spn_target_filter_t* filter, spn_target_t* target);
bool spn_is_visibility_linked(spn_visibility_t target, spn_visibility_t dep);

#endif
