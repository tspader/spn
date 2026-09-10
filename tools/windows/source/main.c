#include "sp.h"
#include "sp/sp_cli.h"
#include "sp/sp_prompt.h"

#include "tail/tail.h"
#include "winvm/winvm.h"
#include "variant/variant.h"

#define try(expr) do { sp_cli_result_t _e = (expr); if (_e) return _e; } while (0)
#define cfmt(mem, ...) sp_str_to_cstr(mem, sp_fmt(mem, __VA_ARGS__).value)

typedef struct {
  winvm_t vm;
  sp_prompt_ctx_t* prompt;
} app_t;

static struct {
  sp_cli_choice_t variant;
} args;

static sp_cli_result_t init(sp_cli_t* cli, app_t* app) {
  sp_mem_t mem = sp_mem_heap_as_allocator(sp_mem_heap_new());
  switch (winvm_init(&app->vm, mem)) {
    case WINVM_INIT_OK: {
      return SP_CLI_OK;
    }
    case WINVM_INIT_ERR_NO_DIR: {
      return sp_cli_set_error_c(cli, "set SPN_WIN_DIR to the directory that holds the VM images");
    }
    case WINVM_INIT_ERR_TEMPLATES: {
      return sp_cli_set_error(cli, sp_fmt(mem, "failed to load templates from {.cyan}", sp_fmt_str(app->vm.paths.templates)).value);
    }
  }
  SP_UNREACHABLE_RETURN(SP_CLI_ERR);
}

static sp_cli_result_t begin(sp_cli_t* cli, app_t* app, const c8* intro) {
  app->prompt = sp_prompt_begin(app->vm.mem);
  if (!app->prompt) {
    return sp_cli_set_error_c(cli, "winvm needs an interactive terminal");
  }
  sp_prompt_intro(app->prompt, intro);
  return SP_CLI_OK;
}

static sp_cli_result_t fail(sp_cli_t* cli, app_t* app, sp_str_t message) {
  sp_prompt_cancel(app->prompt, sp_str_to_cstr(app->vm.mem, message));
  return sp_cli_set_error(cli, message);
}

static sp_cli_result_t oops(sp_cli_t* cli, app_t* app, sp_str_t message) {
  sp_prompt_error(app->prompt, sp_str_to_cstr(app->vm.mem, message));
  return sp_cli_set_error(cli, message);
}

static sp_cli_result_t run_golden(sp_cli_t* cli) {
  app_t app = sp_zero;
  try(init(cli, &app));
  winvm_t* vm = &app.vm;
  sp_mem_t mem = vm->mem;
  try(begin(cli, &app, "winvm golden"));

  sp_str_t origin = sp_fs_join_path(mem, vm->paths.dir, sp_str_lit("origin.qcow2"));

  s32 status = tail_trace(mem, app.prompt,
    cfmt(mem, "vol-download {} from pool {}", sp_fmt_str(vm->cfg.origin), sp_fmt_str(vm->cfg.pool)),
    winvm_log(vm, "golden-download"),
    winvm_voldownload_config(vm, origin));
  if (sp_prompt_cancelled(app.prompt)) {
    sp_fs_remove_file(origin);
    return fail(cli, &app, sp_str_lit("cancelled"));
  }
  if (status) {
    return fail(cli, &app, sp_str_lit("vol-download failed"));
  }

  sp_fs_remove_file(vm->paths.golden);
  status = tail_trace(mem, app.prompt,
    cfmt(mem, "flatten snapshot {} into golden", sp_fmt_str(vm->cfg.snapshot)),
    winvm_log(vm, "golden-convert"),
    winvm_convert_config(vm, origin, vm->paths.golden));
  if (status) {
    return fail(cli, &app, sp_fmt(mem, "failed to flatten {} into {.cyan}", sp_fmt_str(vm->cfg.snapshot), sp_fmt_str(vm->paths.golden)).value);
  }

  winvm_seal(vm, vm->paths.golden);
  sp_fs_remove_file(origin);
  sp_prompt_outro(app.prompt, cfmt(mem, "golden ready at {}", sp_fmt_str(vm->paths.golden)));
  sp_prompt_end(app.prompt);
  return SP_CLI_OK;
}

