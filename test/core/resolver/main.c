#define SP_IMPLEMENTATION
#include "sp.h"

#define UTEST_IMPLEMENTATION
#include "utest.h"

#include "test.h"

#include "ctx/types.h"
#include "index/types.h"
#include "sp/macro.h"

#include "error/types.h"
#include "event/event.h"
#include "sp_fuzz.h"
#include "index/cache.h"
#include "intern/intern.h"
#include "pkg/id.h"
#include "resolve/resolve.h"
#include "resolve/types.h"
#include "semver/compare.h"
#include "semver/convert.h"
#include "semver/parser.h"
#include "spn.h"

UTEST_STATE();

spn_ctx_t spn;

s32 main(s32 argc, const c8** argv) {
  ctx_t* ctx = ctx_get();
  ctx_init(ctx_get());

  spn.intern = sp_intern_new(sp_mem_os_new());
  sp_fuzz_seed_init();

  s32 result = utest_main(argc, argv);

  ctx_deinit(ctx_get());
  return result;
}


/////////////////
// DESCRIPTOR //
////////////////
typedef struct {
  const c8* namespace;
  const c8* name;
  const c8* version;
  spn_index_dep_kind_t kind;
  bool private;
} index_dep_t;

typedef struct {
  const c8* name;
  spn_linkage_t linkages[4];
} index_target_t;

typedef struct {
  spn_semver_t version;
  index_dep_t deps[4];
  index_target_t targets[2];
} index_rel_t;

typedef struct {
  const c8* namespace;
  const c8* name;
  index_rel_t releases[8];
} index_pkg_t;

typedef struct {
  const c8* name;
  const c8* version;
  spn_dep_kind_t kind;
  bool private;
} manifest_dep_t;

typedef struct {
  manifest_dep_t package[8];
  const c8* system[8];
} manifest_deps_t;

// unit names which unit the package must be a member of: NULL is any unit, ""
// is the root unit, and a qualified name is the unit rooted at that request.
// unit_version narrows the unit to the one rooted at that exact instance;
// excluded flips the assertion to require the package NOT be a member
typedef struct {
  const c8* name;
  const c8* namespace;
  spn_semver_t version;
  const c8* unit;
  spn_semver_t unit_version;
  bool excluded;
} expected_t;

// asserts how many instances (distinct versions) of a package the resolve
// holds; count 1 pins that compatible units share one build
typedef struct {
  const c8* name;
  u32 count;
} instance_count_t;

// [config.$pkg] overrides for the root manifest
typedef struct {
  const c8* name;
  spn_linkage_t kind;
} config_kind_t;

// Payload of the expected unsatisfiable event: selected set expects a
// conflict against that version; selected unset expects a plain
// no-version-in-range failure
typedef struct {
  const c8* namespace;
  const c8* name;
  const c8* requester;
  const c8* selected;
} expect_unsat_t;

typedef struct {
  index_pkg_t index[8];
  struct {
    manifest_deps_t deps;
  } manifest;
  spn_linkage_t linkage;
  config_kind_t config[4];
  u64 budget;
  spn_err_t err;
  spn_build_event_kind_t event;
  expect_unsat_t unsat;
  expected_t expected[8];
  instance_count_t instances[8];
} fixture_t;


///////////
// STATE //
///////////
typedef struct {
  sp_str_ht(spn_index_pkg_t) cache;
} state_t;

static state_t state;

struct resolver {
  u32 dummy;
};

UTEST_F_SETUP(resolver) {
  state = sp_zero_s(state_t);
  sp_str_ht_init(sp_mem_arena_as_allocator(ctx_get()->arena), state.cache);
}

UTEST_F_TEARDOWN(resolver) {

}

///////////
// MOCKS //
///////////
void spn_index_cache_init(spn_index_cache_t* cache, sp_mem_t mem, sp_intern_t* intern, spn_index_arr_t* indexes) {

}

spn_index_pkg_t* spn_index_cache_get_package(spn_index_cache_t* cache, spn_pkg_name_t id) {
  sp_str_t qualified = spn_pkg_name_to_qualified(id);
  return sp_str_ht_get(state.cache, qualified);
}

spn_index_rel_t* spn_index_cache_get_release(spn_index_cache_t* cache, spn_pkg_name_t id, spn_semver_t version) {
  return SP_NULLPTR;
}


///////////////
// EXECUTOR //
//////////////
static void build_cache(sp_mem_t mem, fixture_t* fixture) {
  sp_carr_for(fixture->index, i) {
    index_pkg_t* desc = &fixture->index[i];
    if (!desc->name) break;

    spn_pkg_name_t id = {
      .namespace = sp_str_view(desc->namespace),
      .name = sp_str_view(desc->name),
    };

    spn_index_pkg_t pkg = { .id = id };
    sp_da_init(mem, pkg.releases);

    sp_carr_for(desc->releases, r) {
      index_rel_t* rel_desc = &desc->releases[r];
      if (!rel_desc->version.major && !rel_desc->version.minor && !rel_desc->version.patch
          && !rel_desc->deps[0].name) {
        break;
      }

      spn_index_rel_t rel = {
        .id = id,
        .version = rel_desc->version,
      };
      sp_da_init(mem, rel.deps);

      sp_carr_for(rel_desc->deps, d) {
        index_dep_t* dep_desc = &rel_desc->deps[d];
        if (!dep_desc->name) break;

        sp_da_push(rel.deps, ((spn_index_dep_t) {
          .kind = dep_desc->kind,
          .private = dep_desc->private,
          .id = {
            .namespace = sp_str_view(dep_desc->namespace),
            .name = sp_str_view(dep_desc->name),
          },
          .version = sp_str_view(dep_desc->version),
        }));
      }

      sp_da_init(mem, rel.targets);
      sp_carr_for(rel_desc->targets, t) {
        index_target_t* target_desc = &rel_desc->targets[t];
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
static spn_pkg_info_t* build_root(sp_mem_t mem, fixture_t* fixture) {
  spn_pkg_info_t* info = (spn_pkg_info_t*)sp_alloc(mem, sizeof(spn_pkg_info_t));
  *info = sp_zero_s(spn_pkg_info_t);
  info->namespace = sp_str_lit("test");
  info->name = sp_str_lit("root");
  info->qualified = sp_str_view(root_qualified);
  info->version = spn_semver_lit(0, 0, 1);
  sp_da_init(mem, info->deps);

  sp_carr_for(fixture->manifest.deps.package, i) {
    manifest_dep_t* dep = &fixture->manifest.deps.package[i];
    if (!dep->name) break;

    spn_semver_range_t range = sp_zero;
    SP_ASSERT(!spn_semver_parse_range(sp_str_view(dep->version), &range));
    sp_da_push(info->deps, ((spn_requested_pkg_t) {
      .qualified = spn_pkg_canonicalize_name(sp_str_view(dep->name)),
      .source = SPN_PKG_SOURCE_INDEX,
      .kind = dep->kind,
      .private = dep->private,
      .index.range = range,
    }));
  }

  return info;
}

static bool id_eq(spn_pkg_id_t a, spn_pkg_id_t b) {
  return a.qualified == b.qualified && spn_semver_eq(a.version, b.version) && a.hash == b.hash;
}

static bool semver_zero(spn_semver_t version) {
  return !version.major && !version.minor && !version.patch;
}

static bool closure_holds(sp_mem_t mem, spn_resolve_query_t* query, sp_da(spn_pkg_id_t) roots, spn_resolved_pkg_t* pkg) {
  sp_da(spn_pkg_id_t) work = sp_da_new(mem, spn_pkg_id_t);
  sp_da(spn_pkg_id_t) seen = sp_da_new(mem, spn_pkg_id_t);
  sp_da_for(roots, it) {
    sp_da_push(work, roots[it]);
  }

  while (!sp_da_empty(work)) {
    spn_pkg_id_t id = *sp_da_back(work);
    sp_da_pop(work);

    bool visited = false;
    sp_da_for(seen, it) {
      if (id_eq(seen[it], id)) {
        visited = true;
        break;
      }
    }
    if (visited) continue;
    sp_da_push(seen, id);

    if (id_eq(id, pkg->id)) return true;

    spn_resolved_pkg_t* node = sp_ht_getp(query->result, id);
    if (!node) continue;
    sp_da_for(node->edges, it) {
      if (node->edges[it].edge == SPN_DEP_EDGE_SCOPE) {
        sp_da_push(work, node->edges[it].id);
      }
    }
  }

  return false;
}

// Membership is derived, never stored: an instance is in a unit iff it is
// reachable from that unit's boundary through scope edges. The root unit is
// rooted at the root package; a process unit at the target of a build/test
// edge; a private unit at the private edge targets of its owning instance.
static bool resolved_in_unit(spn_resolve_query_t* query, spn_resolved_pkg_t* pkg, const c8* unit, spn_semver_t unit_version) {
  if (!unit) return true;

  sp_mem_t mem = sp_mem_arena_as_allocator(ctx_get()->arena);
  sp_da(spn_pkg_id_t) roots = sp_da_new(mem, spn_pkg_id_t);

  if (!*unit) {
    sp_ht_for_kv(query->result, it) {
      if (sp_str_equal(it.val->qualified, sp_str_view(root_qualified))) {
        sp_da_push(roots, it.val->id);
      }
    }
  }
  else {
    sp_str_t name = spn_pkg_canonicalize_name(sp_str_view(unit));
    sp_ht_for_kv(query->result, it) {
      spn_resolved_pkg_t* owner = it.val;
      sp_da_for(owner->edges, et) {
        spn_resolved_dep_t* edge = &owner->edges[et];
        switch (edge->edge) {
          case SPN_DEP_EDGE_PROCESS: {
            spn_resolved_pkg_t* target = sp_ht_getp(query->result, edge->id);
            if (!target || !sp_str_equal(target->qualified, name)) break;
            if (!semver_zero(unit_version) && !spn_semver_eq(target->version, unit_version)) break;
            sp_da_push(roots, edge->id);
            break;
          }
          case SPN_DEP_EDGE_PRIVATE: {
            if (!sp_str_equal(owner->qualified, name)) break;
            if (!semver_zero(unit_version) && !spn_semver_eq(owner->version, unit_version)) break;
            sp_da_push(roots, edge->id);
            break;
          }
          case SPN_DEP_EDGE_SCOPE:
          case SPN_DEP_EDGE_PRUNED: {
            break;
          }
        }
      }
    }
  }

  return closure_holds(mem, query, roots, pkg);
}

typedef struct {
  spn_resolve_query_t query;
  sp_da(spn_build_event_t) events;
  spn_err_t err;
} resolve_result_t;

static resolve_result_t execute_fixture(fixture_t* fixture, sp_intern_t* intern) {
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
  spn_resolver_init(&resolver, mem, intern, &cache, &registry, events, (spn_profile_info_t) { .linkage = fixture->linkage }, config, fixture->budget);

  resolve_result_t result = sp_zero_s(resolve_result_t);
  spn_resolve_query_init(mem, &result.query);
  spn_resolve_query_add(&result.query, (spn_requested_pkg_t) {
    .qualified = sp_str_view(root_qualified),
    .source = SPN_PKG_SOURCE_ROOT,
  });

  result.err = spn_resolve_from_solver(&resolver, &result.query);
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

static void assert_unsat_event(s32* utest_result, sp_mem_t mem, resolve_result_t* result, expect_unsat_t unsat) {
  spn_build_event_t* event = find_event(result, SPN_EVENT_ERR_UNSATISFIABLE_VERSION);
  ASSERT_TRUE(event != SP_NULLPTR);

  sp_str_t qualified = spn_pkg_canonicalize_pair(sp_str_view(unsat.namespace), sp_str_view(unsat.name));
  ASSERT_TRUE(sp_str_equal(event->unsatisfiable.request.qualified, qualified));

  if (unsat.requester) {
    ASSERT_TRUE(sp_str_equal(event->unsatisfiable.requester, sp_str_view(unsat.requester)));
  }

  if (unsat.selected) {
    ASSERT_TRUE(event->unsatisfiable.conflict);
    ASSERT_TRUE(sp_str_equal(spn_semver_to_str(mem, event->unsatisfiable.selected), sp_str_view(unsat.selected)));
  }
  else {
    ASSERT_FALSE(event->unsatisfiable.conflict);
  }
}

static sp_str_t instance_name(spn_resolve_query_t* query, sp_intern_id_t qualified) {
  if (!qualified) return sp_str_lit("");

  sp_ht_for_kv(query->result, it) {
    if (it.key->qualified == qualified) {
      return it.val->qualified;
    }
  }
  return sp_str_lit("<unresolved>");
}

static void assert_resolves_equal(s32* utest_result, spn_resolve_query_t* a, spn_resolve_query_t* b) {
  sp_mem_t mem = sp_mem_arena_as_allocator(ctx_get()->arena);

  ASSERT_EQ(sp_ht_size(a->result), sp_ht_size(b->result));
  sp_da(spn_resolved_pkg_t*) claimed = sp_da_new(mem, spn_resolved_pkg_t*);

  sp_ht_for_kv(a->result, it) {
    spn_resolved_pkg_t* pa = it.val;

    spn_resolved_pkg_t* pb = SP_NULLPTR;
    sp_ht_for_kv(b->result, jt) {
      bool taken = false;
      sp_da_for(claimed, ct) {
        if (claimed[ct] == jt.val) {
          taken = true;
          break;
        }
      }
      if (taken) continue;

      if (sp_str_equal(jt.val->qualified, pa->qualified) && spn_semver_eq(jt.val->version, pa->version) && jt.val->id.hash == pa->id.hash) {
        pb = jt.val;
        break;
      }
    }
    ASSERT_TRUE(pb != SP_NULLPTR);
    sp_da_push(claimed, pb);

    ASSERT_EQ(sp_da_size(pa->edges), sp_da_size(pb->edges));
    sp_da(u8) edge_used = sp_da_new(mem, u8);
    sp_da_for(pb->edges, ft) {
      sp_da_push(edge_used, 0);
    }
    sp_da_for(pa->edges, et) {
      spn_resolved_dep_t* ea = &pa->edges[et];
      sp_str_t target = instance_name(a, ea->id.qualified);

      bool matched = false;
      sp_da_for(pb->edges, ft) {
        spn_resolved_dep_t* eb = &pb->edges[ft];
        if (edge_used[ft]) continue;
        if (!sp_str_equal(instance_name(b, eb->id.qualified), target)) continue;
        if (!spn_semver_eq(eb->id.version, ea->id.version)) continue;
        if (eb->id.hash != ea->id.hash) continue;
        if (eb->kind != ea->kind) continue;
        if (eb->edge != ea->edge) continue;
        if (eb->private != ea->private) continue;
        edge_used[ft] = 1;
        matched = true;
        break;
      }
      ASSERT_TRUE(matched);
    }
  }
}

static sp_da(sp_str_t) fixture_names(sp_mem_t mem, fixture_t* fixture) {
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

void run_fixture(s32* utest_result, fixture_t fixture) {
  sp_mem_t mem = sp_mem_arena_as_allocator(ctx_get()->arena);
  build_cache(mem, &fixture);

  sp_da(sp_str_t) names = fixture_names(mem, &fixture);
  sp_fuzz_prng_t prng = sp_fuzz_stream(names);

  sp_intern_t* intern = sp_intern_new(sp_mem_os_new());
  resolve_result_t canonical = execute_fixture(&fixture, intern);
  ASSERT_EQ(canonical.err, fixture.err);

  if (fixture.event) {
    ASSERT_TRUE(pushed_event(&canonical, fixture.event));
  }

  if (fixture.unsat.name) {
    assert_unsat_event(utest_result, mem, &canonical, fixture.unsat);
    if (*utest_result) return;
  }

  // Picks may never depend on intern state: resolve against perturbed interns
  // and require structurally identical results every round
  for (u32 round = 0; round < 8; round++) {
    resolve_result_t shaken = execute_fixture(&fixture, sp_fuzz_perturbed_intern(&prng, names));
    ASSERT_EQ(shaken.err, canonical.err);
    if (fixture.event) {
      ASSERT_TRUE(pushed_event(&shaken, fixture.event));
    }
    if (fixture.unsat.name) {
      assert_unsat_event(utest_result, mem, &shaken, fixture.unsat);
      if (*utest_result) return;
    }
    if (canonical.err == SPN_OK) {
      assert_resolves_equal(utest_result, &canonical.query, &shaken.query);
      if (*utest_result) return;
    }
  }

  spn_resolve_query_t query = canonical.query;

  if (canonical.err != SPN_OK) {
    return;
  }

  sp_carr_for(fixture.expected, it) {
    expected_t expected = fixture.expected[it];
    if (!expected.name) break;

    sp_str_t qualified = spn_pkg_canonicalize_pair(sp_str_view(expected.namespace), sp_str_view(expected.name));

    bool found = false;
    sp_ht_for_kv(query.result, jt) {
      spn_resolved_pkg_t* pkg = jt.val;
      if (!sp_str_equal(pkg->qualified, qualified)) continue;
      if (!spn_semver_eq(pkg->version, expected.version)) continue;
      if (!resolved_in_unit(&query, pkg, expected.unit, expected.unit_version)) continue;
      found = true;
      break;
    }
    if (expected.excluded) {
      ASSERT_FALSE(found);
    } else {
      ASSERT_TRUE(found);
    }
  }

  sp_carr_for(fixture.instances, it) {
    instance_count_t instance = fixture.instances[it];
    if (!instance.name) break;

    sp_str_t qualified = spn_pkg_canonicalize_name(sp_str_view(instance.name));

    u32 count = 0;
    sp_ht_for_kv(query.result, jt) {
      if (sp_str_equal(jt.val->qualified, qualified)) count++;
    }
    ASSERT_EQ(count, instance.count);
  }
}

////////////
// CASES //
///////////
UTEST_F(resolver, none_resolves) {
  run_fixture(utest_result, (fixture_t) {
    .err = SPN_OK,
  });
}

UTEST_F(resolver, linear_resolves) {
  run_fixture(utest_result, (fixture_t) {
    .index = {
      {
        .namespace = "spn",
        .name = "math",
        .releases = {
          { .version = spn_semver_lit(1, 0, 0) },
          { .version = spn_semver_lit(1, 1, 0) },
          { .version = spn_semver_lit(2, 0, 0) },
        }
      }
    },
    .manifest = {
      .deps.package = {
        { .name = "spn/math", .version = "^1.0.0" },
      }
    },
    .err = SPN_OK,
    .expected = {
      { .name = "math", .namespace = "spn", .version = spn_semver_lit(1, 1, 0) },
    },
  });
}

UTEST_F(resolver, linear_missing) {
  run_fixture(utest_result, (fixture_t) {
    .manifest = {
      .deps.package = {
        { .name = "spn/math", .version = "^1.0.0" },
      }
    },
    .err = SPN_ERROR,
    .event = SPN_EVENT_ERR_UNKNOWN_PKG,
  });
}

UTEST_F(resolver, diamond_compatible) {
  run_fixture(utest_result, (fixture_t) {
    .index = {
      {
        .namespace = "spn",
        .name = "renderer",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "math", .version = "^1.0.0" },
            }
          },
        }
      },
      {
        .namespace = "spn",
        .name = "audio",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "math", .version = "^1.5.0" },
            }
          },
        }
      },
      {
        .namespace = "spn",
        .name = "math",
        .releases = {
          { .version = spn_semver_lit(1, 0, 0) },
          { .version = spn_semver_lit(1, 5, 0) },
          { .version = spn_semver_lit(1, 9, 0) },
          { .version = spn_semver_lit(2, 0, 0) },
        }
      },
    },
    .manifest = {
      .deps.package = {
        { .name = "spn/renderer", .version = "^1.0.0" },
        { .name = "spn/audio", .version = "^1.0.0" },
      }
    },
    .err = SPN_OK,
    .expected = {
      { .name = "renderer", .namespace = "spn", .version = spn_semver_lit(1, 0, 0) },
      { .name = "audio", .namespace = "spn", .version = spn_semver_lit(1, 0, 0) },
      { .name = "math", .namespace = "spn", .version = spn_semver_lit(1, 9, 0) },
    },
  });
}

