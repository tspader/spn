#include "api/cmake/cmake.h"

#include "api/api.h"
#include "api/core/types.h"
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

static sp_str_t spn_cmake_format_define(sp_str_t name, sp_str_t value) {
  return sp_format("-D{}={}", SP_FMT_STR(name), SP_FMT_STR(value));
}

s32 spn_cmake(spn_t* build) {
  spn_cmake_t* cmake = spn_cmake_new(build);
  return spn_cmake_run(cmake);
}

spn_cmake_t* spn_cmake_new(spn_t* build) {
  spn_cmake_t* cmake = SP_ALLOC(spn_cmake_t);
  cmake->build = build;
  cmake->generator = SPN_CMAKE_GEN_DEFAULT;
  return cmake;
}

void spn_cmake_set_generator(spn_cmake_t* cmake, spn_cmake_gen_t gen) {
  cmake->generator = gen;
}

void spn_cmake_add_define(spn_cmake_t* cmake, const c8* name, const c8* value) {
  spn_cmake_define_t define = {
    .name = sp_str_from_cstr(name),
    .value = sp_str_from_cstr(value),
  };
  sp_da_push(cmake->defines, define);
}

void spn_cmake_add_arg(spn_cmake_t* cmake, const c8* arg) {
  sp_da_push(cmake->args, sp_str_from_cstr(arg));
}

static sp_str_t spn_cmake_generate_toolchain_file(spn_pkg_unit_t* unit) {
  spn_session_t* session = unit->session;
  spn_toolchain_unit_t* tc = session->units.toolchain;
  if (!tc) return sp_str_lit("");

  sp_str_t tools_dir = sp_fs_join_path(unit->paths.generated, sp_str_lit("tools"));
  sp_fs_create_dir(tools_dir);
  sp_str_t path = sp_fs_join_path(tools_dir, sp_str_lit("spn.cmake"));

  sp_io_writer_t io = sp_io_writer_from_file(path, SP_IO_WRITE_MODE_OVERWRITE);

  spn_triple_t target = { session->profile.arch, session->profile.os, session->profile.abi };

  sp_io_write_str(&io, sp_format("set(CMAKE_SYSTEM_NAME {})\n", SP_FMT_STR(spn_os_to_cmake_system_name(session->profile.os))));
  sp_io_write_str(&io, sp_format("set(CMAKE_SYSTEM_PROCESSOR {})\n", SP_FMT_STR(spn_arch_to_str(session->profile.arch))));
  sp_io_write_str(&io, sp_format("set(CMAKE_C_COMPILER {})\n", SP_FMT_STR(spn_toolchain_launcher_to_str(tc->compiler))));
  sp_io_write_str(&io, sp_format("set(CMAKE_C_COMPILER_TARGET {})\n", SP_FMT_STR(spn_triple_to_cc_target(target))));
  sp_io_write_str(&io, sp_format("set(CMAKE_LINKER {})\n", SP_FMT_STR(spn_toolchain_launcher_to_str(tc->linker))));
  sp_io_write_str(&io, sp_format("set(CMAKE_AR {})\n", SP_FMT_STR(tc->archiver.program)));

  sp_io_writer_close(&io);
  return path;
}

s32 spn_cmake_configure(spn_cmake_t* cmake) {
  spn_pkg_unit_t* unit = spn_api_unit(cmake->build);
  spn_profile_info_t* profile = &unit->session->profile;

  sp_ps_config_t config = {
    .command = sp_str_lit("cmake"),
    .args = {
      sp_str_lit("-S"), unit->paths.source,
      sp_str_lit("-B"), unit->paths.work,
    }
  };

  if (cmake->generator != SPN_CMAKE_GEN_DEFAULT) {
    sp_ps_config_add_arg(&config, sp_str_lit("-G"));
    sp_ps_config_add_arg(&config, spn_cmake_gen_to_str(cmake->generator));
  }

  // Generate and pass a CMake toolchain file for cross-compilation support.
  // Even for native builds this is useful: it tells CMake exactly which
  // compiler, linker, and archiver to use.
  sp_str_t toolchain_file = spn_cmake_generate_toolchain_file(unit);
  if (!sp_str_empty(toolchain_file)) {
    sp_ps_config_add_arg(&config, spn_cmake_format_define(
      sp_str_lit("CMAKE_TOOLCHAIN_FILE"), toolchain_file));
  }

  sp_ps_config_add_arg(&config, spn_cmake_format_define(
    sp_str_lit("CMAKE_INSTALL_PREFIX"),
    unit->paths.store)
  );

  sp_ps_config_add_arg(&config, spn_cmake_format_define(
    sp_str_lit("BUILD_SHARED_LIBS"),
    profile->linkage == SPN_LIB_KIND_SHARED ? sp_str_lit("ON") : sp_str_lit("OFF"))
  );

  sp_ps_config_add_arg(&config, spn_cmake_format_define(
    sp_str_lit("CMAKE_BUILD_TYPE"),
    profile->mode == SPN_BUILD_MODE_RELEASE ? sp_str_lit("Release") : sp_str_lit("Debug"))
  );

  sp_da_for(cmake->defines, it) {
    spn_cmake_define_t define = cmake->defines[it];
    sp_ps_config_add_arg(&config, spn_cmake_format_define(define.name, define.value));
  }

  sp_da_for(cmake->args, it) {
    sp_ps_config_add_arg(&config, cmake->args[it]);
  }

  sp_ps_output_t result = spn_api_subprocess(unit, config);
  return result.status.exit_code;
}

s32 spn_cmake_build(spn_cmake_t* cmake) {
  spn_pkg_unit_t* unit = spn_api_unit(cmake->build);

  sp_ps_output_t result = spn_api_subprocess(unit, (sp_ps_config_t) {
    .command = sp_str_lit("cmake"),
    .args = {
      sp_str_lit("--build"),
      unit->paths.work,
    }
  });
  return result.status.exit_code;
}

s32 spn_cmake_install(spn_cmake_t* cmake) {
  spn_pkg_unit_t* unit = spn_api_unit(cmake->build);

  sp_ps_output_t result = spn_api_subprocess(unit, (sp_ps_config_t) {
    .command = sp_str_lit("cmake"),
    .args = {
      sp_str_lit("--install"),
      unit->paths.work,
    }
  });
  return result.status.exit_code;
}

s32 spn_cmake_run(spn_cmake_t* cmake) {
  s32 err = spn_cmake_configure(cmake);
  if (err) {
    return err;
  }
  return spn_cmake_build(cmake);
}
