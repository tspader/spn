#include "sp_fuzz.h"
#include "sp/prompt.h"

#include "codegen/codegen.h"
#include "external/tom.h"
#include "fuzz.gen.h"
#include "intern/intern.h"
#include "toml/issue.h"
#include "toml/loader.h"

sp_app_result_t    sp_prompt_app_on_init(sp_app_t* app);
sp_app_result_t    sp_prompt_app_on_poll(sp_app_t* app);
sp_prompt_widget_t sp_prompt_progress_widget(sp_prompt_ctx_t* ctx, sp_prompt_progress_t config);

typedef struct {
  const sp_fuzz_desc_t* desc;
  s64 iters;
  s64 iter;
  const c8* seed;
  const c8* config;
  const c8* profile;
  const c8* render;
  bool keep_going;
  bool dry_run;
  bool list_profiles;
  s32 status;
} sp_fuzz_cli_t;

typedef struct {
  sp_prompt_ctx_t* ctx;
  sp_app_t app;
  bool on;
} sp_fuzz_prompt_t;

static struct {
  const c8* config;
  const c8* profile;
  bool keep_going;
  sp_str_t render;
  spn_cg_fuzz_graph_t graph;
} sp_fuzz_cli_state;

sp_str_t sp_fuzz_render_path(void) {
  return sp_fuzz_cli_state.render;
}

const spn_cg_fuzz_graph_t* sp_fuzz_graph(void) {
  return &sp_fuzz_cli_state.graph;
}

static sp_err_t sp_fuzz_fmt_hex(sp_io_writer_t* io, sp_fmt_arg_t* arg) {
  return sp_fmt_write_u64_ex(io, arg->value.u, SP_FMT_RADIX_HEX);
}

static void sp_fuzz_repro_common(sp_io_writer_t* io) {
  if (sp_fuzz_cli_state.config) {
    sp_fmt_io(io, "--config {} ", sp_fmt_cstr(sp_fuzz_cli_state.config));
  }
  if (sp_fuzz_cli_state.profile) {
    sp_fmt_io(io, "--profile {} ", sp_fmt_cstr(sp_fuzz_cli_state.profile));
  }
  sp_fmt_io(io, "--seed 0x{}", sp_fmt_u64_custom(sp_fuzz_seed_get(), sp_fuzz_fmt_hex));
  if (sp_fuzz_cli_state.keep_going) {
    sp_fmt_io(io, " --keep-going");
  }
  if (!sp_str_empty(sp_fuzz_cli_state.render)) {
    sp_fmt_io(io, " --render {}", sp_fmt_str(sp_fuzz_cli_state.render));
  }
}

sp_str_t sp_fuzz_repro_args(sp_mem_t mem, u64 iter) {
  sp_io_dyn_mem_writer_t out;
  sp_io_dyn_mem_writer_init(mem, &out);
  sp_fuzz_repro_common(&out.base);
  sp_fmt_io(&out.base, " --iter {}", sp_fmt_uint(iter));
  return sp_io_dyn_mem_writer_as_str(&out);
}

static sp_str_t sp_fuzz_params(sp_mem_t mem, sp_fuzz_opts_t opts) {
  sp_io_dyn_mem_writer_t out;
  sp_io_dyn_mem_writer_init(mem, &out);
  sp_fuzz_repro_common(&out.base);
  if (opts.only >= 0) {
    sp_fmt_io(&out.base, " --iter {}", sp_fmt_uint((u64)opts.only));
  }
  else {
    sp_fmt_io(&out.base, " --iters {}", sp_fmt_uint(opts.iters));
  }
  return sp_io_dyn_mem_writer_as_str(&out);
}

static const spn_cg_fuzz_profile_t* sp_fuzz_find_profile(const spn_cg_fuzz_t* cfg, sp_str_t name) {
  sp_da_for(cfg->profile, it) {
    if (sp_str_equal(cfg->profile[it].key, name)) {
      return &cfg->profile[it].value;
    }
  }
  return SP_NULLPTR;
}

static sp_str_t sp_fuzz_profile_names(sp_mem_t mem, const spn_cg_fuzz_t* cfg) {
  sp_io_dyn_mem_writer_t out;
  sp_io_dyn_mem_writer_init(mem, &out);
  sp_da_for(cfg->profile, it) {
    sp_fmt_io(&out.base, it ? ", {}" : "{}", sp_fmt_str(cfg->profile[it].key));
  }
  return sp_io_dyn_mem_writer_as_str(&out);
}

#define sp_fuzz_overlay_opt(dst, src, field) \
  do { \
    if (!sp_opt_is_null((src)->field)) { \
      (dst)->field = (src)->field; \
    } \
  } while (0)

