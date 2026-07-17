#include "common.h"

typedef struct {
  const c8* profile;
  const c8* manifest;
  test_when_t when;
  command_expect_t expect;
} opt_build_t;

typedef struct {
  const c8* project;
  const c8* copy [4];
  test_when_t when;
  opt_build_t builds [3];
} opt_test_t;

static bool opt_build_present(const opt_build_t* build) {
  return build->profile || build->manifest || build->expect.rc || build->expect.bin.name;
}

static void opt_set_manifest(s32* utest_result, fixture_t* fixture, const c8* manifest) {
  sp_str_t from = tmpfs_get(&fixture->fs, sp_str_view(manifest));
  sp_str_t to = tmpfs_get(&fixture->fs, sp_str_lit("spn.toml"));
  sp_str_t content = test_read_file(fixture->fs.mem, from);
  tmpfs_create(&fixture->fs, sp_str_lit("spn.toml"), content);
  sp_fs_remove_file(from);
  SP_EXPECT_NOT_EXISTS_TMPFS(&fixture->fs, from);
  SP_EXPECT_EXISTS_TMPFS(&fixture->fs, to);
}

static void run_opt_test(s32* utest_result, fixture_t* fixture, opt_test_t test) {
  sp_str_t blocked = test_when_blocked(&test.when);
  if (blocked.len) {
    utest_skip_reason(blocked);
    UTEST_SKIP("");
  }

  prepare_test(utest_result, fixture, test.project, test.copy);

  sp_carr_for(test.builds, it) {
    const opt_build_t* build = &test.builds[it];
    if (!opt_build_present(build)) {
      break;
    }

    if (test_when_blocked(&build->when).len) {
      continue;
    }

    if (build->manifest) {
      opt_set_manifest(utest_result, fixture, build->manifest);
    }

    command_test_t command = {
      .args = { "build" },
      .expect = build->expect,
    };
    if (build->profile) {
      command.args[1] = "-p";
      command.args[2] = build->profile;
      command.expect.bin.profile = build->profile;
    }
    run_command_test(utest_result, fixture, command);
  }
}

SPN_TEST_SUITE(when)

UTEST_F(when, facts) {
  tmpfs_init_named(&uf->fixture.fs, "when_facts");

  run_opt_test(utest_result, &uf->fixture, (opt_test_t) {
    .project = "test/integration/fixtures/when/facts",
    .builds = {
      { .expect = { .bin = { .name = "main" } } },
    },
  });
}

UTEST_F(when, not_form) {
  tmpfs_init_named(&uf->fixture.fs, "when_not_form");

  run_opt_test(utest_result, &uf->fixture, (opt_test_t) {
    .project = "test/integration/fixtures/when/not_form",
    .builds = {
      { .expect = { .bin = { .name = "main" } } },
    },
  });
}

UTEST_F(when, and_form) {
  tmpfs_init_named(&uf->fixture.fs, "when_and_form");

  run_opt_test(utest_result, &uf->fixture, (opt_test_t) {
    .project = "test/integration/fixtures/when/and_form",
    .builds = {
      { .expect = { .bin = { .name = "main", .rc = 10 } } },
      { .profile = "release", .expect = { .bin = { .name = "main", .rc = 20 } } },
    },
  });
}

UTEST_F(when, source) {
  tmpfs_init_named(&uf->fixture.fs, "when_source");

  run_opt_test(utest_result, &uf->fixture, (opt_test_t) {
    .project = "test/integration/fixtures/when/source",
    .copy = { "impl_linux.c", "impl_windows.c" },
    .builds = {
      { .expect = { .bin = { .name = "main" } } },
    },
  });
}

UTEST_F(when, flags) {
  tmpfs_init_named(&uf->fixture.fs, "when_flags");

  run_opt_test(utest_result, &uf->fixture, (opt_test_t) {
    .project = "test/integration/fixtures/when/flags",
    .builds = {
      { .expect = { .bin = { .name = "main" } } },
    },
  });
}

UTEST_F(when, system_deps) {
  tmpfs_init_named(&uf->fixture.fs, "when_system_deps");

  run_opt_test(utest_result, &uf->fixture, (opt_test_t) {
    .project = "test/integration/fixtures/when/system_deps",
    .builds = {
      { .expect = { .bin = { .name = "main" } } },
    },
  });
}

UTEST_F(when, dep_edge) {
  tmpfs_init_named(&uf->fixture.fs, "when_dep_edge");

  run_opt_test(utest_result, &uf->fixture, (opt_test_t) {
    .project = "test/integration/fixtures/when/dep_edge",
    .builds = {
      { .expect = { .bin = { .name = "main" } } },
    },
  });
}

SPN_TEST_SUITE(options)

