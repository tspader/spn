#include "sp.h"
#include "sp/macro.h"

#define SP_GLOB_IMPLEMENTATION
#include "sp/sp_glob.h"

#include "utest.h"

#include "ctx/types.h"
#include "external/wasm/types.h"
#include "intern/intern.h"
#include "session/session.h"
#include "session/types.h"
#include "unit/types.h"
#include "unit/unit.h"

// The metaprogram runtime is out of scope here; satisfy unit/target.c's
// reference without pulling wamr into the binary
void spn_wasm_script_init(spn_wasm_script_t* script, sp_str_t module) {
  *script = (spn_wasm_script_t) { .path = module };
}

#define PLAN_TEST_MAX_PKGS 8
#define PLAN_TEST_MAX_DEPS 8
#define PLAN_TEST_MAX_LIBS 4
#define PLAN_TEST_MAX_STRS 8

typedef struct {
  const c8* to;
  spn_dep_kind_t kind;
  bool private;
} plan_dep_t;

typedef struct {
  const c8* name;
  spn_linkage_set_t linkages;
  bool no_link;
  const c8* source [PLAN_TEST_MAX_STRS];
  const c8* deps [PLAN_TEST_MAX_STRS];
  const c8* frameworks [PLAN_TEST_MAX_STRS];
  spn_os_version_t min_os;
} plan_lib_t;

typedef struct {
  const c8* name;
  plan_dep_t deps [PLAN_TEST_MAX_DEPS];
  plan_lib_t libs [PLAN_TEST_MAX_LIBS];
  const c8* system_deps [PLAN_TEST_MAX_STRS];
  const c8* frameworks [PLAN_TEST_MAX_STRS];
  spn_os_version_t min_os;
} plan_pkg_t;

// The examined link unit; it lands on the root package as a target named "app"
typedef struct {
  spn_target_kind_t kind;
  spn_linkage_set_t linkages;
  const c8* source [PLAN_TEST_MAX_STRS];
  const c8* deps [PLAN_TEST_MAX_STRS];
  const c8* frameworks [PLAN_TEST_MAX_STRS];
  spn_os_version_t min_os;
} plan_target_t;

typedef struct {
  const c8* libs [PLAN_TEST_MAX_STRS];
  const c8* whole_archives [PLAN_TEST_MAX_STRS]; // matched by file name suffix
  const c8* private_libs [PLAN_TEST_MAX_STRS];
  const c8* system_libs [PLAN_TEST_MAX_STRS];
  const c8* frameworks [PLAN_TEST_MAX_STRS];
  spn_os_version_t min_os;
  spn_lang_t lang;
} plan_expect_t;

typedef struct {
  spn_os_t os; // zero selects linux
  const c8* sysroot;
  plan_target_t target;
  plan_pkg_t pkgs [PLAN_TEST_MAX_PKGS]; // pkgs[0] is the root package
  plan_expect_t expect;
} plan_test_t;

UTEST_EMPTY_FIXTURE(link_plan)

static sp_da(sp_str_t) plan_str_list(sp_mem_t mem, const c8* const* items, u32 max) {
  sp_da(sp_str_t) list = sp_da_new(mem, sp_str_t);
  sp_for(it, max) {
    if (!items[it]) {
      break;
    }
    sp_da_push(list, sp_str_view(items[it]));
  }
  return list;
}

static spn_target_info_t plan_lib_info(sp_mem_t mem, const plan_lib_t* spec) {
  spn_target_info_t info = sp_zero;
  info.name = sp_str_view(spec->name);
  info.kind = SPN_TARGET_LIB;
  info.linkages = spec->linkages;
  info.no_link = spec->no_link;
  info.source = plan_str_list(mem, spec->source, PLAN_TEST_MAX_STRS);
  info.deps = plan_str_list(mem, spec->deps, PLAN_TEST_MAX_STRS);
  info.macos.frameworks = plan_str_list(mem, spec->frameworks, PLAN_TEST_MAX_STRS);
  info.macos.min_os = spec->min_os;
  return info;
}

