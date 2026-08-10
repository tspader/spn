#ifndef SPN_TEST_HARNESS_H
#define SPN_TEST_HARNESS_H

#include "sp.h"
#include "sp/sp_test.h"
#include "fixture.h"
#include "caps.h"
#include "event/types.h"

#define SPN_TEST_MAX_ACTIONS 32

// @spader Anything that verifies a file's contents is banned; it's an old, hacky way of writing
// integration tests that was fine at some point in the past when everything was simpler but
// which now is unacceptable.
//
// Ditto for "actually run the binary and check the RC". There are a very few tests that will be
// implemented by running the resulting binary, but the pattern we use where it's
//
// #if SOMETHING_CORRECT
//   return 69;
// #else
//   return 420;
// #endif
//
// And then verifying that we get 69 -- this pattern is wrong. It makes brittle tests that are incompatible
// with cross compiling.
typedef enum {
  ACTION_NONE,
  ACTION_CREATE_FILE,
  ACTION_REMOVE_DIR,
  ACTION_RUN_BIN, // @spader See comment above; do not use this unless you're very confident you need to
  ACTION_RUN_TEST,
  ACTION_VERIFY_EXISTS,
  ACTION_VERIFY_NOT_EXISTS,
  ACTION_VERIFY_INCLUDE,
  ACTION_VERIFY_FILE_CONTAINS,
  ACTION_VERIFY_FILE_NOT_CONTAINS,
  ACTION_VERIFY_CC_ARG,
  ACTION_VERIFY_NO_CC_ARG,
  ACTION_VERIFY_LOCKED,
  ACTION_VERIFY_PKG_LOCKED,
  ACTION_VERIFY_EVENT,
  ACTION_VERIFY_NO_EVENT,
  ACTION_VERIFY_RESULT,
  ACTION_VERIFY_DIR_COUNT,
  ACTION_VERIFY_EVENT_COUNT,
  ACTION_RUN_CLI,
} action_kind_t;

typedef struct {
  action_kind_t kind;

  union {
    struct { sp_str_t file; sp_str_t content; } create;
    struct { const c8* dir; } rm;
    struct { const c8* name; s32 rc; } bin;
    sp_str_t exists;
    struct { sp_str_t file; } verify_include;
    struct { sp_str_t file; sp_str_t needle; } verify_file_contains;
    struct { sp_str_t file; sp_str_t needle; } verify_file_not_contains;
    const c8* verify_cc_arg [4];
    struct { const c8* name; } verify_locked;
    struct { spn_build_event_kind_t event; const c8* key; const c8* value; } verify_event;
    struct { spn_err_t err; } verify_result;
    struct { const c8* dir; u32 count; } verify_dir_count;
    struct { spn_build_event_kind_t event; const c8* key; const c8* value; u32 count; } verify_event_count;
    struct { const c8* cmd; const c8* args [8]; const c8* env [4]; s32 rc; } cli;
  };
} action_t;

typedef struct {
  const c8* project;
  const c8* copy [16];
  test_when_t when;
  action_t actions [SPN_TEST_MAX_ACTIONS];
} test_t;

#define SPN_TEST_COMMAND_MAX_ARGS 9
#define SPN_TEST_COMMAND_MAX_ENV 4
#define SPN_TEST_COMMAND_MAX_CONTAINS 4
#define SPN_TEST_COMMAND_MAX_EVENTS 4
#define SPN_TEST_COMMAND_MAX_FILES 4
#define SPN_TEST_COMMAND_MAX_FILE_CONTAINS 4
#define SPN_TEST_COMMAND_MAX_PATHS 8
#define SPN_TEST_COMMAND_MAX_PACKAGES 4
#define SPN_TEST_COMMAND_MAX_CC 4

typedef struct {
  spn_build_event_kind_t event;
  const c8* key;
  const c8* value;
  bool absent;
} command_event_t;

typedef struct {
  sp_str_t file;
  const c8* content;
  const c8* contains [SPN_TEST_COMMAND_MAX_FILE_CONTAINS];
  const c8* excludes [SPN_TEST_COMMAND_MAX_FILE_CONTAINS];
  bool json;
} command_file_t;

typedef struct {
  const c8* name;
  const c8* profile;
  sp_str_t path;
  s32 rc;
  bool build_only;
  const c8* contains [SPN_TEST_COMMAND_MAX_CONTAINS];
} command_bin_t;

typedef struct {
  const c8* args [SPN_TEST_COMMAND_MAX_FILE_CONTAINS];
  bool absent;
} command_cc_t;