static sp_cli_result_t build_one(sp_cli_t* cli, app_t* app, const winvm_variant_t* variant) {
  winvm_t* vm = &app->vm;
  sp_mem_t mem = vm->mem;

  if (!sp_fs_is_file(vm->paths.golden)) {
    return oops(cli, app, sp_fmt(mem, "no golden at {.cyan}; run winvm golden", sp_fmt_str(vm->paths.golden)).value);
  }

  winvm_destroy(vm, variant);
  winvm_undefine(vm, variant);
  sp_str_t image = winvm_image(vm, variant);
  if (winvm_overlay(vm, vm->paths.golden, image)) {
    return oops(cli, app, sp_fmt(mem, "failed to create overlay {.cyan}", sp_fmt_str(image)).value);
  }
  winvm_lease(vm, variant);
  if (winvm_boot(vm, variant, image)) {
    return oops(cli, app, sp_fmt(mem, "failed to start {.cyan}", sp_fmt_str(winvm_domain(vm, variant))).value);
  }

  s32 status = tail_trace(mem, app->prompt, cfmt(mem, "boot {}", sp_fmt_cstr(variant->name)),
    winvm_log(vm, cfmt(mem, "boot-{}", sp_fmt_cstr(variant->name))), winvm_wait_ssh_config(vm, variant, 360));
  if (sp_prompt_cancelled(app->prompt)) {
    winvm_destroy(vm, variant);
    return fail(cli, app, sp_str_lit("cancelled"));
  }
  if (status) {
    winvm_destroy(vm, variant);
    winvm_undefine(vm, variant);
    return oops(cli, app, sp_fmt(mem, "{} never came up on ssh", sp_fmt_cstr(variant->name)).value);
  }

  winvm_step_t steps[1 + WINVM_MAX_STEPS];
  u32 count = 0;
  steps[count++] = (winvm_step_t) { .recipe = "common" };
  sp_carr_for_until(variant->steps, it, variant->steps[it].recipe) {
    steps[count++] = variant->steps[it];
  }

  sp_for(it, count) {
    winvm_step_t step = steps[it];
    if (winvm_upload_recipe(vm, variant, step)) {
      winvm_destroy(vm, variant);
      winvm_undefine(vm, variant);
      return oops(cli, app, sp_fmt(mem, "failed to copy recipe {.cyan} to {}", sp_fmt_cstr(step.recipe), sp_fmt_str(winvm_domain(vm, variant))).value);
    }
    const c8* title = step.arg
      ? cfmt(mem, "{} {} in {}", sp_fmt_cstr(step.recipe), sp_fmt_cstr(step.arg), sp_fmt_cstr(variant->name))
      : cfmt(mem, "{} in {}", sp_fmt_cstr(step.recipe), sp_fmt_cstr(variant->name));
    status = tail_trace(mem, app->prompt, title,
      winvm_log(vm, cfmt(mem, "provision-{}-{}", sp_fmt_cstr(variant->name), sp_fmt_cstr(step.recipe))),
      winvm_recipe_config(vm, variant, step));
    if (sp_prompt_cancelled(app->prompt)) {
      winvm_destroy(vm, variant);
      winvm_undefine(vm, variant);
      return fail(cli, app, sp_str_lit("cancelled"));
    }
    if (status) {
      winvm_destroy(vm, variant);
      winvm_undefine(vm, variant);
      return oops(cli, app, sp_fmt(mem, "recipe {.cyan} failed in {}", sp_fmt_cstr(step.recipe), sp_fmt_cstr(variant->name)).value);
    }
  }

  winvm_shutdown(vm, variant);
  status = tail_trace(mem, app->prompt, cfmt(mem, "shut down {}", sp_fmt_cstr(variant->name)),
    winvm_log(vm, cfmt(mem, "shutdown-{}", sp_fmt_cstr(variant->name))), winvm_wait_off_config(vm, variant, 180));
  if (status) {
    winvm_destroy(vm, variant);
  }
  winvm_undefine(vm, variant);
  winvm_seal(vm, image);
  sp_prompt_success(app->prompt, cfmt(mem, "built {} -> {}", sp_fmt_cstr(variant->name), sp_fmt_str(image)));
  return SP_CLI_OK;
}

static sp_cli_result_t run_build(sp_cli_t* cli) {
  app_t app = sp_zero;
  try(init(cli, &app));
  sp_mem_t mem = app.vm.mem;

  sp_da(const winvm_variant_t*) selected = sp_da_new(mem, const winvm_variant_t*);
  for (const c8** it = cli->rest; *it; it++) {
    const winvm_variant_t* variant = winvm_variant_find(*it);
    if (!variant) {
      return sp_cli_set_error(cli, sp_fmt(mem, "unknown variant {.cyan}; see winvm list", sp_fmt_cstr(*it)).value);
    }
    sp_da_push(selected, variant);
  }
  if (sp_da_empty(selected)) {
    return sp_cli_set_error_c(cli, "name at least one variant to build; see winvm list");
  }

  try(begin(cli, &app, "winvm build"));
  u32 failures = 0;
  sp_da_for(selected, it) {
    if (build_one(cli, &app, selected[it])) {
      failures++;
    }
    if (sp_prompt_cancelled(app.prompt)) {
      break;
    }
  }
  if (failures) {
    sp_prompt_outro(app.prompt, cfmt(mem, "{} variants failed", sp_fmt_uint(failures)));
    sp_prompt_end(app.prompt);
    return sp_cli_set_error(cli, sp_fmt(mem, "{} variants failed", sp_fmt_uint(failures)).value);
  }
  sp_prompt_outro(app.prompt, "all variants built");
  sp_prompt_end(app.prompt);
  return SP_CLI_OK;
}

