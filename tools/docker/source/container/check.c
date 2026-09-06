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

s32 main(s32 num_args, const c8** args) {
  sp_assert(num_args == 2);
  sp_mem_t mem = sp_mem_os_new();

  sp_ps_config_cstr_t steps [] = {
    {
      .command = CONTAINER_SPN,
      .args = { "init", CONTAINER_PROJECT },
      .cwd = CONTAINER_WORK,
    },
    {
      .command = CONTAINER_SPN,
      .args = { "build", "--toolchain", args[1] },
      .cwd = CONTAINER_PROJECT_DIR,
    },
    {
      .command = CONTAINER_PROJECT_EXE,
      .cwd = CONTAINER_PROJECT_DIR,
    },
  };

  sp_carr_for(steps, it) {
    if (execute(mem, steps[it])) {
      sp_log("{.red} {}", sp_fmt_cstr("FAIL"), sp_fmt_cstr(steps[it].command));
      return 1;
    }
  }
  return 0;
}
