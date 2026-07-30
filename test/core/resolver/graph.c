#include "resolver.h"

static const fixture_t graph_cases [] = {
  {
    .name = "none_resolves",
  },

  {
    .name = "linear_resolves",
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
  },

  {
    .name = "linear_missing",
    .manifest = {
      .deps.package = {
        { .name = "spn/math", .version = "^1.0.0" },
      }
    },
    .err = SPN_ERR_PKG_UNKNOWN,
    .event = SPN_EVENT_ERR_UNKNOWN_PKG,
  },

  {
    .name = "diamond_compatible",
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
  },

  {
    .name = "diamond_disjoint",
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
    .err = SPN_ERR_PKG_NO_MATCH,
    .event = SPN_EVENT_ERR_UNSATISFIABLE_VERSION,
  },

  {
    .name = "diamond_missing_renderer",
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
    .err = SPN_ERR_PKG_UNKNOWN,
    .event = SPN_EVENT_ERR_UNKNOWN_PKG,
  },

  {
    .name = "diamond_missing_math",
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
    .err = SPN_ERR_PKG_UNKNOWN,
    .event = SPN_EVENT_ERR_UNKNOWN_PKG,
  },

  {
    .name = "cycle_direct",
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
    .err = SPN_ERR_DEP_CYCLE,
    .event = SPN_EVENT_ERR_CIRCULAR_DEP,
  },

  {
    .name = "cycle_indirect",
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
    .err = SPN_ERR_DEP_CYCLE,
    .event = SPN_EVENT_ERR_CIRCULAR_DEP,
  },

  // audio has 3 versions in ^1.0.0; 1.2.0 requires missing "physics", 1.1.0 and 1.0.0 are fine.
  // A correct resolver should pick audio 1.1.0 (newest valid).
  // The naive resolver uses 1.2.0's deps (latest in range) and fails on missing physics.
  {
    .name = "backtrack_simple",
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
  },

  // The budget bounds search, never semantics: exhausting it is a hard error,
  // never a fallback split, so identical inputs always produce the same answer
  // or the same error. Same graph as backtrack_simple; a budget of one pop dies
  // before the resolvable audio 1.1.0 is ever tried.
  {
    .name = "budget_exhausted_fails",
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
  },

  {
    .name = "root_transitive_conflict",
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
    .err = SPN_ERR_PKG_NO_MATCH,
    .event = SPN_EVENT_ERR_UNSATISFIABLE_VERSION,
    .unsat = { .namespace = "spn", .name = "math", .requester = "test/root" },
  },

  // A conflict is reported as a conflict: the failure names the version the
  // scope already selected and who issued the losing request, never "no
  // version satisfies" when satisfying versions exist
  {
    .name = "conflict_reports_selected",
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
