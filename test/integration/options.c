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

static sp_err_t opt_set_manifest(sp_test_t* t, fixture_t* fixture, const c8* manifest) {
  sp_str_t from = fixture_path(fixture, sp_cstr_as_str(manifest));
  sp_str_t to = fixture_path(fixture, sp_str_lit("spn.toml"));
  sp_str_t content = test_read_file(fixture->mem, from);
  fixture_create(fixture, sp_str_lit("spn.toml"), content);
  sp_fs_remove_file(from);
  sp_must(t, !sp_fs_exists(from));
  sp_must(t, sp_fs_exists(to));
  return SP_OK;
}

static sp_err_t run_opt_test(sp_test_t* t, opt_test_t test) {
  fixture_t fixture = sp_zero;
  sp_try(fixture_init(t, &fixture));

  sp_str_t blocked = test_when_blocked(&test.when);
  if (blocked.len) {
    return sp_test_skip(t, "{}", sp_fmt_str(blocked));
  }

  sp_try(prepare_test(t, &fixture, test.project, test.copy));

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
        blocked = sp_fmt(fixture.mem, "{} has no cross target", sp_fmt_cstr(toolchain->name)).value;
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
      sp_try(opt_set_manifest(t, &fixture, build->manifest));
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
    sp_try(run_command(t, &fixture, command));
    ran++;
  }

  if (!ran) {
    return sp_test_skip(t, "{}", sp_fmt_str(blocked));
  }
  return SP_OK;
}

sp_test(options, when) {
  return run_opt_test(t, (opt_test_t) {
    .project = "test/integration/fixtures/options/when",
    .copy = { "src/*", "packages/*" },
    .builds = {
      { .present = true },
      { .alternate = true },
    },
  });
}

sp_test(options, public_define) {
  return run_opt_test(t, (opt_test_t) {
    .project = "test/integration/fixtures/options/public_define",
    .builds = { { .present = true } },
  });
}

sp_test(options, gates_dep) {
  return run_opt_test(t, (opt_test_t) {
    .project = "test/integration/fixtures/options/gates_dep",
    .builds = {
      { .present = true },
      { .profile = "tracing" },
    },
  });
}

sp_test(options, additive) {
  return run_opt_test(t, (opt_test_t) {
    .project = "test/integration/fixtures/options/additive",
    .copy = { "main.off.c", "spn.off.toml" },
    .builds = {
      { .present = true },
      { .manifest = "spn.off.toml" },
    },
  });
}

sp_test(options, edge_gates_dep) {
  return run_opt_test(t, (opt_test_t) {
    .project = "test/integration/fixtures/options/edge_gates_dep",
    .copy = { "vendor/*" },
    .builds = { { .present = true } },
  });
}

sp_test(options, index_gated_dep) {
  return run_opt_test(t, (opt_test_t) {
    .project = "test/integration/fixtures/options/index_gated_dep",
    .builds = { { .present = true } },
  });
}

sp_test(options, index_eager_gate) {
  return run_opt_test(t, (opt_test_t) {
    .project = "test/integration/fixtures/options/index_eager_gate",
    .builds = { { .present = true } },
  });
}

sp_test(options, edge_gates_build_dep) {
  return run_opt_test(t, (opt_test_t) {
    .project = "test/integration/fixtures/options/edge_gates_build_dep",
    .copy = { "vendor/*" },
    .builds = { { .present = true } },
  });
}

sp_test(options, dep_rebuild) {
  return run_opt_test(t, (opt_test_t) {
    .project = "test/integration/fixtures/options/dep_rebuild",
    .copy = { "main.on.c", "spn.on.toml", "spn.off.toml" },
    .builds = {
      { .present = true },
      { .manifest = "spn.on.toml" },
      { .manifest = "spn.off.toml" },
    },
  });
}

sp_test(options, fact_identity) {
  return run_opt_test(t, (opt_test_t) {
    .project = "test/integration/fixtures/options/fact_identity",
    .builds = {
      { .present = true },
      { .profile = "release" },
      { .present = true },
    },
  });
}

sp_test(options, default_identity) {
  return run_opt_test(t, (opt_test_t) {
    .project = "test/integration/fixtures/options/default_identity",
    .copy = { "main.on.c", "spn.on.toml" },
    .builds = {
      { .present = true },
      { .manifest = "spn.on.toml" },
    },
  });
}

sp_test(options, private_versions) {
  return run_opt_test(t, (opt_test_t) {
    .project = "test/integration/fixtures/options/private_versions",
    .builds = { { .present = true } },
  });
}
