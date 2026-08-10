#include "spn_test.h"

#include "ctx/types.h"
#include "event/event.h"
#include "index/index.h"
#include "index/publish.h"
#include "intern/intern.h"
#include "semver/convert.h"

sp_test_suite(cmd_publish, .serial = true);

typedef struct {
  const c8* name;
  git_repo_fixture_t repo;
  git_repo_fixture_t source_repo;
  const c8* generated_manifest;

  struct {
    const c8* subdir;
    u32 source_commit;
  } opts;

  struct {
    spn_err_t kind;
    const c8* namespace;
    const c8* name;
    spn_semver_t version;
    u32 source_commit;
    const c8* source_dir;
    const c8* manifest_url;
    u32 manifest_commit;
    const c8* manifest_dir;
    struct {
      const c8* name;
      spn_linkage_t linkages [4];
    } targets [2];
    struct {
      const c8* name;
      const c8* dep;
    } path_dep;
  } expect;
} case_t;

static const case_t cases [] = {
  {
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
      .version = { .major = 1 },
      .source_commit = 1,
      .source_dir = "",
    },
  },
  {
    .name = "lib_targets",
    .repo = {
      .name = "spum_libs",
      .commits = {
        {
          .message = "v1",
          .files = {
            { "spn.toml",
              ts(package) "\n"
              tkv(namespace, "core") "\n"
              tkv(name, "spum") "\n"
              tkv(version, "1.0.0") "\n"
              "\n"
              "[[lib]]\n"
              tkv(name, "spum") "\n"
              "kinds = [\"static\", \"shared\"]\n"
              "source = [\"spum.c\"]\n"
            },
            { "spum.c", "int spum() { return 0; }" },
            { "spn.c", "void build() {}" },
          },
        },
      },
    },
    .expect = {
      .kind = SPN_OK,
      .namespace = "core",
      .name = "spum",
      .version = { .major = 1 },
      .source_commit = 1,
      .source_dir = "",
      .targets = {
        { .name = "spum", .linkages = { SPN_LIB_KIND_STATIC, SPN_LIB_KIND_SHARED } },
      },
    },
  },
  {
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
      .version = { .major = 2 },
      .source_commit = 1,
      .source_dir = "packages/spum",
    },
  },
  {
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
      .source_commit = 2,
    },
    .expect = {
      .kind = SPN_OK,
      .namespace = "core",
      .name = "spum",
      .version = { .major = 2 },
      .source_commit = 2,
      .source_dir = "",
    },
  },
  {
    .name = "out_of_tree_manifest",
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
            { "toml/spn.toml" },
            { "toml/spn.c", "void package() {}" },
          },
        },
      },
    },
    .generated_manifest = "toml/spn.toml",
    .opts = {
      .subdir = "toml",
    },
    .expect = {
      .kind = SPN_OK,
      .namespace = "core",
      .name = "toml",
      .version = { .major = 1, .minor = 2, .patch = 1 },
      .source_commit = 1,
      .source_dir = "",
      .manifest_url = "spam",
      .manifest_commit = 1,
      .manifest_dir = "toml",
    },
  },
  {
    .name = "out_of_tree_manifest_root",
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
            { "spn.toml" },
            { "spn.c", "void package() {}" },
          },
        },
      },
    },
    .generated_manifest = "spn.toml",
    .expect = {
      .kind = SPN_OK,
      .namespace = "core",
      .name = "spum",
      .version = { .major = 1 },
      .source_commit = 1,
      .source_dir = "",
      .manifest_url = "wrapper",
      .manifest_commit = 1,
      .manifest_dir = "",
    },
  },
  {
    .name = "path_dep_rejected",
    .repo = {
      .name = "A",
      .commits = {
        {
          .message = "v1",
          .files = {
            { "spn.toml",
              ts(package) "\n"
              tkv(name, "A") "\n"
              tkv(version, "1.0.0") "\n"
              "\n"
              ts(deps.package) "\n"
              "B = { path = \"../B\" }\n"
            },
            { "spn.c", "void build() {}" },
          },
        },
      },
    },
    .expect = {
      .kind = SPN_ERR_INDEX_PATH_DEP,
      .path_dep = { .name = "core/A", .dep = "core/B" },
    },
  },
};

