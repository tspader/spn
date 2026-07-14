#define SP_IMPLEMENTATION
#include "sp.h"
#include "sp/sp_cli.h"
#include "sp/atomic_file.h"
#include "sp/sp_glob.h"
#include "dag/dag.h"
#include "error/types.h"
#include "occ.h"

typedef struct {
  sp_mem_t mem;
  spn_dag_t* g;
  sp_str_t clang;
  sp_str_t root;
  sp_da(sp_str_t) cflags;
  spn_dag_store_t store;
  spn_dag_file_cache_t files;
  spn_dag_action_cache_t cache;
  spn_dag_discovery_t discovery;
  u32 actions;
  u32 executed;
} lib_app_t;

typedef struct {
  lib_app_t* app;
  sp_str_t input;
  spn_dag_id_t obj;
} lib_compile_t;

typedef struct {
  lib_app_t* app;
  sp_str_t root;
  sp_str_t pattern;
  sp_da(spn_dag_obs_t) obs;
} lib_headers_t;

static void lib_log_run(lib_app_t* app, sp_ps_config_t ps) {
  sp_str_t line = ps.command;
  sp_da_for(ps.dyn_args, it) {
    line = sp_fmt(app->mem, "{} {}", sp_fmt_str(line), sp_fmt_str(ps.dyn_args[it])).value;
  }
  sp_os_print(sp_fmt(app->mem, "  run   {}\n", sp_fmt_str(line)).value);
}

static s32 lib_run(lib_app_t* app, sp_ps_config_t ps) {
  app->executed++;
  lib_log_run(app, ps);
  return sp_ps_run(app->mem, ps).status.exit_code;
}

static sp_str_t lib_obj_path(lib_compile_t* c) {
  return spn_dag_find_artifact(c->app->g, c->obj)->path;
}

static sp_str_t lib_dep_path(lib_compile_t* c, sp_mem_t mem) {
  return sp_fmt(mem, "{}.d", sp_fmt_str(lib_obj_path(c))).value;
}

static s32 lib_compile_exec(spn_dag_action_t* action, void* user_data) {
  lib_compile_t* c = (lib_compile_t*)user_data;
  lib_app_t* app = c->app;

  sp_ps_config_t ps = { .command = app->clang };
  sp_da_init(app->mem, ps.dyn_args);
  sp_da_push(ps.dyn_args, sp_str_lit("-c"));
  sp_da_push(ps.dyn_args, sp_str_lit("-MD"));
  sp_da_push(ps.dyn_args, sp_str_lit("-MF"));
  sp_da_push(ps.dyn_args, lib_dep_path(c, app->mem));
  sp_da_push(ps.dyn_args, sp_str_lit("-o"));
  sp_da_push(ps.dyn_args, lib_obj_path(c));
  sp_da_for(app->cflags, it) {
    sp_da_push(ps.dyn_args, app->cflags[it]);
  }
  sp_da_push(ps.dyn_args, c->input);
  return lib_run(app, ps);
}

