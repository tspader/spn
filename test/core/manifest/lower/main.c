#define SP_IMPLEMENTATION
#include "sp.h"

#define UTEST_IMPLEMENTATION
#include "utest.h"

#include "test.h"
#include "intern/intern.h"
#include "codegen/codegen.h"
#include "codegen/lower.h"
#include "manifest.gen.h"
#include "target/types.h"
#include "profile/types.h"
#include "index/types.h"
#include "toolchain/types.h"
#include "toml/loader.h"
#include "semver/compare.h"
#include "when/when.h"

UTEST_MAIN();

#define EXPECT_STR(actual, cstr) EXPECT_TRUE(sp_str_equal((actual), sp_str_view(cstr)))

////////////////
// DESCRIPTOR //
////////////////
typedef struct {
  const c8* value;
  const c8* when;
} gated_t;

typedef struct {
  const c8* name;
  spn_linkage_set_t linkages;
  bool no_link;
  gated_t source [4];
  const c8* headers [4];
  const c8* include [4];
  gated_t define [4];
  gated_t flags [4];
  gated_t system_deps [4];
  gated_t deps [4];
  spn_cxx_options_t cxx;
} target_t;

typedef struct {
  const c8* name;
  spn_pkg_source_t source;
  const c8* file;
  u8 build;
  u8 test;
  u8 private;
  const c8* when;
  const c8* options;
} dep_t;

typedef struct {
  const c8* when;
  const c8* value;
} option_default_t;

typedef struct {
  const c8* name;
  spn_option_type_t type;
  bool additive;
  bool public;
  const c8* define;
  const c8* values [4];
  option_default_t defaults [4];
} option_t;

typedef struct {
  const c8* name;
  bool remote;
  const c8* url;
  const c8* sha256;
  const c8* mirrors;
  const c8* compiler;
  const c8* args [8];
  const c8* linker;
  const c8* archiver;
  const c8* cxx;
  const c8* cxx_args [8];
  spn_cc_driver_t driver;
  spn_triple_t targets [4];
} toolchain_t;

typedef struct {
  const c8* name;
  const c8* toolchain;
  spn_linkage_t linkage;
  spn_c_standard_t standard;
  spn_build_mode_t mode;
  spn_opt_level_t opt;
  spn_sanitizer_set_t sanitizers;
  bool sanitizers_set;
  spn_os_t os;
  spn_arch_t arch;
  spn_abi_t abi;
  const c8* options;
} profile_t;

typedef struct {
  const c8* name;
  const c8* url;
  spn_index_protocol_t protocol;
  spn_index_kind_t kind;
} index_t;

typedef struct {
  const c8* key;
  spn_linkage_t kind;
  const c8* options;
  bool defaults_declined;
} config_t;

typedef struct {
  spn_err_t code;
  const c8* path;
} issue_t;

typedef struct {
  const c8* manifest;
  const c8* name;
  const c8* namespace;
  const c8* qualified;
  const c8* url;
  const c8* repo;
  const c8* author;
  const c8* maintainer;
  spn_semver_t version;
  const c8* commit;
  const c8* include [8];
  const c8* include_resolved;
  const c8* define [8];
  gated_t system_deps [8];
  issue_t issues [4];
  target_t libs [8];
  target_t exes [8];
  target_t scripts [8];
  target_t tests [8];
  dep_t deps [8];
  toolchain_t toolchains [4];
  profile_t profiles [4];
  index_t indexes [4];
  config_t config [8];
  option_t options [4];
} test_t;

//////////////
// EXECUTOR //
//////////////
static void check_strings(s32* utest_result, sp_da(sp_str_t) actual, const c8* const* expected, u32 cap) {
  u32 n = 0;
  for (u32 i = 0; i < cap; i++) {
    if (!expected[i]) break;
    n++;
  }
  if (!n) return;
  ASSERT_EQ(n, (u32)sp_da_size(actual));
  sp_for(i, n) EXPECT_STR(actual[i], expected[i]);
}

static void check_launcher_args(s32* utest_result, spn_toolchain_launcher_t launcher, const c8* const* expected, u32 cap) {
  u32 n = 0;
  for (u32 it = 0; it < cap; it++) {
    if (!expected[it]) break;
    n++;
  }
  if (!n) return;
  ASSERT_EQ(n, (u32)sp_da_size(launcher.args));
  sp_for(it, n) EXPECT_STR(launcher.args[it], expected[it]);
}

static void check_gated(s32* utest_result, sp_mem_t mem, spn_gated_list_t actual, const gated_t* expected, u32 cap) {
  u32 n = 0;
  for (u32 i = 0; i < cap; i++) {
    if (!expected[i].value) break;
    n++;
  }
  if (!n) return;
  ASSERT_EQ(n, (u32)sp_da_size(actual));
  sp_for(i, n) {
    EXPECT_STR(actual[i].value, expected[i].value);
    EXPECT_STR(spn_when_to_str(mem, &actual[i].when), expected[i].when ? expected[i].when : "always");
  }
}

