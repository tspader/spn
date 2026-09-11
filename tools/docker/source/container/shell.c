#define SP_IMPLEMENTATION
#include "sp.h"

#include "container.h"

static s32 execute(sp_mem_t mem, sp_ps_config_cstr_t config) {
  sp_ps_t ps = sp_ps_create_c(mem, config);
  if (!ps.os) {
    return -1;
  }
  sp_ps_status_t status = sp_ps_wait(&ps);
  sp_ps_free(&ps);
  return status.exit_code;
}

static void seed_profile(sp_mem_t mem, const c8* body) {
  sp_str_t manifest = sp_str_lit(CONTAINER_PROJECT_DIR "/spn.toml");
  sp_str_t content = sp_zero;
  if (sp_io_read_file(mem, manifest, &content)) {
    return;
  }
  sp_str_t profile = sp_fmt(mem, "\n[profile.default]\n{}", sp_fmt_cstr(body)).value;
  sp_fs_create_file_str(manifest, sp_str_concat(mem, content, profile));
}

s32 main(s32 num_args, const c8** args) {
  sp_mem_t mem = sp_mem_os_new();

  sp_fs_create_dir(sp_str_lit("/usr/local/bin"));
  sp_fs_create_sym_link(sp_str_lit(CONTAINER_SPN), sp_str_lit("/usr/local/bin/spn"));

  execute(mem, (sp_ps_config_cstr_t) {
    .command = CONTAINER_SPN,
    .args = { "init", CONTAINER_PROJECT },
    .cwd = CONTAINER_WORK,
    .io = SP_PS_NO_STDIO,
  });
  if (num_args == 2) {
    seed_profile(mem, args[1]);
  }

  sp_log("spn smoke: fresh {.cyan} on PATH, {.cyan} project seeded, lanes declared in {.cyan}", sp_fmt_cstr("spn"), sp_fmt_cstr(CONTAINER_PROJECT), sp_fmt_cstr(CONTAINER_CONFIG));
  if (num_args == 2) {
    sp_log("spn smoke: {.cyan} seeded in {.cyan}:\n{}", sp_fmt_cstr("[profile.default]"), sp_fmt_cstr(CONTAINER_PROJECT "/spn.toml"), sp_fmt_cstr(args[1]));
  }

  const c8* cwd = sp_fs_is_dir(sp_str_lit(CONTAINER_PROJECT_DIR)) ? CONTAINER_PROJECT_DIR : CONTAINER_WORK;

  const c8* shells [] = { "/bin/bash", "/bin/sh" };
  sp_carr_for(shells, it) {
    if (!sp_fs_is_target_file(sp_cstr_as_str(shells[it]))) {
      continue;
    }
    return execute(mem, (sp_ps_config_cstr_t) {
      .command = shells[it],
      .cwd = cwd,
    });
  }
  return 1;
}
