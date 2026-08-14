#include "semver/convert.h"
#include "semver/parser.h"
#include "sp/macro.h"

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

spn_semver_range_t spn_semver_tilde_to_range(spn_semver_parsed_t parsed) {
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

spn_semver_range_t spn_semver_wildcard_to_range(spn_semver_parsed_t parsed) {
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

spn_semver_range_t spn_semver_comparison_to_range(spn_semver_op_t op, spn_semver_t version) {
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
    case SPN_SEMVER_OP_EQ: return sp_str_lit("=");
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

spn_semver_t spn_semver_from_str(sp_str_t str) {
  spn_semver_parser_t parser = {
    .str = str
  };
  spn_semver_parsed_t parsed = spn_semver_parser_parse(&parser);
  return parsed.version;
}

sp_str_t spn_semver_to_str(sp_mem_t mem, spn_semver_t version) {
  return sp_fmt(
    mem,
    "{}.{}.{}",
    SP_FMT_U32(version.major),
    SP_FMT_U32(version.minor),
    SP_FMT_U32(version.patch)
  ).value;
}

static bool semver_version_eq(spn_semver_t a, spn_semver_t b) {
  return a.major == b.major && a.minor == b.minor && a.patch == b.patch;
}

// The parser normalizes every expression into low/high bounds, so rendering
// must reconstruct the surface form from the bounds; printing the low bound
// alone turns =1.0.0 into >=1.0.0 and <2.0.0 into >=0.0.0
sp_str_t spn_semver_range_to_str(sp_mem_t mem, spn_semver_range_t range) {
  spn_semver_t max = { SP_LIMIT_U32_MAX, SP_LIMIT_U32_MAX, SP_LIMIT_U32_MAX };
  spn_semver_t zero = SP_ZERO_STRUCT(spn_semver_t);

  switch (range.mod) {
    case SPN_SEMVER_MOD_CMP: {
      if (semver_version_eq(range.low.version, range.high.version)) {
        return sp_fmt(mem, "={}", SP_FMT_STR(spn_semver_to_str(mem, range.low.version))).value;
      }
      if (semver_version_eq(range.low.version, zero) && range.low.op == SPN_SEMVER_OP_GEQ) {
        return sp_fmt(
          mem,
          "{}{}",
          SP_FMT_STR(spn_semver_op_to_str(range.high.op)),
          SP_FMT_STR(spn_semver_to_str(mem, range.high.version))
        ).value;
      }
      return sp_fmt(
        mem,
        "{}{}",
        SP_FMT_STR(spn_semver_op_to_str(range.low.op)),
        SP_FMT_STR(spn_semver_to_str(mem, range.low.version))
      ).value;
    }
    case SPN_SEMVER_MOD_WILDCARD: {
      if (semver_version_eq(range.low.version, zero) && semver_version_eq(range.high.version, max)) {
        return sp_str_lit("*");
      }
      if (range.low.version.minor == 0 && range.low.version.patch == 0 && range.high.version.major == range.low.version.major + 1) {
        return sp_fmt(mem, "{}.*", SP_FMT_U32(range.low.version.major)).value;
      }
      return sp_fmt(mem, "{}.{}.*", SP_FMT_U32(range.low.version.major), SP_FMT_U32(range.low.version.minor)).value;
    }
    case SPN_SEMVER_MOD_TILDE: {
      // ~N spans a whole major (>=N.0.0 <N+1.0.0), which ~N.0.0 does not
      if (range.low.version.minor == 0 && range.low.version.patch == 0 && range.high.version.major == range.low.version.major + 1) {
        return sp_fmt(mem, "~{}", SP_FMT_U32(range.low.version.major)).value;
      }
      break;
    }
    case SPN_SEMVER_MOD_CARET:
    case SPN_SEMVER_MOD_NONE: {
      break;
    }
  }

  return sp_fmt(
    mem,
    "{}{}",
    SP_FMT_STR(spn_semver_mod_to_str(range.mod, range.low.op)),
    SP_FMT_STR(spn_semver_to_str(mem, range.low.version))
  ).value;
}


