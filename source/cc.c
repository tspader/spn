#include "cc.h"

#include "gen.h"
#include "profile.h"
#include "sp/io.h"
#include "sp/os.h"
#include "sp/str.h"

sp_str_t spn_app_project_dir(void);
sp_str_t spn_pkg_unit_get_include_dir(spn_pkg_unit_t* unit);
sp_str_t spn_intern(sp_str_t str);
void spn_log_warn(const c8* fmt, ...);

void spn_cc_set_profile(spn_cc_t* cc, spn_profile_t* profile) {
  cc->standard = profile->standard;
  cc->mode = profile->mode;
  cc->linkage = profile->linkage;
  cc->compiler.kind = profile->cc.kind;
  cc->compiler.exe = spn_intern(profile->cc.exe);
}

void spn_cc_set_output_dir(spn_cc_t* cc, sp_str_t dir) {
  cc->dir = sp_str_copy(dir);
}

void spn_cc_add_include(spn_cc_t* cc, sp_str_t dir) {
  sp_da_push(cc->include, dir);
}

void spn_cc_add_relative_include(spn_cc_t* cc, sp_str_t dir) {
  sp_da_push(cc->include, sp_fs_join_path(spn_app_project_dir(), dir));
}

void spn_cc_add_define(spn_cc_t* cc, sp_str_t var) {
  sp_da_push(cc->define, var);
}

void spn_cc_add_pkg(spn_cc_t* cc, spn_pkg_t* pkg) {
  (void)cc;
  (void)pkg;
}

void spn_cc_target_add_relative_source(spn_cc_target_t* target, sp_str_t file_path) {
  sp_da_push(target->source, sp_fs_join_path(spn_app_project_dir(), file_path));
}

void spn_cc_target_add_absolute_source(spn_cc_target_t* target, sp_str_t path) {
  sp_da_push(target->source, sp_str_copy(path));
}