UTEST_F(options, declare_default) {
  tmpfs_init_named(&uf->fixture.fs, "options_declare_default");

  run_opt_test(utest_result, &uf->fixture, (opt_test_t) {
    .project = "test/integration/fixtures/options/declare_default",
    .builds = {
      { .expect = { .bin = { .name = "main" } } },
    },
  });
}

UTEST_F(options, root_set) {
  tmpfs_init_named(&uf->fixture.fs, "options_root_set");

  run_opt_test(utest_result, &uf->fixture, (opt_test_t) {
    .project = "test/integration/fixtures/options/root_set",
    .copy = { "spn.on.toml" },
    .builds = {
      { .expect = { .bin = { .name = "main", .rc = 10 } } },
      { .manifest = "spn.on.toml", .expect = { .bin = { .name = "main", .rc = 20 } } },
    },
  });
}

UTEST_F(options, enum_gate) {
  tmpfs_init_named(&uf->fixture.fs, "options_enum_gate");

  run_opt_test(utest_result, &uf->fixture, (opt_test_t) {
    .project = "test/integration/fixtures/options/enum_gate",
    .builds = {
      { .expect = { .bin = { .name = "main" } } },
    },
  });
}

UTEST_F(options, public_define) {
  tmpfs_init_named(&uf->fixture.fs, "options_public_define");

  run_opt_test(utest_result, &uf->fixture, (opt_test_t) {
    .project = "test/integration/fixtures/options/public_define",
    .builds = {
      { .expect = { .bin = { .name = "main" } } },
    },
  });
}

UTEST_F(options, gates_dep) {
  tmpfs_init_named(&uf->fixture.fs, "options_gates_dep");

  run_opt_test(utest_result, &uf->fixture, (opt_test_t) {
    .project = "test/integration/fixtures/options/gates_dep",
    .builds = {
      {
        .expect = {
          .bin = { .name = "main", .rc = 10 },
          .events = {
            { .event = "resolve_package", .key = "version", .value = "1.0.0", .absent = true },
          },
        },
      },
      {
        .profile = "tracing",
        .expect = {
          .bin = { .name = "main", .rc = 20 },
          .events = {
            { .event = "resolve_package", .key = "version", .value = "1.0.0" },
          },
        },
      },
    },
  });
}

UTEST_F(options, edge_request) {
  tmpfs_init_named(&uf->fixture.fs, "options_edge_request");

  run_opt_test(utest_result, &uf->fixture, (opt_test_t) {
    .project = "test/integration/fixtures/options/edge_request",
    .builds = {
      { .expect = { .bin = { .name = "main" } } },
    },
  });
}

UTEST_F(options, additive) {
  tmpfs_init_named(&uf->fixture.fs, "options_additive");

  run_opt_test(utest_result, &uf->fixture, (opt_test_t) {
    .project = "test/integration/fixtures/options/additive",
    .copy = { "spn.off.toml" },
    .builds = {
      { .expect = { .bin = { .name = "main", .rc = 3 } } },
      { .manifest = "spn.off.toml", .expect = { .bin = { .name = "main", .rc = 2 } } },
    },
  });
}

UTEST_F(options, tiebreak) {
  tmpfs_init_named(&uf->fixture.fs, "options_tiebreak");

  run_opt_test(utest_result, &uf->fixture, (opt_test_t) {
    .project = "test/integration/fixtures/options/tiebreak",
    .builds = {
      { .expect = { .bin = { .name = "main" } } },
    },
  });
}

UTEST_F(options, defaults_false) {
  tmpfs_init_named(&uf->fixture.fs, "options_defaults_false");

  run_opt_test(utest_result, &uf->fixture, (opt_test_t) {
    .project = "test/integration/fixtures/options/defaults_false",
    .builds = {
      { .expect = { .bin = { .name = "main" } } },
    },
  });
}

UTEST_F(options, edge_gates_dep) {
  tmpfs_init_named(&uf->fixture.fs, "options_edge_gates_dep");

  run_opt_test(utest_result, &uf->fixture, (opt_test_t) {
    .project = "test/integration/fixtures/options/edge_gates_dep",
    .copy = { "vendor/*" },
    .builds = {
      {
        .expect = {
          .bin = { .name = "main", .rc = 1 },
          .events = {
            { .event = "resolve_package", .key = "name", .value = "core/b" },
          },
        },
      },
    },
  });
}

UTEST_F(options, late_gate) {
  tmpfs_init_named(&uf->fixture.fs, "options_late_gate");

  run_opt_test(utest_result, &uf->fixture, (opt_test_t) {
    .project = "test/integration/fixtures/options/late_gate",
    .copy = { "vendor/*" },
    .builds = {
      {
        .expect = {
          .rc = 1,
          .contains = { "k5", "k6" },
          .events = {
            { .event = "err_option", .key = "pkg", .value = "k5" },
            { .event = "err_option", .key = "a", .value = "k6" },
          },
        },
      },
    },
  });
}

