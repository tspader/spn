#include "error/types.h"
#include "sp.h"

// STANDARD
#ifdef SP_WIN32
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

#include "ctx/ctx.h"
#include "ctx/types.h"
#include "forward/types.h"
#include "unit/types.h"

#include "app/app.h"
#include "codegen/codegen.h"
#include "codegen/lower.h"
#include "codegen/gen/config.gen.h"
#include "codegen/gen/manifest.gen.h"
#include "cli/cli.h"
#include "event/event.h"
#include "event/log.h"
#include "external/tom.h"
#include "git/key.h"
#include "index/index.h"
#include "intern/intern.h"
#include "lock/lock.h"
#include "log/lazy/lazy.h"
#include "log/log.h"
#include "sp/sp_om.h"
#include "profile/profile.h"
#include "session/session.h"
#include "spn.embed.h"
#include "toolchain/toolchain.h"
#include "triple/triple.h"
#include "sp/io.h"
#include "sp/macro.h"
#include "sp/os.h"
#include "sp/sp_glob.h"
#include "task/task.h"
#include "toml/loader.h"
#include "tui/tui.h"
#include "version.h"

// SINGLE HEADER
#define SP_IMPLEMENTATION
#include "sp.h"
#include "sp/atomic_file.h"
#include "sp/prompt.h"
#include "sp/sp_cli.h"
#include "sp/sp_template.h"
#include "sp/coff.h"
#include "sp/sp_elf.h"
#include "sp/macho.h"

#define SP_GRAPH_IMPLEMENTATION
#include "sp/sp_graph.h"

#define SP_MATH_IMPLEMENTATION
#include "sp/sp_math.h"

#define SP_GLOB_IMPLEMENTATION
#include "sp/sp_glob.h"

#define TOML_IMPLEMENTATION
#include "toml.h"

spn_app_t app;
spn_ctx_t spn;

void on_signal(sp_os_signal_t signal, void* userdata) {
  (void)userdata;
  switch (signal) {
    case SP_OS_SIGNAL_INTERRUPT: {
      sp_atomic_s32_set(&spn.aborted, 1);
      sp_atomic_s32_set(&spn.sp->shutdown, 1);
      break;
    }
    case SP_OS_SIGNAL_ABORT:
    case SP_OS_SIGNAL_TERMINATE: {
      break;
    }
  }
}

static sp_str_t join_path(sp_str_t base, const c8* dir) {
  return sp_fs_join_path(spn.heap, base, sp_cstr_as_str(dir));
}

static sp_str_t env_or(const c8* env, sp_str_t fallback) {
  sp_str_t path = sp_env_get(spn.env, sp_cstr_as_str(env));
  return sp_str_empty(path) ? fallback : path;
}

spn_err_t extract_runtime() {
  spn_err_t err = SPN_OK;
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();

  sp_str_t version = sp_zero;
  if (sp_fs_exists(spn.paths.version)) {
    sp_io_read_file(scratch.mem, spn.paths.version, &version);
    version = sp_str_trim(version);
  }

  // @spader Use SHA256 for this
  // The stamp must change whenever the embedded runtime does, not just on
  // release; otherwise dev builds compile scripts against a stale extraction
  sp_hash_t runtime_hash = 0;
  sp_carr_for(spn_embed_manifest, it) {
    spn_embed_entry_t entry = spn_embed_manifest[it];
    sp_hash_t hashes [] = {
      runtime_hash,
      sp_hash_cstr(entry.path),
      sp_hash_bytes(entry.data, entry.size, 0),
    };
    runtime_hash = sp_hash_combine(hashes, sp_carr_len(hashes));
  }
  sp_str_t stamp = sp_fmt(scratch.mem, "{}:{}", sp_fmt_cstr(SPN_VERSION), sp_fmt_uint(runtime_hash)).value;

  if (!sp_str_equal(version, stamp)) {
    sp_fs_remove_dir(spn.paths.runtime);
    sp_fs_create_dir(spn.paths.runtime);
    sp_fs_create_dir(spn.paths.include);

    sp_glob_set_t* glob = sp_glob_set_new(scratch.mem);
    sp_glob_set_add(glob, "include/*");
    sp_glob_set_build(glob);

    sp_carr_for(spn_embed_manifest, it) {
      spn_embed_entry_t entry = spn_embed_manifest[it];
      sp_str_t path = sp_str_view(entry.path);
      if (sp_glob_set_match(glob, path)) {
        path = sp_fs_join_path(scratch.mem, spn.paths.runtime, path);
        sp_io_file_writer_t io = sp_zero;
        sp_io_file_writer_from_path(&io, path);
        sp_io_write(&io.base, entry.data, entry.size, SP_NULLPTR);
        sp_io_file_writer_close(&io);
      }
    }

    {
      sp_io_file_writer_t io = sp_zero;
      sp_io_file_writer_from_path(&io, spn.paths.version);
      sp_io_write_str(&io.base, stamp, SP_NULLPTR);
      sp_io_file_writer_close(&io);
    }
  }

  sp_mem_end_scratch(scratch);
  return err;
}

