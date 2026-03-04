#ifndef SPN_SEMVER_CONVERT_H
#define SPN_SEMVER_CONVERT_H

#include "semver/types.h"

spn_semver_range_t spn_semver_wildcard_to_range(spn_semver_parsed_t parsed);
spn_semver_range_t spn_semver_tilde_to_range(spn_semver_parsed_t parsed);
spn_semver_range_t spn_semver_comparison_to_range(spn_semver_op_t op, spn_semver_t version);
spn_semver_range_t spn_semver_caret_to_range(spn_semver_parsed_t parsed);
spn_semver_t spn_semver_from_str(sp_str_t str);
sp_str_t spn_semver_range_to_str(spn_semver_range_t range);
sp_str_t spn_semver_to_str(spn_semver_t version);

#endif
