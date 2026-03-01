// EXTERNAL
#define SP_MAIN
#define SP_IMPLEMENTATION
#include "sp.h"

#define TOML_IMPLEMENTATION
#include "toml.h"

// STANDARD
#include <setjmp.h>

#ifdef _WIN32
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif

  #ifndef NOMINMAX
    #define NOMINMAX
  #endif

  #include <windows.h>
  #include <shlobj.h>
  #include <commdlg.h>
  #include <shellapi.h>
  #include <conio.h>
  #include <io.h>
#endif

#if defined(SP_POSIX)
  #include <dlfcn.h>
  #include <signal.h>
  #include <unistd.h>
#endif

// SPN
#include "app.h"
#include "cli.h"
#include "ctx.h"
#include "filter.h"
#include "gen.h"
#include "intern.h"
#include "lock.h"
#include "option.h"
#include "index.h"
#include "resolve.h"
#include "signal.spn.h"
#include "spn.h"
#include "log.h"
#include "pkg.h"
#include "profile.h"
#include "semver.h"
#include "graph.h"
#include "node.h"
#include "session.h"
#include "ordered_map.h"
#include "spinner.h"
#include "terminal.h"
#include "tui.h"
#include "event.h"
#include "external/cJSON.h"
#include "external/git.h"
#include "external/mz.h"
#include "external/tcc.h"
#include "external/tom.h"
#include "sp/color.h"
#include "sp/ht.h"
#include "sp/io.h"
#include "sp/macro.h"
#include "sp/os.h"
#include "sp/ps.h"
#include "sp/str.h"
#include "task/task.h"

#include "spn.embed.h"

spn_app_t app;
spn_ctx_t spn;

