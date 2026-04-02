// SINGLE HEADER
#define SP_MAIN
#define SP_IMPLEMENTATION
#include "sp.h"
#include "sp/coff.h"
#include "sp/elf.h"
#define SP_GLOB_IMPLEMENTATION
#include "sp/glob.h"

#define TOML_IMPLEMENTATION
#include "toml.h"

// STANDARD
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

// SPN
#include "spn.h"

#include "toolchain/types.h"
#include "unit/types.h"

#include "app/app.h"
#include "cli/cli.h"
#include "ctx/types.h"
#include "event/event.h"
#include "event/log.h"
#include "external/git.h"
#include "git/key.h"
#include "index/index.h"
#include "intern.h"
#include "lock/lock.h"
#include "log/log.h"
#include "ordered_map.h"
#include "pkg/load.h"
#include "pkg/pkg.h"
#include "pkg/mutate.h"
#include "semver/types.h"
#include "session/session.h"
#include "spn.embed.h"
#include "sp/io.h"
#include "sp/macro.h"
#include "sp/os.h"
#include "task/task.h"
#include "tui/tui.h"
#include "unit/build.h"
#include "version.h"

#include <sys/stat.h>

spn_app_t app;
spn_ctx_t spn;

void on_signal(sp_os_signal_t signal) {
  switch (signal) {
    case SP_OS_SIGNAL_INTERRUPT: {
      sp_atomic_s32_set(&spn.sp->shutdown, 1);
      break;
    }
    case SP_OS_SIGNAL_ABORT:
    case SP_OS_SIGNAL_TERMINATE: {
      break;
    }
  }
}