#define try(expr) \
  do { \
    spn_err_union_t __err = (expr); \
    if (__err.kind) { \
      spn_event_buffer_push(spn.events, (spn_build_event_t) { \
        .kind = SPN_EVENT_ERR, \
        .err = __err }); \
      return SP_APP_ERR; \
    } \
  } while (0)

sp_app_result_t spn_init(sp_app_t* sp) {
  sp_os_register_signal_handler(SP_OS_SIGNAL_INTERRUPT, on_signal, SP_NULLPTR);

  spn.sp = sp;
  spn.mem = sp_mem_os_new();
  spn.arena = sp_mem_arena_new(spn.mem);
  spn.heap = sp_mem_arena_as_allocator(spn.arena);
  spn.intern = sp_intern_new(spn.mem);
  spn.env = sp_alloc_type(spn.heap, sp_env_t);
  *spn.env = sp_env_capture(spn.heap);

  sp_io_stream_writer_from_fd(&spn.logger.out, sp_sys_stdout, SP_IO_CLOSE_MODE_NONE);
  sp_io_stream_writer_from_fd(&spn.logger.err, sp_sys_stderr, SP_IO_CLOSE_MODE_NONE);
  spn.logger.level = SPN_LOG_LEVEL_INFO;
  sp_str_t log_level = sp_env_get(spn.env, sp_str_lit("SPN_LOG_LEVEL"));
  if (!sp_str_empty(log_level)) {
    spn.logger.level = spn_log_level_from_str(log_level);
  }

  spn_cli_t* cli = &spn.cli;
  sp_cli_t parsed = sp_cli_parse((sp_cli_desc_t) {
    .root = spn_cli(),
    .args = spn.args,
    .num_args = spn.num_args,
    .user_data = &spn.cli,
  });

  switch (parsed.status) {
    case SP_CLI_HELP: {
      sp_cli_write_help(&spn.logger.out.base, &parsed);
      return SP_APP_QUIT;
    }
    case SP_CLI_ERR: {
      sp_fmt_io(&spn.logger.err.base, "{.red}: ", sp_fmt_cstr("error"));
      sp_cli_err_print(&spn.logger.err.base, parsed.err);
      sp_fmt_io(&spn.logger.err.base, "\n");
      return SP_APP_ERR;
    }
    case SP_CLI_OK:
    case SP_CLI_CONTINUE: {
      break;
    }
  }

  spn_tui_init(&spn.tui, &app.session, SPN_OUTPUT_MODE_INTERACTIVE);

  spn.events = spn_event_buffer_new(spn.mem);
  spn_event_log_init(spn.heap);

  sp_atomic_s32_set(&spn.control, 0);

  spn.paths.cwd = sp_fs_get_cwd(spn.heap);
  spn.paths.bin = sp_fs_get_bin_path(spn.heap);
  spn.paths.patches = sp_env_get(spn.env, sp_str_lit("SPN_PATCH_DIR"));
  spn.paths.config.dir = join_path(env_or("SPN_CONFIG_DIR", sp_fs_get_config_path(spn.heap)), "spn");
    spn.paths.config.toml = sp_fs_join_path(spn.heap, spn.paths.config.dir, SP_LIT("spn.toml"));
  spn.paths.storage = env_or("SPN_STORAGE_DIR", join_path(sp_fs_get_storage_path(spn.heap), "spn"));
    spn.paths.caches.dir = join_path(spn.paths.storage, "cache");
      spn.paths.caches.git.dir = join_path(spn.paths.caches.dir, "source");
        spn.paths.caches.git.dbs = join_path(spn.paths.caches.git.dir, "dbs");
        spn.paths.caches.git.checkouts = join_path(spn.paths.caches.git.dir, "checkouts");
      spn.paths.caches.store.dir = join_path(spn.paths.caches.dir, "store");
      spn.paths.caches.build.dir = join_path(spn.paths.caches.dir, "build");
      spn.paths.toolchain = env_or("SPN_TOOLCHAIN_DIR", join_path(spn.paths.caches.dir, "toolchain"));
    spn.paths.index = join_path(spn.paths.storage, "index");
    spn.paths.log = join_path(spn.paths.storage, "log");
    spn.paths.cache = join_path(spn.paths.storage, "cache");
    spn.paths.runtime = join_path(spn.paths.storage, "runtime");
      spn.paths.include = join_path(spn.paths.runtime, "include");
      spn.paths.version = join_path(spn.paths.runtime, "version.stamp");
    spn.paths.tools.dir = sp_fs_join_path(spn.heap, spn.paths.storage, sp_str_lit("tools"));
      spn.paths.tools.manifest = sp_fs_join_path(spn.heap, spn.paths.tools.dir, sp_str_lit("spn.toml"));
      spn.paths.tools.lock = sp_fs_join_path(spn.heap, spn.paths.storage, sp_str_lit("spn.lock"));

  sp_fs_create_dir(spn.paths.log);
  {
    sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
    sp_str_t jsonl_path = sp_fs_join_path(scratch.mem, spn.paths.log, sp_str_lit("build.jsonl"));
    sp_fs_create_file(jsonl_path);
    sp_io_file_writer_from_path(&spn.logger.jsonl, jsonl_path);
    sp_mem_end_scratch(scratch);
  }
  sp_fs_create_dir(spn.paths.caches.dir);
  sp_fs_create_dir(spn.paths.caches.git.dir);
  sp_fs_create_dir(spn.paths.caches.git.dbs);
  sp_fs_create_dir(spn.paths.caches.git.checkouts);
  sp_fs_create_dir(spn.paths.caches.build.dir);
  sp_fs_create_dir(spn.paths.caches.store.dir);
  sp_fs_create_dir(spn.paths.index);
  sp_fs_create_dir(spn.paths.toolchain);
  sp_fs_create_dir(spn.paths.bin);
  sp_fs_create_dir(spn.paths.tools.dir);

  extract_runtime();

  // CONFIG
  if (sp_fs_exists(spn.paths.config.toml)) {
    spn_toml_loader_t loader = sp_zero;
    spn_toml_loader_init(&loader, spn.mem, spn.intern);
    if (spn_codegen_load_config(&loader, spn.paths.config.toml, &spn.config)) {
      spn_event_buffer_push(spn.events, (spn_build_event_t) {
        .kind = SPN_EVENT_ERR,
        .err = {
          .kind = SPN_ERR_MANIFEST_ISSUES,
          .issues = loader.issues,
        },
      });
      return SP_APP_ERR;
    }
  }


  spn_cli_commit();

  if (cli->quiet) {
    spn.logger.verbosity = SPN_VERBOSITY_QUIET;
  } else if (cli->verbose) {
    spn.logger.verbosity = SPN_VERBOSITY_VERBOSE;
  } else {
    spn.logger.verbosity = SPN_VERBOSITY_NORMAL;
  }

  if (sp_str_valid(cli->project_dir)) {
    spn.paths.project = sp_fs_canonicalize_path(spn.heap, cli->project_dir);
  }
  else {
    spn.paths.project = sp_str_copy(spn.heap, spn.paths.cwd);
  }
  spn.paths.manifest = sp_fs_join_path(spn.heap, spn.paths.project, sp_str_lit("spn.toml"));
  app.paths.lock = sp_fs_join_path(spn.heap, spn.paths.project, SP_LIT("spn.lock"));

  bool has_manifest = sp_fs_exists(spn.paths.manifest);
  if (!has_manifest) {
    if (spn_cli_requires_manifest(parsed.cmd)) {
      spn_log_error("no manifest found at {.cyan}", SP_FMT_STR(spn.paths.manifest));
      return SP_APP_ERR;
    }
  }
  else {
    try(spn_toml_load_manifest(spn.mem, spn.intern, spn.paths.manifest, &app.package));


    if (sp_fs_exists(app.paths.lock)) {
      sp_opt_set(app.lock, spn_lock_file_load(spn.heap, app.paths.lock, spn.events));
    }
  }

  // INDEXES
  //
  // Search order is array order: the root manifest's indexes shadow the
  // user config's, which shadow the builtin core. Relative filesystem urls
  // resolve against whoever declared them.
  sp_da_init(spn.heap, spn.indexes);

  if (has_manifest) {
    sp_str_om_for(app.package.indexes, it) {
      spn_index_info_t* index = sp_str_om_at(app.package.indexes, it);
      sp_da_push(spn.indexes, *index);
    }
  }

  sp_da_for(spn.config.index, it) {
    if (spn_find_index(spn.config.index[it].name)) {
      continue;
    }
    sp_da_push(spn.indexes, ((spn_index_info_t) {
      .name = spn.config.index[it].name,
      .url =  spn.config.index[it].url,
      .rev =  spn.config.index[it].rev,
      .publish_url = spn.config.index[it].publish_url,
      .protocol = spn.config.index[it].protocol,
      .kind = SPN_INDEX_USER,
    }));
  }

  if (!spn_find_index(sp_str_lit("core"))) {
    sp_da_push(spn.indexes, ((spn_index_info_t) {
      .name = sp_str_lit("core"),
      .url = sp_str_lit("https://github.com/tspader/spandex.git"),
      .protocol = SPN_INDEX_PROTOCOL_GIT,
      .kind = SPN_INDEX_BUILTIN,
    }));
  }

  sp_da_for(spn.indexes, i) {
    spn_index_info_t* index = &spn.indexes[i];
    if (index->protocol == SPN_INDEX_PROTOCOL_FILESYSTEM) {
      sp_str_t base = index->kind == SPN_INDEX_WORKSPACE ? spn.paths.project : spn.paths.config.dir;
      index->location = index->url;
      if (!sp_fs_is_absolute(index->url)) {
        sp_str_t joined = sp_fs_join_path(spn.heap, base, index->url);
        sp_str_t canonical = sp_fs_canonicalize_path(spn.heap, joined);
        index->location = sp_str_empty(canonical) ? joined : canonical;
      }
    } else {
      index->location = sp_fs_join_path(spn.heap, spn.paths.index, spn_git_db_key(spn.heap, index->url));
    }
    if (!index->refresh) {
      index->refresh = spn.cli.refresh ? spn.cli.refresh : 600;
    }
    spn_index_init(index, spn.mem);
  }

  try(spn_profile_overrides_parse(&spn.cli.profile, &app.config.overrides));

  switch (sp_cli_dispatch(&parsed)) {
    case SP_CLI_CONTINUE: break;
    case SP_CLI_OK: return SP_APP_QUIT;
    case SP_CLI_HELP: sp_cli_write_help(&spn.logger.out.base, &parsed); return SP_APP_QUIT;
    case SP_CLI_ERR: {
      sp_fmt_io(&spn.logger.err.base, "{.red}: ", sp_fmt_cstr("error"));
      sp_cli_err_print(&spn.logger.err.base, parsed.err);
      sp_fmt_io(&spn.logger.err.base, "\n");
      return SP_APP_ERR;
    }
  }

  if (has_manifest) {
    app.session.intern = spn.intern;
    app.session.events = spn.events;
    app.session.paths.root = spn.paths.project;
    app.session.paths.system = spn.paths;

    try(spn_session_init(&app.session, spn.heap, &app.package, app.config));
  }

  return SP_APP_CONTINUE;
}

