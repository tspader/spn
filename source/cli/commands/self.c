#include "host/host.h"

#include "sp/sp_prompt.h"

#include "install/install.h"
#include "install/plan.h"
#include "tui/tui.h"
#include "version/version.h"

#define cfmt(mem, ...) sp_str_to_cstr(mem, sp_fmt(mem, __VA_ARGS__).value)

static struct {
  bool unattended;
} args;

static void print_msg(spn_install_msg_t* msg) {
  switch (msg->kind) {
    case SPN_INSTALL_MSG_NONE: {
      break;
    }
    case SPN_INSTALL_MSG_STUCK_WRITE: {
      spn_print_err(&tui, "install: could not write {}", sp_fmt_str(msg->subject));
      break;
    }
    case SPN_INSTALL_MSG_STUCK_APPEND: {
      spn_print_err(&tui, "install: could not add {} to {}", sp_fmt_str(msg->detail), sp_fmt_str(msg->subject));
      break;
    }
    case SPN_INSTALL_MSG_STUCK_REGISTRY: {
      spn_print_err(&tui, "install: could not update the user PATH in the registry");
      break;
    }
    case SPN_INSTALL_MSG_RESTART_SHELL: {
      if (sp_str_empty(msg->subject)) {
        spn_print(&tui, "install: restart your shell to use spn");
        break;
      }
      spn_print(&tui, "install: restart your shell, or run:");
      spn_print(&tui, "install:   {}", sp_fmt_str(msg->subject));
      break;
    }
    case SPN_INSTALL_MSG_RESTART_TERMINAL: {
      spn_print(&tui, "install: restart your terminal to use spn");
      break;
    }
    case SPN_INSTALL_MSG_MANUAL: {
      spn_print(&tui, "install: add {} to your PATH", sp_fmt_str(msg->subject));
      break;
    }
    case SPN_INSTALL_MSG_SHADOW: {
      spn_print_err(&tui, "install: another spn at {} shadows {}", sp_fmt_str(msg->subject), sp_fmt_str(msg->detail));
      break;
    }
  }
}

static sp_cli_result_t install_error(sp_cli_t* cli, spn_install_t* result) {
  switch (result->err) {
    case SPN_INSTALL_OK: {
      return SP_CLI_OK;
    }
    case SPN_INSTALL_ERR_NO_HOME: {
      return spn_cli_error(cli, "HOME is not set");
    }
    case SPN_INSTALL_ERR_EXE: {
      return spn_cli_error(cli, "failed to write {}; close any running spn and retry", sp_fmt_str(result->failed.path));
    }
    case SPN_INSTALL_ERR_STUCK: {
      return SP_CLI_ERR;
    }
  }
  return SP_CLI_OK;
}

typedef struct {
  sp_str_t symbol;
  u8 ansi;
  sp_fmt_styled_r styled;
} prose_t;

static sp_prompt_style_t ansi_style(u8 ansi) {
  return (sp_prompt_style_t) { .tag = SP_PROMPT_STYLE_ANSI, .ansi = ansi };
}

static void on_prose_event(sp_prompt_ctx_t* ctx, sp_prompt_event_t event) {
  sp_prompt_set_state(ctx, SP_PROMPT_STATE_SUBMIT);
}

static void on_prose_render(sp_prompt_ctx_t* ctx) {
  prose_t* prose = (prose_t*)sp_prompt_get_user_data(ctx);
  sp_prompt_render_line(ctx, prose->symbol, ansi_style(prose->ansi));
  sp_prompt_render_line(ctx, sp_str_lit("  "), sp_zero_s(sp_prompt_style_t));

  sp_str_t text = prose->styled.text;
  u32 cursor = 0;
  sp_da_for(prose->styled.spans, it) {
    sp_fmt_span_t span = prose->styled.spans[it];
    if (span.start < cursor) {
      continue;
    }
    sp_prompt_render_line(ctx, sp_str(text.data + cursor, span.start - cursor), sp_zero_s(sp_prompt_style_t));
    sp_prompt_render_line(ctx, sp_str(text.data + span.start, span.len), ansi_style(sp_fmt_style_to_ansi_u8(span.style)));
    cursor = span.start + span.len;
  }
  sp_prompt_render_line(ctx, sp_str(text.data + cursor, text.len - cursor), sp_zero_s(sp_prompt_style_t));
  sp_prompt_line(ctx, sp_str_lit(""));
}

