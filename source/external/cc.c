#include "ctx/types.h"
#include "session/types.h"
#include "tcc/tcc.h"
#include "toolchain/types.h"

#include "gen.h"
#include "intern/intern.h"
#include "external/tcc/tcc.h"
#include "external/cc.h"
#include "triple/triple.h"
#include "sp/io.h"

void spn_cc_add_runtime(spn_cc_t* cc, sp_str_t runtime, sp_str_t include) {
  cc->spn.runtime = runtime;
  cc->spn.include = include;
}

void spn_cc_set_toolchain(spn_cc_t* cc, spn_toolchain_unit_t* toolchain) {
  cc->driver = toolchain->info.driver;
  cc->compiler = toolchain->compiler;
  cc->archiver = toolchain->archiver;
  cc->linker = toolchain->linker;
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
  cc->dir = sp_str_copy(spn_mem_todo, dir);
}

void spn_cc_add_include(spn_cc_t* cc, sp_str_t dir) {
  sp_da_push(cc->include, dir);
}

void spn_cc_add_relative_include(spn_cc_t* cc, sp_str_t dir) {
  sp_broken();
  //sp_da_push(cc->include, sp_fs_join_path(spn_mem_todo, spn_app_project_dir(), dir));
}

void spn_cc_add_define(spn_cc_t* cc, sp_str_t var) {
  sp_da_push(cc->define, var);
}

void spn_cc_target_add_relative_source(spn_cc_target_t* target, sp_str_t file_path) {
  sp_broken();
  //sp_da_push(target->source, sp_fs_join_path(spn_mem_todo, spn_app_project_dir(), file_path));
}

void spn_cc_target_add_absolute_source(spn_cc_target_t* target, sp_str_t path) {
  sp_da_push(target->source, sp_str_copy(spn_mem_todo, path));
}

void spn_cc_target_add_relative_include(spn_cc_target_t* target, sp_str_t dir) {
  sp_broken();
  //spn_cc_target_add_absolute_include(target, sp_fs_join_path(spn_mem_todo, spn_app_project_dir(), dir));
}

void spn_cc_target_add_absolute_include(spn_cc_target_t* target, sp_str_t dir) {
  sp_da_push(target->include, dir);
}

void spn_cc_target_add_define(spn_cc_target_t* target, sp_str_t var) {
  sp_da_push(target->define, sp_str_copy(spn_mem_todo, var));
}

void spn_cc_target_add_flag(spn_cc_target_t* target, sp_str_t flag) {
  sp_da_push(target->flags, sp_str_copy(spn_mem_todo, flag));
}

void spn_cc_target_add_lib(spn_cc_target_t* target, sp_str_t lib) {
  sp_da_push(target->libs, sp_str_copy(spn_mem_todo, lib));
}

void spn_cc_target_add_lib_dir(spn_cc_target_t* target, sp_str_t dir) {
  sp_da_push(target->lib_dirs, sp_str_copy(spn_mem_todo, dir));
}

void spn_cc_target_add_rpath(spn_cc_target_t* target, sp_str_t dir) {
  sp_da_push(target->rpath, sp_str_copy(spn_mem_todo, dir));
}

void spn_cc_target_add_system_lib(spn_cc_target_t* target, sp_str_t name) {
  sp_da_push(target->system_libs, sp_str_copy(spn_mem_todo, name));
}

sp_str_t spn_cc_symbol_from_embedded_file(sp_str_t file_path) {
  sp_str_t symbol = file_path;
  symbol = sp_str_replace_c8(spn_mem_todo, symbol, '/', '_');
  symbol = sp_str_replace_c8(spn_mem_todo, symbol, '.', '_');
  symbol = sp_str_replace_c8(spn_mem_todo, symbol, '-', '_');
  return symbol;
}

void spn_cc_target_add_dep(spn_cc_target_t* target, spn_pkg_unit_t* unit) {
  spn_cc_target_add_absolute_include(target, unit->paths.include);

  // @spader @refactor Add libraries
}