SP_PRIVATE u32 get_short_thread_id(u64 thread_id) {
  // Give each unique OS thread ID a small, monotonic ID so it's easier
  // to track when reading logs
  static sp_ht(u64, u32) thread_map = SP_NULLPTR;
  static u32 id = 0;

  if (!thread_map) sp_ht_init(spn.heap, thread_map);
  if (!sp_ht_key_exists(thread_map, thread_id)) {
    sp_ht_insert(thread_map, thread_id, id++);
  }
  return *sp_ht_getp(thread_map, thread_id);
}

static void spn_drain_events(void) {
  if (!spn.events) return;

  sp_mem_arena_marker_t s = sp_mem_begin_scratch();
  sp_da(spn_build_event_t) events = spn_event_buffer_drain(s.mem, spn.events);

  sp_da_for(events, it) {
    spn_build_event_t* event = &events[it];
    event->thread_id = get_short_thread_id(event->thread_id);

    if (spn.logger.jsonl.fd) {
      spn_event_log_jsonl(&spn.logger.jsonl.base, event);
    }
    if (event->io) {
      spn_event_log_jsonl(&event->io->jsonl.writer, event);
      spn_event_log_build(&event->io->build.writer, event);
    }

    spn_tui_log_event(event);
  }

  sp_mem_end_scratch(s);
}