static void check_targets(s32* utest_result, sp_mem_t mem, spn_target_map_t om, const target_t* arr, u32 n, spn_target_kind_t kind) {
  for (u32 i = 0; i < n; i++) {
    if (!arr[i].name) break;
    spn_target_info_t* t = sp_str_om_get(om, sp_str_view(arr[i].name));
    ASSERT_TRUE(t);
    EXPECT_EQ((u32)kind, (u32)t->kind);
    EXPECT_EQ(arr[i].linkages.source, t->linkages.source);
    EXPECT_EQ(arr[i].linkages.shared, t->linkages.shared);
    EXPECT_EQ(arr[i].linkages.static_lib, t->linkages.static_lib);
    EXPECT_EQ(arr[i].linkages.object, t->linkages.object);
    EXPECT_EQ(arr[i].no_link, t->no_link);
    EXPECT_EQ((u32)0, (u32)sp_da_size(t->source));
    EXPECT_EQ((u32)0, (u32)sp_da_size(t->define));
    EXPECT_EQ((u32)0, (u32)sp_da_size(t->flags));
    EXPECT_EQ((u32)0, (u32)sp_da_size(t->system_deps));
    EXPECT_EQ((u32)0, (u32)sp_da_size(t->deps));
    check_gated(utest_result, mem, t->gated.source, arr[i].source, SP_CARR_LEN(arr[i].source));
    check_strings(utest_result, t->headers, arr[i].headers, SP_CARR_LEN(arr[i].headers));
    check_strings(utest_result, t->include, arr[i].include, SP_CARR_LEN(arr[i].include));
    check_gated(utest_result, mem, t->gated.define, arr[i].define, SP_CARR_LEN(arr[i].define));
    check_gated(utest_result, mem, t->gated.flags, arr[i].flags, SP_CARR_LEN(arr[i].flags));
    check_gated(utest_result, mem, t->gated.system_deps, arr[i].system_deps, SP_CARR_LEN(arr[i].system_deps));
    check_gated(utest_result, mem, t->gated.deps, arr[i].deps, SP_CARR_LEN(arr[i].deps));
    EXPECT_EQ((u32)arr[i].cxx.standard, (u32)t->cxx.standard);
    EXPECT_EQ(arr[i].cxx.no_exceptions, t->cxx.no_exceptions);
    EXPECT_EQ(arr[i].cxx.no_rtti, t->cxx.no_rtti);
  }
}

