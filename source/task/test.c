#include "app.h"
#include "event.h"
#include "task.h"
#include "session.h"

spn_task_result_t spn_task_run_tests(spn_app_t* app) {
  spn_session_t* b = &app->session;
  spn_pkg_unit_t* root = &b->units.root;

  sp_ht(sp_str_t, s32) tests = SP_NULLPTR;

  sp_tm_timer_t timer = sp_tm_start_timer();

  sp_om_for(root->targets, it) {
    spn_target_unit_t* unit = sp_om_at(root->targets, it);
    spn_target_t* target = unit->info;

    if (!spn_target_filter_pass(&b->filter, target)) {
      continue;
    }

    sp_fs_create_file(unit->paths.logs.test);
    unit->logs.test = sp_io_writer_from_file(unit->paths.logs.test, SP_IO_WRITE_MODE_OVERWRITE);

    spn_event_buffer_push_ex(spn.events, unit->pkg, &unit->logs, (spn_build_event_t) {
      .kind = SPN_EVENT_TEST_RUN
    });
    spn_poll(spn.sp);

    sp_ps_t ps = sp_ps_create((sp_ps_config_t) {
      .command = sp_fs_join_path(unit->paths.bin, target->name),
      .io = {
        .in =  { .mode = SP_PS_IO_MODE_NULL },
        .out = { .mode = SP_PS_IO_MODE_EXISTING, .fd = unit->logs.test.file.fd },
        .err = { .mode = SP_PS_IO_MODE_REDIRECT }
      },
      .cwd = unit->paths.work,
    });
    sp_ps_output_t result = sp_ps_output(&ps);
    sp_ht_insert(tests, target->name, result.status.exit_code);

    // sp_tui_up(1);
    // sp_tui_home();
    // sp_tui_clear_line();
    spn_event_buffer_push_ex(spn.events, unit->pkg, &unit->logs, result.status.exit_code ?
      (spn_build_event_t) { .kind = SPN_EVENT_TEST_FAILED } :
      (spn_build_event_t) { .kind = SPN_EVENT_TEST_PASSED }
    );
    spn_poll(spn.sp);
  }
  u64 elapsed = sp_tm_read_timer(&timer);

  bool ok = true;
  sp_ht_for_kv(tests, it) {
    spn_target_unit_t* unit = sp_om_get(root->targets, *it.key);

    if (*it.val) {
      ok = false;
      sp_io_writer_close(&unit->logs.test);
      sp_io_write_str(&spn.logger.err, sp_io_read_file(unit->paths.logs.test));
    }
  }

  spn_event_buffer_push_ctx(spn.events, &spn_session_find_root(b)->ctx, (spn_build_event_t) {
    .kind =  ok ?
      SPN_EVENT_TESTS_PASSED :
      SPN_EVENT_TEST_FAILED,
    .test.passed = {
      .time = elapsed,
      .n = sp_ht_size(tests),
      .profile = app->session.profile
    }
  });

  return SPN_TASK_DONE;
}

