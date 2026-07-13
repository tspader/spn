#define SP_IMPLEMENTATION
#include "sp.h"
#include "sp/sp_cli.h"
#include "dag/dag.h"

typedef struct {
  const c8* output;
  const c8* mode;
  const c8* opt;
  const c8* sanitize;
  const c8* include;

  sp_mem_t mem;
  spn_dag_t* g;
  sp_str_t clang;
  sp_str_t root;
  sp_da(sp_str_t) flags;
  sp_da(sp_str_t) search_dirs;
  spn_dag_store_t store;
  spn_dag_file_cache_t files;
  spn_dag_action_cache_t cache;
  spn_dag_discovery_t discovery;
  u32 actions;
  u32 executed;
} cc_app_t;

typedef struct {
  cc_app_t* app;
  sp_str_t input;
  sp_str_t obj;
  sp_str_t dep;
} cc_compile_t;

typedef struct {
  cc_app_t* app;
  sp_str_t output;
} cc_link_t;

static void cc_log_run(cc_app_t* app, sp_ps_config_t ps) {
  sp_str_t line = ps.command;
  sp_da_for(ps.dyn_args, it) {
    line = sp_fmt(app->mem, "{} {}", sp_fmt_str(line), sp_fmt_str(ps.dyn_args[it])).value;
  }
  sp_os_print(sp_fmt(app->mem, "  run   {}\n", sp_fmt_str(line)).value);
}

static s32 cc_compile_exec(spn_dag_action_t* action, void* user_data) {
  cc_compile_t* c = (cc_compile_t*)user_data;
  cc_app_t* app = c->app;
  app->executed++;

  sp_ps_config_t ps = { .command = app->clang };
  sp_da_init(app->mem, ps.dyn_args);
  sp_da_push(ps.dyn_args, sp_str_lit("-c"));
  sp_da_push(ps.dyn_args, sp_str_lit("-MD"));
  sp_da_push(ps.dyn_args, sp_str_lit("-MF"));
  sp_da_push(ps.dyn_args, c->dep);
  sp_da_push(ps.dyn_args, sp_str_lit("-o"));
  sp_da_push(ps.dyn_args, c->obj);
  sp_da_for(app->flags, it) {
    sp_da_push(ps.dyn_args, app->flags[it]);
  }
  sp_da_push(ps.dyn_args, c->input);

  cc_log_run(app, ps);
  return sp_ps_run(app->mem, ps).status.exit_code;
}

static spn_err_t cc_compile_discover(spn_dag_action_t* action, void* user_data, sp_mem_t mem, sp_da(spn_dag_obs_t)* out) {
  cc_compile_t* c = (cc_compile_t*)user_data;
  spn_cc_ctx_t ctx = {
    .g = c->app->g,
    .search_dirs = c->app->search_dirs
  };
  return spn_cc_discover(action, &ctx, mem, out);
}

static s32 cc_link_exec(spn_dag_action_t* action, void* user_data) {
  cc_link_t* l = (cc_link_t*)user_data;
  cc_app_t* app = l->app;
  app->executed++;

  sp_ps_config_t ps = { .command = app->clang };
  sp_da_init(app->mem, ps.dyn_args);
  sp_da_push(ps.dyn_args, sp_str_lit("-o"));
  sp_da_push(ps.dyn_args, l->output);
  sp_da_for(action->consumes, it) {
    sp_da_push(ps.dyn_args, spn_dag_find_artifact(app->g, action->consumes[it])->path);
  }

  cc_log_run(app, ps);
  return sp_ps_run(app->mem, ps).status.exit_code;
}

static spn_dag_digest_t cc_identity(cc_app_t* app, sp_str_t verb) {
  sp_str_t s = verb;
  sp_da_for(app->flags, it) {
    s = sp_fmt(app->mem, "{} {}", sp_fmt_str(s), sp_fmt_str(app->flags[it])).value;
  }
  sp_da_for(app->search_dirs, it) {
    s = sp_fmt(app->mem, "{} @{}", sp_fmt_str(s), sp_fmt_str(app->search_dirs[it])).value;
  }
  return spn_dag_digest(s.data, s.len);
}