UTEST_F(options, config_negated) {
  tmpfs_init_named(&uf->fixture.fs, "options_config_negated");

  run_opt_test(utest_result, &uf->fixture, (opt_test_t) {
    .project = "test/integration/fixtures/options/config_negated",
    .builds = {
      { .expect = { .rc = 1, .contains = { "verbose" } } },
    },
  });
}

UTEST_F(options, index_gated_dep) {
  tmpfs_init_named(&uf->fixture.fs, "options_index_gated_dep");

  run_opt_test(utest_result, &uf->fixture, (opt_test_t) {
    .project = "test/integration/fixtures/options/index_gated_dep",
    .builds = {
      { .expect = { .bin = { .name = "main", .rc = 1 } } },
    },
  });
}

UTEST_F(options, index_eager_gate) {
  tmpfs_init_named(&uf->fixture.fs, "options_index_eager_gate");

  run_opt_test(utest_result, &uf->fixture, (opt_test_t) {
    .project = "test/integration/fixtures/options/index_eager_gate",
    .builds = {
      {
        .expect = {
          .bin = { .name = "main", .rc = 1 },
          .events = {
            { .event = "err_unknown_pkg", .absent = true },
          },
          .files = {
            {
              .file = sp_str_lit(".home/storage/spn/packages/core/a.jsonl"),
              .contains = { "\"when\":{\"x\":true}", "\"type\":\"bool\"" },
            },
          },
        },
      },
    },
  });
}

UTEST_F(options, edge_gates_build_dep) {
  tmpfs_init_named(&uf->fixture.fs, "options_edge_gates_build_dep");

  run_opt_test(utest_result, &uf->fixture, (opt_test_t) {
    .project = "test/integration/fixtures/options/edge_gates_build_dep",
    .copy = { "vendor/*" },
    .builds = {
      {
        .expect = {
          .bin = { .name = "main", .rc = 1 },
          .events = {
            { .event = "resolve_package", .key = "name", .value = "core/b" },
          },
        },
      },
    },
  });
}

UTEST_F(options, dep_rebuild) {
  tmpfs_init_named(&uf->fixture.fs, "options_dep_rebuild");

  run_opt_test(utest_result, &uf->fixture, (opt_test_t) {
    .project = "test/integration/fixtures/options/dep_rebuild",
    .copy = { "spn.on.toml", "spn.off.toml" },
    .builds = {
      { .expect = { .bin = { .name = "main", .rc = 1 } } },
      { .manifest = "spn.on.toml", .expect = { .bin = { .name = "main", .rc = 2 } } },
      { .manifest = "spn.off.toml", .expect = { .bin = { .name = "main", .rc = 1 } } },
    },
  });
}

UTEST_F(options, fact_identity) {
  tmpfs_init_named(&uf->fixture.fs, "options_fact_identity");

  command_expect_t debug = { .bin = { .name = "main", .rc = 1 } };
  command_expect_t release = { .bin = { .name = "main", .rc = 2 } };
  run_opt_test(utest_result, &uf->fixture, (opt_test_t) {
    .project = "test/integration/fixtures/options/fact_identity",
    .builds = {
      { .expect = debug },
      { .profile = "release", .expect = release },
      { .expect = debug },
    },
  });
}

UTEST_F(options, default_identity) {
  tmpfs_init_named(&uf->fixture.fs, "options_default_identity");

  run_opt_test(utest_result, &uf->fixture, (opt_test_t) {
    .project = "test/integration/fixtures/options/default_identity",
    .copy = { "spn.on.toml" },
    .builds = {
      { .expect = { .bin = { .name = "main", .rc = 0 } } },
      { .manifest = "spn.on.toml", .expect = { .bin = { .name = "main", .rc = 3 } } },
    },
  });
}

UTEST_F(options, private_versions) {
  tmpfs_init_named(&uf->fixture.fs, "options_private_versions");

  run_opt_test(utest_result, &uf->fixture, (opt_test_t) {
    .project = "test/integration/fixtures/options/private_versions",
    .builds = {
      { .expect = { .bin = { .name = "main", .rc = 0 } } },
    },
  });
}

UTEST_F(options, config_unknown) {
  tmpfs_init_named(&uf->fixture.fs, "options_config_unknown");

  run_opt_test(utest_result, &uf->fixture, (opt_test_t) {
    .project = "test/integration/fixtures/options/config_unknown",
    .builds = {
      { .expect = { .rc = 1, .contains = { "nosuch" } } },
    },
  });
}

UTEST_F(options, config_stale) {
  tmpfs_init_named(&uf->fixture.fs, "options_config_stale");

  run_opt_test(utest_result, &uf->fixture, (opt_test_t) {
    .project = "test/integration/fixtures/options/config_stale",
    .builds = {
      { .expect = { .rc = 1, .contains = { "zed" } } },
    },
  });
}
