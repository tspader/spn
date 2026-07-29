#include "toml.h"

typedef struct {
  const c8* name;
  const c8* toml;
} test_t;

static const test_t tests [] = {
  {
    .name = "missing_equals",
    .toml = r("foo"),
  },
  {
    .name = "unterminated_string",
    .toml = r("foo = \"1.0.0"),
  },
  {
    .name = "unclosed_array",
    .toml = r("foo = [1, 2"),
  },
  {
    .name = "unclosed_inline_table",
    .toml = r("foo = { version = \"1.0.0\""),
  },
  {
    .name = "bad_escape",
    .toml = r("foo = \"\\x41\""),
  },
  {
    .name = "garbage_after_value",
    .toml = r("foo = \"1.0.0\" garbage"),
  },
  {
    .name = "unclosed_header",
    .toml = r("[deps.package"),
  },
  {
    .name = "unicode_bad_digits",
    .toml = r("foo = \"\\uZZZZ\""),
  },
  {
    .name = "unicode_short",
    .toml = r("foo = \"\\u00e\""),
  },
  {
    .name = "unicode_out_of_range",
    .toml = r("foo = \"\\UFFFFFFFF\""),
  },
};

sp_test_each(toml_edit, parse_error, test_t, tests) {
  spn_toml_edit_t edit = sp_zero;
  sp_expect_eq(t, SPN_ERR_TOML_PARSE, spn_toml_edit_init(&edit, sp_test_arena(t), sp_str_view(it->toml)));
  return SP_OK;
}
