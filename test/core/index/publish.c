#include "sp.h"
#include "utest.h"
#include "test.h"

#include "index/index.h"
#include "index/json.h"
#include "sp/str.h"

typedef struct {
  const c8* namespace;
  const c8* name;
  const c8* version;
  spn_index_dep_kind_t kind;
} dep_input_t;

typedef struct {
  const c8* name;

  struct {
    const c8* namespace;
    const c8* name;
    spn_semver_t version;
    bool yanked;
    struct { const c8* url; const c8* rev; const c8* dir; } source;
    struct { const c8* url; const c8* rev; const c8* dir; } manifest;
    struct { const c8* manifest; const c8* script; } paths;
    dep_input_t deps[4];
  } input;
} roundtrip_case_t;

struct index_roundtrip {
  s32 unused;
};

static void run_roundtrip_case(s32* utest_result, struct index_roundtrip* fixture, roundtrip_case_t c) {
  spn_index_rel_t rel = {
    .id = {
      .namespace = sp_str_view(c.input.namespace),
      .name = sp_str_view(c.input.name),
    },
    .version = c.input.version,
    .yanked = c.input.yanked,
  };

  if (c.input.source.url) { rel.source.url = sp_str_view(c.input.source.url); }
  if (c.input.source.rev) { rel.source.rev = sp_str_view(c.input.source.rev); }
  if (c.input.source.dir) { rel.source.dir = sp_str_view(c.input.source.dir); }

  if (c.input.manifest.url) { rel.manifest.url = sp_str_view(c.input.manifest.url); }
  if (c.input.manifest.rev) { rel.manifest.rev = sp_str_view(c.input.manifest.rev); }
  if (c.input.manifest.dir) { rel.manifest.dir = sp_str_view(c.input.manifest.dir); }

  if (c.input.paths.manifest) { rel.paths.manifest = sp_str_view(c.input.paths.manifest); }
  if (c.input.paths.script) { rel.paths.script = sp_str_view(c.input.paths.script); }

  sp_for(it, sp_carr_len(c.input.deps)) {
    dep_input_t dep = c.input.deps[it];
    if (!dep.namespace) { break; }
    sp_da_push(rel.deps, ((spn_index_dep_t) {
      .id = {
        .namespace = sp_str_view(dep.namespace),
        .name = sp_str_view(dep.name),
      },
      .version = sp_str_view(dep.version),
      .kind = dep.kind,
    }));
  }

  sp_str_t json = spn_index_rel_to_json(&rel);
  EXPECT_TRUE(!sp_str_empty(json));

  spn_index_info_t index = SP_ZERO_INITIALIZE();
  spn_index_init(&index);

  spn_index_pkg_t pkg = SP_ZERO_INITIALIZE();
  spn_err_t err = spn_index_parse_pkg(&index.json.ctx, index.json.schema, rel.id, json, &pkg);
  EXPECT_EQ(SPN_OK, err);

  if (err == SPN_OK && sp_da_size(pkg.releases) == 1) {
    spn_index_rel_t* parsed = &pkg.releases[0];

    SP_EXPECT_STR_EQ_CSTR(parsed->id.namespace, c.input.namespace);
    SP_EXPECT_STR_EQ_CSTR(parsed->id.name, c.input.name);
    EXPECT_EQ(c.input.version.major, parsed->version.major);
    EXPECT_EQ(c.input.version.minor, parsed->version.minor);
    EXPECT_EQ(c.input.version.patch, parsed->version.patch);
    EXPECT_EQ(c.input.yanked, parsed->yanked);

    if (c.input.source.url) { SP_EXPECT_STR_EQ_CSTR(parsed->source.url, c.input.source.url); }
    if (c.input.source.rev) { SP_EXPECT_STR_EQ_CSTR(parsed->source.rev, c.input.source.rev); }
    if (c.input.source.dir) { SP_EXPECT_STR_EQ_CSTR(parsed->source.dir, c.input.source.dir); }

    if (c.input.manifest.url) { SP_EXPECT_STR_EQ_CSTR(parsed->manifest.url, c.input.manifest.url); }
    if (c.input.manifest.rev) { SP_EXPECT_STR_EQ_CSTR(parsed->manifest.rev, c.input.manifest.rev); }
    if (c.input.manifest.dir) { SP_EXPECT_STR_EQ_CSTR(parsed->manifest.dir, c.input.manifest.dir); }

    if (c.input.paths.manifest) { SP_EXPECT_STR_EQ_CSTR(parsed->paths.manifest, c.input.paths.manifest); }
    if (c.input.paths.script) { SP_EXPECT_STR_EQ_CSTR(parsed->paths.script, c.input.paths.script); }

    u32 expected_deps = 0;
    sp_for(it, sp_carr_len(c.input.deps)) {
      if (!c.input.deps[it].namespace) { break; }
      expected_deps++;
    }
    EXPECT_EQ(expected_deps, sp_da_size(parsed->deps));

    sp_for(it, expected_deps) {
      SP_EXPECT_STR_EQ_CSTR(parsed->deps[it].id.namespace, c.input.deps[it].namespace);
      SP_EXPECT_STR_EQ_CSTR(parsed->deps[it].id.name, c.input.deps[it].name);
      SP_EXPECT_STR_EQ_CSTR(parsed->deps[it].version, c.input.deps[it].version);
      EXPECT_EQ(c.input.deps[it].kind, parsed->deps[it].kind);
    }
  }

  spn_index_deinit(&index);
}