sp_app_result_t spn_init(sp_app_t* sp) {
  spn.sp = sp;

  spn.intern = sp_intern_new();
  spn.arena = sp_mem_arena_new_ex(256, SP_MEM_ARENA_MODE_NO_REALLOC, 1);

  spn_install_signal_handlers();
  spn.logger.out = sp_io_writer_from_fd(STDOUT_FILENO, SP_IO_CLOSE_MODE_NONE);
  spn.logger.err = sp_io_writer_from_fd(STDERR_FILENO, SP_IO_CLOSE_MODE_NONE);

  spn.env = sp_alloc_type(sp_env_t);
  *spn.env = sp_env_capture();

  spn.log_level = SPN_LOG_LEVEL_INFO;
  sp_str_t log_level = sp_env_get(spn.env, sp_str_lit("SPN_LOG_LEVEL"));
  if (!sp_str_empty(log_level)) {
    spn.log_level = spn_log_level_from_str(log_level);
  }

  spn_tui_init(&spn.tui, SPN_OUTPUT_MODE_INTERACTIVE);

  spn.events = spn_event_buffer_new();

  sp_atomic_s32_set(&spn.control, 0);


  spn.paths.cwd = sp_fs_get_cwd();
  spn.paths.bin = sp_os_get_bin_path();

  sp_str_t storage = sp_env_get(spn.env, sp_str_lit("SPN_STORAGE_DIR"));
  if (sp_str_empty(storage)) {
    storage = sp_fs_join_path(sp_fs_get_storage_path(), sp_str_lit("spn"));
  }

  spn.paths.storage = storage;
  spn.paths.tools.dir = sp_fs_join_path(spn.paths.storage, sp_str_lit("tools"));
  spn.paths.tools.manifest = sp_fs_join_path(spn.paths.tools.dir, sp_str_lit("spn.toml"));
  spn.paths.tools.lock = sp_fs_join_path(spn.paths.storage, sp_str_lit("spn.lock"));

  // CONFIG
  sp_str_t config_dir = sp_env_get(spn.env, sp_str_lit("SPN_CONFIG_DIR"));
  if (sp_str_empty(config_dir)) {
    config_dir = sp_fs_get_config_path();
  }
  spn.paths.config_dir = sp_fs_join_path(config_dir, SP_LIT("spn"));
  spn.paths.config = sp_fs_join_path(spn.paths.config_dir, SP_LIT("spn.toml"));

  if (sp_fs_exists(spn.paths.config)) {
    bool parse_error = false;
    toml_table_t* toml = spn_toml_parse_ex(spn.paths.config, &parse_error);
    if (parse_error) {
      spn_event_buffer_push_ex(spn.events, SP_NULLPTR, SP_NULLPTR, (spn_build_event_t) {
        .kind = SPN_EVENT_ERR,
        .err = {
          .code = SPN_ERROR,
          .kind = SPN_ERR_KIND_MANIFEST_PARSE,
          .manifest_parse = {
            .path = spn.paths.config,
          },
        },
      });
    }

    if (toml) {
      toml_value_t dir = toml_table_string(toml, "spn");
      if (dir.ok) {
        spn.paths.spn = sp_str_view(dir.u.s);
      }

      toml_array_t* registries = toml_table_array(toml, "index");
      if (registries) {
        spn_toml_arr_for(registries, n) {
          toml_table_t* it = toml_array_table(registries, n);
          spn_index_t registry = {
            .location = spn_toml_str(it, "location"),
            .kind = SPN_INDEX_WORKSPACE
          };

          sp_dyn_array_push(spn.indexes, registry);
        }
      }
    }
  }

  if (!sp_str_valid(spn.paths.spn)) {
    spn.paths.spn = sp_fs_join_path(spn.paths.storage, sp_str_lit("spn"));
  }

  spn.paths.index = sp_fs_join_path(spn.paths.spn, sp_str_lit("packages"));
  spn.paths.include = sp_fs_join_path(spn.paths.spn, sp_str_lit("include"));

  if (!sp_fs_exists(spn.paths.spn)) {
    sp_str_t url = SP_LIT("https://github.com/tspader/spn.git");
    SP_LOG(
      "Cloning index from {:fg brightcyan} to {:fg brightcyan}",
      SP_FMT_STR(url),
      SP_FMT_STR(spn.paths.spn)
    );

    SP_ASSERT(!spn_git_clone(url, spn.paths.spn));
  }

  // Add the builtin index
  sp_dyn_array_push(spn.indexes, ((spn_index_t) {
    .location = spn.paths.index,
    .kind = SPN_INDEX_BUILTIN
  }));

  // Find the cache directory after the config has been fully loaded
  spn.paths.runtime = sp_fs_join_path(spn.paths.storage, SP_LIT("runtime"));
  spn.paths.log = sp_fs_join_path(spn.paths.storage, SP_LIT("log"));
  spn.paths.cache = sp_fs_join_path(spn.paths.storage, SP_LIT("cache"));
  spn.paths.source = sp_fs_join_path(spn.paths.cache, SP_LIT("source"));
  spn.paths.build = sp_fs_join_path(spn.paths.cache, SP_LIT("build"));
  spn.paths.store = sp_fs_join_path(spn.paths.cache, SP_LIT("store"));

  sp_fs_create_dir(spn.paths.log);
  sp_fs_create_dir(spn.paths.cache);
  sp_fs_create_dir(spn.paths.source);
  sp_fs_create_dir(spn.paths.build);
  sp_fs_create_dir(spn.paths.store);
  sp_fs_create_dir(spn.paths.bin);
  sp_fs_create_dir(spn.paths.tools.dir);

  // @spader
  // spn_extract_runtime()
  if (!sp_fs_exists(spn.paths.runtime)) {
    sp_fs_create_dir(spn.paths.runtime);
    sp_fs_create_dir(sp_fs_join_path(spn.paths.runtime, sp_str_lit("include")));

    const struct { sp_str_t path; const u8* data; u64 size; } runtime [] = {
      { sp_str_lit("bcheck.o"), bcheck_o, bcheck_o_size },
      { sp_str_lit("bt-exe.o"), bt_exe_o, bt_exe_o_size },
      { sp_str_lit("bt-log.o"), bt_log_o, bt_log_o_size },
      { sp_str_lit("libtcc1.a"), libtcc1_a, libtcc1_a_size },
      { sp_str_lit("runmain.o"), runmain_o, runmain_o_size },
      { sp_str_lit("run_nostdlib.o"), run_nostdlib_o, run_nostdlib_o_size },
      { sp_str_lit("include/float.h"), include_float_h, include_float_h_size },
      { sp_str_lit("include/stdalign.h"), include_stdalign_h, include_stdalign_h_size },
      { sp_str_lit("include/stdarg.h"), include_stdarg_h, include_stdarg_h_size },
      { sp_str_lit("include/stdatomic.h"), include_stdatomic_h, include_stdatomic_h_size },
      { sp_str_lit("include/stdbool.h"), include_stdbool_h, include_stdbool_h_size },
      { sp_str_lit("include/stddef.h"), include_stddef_h, include_stddef_h_size },
      { sp_str_lit("include/stdnoreturn.h"), include_stdnoreturn_h, include_stdnoreturn_h_size },
      { sp_str_lit("include/tccdefs.h"), include_tccdefs_h, include_tccdefs_h_size },
      { sp_str_lit("include/tcclib.h"), include_tcclib_h, include_tcclib_h_size },
      { sp_str_lit("include/tgmath.h"), include_tgmath_h, include_tgmath_h_size },
      { sp_str_lit("include/varargs.h"), include_varargs_h, include_varargs_h_size },
    };
    sp_carr_for(runtime, it) {
      sp_str_t path = sp_fs_join_path(spn.paths.runtime, runtime[it].path);
      sp_io_writer_t io = sp_io_writer_from_file(path, SP_IO_WRITE_MODE_OVERWRITE);
      sp_io_write(&io, runtime[it].data, runtime[it].size);
    }
  }

  spn_cli_t* cli = &spn.cli;
  spn.cli.cmd = spn_cli();

  spn_cli_parser_t parser = {
    .args = spn.args + 1,
    .num_args = spn.num_args - 1,
    .cmd = &cli->cmd
  };

  switch (spn_cli_parse(&parser)) {
    case SP_APP_CONTINUE: break;
    case SP_APP_QUIT: spn_cli_help(&parser); return SP_APP_QUIT;
    case SP_APP_ERR: {
      if (sp_str_valid(parser.err)) {
        sp_io_write_line(&spn.logger.err, parser.err);
      }

      spn_cli_help(&parser);
      return SP_APP_ERR;
    }
  }

  // Initialize verbosity from CLI flags
  if (cli->quiet) {
    spn.verbosity = SPN_VERBOSITY_QUIET;
  } else if (cli->verbose) {
    spn.verbosity = SPN_VERBOSITY_VERBOSE;
  } else {
    spn.verbosity = SPN_VERBOSITY_NORMAL;
  }

  if (sp_str_valid(cli->project_dir)) {
    spn.paths.project = sp_fs_canonicalize_path(cli->project_dir);
  }
  else {
    spn.paths.project = sp_str_copy(spn.paths.cwd);
  }
  spn.paths.manifest = sp_fs_join_path(spn.paths.project, sp_str_lit("spn.toml"));

  app = SP_ZERO_STRUCT(spn_app_t);
  spn_app_init(&app);
  if (spn_app_load(&app, spn.paths.manifest)) {
    spn_poll(sp);
    SP_EXIT_FAILURE();
  }

  switch (spn_cli_dispatch(&parser, cli)) {
    case SP_APP_CONTINUE: return SP_APP_CONTINUE;
    case SP_APP_QUIT: return SP_APP_QUIT;
    case SP_APP_ERR: return SP_APP_ERR;
  }

  sp_unreachable_return(SP_APP_ERR);
}

