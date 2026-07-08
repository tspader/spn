#include "cli/cli.h"

#include "ctx/types.h"
#include "log/log.h"
#include "sp/prompt.h"
#include "sp/sp_template.h"
#include "spn.embed.h"

#define SPN_INIT_MAX_FILES 8

typedef struct {
  sp_str_t rel;
  sp_str_t path;
  sp_str_t content;
} spn_init_file_t;

typedef struct {
  sp_prompt_ctx_t* prompt;
  sp_str_t name;
  spn_init_file_t files [SPN_INIT_MAX_FILES];
  u32 num_files;
} spn_init_t;

static bool is_name_valid(sp_str_t name) {
  if (sp_str_empty(name)) return false;
  sp_str_for(name, it) {
    c8 c = name.data[it];
    if (c == '"' || c == '\\' || c == '\n' || c == '\r' || c == '\t' || c == ' ') return false;
  }
  return true;
}

static spn_err_t spn_init_render(spn_init_t* init, sp_mem_t mem, sp_str_t dir, bool bare) {
  sp_template_scope_t* scope = sp_template_scope_create(mem);
  sp_template_set(scope, sp_str_lit("name"), init->name);

  spn_init_file_t manifest = sp_zero;
  sp_carr_for(spn_embed_manifest, it) {
    spn_embed_entry_t entry = spn_embed_manifest[it];
    sp_str_t path = sp_str_view(entry.path);
    if (!sp_str_starts_with(path, sp_str_lit("init/"))) {
      continue;
    }

    sp_str_t rel = sp_str_strip_left(path, sp_str_lit("init/"));
    bool is_manifest = sp_str_equal_cstr(rel, "spn.toml");
    if (bare && !is_manifest) {
      continue;
    }
    if (sp_str_equal_cstr(rel, "gitignore")) {
      rel = sp_str_lit(".gitignore");
    }

    sp_io_dyn_mem_writer_t content = sp_zero;
    sp_io_dyn_mem_writer_init(mem, &content);
    if (sp_template_render(&content.base, sp_str((const c8*)entry.data, entry.size), scope, SP_NULLPTR)) {
      return SPN_ERROR;
    }

    spn_init_file_t file = {
      .rel = rel,
      .path = sp_fs_join_path(mem, dir, rel),
      .content = sp_io_dyn_mem_writer_take_str(&content),
    };

    if (is_manifest) {
      manifest = file;
      continue;
    }

    SP_ASSERT(init->num_files < SPN_INIT_MAX_FILES - 1);
    init->files[init->num_files++] = file;
  }

  if (sp_str_empty(manifest.rel)) {
    return SPN_ERROR;
  }

  init->files[init->num_files++] = manifest;
  return SPN_OK;
}

static sp_str_t spn_init_run(spn_cli_init_t* command, sp_mem_arena_marker_t s, spn_init_t* init) {
  sp_str_t dir = spn.paths.project;
  if (!sp_str_empty(command->path)) {
    dir = sp_fs_is_absolute(command->path) ? command->path : sp_fs_join_path(s.mem, spn.paths.project, command->path);
  }

  if (sp_fs_create_dir(dir) != SP_OK) {
    return sp_fmt(s.mem, "failed to create {.cyan}", sp_fmt_str(dir)).value;
  }

  dir = sp_fs_canonicalize_path(s.mem, dir);
  sp_str_t manifest = sp_fs_join_path(s.mem, dir, sp_str_lit("spn.toml"));
  if (sp_fs_exists(manifest)) {
    return sp_fmt(s.mem, "manifest already exists at {.cyan}", sp_fmt_str(manifest)).value;
  }

  init->name = sp_fs_get_name(dir);

  if (init->prompt) {
    const c8* entered = sp_prompt_text(init->prompt, "name", sp_str_to_cstr(s.mem, init->name));
    if (sp_prompt_cancelled(init->prompt)) {
      return sp_str_lit("cancelled");
    }

    sp_str_t response = sp_str_copy(s.mem, sp_cstr_as_str(entered));
    if (!sp_str_empty(response)) {
      init->name = response;
    }
  }

  if (!is_name_valid(init->name)) {
    return sp_fmt(s.mem, "invalid name {.quote}", sp_fmt_str(init->name)).value;
  }

  if (spn_init_render(init, s.mem, dir, command->bare)) {
    return sp_str_lit("failed to render templates");
  }

  sp_for(it, init->num_files) {
    if (sp_fs_create_file_str(init->files[it].path, init->files[it].content) != SP_OK) {
      return sp_fmt(s.mem, "failed to write {}", sp_fmt_str(init->files[it].path)).value;
    }
  }

  return sp_zero_s(sp_str_t);
}

static void spn_init_files_submit(sp_prompt_ctx_t* ctx, sp_prompt_event_t event) {
  SP_UNUSED(event);
  sp_prompt_set_state(ctx, SP_PROMPT_STATE_SUBMIT);
}

static void spn_init_files_render(sp_prompt_ctx_t* ctx) {
  spn_init_t* init = (spn_init_t*)sp_prompt_user_data(ctx);
  sp_prompt_style_t green = {
    .tag = SP_PROMPT_STYLE_ANSI,
    .ansi = SP_ANSI_FG_GREEN_U8,
  };

  sp_for(it, init->num_files) {
    sp_prompt_render_line(ctx, sp_str_lit("│  "), sp_zero_s(sp_prompt_style_t));
    sp_prompt_render_line(ctx, sp_str_lit("+ "), green);
    sp_prompt_line(ctx, init->files[it].rel);
  }
}

spn_task_result_t spn_task_init(spn_app_t* app) {
  sp_mem_arena_marker_t s = sp_mem_begin_scratch();
  spn_cli_init_t* command = &spn.cli.init;

  spn_init_t init = sp_zero;
  if (sp_str_empty(command->path) && sp_os_is_tty(sp_sys_stdout)) {
    init.prompt = sp_prompt_begin(spn.mem);
  }

  if (init.prompt) {
    sp_prompt_intro(init.prompt, "spn init");
  }

  sp_str_t error = spn_init_run(command, s, &init);
  sp_str_t hint = sp_fmt(s.mem, "To run your program:\n\n  spn run {}", sp_fmt_str(init.name)).value;

  if (init.prompt) {
    if (sp_prompt_cancelled(init.prompt)) {
      sp_prompt_cancel(init.prompt, "cancelled");
    }
    else if (!sp_str_empty(error)) {
      sp_prompt_error(init.prompt, sp_str_to_cstr(s.mem, error));
    }
    else {
      sp_prompt_run(init.prompt, (sp_prompt_widget_t) {
        .user_data = &init,
        .on_event = spn_init_files_submit,
        .render = spn_init_files_render,
      });
      sp_prompt_note(init.prompt, sp_str_to_cstr(s.mem, hint), "Done");
    }
    sp_prompt_end(init.prompt);
  }
  else if (!sp_str_empty(error)) {
    spn_log_error("{}", sp_fmt_str(error));
  }
  else {
    sp_for(it, init.num_files) {
      spn_log_info("- {}", sp_fmt_str(init.files[it].rel));
    }
    spn_log_info("");
    spn_log_info("{}", sp_fmt_str(hint));
  }

  sp_mem_end_scratch(s);
  return sp_str_empty(error) ? SPN_TASK_DONE : SPN_TASK_ERROR;
}

sp_cli_result_t spn_cli_init(sp_cli_t* cli) {
  spn_task_enqueue(&app.tasks, SPN_TASK_KIND_INIT);
  return SP_CLI_CONTINUE;
}
