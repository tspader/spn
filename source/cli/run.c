#include "sp.h"
#include "sp/macro.h"
#include "cli/cli.h"

#include "ctx/types.h"
#include "intern/intern.h"
#include "task/task.h"

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

sp_cli_result_t spn_cli_run(sp_cli_t* cli) {
  spn_cli_run_t* command = &spn.cli.run;
  bool has_manifest = sp_fs_exists(spn.paths.manifest);
  bool source = spn_cli_run_is_source_entry(command->entry, has_manifest);

  app.config.compile.targets.kind = SPN_TARGET_SELECTION_EXPLICIT;
  if (!source) {
    spn_target_names_t names = sp_da_new(spn.heap, sp_str_t);
    sp_da_push(names, command->entry);
    app.config.compile.targets.targets.script = (spn_target_rule_t) {
      .kind = SPN_TARGET_RULE_NAMED,
      .names = names,
    };
  }
  if (source) {
    app.config.action = (spn_action_t) {
      .kind = SPN_ACTION_RUN_SOURCE,
      .source = { .path = command->entry },
    };
  }
  else {
    app.config.action = (spn_action_t) {
      .kind = SPN_ACTION_RUN_ROOTS,
    };
  }

  if (source) {
    if (has_manifest) {
      return spn_plan(
        SPN_TASK_SYNC_INDEXES,
        SPN_TASK_RESOLVE,
        SPN_TASK_SYNC_PACKAGES,
        SPN_TASK_PLAN,
    SPN_TASK_PLAN,
        SPN_TASK_CONFIGURE_GRAPH,
        SPN_TASK_RUN
      );
    }

    return spn_plan(SPN_TASK_RUN);
  }

  if (!has_manifest) {
    return spn_cli_errf(cli, "no manifest found in {.cyan}; pass a relative {.yellow} file instead",
      sp_fmt_str(spn.paths.project),
      sp_fmt_cstr(".c")
    );
  }

  if (!sp_str_om_has(app.package.scripts, spn_intern(command->entry))) {
    return spn_cli_errf(cli, "script target {.yellow} is not defined",
      sp_fmt_str(command->entry)
    );
  }

  return spn_plan(
    SPN_TASK_SYNC_INDEXES,
    SPN_TASK_RESOLVE,
    SPN_TASK_SYNC_PACKAGES,
    SPN_TASK_PLAN,
    SPN_TASK_CONFIGURE_GRAPH,
    SPN_TASK_CREATE_UNITS,
    SPN_TASK_BUILD_GRAPH,
    SPN_TASK_RUN
  );
}
