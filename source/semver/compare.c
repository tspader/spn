#include "semver/types.h"
#include "semver/compare.h"

bool spn_semver_eq(spn_semver_t lhs, spn_semver_t rhs) {
  return lhs.major == rhs.major && lhs.minor == rhs.minor && lhs.patch == rhs.patch;
}

bool spn_semver_geq(spn_semver_t lhs, spn_semver_t rhs) {
  if (lhs.major != rhs.major) return lhs.major > rhs.major;
  if (lhs.minor != rhs.minor) return lhs.minor > rhs.minor;
  return lhs.patch >= rhs.patch;
}

bool spn_semver_ge(spn_semver_t lhs, spn_semver_t rhs) {
  if (lhs.major != rhs.major) return lhs.major > rhs.major;
  if (lhs.minor != rhs.minor) return lhs.minor > rhs.minor;
  return lhs.patch > rhs.patch;
}

bool spn_semver_leq(spn_semver_t lhs, spn_semver_t rhs) {
  if (lhs.major != rhs.major) return lhs.major < rhs.major;
  if (lhs.minor != rhs.minor) return lhs.minor < rhs.minor;
  return lhs.patch <= rhs.patch;
}

bool spn_semver_le(spn_semver_t lhs, spn_semver_t rhs) {
  if (lhs.major != rhs.major) return lhs.major < rhs.major;
  if (lhs.minor != rhs.minor) return lhs.minor < rhs.minor;
  return lhs.patch < rhs.patch;
}

s32 spn_semver_cmp(spn_semver_t lhs, spn_semver_t rhs) {
  if (spn_semver_eq(lhs, rhs)) return SP_QSORT_EQUAL;
  if (spn_semver_leq(lhs, rhs)) return SP_QSORT_A_FIRST;
  return SP_QSORT_B_FIRST;
}

s32 spn_semver_sort_kernel(const void* a, const void* b) {
  const spn_semver_t* lhs = (const spn_semver_t*)a;
  const spn_semver_t* rhs = (const spn_semver_t*)b;
  return spn_semver_cmp(*lhs, *rhs);
}

bool spn_semver_satisfies(spn_semver_t version, spn_semver_t bound_version, spn_semver_op_t op) {
  switch (op) {
    case SPN_SEMVER_OP_EQ: {
      return spn_semver_eq(version, bound_version);
    }
    case SPN_SEMVER_OP_LT: {
      return spn_semver_le(version, bound_version);
    }
    case SPN_SEMVER_OP_LEQ: {
      return spn_semver_leq(version, bound_version);
    }
    case SPN_SEMVER_OP_GT: {
      return spn_semver_ge(version, bound_version);
    }
    case SPN_SEMVER_OP_GEQ: {
      return spn_semver_geq(version, bound_version);
    }
    default: {
      SP_UNREACHABLE_RETURN(false);
    }
  }
}

bool spn_semver_is_empty(spn_semver_t version) {
  return !version.major && !version.minor && !version.patch;
}
