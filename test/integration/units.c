// Link-unit resolution: deps are public by default and resolve in their
// consumer's scope, so version conflicts are errors. A dep declared private
// is an implementation detail; behind a shared (dynamic) boundary it may
// diverge from the consumer's picks, and privacy inherits down the private
// edge. Divergence is also legal across process boundaries (build deps, test
// deps, tools). Every passing case asserts end behavior (built binaries
// return version-derived exit codes), so a resolver that silently links the
// wrong version fails the same as one that errors. Cases marked *pin* pass
// against the flat resolver and must keep passing; the rest fail until link
// units land.
SPN_TEST_SUITE(units)

// Root links foo 2.0.0 but its build scripts want foo 1.0.0. Today lowering
// collapses the two requests into one (last kind wins), so main silently
// links 1.0.0 and exits nonzero.
UTEST_F(units, build_dep_conflict) {
  tmpfs_init_named(&uf->fixture.fs, "units_build_dep_conflict");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/units/build_dep_conflict",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_RUN_BIN, .bin = { .name = "main", .rc = 0 } },
    },
  });
}

// Root links foo 2.0.0 and build-depends on tool, whose own dep wants foo
// 1.0.0: the conflict arrives through the build dep's subtree rather than
// from the root's requests. Today this is an unsatisfiable resolution; with
// link units the tool's subtree resolves in its own unit.
UTEST_F(units, build_dep_transitive_conflict) {
  tmpfs_init_named(&uf->fixture.fs, "units_build_dep_transitive_conflict");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/units/build_dep_transitive_conflict",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_VERIFY_NO_EVENT, .verify_event = { .event = "err_unsatisfiable_version" } },
      { .kind = ACTION_RUN_BIN, .bin = { .name = "main", .rc = 0 } },
    },
  });
}

// gfx's foo dep is public (the default), so it resolves in main's scope and
// the conflict stays a hard error, reported as an unsatisfiable version
UTEST_F(units, shared_conflict) {
  tmpfs_init_named(&uf->fixture.fs, "units_shared_conflict");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/units/shared_conflict",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build", .rc = 1 } },
      { .kind = ACTION_VERIFY_EVENT, .verify_event = { .event = "err_unsatisfiable_version" } },
    },
  });
}

// gfx declares foo private, so gfx.so carries its own foo 1.0.0 copy while
// main links foo 2.0.0. The binary observes both copies: private divergence
// behind a dynamic boundary is the one duplicate spn permits.
UTEST_F(units, shared_private) {
  tmpfs_init_named(&uf->fixture.fs, "units_shared_private");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/units/shared_private",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_VERIFY_NO_EVENT, .verify_event = { .event = "err_unsatisfiable_version" } },
      { .kind = ACTION_VERIFY_NO_EVENT, .verify_event = { .event = "err_dynamic_duplicate" } },
      { .kind = ACTION_RUN_BIN, .bin = { .name = "main", .rc = 0 } },
    },
  });
}

// The static twin of shared_conflict: identical topology, identical outcome
UTEST_F(units, static_conflict) {
  tmpfs_init_named(&uf->fixture.fs, "units_static_conflict");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/units/static_conflict",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build", .rc = 1 } },
      { .kind = ACTION_VERIFY_EVENT, .verify_event = { .event = "err_unsatisfiable_version" } },
    },
  });
}

// The root accepts foo ^1.0.0 and the tool accepts foo >=1.0.0: without the
// preference heuristic the tool's unit grabs 2.0.0 and foo builds twice; with
// it, both units share the root's 1.5.0
UTEST_F(units, no_double_build) {
  tmpfs_init_named(&uf->fixture.fs, "units_no_double_build");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/units/no_double_build",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_VERIFY_EVENT, .verify_event = { .event = "resolve_package", .key = "version", .value = "1.5.0" } },
      { .kind = ACTION_VERIFY_NO_EVENT, .verify_event = { .event = "resolve_package", .key = "version", .value = "2.0.0" } },
      { .kind = ACTION_RUN_BIN, .bin = { .name = "main", .rc = 0 } },
    },
  });
}

// applib build-depends on tool and tool links the same applib instance.
// applib's script can't compile until tool builds, and tool can't link until
// applib builds; unbuildable no matter how resolution splits. The flat
// resolver reports a name cycle; link units must report an instance cycle.
UTEST_F(units, build_dep_cycle) {
  tmpfs_init_named(&uf->fixture.fs, "units_build_dep_cycle");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/units/build_dep_cycle",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build", .rc = 1 } },
      { .kind = ACTION_VERIFY_EVENT, .verify_event = { .event = "err_unit_cycle" } },
    },
  });
}

// applib 2.0.0 build-deps tool, and tool links applib 1.0.0: two distinct
// instances, so there is no cycle and the build order is applib 1.0.0 ->
// tool -> applib 2.0.0. The flat resolver sees one applib and errors; this is
// the legal shape instance-level cycle detection must preserve.
UTEST_F(units, build_dep_bootstrap) {
  tmpfs_init_named(&uf->fixture.fs, "units_build_dep_bootstrap");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/units/build_dep_bootstrap",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_VERIFY_NO_EVENT, .verify_event = { .event = "err_unit_cycle" } },
      { .kind = ACTION_VERIFY_NO_EVENT, .verify_event = { .event = "err_unsatisfiable_version" } },
      { .kind = ACTION_RUN_BIN, .bin = { .name = "main", .rc = 0 } },
    },
  });
}
