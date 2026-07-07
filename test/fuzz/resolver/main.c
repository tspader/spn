#define SP_IMPLEMENTATION
#include "sp.h"

#include "fuzz.h"

#include "ctx/types.h"
#include "event/event.h"
#include "index/cache.h"
#include "intern/intern.h"
#include "pkg/id.h"
#include "resolve/resolve.h"
#include "semver/parser.h"
#include "spn.h"

spn_ctx_t spn;

const c8* fz_root_qualified = "test/root";

sp_str_t fz_err_to_str(fz_err_t err) {
  switch (err) {
    case FZ_OK:                           return sp_str_lit("ok");
    case FZ_ERR_PLANTED_UNSAT:            return sp_str_lit("generator planted an unsatisfiable universe");
    case FZ_ERR_SOLVE_FOREIGN_PKG:        return sp_str_lit("solve holds a package the universe does not define");
    case FZ_ERR_SOLVE_DUPLICATE_INSTANCE: return sp_str_lit("two instances of one name in a single scope");
    case FZ_ERR_SOLVE_FOREIGN_VERSION:    return sp_str_lit("solve holds a version the index does not define");
    case FZ_ERR_ROOT_UNRESOLVED:          return sp_str_lit("root request not resolved");
    case FZ_ERR_ROOT_OUT_OF_RANGE:        return sp_str_lit("root request resolved out of range");
    case FZ_ERR_DEP_UNRESOLVED:           return sp_str_lit("resolved package's dep not resolved");
    case FZ_ERR_DEP_OUT_OF_RANGE:         return sp_str_lit("resolved package's dep resolved out of range");
    case FZ_ERR_EVENT_MISSING:            return sp_str_lit("error without a failure event");
    case FZ_ERR_PLANTED_REJECTED:         return sp_str_lit("resolver rejected a universe that is satisfiable by construction");
    case FZ_ERR_INCOMPLETE:               return sp_str_lit("resolver claims unsatisfiable; the oracle found an assignment");
    case FZ_ERR_ROOT_MISSING:             return sp_str_lit("solve does not hold the root instance");
    case FZ_ERR_EDGE_MISSING:             return sp_str_lit("instance is missing an edge for one of its deps");
    case FZ_ERR_EDGE_EXTRA:               return sp_str_lit("instance holds an edge no dep produces");
    case FZ_ERR_EDGE_MISCLASSIFIED:       return sp_str_lit("edge kind does not match the dep's classification");
    case FZ_ERR_EDGE_OUT_OF_RANGE:        return sp_str_lit("edge target does not satisfy the dep's range");
    case FZ_ERR_UNIT_DUPLICATE:           return sp_str_lit("two instances of one name in a single unit");
    case FZ_ERR_GRAPH_CYCLE:              return sp_str_lit("resolved graph holds a cycle");
    case FZ_ERR_SHARED_DUP_MISSED:        return sp_str_lit("two instances of a shared name in one process");
    case FZ_ERR_IDENTITY_SPLIT:           return sp_str_lit("structurally identical subtrees did not converge to one instance");
    case FZ_ERR_SHUFFLE_VERDICT:          return sp_str_lit("verdict changed under input reordering");
    case FZ_ERR_SHUFFLE_SOLUTION:         return sp_str_lit("solution changed under input reordering");
    case FZ_ERR_RENAME_VERDICT:           return sp_str_lit("verdict changed under package renaming");
    case FZ_ERR_INTERN_VERDICT:           return sp_str_lit("verdict changed under intern perturbation");
    case FZ_ERR_INTERN_SOLUTION:          return sp_str_lit("solution changed under intern perturbation");
    case FZ_ERR_PIN_VERDICT:              return sp_str_lit("solve is unreachable when the index holds only its releases");
    case FZ_ERR_PIN_SOLUTION:             return sp_str_lit("solve changed when the index holds only its releases");
    case FZ_ERR_EXTEND_VERDICT:           return sp_str_lit("adding a release broke a satisfiable universe");
    case FZ_ERR_COUNT:                    break;
  }
  sp_unreachable_return(sp_str_lit("unknown"));
}


//////////////
// EXECUTOR //
//////////////
typedef struct {
  sp_str_ht(spn_index_pkg_t) cache;
} fz_state_t;

