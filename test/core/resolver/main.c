#define SP_IMPLEMENTATION
#include "sp.h"

#include "resolver.h"
#include "ctx.h"

#include "ctx/types.h"
#include "index/types.h"
#include "sp/macro.h"

#include "error/types.h"
#include "event/event.h"
#include "sp_fuzz.h"
#include "index/cache.h"
#include "intern/intern.h"
#include "pkg/id.h"
#include "resolve/dump.h"
#include "resolve/resolve.h"
#include "resolve/types.h"
#include "semver/compare.h"
#include "semver/convert.h"
#include "semver/parser.h"
#include "when/when.h"
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


///////////////
// EXECUTOR //
//////////////
static spn_option_value_t build_value(value_lit_t lit) {
  if (lit.str) {
    return spn_option_value_str(sp_str_view(lit.str));
  }
  if (lit.is_bool) {
    return spn_option_value_bool(lit.b);
  }
  return spn_option_value_none();
}

static spn_when_t build_when(sp_mem_t mem, const clause_lit_t* clauses, u64 count) {
  spn_when_t when = sp_zero;
  for (u64 it = 0; it < count; it++) {
    if (!clauses[it].key) break;
    if (sp_da_empty(when.clauses)) {
      when.clauses = sp_da_new(mem, spn_when_clause_t);
    }
    sp_da_push(when.clauses, ((spn_when_clause_t) {
      .key = sp_str_view(clauses[it].key),
      .negated = clauses[it].negated,
      .value = build_value((value_lit_t) { .str = clauses[it].str, .b = clauses[it].b, .is_bool = clauses[it].is_bool }),
    }));
  }
  return when;
}

static spn_option_map_t build_options(sp_mem_t mem, const index_option_t* descs, u64 count) {
  spn_option_map_t options = sp_zero;
  for (u64 it = 0; it < count; it++) {
    const index_option_t* desc = &descs[it];
    if (!desc->name) break;

    spn_option_info_t option = {
      .name = sp_str_view(desc->name),
      .type = desc->type,
      .values = sp_da_new(mem, sp_str_t),
      .defaults = sp_da_new(mem, spn_option_default_t),
    };
    sp_carr_for(desc->values, vt) {
      if (!desc->values[vt]) break;
      sp_da_push(option.values, sp_str_view(desc->values[vt]));
    }
    spn_option_value_t fallback = build_value(desc->fallback);
    if (fallback.kind != SPN_OPTION_VALUE_NONE) {
      sp_da_push(option.defaults, ((spn_option_default_t) { .value = fallback }));
    }
    sp_str_om_insert(options, option.name, option);
  }
  return options;
}

static void build_cache(sp_mem_t mem, const fixture_t* fixture) {
  sp_carr_for(fixture->index, i) {
    const index_pkg_t* desc = &fixture->index[i];
    if (!desc->name) break;

    spn_pkg_name_t id = {
      .namespace = sp_str_view(desc->namespace),
      .name = sp_str_view(desc->name),
    };

    spn_index_pkg_t pkg = { .id = id };
    sp_da_init(mem, pkg.releases);

    sp_carr_for(desc->releases, r) {
      const index_rel_t* rel_desc = &desc->releases[r];
      if (!rel_desc->version.major && !rel_desc->version.minor && !rel_desc->version.patch
          && !rel_desc->deps[0].name) {
        break;
      }

      spn_index_release_t rel = {
        .id = id,
        .version = rel_desc->version,
        .yanked = rel_desc->yanked,
        .options = build_options(mem, rel_desc->options, SP_CARR_LEN(rel_desc->options)),
      };
      sp_da_init(mem, rel.deps);

      sp_carr_for(rel_desc->deps, d) {
        const index_dep_t* dep_desc = &rel_desc->deps[d];
        if (!dep_desc->name) break;

        sp_da_push(rel.deps, ((spn_index_dep_t) {
          .kind = dep_desc->kind,
          .private = dep_desc->private,
          .id = {
            .namespace = sp_str_view(dep_desc->namespace),
            .name = sp_str_view(dep_desc->name),
          },
          .version = sp_str_view(dep_desc->version),
          .when = build_when(mem, dep_desc->when, SP_CARR_LEN(dep_desc->when)),
        }));
      }

      sp_da_init(mem, rel.targets);
      sp_carr_for(rel_desc->targets, t) {
        const index_target_t* target_desc = &rel_desc->targets[t];
        if (!target_desc->name) break;

        spn_index_target_t target = { .name = sp_str_view(target_desc->name) };
        sp_da_init(mem, target.linkages);
        sp_carr_for(target_desc->linkages, l) {
          if (target_desc->linkages[l] == SPN_LIB_KIND_NONE) break;
          sp_da_push(target.linkages, target_desc->linkages[l]);
        }
        sp_da_push(rel.targets, target);
      }

      sp_da_push(pkg.releases, rel);
    }

    sp_str_t qualified = spn_pkg_name_to_qualified(id);
    sp_str_ht_insert(state.cache, qualified, pkg);
  }
}