sp_app_result_t spn_init(sp_app_t* sp) {
  spn.sp = sp;

  spn.intern = sp_intern_new();
  spn.arena = sp_mem_arena_new_ex(256, SP_MEM_ARENA_MODE_NO_REALLOC, 1);

  sp_os_register_signal_handler(SP_OS_SIGNAL_INTERRUPT, on_signal);

  spn.logger.out = sp_io_writer_from_fd(1, SP_IO_CLOSE_MODE_NONE);
  spn.logger.err = sp_io_writer_from_fd(2, SP_IO_CLOSE_MODE_NONE);

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
  spn.paths.bin = sp_fs_get_bin_path();

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
          .kind = SPN_ERR_MANIFEST_PARSE,
          .manifest_parse = {
            .path = spn.paths.config,
          },
        },
      });
    }

    if (toml) {
      toml_array_t* indexes = toml_table_array(toml, "index");
      if (indexes) {
        spn_toml_arr_for(indexes, n) {
          toml_table_t* it = toml_array_table(indexes, n);
          spn_index_t index = SP_ZERO_INITIALIZE();
          spn_err_union_t idx_err = spn_index_load(it, sp_str_lit("index"), n, &index);
          if (idx_err.kind == SPN_ERR_MANIFEST_FIELD) {
            spn_log_error("invalid index in config: {:fg brightcyan} expected {:fg brightyellow}, got {:fg brightred}",
              SP_FMT_STR(idx_err.manifest_field.path),
              SP_FMT_STR(idx_err.manifest_field.expected),
              SP_FMT_STR(idx_err.manifest_field.actual)
            );
            return SP_APP_ERR;
          } else if (idx_err.kind) {
            spn_log_error("failed to load index from config");
            return SP_APP_ERR;
          }
          sp_da_push(spn.indexes, index);
        }
      }
    }
  }

  sp_str_t index_dir = sp_fs_join_path(spn.paths.storage, sp_str_lit("index"));
  sp_fs_create_dir(index_dir);

  bool has_core_index = false;
  sp_da_for(spn.indexes, i) {
    if (sp_str_equal(spn.indexes[i].name, sp_str_lit("core"))) {
      has_core_index = true;
      break;
    }
  }

  if (!has_core_index) {
    sp_da_push(spn.indexes, ((spn_index_t) {
      .name = sp_str_lit("core"),
      .url = sp_str_lit("https://github.com/tspader/spandex.git"),
      .protocol = SPN_INDEX_PROTOCOL_GIT,
      .kind = SPN_INDEX_BUILTIN,
    }));
  }

  sp_da_for(spn.indexes, i) {
    if (spn.indexes[i].protocol == SPN_INDEX_PROTOCOL_FILESYSTEM) {
      spn.indexes[i].location = spn.indexes[i].url;
    } else {
      spn.indexes[i].location = sp_fs_join_path(index_dir, spn_git_db_key(spn.indexes[i].url));
    }
  }

  sp_da_for(spn.indexes, idx) {
    spn_index_init(&spn.indexes[idx]);
    spn_index_sync(&spn.indexes[idx]);
  }

  // Find the cache directory after the config has been fully loaded
  spn.paths.runtime = sp_fs_join_path(spn.paths.storage, SP_LIT("runtime"));
  spn.paths.include = sp_fs_join_path(spn.paths.runtime, sp_str_lit("include"));
  spn.paths.version = sp_fs_join_path(spn.paths.runtime, SP_LIT("version.stamp"));
  spn.paths.log = sp_fs_join_path(spn.paths.storage, SP_LIT("log"));
  spn.paths.cache = sp_fs_join_path(spn.paths.storage, SP_LIT("cache"));
  spn.paths.source = sp_fs_join_path(spn.paths.cache, SP_LIT("source"));
  spn.paths.build = sp_fs_join_path(spn.paths.cache, SP_LIT("build"));
  spn.paths.store = sp_fs_join_path(spn.paths.cache, SP_LIT("store"));

  sp_fs_create_dir(spn.paths.log);

  spn_event_log_init();
  {
    sp_str_t jsonl_path = sp_fs_join_path(spn.paths.log, sp_str_lit("build.jsonl"));
    sp_fs_create_file(jsonl_path);
    spn.logger.jsonl = sp_io_writer_from_file(jsonl_path, SP_IO_WRITE_MODE_OVERWRITE);
  }

  sp_fs_create_dir(spn.paths.cache);
  sp_fs_create_dir(spn.paths.source);
  sp_fs_create_dir(spn.paths.build);
  sp_fs_create_dir(spn.paths.store);
  sp_fs_create_dir(spn.paths.bin);
  sp_fs_create_dir(spn.paths.tools.dir);

  // @spader
  // spn_extract_runtime()

  sp_str_t version = sp_zero_initialize();
  if (sp_fs_exists(spn.paths.version)) {
    version = sp_io_read_file(spn.paths.version);
    version = sp_str_trim(version);
  }

  if (!sp_str_equal_cstr(version, SPN_VERSION)) {
    sp_fs_remove_dir(spn.paths.runtime);
    sp_fs_create_dir(spn.paths.runtime);
    sp_fs_create_dir(spn.paths.include);
    sp_fs_create_dir(sp_fs_join_path(spn.paths.runtime, sp_str_lit("lib")));

    sp_glob_set_t* glob = sp_glob_set_new();
    sp_glob_set_add(glob, "include/*");
    sp_glob_set_add(glob, "*.o");
    sp_glob_set_add(glob, "*.a");
    sp_glob_set_build(glob);

    sp_carr_for(spn_embed_manifest, it) {
      spn_embed_entry_t entry = spn_embed_manifest[it];
      sp_str_t path = sp_str_view(entry.path);
      if (sp_glob_set_match(glob, path)) {
        path = sp_fs_join_path(spn.paths.runtime, path);
        sp_io_writer_t io = sp_io_writer_from_file(path, SP_IO_WRITE_MODE_OVERWRITE);
        sp_io_write(&io, entry.data, entry.size);
        sp_io_writer_close(&io);
      }
    }

    {
      sp_io_writer_t io = sp_io_writer_from_file(spn.paths.version, SP_IO_WRITE_MODE_OVERWRITE);
      sp_io_write_cstr(&io, SPN_VERSION);
      sp_io_writer_close(&io);
    }
  }

  spn_cli_t* cli = &spn.cli;
  spn.cli.usage = spn_cli();

  spn_cli_parser_t parser = {
    .args = spn.args + 1,
    .num_args = spn.num_args - 1,
    .cmd = &cli->usage
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

  // @spader
  // We switch here because most operations work on a project, and start by loading the manifest. But for
  // `spn run`, you can point it at a loose C file. This feels kind of ad hoc, like we're throwing away
  // the fact that we're not in a project for the sake of false uniformity.
  //
  if (sp_fs_exists(spn.paths.manifest)) {
    if (spn_pkg_from_manifest(&app.package, spn.paths.manifest)) {
      spn_poll(sp);
      return SP_APP_ERR;
    }
  } else {
    app.package = spn_pkg_new(sp_str_lit(""));
    spn_pkg_set_manifest(&app.package, spn.paths.manifest);
  }

  app.paths.dir = app.package.paths.root;
  app.paths.lock = sp_fs_join_path(app.paths.dir, SP_LIT("spn.lock"));

  if (sp_fs_exists(app.paths.lock)) {
    sp_opt_set(app.lock, spn_lock_file_load(app.paths.lock, spn.events));
  }

  spn_session_t* session = &app.session;
  session->pkg = &app.package;
  session->paths.root = app.package.paths.root;
  session->paths.build = sp_fs_join_path(session->paths.root, sp_str_lit("build"));
  session->env = sp_env_capture();
  sp_mutex_init(&session->mutex, SP_MUTEX_PLAIN);

  // Build the list of available toolchains
  sp_om_for(app.package.toolchains, it) {
    spn_toolchain_entry_t entry = *sp_om_at(app.package.toolchains, it);
    sp_str_ht_insert(session->toolchains, entry.name, entry);
  }
  spn_toolchain_entry_t builtin_toolchains[] = {
    {
      .name = sp_str_lit("builtin"),
      .kind = SPN_TOOLCHAIN_INDEX,
      .request = {
        .package = sp_str_lit("core/zig"),
        .range = spn_semver_any()
      },
    },
    {
      .name = sp_str_lit("gcc"),
      .kind = SPN_TOOLCHAIN_BUILTIN,
      .info = {
        .name = sp_str_lit("gcc"),
        .compiler = { .program = sp_str_lit("gcc") },
        .linker = { .program = sp_str_lit("gcc") },
        .archiver = { .program = sp_str_lit("ar") },
        .driver = SPN_CC_DRIVER_GCC,
      },
    },
    {
      .name = sp_str_lit("clang"),
      .kind = SPN_TOOLCHAIN_BUILTIN,
      .info = {
        .name = sp_str_lit("clang"),
        .compiler = { .program = sp_str_lit("clang") },
        .linker = { .program = sp_str_lit("clang") },
        .archiver = { .program = sp_str_lit("ar") },
        .driver = SPN_CC_DRIVER_GCC,
      },
    },
    {
      .name = sp_str_lit("tcc"),
      .kind = SPN_TOOLCHAIN_BUILTIN,
      .info = {
        .name = sp_str_lit("tcc"),
        .compiler = { .program = sp_str_lit("tcc") },
        .linker = { .program = sp_str_lit("tcc") },
        .archiver = { .program = sp_str_lit("ar") },
        .driver = SPN_CC_DRIVER_GCC,
      },
    },
  };
  sp_carr_for(builtin_toolchains, it) {
    spn_toolchain_entry_t entry = builtin_toolchains[it];
    sp_str_ht_insert(session->toolchains, entry.name, entry);
  }

  sp_str_ht_for_kv(session->toolchains, it) {
    app.config.toolchain = it.val;
    break;
  }
  sp_assert(app.config.toolchain);

  // @spader These need to be done like toolchains
  if (sp_om_empty(app.package.profiles)) {
    spn_profile_t profiles[] = {
      {
        .name = sp_str_lit("debug"),
        .linkage = SPN_LIB_KIND_SHARED,
        .standard = SPN_C11,
        .mode = SPN_BUILD_MODE_DEBUG,
        .kind = SPN_PROFILE_BUILTIN,
        .toolchain = sp_str_lit("system")
      },
      {
        .name = sp_str_lit("release"),
        .linkage = SPN_LIB_KIND_SHARED,
        .standard = SPN_C11,
        .mode = SPN_BUILD_MODE_RELEASE,
        .kind = SPN_PROFILE_BUILTIN,
        .toolchain = sp_str_lit("system")
      }
    };
    sp_carr_for(profiles, it) {
      spn_pkg_add_profile_ex(&app.package, profiles[it]);
    }
  }

  switch (spn_cli_dispatch(&parser, cli)) {
    case SP_APP_CONTINUE: return SP_APP_CONTINUE;
    case SP_APP_QUIT: return SP_APP_QUIT;
    case SP_APP_ERR: return SP_APP_ERR;
  }

  sp_unreachable_return(SP_APP_ERR);
}