static fz_state_t fz_state;

void spn_index_cache_init(spn_index_cache_t* cache, sp_mem_t mem, sp_intern_t* intern, spn_index_arr_t* indexes) {

}

spn_index_pkg_t* spn_index_cache_get_package(spn_index_cache_t* cache, spn_pkg_name_t id) {
  sp_str_t qualified = spn_pkg_name_to_qualified(id);
  return sp_str_ht_get(fz_state.cache, qualified);
}

spn_index_rel_t* spn_index_cache_get_release(spn_index_cache_t* cache, spn_pkg_name_t id, spn_semver_t version) {
  return SP_NULLPTR;
}

typedef struct {
  spn_resolve_query_t query;
  sp_da(spn_build_event_t) events;
  spn_err_t err;
} fz_result_t;

static spn_dep_kind_t fz_root_dep_kind(spn_index_dep_kind_t kind) {
  switch (kind) {
    case SPN_INDEX_DEP_NORMAL: return SPN_DEP_KIND_PACKAGE;
    case SPN_INDEX_DEP_BUILD:  return SPN_DEP_KIND_BUILD;
    case SPN_INDEX_DEP_TEST:   return SPN_DEP_KIND_TEST;
  }
  sp_unreachable_return(SPN_DEP_KIND_PACKAGE);
}

static fz_result_t fz_execute(sp_mem_t mem, fz_universe_t* u, sp_intern_t* intern) {
  fz_state = sp_zero_s(fz_state_t);
  sp_str_ht_init(mem, fz_state.cache);

  sp_da_for(u->pkgs, it) {
    spn_index_pkg_t pkg = {
      .id = {
        .namespace = sp_str_lit("spn"),
        .name = sp_str_view(fz_names[it]),
      }
    };
    sp_da_init(mem, pkg.releases);

    sp_da_for(u->pkgs[it].releases, rt) {
      fz_release_t* source = &u->pkgs[it].releases[rt];
      spn_index_rel_t release = {
        .id = pkg.id,
        .version = source->version,
      };
      sp_da_init(mem, release.deps);
      sp_da_init(mem, release.targets);

      sp_da_for(source->deps, dt) {
        sp_da_push(release.deps, ((spn_index_dep_t) {
          .kind = source->deps[dt].kind,
          .private = source->deps[dt].private,
          .id = {
            .namespace = sp_str_lit("spn"),
            .name = sp_str_view(fz_names[source->deps[dt].pkg]),
          },
          .version = fz_range_render(mem, source->deps[dt]),
        }));
      }

      if (u->pkgs[it].shared) {
        spn_index_rel_target_t target = { .name = sp_str_view(fz_names[it]) };
        sp_da_init(mem, target.linkages);
        sp_da_push(target.linkages, SPN_LIB_KIND_SHARED);
        sp_da_push(release.targets, target);
      }

      sp_da_push(pkg.releases, release);
    }

    sp_str_ht_insert(fz_state.cache, spn_pkg_name_to_qualified(pkg.id), pkg);
  }

  if (!intern) {
    intern = sp_intern_new(mem);
  }

  spn_pkg_info_t* root = (spn_pkg_info_t*)sp_alloc(mem, sizeof(spn_pkg_info_t));
  *root = sp_zero_s(spn_pkg_info_t);
  root->namespace = sp_str_lit("test");
  root->name = sp_str_lit("root");
  root->qualified = sp_str_view(fz_root_qualified);
  root->version = spn_semver_lit(0, 0, 1);
  sp_da_init(mem, root->deps);

  sp_da_for(u->roots, it) {
    sp_da_push(root->deps, ((spn_requested_pkg_t) {
      .qualified = spn_pkg_canonicalize_pair(sp_str_lit("spn"), sp_str_view(fz_names[u->roots[it].pkg])),
      .source = SPN_PKG_SOURCE_INDEX,
      .kind = fz_root_dep_kind(u->roots[it].kind),
      .private = u->roots[it].private,
      .index.range = spn_semver_parse_range(fz_range_render(mem, u->roots[it])),
    }));
  }

  spn_pkg_registry_t registry = sp_zero;
  sp_ht_init(mem, registry);
  sp_ht_insert(registry, spn_pkg_id(intern, root->qualified), ((spn_registry_pkg_t) {
    .source = SPN_PKG_SOURCE_ROOT,
    .info = root,
  }));

  spn_event_buffer_t* events = spn_event_buffer_new(mem);
  spn_index_cache_t cache = sp_zero;
  sp_da(spn_pkg_config_entry_t) config = sp_da_new(mem, spn_pkg_config_entry_t);

  spn_resolver_t resolver = sp_zero;
  spn_resolver_init(&resolver, mem, intern, &cache, &registry, events, SPN_LIB_KIND_NONE, config, u->profile.budget);

  fz_result_t result = sp_zero_s(fz_result_t);
  spn_resolve_query_init(mem, &result.query);
  spn_resolve_query_add(&result.query, (spn_requested_pkg_t) {
    .qualified = sp_str_view(fz_root_qualified),
    .source = SPN_PKG_SOURCE_ROOT,
  });

  result.err = spn_resolve_from_solver(&resolver, &result.query);
  result.events = spn_event_buffer_drain(mem, events);
  return result;
}

