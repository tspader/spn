#include "spn_test.h"

#include "ctx/types.h"
#include "event/event.h"
#include "index/index.h"
#include "index/publish.h"
#include "intern/intern.h"
#include "semver/convert.h"

// The linked production ctx.c reads this global (spn_intern -> spn.intern),
// so the suite keeps it, brings it up once, and runs serial
spn_ctx_t spn;

sp_test_suite(cmd_publish, .serial = true);

static sp_test_once_t spn_init_once;

static sp_err_t init_spn(void* user) {
  spn.mem = sp_mem_os_new();
  spn.intern = sp_intern_new(spn.mem);
  spn.events = spn_event_buffer_new(spn.mem);
  return SP_OK;
}

static sp_err_t setup_spn(sp_test_t* t) {
  return sp_test_once(&spn_init_once, init_spn, SP_NULLPTR);
}

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
    struct {
      const c8* name;
      spn_linkage_t linkages [4];
    } targets [2];
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
      .source_rev = "0",
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
      .source_rev = "0",
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
      .source_rev = "0",
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
      .source_rev = "1",
    },
    .expect = {
      .kind = SPN_OK,
      .namespace = "core",
      .name = "spum",
      .version = { .major = 2 },
      .source_rev = "1",
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
      .version = { .major = 1, .minor = 2, .patch = 1 },
      .source_rev = "0",
      .source_dir = "",
      .manifest_url = "spam",
      .manifest_rev = "0",
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
      .version = { .major = 1 },
      .source_rev = "0",
      .source_dir = "",
      .manifest_url = "wrapper",
      .manifest_rev = "0",
      .manifest_dir = "",
    },
  },
};

sp_test_each(cmd_publish, publish, case_t, cases, .setup = setup_spn) {
  sp_mem_t mem = sp_test_arena(t);
  case_t c = *it;

  git_repo_result_t source_repo = sp_zero;
  if (c.source_repo.name) {
    source_repo = git_repo_build_at(sp_test_dir(t), c.source_repo.name, &c.source_repo);

    // An out-of-tree manifest can't know the source repo's url or commit until
    // that repo exists; PLACEHOLDER manifests are rewritten with the real
    // values once it has been built
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

  git_repo_result_t repo = git_repo_build_at(sp_test_dir(t), c.repo.name, &c.repo);

  sp_str_t index_root = sp_fs_join_path(mem, sp_test_dir(t), sp_str_lit("index"));
  sp_fs_create_dir(index_root);

  spn_index_info_t index = {
    .location = index_root,
    .protocol = SPN_INDEX_PROTOCOL_FILESYSTEM,
  };
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
    .url = repo.path,
    .revision = repo.commits[rev_idx],
  };

  spn_index_release_t release = sp_zero;
  spn_err_union_t result = spn_publish_build(&opts, &release);
  if (!result.kind) {
    result = spn_index_publish(&index, &release);
  }
  sp_expect_eq(t, c.expect.kind, result.kind);

  if (c.expect.namespace && result.kind == SPN_OK) {
    spn_index_pkg_t* pkg = spn_index_get_package(&index, (spn_pkg_name_t) {
      .namespace = sp_str_view(c.expect.namespace),
      .name = sp_str_view(c.expect.name),
    });

    sp_expect(t, pkg != SP_NULLPTR);
    if (pkg && sp_da_size(pkg->releases) > 0) {
      spn_index_release_t* rel = &pkg->releases[0];

      sp_expect_eq(t, c.expect.version.major, rel->version.major);
      sp_expect_eq(t, c.expect.version.minor, rel->version.minor);
      sp_expect_eq(t, c.expect.version.patch, rel->version.patch);

      if (source_repo.path.len) {
        sp_expect_str_eq(t, rel->source.url, source_repo.path);
      } else {
        sp_expect_str_eq(t, rel->source.url, repo.path);
      }

      if (c.expect.source_rev) {
        git_repo_result_t* rev_repo = source_repo.path.len ? &source_repo : &repo;
        u32 expect_rev_idx = sp_parse_u32(sp_str_view(c.expect.source_rev));
        sp_expect_str_eq(t, rel->source.rev, rev_repo->commits[expect_rev_idx]);
      }

      if (c.expect.source_dir) {
        sp_expect_str_eq_c(t, rel->source.dir, c.expect.source_dir);
      }

      if (c.expect.manifest_url) {
        sp_expect_str_eq(t, rel->manifest.url, repo.path);

        if (c.expect.manifest_rev) {
          u32 expect_rev_idx = sp_parse_u32(sp_str_view(c.expect.manifest_rev));
          sp_expect_str_eq(t, rel->manifest.rev, repo.commits[expect_rev_idx]);
        }

        if (c.expect.manifest_dir) {
          sp_expect_str_eq_c(t, rel->manifest.dir, c.expect.manifest_dir);
        }
      } else {
        sp_expect_str_eq(t, rel->manifest.url, sp_str_lit(""));
      }

      u32 expected_targets = 0;
      sp_carr_for(c.expect.targets, ti) {
        if (!c.expect.targets[ti].name) break;
        expected_targets++;
      }
      sp_expect_eq(t, expected_targets, sp_da_size(rel->targets));

      sp_for(ti, expected_targets) {
        if (ti >= sp_da_size(rel->targets)) break;
        sp_expect_str_eq_c(t, rel->targets[ti].name, c.expect.targets[ti].name);

        u32 expected_linkages = 0;
        sp_carr_for(c.expect.targets[ti].linkages, li) {
          if (c.expect.targets[ti].linkages[li] == SPN_LIB_KIND_NONE) break;
          expected_linkages++;
        }
        sp_expect_eq(t, expected_linkages, sp_da_size(rel->targets[ti].linkages));

        sp_for(li, expected_linkages) {
          if (li >= sp_da_size(rel->targets[ti].linkages)) break;
          sp_expect_eq(t, c.expect.targets[ti].linkages[li], rel->targets[ti].linkages[li]);
        }
      }
    }
  }

  spn_index_deinit(&index);
  return SP_OK;
}
