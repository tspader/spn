#define SP_IMPLEMENTATION
#include "sp.h"
#include "sp/sp_cli.h"
#include "sp/atomic_file.h"
#include "sp/sp_glob.h"
#include "dag/dag.h"
#include "error/types.h"
#include "dag/occ.h"

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
  spn_dag_id_t obj;
} cc_compile_t;

typedef struct {
  cc_app_t* app;
} cc_link_t;

static void cc_log_run(cc_app_t* app, sp_ps_config_t ps) {
  sp_str_t line = ps.command;
  sp_da_for(ps.dyn_args, it) {
    line = sp_fmt(app->mem, "{} {}", sp_fmt_str(line), sp_fmt_str(ps.dyn_args[it])).value;
  }
  sp_os_print(sp_fmt(app->mem, "  run   {}\n", sp_fmt_str(line)).value);
}

static sp_str_t cc_obj_path(cc_compile_t* c) {
  return spn_dag_find_artifact(c->app->g, c->obj)->path;
}

static sp_str_t cc_dep_path(cc_compile_t* c, sp_mem_t mem) {
  return sp_fmt(mem, "{}.d", sp_fmt_str(cc_obj_path(c))).value;
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
  sp_da_push(ps.dyn_args, cc_dep_path(c, app->mem));
  sp_da_push(ps.dyn_args, sp_str_lit("-o"));
  sp_da_push(ps.dyn_args, cc_obj_path(c));
  sp_da_for(app->flags, it) {
    sp_da_push(ps.dyn_args, app->flags[it]);
  }
  sp_da_push(ps.dyn_args, c->input);

  cc_log_run(app, ps);
  return sp_ps_run(app->mem, ps).status.exit_code;
}

static spn_err_t cc_deps_parse(sp_mem_t mem, sp_str_t content, sp_da(sp_str_t)* prereqs) {
  occ_parser_t p = sp_zero;
  if (occ_init(&p, content)) {
    return SPN_ERROR;
  }

  sp_str_t prereq = sp_zero;
  while (occ_next(&p, &prereq)) {
    sp_da_push(*prereqs, sp_str_copy(mem, prereq));
  }

  return p.err ? SPN_ERROR : SPN_OK;
}

static void cc_probe_shadows(cc_app_t* app, sp_str_t prereq, sp_mem_t mem, sp_da(spn_dag_obs_t)* obs) {
  sp_da_for(app->search_dirs, it) {
    sp_str_t dir = app->search_dirs[it];
    if (prereq.len <= dir.len + 1) {
      continue;
    }
    if (!sp_str_starts_with(prereq, dir) || prereq.data[dir.len] != '/') {
      continue;
    }

    sp_str_t suffix = sp_str_sub(prereq, dir.len + 1, prereq.len - dir.len - 1);
    sp_for(shadow, it) {
      sp_da_push(*obs, ((spn_dag_obs_t) {
        .kind = SPN_DAG_OBS_ABSENT,
        .path = sp_fs_join_path(mem, app->search_dirs[shadow], suffix)
      }));
    }
    return;
  }
}

static spn_err_t cc_compile_discover(spn_dag_action_t* action, void* user_data, sp_mem_t mem, sp_da(spn_dag_obs_t)* out) {
  cc_compile_t* c = (cc_compile_t*)user_data;
  sp_str_t content = sp_zero;
  spn_try_as(sp_io_read_file(mem, cc_dep_path(c, mem), &content), SPN_ERROR);

  sp_da(sp_str_t) prereqs = sp_da_new(mem, sp_str_t);
  spn_try(cc_deps_parse(mem, content, &prereqs));

  sp_da_for(prereqs, it) {
    sp_da_push(*out, ((spn_dag_obs_t) {
      .kind = SPN_DAG_OBS_FILE,
      .path = prereqs[it]
    }));
    cc_probe_shadows(c->app, prereqs[it], mem, out);
  }

  return SPN_OK;
}

static s32 cc_link_exec(spn_dag_action_t* action, void* user_data) {
  cc_link_t* l = (cc_link_t*)user_data;
  cc_app_t* app = l->app;
  app->executed++;

  sp_ps_config_t ps = { .command = app->clang };
  sp_da_init(app->mem, ps.dyn_args);
  sp_da_push(ps.dyn_args, sp_str_lit("-o"));
  sp_da_push(ps.dyn_args, spn_dag_find_artifact(app->g, action->produces[0])->path);
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

static void cc_report(cc_app_t* app) {
  u32 hits = app->actions - app->executed;
  sp_os_print(sp_fmt(app->mem, "\n{} action(s): {} executed, {} restored from cache\n",
    sp_fmt_uint(app->actions), sp_fmt_uint(app->executed), sp_fmt_uint(hits)).value);
  sp_os_print(sp_fmt(app->mem, "cache: {}\n", sp_fmt_str(app->root)).value);
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
  app->root = sp_str_lit(".cache/spn");
  cc_flags_init(app);

  spn_dag_store_init(&app->store, (spn_dag_store_config_t) {
    .kind = SPN_DAG_STORE_FILESYSTEM,
    .mem = app->mem,
    .dir = sp_fs_join_path(app->mem, app->root, sp_str_lit("store"))
  });
  spn_dag_file_cache_init(&app->files, app->mem);
  spn_dag_action_cache_init(&app->cache, app->mem, sp_fs_join_path(app->mem, app->root, sp_str_lit("strong")));
  spn_dag_discovery_init(&app->discovery, app->mem, sp_fs_join_path(app->mem, app->root, sp_str_lit("weak")));

  app->g = spn_dag_new(app->mem);
  spn_dag_id_t link = spn_dag_add_action(app->g, (spn_dag_action_config_t) {
    .identity = cc_identity(app, sp_str_lit("link")),
    .execute = cc_link_exec,
  });
  cc_link_t* link_ctx = sp_alloc_type(app->mem, cc_link_t);
  link_ctx->app = app;
  spn_dag_find_action(app->g, link)->user_data = link_ctx;

  spn_dag_id_t out_id = spn_dag_add_file(app->g, sp_str_view(app->output));
  spn_dag_action_add_output(app->g, link, out_id);
  app->actions = 1;

  sp_for(it, cli->num_rest) {
    sp_str_t input = sp_str_view(cli->rest[it]);

    cc_compile_t* ctx = sp_alloc_type(app->mem, cc_compile_t);
    ctx->app = app;
    ctx->input = input;

    spn_dag_id_t compile = spn_dag_add_action(app->g, (spn_dag_action_config_t) {
      .identity = cc_identity(app, sp_fmt(app->mem, "compile {}", sp_fmt_str(input)).value),
      .execute = cc_compile_exec,
      .discover = cc_compile_discover,
      .user_data = ctx
    });
    spn_dag_action_add_input(app->g, compile, spn_dag_add_file(app->g, input));

    sp_str_t name = sp_fmt(app->mem, "{}.o", sp_fmt_str(sp_fs_get_stem(input))).value;
    spn_dag_id_t obj_id = spn_dag_add_output(app->g, name);
    spn_dag_action_add_output(app->g, compile, obj_id);
    ctx->obj = obj_id;

    spn_dag_action_add_input(app->g, link, obj_id);
    app->actions++;
  }

  spn_dag_env_t env = {
    .files = &app->files,
    .cache = &app->cache,
    .store = &app->store,
    .discovery = &app->discovery,
    .scratch = sp_fs_join_path(app->mem, app->root, sp_str_lit("tmp"))
  };
  spn_err_t err = spn_dag_run(app->g, &env);

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
    .name = "spcc",
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
