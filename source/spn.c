#include "error/error.h"
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

#include "project/project.h"
#include "codegen/codegen.h"
#include "codegen/lower.h"
#include "enum/enum.h"
#include "codegen/gen/config.gen.h"
#include "codegen/gen/manifest.gen.h"
#include "cli/cli.h"
#include "event/build.h"
#include "event/event.h"
#include "event/log.h"
#include "external/tom.h"
#include "git/cache.h"
#include "index/index.h"
#include "intern/intern.h"
#include "lock/lock.h"
#include "log/lazy/lazy.h"
#include "log/log.h"
#include "sp/sp_om.h"
#include "pkg/load.h"
#include "profile/profile.h"
#include "session/session.h"
#include "spn.embed.h"
#include "toolchain/toolchain.h"
#include "triple/triple.h"
#include "sp/io.h"
#include "sp/macro.h"
#include "sp/os.h"
#include "sp/sp_glob.h"
#include "op/op.h"
#include "toml/loader.h"
#include "tui/tui.h"
#include "version.h"

// SINGLE HEADER
#define SP_IMPLEMENTATION
#include "sp.h"
#include "sp/atomic_file.h"
#include "sp/sp_prompt.h"
#include "sp/sp_cli.h"
#include "sp/sp_template.h"
#include "sp/coff.h"
#include "sp/sp_elf.h"
#include "sp/macho.h"

#define SP_MATH_IMPLEMENTATION
#include "sp/sp_math.h"

#define SP_GLOB_IMPLEMENTATION
#include "sp/sp_glob.h"

#define TOML_IMPLEMENTATION
#include "toml.h"

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

static spn_err_union_t invalid_flag(const c8* flag, sp_str_t value, const c8* expected) {
  return (spn_err_union_t) {
    .kind = SPN_ERR_FLAG_INVALID,
    .flag = {
      .name = sp_str_view(flag),
      .value = value,
      .expected = sp_str_view(expected),
    },
  };
}

static spn_err_union_t parse_profile_overrides(spn_profile_args_t* args, spn_profile_info_t* result) {
  spn_triple_t target = sp_zero;
  if (!sp_str_empty(args->target)) {
    const c8* expected = "an <arch>-<os>-<abi> triple like x86_64-linux-gnu";

    sp_str_t segments [3] = sp_zero;
    u32 num_segments = 0;
    sp_str_t remaining = args->target;
    while (true) {
      s32 separator = sp_str_find_c8(remaining, '-');
      sp_str_t segment = separator < 0 ? remaining : sp_str_prefix(remaining, separator);
      if (sp_str_empty(segment) || num_segments == sp_carr_len(segments)) {
        return invalid_flag("--target", args->target, expected);
      }
      segments[num_segments++] = segment;
      if (separator < 0) break;
      remaining = sp_str_suffix(remaining, remaining.len - separator - 1);
    }

    target.arch = spn_arch_from_str(segments[0]);
    if (!target.arch) {
      return invalid_flag("--target", args->target, expected);
    }
    if (num_segments > 1) {
      target.os = spn_os_from_str(segments[1]);
      if (!target.os) {
        return invalid_flag("--target", args->target, expected);
      }
    }
    if (num_segments > 2) {
      target.abi = spn_abi_from_str(segments[2]);
      if (!target.abi) {
        return invalid_flag("--target", args->target, expected);
      }
    }
  }

  spn_triple_t parts = {
    .arch = spn_arch_from_str(args->arch),
    .os = spn_os_from_str(args->os),
    .abi = spn_abi_from_str(args->abi),
  };
  if (!sp_str_empty(args->arch) && !parts.arch) {
    return invalid_flag("--arch", args->arch, "x86_64, aarch64, wasm32");
  }
  if (!sp_str_empty(args->os) && !parts.os) {
    return invalid_flag("--os", args->os, "linux, macos, windows, wasi");
  }
  if (!sp_str_empty(args->abi) && !parts.abi) {
    return invalid_flag("--abi", args->abi, "gnu, musl, msvc, mingw");
  }

  spn_build_mode_t mode = spn_build_mode_from_str(args->mode);
  if (!sp_str_empty(args->mode) && !mode) {
    return invalid_flag("--mode", args->mode, "debug, release");
  }

  spn_opt_level_t opt = spn_opt_level_from_str(args->opt);
  if (!sp_str_empty(args->opt) && !opt) {
    return invalid_flag("--opt", args->opt, "0, 1, 2, 3, s, z");
  }

  spn_sanitizer_set_t sanitizers = 0;
  bool sanitizers_set = false;
  if (sp_str_equal_cstr(args->sanitize, "none")) {
    sanitizers_set = true;
  }
  else if (!sp_str_empty(args->sanitize)) {
    sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
    sp_da(sp_str_t) names = sp_str_split_c8(scratch.mem, args->sanitize, ',');
    sp_da_for(names, it) {
      spn_sanitizer_t sanitizer = spn_sanitizer_from_str(names[it]);
      if (!sanitizer) {
        sp_mem_end_scratch(scratch);
        return invalid_flag("--sanitize", args->sanitize, "a comma-separated list of address, thread, undefined, memory, leak, or none");
      }
      sanitizers |= sanitizer;
    }
    sp_mem_end_scratch(scratch);
    if (spn_sanitizer_set_conflicting(sanitizers)) {
      return invalid_flag("--sanitize", args->sanitize, "a compatible set (thread and memory don't combine with each other, address, or leak)");
    }
  }

  target = spn_triple_merge(target, parts);

  *result = (spn_profile_info_t) {
    .name = args->name,
    .toolchain = args->toolchain,
    .mode = mode,
    .opt = opt,
    .sanitizers = sanitizers,
    .sanitizers_set = sanitizers_set,
    .os = target.os,
    .arch = target.arch,
    .abi = target.abi,
  };
  return spn_result(SPN_OK);
}


