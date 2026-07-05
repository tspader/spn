#include "sp.h"
#include "utest.h"
#include "test.h"

#include "ctx/types.h"
#include "index/index.h"
#include "index/publish.h"
#include "semver/convert.h"

typedef struct {
  const c8* name;
  git_repo_fixture_t repo;
  git_repo_fixture_t source_repo;

  struct {
    const c8* subdir;
    const c8* source_rev;
  } opts;

  struct {
    spn_err_t kind;
    const c8* namespace;
    const c8* name;
    spn_semver_t version;
    const c8* source_rev;
    const c8* source_dir;
    const c8* manifest_url;
    const c8* manifest_rev;
    const c8* manifest_dir;
  } expect;
} case_t;

struct cmd_publish {
  s32 unused;
};

static void run_case(s32* utest_result, struct cmd_publish* fixture, case_t c) {
  ctx_t* harness = ctx_get();
  sp_mem_t mem = sp_mem_arena_as_allocator(harness->arena);

  git_repo_result_t source_repo = SP_ZERO_INITIALIZE();
  if (c.source_repo.name) {
    source_repo = git_repo_build(&harness->fs, c.source_repo.name, &c.source_repo);

    sp_carr_for(c.repo.commits, ci) {
      if (!c.repo.commits[ci].message) break;
      sp_carr_for(c.repo.commits[ci].files, fi) {
        git_repo_file_t* file = &c.repo.commits[ci].files[fi];
        if (!file->path) break;
        if (!sp_str_ends_with(sp_str_view(file->path), SP_LIT("spn.toml"))) continue;
        if (!sp_str_contains(sp_str_view(file->content), SP_LIT("PLACEHOLDER"))) continue;

        sp_str_t content = sp_fmt(
          mem,
          ts(package)
          "name = \"{}\"\n"
          "version = \"{}\"\n"
          "url = \"{}\"\n"
          "commit = \"{}\"\n",
          sp_fmt_cstr(c.expect.name),
          sp_fmt_str(spn_semver_to_str(mem, c.expect.version)),
          sp_fmt_str(source_repo.path),
          sp_fmt_str(source_repo.commits[0])
        ).value;
        file->content = sp_str_to_cstr(mem, content);
      }
    }
  }

  git_repo_result_t repo = git_repo_build(&harness->fs, c.repo.name, &c.repo);

  sp_str_t index_root = tmpfs_get(&harness->fs,
    sp_fmt(mem, "{}_index", sp_fmt_cstr(c.name)).value);
  sp_fs_create_dir(index_root);

  spn_index_info_t index = { .location = index_root };
  spn_index_init(&index, mem);

  sp_str_t cwd = repo.path;
  if (c.opts.subdir) {
    cwd = sp_fs_join_path(mem, repo.path, sp_str_view(c.opts.subdir));
  }

  u32 rev_idx = c.opts.source_rev ? sp_parse_u32(sp_str_view(c.opts.source_rev)) : 0;

  spn_publish_opts_t opts = {
    .mem = mem,
    .intern = spn.intern,
    .cwd = cwd,
    .index = &index,
    .url = repo.path,
    .revision = repo.commits[rev_idx],
  };

  spn_err_union_t result = spn_publish(&opts);
  EXPECT_EQ(c.expect.kind, result.kind);

  if (c.expect.namespace && result.kind == SPN_OK) {
    spn_index_pkg_t* pkg = spn_index_get_package(&index, (spn_pkg_name_t) {
      .namespace = sp_str_view(c.expect.namespace),
      .name = sp_str_view(c.expect.name),
    });

    EXPECT_TRUE(pkg != SP_NULLPTR);
    if (pkg && sp_da_size(pkg->releases) > 0) {
      spn_index_rel_t* rel = &pkg->releases[0];

      EXPECT_EQ(c.expect.version.major, rel->version.major);
      EXPECT_EQ(c.expect.version.minor, rel->version.minor);
      EXPECT_EQ(c.expect.version.patch, rel->version.patch);

      if (source_repo.path.len) {
        SP_EXPECT_STR_EQ(rel->source.url, source_repo.path);
      } else {
        SP_EXPECT_STR_EQ(rel->source.url, repo.path);
      }

      if (c.expect.source_rev) {
        git_repo_result_t* rev_repo = source_repo.path.len ? &source_repo : &repo;
        u32 expect_rev_idx = sp_parse_u32(sp_str_view(c.expect.source_rev));
        SP_EXPECT_STR_EQ(rel->source.rev, rev_repo->commits[expect_rev_idx]);
      }

      if (c.expect.source_dir) {
        SP_EXPECT_STR_EQ_CSTR(rel->source.dir, c.expect.source_dir);
      }

      if (c.expect.manifest_url) {
        SP_EXPECT_STR_EQ(rel->manifest.url, repo.path);

        if (c.expect.manifest_rev) {
          u32 expect_rev_idx = sp_parse_u32(sp_str_view(c.expect.manifest_rev));
          SP_EXPECT_STR_EQ(rel->manifest.rev, repo.commits[expect_rev_idx]);
        }

        if (c.expect.manifest_dir) {
          SP_EXPECT_STR_EQ_CSTR(rel->manifest.dir, c.expect.manifest_dir);
        }
      } else {
        SP_EXPECT_STR_EQ(rel->manifest.url, sp_str_lit(""));
      }
    }
  }

  spn_index_deinit(&index);
}

