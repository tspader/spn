#include "index.h"

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
} query_test_t;

static const query_test_t tests [] = {
  {
    .name = "returns_releases",
    .file = {
      .lines = {
        "{" kv("namespace", "core") "," kv("name", "spum") "," kv("version", "1.0.0") "," kv("yanked", false) "}",
      },
    },
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
    .file = { .exists = true },
  },
  {
    .name = "malformed_line",
    .file = {
      .lines = { "not json" },
    },
  },
};

static bool semver_is_zero(spn_semver_t version) {
  return version.major == 0 && version.minor == 0 && version.patch == 0;
}

sp_test_each(index_query, get_package, query_test_t, tests) {
  sp_mem_t mem = sp_test_arena(t);

  sp_str_t location = sp_fs_join_path(mem, sp_test_dir(t), sp_str_lit("index"));
  sp_fs_create_dir(location);

  if (it->file.exists || it->file.lines[0]) {
    sp_io_dyn_mem_writer_t builder = sp_zero;
    sp_io_dyn_mem_writer_init(mem, &builder);
    sp_carr_for(it->file.lines, line) {
      if (!it->file.lines[line]) {
        break;
      }
      sp_io_write_line(&builder.base, sp_str_view(it->file.lines[line]));
    }

    sp_str_t file = sp_fs_join_path(mem, location, sp_str_lit("core/spum.jsonl"));
    sp_fs_create_dir(sp_fs_parent_path(file));
    sp_fs_create_file_str(file, sp_io_dyn_mem_writer_take_str(&builder));
  }

  spn_index_info_t index = {
    .location = location,
  };
  spn_index_init(&index, mem);

  spn_index_pkg_t* pkg = spn_index_get_package(&index, (spn_pkg_name_t) {
    .namespace = sp_str_lit("core"),
    .name = sp_str_lit("spum"),
  });
  sp_expect_eq(t, it->expect.exists, pkg != SP_NULLPTR);

  if (pkg) {
    u32 expected = 0;
    sp_carr_detect_len(it->expect.versions, expected, !semver_is_zero(it->expect.versions[expected]));

    sp_must_eq(t, expected, sp_da_size(pkg->releases));
    sp_for(at, expected) {
      sp_expect(t, spn_semver_eq(it->expect.versions[at], pkg->releases[at].version));
    }
  }

  spn_index_deinit(&index);
  return SP_OK;
}
