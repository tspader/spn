#include "ctx/types.h"
#include "session/types.h"

#include "enum/enum.h"
#include "event/event.h"
#include "log/log.h"
#include "toolchain/toolchain.h"
#include "unit/build.h"
#include "sp/io.h"
#include "sp/tm.h"

sp_str_t spn_cache_dir_kind_to_path(spn_pkg_dir_t kind) {
  switch (kind) {
    case SPN_DIR_PROJECT: {
      return spn.paths.project;
    }
    case SPN_DIR_CACHE: {
      return spn.paths.cache;
    }
    case SPN_DIR_STORE: {
      return spn.paths.store;
    }
    case SPN_DIR_SOURCE: {
      return spn.paths.source;
    }
    case SPN_DIR_WORK: {
      return spn.paths.cwd;
    }
    case SPN_DIR_NONE:
    case SPN_DIR_INCLUDE:
    case SPN_DIR_LIB:
    case SPN_DIR_VENDOR: {
      SP_UNREACHABLE_RETURN(sp_str_lit(""));
    }
  }

  SP_UNREACHABLE_RETURN(sp_str_lit(""));
}

sp_str_t spn_build_ctx_resolve_dir(const spn_build_ctx_t* ctx, spn_pkg_dir_t kind, sp_str_t sub) {
  return sp_fs_join_path(spn_build_ctx_get_dir(ctx, kind), sub);
}

sp_str_t spn_build_ctx_get_dir(const spn_build_ctx_t* ctx, spn_pkg_dir_t kind) {
  switch (kind) {
    case SPN_DIR_STORE: {
      return ctx->paths.store;
    }
    case SPN_DIR_INCLUDE: {
      return ctx->paths.include;
    }
    case SPN_DIR_LIB: {
      return ctx->paths.lib;
    }
    case SPN_DIR_VENDOR: {
      return ctx->paths.vendor;
    }
    case SPN_DIR_SOURCE: {
      return ctx->paths.source;
    }
    case SPN_DIR_WORK: {
      return ctx->paths.work;
    }
    case SPN_DIR_CACHE: {
      return ctx->paths.store;
    }
    case SPN_DIR_PROJECT: {
      return spn.paths.project;
    }
    case SPN_DIR_NONE: {
      return sp_str_lit("");
    }
  }

  SP_UNREACHABLE_RETURN(sp_str_lit(""));
}

sp_str_t spn_build_ctx_get_include_dir(spn_build_ctx_t* ctx) {
  return ctx->paths.include;
}

sp_str_t spn_build_ctx_get_lib_dir(spn_build_ctx_t* ctx) {
  switch (ctx->linkage) {
    case SPN_LIB_KIND_SHARED: {
      return ctx->paths.lib;
    }
    case SPN_LIB_KIND_STATIC:
    case SPN_LIB_KIND_SOURCE: {
      return SP_ZERO_STRUCT(sp_str_t);
    }
  }

  SP_UNREACHABLE_RETURN(SP_ZERO_STRUCT(sp_str_t));
}

sp_str_t spn_build_ctx_get_rpath(spn_build_ctx_t* ctx) {
  return spn_build_ctx_get_lib_dir(ctx);
}

sp_str_t spn_build_ctx_get_lib_path(spn_build_ctx_t* ctx, spn_target_t* lib_target) {
  spn_linkage_t linkage = spn_target_kind_to_pkg_linkage(lib_target->kind);
  sp_os_lib_kind_t os_kind = spn_lib_kind_to_sp_os_lib_kind(linkage);
  sp_str_t lib = lib_target->name;
  lib = sp_os_lib_to_file_name(lib, os_kind);
  lib = sp_fs_join_path(ctx->paths.lib, lib);
  return lib;
}

spn_build_ctx_t spn_build_ctx_make(spn_build_ctx_config_t config) {
  spn_build_ctx_t ctx = SP_ZERO_INITIALIZE();
  spn_build_ctx_init(&ctx, config);
  return ctx;
}

void spn_build_ctx_init(spn_build_ctx_t* ctx, spn_build_ctx_config_t config) {
  ctx->arena = sp_mem_arena_new_ex(256, SP_MEM_ARENA_MODE_NO_REALLOC, 1);

  ctx->name = sp_str_copy(config.name);
  ctx->linkage = config.linkage;
  ctx->pkg = config.package;
  ctx->session = config.session;
  ctx->paths.source = config.paths.source;
  ctx->paths.store = config.paths.store;
  ctx->paths.include = sp_fs_join_path(ctx->paths.store, SP_LIT("include"));
  ctx->paths.bin = sp_fs_join_path(ctx->paths.store, SP_LIT("bin"));
  ctx->paths.lib = sp_fs_join_path(ctx->paths.store, SP_LIT("lib"));
  ctx->paths.vendor = sp_fs_join_path(ctx->paths.store, SP_LIT("vendor"));

  ctx->paths.work = config.paths.work;
  ctx->paths.generated = sp_fs_join_path(ctx->paths.work, SP_LIT("spn"));

  ctx->paths.logs.build = sp_fs_join_path(ctx->paths.work, spn_build_ctx_get_build_log_name(ctx));
  ctx->paths.logs.test = sp_fs_join_path(ctx->paths.work, spn_build_ctx_get_test_log_name(ctx));
  ctx->paths.logs.jsonl = sp_fs_join_path(ctx->paths.work, spn_build_ctx_get_jsonl_log_name(ctx));

  sp_fs_create_dir(ctx->paths.work);
  sp_fs_create_dir(ctx->paths.generated);
  sp_fs_create_dir(ctx->paths.store);
  sp_fs_create_dir(ctx->paths.bin);
  sp_fs_create_dir(ctx->paths.include);
  sp_fs_create_dir(ctx->paths.lib);
  sp_fs_create_dir(ctx->paths.vendor);
  sp_fs_create_file(ctx->paths.logs.build);
  sp_fs_create_file(ctx->paths.logs.jsonl);

  ctx->logs.build = sp_io_writer_from_file(ctx->paths.logs.build, SP_IO_WRITE_MODE_APPEND);
  ctx->logs.jsonl = sp_io_writer_from_file(ctx->paths.logs.jsonl, SP_IO_WRITE_MODE_APPEND);
}