UTEST_F(resolver, diamond_disjoint) {
  run_fixture(utest_result, (fixture_t) {
    .index = {
      {
        .namespace = "spn",
        .name = "renderer",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "math", .version = "^1.0.0" },
            }
          },
        }
      },
      {
        .namespace = "spn",
        .name = "audio",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "math", .version = "^2.0.0" },
            }
          },
        }
      },
      {
        .namespace = "spn",
        .name = "math",
        .releases = {
          { .version = spn_semver_lit(1, 0, 0) },
          { .version = spn_semver_lit(1, 5, 0) },
          { .version = spn_semver_lit(1, 9, 0) },
          { .version = spn_semver_lit(2, 0, 0) },
        }
      },
    },
    .manifest = {
      .deps.package = {
        { .name = "spn/renderer", .version = "^1.0.0" },
        { .name = "spn/audio", .version = "^1.0.0" },
      }
    },
    .err = SPN_ERROR,
    .event = SPN_EVENT_ERR_UNSATISFIABLE_VERSION,
  });
}

UTEST_F(resolver, diamond_missing_renderer) {
  run_fixture(utest_result, (fixture_t) {
    .index = {
      {
        .namespace = "spn",
        .name = "audio",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "math", .version = "^1.5.0" },
            }
          },
        }
      },
      {
        .namespace = "spn",
        .name = "math",
        .releases = {
          { .version = spn_semver_lit(1, 0, 0) },
          { .version = spn_semver_lit(1, 5, 0) },
          { .version = spn_semver_lit(1, 9, 0) },
          { .version = spn_semver_lit(2, 0, 0) },
        }
      },
    },
    .manifest = {
      .deps.package = {
        { .name = "spn/renderer", .version = "^1.0.0" },
        { .name = "spn/audio", .version = "^1.0.0" },
      }
    },
    .err = SPN_ERROR,
    .event = SPN_EVENT_ERR_UNKNOWN_PKG,
  });
}

UTEST_F(resolver, diamond_missing_audio) {
  run_fixture(utest_result, (fixture_t) {
    .index = {
      {
        .namespace = "spn",
        .name = "renderer",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "math", .version = "^1.0.0" },
            }
          },
        }
      },
      {
        .namespace = "spn",
        .name = "math",
        .releases = {
          { .version = spn_semver_lit(1, 0, 0) },
          { .version = spn_semver_lit(1, 5, 0) },
          { .version = spn_semver_lit(1, 9, 0) },
          { .version = spn_semver_lit(2, 0, 0) },
        }
      },
    },
    .manifest = {
      .deps.package = {
        { .name = "spn/renderer", .version = "^1.0.0" },
        { .name = "spn/audio", .version = "^1.0.0" },
      }
    },
    .err = SPN_ERROR,
    .event = SPN_EVENT_ERR_UNKNOWN_PKG,
  });
}

UTEST_F(resolver, diamond_missing_math) {
  run_fixture(utest_result, (fixture_t) {
    .index = {
      {
        .namespace = "spn",
        .name = "renderer",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "math", .version = "^1.0.0" },
            }
          },
        }
      },
      {
        .namespace = "spn",
        .name = "audio",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "math", .version = "^1.5.0" },
            }
          },
        }
      },
    },
    .manifest = {
      .deps.package = {
        { .name = "spn/renderer", .version = "^1.0.0" },
        { .name = "spn/audio", .version = "^1.0.0" },
      }
    },
    .err = SPN_ERROR,
    .event = SPN_EVENT_ERR_UNKNOWN_PKG,
  });
}

UTEST_F(resolver, cycle_direct) {
  run_fixture(utest_result, (fixture_t) {
    .index = {
      {
        .namespace = "spn",
        .name = "audio",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "math", .version = "^1.0.0" },
            }
          },
        }
      },
      {
        .namespace = "spn",
        .name = "math",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "audio", .version = "^1.0.0" },
            }
          },
        }
      },
    },
    .manifest = {
      .deps.package = {
        { .name = "spn/audio", .version = "^1.0.0" },
      }
    },
    .err = SPN_ERROR,
    .event = SPN_EVENT_ERR_CIRCULAR_DEP,
  });
}

UTEST_F(resolver, cycle_indirect) {
  run_fixture(utest_result, (fixture_t) {
    .index = {
      {
        .namespace = "spn",
        .name = "audio",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "math", .version = "^1.0.0" },
            }
          },
        }
      },
      {
        .namespace = "spn",
        .name = "math",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "renderer", .version = "^1.0.0" },
            }
          },
        }
      },
      {
        .namespace = "spn",
        .name = "renderer",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "audio", .version = "^1.0.0" },
            }
          },
        }
      },
    },
    .manifest = {
      .deps.package = {
        { .name = "spn/audio", .version = "^1.0.0" },
      }
    },
    .err = SPN_ERROR,
    .event = SPN_EVENT_ERR_CIRCULAR_DEP,
  });
}

UTEST_F(resolver, system_deps) {
  run_fixture(utest_result, (fixture_t) {
    .index = {
      {
        .namespace = "spn",
        .name = "renderer",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "math", .version = "^1.0.0" },
            }
          },
        }
      },
      {
        .namespace = "spn",
        .name = "audio",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "math", .version = "^1.5.0" },
            }
          },
        }
      },
      {
        .namespace = "spn",
        .name = "math",
        .releases = {
          { .version = spn_semver_lit(1, 0, 0) },
          { .version = spn_semver_lit(1, 5, 0) },
          { .version = spn_semver_lit(1, 9, 0) },
          { .version = spn_semver_lit(2, 0, 0) },
        }
      },
    },
    .manifest = {
      .deps = {
        .package = {
          { .name = "spn/renderer", .version = "^1.0.0" },
          { .name = "spn/audio", .version = "^1.0.0" },
        },
        .system = { "alsa", "x11" },
      }
    },
    .err = SPN_OK,
    .expected = {
      { .name = "renderer", .namespace = "spn", .version = spn_semver_lit(1, 0, 0) },
      { .name = "audio", .namespace = "spn", .version = spn_semver_lit(1, 0, 0) },
      { .name = "math", .namespace = "spn", .version = spn_semver_lit(1, 9, 0) },
    },
  });
}

UTEST_F(resolver, system_deps_dedup) {
  run_fixture(utest_result, (fixture_t) {
    .index = {
      {
        .namespace = "spn",
        .name = "renderer",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "math", .version = "^1.0.0" },
            }
          },
        }
      },
      {
        .namespace = "spn",
        .name = "audio",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "math", .version = "^1.5.0" },
            }
          },
        }
      },
      {
        .namespace = "spn",
        .name = "math",
        .releases = {
          { .version = spn_semver_lit(1, 0, 0) },
          { .version = spn_semver_lit(1, 5, 0) },
          { .version = spn_semver_lit(1, 9, 0) },
          { .version = spn_semver_lit(2, 0, 0) },
        }
      },
    },
    .manifest = {
      .deps = {
        .package = {
          { .name = "spn/renderer", .version = "^1.0.0" },
          { .name = "spn/audio", .version = "^1.0.0" },
        },
        .system = { "alsa" },
      }
    },
    .err = SPN_OK,
    .expected = {
      { .name = "renderer", .namespace = "spn", .version = spn_semver_lit(1, 0, 0) },
      { .name = "audio", .namespace = "spn", .version = spn_semver_lit(1, 0, 0) },
      { .name = "math", .namespace = "spn", .version = spn_semver_lit(1, 9, 0) },
    },
  });
}

UTEST_F(resolver, backtrack_simple) {
  // audio has 3 versions in ^1.0.0; 1.2.0 requires missing "physics", 1.1.0 and 1.0.0 are fine.
  // A correct resolver should pick audio 1.1.0 (newest valid).
  // The naive resolver uses 1.2.0's deps (latest in range) and fails on missing physics.
  run_fixture(utest_result, (fixture_t) {
    .index = {
      {
        .namespace = "spn",
        .name = "audio",
        .releases = {
          { .version = spn_semver_lit(1, 0, 0) },
          { .version = spn_semver_lit(1, 1, 0) },
          {
            .version = spn_semver_lit(1, 2, 0),
            .deps = {
              { .namespace = "spn", .name = "physics", .version = "^1.0.0" },
            }
          },
        }
      },
    },
    .manifest = {
      .deps.package = {
        { .name = "spn/audio", .version = "^1.0.0" },
      }
    },
    .err = SPN_OK,
    .expected = {
      { .name = "audio", .namespace = "spn", .version = spn_semver_lit(1, 1, 0) },
    },
  });
}

// The budget bounds search, never semantics: exhausting it is a hard error,
// never a fallback split, so identical inputs always produce the same answer
// or the same error. Same graph as backtrack_simple; a budget of one pop dies
// before the resolvable audio 1.1.0 is ever tried.
UTEST_F(resolver, budget_exhausted_fails) {
  run_fixture(utest_result, (fixture_t) {
    .index = {
      {
        .namespace = "spn",
        .name = "audio",
        .releases = {
          { .version = spn_semver_lit(1, 0, 0) },
          { .version = spn_semver_lit(1, 1, 0) },
          {
            .version = spn_semver_lit(1, 2, 0),
            .deps = {
              { .namespace = "spn", .name = "physics", .version = "^1.0.0" },
            }
          },
        }
      },
    },
    .manifest = {
      .deps.package = {
        { .name = "spn/audio", .version = "^1.0.0" },
      }
    },
    .budget = 1,
    .err = SPN_ERROR,
    .event = SPN_EVENT_ERR_RESOLUTION_TOO_COMPLEX,
  });
}

UTEST_F(resolver, backtrack_transitive_missing) {
  // audio has 2 versions within ^1.0.0:
  //   1.0.0 is fine (requires math ^1.0.0)
  //   1.1.0 requires missing "physics"
  // A correct resolver should backtrack from 1.1.0 and pick audio 1.0.0.
  // The naive resolver uses 1.1.0's deps (latest) and fails on missing physics.
  run_fixture(utest_result, (fixture_t) {
    .index = {
      {
        .namespace = "spn",
        .name = "audio",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "math", .version = "^1.0.0" },
            }
          },
          {
            .version = spn_semver_lit(1, 1, 0),
            .deps = {
              { .namespace = "spn", .name = "physics", .version = "^1.0.0" },
            }
          },
        }
      },
      {
        .namespace = "spn",
        .name = "math",
        .releases = {
          { .version = spn_semver_lit(1, 0, 0) },
        }
      },
    },
    .manifest = {
      .deps.package = {
        { .name = "spn/audio", .version = "^1.0.0" },
      }
    },
    .err = SPN_OK,
    .expected = {
      { .name = "audio", .namespace = "spn", .version = spn_semver_lit(1, 0, 0) },
      { .name = "math", .namespace = "spn", .version = spn_semver_lit(1, 0, 0) },
    },
  });
}

UTEST_F(resolver, backtrack_divergent_deps) {
  // renderer has 2 versions with different deps:
  //   1.0.0 depends on math ^1.0.0
  //   2.0.0 depends on math ^2.0.0
  // Root requires renderer ^1.0.0 (should pick 1.0.0).
  // Naive resolver uses 2.0.0's deps (latest) for the transitive walk,
  // so it adds math ^2.0.0 constraint instead of ^1.0.0.
  // math only has 1.0.0, so naive resolver either picks wrong version or fails.
  run_fixture(utest_result, (fixture_t) {
    .index = {
      {
        .namespace = "spn",
        .name = "renderer",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "math", .version = "^1.0.0" },
            }
          },
          {
            .version = spn_semver_lit(2, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "math", .version = "^2.0.0" },
            }
          },
        }
      },
      {
        .namespace = "spn",
        .name = "math",
        .releases = {
          { .version = spn_semver_lit(1, 0, 0) },
        }
      },
    },
    .manifest = {
      .deps.package = {
        { .name = "spn/renderer", .version = "^1.0.0" },
      }
    },
    .err = SPN_OK,
    .expected = {
      { .name = "renderer", .namespace = "spn", .version = spn_semver_lit(1, 0, 0) },
      { .name = "math", .namespace = "spn", .version = spn_semver_lit(1, 0, 0) },
    },
  });
}

