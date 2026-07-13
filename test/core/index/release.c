#include "sp.h"
#include "utest.h"
#include "test.h"

#include "index/json.h"
#include "semver/convert.h"
#include "sp/io.h"

typedef struct {
  const c8* namespace;
  const c8* name;
  const c8* version;
  spn_index_dep_kind_t kind;
} release_test_dep_t;

typedef struct {
  const c8* name;
  spn_linkage_t linkages [4];
} release_test_target_t;

typedef struct {
  const c8* namespace;
  const c8* name;
  const c8* version;
  bool yanked;
  struct { const c8* url; const c8* rev; const c8* dir; } source;
  struct { const c8* url; const c8* rev; const c8* dir; } manifest;
  struct { const c8* manifest; const c8* script; } paths;
  release_test_dep_t deps [4];
  release_test_target_t targets [4];
} release_test_rel_t;

typedef struct {
  spn_err_t err;
  release_test_rel_t releases [4];
} release_test_expect_t;

typedef struct {
  const c8* file;
  bool golden;
  release_test_expect_t expect;
} release_test_t;

UTEST_EMPTY_FIXTURE(index_release)

static spn_index_release_t release_test_build(sp_mem_t mem, release_test_rel_t* rel) {
  spn_index_release_t built = {
    .id = {
      .namespace = sp_cstr_as_str(rel->namespace),
      .name = sp_cstr_as_str(rel->name),
    },
    .version = spn_semver_from_str(sp_cstr_as_str(rel->version)),
    .yanked = rel->yanked,
  };

  if (rel->source.url) { built.source.url = sp_cstr_as_str(rel->source.url); }
  if (rel->source.rev) { built.source.rev = sp_cstr_as_str(rel->source.rev); }
  if (rel->source.dir) { built.source.dir = sp_cstr_as_str(rel->source.dir); }
  if (rel->manifest.url) { built.manifest.url = sp_cstr_as_str(rel->manifest.url); }
  if (rel->manifest.rev) { built.manifest.rev = sp_cstr_as_str(rel->manifest.rev); }
  if (rel->manifest.dir) { built.manifest.dir = sp_cstr_as_str(rel->manifest.dir); }
  if (rel->paths.manifest) { built.paths.manifest = sp_cstr_as_str(rel->paths.manifest); }
  if (rel->paths.script) { built.paths.script = sp_cstr_as_str(rel->paths.script); }

  sp_da_init(mem, built.deps);
  sp_carr_for(rel->deps, it) {
    if (!rel->deps[it].namespace) { break; }
    sp_da_push(built.deps, ((spn_index_dep_t) {
      .kind = rel->deps[it].kind,
      .id = {
        .namespace = sp_cstr_as_str(rel->deps[it].namespace),
        .name = sp_cstr_as_str(rel->deps[it].name),
      },
      .version = sp_cstr_as_str(rel->deps[it].version),
    }));
  }

  sp_da_init(mem, built.targets);
  sp_carr_for(rel->targets, it) {
    if (!rel->targets[it].name) break;

    spn_index_target_t target = sp_zero;
    target.name = sp_cstr_as_str(rel->targets[it].name);
    sp_da_init(mem, target.linkages);
    sp_carr_for(rel->targets[it].linkages, kind) {
      if (rel->targets[it].linkages[kind] == SPN_LIB_KIND_NONE) { break; }
      sp_da_push(target.linkages, rel->targets[it].linkages[kind]);
    }
    sp_da_push(built.targets, target);
  }

  return built;
}

