#include "cli/cli.h"

#include "spn/host.h"

#include "tui/tui.h"

static spn_err_t run_tests() {
  spn_session_t* session = spn.session;
  sp_tm_timer_t timer = sp_tm_start_timer();

  u32 passed = 0;
  u32 failed = 0;

  sp_da_for(session->plans, it) {
    spn_build_plan_t* plan = &session->plans[it];
    sp_da_for(plan->roots, jt) {
      spn_target_unit_t* root = spn_session_get_target_unit(session, plan->roots[jt]);
      if (root->info->kind != SPN_TARGET_TEST) {
        continue;
      }

      spn_event_buffer_push(spn.events, (spn_build_event_t) {
        .kind = SPN_EVENT_TARGET_RUN,
        .pkg = root->pkg->info,
        .run = {
          .name = root->info->name,
          .command = spn_target_staged_path(session->mem, root),
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
            .name = root->info->name,
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
            .name = root->info->name,
            .time = run.time,
          }
        });
      }
      spn_tui_flush(&tui);
    }
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
