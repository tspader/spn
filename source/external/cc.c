#include "sp.h"
#include "sp/macro.h"
#include "ctx/types.h"
#include "session/types.h"
#include "toolchain/types.h"

#include "gen.h"
#include "intern/intern.h"
#include "external/cc.h"
#include "triple/triple.h"
#include "sp/io.h"

void spn_cc_init(spn_cc_t* cc, sp_mem_t mem) {
  *cc = sp_zero_s(spn_cc_t);
  cc->mem = mem;
  sp_da_init(mem, cc->include);
  sp_da_init(mem, cc->define);
  sp_da_init(mem, cc->targets);
}

void spn_cc_add_runtime(spn_cc_t* cc, sp_str_t runtime, sp_str_t include) {
  cc->spn.runtime = runtime;
  cc->spn.include = include;
}

void spn_cc_set_toolchain(spn_cc_t* cc, spn_toolchain_unit_t* toolchain) {
  cc->driver = toolchain->toolchain->driver;
  cc->compiler = toolchain->compiler;
  cc->archiver = toolchain->archiver;
  cc->linker = toolchain->linker;
  cc->cxx = toolchain->cxx;
}

void spn_cc_set_profile(spn_cc_t* cc, spn_profile_info_t profile) {
  cc->arch = profile.arch;
  cc->os = profile.os;
  cc->abi = profile.abi;
  cc->mode = profile.mode;
  cc->linkage = profile.linkage;
  cc->standard = profile.standard;
}

void spn_cc_set_output_dir(spn_cc_t* cc, sp_str_t dir) {
  cc->dir = sp_str_copy(cc->mem, dir);
}

void spn_cc_add_include(spn_cc_t* cc, sp_str_t dir) {
  sp_da_push(cc->include, dir);
}

// Manifest paths are source-relative; the build script API hands us absolute paths
static sp_str_t spn_cc_resolve_pkg_path(sp_mem_t mem, spn_pkg_unit_t* pkg, sp_str_t path) {
  if (sp_str_starts_with(path, sp_str_lit("/"))) return path;
  return sp_fs_join_path(mem, pkg->paths.source, path);
}

void spn_cc_add_pkg(spn_cc_t* cc, spn_pkg_unit_t* pkg) {
  sp_da_for(pkg->info->include, it) {
    spn_cc_add_include(cc, spn_cc_resolve_pkg_path(cc->mem, pkg, pkg->info->include[it]));
  }

  sp_da_for(pkg->info->define, it) {
    spn_cc_add_define(cc, pkg->info->define[it]);
  }
}

void spn_cc_target_set_lang(spn_cc_target_t* target, spn_lang_t lang) {
  target->lang = lang;
}

void spn_cc_target_add_info(spn_cc_target_t* target, spn_pkg_unit_t* pkg, spn_target_info_t* info) {
  target->cxx = info->cxx;

  sp_da_for(info->include, it) {
    spn_cc_target_add_absolute_include(target, spn_cc_resolve_pkg_path(target->cc->mem, pkg, info->include[it]));
  }

  sp_da_for(info->define, it) {
    spn_cc_target_add_define(target, info->define[it]);
  }

  sp_da_for(info->flags, it) {
    spn_cc_target_add_flag(target, info->flags[it]);
  }
}

void spn_cc_add_relative_include(spn_cc_t* cc, sp_str_t dir) {
  sp_broken();
}

void spn_cc_add_define(spn_cc_t* cc, sp_str_t var) {
  sp_da_push(cc->define, var);
}

void spn_cc_target_add_relative_source(spn_cc_target_t* target, sp_str_t file_path) {
  sp_broken();
}

void spn_cc_target_add_absolute_source(spn_cc_target_t* target, sp_str_t path) {
  sp_da_push(target->source, sp_str_copy(target->cc->mem, path));
}

void spn_cc_target_add_relative_include(spn_cc_target_t* target, sp_str_t dir) {
  sp_broken();
}

void spn_cc_target_add_absolute_include(spn_cc_target_t* target, sp_str_t dir) {
  sp_da_push(target->include, dir);
}

void spn_cc_target_add_define(spn_cc_target_t* target, sp_str_t var) {
  sp_da_push(target->define, sp_str_copy(target->cc->mem, var));
}

void spn_cc_target_add_flag(spn_cc_target_t* target, sp_str_t flag) {
  sp_da_push(target->flags, sp_str_copy(target->cc->mem, flag));
}

void spn_cc_target_add_lib(spn_cc_target_t* target, sp_str_t lib) {
  sp_da_push(target->libs, sp_str_copy(target->cc->mem, lib));
}

