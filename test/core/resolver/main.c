#define SP_IMPLEMENTATION
#include "sp.h"

#include "resolver.h"
#include "fixture.h"

#include "ctx/types.h"
#include "index/types.h"

#include "error/types.h"
#include "event/event.h"
#include "sp_fuzz.h"
#include "codegen/lower.h"
#include "index/cache.h"
#include "index/json.h"
#include "toml/loader.h"
#include "intern/intern.h"
#include "pkg/id.h"
#include "resolve/dump.h"
#include "resolve/resolve.h"
#include "resolve/types.h"
#include "semver/compare.h"
#include "semver/convert.h"
#include "spn.h"

spn_ctx_t spn;

static sp_mem_arena_t* arena;

sp_test_suite(resolver, .serial = true);

s32 main(s32 argc, const c8** argv) {
  arena = sp_mem_arena_new(sp_mem_os_new());

  spn.intern = sp_intern_new(sp_mem_os_new());
  sp_fuzz_seed_init();

  s32 result = sp_test_main(argc, argv, SP_NULLPTR);

  sp_mem_arena_destroy(arena);
  return result;
}


///////////
// STATE //
///////////
typedef struct {
  sp_str_ht(spn_index_pkg_t) cache;
} state_t;

static state_t state;

///////////
// MOCKS //
///////////
void spn_index_cache_init(spn_index_cache_t* cache, sp_mem_t mem, sp_intern_t* intern, spn_index_arr_t* indexes) {

}

spn_index_pkg_t* spn_index_cache_get_package(spn_index_cache_t* cache, spn_pkg_name_t id) {
  sp_str_t qualified = spn_pkg_name_to_qualified(id);
  return sp_str_ht_get(state.cache, qualified);
}

spn_index_release_t* spn_index_cache_get_release(spn_index_cache_t* cache, spn_pkg_name_t id, spn_semver_t version) {
  return SP_NULLPTR;
}


static const c8* root_qualified = "test/root";


static resolve_result_t execute_fixture(const fixture_t* fixture, const spn_pkg_info_t* root, sp_intern_t* intern) {
  sp_mem_t mem = sp_mem_arena_as_allocator(arena);

  spn_event_buffer_t* events = spn_event_buffer_new(sp_mem_os_new());
  spn_index_cache_t cache = sp_zero;
  spn_pkg_registry_t registry = sp_zero;
  sp_ht_init(mem, registry);

  sp_ht_insert(registry, spn_pkg_id(intern, root->qualified), ((spn_registry_pkg_t) {
    .source = SPN_PKG_SOURCE_ROOT,
    .info = (spn_pkg_info_t*)root,
  }));

  spn_resolver_t resolver = sp_zero;
  spn_resolver_init(&resolver, mem, intern, &cache, &registry, events, (spn_profile_info_t) {
    .linkage = fixture->linkage,
    .os = fixture->facts.os,
    .arch = fixture->facts.arch,
    .abi = fixture->facts.abi,
    .mode = fixture->facts.mode,
    .opt = fixture->facts.opt,
    .sanitizers = fixture->facts.sanitizers,
  }, root->config, fixture->budget);

  resolve_result_t result = sp_zero_s(resolve_result_t);
  result.intern = intern;
  spn_resolve_query_init(mem, &result.query);
  spn_resolve_query_add(&result.query, (spn_requested_dep_t) {
    .qualified = sp_str_view(root_qualified),
    .source = SPN_PKG_SOURCE_ROOT,
  });

  result.err = spn_resolve_from_solver(&resolver, &result.query).kind;
  result.events = spn_event_buffer_drain(mem, events);
  return result;
}

static bool pushed_event(resolve_result_t* result, spn_build_event_kind_t kind) {
  sp_da_for(result->events, it) {
    if (result->events[it].kind == kind) {
      return true;
    }
  }
  return false;
}

static spn_build_event_t* find_event(resolve_result_t* result, spn_build_event_kind_t kind) {
  sp_da_for(result->events, it) {
    if (result->events[it].kind == kind) {
      return &result->events[it];
    }
  }
  return SP_NULLPTR;
}

