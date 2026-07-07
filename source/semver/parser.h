#ifndef SPN_SEMVER_PARSER_H
#define SPN_SEMVER_PARSER_H

#include "sp.h"
#include "spn.h"

#include "semver/types.h"

typedef struct {
  sp_str_t str;
  u32 it;
} spn_semver_parser_t;

c8   spn_semver_parser_peek(spn_semver_parser_t* parser);
void spn_semver_parser_eat(spn_semver_parser_t* parser);
bool spn_semver_parser_is_digit(c8 c);
bool spn_semver_parser_is_whitespace(c8 c);
bool spn_semver_parser_is_done(spn_semver_parser_t* parser);
void spn_semver_parser_eat_whitespace(spn_semver_parser_t* parser);
u32  spn_semver_parser_parse_number(spn_semver_parser_t* parser);
spn_semver_parsed_t spn_semver_parser_parse(spn_semver_parser_t* parser);
spn_err_t spn_semver_parse_range(sp_str_t str, spn_semver_range_t* range);

#endif