static spn_target_info_t plan_target_info(sp_mem_t mem, const plan_target_t* spec) {
  spn_target_info_t info = sp_zero;
  info.name = sp_str_lit("app");
  info.kind = spec->kind;
  info.linkages = spec->linkages;
  info.source = plan_str_list(mem, spec->source, PLAN_TEST_MAX_STRS);
  info.deps = plan_str_list(mem, spec->deps, PLAN_TEST_MAX_STRS);
  info.macos.frameworks = plan_str_list(mem, spec->frameworks, PLAN_TEST_MAX_STRS);
  info.macos.min_os = spec->min_os;
  return info;
}

static spn_pkg_id_t plan_pkg_id(spn_session_t* s, const c8* name, u32 index) {
  return (spn_pkg_id_t) {
    .qualified = sp_intern_get_or_insert(s->intern, sp_str_view(name)),
    .hash = index + 1,
  };
}

static spn_build_unit_t* plan_build_unit(spn_session_t* s, spn_build_id_t id, const c8* root, spn_profile_info_t profile) {
  sp_om_insert(s->units.builds, id, sp_zero_struct(spn_build_unit_t));
  spn_build_unit_t* build = sp_om_back(s->units.builds);
  build->id = id;
  build->profile = profile;
  build->paths.root = sp_str_view(root);
  sp_da_init(s->mem, build->include);
  sp_da_init(s->mem, build->packages);

  spn_toolchain_info_t* info = sp_alloc_type(s->mem, spn_toolchain_info_t);
  info->name = sp_str_lit("test");
  info->driver = SPN_CC_DRIVER_GCC;
  info->compiler.program = sp_str_lit("cc");
  info->cxx.program = sp_str_lit("c++");
  info->linker.program = sp_str_lit("cc");
  info->archiver.program = sp_str_lit("ar");

  spn_toolchain_unit_t* toolchain = sp_alloc_type(s->mem, spn_toolchain_unit_t);
  toolchain->info = info;
  toolchain->cc = (spn_cc_toolchain_t) {
    .name = info->name,
    .driver = SPN_CC_DRIVER_GCC,
    .compiler = info->compiler,
    .cxx = info->cxx,
    .linker = info->linker,
    .archiver = info->archiver,
    .archiver_driver = SPN_AR_DRIVER_GNU,
  };
  build->toolchain = toolchain;
  return build;
}

static void expect_str_list(s32* utest_result, sp_da(sp_str_t) actual, const c8* const* expect, bool suffix) {
  u32 expected = 0;
  sp_for(it, PLAN_TEST_MAX_STRS) {
    if (!expect[it]) {
      break;
    }
    expected++;
  }

  ASSERT_EQ(expected, sp_da_size(actual));
  sp_for(it, expected) {
    if (suffix) {
      EXPECT_TRUE(sp_str_ends_with(actual[it], sp_str_view(expect[it])));
    }
    else {
      EXPECT_TRUE(sp_str_equal(actual[it], sp_str_view(expect[it])));
    }
  }
}

