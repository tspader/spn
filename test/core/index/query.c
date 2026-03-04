#include "sp.h"
#include "utest.h"
#include "test.h"

#include "index/index.h"

typedef struct {
  const c8* path;
  const c8* lines[8];
} index_file_t;

struct index_query {
  s32 unused;
};

typedef struct {
  const c8* url;
  const c8* rev;
  const c8* dir;
} source_expect_t;

typedef struct {
  const c8* name;

  struct {
    index_file_t files[4];
  } fixture;

  struct {
    bool exists;

    struct {
      struct {
        spn_semver_t version;
        bool yanked;
        u32 deps;
        source_expect_t source;
        source_expect_t manifest;
      } entries[4];
    } releases;
  } expect;
} case_t;

static bool semver_is_zero(spn_semver_t version) {
  return version.major == 0 && version.minor == 0 && version.patch == 0;
}

static void write_index_files(ctx_t* harness, const c8* name, const index_file_t files[4]) {
  sp_str_t prefix = sp_str_view(name);

  sp_for(it, 4) {
    index_file_t file = files[it];
    if (!file.path) {
      break;
    }

    sp_str_builder_t builder = SP_ZERO_INITIALIZE();
    sp_for(line_it, sp_carr_len(file.lines)) {
      const c8* line = file.lines[line_it];
      if (!line) {
        break;
      }

      sp_str_builder_append(&builder, sp_str_view(line));
      sp_str_builder_new_line(&builder);
    }

    tmpfs_create(
      &harness->fs,
      sp_fs_join_path(prefix, sp_fs_join_path(sp_str_lit("index"), sp_str_view(file.path))),
      sp_str_builder_to_str(&builder)
    );
  }
}

static void run_index_query_case(s32* utest_result, struct index_query* fixture, case_t c) {
  SP_UNUSED(fixture);

  ctx_t* harness = ctx_get();
  sp_str_t case_root = tmpfs_get(&harness->fs, sp_str_view(c.name));

  sp_str_t index_root = sp_fs_join_path(case_root, sp_str_lit("index"));
  sp_fs_create_dir(index_root);

  write_index_files(harness, c.name, c.fixture.files);

  spn_index_t index = {
    .location = index_root,
  };
  spn_index_init(&index);

  spn_index_pkg_t* pkg = spn_index_get_package(&index, (spn_pkg_id_t) {
    .namespace = sp_str_lit("core"),
    .name = sp_str_lit("spum"),
  });

  if (c.expect.exists) {
    EXPECT_TRUE(pkg != SP_NULLPTR);
  }
  else {
    EXPECT_TRUE(pkg == SP_NULLPTR);
  }

  if (pkg) {
    u32 expected = 0;
    sp_for(it, sp_carr_len(c.expect.releases.entries)) {
      if (semver_is_zero(c.expect.releases.entries[it].version)) {
        break;
      }

      expected++;
    }

    EXPECT_EQ(expected, sp_da_size(pkg->releases));
    sp_for(it, expected) {
      EXPECT_EQ(c.expect.releases.entries[it].version.major, pkg->releases[it].version.major);
      EXPECT_EQ(c.expect.releases.entries[it].version.minor, pkg->releases[it].version.minor);
      EXPECT_EQ(c.expect.releases.entries[it].version.patch, pkg->releases[it].version.patch);
      EXPECT_EQ(c.expect.releases.entries[it].yanked, pkg->releases[it].yanked);
      EXPECT_EQ(c.expect.releases.entries[it].deps, sp_da_size(pkg->releases[it].deps));

      source_expect_t src = c.expect.releases.entries[it].source;
      if (src.url) SP_EXPECT_STR_EQ_CSTR(pkg->releases[it].source.url, src.url);
      if (src.rev) SP_EXPECT_STR_EQ_CSTR(pkg->releases[it].source.rev, src.rev);
      if (src.dir) SP_EXPECT_STR_EQ_CSTR(pkg->releases[it].source.dir, src.dir);

      source_expect_t mfst = c.expect.releases.entries[it].manifest;
      if (mfst.url) SP_EXPECT_STR_EQ_CSTR(pkg->releases[it].manifest.url, mfst.url);
      if (mfst.rev) SP_EXPECT_STR_EQ_CSTR(pkg->releases[it].manifest.rev, mfst.rev);
      if (mfst.dir) SP_EXPECT_STR_EQ_CSTR(pkg->releases[it].manifest.dir, mfst.dir);
    }
  }

  spn_index_deinit(&index);
}

UTEST_F_SETUP(index_query) {
  ctx_t* harness = ctx_get();
  sp_context_push_allocator(sp_mem_arena_as_allocator(harness->arena));
}

