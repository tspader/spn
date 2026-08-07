#include "unit.h"

sp_test_suite(link_plan, .serial = true);

// The examined link unit; it lands on the root package as a target named "app"
typedef struct {
  spn_target_kind_t kind;
  spn_linkage_set_t linkages;
  const c8* source [UNIT_TEST_MAX_STRS];
  const c8* deps [UNIT_TEST_MAX_STRS];
  const c8* frameworks [UNIT_TEST_MAX_STRS];
  spn_os_version_t min_os;
} plan_target_t;

typedef struct {
  const c8* libs [UNIT_TEST_MAX_STRS];
  const c8* whole_archives [UNIT_TEST_MAX_STRS]; // matched by file name suffix
  const c8* private_libs [UNIT_TEST_MAX_STRS];
  const c8* system_libs [UNIT_TEST_MAX_STRS];
  const c8* frameworks [UNIT_TEST_MAX_STRS];
  spn_os_version_t min_os;
  spn_lang_t lang;
} plan_expect_t;

typedef struct {
  const c8* name;
  plan_target_t target;
  unit_graph_test_t graph;
  plan_expect_t expect;
} plan_test_t;

static const plan_test_t tests [] = {
  // An executable takes everything on the link line: static and shared libs
  // both, private or not, while object, source, and no_link libs stay off
  {
    .name = "exe_places_all_deps_on_link_line",
    .target = { .kind = SPN_TARGET_KIND_EXE, .source = { "main.c" } },
    .graph = {
      .pkgs = {
        {
          .name = "P1",
          .deps = {
            { "D1" }, { "D2" }, { "D3", .private = true }, { "D4" }, { "D5" }, { "D6" }
          },
          .system_deps = { "pthread" }
        },
        { .name = "D1", .libs = { { "D1", STATIC_ONLY, .source = { "a.c" } } }, .system_deps = { "m" } },
        { .name = "D2", .libs = { { "D2", SHARED_ONLY, .source = { "s.c" } } } },
        { .name = "D3", .libs = { { "D3", STATIC_ONLY, .source = { "p.c" } } } },
        { .name = "D4", .libs = { { "D4", OBJECT_ONLY, .source = { "b.c" } } } },
        { .name = "D5", .libs = { { "D5", SOURCE_ONLY, .source = { "r.c" } } } },
        { .name = "D6", .libs = { { "D6", STATIC_ONLY, .no_link = true, .source = { "t.c" } } } },
      },
    },
    .expect = {
      .libs = { "D3", "D2", "D1" },
      .system_libs = { "pthread", "m" },
    },
  },
  // A shared lib splits its static deps by privacy: public ones are embedded
  // whole so their symbols re-export, private ones link normally and stay hidden
  {
    .name = "shared_lib_splits_static_deps",
    .target = { .kind = SPN_TARGET_KIND_LIB, .linkages = SHARED_ONLY, .source = { "app.c" } },
    .graph = {
      .pkgs = {
        { .name = "P1", .deps = { { "D1" }, { "D2" }, { "D3", .private = true } } },
        { .name = "D1", .libs = { { "D1", STATIC_ONLY, .source = { "a.c" } } } },
        { .name = "D2", .libs = { { "D2", SHARED_ONLY, .source = { "s.c" } } } },
        { .name = "D3", .libs = { { "D3", STATIC_ONLY, .source = { "p.c" } } } },
      },
    },
    .expect = {
      .libs = { "D2" },
      .whole_archives = { "libD1.a" },
      .private_libs = { "D3" },
    },
  },
  {
    .name = "siblings_link_before_packages",
    .target = { .kind = SPN_TARGET_KIND_EXE, .source = { "main.c" }, .deps = { "L1" } },
    .graph = {
      .pkgs = {
        { .name = "P1",
          .deps = { { "D1" } },
          .libs = {
            { "L1", STATIC_ONLY, .source = { "u.c" }, .deps = { "L2" } },
            { "L2", STATIC_ONLY, .source = { "b.c" } },
          } },
        { .name = "D1", .libs = { { "D1", STATIC_ONLY, .source = { "s.c" } } } },
      },
    },
    .expect = {
      .libs = { "L1", "L2", "D1" },
    },
  },
  {
    .name = "cxx_static_dep_promotes_lang",
    .target = { .kind = SPN_TARGET_KIND_EXE, .source = { "main.c" } },
    .graph = {
      .pkgs = {
        { .name = "P1", .deps = { { "D1" } } },
        { .name = "D1", .libs = { { "D1", STATIC_ONLY, .source = { "x.cpp" } } } },
      },
    },
    .expect = {
      .libs = { "D1" },
      .lang = SPN_LANG_CXX,
    },
  },
  // A shared lib already linked its own C++ runtime; the consumer stays C
  {
    .name = "cxx_shared_dep_keeps_c",
    .target = { .kind = SPN_TARGET_KIND_EXE, .source = { "main.c" } },
    .graph = {
      .pkgs = {
        { .name = "P1", .deps = { { "D1" } } },
        { .name = "D1", .libs = { { "D1", SHARED_ONLY, .source = { "x.cpp" } } } },
      },
    },
    .expect = {
      .libs = { "D1" },
    },
  },
  // min_os is the max over the target, every closure package, and every closure
  // lib, including shared boundaries the recursion otherwise stops at
  {
    .name = "min_os_folds_over_closure",
    .target = { .kind = SPN_TARGET_KIND_EXE, .source = { "main.c" }, .min_os = { 11, 0 } },
    .graph = {
      .os = SPN_OS_MACOS,
      .sysroot = "/sdk",
      .pkgs = {
        { .name = "P1", .deps = { { "D1" }, { "D2" } } },
        { .name = "D1", .min_os = { 12, 5 },
          .libs = { { "D1", STATIC_ONLY, .source = { "a.c" }, .min_os = { 13, 1 } } } },
        { .name = "D2",
          .libs = { { "D2", SHARED_ONLY, .source = { "s.c" }, .min_os = { 14, 0 } } } },
      },
    },
    .expect = {
      .libs = { "D2", "D1" },
      .min_os = { 14, 0 },
    },
  },
  // Frameworks dedupe, and only packages that link code contribute their
  // package-level frameworks: shared-only and no_link-only packages are skipped
  {
    .name = "frameworks_dedupe_and_respect_links_code",
    .target = { .kind = SPN_TARGET_KIND_EXE, .source = { "main.c" }, .frameworks = { "Metal" } },
    .graph = {
      .os = SPN_OS_MACOS,
      .sysroot = "/sdk",
      .pkgs = {
        { .name = "P1", .deps = { { "D1" }, { "D2" }, { "D3" } } },
        { .name = "D1", .frameworks = { "Cocoa" },
          .libs = { { "D1", STATIC_ONLY, .source = { "a.c" }, .frameworks = { "Metal", "IOKit" } } } },
        { .name = "D2", .frameworks = { "AppKit" },
          .libs = { { "D2", SHARED_ONLY, .source = { "s.c" }, .frameworks = { "CoreAudio" } } } },
        { .name = "D3", .frameworks = { "Carbon" },
          .libs = { { "L1", STATIC_ONLY, .no_link = true, .source = { "n.c" }, .frameworks = { "Cocoa" } } } },
      },
    },
    .expect = {
      .libs = { "D2", "D1" },
      .frameworks = { "Metal", "Cocoa", "IOKit" },
    },
  },
  {
    .name = "test_target_links_test_deps",
    .target = { .kind = SPN_TARGET_KIND_TEST, .source = { "main.c" } },
    .graph = {
      .pkgs = {
        { .name = "P1", .deps = { { "T1", .kind = SPN_DEP_KIND_TEST }, { "D1" } } },
        { .name = "T1", .libs = { { "T1", STATIC_ONLY, .source = { "t.c" } } } },
        { .name = "D1", .libs = { { "D1", STATIC_ONLY, .source = { "a.c" } } } },
      },
    },
    .expect = {
      .libs = { "D1", "T1" },
    },
  },
};