static void cc_push_search_dirs(cc_app_t* app, sp_str_t dirs) {
  if (sp_str_empty(dirs)) {
    return;
  }
  sp_da(sp_str_t) split = sp_str_split_c8(app->mem, dirs, ':');
  sp_da_for(split, it) {
    if (!sp_str_empty(split[it])) {
      sp_da_push(app->search_dirs, sp_fs_normalize_path(app->mem, split[it]));
    }
  }
}

static void cc_flags_init(cc_app_t* app) {
  sp_da_init(app->mem, app->flags);
  if (app->mode && sp_str_equal(sp_str_view(app->mode), sp_str_lit("release"))) {
    sp_da_push(app->flags, sp_str_lit("-DNDEBUG"));
  } else {
    sp_da_push(app->flags, sp_str_lit("-g"));
  }
  sp_da_push(app->flags, sp_fmt(app->mem, "-O{}", sp_fmt_cstr(app->opt ? app->opt : "0")).value);
  if (app->sanitize && *app->sanitize) {
    sp_da_push(app->flags, sp_fmt(app->mem, "-fsanitize={}", sp_fmt_cstr(app->sanitize)).value);
  }

  sp_da_init(app->mem, app->search_dirs);
  if (app->include) {
    u32 before = (u32)sp_da_size(app->search_dirs);
    cc_push_search_dirs(app, sp_str_view(app->include));
    for (u32 it = before; it < (u32)sp_da_size(app->search_dirs); it++) {
      sp_da_push(app->flags, sp_fmt(app->mem, "-I{}", sp_fmt_str(app->search_dirs[it])).value);
    }
  }
  cc_push_search_dirs(app, sp_os_env_get(sp_str_lit("CPATH")));
}

static sp_str_t cc_obj_name(cc_app_t* app, sp_str_t input, sp_str_t ext) {
  sp_str_t stem = sp_fs_get_stem(input);
  spn_dag_digest_t digest = spn_dag_digest(input.data, input.len);
  sp_str_t tag = sp_str_sub(spn_dag_digest_hex(app->mem, digest), 0, 8);
  return sp_fmt(app->mem, "{}-{}.{}", sp_fmt_str(stem), sp_fmt_str(tag), sp_fmt_str(ext)).value;
}

static void cc_report(cc_app_t* app) {
  u32 hits = app->actions - app->executed;
  sp_os_print(sp_fmt(app->mem, "\n{} action(s): {} executed, {} restored from cache\n",
    sp_fmt_uint(app->actions), sp_fmt_uint(app->executed), sp_fmt_uint(hits)).value);
  sp_os_print(sp_fmt(app->mem, "store: {}/store\n", sp_fmt_str(app->root)).value);
}