sp_app_result_t spn_poll(sp_app_t* sp) {
  sp_da(spn_build_event_t) events = spn_event_buffer_drain(spn.events);

  sp_da_for(events, it) {
    spn_build_event_t* event = &events[it];

    // map raw thread IDs to short sequential IDs
    {
      static sp_ht(u64, u32) thread_map = SP_NULLPTR;
      static u32 next_thread_id = 0;
      if (!sp_ht_key_exists(thread_map, event->thread_id)) {
        sp_ht_insert(thread_map, event->thread_id, next_thread_id++);
      }
      event->thread_id = *sp_ht_getp(thread_map, event->thread_id);
    }

    // process anything we need to do special per-event
    switch (event->kind) {
      case SPN_EVENT_ADD_TARGET: {
        spn.tui.info.max_name = sp_max(spn.tui.info.max_name, event->target.name.len);
        break;
      }
      default: break;
    }

    // write jsonl (all events, unfiltered)
    spn_event_log_jsonl(&spn.logger.jsonl, event);
    if (event->io) {
      spn_event_log_jsonl(&event->io->jsonl, event);
    }

    // write to tui (filtered by verbosity)
    if (spn_build_event_get_verbosity(event->kind) <= spn.verbosity) {
      sp_io_write_line(&spn.logger.err, spn_tui_render_event(event, spn.tui.info.max_name));
    }
  }

  return SP_APP_CONTINUE;
}

