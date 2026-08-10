#include "index.h"

typedef struct {
  const c8* namespace;
  const c8* name;
  const c8* version;
  spn_index_dep_kind_t kind;
} release_dep_t;

typedef struct {
  const c8* name;
  spn_linkage_t linkages [4];
} release_target_t;

typedef struct {
  const c8* namespace;
  const c8* name;
  const c8* version;
  bool yanked;
  struct { const c8* url; const c8* rev; const c8* dir; } source;
  struct { const c8* url; const c8* rev; const c8* dir; } manifest;
  struct { const c8* manifest; const c8* script; } paths;
  release_dep_t deps [4];
  release_target_t targets [4];
} release_rel_t;

typedef struct {
  spn_err_t err;
  release_rel_t releases [4];
} release_expect_t;

typedef struct {
  const c8* name;
  bool golden;
  release_expect_t expect;
} release_test_t;

static const release_test_t tests [] = {
  {
    .name = "minimal",
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
  },
  {
    .name = "full",
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
  },
  {
    .name = "partial",
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
  },
  {
    .name = "multiple",
    .golden = true,
    .expect = {
      .releases = {
        { .namespace = "core", .name = "spum", .version = "1.0.0" },
        { .namespace = "core", .name = "spum", .version = "2.0.0" },
      },
    },
  },
  {
    .name = "unknown_linkage",
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
  },
  {
    .name = "linkage_non_string",
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
  },
  {
    .name = "unknown_dep_kind",
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
  },
  {
    .name = "deps_non_object",
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
  },
  {
    .name = "dep_kind_missing",
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
  },
  {
    .name = "unknown_field",
    .expect = {
      .releases = {
        { .namespace = "core", .name = "spum", .version = "1.0.0" },
      },
    },
  },
  {
    .name = "targets_not_array",
    .expect = {
      .releases = {
        { .namespace = "core", .name = "spum", .version = "1.0.0" },
      },
    },
  },
  {
    .name = "unsorted",
    .expect = {
      .releases = {
        { .namespace = "core", .name = "spum", .version = "1.0.0" },
        { .namespace = "core", .name = "spum", .version = "2.0.0" },
        { .namespace = "core", .name = "spum", .version = "3.0.0" },
      },
    },
  },
  {
    .name = "blank_lines",
    .expect = {
      .releases = {
        { .namespace = "core", .name = "spum", .version = "1.0.0" },
        { .namespace = "core", .name = "spum", .version = "2.0.0" },
      },
    },
  },
  {
    .name = "duplicate_version",
    .expect = { .err = SPN_ERROR },
  },
  {
    .name = "dep_range_invalid",
    .expect = { .err = SPN_ERROR },
  },
  {
    .name = "version_missing",
    .expect = { .err = SPN_ERROR },
  },
  {
    .name = "version_invalid",
    .expect = { .err = SPN_ERROR },
  },
  {
    .name = "version_noncanonical",
    .expect = { .err = SPN_ERROR },
  },
  {
    .name = "version_non_string",
    .expect = { .err = SPN_ERROR },
  },
  {
    .name = "name_mismatch",
    .expect = { .err = SPN_ERROR },
  },
  {
    .name = "namespace_mismatch",
    .expect = { .err = SPN_ERROR },
  },
  {
    .name = "empty",
    .expect = { .err = SPN_ERROR },
  },
  {
    .name = "invalid_json",
    .expect = { .err = SPN_ERROR },
  },
  {
    .name = "non_object",
    .expect = { .err = SPN_ERROR },
  },
};

