#define SP_CLI_IMPLEMENTATION
#include "sp.h"
#include "sp/sp_cli.h"
#include "sp/sp_prompt.h"

#include "docker/docker.h"
#include "tail/tail.h"
#include "variant/variant.h"

typedef struct {
  sp_mem_t mem;
  docker_t docker;
  sp_prompt_ctx_t* prompt;
} smoke_t;

static struct {
  const c8* variant;
  const c8* filter;
} args;

typedef struct {
  const variant_t* variant;
  const c8* lane;
} lane_t;

#define try(expr) do { sp_cli_result_t _e = (expr); if (_e) return _e; } while (0)
#define cfmt(mem, ...) sp_str_to_cstr(mem, sp_fmt(mem, __VA_ARGS__).value)

static sp_cli_result_t init(sp_cli_t* cli, smoke_t* smoke) {
  smoke->mem = sp_mem_os_new();
  sp_mem_t mem = smoke->mem;

  switch (docker_init(&smoke->docker, mem)) {
    case DOCKER_INIT_OK: {
      return SP_CLI_OK;
    }
    case DOCKER_INIT_ERR_REPO: {
      return sp_cli_set_error_c(cli, "not inside the spn repo");
    }
    case DOCKER_INIT_ERR_BINARY: {
      return sp_cli_set_error(cli, sp_fmt(mem, "missing {.cyan}; run {.cyan}", sp_fmt_str(smoke->docker.err.binary.path), sp_fmt_cstr(smoke->docker.err.binary.hint)).value);
    }
    case DOCKER_INIT_ERR_TEMPLATES: {
      return sp_cli_set_error(cli, sp_fmt(mem, "failed to load templates from {.cyan}", sp_fmt_str(smoke->docker.paths.templates)).value);
    }
  }
  SP_UNREACHABLE_RETURN(SP_CLI_ERR);
}

static sp_cli_result_t require(sp_cli_t* cli, smoke_t* smoke, const variant_t* variant) {
  if (docker_require(&smoke->docker, variant)) {
    return SP_CLI_OK;
  }
  return sp_cli_set_error(cli, sp_fmt(smoke->mem, "missing {.cyan}; run {.cyan}", sp_fmt_str(smoke->docker.err.binary.path), sp_fmt_cstr(smoke->docker.err.binary.hint)).value);
}

static sp_cli_result_t begin(sp_cli_t* cli, smoke_t* smoke) {
  smoke->prompt = sp_prompt_begin(smoke->mem);
  if (!smoke->prompt) {
    return sp_cli_set_error_c(cli, "smoke needs an interactive terminal");
  }
  sp_prompt_intro(smoke->prompt, "spn smoke");
  return SP_CLI_OK;
}

static sp_cli_result_t fail(sp_cli_t* cli, smoke_t* smoke, sp_str_t message) {
  sp_prompt_cancel(smoke->prompt, sp_str_to_cstr(smoke->mem, message));
  return sp_cli_set_error(cli, message);
}

static sp_cli_result_t build_image(sp_cli_t* cli, smoke_t* smoke, const variant_t* variant) {
  sp_mem_t mem = smoke->mem;

  switch (docker_render(&smoke->docker, variant)) {
    case DOCKER_RENDER_OK: {
      break;
    }
    case DOCKER_RENDER_ERR_MISSING: {
      return fail(cli, smoke, sp_fmt(mem, "missing template {}", sp_fmt_cstr(variant_template(variant))).value);
    }
    case DOCKER_RENDER_ERR_FAILED: {
      return fail(cli, smoke, sp_fmt(mem, "failed to render template {} with code {}", sp_fmt_cstr(variant_template(variant)), sp_fmt_int(smoke->docker.err.render)).value);
    }
  }

  const c8* title = cfmt(mem, "docker build {}", sp_fmt_cstr(docker_image(&smoke->docker, variant)));
  s32 status = tail_trace(mem, smoke->prompt, title, docker_build(&smoke->docker, variant));
  if (sp_prompt_cancelled(smoke->prompt)) {
    return fail(cli, smoke, sp_str_lit("cancelled"));
  }
  if (status) {
    return fail(cli, smoke, sp_fmt(mem, "docker build failed for {}", sp_fmt_cstr(variant->name)).value);
  }
  return SP_CLI_OK;
}

