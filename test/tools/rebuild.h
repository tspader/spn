#ifndef SPN_TEST_REBUILD_H
#define SPN_TEST_REBUILD_H

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
  sp_str_t remove_files[SPN_TEST_REBUILD_MAX_CHANGES];
  rebuild_move_t moves[SPN_TEST_REBUILD_MAX_CHANGES];
  rebuild_write_t writes[SPN_TEST_REBUILD_MAX_CHANGES];
  sp_str_t remove_dirs[SPN_TEST_REBUILD_MAX_CHANGES];
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
  const c8* copy[16];
  command_test_t first;
  rebuild_step_t rebuilds[SPN_TEST_REBUILD_MAX_STEPS];
  rebuild_watch_t watches[SPN_TEST_REBUILD_MAX_WATCHES];
} rebuild_test_t;

#endif