static const c8* root_qualified = "test/root";

// The fixture manifest becomes a real root package, like production's
// add_root: the root unit is the closure from the root package's node
static spn_pkg_info_t* build_root(sp_mem_t mem, const fixture_t* fixture) {
  spn_pkg_info_t* info = (spn_pkg_info_t*)sp_alloc(mem, sizeof(spn_pkg_info_t));
  *info = sp_zero_s(spn_pkg_info_t);
  info->namespace = sp_str_lit("test");
  info->name = sp_str_lit("root");
  info->qualified = sp_str_view(root_qualified);
  info->version = spn_semver_lit(0, 0, 1);
  sp_da_init(mem, info->deps);

  sp_carr_for(fixture->manifest.deps.package, i) {
    const manifest_dep_t* dep = &fixture->manifest.deps.package[i];
    if (!dep->name) break;

    spn_semver_range_t range = sp_zero;
    SP_ASSERT(!spn_semver_parse_range(sp_str_view(dep->version), &range));
    sp_da_push(info->deps, ((spn_requested_dep_t) {
      .qualified = spn_pkg_canonicalize_name(sp_str_view(dep->name)),
      .source = SPN_PKG_SOURCE_INDEX,
      .kind = dep->kind,
      .private = dep->private,
      .when = build_when(mem, dep->when, SP_CARR_LEN(dep->when)),
      .index.range = range,
    }));
  }

  return info;
}


static resolve_result_t execute_fixture(const fixture_t* fixture, sp_intern_t* intern) {
  sp_mem_t mem = sp_mem_arena_as_allocator(ctx_get()->arena);

  sp_da(spn_pkg_config_entry_t) config = sp_da_new(mem, spn_pkg_config_entry_t);
  sp_carr_for(fixture->config, it) {
    if (!fixture->config[it].name) break;

    spn_pkg_config_entry_t entry = { .key = sp_str_view(fixture->config[it].name) };
    sp_opt_set(entry.value.kind, fixture->config[it].kind);
    sp_da_push(config, entry);
  }

  spn_event_buffer_t* events = spn_event_buffer_new(sp_mem_os_new());
  spn_index_cache_t cache = sp_zero;
  spn_pkg_registry_t registry = sp_zero;
  sp_ht_init(mem, registry);

  spn_pkg_info_t* root = build_root(mem, fixture);
  sp_ht_insert(registry, spn_pkg_id(intern, root->qualified), ((spn_registry_pkg_t) {
    .source = SPN_PKG_SOURCE_ROOT,
    .info = root,
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
  }, config, fixture->budget);

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


static sp_da(sp_str_t) fixture_names(sp_mem_t mem, const fixture_t* fixture) {
  sp_da(sp_str_t) names = sp_da_new(mem, sp_str_t);
  sp_da_push(names, sp_str_lit(""));
  sp_da_push(names, sp_str_view(root_qualified));

  sp_carr_for(fixture->index, it) {
    if (!fixture->index[it].name) break;
    sp_da_push(names, spn_pkg_canonicalize_pair(sp_str_view(fixture->index[it].namespace), sp_str_view(fixture->index[it].name)));
  }

  sp_carr_for(fixture->manifest.deps.package, it) {
    if (!fixture->manifest.deps.package[it].name) break;
    sp_da_push(names, spn_pkg_canonicalize_name(sp_str_view(fixture->manifest.deps.package[it].name)));
  }

  return names;
}

sp_err_t run_fixture(sp_test_t* t, const fixture_t* fixture) {
  sp_mem_t mem = sp_mem_arena_as_allocator(ctx_get()->arena);
  state = sp_zero_s(state_t);
  sp_str_ht_init(mem, state.cache);
  build_cache(mem, fixture);

  sp_da(sp_str_t) names = fixture_names(mem, fixture);
  sp_fuzz_prng_t prng = sp_fuzz_stream(names);

  sp_intern_t* intern = sp_intern_new(sp_mem_os_new());
  resolve_result_t canonical = execute_fixture(fixture, intern);
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
    resolve_result_t shaken = execute_fixture(fixture, sp_fuzz_perturbed_intern(&prng, names));
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