UTEST_F(resolver, root_transitive_conflict) {
  run_fixture(utest_result, (fixture_t) {
    .index = {
      {
        .namespace = "spn",
        .name = "audio",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "math", .version = "^2.0.0" },
            }
          },
        }
      },
      {
        .namespace = "spn",
        .name = "math",
        .releases = {
          { .version = spn_semver_lit(1, 0, 0) },
          { .version = spn_semver_lit(2, 0, 0) },
        }
      },
    },
    .manifest = {
      .deps.package = {
        { .name = "spn/audio", .version = "^1.0.0" },
        { .name = "spn/math", .version = "^1.0.0" },
      }
    },
    .err = SPN_ERROR,
  });
}

UTEST_F(resolver, version_no_match) {
  run_fixture(utest_result, (fixture_t) {
    .index = {
      {
        .namespace = "spn",
        .name = "math",
        .releases = {
          { .version = spn_semver_lit(0, 1, 0) },
          { .version = spn_semver_lit(1, 0, 0) },
        }
      }
    },
    .manifest = {
      .deps.package = {
        { .name = "spn/math", .version = "^2.0.0" },
      }
    },
    .err = SPN_ERROR,
    .event = SPN_EVENT_ERR_UNSATISFIABLE_VERSION,
    .unsat = { .namespace = "spn", .name = "math", .requester = "test/root" },
  });
}

// A conflict is reported as a conflict: the failure names the version the
// scope already selected and who issued the losing request, never "no
// version satisfies" when satisfying versions exist
UTEST_F(resolver, conflict_reports_selected) {
  run_fixture(utest_result, (fixture_t) {
    .index = {
      {
        .namespace = "spn",
        .name = "b",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "a", .version = "^2.0.0" },
            }
          },
        }
      },
      {
        .namespace = "spn",
        .name = "a",
        .releases = {
          { .version = spn_semver_lit(1, 0, 0) },
          { .version = spn_semver_lit(2, 0, 0) },
        }
      },
    },
    .manifest = {
      .deps.package = {
        { .name = "spn/a", .version = "^1.0.0" },
        { .name = "spn/b", .version = "^1.0.0" },
      }
    },
    .err = SPN_ERROR,
    .event = SPN_EVENT_ERR_UNSATISFIABLE_VERSION,
    .unsat = {
      .namespace = "spn",
      .name = "a",
      .requester = "spn/b",
      .selected = "1.0.0",
    },
  });
}

UTEST_F(resolver, smoke) {
  run_fixture(utest_result, (fixture_t) {
    .index = {
      {
        .namespace = "spn",
        .name = "renderer",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "math", .version = "^1.0.0" },
            }
          },
        }
      },
      {
        .namespace = "spn",
        .name = "audio",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "math", .version = "^1.0.0" },
            }
          },
        }
      },
      {
        .namespace = "spn",
        .name = "math",
        .releases = {
          { .version = spn_semver_lit(1, 0, 0) },
        }
      },
    },
    .manifest = {
      .deps.package = {
        { .name = "spn/renderer", .version = "^1.0.0" },
        { .name = "spn/audio", .version = "^1.0.0" },
      }
    },
    .err = SPN_OK,
    .expected = {
      { .name = "renderer", .namespace = "spn", .version = spn_semver_lit(1, 0, 0) },
      { .name = "audio", .namespace = "spn", .version = spn_semver_lit(1, 0, 0) },
      { .name = "math", .namespace = "spn", .version = spn_semver_lit(1, 0, 0) },
    },
  });
}

/////////////////
// LINK UNITS //
////////////////
// A build dep never links into the product, so it may resolve to a version
// that conflicts with the linked one. The build dep roots its own unit.
UTEST_F(resolver, build_dep_root_conflict) {
  run_fixture(utest_result, (fixture_t) {
    .index = {
      {
        .namespace = "spn",
        .name = "foo",
        .releases = {
          { .version = spn_semver_lit(1, 0, 0) },
          { .version = spn_semver_lit(2, 0, 0) },
        }
      },
    },
    .manifest = {
      .deps.package = {
        { .name = "spn/foo", .version = "^2.0.0", .kind = SPN_DEP_KIND_PACKAGE },
        { .name = "spn/foo", .version = "^1.0.0", .kind = SPN_DEP_KIND_BUILD },
      }
    },
    .err = SPN_OK,
    .expected = {
      { .name = "foo", .namespace = "spn", .version = spn_semver_lit(2, 0, 0), .unit = "" },
      { .name = "foo", .namespace = "spn", .version = spn_semver_lit(1, 0, 0), .unit = "spn/foo" },
    },
    .instances = {
      { .name = "spn/foo", .count = 2 },
    },
  });
}

// Same conflict, but the build dep edge is inside a dependency rather than
// on the root
UTEST_F(resolver, build_dep_transitive_conflict) {
  run_fixture(utest_result, (fixture_t) {
    .index = {
      {
        .namespace = "spn",
        .name = "renderer",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "foo", .version = "^1.0.0", .kind = SPN_INDEX_DEP_BUILD },
            }
          },
        }
      },
      {
        .namespace = "spn",
        .name = "foo",
        .releases = {
          { .version = spn_semver_lit(1, 0, 0) },
          { .version = spn_semver_lit(2, 0, 0) },
        }
      },
    },
    .manifest = {
      .deps.package = {
        { .name = "spn/renderer", .version = "^1.0.0" },
        { .name = "spn/foo", .version = "^2.0.0" },
      }
    },
    .err = SPN_OK,
    .expected = {
      { .name = "renderer", .namespace = "spn", .version = spn_semver_lit(1, 0, 0), .unit = "" },
      { .name = "foo", .namespace = "spn", .version = spn_semver_lit(2, 0, 0), .unit = "" },
      { .name = "foo", .namespace = "spn", .version = spn_semver_lit(1, 0, 0), .unit = "spn/foo" },
    },
    .instances = {
      { .name = "spn/foo", .count = 2 },
    },
  });
}

// Root test deps build into test executables, not the product, so they may
// conflict with linked deps just like build deps
UTEST_F(resolver, test_dep_root_conflict) {
  run_fixture(utest_result, (fixture_t) {
    .index = {
      {
        .namespace = "spn",
        .name = "foo",
        .releases = {
          { .version = spn_semver_lit(1, 0, 0) },
          { .version = spn_semver_lit(2, 0, 0) },
        }
      },
    },
    .manifest = {
      .deps.package = {
        { .name = "spn/foo", .version = "^2.0.0", .kind = SPN_DEP_KIND_PACKAGE },
        { .name = "spn/foo", .version = "^1.0.0", .kind = SPN_DEP_KIND_TEST },
      }
    },
    .err = SPN_OK,
    .expected = {
      { .name = "foo", .namespace = "spn", .version = spn_semver_lit(2, 0, 0), .unit = "" },
      { .name = "foo", .namespace = "spn", .version = spn_semver_lit(1, 0, 0), .unit = "spn/foo" },
    },
    .instances = {
      { .name = "spn/foo", .count = 2 },
    },
  });
}

// A dependency's test deps never build at all in a consumer's graph; they
// must be pruned rather than resolved into a conflict
UTEST_F(resolver, transitive_test_dep_pruned) {
  run_fixture(utest_result, (fixture_t) {
    .index = {
      {
        .namespace = "spn",
        .name = "renderer",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "foo", .version = "^1.0.0", .kind = SPN_INDEX_DEP_TEST },
            }
          },
        }
      },
      {
        .namespace = "spn",
        .name = "foo",
        .releases = {
          { .version = spn_semver_lit(1, 0, 0) },
          { .version = spn_semver_lit(2, 0, 0) },
        }
      },
    },
    .manifest = {
      .deps.package = {
        { .name = "spn/renderer", .version = "^1.0.0" },
        { .name = "spn/foo", .version = "^2.0.0" },
      }
    },
    .err = SPN_OK,
    .expected = {
      { .name = "renderer", .namespace = "spn", .version = spn_semver_lit(1, 0, 0), .unit = "" },
      { .name = "foo", .namespace = "spn", .version = spn_semver_lit(2, 0, 0), .unit = "" },
    },
    .instances = {
      { .name = "spn/foo", .count = 1 },
    },
  });
}

// Compatible ranges across units must unify onto one instance; splitting
// resolution is never license to build a package twice unnecessarily
UTEST_F(resolver, build_dep_compatible_unifies) {
  run_fixture(utest_result, (fixture_t) {
    .index = {
      {
        .namespace = "spn",
        .name = "tool",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "foo", .version = "^1.0.0" },
            }
          },
        }
      },
      {
        .namespace = "spn",
        .name = "foo",
        .releases = {
          { .version = spn_semver_lit(1, 5, 0) },
          { .version = spn_semver_lit(1, 9, 0) },
          { .version = spn_semver_lit(2, 0, 0) },
        }
      },
    },
    .manifest = {
      .deps.package = {
        { .name = "spn/foo", .version = "^1.0.0", .kind = SPN_DEP_KIND_PACKAGE },
        { .name = "spn/tool", .version = "^1.0.0", .kind = SPN_DEP_KIND_BUILD },
      }
    },
    .err = SPN_OK,
    .expected = {
      { .name = "foo", .namespace = "spn", .version = spn_semver_lit(1, 9, 0), .unit = "" },
      { .name = "tool", .namespace = "spn", .version = spn_semver_lit(1, 0, 0) },
    },
    .instances = {
      { .name = "spn/foo", .count = 1 },
      { .name = "spn/tool", .count = 1 },
    },
  });
}

// The build unit's range admits 2.0.0, but the root unit already picked
// 1.9.0; the solver must prefer the already-picked version over the newest
UTEST_F(resolver, preference_prefers_unified) {
  run_fixture(utest_result, (fixture_t) {
    .index = {
      {
        .namespace = "spn",
        .name = "foo",
        .releases = {
          { .version = spn_semver_lit(1, 5, 0) },
          { .version = spn_semver_lit(1, 9, 0) },
          { .version = spn_semver_lit(2, 0, 0) },
        }
      },
    },
    .manifest = {
      .deps.package = {
        { .name = "spn/foo", .version = "^1.5.0", .kind = SPN_DEP_KIND_PACKAGE },
        { .name = "spn/foo", .version = ">=1.5.0", .kind = SPN_DEP_KIND_BUILD },
      }
    },
    .err = SPN_OK,
    .expected = {
      { .name = "foo", .namespace = "spn", .version = spn_semver_lit(1, 9, 0), .unit = "" },
      { .name = "foo", .namespace = "spn", .version = spn_semver_lit(1, 9, 0), .unit = "spn/foo" },
    },
    .instances = {
      { .name = "spn/foo", .count = 1 },
    },
  });
}

// The tool's pin admits a version the root's range also admits, so pooling
// constraints would downgrade the root to 1.0.0 to share one instance. The
// root's pick is sovereign; the tool pays with its own copy instead
UTEST_F(resolver, build_dep_never_constrains_root) {
  run_fixture(utest_result, (fixture_t) {
    .index = {
      {
        .namespace = "spn",
        .name = "foo",
        .releases = {
          { .version = spn_semver_lit(1, 0, 0) },
          { .version = spn_semver_lit(1, 9, 0) },
        }
      },
      {
        .namespace = "spn",
        .name = "tool",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "foo", .version = "=1.0.0" },
            }
          },
        }
      },
    },
    .manifest = {
      .deps.package = {
        { .name = "spn/foo", .version = "^1.0.0", .kind = SPN_DEP_KIND_PACKAGE },
        { .name = "spn/tool", .version = "^1.0.0", .kind = SPN_DEP_KIND_BUILD },
      }
    },
    .err = SPN_OK,
    .expected = {
      { .name = "foo", .namespace = "spn", .version = spn_semver_lit(1, 9, 0), .unit = "" },
      { .name = "foo", .namespace = "spn", .version = spn_semver_lit(1, 0, 0), .unit = "spn/tool" },
    },
    .instances = {
      { .name = "spn/foo", .count = 2 },
    },
  });
}

// Convergence is a constraint, not a preference: the newest bar needs a foo
// the root's pick excludes, so the tool's unit must fall back to the older
// bar that shares the root's foo rather than split a second foo
UTEST_F(resolver, convergence_forces_older_sibling) {
  run_fixture(utest_result, (fixture_t) {
    .index = {
      {
        .namespace = "spn",
        .name = "tool",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "bar", .version = "^1.0.0" },
            }
          },
        }
      },
      {
        .namespace = "spn",
        .name = "bar",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "foo", .version = "^1.0.0" },
            }
          },
          {
            .version = spn_semver_lit(1, 1, 0),
            .deps = {
              { .namespace = "spn", .name = "foo", .version = "^2.0.0" },
            }
          },
        }
      },
      {
        .namespace = "spn",
        .name = "foo",
        .releases = {
          { .version = spn_semver_lit(1, 9, 0) },
          { .version = spn_semver_lit(2, 0, 0) },
        }
      },
    },
    .manifest = {
      .deps.package = {
        { .name = "spn/foo", .version = "^1.0.0", .kind = SPN_DEP_KIND_PACKAGE },
        { .name = "spn/tool", .version = "^1.0.0", .kind = SPN_DEP_KIND_BUILD },
      }
    },
    .err = SPN_OK,
    .expected = {
      { .name = "foo", .namespace = "spn", .version = spn_semver_lit(1, 9, 0), .unit = "" },
      { .name = "foo", .namespace = "spn", .version = spn_semver_lit(1, 9, 0), .unit = "spn/tool" },
      { .name = "bar", .namespace = "spn", .version = spn_semver_lit(1, 0, 0), .unit = "spn/tool" },
    },
    .instances = {
      { .name = "spn/foo", .count = 1 },
      { .name = "spn/bar", .count = 1 },
    },
  });
}

// A group inheriting two decided versions of one name takes the earliest
// admissible priority, not the newest version: opt's range admits both the
// root's foo 1.9.0 and gen's split foo 2.0.0, and the root decided first.
// Today's preference draws candidates in hash iteration order, so this pin
// flickers with intern state — the order dependence is the defect it pins.
UTEST_F(resolver, tiebreak_takes_earliest_admissible) {
  run_fixture(utest_result, (fixture_t) {
    .index = {
      {
        .namespace = "spn",
        .name = "gen",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "foo", .version = "=2.0.0" },
            }
          },
        }
      },
      {
        .namespace = "spn",
        .name = "opt",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "foo", .version = ">=1.0.0" },
            }
          },
        }
      },
      {
        .namespace = "spn",
        .name = "foo",
        .releases = {
          { .version = spn_semver_lit(1, 0, 0) },
          { .version = spn_semver_lit(1, 9, 0) },
          { .version = spn_semver_lit(2, 0, 0) },
        }
      },
    },
    .manifest = {
      .deps.package = {
        { .name = "spn/foo", .version = "^1.0.0" },
        { .name = "spn/gen", .version = "^1.0.0", .kind = SPN_DEP_KIND_BUILD },
        { .name = "spn/opt", .version = "^1.0.0", .kind = SPN_DEP_KIND_BUILD },
      }
    },
    .err = SPN_OK,
    .expected = {
      { .name = "foo", .namespace = "spn", .version = spn_semver_lit(1, 9, 0), .unit = "" },
      { .name = "foo", .namespace = "spn", .version = spn_semver_lit(2, 0, 0), .unit = "spn/gen" },
      { .name = "foo", .namespace = "spn", .version = spn_semver_lit(1, 9, 0), .unit = "spn/opt" },
    },
    .instances = {
      { .name = "spn/foo", .count = 2 },
    },
  });
}