static void sp_fuzz_overlay_profile(spn_cg_fuzz_profile_t* dst, const spn_cg_fuzz_profile_t* src) {
  sp_fuzz_overlay_opt(dst, src, iterations);
  sp_fuzz_overlay_opt(dst, src, iteration);
  sp_fuzz_overlay_opt(dst, src, seed);
  sp_fuzz_overlay_opt(dst, src, keep_going);
  if (!sp_str_empty(src->render)) {
    dst->render = src->render;
  }
  sp_fuzz_overlay_opt(&dst->graph, &src->graph, max_actions);
  sp_fuzz_overlay_opt(&dst->graph, &src->graph, small_actions);
  sp_fuzz_overlay_opt(&dst->graph, &src->graph, max_sources);
  sp_fuzz_overlay_opt(&dst->graph, &src->graph, max_produces);
  sp_fuzz_overlay_opt(&dst->graph, &src->graph, max_phantoms);
  sp_fuzz_overlay_opt(&dst->graph, &src->graph, max_obs);
}

static sp_cli_result_t sp_fuzz_resolve_profile(sp_cli_t* cli, sp_mem_t mem, const spn_cg_fuzz_t* cfg, sp_str_t name, spn_cg_fuzz_profile_t* out) {
  u64 count = sp_da_size(cfg->profile);
  const spn_cg_fuzz_profile_t** chain = sp_alloc_n(mem, const spn_cg_fuzz_profile_t*, count ? count : 1);
  u64 depth = 0;

  sp_str_t cursor = name;
  while (!sp_str_empty(cursor)) {
    const spn_cg_fuzz_profile_t* found = sp_fuzz_find_profile(cfg, cursor);
    if (!found) {
      sp_str_t detail = depth
        ? sp_fmt(mem, "profile {.cyan} inherits from unknown profile {.red}", sp_fmt_str(name), sp_fmt_str(cursor)).value
        : sp_fmt(mem, "unknown profile {.red}; available: {}", sp_fmt_str(cursor), sp_fmt_str(sp_fuzz_profile_names(mem, cfg))).value;
      return sp_cli_set_error(cli, detail);
    }
    if (depth >= count) {
      return sp_cli_set_error(cli, sp_fmt(mem, "profile {.cyan} has an inheritance cycle", sp_fmt_str(name)).value);
    }
    chain[depth++] = found;
    cursor = found->from;
  }

  for (u64 it = depth; it-- > 0;) {
    sp_fuzz_overlay_profile(out, chain[it]);
  }
  return SP_CLI_OK;
}

static sp_cli_result_t sp_fuzz_load_config(sp_cli_t* cli, sp_mem_t mem, sp_fuzz_cli_t* config, spn_cg_fuzz_t* out) {
  bool implied = !config->config;
  if (implied) {
    config->config = "fuzz.toml";
    if (!sp_fs_is_file(sp_cstr_as_str(config->config))) {
      return sp_cli_set_error(cli, sp_fmt(mem,
        "{} implies {.cyan}, but there is no fuzz.toml in the current directory",
        sp_fmt_cstr(config->profile ? "--profile" : "--list-profiles"),
        sp_fmt_cstr("--config fuzz.toml")).value);
    }
  }

  spn_toml_loader_t loader = sp_zero;
  spn_toml_loader_init(&loader, mem, sp_intern_new(mem));
  loader.strict = true;

  toml_table_t* table = spn_codegen_parse(&loader, sp_cstr_as_str(config->config));
  if (table) {
    spn_fuzz_read(&loader, table, out);
    toml_free(table);
  }
  if (!sp_da_empty(loader.issues)) {
    return sp_cli_set_error(cli, sp_fmt(mem, "{.cyan}: {}", sp_fmt_cstr(config->config), sp_fmt_str(spn_codegen_issues_message(mem, loader.issues))).value);
  }
  return SP_CLI_OK;
}

static sp_cli_result_t sp_fuzz_load_profile(sp_cli_t* cli, sp_mem_t mem, sp_fuzz_cli_t* config, spn_cg_fuzz_profile_t* out) {
  if (!config->config && !config->profile) {
    return SP_CLI_OK;
  }

  spn_cg_fuzz_t cfg = sp_zero;
  sp_cli_result_t err = sp_fuzz_load_config(cli, mem, config, &cfg);
  if (err) {
    return err;
  }

  sp_str_t name = config->profile ? sp_cstr_as_str(config->profile) : sp_str_lit("default");
  if (!config->profile && !sp_fuzz_find_profile(&cfg, name)) {
    return SP_CLI_OK;
  }
  return sp_fuzz_resolve_profile(cli, mem, &cfg, name, out);
}

