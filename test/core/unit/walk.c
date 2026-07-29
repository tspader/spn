#include "unit.h"

#define WALK_TEST_MAX_UNITS 16

typedef enum {
  WALK_TARGET,
  WALK_METAPROGRAM,
} walk_build_t;

typedef struct {
  const c8* pkg;
  walk_build_t build;
  bool host;
} walk_unit_t;

typedef struct {
  const c8* name;
  unit_graph_test_t graph;
  walk_unit_t expect [WALK_TEST_MAX_UNITS];
} walk_test_t;

static const walk_test_t tests [] = {
  {
    .name = "package_chain_stays_in_target_build",
    .graph = {
      .pkgs = {
        { .name = "R", .deps = { { "A" } } },
        { .name = "A", .deps = { { "B" } } },
        { .name = "B" },
      },
    },
    .expect = {
      { "R" },
      { "A" },
      { "B" },
    },
  },
  {
    .name = "test_dep_of_root_included",
    .graph = {
      .pkgs = {
        { .name = "R", .deps = { { "T", SPN_DEP_KIND_TEST } } },
        { .name = "T" },
      },
    },
    .expect = {
      { "R" },
      { "T" },
    },
  },
  {
    .name = "test_dep_of_dep_excluded",
    .graph = {
      .pkgs = {
        { .name = "R", .deps = { { "A" } } },
        { .name = "A", .deps = { { "T", SPN_DEP_KIND_TEST } } },
        { .name = "T" },
      },
    },
    .expect = {
      { "R" },
      { "A" },
    },
  },
  {
    .name = "build_dep_lands_in_metaprogram_build",
    .graph = {
      .pkgs = {
        { .name = "R", .scripts = true, .deps = { { "T", SPN_DEP_KIND_BUILD } } },
        { .name = "T" },
      },
    },
    .expect = {
      { "R" },
      { "R", WALK_METAPROGRAM, .host = true },
      { "T", WALK_METAPROGRAM },
    },
  },
  {
    .name = "build_dep_without_scripts_is_dead",
    .graph = {
      .pkgs = {
        { .name = "R", .deps = { { "T", SPN_DEP_KIND_BUILD } } },
        { .name = "T" },
      },
    },
    .expect = {
      { "R" },
    },
  },
  {
    .name = "metaprogram_build_is_absorbing",
    .graph = {
      .pkgs = {
        { .name = "R", .scripts = true, .deps = { { "T", SPN_DEP_KIND_BUILD } } },
        { .name = "T", .deps = { { "U" }, { "G", SPN_DEP_KIND_BUILD } } },
        { .name = "U" },
        { .name = "G" },
      },
    },
    .expect = {
      { "R" },
      { "R", WALK_METAPROGRAM, .host = true },
      { "T", WALK_METAPROGRAM },
      { "U", WALK_METAPROGRAM },
      { "G", WALK_METAPROGRAM },
    },
  },
  {
    .name = "dual_membership_gets_two_units",
    .graph = {
      .pkgs = {
        { .name = "R", .scripts = true, .deps = { { "A" }, { "A", SPN_DEP_KIND_BUILD } } },
        { .name = "A" },
      },
    },
    .expect = {
      { "R" },
      { "R", WALK_METAPROGRAM, .host = true },
      { "A" },
      { "A", WALK_METAPROGRAM },
    },
  },
  // A script-haver pulled in as a build dep is a member of the metaprogram
  // build, not a host: its own package deps must build there too
  {
    .name = "script_haver_as_build_dep_is_a_member",
    .graph = {
      .pkgs = {
        { .name = "R", .scripts = true, .deps = { { "B", SPN_DEP_KIND_BUILD } } },
        { .name = "B", .scripts = true, .deps = { { "U" } } },
        { .name = "U" },
      },
    },
    .expect = {
      { "R" },
      { "R", WALK_METAPROGRAM, .host = true },
      { "B", WALK_METAPROGRAM },
      { "U", WALK_METAPROGRAM },
    },
  },
};

sp_test_each(unit, walk, walk_test_t, tests) {
  sp_mem_t mem = sp_test_arena(t);
  spn_session_t* s = build_session(mem, &it->graph);

  spn_err_union_t err = spn_units_add_packages(s);
  sp_must_eq(t, SPN_OK, err.kind);

  u32 expected = 0;
  sp_carr_detect_len(it->expect, expected, it->expect[expected].pkg);
  sp_must_eq(t, expected, sp_om_size(s->units.packages));

  sp_for(et, expected) {
    walk_unit_t* expect = &it->expect[et];
    sp_test_kv_c(t, "pkg", expect->pkg);

    spn_pkg_id_t id = find_pkg_id(s, &it->graph, expect->pkg);
    spn_build_unit_t* build = expect->build == WALK_TARGET ? s->units.target : s->units.metaprogram;
    spn_pkg_unit_t* unit = spn_session_find_pkg_unit(s, build, id);
    sp_must(t, unit != SP_NULLPTR);
    sp_expect_eq(t, expect->host, spn_pkg_unit_is_script_host(unit));

    spn_loaded_pkg_t* loaded = sp_ht_getp(s->packages, id);
    if (sp_da_empty(loaded->configure.source) && sp_da_empty(loaded->build.source)) {
      sp_expect(t, unit->metaprogram == SP_NULLPTR);
    }
    else {
      sp_must(t, unit->metaprogram != SP_NULLPTR);
      sp_expect(t, unit->metaprogram == spn_session_find_pkg_unit(s, s->units.metaprogram, id));
      sp_expect(t, unit->metaprogram->metaprogram == unit->metaprogram);
    }
  }

  return SP_OK;
}
