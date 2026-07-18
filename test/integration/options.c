#include "common.h"

typedef struct {
  const c8* profile;
  const c8* manifest;
  const c8* target;
  bool alternate;
  bool present;
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
  return build->present || build->profile || build->manifest || build->target ||
    build->alternate || build->expect.rc || build->expect.bin.name;
}

static void opt_set_manifest(s32* utest_result, fixture_t* fixture, const c8* manifest) {
  sp_str_t from = tmpfs_get(&fixture->fs, sp_cstr_as_str(manifest));
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

  const test_toolchain_t* toolchain = test_toolchain();
  u32 ran = 0;
  sp_carr_for(test.builds, it) {
    const opt_build_t* build = &test.builds[it];
    if (!opt_build_present(build)) {
      break;
    }

    const c8* target = build->target;
    if (build->alternate) {
      target = test_target_alternate();
      if (!target) {
        blocked = sp_fmt(fixture->fs.mem, "{} has no cross target", sp_fmt_cstr(toolchain->name)).value;
        continue;
      }
    }

    test_when_t when = build->when;
    if (!when.target) {
      when.target = target;
    }
    blocked = test_when_blocked(&when);
    if (blocked.len) {
      continue;
    }

    if (build->manifest) {
      opt_set_manifest(utest_result, fixture, build->manifest);
    }

    command_test_t command = {
      .args = { "build" },
      .expect = build->expect,
    };
    u32 arg = 1;
    if (build->profile) {
      command.args[arg++] = "-p";
      command.args[arg++] = build->profile;
      command.expect.bin.profile = build->profile;
    }
    if (target) {
      command.args[arg++] = "--target";
      command.args[arg++] = target;
    }
    if (!sp_str_equal_cstr(sp_str_lit("zig"), toolchain->name)) {
      command.args[arg++] = "--toolchain";
      command.args[arg++] = toolchain->name;
    }
    if ((command.expect.bin.name || command.expect.bin.path.len) && !test_when_runs(&when)) {
      command.expect.bin.build_only = true;
    }
    run_command_test(utest_result, fixture, command);
    ran++;
  }

  if (!ran) {
    utest_skip_reason(blocked);
    UTEST_SKIP("");
  }
}

SPN_TEST_SUITE(options)

UTEST_F(options, when) {
  tmpfs_init_named(&uf->fixture.fs, "when");

  run_opt_test(utest_result, &uf->fixture, (opt_test_t) {
    .project = "test/integration/fixtures/options/when",
    .copy = { "src/*", "packages/*" },
    .builds = {
      { .present = true },
      { .alternate = true },
    },
  });
}


UTEST_F(options, public_define) {
  tmpfs_init_named(&uf->fixture.fs, "options_public_define");

  run_opt_test(utest_result, &uf->fixture, (opt_test_t) {
    .project = "test/integration/fixtures/options/public_define",
    .builds = { { .present = true } },
  });
}

UTEST_F(options, gates_dep) {
  tmpfs_init_named(&uf->fixture.fs, "options_gates_dep");

  run_opt_test(utest_result, &uf->fixture, (opt_test_t) {
    .project = "test/integration/fixtures/options/gates_dep",
    .builds = {
      { .present = true },
      { .profile = "tracing" },
    },
  });
}

UTEST_F(options, additive) {
  tmpfs_init_named(&uf->fixture.fs, "options_additive");

  run_opt_test(utest_result, &uf->fixture, (opt_test_t) {
    .project = "test/integration/fixtures/options/additive",
    .copy = { "main.off.c", "spn.off.toml" },
    .builds = {
      { .present = true },
      { .manifest = "spn.off.toml" },
    },
  });
}

UTEST_F(options, edge_gates_dep) {
  tmpfs_init_named(&uf->fixture.fs, "options_edge_gates_dep");

  run_opt_test(utest_result, &uf->fixture, (opt_test_t) {
    .project = "test/integration/fixtures/options/edge_gates_dep",
    .copy = { "vendor/*" },
    .builds = { { .present = true } },
  });
}

UTEST_F(options, index_gated_dep) {
  tmpfs_init_named(&uf->fixture.fs, "options_index_gated_dep");

  run_opt_test(utest_result, &uf->fixture, (opt_test_t) {
    .project = "test/integration/fixtures/options/index_gated_dep",
    .builds = { { .present = true } },
  });
}

UTEST_F(options, index_eager_gate) {
  tmpfs_init_named(&uf->fixture.fs, "options_index_eager_gate");

  run_opt_test(utest_result, &uf->fixture, (opt_test_t) {
    .project = "test/integration/fixtures/options/index_eager_gate",
    .builds = { { .present = true } },
  });
}

UTEST_F(options, edge_gates_build_dep) {
  tmpfs_init_named(&uf->fixture.fs, "options_edge_gates_build_dep");

  run_opt_test(utest_result, &uf->fixture, (opt_test_t) {
    .project = "test/integration/fixtures/options/edge_gates_build_dep",
    .copy = { "vendor/*" },
    .builds = { { .present = true } },
  });
}

UTEST_F(options, dep_rebuild) {
  tmpfs_init_named(&uf->fixture.fs, "options_dep_rebuild");

  run_opt_test(utest_result, &uf->fixture, (opt_test_t) {
    .project = "test/integration/fixtures/options/dep_rebuild",
    .copy = { "main.on.c", "spn.on.toml", "spn.off.toml" },
    .builds = {
      { .present = true },
      { .manifest = "spn.on.toml" },
      { .manifest = "spn.off.toml" },
    },
  });
}

UTEST_F(options, fact_identity) {
  tmpfs_init_named(&uf->fixture.fs, "options_fact_identity");

  run_opt_test(utest_result, &uf->fixture, (opt_test_t) {
    .project = "test/integration/fixtures/options/fact_identity",
    .builds = {
      { .present = true },
      { .profile = "release" },
      { .present = true },
    },
  });
}

UTEST_F(options, default_identity) {
  tmpfs_init_named(&uf->fixture.fs, "options_default_identity");

  run_opt_test(utest_result, &uf->fixture, (opt_test_t) {
    .project = "test/integration/fixtures/options/default_identity",
    .copy = { "main.on.c", "spn.on.toml" },
    .builds = {
      { .present = true },
      { .manifest = "spn.on.toml" },
    },
  });
}

UTEST_F(options, private_versions) {
  tmpfs_init_named(&uf->fixture.fs, "options_private_versions");

  run_opt_test(utest_result, &uf->fixture, (opt_test_t) {
    .project = "test/integration/fixtures/options/private_versions",
    .builds = { { .present = true } },
  });
}
