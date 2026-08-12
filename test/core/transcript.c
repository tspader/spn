#include "spn_test.h"

#include "event/build.h"


typedef struct {
  const c8* out;
} expect_t;

typedef struct {
  const c8* name;
  spn_build_event_kind_t kind;
  const c8* transcript;
  expect_t expect;
} test_t;

static const test_t tests [] = {
  {
    .name = "compile_passed",
    .kind = SPN_EVENT_TARGET_BUILD_PASSED,
    .transcript = "warning",
    .expect = { "zig cc -c main.c\nwarning\n" },
  },
  {
    .name = "compile_failed",
    .kind = SPN_EVENT_TARGET_BUILD_FAILED,
    .transcript = "error",
    .expect = { "zig cc -c main.c\nerror\n" },
  },
  {
    .name = "link_passed",
    .kind = SPN_EVENT_LINK_PASSED,
    .transcript = "warning",
    .expect = { "zig cc -c main.c\nwarning\n" },
  },
  {
    .name = "link_failed",
    .kind = SPN_EVENT_LINK_FAILED,
    .transcript = "error",
    .expect = { "zig cc -c main.c\nerror\n" },
  },
  {
    .name = "empty",
    .kind = SPN_EVENT_TARGET_BUILD_PASSED,
    .transcript = "",
    .expect = { "" },
  },
  {
    .name = "unrelated",
    .kind = SPN_EVENT_BUILD_PASSED,
    .transcript = "ignored",
    .expect = { "" },
  },
};

sp_test_each(transcript, log_build, test_t, tests) {
  sp_mem_t mem = sp_test_arena(t);

  sp_str_t command = sp_str_lit("zig cc -c main.c");

  spn_build_event_t event = {
    .kind = it->kind,
  };
  switch (it->kind) {
    case SPN_EVENT_TARGET_BUILD_PASSED: {
      event.target_passed.command = command;
      event.target_passed.out = sp_str_view(it->transcript);
      break;
    }
    case SPN_EVENT_TARGET_BUILD_FAILED: {
      event.target_failed.command = command;
      event.target_failed.out = sp_str_view(it->transcript);
      break;
    }
    case SPN_EVENT_LINK_PASSED: {
      event.link_passed.command = command;
      event.link_passed.out = sp_str_view(it->transcript);
      break;
    }
    case SPN_EVENT_LINK_FAILED: {
      event.link_failed.command = command;
      event.link_failed.out = sp_str_view(it->transcript);
      break;
    }
    default: {
      break;
    }
  }

  sp_io_dyn_mem_writer_t writer = sp_zero;
  sp_io_dyn_mem_writer_init(mem, &writer);
  spn_event_log_build(&writer.base, &event);
  sp_expect_str_eq_c(t, sp_io_dyn_mem_writer_as_str(&writer), it->expect.out);

  return SP_OK;
}