UTEST_F_SETUP(index_roundtrip) {
  ctx_t* harness = ctx_get();
  sp_context_push_allocator(sp_mem_arena_as_allocator(harness->arena));
}

UTEST_F_TEARDOWN(index_roundtrip) {
  sp_context_pop();
}

UTEST_F(index_roundtrip, basic_source_only) {
  run_roundtrip_case(utest_result, uf, (roundtrip_case_t) {
    .name = "basic_source_only",
    .input = {
      .namespace = "core",
      .name = "spum",
      .version = spn_semver_lit(1, 0, 0),
      .source = { .url = "https://github.com/example/spum", .rev = "abc123" },
      .paths = { .manifest = "spn.toml", .script = "spn.c" },
    },
  });
}

UTEST_F(index_roundtrip, with_manifest_pointer) {
  run_roundtrip_case(utest_result, uf, (roundtrip_case_t) {
    .name = "with_manifest_pointer",
    .input = {
      .namespace = "core",
      .name = "spum",
      .version = spn_semver_lit(3, 51, 0),
      .source = { .url = "https://github.com/sqlite/sqlite", .rev = "deadbeef" },
      .manifest = { .url = "https://github.com/spn-core/packages", .rev = "cafebabe", .dir = "sqlite" },
      .paths = { .manifest = "spn.toml", .script = "spn.c" },
    },
  });
}

UTEST_F(index_roundtrip, with_deps) {
  run_roundtrip_case(utest_result, uf, (roundtrip_case_t) {
    .name = "with_deps",
    .input = {
      .namespace = "core",
      .name = "spum",
      .version = spn_semver_lit(2, 0, 0),
      .source = { .url = "https://github.com/example/spum", .rev = "def456" },
      .paths = { .manifest = "spn.toml", .script = "spn.c" },
      .deps = {
        { .namespace = "core", .name = "curl", .version = "^1.0.0", .kind = SPN_INDEX_DEP_NORMAL },
        { .namespace = "core", .name = "zlib", .version = "^2.0.0", .kind = SPN_INDEX_DEP_NORMAL },
      },
    },
  });
}

UTEST_F(index_roundtrip, with_build_deps) {
  run_roundtrip_case(utest_result, uf, (roundtrip_case_t) {
    .name = "with_build_deps",
    .input = {
      .namespace = "core",
      .name = "spum",
      .version = spn_semver_lit(1, 0, 0),
      .source = { .url = "https://github.com/example/spum", .rev = "abc123" },
      .paths = { .manifest = "spn.toml", .script = "spn.c" },
      .deps = {
        { .namespace = "core", .name = "cmake", .version = "^3.0.0", .kind = SPN_INDEX_DEP_BUILD },
      },
    },
  });
}