static spn_err_t lib_deps_parse(sp_mem_t mem, sp_str_t content, sp_da(sp_str_t)* prereqs) {
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

static spn_err_t lib_compile_discover(spn_dag_action_t* action, void* user_data, sp_mem_t mem, sp_da(spn_dag_obs_t)* out) {
  lib_compile_t* c = (lib_compile_t*)user_data;
  sp_str_t content = sp_zero;
  spn_try_as(sp_io_read_file(mem, lib_dep_path(c, mem), &content), SPN_ERROR);

  sp_da(sp_str_t) prereqs = sp_da_new(mem, sp_str_t);
  spn_try(lib_deps_parse(mem, content, &prereqs));

  sp_da_for(prereqs, it) {
    sp_da_push(*out, ((spn_dag_obs_t) {
      .kind = SPN_DAG_OBS_FILE,
      .path = prereqs[it]
    }));
  }
  return SPN_OK;
}

static s32 lib_archive_exec(spn_dag_action_t* action, void* user_data) {
  lib_app_t* app = (lib_app_t*)user_data;

  sp_ps_config_t ps = { .command = sp_str_lit("ar") };
  sp_da_init(app->mem, ps.dyn_args);
  sp_da_push(ps.dyn_args, sp_str_lit("rcs"));
  sp_da_push(ps.dyn_args, spn_dag_find_artifact(app->g, action->produces[0])->path);
  sp_da_for(action->consumes, it) {
    sp_da_push(ps.dyn_args, spn_dag_find_artifact(app->g, action->consumes[it])->path);
  }
  return lib_run(app, ps);
}

static s32 lib_shared_exec(spn_dag_action_t* action, void* user_data) {
  lib_app_t* app = (lib_app_t*)user_data;

  sp_ps_config_t ps = { .command = app->clang };
  sp_da_init(app->mem, ps.dyn_args);
  sp_da_push(ps.dyn_args, sp_str_lit("-shared"));
  sp_da_push(ps.dyn_args, sp_str_lit("-o"));
  sp_da_push(ps.dyn_args, spn_dag_find_artifact(app->g, action->produces[0])->path);
  sp_da_for(action->consumes, it) {
    sp_da_push(ps.dyn_args, spn_dag_find_artifact(app->g, action->consumes[it])->path);
  }
  return lib_run(app, ps);
}

static s32 lib_exe_exec(spn_dag_action_t* action, void* user_data) {
  lib_app_t* app = (lib_app_t*)user_data;

  sp_ps_config_t ps = { .command = app->clang };
  sp_da_init(app->mem, ps.dyn_args);
  sp_da_push(ps.dyn_args, sp_str_lit("-o"));
  sp_da_push(ps.dyn_args, spn_dag_find_artifact(app->g, action->produces[0])->path);
  sp_da_for(action->consumes, it) {
    sp_da_push(ps.dyn_args, spn_dag_find_artifact(app->g, action->consumes[it])->path);
  }
  sp_da_push(ps.dyn_args, sp_str_lit("-lm"));
  return lib_run(app, ps);
}

static s32 lib_headers_exec(spn_dag_action_t* action, void* user_data) {
  lib_headers_t* h = (lib_headers_t*)user_data;
  lib_app_t* app = h->app;
  app->executed++;

  sp_str_t tree = spn_dag_find_artifact(app->g, action->produces[0])->path;
  sp_os_print(sp_fmt(app->mem, "  copy  {}/{} -> {}\n", sp_fmt_str(h->root), sp_fmt_str(h->pattern), sp_fmt_str(tree)).value);

  sp_mem_arena_marker_t s = sp_mem_begin_scratch();
  spn_err_t err = SPN_OK;

  sp_da_init(app->mem, h->obs);
  sp_da(spn_dag_match_t) matches = sp_da_new(s.mem, spn_dag_match_t);
  if (spn_dag_glob(app->mem, h->root, h->pattern, &h->obs, &matches)) {
    err = SPN_ERROR;
    goto done;
  }
  sp_da_for(matches, it) {
    sp_str_t to = sp_fs_join_path(s.mem, tree, matches[it].relative);
    sp_fs_create_dir(sp_fs_parent_path(to));
    if (sp_fs_copy(matches[it].path, to)) {
      err = SPN_ERROR;
      goto done;
    }
  }

done:
  sp_mem_end_scratch(s);
  return err ? 1 : 0;
}

static spn_err_t lib_headers_discover(spn_dag_action_t* action, void* user_data, sp_mem_t mem, sp_da(spn_dag_obs_t)* out) {
  lib_headers_t* h = (lib_headers_t*)user_data;
  sp_da_for(h->obs, it) {
    sp_da_push(*out, h->obs[it]);
  }
  return SPN_OK;
}

static spn_dag_digest_t lib_identity(lib_app_t* app, sp_str_t verb, sp_da(sp_str_t) parts) {
  sp_mem_arena_marker_t s = sp_mem_begin_scratch();

  sp_io_dyn_mem_writer_t sink = sp_zero;
  sp_io_dyn_mem_writer_init(s.mem, &sink);
  sp_fmt_io(&sink.base, "{}:{};", sp_fmt_uint(verb.len), sp_fmt_str(verb));
  sp_fmt_io(&sink.base, "{}:{};", sp_fmt_uint(app->clang.len), sp_fmt_str(app->clang));
  sp_da_for(parts, it) {
    sp_fmt_io(&sink.base, "{}:{};", sp_fmt_uint(parts[it].len), sp_fmt_str(parts[it]));
  }

  sp_str_t bytes = sp_io_dyn_mem_writer_as_str(&sink);
  spn_dag_digest_t digest = spn_dag_digest(bytes.data, bytes.len);
  sp_mem_end_scratch(s);
  return digest;
}

static spn_dag_id_t lib_add_compile(lib_app_t* app, sp_str_t input) {
  lib_compile_t* ctx = sp_alloc_type(app->mem, lib_compile_t);
  ctx->app = app;
  ctx->input = input;

  sp_da(sp_str_t) parts = sp_da_new(app->mem, sp_str_t);
  sp_da_push(parts, input);
  sp_da_for(app->cflags, it) {
    sp_da_push(parts, app->cflags[it]);
  }

  spn_dag_id_t compile = spn_dag_add_action(app->g, (spn_dag_action_config_t) {
    .identity = lib_identity(app, sp_str_lit("compile"), parts),
    .execute = lib_compile_exec,
    .discover = lib_compile_discover,
    .user_data = ctx
  });
  spn_dag_action_add_input(app->g, compile, spn_dag_add_file(app->g, input));

  sp_str_t name = sp_fmt(app->mem, "{}.o", sp_fmt_str(sp_fs_get_stem(input))).value;
  ctx->obj = spn_dag_add_output(app->g, name);
  spn_dag_action_add_output(app->g, compile, ctx->obj);
  app->actions++;
  return ctx->obj;
}

static spn_dag_id_t lib_add_link(lib_app_t* app, sp_str_t verb, spn_dag_exec_fn_t exec, sp_str_t output, sp_da(spn_dag_id_t) inputs) {
  spn_dag_id_t action = spn_dag_add_action(app->g, (spn_dag_action_config_t) {
    .identity = lib_identity(app, verb, app->cflags),
    .execute = exec,
    .user_data = app
  });
  sp_da_for(inputs, it) {
    spn_dag_action_add_input(app->g, action, inputs[it]);
  }
  spn_dag_id_t out = spn_dag_add_file(app->g, output);
  spn_dag_action_add_output(app->g, action, out);
  app->actions++;
  return out;
}

static void lib_report(lib_app_t* app) {
  u32 hits = app->actions - app->executed;
  sp_os_print(sp_fmt(app->mem, "\n{} action(s): {} executed, {} restored from cache\n",
    sp_fmt_uint(app->actions), sp_fmt_uint(app->executed), sp_fmt_uint(hits)).value);
}

static sp_cli_result_t lib_build(sp_cli_t* cli) {
  lib_app_t* app = (lib_app_t*)cli->user_data;

  app->clang = sp_str_lit("clang");
  app->root = sp_str_lit(".sp-lib");
  sp_da_init(app->mem, app->cflags);
  sp_da_push(app->cflags, sp_str_lit("-g"));
  sp_da_push(app->cflags, sp_str_lit("-O0"));
  sp_da_push(app->cflags, sp_str_lit("-fPIC"));
  sp_da_push(app->cflags, sp_str_lit("-Iinclude"));

  spn_dag_store_init(&app->store, (spn_dag_store_config_t) {
    .kind = SPN_DAG_STORE_FILESYSTEM,
    .mem = app->mem,
    .dir = sp_fs_join_path(app->mem, app->root, sp_str_lit("blobs"))
  });
  spn_dag_file_cache_init(&app->files, app->mem);
  spn_dag_action_cache_init(&app->cache, app->mem, sp_fs_join_path(app->mem, app->root, sp_str_lit("strong")));
  spn_dag_discovery_init(&app->discovery, app->mem, sp_fs_join_path(app->mem, app->root, sp_str_lit("weak")));
  app->g = spn_dag_new(app->mem);

  sp_da(spn_dag_id_t) lib_objs = sp_da_new(app->mem, spn_dag_id_t);
  sp_da(spn_dag_id_t) exe_objs = sp_da_new(app->mem, spn_dag_id_t);
  sp_da(sp_fs_entry_t) sources = sp_fs_collect(app->mem, sp_str_lit("src"));
  sp_da_for(sources, it) {
    if (sources[it].kind != SP_FS_KIND_FILE || !sp_str_ends_with(sources[it].path, sp_str_lit(".c"))) {
      continue;
    }
    spn_dag_id_t obj = lib_add_compile(app, sources[it].path);
    if (sp_str_equal(sources[it].name, sp_str_lit("main.c"))) {
      sp_da_push(exe_objs, obj);
    } else {
      sp_da_push(lib_objs, obj);
    }
  }

  spn_dag_id_t archive = lib_add_link(app, sp_str_lit("archive"), lib_archive_exec, sp_str_lit("install/lib/libgeo.a"), lib_objs);
  lib_add_link(app, sp_str_lit("shared"), lib_shared_exec, sp_str_lit("install/lib/libgeo.so"), lib_objs);

  sp_da_push(exe_objs, archive);
  lib_add_link(app, sp_str_lit("exe"), lib_exe_exec, sp_str_lit("install/bin/geodemo"), exe_objs);

  lib_headers_t* headers = sp_alloc_type(app->mem, lib_headers_t);
  headers->app = app;
  headers->root = sp_str_lit("include");
  headers->pattern = sp_str_lit("**/*.h");

  sp_da(sp_str_t) header_parts = sp_da_new(app->mem, sp_str_t);
  sp_da_push(header_parts, headers->root);
  sp_da_push(header_parts, headers->pattern);
  spn_dag_id_t install = spn_dag_add_action(app->g, (spn_dag_action_config_t) {
    .identity = lib_identity(app, sp_str_lit("headers"), header_parts),
    .execute = lib_headers_exec,
    .discover = lib_headers_discover,
    .user_data = headers
  });
  spn_dag_action_add_output(app->g, install, spn_dag_add_tree(app->g, sp_str_lit("install/include")));
  app->actions++;

  spn_dag_env_t env = {
    .files = &app->files,
    .cache = &app->cache,
    .store = &app->store,
    .discovery = &app->discovery,
    .scratch = sp_fs_join_path(app->mem, app->root, sp_str_lit("tmp"))
  };
  if (spn_dag_run(app->g, &env)) {
    return sp_cli_set_error(cli, sp_str_lit("build failed"));
  }

  lib_report(app);
  return SP_CLI_OK;
}

s32 main(s32 num_args, const c8** args) {
  lib_app_t app = sp_zero;
  app.mem = sp_mem_os_new();

  sp_cli_cmd_t root = {
    .name = "sp-lib",
    .summary = "Build and install the geo demo library (content-addressed DAG)",
    .handler = lib_build,
  };

  return sp_cli_main((sp_cli_desc_t) {
    .root = &root,
    .num_args = num_args,
    .args = args,
    .user_data = &app,
  });
}
