#include "common.h"

SPN_TEST_SUITE(exports)

// Shared lib S defines spn_test_s; its own symbols are always exported, so a
// dlsym probe from an exe linking S must find it.
UTEST_F(exports, own_exported) {
  tmpfs_init_named(&uf->fixture.fs, "exports_own_exported");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/exports/own_exported",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_RUN_BIN, .bin = { .name = "main", .rc = 0 } },
    },
  });
}

// S privately embeds static P and routes spn_test_s through it, so P's
// objects are pulled into libS. P's spn_test_p must not leak out of S's
// dynamic symbol table.
UTEST_F(exports, private_hidden) {
  tmpfs_init_named(&uf->fixture.fs, "exports_private_hidden");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/exports/private_hidden",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_RUN_BIN, .bin = { .name = "main", .rc = 0 } },
    },
  });
}

// S has public static dep D but references none of it. A public static dep
// is part of S's API surface, so spn_test_d must still be exported from
// libS: public static deps link whole-archive into shared libs.
UTEST_F(exports, public_reexported) {
  tmpfs_init_named(&uf->fixture.fs, "exports_public_reexported");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/exports/public_reexported",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_RUN_BIN, .bin = { .name = "main", .rc = 0 } },
    },
  });
}

// S's public static deps A and B both define spn_test_x. Whole-archive
// linking makes the collision a hard link failure instead of a silent
// first-archive-wins pick.
UTEST_F(exports, collision_loud) {
  tmpfs_init_named(&uf->fixture.fs, "exports_collision_loud");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/exports/collision_loud",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build", .rc = 1 } },
      { .kind = ACTION_VERIFY_EVENT, .verify_event = { .event = "link_failed" } },
    },
  });
}

// main statically embeds P and also links shared S, which privately embeds
// its own copy of P. P counts calls, so if S's copy were exported and
// interposed either way, one side's counter would absorb the other's bumps.
UTEST_F(exports, no_interference) {
  tmpfs_init_named(&uf->fixture.fs, "exports_no_interference");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/exports/no_interference",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_RUN_BIN, .bin = { .name = "main", .rc = 0 } },
    },
  });
}