UTEST_F_SETUP(cmd_publish) {
}

UTEST_F_TEARDOWN(cmd_publish) {
}

UTEST_F(cmd_publish, native_package) {
  run_case(utest_result, uf, (case_t) {
    .name = "native_package",
    .repo = {
      .name = "spum",
      .commits = {
        {
          .message = "v1",
          .files = {
            { "spn.toml",
              ts(package) "\n"
              tkv(namespace, "core") "\n"
              tkv(name, "spum") "\n"
              tkv(version, "1.0.0") "\n"
            },
            { "spn.c", "void build() {}" },
          },
        },
      },
    },
    .expect = {
      .kind = SPN_OK,
      .namespace = "core",
      .name = "spum",
      .version = spn_semver_lit(1, 0, 0),
      .source_rev = "0",
      .source_dir = "",
    },
  });
}

UTEST_F(cmd_publish, monorepo_subdir) {
  run_case(utest_result, uf, (case_t) {
    .name = "monorepo_subdir",
    .repo = {
      .name = "mono",
      .commits = {
        {
          .message = "initial",
          .files = {
            { "packages/spum/spn.toml",
              ts(package) "\n"
              tkv(namespace, "core") "\n"
              tkv(name, "spum") "\n"
              tkv(version, "2.0.0") "\n"
            },
            { "packages/spum/spn.c", "void build() {}" },
          },
        },
      },
    },
    .opts = {
      .subdir = "packages/spum",
    },
    .expect = {
      .kind = SPN_OK,
      .namespace = "core",
      .name = "spum",
      .version = spn_semver_lit(2, 0, 0),
      .source_rev = "0",
      .source_dir = "packages/spum",
    },
  });
}

UTEST_F(cmd_publish, second_commit) {
  run_case(utest_result, uf, (case_t) {
    .name = "second_commit",
    .repo = {
      .name = "versioned",
      .commits = {
        {
          .message = "v1",
          .files = {
            { "spn.toml",
              ts(package) "\n"
              tkv(namespace, "core") "\n"
              tkv(name, "spum") "\n"
              tkv(version, "1.0.0") "\n"
            },
            { "spn.c", "void build() {}" },
          },
        },
        {
          .message = "v2",
          .files = {
            { "spn.toml",
              ts(package) "\n"
              tkv(namespace, "core") "\n"
              tkv(name, "spum") "\n"
              tkv(version, "2.0.0") "\n"
            },
            { "spn.c", "void build() {}" },
          },
        },
      },
    },
    .opts = {
      .source_rev = "1",
    },
    .expect = {
      .kind = SPN_OK,
      .namespace = "core",
      .name = "spum",
      .version = spn_semver_lit(2, 0, 0),
      .source_rev = "1",
      .source_dir = "",
    },
  });
}

UTEST_F(cmd_publish, out_of_tree_manifest) {
  run_case(utest_result, uf, (case_t) {
    .name = "out_of_tree",
    .source_repo = {
      .name = "upstream",
      .commits = {
        {
          .message = "initial",
          .files = {
            { "toml.h", "#pragma once" },
            { "toml.c", "int parse() { return 0; }" },
          },
        },
      },
    },
    .repo = {
      .name = "spam",
      .commits = {
        {
          .message = "add toml wrapper",
          .files = {
            { "toml/spn.toml", "PLACEHOLDER" },
            { "toml/spn.c", "void package() {}" },
          },
        },
      },
    },
    .opts = {
      .subdir = "toml",
    },
    .expect = {
      .kind = SPN_OK,
      .namespace = "core",
      .name = "toml",
      .version = spn_semver_lit(1, 2, 1),
      .source_rev = "0",
      .source_dir = "",
      .manifest_url = "spam",
      .manifest_rev = "0",
      .manifest_dir = "toml",
    },
  });
}

UTEST_F(cmd_publish, out_of_tree_manifest_root) {
  run_case(utest_result, uf, (case_t) {
    .name = "out_of_tree_root",
    .source_repo = {
      .name = "upstream",
      .commits = {
        {
          .message = "initial",
          .files = {
            { "lib.h", "#pragma once" },
          },
        },
      },
    },
    .repo = {
      .name = "wrapper",
      .commits = {
        {
          .message = "add wrapper",
          .files = {
            { "spn.toml", "PLACEHOLDER" },
            { "spn.c", "void package() {}" },
          },
        },
      },
    },
    .expect = {
      .kind = SPN_OK,
      .namespace = "core",
      .name = "spum",
      .version = spn_semver_lit(1, 0, 0),
      .source_rev = "0",
      .source_dir = "",
      .manifest_url = "wrapper",
      .manifest_rev = "0",
      .manifest_dir = "",
    },
  });
}
