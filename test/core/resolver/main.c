#define SP_IMPLEMENTATION
#include "sp.h"

#include "resolver.h"
#include "ctx.h"

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

sp_test_suite(resolver, .serial = true);

s32 main(s32 argc, const c8** argv) {
  ctx_t* ctx = ctx_get();
  ctx_init(ctx_get());

  spn.intern = sp_intern_new(sp_mem_os_new());
  sp_fuzz_seed_init();

  s32 result = sp_test_main(argc, argv, SP_NULLPTR);

  ctx_deinit(ctx_get());
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
  sp_mem_t mem = sp_mem_arena_as_allocator(ctx_get()->arena);

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
  sp_mem_t mem = sp_mem_arena_as_allocator(ctx_get()->arena);
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

  // audio has 3 versions in ^1.0.0; 1.2.0 requires missing "physics", 1.1.0 and 1.0.0 are fine.
  // A correct resolver should pick audio 1.1.0 (newest valid).
  // The naive resolver uses 1.2.0's deps (latest in range) and fails on missing physics.
  {
    .name = "backtrack_simple",
  },

  // The budget bounds search, never semantics: exhausting it is a hard error,
  // never a fallback split, so identical inputs always produce the same answer
  // or the same error. Same graph as backtrack_simple; a budget of one pop dies
  // before the resolvable audio 1.1.0 is ever tried.
  {
    .name = "budget_exhausted_fails",
    .budget = 1,
    .err = SPN_ERR_RESOLVE_TOO_COMPLEX,
    .event = SPN_EVENT_ERR_RESOLUTION_TOO_COMPLEX,
  },

  // audio has 2 versions within ^1.0.0:
  //   1.0.0 is fine (requires math ^1.0.0)
  //   1.1.0 requires missing "physics"
  // A correct resolver should backtrack from 1.1.0 and pick audio 1.0.0.
  // The naive resolver uses 1.1.0's deps (latest) and fails on missing physics.
  {
    .name = "backtrack_transitive_missing",
  },

  // renderer has 2 versions with different deps:
  //   1.0.0 depends on math ^1.0.0
  //   2.0.0 depends on math ^2.0.0
  // Root requires renderer ^1.0.0 (should pick 1.0.0).
  // Naive resolver uses 2.0.0's deps (latest) for the transitive walk,
  // so it adds math ^2.0.0 constraint instead of ^1.0.0.
  // math only has 1.0.0, so naive resolver either picks wrong version or fails.
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

  // A conflict is reported as a conflict: the failure names the version the
  // scope already selected and who issued the losing request, never "no
  // version satisfies" when satisfying versions exist
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
  // A yanked release is invisible to free selection: the newest in-range
  // version is skipped for the newest un-yanked one
  {
    .name = "yanked_release_skipped",
  },

  // A range only a yanked release satisfies is unsatisfiable, reported as
  // no-version-in-range rather than a conflict
  {
    .name = "yanked_only_candidate_fails",
    .err = SPN_ERR_PKG_NO_MATCH,
    .event = SPN_EVENT_ERR_UNSATISFIABLE_VERSION,
    .unsat = { .namespace = "spn", .name = "a", .requester = "test/root" },
  },

  // An index dep whose version range fails to parse is a manifest error naming
  // the release, never a silent skip
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
  // A fact gate on an index dep evaluates at resolve time: the false edge is
  // never resolved (its package exists nowhere), the true edge is
  {
    .name = "index_dep_fact_gate",
    .facts = { .os = SPN_OS_LINUX, .arch = SPN_ARCH_X64, .abi = SPN_ABI_GNU },
  },

  // The { not = v } form gates with opposite polarity: os != windows holds on
  // this profile, os != linux does not
  {
    .name = "index_dep_negated_gate",
    .facts = { .os = SPN_OS_LINUX, .arch = SPN_ARCH_X64, .abi = SPN_ABI_GNU },
  },

  // The gate env includes the release's own declared options at their
  // resolved defaults: x defaults true and admits its edge, y is an
  // undefaulted bool (false) and cuts the edge to the missing package
  {
    .name = "index_dep_option_gate",
  },

  // Root manifest deps gate through the local-package path exactly like index
  // deps gate through candidates
  {
    .name = "root_dep_gate",
    .facts = { .os = SPN_OS_LINUX, .arch = SPN_ARCH_X64, .abi = SPN_ABI_GNU },
  },
};

