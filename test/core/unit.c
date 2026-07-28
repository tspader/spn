#define SP_IMPLEMENTATION
#include "sp.h"
#include "sp/macro.h"

#include "utest.h"

#include "ctx/types.h"
#include "intern/intern.h"
#include "session/session.h"
#include "session/types.h"
#include "unit/types.h"
#include "unit/unit.h"

spn_ctx_t spn;

UTEST_MAIN()

#define WALK_TEST_MAX_PKGS 8
#define WALK_TEST_MAX_DEPS 4
#define WALK_TEST_MAX_UNITS 16

typedef enum {
  WALK_TARGET,
  WALK_METAPROGRAM,
} walk_build_t;

typedef struct {
  const c8* to;
  spn_dep_kind_t kind;
} walk_dep_t;

typedef struct {
  const c8* name;
  bool scripts;
  walk_dep_t deps [WALK_TEST_MAX_DEPS];
} walk_pkg_t;

typedef struct {
  const c8* pkg;
  walk_build_t build;
  bool host;
} walk_unit_t;

typedef struct {
  walk_pkg_t pkgs [WALK_TEST_MAX_PKGS];
  walk_unit_t expect [WALK_TEST_MAX_UNITS];
} walk_test_t;

UTEST_EMPTY_FIXTURE(unit)

static spn_pkg_id_t walk_pkg_id(spn_session_t* s, const c8* name, u32 index) {
  return (spn_pkg_id_t) {
    .qualified = sp_intern_get_or_insert(s->intern, sp_str_view(name)),
    .hash = index + 1,
  };
}

static spn_build_unit_t* walk_build(spn_session_t* s, spn_build_id_t id, const c8* root) {
  sp_om_insert(s->units.builds, id, sp_zero_struct(spn_build_unit_t));
  spn_build_unit_t* build = sp_om_back(s->units.builds);
  build->id = id;
  build->paths.root = sp_str_view(root);
  sp_da_init(s->mem, build->include);
  sp_da_init(s->mem, build->packages);
  return build;
}

void run_walk_test(s32* utest_result, walk_test_t t) {
  sp_mem_t mem = sp_mem_os_new();
  spn_session_t* s = sp_alloc_type(mem, spn_session_t);
  s->mem = mem;
  s->intern = sp_intern_new(mem);
  s->pkg = sp_alloc_type(mem, spn_pkg_info_t);
  sp_ht_init(mem, s->resolve);
  sp_ht_init(mem, s->packages);
  sp_ht_init(mem, s->options);
  sp_ht_init(mem, s->fingerprints);
  sp_da_init(mem, s->plans);
  sp_om_new(s->units.builds);
  sp_om_new(s->units.packages);
  sp_om_new(s->units.targets);
  sp_om_new(s->units.objects);
  sp_mutex_init(&s->mutex, SP_MUTEX_PLAIN);

  s->units.target = walk_build(s, 1, "/build/debug");
  s->units.metaprogram = walk_build(s, 2, "/build/wasm32-wasi");

  u32 count = 0;
  sp_carr_for(t.pkgs, it) {
    if (!t.pkgs[it].name) {
      break;
    }
    count++;
  }

  sp_for(it, count) {
    walk_pkg_t* pkg = &t.pkgs[it];
    spn_pkg_id_t id = walk_pkg_id(s, pkg->name, it);

    spn_pkg_info_t* info = sp_alloc_type(mem, spn_pkg_info_t);
    info->name = sp_str_view(pkg->name);
    info->qualified = sp_str_view(pkg->name);

    spn_loaded_pkg_t loaded = {
      .source = it == 0 ? SPN_PKG_SOURCE_ROOT : SPN_PKG_SOURCE_FILE,
      .info = info,
    };
    if (pkg->scripts) {
      sp_da_init(mem, loaded.configure.source);
      sp_da_push(loaded.configure.source, sp_str_lit("configure.c"));
    }
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
            .id = walk_pkg_id(s, t.pkgs[jt].name, jt),
            .kind = pkg->deps[dt].kind,
          }));
        }
      }
    }
    sp_ht_insert(s->resolve, id, resolved);
  }

  sp_da_push(s->plans, ((spn_build_plan_t) { .build = s->units.target }));

  spn_err_union_t err = spn_units_add_packages(s);
  ASSERT_EQ(SPN_OK, err.kind);

  u32 expected = 0;
  sp_carr_for(t.expect, it) {
    if (!t.expect[it].pkg) {
      break;
    }
    expected++;
  }
  ASSERT_EQ(expected, sp_om_size(s->units.packages));

  sp_carr_for(t.expect, it) {
    walk_unit_t* expect = &t.expect[it];
    if (!expect->pkg) {
      break;
    }

    spn_pkg_id_t id = sp_zero_struct(spn_pkg_id_t);
    sp_for(jt, count) {
      if (sp_str_equal_cstr(sp_str_view(t.pkgs[jt].name), expect->pkg)) {
        id = walk_pkg_id(s, t.pkgs[jt].name, jt);
      }
    }

    spn_build_unit_t* build = expect->build == WALK_TARGET ? s->units.target : s->units.metaprogram;
    spn_pkg_unit_t* unit = spn_session_find_pkg_unit(s, build, id);
    ASSERT_TRUE(unit != SP_NULLPTR);
    EXPECT_EQ(expect->host, spn_pkg_unit_is_script_host(unit));

    spn_loaded_pkg_t* loaded = sp_ht_getp(s->packages, id);
    if (sp_da_empty(loaded->configure.source) && sp_da_empty(loaded->build.source)) {
      EXPECT_TRUE(unit->metaprogram == SP_NULLPTR);
    }
    else {
      ASSERT_TRUE(unit->metaprogram != SP_NULLPTR);
      EXPECT_TRUE(unit->metaprogram == spn_session_find_pkg_unit(s, s->units.metaprogram, id));
      EXPECT_TRUE(unit->metaprogram->metaprogram == unit->metaprogram);
    }
  }
}

