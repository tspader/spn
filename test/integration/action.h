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
  ACTION_VERIFY_NOT_EXISTS,
  ACTION_VERIFY_INCLUDE,
  ACTION_VERIFY_CONTENT,
  ACTION_VERIFY_FILE_NONEMPTY,
  ACTION_VERIFY_FILE_CONTAINS,
  ACTION_VERIFY_FILE_NOT_CONTAINS,
  ACTION_SNAPSHOT_MTIME,
  ACTION_VERIFY_MTIME_UNCHANGED,
  ACTION_VERIFY_MTIME_CHANGED,
  ACTION_VERIFY_LOCKED,
  ACTION_VERIFY_PKG_LOCKED,
  ACTION_VERIFY_EVENT,
  ACTION_VERIFY_NO_EVENT,
  ACTION_RUN_CLI,
  ACTION_VERIFY_CLI_CONTAINS,
  ACTION_VERIFY_CLI_NOT_CONTAINS,
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
    struct { sp_str_t file; } verify_not_exists;
    struct { sp_str_t file; } verify_include;
    struct { sp_str_t file; sp_str_t content; } verify_content;
    struct { sp_str_t file; } verify_file_nonempty;
    struct { sp_str_t file; sp_str_t needle; } verify_file_contains;
    struct { sp_str_t file; sp_str_t needle; } verify_file_not_contains;
    struct { sp_str_t file; } snapshot_mtime;
    struct { sp_str_t file; } verify_mtime;
    struct { const c8* name; } verify_locked;
    struct { const c8* event; const c8* key; const c8* value; } verify_event;
    struct { const c8* cmd; const c8* args [4]; s32 rc; } cli;
    struct { sp_str_t needle; } verify_cli;
  };
} action_t;

typedef struct {
  const c8* project;
  const c8* copy [16];
  action_t actions[SPN_TEST_MAX_ACTIONS];
} test_t;

#endif
