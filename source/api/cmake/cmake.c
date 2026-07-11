#include "sp.h"
#include "sp/macro.h"
#include "api/cmake/cmake.h"

#include "api/api.h"
#include "api/core/types.h"
#include "ctx/types.h"
#include "enum/enum.h"
#include "session/types.h"
#include "profile/types.h"
#include "unit/types.h"

#include "toolchain/toolchain.h"
#include "triple/triple.h"
#include "sp/io.h"

sp_str_t spn_cmake_gen_to_str(spn_cmake_gen_t gen) {
  switch (gen) {
    case SPN_CMAKE_GEN_DEFAULT: {
      return SP_ZERO_STRUCT(sp_str_t);
    }
    case SPN_CMAKE_GEN_UNIX_MAKEFILES: {
      return sp_str_lit("Unix Makefiles");
    }
    case SPN_CMAKE_GEN_NINJA: {
      return sp_str_lit("Ninja");
    }
    case SPN_CMAKE_GEN_XCODE: {
      return sp_str_lit("Xcode");
    }
    case SPN_CMAKE_GEN_MSVC: {
      return sp_str_lit("Visual Studio 17 2022");
    }
    case SPN_CMAKE_GEN_MINGW: {
      return sp_str_lit("MinGW Makefiles");
    }
  }
  return SP_ZERO_STRUCT(sp_str_t);
}

static sp_str_t spn_cmake_format_define(sp_mem_t mem, sp_str_t name, sp_str_t value) {
  return sp_fmt(mem, "-D{}={}", sp_fmt_str(name), sp_fmt_str(value)).value;
}

s32 spn_cmake(spn_t* build) {
  spn_cmake_t* cmake = spn_cmake_new(build);
  return spn_cmake_run(cmake);
}

spn_cmake_t* spn_cmake_new(spn_t* build) {
  sp_mem_t mem = spn.mem;
  spn_cmake_t* cmake = sp_alloc_type(mem, spn_cmake_t);
  *cmake = (spn_cmake_t) {
    .mem = mem,
    .build = build,
    .generator = SPN_CMAKE_GEN_DEFAULT,
  };
  sp_da_init(mem, cmake->defines);
  sp_da_init(mem, cmake->args);
  return cmake;
}

void spn_cmake_set_generator(spn_cmake_t* cmake, spn_cmake_gen_t gen) {
  cmake->generator = gen;
}

void spn_cmake_add_define(spn_cmake_t* cmake, const c8* name, const c8* value) {
  spn_cmake_define_t define = {
    .name = sp_str_from_cstr(cmake->mem, name),
    .value = sp_str_from_cstr(cmake->mem, value),
  };
  sp_da_push(cmake->defines, define);
}

void spn_cmake_add_arg(spn_cmake_t* cmake, const c8* arg) {
  sp_da_push(cmake->args, sp_str_from_cstr(cmake->mem, arg));
}

// CMake's compiler probes choke on multi-word launchers ("zig cc"), so any
// launcher with baked-in args becomes a wrapper script and CMake sees one
// program
static sp_str_t spn_cmake_launcher_program(sp_mem_t mem, sp_str_t tools_dir, const c8* name, spn_toolchain_launcher_t launcher) {
  if (sp_da_empty(launcher.args)) {
    return launcher.program;
  }

  sp_str_t path = sp_fs_join_path(mem, tools_dir, sp_fmt(mem, "{}.sh", sp_fmt_cstr(name)).value);

  sp_io_file_writer_t writer;
  sp_io_file_writer_from_path(&writer, path);
  sp_fmt_io(&writer.base, "#!/bin/sh\nexec \"{}\"", SP_FMT_STR(launcher.program));
  sp_da_for(launcher.args, it) {
    sp_fmt_io(&writer.base, " \"{}\"", SP_FMT_STR(launcher.args[it]));
  }
  sp_fmt_io(&writer.base, " \"$@\"\n");
  sp_io_file_writer_close(&writer);

  sp_sys_file_meta_t meta = { .raw_attrs = 0755 };
  sp_sys_chmod_s(sp_sys_get_root(0), path, &meta);
  return path;
}

static void set(sp_io_writer_t* io, const c8* name, sp_str_t value) {
  sp_fmt_io(io, "set({} {})", sp_fmt_cstr(name), sp_fmt_str(value));
  sp_fmt_io(io, "\n");
}

