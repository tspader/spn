#include "cli/types.h"
#include "ctx/types.h"

#include "error/types.h"
#include "log/log.h"
#include "sp/prompt.h"
#include "sp/sp_template.h"
#include "spn.embed.h"
#include "task/task.h"
#include "tui/tui.h"

typedef struct {
  u32 index;
  bool bare;
  bool done;
  sp_str_t rel;
  sp_str_t tpl;
} iterator_t;

static void it_next(iterator_t* it) {
  while (it->index < sp_carr_len(spn_embed_manifest)) {
    spn_embed_entry_t entry = spn_embed_manifest[it->index++];
    sp_str_t path = sp_str_view(entry.path);
    if (!sp_str_starts_with(path, sp_str_lit("init/"))) continue;

    sp_str_t rel = sp_str_strip_left(path, sp_str_lit("init/"));
    if (it->bare && !sp_str_equal_cstr(rel, "spn.toml")) continue;
    if (sp_str_equal_cstr(rel, "gitignore")) rel = sp_str_lit(".gitignore");

    it->rel = rel;
    it->tpl = sp_str((const c8*)entry.data, entry.size);
    return;
  }

  it->done = true;
}

static iterator_t it_new(bool bare) {
  iterator_t it = { .bare = bare };
  it_next(&it);
  return it;
}

static bool is_name_valid(sp_str_t name) {
  if (sp_str_empty(name)) return false;
  sp_str_for(name, it) {
    c8 c = name.data[it];
    if (c == '"' || c == '\\' || c == '\n' || c == '\r' || c == '\t' || c == ' ') return false;
  }
  return true;
}

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

static spn_err_union_t validate_dir(sp_mem_t mem, spn_cli_init_t* command, sp_str_t dir) {
  sp_mem_arena_marker_t s = sp_mem_begin_scratch_for(mem);
  spn_err_union_t err = spn_result(SPN_OK);

  for (iterator_t it = it_new(command->bare); !it.done; it_next(&it)) {
    sp_str_t path = sp_fs_join_path(s.mem, dir, it.rel);
    if (sp_fs_exists(path)) {
      err = (spn_err_union_t) { .kind = SPN_ERR_INIT_EXISTS, .fs = { .path = sp_str_copy(mem, path) } };
      break;
    }
  }

  sp_mem_end_scratch(s);
  return err;
}

static spn_err_union_t render(sp_mem_t mem, sp_str_t dir, sp_str_t name, bool bare) {
  if (!is_name_valid(name)) {
    return (spn_err_union_t) { .kind = SPN_ERR_INIT_NAME, .pkg = { .name = sp_str_copy(mem, name) } };
  }

  if (sp_fs_create_dir(dir) != SP_OK) {
    return (spn_err_union_t) { .kind = SPN_ERR_FS_WRITE, .fs = { .path = sp_str_copy(mem, dir) } };
  }

  sp_mem_arena_marker_t s = sp_mem_begin_scratch_for(mem);
  spn_err_union_t err = spn_result(SPN_OK);

  sp_template_scope_t* scope = sp_template_scope_create(s.mem);
  sp_template_set(scope, sp_str_lit("name"), name);

  for (iterator_t it = it_new(bare); !it.done; it_next(&it)) {
    sp_io_dyn_mem_writer_t writer = sp_zero;
    sp_io_dyn_mem_writer_init(s.mem, &writer);
    if (sp_template_render(&writer.base, it.tpl, scope, SP_NULLPTR)) {
      err = spn_result(SPN_ERROR);
      break;
    }

    sp_str_t path = sp_fs_join_path(s.mem, dir, it.rel);
    if (sp_fs_create_file_str(path, sp_io_dyn_mem_writer_take_str(&writer)) != SP_OK) {
      err = (spn_err_union_t) { .kind = SPN_ERR_FS_WRITE, .fs = { .path = sp_str_copy(mem, path) } };
      break;
    }
  }

  sp_mem_end_scratch(s);
  return err;
}

static void on_submit_prompt(sp_prompt_ctx_t* ctx, sp_prompt_event_t event) {
  sp_prompt_set_state(ctx, SP_PROMPT_STATE_SUBMIT);
}

static void on_render_prompt(sp_prompt_ctx_t* ctx) {
  bool bare = *(bool*)sp_prompt_user_data(ctx);
  sp_prompt_style_t green = {
    .tag = SP_PROMPT_STYLE_ANSI,
    .ansi = SP_ANSI_FG_GREEN_U8,
  };

  for (iterator_t it = it_new(bare); !it.done; it_next(&it)) {
    sp_prompt_render_line(ctx, sp_str_lit("│  "), sp_zero_s(sp_prompt_style_t));
    sp_prompt_render_line(ctx, sp_str_lit("+ "), green);
    sp_prompt_line(ctx, it.rel);
  }
}

static spn_err_union_t run_prompt(sp_mem_t mem, spn_cli_init_t* command, sp_str_t dir, sp_str_t name) {
  sp_mem_arena_marker_t s = sp_mem_begin_scratch_for(mem);
  spn_err_union_t err = spn_result(SPN_OK);

  sp_prompt_ctx_t* prompt = sp_prompt_begin(mem);
  sp_prompt_intro(prompt, "spn init");

  const c8* entered = sp_prompt_text(prompt, "name", sp_str_to_cstr(s.mem, name));
  if (sp_prompt_cancelled(prompt)) {
    sp_prompt_cancel(prompt, "cancelled");
    err = spn_err_reported(SPN_ERROR);
    goto cleanup;
  }

  sp_str_t response = sp_str_copy(s.mem, sp_str_view(entered));
  if (!sp_str_empty(response)) {
    name = response;
  }

  err = render(mem, dir, name, command->bare);
  if (err.kind) {
    spn_build_event_t event = { .kind = SPN_EVENT_ERR, .err = err };
    sp_prompt_error(prompt, sp_str_to_cstr(s.mem, spn_tui_render_event_detail(s.mem, &event)));
    err.reported = true;
  }
  else {
    sp_prompt_run(prompt, (sp_prompt_widget_t) {
      .user_data = &command->bare,
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

static spn_err_union_t run_unattended(sp_mem_t mem, spn_cli_init_t* command, sp_str_t dir, sp_str_t name) {
  try_union(render(mem, dir, name, command->bare));

  for (iterator_t it = it_new(command->bare); !it.done; it_next(&it)) {
    spn_log_info("- {}", sp_fmt_str(it.rel));
  }
  spn_log_info("");
  spn_log_info("To build your program:\n\n  spn build {}", sp_fmt_str(name));

  return spn_result(SPN_OK);
}

static spn_err_union_t run(sp_mem_t mem, spn_cli_init_t* command, sp_str_t project) {
  sp_str_t dir = get_dir(mem, command, project);
  try_union(validate_dir(mem, command, dir));

  sp_str_t name = sp_fs_get_name(dir);
  if (sp_os_is_tty(sp_sys_stdout) && sp_str_empty(command->path)) {
    return run_prompt(mem, command, dir, name);
  }

  return run_unattended(mem, command, dir, name);
}

spn_task_step_t spn_task_init(spn_app_t* app) {
  spn_err_union_t err = run(spn.mem, &spn.cli.init, spn.paths.project);
  if (err.kind) return spn_task_fail_with(err);
  return spn_task_done();
}