void spn_cc_target_add_lib_dir(spn_cc_target_t* target, sp_str_t dir) {
  sp_da_push(target->lib_dirs, sp_str_copy(target->cc->mem, dir));
}

void spn_cc_target_add_rpath(spn_cc_target_t* target, sp_str_t dir) {
  sp_da_push(target->rpath, sp_str_copy(target->cc->mem, dir));
}

void spn_cc_target_add_system_lib(spn_cc_target_t* target, sp_str_t name) {
  sp_da_push(target->system_libs, sp_str_copy(target->cc->mem, name));
}

sp_str_t spn_cc_symbol_from_embedded_file(sp_mem_t mem, sp_str_t file_path) {
  c8* data = sp_alloc_n(mem, c8, file_path.len);
  for (u32 it = 0; it < file_path.len; it++) {
    c8 c = file_path.data[it];
    data[it] = (c == '/' || c == '.' || c == '-') ? '_' : c;
  }
  return sp_str(data, file_path.len);
}

void spn_cc_target_add_dep(spn_cc_target_t* target, spn_pkg_unit_t* unit) {
  spn_cc_target_add_absolute_include(target, unit->paths.include);

  // @spader @refactor Add libraries
}

spn_cc_target_t* spn_cc_add_target(spn_cc_t* cc, spn_cc_output_kind_t kind, sp_str_t output) {
  spn_cc_target_t target = {
    .output = sp_str_copy(cc->mem, output),
    .kind = kind,
    .cc = cc
  };
  sp_da_init(cc->mem, target.source);
  sp_da_init(cc->mem, target.include);
  sp_da_init(cc->mem, target.define);
  sp_da_init(cc->mem, target.flags);
  sp_da_init(cc->mem, target.libs);
  sp_da_init(cc->mem, target.system_libs);
  sp_da_init(cc->mem, target.lib_dirs);
  sp_da_init(cc->mem, target.rpath);
  sp_da_push(cc->targets, target);
  return sp_da_back(cc->targets);
}

void spn_cc_embed_ctx_init(spn_cc_embed_ctx_t* ctx, sp_mem_t mem, spn_os_t target_os) {
  ctx->arena = sp_mem_arena_new(mem);
  ctx->mem = sp_mem_arena_as_allocator(ctx->arena);
  sp_da_init(ctx->mem, ctx->entries);

  spn_obj_kind_t format;
  switch (target_os) {
    case SPN_OS_WINDOWS: format = SPN_OBJ_COFF; break;
    case SPN_OS_MACOS:   format = SPN_OBJ_MACHO; break;
    default:             format = SPN_OBJ_ELF; break;
  }
  spn_obj_init(&ctx->obj, ctx->mem, format);
}

void spn_cc_embed_ctx_free(spn_cc_embed_ctx_t* ctx) {
  sp_mem_arena_destroy(ctx->arena);
  *ctx = sp_zero_s(spn_cc_embed_ctx_t);
}

spn_err_t spn_cc_embed_ctx_add(
  spn_cc_embed_ctx_t* ctx,
  sp_mem_buffer_t data,
  sp_str_t symbol,
  sp_str_t path,
  sp_str_t data_type,
  sp_str_t size_type
) {
  symbol = sp_str_copy(ctx->mem, symbol);

  spn_obj_add_symbol(&ctx->obj, symbol, data.data, data.len);
  spn_obj_add_symbol(&ctx->obj, sp_fmt(ctx->mem, "{}_size", sp_fmt_str(symbol)).value, &data.len, sizeof(u64));

  sp_da_push(ctx->entries, ((spn_cc_embed_t) {
    .path = sp_str_copy(ctx->mem, path),
    .symbol = symbol,
    .size = data.len,
    .types = {
      .size = sp_str_copy(ctx->mem, size_type),
      .data = sp_str_copy(ctx->mem, data_type),
    }
  }));

  return SPN_OK;
}