static sp_cli_result_t up(sp_cli_t* cli, app_t* app, const winvm_variant_t* variant) {
  winvm_t* vm = &app->vm;
  sp_mem_t mem = vm->mem;

  sp_str_t image = winvm_image(vm, variant);
  if (!sp_fs_is_file(image)) {
    return oops(cli, app, sp_fmt(mem, "{} is not built; run winvm build {}", sp_fmt_cstr(variant->name), sp_fmt_cstr(variant->name)).value);
  }
  if (winvm_running(vm, variant)) {
    sp_prompt_success(app->prompt, cfmt(mem, "{} already up at {}", sp_fmt_cstr(variant->name), sp_fmt_str(winvm_ip(vm, variant))));
    return SP_CLI_OK;
  }

  winvm_lease(vm, variant);
  sp_str_t work = winvm_work(vm, variant);
  if (!sp_fs_is_file(work) && winvm_overlay(vm, image, work)) {
    return oops(cli, app, sp_fmt(mem, "failed to create work overlay {.cyan}", sp_fmt_str(work)).value);
  }
  if (winvm_boot(vm, variant, work)) {
    return oops(cli, app, sp_fmt(mem, "failed to start {.cyan}", sp_fmt_str(winvm_domain(vm, variant))).value);
  }

  s32 status = tail_trace(mem, app->prompt, cfmt(mem, "boot {}", sp_fmt_cstr(variant->name)),
    winvm_log(vm, cfmt(mem, "up-{}", sp_fmt_cstr(variant->name))), winvm_wait_ssh_config(vm, variant, 360));
  if (status) {
    return oops(cli, app, sp_fmt(mem, "{} never came up on ssh", sp_fmt_cstr(variant->name)).value);
  }
  sp_prompt_success(app->prompt, cfmt(mem, "{} up at {}", sp_fmt_cstr(variant->name), sp_fmt_str(winvm_ip(vm, variant))));
  return SP_CLI_OK;
}

static sp_cli_result_t run_up(sp_cli_t* cli) {
  app_t app = sp_zero;
  try(init(cli, &app));
  const winvm_variant_t* variant = &winvm_variants[args.variant.value];
  try(begin(cli, &app, "winvm up"));
  sp_cli_result_t result = up(cli, &app, variant);
  sp_prompt_end(app.prompt);
  return result;
}

static sp_cli_result_t run_down(sp_cli_t* cli) {
  app_t app = sp_zero;
  try(init(cli, &app));
  winvm_t* vm = &app.vm;
  sp_mem_t mem = vm->mem;
  const winvm_variant_t* variant = &winvm_variants[args.variant.value];
  try(begin(cli, &app, "winvm down"));

  if (!winvm_running(vm, variant)) {
    sp_prompt_outro(app.prompt, cfmt(mem, "{} is already off", sp_fmt_cstr(variant->name)));
    sp_prompt_end(app.prompt);
    return SP_CLI_OK;
  }
  winvm_shutdown(vm, variant);
  s32 status = tail_trace(mem, app.prompt, cfmt(mem, "shut down {}", sp_fmt_cstr(variant->name)),
    winvm_log(vm, cfmt(mem, "down-{}", sp_fmt_cstr(variant->name))), winvm_wait_off_config(vm, variant, 180));
  if (status) {
    winvm_destroy(vm, variant);
  }
  sp_prompt_outro(app.prompt, cfmt(mem, "{} is down", sp_fmt_cstr(variant->name)));
  sp_prompt_end(app.prompt);
  return SP_CLI_OK;
}

static sp_cli_result_t run_reset(sp_cli_t* cli) {
  app_t app = sp_zero;
  try(init(cli, &app));
  winvm_t* vm = &app.vm;
  const winvm_variant_t* variant = &winvm_variants[args.variant.value];
  try(begin(cli, &app, "winvm reset"));

  winvm_destroy(vm, variant);
  winvm_undefine(vm, variant);
  sp_fs_remove_file(winvm_work(vm, variant));
  sp_cli_result_t result = up(cli, &app, variant);
  sp_prompt_end(app.prompt);
  return result;
}