// Mutually exclusive inherited picks: use can hold the root's c 1.0.0 or mk's
// b 2.0.0 (which needs c 2.0.0), never both. The root's pick has higher
// priority, so c pins and b is the loser that splits — regardless of the
// order use's own requests discover the names
UTEST_F(resolver, tiebreak_higher_priority_pins_loser_splits) {
  run_fixture(utest_result, (fixture_t) {
    .index = {
      {
        .namespace = "spn",
        .name = "mk",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "b", .version = "^2.0.0" },
            }
          },
        }
      },
      {
        .namespace = "spn",
        .name = "use",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "b", .version = ">=1.0.0" },
              { .namespace = "spn", .name = "c", .version = ">=1.0.0" },
            }
          },
        }
      },
      {
        .namespace = "spn",
        .name = "b",
        .releases = {
          { .version = spn_semver_lit(1, 0, 0) },
          {
            .version = spn_semver_lit(2, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "c", .version = "=2.0.0" },
            }
          },
        }
      },
      {
        .namespace = "spn",
        .name = "c",
        .releases = {
          { .version = spn_semver_lit(1, 0, 0) },
          { .version = spn_semver_lit(2, 0, 0) },
        }
      },
    },
    .manifest = {
      .deps.package = {
        { .name = "spn/c", .version = "=1.0.0" },
        { .name = "spn/mk", .version = "^1.0.0", .kind = SPN_DEP_KIND_BUILD },
        { .name = "spn/use", .version = "^1.0.0", .kind = SPN_DEP_KIND_BUILD },
      }
    },
    .err = SPN_OK,
    .expected = {
      { .name = "c", .namespace = "spn", .version = spn_semver_lit(1, 0, 0), .unit = "" },
      { .name = "b", .namespace = "spn", .version = spn_semver_lit(2, 0, 0), .unit = "spn/mk" },
      { .name = "c", .namespace = "spn", .version = spn_semver_lit(2, 0, 0), .unit = "spn/mk" },
      { .name = "c", .namespace = "spn", .version = spn_semver_lit(1, 0, 0), .unit = "spn/use" },
      { .name = "b", .namespace = "spn", .version = spn_semver_lit(1, 0, 0), .unit = "spn/use" },
    },
    .instances = {
      { .name = "spn/b", .count = 2 },
      { .name = "spn/c", .count = 2 },
    },
  });
}

// Deps are public by default: their types may appear in the package's API, so
// they resolve in the consumer's scope and a conflict stays an error even
// across a shared (dynamic) boundary
UTEST_F(resolver, shared_lib_public_conflict_fails) {
  run_fixture(utest_result, (fixture_t) {
    .index = {
      {
        .namespace = "spn",
        .name = "gfx",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "foo", .version = "^1.0.0" },
            },
            .targets = {
              { .name = "gfx", .linkages = { SPN_LIB_KIND_SHARED } },
            }
          },
        }
      },
      {
        .namespace = "spn",
        .name = "foo",
        .releases = {
          { .version = spn_semver_lit(1, 0, 0) },
          { .version = spn_semver_lit(2, 0, 0) },
        }
      },
    },
    .manifest = {
      .deps.package = {
        { .name = "spn/gfx", .version = "^1.0.0" },
        { .name = "spn/foo", .version = "^2.0.0" },
      }
    },
    .err = SPN_ERROR,
    .event = SPN_EVENT_ERR_UNSATISFIABLE_VERSION,
  });
}

// The public default holds when the shared lib is discovered transitively,
// not just on a direct dep of the root
UTEST_F(resolver, shared_lib_transitive_conflict_fails) {
  run_fixture(utest_result, (fixture_t) {
    .index = {
      {
        .namespace = "spn",
        .name = "app",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "gfx", .version = "^1.0.0" },
            }
          },
        }
      },
      {
        .namespace = "spn",
        .name = "gfx",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "foo", .version = "^1.0.0" },
            },
            .targets = {
              { .name = "gfx", .linkages = { SPN_LIB_KIND_SHARED } },
            }
          },
        }
      },
      {
        .namespace = "spn",
        .name = "foo",
        .releases = {
          { .version = spn_semver_lit(1, 0, 0) },
          { .version = spn_semver_lit(2, 0, 0) },
        }
      },
    },
    .manifest = {
      .deps.package = {
        { .name = "spn/app", .version = "^1.0.0" },
        { .name = "spn/foo", .version = "^2.0.0" },
      }
    },
    .err = SPN_ERROR,
    .event = SPN_EVENT_ERR_UNSATISFIABLE_VERSION,
  });
}

// A dep declared private is an implementation detail: behind a shared
// boundary its subtree resolves in gfx's own scope and may diverge from the
// consumer's picks. gfx.so carries its own foo 1.0.0 copy, symbols hidden.
UTEST_F(resolver, shared_lib_private_diverges) {
  run_fixture(utest_result, (fixture_t) {
    .index = {
      {
        .namespace = "spn",
        .name = "gfx",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "foo", .version = "^1.0.0", .private = true },
            },
            .targets = {
              { .name = "gfx", .linkages = { SPN_LIB_KIND_SHARED } },
            }
          },
        }
      },
      {
        .namespace = "spn",
        .name = "foo",
        .releases = {
          { .version = spn_semver_lit(1, 0, 0) },
          { .version = spn_semver_lit(2, 0, 0) },
        }
      },
    },
    .manifest = {
      .deps.package = {
        { .name = "spn/gfx", .version = "^1.0.0" },
        { .name = "spn/foo", .version = "^2.0.0" },
      }
    },
    .err = SPN_OK,
    .expected = {
      { .name = "gfx", .namespace = "spn", .version = spn_semver_lit(1, 0, 0) },
      { .name = "foo", .namespace = "spn", .version = spn_semver_lit(2, 0, 0), .unit = "" },
      { .name = "foo", .namespace = "spn", .version = spn_semver_lit(1, 0, 0), .unit = "spn/gfx" },
    },
    .instances = {
      { .name = "spn/foo", .count = 2 },
      { .name = "spn/gfx", .count = 1 },
    },
  });
}

// Privacy alone never costs a duplicate: gfx's private range admits foo
// 2.0.0, which an isolated solve would take, but the root already decided
// 1.9.0 and the private unit must converge onto that instance
UTEST_F(resolver, private_compatible_unifies) {
  run_fixture(utest_result, (fixture_t) {
    .index = {
      {
        .namespace = "spn",
        .name = "gfx",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "foo", .version = ">=1.0.0", .private = true },
            },
            .targets = {
              { .name = "gfx", .linkages = { SPN_LIB_KIND_SHARED } },
            }
          },
        }
      },
      {
        .namespace = "spn",
        .name = "foo",
        .releases = {
          { .version = spn_semver_lit(1, 0, 0) },
          { .version = spn_semver_lit(1, 9, 0) },
          { .version = spn_semver_lit(2, 0, 0) },
        }
      },
    },
    .manifest = {
      .deps.package = {
        { .name = "spn/gfx", .version = "^1.0.0" },
        { .name = "spn/foo", .version = "^1.0.0" },
      }
    },
    .err = SPN_OK,
    .expected = {
      { .name = "gfx", .namespace = "spn", .version = spn_semver_lit(1, 0, 0), .unit = "" },
      { .name = "foo", .namespace = "spn", .version = spn_semver_lit(1, 9, 0), .unit = "" },
      { .name = "foo", .namespace = "spn", .version = spn_semver_lit(1, 9, 0), .unit = "spn/gfx" },
    },
    .instances = {
      { .name = "spn/foo", .count = 1 },
      { .name = "spn/gfx", .count = 1 },
    },
  });
}

// Privacy inherits down the private edge: bar is private to gfx, so bar's
// *public* dep baz is still private to gfx. baz unifies with bar inside gfx's
// scope, not with the root's baz.
UTEST_F(resolver, shared_lib_private_transitive_diverges) {
  run_fixture(utest_result, (fixture_t) {
    .index = {
      {
        .namespace = "spn",
        .name = "gfx",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "bar", .version = "^1.0.0", .private = true },
            },
            .targets = {
              { .name = "gfx", .linkages = { SPN_LIB_KIND_SHARED } },
            }
          },
        }
      },
      {
        .namespace = "spn",
        .name = "bar",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "baz", .version = "^1.0.0" },
            }
          },
        }
      },
      {
        .namespace = "spn",
        .name = "baz",
        .releases = {
          { .version = spn_semver_lit(1, 0, 0) },
          { .version = spn_semver_lit(2, 0, 0) },
        }
      },
    },
    .manifest = {
      .deps.package = {
        { .name = "spn/gfx", .version = "^1.0.0" },
        { .name = "spn/baz", .version = "^2.0.0" },
      }
    },
    .err = SPN_OK,
    .expected = {
      { .name = "baz", .namespace = "spn", .version = spn_semver_lit(2, 0, 0), .unit = "" },
      { .name = "bar", .namespace = "spn", .version = spn_semver_lit(1, 0, 0), .unit = "spn/gfx" },
      { .name = "baz", .namespace = "spn", .version = spn_semver_lit(1, 0, 0), .unit = "spn/gfx" },
    },
    .instances = {
      { .name = "spn/baz", .count = 2 },
      { .name = "spn/bar", .count = 1 },
    },
  });
}

// The static twin of shared_lib_public_conflict_fails: identical topology,
// identical outcome
UTEST_F(resolver, static_lib_conflict_fails) {
  run_fixture(utest_result, (fixture_t) {
    .index = {
      {
        .namespace = "spn",
        .name = "gfx",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "foo", .version = "^1.0.0" },
            },
            .targets = {
              { .name = "gfx", .linkages = { SPN_LIB_KIND_STATIC } },
            }
          },
        }
      },
      {
        .namespace = "spn",
        .name = "foo",
        .releases = {
          { .version = spn_semver_lit(1, 0, 0) },
          { .version = spn_semver_lit(2, 0, 0) },
        }
      },
    },
    .manifest = {
      .deps.package = {
        { .name = "spn/gfx", .version = "^1.0.0" },
        { .name = "spn/foo", .version = "^2.0.0" },
      }
    },
    .err = SPN_ERROR,
    .event = SPN_EVENT_ERR_UNSATISFIABLE_VERSION,
  });
}

// Privacy never splits a link unit: a static gfx links into its consumer, so
// its private foo lands on the same link line as the root's foo and the
// conflict stays an error
UTEST_F(resolver, static_lib_private_conflict_fails) {
  run_fixture(utest_result, (fixture_t) {
    .index = {
      {
        .namespace = "spn",
        .name = "gfx",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "foo", .version = "^1.0.0", .private = true },
            },
            .targets = {
              { .name = "gfx", .linkages = { SPN_LIB_KIND_STATIC } },
            }
          },
        }
      },
      {
        .namespace = "spn",
        .name = "foo",
        .releases = {
          { .version = spn_semver_lit(1, 0, 0) },
          { .version = spn_semver_lit(2, 0, 0) },
        }
      },
    },
    .manifest = {
      .deps.package = {
        { .name = "spn/gfx", .version = "^1.0.0" },
        { .name = "spn/foo", .version = "^2.0.0" },
      }
    },
    .err = SPN_ERROR,
    .event = SPN_EVENT_ERR_UNSATISFIABLE_VERSION,
  });
}

// Boundaries are classified by the linkage selected for this build, not by
// capability: gfx offers static and shared, default selection picks static,
// so gfx links into its consumer and its private foo conflicts exactly as a
// static-only gfx's would
UTEST_F(resolver, private_static_default_conflicts) {
  run_fixture(utest_result, (fixture_t) {
    .index = {
      {
        .namespace = "spn",
        .name = "gfx",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "foo", .version = "^1.0.0", .private = true },
            },
            .targets = {
              { .name = "gfx", .linkages = { SPN_LIB_KIND_STATIC, SPN_LIB_KIND_SHARED } },
            }
          },
        }
      },
      {
        .namespace = "spn",
        .name = "foo",
        .releases = {
          { .version = spn_semver_lit(1, 0, 0) },
          { .version = spn_semver_lit(2, 0, 0) },
        }
      },
    },
    .manifest = {
      .deps.package = {
        { .name = "spn/gfx", .version = "^1.0.0" },
        { .name = "spn/foo", .version = "^2.0.0" },
      }
    },
    .err = SPN_ERROR,
    .event = SPN_EVENT_ERR_UNSATISFIABLE_VERSION,
  });
}

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
UTEST_F(resolver, config_shared_private_diverges) {
  run_fixture(utest_result, (fixture_t) {
    .index = {
      {
        .namespace = "spn",
        .name = "gfx",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "foo", .version = "^1.0.0", .private = true },
            },
            .targets = {
              { .name = "gfx", .linkages = { SPN_LIB_KIND_STATIC, SPN_LIB_KIND_SHARED } },
            }
          },
        }
      },
      {
        .namespace = "spn",
        .name = "foo",
        .releases = {
          { .version = spn_semver_lit(1, 0, 0) },
          { .version = spn_semver_lit(2, 0, 0) },
        }
      },
    },
    .manifest = {
      .deps.package = {
        { .name = "spn/gfx", .version = "^1.0.0" },
        { .name = "spn/foo", .version = "^2.0.0" },
      }
    },
    .config = {
      { .name = "gfx", .kind = SPN_LIB_KIND_SHARED },
    },
    .err = SPN_OK,
    .expected = {
      { .name = "gfx", .namespace = "spn", .version = spn_semver_lit(1, 0, 0) },
      { .name = "foo", .namespace = "spn", .version = spn_semver_lit(2, 0, 0), .unit = "" },
      { .name = "foo", .namespace = "spn", .version = spn_semver_lit(1, 0, 0), .unit = "spn/gfx" },
    },
    .instances = {
      { .name = "spn/foo", .count = 2 },
      { .name = "spn/gfx", .count = 1 },
    },
  });
}

// Two consumers of one shared lib share one instance of it, picked from the
// intersection of their ranges
UTEST_F(resolver, shared_lib_consumers_unify) {
  run_fixture(utest_result, (fixture_t) {
    .index = {
      {
        .namespace = "spn",
        .name = "audio",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "gfx", .version = "^1.2.0" },
            }
          },
        }
      },
      {
        .namespace = "spn",
        .name = "video",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "gfx", .version = "^1.4.0" },
            }
          },
        }
      },
      {
        .namespace = "spn",
        .name = "gfx",
        .releases = {
          { .version = spn_semver_lit(1, 2, 0), .targets = { { .name = "gfx", .linkages = { SPN_LIB_KIND_SHARED } } } },
          { .version = spn_semver_lit(1, 4, 0), .targets = { { .name = "gfx", .linkages = { SPN_LIB_KIND_SHARED } } } },
          { .version = spn_semver_lit(1, 9, 0), .targets = { { .name = "gfx", .linkages = { SPN_LIB_KIND_SHARED } } } },
        }
      },
    },
    .manifest = {
      .deps.package = {
        { .name = "spn/audio", .version = "^1.0.0" },
        { .name = "spn/video", .version = "^1.0.0" },
      }
    },
    .err = SPN_OK,
    .expected = {
      { .name = "gfx", .namespace = "spn", .version = spn_semver_lit(1, 9, 0) },
    },
    .instances = {
      { .name = "spn/gfx", .count = 1 },
    },
  });
}

// Two consumers with disjoint ranges on one shared lib would load two gfx.so
// instances into the root's process. The loader dedupes dynamic libs by name,
// so this is physics, not policy: an error regardless of privacy
UTEST_F(resolver, shared_lib_consumer_disjoint_fails) {
  run_fixture(utest_result, (fixture_t) {
    .index = {
      {
        .namespace = "spn",
        .name = "audio",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "gfx", .version = "^1.0.0" },
            }
          },
        }
      },
      {
        .namespace = "spn",
        .name = "video",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "gfx", .version = "^2.0.0" },
            }
          },
        }
      },
      {
        .namespace = "spn",
        .name = "gfx",
        .releases = {
          { .version = spn_semver_lit(1, 9, 0), .targets = { { .name = "gfx", .linkages = { SPN_LIB_KIND_SHARED } } } },
          { .version = spn_semver_lit(2, 0, 0), .targets = { { .name = "gfx", .linkages = { SPN_LIB_KIND_SHARED } } } },
        }
      },
    },
    .manifest = {
      .deps.package = {
        { .name = "spn/audio", .version = "^1.0.0" },
        { .name = "spn/video", .version = "^1.0.0" },
      }
    },
    .err = SPN_ERROR,
    .event = SPN_EVENT_ERR_UNSATISFIABLE_VERSION,
  });
}

