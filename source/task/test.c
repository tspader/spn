#include "app/app.h"
#include "ctx/ctx.h"
#include "event/event.h"
#include "external/cc.h"
#include "filter/filter.h"
#include "external/tcc.h"
#include "log/log.h"
#include "task.h"
#include "session/session.h"

static spn_err_t spn_task_add_build_deps_to_tcc(spn_app_t* app, spn_tcc_t* tcc) {
  if (!app->resolver) {
    return SPN_OK;
  }

  spn_cc_t cc = SP_ZERO_INITIALIZE();
  spn_cc_set_profile(&cc, app->config.profile);

  spn_cc_target_t* target = spn_cc_add_target(&cc, SPN_TARGET_JIT, sp_str_lit("run"));
  sp_ht_for_kv(app->package.deps, it) {
    switch (it.val->visibility) {
      case SPN_VISIBILITY_BUILD: {
        spn_cc_target_add_dep(target, spn_session_find_pkg(&app->session, *it.key));
        break;
      }
      case SPN_VISIBILITY_PUBLIC:
      case SPN_VISIBILITY_TEST:
      case SPN_VISIBILITY_SCRIPT: {
        break;
      }
    }
  }

  spn_cc_target_to_tcc(&cc, target, tcc);
  return SPN_OK;
}

static bool spn_task_run_path_is_absolute(sp_str_t path) {
  return sp_str_starts_with(path, sp_str_lit("/")) ||
    sp_str_starts_with(path, sp_str_lit("\\")) ||
    (path.len > 1 && path.data[1] == ':');
}

static sp_str_t spn_task_run_resolve_source_path(sp_str_t path) {
  if (spn_task_run_path_is_absolute(path) && sp_fs_exists(path)) {
    return sp_fs_canonicalize_path(path);
  }

  sp_str_t project_path = sp_fs_join_path(spn.paths.project, path);
  if (sp_fs_exists(project_path)) {
    return sp_fs_canonicalize_path(project_path);
  }

  return sp_str_lit("");
}

static spn_task_result_t spn_task_run_script(spn_app_t* app) {
  spn_pkg_unit_t* root = &app->session.units.root;
  spn_target_unit_t* unit = sp_om_get(root->targets, app->config.run.target);
  if (!unit) {
    spn_log_error("script {:fg brightyellow} was not built", SP_FMT_STR(app->config.run.target));
    return SPN_TASK_ERROR;
  }

  sp_str_t command = sp_fs_join_path(unit->paths.bin, unit->info->name);
  if (!sp_fs_exists(command)) {
    spn_log_error("script binary {:fg brightyellow} does not exist", SP_FMT_STR(command));
    return SPN_TASK_ERROR;
  }

  sp_ps_output_t result = sp_ps_run((sp_ps_config_t) {
    .command = command,
    .cwd = unit->paths.source,
    .io = {
      .in = { .mode = SP_PS_IO_MODE_NULL },
      .out = { .mode = SP_PS_IO_MODE_INHERIT },
      .err = { .mode = SP_PS_IO_MODE_INHERIT },
    },
  });

  if (result.status.exit_code) {
    spn_log_error("script {:fg brightyellow} failed with exit code {}",
      SP_FMT_STR(unit->info->name),
      SP_FMT_S32(result.status.exit_code)
    );
    return SPN_TASK_ERROR;
  }

  return SPN_TASK_DONE;
}

static spn_task_result_t spn_task_run_source(spn_app_t* app) {
  sp_str_t path = spn_task_run_resolve_source_path(app->config.run.target);
  if (sp_str_empty(path) || !sp_fs_exists(path)) {
    spn_log_error("source file {:fg brightyellow} does not exist", SP_FMT_STR(app->config.run.target));
    return SPN_TASK_ERROR;
  }

  sp_mem_arena_t* arena = sp_mem_arena_new(256);
  spn_tcc_err_ctx_t error_context = {
    .arena = arena,
    .error = sp_str_lit(""),
  };

  spn_tcc_t* tcc = tcc_new();
  if (!tcc) {
    spn_log_error("failed to initialize tcc");
    return SPN_TASK_ERROR;
  }

  if (spn_tcc_prepare_script(tcc, &error_context) != SPN_OK) {
    goto fail;
  }
  if (spn_task_add_build_deps_to_tcc(app, tcc) != SPN_OK) {
    goto fail;
  }
  if (spn_tcc_add_file(tcc, path) != SPN_OK) {
    goto fail;
  }

  c8* argv[] = {
    (c8*)sp_str_to_cstr(path),
    SP_NULLPTR,
  };

  sp_str_t cwd = sp_fs_get_cwd();
  if (sp_chdir(sp_str_to_cstr(spn.paths.project))) {
    spn_log_error("failed to change directory to {:fg brightyellow}", SP_FMT_STR(spn.paths.project));
    tcc_delete(tcc);
    return SPN_TASK_ERROR;
  }

  s32 result = tcc_run(tcc, 1, argv);
  sp_chdir(sp_str_to_cstr(cwd));
  if (result) {
    spn_log_error("source run failed for {:fg brightyellow} with exit code {}",
      SP_FMT_STR(path),
      SP_FMT_S32(result)
    );
    tcc_delete(tcc);
    return SPN_TASK_ERROR;
  }

  tcc_delete(tcc);
  return SPN_TASK_DONE;

fail:
  if (!sp_str_empty(error_context.error)) {
    spn_log_error("{}", SP_FMT_STR(error_context.error));
  } else {
    spn_log_error("failed to compile {:fg brightyellow}", SP_FMT_STR(path));
  }
  tcc_delete(tcc);
  return SPN_TASK_ERROR;
}

spn_task_result_t spn_task_run(spn_app_t* app) {
  switch (app->config.run.kind) {
    case SPN_RUN_KIND_NONE: {
      return SPN_TASK_DONE;
    }
    case SPN_RUN_KIND_TESTS: {
      return spn_task_run_tests(app);
    }
    case SPN_RUN_KIND_SCRIPT: {
      return spn_task_run_script(app);
    }
    case SPN_RUN_KIND_SOURCE: {
      return spn_task_run_source(app);
    }
  }

  SP_UNREACHABLE_RETURN(SPN_TASK_ERROR);
}

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

    sp_str_t test_cmd = sp_fs_join_path(unit->paths.bin, target->name);

    spn_event_buffer_push_ex(spn.events, unit->pkg, &unit->logs, (spn_build_event_t) {
      .kind = SPN_EVENT_TEST_RUN
    });
    spn_poll(spn.sp);

    sp_ps_t ps = sp_ps_create((sp_ps_config_t) {
      .command = test_cmd,
      .io = {
        .in =  { .mode = SP_PS_IO_MODE_NULL },
        .out = { .mode = SP_PS_IO_MODE_EXISTING, .fd = unit->logs.test.file.fd },
        .err = { .mode = SP_PS_IO_MODE_REDIRECT }
      },
      .cwd = unit->paths.work,
    });
    sp_ps_output_t result = sp_ps_output(&ps);
    sp_ht_insert(tests, target->name, result.status.exit_code);

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