static sp_err_t assert_unsat_event(sp_test_t* t, sp_mem_t mem, resolve_result_t* result, expect_unsat_t unsat) {
  spn_build_event_t* event = find_event(result, SPN_EVENT_ERR_UNSATISFIABLE_VERSION);
  sp_must(t, event != SP_NULLPTR);

  sp_str_t qualified = spn_pkg_canonicalize_pair(sp_str_view(unsat.namespace), sp_str_view(unsat.name));
  sp_must(t, sp_str_equal(event->unsatisfiable.request.qualified, qualified));

  if (unsat.requester) {
    sp_must(t, sp_str_equal(event->unsatisfiable.requester, sp_str_view(unsat.requester)));
  }

  if (unsat.selected) {
    sp_must(t, event->unsatisfiable.conflict);
    sp_must(t, sp_str_equal(spn_semver_to_str(mem, event->unsatisfiable.selected), sp_str_view(unsat.selected)));
  }
  else {
    sp_must(t, !(event->unsatisfiable.conflict));
  }
  return SP_OK;
}


static sp_str_t fx_dir(sp_test_t* t, sp_mem_t mem) {
  sp_str_t name = sp_test_get_name(t);
  sp_str_t prefix = sp_str_lit("resolver.");
  sp_str_t sub = sp_str_replace_c8(mem, sp_str_sub(name, (s32)prefix.len, (s32)(name.len - prefix.len)), '.', '/');
  return test_repo_path(mem, sp_fs_join_path(mem, sp_str_lit("test/core/resolver/fixtures"), sub));
}

static s32 fx_sort_entries(const void* a, const void* b) {
  return sp_str_compare_alphabetical(((const sp_fs_entry_t*)a)->name, ((const sp_fs_entry_t*)b)->name);
}

static sp_err_t fx_load_index(sp_test_t* t, sp_mem_t mem, sp_str_t dir, sp_da(sp_str_t)* names) {
  sp_da(sp_fs_entry_t) namespaces = sp_fs_collect(mem, dir);
  sp_da_sort(namespaces, fx_sort_entries);
  sp_da_for(namespaces, it) {
    if (namespaces[it].kind != SP_FS_KIND_DIR) continue;

    sp_da(sp_fs_entry_t) files = sp_fs_collect(mem, namespaces[it].path);
    sp_da_sort(files, fx_sort_entries);
    sp_da_for(files, jt) {
      if (!sp_str_ends_with(files[jt].name, sp_str_lit(".jsonl"))) continue;

      sp_str_t blob = sp_zero;
      sp_must_ok(t, sp_io_read_file(mem, files[jt].path, &blob));

      spn_pkg_name_t id = {
        .namespace = namespaces[it].name,
        .name = sp_str_strip_right(files[jt].name, sp_str_lit(".jsonl")),
      };
      spn_index_pkg_t pkg = sp_zero;
      sp_must_eq(t, SPN_OK, spn_index_parse_pkg(mem, id, blob, &pkg));

      sp_str_t qualified = spn_pkg_name_to_qualified(id);
      sp_str_ht_insert(state.cache, qualified, pkg);
      sp_da_push(*names, qualified);
    }
  }
  return SP_OK;
}

static sp_err_t fx_load_root(sp_test_t* t, sp_mem_t mem, sp_str_t dir, spn_pkg_info_t* root) {
  sp_str_t manifest = sp_fs_join_path(mem, dir, sp_str_lit("spn.toml"));
  sp_must(t, sp_fs_is_target_file(manifest));

  spn_toml_loader_t loader = sp_zero;
  spn_toml_loader_init(&loader, mem, spn.intern);
  sp_must_eq(t, SPN_OK, spn_codegen_load_pkg(&loader, manifest, root));
  sp_must_eq(t, 0, sp_da_size(loader.issues));
  return SP_OK;
}

