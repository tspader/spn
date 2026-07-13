#define SP_IMPLEMENTATION
#include "sp.h"

#define UTEST_IMPLEMENTATION
#include "utest.h"

#include "test.h"
#include "spn.h"
#include "toml/edit.h"

UTEST_MAIN();

#define TOML_TEST_MAX_SETS 4
#define TOML_TEST_MAX_PATH 4

typedef struct {
  const c8* path [TOML_TEST_MAX_PATH];
  const c8* value;
  spn_err_t err;
} toml_set_t;

typedef struct {
  const c8* path [TOML_TEST_MAX_PATH];
  const c8* value;
  bool missing;
} toml_get_t;

typedef struct {
  spn_err_t err;
  const c8* golden;
  const c8* output;
  toml_get_t get;
} toml_expect_t;

typedef struct {
  const c8* manifest;
  const c8* toml;
  toml_set_t set [TOML_TEST_MAX_SETS];
  toml_expect_t expect;
} toml_test_t;

static sp_str_t toml_test_fixture(sp_mem_t mem, const c8* dir, const c8* name) {
  sp_str_t file = sp_fmt(mem, "{}.toml", sp_fmt_cstr(name)).value;
  sp_str_t path = sp_fs_join_path(mem, test_repo_path(mem, sp_str_view(dir)), file);
  sp_str_t content = sp_zero;
  sp_assert(sp_io_read_file(mem, path, &content) == SP_OK);
  return content;
}

static u32 collect_path(const c8* const* segments, sp_str_t* path) {
  u32 num_segments = 0;
  sp_for(it, TOML_TEST_MAX_PATH) {
    if (!segments[it]) break;
    path[num_segments++] = sp_str_view(segments[it]);
  }
  return num_segments;
}

static void run_toml_test(s32* utest_result, toml_test_t t) {
  sp_mem_t mem = sp_mem_os_new();

  sp_str_t source = t.manifest
    ? toml_test_fixture(mem, TOML_MANIFEST_DIR, t.manifest)
    : sp_str_view(t.toml);

  spn_toml_edit_t edit = sp_zero;
  spn_err_t err = spn_toml_edit_init(&edit, mem, source);
  EXPECT_EQ(t.expect.err, err);
  if (err || t.expect.err) return;

  if (t.expect.get.path[0]) {
    sp_str_t path [TOML_TEST_MAX_PATH] = sp_zero;
    u32 num_segments = collect_path(t.expect.get.path, path);
    spn_toml_edit_entry_t* entry = spn_toml_edit_find(&edit, path, num_segments);
    if (t.expect.get.missing) {
      EXPECT_TRUE(entry == SP_NULLPTR);
    }
    else {
      ASSERT_TRUE(entry != SP_NULLPTR);
      SP_EXPECT_STR_EQ(spn_toml_edit_entry_str(&edit, entry), sp_str_view(t.expect.get.value));
    }
  }

  sp_carr_for(t.set, it) {
    if (!t.set[it].value) break;
    sp_str_t path [TOML_TEST_MAX_PATH] = sp_zero;
    u32 num_segments = collect_path(t.set[it].path, path);
    EXPECT_EQ(t.set[it].err, spn_toml_edit_set_str(&edit, path, num_segments, sp_str_view(t.set[it].value)));
  }

  sp_str_t expected = source;
  if (t.expect.golden) {
    expected = toml_test_fixture(mem, TOML_GOLDEN_DIR, t.expect.golden);
  }
  else if (t.expect.output) {
    expected = sp_str_view(t.expect.output);
  }
  SP_EXPECT_STR_EQ(spn_toml_edit_render(&edit, mem), expected);
}

UTEST_EMPTY_FIXTURE(toml_edit)


// ROUND TRIP
UTEST_F(toml_edit, roundtrip_empty) {
  run_toml_test(&ur, (toml_test_t) {
    .toml = "",
  });
}

UTEST_F(toml_edit, roundtrip_manifest) {
  run_toml_test(&ur, (toml_test_t) {
    .manifest = "manifest",
  });
}

UTEST_F(toml_edit, roundtrip_strings) {
  run_toml_test(&ur, (toml_test_t) {
    .manifest = "strings",
  });
}

UTEST_F(toml_edit, roundtrip_crlf) {
  run_toml_test(&ur, (toml_test_t) {
    .toml = "[deps.package]\r\nfoo = \"1.0.0\"\r\n",
  });
}

UTEST_F(toml_edit, roundtrip_dotted_and_ws) {
  run_toml_test(&ur, (toml_test_t) {
    .manifest = "dotted_ws",
  });
}


// PARSE ERRORS
UTEST_F(toml_edit, err_missing_equals) {
  run_toml_test(&ur, (toml_test_t) {
    .toml = r("foo"),
    .expect = { .err = SPN_ERR_TOML_PARSE },
  });
}

