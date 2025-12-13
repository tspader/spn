#define SP_IMPLEMENTATION
#include "sp.h"

#define SP_TEST_IMPLEMENTATION
#include "test.h"
#include "utest.h"

UTEST_MAIN()

#define uf utest_fixture

#define SPN_TEST_MAX_ACTIONS 32
#define SPN_TEST_MAX_ARGS 16

typedef enum {
  SPN_TEST_ACTION_CREATE_FILE,
  SPN_TEST_ACTION_SUBPROCESS,
  SPN_TEST_ACTION_VERIFY_EXISTS,
  SPN_TEST_ACTION_VERIFY_CONTENT,
  // ...
} action_kind_t;

typedef struct {
  action_kind_t kind;

  union {
    struct { sp_str_t file; sp_str_t content; } create;
    struct { sp_ps_config_t config; } process;
    struct { sp_str_t file; } verify_exists;
    struct { sp_str_t file; sp_str_t content; } verify_content;
    // ...
  };
} action_t;

typedef struct {
  action_t actions [SPN_TEST_MAX_ACTIONS];
  sp_str_t args [SPN_TEST_MAX_ARGS];
} test_t;

typedef struct {
  sp_str_t root;
  sp_str_t   project;
  sp_str_t     src;
  sp_str_t     include;
  struct {
    sp_str_t include;
    sp_str_t lib;
    sp_str_t bin;
  } store;
  // ...
} paths_t;

typedef struct {
  test_t test;
  paths_t paths;
} runner_t;

void run_test(test_t test) {

}

struct spn_init {
  sp_test_file_manager_t file_manager;
  runner_t runner;
};

UTEST_F_SETUP(spn_init) {
  sp_test_file_manager_init(&uf->file_manager);
}

UTEST_F_TEARDOWN(spn_init) {
  sp_test_file_manager_cleanup(&uf->file_manager);
}

UTEST_F(spn_init, creates_manifest) {
  run_test((test_t) {
    .actions = {
      { SPN_TEST_ACTION_CREATE_FILE, .create.file = sp_str_lit("spn.toml") },
    }
  });
}

UTEST_F(spn_init, init) {

}
