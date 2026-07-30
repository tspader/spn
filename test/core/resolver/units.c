#include "resolver.h"

static const fixture_t units_cases [] = {
  // A build dep never links into the product, so it may resolve to a version
  // that conflicts with the linked one. The build dep roots its own unit.
  {
    .name = "build_dep_root_conflict",
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
  },

  // Same conflict, but the build dep edge is inside a dependency rather than
  // on the root
  {
    .name = "build_dep_transitive_conflict",
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
  },

  // Root test deps build into test executables, not the product, so they may
  // conflict with linked deps just like build deps
  {
    .name = "test_dep_root_conflict",
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
  },

  // A dependency's test deps never build at all in a consumer's graph; they
  // must be pruned rather than resolved into a conflict
  {
    .name = "transitive_test_dep_pruned",
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
  },

  // Compatible ranges across units must unify onto one instance; splitting
  // resolution is never license to build a package twice unnecessarily
  {
    .name = "build_dep_compatible_unifies",
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
  },

  // The build unit's range admits 2.0.0, but the root unit already picked
  // 1.9.0; the solver must prefer the already-picked version over the newest
  {
    .name = "preference_prefers_unified",
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
  },

  // The tool's pin admits a version the root's range also admits, so pooling
  // constraints would downgrade the root to 1.0.0 to share one instance. The
  // root's pick is sovereign; the tool pays with its own copy instead
  {
    .name = "build_dep_never_constrains_root",
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
  },

  // Convergence is a constraint, not a preference: the newest bar needs a foo
  // the root's pick excludes, so the tool's unit must fall back to the older
  // bar that shares the root's foo rather than split a second foo
  {
    .name = "convergence_forces_older_sibling",
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
  },

  // A group inheriting two decided versions of one name takes the earliest
  // admissible priority, not the newest version: opt's range admits both the
  // root's foo 1.9.0 and gen's split foo 2.0.0, and the root decided first.
  // Today's preference draws candidates in hash iteration order, so this pin
  // flickers with intern state — the order dependence is the defect it pins.
  {
    .name = "tiebreak_takes_earliest_admissible",
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
  },

  // Mutually exclusive inherited picks: use can hold the root's c 1.0.0 or mk's
  // b 2.0.0 (which needs c 2.0.0), never both. The root's pick has higher
  // priority, so c pins and b is the loser that splits — regardless of the
  // order use's own requests discover the names
  {
    .name = "tiebreak_higher_priority_pins_loser_splits",
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
  },

  // Deps are public by default: their types may appear in the package's API, so
  // they resolve in the consumer's scope and a conflict stays an error even
  // across a shared (dynamic) boundary
  {
    .name = "shared_lib_public_conflict_fails",
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
    .err = SPN_ERR_PKG_NO_MATCH,
    .event = SPN_EVENT_ERR_UNSATISFIABLE_VERSION,
  },

  // The public default holds when the shared lib is discovered transitively,
  // not just on a direct dep of the root
  {
    .name = "shared_lib_transitive_conflict_fails",
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
    .err = SPN_ERR_PKG_NO_MATCH,
    .event = SPN_EVENT_ERR_UNSATISFIABLE_VERSION,
  },

  // A dep declared private is an implementation detail: behind a shared
  // boundary its subtree resolves in gfx's own scope and may diverge from the
  // consumer's picks. gfx.so carries its own foo 1.0.0 copy, symbols hidden.
  {
    .name = "shared_lib_private_diverges",
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
  },

  // Privacy alone never costs a duplicate: gfx's private range admits foo
  // 2.0.0, which an isolated solve would take, but the root already decided
  // 1.9.0 and the private unit must converge onto that instance
  {
    .name = "private_compatible_unifies",
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
  },

  // Privacy inherits down the private edge: bar is private to gfx, so bar's
  // *public* dep baz is still private to gfx. baz unifies with bar inside gfx's
  // scope, not with the root's baz.
  {
    .name = "shared_lib_private_transitive_diverges",
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
  },

  // The static twin of shared_lib_public_conflict_fails: identical topology,
  // identical outcome
  {
    .name = "static_lib_conflict_fails",
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
    .err = SPN_ERR_PKG_NO_MATCH,
    .event = SPN_EVENT_ERR_UNSATISFIABLE_VERSION,
  },

  // Privacy never splits a link unit: a static gfx links into its consumer, so
  // its private foo lands on the same link line as the root's foo and the
  // conflict stays an error
  {
    .name = "static_lib_private_conflict_fails",
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
    .err = SPN_ERR_PKG_NO_MATCH,
    .event = SPN_EVENT_ERR_UNSATISFIABLE_VERSION,
  },

  // Boundaries are classified by the linkage selected for this build, not by
  // capability: gfx offers static and shared, default selection picks static,
  // so gfx links into its consumer and its private foo conflicts exactly as a
  // static-only gfx's would
  {
    .name = "private_static_default_conflicts",
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
  },

  // Two consumers of one shared lib share one instance of it, picked from the
  // intersection of their ranges
  {
    .name = "shared_lib_consumers_unify",
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
  },

  // Two consumers with disjoint ranges on one shared lib would load two gfx.so
  // instances into the root's process. The loader dedupes dynamic libs by name,
  // so this is physics, not policy: an error regardless of privacy
  {
    .name = "shared_lib_consumer_disjoint_fails",
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
    .err = SPN_ERR_PKG_NO_MATCH,
    .event = SPN_EVENT_ERR_UNSATISFIABLE_VERSION,
  },

  // Privacy can't dodge the dynamic rule: audio.so and video.so each privately
  // want a disjoint gfx, but gfx only builds shared, so both copies would load
  // into the root's process. Divergence requires the private dep to embed
  // static; a shared-only private dep with disjoint consumers is an error.
  {
    .name = "shared_lib_private_dynamic_dup_fails",
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
    .err = SPN_ERR_DYNAMIC_DUPLICATE,
    .event = SPN_EVENT_ERR_DYNAMIC_DUPLICATE,
  },

  // When you have two private, static dependencies in the tree which conflict,
  // they never see each other, so it's OK. But when they're shared, you'd have
  // conflicting symbols for the loader, so this must fail.
  {
    .name = "config_shared_dynamic_dup_fails",
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
    .err = SPN_ERR_DYNAMIC_DUPLICATE,
    .event = SPN_EVENT_ERR_DYNAMIC_DUPLICATE,
  },

  // A build dep cycle through a single instance can never build: the tool links
  // the same audio whose build waits on the tool. The boundary doesn't make the
  // graph acyclic, it only changes where the cycle is detected.
  {
    .name = "build_dep_cycle_fails",
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
    .err = SPN_ERR_UNIT_CYCLE,
    .event = SPN_EVENT_ERR_UNIT_CYCLE,
  },

  // The legal shape of a build dep cycle: the tool links an *older* release of
  // audio, so the two audio instances are distinct and the build order is
  // audio 1.0.0 -> tool -> audio 2.0.0. This is what instance-level (rather
  // than name-level) cycle detection must preserve.
  {
    .name = "build_dep_bootstrap",
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
  },

  // A failure recorded inside a candidate that backtracking recovers from must
  // not shadow the resolve's real failure
  {
    .name = "backtrack_failure_not_sticky",
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
    .err = SPN_ERR_PKG_NO_MATCH,
    .event = SPN_EVENT_ERR_UNSATISFIABLE_VERSION,
  },

  // A failed candidate must release the picks its subtree made: audio 1.1.0
  // resolves math 2.0.0 before failing on missing physics, and the fallback to
  // audio 1.0.0 needs math ^1.0.0, which resolves fine on a clean slate
  {
    .name = "backtrack_releases_subtree_picks",
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
  },

  // A pick stranded by a failed candidate must not be committed: audio 1.1.0
  // resolves math before failing on missing physics, audio 1.0.0 has no deps,
  // so the resolve holds no math at all
  {
    .name = "backtrack_orphan_not_committed",
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
  },

  // The tool's unit diverges on a, so b's resolved subtree differs between the
  // units even though both hold b 1.0.0. One b build can't link both a's: the
  // instance must split rather than merge two dep sets under one identity.
  {
    .name = "divergent_dep_splits_instance",
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
  },

  // Each build dep roots its own process: two consumers with disjoint ranges on
  // one tool hold two instances instead of conflicting
  {
    .name = "build_dep_disjoint_tools_diverge",
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
  },

  // A shared lib's private scope is per-instance: gfx 1.0.0 in the tool's unit
  // and gfx 2.0.0 in the root's each carry their own private foo
  {
    .name = "private_scopes_per_instance",
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
  },

  // Sibling groups converge on each other in manifest order: gen decides foo
  // first, and opt's wider range admits 2.0.0 but must take gen's 1.9.0. An
  // isolated solve of opt would split a pointless second foo.
  {
    .name = "sibling_tools_unify",
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
  },

  // Splitting units doesn't make missing packages resolvable; a boundary
  // still resolves for real
  {
    .name = "build_dep_missing_still_fails",
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
    .err = SPN_ERR_PKG_UNKNOWN,
    .event = SPN_EVENT_ERR_UNKNOWN_PKG,
  },
};

sp_test_each(resolver, units, fixture_t, units_cases) {
  return run_fixture(t, it);
}
