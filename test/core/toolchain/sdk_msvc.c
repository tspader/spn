#include "toolchain.h"

typedef struct {
  const c8* bin;
  const c8* include [4];
  const c8* lib [3];
} expect_t;

typedef struct {
  const c8* name;
  const c8* kits;
  const c8* kits_version;
  const c8* vs;
  const c8* tools_version;
  spn_arch_t arch;
  expect_t expect;
} test_t;

static const test_t tests [] = {
  {
    .name = "roles_map_to_install_dirs",
    .kits = "C:/K/10", .kits_version = "10.0.1.0", .vs = "C:/V", .tools_version = "14.1", .arch = SPN_ARCH_X64,
    .expect = {
      .bin = "C:/V/VC/Tools/MSVC/14.1/bin/Hostx64/x64",
      .include = { "C:/V/VC/Tools/MSVC/14.1/include", "C:/K/10/Include/10.0.1.0/ucrt", "C:/K/10/Include/10.0.1.0/um", "C:/K/10/Include/10.0.1.0/shared" },
      .lib = { "C:/V/VC/Tools/MSVC/14.1/Lib/x64", "C:/K/10/Lib/10.0.1.0/ucrt/x64", "C:/K/10/Lib/10.0.1.0/um/x64" },
    },
  },
  {
    .name = "arm64_target_has_its_own_tools",
    .kits = "C:/K/10", .kits_version = "10.0.1.0", .vs = "C:/V", .tools_version = "14.1", .arch = SPN_ARCH_ARM64,
    .expect = {
      .bin = "C:/V/VC/Tools/MSVC/14.1/bin/Hostx64/arm64",
      .include = { "C:/V/VC/Tools/MSVC/14.1/include", "C:/K/10/Include/10.0.1.0/ucrt", "C:/K/10/Include/10.0.1.0/um", "C:/K/10/Include/10.0.1.0/shared" },
      .lib = { "C:/V/VC/Tools/MSVC/14.1/Lib/arm64", "C:/K/10/Lib/10.0.1.0/ucrt/arm64", "C:/K/10/Lib/10.0.1.0/um/arm64" },
    },
  },
};

static sp_msvc_path_t msvc_path(const c8* str) {
  sp_msvc_path_t path = sp_zero;
  sp_str_copy_to(sp_cstr_as_str(str), path.data, SP_MSVC_PATH_MAX);
  path.len = (u32)sp_cstr_len(str);
  return path;
}

static sp_msvc_version_t msvc_version(const c8* str) {
  sp_msvc_version_t version = sp_zero;
  sp_str_copy_to(sp_cstr_as_str(str), version.str, SP_MSVC_VERSION_MAX);
  version.str_len = (u32)sp_cstr_len(str);
  return version;
}

static sp_msvc_arch_t msvc_arch(spn_arch_t arch) {
  return arch == SPN_ARCH_ARM64 ? SP_MSVC_ARCH_ARM64 : SP_MSVC_ARCH_X64;
}

sp_test_each(sdk_msvc, from_install, test_t, tests) {
  sp_msvc_sdk_t kits = { .version = msvc_version(it->kits_version), .root = msvc_path(it->kits), .arch = msvc_arch(it->arch) };
  sp_msvc_vs_t vs = { .version.tools = msvc_version(it->tools_version), .install_path = msvc_path(it->vs), .host = SP_MSVC_ARCH_X64, .target = msvc_arch(it->arch) };
  spn_sdk_msvc_t msvc = spn_sdk_from_msvc(sp_test_arena(t), &kits, &vs, it->arch);
  sp_expect_eq(t, (u32)it->arch, (u32)msvc.arch);
  if (test_check_path(t, msvc.bin, (test_path_t) { it->expect.bin })) {
    return SP_ERR;
  }
  spn_path_t include [] = { msvc.include.vc, msvc.include.ucrt, msvc.include.um, msvc.include.shared };
  sp_carr_for(include, at) {
    if (test_check_path(t, include[at], (test_path_t) { it->expect.include[at] })) {
      return SP_ERR;
    }
  }
  spn_path_t lib [] = { msvc.lib.vc, msvc.lib.ucrt, msvc.lib.um };
  sp_carr_for(lib, at) {
    if (test_check_path(t, lib[at], (test_path_t) { it->expect.lib[at] })) {
      return SP_ERR;
    }
  }
  return SP_OK;
}