static void run_case(s32* utest_result, test_t test) {
  sp_mem_heap_t* heap = sp_mem_heap_new();
  sp_mem_t mem = sp_mem_heap_as_allocator(heap);
  sp_intern_t* interner = sp_intern_new(mem);

  spn_toml_loader_t ctx = sp_zero;
  spn_toml_loader_init(&ctx, mem, interner);

  sp_str_t file = sp_fmt(mem, "{}.toml", sp_fmt_cstr(test.manifest)).value;
  sp_str_t path = sp_fs_join_path(mem, test_repo_path(mem, sp_str_lit(MANIFEST_DIR)), file);

  ctx.dir = test_repo_path(mem, sp_str_lit(MANIFEST_DIR));

  spn_cg_manifest_t cg = sp_zero;
  spn_codegen_load(&ctx, path, &cg);

  spn_pkg_info_t pkg = sp_zero;
  spn_pkg_lower(&ctx, &cg, &pkg);

  // Issues
  u32 num_issues = 0;
  sp_carr_for(test.issues, it) {
    if (!test.issues[it].code) break;
    num_issues++;
  }
  ASSERT_EQ(num_issues, (u32)sp_da_size(ctx.issues));
  sp_for(it, num_issues) {
    EXPECT_EQ((u32)test.issues[it].code, (u32)ctx.issues[it].code);
    if (test.issues[it].path) EXPECT_STR(ctx.issues[it].path, test.issues[it].path);
  }

  // Package scalars
  if (test.name)       EXPECT_STR(pkg.name, test.name);
  if (test.namespace)  EXPECT_STR(pkg.namespace, test.namespace);
  if (test.qualified)  EXPECT_STR(pkg.qualified, test.qualified);
  if (test.url)        EXPECT_STR(pkg.upstream.url, test.url);
  if (test.repo)       EXPECT_STR(pkg.repo, test.repo);
  if (test.author)     EXPECT_STR(pkg.author, test.author);
  if (test.maintainer) EXPECT_STR(pkg.maintainer, test.maintainer);

  if (!spn_semver_is_empty(test.version)) {
    EXPECT_EQ(test.version.major, pkg.version.major);
    EXPECT_EQ(test.version.minor, pkg.version.minor);
    EXPECT_EQ(test.version.patch, pkg.version.patch);
  }

  if (test.commit) {
    EXPECT_STR(pkg.upstream.commit, test.commit);
  }

  // Package arrays
  check_strings(utest_result, pkg.include, test.include, SP_CARR_LEN(test.include));
  check_strings(utest_result, pkg.define,  test.define,  SP_CARR_LEN(test.define));
  check_gated(utest_result, mem, pkg.gated.system_deps, test.system_deps, SP_CARR_LEN(test.system_deps));

  if (test.include_resolved) {
    ASSERT_EQ((u32)1, (u32)sp_da_size(pkg.include));
    EXPECT_TRUE(sp_str_ends_with(pkg.include[0], sp_str_view(test.include_resolved)));
  }

  // Targets
  check_targets(utest_result, mem, pkg.libs,    test.libs,    SP_CARR_LEN(test.libs),    SPN_TARGET_LIB);
  check_targets(utest_result, mem, pkg.exes,    test.exes,    SP_CARR_LEN(test.exes),    SPN_TARGET_EXE);
  check_targets(utest_result, mem, pkg.scripts, test.scripts, SP_CARR_LEN(test.scripts), SPN_TARGET_SCRIPT);
  check_targets(utest_result, mem, pkg.tests,   test.tests,   SP_CARR_LEN(test.tests),   SPN_TARGET_TEST);

  // Deps
  sp_carr_for(test.deps, it) {
    dep_t expected = test.deps[it];
    if (!expected.name) break;

    spn_dep_kind_t kind = expected.build ? SPN_DEP_KIND_BUILD : expected.test ? SPN_DEP_KIND_TEST : SPN_DEP_KIND_PACKAGE;

    spn_requested_dep_t* req = SP_NULLPTR;
    sp_da_for(pkg.deps, jt) {
      if (sp_str_equal(pkg.deps[jt].qualified, sp_str_view(expected.name)) && pkg.deps[jt].kind == kind) {
        req = &pkg.deps[jt];
        break;
      }
    }
    ASSERT_TRUE(req);
    EXPECT_EQ((u32)expected.source, (u32)req->source);
    EXPECT_EQ(expected.private != 0, req->private);

    if (expected.file) EXPECT_TRUE(sp_str_ends_with(req->file.path, sp_str_view(expected.file)));
    if (expected.when) EXPECT_STR(spn_when_to_str(mem, &req->when), expected.when);
    if (expected.options) EXPECT_STR(spn_when_to_str(mem, &req->options), expected.options);
  }

  // Options
  sp_carr_for(test.options, it) {
    option_t expected = test.options[it];
    if (!expected.name) break;

    spn_option_info_t** slot = sp_str_om_getp(pkg.options, sp_str_view(expected.name));
    ASSERT_TRUE(slot);
    spn_option_info_t* option = *slot;
    EXPECT_STR(option->name, expected.name);
    EXPECT_EQ((u32)expected.type, (u32)option->type);
    EXPECT_EQ(expected.additive, option->additive);
    EXPECT_EQ(expected.public, option->public);
    if (expected.define) EXPECT_STR(option->define, expected.define);
    check_strings(utest_result, option->values, expected.values, SP_CARR_LEN(expected.values));

    u32 num_defaults = 0;
    sp_carr_for(expected.defaults, jt) {
      if (!expected.defaults[jt].value) break;
      num_defaults++;
    }
    ASSERT_EQ(num_defaults, (u32)sp_da_size(option->defaults));
    sp_for(jt, num_defaults) {
      EXPECT_STR(spn_when_to_str(mem, &option->defaults[jt].when), expected.defaults[jt].when ? expected.defaults[jt].when : "always");
      EXPECT_STR(spn_option_value_to_str(mem, option->defaults[jt].value), expected.defaults[jt].value);
    }
  }

  // Toolchains
  sp_carr_for(test.toolchains, it) {
    toolchain_t expected = test.toolchains[it];
    if (!expected.name) break;

    spn_toolchain_info_t* tc = sp_str_om_get(pkg.toolchains, sp_str_view(expected.name));
    ASSERT_TRUE(tc);
    EXPECT_EQ(expected.remote, !sp_opt_is_null(tc->artifact));

    if (expected.url)     EXPECT_STR(sp_opt_get(tc->artifact).url, expected.url);
    if (expected.sha256)  EXPECT_STR(sp_opt_get(tc->artifact).sha256, expected.sha256);
    if (expected.mirrors) EXPECT_STR(sp_opt_get(tc->artifact).mirror_list, expected.mirrors);
    if (expected.compiler) EXPECT_STR(tc->compiler.program, expected.compiler);
    if (expected.linker)   EXPECT_STR(tc->linker.program, expected.linker);
    if (expected.archiver) EXPECT_STR(tc->archiver.program, expected.archiver);
    if (expected.cxx)      EXPECT_STR(tc->cxx.program, expected.cxx);
    if (expected.driver)   EXPECT_EQ((u32)expected.driver, (u32)tc->driver);

    check_launcher_args(utest_result, tc->compiler, expected.args, SP_CARR_LEN(expected.args));
    check_launcher_args(utest_result, tc->cxx, expected.cxx_args, SP_CARR_LEN(expected.cxx_args));

    sp_carr_for(expected.targets, t) {
      spn_triple_t triple = expected.targets[t];
      if (triple.arch == SPN_ARCH_NONE) break;
      ASSERT_TRUE(t < sp_da_size(tc->targets));
      EXPECT_EQ((u32)triple.arch, (u32)tc->targets[t].arch);
      EXPECT_EQ((u32)triple.os, (u32)tc->targets[t].os);
      EXPECT_EQ((u32)triple.abi, (u32)tc->targets[t].abi);
    }
  }

  // Profiles
  sp_carr_for(test.profiles, it) {
    profile_t expected = test.profiles[it];
    if (!expected.name) break;

    spn_profile_info_t* p = sp_str_om_get(pkg.profiles, sp_str_view(expected.name));
    ASSERT_TRUE(p);
    EXPECT_STR(p->name, expected.name);
    if (expected.toolchain) EXPECT_STR(p->toolchain, expected.toolchain);
    EXPECT_EQ((u32)expected.linkage, (u32)p->linkage);
    if (expected.standard) EXPECT_EQ((u32)expected.standard, (u32)p->standard);
    if (expected.mode) EXPECT_EQ((u32)expected.mode, (u32)p->mode);
    if (expected.opt) EXPECT_EQ((u32)expected.opt, (u32)p->opt);
    EXPECT_EQ(expected.sanitizers, p->sanitizers);
    EXPECT_EQ(expected.sanitizers_set, p->sanitizers_set);
    EXPECT_EQ((u32)expected.os, (u32)p->os);
    EXPECT_EQ((u32)expected.arch, (u32)p->arch);
    if (expected.abi) EXPECT_EQ((u32)expected.abi, (u32)p->abi);
    if (expected.options) EXPECT_STR(spn_when_to_str(mem, &p->options), expected.options);
  }

  // Indexes
  sp_carr_for(test.indexes, it) {
    index_t expected = test.indexes[it];
    if (!expected.name) break;

    spn_index_info_t* idx = sp_str_om_get(pkg.indexes, sp_str_view(expected.name));
    ASSERT_TRUE(idx);
    if (expected.url) EXPECT_STR(idx->url, expected.url);
    if (expected.protocol) EXPECT_EQ((u32)expected.protocol, (u32)idx->protocol);
    EXPECT_EQ((u32)expected.kind, (u32)idx->kind);
  }

  // Config
  sp_carr_for(test.config, it) {
    config_t expected = test.config[it];
    if (!expected.key) break;

    spn_pkg_config_entry_t* entry = NULL;
    sp_da_for(pkg.config, j) {
      if (sp_str_equal(pkg.config[j].key, sp_str_view(expected.key))) {
        entry = &pkg.config[j];
        break;
      }
    }
    ASSERT_TRUE(entry);
    if (expected.kind) {
      ASSERT_FALSE(sp_opt_is_null(entry->value.kind));
      EXPECT_EQ((u32)expected.kind, (u32)sp_opt_get(entry->value.kind));
    }
    if (expected.options) EXPECT_STR(spn_when_to_str(mem, &entry->value.options), expected.options);
    EXPECT_EQ(expected.defaults_declined, entry->value.defaults_declined);
  }
}

