#include "cli/cli.h"

#include "spn/host.h"

#include "error/error.h"
#include "event/event.h"
#include "tui/tui.h"

static spn_err_t run_tests() {
  spn_session_t* session = spn.session;
  sp_tm_timer_t timer = sp_tm_start_timer();

  u32 passed = 0;
  u32 failed = 0;

  u32 num_targets = spn_session_num_targets(session);
  sp_for(it, num_targets) {
    spn_target_t* root = spn_session_target_at(session, it);
    if (spn_target_kind(root) != SPN_TARGET_TEST) {
      continue;
    }

    spn_event_buffer_push(spn.events, (spn_build_event_t) {
      .kind = SPN_EVENT_TARGET_RUN,
      .pkg = spn_target_pkg(root),
      .run = {
        .name = spn_target_name(root),
        .command = spn_target_staged_path(spn.heap, root),
      }
    });
    spn_tui_flush(&tui);

    spn_test_run_t run = sp_zero;
    spn_try(spn_op_run_test(session, root, &run));

    if (run.code) {
      failed++;
      spn_event_buffer_push(spn.events, (spn_build_event_t) {
        .kind = SPN_EVENT_TEST_FAILED,
        .test_failed = {
          .name = spn_target_name(root),
          .code = run.code,
          .out = run.out,
          .err = run.err,
          .time = run.time,
        }
      });
    }
    else {
      passed++;
      spn_event_buffer_push(spn.events, (spn_build_event_t) {
        .kind = SPN_EVENT_TEST_PASSED,
        .test_passed = {
          .name = spn_target_name(root),
          .time = run.time,
        }
      });
    }
    spn_tui_flush(&tui);
  }

  spn_event_buffer_push(spn.events, (spn_build_event_t) {
    .kind = SPN_EVENT_TEST_SUMMARY,
    .test_summary = {
      .passed = passed,
      .failed = failed,
      .time = sp_tm_read_timer(&timer),
    }
  });
  spn_tui_flush(&tui);

  if (failed) {
    return spn_err_emit(&spn, spn_err_reported(SPN_ERR_TEST_FAILED));
  }
  return SPN_OK;
}

sp_cli_result_t spn_cli_test(sp_cli_t* cli) {
  spn_target_names_t names = sp_da_new(spn.heap, sp_str_t);
  for (const c8** it = cli->rest; *it; it++) {
    sp_da_push(names, sp_cstr_as_str(*it));
  }

  spn_session_config_t config = {
    .selection = {
      .kind = SPN_TARGET_SELECTION_EXPLICIT,
      .test = {
        .kind = sp_da_empty(names) ? SPN_TARGET_RULE_ALL : SPN_TARGET_RULE_NAMED,
        .names = names,
      },
    },
  };
  sp_cli_result_t session = spn_cli_session(cli, config);
  if (session != SP_CLI_OK) {
    return session;
  }
  if (spn_op_build(spn.session)) {
    return SP_CLI_ERR;
  }
  spn_tui_handoff(&tui);
  return run_tests() ? SP_CLI_ERR : SP_CLI_OK;
}