sp_test_each(resolver, gates, fixture_t, gates_cases) {
  return run_fixture(t, it);
}


static const fixture_t units_cases [] = {
  // A build dep never links into the product, so it may resolve to a version
  // that conflicts with the linked one. The build dep roots its own unit.
  {
    .name = "build_dep_root_conflict",
  },

  // Same conflict, but the build dep edge is inside a dependency rather than
  // on the root
  {
    .name = "build_dep_transitive_conflict",
  },

  // Root test deps build into test executables, not the product, so they may
  // conflict with linked deps just like build deps
  {
    .name = "test_dep_root_conflict",
  },

  // A dependency's test deps never build at all in a consumer's graph; they
  // must be pruned rather than resolved into a conflict
  {
    .name = "transitive_test_dep_pruned",
  },

  // Compatible ranges across units must unify onto one instance; splitting
  // resolution is never license to build a package twice unnecessarily
  {
    .name = "build_dep_compatible_unifies",
  },

  // The build unit's range admits 2.0.0, but the root unit already picked
  // 1.9.0; the solver must prefer the already-picked version over the newest
  {
    .name = "preference_prefers_unified",
  },

  // The tool's pin admits a version the root's range also admits, so pooling
  // constraints would downgrade the root to 1.0.0 to share one instance. The
  // root's pick is sovereign; the tool pays with its own copy instead
  {
    .name = "build_dep_never_constrains_root",
  },

  // Convergence is a constraint, not a preference: the newest bar needs a foo
  // the root's pick excludes, so the tool's unit must fall back to the older
  // bar that shares the root's foo rather than split a second foo
  {
    .name = "convergence_forces_older_sibling",
  },

  // A group inheriting two decided versions of one name takes the earliest
  // admissible priority, not the newest version: opt's range admits both the
  // root's foo 1.9.0 and gen's split foo 2.0.0, and the root decided first.
  // Today's preference draws candidates in hash iteration order, so this pin
  // flickers with intern state — the order dependence is the defect it pins.
  {
    .name = "tiebreak_takes_earliest_admissible",
  },

  // Mutually exclusive inherited picks: use can hold the root's c 1.0.0 or mk's
  // b 2.0.0 (which needs c 2.0.0), never both. The root's pick has higher
  // priority, so c pins and b is the loser that splits — regardless of the
  // order use's own requests discover the names
  {
    .name = "tiebreak_higher_priority_pins_loser_splits",
  },

  // Deps are public by default: their types may appear in the package's API, so
  // they resolve in the consumer's scope and a conflict stays an error even
  // across a shared (dynamic) boundary
  {
    .name = "shared_lib_public_conflict_fails",
    .err = SPN_ERR_PKG_NO_MATCH,
    .event = SPN_EVENT_ERR_UNSATISFIABLE_VERSION,
  },

  // The public default holds when the shared lib is discovered transitively,
  // not just on a direct dep of the root
  {
    .name = "shared_lib_transitive_conflict_fails",
    .err = SPN_ERR_PKG_NO_MATCH,
    .event = SPN_EVENT_ERR_UNSATISFIABLE_VERSION,
  },

  // A dep declared private is an implementation detail: behind a shared
  // boundary its subtree resolves in gfx's own scope and may diverge from the
  // consumer's picks. gfx.so carries its own foo 1.0.0 copy, symbols hidden.
  {
    .name = "shared_lib_private_diverges",
  },

  // Privacy alone never costs a duplicate: gfx's private range admits foo
  // 2.0.0, which an isolated solve would take, but the root already decided
  // 1.9.0 and the private unit must converge onto that instance
  {
    .name = "private_compatible_unifies",
  },

  // Privacy inherits down the private edge: bar is private to gfx, so bar's
  // *public* dep baz is still private to gfx. baz unifies with bar inside gfx's
  // scope, not with the root's baz.
  {
    .name = "shared_lib_private_transitive_diverges",
  },

  // The static twin of shared_lib_public_conflict_fails: identical topology,
  // identical outcome
  {
    .name = "static_lib_conflict_fails",
    .err = SPN_ERR_PKG_NO_MATCH,
    .event = SPN_EVENT_ERR_UNSATISFIABLE_VERSION,
  },

  // Privacy never splits a link unit: a static gfx links into its consumer, so
  // its private foo lands on the same link line as the root's foo and the
  // conflict stays an error
  {
    .name = "static_lib_private_conflict_fails",
    .err = SPN_ERR_PKG_NO_MATCH,
    .event = SPN_EVENT_ERR_UNSATISFIABLE_VERSION,
  },

  // Boundaries are classified by the linkage selected for this build, not by
  // capability: gfx offers static and shared, default selection picks static,
  // so gfx links into its consumer and its private foo conflicts exactly as a
  // static-only gfx's would
  {
    .name = "private_static_default_conflicts",
    .err = SPN_ERR_PKG_NO_MATCH,
    .event = SPN_EVENT_ERR_UNSATISFIABLE_VERSION,
  },

  // When you link a library, but override its linkage to be shared, it
  // creates a new link unit and can therefore pull in libraries which would
  // conflict with a library specified by the root.
  //
  // In this test, we're building an executable E. E depends on:
  //   - F@2.0.0, static
  //   - G@1.0.0, shared
  //
  // But G@1.0.0 depends on an incompatible F@1.0.0. If we were linking to G
  // statically, this would be unsatisfiable, because it would require F@1.0.0
  // and F@2.0.0 linked into E. But since we link G dynamically, it's OK.
  {
    .name = "config_shared_private_diverges",
  },

  // Two consumers of one shared lib share one instance of it, picked from the
  // intersection of their ranges
  {
    .name = "shared_lib_consumers_unify",
  },

  // Two consumers with disjoint ranges on one shared lib would load two gfx.so
  // instances into the root's process. The loader dedupes dynamic libs by name,
  // so this is physics, not policy: an error regardless of privacy
  {
    .name = "shared_lib_consumer_disjoint_fails",
    .err = SPN_ERR_PKG_NO_MATCH,
    .event = SPN_EVENT_ERR_UNSATISFIABLE_VERSION,
  },

  // Privacy can't dodge the dynamic rule: audio.so and video.so each privately
  // want a disjoint gfx, but gfx only builds shared, so both copies would load
  // into the root's process. Divergence requires the private dep to embed
  // static; a shared-only private dep with disjoint consumers is an error.
  {
    .name = "shared_lib_private_dynamic_dup_fails",
    .err = SPN_ERR_DYNAMIC_DUPLICATE,
    .event = SPN_EVENT_ERR_DYNAMIC_DUPLICATE,
  },

  // When you have two private, static dependencies in the tree which conflict,
  // they never see each other, so it's OK. But when they're shared, you'd have
  // conflicting symbols for the loader, so this must fail.
  {
    .name = "config_shared_dynamic_dup_fails",
    .err = SPN_ERR_DYNAMIC_DUPLICATE,
    .event = SPN_EVENT_ERR_DYNAMIC_DUPLICATE,
  },

  // A build dep cycle through a single instance can never build: the tool links
  // the same audio whose build waits on the tool. The boundary doesn't make the
  // graph acyclic, it only changes where the cycle is detected.
  {
    .name = "build_dep_cycle_fails",
    .err = SPN_ERR_UNIT_CYCLE,
    .event = SPN_EVENT_ERR_UNIT_CYCLE,
  },

  // The legal shape of a build dep cycle: the tool links an *older* release of
  // audio, so the two audio instances are distinct and the build order is
  // audio 1.0.0 -> tool -> audio 2.0.0. This is what instance-level (rather
  // than name-level) cycle detection must preserve.
  {
    .name = "build_dep_bootstrap",
  },

  // A failure recorded inside a candidate that backtracking recovers from must
  // not shadow the resolve's real failure
  {
    .name = "backtrack_failure_not_sticky",
    .err = SPN_ERR_PKG_NO_MATCH,
    .event = SPN_EVENT_ERR_UNSATISFIABLE_VERSION,
  },

  // A failed candidate must release the picks its subtree made: audio 1.1.0
  // resolves math 2.0.0 before failing on missing physics, and the fallback to
  // audio 1.0.0 needs math ^1.0.0, which resolves fine on a clean slate
  {
    .name = "backtrack_releases_subtree_picks",
  },

  // A pick stranded by a failed candidate must not be committed: audio 1.1.0
  // resolves math before failing on missing physics, audio 1.0.0 has no deps,
  // so the resolve holds no math at all
  {
    .name = "backtrack_orphan_not_committed",
  },

  // The tool's unit diverges on a, so b's resolved subtree differs between the
  // units even though both hold b 1.0.0. One b build can't link both a's: the
  // instance must split rather than merge two dep sets under one identity.
  {
    .name = "divergent_dep_splits_instance",
  },

  // Each build dep roots its own process: two consumers with disjoint ranges on
  // one tool hold two instances instead of conflicting
  {
    .name = "build_dep_disjoint_tools_diverge",
  },

  // A shared lib's private scope is per-instance: gfx 1.0.0 in the tool's unit
  // and gfx 2.0.0 in the root's each carry their own private foo
  {
    .name = "private_scopes_per_instance",
  },

  // Sibling groups converge on each other in manifest order: gen decides foo
  // first, and opt's wider range admits 2.0.0 but must take gen's 1.9.0. An
  // isolated solve of opt would split a pointless second foo.
  {
    .name = "sibling_tools_unify",
  },

  // Splitting units doesn't make missing packages resolvable; a boundary
  // still resolves for real
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
  // Two sibling tools both pin a to the same older version, so each ends up
  // holding b 1.0.0 over a 1.0.0. Identical resolved subtrees must collapse
  // to ONE split instance shared by both tool units, never one per group: two
  // b instances total (the root's over a 1.9.0 plus one shared split),
  // never three
  {
    .name = "split_instances_reconverge",
  },

  // The tool pins a older, so the divergence propagates up the chain: c and b
  // hold their root versions but over a different a subtree, splitting BOTH at
  // the same version purely transitively
  {
    .name = "split_propagates_through_middle",
  },

  // The kept pin set is the lexicographically-first feasible subset of the
  // inherited table [a@1, b@1, c@1]. Keeping a@1 forces x 1.0.0, which excludes
  // b@1 (drops, splits to b 2.0.0) and never requests c (c@1 holds vacuously).
  // A newest-first solve would take x 1.2.0 and split a instead
  {
    .name = "pin_walk_lexicographic_subset",
  },

  // convergence_forces_older_sibling, two levels down: keeping the root's foo
  // pick is only satisfiable via older choices at BOTH transitive levels below
  // the tool's request (mid 1.0.0 instead of 1.1.0, bar 1.0.0 instead of 2.0.0)
  {
    .name = "convergence_forces_older_transitive",
  },

  // A tool's tool: tb is a build dep two process boundaries deep. Only tb's
  // unit may split foo; the middle tool converges with the root's pick
  {
    .name = "nested_tool_splits_only_inner",
  },

  // Privacy nests: inner is private to gfx, leaf is private to inner. Two
  // shared boundaries deep, leaf may still diverge from the root's pick, and
  // neither inner nor its leaf leaks into the root unit
  {
    .name = "private_inside_private_diverges",
  },

  // Nested privacy can't dodge the loader: leaf only builds shared, so the
  // root's leaf 2.0.0 and inner's private leaf 1.0.0 both load into the root
  // process regardless of being two private boundaries deep
  {
    .name = "private_inside_private_dynamic_dup_fails",
    .err = SPN_ERR_DYNAMIC_DUPLICATE,
    .event = SPN_EVENT_ERR_DYNAMIC_DUPLICATE,
  },

  // A build dep discovered inside a shared lib's private subtree still roots
  // its own process: the tool may take a foo the root's pick excludes, and that
  // foo never enters gfx's private unit
  {
    .name = "build_dep_inside_private_diverges",
  },

  // One package reached through all three edge kinds at once: the root links
  // foo publicly, gfx carries a private copy behind its shared boundary, and
  // the tool holds a third in its own process. Three instances, each fenced
  // into exactly its own unit
  {
    .name = "boundary_diamond_three_instances",
  },

  // Bootstrap plus divergence: the tool must link the older audio (the root's
  // 2.0.0 is excluded), and its OTHER dep zed also falls outside the root's
  // pick, so the tool's unit diverges on both while the build order stays
  // acyclic: audio 1.0.0 -> tool -> audio 2.0.0
  {
    .name = "bootstrap_with_divergent_sibling",
  },

  // Rule 5's same-version case: audio's and video's private scopes both hold
  // zed 1.0.0, but over different math subtrees. zed only builds shared, the
  // loader dedups by name, and one consumer would run against the wrong
  // embedded math: an error even though the versions are equal
  {
    .name = "same_version_dynamic_dup_fails",
    .err = SPN_ERR_DYNAMIC_DUPLICATE,
    .event = SPN_EVENT_ERR_DYNAMIC_DUPLICATE,
  },

  // Two private groups, one split: gfxa's range excludes the root's foo 2.0.0
  // and splits to 1.9.0. gfxb's range admits both decided versions, and must
  // take the EARLIEST admissible pick (the root's 2.0.0), not gfxa's newer
  // split priority-wise or the numerically-newest
  {
    .name = "private_groups_converge_on_earliest",
  },

  // A root test dep's transitive subtree diverges from the product's picks in
  // its own process instead of conflicting
  {
    .name = "test_dep_transitive_diverges",
  },

  // The tool's range admits the root's audio 2.0.0, so convergence takes it —
  // and manufactures a genuine instance cycle (audio 2.0.0 -> tool -> audio
  // 2.0.0). Rule 6: instance cycles are errors, never split triggers; the
  // resolver must not quietly fall back to audio 1.0.0
  {
    .name = "admissible_pick_cycle_fails",
    .err = SPN_ERR_UNIT_CYCLE,
    .event = SPN_EVENT_ERR_UNIT_CYCLE,
  },

  // Rule 1: a process boundary is a legal placement for a second dynamic
  // instance. The tool's process loads gfx.so 1.0.0, the root's loads 2.0.0,
  // and they never meet
  {
    .name = "shared_lib_diverges_across_process",
  },

  // Two groups discover the same lib instance, and the lib carries a build
  // dep: the re-pushed boundary lands on the SAME tool group, so lib, gen, and
  // gen's dep all stay single instances
  {
    .name = "converged_lib_single_tool_group",
  },

  // The root package is an executable, so its own private dep still lives on
  // the root link line: a conflict with a public dep's requirement stays a hard
  // error, exactly as if the dep weren't private
  {
    .name = "root_private_dep_conflict_fails",
    .err = SPN_ERR_PKG_NO_MATCH,
    .event = SPN_EVENT_ERR_UNSATISFIABLE_VERSION,
  },

  // Lots of equal-priority free choices across three groups; the perturbed
  // intern rounds in the harness assert the picks never depend on hash order
  {
    .name = "determinism_many_ties",
  },

  // Root requests c before a, and a's range would admit an older c. Greedy
  // takes c 2.0.0 first, leaving a's ^1.0.0 with no candidate; c 1.9.0
  // satisfies both, so this should resolve.
  {
    .name = "sibling_order_greedy",
  },

  // Identical graph, opposite manifest order: a resolves first and pulls
  // c 1.9.0, which the root's >=1.0.0 then accepts. Passing while
  // sibling_order_greedy fails pins the order dependence.
  {
    .name = "sibling_order_reversed",
  },

  // Avoidable dynamic duplicate: a's private dep pins c =1.0.0 and the root's
  // >=1.0.0 admits 1.0.0 too, so c could unify. The root greedily picks 2.0.0
  // before the private constraint is visible, the private scope splits, and
  // both shared c copies land in process 0 as a hard error.
  {
    .name = "avoidable_dynamic_dup",
  },
};