UTEST_F(index_roundtrip, with_test_deps) {
  run_roundtrip_case(utest_result, uf, (roundtrip_case_t) {
    .name = "with_test_deps",
    .input = {
      .namespace = "core",
      .name = "spum",
      .version = spn_semver_lit(1, 0, 0),
      .source = { .url = "https://github.com/example/spum", .rev = "abc123" },
      .paths = { .manifest = "spn.toml", .script = "spn.c" },
      .deps = {
        { .namespace = "core", .name = "utest", .version = "^1.0.0", .kind = SPN_INDEX_DEP_TEST },
      },
    },
  });
}

UTEST_F(index_roundtrip, with_mixed_dep_kinds) {
  run_roundtrip_case(utest_result, uf, (roundtrip_case_t) {
    .name = "with_mixed_dep_kinds",
    .input = {
      .namespace = "core",
      .name = "spum",
      .version = spn_semver_lit(3, 0, 0),
      .source = { .url = "https://github.com/example/spum", .rev = "fff999" },
      .paths = { .manifest = "spn.toml", .script = "spn.c" },
      .deps = {
        { .namespace = "core", .name = "curl", .version = "^1.0.0", .kind = SPN_INDEX_DEP_NORMAL },
        { .namespace = "core", .name = "cmake", .version = "^3.0.0", .kind = SPN_INDEX_DEP_BUILD },
        { .namespace = "core", .name = "utest", .version = "^1.0.0", .kind = SPN_INDEX_DEP_TEST },
      },
    },
  });
}

UTEST_F(index_roundtrip, with_subdir) {
  run_roundtrip_case(utest_result, uf, (roundtrip_case_t) {
    .name = "with_subdir",
    .input = {
      .namespace = "core",
      .name = "spum",
      .version = spn_semver_lit(1, 2, 3),
      .source = { .url = "https://github.com/example/mono", .rev = "aaa111", .dir = "packages/spum" },
      .paths = { .manifest = "spn.toml", .script = "spn.c" },
    },
  });
}

typedef struct {
  const c8* path;
  const c8* lines[8];
} publish_file_t;

typedef struct {
  const c8* name;

  struct {
    publish_file_t files[4];
  } fixture;

  struct {
    const c8* namespace;
    const c8* name;
    spn_semver_t version;
    struct { const c8* url; const c8* rev; } source;
  } release;

  struct {
    spn_err_t result;
    u32 lines;
  } expect;
} publish_case_t;

struct index_publish {
  s32 unused;
};

static void write_publish_fixtures(ctx_t* harness, const c8* name, const publish_file_t files[4]) {
  sp_str_t prefix = sp_str_view(name);

  sp_for(it, 4) {
    publish_file_t file = files[it];
    if (!file.path) { break; }

    sp_str_builder_t builder = SP_ZERO_INITIALIZE();
    sp_for(line_it, sp_carr_len(file.lines)) {
      if (!file.lines[line_it]) { break; }
      sp_str_builder_append(&builder, sp_str_view(file.lines[line_it]));
      sp_str_builder_new_line(&builder);
    }

    tmpfs_create(
      &harness->fs,
      sp_fs_join_path(prefix, sp_fs_join_path(sp_str_lit("index"), sp_str_view(file.path))),
      sp_str_builder_to_str(&builder)
    );
  }
}