void spn_cc_target_add_relative_include(spn_cc_target_t* target, sp_str_t dir) {
  spn_cc_target_add_absolute_include(target, sp_fs_join_path(spn_app_project_dir(), dir));
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
  spn_cc_target_add_absolute_include(target, spn_pkg_unit_get_include_dir(unit));
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

void spn_cc_embed_ctx_init(spn_cc_embed_ctx_t* ctx) {
  ctx->elf = sp_elf_new_with_null_section();
  sp_elf_symtab_new(ctx->elf);
  sp_elf_section_t* section = sp_elf_add_section(ctx->elf, sp_str_lit(".rodata"), SHT_PROGBITS, 8);
  section->flags = SHF_ALLOC | SHF_WRITE;
}

spn_err_t spn_cc_embed_ctx_add(
  spn_cc_embed_ctx_t* ctx,
  sp_io_reader_t io,
  sp_str_t symbol,
  sp_str_t data_type,
  sp_str_t size_type
) {
  sp_elf_section_t* symtab = sp_elf_find_section_by_name(ctx->elf, sp_str_lit(".symtab"));
  sp_elf_section_t* section = sp_elf_find_section_by_name(ctx->elf, sp_str_lit(".rodata"));

  u64 size = sp_io_reader_size(&io);
  sp_io_reader_seek(&io, 0, SP_IO_SEEK_SET);

  {
    u64 offset = section->buffer.size;
    u8* ptr = sp_elf_section_reserve_bytes(section, size);
    sp_io_read(&io, ptr, size);

    sp_elf_add_symbol(
      symtab, ctx->elf,
      symbol,
      offset, size,
      STB_GLOBAL, STT_OBJECT,
      section->index
    );
  }

  {
    u64 offset = section->buffer.size;
    u64* ptr = (u64*)sp_elf_section_reserve_bytes(section, sizeof(u64));
    *ptr = size;
    sp_elf_add_symbol(
      symtab, ctx->elf,
      spn_intern(sp_format("{}_size", SP_FMT_STR(symbol))),
      offset, sizeof(u64),
      STB_GLOBAL, STT_OBJECT,
      section->index
    );
  }

  sp_da_push(ctx->entries, ((spn_cc_embed_t) {
    .symbol = spn_intern(symbol),
    .size = size,
    .types = {
      .size = spn_intern(size_type),
      .data = spn_intern(data_type),
    }
  }));

  return SPN_OK;
}

spn_err_t spn_cc_embed_ctx_write(spn_cc_embed_ctx_t* ctx, sp_str_t object, sp_str_t header) {
  sp_try_as(sp_elf_write_to_file(ctx->elf, object), SPN_ERROR);

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

  sp_io_writer_close(&io);
  return SPN_OK;
}

void spn_cc_to_ps(spn_cc_t* cc, sp_ps_config_t* ps) {
  ps->command = sp_str_copy(cc->compiler.exe);

  sp_da_for(cc->include, it) {
    sp_ps_config_add_arg(ps, spn_gen_format_entry(cc->include[it], SPN_GEN_INCLUDE, cc->compiler.kind));
  }
  sp_da_for(cc->define, it) {
    sp_ps_config_add_arg(ps, spn_gen_format_entry(cc->define[it], SPN_GEN_DEFINE, cc->compiler.kind));
  }

  sp_ps_config_add_arg(ps, spn_cc_c_standard_to_switch(cc->standard));
  sp_ps_config_add_arg(ps, spn_cc_build_mode_to_switch(cc->mode));
  sp_ps_config_add_arg(ps, spn_cc_lib_kind_to_switch(cc->linkage));
}

void spn_cc_target_to_ps(spn_cc_t* cc, spn_cc_target_t* target, sp_ps_config_t* ps) {
  switch (target->kind) {
    case SPN_TARGET_OBJECT: {
      sp_ps_config_add_arg(ps, sp_str_lit("-c"));
      break;
    }
    case SPN_TARGET_NONE:
    case SPN_TARGET_SHARED_LIB:
    case SPN_TARGET_STATIC_LIB:
    case SPN_TARGET_EXE:
    case SPN_TARGET_JIT: {
      break;
    }
  }

  sp_da_for(target->source, it) {
    sp_ps_config_add_arg(ps, target->source[it]);
  }
  sp_da_for(target->include, it) {
    sp_ps_config_add_arg(ps, spn_gen_format_entry(target->include[it], SPN_GEN_INCLUDE, cc->compiler.kind));
  }
  sp_da_for(target->define, it) {
    sp_ps_config_add_arg(ps, spn_gen_format_entry(target->define[it], SPN_GEN_DEFINE, cc->compiler.kind));
  }
  sp_da_for(target->lib_dirs, it) {
    sp_ps_config_add_arg(ps, spn_gen_format_entry(target->lib_dirs[it], SPN_GEN_LIB_INCLUDE, cc->compiler.kind));
  }
  sp_da_for(target->libs, it) {
    sp_ps_config_add_arg(ps, spn_gen_format_entry(target->libs[it], SPN_GEN_LIBS, cc->compiler.kind));
  }
  sp_da_for(target->rpath, it) {
    sp_ps_config_add_arg(ps, spn_gen_format_entry(target->rpath[it], SPN_GEN_RPATH, cc->compiler.kind));
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

spn_cc_kind_t spn_cc_kind_from_str(sp_str_t str) {
  if      (sp_str_equal_cstr(str, "")) return SPN_CC_NONE;
  else if (sp_str_equal_cstr(str, "tcc")) return SPN_CC_TCC;
  else if (sp_str_equal_cstr(str, "gcc")) return SPN_CC_GCC;
  else if (sp_str_equal_cstr(str, "clang")) return SPN_CC_CLANG;
  else if (sp_str_equal_cstr(str, "musl-gcc")) return SPN_CC_MUSL_GCC;
  else if (sp_str_equal_cstr(str, "zcc")) return SPN_CC_ZIG;
  else if (sp_str_equal_cstr(str, "zig cc")) return SPN_CC_ZIG;
  else if (sp_str_equal_cstr(str, "cosmocc")) return SPN_CC_COSMOCC;

  spn_log_warn("Unknown compiler {:fg brightyellow}; we'll assume a gcc command line when generating switches", SP_FMT_STR(str));
  return SPN_CC_CUSTOM;
}

sp_str_t spn_c_standard_to_str(spn_c_standard_t standard) {
  switch (standard) {
    case SPN_C11: return sp_str_lit("c11");
    case SPN_C99: return sp_str_lit("c99");
    case SPN_C89: return sp_str_lit("c89");
  }

  SP_UNREACHABLE_RETURN(sp_str_lit(""));
}

spn_c_standard_t spn_c_standard_from_str(sp_str_t str) {
  if      (sp_str_equal_cstr(str, "c89")) return SPN_C89;
  else if (sp_str_equal_cstr(str, "c99")) return SPN_C99;
  else if (sp_str_equal_cstr(str, "c11")) return SPN_C11;

  SP_FATAL("Unknown C standard {:fg brightyellow}; options are [c89, c99, c11]", SP_FMT_STR(str));
  SP_UNREACHABLE_RETURN(SPN_C99);
}