static void run_plan_test(s32* utest_result, plan_test_t t) {
  sp_mem_t mem = sp_mem_os_new();
  spn_session_t* s = sp_alloc_type(mem, spn_session_t);
  s->mem = mem;
  s->intern = sp_intern_new(mem);
  spn.intern = s->intern;
  sp_ht_init(mem, s->resolve);
  sp_ht_init(mem, s->packages);
  sp_ht_init(mem, s->options);
  sp_ht_init(mem, s->fingerprints);
  sp_da_init(mem, s->plans);
  sp_om_new(s->units.builds);
  sp_om_new(s->units.packages);
  sp_om_new(s->units.targets);
  sp_om_new(s->units.objects);

  spn_profile_info_t profile = {
    .name = sp_str_lit("debug"),
    .toolchain = sp_str_lit("auto"),
    .standard = SPN_C11,
    .mode = SPN_BUILD_MODE_DEBUG,
    .os = t.os ? t.os : SPN_OS_LINUX,
    .arch = SPN_ARCH_X64,
    .abi = t.os == SPN_OS_MACOS ? SPN_ABI_NONE : SPN_ABI_GNU,
    .sysroot = t.sysroot ? sp_str_view(t.sysroot) : sp_str_lit(""),
  };

  s->units.target = plan_build_unit(s, 1, "/bld/debug", profile);
  s->units.metaprogram = plan_build_unit(s, 2, "/bld/wasm32-wasi", profile);

  u32 count = 0;
  sp_carr_for(t.pkgs, it) {
    if (!t.pkgs[it].name) {
      break;
    }
    count++;
  }

  sp_for(it, count) {
    plan_pkg_t* pkg = &t.pkgs[it];
    spn_pkg_id_t id = plan_pkg_id(s, pkg->name, it);

    spn_pkg_info_t* info = sp_alloc_type(mem, spn_pkg_info_t);
    info->name = sp_str_view(pkg->name);
    info->qualified = sp_str_view(pkg->name);
    info->system_deps = plan_str_list(mem, pkg->system_deps, PLAN_TEST_MAX_STRS);
    info->macos.frameworks = plan_str_list(mem, pkg->frameworks, PLAN_TEST_MAX_STRS);
    info->macos.min_os = pkg->min_os;

    sp_carr_for(pkg->libs, lt) {
      if (!pkg->libs[lt].name) {
        break;
      }
      spn_target_info_t lib = plan_lib_info(mem, &pkg->libs[lt]);
      sp_str_om_insert(info->libs, lib.name, lib);
    }

    if (it == 0) {
      s->pkg = info;
      spn_target_info_t target = plan_target_info(mem, &t.target);
      switch (t.target.kind) {
        case SPN_TARGET_LIB:  sp_str_om_insert(info->libs, target.name, target); break;
        case SPN_TARGET_TEST: sp_str_om_insert(info->tests, target.name, target); break;
        default:              sp_str_om_insert(info->exes, target.name, target); break;
      }
    }

    spn_loaded_pkg_t loaded = {
      .source = it == 0 ? SPN_PKG_SOURCE_ROOT : SPN_PKG_SOURCE_FILE,
      .info = info,
    };
    sp_ht_insert(s->packages, id, loaded);

    spn_resolved_pkg_t resolved = {
      .id = id,
      .source = loaded.source,
    };
    sp_da_init(mem, resolved.edges);
    sp_carr_for(pkg->deps, dt) {
      if (!pkg->deps[dt].to) {
        break;
      }
      sp_for(jt, count) {
        if (sp_str_equal_cstr(sp_str_view(t.pkgs[jt].name), pkg->deps[dt].to)) {
          sp_da_push(resolved.edges, ((spn_resolved_dep_t) {
            .id = plan_pkg_id(s, t.pkgs[jt].name, jt),
            .kind = pkg->deps[dt].kind,
            .private = pkg->deps[dt].private,
          }));
        }
      }
    }
    sp_ht_insert(s->resolve, id, resolved);
  }

  spn_build_plan_t build_plan = { .build = s->units.target };
  sp_da_init(mem, build_plan.roots);
  sp_da_push(s->plans, build_plan);

  spn_err_union_t err = spn_units_add_packages(s);
  ASSERT_EQ(SPN_OK, err.kind);
  err = spn_units_add_targets(s, SPN_UNIT_SCOPE_TARGET);
  ASSERT_EQ(SPN_OK, err.kind);

  spn_pkg_unit_t* root = spn_session_find_pkg_unit(s, s->units.target, plan_pkg_id(s, t.pkgs[0].name, 0));
  ASSERT_TRUE(root != SP_NULLPTR);
  spn_target_unit_t* app = spn_session_find_target_in_pkg(s, root, sp_str_lit("app"));
  ASSERT_TRUE(app != SP_NULLPTR);

  spn_link_plan_t* plan = &app->link;
  expect_str_list(utest_result, plan->cc.libs, t.expect.libs, false);
  expect_str_list(utest_result, plan->cc.whole_archives, t.expect.whole_archives, true);
  expect_str_list(utest_result, plan->cc.private_libs, t.expect.private_libs, false);
  expect_str_list(utest_result, plan->cc.system_libs, t.expect.system_libs, false);
  expect_str_list(utest_result, plan->cc.frameworks, t.expect.frameworks, false);
  EXPECT_EQ(t.expect.min_os.major, plan->cc.min_os.major);
  EXPECT_EQ(t.expect.min_os.minor, plan->cc.min_os.minor);
  EXPECT_EQ(t.expect.lang, plan->cc.lang);
  EXPECT_TRUE(plan->cc.rpath);
}