static bool fz_pushed_event(fz_result_t* result, spn_build_event_kind_t kind) {
  sp_da_for(result->events, it) {
    if (result->events[it].kind == kind) {
      return true;
    }
  }
  return false;
}


//////////
// MAIN //
//////////
static fz_err_t fz_check_result(sp_mem_t mem, fz_universe_t* u, fz_result_t* result) {
  if (!result->err) {
    fz_err_t err = u->profile.features ? FZ_OK : fz_check_solve(u, &result->query);
    if (err) return err;
    return fz_check_units(mem, u, &result->query);
  }

  if (sp_da_empty(result->events)) {
    return FZ_ERR_EVENT_MISSING;
  }
  if (fz_pushed_event(result, SPN_EVENT_ERR_RESOLUTION_TOO_COMPLEX)) {
    return FZ_OK;
  }
  if (u->planted) {
    return FZ_ERR_PLANTED_REJECTED;
  }
  if (!u->profile.features && fz_oracle_sat(u)) {
    return FZ_ERR_INCOMPLETE;
  }
  return FZ_OK;
}

static sp_da(sp_str_t) fz_intern_names(sp_mem_t mem) {
  sp_da(sp_str_t) names = sp_da_new(mem, sp_str_t);
  sp_da_push(names, sp_str_lit(""));
  sp_da_push(names, sp_str_view(fz_root_qualified));
  sp_carr_for(fz_names, it) {
    sp_da_push(names, spn_pkg_canonicalize_pair(sp_str_lit("spn"), sp_str_view(fz_names[it])));
  }
  return names;
}

