#include "resolver.h"

static const fixture_t convergence_cases [] = {
  // Two sibling tools both pin a to the same older version, so each ends up
  // holding b 1.0.0 over a 1.0.0. Identical resolved subtrees must collapse
  // to ONE split instance shared by both tool units, never one per group: two
  // b instances total (the root's over a 1.9.0 plus one shared split),
  // never three
  {
    .name = "split_instances_reconverge",
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
  },

  // The tool pins a older, so the divergence propagates up the chain: c and b
  // hold their root versions but over a different a subtree, splitting BOTH at
  // the same version purely transitively
  {
    .name = "split_propagates_through_middle",
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
  },

  // The kept pin set is the lexicographically-first feasible subset of the
  // inherited table [a@1, b@1, c@1]. Keeping a@1 forces x 1.0.0, which excludes
  // b@1 (drops, splits to b 2.0.0) and never requests c (c@1 holds vacuously).
  // A newest-first solve would take x 1.2.0 and split a instead
  {
    .name = "pin_walk_lexicographic_subset",
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
  },

  // convergence_forces_older_sibling, two levels down: keeping the root's foo
  // pick is only satisfiable via older choices at BOTH transitive levels below
  // the tool's request (mid 1.0.0 instead of 1.1.0, bar 1.0.0 instead of 2.0.0)
  {
    .name = "convergence_forces_older_transitive",
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
  },

  // A tool's tool: tb is a build dep two process boundaries deep. Only tb's
  // unit may split foo; the middle tool converges with the root's pick
  {
    .name = "nested_tool_splits_only_inner",
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
  },

  // Privacy nests: inner is private to gfx, leaf is private to inner. Two
  // shared boundaries deep, leaf may still diverge from the root's pick, and
  // neither inner nor its leaf leaks into the root unit
  {
    .name = "private_inside_private_diverges",
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
  },

  // Nested privacy can't dodge the loader: leaf only builds shared, so the
  // root's leaf 2.0.0 and inner's private leaf 1.0.0 both load into the root
  // process regardless of being two private boundaries deep
  {
    .name = "private_inside_private_dynamic_dup_fails",
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
    .err = SPN_ERR_DYNAMIC_DUPLICATE,
    .event = SPN_EVENT_ERR_DYNAMIC_DUPLICATE,
  },

  // A build dep discovered inside a shared lib's private subtree still roots
  // its own process: the tool may take a foo the root's pick excludes, and that
  // foo never enters gfx's private unit
  {
    .name = "build_dep_inside_private_diverges",
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
  },

  // One package reached through all three edge kinds at once: the root links
  // foo publicly, gfx carries a private copy behind its shared boundary, and
  // the tool holds a third in its own process. Three instances, each fenced
  // into exactly its own unit
  {
    .name = "boundary_diamond_three_instances",
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
  },

  // Bootstrap plus divergence: the tool must link the older audio (the root's
  // 2.0.0 is excluded), and its OTHER dep zed also falls outside the root's
  // pick, so the tool's unit diverges on both while the build order stays
  // acyclic: audio 1.0.0 -> tool -> audio 2.0.0
  {
    .name = "bootstrap_with_divergent_sibling",
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
  },

  // Rule 5's same-version case: audio's and video's private scopes both hold
  // zed 1.0.0, but over different math subtrees. zed only builds shared, the
  // loader dedups by name, and one consumer would run against the wrong
  // embedded math: an error even though the versions are equal
  {
    .name = "same_version_dynamic_dup_fails",
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
    .err = SPN_ERR_DYNAMIC_DUPLICATE,
    .event = SPN_EVENT_ERR_DYNAMIC_DUPLICATE,
  },

  // Two private groups, one split: gfxa's range excludes the root's foo 2.0.0
  // and splits to 1.9.0. gfxb's range admits both decided versions, and must
  // take the EARLIEST admissible pick (the root's 2.0.0), not gfxa's newer
  // split priority-wise or the numerically-newest
  {
    .name = "private_groups_converge_on_earliest",
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
  },

  // A root test dep's transitive subtree diverges from the product's picks in
  // its own process instead of conflicting
  {
    .name = "test_dep_transitive_diverges",
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
  },

  // The tool's range admits the root's audio 2.0.0, so convergence takes it —
  // and manufactures a genuine instance cycle (audio 2.0.0 -> tool -> audio
  // 2.0.0). Rule 6: instance cycles are errors, never split triggers; the
  // resolver must not quietly fall back to audio 1.0.0
  {
    .name = "admissible_pick_cycle_fails",
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
    .err = SPN_ERR_UNIT_CYCLE,
    .event = SPN_EVENT_ERR_UNIT_CYCLE,
  },

  // Rule 1: a process boundary is a legal placement for a second dynamic
  // instance. The tool's process loads gfx.so 1.0.0, the root's loads 2.0.0,
  // and they never meet
  {
    .name = "shared_lib_diverges_across_process",
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
  },

  // Two groups discover the same lib instance, and the lib carries a build
  // dep: the re-pushed boundary lands on the SAME tool group, so lib, gen, and
  // gen's dep all stay single instances
  {
    .name = "converged_lib_single_tool_group",
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
  },

  // The root package is an executable, so its own private dep still lives on
  // the root link line: a conflict with a public dep's requirement stays a hard
  // error, exactly as if the dep weren't private
  {
    .name = "root_private_dep_conflict_fails",
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
    .err = SPN_ERR_PKG_NO_MATCH,
    .event = SPN_EVENT_ERR_UNSATISFIABLE_VERSION,
  },

  // Lots of equal-priority free choices across three groups; the perturbed
  // intern rounds in the harness assert the picks never depend on hash order
  {
    .name = "determinism_many_ties",
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
  },

  // Root requests c before a, and a's range would admit an older c. Greedy
  // takes c 2.0.0 first, leaving a's ^1.0.0 with no candidate; c 1.9.0
  // satisfies both, so this should resolve.
  {
    .name = "sibling_order_greedy",
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
  },

  // Identical graph, opposite manifest order: a resolves first and pulls
  // c 1.9.0, which the root's >=1.0.0 then accepts. Passing while
  // sibling_order_greedy fails pins the order dependence.
  {
    .name = "sibling_order_reversed",
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
  },

  // Avoidable dynamic duplicate: a's private dep pins c =1.0.0 and the root's
  // >=1.0.0 admits 1.0.0 too, so c could unify. The root greedily picks 2.0.0
  // before the private constraint is visible, the private scope splits, and
  // both shared c copies land in process 0 as a hard error.
  {
    .name = "avoidable_dynamic_dup",
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
  },
};

sp_test_each(resolver, convergence, fixture_t, convergence_cases) {
  return run_fixture(t, it);
}

// Transitive form: neither range is declared at the root, so no manifest
// order avoids it. a's loose >=1.0.0 resolves first and takes c 2.0.0;
// b's ^1.0.0 then has no candidate, though c 1.9.0 satisfies everyone.
sp_test(resolver, transitive_sibling_order) {
  return sp_test_skip(t, "");
  return run_fixture(t, &(fixture_t) {
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
  });
}