///////////
// CASES //
///////////
UTEST(lower, lib_shared) {
  run_case(utest_result, (test_t) {
    .manifest = "shared_lib",
    .libs = {
      { .name = "t", .linkages = { .shared = true } }
    }
  });
}

UTEST(lower, lib_object) {
  run_case(utest_result, (test_t) {
    .manifest = "object_lib",
    .libs = {
      { .name = "t", .linkages = { .object = true } }
    }
  });
}

UTEST(lower, lib_source_static) {
  run_case(utest_result, (test_t) {
    .manifest = "source_static",
    .libs = {
      { .name = "t", .linkages = { .source = true, .static_lib = true } }
    }
  });
}

UTEST(lower, lib_no_link) {
  run_case(utest_result, (test_t) {
    .manifest = "no_link_lib",
    .libs = {
      { .name = "t", .linkages = { .static_lib = true }, .no_link = true }
    }
  });
}

UTEST(lower, lib_link_true) {
  run_case(utest_result, (test_t) {
    .manifest = "link_true_lib",
    .libs = {
      { .name = "t", .linkages = { .static_lib = true }, .no_link = false }
    }
  });
}

UTEST(lower, lib_all_fields) {
  run_case(utest_result, (test_t) {
    .manifest = "lib_all_fields",
    .libs = {
      {
        .name = "t",
        .linkages = { .static_lib = true },
        .source = { { "main.c" } },
        .headers = { "header.h" },
        .include = { "include/dir" },
        .define = { { "SPUM" } },
        .flags = { { "-flag" } },
        .deps = { { "spum" } },
      }
    }
  });
}