static fz_err_t fz_run_iteration(sp_fuzz_prng_t base, u64 iter) {
  sp_mem_arena_t* arena = sp_mem_arena_new(sp_mem_os_new());
  sp_mem_t mem = sp_mem_arena_as_allocator(arena);
  sp_fuzz_prng_t prng = sp_fuzz_iter(base, iter);

  fz_profile_t profile = fz_gen_profile(&prng);
  fz_universe_t universe = fz_gen_universe(mem, &prng, profile);

  fz_err_t err = FZ_OK;
  if (universe.planted && !fz_oracle_sat(&universe)) {
    err = FZ_ERR_PLANTED_UNSAT;
  }

  fz_result_t result = sp_zero_s(fz_result_t);
  if (!err) {
    result = fz_execute(mem, &universe, SP_NULLPTR);
    err = fz_check_result(mem, &universe, &result);
  }

  // On a metamorphic failure, dump the variant the divergence was observed
  // on: it is a self-contained universe whose verdict is wrong on one side,
  // so its fixture reproduces where the original's would pass. Intern
  // failures keep the original; the fixture runner perturbs interns itself
  fz_universe_t dumped = universe;
  bool stable = !err && !profile.budget && !fz_pushed_event(&result, SPN_EVENT_ERR_RESOLUTION_TOO_COMPLEX);
  if (stable) {
    fz_solution_t solution = sp_zero;
    if (!result.err) {
      solution = fz_solution(mem, &universe, &result.query);
    }

    fz_universe_t shuffled = fz_shuffle_universe(mem, &prng, &universe);
    fz_result_t shuffled_result = fz_execute(mem, &shuffled, SP_NULLPTR);
    if (!shuffled_result.err != !result.err) {
      err = FZ_ERR_SHUFFLE_VERDICT;
      dumped = shuffled;
    }
    else if (!result.err && !fz_solution_equal(solution, fz_solution(mem, &shuffled, &shuffled_result.query))) {
      err = FZ_ERR_SHUFFLE_SOLUTION;
      dumped = shuffled;
    }

    if (!err) {
      fz_universe_t renamed = fz_rename_universe(mem, &prng, &universe);
      fz_result_t renamed_result = fz_execute(mem, &renamed, SP_NULLPTR);
      if (!renamed_result.err != !result.err) {
        err = FZ_ERR_RENAME_VERDICT;
        dumped = renamed;
      }
    }

    if (!err) {
      sp_intern_t* intern = sp_fuzz_perturbed_intern(&prng, fz_intern_names(mem));
      fz_result_t perturbed = fz_execute(mem, &universe, intern);
      if (!perturbed.err != !result.err) {
        err = FZ_ERR_INTERN_VERDICT;
      }
      else if (!result.err && !fz_solution_equal(solution, fz_solution(mem, &universe, &perturbed.query))) {
        err = FZ_ERR_INTERN_SOLUTION;
      }
    }

    if (!err && !result.err) {
      fz_universe_t pinned = fz_pin_universe(mem, &universe, solution);
      fz_result_t pinned_result = fz_execute(mem, &pinned, SP_NULLPTR);
      if (pinned_result.err) {
        err = FZ_ERR_PIN_VERDICT;
        dumped = pinned;
      }
      else if (!profile.features && !fz_solution_equal(solution, fz_solution(mem, &pinned, &pinned_result.query))) {
        err = FZ_ERR_PIN_SOLUTION;
        dumped = pinned;
      }
    }

    if (!err && !result.err) {
      fz_universe_t extended = fz_extend_universe(mem, &prng, &universe);
      fz_result_t extended_result = fz_execute(mem, &extended, SP_NULLPTR);
      if (extended_result.err && !fz_pushed_event(&extended_result, SPN_EVENT_ERR_RESOLUTION_TOO_COMPLEX)) {
        err = FZ_ERR_EXTEND_VERDICT;
        dumped = extended;
      }
    }
  }

  if (err) {
    fz_dump(&dumped, iter);
  }

  sp_mem_arena_destroy(arena);
  return err;
}

s32 main(void) {
  spn.intern = sp_intern_new(sp_mem_os_new());
  sp_fuzz_seed_init();
  sp_fuzz_opts_t opts = sp_fuzz_opts(512);

  sp_io_stream_writer_t out = sp_io_get_std_out();
  if (!fz_ranges_agree()) {
    sp_fmt_io(&out.base, "fuzz: fz_range_sat disagrees with the resolver's range parser\n");
    return 1;
  }

  sp_da(sp_str_t) names = sp_da_new(sp_mem_os_new(), sp_str_t);
  sp_da_push(names, sp_str_lit("fuzz/resolver"));
  sp_fuzz_prng_t base = sp_fuzz_stream(names);

  bool keep_going = !sp_str_empty(sp_os_env_get(sp_str_lit("SPN_FUZZ_KEEP_GOING")));
  u64 failures[FZ_ERR_COUNT] = sp_zero;
  u64 failed = 0;

  for (u64 iter = 0; iter < opts.iters; iter++) {
    if (opts.only >= 0 && iter != (u64)opts.only) continue;

    fz_err_t err = fz_run_iteration(base, iter);
    if (!err) continue;

    failed++;
    failures[err]++;
    sp_fmt_io(&out.base, "fuzz: {} (iter {})\n", sp_fmt_str(fz_err_to_str(err)), sp_fmt_uint(iter));
    if (!keep_going) {
      return (s32)err;
    }
  }

  if (failed) {
    sp_fmt_io(&out.base, "fuzz: {} of {} iterations failed\n", sp_fmt_uint(failed), sp_fmt_uint(opts.iters));
    sp_carr_for(failures, it) {
      if (!failures[it]) continue;
      sp_fmt_io(&out.base, "  {}: {}\n", sp_fmt_uint(failures[it]), sp_fmt_str(fz_err_to_str((fz_err_t)it)));
    }
    return 1;
  }

  return 0;
}
