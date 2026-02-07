#ifndef SPN_TEST_INTEGRATION_ACTION_H
#define SPN_TEST_INTEGRATION_ACTION_H

#define SPN_TEST_MAX_ACTIONS 32

typedef enum {
  SPN_TEST_ACTION_NONE,
  SPN_TEST_ACTION_CREATE_FILE,
  SPN_TEST_ACTION_SUBPROCESS,
  SPN_TEST_ACTION_VERIFY_EXISTS,
  SPN_TEST_ACTION_VERIFY_CONTENT,
} action_kind_t;

typedef struct {
  action_kind_t kind;

  union {
    struct {
      sp_str_t file;
      sp_str_t content;
    } create;

    struct {
      sp_ps_config_t config;
      s32 expect_exit_code;
    } process;

    struct {
      sp_str_t file;
    } verify_exists;

    struct {
      sp_str_t file;
      sp_str_t content;
    } verify_content;
  };
} action_t;

typedef struct {
  action_t actions[SPN_TEST_MAX_ACTIONS];
  u32 num_actions;
} test_t;

#endif
