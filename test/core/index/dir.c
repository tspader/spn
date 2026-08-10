#include "index.h"

#include "pkg/id.h"
#include "when/when.h"

#define DIR_TEST_MAX_DEPS 4
#define DIR_TEST_MAX_TARGETS 2
#define DIR_TEST_MAX_OPTIONS 2
#define DIR_TEST_MAX_LINKAGES 4

typedef struct {
  bool exists;
  spn_err_t err;
  spn_semver_t version;
  struct {
    const c8* namespace;
    const c8* name;
    const c8* range;
    spn_index_dep_kind_t kind;
    bool private;
    const c8* when;
    const c8* options;
  } deps [DIR_TEST_MAX_DEPS];
  struct { const c8* name; spn_linkage_t linkages [DIR_TEST_MAX_LINKAGES]; } targets [DIR_TEST_MAX_TARGETS];
  struct { const c8* name; spn_option_type_t type; u32 defaults; } options [DIR_TEST_MAX_OPTIONS];
  struct { const c8* url; const c8* rev; } source;
  const c8* dep;
} dir_expect_t;

typedef struct {
  const c8* name;
  const c8* package;
  const c8* toml;
  struct { const c8* namespace; const c8* name; } request;
  dir_expect_t expect;
} dir_test_t;

static const dir_test_t tests [] = {
  {
    .name = "release",
    .package = "A",
    .toml = "minimal",
    .expect = {
      .exists = true,
      .version = { 1, 2, 3 },
    },
  },
  {
    .name = "upstream",
    .package = "A",
    .toml = "upstream",
    .expect = {
      .exists = true,
      .version = { 1, 0, 0 },
      .source = { .url = "https://spn.dev/A.git", .rev = "deadbeef" },
    },
  },
  {
    .name = "deps",
    .package = "A",
    .toml = "deps",
    .expect = {
      .exists = true,
      .version = { 1, 0, 0 },
      .deps = {
        { .namespace = "core", .name = "B", .range = "^1.0.0", .private = true, .when = "os = \"linux\"", .options = "O = true" },
        { .namespace = "core", .name = "C", .range = "^2.0.0" },
      },
    },
  },
  {
    .name = "dep_kinds",
    .package = "A",
    .toml = "kinds",
    .expect = {
      .exists = true,
      .version = { 1, 0, 0 },
      .deps = {
        { .namespace = "core", .name = "B", .range = "^1.0.0" },
        { .namespace = "core", .name = "D", .range = "^1.0.0", .kind = SPN_INDEX_DEP_TEST },
        { .namespace = "core", .name = "C", .range = "^1.0.0", .kind = SPN_INDEX_DEP_BUILD },
      },
    },
  },
  {
    .name = "targets",
    .package = "A",
    .toml = "targets",
    .expect = {
      .exists = true,
      .version = { 1, 0, 0 },
      .targets = {
        { .name = "A", .linkages = { SPN_LIB_KIND_STATIC, SPN_LIB_KIND_SHARED } },
      },
    },
  },
  {
    .name = "options",
    .package = "A",
    .toml = "options",
    .expect = {
      .exists = true,
      .version = { 1, 0, 0 },
      .options = {
        { .name = "O", .type = SPN_OPTION_TYPE_BOOL, .defaults = 1 },
      },
    },
  },
  {
    .name = "namespaced",
    .package = "A",
    .toml = "namespaced",
    .request = { .namespace = "N" },
    .expect = {
      .exists = true,
      .version = { 1, 0, 0 },
    },
  },
  {
    .name = "missing",
    .request = { .name = "A" },
  },
  {
    .name = "empty_dir",
    .package = "A",
  },
  {
    .name = "broken_manifest",
    .package = "A",
    .toml = "broken",
    .expect = {
      .err = SPN_ERR_MANIFEST_ISSUES,
    },
  },
  {
    .name = "namespace_mismatch",
    .package = "A",
    .toml = "namespaced",
  },
  {
    .name = "name_mismatch",
    .package = "A",
    .toml = "renamed",
    .expect = {
      .err = SPN_ERR_INDEX_CORRUPT,
    },
  },
  {
    .name = "path_dep",
    .package = "A",
    .toml = "path_dep",
    .expect = {
      .err = SPN_ERR_INDEX_PATH_DEP,
      .dep = "core/B",
    },
  },
};