static sp_cli_result_t shell_session(sp_cli_t* cli, smoke_t* smoke, const variant_t* variant) {
  try(build_image(cli, smoke, variant));
  sp_prompt_outro(smoke->prompt, cfmt(smoke->mem, "dropping into {}", sp_fmt_cstr(variant->name)));
  return SP_CLI_OK;
}

static sp_cli_result_t run_shell(sp_cli_t* cli) {
  smoke_t smoke = sp_zero;
  try(init(cli, &smoke));

  const variant_t* variant = variant_find(args.variant);
  if (!variant) {
    return sp_cli_set_error(cli, sp_fmt(smoke.mem, "unknown variant {.cyan}; see smoke list", sp_fmt_cstr(args.variant)).value);
  }
  try(require(cli, &smoke, variant));

  try(begin(cli, &smoke));
  sp_cli_result_t result = shell_session(cli, &smoke, variant);
  sp_prompt_end(smoke.prompt);
  try(result);

  sp_ps_t ps = sp_ps_create_c(smoke.mem, docker_shell(&smoke.docker, variant));
  if (!ps.os) {
    return sp_cli_set_error_c(cli, "failed to launch docker");
  }
  sp_ps_wait(&ps);
  sp_ps_free(&ps);
  return SP_CLI_OK;
}

static sp_cli_result_t check_session(sp_cli_t* cli, smoke_t* smoke, sp_da(const variant_t*) selected) {
  sp_mem_t mem = smoke->mem;

  u32 failures = 0;
  sp_da_for(selected, it) {
    const variant_t* variant = selected[it];
    try(build_image(cli, smoke, variant));

    const c8* title = cfmt(mem, "check {} (--toolchain {})", sp_fmt_cstr(variant->name), sp_fmt_cstr(toolchain_name(variant->toolchain)));
    s32 status = tail_trace(mem, smoke->prompt, title, docker_check(&smoke->docker, variant));
    if (sp_prompt_cancelled(smoke->prompt)) {
      return fail(cli, smoke, sp_str_lit("cancelled"));
    }
    if (status) {
      failures++;
      sp_prompt_error(smoke->prompt, cfmt(mem, "FAIL {}", sp_fmt_cstr(variant->name)));
    }
    else {
      sp_prompt_success(smoke->prompt, cfmt(mem, "PASS {}", sp_fmt_cstr(variant->name)));
    }
  }

  if (failures) {
    sp_prompt_outro(smoke->prompt, cfmt(mem, "{} variants failed", sp_fmt_uint(failures)));
    return sp_cli_set_error(cli, sp_fmt(mem, "{} variants failed", sp_fmt_uint(failures)).value);
  }
  sp_prompt_outro(smoke->prompt, "all variants passed");
  return SP_CLI_OK;
}

static sp_cli_result_t run_check(sp_cli_t* cli) {
  smoke_t smoke = sp_zero;
  try(init(cli, &smoke));
  sp_mem_t mem = smoke.mem;

  sp_da(const variant_t*) selected = sp_da_new(mem, const variant_t*);
  for (const c8** it = cli->rest; *it; it++) {
    const variant_t* variant = variant_find(*it);
    if (!variant) {
      return sp_cli_set_error(cli, sp_fmt(mem, "unknown variant {.cyan}; see smoke list", sp_fmt_cstr(*it)).value);
    }
    if (variant->toolchain == TOOLCHAIN_NONE) {
      return sp_cli_set_error(cli, sp_fmt(mem, "{.cyan} has no toolchain to check", sp_fmt_cstr(*it)).value);
    }
    sp_da_push(selected, variant);
  }
  if (sp_da_empty(selected)) {
    sp_for(it, num_variants) {
      if (variants[it].toolchain != TOOLCHAIN_NONE) {
        sp_da_push(selected, &variants[it]);
      }
    }
  }
  sp_da_for(selected, it) {
    try(require(cli, &smoke, selected[it]));
  }

  try(begin(cli, &smoke));
  sp_cli_result_t result = check_session(cli, &smoke, selected);
  sp_prompt_end(smoke.prompt);
  return result;
}