typedef struct {
  s32 rc;
  command_bin_t bin;
  const c8* contains [SPN_TEST_COMMAND_MAX_CONTAINS];
  const c8* excludes [SPN_TEST_COMMAND_MAX_CONTAINS];
  command_event_t events [SPN_TEST_COMMAND_MAX_EVENTS];
  command_file_t files [SPN_TEST_COMMAND_MAX_FILES];
  command_cc_t cc [SPN_TEST_COMMAND_MAX_CC];
  sp_str_t exists [SPN_TEST_COMMAND_MAX_PATHS];
  sp_str_t missing [SPN_TEST_COMMAND_MAX_PATHS];
  bool lock;
  const c8* packages [SPN_TEST_COMMAND_MAX_PACKAGES];
} command_expect_t;

typedef struct {
  const c8* project;
  const c8* copy [16];
  const c8* args [SPN_TEST_COMMAND_MAX_ARGS];
  const c8* env [SPN_TEST_COMMAND_MAX_ENV];
  command_expect_t expect;
} command_test_t;

#define SPN_TEST_REBUILD_MAX_CHANGES 4
#define SPN_TEST_REBUILD_MAX_STEPS 3
#define SPN_TEST_REBUILD_MAX_WATCHES 4

typedef enum {
  REBUILD_MTIME_NONE,
  REBUILD_MTIME_UNCHANGED,
  REBUILD_MTIME_CHANGED,
} rebuild_mtime_t;

typedef struct {
  sp_str_t file;
  sp_str_t content;
} rebuild_write_t;

typedef struct {
  sp_str_t from;
  sp_str_t to;
} rebuild_move_t;

typedef struct {
  sp_str_t remove_files [SPN_TEST_REBUILD_MAX_CHANGES];
  rebuild_move_t moves [SPN_TEST_REBUILD_MAX_CHANGES];
  rebuild_write_t writes [SPN_TEST_REBUILD_MAX_CHANGES];
  sp_str_t remove_dirs [SPN_TEST_REBUILD_MAX_CHANGES];
} rebuild_change_t;

typedef struct {
  rebuild_change_t change;
  command_test_t command;
} rebuild_step_t;

typedef struct {
  sp_str_t file;
  rebuild_mtime_t mtime;
} rebuild_watch_t;

typedef struct {
  const c8* project;
  const c8* copy [16];
  command_test_t first;
  rebuild_step_t rebuilds [SPN_TEST_REBUILD_MAX_STEPS];
  rebuild_watch_t watches [SPN_TEST_REBUILD_MAX_WATCHES];
} rebuild_test_t;

typedef struct {
  const c8* profile;
  const c8* manifest;
  const c8* target;
  bool alternate;
  bool present;
  test_when_t when;
  command_expect_t expect;
} opt_build_t;

typedef struct {
  const c8* project;
  const c8* copy [4];
  test_when_t when;
  opt_build_t builds [3];
} opt_test_t;

typedef struct {
  sp_mem_t mem;
  sp_str_t root;
  struct {
    sp_str_t root;
    sp_str_t spn;
    sp_str_t storage;
    sp_str_t toolchain;
    sp_str_t config;
    sp_str_t index;
    sp_str_t include;
    sp_str_t patches;
  } paths;
} fixture_t;

fixture_t fixture_new(sp_test_t* t);
sp_err_t  fixture_init(sp_test_t* t, fixture_t* fixture);
sp_str_t  fixture_path(fixture_t* fixture, sp_str_t relative);
void      fixture_create(fixture_t* fixture, sp_str_t relative, sp_str_t content);

sp_str_t shared_lib(const c8* name);
sp_str_t static_lib(const c8* name);
sp_str_t profile_static_lib(const c8* profile, const c8* name);
sp_str_t staged_lib(const c8* name);
sp_str_t test_lib(const c8* name);
sp_str_t exe(const c8* name);
sp_str_t test_exe(const c8* name);
sp_str_t example_exe(const c8* name);
sp_str_t target_exe(const c8* name, const c8* triple);
sp_str_t store_file(const c8* rest);
sp_str_t work_file(const c8* rest);
sp_str_t profile_store_file(const c8* profile, const c8* rest);
sp_str_t target_store_file(const c8* rest, const c8* triple);

sp_err_t expect_exists(sp_test_t* t, fixture_t* fixture, sp_str_t path, bool expected, const c8* file, u32 line);

sp_err_t prepare_test(sp_test_t* t, fixture_t* fixture, const c8* project, const c8* const* copy);
sp_err_t run_command(sp_test_t* t, fixture_t* fixture, command_test_t test);
sp_err_t run_command_test(sp_test_t* t, command_test_t test);
sp_err_t run_rebuild_test(sp_test_t* t, rebuild_test_t test);
sp_err_t run_actions(sp_test_t* t, fixture_t* fixture, const action_t* actions);
sp_err_t run_test(sp_test_t* t, test_t test);
sp_err_t run_opt_test(sp_test_t* t, opt_test_t test);

#endif