// Privacy can't dodge the dynamic rule: audio.so and video.so each privately
// want a disjoint gfx, but gfx only builds shared, so both copies would load
// into the root's process. Divergence requires the private dep to embed
// static; a shared-only private dep with disjoint consumers is an error.
UTEST_F(resolver, shared_lib_private_dynamic_dup_fails) {
  run_fixture(utest_result, (fixture_t) {
    .index = {
      {
        .namespace = "spn",
        .name = "audio",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "gfx", .version = "^1.0.0", .private = true },
            },
            .targets = {
              { .name = "audio", .linkages = { SPN_LIB_KIND_SHARED } },
            }
          },
        }
      },
      {
        .namespace = "spn",
        .name = "video",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "gfx", .version = "^2.0.0", .private = true },
            },
            .targets = {
              { .name = "video", .linkages = { SPN_LIB_KIND_SHARED } },
            }
          },
        }
      },
      {
        .namespace = "spn",
        .name = "gfx",
        .releases = {
          { .version = spn_semver_lit(1, 9, 0), .targets = { { .name = "gfx", .linkages = { SPN_LIB_KIND_SHARED } } } },
          { .version = spn_semver_lit(2, 0, 0), .targets = { { .name = "gfx", .linkages = { SPN_LIB_KIND_SHARED } } } },
        }
      },
    },
    .manifest = {
      .deps.package = {
        { .name = "spn/audio", .version = "^1.0.0" },
        { .name = "spn/video", .version = "^1.0.0" },
      }
    },
    .err = SPN_ERROR,
    .event = SPN_EVENT_ERR_DYNAMIC_DUPLICATE,
  });
}

// When you have two private, static dependencies in the tree which conflict,
// they never see each other, so it's OK. But when they're shared, you'd have
// conflicting symbols for the loader, so this must fail.
UTEST_F(resolver, config_shared_dynamic_dup_fails) {
  run_fixture(utest_result, (fixture_t) {
    .index = {
      {
        .namespace = "spn",
        .name = "audio",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "gfx", .version = "^1.0.0", .private = true },
            },
            .targets = {
              { .name = "audio", .linkages = { SPN_LIB_KIND_SHARED } },
            }
          },
        }
      },
      {
        .namespace = "spn",
        .name = "video",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "gfx", .version = "^2.0.0", .private = true },
            },
            .targets = {
              { .name = "video", .linkages = { SPN_LIB_KIND_SHARED } },
            }
          },
        }
      },
      {
        .namespace = "spn",
        .name = "gfx",
        .releases = {
          { .version = spn_semver_lit(1, 9, 0), .targets = { { .name = "gfx", .linkages = { SPN_LIB_KIND_STATIC, SPN_LIB_KIND_SHARED } } } },
          { .version = spn_semver_lit(2, 0, 0), .targets = { { .name = "gfx", .linkages = { SPN_LIB_KIND_STATIC, SPN_LIB_KIND_SHARED } } } },
        }
      },
    },
    .manifest = {
      .deps.package = {
        { .name = "spn/audio", .version = "^1.0.0" },
        { .name = "spn/video", .version = "^1.0.0" },
      }
    },
    .config = {
      { .name = "gfx", .kind = SPN_LIB_KIND_SHARED },
    },
    .err = SPN_ERROR,
    .event = SPN_EVENT_ERR_DYNAMIC_DUPLICATE,
  });
}

// A build dep cycle through a single instance can never build: the tool links
// the same audio whose build waits on the tool. The boundary doesn't make the
// graph acyclic, it only changes where the cycle is detected.
UTEST_F(resolver, build_dep_cycle_fails) {
  run_fixture(utest_result, (fixture_t) {
    .index = {
      {
        .namespace = "spn",
        .name = "audio",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "tool", .version = "^1.0.0", .kind = SPN_INDEX_DEP_BUILD },
            }
          },
        }
      },
      {
        .namespace = "spn",
        .name = "tool",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "audio", .version = "^1.0.0" },
            }
          },
        }
      },
    },
    .manifest = {
      .deps.package = {
        { .name = "spn/audio", .version = "^1.0.0" },
      }
    },
    .err = SPN_ERROR,
    .event = SPN_EVENT_ERR_UNIT_CYCLE,
  });
}

// The legal shape of a build dep cycle: the tool links an *older* release of
// audio, so the two audio instances are distinct and the build order is
// audio 1.0.0 -> tool -> audio 2.0.0. This is what instance-level (rather
// than name-level) cycle detection must preserve.
UTEST_F(resolver, build_dep_bootstrap) {
  run_fixture(utest_result, (fixture_t) {
    .index = {
      {
        .namespace = "spn",
        .name = "audio",
        .releases = {
          { .version = spn_semver_lit(1, 0, 0) },
          {
            .version = spn_semver_lit(2, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "tool", .version = "^1.0.0", .kind = SPN_INDEX_DEP_BUILD },
            }
          },
        }
      },
      {
        .namespace = "spn",
        .name = "tool",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "audio", .version = "^1.0.0" },
            }
          },
        }
      },
    },
    .manifest = {
      .deps.package = {
        { .name = "spn/audio", .version = "^2.0.0" },
      }
    },
    .err = SPN_OK,
    .expected = {
      { .name = "audio", .namespace = "spn", .version = spn_semver_lit(2, 0, 0), .unit = "" },
      { .name = "tool", .namespace = "spn", .version = spn_semver_lit(1, 0, 0), .unit = "spn/tool" },
      { .name = "audio", .namespace = "spn", .version = spn_semver_lit(1, 0, 0), .unit = "spn/tool" },
    },
    .instances = {
      { .name = "spn/audio", .count = 2 },
      { .name = "spn/tool", .count = 1 },
    },
  });
}

// A failure recorded inside a candidate that backtracking recovers from must
// not shadow the resolve's real failure
UTEST_F(resolver, backtrack_failure_not_sticky) {
  run_fixture(utest_result, (fixture_t) {
    .index = {
      {
        .namespace = "spn",
        .name = "audio",
        .releases = {
          { .version = spn_semver_lit(1, 0, 0) },
          {
            .version = spn_semver_lit(1, 1, 0),
            .deps = {
              { .namespace = "spn", .name = "physics", .version = "^1.0.0" },
            }
          },
        }
      },
      {
        .namespace = "spn",
        .name = "math",
        .releases = {
          { .version = spn_semver_lit(1, 0, 0) },
        }
      },
    },
    .manifest = {
      .deps.package = {
        { .name = "spn/audio", .version = "^1.0.0" },
        { .name = "spn/math", .version = "^2.0.0" },
      }
    },
    .err = SPN_ERROR,
    .event = SPN_EVENT_ERR_UNSATISFIABLE_VERSION,
  });
}

// A failed candidate must release the picks its subtree made: audio 1.1.0
// resolves math 2.0.0 before failing on missing physics, and the fallback to
// audio 1.0.0 needs math ^1.0.0, which resolves fine on a clean slate
UTEST_F(resolver, backtrack_releases_subtree_picks) {
  run_fixture(utest_result, (fixture_t) {
    .index = {
      {
        .namespace = "spn",
        .name = "audio",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "math", .version = "^1.0.0" },
            }
          },
          {
            .version = spn_semver_lit(1, 1, 0),
            .deps = {
              { .namespace = "spn", .name = "math", .version = "^2.0.0" },
              { .namespace = "spn", .name = "physics", .version = "^1.0.0" },
            }
          },
        }
      },
      {
        .namespace = "spn",
        .name = "math",
        .releases = {
          { .version = spn_semver_lit(1, 0, 0) },
          { .version = spn_semver_lit(2, 0, 0) },
        }
      },
    },
    .manifest = {
      .deps.package = {
        { .name = "spn/audio", .version = "^1.0.0" },
      }
    },
    .err = SPN_OK,
    .expected = {
      { .name = "audio", .namespace = "spn", .version = spn_semver_lit(1, 0, 0) },
      { .name = "math", .namespace = "spn", .version = spn_semver_lit(1, 0, 0) },
    },
    .instances = {
      { .name = "spn/math", .count = 1 },
    },
  });
}

// A pick stranded by a failed candidate must not be committed: audio 1.1.0
// resolves math before failing on missing physics, audio 1.0.0 has no deps,
// so the resolve holds no math at all
UTEST_F(resolver, backtrack_orphan_not_committed) {
  run_fixture(utest_result, (fixture_t) {
    .index = {
      {
        .namespace = "spn",
        .name = "audio",
        .releases = {
          { .version = spn_semver_lit(1, 0, 0) },
          {
            .version = spn_semver_lit(1, 1, 0),
            .deps = {
              { .namespace = "spn", .name = "math", .version = "^1.0.0" },
              { .namespace = "spn", .name = "physics", .version = "^1.0.0" },
            }
          },
        }
      },
      {
        .namespace = "spn",
        .name = "math",
        .releases = {
          { .version = spn_semver_lit(1, 0, 0) },
        }
      },
    },
    .manifest = {
      .deps.package = {
        { .name = "spn/audio", .version = "^1.0.0" },
      }
    },
    .err = SPN_OK,
    .expected = {
      { .name = "audio", .namespace = "spn", .version = spn_semver_lit(1, 0, 0) },
    },
    .instances = {
      { .name = "spn/math", .count = 0 },
    },
  });
}

// The tool's unit diverges on a, so b's resolved subtree differs between the
// units even though both hold b 1.0.0. One b build can't link both a's: the
// instance must split rather than merge two dep sets under one identity.
UTEST_F(resolver, divergent_dep_splits_instance) {
  run_fixture(utest_result, (fixture_t) {
    .index = {
      {
        .namespace = "spn",
        .name = "b",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "a", .version = "^1.0.0" },
            }
          },
        }
      },
      {
        .namespace = "spn",
        .name = "c",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "a", .version = "=1.0.0" },
              { .namespace = "spn", .name = "b", .version = "^1.0.0" },
            }
          },
        }
      },
      {
        .namespace = "spn",
        .name = "a",
        .releases = {
          { .version = spn_semver_lit(1, 0, 0) },
          { .version = spn_semver_lit(1, 9, 0) },
        }
      },
    },
    .manifest = {
      .deps.package = {
        { .name = "spn/a", .version = "^1.9.0" },
        { .name = "spn/b", .version = "^1.0.0" },
        { .name = "spn/c", .version = "^1.0.0", .kind = SPN_DEP_KIND_BUILD },
      }
    },
    .err = SPN_OK,
    .expected = {
      { .name = "a", .namespace = "spn", .version = spn_semver_lit(1, 9, 0), .unit = "" },
      { .name = "b", .namespace = "spn", .version = spn_semver_lit(1, 0, 0), .unit = "" },
      { .name = "a", .namespace = "spn", .version = spn_semver_lit(1, 0, 0), .unit = "spn/c" },
      { .name = "b", .namespace = "spn", .version = spn_semver_lit(1, 0, 0), .unit = "spn/c" },
    },
    .instances = {
      { .name = "spn/a", .count = 2 },
      { .name = "spn/b", .count = 2 },
    },
  });
}

// Each build dep roots its own process: two consumers with disjoint ranges on
// one tool hold two instances instead of conflicting
UTEST_F(resolver, build_dep_disjoint_tools_diverge) {
  run_fixture(utest_result, (fixture_t) {
    .index = {
      {
        .namespace = "spn",
        .name = "renderer",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "foo", .version = "^1.0.0", .kind = SPN_INDEX_DEP_BUILD },
            }
          },
        }
      },
      {
        .namespace = "spn",
        .name = "audio",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "foo", .version = "^2.0.0", .kind = SPN_INDEX_DEP_BUILD },
            }
          },
        }
      },
      {
        .namespace = "spn",
        .name = "foo",
        .releases = {
          { .version = spn_semver_lit(1, 0, 0) },
          { .version = spn_semver_lit(2, 0, 0) },
        }
      },
    },
    .manifest = {
      .deps.package = {
        { .name = "spn/renderer", .version = "^1.0.0" },
        { .name = "spn/audio", .version = "^1.0.0" },
      }
    },
    .err = SPN_OK,
    .expected = {
      { .name = "foo", .namespace = "spn", .version = spn_semver_lit(1, 0, 0), .unit = "spn/foo" },
      { .name = "foo", .namespace = "spn", .version = spn_semver_lit(2, 0, 0), .unit = "spn/foo" },
    },
    .instances = {
      { .name = "spn/foo", .count = 2 },
    },
  });
}

// A shared lib's private scope is per-instance: gfx 1.0.0 in the tool's unit
// and gfx 2.0.0 in the root's each carry their own private foo
UTEST_F(resolver, private_scopes_per_instance) {
  run_fixture(utest_result, (fixture_t) {
    .index = {
      {
        .namespace = "spn",
        .name = "gfx",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "foo", .version = "^1.0.0", .private = true },
            },
            .targets = {
              { .name = "gfx", .linkages = { SPN_LIB_KIND_SHARED } },
            }
          },
          {
            .version = spn_semver_lit(2, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "foo", .version = "^2.0.0", .private = true },
            },
            .targets = {
              { .name = "gfx", .linkages = { SPN_LIB_KIND_SHARED } },
            }
          },
        }
      },
      {
        .namespace = "spn",
        .name = "tool",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "gfx", .version = "^1.0.0" },
            }
          },
        }
      },
      {
        .namespace = "spn",
        .name = "foo",
        .releases = {
          { .version = spn_semver_lit(1, 0, 0) },
          { .version = spn_semver_lit(2, 0, 0) },
        }
      },
    },
    .manifest = {
      .deps.package = {
        { .name = "spn/gfx", .version = "^2.0.0", .kind = SPN_DEP_KIND_PACKAGE },
        { .name = "spn/tool", .version = "^1.0.0", .kind = SPN_DEP_KIND_BUILD },
      }
    },
    .err = SPN_OK,
    .expected = {
      { .name = "gfx", .namespace = "spn", .version = spn_semver_lit(2, 0, 0), .unit = "" },
      { .name = "gfx", .namespace = "spn", .version = spn_semver_lit(1, 0, 0), .unit = "spn/tool" },
      { .name = "foo", .namespace = "spn", .version = spn_semver_lit(2, 0, 0), .unit = "spn/gfx", .unit_version = spn_semver_lit(2, 0, 0) },
      { .name = "foo", .namespace = "spn", .version = spn_semver_lit(1, 0, 0), .unit = "spn/gfx", .unit_version = spn_semver_lit(1, 0, 0) },
      { .name = "foo", .namespace = "spn", .version = spn_semver_lit(2, 0, 0), .unit = "spn/gfx", .unit_version = spn_semver_lit(1, 0, 0), .excluded = true },
      { .name = "foo", .namespace = "spn", .version = spn_semver_lit(1, 0, 0), .unit = "spn/gfx", .unit_version = spn_semver_lit(2, 0, 0), .excluded = true },
    },
    .instances = {
      { .name = "spn/gfx", .count = 2 },
      { .name = "spn/foo", .count = 2 },
    },
  });
}

// Sibling groups converge on each other in manifest order: gen decides foo
// first, and opt's wider range admits 2.0.0 but must take gen's 1.9.0. An
// isolated solve of opt would split a pointless second foo.
UTEST_F(resolver, sibling_tools_unify) {
  run_fixture(utest_result, (fixture_t) {
    .index = {
      {
        .namespace = "spn",
        .name = "gen",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "foo", .version = "^1.0.0" },
            }
          },
        }
      },
      {
        .namespace = "spn",
        .name = "opt",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "foo", .version = ">=1.0.0" },
            }
          },
        }
      },
      {
        .namespace = "spn",
        .name = "foo",
        .releases = {
          { .version = spn_semver_lit(1, 0, 0) },
          { .version = spn_semver_lit(1, 9, 0) },
          { .version = spn_semver_lit(2, 0, 0) },
        }
      },
    },
    .manifest = {
      .deps.package = {
        { .name = "spn/gen", .version = "^1.0.0", .kind = SPN_DEP_KIND_BUILD },
        { .name = "spn/opt", .version = "^1.0.0", .kind = SPN_DEP_KIND_BUILD },
      }
    },
    .err = SPN_OK,
    .expected = {
      { .name = "foo", .namespace = "spn", .version = spn_semver_lit(1, 9, 0), .unit = "spn/gen" },
      { .name = "foo", .namespace = "spn", .version = spn_semver_lit(1, 9, 0), .unit = "spn/opt" },
    },
    .instances = {
      { .name = "spn/foo", .count = 1 },
      { .name = "spn/gen", .count = 1 },
      { .name = "spn/opt", .count = 1 },
    },
  });
}