UTEST(lower, exe) {
  run_case(utest_result, (test_t) {
    .manifest = "exe",
    .exes = {
      { .name = "t" }
    }
  });
}

UTEST(lower, script) {
  run_case(utest_result, (test_t) {
    .manifest = "script",
    .scripts = {
      { .name = "t" }
    }
  });
}

UTEST(lower, test) {
  run_case(utest_result, (test_t) {
    .manifest = "target_test",
    .tests = {
      { .name = "t" }
    }
  });
}

UTEST(lower, dep_index) {
  run_case(utest_result, (test_t) {
    .manifest = "dep_index",
    .deps = {
      { .name = "core/foo", .source = SPN_PKG_SOURCE_INDEX }
    }
  });
}

UTEST(lower, dep_file) {
  run_case(utest_result, (test_t) {
    .manifest = "dep_file",
    .deps = {
      { .name = "core/foo", .source = SPN_PKG_SOURCE_FILE, .file = "/abs/foo/spn.toml" }
    }
  });
}

UTEST(lower, dep_file_relative) {
  run_case(utest_result, (test_t) {
    .manifest = "dep_file_relative",
    .deps = {
      { .name = "core/foo", .source = SPN_PKG_SOURCE_FILE, .file = "vendor/foo/spn.toml" }
    }
  });
}

UTEST(lower, dep_test) {
  run_case(utest_result, (test_t) {
    .manifest = "dep_test",
    .deps = {
      { .name = "core/bar", .source = SPN_PKG_SOURCE_INDEX, .test = 1 }
    }
  });
}

UTEST(lower, dep_build) {
  run_case(utest_result, (test_t) {
    .manifest = "dep_build",
    .deps = {
      { .name = "core/baz", .source = SPN_PKG_SOURCE_INDEX, .build = 1 }
    }
  });
}

UTEST(lower, dep_namespaced) {
  run_case(utest_result, (test_t) {
    .manifest = "dep_namespaced",
    .deps = {
      { .name = "ns/q", .source = SPN_PKG_SOURCE_INDEX }
    }
  });
}

UTEST(lower, dep_private) {
  run_case(utest_result, (test_t) {
    .manifest = "dep_private",
    .deps = {
      { .name = "core/foo", .source = SPN_PKG_SOURCE_INDEX, .private = 1 }
    }
  });
}

// The same name under two kinds must survive as two requests, not collapse
UTEST(lower, dep_dual_kind) {
  run_case(utest_result, (test_t) {
    .manifest = "dep_dual_kind",
    .deps = {
      { .name = "core/foo", .source = SPN_PKG_SOURCE_INDEX },
      { .name = "core/foo", .source = SPN_PKG_SOURCE_INDEX, .build = 1 }
    }
  });
}

UTEST(lower, cxx_lib) {
  run_case(utest_result, (test_t) {
    .manifest = "cxx_lib",
    .libs = {
      {
        .name = "t",
        .linkages = { .static_lib = true },
        .source = { { "spum.cpp" } },
        .cxx = { .standard = SPN_CXX14, .no_exceptions = true, .no_rtti = true },
      }
    }
  });
}

UTEST(lower, cxx_lib_defaults) {
  run_case(utest_result, (test_t) {
    .manifest = "cxx_lib_defaults",
    .libs = {
      {
        .name = "t",
        .linkages = { .static_lib = true },
        .source = { { "spum.cpp" } },
      }
    }
  });
}

UTEST(lower, validate_cxx_bad_standard) {
  run_case(utest_result, (test_t) {
    .manifest = "validate_cxx_bad_standard",
    .issues = {
      { SPN_CODEGEN_ERR_INVALID, "lib[0].cxx.standard" }
    }
  });
}

UTEST(lower, validate_cxx_source_on_build_script) {
  run_case(utest_result, (test_t) {
    .manifest = "validate_cxx_source_on_build_script",
    .issues = {
      { SPN_CODEGEN_ERR_INVALID, "package.build.source" }
    }
  });
}