static void prose(sp_prompt_ctx_t* prompt, sp_str_t symbol, u8 ansi, const c8* fmt, va_list args) {
  sp_mem_arena_marker_t s = sp_mem_begin_scratch();
  prose_t widget = {
    .symbol = symbol,
    .ansi = ansi,
    .styled = sp_fmt_styled_v(s.mem, sp_cstr_as_str(fmt), args),
  };
  if (widget.styled.err) {
    widget.styled = (sp_fmt_styled_r) { .text = sp_cstr_as_str(fmt) };
  }
  sp_prompt_run(prompt, (sp_prompt_widget_t) {
    .user_data = &widget,
    .on_event = on_prose_event,
    .render = on_prose_render,
  });
  sp_mem_end_scratch(s);
}

static void prose_info(sp_prompt_ctx_t* prompt, const c8* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  prose(prompt, sp_str_lit("●"), SP_ANSI_FG_CYAN_U8, fmt, args);
  va_end(args);
}

static void prose_warn(sp_prompt_ctx_t* prompt, const c8* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  prose(prompt, sp_str_lit("▲"), SP_ANSI_FG_YELLOW_U8, fmt, args);
  va_end(args);
}

static void prose_error(sp_prompt_ctx_t* prompt, const c8* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  prose(prompt, sp_str_lit("■"), SP_ANSI_FG_RED_U8, fmt, args);
  va_end(args);
}

static void prompt_shadow(sp_prompt_ctx_t* prompt, spn_install_facts_t* facts) {
  prose_warn(
    prompt,
    "{.cyan} is already installed ({.yellow})",
    sp_fmt_cstr("spn"),
    sp_fmt_str(facts->shadow)
  );
}

static void prompt_msg(sp_prompt_ctx_t* prompt, spn_install_msg_t* msg) {
  switch (msg->kind) {
    case SPN_INSTALL_MSG_NONE:
    case SPN_INSTALL_MSG_SHADOW: {
      break;
    }
    case SPN_INSTALL_MSG_STUCK_WRITE: {
      prose_error(prompt, "could not write {.yellow}", sp_fmt_str(msg->subject));
      break;
    }
    case SPN_INSTALL_MSG_STUCK_APPEND: {
      prose_error(prompt, "could not add {.cyan} to {.yellow}", sp_fmt_str(msg->detail), sp_fmt_str(msg->subject));
      break;
    }
    case SPN_INSTALL_MSG_STUCK_REGISTRY: {
      prose_error(prompt, "could not update the user PATH in the registry");
      break;
    }
    case SPN_INSTALL_MSG_RESTART_SHELL: {
      if (sp_str_empty(msg->subject)) {
        prose_info(prompt, "restart your shell to use {.cyan}", sp_fmt_cstr("spn"));
        break;
      }
      prose_info(prompt, "restart your shell, or run  {.cyan}", sp_fmt_str(msg->subject));
      break;
    }
    case SPN_INSTALL_MSG_RESTART_TERMINAL: {
      prose_info(prompt, "restart your terminal to use {.cyan}", sp_fmt_cstr("spn"));
      break;
    }
    case SPN_INSTALL_MSG_MANUAL: {
      prose_warn(prompt, "add {.yellow} to your PATH", sp_fmt_str(msg->subject));
      break;
    }
  }
}

static const c8* shell_label(spn_install_shell_t kind) {
  switch (kind) {
    case SPN_INSTALL_SHELL_NONE: return "";
    case SPN_INSTALL_SHELL_BASH: return "bash";
    case SPN_INSTALL_SHELL_ZSH: return "zsh";
    case SPN_INSTALL_SHELL_FISH: return "fish";
    case SPN_INSTALL_SHELL_CUSTOM: return "custom";
  }
  return "";
}

