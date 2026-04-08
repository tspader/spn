#include "ctx/types.h"
#include "profile/types.h"
#include "session/types.h"
#include "toolchain/types.h"

#include "ctx/ctx.h"
#include "gen.h"
#include "intern.h"
#include "enum/enum.h"
#include "external/cc.h"
#include "triple/triple.h"
#include "sp/io.h"

void spn_cc_set_toolchain(spn_cc_t* cc, spn_toolchain_t toolchain) {
  cc->toolchain = toolchain;
}

void spn_cc_set_profile(spn_cc_t* cc, spn_profile_t profile) {
  cc->arch = profile.arch;
  cc->os = profile.os;
  cc->abi = profile.abi;
  cc->mode = profile.mode;
  cc->linkage = profile.linkage;
  cc->standard = profile.standard;
}

void spn_cc_set_output_dir(spn_cc_t* cc, sp_str_t dir) {
  cc->dir = sp_str_copy(dir);
}

void spn_cc_add_include(spn_cc_t* cc, sp_str_t dir) {
  sp_da_push(cc->include, dir);
}

void spn_cc_add_relative_include(spn_cc_t* cc, sp_str_t dir) {
  sp_broken();
  //sp_da_push(cc->include, sp_fs_join_path(spn_app_project_dir(), dir));
}

void spn_cc_add_define(spn_cc_t* cc, sp_str_t var) {
  sp_da_push(cc->define, var);
}

void spn_cc_target_add_relative_source(spn_cc_target_t* target, sp_str_t file_path) {
  sp_broken();
  //sp_da_push(target->source, sp_fs_join_path(spn_app_project_dir(), file_path));
}

void spn_cc_target_add_absolute_source(spn_cc_target_t* target, sp_str_t path) {
  sp_da_push(target->source, sp_str_copy(path));
}

void spn_cc_target_add_relative_include(spn_cc_target_t* target, sp_str_t dir) {
  sp_broken();
  //spn_cc_target_add_absolute_include(target, sp_fs_join_path(spn_app_project_dir(), dir));
}

void spn_cc_target_add_absolute_include(spn_cc_target_t* target, sp_str_t dir) {
  sp_da_push(target->include, dir);
}

void spn_cc_target_add_define(spn_cc_target_t* target, sp_str_t var) {
  sp_da_push(target->define, sp_str_copy(var));
}

void spn_cc_target_add_lib(spn_cc_target_t* target, sp_str_t lib) {
  sp_da_push(target->libs, sp_str_copy(lib));
}

void spn_cc_target_add_lib_dir(spn_cc_target_t* target, sp_str_t dir) {
  sp_da_push(target->lib_dirs, sp_str_copy(dir));
}

void spn_cc_target_add_rpath(spn_cc_target_t* target, sp_str_t dir) {
  sp_da_push(target->rpath, sp_str_copy(dir));
}

sp_str_t spn_cc_symbol_from_embedded_file(sp_str_t file_path) {
  sp_str_t symbol = file_path;
  symbol = sp_str_replace_c8(symbol, '/', '_');
  symbol = sp_str_replace_c8(symbol, '.', '_');
  symbol = sp_str_replace_c8(symbol, '-', '_');
  return symbol;
}

void spn_cc_target_add_dep(spn_cc_target_t* target, spn_pkg_unit_t* unit) {
  spn_cc_target_add_absolute_include(target, unit->paths.include);

  // @spader @refactor Add libraries
}

spn_cc_target_t* spn_cc_add_target(spn_cc_t* cc, spn_target_kind_t kind, sp_str_t output) {
  spn_cc_target_t target = {
    .output = sp_str_copy(output),
    .kind = kind,
    .cc = cc
  };
  sp_da_push(cc->targets, target);
  return sp_da_back(cc->targets);
}

void spn_cc_embed_ctx_init(spn_cc_embed_ctx_t* ctx, spn_os_t target_os) {
  spn_obj_kind_t format;
  switch (target_os) {
    case SPN_OS_WINDOWS: format = SPN_OBJ_COFF; break;
    case SPN_OS_MACOS:   format = SPN_OBJ_MACHO; break;
    default:             format = SPN_OBJ_ELF; break;
  }
  spn_obj_init(&ctx->obj, format);
}

