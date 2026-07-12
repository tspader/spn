#define SP_IMPLEMENTATION
#include "sp.h"

#define UTEST_IMPLEMENTATION
#include "utest.h"

#include "event/build.h"
#include "test.h"

UTEST_MAIN()

typedef struct {
  spn_build_event_kind_t kind;
  const c8* transcript;
  const c8* expect;
} transcript_test_t;

static void run_transcript_test(s32* utest_result, transcript_test_t test) {
  sp_mem_t mem = sp_mem_os_new();
  spn_invocation_t invocation = {
    .program = sp_str_lit("zig"),
  };
  sp_da_init(mem, invocation.args);
  sp_da_push(invocation.args, sp_str_lit("cc"));
  sp_da_push(invocation.args, sp_str_lit("-c"));
  sp_da_push(invocation.args, sp_str_lit("main.c"));

  spn_build_event_t event = {
    .kind = test.kind,
  };
  switch (test.kind) {
    case SPN_EVENT_TARGET_BUILD_PASSED: {
      event.target.passed.invocation = &invocation;
      event.target.passed.out = sp_str_view(test.transcript);
      break;
    }
    case SPN_EVENT_TARGET_BUILD_FAILED: {
      event.target.failed.invocation = &invocation;
      event.target.failed.out = sp_str_view(test.transcript);
      break;
    }
    case SPN_EVENT_LINK_PASSED: {
      event.target.link_passed.invocation = &invocation;
      event.target.link_passed.out = sp_str_view(test.transcript);
      break;
    }
    case SPN_EVENT_LINK_FAILED: {
      event.target.link_failed.invocation = &invocation;
      event.target.link_failed.out = sp_str_view(test.transcript);
      break;
    }
    default: {
      break;
    }
  }

  sp_io_dyn_mem_writer_t writer = SP_ZERO_INITIALIZE();
  sp_io_dyn_mem_writer_init(mem, &writer);
  spn_event_log_build(&writer.base, &event);
  EXPECT_TRUE(sp_str_equal_cstr(sp_io_dyn_mem_writer_as_str(&writer), test.expect));
}

UTEST(transcript, compile_passed) {
  run_transcript_test(utest_result, (transcript_test_t) {
    .kind = SPN_EVENT_TARGET_BUILD_PASSED,
    .transcript = "warning",
    .expect = "zig cc -c main.c\nwarning\n",
  });
}

UTEST(transcript, compile_failed) {
  run_transcript_test(utest_result, (transcript_test_t) {
    .kind = SPN_EVENT_TARGET_BUILD_FAILED,
    .transcript = "error",
    .expect = "zig cc -c main.c\nerror\n",
  });
}

UTEST(transcript, link_passed) {
  run_transcript_test(utest_result, (transcript_test_t) {
    .kind = SPN_EVENT_LINK_PASSED,
    .transcript = "warning",
    .expect = "zig cc -c main.c\nwarning\n",
  });
}

UTEST(transcript, link_failed) {
  run_transcript_test(utest_result, (transcript_test_t) {
    .kind = SPN_EVENT_LINK_FAILED,
    .transcript = "error",
    .expect = "zig cc -c main.c\nerror\n",
  });
}

UTEST(transcript, empty) {
  run_transcript_test(utest_result, (transcript_test_t) {
    .kind = SPN_EVENT_TARGET_BUILD_PASSED,
    .transcript = "",
    .expect = "",
  });
}

UTEST(transcript, unrelated) {
  run_transcript_test(utest_result, (transcript_test_t) {
    .kind = SPN_EVENT_BUILD_PASSED,
    .transcript = "ignored",
    .expect = "",
  });
}