static void push_variant_lanes(sp_da(lane_t)* lanes, const variant_t* variant) {
  sp_carr_for_until(variant->lanes, it, variant->lanes[it]) {
    sp_da_push(*lanes, ((lane_t) { .variant = variant, .lane = variant->lanes[it] }));
  }
}

static sp_cli_result_t select_lanes(sp_cli_t* cli, sp_mem_t mem, sp_da(lane_t)* lanes) {
  for (const c8** it = cli->rest; *it; it++) {
    const variant_t* variant = variant_find(*it);
    if (variant) {
      push_variant_lanes(lanes, variant);
      continue;
    }
    const variant_t* host = variant_hosting(*it);
    if (!host) {
      return sp_cli_set_error(cli, sp_fmt(mem, "{.cyan} is neither a variant nor a lane; see smoke list", sp_fmt_cstr(*it)).value);
    }
    sp_da_push(*lanes, ((lane_t) { .variant = host, .lane = *it }));
  }
  if (!sp_da_empty(*lanes)) {
    return SP_CLI_OK;
  }
  sp_for(it, num_variants) {
    sp_carr_for_until(variants[it].lanes, lane, variants[it].lanes[lane]) {
      if (variant_hosting(variants[it].lanes[lane]) == &variants[it]) {
        sp_da_push(*lanes, ((lane_t) { .variant = &variants[it], .lane = variants[it].lanes[lane] }));
      }
    }
  }
  return SP_CLI_OK;
}

static sp_cli_result_t test_session(sp_cli_t* cli, smoke_t* smoke, sp_da(lane_t) lanes, const c8* filter) {
  sp_mem_t mem = smoke->mem;

  u32 failures = 0;
  const variant_t* built = SP_NULLPTR;
  sp_da_for(lanes, it) {
    lane_t lane = lanes[it];
    if (lane.variant != built) {
      try(build_image(cli, smoke, lane.variant));
      built = lane.variant;
    }

    const c8* title = cfmt(mem, "test {} in {}", sp_fmt_cstr(lane.lane), sp_fmt_cstr(lane.variant->name));
    s32 status = tail_trace(mem, smoke->prompt, title, docker_test(&smoke->docker, lane.variant, lane.lane, filter));
    if (sp_prompt_cancelled(smoke->prompt)) {
      return fail(cli, smoke, sp_str_lit("cancelled"));
    }
    if (status) {
      failures++;
      sp_prompt_error(smoke->prompt, cfmt(mem, "FAIL {} in {}", sp_fmt_cstr(lane.lane), sp_fmt_cstr(lane.variant->name)));
    }
    else {
      sp_prompt_success(smoke->prompt, cfmt(mem, "PASS {} in {}", sp_fmt_cstr(lane.lane), sp_fmt_cstr(lane.variant->name)));
    }
  }

  if (failures) {
    sp_prompt_outro(smoke->prompt, cfmt(mem, "{} lanes failed", sp_fmt_uint(failures)));
    return sp_cli_set_error(cli, sp_fmt(mem, "{} lanes failed", sp_fmt_uint(failures)).value);
  }
  sp_prompt_outro(smoke->prompt, "all lanes passed");
  return SP_CLI_OK;
}