UTEST(lower, toolchain_cxx) {
  run_case(utest_result, (test_t) {
    .manifest = "toolchain_cxx",
    .toolchains = {
      {
        .name = "custom",
        .compiler = "cc",
        .linker = "cc",
        .archiver = "ar",
        .cxx = "g++",
        .cxx_args = { "-pthread" },
        .driver = SPN_CC_DRIVER_GCC,
      },
    },
  });
}

UTEST(lower, validate_object_mixed) {
  run_case(utest_result, (test_t) {
    .manifest = "validate_object_mixed",
    .issues = {
      { SPN_CODEGEN_ERR_INVALID, "lib[0].kinds" }
    }
  });
}

UTEST(lower, validate_link_on_bin) {
  run_case(utest_result, (test_t) {
    .manifest = "validate_link_on_bin",
    .issues = {
      { SPN_CODEGEN_ERR_INVALID, "bin[0].link" }
    }
  });
}

UTEST(lower, validate_link_on_script) {
  run_case(utest_result, (test_t) {
    .manifest = "validate_link_on_script",
    .issues = {
      { SPN_CODEGEN_ERR_INVALID, "script[0].link" }
    }
  });
}

UTEST(lower, validate_link_on_test) {
  run_case(utest_result, (test_t) {
    .manifest = "validate_link_on_test",
    .issues = {
      { SPN_CODEGEN_ERR_INVALID, "test[0].link" }
    }
  });
}

UTEST(lower, validate_toolchain_incomplete) {
  run_case(utest_result, (test_t) {
    .manifest = "toolchain_incomplete",
    .issues = {
      { SPN_CODEGEN_ERR_MISSING_KEY, "toolchain[0].compiler" }
    }
  });
}

UTEST(lower, validate_toolchain_url_without_sha) {
  run_case(utest_result, (test_t) {
    .manifest = "toolchain_no_sha",
    .issues = {
      { SPN_CODEGEN_ERR_MISSING_KEY, "toolchain[0].sha256" }
    }
  });
}

UTEST(lower, validate_duplicate_name) {
  run_case(utest_result, (test_t) {
    .manifest = "validate_duplicate_name",
    .issues = {
      { SPN_CODEGEN_ERR_DUPLICATE_KEY, "bin[0]" }
    }
  });
}

UTEST(lower, validate_upstream_url_only) {
  run_case(utest_result, (test_t) {
    .manifest = "validate_upstream_url_only",
    .issues = {
      { SPN_CODEGEN_ERR_MISSING_KEY, "package.commit" }
    }
  });
}

UTEST(lower, validate_upstream_commit_only) {
  run_case(utest_result, (test_t) {
    .manifest = "validate_upstream_commit_only",
    .issues = {
      { SPN_CODEGEN_ERR_MISSING_KEY, "package.url" }
    }
  });
}

UTEST(lower, package) {
  run_case(utest_result, (test_t) {
    .manifest = "package",
    .name = "p",
    .namespace = "ns",
    .qualified = "ns/p",
    .url = "u",
    .repo = "r",
    .author = "a",
    .maintainer = "m",
    .version = spn_semver_lit(1, 2, 3),
    .commit = "abc",
    .define = { "SPUM" },
    .system_deps = { { "z" } },
  });
}

UTEST(lower, package_include_resolved) {
  run_case(utest_result, (test_t) {
    .manifest = "package",
    .include_resolved = "/inc",
  });
}

UTEST(lower, qualified_default_namespace) {
  run_case(utest_result, (test_t) {
    .manifest = "qualified_default_namespace",
    .qualified = "core/p",
  });
}

UTEST(lower, toolchain_inline) {
  run_case(utest_result, (test_t) {
    .manifest = "toolchain_inline",
    .toolchains = {
      {
        .name = "zig",
        .remote = true,
        .url = "https://tc",
        .sha256 = "deadbeef",
        .mirrors = "https://mirrors",
        .compiler = "zig",
        .args = { "cc", "-target", "x86_64-linux-gnu" },
        .linker = "zig",
        .archiver = "ar",
        .driver = SPN_CC_DRIVER_CLANG,
        .targets = { { SPN_ARCH_ARM64, SPN_OS_MACOS } },
      },
    },
  });
}

UTEST(lower, profile) {
  run_case(utest_result, (test_t) {
    .manifest = "profiles",
    .profiles = {
      {
        .name = "release",
        .toolchain = "zig",
        .linkage = SPN_LIB_KIND_SHARED,
        .standard = SPN_C99,
        .mode = SPN_BUILD_MODE_RELEASE,
        .opt = SPN_OPT_LEVEL_3,
        .sanitizers = SPN_SANITIZER_ADDRESS | SPN_SANITIZER_UNDEFINED,
        .sanitizers_set = true,
        .os = SPN_OS_LINUX,
        .arch = SPN_ARCH_X64,
        .abi = SPN_ABI_GNU,
      },
      {
        .name = "clean",
        .sanitizers_set = true,
      },
    },
  });
}

