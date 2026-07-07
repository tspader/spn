#ifndef SPN_SEMVER_COMPARE_H
#define SPN_SEMVER_COMPARE_H

#include "semver/types.h"

bool spn_semver_eq(spn_semver_t lhs, spn_semver_t rhs);
bool spn_semver_geq(spn_semver_t lhs, spn_semver_t rhs);
bool spn_semver_ge(spn_semver_t lhs, spn_semver_t rhs);
bool spn_semver_leq(spn_semver_t lhs, spn_semver_t rhs);
bool spn_semver_le(spn_semver_t lhs, spn_semver_t rhs);
s32 spn_semver_cmp(spn_semver_t lhs, spn_semver_t rhs);
s32 spn_semver_sort_kernel(const void* a, const void* b);
bool spn_semver_satisfies(spn_semver_t version, spn_semver_t bound_version, spn_semver_op_t op);
bool spn_semver_in_range(spn_semver_t version, spn_semver_range_t range);
bool spn_semver_is_empty(spn_semver_t version);

#endif