static sp_cli_result_t run_test(sp_cli_t* cli) {
  smoke_t smoke = sp_zero;
  try(init(cli, &smoke));
  sp_mem_t mem = smoke.mem;

  sp_da(lane_t) lanes = sp_da_new(mem, lane_t);
  try(select_lanes(cli, mem, &lanes));
  switch (docker_tests_init(&smoke.docker)) {
    case DOCKER_TESTS_OK: {
      break;
    }
    case DOCKER_TESTS_ERR_GIT: {
      return sp_cli_set_error_c(cli, "failed to find the git dir; is git installed?");
    }
    case DOCKER_TESTS_ERR_USER: {
      return sp_cli_set_error_c(cli, "failed to read the current user with id");
    }
    case DOCKER_TESTS_ERR_BINARY: {
      return sp_cli_set_error(cli, sp_fmt(mem, "missing {.cyan}; run {.cyan}", sp_fmt_str(smoke.docker.err.binary.path), sp_fmt_cstr(smoke.docker.err.binary.hint)).value);
    }
  }

  try(begin(cli, &smoke));
  sp_cli_result_t result = test_session(cli, &smoke, lanes, args.filter ? args.filter : "*");
  sp_prompt_end(smoke.prompt);
  return result;
}

static sp_cli_result_t run_list(sp_cli_t* cli) {
  u32 width = 0;
  sp_for(it, num_variants) {
    width = sp_max(width, sp_cstr_as_str(variants[it].name).len);
  }

  sp_mem_t mem = sp_mem_os_new();
  sp_prompt_ctx_t* prompt = sp_prompt_begin(mem);
  if (!prompt) {
    sp_for(it, num_variants) {
      sp_log("{:<$ .yellow} {}", sp_fmt_uint(width), sp_fmt_cstr(variants[it].name), sp_fmt_str(variant_summary(mem, &variants[it])));
    }
    return SP_CLI_OK;
  }

  sp_prompt_note_t note = sp_prompt_note_new(mem, "variants");
  sp_for(it, num_variants) {
    sp_prompt_note_line_fmt(&note, "{:<$ .cyan} {}", sp_fmt_uint(width), sp_fmt_cstr(variants[it].name), sp_fmt_str(variant_summary(mem, &variants[it])));
  }
  sp_prompt_note_ex(prompt, note);
  sp_prompt_end(prompt);
  return SP_CLI_OK;
}

static sp_str_t variant_names(sp_mem_t mem) {
  const c8** names = sp_alloc_n(mem, const c8*, num_variants);
  sp_for(it, num_variants) {
    names[it] = variants[it].name;
  }
  return sp_str_join_cstr_n(mem, names, num_variants, sp_str_lit(", "));
}

s32 main(s32 num_args, const c8** argv) {
  sp_mem_t mem = sp_mem_os_new();
  sp_str_t names = variant_names(mem);

  sp_cli_cmd_t shell = {
    .name = "shell",
    .summary = "Drop into a shell with spn installed",
    .args = {
      {
        .name = "variant",
        .arity = SP_CLI_ARG_REQUIRED,
        .kind = SP_CLI_OPT_CSTR,
        .summary = cfmt(mem, "which container variant: {}", sp_fmt_str(names)),
        .ptr = &args.variant,
      },
    },
    .handler = run_shell,
  };

  sp_cli_cmd_t check = {
    .name = "check",
    .summary = "Smoke test a fresh project build",
    .args = {
      {
        .name = "variant",
        .arity = SP_CLI_ARG_REST,
        .kind = SP_CLI_OPT_CSTR,
        .summary = cfmt(mem, "variants to check: {}", sp_fmt_str(names)),
      },
    },
    .handler = run_check,
  };

  sp_cli_cmd_t test = {
    .name = "test",
    .summary = "Run the integration suite in the container that hosts a lane",
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
    .args = {
      {
        .name = "lane",
        .arity = SP_CLI_ARG_REST,
        .kind = SP_CLI_OPT_CSTR,
        .summary = "lanes to run, or variants to run every lane of; default is every lane once",
      },
    },
    .handler = run_test,
  };

  sp_cli_cmd_t list = {
    .name = "list",
    .summary = "List container variants and the lanes they host",
    .handler = run_list,
  };

  sp_cli_cmd_t root = {
    .name = "smoke",
    .summary = cfmt(mem, "Manual smoke checks for spn across toolchain and libc variants, in docker\nvariants: {}", sp_fmt_str(names)),
    .commands = { &shell, &check, &test, &list },
  };

  return sp_cli_main((sp_cli_desc_t) {
    .root = &root,
    .num_args = num_args,
    .args = argv,
  });
}
