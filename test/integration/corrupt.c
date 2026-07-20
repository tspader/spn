#include "common.h"

SPN_TEST_SUITE(corrupt)

UTEST_F(corrupt, store_entry_deleted) {
  tmpfs_init_named(&uf->fixture.fs, "corrupt_store_entry_deleted");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/deps/index/binary_static",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_REMOVE_DIR, .rm = { .dir = ".home/storage/cache/store/core/spum" } },
      { .kind = ACTION_REMOVE_DIR, .rm = { .dir = "build" } },
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_VERIFY_EXISTS, .exists = sp_str_lit(".home/storage/cache/store/core/spum") },
      { .kind = ACTION_VERIFY_EXISTS, .exists = store_file("bin/main") },
    },
  });
}

UTEST_F(corrupt, profile_store_poisoned) {
  tmpfs_init_named(&uf->fixture.fs, "corrupt_profile_store_poisoned");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/deps/index/binary_static",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_CREATE_FILE, .create = { .file = static_lib("spum"), .content = sp_str_lit("poison") } },
      { .kind = ACTION_CREATE_FILE, .create = { .file = store_file("bin/main"), .content = sp_str_lit("poison") } },
      { .kind = ACTION_REMOVE_DIR, .rm = { .dir = "build" } },
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_VERIFY_EXISTS, .exists = store_file("bin/main") },
    },
  });
}

UTEST_F(corrupt, checkout_deleted) {
  tmpfs_init_named(&uf->fixture.fs, "corrupt_checkout_deleted");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/deps/index/binary_static",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_REMOVE_DIR, .rm = { .dir = ".home/storage/cache/source" } },
      { .kind = ACTION_REMOVE_DIR, .rm = { .dir = "build" } },
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_VERIFY_EXISTS, .exists = store_file("bin/main") },
    },
  });
}

UTEST_F(corrupt, build_cache_deleted) {
  tmpfs_init_named(&uf->fixture.fs, "corrupt_build_cache_deleted");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/deps/index/binary_static",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_REMOVE_DIR, .rm = { .dir = ".home/storage/cache/build" } },
      { .kind = ACTION_REMOVE_DIR, .rm = { .dir = "build" } },
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_VERIFY_EXISTS, .exists = store_file("bin/main") },
    },
  });
}
