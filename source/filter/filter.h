#ifndef SPN_FILTER_FILTER_H
#define SPN_FILTER_FILTER_H

#include "filter/types.h"
#include "target/types.h"

bool spn_target_filter_pass(spn_target_filter_t* filter, spn_target_info_t* target);

#endif
