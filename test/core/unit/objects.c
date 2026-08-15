#include "unit.h"

#include "paths/paths.h"
#include "target/mutate.h"

#define OBJECTS_TEST_MAX_SOURCE 4

typedef struct {
  const c8* path;
  spn_tree_t tree;
} objects_source_t;

typedef struct {
  const c8* objects [OBJECTS_TEST_MAX_SOURCE];
} objects_expect_t;

typedef struct {
  const c8* name;
  objects_source_t source [OBJECTS_TEST_MAX_SOURCE];
  objects_expect_t expect;
} objects_test_t;

static const objects_test_t tests [] = {
  {
    .name = "one_object_per_file",
    .source = { { "a.c", SPN_TREE_SOURCE }, { "b.c", SPN_TREE_SOURCE } },
    .expect = { .objects = { "object/exe/app/manifest/a.c.o", "object/exe/app/manifest/b.c.o" } },
  },
  {
    .name = "shared_tree_dedups_both_declarations",
    .source = { { "a.c", SPN_TREE_SOURCE }, { "a.c", SPN_TREE_MANIFEST } },
    .expect = { .objects = { "object/exe/app/manifest/a.c.o" } },
  },
  {
    .name = "outside_tree_files_keep_a_root_label",
    .source = { { "manifest/x.c", SPN_TREE_SOURCE }, { "/manifest/x.c", SPN_TREE_SOURCE } },
    .expect = { .objects = { "object/exe/app/manifest/manifest/x.c.o", "object/exe/app/absolute/manifest/x.c.o" } },
  },
};

sp_test_each(unit_objects, create, objects_test_t, tests, .setup = spn_test_ctx_setup) {
  sp_mem_t mem = sp_test_arena(t);
  unit_graph_test_t graph = { .pkgs = { { .name = "A" } } };
  spn_session_t* s = build_session(mem, &graph);

  spn_pkg_id_t id = find_pkg_id(s, &graph, "A");
  spn_loaded_pkg_t* loaded = sp_ht_getp(s->packages, id);

  spn_target_info_t app = { .name = sp_str_lit("app"), .kind = SPN_TARGET_KIND_EXE };
  spn_target_info_init(mem, &app);
  sp_carr_for(it->source, st) {
    if (!it->source[st].path) {
      break;
    }
    sp_da_push(app.source, spn_tree_path(mem, &spn.roots, loaded->roots, it->source[st].tree, sp_cstr_as_str(it->source[st].path)));
  }
  sp_str_om_insert(s->pkg->exes, app.name, app);

  sp_must_eq(t, SPN_OK, spn_units_add_packages(s));
  sp_must_eq(t, SPN_OK, spn_units_add_targets(s, SPN_UNIT_SCOPE_TARGET));

  spn_pkg_unit_t* pkg = spn_session_find_pkg_unit(s, s->units.target, id);
  sp_must(t, pkg != SP_NULLPTR);
  spn_target_unit_t* target = spn_session_find_target_in_pkg(s, pkg, sp_str_lit("app"), SPN_TARGET_KIND_EXE);
  sp_must(t, target != SP_NULLPTR);

  u32 count = 0;
  sp_carr_detect_len(it->expect.objects, count, it->expect.objects[count]);
  sp_must_eq(t, count, (u32)sp_da_size(target->objects));
  sp_for(ot, count) {
    sp_test_kv_c(t, "object", it->expect.objects[ot]);
    sp_expect(t, sp_str_ends_with(target->objects[ot]->paths.object.sub, sp_str_view(it->expect.objects[ot])));
  }

  return SP_OK;
}