#define STATIC_ONLY ((spn_linkage_set_t) { .static_lib = true })
#define SHARED_ONLY ((spn_linkage_set_t) { .shared = true })
#define OBJECT_ONLY ((spn_linkage_set_t) { .object = true })
#define SOURCE_ONLY ((spn_linkage_set_t) { .source = true })

// An executable takes everything on the link line: static and shared libs
// both, private or not, while object, source, and no_link libs stay off
UTEST_F(link_plan, exe_places_all_deps_on_link_line) {
  run_plan_test(utest_result, (plan_test_t) {
    .target = { .kind = SPN_TARGET_EXE, .source = { "main.c" } },
    .pkgs = {
      {
        .name = "main",
        .deps = {
          { "arch" }, { "shim" }, { "priv", .private = true }, { "blob" }, { "raw" }, { "tcc" }
        },
        .system_deps = { "pthread" }
      },
      { .name = "arch", .libs = { { "arch", STATIC_ONLY, .source = { "a.c" } } }, .system_deps = { "m" } },
      { .name = "shim", .libs = { { "shim", SHARED_ONLY, .source = { "s.c" } } } },
      { .name = "priv", .libs = { { "priv", STATIC_ONLY, .source = { "p.c" } } } },
      { .name = "blob", .libs = { { "blob", OBJECT_ONLY, .source = { "b.c" } } } },
      { .name = "raw", .libs = { { "raw", SOURCE_ONLY, .source = { "r.c" } } } },
      { .name = "tcc", .libs = { { "tcc", STATIC_ONLY, .no_link = true, .source = { "t.c" } } } },
    },
    .expect = {
      .libs = { "priv", "shim", "arch" },
      .system_libs = { "pthread", "m" },
      .lang = SPN_LANG_C,
    },
  });
}

// A shared lib splits its static deps by privacy: public ones are embedded
// whole so their symbols re-export, private ones link normally and stay hidden
UTEST_F(link_plan, shared_lib_splits_static_deps) {
  run_plan_test(utest_result, (plan_test_t) {
    .target = { .kind = SPN_TARGET_LIB, .linkages = SHARED_ONLY, .source = { "app.c" } },
    .pkgs = {
      { .name = "main", .deps = { { "arch" }, { "shim" }, { "priv", .private = true } } },
      { .name = "arch", .libs = { { "arch", STATIC_ONLY, .source = { "a.c" } } } },
      { .name = "shim", .libs = { { "shim", SHARED_ONLY, .source = { "s.c" } } } },
      { .name = "priv", .libs = { { "priv", STATIC_ONLY, .source = { "p.c" } } } },
    },
    .expect = {
      .libs = { "shim" },
      .whole_archives = { "libarch.a" },
      .private_libs = { "priv" },
      .lang = SPN_LANG_C,
    },
  });
}

UTEST_F(link_plan, siblings_link_before_packages) {
  run_plan_test(utest_result, (plan_test_t) {
    .target = { .kind = SPN_TARGET_EXE, .source = { "main.c" }, .deps = { "util" } },
    .pkgs = {
      { .name = "main",
        .deps = { { "spum" } },
        .libs = {
          { "util", STATIC_ONLY, .source = { "u.c" }, .deps = { "base" } },
          { "base", STATIC_ONLY, .source = { "b.c" } },
        } },
      { .name = "spum", .libs = { { "spum", STATIC_ONLY, .source = { "s.c" } } } },
    },
    .expect = {
      .libs = { "util", "base", "spum" },
      .lang = SPN_LANG_C,
    },
  });
}