static bool has_choice(spn_install_choices_t* choices, spn_install_shell_t kind) {
  sp_for(it, choices->num_path) {
    if (choices->path[it].kind == kind) {
      return true;
    }
  }
  return false;
}

static sp_str_t custom_choice(spn_install_choices_t* choices) {
  sp_for(it, choices->num_path) {
    if (choices->path[it].kind == SPN_INSTALL_SHELL_CUSTOM) {
      return choices->path[it].custom;
    }
  }
  return sp_zero_s(sp_str_t);
}

static const c8* shell_hint(sp_mem_t mem, spn_install_layout_t* layout, spn_install_facts_t* facts, spn_install_shell_t kind) {
  if (kind == SPN_INSTALL_SHELL_FISH) {
    if (facts->fish_current) {
      return "already configured";
    }
    return sp_str_to_cstr(mem, layout->fish_conf);
  }

  sp_str_t hooks [SPN_INSTALL_MAX_RC];
  u32 num_hooks = spn_install_shell_hooks(layout, facts, kind, hooks);
  if (!num_hooks) {
    return "already configured";
  }
  return sp_str_to_cstr(mem, sp_str_join_n(mem, hooks, num_hooks, sp_str_lit(", ")));
}

static sp_str_t expand_path(sp_mem_t mem, sp_str_t path, sp_str_t home) {
  if (sp_str_equal(path, sp_str_lit("~"))) {
    path = home;
  }
  else if (sp_str_starts_with(path, sp_str_lit("~/")) && !sp_str_empty(home)) {
    path = sp_fs_join_path(mem, home, sp_str_sub(path, 2, (s32)path.len - 2));
  }
  path = sp_fs_normalize_path(mem, path);
  if (sp_fs_is_absolute(path)) {
    return path;
  }
  return sp_fs_join_path(mem, sp_fs_get_cwd(mem), path);
}

static spn_install_path_choice_t custom_file(sp_prompt_ctx_t* prompt, sp_mem_t mem, spn_install_layout_t* layout, sp_str_t current) {
  spn_install_path_choice_t choice = { .kind = SPN_INSTALL_SHELL_CUSTOM };

  while (true) {
    const c8* entered = sp_prompt_text(prompt, "custom file", sp_str_to_cstr(mem, current));
    if (sp_prompt_cancelled(prompt)) {
      return choice;
    }

    sp_str_t path = sp_cstr_as_str(entered);
    if (sp_str_empty(path)) {
      return choice;
    }
    current = path;
    path = expand_path(mem, path, layout->home);

    // a rejection is a typo, not a change of mind; ask again rather than
    // quietly dropping the file the user asked for
    if (sp_fs_is_dir(path)) {
      prose_error(prompt, "{.yellow} is a directory", sp_fmt_str(path));
      continue;
    }
    if (sp_str_equal(path, layout->root) || sp_str_starts_with(path, sp_str_concat(mem, layout->root, sp_str_lit("/")))) {
      prose_error(prompt, "{.yellow} is managed by spn", sp_fmt_str(path));
      continue;
    }

    if (sp_fs_exists(path)) {
      sp_str_t content = sp_zero;
      if (!sp_io_read_file(mem, path, &content)) {
        choice.has_line = sp_str_contains(content, sp_str_lit(SPN_INSTALL_RC_LINE));
      }
    }
    else {
      bool create = sp_prompt_confirm(prompt, cfmt(mem, "{} does not exist; create it?", sp_fmt_str(path)), true);
      if (sp_prompt_cancelled(prompt)) {
        return choice;
      }
      if (!create) {
        continue;
      }
    }

    choice.custom = path;
    return choice;
  }
}

