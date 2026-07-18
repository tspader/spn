#ifndef SPN_TEST_COMMAND_H
#define SPN_TEST_COMMAND_H

#define SPN_TEST_COMMAND_MAX_ARGS 9
#define SPN_TEST_COMMAND_MAX_ENV 4
#define SPN_TEST_COMMAND_MAX_CONTAINS 4
#define SPN_TEST_COMMAND_MAX_EVENTS 4
#define SPN_TEST_COMMAND_MAX_FILES 4
#define SPN_TEST_COMMAND_MAX_FILE_CONTAINS 4
#define SPN_TEST_COMMAND_MAX_PATHS 8
#define SPN_TEST_COMMAND_MAX_PACKAGES 4

typedef struct {
  const c8* event;
  const c8* key;
  const c8* value;
  bool absent;
} command_event_t;

typedef struct {
  sp_str_t file;
  const c8* content;
  const c8* contains[SPN_TEST_COMMAND_MAX_FILE_CONTAINS];
  const c8* excludes[SPN_TEST_COMMAND_MAX_FILE_CONTAINS];
  bool json;
} command_file_t;

typedef struct {
  const c8* name;
  const c8* profile;
  sp_str_t path;
  s32 rc;
  bool build_only;
  const c8* contains[SPN_TEST_COMMAND_MAX_CONTAINS];
} command_bin_t;

typedef struct {
  s32 rc;
  command_bin_t bin;
  const c8* contains[SPN_TEST_COMMAND_MAX_CONTAINS];
  const c8* excludes[SPN_TEST_COMMAND_MAX_CONTAINS];
  command_event_t events[SPN_TEST_COMMAND_MAX_EVENTS];
  command_file_t files[SPN_TEST_COMMAND_MAX_FILES];
  sp_str_t exists[SPN_TEST_COMMAND_MAX_PATHS];
  sp_str_t missing[SPN_TEST_COMMAND_MAX_PATHS];
  bool lock;
  const c8* packages[SPN_TEST_COMMAND_MAX_PACKAGES];
} command_expect_t;

typedef struct {
  const c8* project;
  const c8* copy[16];
  const c8* args[SPN_TEST_COMMAND_MAX_ARGS];
  const c8* env[SPN_TEST_COMMAND_MAX_ENV];
  command_expect_t expect;
} command_test_t;

#endif
