#include "cli/cli.h"

#include "app/app.h"
#include "ctx/ctx.h"
#include "enum/enum.h"
#include "intern/intern.h"
#include "log/log.h"

static bool spn_cli_run_path_is_absolute(sp_str_t path) {
  return sp_str_starts_with(path, sp_str_lit("/")) ||
    sp_str_starts_with(path, sp_str_lit("\\")) ||
    (path.len > 1 && path.data[1] == ':');
}

static bool spn_cli_run_path_is_explicit(sp_str_t path) {
  return spn_cli_run_path_is_absolute(path) ||
    sp_str_starts_with(path, sp_str_lit(".")) ||
    sp_str_find_c8(path, '/') != SP_STR_NO_MATCH ||
    sp_str_find_c8(path, '\\') != SP_STR_NO_MATCH;
}

static bool spn_cli_run_is_source_entry(sp_str_t entry, bool has_manifest) {
  if (!sp_str_equal(sp_fs_get_ext(entry), sp_str_lit("c"))) {
    return false;
  }

  if (!has_manifest) {
    return true;
  }

  if (sp_str_om_has(app.package.scripts, spn_intern(entry))) {
    return false;
  }

  if (spn_cli_run_path_is_explicit(entry)) {
    return true;
  }

  return sp_fs_exists(sp_fs_join_path(spn.mem, spn.paths.project, entry));
}

sp_app_result_t spn_cli_run(spn_cli_t* cli) {
  spn_cli_run_t* command = &cli->run;
  bool has_manifest = sp_fs_exists(spn.paths.manifest);
  bool source = spn_cli_run_is_source_entry(command->entry, has_manifest);

  app.config.filter = (spn_target_filter_t) {
    .name = source ? sp_str_lit("") : command->entry,
    .disabled = {
      .public = source,
      .test = source,
      .script = source,
    }
  };
  app.config.run = (spn_run_config_t) {
    .kind = source ? SPN_RUN_KIND_SOURCE : SPN_RUN_KIND_SCRIPT,
    .target = command->entry,
  };

  app.config.overrides = (spn_profile_info_t) {
    .name = command->profile,
    .toolchain = command->toolchain,
    .mode = sp_str_empty(command->mode) ? 0 : spn_build_mode_from_str(command->mode),
  };

  if (source) {
    if (has_manifest) {
      spn_task_enqueue(&app.tasks, SPN_TASK_RESOLVE);
      spn_task_enqueue(&app.tasks, SPN_TASK_SYNC_PACKAGES);
      spn_task_enqueue(&app.tasks, SPN_TASK_RUN_CONFIGURE_GRAPH);
    }

    spn_task_enqueue(&app.tasks, SPN_TASK_KIND_RUN);
    return SP_APP_CONTINUE;
  }

  if (!has_manifest) {
    spn_log_error("no manifest found in {.cyan}; pass a relative {.yellow} file instead",
      SP_FMT_STR(spn.paths.project),
      SP_FMT_CSTR(".c")
    );
    return SP_APP_ERR;
  }

  if (!sp_str_om_has(app.package.scripts, spn_intern(command->entry))) {
    spn_log_error("script target {.yellow} is not defined",
      SP_FMT_STR(command->entry)
    );
    return SP_APP_ERR;
  }

  spn_task_enqueue(&app.tasks, SPN_TASK_RESOLVE);
  spn_task_enqueue(&app.tasks, SPN_TASK_SYNC_PACKAGES);
  spn_task_enqueue(&app.tasks, SPN_TASK_RUN_CONFIGURE_GRAPH);
  spn_task_enqueue(&app.tasks, SPN_TASK_CREATE_UNITS);
  spn_task_enqueue(&app.tasks, SPN_TASK_KIND_RUN_BUILD_GRAPH);
  spn_task_enqueue(&app.tasks, SPN_TASK_KIND_RUN);

  return SP_APP_CONTINUE;
}
