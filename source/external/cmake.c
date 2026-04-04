#include "cmake.h"

#include "enum/enum.h"
#include "session/types.h"
#include "profile/types.h"
#include "toolchain/toolchain.h"
#include "triple/triple.h"
#include "unit/build.h"
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

s32 spn_cmake(spn_build_ctx_t* build) {
  spn_cmake_t* cmake = spn_cmake_new(build);
  return spn_cmake_run(cmake);
}

spn_cmake_t* spn_cmake_new(spn_build_ctx_t* build) {
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

static sp_str_t spn_cmake_generate_toolchain_file(spn_build_ctx_t* build) {
  spn_session_t* session = build->session;
  spn_toolchain_t* tc = &session->toolchain;

  sp_str_t tools_dir = sp_fs_join_path(build->paths.generated, sp_str_lit("tools"));
  sp_fs_create_dir(tools_dir);
  sp_str_t path = sp_fs_join_path(tools_dir, sp_str_lit("spn.cmake"));

  sp_io_writer_t io = sp_io_writer_from_file(path, SP_IO_WRITE_MODE_OVERWRITE);

  spn_triple_t target = { session->profile.arch, session->profile.os, session->profile.abi };

  sp_str_t system_name = spn_os_to_cmake_system_name(session->profile.os);
  sp_io_write_str(&io, sp_format("set(CMAKE_SYSTEM_NAME {})\n", SP_FMT_STR(system_name)));
  sp_io_write_str(&io, sp_format("set(CMAKE_SYSTEM_PROCESSOR {})\n", SP_FMT_STR(spn_arch_to_str(session->profile.arch))));
  sp_io_write_str(&io, sp_format("set(CMAKE_C_COMPILER {})\n", SP_FMT_STR(spn_toolchain_launcher_to_str(tc->compiler))));
  sp_io_write_str(&io, sp_format("set(CMAKE_C_COMPILER_TARGET {})\n", SP_FMT_STR(spn_triple_to_cc_target(target))));
  sp_io_write_str(&io, sp_format("set(CMAKE_LINKER {})\n", SP_FMT_STR(spn_toolchain_launcher_to_str(tc->linker))));
  sp_io_write_str(&io, sp_format("set(CMAKE_AR {})\n", SP_FMT_STR(tc->archiver.program)));

  sp_io_writer_close(&io);
  return path;
}

s32 spn_cmake_configure(spn_cmake_t* cmake) {
  spn_build_ctx_t* build = cmake->build;

  sp_ps_config_t config = {
    .command = SP_LIT("cmake"),
    .args = {
      SP_LIT("-S"), spn_ctx_build_source_dir(build),
      SP_LIT("-B"), spn_ctx_build_work_dir(build),
    }
  };

  if (cmake->generator != SPN_CMAKE_GEN_DEFAULT) {
    sp_ps_config_add_arg(&config, SP_LIT("-G"));
    sp_ps_config_add_arg(&config, spn_cmake_gen_to_str(cmake->generator));
  }

  // Generate and pass a CMake toolchain file for cross-compilation support.
  // Even for native builds this is useful: it tells CMake exactly which
  // compiler, linker, and archiver to use.
  sp_str_t toolchain_file = spn_cmake_generate_toolchain_file(build);
  sp_ps_config_add_arg(&config, spn_cmake_format_define(
    SP_LIT("CMAKE_TOOLCHAIN_FILE"), toolchain_file));

  sp_ps_config_add_arg(&config, spn_cmake_format_define(
    SP_LIT("CMAKE_INSTALL_PREFIX"),
    spn_ctx_build_store_dir(build))
  );

  sp_ps_config_add_arg(&config, spn_cmake_format_define(
    SP_LIT("BUILD_SHARED_LIBS"),
    spn_ctx_build_linkage(build) == SPN_LIB_KIND_SHARED ? SP_LIT("ON") : SP_LIT("OFF"))
  );

  sp_ps_config_add_arg(&config, spn_cmake_format_define(
    SP_LIT("CMAKE_BUILD_TYPE"),
    build->session->profile.mode == SPN_BUILD_MODE_RELEASE ? SP_LIT("Release") : SP_LIT("Debug"))
  );

  sp_da_for(cmake->defines, it) {
    spn_cmake_define_t define = cmake->defines[it];
    sp_ps_config_add_arg(&config, spn_cmake_format_define(define.name, define.value));
  }

  sp_da_for(cmake->args, it) {
    sp_ps_config_add_arg(&config, cmake->args[it]);
  }

  sp_ps_output_t result = spn_ctx_build_subprocess(build, config);
  return result.status.exit_code;
}

s32 spn_cmake_build(spn_cmake_t* cmake) {
  spn_build_ctx_t* build = cmake->build;

  sp_ps_output_t result = spn_ctx_build_subprocess(build, (sp_ps_config_t) {
    .command = SP_LIT("cmake"),
    .args = {
      SP_LIT("--build"),
      spn_ctx_build_work_dir(build)
    }
  });
  return result.status.exit_code;
}

s32 spn_cmake_install(spn_cmake_t* cmake) {
  spn_build_ctx_t* build = cmake->build;

  sp_ps_output_t result = spn_ctx_build_subprocess(build, (sp_ps_config_t) {
    .command = SP_LIT("cmake"),
    .args = {
      SP_LIT("--install"),
      spn_ctx_build_work_dir(build)
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
