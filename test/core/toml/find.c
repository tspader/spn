#include "toml.h"

typedef struct {
  const c8* path [TOML_TEST_MAX_PATH];
  const c8* value;
  bool missing;
} expect_t;

typedef struct {
  const c8* name;
  const c8* manifest;
  expect_t expect;
} test_t;

static const test_t tests [] = {
  {
    .name = "string",
    .manifest = "simple",
    .expect = { .path = { "deps", "package", "foo" }, .value = "1.0.0" },
  },
  {
    .name = "decodes_escapes",
    .manifest = "escapes",
    .expect = { .path = { "foo" }, .value = "a\"b\nc \xc3\xa9" },
  },
  {
    .name = "literal_verbatim",
    .manifest = "literal",
    .expect = { .path = { "foo" }, .value = "c:\\path" },
  },
  {
    .name = "quoted_key",
    .manifest = "quoted_key",
    .expect = { .path = { "deps", "package", "spader/sp" }, .value = "3.0.0" },
  },
  {
    .name = "dotted_entry",
    .manifest = "dotted",
    .expect = { .path = { "deps", "package", "foo" }, .value = "1.0.0" },
  },
  {
    .name = "inline_table_version",
    .manifest = "inline_table",
    .expect = { .path = { "deps", "package", "foo", "version" }, .value = "1.0.0" },
  },
  {
    .name = "missing",
    .manifest = "simple",
    .expect = { .path = { "deps", "package", "bar" }, .missing = true },
  },
  {
    .name = "scalar_raw",
    .manifest = "scalar",
    .expect = { .path = { "foo" }, .value = "42" },
  },
  {
    .name = "multiline_string",
    .manifest = "multiline",
    .expect = { .path = { "foo" }, .value = "hello\n" },
  },
  {
    .name = "multiline_empty",
    .manifest = "multiline_empty",
    .expect = { .path = { "foo" }, .value = "" },
  },
};

sp_test_each(toml_edit, find, test_t, tests) {
  sp_mem_t mem = sp_test_arena(t);
  sp_str_t source = sp_zero;
  if (toml_read_source(t, it->manifest, SP_NULLPTR, &source)) return SP_ERR;

  spn_toml_edit_t edit = sp_zero;
  sp_must_eq(t, SPN_OK, spn_toml_edit_init(&edit, mem, source));

  sp_str_t path [TOML_TEST_MAX_PATH] = sp_zero;
  u32 num_segments = toml_collect_path(it->expect.path, path);
  spn_toml_edit_entry_t* entry = spn_toml_edit_find(&edit, path, num_segments);
  if (it->expect.missing) {
    sp_expect(t, entry == SP_NULLPTR);
  }
  else {
    sp_must(t, entry != SP_NULLPTR);
    sp_expect_str_eq(t, spn_toml_edit_entry_str(&edit, entry), sp_str_view(it->expect.value));
  }

  sp_expect_str_eq(t, spn_toml_edit_render(&edit, mem), source);

  return SP_OK;
}