static sp_cli_result_t run_ssh(sp_cli_t* cli) {
  app_t app = sp_zero;
  try(init(cli, &app));
  winvm_t* vm = &app.vm;
  sp_mem_t mem = vm->mem;
  const winvm_variant_t* variant = &winvm_variants[args.variant.value];
  if (!winvm_running(vm, variant)) {
    return sp_cli_set_error(cli, sp_fmt(mem, "{} is not running; run winvm up {}", sp_fmt_cstr(variant->name), sp_fmt_cstr(variant->name)).value);
  }
  sp_ps_t ps = sp_ps_create(mem, winvm_ssh_config(vm, variant, SP_NULLPTR));
  if (!ps.os) {
    return sp_cli_set_error_c(cli, "failed to launch ssh");
  }
  sp_ps_wait(&ps);
  sp_ps_free(&ps);
  return SP_CLI_OK;
}

static sp_cli_result_t run_list(sp_cli_t* cli) {
  app_t app = sp_zero;
  try(init(cli, &app));
  winvm_t* vm = &app.vm;
  sp_mem_t mem = vm->mem;

  u32 width = 0;
  sp_for(it, winvm_num_variants) {
    width = sp_max(width, sp_cstr_as_str(winvm_variants[it].name).len);
  }

  sp_for(it, winvm_num_variants) {
    const winvm_variant_t* variant = &winvm_variants[it];
    sp_str_t state = winvm_state(vm, variant);
    sp_str_t status;
    if (!sp_str_empty(state)) {
      status = sp_fmt(mem, "{} {}", sp_fmt_str(state), sp_fmt_str(winvm_ip(vm, variant))).value;
    }
    else if (sp_fs_is_file(winvm_image(vm, variant))) {
      status = sp_str_lit("built");
    }
    else {
      status = sp_str_lit("not built");
    }
    sp_log("{:<$ .yellow}  {:<$ }  {}",
      sp_fmt_uint(width), sp_fmt_cstr(variant->name),
      sp_fmt_uint(24), sp_fmt_str(status),
      sp_fmt_str(winvm_variant_summary(mem, variant)));
  }
  return SP_CLI_OK;
}

s32 main(s32 num_args, const c8** argv) {
  sp_cli_arg_t one = {
    .name = "variant",
    .arity = SP_CLI_ARG_REQUIRED,
    .kind = SP_CLI_OPT_CHOICE,
    .summary = "which variant to act on",
    .ptr = &args.variant,
  };
  sp_cli_arg_t many = {
    .name = "variant",
    .arity = SP_CLI_ARG_REST,
    .kind = SP_CLI_OPT_CSTR,
    .summary = "variants to build",
  };
  sp_for(it, winvm_num_variants) {
    sp_cli_choice_t choice = { .name = winvm_variants[it].name, .value = (s32)it };
    one.choices[it] = choice;
    many.choices[it] = choice;
  }

  sp_cli_cmd_t golden = {
    .name = "golden",
    .summary = "Build the golden base by flattening the origin snapshot (no root)",
    .handler = run_golden,
  };
  sp_cli_cmd_t build = {
    .name = "build",
    .summary = "Provision one or more variant images off the golden",
    .args = { many },
    .handler = run_build,
  };
  sp_cli_cmd_t up_cmd = { .name = "up", .summary = "Start a variant on a fresh disposable overlay", .args = { one }, .handler = run_up };
  sp_cli_cmd_t down = { .name = "down", .summary = "Shut a variant down (keeps its overlay)", .args = { one }, .handler = run_down };
  sp_cli_cmd_t reset = { .name = "reset", .summary = "Discard a variant's overlay and start it clean", .args = { one }, .handler = run_reset };
  sp_cli_cmd_t ssh = { .name = "ssh", .summary = "Open an interactive shell on a running variant", .args = { one }, .handler = run_ssh };
  sp_cli_cmd_t list = { .name = "list", .summary = "List variants and their state", .handler = run_list };

  sp_cli_cmd_t root = {
    .name = "winvm",
    .summary = "Provision and run Windows toolchain VMs off a golden snapshot",
    .commands = { &golden, &build, &up_cmd, &down, &reset, &ssh, &list },
  };

  return sp_cli_main((sp_cli_desc_t) {
    .root = &root,
    .num_args = num_args,
    .args = argv,
  });
}