UTEST_F(toml_edit, err_unterminated_string) {
  run_toml_test(&ur, (toml_test_t) {
    .toml = r("foo = \"1.0.0"),
    .expect = { .err = SPN_ERR_TOML_PARSE },
  });
}

UTEST_F(toml_edit, err_unclosed_array) {
  run_toml_test(&ur, (toml_test_t) {
    .toml = r("foo = [1, 2"),
    .expect = { .err = SPN_ERR_TOML_PARSE },
  });
}

UTEST_F(toml_edit, err_unclosed_inline_table) {
  run_toml_test(&ur, (toml_test_t) {
    .toml = r("foo = { version = \"1.0.0\""),
    .expect = { .err = SPN_ERR_TOML_PARSE },
  });
}

UTEST_F(toml_edit, err_bad_escape) {
  run_toml_test(&ur, (toml_test_t) {
    .toml = r("foo = \"\\x41\""),
    .expect = { .err = SPN_ERR_TOML_PARSE },
  });
}

UTEST_F(toml_edit, err_garbage_after_value) {
  run_toml_test(&ur, (toml_test_t) {
    .toml = r("foo = \"1.0.0\" garbage"),
    .expect = { .err = SPN_ERR_TOML_PARSE },
  });
}

UTEST_F(toml_edit, err_unclosed_header) {
  run_toml_test(&ur, (toml_test_t) {
    .toml = r("[deps.package"),
    .expect = { .err = SPN_ERR_TOML_PARSE },
  });
}

UTEST_F(toml_edit, err_unicode_bad_digits) {
  run_toml_test(&ur, (toml_test_t) {
    .toml = r("foo = \"\\uZZZZ\""),
    .expect = { .err = SPN_ERR_TOML_PARSE },
  });
}

UTEST_F(toml_edit, err_unicode_short) {
  run_toml_test(&ur, (toml_test_t) {
    .toml = r("foo = \"\\u00e\""),
    .expect = { .err = SPN_ERR_TOML_PARSE },
  });
}

UTEST_F(toml_edit, err_unicode_out_of_range) {
  run_toml_test(&ur, (toml_test_t) {
    .toml = r("foo = \"\\UFFFFFFFF\""),
    .expect = { .err = SPN_ERR_TOML_PARSE },
  });
}


// FIND
UTEST_F(toml_edit, get_string) {
  run_toml_test(&ur, (toml_test_t) {
    .manifest = "simple",
    .expect = { .get = { .path = { "deps", "package", "foo" }, .value = "1.0.0" } },
  });
}

UTEST_F(toml_edit, get_decodes_escapes) {
  run_toml_test(&ur, (toml_test_t) {
    .manifest = "escapes",
    .expect = { .get = { .path = { "foo" }, .value = "a\"b\nc \xc3\xa9" } },
  });
}

UTEST_F(toml_edit, get_literal_verbatim) {
  run_toml_test(&ur, (toml_test_t) {
    .manifest = "literal",
    .expect = { .get = { .path = { "foo" }, .value = "c:\\path" } },
  });
}

UTEST_F(toml_edit, get_quoted_key) {
  run_toml_test(&ur, (toml_test_t) {
    .manifest = "quoted_key",
    .expect = { .get = { .path = { "deps", "package", "spader/sp" }, .value = "3.0.0" } },
  });
}

UTEST_F(toml_edit, get_dotted_entry) {
  run_toml_test(&ur, (toml_test_t) {
    .manifest = "dotted",
    .expect = { .get = { .path = { "deps", "package", "foo" }, .value = "1.0.0" } },
  });
}

UTEST_F(toml_edit, get_inline_table_version) {
  run_toml_test(&ur, (toml_test_t) {
    .manifest = "inline_table",
    .expect = { .get = { .path = { "deps", "package", "foo", "version" }, .value = "1.0.0" } },
  });
}

UTEST_F(toml_edit, get_missing) {
  run_toml_test(&ur, (toml_test_t) {
    .manifest = "simple",
    .expect = { .get = { .path = { "deps", "package", "bar" }, .missing = true } },
  });
}

UTEST_F(toml_edit, get_scalar_raw) {
  run_toml_test(&ur, (toml_test_t) {
    .manifest = "scalar",
    .expect = { .get = { .path = { "foo" }, .value = "42" } },
  });
}

UTEST_F(toml_edit, get_multiline_string) {
  run_toml_test(&ur, (toml_test_t) {
    .manifest = "multiline",
    .expect = { .get = { .path = { "foo" }, .value = "hello\n" } },
  });
}

