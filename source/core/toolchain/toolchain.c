#include "sp.h"
#include "macro/macro.h"
#include "paths/paths.h"
#include "toolchain/toolchain.h"

spn_path_t spn_toolchain_artifact_root(spn_artifact_t artifact) {
  return (spn_path_t) { .root = SPN_PATH_ROOT_TOOLCHAIN, .sub = artifact.sha256 };
}

spn_toolchain_launcher_t spn_toolchain_launcher_with_root(sp_mem_t mem, spn_toolchain_launcher_t launcher, spn_path_t root) {
  sp_str_t name = launcher.program.path.sub;
#if defined(SP_WIN32)
  name = sp_fmt(mem, "{}.exe", sp_fmt_str(name)).value;
#endif

  spn_toolchain_launcher_t result = launcher;
  result.program = spn_arg_path(spn_path_join(mem, root, name));
  return result;
}

static bool pathless(sp_str_t program) {
  sp_for(it, program.len) {
    if (sp_fs_is_sep(program.data[it])) {
      return false;
    }
  }
  return true;
}

static bool searched(spn_toolchain_source_t source, sp_str_t program) {
  return source != SPN_TOOLCHAIN_SOURCE_DISTRIBUTION && pathless(program);
}

spn_path_check_t spn_toolchain_path(spn_toolchain_source_t source, spn_path_root_t base, sp_str_t str, spn_path_t* path) {
  if (!spn_path_normal(str)) {
    return SPN_PATH_MALFORMED;
  }

  bool absolute = sp_fs_is_absolute(str);
  switch (source) {
    case SPN_TOOLCHAIN_SOURCE_DISTRIBUTION: {
      if (absolute) {
        return SPN_PATH_UNROOTED;
      }
      *path = (spn_path_t) { .sub = str };
      return SPN_PATH_OK;
    }
    case SPN_TOOLCHAIN_SOURCE_LOCAL:
    case SPN_TOOLCHAIN_SOURCE_MIXED: {
      if (absolute) {
        *path = (spn_path_t) { .sub = str };
        return SPN_PATH_OK;
      }
      if (base == SPN_PATH_ROOT_NONE) {
        return SPN_PATH_UNROOTED;
      }
      *path = (spn_path_t) { .root = base, .sub = str };
      return SPN_PATH_OK;
    }
  }
  SP_UNREACHABLE_RETURN(SPN_PATH_MALFORMED);
}

spn_path_check_t spn_toolchain_program(spn_toolchain_source_t source, spn_path_root_t base, sp_str_t program, spn_arg_t* arg) {
  if (!spn_path_normal(program)) {
    return SPN_PATH_MALFORMED;
  }
  if (searched(source, program)) {
    *arg = spn_arg_lit(program);
    return SPN_PATH_OK;
  }

  spn_path_t path = sp_zero;
  spn_path_check_t check = spn_toolchain_path(source, base, program, &path);
  if (check != SPN_PATH_OK) {
    return check;
  }
  *arg = spn_arg_path(path);
  return SPN_PATH_OK;
}

bool spn_toolchain_has_cxx(spn_toolchain_info_t* toolchain) {
  return !spn_arg_empty(toolchain->cxx.program);
}

spn_toolchain_ref_t spn_toolchain_ref_from_str(sp_str_t str) {
  if (sp_str_empty(str)) {
    return (spn_toolchain_ref_t) { .kind = SPN_TOOLCHAIN_REF_NONE };
  }
  if (sp_str_equal_cstr(str, "auto")) {
    return (spn_toolchain_ref_t) { .kind = SPN_TOOLCHAIN_REF_AUTO };
  }
  return (spn_toolchain_ref_t) { .kind = SPN_TOOLCHAIN_REF_NAMED, .name = str };
}

spn_toolchain_source_t spn_toolchain_source(sp_da(spn_toolchain_host_t) hosts) {
  bool local = false;
  bool distributed = false;
  sp_da_for(hosts, it) {
    if (sp_str_empty(hosts[it].artifact.url)) {
      local = true;
    }
    else {
      distributed = true;
    }
  }
  if (local && distributed) {
    return SPN_TOOLCHAIN_SOURCE_MIXED;
  }
  return distributed ? SPN_TOOLCHAIN_SOURCE_DISTRIBUTION : SPN_TOOLCHAIN_SOURCE_LOCAL;
}

