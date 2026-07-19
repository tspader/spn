#include "common.h"

SPN_TEST_SUITE(freshness)

UTEST_F(freshness, noop) {
  tmpfs_init_named(&uf->fixture.fs, "freshness_noop");

  run_rebuild_test(utest_result, &uf->fixture, (rebuild_test_t) {
    .project = "test/integration/fixtures/freshness/bin",
    .first = {
      .args = { "build" },
      .expect = {
        .events = { { .event = SPN_EVENT_TARGET_BUILD_PASSED } },
        .exists = { exe("main") },
      },
    },
    .rebuilds = {
      {
        .command = {
          .args = { "build" },
          .expect.events = {
            { .event = SPN_EVENT_TARGET_BUILD_PASSED, .absent = true },
            { .event = SPN_EVENT_LINK_PASSED, .absent = true },
          },
        },
      },
    },
    .watches = {
      { .file = exe("main"), .mtime = REBUILD_MTIME_UNCHANGED },
    },
  });
}

UTEST_F(freshness, source_change) {
  tmpfs_init_named(&uf->fixture.fs, "freshness_source_change");

  run_rebuild_test(utest_result, &uf->fixture, (rebuild_test_t) {
    .project = "test/integration/fixtures/freshness/bin",
    .copy = { "main.change.c" },
    .first = {
      .args = { "build" },
      .expect.exists = { exe("main") },
    },
    .rebuilds = {
      {
        .change.moves = {
          { .from = sp_str_lit("main.change.c"), .to = sp_str_lit("main.c") },
        },
        .command = {
          .args = { "build" },
          .expect.events = { { .event = SPN_EVENT_TARGET_BUILD_PASSED } },
        },
      },
    },
    .watches = {
      { .file = exe("main"), .mtime = REBUILD_MTIME_CHANGED },
    },
  });
}

UTEST_F(freshness, touch_without_change) {
  tmpfs_init_named(&uf->fixture.fs, "freshness_touch_without_change");

  run_rebuild_test(utest_result, &uf->fixture, (rebuild_test_t) {
    .project = "test/integration/fixtures/freshness/bin",
    .copy = { "main.same.c" },
    .first = {
      .args = { "build" },
      .expect.exists = { exe("main") },
    },
    .rebuilds = {
      {
        .change.moves = {
          { .from = sp_str_lit("main.same.c"), .to = sp_str_lit("main.c") },
        },
        .command = {
          .args = { "build" },
          .expect.events = { { .event = SPN_EVENT_TARGET_BUILD_PASSED, .absent = true } },
        },
      },
    },
    .watches = {
      { .file = exe("main"), .mtime = REBUILD_MTIME_UNCHANGED },
    },
  });
}

UTEST_F(freshness, dep_source_change) {
  tmpfs_init_named(&uf->fixture.fs, "freshness_dep_source_change");

  run_rebuild_test(utest_result, &uf->fixture, (rebuild_test_t) {
    .project = "test/integration/fixtures/freshness/dep",
    .copy = { "packages/*" },
    .first = {
      .args = { "build", "-p", "debug" },
      .expect.exists = { static_lib("spum"), store_file("bin/main") },
    },
    .rebuilds = {
      {
        .change.moves = {
          { .from = sp_str_lit("packages/spum/spum.change.c"), .to = sp_str_lit("packages/spum/spum.c") },
        },
        .command = {
          .args = { "build", "-p", "debug" },
          .expect.events = { { .event = SPN_EVENT_TARGET_BUILD_PASSED } },
        },
      },
      {
        .command = {
          .args = { "build", "-p", "debug" },
          .expect.events = { { .event = SPN_EVENT_TARGET_BUILD_PASSED, .absent = true } },
        },
      },
    },
    .watches = {
      { .file = static_lib("spum"), .mtime = REBUILD_MTIME_CHANGED },
      { .file = store_file("bin/main"), .mtime = REBUILD_MTIME_CHANGED },
    },
  });
}

UTEST_F(freshness, dep_header_inert) {
  tmpfs_init_named(&uf->fixture.fs, "freshness_dep_header_inert");

  run_rebuild_test(utest_result, &uf->fixture, (rebuild_test_t) {
    .project = "test/integration/fixtures/freshness/dep",
    .copy = { "packages/*" },
    .first = {
      .args = { "build", "-p", "debug" },
      .expect.exists = { store_file("bin/main") },
    },
    .rebuilds = {
      {
        .change.moves = {
          { .from = sp_str_lit("packages/spum/spum.inert.h"), .to = sp_str_lit("packages/spum/spum.h") },
        },
        .command = {
          .args = { "build", "-p", "debug" },
          .expect.events = { { .event = SPN_EVENT_TARGET_BUILD_PASSED } },
        },
      },
    },
    .watches = {
      { .file = store_file("bin/main"), .mtime = REBUILD_MTIME_UNCHANGED },
    },
  });
}

UTEST_F(freshness, dep_header_change) {
  tmpfs_init_named(&uf->fixture.fs, "freshness_dep_header_change");

  run_rebuild_test(utest_result, &uf->fixture, (rebuild_test_t) {
    .project = "test/integration/fixtures/freshness/dep",
    .copy = { "packages/*", "main.code.c" },
    .first = {
      .args = { "build", "-p", "debug" },
      .expect.exists = { store_file("bin/main") },
    },
    .rebuilds = {
      {
        .change.moves = {
          { .from = sp_str_lit("main.code.c"), .to = sp_str_lit("main.c") },
          { .from = sp_str_lit("packages/spum/spum.seven.h"), .to = sp_str_lit("packages/spum/spum.h") },
        },
        .command = {
          .args = { "build", "-p", "debug" },
          .expect.bin = { .name = "main", .rc = 7 },
        },
      },
      {
        .change.moves = {
          { .from = sp_str_lit("packages/spum/spum.eight.h"), .to = sp_str_lit("packages/spum/spum.h") },
        },
        .command = {
          .args = { "build", "-p", "debug" },
          .expect.bin = { .name = "main", .rc = 8 },
        },
      },
    },
    .watches = {
      { .file = store_file("bin/main"), .mtime = REBUILD_MTIME_CHANGED },
    },
  });
}

UTEST_F(freshness, output_deleted) {
  tmpfs_init_named(&uf->fixture.fs, "freshness_output_deleted");

  run_rebuild_test(utest_result, &uf->fixture, (rebuild_test_t) {
    .project = "test/integration/fixtures/freshness/bin",
    .first = {
      .args = { "build" },
      .expect.exists = { exe("main") },
    },
    .rebuilds = {
      {
        .change.remove_files = { exe("main") },
        .command = {
          .args = { "build" },
          .expect.exists = { exe("main") },
        },
      },
    },
  });
}
