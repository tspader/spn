#include "index.h"

typedef struct {
  const c8* name;
  const c8* fixture;

  struct {
    bool exists;
    spn_err_t err;
    spn_semver_t versions [4];
  } expect;
} query_test_t;

static const query_test_t tests [] = {
  {
    .name = "returns_releases",
    .fixture = "minimal",
    .expect = {
      .exists = true,
      .versions = { { 1, 0, 0 } },
    },
  },
  {
    .name = "missing_file",
  },
  {
    .name = "empty_blob",
    .fixture = "empty",
    .expect = {
      .err = SPN_ERR_INDEX_CORRUPT,
    },
  },
  {
    .name = "malformed_line",
    .fixture = "invalid_json",
    .expect = {
      .err = SPN_ERR_INDEX_CORRUPT,
    },
  },
};

static bool semver_is_zero(spn_semver_t version) {
  return version.major == 0 && version.minor == 0 && version.patch == 0;
}

sp_test_each(index_query, get_package, query_test_t, tests, .setup = spn_test_ctx_setup) {
  sp_mem_t mem = sp_test_arena(t);

  sp_str_t location = sp_fs_join_path(mem, sp_test_dir(t), sp_str_lit("index"));
  sp_fs_create_dir(location);

  sp_str_t file = sp_fs_join_path(mem, location, sp_str_lit("core/spum.jsonl"));
  if (it->fixture) {
    sp_str_t fixture = test_repo_path(mem, sp_test_format(t, "test/core/index/releases/{}.jsonl", sp_fmt_cstr(it->fixture)));
    sp_str_t blob = sp_zero;
    sp_io_read_file(mem, fixture, &blob);
    sp_fs_create_dir(sp_fs_parent_path(file));
    sp_fs_create_file_str(file, blob);
  }

  spn_index_info_t index = {
    .location = location,
  };

  spn_index_pkg_t* pkg = SP_NULLPTR;
  spn_index_diag_t diag = sp_zero;
  spn_err_t err = spn_index_get_package(&index, mem, spn.intern, (spn_pkg_name_t) {
    .namespace = sp_str_lit("core"),
    .name = sp_str_lit("spum"),
  }, &pkg, &diag);
  sp_expect_eq(t, it->expect.exists, pkg != SP_NULLPTR);
  sp_expect_eq(t, it->expect.err, err);
  if (err == SPN_ERR_INDEX_CORRUPT) {
    sp_expect_str_eq(t, diag.path, file);
  }

  if (pkg) {
    u32 expected = 0;
    sp_carr_detect_len(it->expect.versions, expected, !semver_is_zero(it->expect.versions[expected]));

    sp_must_eq(t, expected, sp_da_size(pkg->releases));
    sp_for(at, expected) {
      sp_expect(t, spn_semver_eq(it->expect.versions[at], pkg->releases[at].version));
    }
  }

  return SP_OK;
}
