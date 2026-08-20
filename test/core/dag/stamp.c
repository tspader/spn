#include "dag_test.h"
#include "dag/stamp.h"

typedef struct {
  bool fenced;
} expect_t;

typedef struct {
  const c8* name;
  s64 fence_sec;
  s64 fence_nsec;
  s64 mtime_sec;
  s64 mtime_nsec;
  expect_t expect;
} test_t;

static const test_t tests [] = {
  {
    .name = "same_stamp",
    .fence_sec = 10, .fence_nsec = 500,
    .mtime_sec = 10, .mtime_nsec = 500,
  },
  {
    .name = "later_nsec",
    .fence_sec = 10, .fence_nsec = 500,
    .mtime_sec = 10, .mtime_nsec = 501,
  },
  {
    .name = "later_sec",
    .fence_sec = 10, .fence_nsec = 500,
    .mtime_sec = 11,
  },
  {
    .name = "earlier_nsec",
    .fence_sec = 10, .fence_nsec = 500,
    .mtime_sec = 10, .mtime_nsec = 499,
    .expect = { .fenced = true }
  },
  {
    .name = "earlier_sec",
    .fence_sec = 10, .fence_nsec = 500,
    .mtime_sec = 9, .mtime_nsec = 999999999,
    .expect = { .fenced = true }
  },
  {
    .name = "zero_fence",
  },
};

sp_test_each(dag_stamp, fenced, test_t, tests) {
  sp_sys_timespec_t fence = { .tv_sec = it->fence_sec, .tv_nsec = it->fence_nsec };
  sp_sys_timespec_t mtime = { .tv_sec = it->mtime_sec, .tv_nsec = it->mtime_nsec };
  sp_expect_eq(t, it->expect.fenced, is_timestamp_fenced(fence, mtime));
  return SP_OK;
}
