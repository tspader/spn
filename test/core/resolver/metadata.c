#include "resolver.h"

static const fixture_t metadata_cases [] = {
  // A yanked release is invisible to free selection: the newest in-range
  // version is skipped for the newest un-yanked one
  {
    .name = "yanked_release_skipped",
    .index = {
      {
        .namespace = "spn",
        .name = "a",
        .releases = {
          { .version = spn_semver_lit(1, 0, 0) },
          { .version = spn_semver_lit(1, 1, 0), .yanked = true },
        }
      },
    },
    .manifest = {
      .deps.package = {
        { .name = "spn/a", .version = "^1.0.0" },
      }
    },
  },

  // A range only a yanked release satisfies is unsatisfiable, reported as
  // no-version-in-range rather than a conflict
  {
    .name = "yanked_only_candidate_fails",
    .index = {
      {
        .namespace = "spn",
        .name = "a",
        .releases = {
          { .version = spn_semver_lit(1, 0, 0), .yanked = true },
          { .version = spn_semver_lit(2, 0, 0) },
        }
      },
    },
    .manifest = {
      .deps.package = {
        { .name = "spn/a", .version = "^1.0.0" },
      }
    },
    .err = SPN_ERR_PKG_NO_MATCH,
    .event = SPN_EVENT_ERR_UNSATISFIABLE_VERSION,
    .unsat = { .namespace = "spn", .name = "a", .requester = "test/root" },
  },

  // An index dep whose version range fails to parse is a manifest error naming
  // the release, never a silent skip
  {
    .name = "index_invalid_range",
    .index = {
      {
        .namespace = "spn",
        .name = "a",
        .releases = {
          {
            .version = spn_semver_lit(1, 0, 0),
            .deps = {
              { .namespace = "spn", .name = "b", .version = "kram" },
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
    .err = SPN_ERR_DEP_MANIFEST,
    .event = SPN_EVENT_ERR_MANIFEST,
  },
};

sp_test_each(resolver, metadata, fixture_t, metadata_cases) {
  return run_fixture(t, it);
}
