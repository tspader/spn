#include "host/host.h"

#include "sp/prompt.h"

#include "tui/tui.h"

static struct {
  bool bare;
  sp_str_t path;
} args;

static sp_str_t get_dir(sp_mem_t mem, sp_str_t project) {
  sp_mem_arena_marker_t s = sp_mem_begin_scratch_for(mem);

  sp_str_t dir = project;
  if (!sp_str_empty(args.path)) {
    dir = sp_fs_is_absolute(args.path) ? args.path : sp_fs_join_path(s.mem, project, args.path);
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
  spn_str_arr_t files = *(spn_str_arr_t*)sp_prompt_get_user_data(ctx);
  sp_prompt_style_t green = {
    .tag = SP_PROMPT_STYLE_ANSI,
    .ansi = SP_ANSI_FG_GREEN_U8,
  };

  sp_for(it, files.count) {
    sp_prompt_render_line(ctx, sp_str_lit("│  "), sp_zero_s(sp_prompt_style_t));
    sp_prompt_render_line(ctx, sp_str_lit("+ "), green);
    sp_prompt_line(ctx, files.items[it]);
  }
}

static spn_err_t run_prompt(sp_mem_t mem, sp_str_t dir, sp_str_t name) {
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

  spn_op_t* op = spn_scaffold_project(host.ctx, (spn_scaffold_request_t) {
    .dir = dir,
    .name = name,
    .bare = args.bare,
  });
  err = spn_cli_wait(op);

  if (err) {
    sp_prompt_cancel(prompt, "failed");
  }
  else {
    spn_str_arr_t files = spn_op_result(op).scaffold.files;
    sp_prompt_run(prompt, (sp_prompt_widget_t) {
      .user_data = &files,
      .on_event = on_submit_prompt,
      .render = on_render_prompt,
    });
    sp_str_t hint = sp_fmt(s.mem, "To build your program:\n\n  spn build {}", sp_fmt_str(name)).value;
    sp_prompt_note(prompt, sp_str_to_cstr(s.mem, hint), "Done");
  }
  spn_op_free(op);

cleanup:
  sp_prompt_end(prompt);
  sp_mem_end_scratch(s);
  return err;
}

static spn_err_t run_unattended(sp_str_t dir, sp_str_t name) {
  spn_op_t* op = spn_scaffold_project(host.ctx, (spn_scaffold_request_t) {
    .dir = dir,
    .name = name,
    .bare = args.bare,
  });
  spn_err_t err = spn_cli_wait(op);
  if (err) {
    spn_op_free(op);
    return err;
  }

  spn_str_arr_t files = spn_op_result(op).scaffold.files;
  sp_for(it, files.count) {
    spn_print(&tui, "- {}", sp_fmt_str(files.items[it]));
  }
  spn_op_free(op);

  spn_print(&tui, "");
  spn_print(&tui, "To build your program:\n\n  spn build {}", sp_fmt_str(name));

  return SPN_OK;
}

static spn_err_t scaffold(sp_mem_t mem, sp_str_t project) {
  sp_str_t dir = get_dir(mem, project);
  spn_try(spn_scaffold_check(host.ctx, (spn_scaffold_request_t) {
    .dir = dir,
    .bare = args.bare,
  }));

  sp_str_t name = sp_fs_get_name(dir);
  if (tui.mode == SPN_OUTPUT_MODE_INTERACTIVE && sp_sys_is_tty(sp_sys_stdout) && sp_str_empty(args.path)) {
    return run_prompt(mem, dir, name);
  }

  return run_unattended(dir, name);
}

static sp_cli_result_t init(sp_cli_t* cli) {
  try(spn_cli_open(true));

  spn_tui_handoff(&tui);
  return scaffold(host.mem, spn_ctx_project_dir(host.ctx)) ? SP_CLI_ERR : SP_CLI_OK;
}

sp_cli_cmd_t spn_cmd_init = {
  .name = "init",
  .summary = "Scaffold a new project",
  .opts = {
    {
      .name = "bare",
      .summary = "Only write a manifest",
      .kind = SP_CLI_OPT_BOOLEAN,
      .ptr = &args.bare,
    },
  },
  .args = {
    {
      .name = "path",
      .arity = SP_CLI_ARG_OPTIONAL,
      .kind = SP_CLI_OPT_STR,
      .summary = "Directory to scaffold into",
      .ptr = &args.path,
    },
  },
  .handler = init,
};
