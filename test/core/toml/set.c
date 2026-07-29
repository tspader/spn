#include "toml.h"

typedef struct {
  const c8* path [TOML_TEST_MAX_PATH];
  const c8* value;
  spn_err_t err;
} set_t;

// Exactly one of golden/output, or neither: neither means the edit must
// render back to the source unchanged
typedef struct {
  const c8* golden;
  const c8* output;
} expect_t;

typedef struct {
  const c8* name;
  const c8* manifest;
  const c8* toml;
  set_t set [TOML_TEST_MAX_SETS];
  expect_t expect;
} test_t;

static sp_err_t run_set_test(sp_test_t* t, test_t* it) {
  sp_mem_t mem = sp_test_arena(t);
  sp_str_t source = sp_zero;
  if (toml_read_source(t, it->manifest, it->toml, &source)) return SP_ERR;

  spn_toml_edit_t edit = sp_zero;
  sp_must_eq(t, SPN_OK, spn_toml_edit_init(&edit, mem, source));

  sp_carr_for(it->set, at) {
    if (!it->set[at].value) break;
    sp_str_t path [TOML_TEST_MAX_PATH] = sp_zero;
    u32 num_segments = toml_collect_path(it->set[at].path, path);
    sp_expect_eq(t, it->set[at].err, spn_toml_edit_set_str(&edit, path, num_segments, sp_str_view(it->set[at].value)));
  }

  sp_str_t rendered = spn_toml_edit_render(&edit, mem);
  if (it->expect.golden) {
    sp_expect_golden(t, sp_test_format(t, "golden/{}.toml", sp_fmt_cstr(it->expect.golden)), rendered);
  }
  else if (it->expect.output) {
    sp_expect_str_eq(t, rendered, sp_str_view(it->expect.output));
  }
  else {
    sp_expect_str_eq(t, rendered, source);
  }

  return SP_OK;
}

static const test_t replace_tests [] = {
  {
    .name = "keeps_comment",
    .manifest = "commented",
    .set = { { .path = { "deps", "package", "foo" }, .value = "2.0.0" } },
    .expect = { .golden = "set_replace_keeps_comment" },
  },
  {
    .name = "keeps_spacing",
    .manifest = "spacing",
    .set = { { .path = { "foo" }, .value = "2.0.0" } },
    .expect = { .golden = "set_replace_keeps_spacing" },
  },
  {
    .name = "inline_table_wholesale",
    .manifest = "inline_table",
    .set = { { .path = { "deps", "package", "foo" }, .value = "2.0.0" } },
    .expect = { .golden = "set_replace_inline_table_wholesale" },
  },
  {
    .name = "version_in_inline_table",
    .manifest = "inline_table",
    .set = { { .path = { "deps", "package", "foo", "version" }, .value = "2.0.0" } },
    .expect = { .golden = "set_replace_version_in_inline_table" },
  },
  {
    .name = "escapes_value",
    .manifest = "string",
    .set = { { .path = { "foo" }, .value = "say \"hi\"\n" } },
    .expect = { .golden = "set_escapes_value" },
  },
};

static const test_t insert_tests [] = {
  {
    .name = "into_table",
    .manifest = "two_sections",
    .set = { { .path = { "deps", "package", "bar" }, .value = "2.0.0" } },
    .expect = { .golden = "set_insert_into_table" },
  },
  {
    .name = "before_trailing_comment",
    .manifest = "trailing_comment",
    .set = { { .path = { "deps", "package", "bar" }, .value = "2.0.0" } },
    .expect = { .golden = "set_insert_before_trailing_comment" },
  },
  {
    .name = "into_empty_table",
    .manifest = "empty_table",
    .set = { { .path = { "deps", "package", "foo" }, .value = "1.0.0" } },
    .expect = { .golden = "set_insert_into_empty_table" },
  },
  {
    .name = "quoted_key",
    .manifest = "simple",
    .set = { { .path = { "deps", "package", "spader/sp" }, .value = "3.0.0" } },
    .expect = { .golden = "set_insert_quoted_key" },
  },
  {
    .name = "dotted_sibling",
    .manifest = "dotted",
    .set = { { .path = { "deps", "package", "bar" }, .value = "2.0.0" } },
    .expect = { .golden = "set_insert_dotted_sibling" },
  },
  {
    .name = "into_inline_table",
    .manifest = "inline_private",
    .set = { { .path = { "foo", "version" }, .value = "1.0.0" } },
    .expect = { .golden = "set_insert_into_inline_table" },
  },
  {
    .name = "into_empty_inline_table",
    .manifest = "inline_empty",
    .set = { { .path = { "foo", "version" }, .value = "1.0.0" } },
    .expect = { .golden = "set_insert_into_empty_inline_table" },
  },
  {
    .name = "crlf",
    .toml = "[deps.package]\r\nfoo = \"1.0.0\"\r\n",
    .set = { { .path = { "deps", "package", "bar" }, .value = "2.0.0" } },
    .expect = { .output = "[deps.package]\r\nfoo = \"1.0.0\"\r\nbar = \"2.0.0\"\r\n" },
  },
  {
    .name = "no_trailing_newline",
    .toml = "[deps.package]\nfoo = \"1.0.0\"",
    .set = { { .path = { "deps", "package", "bar" }, .value = "2.0.0" } },
    .expect = { .output = "[deps.package]\nfoo = \"1.0.0\"\nbar = \"2.0.0\"\n" },
  },
  {
    .name = "duplicate_sections_uses_last",
    .manifest = "duplicate_sections",
    .set = { { .path = { "deps", "package", "bar" }, .value = "2.0.0" } },
    .expect = { .golden = "set_insert_duplicate_sections_uses_last" },
  },
};