static sp_cli_result_t sp_fuzz_list_profiles(sp_cli_t* cli, sp_mem_t mem, sp_fuzz_cli_t* config) {
  spn_cg_fuzz_t cfg = sp_zero;
  sp_cli_result_t err = sp_fuzz_load_config(cli, mem, config, &cfg);
  if (err) {
    return err;
  }

  sp_io_stream_writer_t out = sp_io_get_std_out();
  if (sp_da_empty(cfg.profile)) {
    sp_fmt_io(&out.base, "{.cyan} defines no profiles\n", sp_fmt_cstr(config->config));
    return SP_CLI_OK;
  }
  sp_da_for(cfg.profile, it) {
    spn_cg_fuzz_profile_t* profile = &cfg.profile[it].value;
    sp_fmt_io(&out.base, "{}", sp_fmt_str(cfg.profile[it].key));
    if (!sp_str_empty(profile->from)) {
      sp_fmt_io(&out.base, " <- {}", sp_fmt_str(profile->from));
    }
    if (!sp_opt_is_null(profile->iterations)) {
      sp_fmt_io(&out.base, " ({} iterations)", sp_fmt_uint(sp_opt_get(profile->iterations)));
    }
    sp_fmt_io(&out.base, "\n");
  }
  return SP_CLI_OK;
}