sp_app_result_t spn_poll(sp_app_t* sp) {
  spn_drain_events();
  spn_prompt_pump();

  return SP_APP_CONTINUE;
}

sp_app_result_t spn_update(sp_app_t* sp) {
  if (sp_atomic_s32_get(&sp->shutdown)) {
    return SP_APP_QUIT;
  }

  spn_task_executor_t* ex = &app.tasks;
  if (ex->index >= ex->len) {
    return SP_APP_QUIT;
  }

  spn_task_desc_t* task = spn_task_get(ex->data[ex->index]);
  spn_task_step_t step = sp_zero;
  if (!ex->initted) {
    ex->initted = true;
    step = task->init ? task->init(&app) : task->update(&app);
  }
  else {
    step = task->update(&app);
  }

  if (step.err.kind) {
    if (step.err.kind != SPN_ERROR) {
      spn_event_buffer_push(spn.events, (spn_build_event_t) {
        .kind = SPN_EVENT_ERR,
        .err = step.err,
      });
    }
    spn_prompt_stop(false);
    spn_poll(sp);
    return SP_APP_ERR;
  }

  switch (step.status) {
    case SPN_TASK_CONTINUE: return SP_APP_CONTINUE;
    case SPN_TASK_DONE: {
      ex->index++;
      ex->initted = false;
      return SP_APP_CONTINUE;
    }
  }

  sp_unreachable_return(SP_APP_ERR);
}

