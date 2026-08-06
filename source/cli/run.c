#include "cli/cli.h"

#include "spn/host.h"

#include "tui/tui.h"

static bool is_source_entry(sp_str_t entry, spn_project_t* project) {
  if (!sp_str_equal(sp_fs_get_ext(entry), sp_str_lit("c"))) {
    return false;
  }
  if (!project) {
    return true;
  }
  return !spn_project_has_script(project, spn.intern, entry);
}

spn_err_t spn_cli_run_roots() {
  spn_session_t* session = spn.session;

  spn_target_unit_t* root = spn_session_script_root(session);
  if (!root) {
    return SPN_OK;
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

  return spn_op_run_target(session, root);
}

sp_cli_result_t spn_cli_run(sp_cli_t* cli) {
  spn_cli_run_t* cmd = &args.run;

  if (is_source_entry(cmd->entry, spn.project)) {
    return spn_cli_error(cli, "{.yellow} cannot run native sources; build scripts are wasm", sp_fmt_str(cmd->entry));
  }

  if (!spn.project) {
    return spn_cli_error(cli, "no manifest found in {.cyan}; pass a relative {.yellow} file instead",
      sp_fmt_str(spn.paths.project),
      sp_fmt_cstr(".c")
    );
  }

  if (!spn_project_has_script(spn.project, spn.intern, cmd->entry)) {
    return spn_cli_error(cli, "script target {.yellow} is not defined",
      sp_fmt_str(cmd->entry)
    );
  }

  spn_target_names_t names = sp_da_new(spn.heap, sp_str_t);
  sp_da_push(names, cmd->entry);
  spn_session_config_t config = {
    .selection = {
      .kind = SPN_TARGET_SELECTION_EXPLICIT,
      .script = {
        .kind = SPN_TARGET_RULE_NAMED,
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
  spn_cli_exec(cli)->finish = spn_cli_run_roots;
  return SP_CLI_OK;
}