// Splitting units doesn't make missing packages resolvable; a boundary
// still resolves for real
UTEST_F(resolver, build_dep_missing_still_fails) {
  run_fixture(utest_result, (fixture_t) {
    .index = {
      {
        .namespace = "spn",
        .name = "audio",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "tool", .version = "^1.0.0", .kind = SPN_INDEX_DEP_BUILD },
            }
          },
        }
      },
    },
    .manifest = {
      .deps.package = {
        { .name = "spn/audio", .version = "^1.0.0" },
      }
    },
    .err = SPN_ERROR,
    .event = SPN_EVENT_ERR_UNKNOWN_PKG,
  });
}

//////////////
// IDENTITY //
//////////////
static resolve_result_t run_isolated(fixture_t* fixture) {
  sp_mem_t mem = sp_mem_arena_as_allocator(ctx_get()->arena);
  state = sp_zero_s(state_t);
  sp_str_ht_init(mem, state.cache);
  build_cache(mem, fixture);
  return execute_fixture(fixture, sp_intern_new(sp_mem_os_new()));
}

static sp_hash_t instance_hash(resolve_result_t* result, const c8* namespace, const c8* name, spn_semver_t version) {
  sp_str_t qualified = spn_pkg_canonicalize_pair(sp_str_view(namespace), sp_str_view(name));
  sp_ht_for_kv(result->query.result, it) {
    if (sp_str_equal(it.val->qualified, qualified) && spn_semver_eq(it.val->version, version)) {
      return it.val->id.hash;
    }
  }
  return 0;
}

// A leaf hashes to bare (name, version): identical builds get identical
// identity in every project regardless of how they were reached
UTEST_F(resolver, hash_leaf_identity_across_projects) {
  fixture_t direct = {
    .index = {
      {
        .namespace = "spn",
        .name = "math",
        .releases = {
          { .version = spn_semver_lit(1, 1, 0) },
        }
      },
    },
    .manifest = {
      .deps.package = {
        { .name = "spn/math", .version = "^1.0.0" },
      }
    },
  };

  fixture_t transitive = {
    .index = {
      {
        .namespace = "spn",
        .name = "renderer",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "math", .version = "^1.1.0" },
            }
          },
        }
      },
      {
        .namespace = "spn",
        .name = "math",
        .releases = {
          { .version = spn_semver_lit(1, 1, 0) },
        }
      },
    },
    .manifest = {
      .deps.package = {
        { .name = "spn/renderer", .version = "^1.0.0" },
      }
    },
  };

  resolve_result_t first = run_isolated(&direct);
  resolve_result_t second = run_isolated(&transitive);
  ASSERT_EQ(first.err, SPN_OK);
  ASSERT_EQ(second.err, SPN_OK);

  sp_hash_t a = instance_hash(&first, "spn", "math", spn_semver_lit(1, 1, 0));
  sp_hash_t b = instance_hash(&second, "spn", "math", spn_semver_lit(1, 1, 0));
  ASSERT_TRUE(a != 0);
  ASSERT_EQ(a, b);
}

// A depender's identity covers its resolved subtree: the same renderer over
// a different math is a different build
UTEST_F(resolver, hash_tracks_child_subtree) {
  fixture_t old = {
    .index = {
      {
        .namespace = "spn",
        .name = "renderer",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "math", .version = ">=1.0.0" },
            }
          },
        }
      },
      {
        .namespace = "spn",
        .name = "math",
        .releases = {
          { .version = spn_semver_lit(1, 0, 0) },
        }
      },
    },
    .manifest = {
      .deps.package = {
        { .name = "spn/renderer", .version = "^1.0.0" },
      }
    },
  };

  fixture_t new = old;
  new.index[1].releases[0].version = spn_semver_lit(1, 5, 0);

  resolve_result_t first = run_isolated(&old);
  resolve_result_t second = run_isolated(&new);
  ASSERT_EQ(first.err, SPN_OK);
  ASSERT_EQ(second.err, SPN_OK);

  sp_hash_t a = instance_hash(&first, "spn", "renderer", spn_semver_lit(1, 0, 0));
  sp_hash_t b = instance_hash(&second, "spn", "renderer", spn_semver_lit(1, 0, 0));
  ASSERT_TRUE(a != 0);
  ASSERT_TRUE(b != 0);
  ASSERT_NE(a, b);
}

// A tool shapes what it generates: a build-dep subtree change re-identifies
// every consumer
UTEST_F(resolver, hash_tracks_build_subtree) {
  fixture_t old = {
    .index = {
      {
        .namespace = "spn",
        .name = "app",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "tool", .version = "^1.0.0", .kind = SPN_INDEX_DEP_BUILD },
            }
          },
        }
      },
      {
        .namespace = "spn",
        .name = "tool",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "leaf", .version = ">=1.0.0" },
            }
          },
        }
      },
      {
        .namespace = "spn",
        .name = "leaf",
        .releases = {
          { .version = spn_semver_lit(1, 0, 0) },
        }
      },
    },
    .manifest = {
      .deps.package = {
        { .name = "spn/app", .version = "^1.0.0" },
      }
    },
  };

  fixture_t new = old;
  new.index[2].releases[0].version = spn_semver_lit(1, 5, 0);

  resolve_result_t first = run_isolated(&old);
  resolve_result_t second = run_isolated(&new);
  ASSERT_EQ(first.err, SPN_OK);
  ASSERT_EQ(second.err, SPN_OK);

  sp_hash_t a = instance_hash(&first, "spn", "app", spn_semver_lit(1, 0, 0));
  sp_hash_t b = instance_hash(&second, "spn", "app", spn_semver_lit(1, 0, 0));
  ASSERT_TRUE(a != 0);
  ASSERT_TRUE(b != 0);
  ASSERT_NE(a, b);
}

/////////////////
// CONVERGENCE //
/////////////////

// Two sibling tools both pin a to the same older version, so each ends up
// holding b 1.0.0 over a 1.0.0. Identical resolved subtrees must collapse
// to ONE split instance shared by both tool units, never one per group: two
// b instances total (the root's over a 1.9.0 plus one shared split),
// never three
UTEST_F(resolver, split_instances_reconverge) {
  run_fixture(utest_result, (fixture_t) {
    .index = {
      {
        .namespace = "spn",
        .name = "b",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "a", .version = "^1.0.0" },
            }
          },
        }
      },
      {
        .namespace = "spn",
        .name = "c1",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "a", .version = "=1.0.0" },
              { .namespace = "spn", .name = "b", .version = "^1.0.0" },
            }
          },
        }
      },
      {
        .namespace = "spn",
        .name = "c2",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "a", .version = "=1.0.0" },
              { .namespace = "spn", .name = "b", .version = "^1.0.0" },
            }
          },
        }
      },
      {
        .namespace = "spn",
        .name = "a",
        .releases = {
          { .version = spn_semver_lit(1, 0, 0) },
          { .version = spn_semver_lit(1, 9, 0) },
        }
      },
    },
    .manifest = {
      .deps.package = {
        { .name = "spn/a", .version = "^1.0.0" },
        { .name = "spn/b", .version = "^1.0.0" },
        { .name = "spn/c1", .version = "^1.0.0", .kind = SPN_DEP_KIND_BUILD },
        { .name = "spn/c2", .version = "^1.0.0", .kind = SPN_DEP_KIND_BUILD },
      }
    },
    .err = SPN_OK,
    .expected = {
      { .name = "a", .namespace = "spn", .version = spn_semver_lit(1, 9, 0), .unit = "" },
      { .name = "a", .namespace = "spn", .version = spn_semver_lit(1, 0, 0), .unit = "spn/c1" },
      { .name = "a", .namespace = "spn", .version = spn_semver_lit(1, 0, 0), .unit = "spn/c2" },
      { .name = "b", .namespace = "spn", .version = spn_semver_lit(1, 0, 0), .unit = "" },
      { .name = "b", .namespace = "spn", .version = spn_semver_lit(1, 0, 0), .unit = "spn/c1" },
      { .name = "a", .namespace = "spn", .version = spn_semver_lit(1, 9, 0), .unit = "spn/c1", .excluded = true },
    },
    .instances = {
      { .name = "spn/b", .count = 2 },
      { .name = "spn/a", .count = 2 },
      { .name = "spn/c1", .count = 1 },
      { .name = "spn/c2", .count = 1 },
    },
  });
}

// The tool pins a older, so the divergence propagates up the chain: c and b
// hold their root versions but over a different a subtree, splitting BOTH at
// the same version purely transitively
UTEST_F(resolver, split_propagates_through_middle) {
  run_fixture(utest_result, (fixture_t) {
    .index = {
      {
        .namespace = "spn",
        .name = "c",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "b", .version = "^1.0.0" },
            }
          },
        }
      },
      {
        .namespace = "spn",
        .name = "b",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "a", .version = "^1.0.0" },
            }
          },
        }
      },
      {
        .namespace = "spn",
        .name = "a",
        .releases = {
          { .version = spn_semver_lit(1, 0, 0) },
          { .version = spn_semver_lit(1, 9, 0) },
        }
      },
      {
        .namespace = "spn",
        .name = "tool",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "a", .version = "=1.0.0" },
              { .namespace = "spn", .name = "c", .version = "^1.0.0" },
            }
          },
        }
      },
    },
    .manifest = {
      .deps.package = {
        { .name = "spn/c", .version = "^1.0.0" },
        { .name = "spn/tool", .version = "^1.0.0", .kind = SPN_DEP_KIND_BUILD },
      }
    },
    .err = SPN_OK,
    .expected = {
      { .name = "a", .namespace = "spn", .version = spn_semver_lit(1, 9, 0), .unit = "" },
      { .name = "a", .namespace = "spn", .version = spn_semver_lit(1, 0, 0), .unit = "spn/tool" },
      { .name = "c", .namespace = "spn", .version = spn_semver_lit(1, 0, 0), .unit = "" },
      { .name = "c", .namespace = "spn", .version = spn_semver_lit(1, 0, 0), .unit = "spn/tool" },
      { .name = "b", .namespace = "spn", .version = spn_semver_lit(1, 0, 0), .unit = "spn/tool" },
      { .name = "a", .namespace = "spn", .version = spn_semver_lit(1, 9, 0), .unit = "spn/tool", .excluded = true },
    },
    .instances = {
      { .name = "spn/c", .count = 2 },
      { .name = "spn/b", .count = 2 },
      { .name = "spn/a", .count = 2 },
      { .name = "spn/tool", .count = 1 },
    },
  });
}

// The kept pin set is the lexicographically-first feasible subset of the
// inherited table [a@1, b@1, c@1]. Keeping a@1 forces x 1.0.0, which excludes
// b@1 (drops, splits to b 2.0.0) and never requests c (c@1 holds vacuously).
// A newest-first solve would take x 1.2.0 and split a instead
UTEST_F(resolver, pin_walk_lexicographic_subset) {
  run_fixture(utest_result, (fixture_t) {
    .index = {
      {
        .namespace = "spn",
        .name = "a",
        .releases = {
          { .version = spn_semver_lit(1, 0, 0) },
          { .version = spn_semver_lit(2, 0, 0) },
        }
      },
      {
        .namespace = "spn",
        .name = "b",
        .releases = {
          { .version = spn_semver_lit(1, 0, 0) },
          { .version = spn_semver_lit(2, 0, 0) },
        }
      },
      {
        .namespace = "spn",
        .name = "c",
        .releases = {
          { .version = spn_semver_lit(1, 0, 0) },
          { .version = spn_semver_lit(2, 0, 0) },
        }
      },
      {
        .namespace = "spn",
        .name = "x",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "a", .version = "=1.0.0" },
              { .namespace = "spn", .name = "b", .version = "=2.0.0" },
            }
          },
          {
            .version = spn_semver_lit(1, 1, 0),
            .deps = {
              { .namespace = "spn", .name = "a", .version = "=2.0.0" },
              { .namespace = "spn", .name = "b", .version = "=1.0.0" },
              { .namespace = "spn", .name = "c", .version = "=2.0.0" },
            }
          },
          {
            .version = spn_semver_lit(1, 2, 0),
            .deps = {
              { .namespace = "spn", .name = "a", .version = "=2.0.0" },
              { .namespace = "spn", .name = "b", .version = "=2.0.0" },
              { .namespace = "spn", .name = "c", .version = "=1.0.0" },
            }
          },
        }
      },
      {
        .namespace = "spn",
        .name = "tool",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "x", .version = "^1.0.0" },
            }
          },
        }
      },
    },
    .manifest = {
      .deps.package = {
        { .name = "spn/a", .version = "=1.0.0" },
        { .name = "spn/b", .version = "=1.0.0" },
        { .name = "spn/c", .version = "=1.0.0" },
        { .name = "spn/tool", .version = "^1.0.0", .kind = SPN_DEP_KIND_BUILD },
      }
    },
    .err = SPN_OK,
    .expected = {
      { .name = "x", .namespace = "spn", .version = spn_semver_lit(1, 0, 0), .unit = "spn/tool" },
      { .name = "a", .namespace = "spn", .version = spn_semver_lit(1, 0, 0), .unit = "" },
      { .name = "a", .namespace = "spn", .version = spn_semver_lit(1, 0, 0), .unit = "spn/tool" },
      { .name = "b", .namespace = "spn", .version = spn_semver_lit(2, 0, 0), .unit = "spn/tool" },
      { .name = "b", .namespace = "spn", .version = spn_semver_lit(1, 0, 0), .unit = "" },
      { .name = "c", .namespace = "spn", .version = spn_semver_lit(1, 0, 0), .unit = "spn/tool", .excluded = true },
      { .name = "b", .namespace = "spn", .version = spn_semver_lit(1, 0, 0), .unit = "spn/tool", .excluded = true },
    },
    .instances = {
      { .name = "spn/a", .count = 1 },
      { .name = "spn/b", .count = 2 },
      { .name = "spn/c", .count = 1 },
      { .name = "spn/x", .count = 1 },
    },
  });
}

// convergence_forces_older_sibling, two levels down: keeping the root's foo
// pick is only satisfiable via older choices at BOTH transitive levels below
// the tool's request (mid 1.0.0 instead of 1.1.0, bar 1.0.0 instead of 2.0.0)
UTEST_F(resolver, convergence_forces_older_transitive) {
  run_fixture(utest_result, (fixture_t) {
    .index = {
      {
        .namespace = "spn",
        .name = "foo",
        .releases = {
          { .version = spn_semver_lit(1, 9, 0) },
          { .version = spn_semver_lit(2, 0, 0) },
        }
      },
      {
        .namespace = "spn",
        .name = "bar",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "foo", .version = "^1.0.0" },
            }
          },
          {
            .version = spn_semver_lit(2, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "foo", .version = "^2.0.0" },
            }
          },
        }
      },
      {
        .namespace = "spn",
        .name = "mid",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "bar", .version = "^1.0.0" },
            }
          },
          {
            .version = spn_semver_lit(1, 1, 0),
            .deps = {
              { .namespace = "spn", .name = "bar", .version = "^2.0.0" },
            }
          },
        }
      },
      {
        .namespace = "spn",
        .name = "tool",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "mid", .version = ">=1.0.0" },
            }
          },
        }
      },
    },
    .manifest = {
      .deps.package = {
        { .name = "spn/foo", .version = "^1.0.0" },
        { .name = "spn/tool", .version = "^1.0.0", .kind = SPN_DEP_KIND_BUILD },
      }
    },
    .err = SPN_OK,
    .expected = {
      { .name = "foo", .namespace = "spn", .version = spn_semver_lit(1, 9, 0), .unit = "" },
      { .name = "foo", .namespace = "spn", .version = spn_semver_lit(1, 9, 0), .unit = "spn/tool" },
      { .name = "mid", .namespace = "spn", .version = spn_semver_lit(1, 0, 0), .unit = "spn/tool" },
      { .name = "bar", .namespace = "spn", .version = spn_semver_lit(1, 0, 0), .unit = "spn/tool" },
      { .name = "mid", .namespace = "spn", .version = spn_semver_lit(1, 1, 0), .excluded = true },
      { .name = "bar", .namespace = "spn", .version = spn_semver_lit(2, 0, 0), .excluded = true },
      { .name = "foo", .namespace = "spn", .version = spn_semver_lit(2, 0, 0), .excluded = true },
    },
    .instances = {
      { .name = "spn/foo", .count = 1 },
      { .name = "spn/bar", .count = 1 },
      { .name = "spn/mid", .count = 1 },
    },
  });
}