UTEST_F(toml_edit, get_multiline_empty) {
  run_toml_test(&ur, (toml_test_t) {
    .manifest = "multiline_empty",
    .expect = { .get = { .path = { "foo" }, .value = "" } },
  });
}


// SET: REPLACE
UTEST_F(toml_edit, set_replace_keeps_comment) {
  run_toml_test(&ur, (toml_test_t) {
    .manifest = "commented",
    .set = { { .path = { "deps", "package", "foo" }, .value = "2.0.0" } },
    .expect = { .golden = "set_replace_keeps_comment" },
  });
}

UTEST_F(toml_edit, set_replace_keeps_spacing) {
  run_toml_test(&ur, (toml_test_t) {
    .manifest = "spacing",
    .set = { { .path = { "foo" }, .value = "2.0.0" } },
    .expect = { .golden = "set_replace_keeps_spacing" },
  });
}

UTEST_F(toml_edit, set_replace_inline_table_wholesale) {
  run_toml_test(&ur, (toml_test_t) {
    .manifest = "inline_table",
    .set = { { .path = { "deps", "package", "foo" }, .value = "2.0.0" } },
    .expect = { .golden = "set_replace_inline_table_wholesale" },
  });
}

UTEST_F(toml_edit, set_replace_version_in_inline_table) {
  run_toml_test(&ur, (toml_test_t) {
    .manifest = "inline_table",
    .set = { { .path = { "deps", "package", "foo", "version" }, .value = "2.0.0" } },
    .expect = { .golden = "set_replace_version_in_inline_table" },
  });
}

UTEST_F(toml_edit, set_escapes_value) {
  run_toml_test(&ur, (toml_test_t) {
    .manifest = "string",
    .set = { { .path = { "foo" }, .value = "say \"hi\"\n" } },
    .expect = { .golden = "set_escapes_value" },
  });
}


// SET: INSERT
UTEST_F(toml_edit, set_insert_into_table) {
  run_toml_test(&ur, (toml_test_t) {
    .manifest = "two_sections",
    .set = { { .path = { "deps", "package", "bar" }, .value = "2.0.0" } },
    .expect = { .golden = "set_insert_into_table" },
  });
}

UTEST_F(toml_edit, set_insert_before_trailing_comment) {
  run_toml_test(&ur, (toml_test_t) {
    .manifest = "trailing_comment",
    .set = { { .path = { "deps", "package", "bar" }, .value = "2.0.0" } },
    .expect = { .golden = "set_insert_before_trailing_comment" },
  });
}

UTEST_F(toml_edit, set_insert_into_empty_table) {
  run_toml_test(&ur, (toml_test_t) {
    .manifest = "empty_table",
    .set = { { .path = { "deps", "package", "foo" }, .value = "1.0.0" } },
    .expect = { .golden = "set_insert_into_empty_table" },
  });
}

UTEST_F(toml_edit, set_insert_quoted_key) {
  run_toml_test(&ur, (toml_test_t) {
    .manifest = "simple",
    .set = { { .path = { "deps", "package", "spader/sp" }, .value = "3.0.0" } },
    .expect = { .golden = "set_insert_quoted_key" },
  });
}

UTEST_F(toml_edit, set_insert_dotted_sibling) {
  run_toml_test(&ur, (toml_test_t) {
    .manifest = "dotted",
    .set = { { .path = { "deps", "package", "bar" }, .value = "2.0.0" } },
    .expect = { .golden = "set_insert_dotted_sibling" },
  });
}

UTEST_F(toml_edit, set_insert_into_inline_table) {
  run_toml_test(&ur, (toml_test_t) {
    .manifest = "inline_private",
    .set = { { .path = { "foo", "version" }, .value = "1.0.0" } },
    .expect = { .golden = "set_insert_into_inline_table" },
  });
}

UTEST_F(toml_edit, set_insert_into_empty_inline_table) {
  run_toml_test(&ur, (toml_test_t) {
    .manifest = "inline_empty",
    .set = { { .path = { "foo", "version" }, .value = "1.0.0" } },
    .expect = { .golden = "set_insert_into_empty_inline_table" },
  });
}

UTEST_F(toml_edit, set_insert_crlf) {
  run_toml_test(&ur, (toml_test_t) {
    .toml = "[deps.package]\r\nfoo = \"1.0.0\"\r\n",
    .set = { { .path = { "deps", "package", "bar" }, .value = "2.0.0" } },
    .expect = { .output = "[deps.package]\r\nfoo = \"1.0.0\"\r\nbar = \"2.0.0\"\r\n" },
  });
}

UTEST_F(toml_edit, set_insert_no_trailing_newline) {
  run_toml_test(&ur, (toml_test_t) {
    .toml = "[deps.package]\nfoo = \"1.0.0\"",
    .set = { { .path = { "deps", "package", "bar" }, .value = "2.0.0" } },
    .expect = { .output = "[deps.package]\nfoo = \"1.0.0\"\nbar = \"2.0.0\"\n" },
  });
}