static void release_test_check(s32* utest_result, sp_mem_t mem, release_test_rel_t* expected, spn_index_release_t* rel) {
  SP_EXPECT_STR_EQ_CSTR(rel->id.namespace, expected->namespace);
  SP_EXPECT_STR_EQ_CSTR(rel->id.name, expected->name);
  SP_EXPECT_STR_EQ_CSTR(spn_semver_to_str(mem, rel->version), expected->version);
  EXPECT_EQ(expected->yanked, rel->yanked);

  if (expected->source.url) { SP_EXPECT_STR_EQ_CSTR(rel->source.url, expected->source.url); }
  if (expected->source.rev) { SP_EXPECT_STR_EQ_CSTR(rel->source.rev, expected->source.rev); }
  if (expected->source.dir) { SP_EXPECT_STR_EQ_CSTR(rel->source.dir, expected->source.dir); }
  if (expected->manifest.url) { SP_EXPECT_STR_EQ_CSTR(rel->manifest.url, expected->manifest.url); }
  if (expected->manifest.rev) { SP_EXPECT_STR_EQ_CSTR(rel->manifest.rev, expected->manifest.rev); }
  if (expected->manifest.dir) { SP_EXPECT_STR_EQ_CSTR(rel->manifest.dir, expected->manifest.dir); }
  if (expected->paths.manifest) { SP_EXPECT_STR_EQ_CSTR(rel->paths.manifest, expected->paths.manifest); }
  if (expected->paths.script) { SP_EXPECT_STR_EQ_CSTR(rel->paths.script, expected->paths.script); }

  u32 num_deps = 0;
  sp_carr_for(expected->deps, it) {
    if (!expected->deps[it].namespace) { break; }
    num_deps++;
  }
  EXPECT_EQ(num_deps, sp_da_size(rel->deps));

  sp_for(it, num_deps) {
    if (it >= sp_da_size(rel->deps)) { break; }
    SP_EXPECT_STR_EQ_CSTR(rel->deps[it].id.namespace, expected->deps[it].namespace);
    SP_EXPECT_STR_EQ_CSTR(rel->deps[it].id.name, expected->deps[it].name);
    SP_EXPECT_STR_EQ_CSTR(rel->deps[it].version, expected->deps[it].version);
    EXPECT_EQ(expected->deps[it].kind, rel->deps[it].kind);
  }

  u32 num_targets = 0;
  sp_carr_for(expected->targets, it) {
    if (!expected->targets[it].name) { break; }
    num_targets++;
  }
  EXPECT_EQ(num_targets, sp_da_size(rel->targets));

  sp_for(it, num_targets) {
    if (it >= sp_da_size(rel->targets)) { break; }
    SP_EXPECT_STR_EQ_CSTR(rel->targets[it].name, expected->targets[it].name);

    u32 num_linkages = 0;
    sp_carr_for(expected->targets[it].linkages, kind) {
      if (expected->targets[it].linkages[kind] == SPN_LIB_KIND_NONE) { break; }
      num_linkages++;
    }
    EXPECT_EQ(num_linkages, sp_da_size(rel->targets[it].linkages));

    sp_for(kind, num_linkages) {
      if (kind >= sp_da_size(rel->targets[it].linkages)) { break; }
      EXPECT_EQ(expected->targets[it].linkages[kind], rel->targets[it].linkages[kind]);
    }
  }
}

static void run_release_test(s32* utest_result, release_test_t t) {
  sp_mem_t mem = sp_mem_arena_as_allocator(ctx_get()->arena);
  sp_str_t path = sp_fs_join_path(mem, sp_cstr_as_str(RELEASE_DIR), sp_cstr_as_str(t.file));

  u32 num_releases = 0;
  sp_carr_for(t.expect.releases, it) {
    if (!t.expect.releases[it].name) { break; }
    num_releases++;
  }

  if (t.golden) {
    sp_io_dyn_mem_writer_t rendered = sp_zero;
    sp_io_dyn_mem_writer_init(mem, &rendered);
    sp_for(it, num_releases) {
      spn_index_release_t rel = release_test_build(mem, &t.expect.releases[it]);
      sp_io_write_line(&rendered.base, spn_index_release_to_json(mem, &rel));
    }

    sp_env_t env = sp_env_capture(mem);
    if (sp_env_contains_c(&env, "SPN_GOLDEN_REGEN")) {
      sp_fs_create_file_str(path, sp_io_dyn_mem_writer_as_str(&rendered));
    }
    else {
      sp_str_t golden = sp_zero;
      sp_io_read_file(mem, path, &golden);
      SP_EXPECT_STR_EQ(sp_io_dyn_mem_writer_as_str(&rendered), golden);
    }
  }

  sp_str_t blob = sp_zero;
  sp_io_read_file(mem, path, &blob);

  spn_pkg_name_t id = {
    .namespace = sp_str_lit("core"),
    .name = sp_str_lit("spum"),
  };

  spn_index_pkg_t pkg = sp_zero;
  spn_err_t err = spn_index_parse_pkg(mem, id, blob, &pkg);
  EXPECT_EQ(t.expect.err, err);
  if (err != SPN_OK || t.expect.err != SPN_OK) { return; }

  EXPECT_EQ(num_releases, sp_da_size(pkg.releases));
  sp_for(it, num_releases) {
    if (it >= sp_da_size(pkg.releases)) { break; }
    release_test_check(utest_result, mem, &t.expect.releases[it], &pkg.releases[it]);
  }
}

