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

  spn.intern = sp_intern_new(spn_allocator);

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
} index_dep_t;

typedef struct {
  spn_semver_t version;
  index_dep_t deps[4];
} index_rel_t;

typedef struct {
  const c8* namespace;
  const c8* name;
  index_rel_t releases[8];
} index_pkg_t;

typedef struct {
  const c8* name;
  const c8* version;
} manifest_dep_t;

typedef struct {
  manifest_dep_t package[8];
  const c8* system[8];
} manifest_deps_t;

typedef struct {
  const c8* name;
  const c8* namespace;
  spn_semver_t version;
} expected_t;

typedef struct {
  index_pkg_t index[8];
  struct {
    manifest_deps_t deps;
  } manifest;
  spn_err_t err;
  expected_t expected[8];
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
}

UTEST_F_TEARDOWN(resolver) {

}

///////////
// MOCKS //
///////////
void spn_index_cache_init(spn_index_cache_t* cache, spn_index_arr_t* indexes) {

}

spn_index_pkg_t* spn_index_cache_get_package(spn_index_cache_t* cache, spn_pkg_id_t id) {
  sp_str_t qualified = spn_pkg_id_to_qualified_name(id);
  return sp_str_ht_get(state.cache, qualified);
}

spn_index_rel_t* spn_index_cache_get_release(spn_index_cache_t* cache, spn_pkg_id_t id, spn_semver_t version) {
  return SP_NULLPTR;
}


///////////////
// EXECUTOR //
//////////////
static void build_cache(fixture_t* fixture) {
  sp_carr_for(fixture->index, i) {
    index_pkg_t* desc = &fixture->index[i];
    if (!desc->name) break;

    spn_pkg_id_t id = {
      .namespace = sp_str_view(desc->namespace),
      .name = sp_str_view(desc->name),
    };

    spn_index_pkg_t pkg = { .id = id };

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

      sp_carr_for(rel_desc->deps, d) {
        index_dep_t* dep_desc = &rel_desc->deps[d];
        if (!dep_desc->name) break;

        sp_da_push(rel.deps, ((spn_index_dep_t) {
          .id = {
            .namespace = sp_str_view(dep_desc->namespace),
            .name = sp_str_view(dep_desc->name),
          },
          .version = sp_str_view(dep_desc->version),
        }));
      }

      sp_da_push(pkg.releases, rel);
    }

    sp_str_t qualified = spn_pkg_id_to_qualified_name(id);
    sp_str_ht_insert(state.cache, qualified, pkg);
  }
}

static void build_manifest(fixture_t* fixture, spn_pkg_info_t* manifest) {
  sp_carr_for(fixture->manifest.deps.package, i) {
    manifest_dep_t* dep = &fixture->manifest.deps.package[i];
    if (!dep->name) break;

    spn_pkg_id_t id = spn_qualified_name_to_pkg_id(sp_str_view(dep->name));
    sp_str_t qualified = spn_pkg_id_to_qualified_name(id);

    spn_requested_pkg_t req = {
      .qualified = spn_pkg_canonicalize_name(sp_str_view(dep->name)),
      .source = SPN_PKG_SOURCE_INDEX,
      .index.range = spn_semver_parse_range(sp_str_view(dep->version)),
    };

    sp_ht_insert(manifest->deps, qualified, req);
  }

  sp_carr_for(fixture->manifest.deps.system, i) {
    const c8* dep = fixture->manifest.deps.system[i];
    if (!dep) break;
    sp_da_push(manifest->system_deps, sp_str_view(dep));
  }
}

void run_fixture(s32* utest_result, fixture_t fixture) {
  build_cache(&fixture);

  spn_pkg_info_t manifest = sp_zero;
  build_manifest(&fixture, &manifest);

  spn_event_buffer_t* events = spn_event_buffer_new();
  spn_index_cache_t cache = sp_zero;
  spn_pkg_registry_t registry = sp_zero;
  spn_resolver_t resolver = sp_zero;
  spn_resolver_init(&resolver, &cache, &registry, events);

  spn_resolve_query_t query = sp_zero;
  sp_ht_for_kv(manifest.deps, it) {
    spn_resolve_query_add(&query, *it.val);
  }

  spn_err_t resolve = spn_resolve_from_solver(&resolver, &query);
  ASSERT_EQ(resolve, fixture.err);
  if (resolve != SPN_OK) {
    return;
  }

  sp_carr_for(fixture.expected, it) {
    expected_t expected = fixture.expected[it];
    if (!expected.name) break;

    spn_pkg_id_t id = {
      .namespace = sp_str_view(expected.namespace),
      .name = sp_str_view(expected.name),
    };
    sp_str_t qualified = spn_pkg_id_to_qualified_name(id);

    spn_resolved_pkg_t* resolved = sp_str_ht_get(query.result, qualified);
    ASSERT_NE(resolved, SP_NULLPTR);
    ASSERT_TRUE(spn_semver_eq(resolved->version, expected.version));
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
