#define SP_IMPLEMENTATION
#include "sp.h"
#include "sp/macro.h"

#include "utest.h"

#include "ctx/types.h"
#include "session/types.h"
#include "unit/types.h"
#include "target/closure.h"

spn_ctx_t spn;

UTEST_MAIN()

#define CLOSURE_TEST_MAX_PKGS 8
#define CLOSURE_TEST_MAX_DEPS 8
#define CLOSURE_TEST_MAX_NAMES 8

typedef struct {
  const c8* name;
  spn_linkage_t kind;
  const c8* deps [CLOSURE_TEST_MAX_DEPS];
} closure_pkg_t;

typedef struct {
  const c8* names [CLOSURE_TEST_MAX_NAMES];
} closure_expect_t;

typedef struct {
  const c8* root;
  closure_pkg_t pkgs [CLOSURE_TEST_MAX_PKGS];
  closure_expect_t expect;
} closure_test_t;

sp_da(spn_pkg_unit_t*) spn_session_pkg_deps(spn_session_t* session, spn_pkg_unit_t* pkg) {
  if (!sp_om_has(session->units.graph, pkg->id)) {
    return SP_NULLPTR;
  }
  return *sp_om_get(session->units.graph, pkg->id);
}

UTEST_EMPTY_FIXTURE(closure)

static spn_pkg_unit_t* find_pkg(spn_pkg_unit_t** pkgs, u32 count, const c8* name) {
  sp_str_t needle = sp_str_view(name);
  sp_for(it, count) {
    if (sp_str_equal(pkgs[it]->info->name, needle)) {
      return pkgs[it];
    }
  }
  return SP_NULLPTR;
}

static spn_pkg_unit_t* build_pkg(sp_mem_t mem, spn_session_t* session, u32 id, closure_pkg_t spec) {
  spn_pkg_info_t* info = sp_alloc_type(mem, spn_pkg_info_t);
  info->name = sp_str_view(spec.name);

  spn_pkg_unit_t* pkg = sp_alloc_type(mem, spn_pkg_unit_t);
  pkg->id.qualified = id;
  pkg->info = info;
  pkg->session = session;
  sp_da_init(mem, pkg->libs);

  if (spec.kind != SPN_LIB_KIND_NONE) {
    spn_target_info_t* lib_info = sp_alloc_type(mem, spn_target_info_t);
    lib_info->name = info->name;

    spn_target_unit_t* lib = sp_alloc_type(mem, spn_target_unit_t);
    lib->info = lib_info;
    lib->pkg = pkg;
    lib->session = session;
    lib->lib_kind = spec.kind;
    sp_da_push(pkg->libs, lib);
  }

  return pkg;
}

void run_closure_test(s32* utest_result, closure_test_t t) {
  sp_mem_t mem = sp_mem_os_new();
  spn_session_t* session = sp_alloc_type(mem, spn_session_t);
  session->mem = mem;

  spn_pkg_unit_t* pkgs [CLOSURE_TEST_MAX_PKGS] = SP_ZERO_INITIALIZE();
  u32 count = 0;
  sp_carr_for(t.pkgs, it) {
    if (!t.pkgs[it].name) {
      break;
    }
    pkgs[it] = build_pkg(mem, session, it + 1, t.pkgs[it]);
    count++;
  }

  sp_for(it, count) {
    sp_da(spn_pkg_unit_t*) deps = sp_da_new(mem, spn_pkg_unit_t*);
    sp_carr_for(t.pkgs[it].deps, d) {
      if (!t.pkgs[it].deps[d]) {
        break;
      }
      sp_da_push(deps, find_pkg(pkgs, count, t.pkgs[it].deps[d]));
    }
    sp_om_insert(session->units.graph, pkgs[it]->id, deps);
  }

  spn_pkg_unit_t* root = find_pkg(pkgs, count, t.root);
  spn_target_unit_t* exe = sp_alloc_type(mem, spn_target_unit_t);
  exe->pkg = root;
  exe->session = session;
  exe->lib_kind = SPN_LIB_KIND_NONE;

  sp_da(spn_pkg_unit_t*) closure = spn_target_link_closure(mem, exe);

  u32 expected = 0;
  sp_carr_for(t.expect.names, it) {
    if (!t.expect.names[it]) {
      break;
    }
    expected++;
  }

  ASSERT_EQ(expected, sp_da_size(closure));
  sp_for(it, expected) {
    EXPECT_TRUE(sp_str_equal(closure[it]->info->name, sp_str_view(t.expect.names[it])));
  }
}

UTEST_F(closure, transitive_static_chain) {
  run_closure_test(utest_result, (closure_test_t) {
    .root = "test",
    .pkgs = {
      { .name = "test", .deps = { "spum" } },
      { .name = "spum", .kind = SPN_LIB_KIND_STATIC, .deps = { "spam" } },
      { .name = "spam", .kind = SPN_LIB_KIND_STATIC },
    },
    .expect = { .names = { "spum", "spam" } },
  });
}

UTEST_F(closure, shared_boundary_stops_recursion) {
  run_closure_test(utest_result, (closure_test_t) {
    .root = "test",
    .pkgs = {
      { .name = "test", .deps = { "spum" } },
      { .name = "spum", .kind = SPN_LIB_KIND_SHARED, .deps = { "spam" } },
      { .name = "spam", .kind = SPN_LIB_KIND_STATIC },
    },
    .expect = { .names = { "spum" } },
  });
}

UTEST_F(closure, diamond_dedups_shared_dependency) {
  run_closure_test(utest_result, (closure_test_t) {
    .root = "test",
    .pkgs = {
      { .name = "test", .deps = { "spum", "spam" } },
      { .name = "spum", .kind = SPN_LIB_KIND_STATIC, .deps = { "grum" } },
      { .name = "spam", .kind = SPN_LIB_KIND_STATIC, .deps = { "grum" } },
      { .name = "grum", .kind = SPN_LIB_KIND_STATIC },
    },
    .expect = { .names = { "spam", "spum", "grum" } },
  });
}

UTEST_F(closure, unreferenced_package_absent) {
  run_closure_test(utest_result, (closure_test_t) {
    .root = "test",
    .pkgs = {
      { .name = "test", .deps = { "spum" } },
      { .name = "spum", .kind = SPN_LIB_KIND_STATIC },
      { .name = "orphan", .kind = SPN_LIB_KIND_STATIC },
    },
    .expect = { .names = { "spum" } },
  });
}