spn_cc_target_t* spn_cc_add_target(spn_cc_t* cc, spn_cc_output_kind_t kind, sp_str_t output) {
  spn_cc_target_t target = {
    .output = sp_str_copy(spn_mem_todo, output),
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
  sp_io_reader_t* io,
  sp_str_t symbol,
  sp_str_t path,
  sp_str_t data_type,
  sp_str_t size_type
) {
  sp_mem_buffer_t buffer = sp_io_read_all(io);

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

  sp_io_writer_t* io = sp_io_writer_from_file(header, SP_IO_WRITE_MODE_OVERWRITE);
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

  sp_io_writer_close(io);
  return SPN_OK;
}

void spn_cc_to_ps(spn_cc_t* cc, sp_ps_config_t* ps) {
  ps->command = cc->compiler.program;
  sp_da_for(cc->compiler.args, ai) {
    sp_ps_config_add_arg(spn_mem_todo, ps, cc->compiler.args[ai]);
  }

  // Clang and Zig require an explicit --target flag for cross-compilation.
  // GCC cross-compilers bake the target into the binary name (e.g. x86_64-w64-mingw32-gcc).
  if (cc->driver == SPN_CC_DRIVER_CLANG) {
    spn_triple_t target = { cc->arch, cc->os, cc->abi };
    sp_str_t target_str = spn_triple_to_cc_target(target);
    if (!sp_str_empty(target_str)) {
      sp_ps_config_add_arg(spn_mem_todo, ps, sp_format("--target={}", SP_FMT_STR(target_str)));
    }
  }

  sp_da_for(cc->include, it) {
    sp_ps_config_add_arg(spn_mem_todo, ps, spn_gen_format_entry(cc->include[it], SPN_GEN_INCLUDE, cc->driver));
  }
  sp_da_for(cc->define, it) {
    sp_ps_config_add_arg(spn_mem_todo, ps, spn_gen_format_entry(cc->define[it], SPN_GEN_DEFINE, cc->driver));
  }

  sp_ps_config_add_arg(spn_mem_todo, ps, spn_cc_c_standard_to_switch(cc->standard));
  sp_ps_config_add_arg(spn_mem_todo, ps, spn_cc_build_mode_to_switch(cc->mode));
}

void spn_cc_target_to_ps(spn_cc_t* cc, spn_cc_target_t* target, sp_ps_config_t* ps) {
  switch (target->kind) {
    case SPN_CC_OUTPUT_OBJECT: {
      sp_ps_config_add_arg(spn_mem_todo, ps, sp_str_lit("-c"));
      break;
    }
    case SPN_CC_OUTPUT_SHARED_LIB: {
      sp_ps_config_add_arg(spn_mem_todo, ps, sp_str_lit("-shared"));
      break;
    }
    case SPN_CC_OUTPUT_EXE: {
      sp_ps_config_add_arg(spn_mem_todo, ps, spn_cc_lib_kind_to_switch(cc->linkage));
      break;
    }
    case SPN_CC_OUTPUT_STATIC_LIB:
    case SPN_CC_OUTPUT_JIT: {
      break;
    }
  }

  spn_cc_driver_t driver = cc->driver;
  sp_da_for(target->source, it) {
    sp_ps_config_add_arg(spn_mem_todo, ps, target->source[it]);
  }
  sp_da_for(target->include, it) {
    sp_ps_config_add_arg(spn_mem_todo, ps, spn_gen_format_entry(target->include[it], SPN_GEN_INCLUDE, driver));
  }
  sp_da_for(target->define, it) {
    sp_ps_config_add_arg(spn_mem_todo, ps, spn_gen_format_entry(target->define[it], SPN_GEN_DEFINE, driver));
  }

  sp_da_for(target->flags, it) {
    sp_ps_config_add_arg(spn_mem_todo, ps, target->flags[it]);
  }
  sp_da_for(target->lib_dirs, it) {
    sp_ps_config_add_arg(spn_mem_todo, ps, spn_gen_format_entry(target->lib_dirs[it], SPN_GEN_LIB_INCLUDE, driver));
  }
  sp_da_for(target->libs, it) {
    sp_ps_config_add_arg(spn_mem_todo, ps, spn_gen_format_entry(target->libs[it], SPN_GEN_LIBS, driver));
  }
  sp_da_for(target->system_libs, it) {
    sp_ps_config_add_arg(spn_mem_todo, ps, spn_gen_format_entry(target->system_libs[it], SPN_GEN_SYSTEM_LIBS, driver));
  }
  sp_da_for(target->rpath, it) {
    sp_ps_config_add_arg(spn_mem_todo, ps, spn_gen_format_entry(target->rpath[it], SPN_GEN_RPATH, driver));
  }

  sp_ps_config_add_arg(spn_mem_todo, ps, sp_str_lit("-Werror=return-type"));
  sp_ps_config_add_arg(spn_mem_todo, ps, sp_str_lit("-o"));
  sp_ps_config_add_arg(spn_mem_todo, ps, sp_fs_join_path(spn_mem_todo, cc->dir, target->output));
}

spn_err_t spn_cc_target_to_tcc(spn_cc_t* cc, spn_cc_target_t* target, spn_tcc_t* tcc) {
  if (!sp_str_empty(cc->spn.runtime)) {
    spn_tcc_set_runtime(tcc, cc->spn.runtime);
  }

  if (!sp_str_empty(cc->spn.include)) {
    spn_try(spn_tcc_add_sys_include(tcc, cc->spn.include));
  }

  tcc_set_options(tcc->s, "-gdwarf -Wall -Werror");
  spn_try_as(tcc_set_output_type(tcc->s, TCC_OUTPUT_MEMORY), SPN_ERROR);
  tcc_define_symbol(tcc->s, "SPN", "");
  sp_try(spn_tcc_register(tcc));

  sp_da_for(cc->include, it) {
    spn_try(spn_tcc_add_include(tcc, cc->include[it]));
  }

  sp_da_for(cc->define, it) {
    spn_tcc_define_symbol(tcc, cc->define[it], sp_str_lit(""));
  }

  sp_da_for(target->include, it) {
    spn_try(spn_tcc_add_include(tcc, target->include[it]));
  }

  sp_da_for(target->define, it) {
    spn_tcc_define_symbol(tcc, target->define[it], sp_str_lit(""));
  }

  sp_da_for(target->lib_dirs, it) {
    spn_tcc_add_library_path(tcc, target->lib_dirs[it]);
  }

  sp_da_for(target->libs, it) {
    spn_try(spn_tcc_add_file(tcc, target->libs[it]));
  }

  sp_da_for(target->source, it) {
    spn_try(spn_tcc_add_file(tcc, target->source[it]));
  }

  return SPN_OK;
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

  sp_tm_timer_t timer = sp_tm_start_timer();
  sp_ps_output_t result = sp_ps_run(spn_mem_todo, ps);
  u64 elapsed = sp_tm_read_timer(&timer);

  sp_str_builder_t log = SP_ZERO_INITIALIZE();
  sp_str_builder_append(&log, target->cc->compiler.program);
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

