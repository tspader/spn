SPN_TEST_SUITE(reexport)

// main deps only sdl_mixer; sdl_mixer.h includes sdl.h from its public sdl
// dep. main must compile without declaring sdl.
UTEST_F(reexport, transitive) {
  UTEST_SKIP("");
  tmpfs_init_named(&uf->fixture.fs, "reexport_transitive");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/reexport/transitive",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_RUN_BIN, .bin = { .name = "main", .rc = 0 } },
    },
  });
}

// sdl is sdl_mixer's private dep and stays out of its public header. main
// includes sdl.h without declaring sdl: the compile must fail, both today
// and after the closure lands.
UTEST_F(reexport, private_hidden) {
  tmpfs_init_named(&uf->fixture.fs, "reexport_private_hidden");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/reexport/private_hidden",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build", .rc = 1 } },
      { .kind = ACTION_VERIFY_EVENT, .verify_event = { .event = "target_build_failed" } },
    },
  });
}

// Privacy gates re-export, not the declarer's own view: sdl_mixer compiles
// and links against its private sdl, and main consumes sdl_mixer normally.
UTEST_F(reexport, private_owner) {
  tmpfs_init_named(&uf->fixture.fs, "reexport_private_owner");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/reexport/private_owner",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_RUN_BIN, .bin = { .name = "main", .rc = 0 } },
    },
  });
}

// A private edge prunes its whole subtree: engine's private sdl_mixer
// publicly re-exports sdl, but neither leaks through engine. A walk that
// skips private edges without cutting their subtrees would wrongly hand
// main sdl's headers through sdl_mixer's public sdl edge.
UTEST_F(reexport, private_subtree) {
  tmpfs_init_named(&uf->fixture.fs, "reexport_private_subtree");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/reexport/private_subtree",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build", .rc = 1 } },
      { .kind = ACTION_VERIFY_EVENT, .verify_event = { .event = "target_build_failed" } },
    },
  });
}

// The compile closure crosses shared-library boundaries. sdl_mixer is a
// shared lib, so the link closure stops there; main still parses sdl.h
// through sdl_mixer.h and needs sdl's include path. main references no sdl
// symbol, only the SDL_INIT_OK macro, so this cannot pass by accident of
// linking.
UTEST_F(reexport, shared_boundary) {
  UTEST_SKIP("");
  tmpfs_init_named(&uf->fixture.fs, "reexport_shared_boundary");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/reexport/shared_boundary",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_RUN_BIN, .bin = { .name = "main", .rc = 0 } },
    },
  });
}

// sdl's opengl option carries define = "SDL_OPENGL", public = true. The
// define rides the include walk: every TU that can see sdl.h compiles with
// it. main compares the value baked into sdl's and sdl_mixer's objects with
// what its own TU sees, so a missing define in any single TU fails.
UTEST_F(reexport, public_define) {
  UTEST_SKIP("");
  tmpfs_init_named(&uf->fixture.fs, "reexport_public_define");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/reexport/public_define",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_RUN_BIN, .bin = { .name = "main", .rc = 0 } },
    },
  });
}