static sp_cli_result_t cc_build(sp_cli_t* cli) {
  cc_app_t* app = (cc_app_t*)cli->user_data;

  if (!app->output) {
    return sp_cli_set_error(cli, sp_str_lit("missing -o <output>"));
  }
  if (!cli->num_rest) {
    return sp_cli_set_error(cli, sp_str_lit("no input .c files"));
  }

  app->clang = sp_str_lit("clang");
  app->root = sp_str_lit(".sp-cc");
  sp_str_t obj_dir = sp_fs_join_path(app->mem, app->root, sp_str_lit("obj"));
  sp_str_t store_dir = sp_fs_join_path(app->mem, app->root, sp_str_lit("store"));
  sp_fs_create_dir(obj_dir);
  cc_flags_init(app);

  spn_dag_store_init(&app->store, (spn_dag_store_config_t) {
    .kind = SPN_DAG_STORE_FILESYSTEM,
    .mem = app->mem,
    .dir = store_dir
  });
  spn_dag_file_cache_init(&app->files, app->mem);
  spn_dag_action_cache_init(&app->cache, app->mem);
  spn_dag_discovery_init(&app->discovery, app->mem);

  sp_str_t files_table = sp_fs_join_path(app->mem, app->root, sp_str_lit("files.jsonl"));
  sp_str_t cache_table = sp_fs_join_path(app->mem, app->root, sp_str_lit("actions.jsonl"));
  sp_str_t disc_table = sp_fs_join_path(app->mem, app->root, sp_str_lit("discovery.jsonl"));
  spn_dag_file_cache_load(&app->files, files_table);
  spn_dag_action_cache_load(&app->cache, cache_table);
  spn_dag_discovery_load(&app->discovery, disc_table);

  app->g = spn_dag_new(app->mem);
  spn_dag_id_t link = spn_dag_add_action(app->g, (spn_dag_action_config_t) {
    .identity = cc_identity(app, sp_str_lit("link")),
    .execute = cc_link_exec,
  });
  cc_link_t* link_ctx = sp_alloc_type(app->mem, cc_link_t);
  link_ctx->app = app;
  link_ctx->output = sp_str_view(app->output);
  spn_dag_find_action(app->g, link)->user_data = link_ctx;

  spn_dag_id_t out_id = spn_dag_add_file(app->g, sp_str_view(app->output));
  spn_dag_action_add_output(app->g, link, out_id);
  app->actions = 1;

  sp_for(it, cli->num_rest) {
    sp_str_t input = sp_str_view(cli->rest[it]);
    sp_str_t obj = sp_fs_join_path(app->mem, obj_dir, cc_obj_name(app, input, sp_str_lit("o")));
    sp_str_t dep = sp_fs_join_path(app->mem, obj_dir, cc_obj_name(app, input, sp_str_lit("d")));

    cc_compile_t* ctx = sp_alloc_type(app->mem, cc_compile_t);
    ctx->app = app;
    ctx->input = input;
    ctx->obj = obj;
    ctx->dep = dep;

    spn_dag_id_t compile = spn_dag_add_action(app->g, (spn_dag_action_config_t) {
      .identity = cc_identity(app, sp_fmt(app->mem, "compile {}", sp_fmt_str(input)).value),
      .execute = cc_compile_exec,
      .discover = cc_compile_discover,
      .user_data = ctx
    });
    spn_dag_action_add_input(app->g, compile, spn_dag_add_file(app->g, input));

    spn_dag_id_t obj_id = spn_dag_add_file(app->g, obj);
    spn_dag_id_t dep_id = spn_dag_add_file(app->g, dep);
    spn_dag_action_add_output(app->g, compile, obj_id);
    spn_dag_action_add_output(app->g, compile, dep_id);

    spn_dag_action_add_input(app->g, link, obj_id);
    app->actions++;
  }

  spn_err_t err = spn_dag_run(app->g, &app->files, &app->cache, &app->store, &app->discovery);

  spn_dag_file_cache_save(&app->files, files_table);
  spn_dag_action_cache_save(&app->cache, cache_table);
  spn_dag_discovery_save(&app->discovery, disc_table);

  if (err) {
    return sp_cli_set_error(cli, sp_str_lit("build failed"));
  }

  cc_report(app);
  return SP_CLI_OK;
}

s32 main(s32 num_args, const c8** args) {
  cc_app_t app = sp_zero;
  app.mem = sp_mem_os_new();

  sp_cli_cmd_t root = {
    .name = "sp-cc",
    .summary = "Content-addressed C compiler frontend (clang + DAG cache)",
    .opts = {
      { .brief = "o", .name = "output", .kind = SP_CLI_OPT_CSTR, .summary = "output binary", .placeholder = "FILE", .ptr = &app.output },
      { .name = "mode", .kind = SP_CLI_OPT_CSTR, .summary = "debug (default) or release", .placeholder = "MODE", .ptr = &app.mode },
      { .name = "opt", .kind = SP_CLI_OPT_CSTR, .summary = "optimization level (0-3, s, z)", .placeholder = "LEVEL", .ptr = &app.opt },
      { .name = "sanitize", .kind = SP_CLI_OPT_CSTR, .summary = "sanitizer (e.g. address)", .placeholder = "SAN", .ptr = &app.sanitize },
      { .brief = "I", .name = "include", .kind = SP_CLI_OPT_CSTR, .summary = "colon-separated include search dirs", .placeholder = "DIRS", .ptr = &app.include },
    },
    .args = {
      { .name = "sources", .arity = SP_CLI_ARG_REST, .summary = "C source files" },
    },
    .handler = cc_build,
  };

  return sp_cli_main((sp_cli_desc_t) {
    .root = &root,
    .num_args = num_args,
    .args = args,
    .user_data = &app,
  });
}
