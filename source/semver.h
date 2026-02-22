#ifndef SPN_SEMVER_H
#define SPN_SEMVER_H

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

typedef struct {
  sp_str_t str;
  u32 it;
} spn_semver_parser_t;

#define spn_semver_lit(major, minor, patch) (spn_semver_t) { major, minor, patch }

c8 spn_semver_parser_peek(spn_semver_parser_t* parser);
void spn_semver_parser_eat(spn_semver_parser_t* parser);
void spn_semver_parser_eat_and_assert(spn_semver_parser_t* parser, c8 c);
bool spn_semver_parser_is_digit(c8 c);
bool spn_semver_parser_is_whitespace(c8 c);
bool spn_semver_parser_is_done(spn_semver_parser_t* parser);
void spn_semver_parser_eat_whitespace(spn_semver_parser_t* parser);
u32 spn_semver_parser_parse_number(spn_semver_parser_t* parser);
spn_semver_parsed_t spn_semver_parser_parse_version(spn_semver_parser_t* parser);
spn_semver_range_t spn_semver_caret_to_range(spn_semver_parsed_t parsed);
spn_semver_t spn_semver_from_str(sp_str_t str);
spn_semver_range_t spn_semver_range_from_str(sp_str_t str);
sp_str_t spn_semver_range_to_str(spn_semver_range_t range);
sp_str_t spn_semver_to_str(spn_semver_t version);
bool spn_semver_eq(spn_semver_t lhs, spn_semver_t rhs);
bool spn_semver_geq(spn_semver_t lhs, spn_semver_t rhs);
bool spn_semver_ge(spn_semver_t lhs, spn_semver_t rhs);
bool spn_semver_leq(spn_semver_t lhs, spn_semver_t rhs);
bool spn_semver_le(spn_semver_t lhs, spn_semver_t rhs);
s32 spn_semver_cmp(spn_semver_t lhs, spn_semver_t rhs);
s32 spn_semver_sort_kernel(const void* a, const void* b);
bool spn_semver_satisfies(spn_semver_t version, spn_semver_t bound_version, spn_semver_op_t op);
bool spn_semver_is_empty(spn_semver_t version);

#endif