void spn_build_ctx_log_ex(spn_build_io_t* logs, spn_log_level_t level, u64 thread_id, sp_str_t source, sp_str_t message) {
  sp_io_writer_t* io = &logs->build;
  sp_tm_epoch_t now = sp_tm_now_epoch();
  sp_io_write_str(io, sp_tm_epoch_to_iso8601_us(now));
  sp_io_write_cstr(io, " [");
  sp_io_write_str(io, spn_log_level_to_str(level));
  sp_io_write_cstr(io, "] t:");
  sp_io_write_str(io, sp_format("{}", SP_FMT_U64(thread_id)));
  sp_io_write_cstr(io, " ");
  if (sp_str_valid(source)) {
    sp_io_write_str(io, source);
    sp_io_write_cstr(io, " ");
  }
  sp_io_write_str(io, message);
  sp_io_write_new_line(io);
}

void spn_build_ctx_log(spn_build_io_t* logs, sp_str_t message) {
  spn_build_ctx_log_ex(logs, SPN_LOG_LEVEL_INFO, 0, SP_ZERO_STRUCT(sp_str_t), message);
}

sp_str_t spn_build_ctx_get_build_log_name(spn_build_ctx_t* ctx) {
  return sp_format("{}.build.log", SP_FMT_STR(ctx->name));
}

sp_str_t spn_build_ctx_get_test_log_name(spn_build_ctx_t* ctx) {
  return sp_format("{}.test.log", SP_FMT_STR(ctx->name));
}

sp_str_t spn_build_ctx_get_jsonl_log_name(spn_build_ctx_t* ctx) {
  return sp_format("{}.build.jsonl", SP_FMT_STR(ctx->name));
}

sp_str_t spn_ctx_build_source_dir(spn_build_ctx_t* build) {
  return build->paths.source;
}

sp_str_t spn_ctx_build_work_dir(spn_build_ctx_t* build) {
  return build->paths.work;
}

sp_str_t spn_ctx_build_store_dir(spn_build_ctx_t* build) {
  return build->paths.store;
}

sp_str_t spn_ctx_build_include_dir(spn_build_ctx_t* build) {
  return build->paths.include;
}

sp_str_t spn_ctx_build_lib_dir(spn_build_ctx_t* build) {
  return build->paths.lib;
}

sp_ps_output_t spn_ctx_build_subprocess(spn_build_ctx_t* ctx, sp_ps_config_t config) {
  spn_event_buffer_push_ex(spn.events, ctx->pkg, &ctx->logs, (spn_build_event_t) {
    .kind = SPN_EVENT_API_CALL,
    .api_call = {
      .fn = sp_str_lit("spn_ctx_build_subprocess"),
      .args = config.command
    }
  });
  config.io = (sp_ps_io_config_t) {
    .in = { .mode = SP_PS_IO_MODE_NULL },
    .out = { .mode = SP_PS_IO_MODE_EXISTING, .fd = ctx->logs.build.file.fd },
    .err = { .mode = SP_PS_IO_MODE_REDIRECT }
  };
  config.cwd = ctx->paths.work;

  // Inject toolchain env vars from session. We use extra[] slots on top
  // of the default SP_PS_ENV_INHERIT so the subprocess gets the full process
  // environment plus our toolchain overrides.
  u32 ei = 0;
  for (; ei < sp_carr_len(config.env.extra); ei++) {
    if (!sp_str_valid(config.env.extra[ei].key)) break;
  }

  sp_str_t cc_key = sp_str_lit("CC");
  sp_str_t ar_key = sp_str_lit("AR");
  sp_str_t ld_key = sp_str_lit("LD");

  sp_str_t cc_val = sp_env_get(&ctx->session->env, cc_key);
  sp_str_t ar_val = sp_env_get(&ctx->session->env, ar_key);
  sp_str_t ld_val = sp_env_get(&ctx->session->env, ld_key);

  if (!sp_str_empty(cc_val) && ei < sp_carr_len(config.env.extra)) {
    config.env.extra[ei++] = (sp_env_var_t) { .key = cc_key, .value = cc_val };
  }
  if (!sp_str_empty(ar_val) && ei < sp_carr_len(config.env.extra)) {
    config.env.extra[ei++] = (sp_env_var_t) { .key = ar_key, .value = ar_val };
  }
  if (!sp_str_empty(ld_val) && ei < sp_carr_len(config.env.extra)) {
    config.env.extra[ei++] = (sp_env_var_t) { .key = ld_key, .value = ld_val };
  }

  sp_da_push(ctx->commands, sp_ps_config_copy(&config));

  sp_ps_t ps = sp_ps_create(config);
  return sp_ps_output(&ps);
}

sp_da(sp_str_t) spn_ctx_build_lib_entries(spn_build_ctx_t* build) {
  sp_da(sp_str_t) entries = SP_NULLPTR;
  sp_om_for(build->pkg->libs, it) {
    spn_target_t* lib = sp_om_at(build->pkg->libs, it);
    sp_da_push(entries, spn_build_ctx_get_lib_path(build, lib));
  }
  return entries;
}

spn_linkage_t spn_ctx_build_linkage(spn_build_ctx_t* build) {
  return build->linkage;
}