// @spader
// Looking at the call stack, most of these strings are already interned. Reinterning is cheap, but it makes
// me feel the same way as null checking everything you possible can. Maybe a separate type for strings that
// have already been interned
spn_err_t spn_cc_embed_ctx_add(
  spn_cc_embed_ctx_t* ctx,
  sp_io_reader_t io,
  sp_str_t symbol,
  sp_str_t path,
  sp_str_t data_type,
  sp_str_t size_type
) {
  sp_mem_buffer_t buffer = sp_io_read_all(&io);

  spn_obj_add_symbol(&ctx->obj, symbol, buffer.data, buffer.len);
  spn_obj_add_symbol(&ctx->obj, spn_intern(sp_format("{}_size", SP_FMT_STR(symbol))), &buffer.len, sizeof(u64));

  sp_da_push(ctx->entries, ((spn_cc_embed_t) {
    .path = spn_intern(path),
    .symbol = spn_intern(symbol),
    .size = buffer.len,
    .types = {
      .size = spn_intern(size_type),
      .data = spn_intern(data_type),
    }
  }));

  return SPN_OK;
}

spn_err_t spn_cc_embed_ctx_write(spn_cc_embed_ctx_t* ctx, sp_str_t object, sp_str_t header) {
  sp_try_as(spn_obj_write(&ctx->obj, object), SPN_ERROR);

  sp_io_writer_t io = sp_io_writer_from_file(header, SP_IO_WRITE_MODE_OVERWRITE);
  sp_da_for(ctx->entries, it) {
    spn_cc_embed_t entry = ctx->entries[it];
    sp_io_write_str(&io, sp_format(
      "extern const {} {} [{}];",
      SP_FMT_STR(entry.types.data),
      SP_FMT_STR(entry.symbol),
      SP_FMT_U64(entry.size)
    ));
    sp_io_write_new_line(&io);

    sp_io_write_str(&io, sp_format(
      "extern const {} {}_size;",
      SP_FMT_STR(entry.types.size),
      SP_FMT_STR(entry.symbol)
    ));
    sp_io_write_new_line(&io);
    sp_io_write_new_line(&io);
  }

  sp_io_write_str(&io, sp_str_lit("typedef struct {"));
  sp_io_write_new_line(&io);
  sp_io_write_str(&io, sp_str_lit("  const char* path;"));
  sp_io_write_new_line(&io);
  sp_io_write_str(&io, sp_str_lit("  const void* data;"));
  sp_io_write_new_line(&io);
  sp_io_write_str(&io, sp_str_lit("  unsigned long long size;"));
  sp_io_write_new_line(&io);
  sp_io_write_str(&io, sp_str_lit("} spn_embed_entry_t;"));
  sp_io_write_new_line(&io);
  sp_io_write_str(&io, sp_format("static const unsigned int spn_embed_count = {};", SP_FMT_U32(sp_da_size(ctx->entries))));
  sp_io_write_new_line(&io);
  sp_io_write_str(&io, sp_str_lit("static const spn_embed_entry_t spn_embed_manifest[] = {"));
  sp_io_write_new_line(&io);
  sp_da_for(ctx->entries, it) {
    spn_cc_embed_t entry = ctx->entries[it];
    sp_io_write_cstr(&io, "  { ");
    sp_io_write_str(&io, sp_format("\"{}\", {}, {}", SP_FMT_STR(entry.path), SP_FMT_STR(entry.symbol), SP_FMT_U64(entry.size)));
    sp_io_write_cstr(&io, " },");
    sp_io_write_new_line(&io);
  }
  sp_io_write_str(&io, sp_str_lit("};"));
  sp_io_write_new_line(&io);

  sp_io_writer_close(&io);
  return SPN_OK;
}

void spn_cc_to_ps(spn_cc_t* cc, sp_ps_config_t* ps) {
  ps->command = cc->toolchain.compiler.program;
  sp_da_for(cc->toolchain.compiler.args, ai) {
    sp_ps_config_add_arg(ps, cc->toolchain.compiler.args[ai]);
  }

  // Clang and Zig require an explicit --target flag for cross-compilation.
  // GCC cross-compilers bake the target into the binary name (e.g. x86_64-w64-mingw32-gcc).
  if (cc->toolchain.info.driver == SPN_CC_DRIVER_CLANG) {
    spn_triple_t target = { cc->arch, cc->os, cc->abi };
    sp_str_t target_str = spn_triple_to_cc_target(target);
    if (!sp_str_empty(target_str)) {
      sp_ps_config_add_arg(ps, sp_format("--target={}", SP_FMT_STR(target_str)));
    }
  }

  sp_da_for(cc->include, it) {
    sp_ps_config_add_arg(ps, spn_gen_format_entry(cc->include[it], SPN_GEN_INCLUDE, cc->toolchain.info.driver));
  }
  sp_da_for(cc->define, it) {
    sp_ps_config_add_arg(ps, spn_gen_format_entry(cc->define[it], SPN_GEN_DEFINE, cc->toolchain.info.driver));
  }

  sp_ps_config_add_arg(ps, spn_cc_c_standard_to_switch(cc->standard));
  sp_ps_config_add_arg(ps, spn_cc_build_mode_to_switch(cc->mode));
}