static const test_t create_table_tests [] = {
  {
    .name = "after_package",
    .manifest = "package",
    .set = { { .path = { "deps", "package", "foo" }, .value = "1.0.0" } },
    .expect = { .golden = "set_create_table" },
  },
  {
    .name = "no_trailing_newline",
    .toml = "[package]\nname = \"test\"",
    .set = { { .path = { "deps", "package", "foo" }, .value = "1.0.0" } },
    .expect = { .output = "[package]\nname = \"test\"\n\n[deps.package]\nfoo = \"1.0.0\"\n" },
  },
  {
    .name = "crlf",
    .toml = "[package]\r\nname = \"test\"\r\n",
    .set = { { .path = { "deps", "package", "foo" }, .value = "1.0.0" } },
    .expect = { .output = "[package]\r\nname = \"test\"\r\n\r\n[deps.package]\r\nfoo = \"1.0.0\"\r\n" },
  },
  {
    .name = "empty_file",
    .toml = "",
    .set = { { .path = { "deps", "package", "foo" }, .value = "1.0.0" } },
    .expect = { .golden = "set_create_table_empty_file" },
  },
  {
    .name = "skips_array_sections",
    .manifest = "array_section",
    .set = { { .path = { "deps", "package", "foo" }, .value = "1.0.0" } },
    .expect = { .golden = "set_create_table_skips_array_sections" },
  },
};

static const test_t error_tests [] = {
  {
    .name = "same_key_twice",
    .manifest = "string",
    .set = {
      { .path = { "foo" }, .value = "2.0.0" },
      { .path = { "foo" }, .value = "3.0.0", .err = SPN_ERROR },
    },
    .expect = { .golden = "set_same_key_twice_errors" },
  },
  {
    .name = "subpath_of_scalar",
    .manifest = "array",
    .set = { { .path = { "foo", "bar" }, .value = "1.0.0", .err = SPN_ERROR } },
  },
  {
    .name = "into_array_of_tables",
    .manifest = "array_section",
    .set = { { .path = { "test", "flags" }, .value = "x", .err = SPN_ERROR } },
  },
  {
    .name = "empty_inline_table_twice",
    .manifest = "inline_empty",
    .set = {
      { .path = { "foo", "version" }, .value = "1.0.0" },
      { .path = { "foo", "private" }, .value = "x", .err = SPN_ERROR },
    },
    .expect = { .golden = "set_insert_into_empty_inline_table" },
  },
};

static const test_t multi_tests [] = {
  {
    .name = "ordered_inserts",
    .manifest = "simple",
    .set = {
      { .path = { "deps", "package", "bar" }, .value = "1.0.0" },
      { .path = { "deps", "package", "baz" }, .value = "2.0.0" },
      { .path = { "deps", "package", "qux" }, .value = "3.0.0" },
      { .path = { "deps", "package", "foo" }, .value = "1.1.0" },
    },
    .expect = { .golden = "set_ordered_inserts" },
  },
  {
    .name = "replace_and_insert",
    .manifest = "two_keys",
    .set = {
      { .path = { "deps", "package", "foo" }, .value = "1.1.0" },
      { .path = { "deps", "package", "baz" }, .value = "3.0.0" },
    },
    .expect = { .golden = "set_multiple" },
  },
};

sp_test_each_fn(toml_edit, set_replace, test_t, replace_tests, run_set_test);
sp_test_each_fn(toml_edit, set_insert, test_t, insert_tests, run_set_test);
sp_test_each_fn(toml_edit, set_create_table, test_t, create_table_tests, run_set_test);
sp_test_each_fn(toml_edit, set_error, test_t, error_tests, run_set_test);
sp_test_each_fn(toml_edit, set_multi, test_t, multi_tests, run_set_test);