sp_app_result_t spn_poll(sp_app_t* sp) {
  spn_app_t* app = (spn_app_t*)sp->user_data;
  sp_da(spn_build_event_t) events = spn_event_buffer_drain(spn.events);

  sp_da_for(events, it) {
    spn_build_event_t* event = &events[it];

    // process anything we need to do special per-event
    switch (event->kind) {
      case SPN_EVENT_TARGET_BUILD: {
        spn_build_ctx_log(event->io, event->target.build.args);
        break;
      }
      case SPN_EVENT_RESOLVE: {
        sp_str_ht_for(app->resolver->resolved, it) {
          sp_str_t name = *sp_str_ht_it_getkp(app->resolver->resolved, it);
          spn_resolved_pkg_t resolved = *sp_str_ht_it_getp(app->resolver->resolved, it);
          spn_build_ctx_log(event->io, sp_format(
            "Resolved {} to version {}",
            SP_FMT_STR(resolved.pkg->name),
            SP_FMT_STR(spn_semver_to_str(resolved.version))
          ));
        }
        break;
      }
      case SPN_EVENT_ADD_TARGET: {
        spn.tui.info.max_name = sp_max(spn.tui.info.max_name, event->target.add.name.len);
        break;
      }
      default: {
        if (event->io) {
          spn_build_ctx_log(event->io, sp_format("event: {}", SP_FMT_STR(spn_build_event_kind_to_str(event->kind))));
        }
        break;
      }
    }

    // write to tui (filtered by verbosity)
    if (spn_build_event_get_verbosity(event->kind) <= spn.verbosity) {
      sp_io_write_line(&spn.logger.err, spn_tui_render_event(event, spn.tui.info.max_name));
    }
  }

  return SP_APP_CONTINUE;
}

