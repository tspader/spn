#include "sp.h"
#include "sp/macro.h"
#include "cli/cli.h"

#include "ctx/ctx.h"
#include "ctx/types.h"
#include "event/event.h"
#include "intern/intern.h"
#include "op/build/build.h"
#include "op/op.h"
#include "project/types.h"
#include "session/session.h"
#include "tui/tui.h"
#include "unit/types.h"

static bool is_source_entry(sp_str_t entry, spn_project_t* project) {
  if (!sp_str_equal(sp_fs_get_ext(entry), sp_str_lit("c"))) {
    return false;
  }
  if (!project) {
    return true;
  }
  return !sp_str_om_has(project->package.scripts, spn_intern(entry));
}

static spn_err_union_t run_script(spn_session_t* session, spn_target_unit_t* unit) {
  spn_pkg_unit_t* root = unit->pkg;

  sp_str_t command = spn_target_staged_path(session->mem, unit);
  if (!sp_fs_exists(command)) {
    return (spn_err_union_t) {
      .kind = SPN_ERR_SCRIPT_MISSING,
      .script = { .name = unit->info->name, .path = command },
    };
  }

  spn_event_buffer_push(spn.events, (spn_build_event_t) {
    .kind = SPN_EVENT_TARGET_RUN,
    .pkg = root->info,
    .run = {
      .name = unit->info->name,
      .command = command,
    }
  });
  spn_tui_flush(&tui);

  sp_ps_t ps = sp_ps_create(session->mem, (sp_ps_config_t) {
    .command = command,
    .cwd = root->paths.source,
    .io = {
      .in =  { .mode = SP_PS_IO_MODE_NULL },
      .out = { .mode = SP_PS_IO_MODE_INHERIT },
      .err = { .mode = SP_PS_IO_MODE_INHERIT },
    },
  });
  sp_ps_status_t status = sp_ps_wait(&ps);

  if (status.exit_code) {
    return (spn_err_union_t) {
      .kind = SPN_ERR_SCRIPT_FAILED,
      .script = { .name = unit->info->name, .code = status.exit_code },
    };
  }

  return spn_result(SPN_OK);
}

spn_err_union_t spn_cli_run_roots() {
  spn_session_t* session = spn.session;
  sp_da_for(session->plans, it) {
    spn_build_plan_t* plan = &session->plans[it];
    sp_da_for(plan->roots, jt) {
      spn_target_unit_t* root = spn_session_get_target_unit(session, plan->roots[jt]);
      if (root->info->kind == SPN_TARGET_SCRIPT) {
        return run_script(session, root);
      }
    }
  }
  return spn_result(SPN_OK);
}

sp_cli_result_t spn_cli_run(sp_cli_t* cli) {
  spn_cli_run_t* cmd = &args.run;
  spn_command_t* command = spn_cli_command(cli);

  if (is_source_entry(cmd->entry, spn.project)) {
    return spn_cli_error(cli, "{.yellow} cannot run native sources; build scripts are wasm", sp_fmt_str(cmd->entry));
  }

  if (!spn.project) {
    return spn_cli_error(cli, "no manifest found in {.cyan}; pass a relative {.yellow} file instead",
      sp_fmt_str(spn.paths.project),
      sp_fmt_cstr(".c")
    );
  }

  if (!sp_str_om_has(spn.project->package.scripts, spn_intern(cmd->entry))) {
    return spn_cli_error(cli, "script target {.yellow} is not defined",
      sp_fmt_str(cmd->entry)
    );
  }

  spn_target_names_t names = sp_da_new(spn.heap, sp_str_t);
  sp_da_push(names, cmd->entry);
  command->config.selection.kind = SPN_TARGET_SELECTION_EXPLICIT;
  command->config.selection.script = (spn_target_rule_t) {
    .kind = SPN_TARGET_RULE_NAMED,
    .names = names,
  };

  command->op = (spn_op_desc_t) { .kind = SPN_OP_BUILD };
  command->finish = spn_cli_run_roots;
  return SP_CLI_CONTINUE;
}