UTEST(lower, index) {
  run_case(utest_result, (test_t) {
    .manifest = "indexes",
    .indexes = {
      {
        .name = "main",
        .url = "https://x",
        .protocol = SPN_INDEX_PROTOCOL_HTTP,
        .kind = SPN_INDEX_WORKSPACE,
      },
    },
  });
}

UTEST(lower, config) {
  run_case(utest_result, (test_t) {
    .manifest = "config",
    .config = {
      { .key = "core/foo", .kind = SPN_LIB_KIND_STATIC }
    },
  });
}

UTEST(lower, when_dep) {
  run_case(utest_result, (test_t) {
    .manifest = "when_dep",
    .deps = {
      { .name = "core/openssl", .source = SPN_PKG_SOURCE_INDEX, .when = "tls = \"openssl\"" },
      { .name = "core/zstd", .source = SPN_PKG_SOURCE_INDEX, .when = "zstd = true, os != \"wasi\"" },
    },
  });
}

UTEST(lower, when_new_facts) {
  run_case(utest_result, (test_t) {
    .manifest = "when_new_facts",
    .deps = {
      { .name = "core/fast", .source = SPN_PKG_SOURCE_INDEX, .when = "opt = \"3\"" },
      { .name = "core/asan", .source = SPN_PKG_SOURCE_INDEX, .when = "sanitize_address = true" },
    },
  });
}

UTEST(lower, validate_when_bad_new_facts) {
  run_case(utest_result, (test_t) {
    .manifest = "validate_when_bad_new_facts",
    .issues = {
      { SPN_CODEGEN_ERR_INVALID, "deps.package[0].when.opt" },
      { SPN_CODEGEN_ERR_INVALID, "deps.package[1].when.sanitize_address" },
    },
  });
}

UTEST(lower, validate_profile_bad_opt) {
  run_case(utest_result, (test_t) {
    .manifest = "validate_profile_bad_opt",
    .issues = {
      { SPN_CODEGEN_ERR_INVALID, "profile[0].opt" },
    },
  });
}

UTEST(lower, validate_profile_sanitize_conflict) {
  run_case(utest_result, (test_t) {
    .manifest = "validate_profile_sanitize_conflict",
    .issues = {
      { SPN_CODEGEN_ERR_INVALID, "profile[0].sanitize" },
    },
  });
}

UTEST(lower, validate_profile_bad_sanitizer) {
  run_case(utest_result, (test_t) {
    .manifest = "validate_profile_bad_sanitizer",
    .issues = {
      { SPN_CODEGEN_ERR_INVALID, "profile.dev.sanitize[0]" },
    },
  });
}

UTEST(lower, options) {
  run_case(utest_result, (test_t) {
    .manifest = "options",
    .options = {
      {
        .name = "tls",
        .type = SPN_OPTION_TYPE_ENUM,
        .values = { "schannel", "openssl", "off" },
        .defaults = {
          { .when = "os = \"windows\"", .value = "\"schannel\"" },
          { .when = "os != \"wasi\"", .value = "\"openssl\"" },
          { .value = "\"off\"" },
        },
      },
      {
        .name = "zstd",
        .type = SPN_OPTION_TYPE_BOOL,
        .defaults = {
          { .value = "false" },
        },
      },
    },
  });
}

UTEST(lower, target_when_gated) {
  run_case(utest_result, (test_t) {
    .manifest = "target_when",
    .libs = {
      {
        .name = "t",
        .linkages = { .static_lib = true },
        .source = { { "a.c" }, { "b.c", "os = \"linux\"" } },
        .define = { { "X" }, { "Y", "os = \"windows\"" } },
        .flags = { { "-g", "mode = \"debug\"" } },
        .system_deps = { { "ws2_32", "os = \"windows\"" } },
      },
    },
  });
}

UTEST(lower, option_extras) {
  run_case(utest_result, (test_t) {
    .manifest = "option_extras",
    .options = {
      {
        .name = "audio",
        .type = SPN_OPTION_TYPE_BOOL,
        .additive = true,
        .public = true,
        .define = "HAS_AUDIO",
        .defaults = { { .value = "false" } },
      },
    },
    .deps = {
      { .name = "core/codec", .source = SPN_PKG_SOURCE_INDEX, .options = "audio = true, backend != \"vk\"" },
    },
    .profiles = {
      { .name = "full", .options = "audio = true" },
    },
    .config = {
      { .key = "codec", .options = "audio = true", .defaults_declined = true },
    },
  });
}