sp_err_t run_fixture(sp_test_t* t, const fixture_t* fixture) {
  sp_mem_t mem = sp_mem_arena_as_allocator(arena);
  state = sp_zero_s(state_t);
  sp_str_ht_init(mem, state.cache);

  sp_str_t dir = fx_dir(t, mem);
  sp_da(sp_str_t) names = sp_da_new(mem, sp_str_t);
  sp_da_push(names, sp_str_lit(""));
  sp_da_push(names, sp_str_view(root_qualified));
  if (fx_load_index(t, mem, dir, &names)) return SP_ERR;

  spn_pkg_info_t root = sp_zero;
  if (fx_load_root(t, mem, dir, &root)) return SP_ERR;
  sp_da_for(root.deps, it) {
    sp_da_push(names, root.deps[it].qualified);
  }

  sp_fuzz_prng_t prng = sp_fuzz_stream(names);

  sp_intern_t* intern = sp_intern_new(sp_mem_os_new());
  resolve_result_t canonical = execute_fixture(fixture, &root, intern);
  sp_must_eq(t, canonical.err, fixture->err);

  if (fixture->event) {
    sp_must(t, pushed_event(&canonical, fixture->event));
  }

  if (fixture->unsat.name) {
    if (assert_unsat_event(t, mem, &canonical, fixture->unsat)) return SP_ERR;
  }

  sp_str_t canonical_dump = sp_zero;
  if (canonical.err == SPN_OK) {
    spn_cg_resolve_t dump = spn_resolve_dump(mem, canonical.intern, &canonical.query);
    canonical_dump = spn_resolve_write(mem, &dump);
    sp_str_t golden = sp_fmt(mem, "goldens/{}.json", sp_fmt_str(sp_test_get_name(t))).value;
    sp_expect_golden(t, golden, canonical_dump);
  }

  // Picks may never depend on intern state: resolve against perturbed interns
  // and require structurally identical results every round
  for (u32 round = 0; round < 8; round++) {
    resolve_result_t shaken = execute_fixture(fixture, &root, sp_fuzz_perturbed_intern(&prng, names));
    sp_must_eq(t, shaken.err, canonical.err);
    if (fixture->event) {
      sp_must(t, pushed_event(&shaken, fixture->event));
    }
    if (fixture->unsat.name) {
      if (assert_unsat_event(t, mem, &shaken, fixture->unsat)) return SP_ERR;
    }
    if (canonical.err == SPN_OK) {
      sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
      spn_cg_resolve_t dump = spn_resolve_dump(scratch.mem, shaken.intern, &shaken.query);
      sp_str_t shaken_dump = sp_str_copy(mem, spn_resolve_write(scratch.mem, &dump));
      sp_mem_end_scratch(scratch);
      sp_must_str_eq(t, shaken_dump, canonical_dump);
    }
  }

  return SP_OK;
}


static const fixture_t graph_cases [] = {
  {
    .name = "none_resolves",
  },

  {
    .name = "linear_resolves",
  },

  {
    .name = "linear_missing",
    .err = SPN_ERR_PKG_UNKNOWN,
    .event = SPN_EVENT_ERR_UNKNOWN_PKG,
  },

  {
    .name = "diamond_compatible",
  },

  {
    .name = "diamond_disjoint",
    .err = SPN_ERR_PKG_NO_MATCH,
    .event = SPN_EVENT_ERR_UNSATISFIABLE_VERSION,
  },

  {
    .name = "diamond_missing_renderer",
    .err = SPN_ERR_PKG_UNKNOWN,
    .event = SPN_EVENT_ERR_UNKNOWN_PKG,
  },

  {
    .name = "diamond_missing_math",
    .err = SPN_ERR_PKG_UNKNOWN,
    .event = SPN_EVENT_ERR_UNKNOWN_PKG,
  },

  {
    .name = "cycle_direct",
    .err = SPN_ERR_DEP_CYCLE,
    .event = SPN_EVENT_ERR_CIRCULAR_DEP,
  },

  {
    .name = "cycle_indirect",
    .err = SPN_ERR_DEP_CYCLE,
    .event = SPN_EVENT_ERR_CIRCULAR_DEP,
  },

  {
    .name = "backtrack_simple",
  },

  {
    .name = "budget_exhausted_fails",
    .budget = 1,
    .err = SPN_ERR_RESOLVE_TOO_COMPLEX,
    .event = SPN_EVENT_ERR_RESOLUTION_TOO_COMPLEX,
  },

  {
    .name = "backtrack_transitive_missing",
  },

  {
    .name = "backtrack_divergent_deps",
  },

  {
    .name = "root_transitive_conflict",
    .err = SPN_ERR_PKG_NO_MATCH,
    .event = SPN_EVENT_ERR_UNSATISFIABLE_VERSION,
    .unsat = {
      .namespace = "spn",
      .name = "math",
      .requester = "test/root",
      .selected = "2.0.0",
    },
  },

  {
    .name = "version_no_match",
    .err = SPN_ERR_PKG_NO_MATCH,
    .event = SPN_EVENT_ERR_UNSATISFIABLE_VERSION,
    .unsat = { .namespace = "spn", .name = "math", .requester = "test/root" },
  },

  {
    .name = "conflict_reports_selected",
    .err = SPN_ERR_PKG_NO_MATCH,
    .event = SPN_EVENT_ERR_UNSATISFIABLE_VERSION,
    .unsat = {
      .namespace = "spn",
      .name = "a",
      .requester = "spn/b",
      .selected = "1.0.0",
    },
  },
};

