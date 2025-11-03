#include "utest.h"


  sp_str_t versions [] = {
    sp_str_lit("1.2.3"),
    sp_str_lit("^1.2.3"),
    sp_str_lit("1.2"),
    sp_str_lit("1"),
    sp_str_lit("0.2.3"),
    sp_str_lit("0.2"),
    sp_str_lit("0.0.3"),
    sp_str_lit("0.0"),
    sp_str_lit("0"),
    sp_str_lit("~1.2.3"),
    sp_str_lit("~1.2"),
    sp_str_lit("~1"),
    sp_str_lit("*"),
    sp_str_lit("1.*"),
    sp_str_lit("1.2.*"),
    sp_str_lit(">= 1.2.0"),
    sp_str_lit("> 1"),
    sp_str_lit("< 2"),
    sp_str_lit("= 1.2.3"),
  };
  SP_CARR_FOR(versions, it) {
    sp_str_t version = versions[it];
    spn_semver_range_t range = spn_semver_range_from_str(version);
    SP_LOG(
      "{:fg brightblack} -> {:fg brightgreen}{:fg cyan}.{:fg cyan}.{:fg cyan}, {:fg brightgreen}{:fg cyan}.{:fg cyan}.{:fg cyan}",
      SP_FMT_STR(sp_str_pad(version, 12)),
      SP_FMT_STR(spn_semver_op_to_str(range.low.op)),
      SP_FMT_U32(range.low.version.major),
      SP_FMT_U32(range.low.version.minor),
      SP_FMT_U32(range.low.version.patch),
      SP_FMT_STR(spn_semver_op_to_str(range.high.op)),
      SP_FMT_U32(range.high.version.major),
      SP_FMT_U32(range.high.version.minor),
      SP_FMT_U32(range.high.version.patch)
    );
  }
  SP_EXIT_SUCCESS();
