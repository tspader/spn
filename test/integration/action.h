#ifndef SPN_TEST_INTEGRATION_ACTION_H
#define SPN_TEST_INTEGRATION_ACTION_H

#define SPN_TEST_MAX_ACTIONS 32

typedef enum {
  ACTION_NONE,
  ACTION_CREATE_FILE,
  ACTION_REMOVE_FILE,
  ACTION_MOVE_FILE,
  ACTION_REMOVE_DIR,
  ACTION_SUBPROCESS,
  ACTION_RUN_BIN,
  ACTION_VERIFY_EXISTS,
  ACTION_VERIFY_CONTENT,
  ACTION_VERIFY_LOCKED,
  ACTION_VERIFY_PKG_LOCKED,
  ACTION_RUN_CLI,
} action_kind_t;

typedef struct {
  action_kind_t kind;

  union {
    struct { sp_str_t file; sp_str_t content; } create;
    struct { const c8* file; const c8* dir; } rm;
    struct { sp_str_t from; sp_str_t to; } mv;
    struct { sp_ps_config_t config; s32 rc; } process;
    struct { const c8* name; s32 rc; } bin;
    struct { sp_str_t file; } verify_exists;
    struct { sp_str_t file; sp_str_t content; } verify_content;
    struct { const c8* name; } verify_locked;
    struct { const c8* cmd; const c8* args [4]; } cli;
  };
} action_t;

typedef struct {
  const c8* project;
  const c8* copy [16];
  action_t actions[SPN_TEST_MAX_ACTIONS];
} test_t;

#endif