UTEST_F_TEARDOWN(index_query) {
  sp_context_pop();
}

UTEST_F(index_query, query_package_parses_and_sorts_versions) {
  run_index_query_case(utest_result, uf, (case_t) {
    .name = "query_package_parses_and_sorts_versions",
    .fixture = {
      .files = {
        {
          .path = "core/spum.jsonl",
          .lines = {
            "{"
              kv("namespace", "core") ","
              kv("name", "spum") ","
              kv("version", "1.2.0") ","
              kv("yanked", false)
            "}",
            "{"
              kv("namespace", "core") ","
              kv("name", "spum") ","
              kv("version", "1.0.0") ","
              kv("yanked", false)
            "}",
            "{"
              kv("namespace", "core") ","
              kv("name", "spum") ","
              kv("version", "1.1.0") ","
              kv("yanked", false)
            "}",
          },
        },
      },
    },
    .expect = {
      .exists = true,
      .releases = {
        .entries = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .yanked = false,
            .deps = 0,
          },
          {
            .version = spn_semver_lit(1, 1, 0),
            .yanked = false,
            .deps = 0,
          },
          {
            .version = spn_semver_lit(1, 2, 0),
            .yanked = false,
            .deps = 0,
          },
        },
      },
    },
  });
}

UTEST_F(index_query, query_package_missing_file_errors) {
  run_index_query_case(utest_result, uf, (case_t) {
    .name = "query_package_missing_file_errors",
    .expect = {
      .exists = false,
    },
  });
}

UTEST_F(index_query, query_package_empty_blob_errors) {
  run_index_query_case(utest_result, uf, (case_t) {
    .name = "query_package_empty_blob_errors",
    .fixture = {
      .files = {
        {
          .path = "core/spum.jsonl",
        },
      },
    },
    .expect = {
      .exists = false,
    },
  });
}

UTEST_F(index_query, query_package_malformed_json_line_errors) {
  run_index_query_case(utest_result, uf, (case_t) {
    .name = "query_package_malformed_json_line_errors",
    .fixture = {
      .files = {
        {
          .path = "core/spum.jsonl",
          .lines = {
            "{"
              kv("namespace", "core") ","
              kv("name", "spum")
            "",
            "{"
              kv("namespace", "core") ","
              kv("name", "spum") ","
              kv("version", "1.0.0") ","
              kv("yanked", false)
            "}",
          },
        },
      },
    },
    .expect = {
      .exists = false,
    },
  });
}

UTEST_F(index_query, query_package_invalid_semver_errors) {
  run_index_query_case(utest_result, uf, (case_t) {
    .name = "query_package_invalid_semver_errors",
    .fixture = {
      .files = {
        {
          .path = "core/spum.jsonl",
          .lines = {
            "{"
              kv("namespace", "core") ","
              kv("name", "spum") ","
              kv("version", "nope") ","
              kv("yanked", false)
            "}",
          },
        },
      },
    },
    .expect = {
      .exists = false,
    },
  });
}

UTEST_F(index_query, query_package_wrong_namespace_errors) {
  run_index_query_case(utest_result, uf, (case_t) {
    .name = "query_package_wrong_namespace_errors",
    .fixture = {
      .files = {
        {
          .path = "core/spum.jsonl",
          .lines = {
            "{"
              kv("namespace", "other") ","
              kv("name", "spum") ","
              kv("version", "1.0.0") ","
              kv("yanked", false)
            "}",
          },
        },
      },
    },
    .expect = {
      .exists = false,
    },
  });
}

UTEST_F(index_query, query_package_wrong_name_errors) {
  run_index_query_case(utest_result, uf, (case_t) {
    .name = "query_package_wrong_name_errors",
    .fixture = {
      .files = {
        {
          .path = "core/spum.jsonl",
          .lines = {
            "{"
              kv("namespace", "core") ","
              kv("name", "other") ","
              kv("version", "1.0.0") ","
              kv("yanked", false)
            "}",
          },
        },
      },
    },
    .expect = {
      .exists = false,
    },
  });
}

UTEST_F(index_query, query_package_parses_yanked_and_optional_fields) {
  run_index_query_case(utest_result, uf, (case_t) {
    .name = "query_package_parses_yanked_and_optional_fields",
    .fixture = {
      .files = {
        {
          .path = "core/spum.jsonl",
          .lines = {
            "{"
              kv("namespace", "core") ","
              kv("name", "spum") ","
              kv("version", "1.0.0") ","
              kv("yanked", true)
            "}",
            "{"
              kv("namespace", "core") ","
              kv("name", "spum") ","
              kv("version", "1.1.0") ","
              kv("yanked", false)
            "}",
          },
        },
      },
    },
    .expect = {
      .exists = true,
      .releases = {
        .entries = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .yanked = true,
            .deps = 0,
          },
          {
            .version = spn_semver_lit(1, 1, 0),
            .yanked = false,
            .deps = 0,
          },
        },
      },
    },
  });
}

