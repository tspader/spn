#include "cli/cli.h"

#include "sp/sp_prompt.h"
#include "tui/tui.h"

static sp_str_t get_dir(sp_mem_t mem, spn_cli_init_t* command, sp_str_t project) {
  sp_mem_arena_marker_t s = sp_mem_begin_scratch_for(mem);

  sp_str_t dir = project;
  if (!sp_str_empty(command->path)) {
    dir = sp_fs_is_absolute(command->path) ? command->path : sp_fs_join_path(s.mem, project, command->path);
  }

  sp_str_t canonical = sp_fs_canonicalize_path(s.mem, dir);
  if (sp_str_empty(canonical)) {
    canonical = sp_fs_normalize_path(s.mem, dir);
  }

  sp_str_t result = sp_str_copy(mem, canonical);
  sp_mem_end_scratch(s);
  return result;
}

static void on_submit_prompt(sp_prompt_ctx_t* ctx, sp_prompt_event_t event) {
  sp_prompt_set_state(ctx, SP_PROMPT_STATE_SUBMIT);
}

static void on_render_prompt(sp_prompt_ctx_t* ctx) {
  sp_da(sp_str_t) files = *(sp_da(sp_str_t)*)sp_prompt_get_user_data(ctx);
  sp_prompt_style_t green = {
    .tag = SP_PROMPT_STYLE_ANSI,
    .ansi = SP_ANSI_FG_GREEN_U8,
  };

  sp_da_for(files, it) {
    sp_prompt_render_line(ctx, sp_str_lit("│  "), sp_zero_s(sp_prompt_style_t));
    sp_prompt_render_line(ctx, sp_str_lit("+ "), green);
    sp_prompt_line(ctx, files[it]);
  }
}

static spn_err_t run_prompt(sp_mem_t mem, spn_cli_init_t* command, sp_str_t dir, sp_str_t name) {
  sp_mem_arena_marker_t s = sp_mem_begin_scratch_for(mem);
  spn_err_t err = SPN_OK;

  sp_prompt_ctx_t* prompt = sp_prompt_begin(mem);
  sp_prompt_intro(prompt, "spn init");

  const c8* entered = sp_prompt_text(prompt, "name", sp_str_to_cstr(s.mem, name));
  if (sp_prompt_cancelled(prompt)) {
    sp_prompt_cancel(prompt, "cancelled");
    err = SPN_ERROR;
    goto cleanup;
  }

  sp_str_t response = sp_str_copy(s.mem, sp_cstr_as_str(entered));
  if (!sp_str_empty(response)) {
    name = response;
  }

  sp_da(sp_str_t) files = sp_zero;
  err = spn_op_scaffold(host.ctx, (spn_scaffold_request_t) {
    .dir = dir,
    .name = name,
    .bare = command->bare,
  }, mem, &files);

  if (err) {
    sp_prompt_cancel(prompt, "failed");
  }
  else {
    sp_prompt_run(prompt, (sp_prompt_widget_t) {
      .user_data = &files,
      .on_event = on_submit_prompt,
      .render = on_render_prompt,
    });
    sp_str_t hint = sp_fmt(s.mem, "To build your program:\n\n  spn build {}", sp_fmt_str(name)).value;
    sp_prompt_note(prompt, sp_str_to_cstr(s.mem, hint), "Done");
  }

cleanup:
  sp_prompt_end(prompt);
  sp_mem_end_scratch(s);
  return err;
}

static spn_err_t run_unattended(sp_mem_t mem, spn_cli_init_t* command, sp_str_t dir, sp_str_t name) {
  sp_da(sp_str_t) files = sp_zero;
  spn_try(spn_op_scaffold(host.ctx, (spn_scaffold_request_t) {
    .dir = dir,
    .name = name,
    .bare = command->bare,
  }, mem, &files));

  sp_da_for(files, it) {
    spn_print("- {}", sp_fmt_str(files[it]));
  }
  spn_print("");
  spn_print("To build your program:\n\n  spn build {}", sp_fmt_str(name));

  return SPN_OK;
}

static spn_err_t run(sp_mem_t mem, spn_cli_init_t* command, sp_str_t project) {
  sp_str_t dir = get_dir(mem, command, project);
  spn_try(spn_op_scaffold_check(host.ctx, (spn_scaffold_request_t) {
    .dir = dir,
    .bare = command->bare,
  }));

  sp_str_t name = sp_fs_get_name(dir);
  if (tui.mode == SPN_OUTPUT_MODE_INTERACTIVE && sp_sys_is_tty(sp_sys_stdout) && sp_str_empty(command->path)) {
    return run_prompt(mem, command, dir, name);
  }

  return run_unattended(mem, command, dir, name);
}

sp_cli_result_t spn_cli_init(sp_cli_t* cli) {
  sp_cli_result_t opened = spn_cli_open(true);
  if (opened != SP_CLI_OK) {
    return opened;
  }

  spn_tui_handoff(&tui);
  return run(host.mem, &args.init, spn_ctx_project_dir(host.ctx)) ? SP_CLI_ERR : SP_CLI_OK;
}
