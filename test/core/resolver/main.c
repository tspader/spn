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
// is the root unit, and a qualified name is the unit rooted at that request
typedef struct {
  const c8* name;
  const c8* namespace;
  spn_semver_t version;
  const c8* unit;
} expected_t;

// asserts how many instances (distinct versions) of a package the resolve
// holds; count 1 pins that compatible units share one build
typedef struct {
  const c8* name;
  u32 count;
} instance_count_t;

// event asserts that a failed resolve pushed the right error kind; zero
// (SPN_EVENT_ERR) means don't check
typedef struct {
  index_pkg_t index[8];
  struct {
    manifest_deps_t deps;
  } manifest;
  spn_err_t err;
  spn_build_event_kind_t event;
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

        spn_index_rel_target_t target = { .name = sp_str_view(target_desc->name) };
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

static void build_query(fixture_t* fixture, spn_resolve_query_t* query) {
  sp_carr_for(fixture->manifest.deps.package, i) {
    manifest_dep_t* dep = &fixture->manifest.deps.package[i];
    if (!dep->name) break;

    spn_resolve_query_add(query, (spn_requested_pkg_t) {
      .qualified = spn_pkg_canonicalize_name(sp_str_view(dep->name)),
      .source = SPN_PKG_SOURCE_INDEX,
      .kind = dep->kind,
      .private = dep->private,
      .index.range = spn_semver_parse_range(sp_str_view(dep->version)),
    });
  }
}

static bool resolved_in_unit(spn_resolved_pkg_t* pkg, const c8* unit) {
  if (!unit) return true;

  sp_intern_id_t root = 0;
  if (*unit) {
    root = sp_intern_get_or_insert(spn.intern, spn_pkg_canonicalize_name(sp_str_view(unit)));
  }

  // A package with no explicit memberships belongs to the root unit
  if (sp_da_empty(pkg->units)) {
    return root == 0;
  }

  sp_da_for(pkg->units, it) {
    if (pkg->units[it].root == root) return true;
  }
  return false;
}

void run_fixture(s32* utest_result, fixture_t fixture) {
  sp_mem_t mem = sp_mem_arena_as_allocator(ctx_get()->arena);
  build_cache(mem, &fixture);

  spn_event_buffer_t* events = spn_event_buffer_new(sp_mem_os_new());
  spn_index_cache_t cache = sp_zero;
  spn_pkg_registry_t registry = sp_zero;
  spn_resolver_t resolver = sp_zero;
  spn_resolver_init(&resolver, mem, spn.intern, &cache, &registry, events);

  spn_resolve_query_t query = sp_zero;
  spn_resolve_query_init(mem, &query);
  build_query(&fixture, &query);

  spn_err_t resolve = spn_resolve_from_solver(&resolver, &query);
  ASSERT_EQ(resolve, fixture.err);

  if (fixture.event) {
    bool pushed = false;
    sp_da(spn_build_event_t) drained = spn_event_buffer_drain(mem, events);
    sp_da_for(drained, it) {
      if (drained[it].kind == fixture.event) {
        pushed = true;
        break;
      }
    }
    ASSERT_TRUE(pushed);
  }

  if (resolve != SPN_OK) {
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
      if (!resolved_in_unit(pkg, expected.unit)) continue;
      found = true;
      break;
    }
    ASSERT_TRUE(found);
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
      { .name = "foo", .namespace = "spn", .version = spn_semver_lit(2, 0, 0), .unit = "spn/gfx" },
      { .name = "foo", .namespace = "spn", .version = spn_semver_lit(1, 0, 0), .unit = "spn/gfx" },
    },
    .instances = {
      { .name = "spn/gfx", .count = 2 },
      { .name = "spn/foo", .count = 2 },
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
