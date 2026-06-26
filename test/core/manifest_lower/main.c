#include "intern/intern.h"
#define SP_IMPLEMENTATION
#include "sp.h"

#define UTEST_IMPLEMENTATION
#include "utest.h"

#include "codegen/codegen.h"
#include "codegen/lower.h"
#include "manifest.gen.h"
#include "target/types.h"
#include "profile/types.h"
#include "index/types.h"
#include "toolchain/types.h"
#include "semver/compare.h"

UTEST_MAIN();

#define EXPECT_STR(actual, cstr) EXPECT_TRUE(sp_str_equal((actual), sp_str_view(cstr)))

////////////////
// DESCRIPTOR //
////////////////
typedef struct {
  const c8* name;
  spn_linkage_set_t linkages;
  bool no_link;
  const c8* source [4];
  const c8* headers [4];
  const c8* include [4];
  const c8* define [4];
  const c8* flags [4];
  const c8* deps [4];
} target_t;

typedef struct {
  const c8* name;
  spn_pkg_source_t source;
  const c8* file;
  u8 build;
  u8 test;
} dep_t;

typedef struct {
  const c8* name;
  spn_toolchain_kind_t kind;
  const c8* url;
  const c8* sysroot;
  const c8* compiler;
  const c8* args [8];
  const c8* linker;
  const c8* archiver;
  spn_cc_driver_t driver;
  bool export;
  const c8* package;
  spn_triple_t hosts [4];
  spn_triple_t targets [4];
} toolchain_t;

typedef struct {
  const c8* name;
  const c8* toolchain;
  spn_linkage_t linkage;
  spn_c_standard_t standard;
  spn_build_mode_t mode;
  spn_os_t os;
  spn_arch_t arch;
  spn_abi_t abi;
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
} config_t;

typedef struct {
  spn_codegen_err_t code;
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
  const c8* system_deps [8];
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

static void check_targets(s32* utest_result, spn_target_info_om_t om, const target_t* arr, u32 n, spn_target_kind_t kind) {
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
    check_strings(utest_result, t->source,  arr[i].source,  SP_CARR_LEN(arr[i].source));
    check_strings(utest_result, t->headers, arr[i].headers, SP_CARR_LEN(arr[i].headers));
    check_strings(utest_result, t->include, arr[i].include, SP_CARR_LEN(arr[i].include));
    check_strings(utest_result, t->define,  arr[i].define,  SP_CARR_LEN(arr[i].define));
    check_strings(utest_result, t->flags,   arr[i].flags,   SP_CARR_LEN(arr[i].flags));
    check_strings(utest_result, t->deps,    arr[i].deps,    SP_CARR_LEN(arr[i].deps));
  }
}

