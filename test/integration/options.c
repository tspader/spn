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

// Every build fact resolves from the profile: os/arch/mode gate defines that
// must be present, and a wasi-gated define must be absent.
UTEST_F(when, facts) {
  tmpfs_init_named(&uf->fixture.fs, "when_facts");

  run_opt_test(utest_result, &uf->fixture, (opt_test_t) {
    .project = "test/integration/fixtures/when/facts",
    .builds = {
      { .expect = { .bin = { .name = "main" } } },
    },
  });
}

// { not = v }: os != wasi holds on the host, mode != debug does not
UTEST_F(when, not_form) {
  tmpfs_init_named(&uf->fixture.fs, "when_not_form");

  run_opt_test(utest_result, &uf->fixture, (opt_test_t) {
    .project = "test/integration/fixtures/when/not_form",
    .builds = {
      { .expect = { .bin = { .name = "main" } } },
    },
  });
}

// Multiple keys AND: { os = "linux", mode = "release" } is false in debug
// even though os matches (an OR evaluator returns 20 on the first build),
// true in release
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

// Gated source entries: both impl files define impl_value, so failing to
// exclude the other platform's impl is a duplicate-symbol link error and
// failing to include the host's is an undefined reference
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

// Gated flags entries: the wasi-gated /DFLAG_INVALID is not a valid gcc
// argument, so failing to exclude it breaks the compile; the os-gated
// define proves inclusion
UTEST_F(when, flags) {
  tmpfs_init_named(&uf->fixture.fs, "when_flags");

  run_opt_test(utest_result, &uf->fixture, (opt_test_t) {
    .project = "test/integration/fixtures/when/flags",
    .builds = {
      { .expect = { .bin = { .name = "main" } } },
    },
  });
}

// Gated entries in the package system_deps list: ws2_32 does not exist on
// linux, so failing to exclude the false arm breaks the link. The true arm
// is c++ rather than m because musl folds libm into libc — main references
// std::terminate by mangled name, which only resolves if the matching entry
// survives filtering.
UTEST_F(when, system_deps) {
  tmpfs_init_named(&uf->fixture.fs, "when_system_deps");

  run_opt_test(utest_result, &uf->fixture, (opt_test_t) {
    .project = "test/integration/fixtures/when/system_deps",
    .builds = {
      { .expect = { .bin = { .name = "main" } } },
    },
  });
}

// Gated dependency edges: wsock has no package in the index, so the build
// succeeds only if the false edge is pruned before resolution ever looks at
// it; plat proves the true edge resolves and links
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

// A declared bool defaults on and its define reaches the dep's own TUs; no
// consumer says anything
UTEST_F(options, declare_default) {
  tmpfs_init_named(&uf->fixture.fs, "options_declare_default");

  run_opt_test(utest_result, &uf->fixture, (opt_test_t) {
    .project = "test/integration/fixtures/options/declare_default",
    .builds = {
      { .expect = { .bin = { .name = "main" } } },
    },
  });
}

// [config.codec] overrides the dep's default (rc 10 proves the root won, not
// the default), and swapping the root manifest flips the built behavior —
// option values participate in staleness, a stale store returns 10 twice
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

// Options join the when key domain: the enum value gates which backend
// source compiles inside the dep. Both backends define render_backend, so a
// wrong or double selection fails the link, and the root picks the
// non-default so the default can't mask a no-op
UTEST_F(options, enum_gate) {
  tmpfs_init_named(&uf->fixture.fs, "options_enum_gate");

  run_opt_test(utest_result, &uf->fixture, (opt_test_t) {
    .project = "test/integration/fixtures/options/enum_gate",
    .builds = {
      { .expect = { .bin = { .name = "main" } } },
    },
  });
}

// define + public = true: the option's define lands in the consumer's own
// compile, not just the dep's. The general propagation machinery is pinned
// in reexport; this pins the option -> public define wiring specifically.
UTEST_F(options, public_define) {
  tmpfs_init_named(&uf->fixture.fs, "options_public_define");

  run_opt_test(utest_result, &uf->fixture, (opt_test_t) {
    .project = "test/integration/fixtures/options/public_define",
    .builds = {
      { .expect = { .bin = { .name = "main" } } },
    },
  });
}

// The root's own option gates its dep edge and a profile flips it: default
// build never resolves tracer (rc 10, no resolve event), -p tracing resolves
// and links it (rc 20). Version 1.0.0 identifies tracer; the root is 0.1.0.
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

// A consumer edge requests a value with no root config in play: the request
// alone flips the dep off its default
UTEST_F(options, edge_request) {
  tmpfs_init_named(&uf->fixture.fs, "options_edge_request");

  run_opt_test(utest_result, &uf->fixture, (opt_test_t) {
    .project = "test/integration/fixtures/options/edge_request",
    .builds = {
      { .expect = { .bin = { .name = "main" } } },
    },
  });
}

// additive = true: with the root silent, requests union — liba's audio =
// true wins over libb's false and libb's video rides along, so caps == 3
// through both consumers. When the root explicitly sets audio = false the
// root is authoritative like any other setter: no request unions over it,
// so caps drops to video alone.
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