sp_test_each(resolver, graph, fixture_t, graph_cases) {
  return run_fixture(t, it);
}


static const fixture_t metadata_cases [] = {
  {
    .name = "yanked_release_skipped",
  },

  {
    .name = "yanked_only_candidate_fails",
    .err = SPN_ERR_PKG_NO_MATCH,
    .event = SPN_EVENT_ERR_UNSATISFIABLE_VERSION,
    .unsat = { .namespace = "spn", .name = "a", .requester = "test/root" },
  },

  {
    .name = "index_invalid_range",
    .err = SPN_ERR_DEP_MANIFEST,
    .event = SPN_EVENT_ERR_MANIFEST,
  },
};

sp_test_each(resolver, metadata, fixture_t, metadata_cases) {
  return run_fixture(t, it);
}


static const fixture_t gates_cases [] = {
  {
    .name = "index_dep_fact_gate",
    .facts = { .os = SPN_OS_LINUX, .arch = SPN_ARCH_X64, .abi = SPN_ABI_GNU },
  },
  {
    .name = "index_dep_negated_gate",
    .facts = { .os = SPN_OS_LINUX, .arch = SPN_ARCH_X64, .abi = SPN_ABI_GNU },
  },

  {
    .name = "index_dep_option_gate",
  },

  {
    .name = "root_dep_gate",
    .facts = { .os = SPN_OS_LINUX, .arch = SPN_ARCH_X64, .abi = SPN_ABI_GNU },
  },
};

sp_test_each(resolver, gates, fixture_t, gates_cases) {
  return run_fixture(t, it);
}


static const fixture_t units_cases [] = {
  {
    .name = "build_dep_root_conflict",
  },

  {
    .name = "build_dep_transitive_conflict",
  },

  {
    .name = "test_dep_root_conflict",
  },

  {
    .name = "transitive_test_dep_pruned",
  },

  {
    .name = "build_dep_compatible_unifies",
  },

  {
    .name = "preference_prefers_unified",
  },

  {
    .name = "build_dep_never_constrains_root",
  },

  {
    .name = "convergence_forces_older_sibling",
  },

  {
    .name = "tiebreak_takes_earliest_admissible",
  },

  {
    .name = "tiebreak_higher_priority_pins_loser_splits",
  },

  {
    .name = "shared_lib_public_conflict_fails",
    .err = SPN_ERR_PKG_NO_MATCH,
    .event = SPN_EVENT_ERR_UNSATISFIABLE_VERSION,
  },

  {
    .name = "shared_lib_transitive_conflict_fails",
    .err = SPN_ERR_PKG_NO_MATCH,
    .event = SPN_EVENT_ERR_UNSATISFIABLE_VERSION,
  },

  {
    .name = "shared_lib_private_diverges",
  },

  {
    .name = "private_compatible_unifies",
  },

  {
    .name = "shared_lib_private_transitive_diverges",
  },

  {
    .name = "static_lib_conflict_fails",
    .err = SPN_ERR_PKG_NO_MATCH,
    .event = SPN_EVENT_ERR_UNSATISFIABLE_VERSION,
  },

  {
    .name = "static_lib_private_conflict_fails",
    .err = SPN_ERR_PKG_NO_MATCH,
    .event = SPN_EVENT_ERR_UNSATISFIABLE_VERSION,
  },

  {
    .name = "private_static_default_conflicts",
    .err = SPN_ERR_PKG_NO_MATCH,
    .event = SPN_EVENT_ERR_UNSATISFIABLE_VERSION,
  },

  {
    .name = "config_shared_private_diverges",
  },

  {
    .name = "shared_lib_consumers_unify",
  },

  {
    .name = "shared_lib_consumer_disjoint_fails",
    .err = SPN_ERR_PKG_NO_MATCH,
    .event = SPN_EVENT_ERR_UNSATISFIABLE_VERSION,
  },

  {
    .name = "shared_lib_private_dynamic_dup_fails",
    .err = SPN_ERR_DYNAMIC_DUPLICATE,
    .event = SPN_EVENT_ERR_DYNAMIC_DUPLICATE,
  },

  {
    .name = "config_shared_dynamic_dup_fails",
    .err = SPN_ERR_DYNAMIC_DUPLICATE,
    .event = SPN_EVENT_ERR_DYNAMIC_DUPLICATE,
  },

  {
    .name = "build_dep_cycle_fails",
    .err = SPN_ERR_UNIT_CYCLE,
    .event = SPN_EVENT_ERR_UNIT_CYCLE,
  },

  {
    .name = "build_dep_bootstrap",
  },

  {
    .name = "backtrack_failure_not_sticky",
    .err = SPN_ERR_PKG_NO_MATCH,
    .event = SPN_EVENT_ERR_UNSATISFIABLE_VERSION,
  },

  {
    .name = "backtrack_releases_subtree_picks",
  },

  {
    .name = "backtrack_orphan_not_committed",
  },

  {
    .name = "divergent_dep_splits_instance",
  },

  {
    .name = "build_dep_disjoint_tools_diverge",
  },

  {
    .name = "private_scopes_per_instance",
  },

  {
    .name = "sibling_tools_unify",
  },

  {
    .name = "build_dep_missing_still_fails",
    .err = SPN_ERR_PKG_UNKNOWN,
    .event = SPN_EVENT_ERR_UNKNOWN_PKG,
  },
};

