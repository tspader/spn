#include "semver.h"

#include "sp/macro.h"
#include "sp/str.h"

spn_semver_range_t spn_semver_caret_to_range(spn_semver_parsed_t parsed) {
  spn_semver_range_t range = {
    .mod = SPN_SEMVER_MOD_CARET
  };

  if (parsed.version.major > 0) {
    range.low.op = SPN_SEMVER_OP_GEQ;
    range.low.version = parsed.version;
    range.high.op = SPN_SEMVER_OP_LT;
    range.high.version.major = parsed.version.major + 1;
    range.high.version.minor = 0;
    range.high.version.patch = 0;
  } else if (parsed.version.minor > 0) {
    range.low.op = SPN_SEMVER_OP_GEQ;
    range.low.version = parsed.version;
    range.high.op = SPN_SEMVER_OP_LT;
    range.high.version.major = parsed.version.major;
    range.high.version.minor = parsed.version.minor + 1;
    range.high.version.patch = 0;
  } else {
    range.low.op = SPN_SEMVER_OP_GEQ;
    range.low.version = parsed.version;
    range.high.op = SPN_SEMVER_OP_LT;
    range.high.version.major = parsed.version.major;
    range.high.version.minor = parsed.version.minor;
    range.high.version.patch = parsed.version.patch + 1;
  }

  return range;
}

static spn_semver_range_t spn_semver_tilde_to_range(spn_semver_parsed_t parsed) {
  spn_semver_range_t range = {
    .mod = SPN_SEMVER_MOD_TILDE
  };

  if (parsed.components.patch) {
    range.low.op = SPN_SEMVER_OP_GEQ;
    range.low.version = parsed.version;
    range.high.op = SPN_SEMVER_OP_LT;
    range.high.version.major = parsed.version.major;
    range.high.version.minor = parsed.version.minor + 1;
    range.high.version.patch = 0;
  } else if (parsed.components.minor) {
    range.low.op = SPN_SEMVER_OP_GEQ;
    range.low.version = parsed.version;
    range.high.op = SPN_SEMVER_OP_LT;
    range.high.version.major = parsed.version.major;
    range.high.version.minor = parsed.version.minor + 1;
    range.high.version.patch = 0;
  } else {
    range.low.op = SPN_SEMVER_OP_GEQ;
    range.low.version = parsed.version;
    range.high.op = SPN_SEMVER_OP_LT;
    range.high.version.major = parsed.version.major + 1;
    range.high.version.minor = 0;
    range.high.version.patch = 0;
  }

  return range;
}

static spn_semver_range_t spn_semver_wildcard_to_range(spn_semver_parsed_t parsed) {
  spn_semver_range_t range = {
    .mod = SPN_SEMVER_MOD_WILDCARD
  };

  if (!parsed.components.major) {
    range.low.op = SPN_SEMVER_OP_GEQ;
    range.low.version.major = 0;
    range.low.version.minor = 0;
    range.low.version.patch = 0;
    range.high.op = SPN_SEMVER_OP_LT;
    range.high.version.major = 0xFFFFFFFF;
    range.high.version.minor = 0xFFFFFFFF;
    range.high.version.patch = 0xFFFFFFFF;
  } else if (!parsed.components.minor) {
    range.low.op = SPN_SEMVER_OP_GEQ;
    range.low.version.major = parsed.version.major;
    range.low.version.minor = 0;
    range.low.version.patch = 0;
    range.high.op = SPN_SEMVER_OP_LT;
    range.high.version.major = parsed.version.major + 1;
    range.high.version.minor = 0;
    range.high.version.patch = 0;
  } else {
    range.low.op = SPN_SEMVER_OP_GEQ;
    range.low.version.major = parsed.version.major;
    range.low.version.minor = parsed.version.minor;
    range.low.version.patch = 0;
    range.high.op = SPN_SEMVER_OP_LT;
    range.high.version.major = parsed.version.major;
    range.high.version.minor = parsed.version.minor + 1;
    range.high.version.patch = 0;
  }

  return range;
}