UTEST_F(index_release, minimal) {
  run_release_test(utest_result, (release_test_t) {
    .file = "minimal.jsonl",
    .golden = true,
    .expect = {
      .releases = {
        {
          .namespace = "core",
          .name = "spum",
          .version = "1.0.0",
        },
      },
    },
  });
}

UTEST_F(index_release, full) {
  run_release_test(utest_result, (release_test_t) {
    .file = "full.jsonl",
    .golden = true,
    .expect = {
      .releases = {
        {
          .namespace = "core",
          .name = "spum",
          .version = "2.1.0",
          .yanked = true,
          .source = { .url = "https://github.com/example/spum", .rev = "abc123", .dir = "packages/spum" },
          .manifest = { .url = "https://github.com/example/packages", .rev = "def456", .dir = "spum" },
          .paths = { .manifest = "spn.toml", .script = "spn.c" },
          .deps = {
            { .namespace = "core", .name = "curl", .version = "^1.0.0", .kind = SPN_INDEX_DEP_NORMAL },
            { .namespace = "core", .name = "cmake", .version = "^3.0.0", .kind = SPN_INDEX_DEP_BUILD },
            { .namespace = "core", .name = "utest", .version = "^1.0.0", .kind = SPN_INDEX_DEP_TEST },
          },
          .targets = {
            { .name = "spum", .linkages = { SPN_LIB_KIND_SOURCE, SPN_LIB_KIND_STATIC, SPN_LIB_KIND_SHARED } },
            { .name = "spum1", .linkages = { SPN_LIB_KIND_OBJECT } },
          },
        },
      },
    },
  });
}

UTEST_F(index_release, partial) {
  run_release_test(utest_result, (release_test_t) {
    .file = "partial.jsonl",
    .golden = true,
    .expect = {
      .releases = {
        {
          .namespace = "core",
          .name = "spum",
          .version = "1.0.0",
          .source = { .url = "https://github.com/example/spum" },
          .paths = { .manifest = "spn.toml" },
        },
      },
    },
  });
}

UTEST_F(index_release, multiple) {
  run_release_test(utest_result, (release_test_t) {
    .file = "multiple.jsonl",
    .golden = true,
    .expect = {
      .releases = {
        { .namespace = "core", .name = "spum", .version = "1.0.0" },
        { .namespace = "core", .name = "spum", .version = "2.0.0" },
      },
    },
  });
}

UTEST_F(index_release, unknown_linkage) {
  run_release_test(utest_result, (release_test_t) {
    .file = "unknown_linkage.jsonl",
    .expect = {
      .releases = {
        {
          .namespace = "core",
          .name = "spum",
          .version = "1.0.0",
          .targets = {
            { .name = "spum", .linkages = { SPN_LIB_KIND_STATIC } },
          },
        },
      },
    },
  });
}

UTEST_F(index_release, linkage_non_string) {
  run_release_test(utest_result, (release_test_t) {
    .file = "linkage_non_string.jsonl",
    .expect = {
      .releases = {
        {
          .namespace = "core",
          .name = "spum",
          .version = "1.0.0",
          .targets = {
            { .name = "spum", .linkages = { SPN_LIB_KIND_STATIC } },
          },
        },
      },
    },
  });
}

UTEST_F(index_release, unknown_dep_kind) {
  run_release_test(utest_result, (release_test_t) {
    .file = "unknown_dep_kind.jsonl",
    .expect = {
      .releases = {
        {
          .namespace = "core",
          .name = "spum",
          .version = "1.0.0",
          .deps = {
            { .namespace = "core", .name = "curl", .version = "^1.0.0", .kind = SPN_INDEX_DEP_NORMAL },
          },
        },
      },
    },
  });
}