UTEST_F(index_query, query_package_parses_deps_smoke) {
  run_index_query_case(utest_result, uf, (case_t) {
    .name = "query_package_parses_deps_smoke",
    .fixture = {
      .files = {
        {
          .path = "core/spum.jsonl",
          .lines = {
            "{"
              kv("namespace", "core") ","
              kv("name", "spum") ","
              kv("version", "1.0.0") ","
              k("source") "{"
                kv("url", "http://stuff.stuff")
              "},"
              kv("yanked", false) ","
              k("deps") "[{"
                kv("namespace", "core") ","
                kv("name", "curl") ","
                kv("version", "^1.0.0")
              "}]"
            "}",
          },
        },
      },
    },
    .expect = {
      .exists = true,
      .releases = {
        .entries = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .yanked = false,
            .deps = 1,
            .source = { .url = "http://stuff.stuff" },
          },
        },
      },
    },
  });
}

UTEST_F(index_query, query_package_parses_source_fields) {
  run_index_query_case(utest_result, uf, (case_t) {
    .name = "query_package_parses_source_fields",
    .fixture = {
      .files = {
        {
          .path = "core/spum.jsonl",
          .lines = {
            "{"
              kv("namespace", "core") ","
              kv("name", "spum") ","
              kv("version", "1.0.0") ","
              kv("yanked", false) ","
              k("source") "{"
                kv("url", "https://github.com/example/spum") ","
                kv("rev", "abc123") ","
                kv("dir", "lib")
              "},"
              k("paths") "{"
                kv("manifest", "spn.toml") ","
                kv("script", "spn.c")
              "}"
            "}",
          },
        },
      },
    },
    .expect = {
      .exists = true,
      .releases = {
        .entries = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .yanked = false,
            .deps = 0,
            .source = {
              .url = "https://github.com/example/spum",
              .rev = "abc123",
              .dir = "lib",
            },
          },
        },
      },
    },
  });
}

UTEST_F(index_query, query_package_parses_manifest_source) {
  run_index_query_case(utest_result, uf, (case_t) {
    .name = "query_package_parses_manifest_source",
    .fixture = {
      .files = {
        {
          .path = "core/spum.jsonl",
          .lines = {
            "{"
              kv("namespace", "core") ","
              kv("name", "spum") ","
              kv("version", "3.51.0") ","
              kv("yanked", false) ","
              k("source") "{"
                kv("url", "https://github.com/sqlite/sqlite") ","
                kv("rev", "deadbeef")
              "},"
              k("manifest") "{"
                kv("url", "https://github.com/spn-core/packages") ","
                kv("rev", "cafebabe") ","
                kv("dir", "sqlite")
              "},"
              k("paths") "{"
                kv("manifest", "spn.toml") ","
                kv("script", "spn.c")
              "}"
            "}",
          },
        },
      },
    },
    .expect = {
      .exists = true,
      .releases = {
        .entries = {
          {
            .version = spn_semver_lit(3, 51, 0),
            .yanked = false,
            .deps = 0,
            .source = {
              .url = "https://github.com/sqlite/sqlite",
              .rev = "deadbeef",
            },
            .manifest = {
              .url = "https://github.com/spn-core/packages",
              .rev = "cafebabe",
              .dir = "sqlite",
            },
          },
        },
      },
    },
  });
}

UTEST_F(index_query, query_package_manifest_absent_when_not_specified) {
  run_index_query_case(utest_result, uf, (case_t) {
    .name = "query_package_manifest_absent_when_not_specified",
    .fixture = {
      .files = {
        {
          .path = "core/spum.jsonl",
          .lines = {
            "{"
              kv("namespace", "core") ","
              kv("name", "spum") ","
              kv("version", "1.0.0") ","
              kv("yanked", false) ","
              k("source") "{"
                kv("url", "https://github.com/example/spum") ","
                kv("rev", "abc123")
              "}"
            "}",
          },
        },
      },
    },
    .expect = {
      .exists = true,
      .releases = {
        .entries = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .yanked = false,
            .deps = 0,
            .source = {
              .url = "https://github.com/example/spum",
              .rev = "abc123",
            },
          },
        },
      },
    },
  });
}
