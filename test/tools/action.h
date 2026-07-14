#ifndef SPN_TEST_ACTION_H
#define SPN_TEST_ACTION_H

#define SPN_TEST_MAX_ACTIONS 32

typedef enum {
  ACTION_NONE,
  ACTION_CREATE_FILE,
  ACTION_REMOVE_DIR,
  ACTION_SUBPROCESS,
  ACTION_RUN_BIN,
  ACTION_RUN_TEST,
  ACTION_VERIFY_EXISTS,
  ACTION_VERIFY_NOT_EXISTS,
  ACTION_VERIFY_INCLUDE,
  ACTION_VERIFY_FILE_CONTAINS,
  ACTION_VERIFY_FILE_NOT_CONTAINS,
  ACTION_VERIFY_LOCKED,
  ACTION_VERIFY_PKG_LOCKED,
  ACTION_VERIFY_EVENT,
  ACTION_VERIFY_NO_EVENT,
  ACTION_VERIFY_DIR_COUNT,
  ACTION_VERIFY_EVENT_COUNT,
  ACTION_RUN_CLI,
  ACTION_VERIFY_CLI_CONTAINS,
  ACTION_VERIFY_CLI_NOT_CONTAINS,
} action_kind_t;

typedef struct {
  action_kind_t kind;

  union {
    struct { sp_str_t file; sp_str_t content; } create;
    struct { const c8* dir; } rm;
    struct { sp_ps_config_t config; s32 rc; } process;
    struct { const c8* name; s32 rc; } bin;
    struct { sp_str_t file; } verify_exists;
    struct { sp_str_t file; } verify_not_exists;
    struct { sp_str_t file; } verify_include;
    struct { sp_str_t file; sp_str_t content; } verify_content;
    struct { sp_str_t file; } verify_file_nonempty;
    struct { sp_str_t file; sp_str_t needle; } verify_file_contains;
    struct { sp_str_t file; } verify_json;
    struct { sp_str_t file; sp_str_t needle; } verify_file_not_contains;
    struct { const c8* name; } verify_locked;
    struct { const c8* event; const c8* key; const c8* value; } verify_event;
    struct { const c8* dir; u32 count; } verify_dir_count;
    struct { const c8* event; const c8* key; const c8* value; u32 count; } verify_event_count;
    struct { const c8* cmd; const c8* args [8]; const c8* env [4]; s32 rc; } cli;
    struct { sp_str_t needle; } verify_cli;
  };
} action_t;

typedef struct {
  const c8* project;
  const c8* copy [16];
  action_t actions[SPN_TEST_MAX_ACTIONS];
} test_t;

#endif