UTEST_F(index_release, deps_non_object) {
  run_release_test(utest_result, (release_test_t) {
    .file = "deps_non_object.jsonl",
    .expect = {
      .releases = {
        {
          .namespace = "core",
          .name = "spum",
          .version = "1.0.0",
          .deps = {
            { .namespace = "core", .name = "curl", .version = "^1.0.0", .kind = SPN_INDEX_DEP_NORMAL },
          },
        },
      },
    },
  });
}

UTEST_F(index_release, dep_kind_missing) {
  run_release_test(utest_result, (release_test_t) {
    .file = "dep_kind_missing.jsonl",
    .expect = {
      .releases = {
        {
          .namespace = "core",
          .name = "spum",
          .version = "1.0.0",
          .deps = {
            { .namespace = "core", .name = "curl", .version = "^1.0.0", .kind = SPN_INDEX_DEP_NORMAL },
          },
        },
      },
    },
  });
}

UTEST_F(index_release, unknown_field) {
  run_release_test(utest_result, (release_test_t) {
    .file = "unknown_field.jsonl",
    .expect = {
      .releases = {
        { .namespace = "core", .name = "spum", .version = "1.0.0" },
      },
    },
  });
}

UTEST_F(index_release, targets_not_array) {
  run_release_test(utest_result, (release_test_t) {
    .file = "targets_not_array.jsonl",
    .expect = {
      .releases = {
        { .namespace = "core", .name = "spum", .version = "1.0.0" },
      },
    },
  });
}

UTEST_F(index_release, unsorted) {
  run_release_test(utest_result, (release_test_t) {
    .file = "unsorted.jsonl",
    .expect = {
      .releases = {
        { .namespace = "core", .name = "spum", .version = "1.0.0" },
        { .namespace = "core", .name = "spum", .version = "2.0.0" },
        { .namespace = "core", .name = "spum", .version = "3.0.0" },
      },
    },
  });
}

UTEST_F(index_release, blank_lines) {
  run_release_test(utest_result, (release_test_t) {
    .file = "blank_lines.jsonl",
    .expect = {
      .releases = {
        { .namespace = "core", .name = "spum", .version = "1.0.0" },
        { .namespace = "core", .name = "spum", .version = "2.0.0" },
      },
    },
  });
}

UTEST_F(index_release, duplicate_version) {
  run_release_test(utest_result, (release_test_t) {
    .file = "duplicate_version.jsonl",
    .expect = { .err = SPN_ERROR },
  });
}

UTEST_F(index_release, version_missing) {
  run_release_test(utest_result, (release_test_t) {
    .file = "version_missing.jsonl",
    .expect = { .err = SPN_ERROR },
  });
}

UTEST_F(index_release, version_invalid) {
  run_release_test(utest_result, (release_test_t) {
    .file = "version_invalid.jsonl",
    .expect = { .err = SPN_ERROR },
  });
}

UTEST_F(index_release, version_noncanonical) {
  run_release_test(utest_result, (release_test_t) {
    .file = "version_noncanonical.jsonl",
    .expect = { .err = SPN_ERROR },
  });
}

UTEST_F(index_release, version_non_string) {
  run_release_test(utest_result, (release_test_t) {
    .file = "version_non_string.jsonl",
    .expect = { .err = SPN_ERROR },
  });
}

UTEST_F(index_release, name_mismatch) {
  run_release_test(utest_result, (release_test_t) {
    .file = "name_mismatch.jsonl",
    .expect = { .err = SPN_ERROR },
  });
}

UTEST_F(index_release, namespace_mismatch) {
  run_release_test(utest_result, (release_test_t) {
    .file = "namespace_mismatch.jsonl",
    .expect = { .err = SPN_ERROR },
  });
}

UTEST_F(index_release, empty) {
  run_release_test(utest_result, (release_test_t) {
    .file = "empty.jsonl",
    .expect = { .err = SPN_ERROR },
  });
}

UTEST_F(index_release, invalid_json) {
  run_release_test(utest_result, (release_test_t) {
    .file = "invalid_json.jsonl",
    .expect = { .err = SPN_ERROR },
  });
}

UTEST_F(index_release, non_object) {
  run_release_test(utest_result, (release_test_t) {
    .file = "non_object.jsonl",
    .expect = { .err = SPN_ERROR },
  });
}