// A tool's tool: tb is a build dep two process boundaries deep. Only tb's
// unit may split foo; the middle tool converges with the root's pick
UTEST_F(resolver, nested_tool_splits_only_inner) {
  run_fixture(utest_result, (fixture_t) {
    .index = {
      {
        .namespace = "spn",
        .name = "app",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "ta", .version = "^1.0.0", .kind = SPN_INDEX_DEP_BUILD },
            }
          },
        }
      },
      {
        .namespace = "spn",
        .name = "ta",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "foo", .version = "^1.0.0" },
              { .namespace = "spn", .name = "tb", .version = "^1.0.0", .kind = SPN_INDEX_DEP_BUILD },
            }
          },
        }
      },
      {
        .namespace = "spn",
        .name = "tb",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "foo", .version = "=1.0.0" },
            }
          },
        }
      },
      {
        .namespace = "spn",
        .name = "foo",
        .releases = {
          { .version = spn_semver_lit(1, 0, 0) },
          { .version = spn_semver_lit(1, 9, 0) },
        }
      },
    },
    .manifest = {
      .deps.package = {
        { .name = "spn/app", .version = "^1.0.0" },
        { .name = "spn/foo", .version = "^1.0.0" },
      }
    },
    .err = SPN_OK,
    .expected = {
      { .name = "foo", .namespace = "spn", .version = spn_semver_lit(1, 9, 0), .unit = "" },
      { .name = "foo", .namespace = "spn", .version = spn_semver_lit(1, 9, 0), .unit = "spn/ta" },
      { .name = "foo", .namespace = "spn", .version = spn_semver_lit(1, 0, 0), .unit = "spn/tb" },
      { .name = "foo", .namespace = "spn", .version = spn_semver_lit(1, 0, 0), .unit = "", .excluded = true },
      { .name = "foo", .namespace = "spn", .version = spn_semver_lit(1, 0, 0), .unit = "spn/ta", .excluded = true },
    },
    .instances = {
      { .name = "spn/foo", .count = 2 },
      { .name = "spn/ta", .count = 1 },
      { .name = "spn/tb", .count = 1 },
    },
  });
}

// Privacy nests: inner is private to gfx, leaf is private to inner. Two
// shared boundaries deep, leaf may still diverge from the root's pick, and
// neither inner nor its leaf leaks into the root unit
UTEST_F(resolver, private_inside_private_diverges) {
  run_fixture(utest_result, (fixture_t) {
    .index = {
      {
        .namespace = "spn",
        .name = "gfx",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "inner", .version = "^1.0.0", .private = true },
            },
            .targets = {
              { .name = "gfx", .linkages = { SPN_LIB_KIND_SHARED } },
            }
          },
        }
      },
      {
        .namespace = "spn",
        .name = "inner",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "leaf", .version = "^1.0.0", .private = true },
            },
            .targets = {
              { .name = "inner", .linkages = { SPN_LIB_KIND_SHARED } },
            }
          },
        }
      },
      {
        .namespace = "spn",
        .name = "leaf",
        .releases = {
          { .version = spn_semver_lit(1, 0, 0) },
          { .version = spn_semver_lit(2, 0, 0) },
        }
      },
    },
    .manifest = {
      .deps.package = {
        { .name = "spn/gfx", .version = "^1.0.0" },
        { .name = "spn/leaf", .version = "^2.0.0" },
      }
    },
    .err = SPN_OK,
    .expected = {
      { .name = "leaf", .namespace = "spn", .version = spn_semver_lit(2, 0, 0), .unit = "" },
      { .name = "inner", .namespace = "spn", .version = spn_semver_lit(1, 0, 0), .unit = "spn/gfx" },
      { .name = "leaf", .namespace = "spn", .version = spn_semver_lit(1, 0, 0), .unit = "spn/inner" },
      { .name = "inner", .namespace = "spn", .version = spn_semver_lit(1, 0, 0), .unit = "", .excluded = true },
      { .name = "leaf", .namespace = "spn", .version = spn_semver_lit(1, 0, 0), .unit = "", .excluded = true },
    },
    .instances = {
      { .name = "spn/leaf", .count = 2 },
      { .name = "spn/inner", .count = 1 },
      { .name = "spn/gfx", .count = 1 },
    },
  });
}

// Nested privacy can't dodge the loader: leaf only builds shared, so the
// root's leaf 2.0.0 and inner's private leaf 1.0.0 both load into the root
// process regardless of being two private boundaries deep
UTEST_F(resolver, private_inside_private_dynamic_dup_fails) {
  run_fixture(utest_result, (fixture_t) {
    .index = {
      {
        .namespace = "spn",
        .name = "gfx",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "inner", .version = "^1.0.0", .private = true },
            },
            .targets = {
              { .name = "gfx", .linkages = { SPN_LIB_KIND_SHARED } },
            }
          },
        }
      },
      {
        .namespace = "spn",
        .name = "inner",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "leaf", .version = "^1.0.0", .private = true },
            },
            .targets = {
              { .name = "inner", .linkages = { SPN_LIB_KIND_SHARED } },
            }
          },
        }
      },
      {
        .namespace = "spn",
        .name = "leaf",
        .releases = {
          { .version = spn_semver_lit(1, 0, 0), .targets = { { .name = "leaf", .linkages = { SPN_LIB_KIND_SHARED } } } },
          { .version = spn_semver_lit(2, 0, 0), .targets = { { .name = "leaf", .linkages = { SPN_LIB_KIND_SHARED } } } },
        }
      },
    },
    .manifest = {
      .deps.package = {
        { .name = "spn/gfx", .version = "^1.0.0" },
        { .name = "spn/leaf", .version = "^2.0.0" },
      }
    },
    .err = SPN_ERROR,
    .event = SPN_EVENT_ERR_DYNAMIC_DUPLICATE,
  });
}

// A build dep discovered inside a shared lib's private subtree still roots
// its own process: the tool may take a foo the root's pick excludes, and that
// foo never enters gfx's private unit
UTEST_F(resolver, build_dep_inside_private_diverges) {
  run_fixture(utest_result, (fixture_t) {
    .index = {
      {
        .namespace = "spn",
        .name = "gfx",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "bar", .version = "^1.0.0", .private = true },
            },
            .targets = {
              { .name = "gfx", .linkages = { SPN_LIB_KIND_SHARED } },
            }
          },
        }
      },
      {
        .namespace = "spn",
        .name = "bar",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "tool", .version = "^1.0.0", .kind = SPN_INDEX_DEP_BUILD },
            }
          },
        }
      },
      {
        .namespace = "spn",
        .name = "tool",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "foo", .version = "=1.0.0" },
            }
          },
        }
      },
      {
        .namespace = "spn",
        .name = "foo",
        .releases = {
          { .version = spn_semver_lit(1, 0, 0) },
          { .version = spn_semver_lit(2, 0, 0) },
        }
      },
    },
    .manifest = {
      .deps.package = {
        { .name = "spn/gfx", .version = "^1.0.0" },
        { .name = "spn/foo", .version = "^2.0.0" },
      }
    },
    .err = SPN_OK,
    .expected = {
      { .name = "foo", .namespace = "spn", .version = spn_semver_lit(2, 0, 0), .unit = "" },
      { .name = "bar", .namespace = "spn", .version = spn_semver_lit(1, 0, 0), .unit = "spn/gfx" },
      { .name = "foo", .namespace = "spn", .version = spn_semver_lit(1, 0, 0), .unit = "spn/tool" },
      { .name = "foo", .namespace = "spn", .version = spn_semver_lit(1, 0, 0), .unit = "spn/gfx", .excluded = true },
      { .name = "foo", .namespace = "spn", .version = spn_semver_lit(1, 0, 0), .unit = "", .excluded = true },
    },
    .instances = {
      { .name = "spn/foo", .count = 2 },
      { .name = "spn/bar", .count = 1 },
      { .name = "spn/tool", .count = 1 },
    },
  });
}

// One package reached through all three edge kinds at once: the root links
// foo publicly, gfx carries a private copy behind its shared boundary, and
// the tool holds a third in its own process. Three instances, each fenced
// into exactly its own unit
UTEST_F(resolver, boundary_diamond_three_instances) {
  run_fixture(utest_result, (fixture_t) {
    .index = {
      {
        .namespace = "spn",
        .name = "foo",
        .releases = {
          { .version = spn_semver_lit(1, 0, 0) },
          { .version = spn_semver_lit(1, 5, 0) },
          { .version = spn_semver_lit(1, 9, 0) },
        }
      },
      {
        .namespace = "spn",
        .name = "gfx",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "foo", .version = "=1.0.0", .private = true },
            },
            .targets = {
              { .name = "gfx", .linkages = { SPN_LIB_KIND_SHARED } },
            }
          },
        }
      },
      {
        .namespace = "spn",
        .name = "tool",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "foo", .version = "=1.5.0" },
            }
          },
        }
      },
    },
    .manifest = {
      .deps.package = {
        { .name = "spn/foo", .version = "^1.0.0" },
        { .name = "spn/gfx", .version = "^1.0.0" },
        { .name = "spn/tool", .version = "^1.0.0", .kind = SPN_DEP_KIND_BUILD },
      }
    },
    .err = SPN_OK,
    .expected = {
      { .name = "foo", .namespace = "spn", .version = spn_semver_lit(1, 9, 0), .unit = "" },
      { .name = "foo", .namespace = "spn", .version = spn_semver_lit(1, 0, 0), .unit = "spn/gfx" },
      { .name = "foo", .namespace = "spn", .version = spn_semver_lit(1, 5, 0), .unit = "spn/tool" },
      { .name = "foo", .namespace = "spn", .version = spn_semver_lit(1, 5, 0), .unit = "", .excluded = true },
      { .name = "foo", .namespace = "spn", .version = spn_semver_lit(1, 0, 0), .unit = "", .excluded = true },
      { .name = "foo", .namespace = "spn", .version = spn_semver_lit(1, 9, 0), .unit = "spn/gfx", .excluded = true },
    },
    .instances = {
      { .name = "spn/foo", .count = 3 },
      { .name = "spn/gfx", .count = 1 },
      { .name = "spn/tool", .count = 1 },
    },
  });
}

// Bootstrap plus divergence: the tool must link the older audio (the root's
// 2.0.0 is excluded), and its OTHER dep zed also falls outside the root's
// pick, so the tool's unit diverges on both while the build order stays
// acyclic: audio 1.0.0 -> tool -> audio 2.0.0
UTEST_F(resolver, bootstrap_with_divergent_sibling) {
  run_fixture(utest_result, (fixture_t) {
    .index = {
      {
        .namespace = "spn",
        .name = "audio",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "zed", .version = "^1.0.0" },
            }
          },
          {
            .version = spn_semver_lit(2, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "zed", .version = "^2.0.0" },
              { .namespace = "spn", .name = "tool", .version = "^1.0.0", .kind = SPN_INDEX_DEP_BUILD },
            }
          },
        }
      },
      {
        .namespace = "spn",
        .name = "tool",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "audio", .version = "^1.0.0" },
              { .namespace = "spn", .name = "zed", .version = "^1.0.0" },
            }
          },
        }
      },
      {
        .namespace = "spn",
        .name = "zed",
        .releases = {
          { .version = spn_semver_lit(1, 0, 0) },
          { .version = spn_semver_lit(1, 5, 0) },
          { .version = spn_semver_lit(2, 0, 0) },
        }
      },
    },
    .manifest = {
      .deps.package = {
        { .name = "spn/audio", .version = "^2.0.0" },
      }
    },
    .err = SPN_OK,
    .expected = {
      { .name = "audio", .namespace = "spn", .version = spn_semver_lit(2, 0, 0), .unit = "" },
      { .name = "zed", .namespace = "spn", .version = spn_semver_lit(2, 0, 0), .unit = "" },
      { .name = "audio", .namespace = "spn", .version = spn_semver_lit(1, 0, 0), .unit = "spn/tool" },
      { .name = "zed", .namespace = "spn", .version = spn_semver_lit(1, 5, 0), .unit = "spn/tool" },
      { .name = "zed", .namespace = "spn", .version = spn_semver_lit(2, 0, 0), .unit = "spn/tool", .excluded = true },
      { .name = "audio", .namespace = "spn", .version = spn_semver_lit(2, 0, 0), .unit = "spn/tool", .excluded = true },
    },
    .instances = {
      { .name = "spn/audio", .count = 2 },
      { .name = "spn/zed", .count = 2 },
      { .name = "spn/tool", .count = 1 },
    },
  });
}

// Rule 5's same-version case: audio's and video's private scopes both hold
// zed 1.0.0, but over different math subtrees. zed only builds shared, the
// loader dedups by name, and one consumer would run against the wrong
// embedded math: an error even though the versions are equal
UTEST_F(resolver, same_version_dynamic_dup_fails) {
  run_fixture(utest_result, (fixture_t) {
    .index = {
      {
        .namespace = "spn",
        .name = "audio",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "math", .version = "=1.0.0", .private = true },
              { .namespace = "spn", .name = "zed", .version = "^1.0.0", .private = true },
            },
            .targets = {
              { .name = "audio", .linkages = { SPN_LIB_KIND_SHARED } },
            }
          },
        }
      },
      {
        .namespace = "spn",
        .name = "video",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "math", .version = "=1.5.0", .private = true },
              { .namespace = "spn", .name = "zed", .version = "^1.0.0", .private = true },
            },
            .targets = {
              { .name = "video", .linkages = { SPN_LIB_KIND_SHARED } },
            }
          },
        }
      },
      {
        .namespace = "spn",
        .name = "zed",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "math", .version = "^1.0.0" },
            },
            .targets = {
              { .name = "zed", .linkages = { SPN_LIB_KIND_SHARED } },
            }
          },
        }
      },
      {
        .namespace = "spn",
        .name = "math",
        .releases = {
          { .version = spn_semver_lit(1, 0, 0) },
          { .version = spn_semver_lit(1, 5, 0) },
        }
      },
    },
    .manifest = {
      .deps.package = {
        { .name = "spn/audio", .version = "^1.0.0" },
        { .name = "spn/video", .version = "^1.0.0" },
      }
    },
    .err = SPN_ERROR,
    .event = SPN_EVENT_ERR_DYNAMIC_DUPLICATE,
  });
}

// Two private groups, one split: gfxa's range excludes the root's foo 2.0.0
// and splits to 1.9.0. gfxb's range admits both decided versions, and must
// take the EARLIEST admissible pick (the root's 2.0.0), not gfxa's newer
// split priority-wise or the numerically-newest
UTEST_F(resolver, private_groups_converge_on_earliest) {
  run_fixture(utest_result, (fixture_t) {
    .index = {
      {
        .namespace = "spn",
        .name = "gfxa",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "foo", .version = "^1.0.0", .private = true },
            },
            .targets = {
              { .name = "gfxa", .linkages = { SPN_LIB_KIND_SHARED } },
            }
          },
        }
      },
      {
        .namespace = "spn",
        .name = "gfxb",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "foo", .version = ">=1.0.0", .private = true },
            },
            .targets = {
              { .name = "gfxb", .linkages = { SPN_LIB_KIND_SHARED } },
            }
          },
        }
      },
      {
        .namespace = "spn",
        .name = "foo",
        .releases = {
          { .version = spn_semver_lit(1, 0, 0) },
          { .version = spn_semver_lit(1, 9, 0) },
          { .version = spn_semver_lit(2, 0, 0) },
        }
      },
    },
    .manifest = {
      .deps.package = {
        { .name = "spn/foo", .version = "=2.0.0" },
        { .name = "spn/gfxa", .version = "^1.0.0" },
        { .name = "spn/gfxb", .version = "^1.0.0" },
      }
    },
    .err = SPN_OK,
    .expected = {
      { .name = "foo", .namespace = "spn", .version = spn_semver_lit(2, 0, 0), .unit = "" },
      { .name = "foo", .namespace = "spn", .version = spn_semver_lit(1, 9, 0), .unit = "spn/gfxa" },
      { .name = "foo", .namespace = "spn", .version = spn_semver_lit(2, 0, 0), .unit = "spn/gfxb" },
      { .name = "foo", .namespace = "spn", .version = spn_semver_lit(1, 9, 0), .unit = "spn/gfxb", .excluded = true },
      { .name = "foo", .namespace = "spn", .version = spn_semver_lit(1, 0, 0), .excluded = true },
    },
    .instances = {
      { .name = "spn/foo", .count = 2 },
      { .name = "spn/gfxa", .count = 1 },
      { .name = "spn/gfxb", .count = 1 },
    },
  });
}