sp_test_each(resolver, convergence, fixture_t, convergence_cases) {
  return run_fixture(t, it);
}

// Transitive form: neither range is declared at the root, so no manifest
// order avoids it. a's loose >=1.0.0 resolves first and takes c 2.0.0;
// b's ^1.0.0 then has no candidate, though c 1.9.0 satisfies everyone.
sp_test(resolver, transitive_sibling_order) {
  return sp_test_skip(t, "");
  return run_fixture(t, &(fixture_t) { 0 });
}


// The resolver-level review contract for worlds.md cut 3
// (.llm/doc/resolve/worlds.md): skipped until the cut lands. Under today's
// concrete resolver these gates evaluate false and both fixtures resolve
// trivially; under ⊤-resolve the bool-gated edges participate in version
// selection unconditionally.

// I3.12b: two bool-gated edges with conflicting version requirements error
// with no demand anywhere — mutually incompatible alternatives are the
// enum's job
sp_test(resolver, cut3_top_resolve_bool_edges_conflict) {
  return sp_test_skip(t, "worlds cut 3");
  return run_fixture(t, &(fixture_t) {
    .err = SPN_ERROR,
    .event = SPN_EVENT_ERR_UNSATISFIABLE_VERSION,
  });
}

// I3.12c: a phantom bool-gated edge holds a sibling to the older version;
// the over-pin is the deliberate price of demand-independent selection
sp_test(resolver, cut3_phantom_edge_over_pins) {
  return sp_test_skip(t, "worlds cut 3");
  return run_fixture(t, &(fixture_t) { 0 });
}