sp_app_result_t spn_update(sp_app_t* sp) {
  if (sp_atomic_s32_get(&sp->shutdown)) {
    return SP_APP_QUIT;
  }

  spn_task_executor_t* task = &app.tasks;
  s32 kind = task->data[task->index];
  spn_task_result_t result = SPN_TASK_DONE;

  switch (kind) {
    case SPN_TASK_KIND_NONE: {
      return SP_APP_QUIT;
    }
    case SPN_TASK_KIND_RESOLVE: {
      result = spn_task_resolve(&app);
      break;
    }
    case SPN_TASK_KIND_SYNC: {
      if (!task->initted) {
        result = spn_task_sync_init(&app);
        break;
      }
      result = spn_task_sync_update(&app);
      break;
    }
    case SPN_TASK_KIND_CONFIGURE: {
      if (!task->initted) spn_task_init_configure_graph(&app);
      result = spn_task_update_configure_graph(&app);
      break;
    }
    case SPN_TASK_KIND_PREPARE_BUILD_GRAPH: {
      result = spn_task_prepare_build_graph(&app);
      break;
    }
    case SPN_TASK_KIND_RUN_BUILD_GRAPH: {
      if (!task->initted) spn_task_init_build_graph(&app);
      result = spn_task_run_build_graph(&app);
      break;
    }
    case SPN_TASK_KIND_RENDER_BUILD_GRAPH: {
      result = spn_task_graph(&app);
      break;
    }
    case SPN_TASK_KIND_RUN: {
      result = spn_task_run(&app);
      break;
    }
    case SPN_TASK_KIND_GENERATE: {
      result = spn_task_generate(&app);
      break;
    }
    case SPN_TASK_KIND_WHICH: {
      result = spn_task_which(&app, &spn.cli.which);
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

  if (!app.session.pkg) return;

  spn_pkg_unit_t* root = spn_session_find_root(&app.session);
  sp_om_for(app.session.units.packages, it) {
    spn_pkg_unit_t* unit = sp_om_at(app.session.units.packages, it);

    sp_fs_create_sym_link(
      unit->ctx.paths.logs.build,
      sp_fs_join_path(root->ctx.paths.work, spn_build_ctx_get_build_log_name(&unit->ctx))
    );
    sp_fs_create_sym_link(
      unit->ctx.paths.logs.jsonl,
      sp_fs_join_path(root->ctx.paths.work, spn_build_ctx_get_jsonl_log_name(&unit->ctx))
    );

    sp_om_for(unit->targets, t) {
      spn_target_unit_t* target = sp_om_at(unit->targets, t);
      sp_io_writer_close(&target->logs.build);
      sp_io_writer_close(&target->logs.test);
      sp_io_writer_close(&target->logs.jsonl);
    }
    sp_io_writer_close(&unit->ctx.logs.build);
    sp_io_writer_close(&unit->ctx.logs.test);
    sp_io_writer_close(&unit->ctx.logs.jsonl);
  }

  sp_io_writer_close(&root->ctx.logs.build);
  sp_io_writer_close(&root->ctx.logs.test);
  sp_io_writer_close(&root->ctx.logs.jsonl);
  sp_io_writer_close(&spn.logger.jsonl);
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