// A root test dep's transitive subtree diverges from the product's picks in
// its own process instead of conflicting
UTEST_F(resolver, test_dep_transitive_diverges) {
  run_fixture(utest_result, (fixture_t) {
    .index = {
      {
        .namespace = "spn",
        .name = "harness",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "foo", .version = "^2.0.0" },
            }
          },
        }
      },
      {
        .namespace = "spn",
        .name = "foo",
        .releases = {
          { .version = spn_semver_lit(1, 0, 0) },
          { .version = spn_semver_lit(2, 0, 0) },
        }
      },
    },
    .manifest = {
      .deps.package = {
        { .name = "spn/foo", .version = "=1.0.0" },
        { .name = "spn/harness", .version = "^1.0.0", .kind = SPN_DEP_KIND_TEST },
      }
    },
    .err = SPN_OK,
    .expected = {
      { .name = "foo", .namespace = "spn", .version = spn_semver_lit(1, 0, 0), .unit = "" },
      { .name = "foo", .namespace = "spn", .version = spn_semver_lit(2, 0, 0), .unit = "spn/harness" },
      { .name = "harness", .namespace = "spn", .version = spn_semver_lit(1, 0, 0), .unit = "spn/harness" },
      { .name = "foo", .namespace = "spn", .version = spn_semver_lit(2, 0, 0), .unit = "", .excluded = true },
    },
    .instances = {
      { .name = "spn/foo", .count = 2 },
      { .name = "spn/harness", .count = 1 },
    },
  });
}

// The tool's range admits the root's audio 2.0.0, so convergence takes it —
// and manufactures a genuine instance cycle (audio 2.0.0 -> tool -> audio
// 2.0.0). Rule 6: instance cycles are errors, never split triggers; the
// resolver must not quietly fall back to audio 1.0.0
UTEST_F(resolver, admissible_pick_cycle_fails) {
  run_fixture(utest_result, (fixture_t) {
    .index = {
      {
        .namespace = "spn",
        .name = "audio",
        .releases = {
          { .version = spn_semver_lit(1, 0, 0) },
          {
            .version = spn_semver_lit(2, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "tool", .version = "^1.0.0", .kind = SPN_INDEX_DEP_BUILD },
            }
          },
        }
      },
      {
        .namespace = "spn",
        .name = "tool",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "audio", .version = ">=1.0.0" },
            }
          },
        }
      },
    },
    .manifest = {
      .deps.package = {
        { .name = "spn/audio", .version = "^2.0.0" },
      }
    },
    .err = SPN_ERROR,
    .event = SPN_EVENT_ERR_UNIT_CYCLE,
  });
}

// Rule 1: a process boundary is a legal placement for a second dynamic
// instance. The tool's process loads gfx.so 1.0.0, the root's loads 2.0.0,
// and they never meet
UTEST_F(resolver, shared_lib_diverges_across_process) {
  run_fixture(utest_result, (fixture_t) {
    .index = {
      {
        .namespace = "spn",
        .name = "gfx",
        .releases = {
          { .version = spn_semver_lit(1, 0, 0), .targets = { { .name = "gfx", .linkages = { SPN_LIB_KIND_SHARED } } } },
          { .version = spn_semver_lit(2, 0, 0), .targets = { { .name = "gfx", .linkages = { SPN_LIB_KIND_SHARED } } } },
        }
      },
      {
        .namespace = "spn",
        .name = "tool",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "gfx", .version = "=1.0.0" },
            }
          },
        }
      },
    },
    .manifest = {
      .deps.package = {
        { .name = "spn/gfx", .version = "^2.0.0" },
        { .name = "spn/tool", .version = "^1.0.0", .kind = SPN_DEP_KIND_BUILD },
      }
    },
    .err = SPN_OK,
    .expected = {
      { .name = "gfx", .namespace = "spn", .version = spn_semver_lit(2, 0, 0), .unit = "" },
      { .name = "gfx", .namespace = "spn", .version = spn_semver_lit(1, 0, 0), .unit = "spn/tool" },
      { .name = "gfx", .namespace = "spn", .version = spn_semver_lit(1, 0, 0), .unit = "", .excluded = true },
    },
    .instances = {
      { .name = "spn/gfx", .count = 2 },
    },
  });
}

// Two groups discover the same lib instance, and the lib carries a build
// dep: the re-pushed boundary lands on the SAME tool group, so lib, gen, and
// gen's dep all stay single instances
UTEST_F(resolver, converged_lib_single_tool_group) {
  run_fixture(utest_result, (fixture_t) {
    .index = {
      {
        .namespace = "spn",
        .name = "lib",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "gen", .version = "^1.0.0", .kind = SPN_INDEX_DEP_BUILD },
            }
          },
        }
      },
      {
        .namespace = "spn",
        .name = "gen",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "foo", .version = "^1.0.0" },
            }
          },
        }
      },
      {
        .namespace = "spn",
        .name = "foo",
        .releases = {
          { .version = spn_semver_lit(1, 0, 0) },
          { .version = spn_semver_lit(1, 9, 0) },
        }
      },
      {
        .namespace = "spn",
        .name = "wrap",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "lib", .version = "^1.0.0" },
            }
          },
        }
      },
    },
    .manifest = {
      .deps.package = {
        { .name = "spn/lib", .version = "^1.0.0" },
        { .name = "spn/wrap", .version = "^1.0.0", .kind = SPN_DEP_KIND_BUILD },
      }
    },
    .err = SPN_OK,
    .expected = {
      { .name = "lib", .namespace = "spn", .version = spn_semver_lit(1, 0, 0), .unit = "" },
      { .name = "lib", .namespace = "spn", .version = spn_semver_lit(1, 0, 0), .unit = "spn/wrap" },
      { .name = "gen", .namespace = "spn", .version = spn_semver_lit(1, 0, 0), .unit = "spn/gen" },
      { .name = "foo", .namespace = "spn", .version = spn_semver_lit(1, 9, 0), .unit = "spn/gen" },
      { .name = "foo", .namespace = "spn", .version = spn_semver_lit(1, 9, 0), .unit = "", .excluded = true },
      { .name = "gen", .namespace = "spn", .version = spn_semver_lit(1, 0, 0), .unit = "", .excluded = true },
    },
    .instances = {
      { .name = "spn/lib", .count = 1 },
      { .name = "spn/gen", .count = 1 },
      { .name = "spn/foo", .count = 1 },
      { .name = "spn/wrap", .count = 1 },
    },
  });
}

// The root package is an executable, so its own private dep still lives on
// the root link line: a conflict with a public dep's requirement stays a hard
// error, exactly as if the dep weren't private
UTEST_F(resolver, root_private_dep_conflict_fails) {
  run_fixture(utest_result, (fixture_t) {
    .index = {
      {
        .namespace = "spn",
        .name = "gfx",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "foo", .version = "^2.0.0" },
            }
          },
        }
      },
      {
        .namespace = "spn",
        .name = "foo",
        .releases = {
          { .version = spn_semver_lit(1, 0, 0) },
          { .version = spn_semver_lit(2, 0, 0) },
        }
      },
    },
    .manifest = {
      .deps.package = {
        { .name = "spn/foo", .version = "=1.0.0", .private = true },
        { .name = "spn/gfx", .version = "^1.0.0" },
      }
    },
    .err = SPN_ERROR,
    .event = SPN_EVENT_ERR_UNSATISFIABLE_VERSION,
  });
}

// Lots of equal-priority free choices across three groups; the perturbed
// intern rounds in the harness assert the picks never depend on hash order
UTEST_F(resolver, determinism_many_ties) {
  run_fixture(utest_result, (fixture_t) {
    .index = {
      {
        .namespace = "spn",
        .name = "a",
        .releases = {
          { .version = spn_semver_lit(1, 0, 0) },
          { .version = spn_semver_lit(1, 1, 0) },
          { .version = spn_semver_lit(1, 2, 0) },
          { .version = spn_semver_lit(2, 0, 0) },
        }
      },
      {
        .namespace = "spn",
        .name = "b",
        .releases = {
          { .version = spn_semver_lit(1, 0, 0) },
          { .version = spn_semver_lit(1, 1, 0) },
          { .version = spn_semver_lit(2, 0, 0) },
        }
      },
      {
        .namespace = "spn",
        .name = "c",
        .releases = {
          { .version = spn_semver_lit(1, 0, 0) },
          { .version = spn_semver_lit(1, 1, 0) },
          { .version = spn_semver_lit(2, 0, 0) },
        }
      },
      {
        .namespace = "spn",
        .name = "t1",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "a", .version = ">=1.0.0" },
              { .namespace = "spn", .name = "b", .version = ">=1.0.0" },
              { .namespace = "spn", .name = "c", .version = "^1.0.0" },
            }
          },
        }
      },
      {
        .namespace = "spn",
        .name = "t2",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "a", .version = ">=1.0.0" },
              { .namespace = "spn", .name = "c", .version = ">=1.0.0" },
            }
          },
        }
      },
    },
    .manifest = {
      .deps.package = {
        { .name = "spn/a", .version = "^1.0.0" },
        { .name = "spn/b", .version = "^1.0.0" },
        { .name = "spn/t1", .version = "^1.0.0", .kind = SPN_DEP_KIND_BUILD },
        { .name = "spn/t2", .version = "^1.0.0", .kind = SPN_DEP_KIND_BUILD },
      }
    },
    .err = SPN_OK,
    .expected = {
      { .name = "a", .namespace = "spn", .version = spn_semver_lit(1, 2, 0), .unit = "" },
      { .name = "a", .namespace = "spn", .version = spn_semver_lit(1, 2, 0), .unit = "spn/t1" },
      { .name = "a", .namespace = "spn", .version = spn_semver_lit(1, 2, 0), .unit = "spn/t2" },
      { .name = "c", .namespace = "spn", .version = spn_semver_lit(1, 1, 0), .unit = "spn/t1" },
      { .name = "c", .namespace = "spn", .version = spn_semver_lit(1, 1, 0), .unit = "spn/t2" },
    },
    .instances = {
      { .name = "spn/a", .count = 1 },
      { .name = "spn/b", .count = 1 },
      { .name = "spn/c", .count = 1 },
    },
  });
}

// Root requests c before a, and a's range would admit an older c. Greedy
// takes c 2.0.0 first, leaving a's ^1.0.0 with no candidate; c 1.9.0
// satisfies both, so this should resolve.
UTEST_F(resolver, sibling_order_greedy) {
  run_fixture(utest_result, (fixture_t) {
    .index = {
      {
        .namespace = "spn",
        .name = "a",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "c", .version = "^1.0.0" },
            }
          },
        }
      },
      {
        .namespace = "spn",
        .name = "c",
        .releases = {
          { .version = spn_semver_lit(1, 9, 0) },
          { .version = spn_semver_lit(2, 0, 0) },
        }
      },
    },
    .manifest = {
      .deps.package = {
        { .name = "spn/c", .version = ">=1.0.0" },
        { .name = "spn/a", .version = "^1.0.0" },
      }
    },
    .err = SPN_OK,
    .expected = {
      { .name = "c", .namespace = "spn", .version = spn_semver_lit(1, 9, 0) },
      { .name = "a", .namespace = "spn", .version = spn_semver_lit(1, 0, 0) },
    },
  });
}

// Identical graph, opposite manifest order: a resolves first and pulls
// c 1.9.0, which the root's >=1.0.0 then accepts. Passing while
// sibling_order_greedy fails pins the order dependence.
UTEST_F(resolver, sibling_order_reversed) {
  run_fixture(utest_result, (fixture_t) {
    .index = {
      {
        .namespace = "spn",
        .name = "a",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "c", .version = "^1.0.0" },
            }
          },
        }
      },
      {
        .namespace = "spn",
        .name = "c",
        .releases = {
          { .version = spn_semver_lit(1, 9, 0) },
          { .version = spn_semver_lit(2, 0, 0) },
        }
      },
    },
    .manifest = {
      .deps.package = {
        { .name = "spn/a", .version = "^1.0.0" },
        { .name = "spn/c", .version = ">=1.0.0" },
      }
    },
    .err = SPN_OK,
    .expected = {
      { .name = "c", .namespace = "spn", .version = spn_semver_lit(1, 9, 0) },
      { .name = "a", .namespace = "spn", .version = spn_semver_lit(1, 0, 0) },
    },
  });
}

// Transitive form: neither range is declared at the root, so no manifest
// order avoids it. a's loose >=1.0.0 resolves first and takes c 2.0.0;
// b's ^1.0.0 then has no candidate, though c 1.9.0 satisfies everyone.
UTEST_F(resolver, transitive_sibling_order) {
  UTEST_SKIP("");
  run_fixture(utest_result, (fixture_t) {
    .index = {
      {
        .namespace = "spn",
        .name = "a",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "c", .version = ">=1.0.0" },
            }
          },
        }
      },
      {
        .namespace = "spn",
        .name = "b",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "c", .version = "^1.0.0" },
            }
          },
        }
      },
      {
        .namespace = "spn",
        .name = "c",
        .releases = {
          { .version = spn_semver_lit(1, 9, 0) },
          { .version = spn_semver_lit(2, 0, 0) },
        }
      },
    },
    .manifest = {
      .deps.package = {
        { .name = "spn/a", .version = "^1.0.0" },
        { .name = "spn/b", .version = "^1.0.0" },
      }
    },
    .err = SPN_OK,
    .expected = {
      { .name = "c", .namespace = "spn", .version = spn_semver_lit(1, 9, 0) },
    },
  });
}

// Avoidable dynamic duplicate: a's private dep pins c =1.0.0 and the root's
// >=1.0.0 admits 1.0.0 too, so c could unify. The root greedily picks 2.0.0
// before the private constraint is visible, the private scope splits, and
// both shared c copies land in process 0 as a hard error.
UTEST_F(resolver, avoidable_dynamic_dup) {
  run_fixture(utest_result, (fixture_t) {
    .index = {
      {
        .namespace = "spn",
        .name = "a",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "c", .version = "=1.0.0", .private = true },
            },
            .targets = {
              { .name = "a", .linkages = { SPN_LIB_KIND_SHARED } },
            }
          },
        }
      },
      {
        .namespace = "spn",
        .name = "c",
        .releases = {
          { .version = spn_semver_lit(1, 0, 0), .targets = { { .name = "c", .linkages = { SPN_LIB_KIND_SHARED } } } },
          { .version = spn_semver_lit(2, 0, 0), .targets = { { .name = "c", .linkages = { SPN_LIB_KIND_SHARED } } } },
        }
      },
    },
    .manifest = {
      .deps.package = {
        { .name = "spn/c", .version = ">=1.0.0" },
        { .name = "spn/a", .version = "^1.0.0" },
      }
    },
    .err = SPN_OK,
    .expected = {
      { .name = "c", .namespace = "spn", .version = spn_semver_lit(1, 0, 0) },
      { .name = "a", .namespace = "spn", .version = spn_semver_lit(1, 0, 0) },
    },
    .instances = {
      { .name = "spn/c", .count = 1 },
    },
  });
}