spn_err_t spn_cc_embed_ctx_write(spn_cc_embed_ctx_t* ctx, sp_str_t object, sp_str_t header) {
  spn_try_as(spn_obj_write(&ctx->obj, object), SPN_ERROR);

  sp_io_file_writer_t writer = sp_zero;
  spn_try_as(sp_io_file_writer_from_path(&writer, header), SPN_ERROR);
  sp_io_writer_t* io = &writer.base;
  sp_da_for(ctx->entries, it) {
    spn_cc_embed_t entry = ctx->entries[it];
    sp_fmt_io(io,
      "extern const {} {} [{}];",
      SP_FMT_STR(entry.types.data),
      SP_FMT_STR(entry.symbol),
      SP_FMT_U64(entry.size)
    );
    sp_io_write_new_line(io);

    sp_fmt_io(io,
      "extern const {} {}_size;",
      SP_FMT_STR(entry.types.size),
      SP_FMT_STR(entry.symbol)
    );
    sp_io_write_new_line(io);
    sp_io_write_new_line(io);
  }

  sp_io_write_str(io, sp_str_lit("typedef struct {"), SP_NULLPTR);
  sp_io_write_new_line(io);
  sp_io_write_str(io, sp_str_lit("  const char* path;"), SP_NULLPTR);
  sp_io_write_new_line(io);
  sp_io_write_str(io, sp_str_lit("  const void* data;"), SP_NULLPTR);
  sp_io_write_new_line(io);
  sp_io_write_str(io, sp_str_lit("  unsigned long long size;"), SP_NULLPTR);
  sp_io_write_new_line(io);
  sp_io_write_str(io, sp_str_lit("} spn_embed_entry_t;"), SP_NULLPTR);
  sp_io_write_new_line(io);
  sp_fmt_io(io, "static const unsigned int spn_embed_count = {};", SP_FMT_U32(sp_da_size(ctx->entries)));
  sp_io_write_new_line(io);
  sp_io_write_str(io, sp_str_lit("static const spn_embed_entry_t spn_embed_manifest[] = {"), SP_NULLPTR);
  sp_io_write_new_line(io);
  sp_da_for(ctx->entries, it) {
    spn_cc_embed_t entry = ctx->entries[it];
    sp_io_write_cstr(io, "  { ", SP_NULLPTR);
    sp_fmt_io(io, "\"{}\", {}, {}", SP_FMT_STR(entry.path), SP_FMT_STR(entry.symbol), SP_FMT_U64(entry.size));
    sp_io_write_cstr(io, " },", SP_NULLPTR);
    sp_io_write_new_line(io);
  }
  sp_io_write_str(io, sp_str_lit("};"), SP_NULLPTR);
  sp_io_write_new_line(io);

  sp_io_file_writer_close(&writer);
  return SPN_OK;
}

static spn_toolchain_launcher_t spn_cc_launcher(spn_cc_t* cc, spn_cc_target_t* target) {
  if (target->lang == SPN_LANG_CXX) {
    SP_ASSERT(!sp_str_empty(cc->cxx.program));
    return cc->cxx;
  }
  return cc->compiler;
}

void spn_cc_to_ps(sp_mem_t mem, spn_cc_t* cc, spn_cc_target_t* target, sp_ps_config_t* ps) {
  spn_toolchain_launcher_t launcher = spn_cc_launcher(cc, target);
  ps->command = launcher.program;
  sp_da_for(launcher.args, it) {
    sp_ps_config_add_arg(mem, ps, launcher.args[it]);
  }

  // Clang and Zig require an explicit --target flag for cross-compilation.
  // GCC cross-compilers bake the target into the binary name (e.g. x86_64-w64-mingw32-gcc).
  if (cc->driver == SPN_CC_DRIVER_CLANG) {
    spn_triple_t triple = { cc->arch, cc->os, cc->abi };
    sp_str_t target_str = spn_triple_to_cc_target(mem, triple);
    if (!sp_str_empty(target_str)) {
      sp_ps_config_add_arg(mem, ps, sp_fmt(mem, "--target={}", SP_FMT_STR(target_str)).value);
    }
  }

  sp_da_for(cc->include, it) {
    sp_ps_config_add_arg(mem, ps, spn_gen_format_entry(mem, cc->include[it], SPN_GEN_INCLUDE, cc->driver));
  }
  sp_da_for(cc->define, it) {
    sp_ps_config_add_arg(mem, ps, spn_gen_format_entry(mem, cc->define[it], SPN_GEN_DEFINE, cc->driver));
  }

  switch (target->kind) {
    case SPN_CC_OUTPUT_OBJECT:
    case SPN_CC_OUTPUT_WASM: {
      switch (target->lang) {
        case SPN_LANG_C: {
          sp_ps_config_add_arg(mem, ps, spn_cc_c_standard_to_switch(cc->standard));
          break;
        }
        case SPN_LANG_CXX: {
          sp_ps_config_add_arg(mem, ps, spn_cc_cxx_standard_to_switch(target->cxx.standard));
          break;
        }
      }
      break;
    }
    case SPN_CC_OUTPUT_SHARED_LIB:
    case SPN_CC_OUTPUT_STATIC_LIB:
    case SPN_CC_OUTPUT_EXE: {
      break;
    }
  }

  sp_ps_config_add_arg(mem, ps, spn_cc_build_mode_to_switch(cc->mode));
}