static spn_semver_range_t spn_semver_comparison_to_range(spn_semver_op_t op, spn_semver_t version) {
  spn_semver_range_t range = {
    .mod = SPN_SEMVER_MOD_CMP
  };

  switch (op) {
    case SPN_SEMVER_OP_EQ: {
      range.low.op = SPN_SEMVER_OP_GEQ;
      range.low.version = version;
      range.high.op = SPN_SEMVER_OP_LEQ;
      range.high.version = version;
      break;
    }
    case SPN_SEMVER_OP_GEQ: {
      range.low.op = SPN_SEMVER_OP_GEQ;
      range.low.version = version;
      range.high.op = SPN_SEMVER_OP_LEQ;
      range.high.version = (spn_semver_t){SP_LIMIT_U32_MAX, SP_LIMIT_U32_MAX, SP_LIMIT_U32_MAX};
      break;
    }
    case SPN_SEMVER_OP_GT: {
      range.low.op = SPN_SEMVER_OP_GT;
      range.low.version = version;
      range.high.op = SPN_SEMVER_OP_LEQ;
      range.high.version = (spn_semver_t){SP_LIMIT_U32_MAX, SP_LIMIT_U32_MAX, SP_LIMIT_U32_MAX};
      break;
    }
    case SPN_SEMVER_OP_LEQ: {
      range.low.op = SPN_SEMVER_OP_GEQ;
      range.low.version = SP_ZERO_STRUCT(spn_semver_t);
      range.high.op = SPN_SEMVER_OP_LEQ;
      range.high.version = version;
      break;
    }
    case SPN_SEMVER_OP_LT: {
      range.low.op = SPN_SEMVER_OP_GEQ;
      range.low.version = SP_ZERO_STRUCT(spn_semver_t);
      range.high.op = SPN_SEMVER_OP_LT;
      range.high.version = version;
      break;
    }
  }

  return range;
}

static sp_str_t spn_semver_op_to_str(spn_semver_op_t op) {
  switch (op) {
    case SPN_SEMVER_OP_EQ: return sp_str_lit("==");
    case SPN_SEMVER_OP_GEQ: return sp_str_lit(">=");
    case SPN_SEMVER_OP_GT: return sp_str_lit(">");
    case SPN_SEMVER_OP_LEQ: return sp_str_lit("<=");
    case SPN_SEMVER_OP_LT: return sp_str_lit("<");
  }
  SP_UNREACHABLE_RETURN(sp_str_lit(""));
}

static sp_str_t spn_semver_mod_to_str(spn_semver_mod_t mod, spn_semver_op_t op) {
  switch (mod) {
    case SPN_SEMVER_MOD_TILDE: return sp_str_lit("~");
    case SPN_SEMVER_MOD_CARET: return sp_str_lit("^");
    case SPN_SEMVER_MOD_WILDCARD: return sp_str_lit("*");
    case SPN_SEMVER_MOD_CMP: return spn_semver_op_to_str(op);
    case SPN_SEMVER_MOD_NONE: return sp_str_lit("");
  }
  SP_UNREACHABLE_RETURN(sp_str_lit(""));
}

c8 spn_semver_parser_peek(spn_semver_parser_t* parser) {
  if (spn_semver_parser_is_done(parser)) return '\0';
  return sp_str_at(parser->str, parser->it);
}

void spn_semver_parser_eat(spn_semver_parser_t* parser) {
  parser->it++;
}