UTEST_F(link_plan, cxx_static_dep_promotes_lang) {
  run_plan_test(utest_result, (plan_test_t) {
    .target = { .kind = SPN_TARGET_EXE, .source = { "main.c" } },
    .pkgs = {
      { .name = "main", .deps = { { "xx" } } },
      { .name = "xx", .libs = { { "xx", STATIC_ONLY, .source = { "x.cpp" } } } },
    },
    .expect = {
      .libs = { "xx" },
      .lang = SPN_LANG_CXX,
    },
  });
}

// A shared lib already linked its own C++ runtime; the consumer stays C
UTEST_F(link_plan, cxx_shared_dep_keeps_c) {
  run_plan_test(utest_result, (plan_test_t) {
    .target = { .kind = SPN_TARGET_EXE, .source = { "main.c" } },
    .pkgs = {
      { .name = "main", .deps = { { "xx" } } },
      { .name = "xx", .libs = { { "xx", SHARED_ONLY, .source = { "x.cpp" } } } },
    },
    .expect = {
      .libs = { "xx" },
      .lang = SPN_LANG_C,
    },
  });
}

// min_os is the max over the target, every closure package, and every closure
// lib, including shared boundaries the recursion otherwise stops at
UTEST_F(link_plan, min_os_folds_over_closure) {
  run_plan_test(utest_result, (plan_test_t) {
    .os = SPN_OS_MACOS,
    .sysroot = "/sdk",
    .target = { .kind = SPN_TARGET_EXE, .source = { "main.c" }, .min_os = { 11, 0 } },
    .pkgs = {
      { .name = "main", .deps = { { "arch" }, { "shim" } } },
      { .name = "arch", .min_os = { 12, 5 },
        .libs = { { "arch", STATIC_ONLY, .source = { "a.c" }, .min_os = { 13, 1 } } } },
      { .name = "shim",
        .libs = { { "shim", SHARED_ONLY, .source = { "s.c" }, .min_os = { 14, 0 } } } },
    },
    .expect = {
      .libs = { "shim", "arch" },
      .min_os = { 14, 0 },
      .lang = SPN_LANG_C,
    },
  });
}

// Frameworks dedupe, and only packages that link code contribute their
// package-level frameworks: shared-only and no_link-only packages are skipped
UTEST_F(link_plan, frameworks_dedupe_and_respect_links_code) {
  run_plan_test(utest_result, (plan_test_t) {
    .os = SPN_OS_MACOS,
    .sysroot = "/sdk",
    .target = { .kind = SPN_TARGET_EXE, .source = { "main.c" }, .frameworks = { "Metal" } },
    .pkgs = {
      { .name = "main", .deps = { { "arch" }, { "shim" }, { "noli" } } },
      { .name = "arch", .frameworks = { "Cocoa" },
        .libs = { { "arch", STATIC_ONLY, .source = { "a.c" }, .frameworks = { "Metal", "IOKit" } } } },
      { .name = "shim", .frameworks = { "AppKit" },
        .libs = { { "shim", SHARED_ONLY, .source = { "s.c" }, .frameworks = { "CoreAudio" } } } },
      { .name = "noli", .frameworks = { "Carbon" },
        .libs = { { "noli_a", STATIC_ONLY, .no_link = true, .source = { "n.c" }, .frameworks = { "Cocoa" } } } },
    },
    .expect = {
      .libs = { "shim", "arch" },
      .frameworks = { "Metal", "Cocoa", "IOKit" },
      .lang = SPN_LANG_C,
    },
  });
}

UTEST_F(link_plan, test_target_links_test_deps) {
  run_plan_test(utest_result, (plan_test_t) {
    .target = { .kind = SPN_TARGET_TEST, .source = { "main.c" } },
    .pkgs = {
      { .name = "main", .deps = { { "tdep", .kind = SPN_DEP_KIND_TEST }, { "arch" } } },
      { .name = "tdep", .libs = { { "tdep", STATIC_ONLY, .source = { "t.c" } } } },
      { .name = "arch", .libs = { { "arch", STATIC_ONLY, .source = { "a.c" } } } },
    },
    .expect = {
      .libs = { "arch", "tdep" },
      .lang = SPN_LANG_C,
    },
  });
}