static void prompt_path(sp_prompt_ctx_t* prompt, sp_mem_t mem, spn_install_probe_t* probe, spn_install_choices_t* choices) {
  spn_install_shell_t kinds [] = {
    SPN_INSTALL_SHELL_BASH,
    SPN_INSTALL_SHELL_ZSH,
    SPN_INSTALL_SHELL_FISH,
    SPN_INSTALL_SHELL_CUSTOM,
  };

  sp_str_t custom = custom_choice(choices);
  sp_prompt_select_option_t options [sp_carr_len(kinds)] = sp_zero;
  sp_carr_for(kinds, it) {
    options[it] = (sp_prompt_select_option_t) {
      .label = shell_label(kinds[it]),
      .hint = kinds[it] == SPN_INSTALL_SHELL_CUSTOM
        ? (sp_str_empty(custom) ? SP_NULLPTR : sp_str_to_cstr(mem, custom))
        : shell_hint(mem, &probe->layout, &probe->facts, kinds[it]),
      .selected = has_choice(choices, kinds[it]),
    };
  }

  sp_prompt_multiselect(prompt, (sp_prompt_multiselect_t) {
    .prompt = "Add spn to $PATH",
    .options = options,
    .num_options = sp_carr_len(kinds),
  });
  if (sp_prompt_cancelled(prompt)) {
    return;
  }

  choices->num_path = 0;
  sp_carr_for(kinds, it) {
    if (!options[it].selected) {
      continue;
    }
    if (kinds[it] == SPN_INSTALL_SHELL_CUSTOM) {
      spn_install_path_choice_t choice = custom_file(prompt, mem, &probe->layout, custom);
      if (!sp_str_empty(choice.custom)) {
        choices->path[choices->num_path++] = choice;
      }
    }
    else {
      choices->path[choices->num_path++] = (spn_install_path_choice_t) { .kind = kinds[it] };
    }
  }
}

static sp_str_t action_label(spn_install_action_t* action) {
  switch (action->kind) {
    case SPN_INSTALL_ACTION_SET_USER_PATH: {
      return sp_str_lit("user PATH");
    }
    default: {
      return action->path;
    }
  }
}

static bool action_creates(spn_install_action_t* action) {
  return !sp_str_empty(action->path) && !sp_fs_exists(action->path);
}

static void prompt_plan(sp_prompt_ctx_t* prompt, sp_mem_t mem, spn_install_plan_t* plan) {
  sp_prompt_note_t note = sp_prompt_note_new(mem, "Changes");
  sp_for(it, plan->count) {
    spn_install_action_t* action = &plan->actions[it];
    if (action_creates(action)) {
      sp_prompt_note_line_fmt(&note, "{.green} {}", sp_fmt_cstr("+"), sp_fmt_str(action_label(action)));
    }
    else {
      sp_prompt_note_line_fmt(&note, "{.yellow} {}", sp_fmt_cstr("~"), sp_fmt_str(action_label(action)));
    }
  }
  sp_prompt_note_ex(prompt, note);
}

static void prompt_choices(sp_prompt_ctx_t* prompt, sp_mem_t mem, spn_install_probe_t* probe, spn_install_choices_t* choices) {
  switch (probe->layout.os) {
    case SPN_INSTALL_OS_UNIX: {
      prompt_path(prompt, mem, probe, choices);
      break;
    }
    case SPN_INSTALL_OS_WINDOWS: {
      choices->registry = sp_prompt_confirm(prompt, cfmt(mem, "Add {} to your PATH?", sp_fmt_str(probe->layout.bin_native)), choices->registry);
      break;
    }
  }
}