// The identical topology settles when the root sets the value: liba's gl
// request loses to the root, both consumers observe vk
UTEST_F(options, tiebreak) {
  tmpfs_init_named(&uf->fixture.fs, "options_tiebreak");

  run_opt_test(utest_result, &uf->fixture, (opt_test_t) {
    .project = "test/integration/fixtures/options/tiebreak",
    .builds = {
      { .expect = { .bin = { .name = "main" } } },
    },
  });
}

// defaults = false declines the dep's defaults wholesale; only the edge's
// ogg request survives, so caps == 2 rather than 3
UTEST_F(options, defaults_false) {
  tmpfs_init_named(&uf->fixture.fs, "options_defaults_false");

  run_opt_test(utest_result, &uf->fixture, (opt_test_t) {
    .project = "test/integration/fixtures/options/defaults_false",
    .builds = {
      { .expect = { .bin = { .name = "main" } } },
    },
  });
}

// The composition of edge_request and gates_dep: the root's edge request
// flips x on, which must pull a's x-gated dep b into the build even though
// the request isn't known when a's edges are first gated
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

// Each re-resolve discovers one more chain level, because a package's edge
// request only lands once its own edge exists: a six-deep chain of gated
// deps blows the resolve cap, and the LATE_GATE error must name the exact
// gate that never settled rather than failing anonymously
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

// { not = ... } is a request-only form: an authoritative setter negating a
// value is meaningless, so the root config declaring one is a load error
// naming the option rather than a silently dropped clause
UTEST_F(options, config_negated) {
  tmpfs_init_named(&uf->fixture.fs, "options_config_negated");

  run_opt_test(utest_result, &uf->fixture, (opt_test_t) {
    .project = "test/integration/fixtures/options/config_negated",
    .builds = {
      { .expect = { .rc = 1, .contains = { "verbose" } } },
    },
  });
}

// A when-gated dep inside an index package whose entry predates published
// gates (the fixture pins a raw old-format index): the index metadata lists
// the dep unconditionally, but a false gate in the fetched manifest must cut
// the edge, not fail the build
UTEST_F(options, index_gated_dep) {
  tmpfs_init_named(&uf->fixture.fs, "options_index_gated_dep");

  run_opt_test(utest_result, &uf->fixture, (opt_test_t) {
    .project = "test/integration/fixtures/options/index_gated_dep",
    .builds = {
      { .expect = { .bin = { .name = "main", .rc = 1 } } },
    },
  });
}

// The published entry carries a's option declarations and the dep's gate, so
// the resolver cuts the edge before ever looking the name up: the gated dep
// doesn't exist in the index at all, and the build must not care
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

// edge_gates_dep for a build-kind dep: the request flips x on, so a's define
// is applied and its gated build dep b must resolve with it — the define
// without the dep is a misbuild
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

// Options are part of a package's transitive build identity: b's compilation
// observes a's public define, so each value of a's option is a distinct b.
// The flip-forward works by accident of mtimes (a's new store stamp dirties
// b); the flip-back must not hand back the on-flavored b that the default
// store path now holds
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

// Test that swapping the build profile on a dependency with no binary artifacts
// still flips its fingerprint.
//
// The dependency has a build script which writes a header defining a value
// differently depending on the profile's mode.
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

// Mutually-referencing defaults make an explicitly-set build look default
// under the final env: x = true and y = true each match the other's when-arm
// default, but the all-defaults build resolved both false, so the two builds
// are different configurations and must not share a store path
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

// Private scoping resolves b at 1.0.0 (x on, inside a) and 2.0.0 (x off, at
// the root): two distinct packages, so the disagreeing requests are not a
// conflict and each instance builds with its own value
UTEST_F(options, private_versions) {
  tmpfs_init_named(&uf->fixture.fs, "options_private_versions");

  run_opt_test(utest_result, &uf->fixture, (opt_test_t) {
    .project = "test/integration/fixtures/options/private_versions",
    .builds = {
      { .expect = { .bin = { .name = "main", .rc = 0 } } },
    },
  });
}

// A [config.<pkg>] key naming no package in the build is a misconfiguration,
// not a no-op: the typo'd key must fail the build by name
UTEST_F(options, config_unknown) {
  tmpfs_init_named(&uf->fixture.fs, "options_config_unknown");

  run_opt_test(utest_result, &uf->fixture, (opt_test_t) {
    .project = "test/integration/fixtures/options/config_unknown",
    .builds = {
      { .expect = { .rc = 1, .contains = { "nosuch" } } },
    },
  });
}

// config_unknown against the packages actually in the build, not every
// package a resolve pass ever loaded: the root's edge request flips a's y
// off, pruning b and zed after they loaded, so [config.zed] names a package
// that isn't in the build and must fail rather than sit silently dead
UTEST_F(options, config_stale) {
  tmpfs_init_named(&uf->fixture.fs, "options_config_stale");

  run_opt_test(utest_result, &uf->fixture, (opt_test_t) {
    .project = "test/integration/fixtures/options/config_stale",
    .builds = {
      { .expect = { .rc = 1, .contains = { "zed" } } },
    },
  });
}