sp_test_each(index_dir, get_package, dir_test_t, tests, .setup = spn_test_ctx_setup) {
  sp_mem_t mem = sp_test_arena(t);

  sp_str_t location = sp_fs_join_path(mem, sp_test_dir(t), sp_str_lit("index"));
  sp_fs_create_dir(location);

  sp_str_t package = sp_zero;
  sp_str_t manifest = sp_zero;
  if (it->package) {
    package = sp_fs_join_path(mem, location, sp_cstr_as_str(it->package));
    manifest = sp_fs_join_path(mem, package, sp_str_lit("spn.toml"));
    sp_fs_create_dir(package);
    if (it->toml) {
      sp_str_t fixture = test_repo_path(mem, sp_test_format(t, "test/core/index/packages/{}.toml", sp_fmt_cstr(it->toml)));
      sp_str_t toml = sp_zero;
      sp_io_read_file(mem, fixture, &toml);
      sp_fs_create_file_str(manifest, toml);
    }
  }

  spn_index_info_t index = {
    .location = location,
    .protocol = SPN_INDEX_PROTOCOL_DIR,
  };

  spn_pkg_name_t request = {
    .namespace = sp_cstr_as_str(it->request.namespace ? it->request.namespace : "core"),
    .name = sp_cstr_as_str(it->request.name ? it->request.name : it->package),
  };

  spn_index_pkg_t* pkg = SP_NULLPTR;
  spn_err_union_t err = spn_index_get_package(&index, mem, spn.intern, request, &pkg);

  sp_expect_eq(t, it->expect.err, err.kind);
  sp_must_eq(t, it->expect.exists, pkg != SP_NULLPTR);

  switch (err.kind) {
    case SPN_ERR_INDEX_CORRUPT: {
      sp_expect_str_eq(t, err.index_corrupt.path, manifest);
      sp_expect_str_eq(t, err.index_corrupt.name, spn_pkg_name_to_qualified(request));
      break;
    }
    case SPN_ERR_MANIFEST_ISSUES: {
      sp_expect_str_eq(t, err.manifest.path, manifest);
      break;
    }
    case SPN_ERR_INDEX_PATH_DEP: {
      sp_expect_str_eq(t, err.pkg.name, spn_pkg_name_to_qualified(request));
      sp_expect_str_eq_c(t, err.pkg.requested, it->expect.dep);
      break;
    }
    default: {
      break;
    }
  }

  if (pkg) {
    sp_must_eq(t, 1, sp_da_size(pkg->releases));
    spn_index_release_t* release = &pkg->releases[0];

    sp_expect(t, spn_semver_eq(it->expect.version, release->version));
    sp_expect_str_eq(t, release->id.namespace, request.namespace);
    sp_expect_str_eq(t, release->id.name, request.name);
    sp_expect_str_eq_c(t, release->paths.manifest, "spn.toml");
    sp_expect_str_eq_c(t, release->paths.script, "spn.c");

    sp_must_eq(t, SPN_PKG_ROOT_LOCAL, release->manifest.kind);
    sp_expect_str_eq(t, release->manifest.local, package);

    if (it->expect.source.url) {
      sp_must_eq(t, SPN_PKG_ROOT_GIT, release->source.kind);
      sp_expect_str_eq_c(t, release->source.git.url, it->expect.source.url);
      sp_expect_str_eq_c(t, release->source.git.rev, it->expect.source.rev);
    } else {
      sp_expect_eq(t, SPN_PKG_ROOT_NONE, release->source.kind);
    }

    u32 deps = 0;
    sp_carr_detect_len(it->expect.deps, deps, it->expect.deps[deps].name);
    sp_must_eq(t, deps, sp_da_size(release->deps));
    sp_for(at, deps) {
      sp_expect_str_eq_c(t, release->deps[at].id.namespace, it->expect.deps[at].namespace);
      sp_expect_str_eq_c(t, release->deps[at].id.name, it->expect.deps[at].name);
      sp_expect_str_eq_c(t, spn_semver_range_to_str(mem, release->deps[at].range), it->expect.deps[at].range);
      sp_expect_eq(t, it->expect.deps[at].kind, release->deps[at].kind);
      sp_expect_eq(t, it->expect.deps[at].private, release->deps[at].private);
      sp_expect_str_eq_c(t, spn_when_to_str(mem, &release->deps[at].when), it->expect.deps[at].when ? it->expect.deps[at].when : "always");
      sp_expect_str_eq_c(t, spn_when_to_str(mem, &release->deps[at].options), it->expect.deps[at].options ? it->expect.deps[at].options : "always");
    }

    u32 targets = 0;
    sp_carr_detect_len(it->expect.targets, targets, it->expect.targets[targets].name);
    sp_must_eq(t, targets, sp_da_size(release->targets));
    sp_for(at, targets) {
      spn_index_target_t* target = &release->targets[at];
      sp_expect_str_eq_c(t, target->name, it->expect.targets[at].name);

      u32 linkages = 0;
      sp_carr_detect_len(it->expect.targets[at].linkages, linkages, it->expect.targets[at].linkages[linkages]);
      sp_must_eq(t, linkages, sp_da_size(target->linkages));
      sp_for(kind, linkages) {
        sp_expect_eq(t, it->expect.targets[at].linkages[kind], target->linkages[kind]);
      }
    }

    u32 options = 0;
    sp_carr_detect_len(it->expect.options, options, it->expect.options[options].name);
    sp_must_eq(t, options, sp_str_om_size(release->options));
    sp_for(at, options) {
      spn_option_info_t* option = sp_str_om_get(release->options, sp_cstr_as_str(it->expect.options[at].name));
      sp_must(t, option != SP_NULLPTR);
      sp_expect_eq(t, it->expect.options[at].type, option->type);
      sp_expect_eq(t, it->expect.options[at].defaults, sp_da_size(option->defaults));
    }
  }

  return SP_OK;
}