static void run_case(s32* utest_result, test_t test) {
  sp_mem_heap_t* heap = sp_mem_heap_new();
  sp_mem_t mem = sp_mem_heap_as_allocator(heap);
  sp_mem_t bulk = sp_mem_heap_as_allocator(heap);
  sp_intern_t* interner = sp_intern_new(mem);

  spn_codegen_ctx_t ctx = sp_zero;
  spn_codegen_ctx_init(&ctx, mem, bulk, interner);

  sp_str_t file = sp_fmt(mem, "{}.toml", sp_fmt_cstr(test.manifest)).value;
  sp_str_t path = sp_fs_join_path(mem, sp_cstr_as_str(MANIFEST_DIR), file);

  ctx.dir = sp_cstr_as_str(MANIFEST_DIR);

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
  if (test.url)        EXPECT_STR(pkg.url, test.url);
  if (test.repo)       EXPECT_STR(pkg.repo, test.repo);
  if (test.author)     EXPECT_STR(pkg.author, test.author);
  if (test.maintainer) EXPECT_STR(pkg.maintainer, test.maintainer);

  if (!spn_semver_is_empty(test.version)) {
    EXPECT_EQ(test.version.major, pkg.version.major);
    EXPECT_EQ(test.version.minor, pkg.version.minor);
    EXPECT_EQ(test.version.patch, pkg.version.patch);

    ASSERT_EQ((u32)1, (u32)sp_da_size(pkg.versions));
    EXPECT_EQ(test.version.major, pkg.versions[0].major);
  }

  if (test.commit) {
    spn_pkg_metadata_t* meta = sp_ht_getp(pkg.metadata, pkg.version);
    ASSERT_TRUE(meta);
    EXPECT_STR(meta->commit, test.commit);
  }

  // Package arrays
  check_strings(utest_result, pkg.include,     test.include,     SP_CARR_LEN(test.include));
  check_strings(utest_result, pkg.define,      test.define,      SP_CARR_LEN(test.define));
  check_strings(utest_result, pkg.system_deps, test.system_deps, SP_CARR_LEN(test.system_deps));

  if (test.include_resolved) {
    ASSERT_EQ((u32)1, (u32)sp_da_size(pkg.include));
    EXPECT_TRUE(sp_str_ends_with(pkg.include[0], sp_str_view(test.include_resolved)));
  }

  // Targets
  check_targets(utest_result, pkg.libs,    test.libs,    SP_CARR_LEN(test.libs),    SPN_TARGET_LIB);
  check_targets(utest_result, pkg.exes,    test.exes,    SP_CARR_LEN(test.exes),    SPN_TARGET_EXE);
  check_targets(utest_result, pkg.scripts, test.scripts, SP_CARR_LEN(test.scripts), SPN_TARGET_SCRIPT);
  check_targets(utest_result, pkg.tests,   test.tests,   SP_CARR_LEN(test.tests),   SPN_TARGET_TEST);

  // Deps
  sp_carr_for(test.deps, it) {
    dep_t expected = test.deps[it];
    if (!expected.name) break;

    spn_requested_pkg_t* req = sp_ht_getp(pkg.deps, sp_str_view(expected.name));
    ASSERT_TRUE(req);
    EXPECT_EQ((u32)expected.source, (u32)req->source);

    spn_dep_kind_t kind = expected.build ? SPN_DEP_KIND_BUILD : expected.test ? SPN_DEP_KIND_TEST : SPN_DEP_KIND_PACKAGE;
    EXPECT_EQ((u32)kind, (u32)req->kind);

    if (expected.file) EXPECT_TRUE(sp_str_ends_with(req->file.path, sp_str_view(expected.file)));
  }

  // Toolchains
  sp_carr_for(test.toolchains, it) {
    toolchain_t expected = test.toolchains[it];
    if (!expected.name) break;

    spn_toolchain_entry_t* tc = sp_str_om_get(pkg.toolchains, sp_str_view(expected.name));
    ASSERT_TRUE(tc);
    EXPECT_EQ((u32)expected.kind, (u32)tc->kind);

    if (expected.kind == SPN_TOOLCHAIN_INDEX) {
      if (expected.package) EXPECT_STR(tc->request.package, expected.package);
      continue;
    }

    if (expected.url)      EXPECT_STR(tc->info.url, expected.url);
    if (expected.sysroot)  EXPECT_STR(tc->info.sysroot, expected.sysroot);
    if (expected.compiler) EXPECT_STR(tc->info.compiler.program, expected.compiler);
    if (expected.linker)   EXPECT_STR(tc->info.linker.program, expected.linker);
    if (expected.archiver) EXPECT_STR(tc->info.archiver.program, expected.archiver);
    if (expected.driver)   EXPECT_EQ((u32)expected.driver, (u32)tc->info.driver);
    EXPECT_EQ(expected.export, tc->info.export);

    u32 num_args = 0;
    sp_carr_for(expected.args, a) {
      if (!expected.args[a]) break;
      num_args++;
    }
    if (num_args) {
      ASSERT_EQ(num_args, (u32)sp_da_size(tc->info.compiler.args));
      sp_for(a, num_args) EXPECT_STR(tc->info.compiler.args[a], expected.args[a]);
    }

    sp_carr_for(expected.hosts, h) {
      spn_triple_t triple = expected.hosts[h];
      if (triple.arch == SPN_ARCH_NONE) break;
      EXPECT_EQ((u32)triple.arch, (u32)tc->info.hosts[h].arch);
      EXPECT_EQ((u32)triple.os, (u32)tc->info.hosts[h].os);
      EXPECT_EQ((u32)triple.abi, (u32)tc->info.hosts[h].abi);
    }

    sp_carr_for(expected.targets, t) {
      spn_triple_t triple = expected.targets[t];
      if (triple.arch == SPN_ARCH_NONE) break;
      EXPECT_EQ((u32)triple.arch, (u32)tc->info.targets[t].arch);
      EXPECT_EQ((u32)triple.os, (u32)tc->info.targets[t].os);
      EXPECT_EQ((u32)triple.abi, (u32)tc->info.targets[t].abi);
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
    EXPECT_EQ((u32)expected.os, (u32)p->os);
    EXPECT_EQ((u32)expected.arch, (u32)p->arch);
    if (expected.abi) EXPECT_EQ((u32)expected.abi, (u32)p->abi);
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
        .source = { "main.c" },
        .headers = { "header.h" },
        .include = { "include/dir" },
        .define = { "SPUM" },
        .flags = { "-flag" },
        .deps = { "spum" },
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
      { .name = "core/foo", .source = SPN_PKG_SOURCE_FILE, .file = "/abs/foo" }
    }
  });
}

UTEST(lower, dep_file_relative) {
  run_case(utest_result, (test_t) {
    .manifest = "dep_file_relative",
    .deps = {
      { .name = "core/foo", .source = SPN_PKG_SOURCE_FILE, .file = "vendor/foo" }
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

UTEST(lower, validate_toolchain_no_host) {
  run_case(utest_result, (test_t) {
    .manifest = "toolchain_no_host",
    .issues = {
      { SPN_CODEGEN_ERR_MISSING_KEY, "toolchain[0].host" },
      { SPN_CODEGEN_ERR_MISSING_KEY, "toolchain[0].target" }
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
    .system_deps = { "z" },
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
        .kind = SPN_TOOLCHAIN_INLINE,
        .url = "https://tc",
        .sysroot = "/sys",
        .compiler = "zig",
        .args = { "cc", "-target", "x86_64-linux-gnu" },
        .linker = "zig",
        .archiver = "ar",
        .driver = SPN_CC_DRIVER_CLANG,
        .export = true,
        .hosts = { { SPN_ARCH_X64, SPN_OS_LINUX, SPN_ABI_GNU } },
        .targets = { { SPN_ARCH_ARM64, SPN_OS_MACOS } },
      },
    },
  });
}

UTEST(lower, toolchain_index) {
  run_case(utest_result, (test_t) {
    .manifest = "toolchain_index",
    .toolchains = {
      {
        .name = "zig",
        .kind = SPN_TOOLCHAIN_INDEX,
        .package = "core/zig",
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
        .os = SPN_OS_LINUX,
        .arch = SPN_ARCH_X64,
        .abi = SPN_ABI_GNU,
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