static spn_index_release_t release_build(sp_mem_t mem, const release_rel_t* rel) {
  spn_index_release_t built = {
    .id = {
      .namespace = sp_cstr_as_str(rel->namespace),
      .name = sp_cstr_as_str(rel->name),
    },
    .version = spn_semver_from_str(sp_cstr_as_str(rel->version)),
    .yanked = rel->yanked,
  };

  if (rel->source.url) {
    built.source = (spn_pkg_root_t) {
      .kind = SPN_PKG_ROOT_GIT,
      .git = {
        .url = sp_cstr_as_str(rel->source.url),
        .rev = sp_cstr_as_str(rel->source.rev ? rel->source.rev : ""),
        .dir = sp_cstr_as_str(rel->source.dir ? rel->source.dir : ""),
      },
    };
  }
  if (rel->manifest.url) {
    built.manifest = (spn_pkg_root_t) {
      .kind = SPN_PKG_ROOT_GIT,
      .git = {
        .url = sp_cstr_as_str(rel->manifest.url),
        .rev = sp_cstr_as_str(rel->manifest.rev ? rel->manifest.rev : ""),
        .dir = sp_cstr_as_str(rel->manifest.dir ? rel->manifest.dir : ""),
      },
    };
  }
  if (rel->paths.manifest) { built.paths.manifest = sp_cstr_as_str(rel->paths.manifest); }
  if (rel->paths.script) { built.paths.script = sp_cstr_as_str(rel->paths.script); }

  sp_da_init(mem, built.deps);
  sp_carr_for(rel->deps, it) {
    if (!rel->deps[it].namespace) { break; }
    spn_index_dep_t dep = {
      .kind = rel->deps[it].kind,
      .id = {
        .namespace = sp_cstr_as_str(rel->deps[it].namespace),
        .name = sp_cstr_as_str(rel->deps[it].name),
      },
    };
    sp_assert(!spn_semver_parse_range(sp_cstr_as_str(rel->deps[it].version), &dep.range));
    sp_da_push(built.deps, dep);
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

static sp_err_t release_check(sp_test_t* t, sp_mem_t mem, const release_rel_t* expected, spn_index_release_t* rel) {
  sp_expect_str_eq_c(t, rel->id.namespace, expected->namespace);
  sp_expect_str_eq_c(t, rel->id.name, expected->name);
  sp_expect_str_eq_c(t, spn_semver_to_str(mem, rel->version), expected->version);
  sp_expect_eq(t, expected->yanked, rel->yanked);

  if (expected->source.url) {
    sp_must_eq(t, SPN_PKG_ROOT_GIT, rel->source.kind);
    sp_expect_str_eq_c(t, rel->source.git.url, expected->source.url);
    if (expected->source.rev) { sp_expect_str_eq_c(t, rel->source.git.rev, expected->source.rev); }
    if (expected->source.dir) { sp_expect_str_eq_c(t, rel->source.git.dir, expected->source.dir); }
  } else {
    sp_expect_eq(t, SPN_PKG_ROOT_NONE, rel->source.kind);
  }
  if (expected->manifest.url) {
    sp_must_eq(t, SPN_PKG_ROOT_GIT, rel->manifest.kind);
    sp_expect_str_eq_c(t, rel->manifest.git.url, expected->manifest.url);
    if (expected->manifest.rev) { sp_expect_str_eq_c(t, rel->manifest.git.rev, expected->manifest.rev); }
    if (expected->manifest.dir) { sp_expect_str_eq_c(t, rel->manifest.git.dir, expected->manifest.dir); }
  } else {
    sp_expect_eq(t, SPN_PKG_ROOT_NONE, rel->manifest.kind);
  }
  if (expected->paths.manifest) { sp_expect_str_eq_c(t, rel->paths.manifest, expected->paths.manifest); }
  if (expected->paths.script) { sp_expect_str_eq_c(t, rel->paths.script, expected->paths.script); }

  u32 num_deps = 0;
  sp_carr_detect_len(expected->deps, num_deps, expected->deps[num_deps].namespace);
  sp_must_eq(t, num_deps, sp_da_size(rel->deps));

  sp_for(it, num_deps) {
    sp_expect_str_eq_c(t, rel->deps[it].id.namespace, expected->deps[it].namespace);
    sp_expect_str_eq_c(t, rel->deps[it].id.name, expected->deps[it].name);
    sp_expect_str_eq_c(t, spn_semver_range_to_str(mem, rel->deps[it].range), expected->deps[it].version);
    sp_expect_eq(t, expected->deps[it].kind, rel->deps[it].kind);
  }

  u32 num_targets = 0;
  sp_carr_detect_len(expected->targets, num_targets, expected->targets[num_targets].name);
  sp_must_eq(t, num_targets, sp_da_size(rel->targets));

  sp_for(it, num_targets) {
    sp_expect_str_eq_c(t, rel->targets[it].name, expected->targets[it].name);

    u32 num_linkages = 0;
    sp_carr_detect_len(expected->targets[it].linkages, num_linkages, expected->targets[it].linkages[num_linkages] != SPN_LIB_KIND_NONE);
    sp_must_eq(t, num_linkages, sp_da_size(rel->targets[it].linkages));

    sp_for(kind, num_linkages) {
      sp_expect_eq(t, expected->targets[it].linkages[kind], rel->targets[it].linkages[kind]);
    }
  }

  return SP_OK;
}

sp_test_each(index_release, parse, release_test_t, tests) {
  sp_mem_t mem = sp_test_arena(t);
  sp_str_t file = sp_test_format(t, "releases/{}.jsonl", sp_fmt_cstr(it->name));
  sp_str_t path = test_repo_path(mem, sp_test_format(t, "test/core/index/{}", sp_fmt_str(file)));

  u32 num_releases = 0;
  sp_carr_detect_len(it->expect.releases, num_releases, it->expect.releases[num_releases].name);

  if (it->golden) {
    sp_io_dyn_mem_writer_t rendered = sp_zero;
    sp_io_dyn_mem_writer_init(mem, &rendered);
    sp_for(at, num_releases) {
      spn_index_release_t rel = release_build(mem, &it->expect.releases[at]);
      sp_io_write_line(&rendered.base, spn_index_release_to_json(mem, &rel));
    }

    sp_expect_golden(t, file, sp_io_dyn_mem_writer_as_str(&rendered));
  }

  sp_str_t blob = sp_zero;
  sp_io_read_file(mem, path, &blob);

  spn_pkg_name_t id = {
    .namespace = sp_str_lit("core"),
    .name = sp_str_lit("spum"),
  };

  spn_index_pkg_t pkg = sp_zero;
  spn_err_t err = spn_index_parse_pkg(mem, id, blob, &pkg);
  sp_expect_eq(t, it->expect.err, err);
  if (err != SPN_OK || it->expect.err != SPN_OK) { return SP_OK; }

  sp_must_eq(t, num_releases, sp_da_size(pkg.releases));
  sp_for(at, num_releases) {
    release_check(t, mem, &it->expect.releases[at], &pkg.releases[at]);
  }

  return SP_OK;
}