sp_test_each(resolver, units, fixture_t, units_cases) {
  return run_fixture(t, it);
}


static const fixture_t convergence_cases [] = {
  {
    .name = "split_instances_reconverge",
  },

  {
    .name = "split_propagates_through_middle",
  },

  {
    .name = "pin_walk_lexicographic_subset",
  },

  {
    .name = "convergence_forces_older_transitive",
  },

  {
    .name = "nested_tool_splits_only_inner",
  },

  {
    .name = "private_inside_private_diverges",
  },

  {
    .name = "private_inside_private_dynamic_dup_fails",
    .err = SPN_ERR_DYNAMIC_DUPLICATE,
    .event = SPN_EVENT_ERR_DYNAMIC_DUPLICATE,
  },

  {
    .name = "build_dep_inside_private_diverges",
  },

  {
    .name = "boundary_diamond_three_instances",
  },

  {
    .name = "bootstrap_with_divergent_sibling",
  },

  {
    .name = "same_version_dynamic_dup_fails",
    .err = SPN_ERR_DYNAMIC_DUPLICATE,
    .event = SPN_EVENT_ERR_DYNAMIC_DUPLICATE,
  },

  {
    .name = "private_groups_converge_on_earliest",
  },

  {
    .name = "test_dep_transitive_diverges",
  },

  {
    .name = "admissible_pick_cycle_fails",
    .err = SPN_ERR_UNIT_CYCLE,
    .event = SPN_EVENT_ERR_UNIT_CYCLE,
  },

  {
    .name = "shared_lib_diverges_across_process",
  },

  {
    .name = "converged_lib_single_tool_group",
  },

  {
    .name = "root_private_dep_conflict_fails",
    .err = SPN_ERR_PKG_NO_MATCH,
    .event = SPN_EVENT_ERR_UNSATISFIABLE_VERSION,
  },

  {
    .name = "determinism_many_ties",
  },

  {
    .name = "sibling_order_greedy",
  },

  {
    .name = "sibling_order_reversed",
  },

  {
    .name = "avoidable_dynamic_dup",
  },
};

sp_test_each(resolver, convergence, fixture_t, convergence_cases) {
  return run_fixture(t, it);
}

sp_test(resolver, transitive_sibling_order) {
  return sp_test_skip(t, "");
  return run_fixture(t, &(fixture_t) { 0 });
}


sp_test(resolver, cut3_top_resolve_bool_edges_conflict) {
  return sp_test_skip(t, "worlds cut 3");
  return run_fixture(t, &(fixture_t) {
    .err = SPN_ERROR,
    .event = SPN_EVENT_ERR_UNSATISFIABLE_VERSION,
  });
}

sp_test(resolver, cut3_phantom_edge_over_pins) {
  return sp_test_skip(t, "worlds cut 3");
  return run_fixture(t, &(fixture_t) { 0 });
}