#define try(expr) \
  do { \
    spn_err_union_t __err = (expr); \
    if (__err.kind) { \
      spn.result = spn_err_emit(__err); \
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
  if (sp_sys_is_tty(sp_sys_stdout)) sp_sys_tty_use_vt(sp_sys_stdout);
  if (sp_sys_is_tty(sp_sys_stderr)) sp_sys_tty_use_vt(sp_sys_stderr);
#ifdef SP_WIN32
  if (sp_sys_is_tty(sp_sys_stdout) || sp_sys_is_tty(sp_sys_stderr)) {
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);
  }
#endif
  spn.logger.level = SPN_LOG_LEVEL_INFO;
  sp_str_t log_level = sp_env_get(spn.env, sp_str_lit("SPN_LOG_LEVEL"));
  if (!sp_str_empty(log_level)) {
    spn.logger.level = spn_log_level_from_str(log_level);
  }

  spn_cli_t* cli = &spn.cli;
  spn_command_t command = sp_zero;
  sp_cli_t parsed = sp_cli_parse((sp_cli_desc_t) {
    .root = spn_cli(),
    .args = spn.args,
    .num_args = spn.num_args,
    .user_data = &command,
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

  if (spn.cli.ci) {
    sp->fps = 100000;
  }

  spn_tui_mode_t output_mode = SPN_OUTPUT_MODE_INTERACTIVE;
  if (!sp_str_empty(spn.cli.output)) {
    output_mode = spn_output_mode_from_str(spn.cli.output);
  }
  spn_tui_init(&spn.tui, output_mode);

  spn.events = spn_event_buffer_new(spn.mem);
  spn_event_log_init(spn.heap);

  spn.paths.cwd = sp_fs_get_cwd(spn.heap);
  spn.paths.bin = sp_fs_get_bin_path(spn.heap);
  spn.paths.patches = sp_env_get(spn.env, sp_str_lit("SPN_PATCH_DIR"));
  spn.paths.config.dir = join_path(env_or("SPN_CONFIG_DIR", sp_fs_get_config_path(spn.heap)), "spn");
    spn.paths.config.toml = sp_fs_join_path(spn.heap, spn.paths.config.dir, SP_LIT("spn.toml"));
  spn.paths.storage = env_or("SPN_STORAGE_DIR", join_path(sp_fs_get_storage_path(spn.heap), "spn"));
    spn.paths.caches.dir = join_path(spn.paths.storage, "cache");
      spn.paths.caches.git.dir = join_path(spn.paths.caches.dir, "source");
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
  sp_fs_create_dir(spn.paths.caches.build.dir);
  sp_fs_create_dir(spn.paths.caches.store.dir);
  sp_fs_create_dir(spn.paths.index);
  sp_fs_create_dir(spn.paths.toolchain);
  sp_fs_create_dir(spn.paths.bin);
  sp_fs_create_dir(spn.paths.tools.dir);

  extract_runtime();

  spn_git_cache_init(&spn.caches.git, spn.mem, spn.intern, spn.paths.caches.git.dir);
  spn.caches.toolchains = (spn_toolchain_store_t) {
    .mem = spn.mem,
    .dir = spn.paths.toolchain,
    .mirror = sp_env_get(spn.env, sp_str_lit("SPN_MIRROR")),
    .fetch = spn_fetch_curl,
  };

  // CONFIG
  sp_da(spn_index_info_t) config_indexes = sp_da_new(spn.heap, spn_index_info_t);
  if (sp_fs_exists(spn.paths.config.toml)) {
    spn_toml_loader_t loader = sp_zero;
    spn_toml_loader_init(&loader, spn.mem, spn.intern);
    spn_err_t loaded = spn_codegen_load_config(&loader, spn.paths.config.toml, &spn.config_file);
    if (!loaded) {
      sp_da_for(spn.config_file.index, it) {
        sp_da_push(config_indexes, spn_index_lower(&loader, it, SPN_INDEX_USER, &spn.config_file.index[it]));
      }
    }
    if (loaded || !sp_da_empty(loader.issues)) {
      spn.result = spn_err_emit((spn_err_union_t) {
        .kind = SPN_ERR_MANIFEST_ISSUES,
        .manifest = { .path = spn.paths.config.toml, .issues = loader.issues },
      });
      return SP_APP_ERR;
    }
  }

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
  try(spn_project_load(spn.heap, spn.intern, spn.events, spn.paths.project, &spn.project));
  if (spn_cli_requires_manifest(parsed.cmd)) {
    if (!spn.project) {
      spn.result = spn_err_emit((spn_err_union_t) {
        .kind = SPN_ERR_NO_MANIFEST,
        .no_manifest = { .path = spn.paths.project },
      });
      return SP_APP_ERR;
    }
  }

  spn_index_assemble(spn.heap, spn.project ? &spn.project->package.indexes : SP_NULLPTR, config_indexes, &spn.indexes);

  sp_da_for(spn.indexes, i) {
    spn_index_info_t* index = &spn.indexes[i];
    index->location = spn_index_location(index, spn.heap, spn.paths.index);
    if (!index->refresh) {
      index->refresh = spn.cli.refresh ? spn.cli.refresh : SPN_INDEX_DEFAULT_REFRESH;
    }
  }

  try(parse_profile_overrides(&spn.cli.profile, &command.config.overrides));

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

  if (spn.project) {
    spn.session = sp_alloc_type(spn.heap, spn_session_t);
    try(spn_session_init(spn.session, &spn, spn.heap, spn.project, command.config));
  }

  spn.exec.finish = command.finish;
  if (command.op.kind) {
    spn.exec.op = spn_op_start(spn.heap, &spn, command.op);
  }

  return SP_APP_CONTINUE;
}

SP_PRIVATE u32 get_short_thread_id(u64 thread_id) {
  // Give each unique OS thread ID a small, monotonic ID so it's easier
  // to track when reading logs
  static sp_ht(u64, u32) thread_map = SP_NULLPTR;
  static u32 id = 0;

  if (!thread_map) sp_ht_init(spn.tui.mem, thread_map);
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
    if (spn.exec.op) {
      spn_op_result(spn.exec.op);
      spn.exec.op = SP_NULLPTR;
    }
    return SP_APP_QUIT;
  }

  if (spn.exec.op) {
    if (!spn_op_poll(spn.exec.op)) {
      return SP_APP_CONTINUE;
    }

    spn_err_union_t result = spn_op_result(spn.exec.op);
    spn.exec.op = SP_NULLPTR;

    if (result.kind) {
      spn.result = spn_err_emit(result);
      spn_prompt_stop(false);
      spn_poll(sp);
      return SP_APP_ERR;
    }

    spn_prompt_stop(true);
    spn_poll(sp);
  }

  if (spn.exec.finish) {
    spn_err_union_t err = spn.exec.finish(&spn);
    spn.exec.finish = SP_NULLPTR;
    if (err.kind) {
      spn.result = spn_err_emit(err);
      return SP_APP_ERR;
    }
  }

  return SP_APP_QUIT;
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
    case SPN_OUTPUT_MODE_JSON: {
      break;
    }
  }

  if (spn.events) {
    bool ok = sp->result != SP_APP_ERR;
    spn_err_t kind = spn.result.kind;
    if (!ok && !kind) {
      kind = SPN_ERROR;
    }
    spn_event_buffer_push(spn.events, (spn_build_event_t) {
      .kind = SPN_EVENT_RESULT,
      .result = {
        .ok = ok,
        .err = sp_cstr_as_str(spn_err_to_str(kind)),
      },
    });
  }

  spn_drain_events();

  if (!spn.session) return;

  if (sp_da_empty(spn.session->plans)) return;

  sp_da_for(spn.session->plans, it) {
    spn_build_unit_t* build = spn.session->plans[it].build;
    spn_pkg_unit_t* requested = spn_session_find_pkg_unit(spn.session, build, spn_session_root_pkg(spn.session));
    if (!requested) {
      continue;
    }

    sp_da_for(build->packages, jt) {
      spn_pkg_unit_t* unit = build->packages[jt];

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
      spn_lazy_log_close(&unit->logs.io.jsonl);
    }
  }

  sp_om_for(spn.session->units.targets, it) {
    spn_target_unit_t* target = sp_om_at(spn.session->units.targets, it);
    spn_lazy_log_close(&target->logs.build);
    spn_lazy_log_close(&target->logs.jsonl);
  }
}

sp_app_config_t spn_main(s32 num_args, const c8** args) {
  spn = (spn_ctx_t) {
    .num_args = num_args,
    .args = args
  };

  return (sp_app_config_t) {
    .on_init = spn_init,
    .on_poll = spn_poll,
    .on_update = spn_update,
    .on_deinit = spn_deinit,
    .fps = 144,
  };
}
SP_APP_MAIN(spn_main)