UTEST_F(toml_edit, set_insert_duplicate_sections_uses_last) {
  run_toml_test(&ur, (toml_test_t) {
    .manifest = "duplicate_sections",
    .set = { { .path = { "deps", "package", "bar" }, .value = "2.0.0" } },
    .expect = { .golden = "set_insert_duplicate_sections_uses_last" },
  });
}


// SET: CREATE TABLE
UTEST_F(toml_edit, set_create_table) {
  run_toml_test(&ur, (toml_test_t) {
    .manifest = "package",
    .set = { { .path = { "deps", "package", "foo" }, .value = "1.0.0" } },
    .expect = { .golden = "set_create_table" },
  });
}

UTEST_F(toml_edit, set_create_table_no_trailing_newline) {
  run_toml_test(&ur, (toml_test_t) {
    .toml = "[package]\nname = \"test\"",
    .set = { { .path = { "deps", "package", "foo" }, .value = "1.0.0" } },
    .expect = { .output = "[package]\nname = \"test\"\n\n[deps.package]\nfoo = \"1.0.0\"\n" },
  });
}

UTEST_F(toml_edit, set_create_table_crlf) {
  run_toml_test(&ur, (toml_test_t) {
    .toml = "[package]\r\nname = \"test\"\r\n",
    .set = { { .path = { "deps", "package", "foo" }, .value = "1.0.0" } },
    .expect = { .output = "[package]\r\nname = \"test\"\r\n\r\n[deps.package]\r\nfoo = \"1.0.0\"\r\n" },
  });
}

UTEST_F(toml_edit, set_create_table_empty_file) {
  run_toml_test(&ur, (toml_test_t) {
    .toml = "",
    .set = { { .path = { "deps", "package", "foo" }, .value = "1.0.0" } },
    .expect = { .golden = "set_create_table_empty_file" },
  });
}

UTEST_F(toml_edit, set_create_table_skips_array_sections) {
  run_toml_test(&ur, (toml_test_t) {
    .manifest = "array_section",
    .set = { { .path = { "deps", "package", "foo" }, .value = "1.0.0" } },
    .expect = { .golden = "set_create_table_skips_array_sections" },
  });
}


// SET: ERRORS
UTEST_F(toml_edit, set_same_key_twice_errors) {
  run_toml_test(&ur, (toml_test_t) {
    .manifest = "string",
    .set = {
      { .path = { "foo" }, .value = "2.0.0" },
      { .path = { "foo" }, .value = "3.0.0", .err = SPN_ERROR },
    },
    .expect = { .golden = "set_same_key_twice_errors" },
  });
}

UTEST_F(toml_edit, set_subpath_of_scalar_errors) {
  run_toml_test(&ur, (toml_test_t) {
    .manifest = "array",
    .set = { { .path = { "foo", "bar" }, .value = "1.0.0", .err = SPN_ERROR } },
  });
}

UTEST_F(toml_edit, set_into_array_of_tables_errors) {
  run_toml_test(&ur, (toml_test_t) {
    .manifest = "array_section",
    .set = { { .path = { "test", "flags" }, .value = "x", .err = SPN_ERROR } },
  });
}

UTEST_F(toml_edit, set_empty_inline_table_twice_errors) {
  run_toml_test(&ur, (toml_test_t) {
    .manifest = "inline_empty",
    .set = {
      { .path = { "foo", "version" }, .value = "1.0.0" },
      { .path = { "foo", "private" }, .value = "x", .err = SPN_ERROR },
    },
    .expect = { .golden = "set_insert_into_empty_inline_table" },
  });
}


// SET: MULTIPLE
UTEST_F(toml_edit, set_ordered_inserts) {
  run_toml_test(&ur, (toml_test_t) {
    .manifest = "simple",
    .set = {
      { .path = { "deps", "package", "bar" }, .value = "1.0.0" },
      { .path = { "deps", "package", "baz" }, .value = "2.0.0" },
      { .path = { "deps", "package", "qux" }, .value = "3.0.0" },
      { .path = { "deps", "package", "foo" }, .value = "1.1.0" },
    },
    .expect = { .golden = "set_ordered_inserts" },
  });
}

UTEST_F(toml_edit, set_multiple) {
  run_toml_test(&ur, (toml_test_t) {
    .manifest = "two_keys",
    .set = {
      { .path = { "deps", "package", "foo" }, .value = "1.1.0" },
      { .path = { "deps", "package", "baz" }, .value = "3.0.0" },
    },
    .expect = { .golden = "set_multiple" },
  });
}