spn_cc_cap_set_t spn_toolchain_driver_caps(spn_cc_driver_t driver) {
  switch (driver) {
    case SPN_CC_DRIVER_GCC: return SPN_CC_CAP_NOLIBC | SPN_CC_CAP_FUSE_LD;
    case SPN_CC_DRIVER_CLANG: return SPN_CC_CAP_TARGET_TRIPLE | SPN_CC_CAP_LLVM_TRIPLE | SPN_CC_CAP_CLANG_FRONTEND | SPN_CC_CAP_NOLIBC | SPN_CC_CAP_FUSE_LD;
    case SPN_CC_DRIVER_ZIG: return SPN_CC_CAP_TARGET_TRIPLE | SPN_CC_CAP_CLANG_FRONTEND | SPN_CC_CAP_CODEVIEW;
    case SPN_CC_DRIVER_MSVC: return 0;
    case SPN_CC_DRIVER_NONE: sp_unreachable_case();
  }
  SP_UNREACHABLE_RETURN(0);
}

bool spn_toolchain_driver_retargets(spn_cc_driver_t driver) {
  return spn_toolchain_driver_caps(driver) & SPN_CC_CAP_TARGET_TRIPLE;
}

spn_abi_t spn_default_abi(spn_cc_driver_t driver, spn_os_t os) {
  switch (os) {
    case SPN_OS_LINUX: return SPN_ABI_NONE;
    case SPN_OS_MACOS: return SPN_ABI_APPLE;
    case SPN_OS_WASI: return SPN_ABI_MUSL;
    case SPN_OS_FREESTANDING: return SPN_ABI_BARE;
    case SPN_OS_WINDOWS: return driver == SPN_CC_DRIVER_CLANG || driver == SPN_CC_DRIVER_MSVC ? SPN_ABI_MSVC : SPN_ABI_GNU;
    case SPN_OS_NONE: sp_unreachable_case();
  }
  SP_UNREACHABLE_RETURN(SPN_ABI_NONE);
}

bool spn_toolchain_driver_composes(spn_cc_driver_t driver, spn_ld_dialect_t dialect) {
  switch (driver) {
    case SPN_CC_DRIVER_GCC: return dialect == SPN_LD_DIALECT_GNU || dialect == SPN_LD_DIALECT_DARWIN;
    case SPN_CC_DRIVER_MSVC: return dialect == SPN_LD_DIALECT_LINK;
    case SPN_CC_DRIVER_CLANG:
    case SPN_CC_DRIVER_ZIG: return true;
    case SPN_CC_DRIVER_NONE: sp_unreachable_case();
  }
  SP_UNREACHABLE_RETURN(false);
}

spn_sdk_kind_t spn_sdk_kind(spn_triple_t target) {
  switch (target.os) {
    case SPN_OS_MACOS: return SPN_SDK_MACOS;
    case SPN_OS_WINDOWS: return target.abi == SPN_ABI_MSVC ? SPN_SDK_MSVC : SPN_SDK_SYSROOT;
    case SPN_OS_LINUX:
    case SPN_OS_WASI: return SPN_SDK_SYSROOT;
    case SPN_OS_FREESTANDING: return SPN_SDK_NONE;
    case SPN_OS_NONE: sp_unreachable_case();
  }
  SP_UNREACHABLE_RETURN(SPN_SDK_NONE);
}

bool spn_sdk_takes_sysroot(spn_sdk_kind_t kind) {
  switch (kind) {
    case SPN_SDK_SYSROOT:
    case SPN_SDK_MACOS: return true;
    case SPN_SDK_NONE:
    case SPN_SDK_MSVC: return false;
  }
  SP_UNREACHABLE_RETURN(false);
}

sp_str_t spn_toolchain_launcher_to_str(const spn_path_roots_t* roots, sp_mem_t mem, spn_toolchain_launcher_t launcher) {
  sp_str_t program = spn_arg_str(roots, mem, launcher.program);
  if (sp_da_empty(launcher.args)) {
    return program;
  }

  sp_io_dyn_mem_writer_t w;
  sp_io_dyn_mem_writer_init(mem, &w);
  sp_io_write_str(&w.base, program, SP_NULLPTR);
  sp_da_for(launcher.args, i) {
    sp_io_write_c8(&w.base, ' ');
    sp_io_write_str(&w.base, launcher.args[i], SP_NULLPTR);
  }
  return sp_io_dyn_mem_writer_take_str(&w);
}