UTEST_F(unit, package_chain_stays_in_target_build) {
  run_walk_test(utest_result, (walk_test_t) {
    .pkgs = {
      { .name = "R", .deps = { { "A" } } },
      { .name = "A", .deps = { { "B" } } },
      { .name = "B" },
    },
    .expect = {
      { "R", WALK_TARGET },
      { "A", WALK_TARGET },
      { "B", WALK_TARGET },
    },
  });
}

UTEST_F(unit, test_dep_of_root_included) {
  run_walk_test(utest_result, (walk_test_t) {
    .pkgs = {
      { .name = "R", .deps = { { "T", SPN_DEP_KIND_TEST } } },
      { .name = "T" },
    },
    .expect = {
      { "R", WALK_TARGET },
      { "T", WALK_TARGET },
    },
  });
}

UTEST_F(unit, test_dep_of_dep_excluded) {
  run_walk_test(utest_result, (walk_test_t) {
    .pkgs = {
      { .name = "R", .deps = { { "A" } } },
      { .name = "A", .deps = { { "T", SPN_DEP_KIND_TEST } } },
      { .name = "T" },
    },
    .expect = {
      { "R", WALK_TARGET },
      { "A", WALK_TARGET },
    },
  });
}

UTEST_F(unit, build_dep_lands_in_metaprogram_build) {
  run_walk_test(utest_result, (walk_test_t) {
    .pkgs = {
      { .name = "R", .scripts = true, .deps = { { "T", SPN_DEP_KIND_BUILD } } },
      { .name = "T" },
    },
    .expect = {
      { "R", WALK_TARGET },
      { "R", WALK_METAPROGRAM, .host = true },
      { "T", WALK_METAPROGRAM },
    },
  });
}

UTEST_F(unit, build_dep_without_scripts_is_dead) {
  run_walk_test(utest_result, (walk_test_t) {
    .pkgs = {
      { .name = "R", .deps = { { "T", SPN_DEP_KIND_BUILD } } },
      { .name = "T" },
    },
    .expect = {
      { "R", WALK_TARGET },
    },
  });
}

UTEST_F(unit, metaprogram_build_is_absorbing) {
  run_walk_test(utest_result, (walk_test_t) {
    .pkgs = {
      { .name = "R", .scripts = true, .deps = { { "T", SPN_DEP_KIND_BUILD } } },
      { .name = "T", .deps = { { "U" }, { "G", SPN_DEP_KIND_BUILD } } },
      { .name = "U" },
      { .name = "G" },
    },
    .expect = {
      { "R", WALK_TARGET },
      { "R", WALK_METAPROGRAM, .host = true },
      { "T", WALK_METAPROGRAM },
      { "U", WALK_METAPROGRAM },
      { "G", WALK_METAPROGRAM },
    },
  });
}

UTEST_F(unit, dual_membership_gets_two_units) {
  run_walk_test(utest_result, (walk_test_t) {
    .pkgs = {
      { .name = "R", .scripts = true, .deps = { { "A" }, { "A", SPN_DEP_KIND_BUILD } } },
      { .name = "A" },
    },
    .expect = {
      { "R", WALK_TARGET },
      { "R", WALK_METAPROGRAM, .host = true },
      { "A", WALK_TARGET },
      { "A", WALK_METAPROGRAM },
    },
  });
}

// A script-haver pulled in as a build dep is a member of the metaprogram
// build, not a host: its own package deps must build there too
UTEST_F(unit, script_haver_as_build_dep_is_a_member) {
  run_walk_test(utest_result, (walk_test_t) {
    .pkgs = {
      { .name = "R", .scripts = true, .deps = { { "B", SPN_DEP_KIND_BUILD } } },
      { .name = "B", .scripts = true, .deps = { { "U" } } },
      { .name = "U" },
    },
    .expect = {
      { "R", WALK_TARGET },
      { "R", WALK_METAPROGRAM, .host = true },
      { "B", WALK_METAPROGRAM },
      { "U", WALK_METAPROGRAM },
    },
  });
}