static void run_publish_case(s32* utest_result, struct index_publish* fixture, publish_case_t c) {
  ctx_t* harness = ctx_get();
  sp_str_t case_root = tmpfs_get(&harness->fs, sp_str_view(c.name));

  sp_str_t index_root = sp_fs_join_path(case_root, sp_str_lit("index"));
  sp_fs_create_dir(index_root);

  write_publish_fixtures(harness, c.name, c.fixture.files);

  spn_index_info_t index = {
    .location = index_root,
  };
  spn_index_init(&index);

  spn_index_rel_t rel = {
    .id = {
      .namespace = sp_str_view(c.release.namespace),
      .name = sp_str_view(c.release.name),
    },
    .version = c.release.version,
  };
  if (c.release.source.url) { rel.source.url = sp_str_view(c.release.source.url); }
  if (c.release.source.rev) { rel.source.rev = sp_str_view(c.release.source.rev); }

  spn_err_t result = spn_index_publish(&index, &rel);
  EXPECT_EQ(c.expect.result, result);

  if (c.expect.lines > 0) {
    sp_str_t ns = sp_str_view(c.release.namespace);
    sp_str_t nm = sp_str_view(c.release.name);
    sp_str_t pkg_path = sp_fs_join_path(
      sp_fs_join_path(index_root, ns),
      sp_format("{}.jsonl", SP_FMT_STR(nm))
    );

    EXPECT_TRUE(sp_fs_exists(pkg_path));
    if (sp_fs_exists(pkg_path)) {
      sp_str_t content = sp_io_read_file(pkg_path);
      u32 count = 0;
      sp_str_for_line(content, line_it) {
        sp_str_t line = sp_str_trim(line_it.line);
        if (!sp_str_empty(line)) { count++; }
      }
      EXPECT_EQ(c.expect.lines, count);
    }
  }

  spn_index_deinit(&index);
}

UTEST_F_SETUP(index_publish) {
  ctx_t* harness = ctx_get();
  sp_context_push_allocator(sp_mem_arena_as_allocator(harness->arena));
}

UTEST_F_TEARDOWN(index_publish) {
  sp_context_pop();
}

UTEST_F(index_publish, publish_to_empty_index) {
  run_publish_case(utest_result, uf, (publish_case_t) {
    .name = "publish_to_empty_index",
    .release = {
      .namespace = "core",
      .name = "spum",
      .version = spn_semver_lit(1, 0, 0),
      .source = { .url = "https://github.com/example/spum", .rev = "abc123" },
    },
    .expect = {
      .result = SPN_OK,
      .lines = 1,
    },
  });
}

UTEST_F(index_publish, publish_second_version) {
  run_publish_case(utest_result, uf, (publish_case_t) {
    .name = "publish_second_version",
    .fixture = {
      .files = {
        {
          .path = "core/spum.jsonl",
          .lines = {
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
    .release = {
      .namespace = "core",
      .name = "spum",
      .version = spn_semver_lit(1, 1, 0),
      .source = { .url = "https://github.com/example/spum", .rev = "def456" },
    },
    .expect = {
      .result = SPN_OK,
      .lines = 2,
    },
  });
}

UTEST_F(index_publish, publish_duplicate_version_rejected) {
  run_publish_case(utest_result, uf, (publish_case_t) {
    .name = "publish_duplicate_version_rejected",
    .fixture = {
      .files = {
        {
          .path = "core/spum.jsonl",
          .lines = {
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
    .release = {
      .namespace = "core",
      .name = "spum",
      .version = spn_semver_lit(1, 0, 0),
      .source = { .url = "https://github.com/example/spum", .rev = "abc123" },
    },
    .expect = {
      .result = SPN_ERR_VERSION_EXISTS,
    },
  });
}

UTEST_F(index_publish, publish_creates_new_file_for_different_package) {
  run_publish_case(utest_result, uf, (publish_case_t) {
    .name = "publish_creates_new_file_for_different_package",
    .fixture = {
      .files = {
        {
          .path = "core/other.jsonl",
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
    .release = {
      .namespace = "core",
      .name = "spum",
      .version = spn_semver_lit(1, 0, 0),
      .source = { .url = "https://github.com/example/spum", .rev = "abc123" },
    },
    .expect = {
      .result = SPN_OK,
      .lines = 1,
    },
  });
}