#define sp_fuzz_graph_knob(io, graph, first, field) \
  do { \
    if (!sp_opt_is_null((graph)->field)) { \
      sp_fmt_io(io, *(first) ? "graph: {}={}" : " {}={}", sp_fmt_cstr(#field), sp_fmt_uint(sp_opt_get((graph)->field))); \
      *(first) = false; \
    } \
  } while (0)

static sp_str_t sp_fuzz_graph_line(sp_mem_t mem, const spn_cg_fuzz_graph_t* graph) {
  sp_io_dyn_mem_writer_t out;
  sp_io_dyn_mem_writer_init(mem, &out);
  bool first = true;
  sp_fuzz_graph_knob(&out.base, graph, &first, max_actions);
  sp_fuzz_graph_knob(&out.base, graph, &first, small_actions);
  sp_fuzz_graph_knob(&out.base, graph, &first, max_sources);
  sp_fuzz_graph_knob(&out.base, graph, &first, max_produces);
  sp_fuzz_graph_knob(&out.base, graph, &first, max_phantoms);
  sp_fuzz_graph_knob(&out.base, graph, &first, max_obs);
  return sp_io_dyn_mem_writer_as_str(&out);
}

static void sp_fuzz_prompt_start(sp_fuzz_prompt_t* prompt, sp_mem_t mem, const sp_fuzz_desc_t* desc, sp_str_t note) {
  if (!sp_os_is_tty(sp_sys_stdout)) {
    return;
  }
  prompt->ctx = sp_prompt_begin(mem);
  if (!prompt->ctx) {
    return;
  }

  sp_prompt_note(prompt->ctx, sp_str_to_cstr(mem, note), desc->name);

  sp_prompt_app(prompt->ctx, sp_prompt_progress_widget(prompt->ctx, (sp_prompt_progress_t) {
    .prompt = "fuzzing",
    .color = { .rgb = { .r = 99, .g = 160, .b = 136 } },
  }));
  prompt->app = (sp_app_t) { .user_data = prompt->ctx };
  sp_prompt_app_on_init(&prompt->app);
  prompt->on = true;
}

static void sp_fuzz_prompt_pump(sp_fuzz_prompt_t* prompt, u64 done, u64 total, u64 failed) {
  if (!prompt->on) {
    return;
  }

  sp_mem_arena_marker_t s = sp_mem_begin_scratch();
  sp_str_t status = failed
    ? sp_fmt(s.mem, "{}/{} iterations, {} failed", sp_fmt_uint(done), sp_fmt_uint(total), sp_fmt_uint(failed)).value
    : sp_fmt(s.mem, "{}/{} iterations", sp_fmt_uint(done), sp_fmt_uint(total)).value;
  sp_prompt_send_status_str(prompt->ctx, status);
  sp_mem_end_scratch(s);

  sp_prompt_send_progress_f32(prompt->ctx, total ? (f32)done / (f32)total : 0.f);
  sp_prompt_app_on_poll(&prompt->app);
}

static void sp_fuzz_prompt_stop(sp_fuzz_prompt_t* prompt, bool ok) {
  if (!prompt->on) {
    return;
  }
  sp_prompt_set_state(prompt->ctx, ok ? SP_PROMPT_STATE_SUBMIT : SP_PROMPT_STATE_ERROR);
  sp_prompt_app_on_poll(&prompt->app);
  sp_prompt_end(prompt->ctx);
  prompt->on = false;
}

static void sp_fuzz_report(sp_fuzz_prompt_t* prompt, sp_io_writer_t* out, sp_str_t line) {
  if (prompt->on) {
    sp_prompt_log_str(prompt->ctx, line);
  }
  else {
    sp_fmt_io(out, "{}\n", sp_fmt_str(line));
  }
}

static sp_cli_result_t sp_fuzz_cli_run(sp_cli_t* cli) {
  sp_fuzz_cli_t* config = (sp_fuzz_cli_t*)cli->user_data;
  const sp_fuzz_desc_t* desc = config->desc;
  sp_mem_t mem = sp_mem_os_new();

  if (config->list_profiles) {
    return sp_fuzz_list_profiles(cli, mem, config);
  }

  spn_cg_fuzz_profile_t profile = sp_zero;
  sp_cli_result_t loaded = sp_fuzz_load_profile(cli, mem, config, &profile);
  if (loaded) {
    return loaded;
  }
  sp_fuzz_cli_state.config = config->config;
  sp_fuzz_cli_state.profile = config->profile;
  sp_fuzz_cli_state.graph = profile.graph;

  if (config->seed) {
    sp_fuzz_seed_compute(sp_cstr_as_str(config->seed));
  }
  else if (!sp_opt_is_null(profile.seed)) {
    sp_fuzz_seed_set(sp_opt_get(profile.seed));
  }
  else {
    sp_fuzz_seed_compute(sp_str_lit(""));
  }

  sp_fuzz_opts_t opts = sp_fuzz_opts(desc->iters);
  if (!sp_opt_is_null(profile.iterations)) {
    opts.iters = sp_opt_get(profile.iterations);
  }
  if (!sp_opt_is_null(profile.iteration)) {
    opts.only = (s64)sp_opt_get(profile.iteration);
    opts.iters = sp_opt_get(profile.iteration) + 1;
  }
  if (config->iters >= 0) {
    opts.iters = (u64)config->iters;
    opts.only = -1;
  }
  if (config->iter >= 0) {
    opts.only = config->iter;
    opts.iters = (u64)config->iter + 1;
  }

  sp_fuzz_cli_state.keep_going = config->keep_going || (!sp_opt_is_null(profile.keep_going) && sp_opt_get(profile.keep_going));
  sp_fuzz_cli_state.render = config->render ? sp_cstr_as_str(config->render) : profile.render;
  bool keep_going = sp_fuzz_cli_state.keep_going;

  sp_str_t graph_line = sp_fuzz_graph_line(mem, &profile.graph);
  if (config->dry_run) {
    sp_io_stream_writer_t out = sp_io_get_std_out();
    sp_fmt_io(&out.base, "{}\n", sp_fmt_str(sp_fuzz_params(mem, opts)));
    if (!sp_str_empty(graph_line)) {
      sp_fmt_io(&out.base, "{}\n", sp_fmt_str(graph_line));
    }
    return SP_CLI_OK;
  }

  sp_da(sp_str_t) names = sp_da_new(mem, sp_str_t);
  sp_da_push(names, sp_str_view(desc->name));
  sp_fuzz_prng_t base = sp_fuzz_stream(names);

  sp_io_stream_writer_t out = sp_io_get_std_out();
  u64* failures = sp_alloc_n(mem, u64, desc->errs);
  sp_mem_zero(failures, desc->errs * sizeof(u64));
  u64 failed = 0;
  u64 done = 0;

  sp_fuzz_prompt_t prompt = sp_zero;
  sp_str_t note = sp_fuzz_params(mem, opts);
  if (!sp_str_empty(graph_line)) {
    note = sp_fmt(mem, "{}\n{}", sp_fmt_str(note), sp_fmt_str(graph_line)).value;
  }
  sp_fuzz_prompt_start(&prompt, mem, desc, note);
  if (!prompt.on) {
    sp_fmt_io(&out.base, "{}\n", sp_fmt_str(note));
  }

  for (u64 iter = 0; iter < opts.iters; iter++) {
    if (opts.only >= 0 && iter != (u64)opts.only) continue;
    done++;

    sp_mem_arena_t* arena = sp_mem_arena_new(sp_mem_os_new());
    u32 err = desc->run(sp_mem_arena_as_allocator(arena), sp_fuzz_iter(base, iter), iter);
    sp_mem_arena_destroy(arena);

    if (err) {
      failed++;
      if (err < desc->errs) {
        failures[err]++;
      }

      sp_mem_arena_marker_t s = sp_mem_begin_scratch();
      sp_fuzz_report(&prompt, &out.base, sp_fmt(s.mem, "fuzz: {} (repro: {})", sp_fmt_str(desc->err_str(err)), sp_fmt_str(sp_fuzz_repro_args(s.mem, iter))).value);
      sp_mem_end_scratch(s);

      if (!keep_going) {
        config->status = (s32)err;
        break;
      }
    }

    sp_fuzz_prompt_pump(&prompt, iter + 1, opts.iters, failed);
    if (prompt.on && sp_prompt_cancelled(prompt.ctx)) {
      config->status = 1;
      break;
    }
  }

  bool was_tty = prompt.on || sp_os_is_tty(sp_sys_stdout);
  sp_fuzz_prompt_stop(&prompt, !failed && !config->status);

  if (!was_tty && !failed && !config->status) {
    sp_fmt_io(&out.base, "fuzz: {} iterations passed\n", sp_fmt_uint(done));
  }

  if (failed && keep_going) {
    sp_fmt_io(&out.base, "fuzz: {} of {} iterations failed\n", sp_fmt_uint(failed), sp_fmt_uint(opts.iters));
    for (u32 it = 0; it < desc->errs; it++) {
      if (!failures[it]) continue;
      sp_fmt_io(&out.base, "  {}: {}\n", sp_fmt_uint(failures[it]), sp_fmt_str(desc->err_str(it)));
    }
    config->status = 1;
  }

  return SP_CLI_OK;
}

s32 sp_fuzz_main(s32 num_args, c8** args, const sp_fuzz_desc_t* desc) {
  sp_fuzz_cli_t config = {
    .desc = desc,
    .iters = -1,
    .iter = -1,
  };

  sp_cli_cmd_t root = {
    .name = desc->name,
    .summary = desc->summary,
    .opts = {
      {
        .brief = "n",
        .name = "iters",
        .kind = SP_CLI_OPT_S64,
        .summary = "Number of iterations to run",
        .placeholder = "N",
        .ptr = &config.iters,
      },
      {
        .brief = "i",
        .name = "iter",
        .kind = SP_CLI_OPT_S64,
        .summary = "Run a single iteration, e.g. to replay a dumped failure; overrides --iters",
        .placeholder = "ITER",
        .ptr = &config.iter,
      },
      {
        .brief = "s",
        .name = "seed",
        .kind = SP_CLI_OPT_CSTR,
        .summary = "PRNG seed, decimal or 0x-hex; random when unset",
        .placeholder = "SEED",
        .ptr = &config.seed,
      },
      {
        .brief = "c",
        .name = "config",
        .kind = SP_CLI_OPT_CSTR,
        .summary = "TOML config with [profile.NAME] tables; profile 'default' applies unless --profile is given",
        .placeholder = "FILE",
        .ptr = &config.config,
      },
      {
        .brief = "p",
        .name = "profile",
        .kind = SP_CLI_OPT_CSTR,
        .summary = "Profile to run from the config (implies --config fuzz.toml when unset)",
        .placeholder = "NAME",
        .ptr = &config.profile,
      },
      {
        .brief = "l",
        .name = "list-profiles",
        .kind = SP_CLI_OPT_BOOLEAN,
        .summary = "List the profiles the config defines and exit",
        .ptr = &config.list_profiles,
      },
      {
        .brief = "d",
        .name = "dry-run",
        .kind = SP_CLI_OPT_BOOLEAN,
        .summary = "Resolve the config and print the effective settings without running",
        .ptr = &config.dry_run,
      },
      {
        .brief = "k",
        .name = "keep-going",
        .kind = SP_CLI_OPT_BOOLEAN,
        .summary = "Run every iteration instead of stopping at the first failure",
        .ptr = &config.keep_going,
      },
      {
        .brief = "r",
        .name = "render",
        .kind = SP_CLI_OPT_CSTR,
        .summary = "Render each iteration into DIR/NNN/ with everything needed to replay it",
        .placeholder = "DIR",
        .ptr = &config.render,
      },
    },
    .handler = sp_fuzz_cli_run,
  };

  switch (sp_cli_run((sp_cli_desc_t) {
    .root = &root,
    .args = (const c8**)args,
    .num_args = num_args,
    .user_data = &config,
  })) {
    case SP_CLI_OK:       return config.status;
    case SP_CLI_HELP:
    case SP_CLI_CONTINUE: return 0;
    case SP_CLI_ERR:      return 1;
  }
  sp_unreachable_return(1);
}