sp_app_result_t spn_update(sp_app_t* sp) {
  spn_app_t* app = (spn_app_t*)sp->user_data;

  if (sp_atomic_s32_get(&sp->shutdown)) {
    return SP_APP_QUIT;
  }

  spn_task_executor_t* task = &app->tasks;
  s32 kind = task->data[task->index];
  spn_task_result_t result = SPN_TASK_DONE;

  // if (!task->initted) {
  //   spn_event_buffer_push_ex(spn.events, app->ev, spn_build_event_t event)
  // }

  switch (kind) {
    case SPN_TASK_KIND_NONE: {
      return SP_APP_QUIT;
    }
    case SPN_TASK_KIND_RESOLVE: {
      result = spn_task_resolve(app);
      break;
    }
    case SPN_TASK_KIND_SYNC: {
      if (!task->initted) spn_task_sync_init(app);
      result = spn_task_sync_update(app);
      break;
    }
    case SPN_TASK_KIND_CONFIGURE: {
      if (!task->initted) spn_task_init_configure_graph(app);
      result = spn_task_update_configure_graph(app);
      break;
    }
    case SPN_TASK_KIND_PREPARE_BUILD_GRAPH: {
      result = spn_task_prepare_build_graph(app);
      break;
    }
    case SPN_TASK_KIND_RUN_BUILD_GRAPH: {
      if (!task->initted) spn_task_init_build_graph(app);
      result = spn_task_run_build_graph(app);
      break;
    }
    case SPN_TASK_KIND_RENDER_BUILD_GRAPH: {
      result = spn_task_graph(app);
      break;
    }
    case SPN_TASK_KIND_RUN: {
      result = spn_task_run_tests(app);
      break;
    }
    case SPN_TASK_KIND_GENERATE: {
      result = spn_task_generate(app);
      break;
    }
    case SPN_TASK_KIND_WHICH: {
      result = spn_task_which(app);
      break;
    }
    case SPN_TASK_KIND_COUNT: {
      SP_UNREACHABLE();
      break;
    }
  }

  task->initted = true;

  switch (result) {
    case SPN_TASK_ERROR: {
      spn_poll(sp);
      return SP_APP_ERR;
    }
    case SPN_TASK_CONTINUE: return SP_APP_CONTINUE;
    case SPN_TASK_DONE: {
      task->index++;
      task->initted = false;
      return SP_APP_CONTINUE;
    }
  }

  sp_unreachable_return(SP_APP_ERR);
}

void spn_deinit(sp_app_t* sp) {
  spn_app_t* app = (spn_app_t*)sp->user_data;

  switch (spn.tui.mode) {
    case SPN_OUTPUT_MODE_INTERACTIVE: {
      // sp_tui_restore(&spn.tui);
      // sp_tui_show_cursor();
      // sp_tui_home();
      sp_tui_flush();
      break;
    }
    case SPN_OUTPUT_MODE_NONINTERACTIVE: {
      break;
    }
    case SPN_OUTPUT_MODE_QUIET: {
      break;
    }
    case SPN_OUTPUT_MODE_NONE: {
      break;
    }
  }

  if (!app->session.pkg) return;

  spn_pkg_unit_t* root = spn_session_find_root(&app->session);
  sp_om_for(app->session.units.packages, it) {
    spn_pkg_unit_t* unit = sp_om_at(app->session.units.packages, it);

    sp_fs_create_sym_link(
      unit->ctx.paths.logs.build,
      sp_fs_join_path(root->ctx.paths.work, spn_build_ctx_get_build_log_name(&unit->ctx))
    );

    sp_om_for(unit->targets, t) {
      spn_target_unit_t* target = sp_om_at(unit->targets, t);
      sp_io_writer_close(&target->logs.build);
      sp_io_writer_close(&target->logs.test);
    }
    sp_io_writer_close(&unit->ctx.logs.build);
    sp_io_writer_close(&unit->ctx.logs.test);
  }

  sp_io_writer_close(&root->ctx.logs.build);
  sp_io_writer_close(&root->ctx.logs.test);
}

sp_app_config_t sp_main(s32 num_args, const c8** args) {
  spn = (spn_ctx_t) {
    .num_args = num_args,
    .args = args
  };
  app = SP_ZERO_STRUCT(spn_app_t);

  return (sp_app_config_t) {
    .user_data = &app,
    .on_init = spn_init,
    .on_poll = spn_poll,
    .on_update = spn_update,
    .on_deinit = spn_deinit,
    .fps = 144,
  };
}
