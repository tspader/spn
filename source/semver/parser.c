#include "semver/parser.h"
#include "semver/convert.h"
#include "semver/compare.h"
#include "sp/macro.h"

c8 spn_semver_parser_peek(spn_semver_parser_t* parser) {
  if (spn_semver_parser_is_done(parser)) return '\0';
  return sp_str_at(parser->str, parser->it);
}

void spn_semver_parser_eat(spn_semver_parser_t* parser) {
  parser->it++;
}

bool spn_semver_parser_is_digit(c8 c) {
  return c >= '0' && c <= '9';
}

bool spn_semver_parser_is_whitespace(c8 c) {
  return c == ' ' || c == '\t' || c == '\n';
}

bool spn_semver_parser_is_done(spn_semver_parser_t* parser) {
  return parser->it >= parser->str.len;
}

void spn_semver_parser_eat_whitespace(spn_semver_parser_t* parser) {
  while (true) {
    if (spn_semver_parser_is_done(parser)) break;
    if (!spn_semver_parser_is_whitespace(spn_semver_parser_peek(parser))) break;

    spn_semver_parser_eat(parser);
  }
}

u32 spn_semver_parser_parse_number(spn_semver_parser_t* parser) {
  u32 result = 0;
  while (true) {
    if (spn_semver_parser_is_done(parser)) break;
    if (!spn_semver_parser_is_digit(spn_semver_parser_peek(parser))) break;

    c8 c = spn_semver_parser_peek(parser);
    result = result * 10 + (c - '0');
    spn_semver_parser_eat(parser);
  }

  return result;
}

spn_semver_parsed_t spn_semver_parser_parse(spn_semver_parser_t* parser) {
  spn_semver_parsed_t parsed = sp_zero;

  parsed.version.major = spn_semver_parser_parse_number(parser);
  parsed.components.major = true;

  if (spn_semver_parser_is_done(parser)) return parsed;
  if (spn_semver_parser_peek(parser) != '.') return parsed;

  spn_semver_parser_eat(parser);
  parsed.version.minor = spn_semver_parser_parse_number(parser);
  parsed.components.minor = true;

  if (spn_semver_parser_is_done(parser)) return parsed;
  if (spn_semver_parser_peek(parser) != '.') return parsed;

  spn_semver_parser_eat(parser);
  parsed.version.patch = spn_semver_parser_parse_number(parser);
  parsed.components.patch = true;

  return parsed;
}

static bool spn_semver_parse_component(spn_semver_parser_t* parser, u32* value) {
  u32 start = parser->it;
  *value = spn_semver_parser_parse_number(parser);
  return parser->it != start;
}

static bool spn_semver_parse_version(spn_semver_parser_t* parser, spn_semver_parsed_t* parsed, bool allow_wildcard, bool* wildcard) {
  if (!spn_semver_parse_component(parser, &parsed->version.major)) {
    return false;
  }
  parsed->components.major = true;

  if (spn_semver_parser_peek(parser) != '.') {
    return true;
  }
  spn_semver_parser_eat(parser);

  if (allow_wildcard && spn_semver_parser_peek(parser) == '*') {
    spn_semver_parser_eat(parser);
    *wildcard = true;
    return true;
  }
  if (!spn_semver_parse_component(parser, &parsed->version.minor)) {
    return false;
  }
  parsed->components.minor = true;

  if (spn_semver_parser_peek(parser) != '.') {
    return true;
  }
  spn_semver_parser_eat(parser);

  if (allow_wildcard && spn_semver_parser_peek(parser) == '*') {
    spn_semver_parser_eat(parser);
    *wildcard = true;
    return true;
  }
  if (!spn_semver_parse_component(parser, &parsed->version.patch)) {
    return false;
  }
  parsed->components.patch = true;

  return true;
}

spn_err_t spn_semver_parse_range(sp_str_t str, spn_semver_range_t* range) {
  spn_semver_parser_t parser = { .str = str, .it = 0 };

  spn_semver_parser_eat_whitespace(&parser);

  spn_semver_parsed_t parsed = sp_zero;
  bool wildcard = false;
  c8 c = spn_semver_parser_peek(&parser);

  if (c == '*') {
    spn_semver_parser_eat(&parser);
    *range = spn_semver_wildcard_to_range((spn_semver_parsed_t) sp_zero);
  }
  else if (c == '^' || c == '~') {
    spn_semver_parser_eat(&parser);
    spn_semver_parser_eat_whitespace(&parser);
    if (!spn_semver_parse_version(&parser, &parsed, false, &wildcard)) {
      return SPN_ERROR;
    }
    *range = (c == '^') ? spn_semver_caret_to_range(parsed) : spn_semver_tilde_to_range(parsed);
  }
  else if (c == '>' || c == '<' || c == '=') {
    spn_semver_op_t op = SPN_SEMVER_OP_EQ;
    spn_semver_parser_eat(&parser);
    if (c == '>') {
      op = SPN_SEMVER_OP_GT;
      if (spn_semver_parser_peek(&parser) == '=') {
        spn_semver_parser_eat(&parser);
        op = SPN_SEMVER_OP_GEQ;
      }
    }
    else if (c == '<') {
      op = SPN_SEMVER_OP_LT;
      if (spn_semver_parser_peek(&parser) == '=') {
        spn_semver_parser_eat(&parser);
        op = SPN_SEMVER_OP_LEQ;
      }
    }

    spn_semver_parser_eat_whitespace(&parser);
    if (!spn_semver_parse_version(&parser, &parsed, false, &wildcard)) {
      return SPN_ERROR;
    }
    *range = spn_semver_comparison_to_range(op, parsed.version);
  }
  else if (spn_semver_parser_is_digit(c)) {
    if (!spn_semver_parse_version(&parser, &parsed, true, &wildcard)) {
      return SPN_ERROR;
    }
    *range = wildcard ? spn_semver_wildcard_to_range(parsed) : spn_semver_caret_to_range(parsed);
  }
  else {
    return SPN_ERROR;
  }

  spn_semver_parser_eat_whitespace(&parser);
  return spn_semver_parser_is_done(&parser) ? SPN_OK : SPN_ERROR;
}