UTEST(lower, package_system_deps_gated) {
  run_case(utest_result, (test_t) {
    .manifest = "package_system_deps_gated",
    .system_deps = { { "m" }, { "ws2_32", "os = \"windows\"" } },
  });
}

UTEST(lower, validate_option_define_on_enum) {
  run_case(utest_result, (test_t) {
    .manifest = "validate_option_define_on_enum",
    .issues = {
      { SPN_CODEGEN_ERR_INVALID, "options[0].define" }
    },
  });
}

UTEST(lower, validate_config_negated_option) {
  run_case(utest_result, (test_t) {
    .manifest = "validate_config_negated_option",
    .issues = {
      { SPN_CODEGEN_ERR_INVALID, "config[0].options.ssl" }
    },
  });
}

UTEST(lower, validate_profile_unknown_option) {
  run_case(utest_result, (test_t) {
    .manifest = "validate_profile_unknown_option",
    .issues = {
      { SPN_CODEGEN_ERR_INVALID, "profile[0].options.nosuch" }
    },
  });
}

UTEST(lower, validate_when_unknown_key) {
  run_case(utest_result, (test_t) {
    .manifest = "validate_when_unknown_key",
    .issues = {
      { SPN_CODEGEN_ERR_INVALID, "deps.package[0].when.simd" }
    },
  });
}

UTEST(lower, validate_when_bad_fact) {
  run_case(utest_result, (test_t) {
    .manifest = "validate_when_bad_fact",
    .issues = {
      { SPN_CODEGEN_ERR_INVALID, "lib[0].source[0].when.os" },
      { SPN_CODEGEN_ERR_INVALID, "lib[0].source[1].when.mode" },
      { SPN_CODEGEN_ERR_INVALID, "lib[0].source[2].when.abi" }
    },
  });
}

UTEST(lower, validate_when_kind_mismatch) {
  run_case(utest_result, (test_t) {
    .manifest = "validate_when_kind_mismatch",
    .issues = {
      { SPN_CODEGEN_ERR_INVALID, "deps.package[0].when.zstd" }
    },
  });
}

UTEST(lower, validate_when_bad_option_value) {
  run_case(utest_result, (test_t) {
    .manifest = "validate_when_bad_option_value",
    .issues = {
      { SPN_CODEGEN_ERR_INVALID, "deps.package[0].when.tls" }
    },
  });
}

UTEST(lower, validate_when_in_default) {
  run_case(utest_result, (test_t) {
    .manifest = "validate_when_in_default",
    .issues = {
      { SPN_CODEGEN_ERR_INVALID, "options[0].default[0].when.simd" }
    },
  });
}

UTEST(lower, validate_option_bad_type) {
  run_case(utest_result, (test_t) {
    .manifest = "validate_option_bad_type",
    .issues = {
      { SPN_CODEGEN_ERR_INVALID, "options[0].type" }
    },
  });
}

UTEST(lower, validate_option_enum_no_values) {
  run_case(utest_result, (test_t) {
    .manifest = "validate_option_enum_no_values",
    .issues = {
      { SPN_CODEGEN_ERR_MISSING_KEY, "options[0].values" }
    },
  });
}

UTEST(lower, validate_option_default_kind) {
  run_case(utest_result, (test_t) {
    .manifest = "validate_option_default_kind",
    .issues = {
      { SPN_CODEGEN_ERR_INVALID, "options[0].default[0].value" }
    },
  });
}

UTEST(lower, validate_option_default_not_in_values) {
  run_case(utest_result, (test_t) {
    .manifest = "validate_option_default_not_in_values",
    .issues = {
      { SPN_CODEGEN_ERR_INVALID, "options[0].default[0].value" }
    },
  });
}

UTEST(lower, validate_option_default_bad_index) {
  run_case(utest_result, (test_t) {
    .manifest = "validate_option_default_bad_index",
    .issues = {
      { SPN_CODEGEN_ERR_INVALID, "options[0].default[1].value" }
    },
  });
}

UTEST(lower, validate_option_shadows_fact) {
  run_case(utest_result, (test_t) {
    .manifest = "validate_option_shadows_fact",
    .issues = {
      { SPN_CODEGEN_ERR_DUPLICATE_KEY, "options[0]" }
    },
  });
}

UTEST(lower, validate_option_bool_values) {
  run_case(utest_result, (test_t) {
    .manifest = "validate_option_bool_values",
    .issues = {
      { SPN_CODEGEN_ERR_INVALID, "options[0].values" }
    },
  });
}

UTEST(lower, validate_option_default_unknown_key) {
  run_case(utest_result, (test_t) {
    .manifest = "validate_option_default_unknown_key",
    .issues = {
      { SPN_CODEGEN_ERR_INVALID, "options.tls.default[0].whne" }
    },
  });
}