static sp_str_t spn_cmake_generate_toolchain_file(sp_mem_t mem, spn_pkg_unit_t* unit) {
  spn_profile_info_t* profile = &unit->ctx->profile;
  spn_toolchain_unit_t* toolchain = unit->ctx->toolchain;
  if (!toolchain) return sp_str_lit("");

  spn_triple_t target = { profile->arch, profile->os, profile->abi };

  sp_str_t tools = sp_fs_join_path(mem, unit->paths.generated, sp_str_lit("tools"));
  sp_str_t path = sp_fs_join_path(mem, tools, sp_str_lit("spn.cmake"));

  sp_fs_create_dir(tools);

  sp_io_file_writer_t writer;
  sp_io_file_writer_from_path(&writer, path);
  sp_io_writer_t* io = &writer.base;

  set(io, "CMAKE_SYSTEM_NAME", spn_os_to_cmake_system_name(profile->os));
  set(io, "CMAKE_SYSTEM_PROCESSOR", spn_arch_to_str(profile->arch));
  set(io, "CMAKE_C_COMPILER", spn_cmake_launcher_program(mem, tools, "cc", toolchain->compiler));
  set(io, "CMAKE_C_COMPILER_TARGET", spn_triple_to_cc_target(mem, target));
  if (spn_toolchain_has_cxx(toolchain->toolchain)) {
    set(io, "CMAKE_CXX_COMPILER", spn_cmake_launcher_program(mem, tools, "cxx", toolchain->cxx));
    set(io, "CMAKE_CXX_COMPILER_TARGET", spn_triple_to_cc_target(mem, target));
  }
  set(io, "CMAKE_LINKER", spn_cmake_launcher_program(mem, tools, "ld", toolchain->linker));
  set(io, "CMAKE_AR", spn_cmake_launcher_program(mem, tools, "ar", toolchain->archiver));

  sp_io_file_writer_close(&writer);
  return path;
}

s32 spn_cmake_configure(spn_cmake_t* cmake) {
  spn_pkg_unit_t* unit = spn_api_unit(cmake->build);
  spn_profile_info_t* profile = &unit->ctx->profile;

  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();

  sp_ps_config_t config = {
    .command = sp_str_lit("cmake"),
    .args = {
      sp_str_lit("-S"), unit->paths.source,
      sp_str_lit("-B"), unit->paths.work,
    }
  };

  if (cmake->generator != SPN_CMAKE_GEN_DEFAULT) {
    sp_ps_config_add_arg(scratch.mem, &config, sp_str_lit("-G"));
    sp_ps_config_add_arg(scratch.mem, &config, spn_cmake_gen_to_str(cmake->generator));
  }

  // Generate and pass a CMake toolchain file for cross-compilation support.
  // Even for native builds this is useful: it tells CMake exactly which
  // compiler, linker, and archiver to use.
  sp_str_t toolchain_file = spn_cmake_generate_toolchain_file(scratch.mem, unit);
  if (!sp_str_empty(toolchain_file)) {
    sp_ps_config_add_arg(scratch.mem, &config, spn_cmake_format_define(scratch.mem,
      sp_str_lit("CMAKE_TOOLCHAIN_FILE"), toolchain_file));
  }

  sp_ps_config_add_arg(scratch.mem, &config, spn_cmake_format_define(scratch.mem,
    sp_str_lit("CMAKE_INSTALL_PREFIX"),
    unit->paths.store)
  );

  sp_ps_config_add_arg(scratch.mem, &config, spn_cmake_format_define(scratch.mem,
    sp_str_lit("BUILD_SHARED_LIBS"),
    profile->linkage == SPN_LIB_KIND_SHARED ? sp_str_lit("ON") : sp_str_lit("OFF"))
  );

  sp_ps_config_add_arg(scratch.mem, &config, spn_cmake_format_define(scratch.mem,
    sp_str_lit("CMAKE_BUILD_TYPE"),
    profile->mode == SPN_BUILD_MODE_RELEASE ? sp_str_lit("Release") : sp_str_lit("Debug"))
  );

  sp_da_for(cmake->defines, it) {
    spn_cmake_define_t define = cmake->defines[it];
    sp_ps_config_add_arg(scratch.mem, &config, spn_cmake_format_define(scratch.mem, define.name, define.value));
  }

  sp_da_for(cmake->args, it) {
    sp_ps_config_add_arg(scratch.mem, &config, cmake->args[it]);
  }

  sp_ps_output_t result = spn_api_subprocess(scratch.mem, unit, config);
  s32 exit_code = result.status.exit_code;
  sp_mem_end_scratch(scratch);
  return exit_code;
}

s32 spn_cmake_build(spn_cmake_t* cmake) {
  spn_pkg_unit_t* unit = spn_api_unit(cmake->build);

  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  sp_ps_output_t result = spn_api_subprocess(scratch.mem, unit, (sp_ps_config_t) {
    .command = sp_str_lit("cmake"),
    .args = {
      sp_str_lit("--build"),
      unit->paths.work,
    }
  });
  s32 exit_code = result.status.exit_code;
  sp_mem_end_scratch(scratch);
  return exit_code;
}

s32 spn_cmake_install(spn_cmake_t* cmake) {
  spn_pkg_unit_t* unit = spn_api_unit(cmake->build);

  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  sp_ps_output_t result = spn_api_subprocess(scratch.mem, unit, (sp_ps_config_t) {
    .command = sp_str_lit("cmake"),
    .args = {
      sp_str_lit("--install"),
      unit->paths.work,
    }
  });
  s32 exit_code = result.status.exit_code;
  sp_mem_end_scratch(scratch);
  return exit_code;
}

s32 spn_cmake_run(spn_cmake_t* cmake) {
  s32 err = spn_cmake_configure(cmake);
  if (err) {
    return err;
  }
  return spn_cmake_build(cmake);
}