void spn_deinit(sp_app_t* sp) {
  switch (spn.tui.mode) {
    case SPN_OUTPUT_MODE_INTERACTIVE: {
      spn_prompt_stop(true);
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

  spn_drain_events();

  if (!app.session.pkg) return;

  if (sp_da_empty(app.session.plan.builds)) return;

  sp_om_for(app.session.units.packages, it) {
    spn_pkg_unit_t* unit = sp_om_at(app.session.units.packages, it);
    spn_build_plan_t* plan = spn_session_plan_for_build(&app.session, unit->build);
    if (!plan) {
      continue;
    }
    spn_pkg_unit_t* requested = spn_session_find_pkg_unit_by_id(&app.session, plan->root);
    if (!requested) {
      continue;
    }

    sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
    sp_fs_create_sym_link(
      unit->paths.logs.build,
      sp_fs_join_path(scratch.mem, requested->paths.work, unit->logs.build)
    );
    sp_fs_create_sym_link(
      unit->paths.logs.jsonl,
      sp_fs_join_path(scratch.mem, requested->paths.work, unit->logs.jsonl)
    );
    sp_mem_end_scratch(scratch);

    spn_lazy_log_close(&unit->logs.io.build);
    spn_lazy_log_close(&unit->logs.io.test);
    spn_lazy_log_close(&unit->logs.io.jsonl);
  }

  sp_om_for(app.session.units.targets, it) {
    spn_target_unit_t* target = sp_om_at(app.session.units.targets, it);
    spn_lazy_log_close(&target->logs.build);
    spn_lazy_log_close(&target->logs.test);
    spn_lazy_log_close(&target->logs.jsonl);
  }
}

sp_app_config_t spn_main(s32 num_args, const c8** args) {
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
SP_APP_MAIN(spn_main)
