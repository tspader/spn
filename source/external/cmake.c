#include "cmake.h"

#include "unit/build.h"
#include "profile/types.h"

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

void spn_cmake(spn_build_ctx_t* build) {
  spn_cmake_t* cmake = spn_cmake_new(build);
  spn_cmake_run(cmake);
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

void spn_cmake_configure(spn_cmake_t* cmake) {
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
    build->profile->mode == SPN_DEP_BUILD_MODE_RELEASE ? SP_LIT("Release") : SP_LIT("Debug"))
  );

  sp_da_for(cmake->defines, it) {
    spn_cmake_define_t define = cmake->defines[it];
    sp_ps_config_add_arg(&config, spn_cmake_format_define(define.name, define.value));
  }

  sp_da_for(cmake->args, it) {
    sp_ps_config_add_arg(&config, cmake->args[it]);
  }

  spn_ctx_build_subprocess(build, config);
}

void spn_cmake_build(spn_cmake_t* cmake) {
  spn_build_ctx_t* build = cmake->build;

  spn_ctx_build_subprocess(build, (sp_ps_config_t) {
    .command = SP_LIT("cmake"),
    .args = {
      SP_LIT("--build"),
      spn_ctx_build_work_dir(build)
    }
  });
}

void spn_cmake_install(spn_cmake_t* cmake) {
  spn_build_ctx_t* build = cmake->build;

  spn_ctx_build_subprocess(build, (sp_ps_config_t) {
    .command = SP_LIT("cmake"),
    .args = {
      SP_LIT("--install"),
      spn_ctx_build_work_dir(build)
    }
  });
}

void spn_cmake_run(spn_cmake_t* cmake) {
  spn_cmake_configure(cmake);
  spn_cmake_build(cmake);
}
