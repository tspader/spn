#include "sp.h"
#include "utest.h"
#include "test.h"

#include "index/index.h"
#include "semver/compare.h"
#include "sp/io.h"

UTEST_EMPTY_FIXTURE(index_query)

typedef struct {
  const c8* name;

  struct {
    bool exists;
    const c8* lines [4];
  } file;

  struct {
    bool exists;
    spn_semver_t versions [4];
  } expect;
} query_case_t;

static bool semver_is_zero(spn_semver_t version) {
  return version.major == 0 && version.minor == 0 && version.patch == 0;
}

static void run_query_case(s32* utest_result, query_case_t c) {
  ctx_t* harness = ctx_get();
  sp_mem_t mem = sp_mem_arena_as_allocator(harness->arena);
  sp_str_t root = tmpfs_get(&harness->fs, sp_str_view(c.name));

  sp_str_t location = sp_fs_join_path(mem, root, sp_str_lit("index"));
  sp_fs_create_dir(location);

  if (c.file.exists || c.file.lines[0]) {
    sp_io_dyn_mem_writer_t builder = sp_zero;
    sp_io_dyn_mem_writer_init(mem, &builder);
    sp_carr_for(c.file.lines, it) {
      if (!c.file.lines[it]) {
        break;
      }
      sp_io_write_line(&builder.base, sp_str_view(c.file.lines[it]));
    }
    tmpfs_create(
      &harness->fs,
      sp_fs_join_path(mem, sp_str_view(c.name), sp_str_lit("index/core/spum.jsonl")),
      sp_io_dyn_mem_writer_take_str(&builder)
    );
  }

  spn_index_info_t index = {
    .location = location,
  };
  spn_index_init(&index, mem);

  spn_index_pkg_t* pkg = spn_index_get_package(&index, (spn_pkg_name_t) {
    .namespace = sp_str_lit("core"),
    .name = sp_str_lit("spum"),
  });
  EXPECT_EQ(c.expect.exists, pkg != SP_NULLPTR);

  if (pkg) {
    u32 expected = 0;
    sp_carr_for(c.expect.versions, it) {
      if (semver_is_zero(c.expect.versions[it])) {
        break;
      }
      expected++;
    }

    EXPECT_EQ(expected, sp_da_size(pkg->releases));
    sp_for(it, SP_MIN(expected, sp_da_size(pkg->releases))) {
      EXPECT_TRUE(spn_semver_eq(c.expect.versions[it], pkg->releases[it].version));
    }
  }

  spn_index_deinit(&index);
}

UTEST_F(index_query, query_package_returns_releases) {
  run_query_case(utest_result, (query_case_t) {
    .name = "query_package_returns_releases",
    .file = {
      .lines = {
        "{" kv("namespace", "core") "," kv("name", "spum") "," kv("version", "1.0.0") "," kv("yanked", false) "}",
      },
    },
    .expect = {
      .exists = true,
      .versions = { spn_semver_lit(1, 0, 0) },
    },
  });
}

UTEST_F(index_query, query_package_missing_file_errors) {
  run_query_case(utest_result, (query_case_t) {
    .name = "query_package_missing_file_errors",
  });
}

UTEST_F(index_query, query_package_empty_blob_errors) {
  run_query_case(utest_result, (query_case_t) {
    .name = "query_package_empty_blob_errors",
    .file = { .exists = true },
  });
}

UTEST_F(index_query, query_package_malformed_json_line_errors) {
  run_query_case(utest_result, (query_case_t) {
    .name = "query_package_malformed_json_line_errors",
    .file = {
      .lines = { "not json" },
    },
  });
}