void spn_cc_target_to_ps(sp_mem_t mem, spn_cc_t* cc, spn_cc_target_t* target, sp_ps_config_t* ps) {
  switch (target->kind) {
    case SPN_CC_OUTPUT_OBJECT: {
      sp_ps_config_add_arg(mem, ps, sp_str_lit("-c"));
      break;
    }
    case SPN_CC_OUTPUT_SHARED_LIB: {
      sp_ps_config_add_arg(mem, ps, sp_str_lit("-shared"));
      break;
    }
    case SPN_CC_OUTPUT_EXE: {
      sp_ps_config_add_arg(mem, ps, spn_cc_lib_kind_to_switch(cc->linkage));
      break;
    }
    case SPN_CC_OUTPUT_WASM: {
      sp_ps_config_add_arg(mem, ps, sp_str_lit("-mexec-model=reactor"));
      sp_ps_config_add_arg(mem, ps, sp_str_lit("-Wl,--no-entry"));
      sp_ps_config_add_arg(mem, ps, sp_str_lit("-Wl,--import-symbols"));
      break;
    }
    case SPN_CC_OUTPUT_STATIC_LIB: {
      break;
    }
  }

  spn_cc_driver_t driver = cc->driver;
  sp_da_for(target->source, it) {
    sp_ps_config_add_arg(mem, ps, target->source[it]);
  }
  sp_da_for(target->include, it) {
    sp_ps_config_add_arg(mem, ps, spn_gen_format_entry(mem, target->include[it], SPN_GEN_INCLUDE, driver));
  }
  sp_da_for(target->define, it) {
    sp_ps_config_add_arg(mem, ps, spn_gen_format_entry(mem, target->define[it], SPN_GEN_DEFINE, driver));
  }

  if (target->lang == SPN_LANG_CXX) {
    if (target->cxx.no_exceptions) {
      sp_ps_config_add_arg(mem, ps, sp_str_lit("-fno-exceptions"));
    }
    if (target->cxx.no_rtti) {
      sp_ps_config_add_arg(mem, ps, sp_str_lit("-fno-rtti"));
    }
  }

  sp_da_for(target->flags, it) {
    sp_ps_config_add_arg(mem, ps, target->flags[it]);
  }
  sp_da_for(target->lib_dirs, it) {
    sp_ps_config_add_arg(mem, ps, spn_gen_format_entry(mem, target->lib_dirs[it], SPN_GEN_LIB_INCLUDE, driver));
  }
  sp_da_for(target->libs, it) {
    sp_ps_config_add_arg(mem, ps, spn_gen_format_entry(mem, target->libs[it], SPN_GEN_LIBS, driver));
  }
  sp_da_for(target->system_libs, it) {
    sp_ps_config_add_arg(mem, ps, spn_gen_format_entry(mem, target->system_libs[it], SPN_GEN_SYSTEM_LIBS, driver));
  }
  sp_da_for(target->rpath, it) {
    sp_ps_config_add_arg(mem, ps, spn_gen_format_entry(mem, target->rpath[it], SPN_GEN_RPATH, driver));
  }

  sp_ps_config_add_arg(mem, ps, sp_str_lit("-Werror=return-type"));
  sp_ps_config_add_arg(mem, ps, sp_str_lit("-o"));
  sp_ps_config_add_arg(mem, ps, sp_fs_join_path(mem, cc->dir, target->output));
}

spn_cc_run_t spn_cc_target_run(spn_cc_target_t* target, sp_str_t cwd) {
  spn_cc_t* cc = target->cc;
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();

  sp_ps_config_t ps = {
    .cwd = cwd,
    .io = {
      .in.mode = SP_PS_IO_MODE_NULL,
      .err.mode = SP_PS_IO_MODE_REDIRECT,
    }
  };
  spn_cc_to_ps(scratch.mem, cc, target, &ps);
  spn_cc_target_to_ps(scratch.mem, cc, target, &ps);

  sp_tm_timer_t timer = sp_tm_start_timer();
  sp_ps_output_t result = sp_ps_run(cc->mem, ps);
  u64 elapsed = sp_tm_read_timer(&timer);

  sp_io_dyn_mem_writer_t log;
  sp_io_dyn_mem_writer_init(cc->mem, &log);
  sp_io_write_str(&log.base, ps.command, SP_NULLPTR);
  sp_io_write_c8(&log.base, ' ');
  sp_da_for(ps.dyn_args, it) {
    sp_io_write_str(&log.base, ps.dyn_args[it], SP_NULLPTR);
    sp_io_write_c8(&log.base, ' ');
  }
  sp_str_t args = sp_io_dyn_mem_writer_take_str(&log);

  sp_mem_end_scratch(scratch);

  return (spn_cc_run_t) {
    .result = result,
    .elapsed = elapsed,
    .args = args,
  };
}