void spn_semver_parser_eat_and_assert(spn_semver_parser_t* parser, c8 c) {
  SP_ASSERT(spn_semver_parser_peek(parser) == c);
  spn_semver_parser_eat(parser);
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

spn_semver_parsed_t spn_semver_parser_parse_version(spn_semver_parser_t* parser) {
  spn_semver_parsed_t parsed = SP_ZERO_INITIALIZE();

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

spn_semver_range_t spn_semver_range_from_str(sp_str_t str) {
  spn_semver_parser_t parser = { .str = str, .it = 0 };
  spn_semver_range_t range = {0};

  spn_semver_parser_eat_whitespace(&parser);

  c8 c = spn_semver_parser_peek(&parser);

  if (c == '^') {
    spn_semver_parser_eat(&parser);
    spn_semver_parsed_t parsed = spn_semver_parser_parse_version(&parser);
    range = spn_semver_caret_to_range(parsed);
  }
  else if (c == '~') {
    spn_semver_parser_eat(&parser);
    spn_semver_parsed_t parsed = spn_semver_parser_parse_version(&parser);
    range = spn_semver_tilde_to_range(parsed);
  }
  else if (c == '*') {
    spn_semver_parser_eat(&parser);
    range = spn_semver_wildcard_to_range((spn_semver_parsed_t){0});
  }
  else if (spn_semver_parser_is_digit(c)) {
    u32 saved_it = parser.it;
    spn_semver_parsed_t parsed = spn_semver_parser_parse_version(&parser);

    if (!spn_semver_parser_is_done(&parser)) {
      c8 next = spn_semver_parser_peek(&parser);
      if (next == '.') {
        spn_semver_parser_eat(&parser);
        SP_ASSERT(!spn_semver_parser_is_done(&parser));
        if (spn_semver_parser_peek(&parser) == '*') {
          spn_semver_parser_eat(&parser);
          range = spn_semver_wildcard_to_range(parsed);
          return range;
        }
        parser.it = saved_it;
      }
    }

    parser.it = saved_it;
    parsed = spn_semver_parser_parse_version(&parser);
    range = spn_semver_caret_to_range(parsed);
  }
  else if (c == '>' || c == '<' || c == '=') {
    spn_semver_op_t op;
    if (c == '>') {
      spn_semver_parser_eat(&parser);

      bool done = spn_semver_parser_is_done(&parser);
      if (done) {
        op = SPN_SEMVER_OP_GT;
      }
      else if (spn_semver_parser_peek(&parser) == '=') {
        spn_semver_parser_eat(&parser);
        op = SPN_SEMVER_OP_GEQ;
      }
      else {
        op = SPN_SEMVER_OP_GT;
      }
    }
    else if (c == '<') {
      spn_semver_parser_eat(&parser);

      bool done = spn_semver_parser_is_done(&parser);
      if (done) {
        op = SPN_SEMVER_OP_LT;
      }
      else if (spn_semver_parser_peek(&parser) == '=') {
        spn_semver_parser_eat(&parser);
        op = SPN_SEMVER_OP_LEQ;
      }
      else {
        op = SPN_SEMVER_OP_LT;
      }
    }
    else {
      spn_semver_parser_eat(&parser);
      op = SPN_SEMVER_OP_EQ;
    }

    spn_semver_parser_eat_whitespace(&parser);
    SP_ASSERT(!spn_semver_parser_is_done(&parser));
    spn_semver_parsed_t parsed = spn_semver_parser_parse_version(&parser);
    range = spn_semver_comparison_to_range(op, parsed.version);
  }
  else {
    SP_FATAL("failed to parse version: {:fg brightred}", SP_FMT_QSTR(str));
  }

  return range;
}

spn_semver_t spn_semver_from_str(sp_str_t str) {
  spn_semver_parser_t parser = {
    .str = str
  };
  spn_semver_parsed_t parsed = spn_semver_parser_parse_version(&parser);
  return parsed.version;
}

sp_str_t spn_semver_to_str(spn_semver_t version) {
  return sp_format(
    "{}.{}.{}",
    SP_FMT_U32(version.major),
    SP_FMT_U32(version.minor),
    SP_FMT_U32(version.patch)
  );
}

sp_str_t spn_semver_range_to_str(spn_semver_range_t range) {
  return sp_format(
    "{}{}",
    SP_FMT_STR(spn_semver_mod_to_str(range.mod, range.low.op)),
    SP_FMT_STR(spn_semver_to_str(range.low.version))
  );
}

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
