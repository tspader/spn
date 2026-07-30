#include "resolver.h"

static const fixture_t gates_cases [] = {
  // A fact gate on an index dep evaluates at resolve time: the false edge is
  // never resolved (its package exists nowhere), the true edge is
  {
    .name = "index_dep_fact_gate",
    .facts = { .os = SPN_OS_LINUX, .arch = SPN_ARCH_X64, .abi = SPN_ABI_GNU },
    .index = {
      {
        .namespace = "spn",
        .name = "a",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "b", .version = "^1.0.0", .when = { { "os", "linux" } } },
              { .namespace = "spn", .name = "w", .version = "^1.0.0", .when = { { "os", "windows" } } },
            }
          },
        }
      },
      {
        .namespace = "spn",
        .name = "b",
        .releases = {
          { .version = spn_semver_lit(1, 0, 0) },
        }
      },
    },
    .manifest = {
      .deps.package = {
        { .name = "spn/a", .version = "^1.0.0" },
      }
    },
  },

  // The { not = v } form gates with opposite polarity: os != windows holds on
  // this profile, os != linux does not
  {
    .name = "index_dep_negated_gate",
    .facts = { .os = SPN_OS_LINUX, .arch = SPN_ARCH_X64, .abi = SPN_ABI_GNU },
    .index = {
      {
        .namespace = "spn",
        .name = "a",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "b", .version = "^1.0.0", .when = { { .key = "os", .str = "windows", .negated = true } } },
              { .namespace = "spn", .name = "w", .version = "^1.0.0", .when = { { .key = "os", .str = "linux", .negated = true } } },
            }
          },
        }
      },
      {
        .namespace = "spn",
        .name = "b",
        .releases = {
          { .version = spn_semver_lit(1, 0, 0) },
        }
      },
    },
    .manifest = {
      .deps.package = {
        { .name = "spn/a", .version = "^1.0.0" },
      }
    },
  },

  // The gate env includes the release's own declared options at their
  // resolved defaults: x defaults true and admits its edge, y is an
  // undefaulted bool (false) and cuts the edge to the missing package
  {
    .name = "index_dep_option_gate",
    .index = {
      {
        .namespace = "spn",
        .name = "a",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .options = {
              { .name = "x", .type = SPN_OPTION_TYPE_BOOL, .fallback = { .is_bool = true, .b = true } },
              { .name = "y", .type = SPN_OPTION_TYPE_BOOL },
            },
            .deps = {
              { .namespace = "spn", .name = "b", .version = "^1.0.0", .when = { { .key = "x", .is_bool = true, .b = true } } },
              { .namespace = "spn", .name = "w", .version = "^1.0.0", .when = { { .key = "y", .is_bool = true, .b = true } } },
            }
          },
        }
      },
      {
        .namespace = "spn",
        .name = "b",
        .releases = {
          { .version = spn_semver_lit(1, 0, 0) },
        }
      },
    },
    .manifest = {
      .deps.package = {
        { .name = "spn/a", .version = "^1.0.0" },
      }
    },
  },

  // Root manifest deps gate through the local-package path exactly like index
  // deps gate through candidates
  {
    .name = "root_dep_gate",
    .facts = { .os = SPN_OS_LINUX, .arch = SPN_ARCH_X64, .abi = SPN_ABI_GNU },
    .index = {
      {
        .namespace = "spn",
        .name = "a",
        .releases = {
          { .version = spn_semver_lit(1, 0, 0) },
        }
      },
    },
    .manifest = {
      .deps.package = {
        { .name = "spn/a", .version = "^1.0.0", .when = { { "os", "linux" } } },
        { .name = "spn/w", .version = "^1.0.0", .when = { { "os", "windows" } } },
      }
    },
  },
};

sp_test_each(resolver, gates, fixture_t, gates_cases) {
  return run_fixture(t, it);
}