sp_test_each(cmd_publish, publish, case_t, cases, .setup = spn_test_ctx_setup) {
  sp_mem_t mem = sp_test_arena(t);
  case_t c = *it;

  git_repo_result_t source_repo = sp_zero;
  if (c.source_repo.name) {
    source_repo = git_repo_build_at(sp_test_dir(t), c.source_repo.name, &c.source_repo);
  }

  if (c.generated_manifest) {
    SP_ASSERT(source_repo.commit_count);

    git_repo_file_t* manifest = SP_NULLPTR;
    sp_carr_for(c.repo.commits, ci) {
      if (!c.repo.commits[ci].message) break;
      sp_carr_for(c.repo.commits[ci].files, fi) {
        git_repo_file_t* file = &c.repo.commits[ci].files[fi];
        if (!file->path) break;
        if (sp_str_equal_cstr(sp_str_view(c.generated_manifest), file->path)) {
          manifest = file;
        }
      }
    }
    SP_ASSERT(manifest);

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
    manifest->content = sp_str_to_cstr(mem, content);
  }

  git_repo_result_t repo = git_repo_build_at(sp_test_dir(t), c.repo.name, &c.repo);

  sp_str_t index_root = sp_fs_join_path(mem, sp_test_dir(t), sp_str_lit("index"));
  sp_fs_create_dir(index_root);
  git_repo_init(index_root);
  git_repo_git(index_root, sp_str_lit("symbolic-ref"), sp_str_lit("HEAD"), sp_str_lit("refs/heads/main"));
  git_repo_git(index_root, sp_str_lit("config"), sp_str_lit("receive.denyCurrentBranch"), sp_str_lit("updateInstead"));
  git_repo_commit(index_root, sp_str_lit("seed"));

  spn_index_info_t index = {
    .git = { .url = index_root },
    .location = sp_fs_join_path(mem, sp_test_dir(t), sp_str_lit("index_clone")),
  };

  sp_str_t cwd = repo.path;
  if (c.opts.subdir) {
    cwd = sp_fs_join_path(mem, repo.path, sp_str_view(c.opts.subdir));
  }

  u32 rev_idx = c.opts.source_commit ? c.opts.source_commit - 1 : 0;
  SP_ASSERT(rev_idx < repo.commit_count);

  spn_publish_opts_t opts = {
    .mem = mem,
    .intern = spn.intern,
    .cwd = cwd,
    .url = repo.path,
    .revision = repo.commits[rev_idx],
  };

  spn_index_release_t release = sp_zero;
  spn_err_union_t result = spn_publish_build(&opts, &release);
  if (!result.kind) {
    result = spn_index_publish(&index, mem, &release);
  }
  sp_expect_eq(t, c.expect.kind, result.kind);

  if (c.expect.path_dep.name) {
    sp_expect_str_eq_c(t, result.pkg.name, c.expect.path_dep.name);
    sp_expect_str_eq_c(t, result.pkg.requested, c.expect.path_dep.dep);
  }

  if (c.expect.namespace && result.kind == SPN_OK) {
    spn_index_pkg_t* pkg = SP_NULLPTR;
    spn_index_get_package(&index, mem, spn.intern, (spn_pkg_name_t) {
      .namespace = sp_str_view(c.expect.namespace),
      .name = sp_str_view(c.expect.name),
    }, &pkg);

    sp_must(t, pkg);
    sp_must(t, sp_da_size(pkg->releases));
    spn_index_release_t* rel = &pkg->releases[0];

    sp_expect_eq(t, c.expect.version.major, rel->version.major);
    sp_expect_eq(t, c.expect.version.minor, rel->version.minor);
    sp_expect_eq(t, c.expect.version.patch, rel->version.patch);

    sp_must_eq(t, SPN_PKG_ROOT_GIT, rel->source.kind);
    if (source_repo.path.len) {
      sp_expect_str_eq(t, rel->source.git.url, source_repo.path);
    } else {
      sp_expect_str_eq(t, rel->source.git.url, repo.path);
    }

    if (c.expect.source_commit) {
      git_repo_result_t* rev_repo = source_repo.path.len ? &source_repo : &repo;
      SP_ASSERT(c.expect.source_commit <= rev_repo->commit_count);
      sp_expect_str_eq(t, rel->source.git.rev, rev_repo->commits[c.expect.source_commit - 1]);
    }

    if (c.expect.source_dir) {
      sp_expect_str_eq_c(t, rel->source.git.dir, c.expect.source_dir);
    }

    if (c.expect.manifest_url) {
      sp_must_eq(t, SPN_PKG_ROOT_GIT, rel->manifest.kind);
      sp_expect_str_eq(t, rel->manifest.git.url, repo.path);

      if (c.expect.manifest_commit) {
        SP_ASSERT(c.expect.manifest_commit <= repo.commit_count);
        sp_expect_str_eq(t, rel->manifest.git.rev, repo.commits[c.expect.manifest_commit - 1]);
      }

      if (c.expect.manifest_dir) {
        sp_expect_str_eq_c(t, rel->manifest.git.dir, c.expect.manifest_dir);
      }
    } else {
      sp_expect_eq(t, SPN_PKG_ROOT_NONE, rel->manifest.kind);
    }

    u32 expected_targets = 0;
    sp_carr_detect_len(c.expect.targets, expected_targets, c.expect.targets[expected_targets].name);
    sp_must_eq(t, expected_targets, sp_da_size(rel->targets));

    sp_for(ti, expected_targets) {
      sp_expect_str_eq_c(t, rel->targets[ti].name, c.expect.targets[ti].name);

      u32 expected_linkages = 0;
      sp_carr_detect_len(c.expect.targets[ti].linkages, expected_linkages, c.expect.targets[ti].linkages[expected_linkages] != SPN_LIB_KIND_NONE);
      sp_must_eq(t, expected_linkages, sp_da_size(rel->targets[ti].linkages));

      sp_for(li, expected_linkages) {
        sp_expect_eq(t, c.expect.targets[ti].linkages[li], rel->targets[ti].linkages[li]);
      }
    }
  }

  return SP_OK;
}