static spn_target_info_t target_info(sp_mem_t mem, const plan_target_t* spec) {
  spn_target_info_t info = sp_zero;
  info.name = sp_str_lit("app");
  info.kind = spec->kind;
  info.linkages = spec->linkages;
  info.source = test_str_list(mem, spec->source, UNIT_TEST_MAX_STRS);
  info.deps = test_str_list(mem, spec->deps, UNIT_TEST_MAX_STRS);
  info.macos.frameworks = test_str_list(mem, spec->frameworks, UNIT_TEST_MAX_STRS);
  info.macos.min_os = spec->min_os;
  return info;
}

static sp_err_t expect_str_suffixes(sp_test_t* t, sp_da(sp_str_t) actual, const c8* const* expect, u32 count) {
  sp_must_eq(t, count, sp_da_size(actual));
  sp_for(it, count) {
    sp_expect(t, sp_str_ends_with(actual[it], sp_str_view(expect[it])));
  }
  return SP_OK;
}

sp_test_each(link_plan, plan, plan_test_t, tests, .setup = spn_test_ctx_setup) {
  sp_mem_t mem = sp_test_arena(t);
  spn_session_t* s = build_session(mem, &it->graph);

  spn_target_info_t target = target_info(mem, &it->target);
  switch (it->target.kind) {
    case SPN_TARGET_KIND_LIB:  sp_str_om_insert(s->pkg->libs, target.name, target); break;
    case SPN_TARGET_KIND_TEST: sp_str_om_insert(s->pkg->tests, target.name, target); break;
    default:              sp_str_om_insert(s->pkg->exes, target.name, target); break;
  }

  spn_err_union_t err = spn_units_add_packages(s);
  sp_must_eq(t, SPN_OK, err.kind);
  err = spn_units_add_targets(s, SPN_UNIT_SCOPE_TARGET);
  sp_must_eq(t, SPN_OK, err.kind);

  spn_pkg_unit_t* root = spn_session_find_pkg_unit(s, s->units.target, find_pkg_id(s, &it->graph, it->graph.pkgs[0].name));
  sp_must(t, root != SP_NULLPTR);
  spn_target_unit_t* app = spn_session_find_target_in_pkg(s, root, sp_str_lit("app"));
  sp_must(t, app != SP_NULLPTR);

  spn_link_plan_t* plan = &app->link;
  sp_must_strs_eq(t, plan->cc.libs, sp_da_size(plan->cc.libs), it->expect.libs);
  u32 num_whole_archives = 0;
  sp_carr_detect_len(it->expect.whole_archives, num_whole_archives, it->expect.whole_archives[num_whole_archives]);
  sp_try(expect_str_suffixes(t, plan->cc.whole_archives, it->expect.whole_archives, num_whole_archives));
  sp_must_strs_eq(t, plan->cc.private_libs, sp_da_size(plan->cc.private_libs), it->expect.private_libs);
  sp_must_strs_eq(t, plan->cc.system_libs, sp_da_size(plan->cc.system_libs), it->expect.system_libs);
  sp_must_strs_eq(t, plan->cc.frameworks, sp_da_size(plan->cc.frameworks), it->expect.frameworks);
  sp_expect_eq(t, it->expect.min_os.major, plan->cc.min_os.major);
  sp_expect_eq(t, it->expect.min_os.minor, plan->cc.min_os.minor);
  sp_expect_eq(t, it->expect.lang, plan->cc.lang);
  sp_expect(t, plan->cc.rpath);

  return SP_OK;
}