static sp_cli_result_t run_prompt(sp_cli_t* cli, sp_prompt_ctx_t* prompt, spn_install_probe_t* probe) {
  sp_mem_arena_marker_t s = sp_mem_begin_scratch_for(host.mem);
  sp_cli_result_t status = SP_CLI_OK;

  sp_prompt_intro(prompt, "spn");

  if (spn_install_shadowed(&probe->layout, &probe->facts)) {
    prompt_shadow(prompt, &probe->facts);
  }

  spn_install_choices_t choices = spn_install_choices(&probe->layout, &probe->facts);
  bool can_path = spn_install_path_state(&probe->layout, &probe->facts, &choices) == SPN_INSTALL_PATH_UPDATED;

  spn_install_plan_t plan = sp_zero;
  while (true) {
    if (can_path) {
      prompt_choices(prompt, s.mem, probe, &choices);
      if (sp_prompt_cancelled(prompt)) {
        sp_prompt_cancel(prompt, "cancelled");
        status = SP_CLI_ERR;
        goto cleanup;
      }
    }

    plan = spn_install_plan(s.mem, &probe->layout, &probe->facts, &choices);
    if (!plan.count) {
      break;
    }

    prompt_plan(prompt, s.mem, &plan);
    if (sp_prompt_confirm(prompt, "Proceed?", true)) {
      break;
    }
    if (!can_path || sp_prompt_cancelled(prompt)) {
      sp_prompt_cancel(prompt, "cancelled");
      status = SP_CLI_ERR;
      goto cleanup;
    }
  }

  spn_install_t result = spn_install_execute(probe, &plan);
  if (result.err == SPN_INSTALL_ERR_EXE) {
    sp_prompt_cancel(prompt, "install failed");
    status = install_error(cli, &result);
    goto cleanup;
  }

  sp_for(it, result.msgs.count) {
    prompt_msg(prompt, &result.msgs.items[it]);
  }

  if (result.stuck) {
    sp_prompt_outro(prompt, cfmt(s.mem, "{} of {} changes failed", sp_fmt_uint(result.stuck), sp_fmt_uint(result.changes)));
    status = install_error(cli, &result);
    goto cleanup;
  }
  sp_prompt_outro(prompt, result.changes ? "spn " SPN_VERSION " installed" : "spn " SPN_VERSION " already installed");

cleanup:
  sp_mem_end_scratch(s);
  return status;
}

static bool use_prompt() {
  if (args.unattended) {
    return false;
  }
  if (tui.mode != SPN_OUTPUT_MODE_INTERACTIVE) {
    return false;
  }
  return sp_sys_is_tty(sp_sys_stdout) && sp_sys_is_tty(sp_sys_stdin);
}

static sp_cli_result_t run_unattended(sp_cli_t* cli) {
  spn_install_t result = spn_install(host.mem);
  if (result.err != SPN_INSTALL_ERR_EXE && !sp_str_empty(result.exe)) {
    if (result.changes) {
      spn_print(&tui, "install: spn {} installed to {}", sp_fmt_cstr(SPN_VERSION), sp_fmt_str(result.exe));
    }
    else {
      spn_print(&tui, "install: spn {} already installed at {}", sp_fmt_cstr(SPN_VERSION), sp_fmt_str(result.exe));
    }
  }
  sp_for(it, result.msgs.count) {
    print_msg(&result.msgs.items[it]);
  }
  return install_error(cli, &result);
}

static sp_cli_result_t install(sp_cli_t* cli) {
  spn_tui_handoff(&tui);

  if (use_prompt()) {
    spn_install_probe_t probe = spn_install_probe(host.mem);
    if (probe.layout.err) {
      spn_install_t result = { .err = probe.layout.err };
      return install_error(cli, &result);
    }

    sp_prompt_ctx_t* prompt = sp_prompt_begin(host.mem);
    if (prompt) {
      sp_cli_result_t result = run_prompt(cli, prompt, &probe);
      sp_prompt_end(prompt);
      return result;
    }
  }

  return run_unattended(cli);
}

static sp_cli_cmd_t spn_cmd_self_install = {
  .name = "install",
  .summary = "Install this binary and set up your PATH",
  .opts = {
    {
      .name = "auto",
      .summary = "Install unattended, accepting all defaults",
      .kind = SP_CLI_OPT_BOOLEAN,
      .ptr = &args.unattended,
    },
  },
  .handler = install,
};

sp_cli_cmd_t spn_cmd_self = {
  .name = "self",
  .summary = "Manage this spn installation",
  .commands = {
    &spn_cmd_self_install,
  },
};
