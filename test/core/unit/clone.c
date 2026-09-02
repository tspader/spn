#include "unit.h"

#include "target/mutate.h"

typedef struct {
  const c8* name;
  unit_graph_test_t graph;
} clone_test_t;

static clone_test_t tests [] = {
  {
    .name = "apply_isolates_units",
    .graph = {
      .pkgs = {
        { .name = "R", .scripts = true, .libs = { { .name = "L", STATIC_ONLY, .source = { "a.c" } } } },
      },
    },
  },
};

sp_test_each(unit, clone, clone_test_t, tests, .setup = spn_test_ctx_setup) {
  sp_mem_t mem = sp_test_arena(t);
  spn_session_t* s = build_session(mem, &it->graph);
  spn_pkg_id_t id = find_pkg_id(s, &it->graph, "R");
  spn_pkg_info_t* loaded = sp_ht_getp(s->packages, id)->info;
  spn_target_info_t* loaded_lib = sp_str_om_get(loaded->libs, sp_str_lit("L"));
  sp_must(t, loaded_lib);
  spn_target_info_init(mem, loaded_lib);

  sp_da_init(mem, loaded->define);
  sp_da_init(mem, loaded->include);
  sp_da_init(mem, loaded->publish.copy);
  sp_da_init(mem, loaded->gated.define);
  sp_da_init(mem, loaded->gated.system_deps);
  sp_da_init(mem, loaded->gated.include);
  sp_da_init(mem, loaded->gated.frameworks);
  sp_da_init(mem, loaded->gated.publish.copy);
  sp_da_push(loaded->gated.define, ((spn_gated_str_t) { .value = sp_str_lit("A") }));
  sp_da_push(loaded->gated.system_deps, ((spn_gated_str_t) { .value = sp_str_lit("A") }));
  sp_da_push(loaded->gated.frameworks, ((spn_gated_str_t) { .value = sp_str_lit("A") }));
  sp_da_push(loaded->gated.include, ((spn_gated_path_t) { .path = sp_str_lit("A"), .tree = SPN_TREE_SOURCE }));
  sp_da_push(loaded->gated.publish.copy, ((spn_publish_copy_t) { .from = sp_str_lit("source/A"), .to = sp_str_lit("include") }));
  sp_da_push(loaded_lib->gated.define, ((spn_gated_str_t) { .value = sp_str_lit("A") }));
  sp_da_push(loaded_lib->gated.frameworks, ((spn_gated_str_t) { .value = sp_str_lit("A") }));

  sp_must_eq(t, SPN_OK, spn_units_add_packages(s));

  spn_build_unit_t* builds [] = { s->units.target, s->units.metaprogram };
  sp_carr_for(builds, bt) {
    spn_pkg_unit_t* unit = spn_session_find_pkg_unit(s, builds[bt], id);
    sp_must(t, unit);
    sp_expect_eq(t, (u32)1, (u32)sp_da_size(unit->info->define));
    sp_expect_eq(t, (u32)1, (u32)sp_da_size(unit->info->system_deps));
    sp_expect_eq(t, (u32)1, (u32)sp_da_size(unit->info->include));
    sp_expect_eq(t, (u32)1, (u32)sp_da_size(unit->info->macos.frameworks));
    sp_expect_eq(t, (u32)1, (u32)sp_da_size(unit->info->publish.copy));
    spn_target_info_t* lib = sp_str_om_get(unit->info->libs, sp_str_lit("L"));
    sp_must(t, lib);
    sp_expect_eq(t, (u32)1, (u32)sp_da_size(lib->define));
    sp_expect_eq(t, (u32)1, (u32)sp_da_size(lib->macos.frameworks));
  }

  sp_expect_eq(t, (u32)0, (u32)sp_da_size(loaded->define));
  sp_expect_eq(t, (u32)0, (u32)sp_da_size(loaded->system_deps));
  sp_expect_eq(t, (u32)0, (u32)sp_da_size(loaded->include));
  sp_expect_eq(t, (u32)0, (u32)sp_da_size(loaded->macos.frameworks));
  sp_expect_eq(t, (u32)0, (u32)sp_da_size(loaded->publish.copy));
  sp_expect_eq(t, (u32)0, (u32)sp_da_size(loaded_lib->define));
  sp_expect_eq(t, (u32)0, (u32)sp_da_size(loaded_lib->macos.frameworks));
  return SP_OK;
}
