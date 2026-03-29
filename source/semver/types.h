#ifndef SPN_SEMVER_TYPES_H
#define SPN_SEMVER_TYPES_H

#include "sp.h"

typedef enum {
  SPN_SEMVER_OP_LT = 0,
  SPN_SEMVER_OP_LEQ = 1,
  SPN_SEMVER_OP_GT = 2,
  SPN_SEMVER_OP_GEQ = 3,
  SPN_SEMVER_OP_EQ = 4,
} spn_semver_op_t;

typedef enum {
  SPN_SEMVER_MOD_NONE,
  SPN_SEMVER_MOD_CARET,
  SPN_SEMVER_MOD_TILDE,
  SPN_SEMVER_MOD_WILDCARD,
  SPN_SEMVER_MOD_CMP,
} spn_semver_mod_t;

typedef struct {
  u32 major;
  u32 minor;
  u32 patch;
  u8 padding [4];
} spn_semver_t;

typedef struct {
  bool major;
  bool minor;
  bool patch;
} spn_semver_components_t;

typedef struct {
  spn_semver_t version;
  spn_semver_components_t components;
} spn_semver_parsed_t;

typedef struct {
  spn_semver_t version;
  spn_semver_op_t op;
} spn_semver_bound_t;

typedef struct {
  spn_semver_bound_t low;
  spn_semver_bound_t high;
  spn_semver_mod_t mod;
} spn_semver_range_t;

#define spn_semver_lit(major, minor, patch) (spn_semver_t) { major, minor, patch }
#define spn_semver_any() (spn_semver_range_t) { \
  .low  = { .op = SPN_SEMVER_OP_GEQ, .version = { 0, 0, 0 } }, \
  .high = { .op = SPN_SEMVER_OP_LT,  .version = { 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF } }, \
  .mod  = SPN_SEMVER_MOD_WILDCARD, \
}

#endif
