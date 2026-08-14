#pragma once

#include "spn_test.h"

#include "ctx/types.h"
#include "sp.h"
#include "spn/core.h"
#include "external/git.h"
#include "index/index.h"
#include "index/json.h"
#include "semver/compare.h"
#include "semver/convert.h"
#include "semver/parser.h"
#include "semver/types.h"
#include "sp/io.h"
#include "sp/str.h"

static inline u32 index_test_count_lines(sp_str_t content) {
  u32 count = 0;
  sp_str_for_line(content, it) {
    if (!sp_str_empty(sp_str_trim(it.line))) {
      count++;
    }
  }
  return count;
}
