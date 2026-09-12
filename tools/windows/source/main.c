#include "sp.h"
#include "sp/sp_cli.h"
#include "sp/sp_prompt.h"

#include "tail/tail.h"
#include "winvm/winvm.h"
#include "variant/variant.h"

#define try(expr) do { sp_cli_result_t _e = (expr); if (_e) return _e; } while (0)
#define cfmt(mem, ...) sp_str_to_cstr(mem, sp_fmt(mem, __VA_ARGS__).value)

#define WINVM_WIN_STORE "build/x86_64-windows-gnu/mingw"

typedef struct {
  winvm_t vm;
  sp_prompt_ctx_t* prompt;
  sp_da(sp_str_t) failures;
} app_t;

typedef enum {
  STEP_OK,
  STEP_FAILED,
  STEP_CANCELLED,
} step_t;

typedef struct {
  sp_str_t spn;
  sp_str_t src;
  sp_str_t bins;
  sp_str_t wintest;
  const c8* filter;
} suite_t;

static struct {
  sp_cli_choice_t variant;
  const c8* filter;
} args;

static sp_cli_result_t init(sp_cli_t* cli, app_t* app) {
  sp_mem_t mem = sp_mem_heap_as_allocator(sp_mem_heap_new());
  app->failures = sp_da_new(mem, sp_str_t);
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

static sp_cli_result_t built(sp_cli_t* cli, app_t* app, const winvm_variant_t* variant) {
  if (sp_fs_is_file(winvm_image(&app->vm, variant))) {
    return SP_CLI_OK;
  }
  return sp_cli_set_error(cli, sp_fmt(app->vm.mem, "{} is not built; run winvm build {}", sp_fmt_cstr(variant->name), sp_fmt_cstr(variant->name)).value);
}

static sp_cli_result_t begin(sp_cli_t* cli, app_t* app, const c8* intro) {
  app->prompt = sp_prompt_begin(app->vm.mem);
  if (!app->prompt) {
    return sp_cli_set_error_c(cli, "winvm needs an interactive terminal");
  }
  sp_prompt_intro(app->prompt, intro);
  return SP_CLI_OK;
}

static step_t cancelled(app_t* app) {
  sp_prompt_cancel(app->prompt, "cancelled");
  return STEP_CANCELLED;
}

static step_t failed(app_t* app, const c8* message) {
  sp_prompt_error(app->prompt, message);
  sp_da_push(app->failures, sp_cstr_as_str(message));
  return STEP_FAILED;
}

static s32 trace(app_t* app, const c8* title, sp_str_t log, sp_ps_config_t config) {
  return tail_trace(app->vm.mem, app->prompt, title, log, config);
}

static sp_cli_result_t finish(sp_cli_t* cli, app_t* app, step_t step, const c8* success) {
  sp_mem_t mem = app->vm.mem;
  if (step == STEP_CANCELLED) {
    sp_prompt_end(app->prompt);
    return sp_cli_set_error_c(cli, "cancelled");
  }
  if (sp_da_empty(app->failures)) {
    sp_prompt_outro(app->prompt, success);
    sp_prompt_end(app->prompt);
    return SP_CLI_OK;
  }

  sp_prompt_note_t note = sp_prompt_note_new(mem, "failures");
  sp_da_for(app->failures, it) {
    sp_prompt_note_line(&note, sp_str_to_cstr(mem, app->failures[it]));
  }
  sp_prompt_note_ex(app->prompt, note);
  const c8* summary = cfmt(mem, "{} failed", sp_fmt_uint(sp_da_size(app->failures)));
  sp_prompt_outro(app->prompt, summary);
  sp_prompt_end(app->prompt);
  return sp_cli_set_error_c(cli, summary);
}

static step_t golden_session(app_t* app) {
  winvm_t* vm = &app->vm;
  sp_mem_t mem = vm->mem;
  sp_str_t origin = sp_fs_join_path(mem, vm->paths.dir, sp_str_lit("origin.qcow2"));

  sp_str_t log = winvm_log(vm, "golden-download");
  s32 status = trace(app, cfmt(mem, "vol-download {} from pool {}", sp_fmt_str(vm->cfg.origin), sp_fmt_str(vm->cfg.pool)),
    log, winvm_voldownload_config(vm, origin));
  if (sp_prompt_cancelled(app->prompt)) {
    sp_fs_remove_file(origin);
    return cancelled(app);
  }
  if (status) {
    return failed(app, cfmt(mem, "vol-download failed ({})", sp_fmt_str(log)));
  }

  sp_fs_remove_file(vm->paths.golden);
  log = winvm_log(vm, "golden-convert");
  status = trace(app, cfmt(mem, "flatten snapshot {} into golden", sp_fmt_str(vm->cfg.snapshot)),
    log, winvm_convert_config(vm, origin, vm->paths.golden));
  if (sp_prompt_cancelled(app->prompt)) {
    sp_fs_remove_file(vm->paths.golden);
    return cancelled(app);
  }
  if (status) {
    return failed(app, cfmt(mem, "failed to flatten {} into {.cyan} ({})", sp_fmt_str(vm->cfg.snapshot), sp_fmt_str(vm->paths.golden), sp_fmt_str(log)));
  }

  winvm_seal(vm, vm->paths.golden);
  sp_fs_remove_file(origin);
  return STEP_OK;
}

static sp_cli_result_t run_golden(sp_cli_t* cli) {
  app_t app = sp_zero;
  try(init(cli, &app));
  try(begin(cli, &app, "winvm golden"));
  step_t step = golden_session(&app);
  return finish(cli, &app, step, cfmt(app.vm.mem, "golden ready at {}", sp_fmt_str(app.vm.paths.golden)));
}

static step_t boot(app_t* app, const winvm_variant_t* variant, sp_str_t disk) {
  winvm_t* vm = &app->vm;
  sp_mem_t mem = vm->mem;
  const c8* name = variant->name;

  winvm_undefine(vm, variant);
  if (winvm_lease(vm, variant)) {
    return failed(app, cfmt(mem, "failed to lease {} for {} on network {}", sp_fmt_str(winvm_ip(vm, variant)), sp_fmt_cstr(name), sp_fmt_str(vm->cfg.network)));
  }
  if (winvm_boot(vm, variant, disk)) {
    return failed(app, cfmt(mem, "failed to define and start {.cyan}", sp_fmt_str(winvm_domain(vm, variant))));
  }

  sp_str_t log = winvm_log(vm, cfmt(mem, "boot-{}", sp_fmt_cstr(name)));
  s32 status = trace(app, cfmt(mem, "boot {}", sp_fmt_cstr(name)), log, winvm_wait_ssh_config(vm, variant, 360));
  if (sp_prompt_cancelled(app->prompt)) {
    return cancelled(app);
  }
  if (status) {
    return failed(app, cfmt(mem, "{} never came up on ssh ({})", sp_fmt_cstr(name), sp_fmt_str(log)));
  }
  sp_prompt_success(app->prompt, cfmt(mem, "{} up at {}", sp_fmt_cstr(name), sp_fmt_str(winvm_ip(vm, variant))));
  return STEP_OK;
}

static step_t fresh(app_t* app, const winvm_variant_t* variant) {
  winvm_t* vm = &app->vm;
  winvm_destroy(vm, variant);
  winvm_undefine(vm, variant);

  sp_str_t work = winvm_work(vm, variant);
  if (winvm_overlay(vm, winvm_image(vm, variant), work)) {
    return failed(app, cfmt(vm->mem, "failed to create work overlay {.cyan}", sp_fmt_str(work)));
  }
  return boot(app, variant, work);
}

static void teardown(app_t* app, const winvm_variant_t* variant) {
  winvm_t* vm = &app->vm;
  winvm_destroy(vm, variant);
  winvm_undefine(vm, variant);
  sp_fs_remove_file(winvm_work(vm, variant));
}

static step_t up(app_t* app, const winvm_variant_t* variant) {
  winvm_t* vm = &app->vm;
  sp_mem_t mem = vm->mem;

  if (winvm_running(vm, variant)) {
    sp_prompt_success(app->prompt, cfmt(mem, "{} already up at {}", sp_fmt_cstr(variant->name), sp_fmt_str(winvm_ip(vm, variant))));
    return STEP_OK;
  }
  sp_str_t work = winvm_work(vm, variant);
  if (!sp_fs_is_file(work) && winvm_overlay(vm, winvm_image(vm, variant), work)) {
    return failed(app, cfmt(mem, "failed to create work overlay {.cyan}", sp_fmt_str(work)));
  }
  return boot(app, variant, work);
}

static step_t down(app_t* app, const winvm_variant_t* variant) {
  winvm_t* vm = &app->vm;
  sp_mem_t mem = vm->mem;
  const c8* name = variant->name;

  if (!winvm_running(vm, variant)) {
    sp_prompt_info(app->prompt, cfmt(mem, "{} is already off", sp_fmt_cstr(name)));
    return STEP_OK;
  }
  winvm_shutdown(vm, variant);
  s32 status = trace(app, cfmt(mem, "shut down {}", sp_fmt_cstr(name)),
    winvm_log(vm, cfmt(mem, "down-{}", sp_fmt_cstr(name))), winvm_wait_off_config(vm, variant, 180));
  if (sp_prompt_cancelled(app->prompt)) {
    return cancelled(app);
  }
  if (status) {
    winvm_destroy(vm, variant);
    sp_prompt_warn(app->prompt, cfmt(mem, "{} did not power off in time; destroyed it", sp_fmt_cstr(name)));
  }
  sp_prompt_success(app->prompt, cfmt(mem, "{} is down", sp_fmt_cstr(name)));
  return STEP_OK;
}

static step_t provision(app_t* app, const winvm_variant_t* variant) {
  winvm_t* vm = &app->vm;
  sp_mem_t mem = vm->mem;
  const c8* name = variant->name;

  winvm_step_t steps[1 + WINVM_MAX_STEPS];
  u32 count = 0;
  steps[count++] = (winvm_step_t) { .recipe = "common" };
  sp_carr_for_until(variant->steps, it, variant->steps[it].recipe) {
    steps[count++] = variant->steps[it];
  }

  sp_for(it, count) {
    winvm_step_t step = steps[it];
    if (winvm_upload_recipe(vm, variant, step)) {
      return failed(app, cfmt(mem, "failed to copy recipe {.cyan} to {}", sp_fmt_cstr(step.recipe), sp_fmt_cstr(name)));
    }
    const c8* title = step.arg
      ? cfmt(mem, "{} {} in {}", sp_fmt_cstr(step.recipe), sp_fmt_cstr(step.arg), sp_fmt_cstr(name))
      : cfmt(mem, "{} in {}", sp_fmt_cstr(step.recipe), sp_fmt_cstr(name));
    sp_str_t log = winvm_log(vm, cfmt(mem, "provision-{}-{}", sp_fmt_cstr(name), sp_fmt_cstr(step.recipe)));
    s32 status = trace(app, title, log, winvm_recipe_config(vm, variant, step));
    if (sp_prompt_cancelled(app->prompt)) {
      return cancelled(app);
    }
    if (status) {
      return failed(app, cfmt(mem, "recipe {.cyan} failed in {} ({})", sp_fmt_cstr(step.recipe), sp_fmt_cstr(name), sp_fmt_str(log)));
    }
  }
  return STEP_OK;
}

static step_t build_one(app_t* app, const winvm_variant_t* variant) {
  winvm_t* vm = &app->vm;
  sp_mem_t mem = vm->mem;

  winvm_destroy(vm, variant);
  winvm_undefine(vm, variant);
  sp_str_t image = winvm_image(vm, variant);
  if (winvm_overlay(vm, vm->paths.golden, image)) {
    return failed(app, cfmt(mem, "failed to create overlay {.cyan}", sp_fmt_str(image)));
  }

  step_t step = boot(app, variant, image);
  if (step == STEP_OK) {
    step = provision(app, variant);
  }
  if (step == STEP_OK) {
    step = down(app, variant);
  }
  if (step != STEP_OK) {
    winvm_destroy(vm, variant);
    winvm_undefine(vm, variant);
    return step;
  }

  winvm_undefine(vm, variant);
  winvm_seal(vm, image);
  sp_prompt_success(app->prompt, cfmt(mem, "built {} -> {}", sp_fmt_cstr(variant->name), sp_fmt_str(image)));
  return STEP_OK;
}

static step_t build_session(app_t* app, sp_da(const winvm_variant_t*) selected) {
  sp_da_for(selected, it) {
    if (build_one(app, selected[it]) == STEP_CANCELLED) {
      return STEP_CANCELLED;
    }
  }
  return STEP_OK;
}

static sp_cli_result_t select_variants(sp_cli_t* cli, sp_mem_t mem, sp_da(const winvm_variant_t*)* selected) {
  for (const c8** it = cli->rest; *it; it++) {
    const winvm_variant_t* variant = winvm_variant_find(*it);
    if (!variant) {
      return sp_cli_set_error(cli, sp_fmt(mem, "unknown variant {.cyan}; see winvm list", sp_fmt_cstr(*it)).value);
    }
    sp_da_push(*selected, variant);
  }
  return SP_CLI_OK;
}

static sp_cli_result_t run_build(sp_cli_t* cli) {
  app_t app = sp_zero;
  try(init(cli, &app));
  sp_mem_t mem = app.vm.mem;

  sp_da(const winvm_variant_t*) selected = sp_da_new(mem, const winvm_variant_t*);
  try(select_variants(cli, mem, &selected));
  if (sp_da_empty(selected)) {
    return sp_cli_set_error_c(cli, "name at least one variant to build; see winvm list");
  }
  if (!sp_fs_is_file(app.vm.paths.golden)) {
    return sp_cli_set_error(cli, sp_fmt(mem, "no golden at {.cyan}; run winvm golden", sp_fmt_str(app.vm.paths.golden)).value);
  }

  try(begin(cli, &app, "winvm build"));
  step_t step = build_session(&app, selected);
  return finish(cli, &app, step, "all variants built");
}

static sp_cli_result_t run_up(sp_cli_t* cli) {
  app_t app = sp_zero;
  try(init(cli, &app));
  const winvm_variant_t* variant = &winvm_variants[args.variant.value];
  try(built(cli, &app, variant));
  try(begin(cli, &app, "winvm up"));
  step_t step = up(&app, variant);
  return finish(cli, &app, step, cfmt(app.vm.mem, "{} is up", sp_fmt_cstr(variant->name)));
}

static sp_cli_result_t run_down(sp_cli_t* cli) {
  app_t app = sp_zero;
  try(init(cli, &app));
  const winvm_variant_t* variant = &winvm_variants[args.variant.value];
  try(begin(cli, &app, "winvm down"));
  step_t step = down(&app, variant);
  return finish(cli, &app, step, cfmt(app.vm.mem, "{} is down", sp_fmt_cstr(variant->name)));
}

static sp_cli_result_t run_reset(sp_cli_t* cli) {
  app_t app = sp_zero;
  try(init(cli, &app));
  const winvm_variant_t* variant = &winvm_variants[args.variant.value];
  try(built(cli, &app, variant));
  try(begin(cli, &app, "winvm reset"));
  step_t step = fresh(&app, variant);
  return finish(cli, &app, step, cfmt(app.vm.mem, "{} is up on a fresh overlay", sp_fmt_cstr(variant->name)));
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
  sp_ps_t ps = sp_ps_create(mem, winvm_shell_config(vm, variant));
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

static sp_str_t spn_bin(sp_mem_t mem) {
  sp_str_t env = sp_os_env_get(sp_str_lit("SPN_BIN"));
  if (!sp_str_empty(env)) {
    return env;
  }
  return sp_fs_join_path(mem, sp_fs_parent_path(sp_fs_get_exe_path(mem)), sp_str_lit("spn"));
}

static sp_ps_config_t host_config(sp_mem_t mem, sp_str_t repo, sp_str_t command, const c8* const* rest) {
  sp_ps_config_t config = { .command = command, .cwd = repo, .io.err = { .mode = SP_PS_IO_MODE_REDIRECT } };
  for (u32 it = 0; rest[it]; it++) {
    sp_ps_config_add_arg(mem, &config, sp_cstr_as_str(rest[it]));
  }
  return config;
}

static step_t host_step(app_t* app, const c8* title, const c8* log_name, sp_str_t command, const c8* const* rest) {
  winvm_t* vm = &app->vm;
  sp_mem_t mem = vm->mem;
  sp_str_t log = winvm_log(vm, log_name);
  s32 status = trace(app, title, log, host_config(mem, vm->paths.repo, command, rest));
  if (sp_prompt_cancelled(app->prompt)) {
    return cancelled(app);
  }
  if (status) {
    return failed(app, cfmt(mem, "{} failed ({})", sp_fmt_cstr(title), sp_fmt_str(log)));
  }
  return STEP_OK;
}

static sp_str_t worktree_ref(sp_mem_t mem, sp_str_t repo) {
  sp_ps_config_t cfg = { .command = sp_str_lit("git"), .cwd = repo };
  sp_ps_config_add_arg(mem, &cfg, sp_str_lit("stash"));
  sp_ps_config_add_arg(mem, &cfg, sp_str_lit("create"));
  sp_ps_output_t out = sp_ps_run(mem, cfg);
  sp_str_t sha = sp_str_trim(out.out);
  return (out.status.exit_code || sp_str_empty(sha)) ? sp_str_lit("HEAD") : sha;
}

static step_t prepare(app_t* app, suite_t suite) {
  winvm_t* vm = &app->vm;
  sp_mem_t mem = vm->mem;

  const c8* a_spn[] = { "build", "-p", "mingw", SP_NULLPTR };
  step_t step = host_step(app, "cross-build spn -p mingw", "build-spn", suite.spn, a_spn);
  if (step != STEP_OK) {
    return step;
  }

  const c8* a_test[] = { "build", "-p", "mingw", "--test", "integration", SP_NULLPTR };
  step = host_step(app, "cross-build integration -p mingw", "build-suite", suite.spn, a_test);
  if (step != STEP_OK) {
    return step;
  }

  sp_fs_create_dir(sp_fs_parent_path(suite.src));
  sp_str_t ref = worktree_ref(mem, vm->paths.repo);
  const c8* a_src[] = { "archive", "--format=tar.gz", "-o", sp_str_to_cstr(mem, suite.src), sp_str_to_cstr(mem, ref), SP_NULLPTR };
  step = host_step(app, "package source", "package-src", sp_str_lit("git"), a_src);
  if (step != STEP_OK) {
    return step;
  }

  const c8* a_bins[] = {
    "-czf", sp_str_to_cstr(mem, suite.bins), "-C", sp_str_to_cstr(mem, vm->paths.repo),
    WINVM_WIN_STORE "/spn.exe", WINVM_WIN_STORE "/test/integration.exe", SP_NULLPTR,
  };
  return host_step(app, "package binaries", "package-bins", sp_str_lit("tar"), a_bins);
}

static step_t read_probes(app_t* app, const winvm_variant_t* variant, sp_da(winvm_probe_t)* probes) {
  winvm_t* vm = &app->vm;
  sp_mem_t mem = vm->mem;
  winvm_probes_result_t read = winvm_probes_read(vm, variant, probes);
  switch (read.err) {
    case WINVM_PROBES_OK: {
      return STEP_OK;
    }
    case WINVM_PROBES_ERR_TREE: {
      return failed(app, cfmt(mem, "failed to read the probe tree at {.cyan}", sp_fmt_str(read.path)));
    }
    case WINVM_PROBES_ERR_MANIFEST: {
      return failed(app, cfmt(mem, "bad probe manifest at {.cyan}", sp_fmt_str(read.path)));
    }
  }
  SP_UNREACHABLE_RETURN(STEP_FAILED);
}

static step_t run_lanes(app_t* app, const winvm_variant_t* variant, suite_t suite, sp_da(winvm_probe_t)* probes) {
  winvm_t* vm = &app->vm;
  sp_mem_t mem = vm->mem;
  const c8* name = variant->name;

  sp_str_t payload[] = { suite.src, suite.bins, suite.wintest };
  sp_str_t log = winvm_log(vm, cfmt(mem, "ship-{}", sp_fmt_cstr(name)));
  s32 status = trace(app, cfmt(mem, "ship payload to {}", sp_fmt_cstr(name)), log,
    winvm_upload_config(vm, variant, payload, sp_carr_len(payload), vm->guest.home));
  if (sp_prompt_cancelled(app->prompt)) {
    return cancelled(app);
  }
  if (status) {
    return failed(app, cfmt(mem, "failed to ship payload to {} ({})", sp_fmt_cstr(name), sp_fmt_str(log)));
  }

  sp_carr_for_until(variant->lanes, it, variant->lanes[it]) {
    const c8* lane = variant->lanes[it];
    log = winvm_log(vm, cfmt(mem, "test-{}-{}", sp_fmt_cstr(name), sp_fmt_cstr(lane)));
    status = trace(app, cfmt(mem, "test {} in {}", sp_fmt_cstr(lane), sp_fmt_cstr(name)), log,
      winvm_test_config(vm, variant, lane, suite.filter));
    if (sp_prompt_cancelled(app->prompt)) {
      return cancelled(app);
    }
    if (status) {
      failed(app, cfmt(mem, "FAIL {} in {} ({})", sp_fmt_cstr(lane), sp_fmt_cstr(name), sp_fmt_str(log)));
    }
    else {
      sp_prompt_success(app->prompt, cfmt(mem, "PASS {} in {}", sp_fmt_cstr(lane), sp_fmt_cstr(name)));
    }
  }

  sp_str_t local = sp_fs_join_path(mem, vm->paths.probes, sp_cstr_as_str(name));
  log = winvm_log(vm, cfmt(mem, "probes-{}", sp_fmt_cstr(name)));
  status = trace(app, cfmt(mem, "collect probes from {}", sp_fmt_cstr(name)), log,
    winvm_download_config(vm, variant, vm->guest.probes, local));
  if (sp_prompt_cancelled(app->prompt)) {
    return cancelled(app);
  }
  if (status) {
    return failed(app, cfmt(mem, "failed to collect probes from {} ({})", sp_fmt_cstr(name), sp_fmt_str(log)));
  }

  return read_probes(app, variant, probes);
}

static step_t test_variant(app_t* app, const winvm_variant_t* variant, suite_t suite, sp_da(winvm_probe_t)* probes) {
  step_t step = fresh(app, variant);
  if (step == STEP_OK) {
    step = run_lanes(app, variant, suite, probes);
  }
  teardown(app, variant);
  return step;
}

static step_t run_probes(app_t* app, sp_da(winvm_probe_t) probes) {
  winvm_t* vm = &app->vm;
  sp_mem_t mem = vm->mem;

  sp_str_t payload[] = { sp_fs_join_path(mem, vm->paths.recipes, sp_str_lit("barerun.ps1")), vm->paths.probes };
  sp_str_t log = winvm_log(vm, "ship-base");
  s32 status = trace(app, "ship probes to base", log,
    winvm_upload_config(vm, winvm_bare, payload, sp_carr_len(payload), vm->guest.home));
  if (sp_prompt_cancelled(app->prompt)) {
    return cancelled(app);
  }
  if (status) {
    return failed(app, cfmt(mem, "failed to ship probes to base ({})", sp_fmt_str(log)));
  }

  sp_da_for(probes, it) {
    winvm_probe_t probe = probes[it];
    const c8* id = cfmt(mem, "{}/{}/{}", sp_fmt_cstr(probe.variant->name), sp_fmt_str(probe.lane), sp_fmt_str(probe.name));
    log = winvm_log(vm, cfmt(mem, "bare-{}-{}-{}", sp_fmt_cstr(probe.variant->name), sp_fmt_str(probe.lane), sp_fmt_str(probe.name)));
    status = tail_trace_expect(mem, app->prompt, cfmt(mem, "bare-run {}", sp_fmt_cstr(id)), log,
      winvm_probe_config(vm, winvm_bare, probe), winvm_probe_status(probe.expect));
    if (sp_prompt_cancelled(app->prompt)) {
      return cancelled(app);
    }
    winvm_probe_outcome_t outcome = winvm_probe_outcome(status);
    if (outcome == probe.expect) {
      sp_prompt_success(app->prompt, cfmt(mem, "PASS bare {}: {}", sp_fmt_cstr(id), sp_fmt_cstr(winvm_probe_outcome_name(outcome))));
    }
    else {
      failed(app, cfmt(mem, "FAIL bare {}: expected {}, got {} ({})", sp_fmt_cstr(id),
        sp_fmt_cstr(winvm_probe_outcome_name(probe.expect)), sp_fmt_cstr(winvm_probe_outcome_name(outcome)), sp_fmt_str(log)));
    }
  }
  return STEP_OK;
}

static step_t bare_run(app_t* app, sp_da(winvm_probe_t) probes) {
  if (sp_da_empty(probes)) {
    sp_prompt_info(app->prompt, "no probes were staged; skipping the bare run");
    return STEP_OK;
  }
  step_t step = fresh(app, winvm_bare);
  if (step == STEP_OK) {
    step = run_probes(app, probes);
  }
  teardown(app, winvm_bare);
  return step;
}

static step_t test_session(app_t* app, sp_da(const winvm_variant_t*) selected, suite_t suite) {
  winvm_t* vm = &app->vm;
  sp_fs_remove_dir(vm->paths.probes);
  sp_fs_create_dir(vm->paths.probes);

  step_t step = prepare(app, suite);
  if (step != STEP_OK) {
    return step;
  }

  sp_da(winvm_probe_t) probes = sp_da_new(vm->mem, winvm_probe_t);
  sp_da_for(selected, it) {
    if (test_variant(app, selected[it], suite, &probes) == STEP_CANCELLED) {
      return STEP_CANCELLED;
    }
  }
  return bare_run(app, probes);
}

static step_t bare_session(app_t* app) {
  winvm_t* vm = &app->vm;
  sp_mem_t mem = vm->mem;

  sp_da(sp_fs_entry_t) dirs = sp_zero;
  if (sp_fs_collect(mem, vm->paths.probes, &dirs)) {
    return failed(app, cfmt(mem, "failed to read the probe store at {.cyan}", sp_fmt_str(vm->paths.probes)));
  }

  sp_da(winvm_probe_t) probes = sp_da_new(mem, winvm_probe_t);
  sp_da_for(dirs, it) {
    const winvm_variant_t* variant = winvm_variant_find(sp_str_to_cstr(mem, dirs[it].name));
    if (!variant) {
      return failed(app, cfmt(mem, "{.cyan} in the probe store is not a variant", sp_fmt_str(dirs[it].path)));
    }
    step_t step = read_probes(app, variant, &probes);
    if (step != STEP_OK) {
      return step;
    }
  }
  return bare_run(app, probes);
}

static sp_cli_result_t run_bare(sp_cli_t* cli) {
  app_t app = sp_zero;
  try(init(cli, &app));
  try(built(cli, &app, winvm_bare));
  try(begin(cli, &app, "winvm bare"));
  step_t step = bare_session(&app);
  return finish(cli, &app, step, "all probes passed");
}

static sp_cli_result_t run_test(sp_cli_t* cli) {
  app_t app = sp_zero;
  try(init(cli, &app));
  winvm_t* vm = &app.vm;
  sp_mem_t mem = vm->mem;

  sp_da(const winvm_variant_t*) selected = sp_da_new(mem, const winvm_variant_t*);
  try(select_variants(cli, mem, &selected));
  sp_da_for(selected, it) {
    if (!selected[it]->lanes[0]) {
      return sp_cli_set_error(cli, sp_fmt(mem, "{.cyan} hosts no test lanes", sp_fmt_cstr(selected[it]->name)).value);
    }
  }
  if (sp_da_empty(selected)) {
    sp_for(it, winvm_num_variants) {
      if (winvm_variants[it].lanes[0]) {
        sp_da_push(selected, &winvm_variants[it]);
      }
    }
  }
  sp_da_for(selected, it) {
    try(built(cli, &app, selected[it]));
  }
  try(built(cli, &app, winvm_bare));

  sp_str_t spn = spn_bin(mem);
  if (!sp_fs_is_file(spn)) {
    return sp_cli_set_error(cli, sp_fmt(mem, "no spn at {.cyan}; run spn build spn, or point SPN_BIN at one", sp_fmt_str(spn)).value);
  }
  suite_t suite = {
    .spn = sp_fs_canonicalize_path(mem, spn),
    .src = sp_fs_join_path(mem, vm->paths.repo, sp_str_lit("build/winvm/spn-src.tar.gz")),
    .bins = sp_fs_join_path(mem, vm->paths.repo, sp_str_lit("build/winvm/spn-bins.tar.gz")),
    .wintest = sp_fs_join_path(mem, vm->paths.recipes, sp_str_lit("wintest.ps1")),
    .filter = args.filter ? args.filter : "*",
  };

  try(begin(cli, &app, "winvm test"));
  step_t step = test_session(&app, selected, suite);
  return finish(cli, &app, step, "all lanes and probes passed");
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
  sp_cli_arg_t some = {
    .name = "variant",
    .arity = SP_CLI_ARG_REST,
    .kind = SP_CLI_OPT_CSTR,
    .summary = "variants to test; default is every variant with lanes",
  };
  sp_for(it, winvm_num_variants) {
    sp_cli_choice_t choice = { .name = winvm_variants[it].name, .value = (s32)it };
    one.choices[it] = choice;
    many.choices[it] = choice;
    some.choices[it] = choice;
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
  sp_cli_cmd_t up_cmd = { .name = "up", .summary = "Start a variant on its work overlay, creating one off the image if missing", .args = { one }, .handler = run_up };
  sp_cli_cmd_t down = { .name = "down", .summary = "Shut a variant down gracefully (keeps its work overlay)", .args = { one }, .handler = run_down };
  sp_cli_cmd_t reset = { .name = "reset", .summary = "Discard a variant's work overlay and start it clean", .args = { one }, .handler = run_reset };
  sp_cli_cmd_t ssh = { .name = "ssh", .summary = "Open an interactive shell on a running variant", .args = { one }, .handler = run_ssh };
  sp_cli_cmd_t list = { .name = "list", .summary = "List variants and their state", .handler = run_list };
  sp_cli_cmd_t bare = { .name = "bare", .summary = "Bare-run the probes already collected in build/winvm/probes on base", .handler = run_bare };
  sp_cli_cmd_t test = {
    .name = "test",
    .summary = "Cross-build the suite, run it per lane on fresh variants, then bare-run the staged probes on base",
    .opts = {
      {
        .brief = 'f',
        .name = "filter",
        .kind = SP_CLI_OPT_CSTR,
        .summary = "only run test cases matching this pattern",
        .placeholder = "PATTERN",
        .ptr = &args.filter,
      },
    },
    .args = { some },
    .handler = run_test,
  };

  sp_cli_cmd_t root = {
    .name = "winvm",
    .summary = "Provision and run Windows toolchain VMs off a golden snapshot",
    .commands = { &golden, &build, &up_cmd, &down, &reset, &ssh, &list, &test, &bare },
  };

  return sp_cli_main((sp_cli_desc_t) {
    .root = &root,
    .num_args = num_args,
    .args = argv,
  });
}
