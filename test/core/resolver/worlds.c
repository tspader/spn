#include "resolver.h"

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
    .index = {
      {
        .namespace = "spn",
        .name = "a",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .options = {
              { .name = "x", .type = SPN_OPTION_TYPE_BOOL },
              { .name = "y", .type = SPN_OPTION_TYPE_BOOL },
            },
            .deps = {
              { .namespace = "spn", .name = "c", .version = "=1.0.0", .when = { { .key = "x", .is_bool = true, .b = true } } },
              { .namespace = "spn", .name = "c", .version = "=2.0.0", .when = { { .key = "y", .is_bool = true, .b = true } } },
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
        { .name = "spn/a", .version = "^1.0.0" },
      }
    },
    .err = SPN_ERROR,
    .event = SPN_EVENT_ERR_UNSATISFIABLE_VERSION,
  });
}

// I3.12c: a phantom bool-gated edge holds a sibling to the older version;
// the over-pin is the deliberate price of demand-independent selection
sp_test(resolver, cut3_phantom_edge_over_pins) {
  return sp_test_skip(t, "worlds cut 3");
  return run_fixture(t, &(fixture_t) {
    .index = {
      {
        .namespace = "spn",
        .name = "a",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .options = {
              { .name = "x", .type = SPN_OPTION_TYPE_BOOL },
            },
            .deps = {
              { .namespace = "spn", .name = "c", .version = "=1.0.0", .when = { { .key = "x", .is_bool = true, .b = true } } },
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
        { .name = "spn/a", .version = "^1.0.0" },
        { .name = "spn/c", .version = "^1.0.0" },
      }
    },
  });
}