void spn_cc_target_to_ps(spn_cc_t* cc, spn_cc_target_t* target, sp_ps_config_t* ps) {
  switch (target->kind) {
    case SPN_TARGET_OBJECT: {
      sp_ps_config_add_arg(ps, sp_str_lit("-c"));
      break;
    }
    case SPN_TARGET_SHARED_LIB: {
      sp_ps_config_add_arg(ps, sp_str_lit("-shared"));
      break;
    }
    case SPN_TARGET_EXE: {
      sp_ps_config_add_arg(ps, spn_cc_lib_kind_to_switch(cc->linkage));
      break;
    }
    case SPN_TARGET_NONE:
    case SPN_TARGET_STATIC_LIB:
    case SPN_TARGET_JIT: {
      break;
    }
  }

  spn_cc_driver_t driver = cc->toolchain.info.driver;
  sp_da_for(target->source, it) {
    sp_ps_config_add_arg(ps, target->source[it]);
  }
  sp_da_for(target->include, it) {
    sp_ps_config_add_arg(ps, spn_gen_format_entry(target->include[it], SPN_GEN_INCLUDE, driver));
  }
  sp_da_for(target->define, it) {
    sp_ps_config_add_arg(ps, spn_gen_format_entry(target->define[it], SPN_GEN_DEFINE, driver));
  }
  if (target->kind != SPN_TARGET_OBJECT) {
    sp_da_for(target->lib_dirs, it) {
      sp_ps_config_add_arg(ps, spn_gen_format_entry(target->lib_dirs[it], SPN_GEN_LIB_INCLUDE, driver));
    }
    sp_da_for(target->libs, it) {
      sp_ps_config_add_arg(ps, spn_gen_format_entry(target->libs[it], SPN_GEN_LIBS, driver));
    }
    sp_da_for(target->rpath, it) {
      sp_ps_config_add_arg(ps, spn_gen_format_entry(target->rpath[it], SPN_GEN_RPATH, driver));
    }
  }

  sp_ps_config_add_arg(ps, sp_str_lit("-Werror=return-type"));
  sp_ps_config_add_arg(ps, sp_str_lit("-o"));
  sp_ps_config_add_arg(ps, sp_fs_join_path(cc->dir, target->output));
}

void spn_cc_target_to_tcc(spn_cc_t* cc, spn_cc_target_t* target, spn_tcc_t* tcc) {
  s32 result = 0;

  sp_da_for(cc->include, it) {
    result = tcc_add_include_path(tcc, sp_str_to_cstr(cc->include[it]));
  }

  sp_da_for(cc->define, it) {
    tcc_define_symbol(tcc, sp_str_to_cstr(cc->define[it]), "");
  }

  sp_da_for(target->include, it) {
    tcc_add_include_path(tcc, sp_str_to_cstr(target->include[it]));
  }

  sp_da_for(target->define, it) {
    tcc_define_symbol(tcc, sp_str_to_cstr(target->define[it]), "");
  }

  sp_da_for(target->lib_dirs, it) {
    result = tcc_add_library_path(tcc, sp_str_to_cstr(target->lib_dirs[it]));
  }

  sp_da_for(target->libs, it) {
    result = tcc_add_file(tcc, sp_str_to_cstr(target->libs[it]));
  }

  (void)result;
}

spn_cc_run_t spn_cc_target_run(spn_cc_target_t* target, sp_str_t cwd) {
  sp_ps_config_t ps = {
    .cwd = cwd,
    .io = {
      .in.mode = SP_PS_IO_MODE_NULL,
      .err.mode = SP_PS_IO_MODE_REDIRECT,
    }
  };
  spn_cc_to_ps(target->cc, &ps);
  spn_cc_target_to_ps(target->cc, target, &ps);

  spn_toolchain_t toolchain = target->cc->toolchain;

  sp_tm_timer_t timer = sp_tm_start_timer();
  sp_ps_output_t result = sp_ps_run(ps);
  u64 elapsed = sp_tm_read_timer(&timer);

  sp_str_builder_t log = SP_ZERO_INITIALIZE();
  sp_str_builder_append(&log, toolchain.compiler.program);
  sp_str_builder_append_c8(&log, ' ');
  sp_da_for(ps.dyn_args, it) {
    sp_str_builder_append(&log, ps.dyn_args[it]);
    sp_str_builder_append_c8(&log, ' ');
  }

  return (spn_cc_run_t) {
    .result = result,
    .elapsed = elapsed,
    .args = sp_str_builder_to_str(&log),
  };
}

