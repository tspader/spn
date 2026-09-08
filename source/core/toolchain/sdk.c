#include "toolchain/sdk.h"

#include "ctx/types.h"
#include "enum/enum.h"
#include "error/error.h"
#include "paths/paths.h"
#include "triple/triple.h"

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

bool spn_sdk_declarable(spn_sdk_kind_t kind) {
  switch (kind) {
    case SPN_SDK_SYSROOT:
    case SPN_SDK_MACOS:
    case SPN_SDK_MSVC: return true;
    case SPN_SDK_NONE: return false;
  }
  SP_UNREACHABLE_RETURN(false);
}

bool spn_sdk_libc(spn_sdk_kind_t kind) {
  switch (kind) {
    case SPN_SDK_MACOS:
    case SPN_SDK_MSVC: return true;
    case SPN_SDK_NONE:
    case SPN_SDK_SYSROOT: return false;
  }
  SP_UNREACHABLE_RETURN(false);
}

static spn_sdk_msvc_t xwin_layout(sp_mem_t mem, spn_path_t root, spn_arch_t arch) {
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch_for(mem);
  spn_path_t crt = spn_path_join(scratch.mem, root, sp_str_lit("crt"));
  spn_path_t sdk = spn_path_join(scratch.mem, root, sp_str_lit("sdk"));
  spn_path_t include = spn_path_join(scratch.mem, sdk, sp_str_lit("include"));
  spn_path_t lib = spn_path_join(scratch.mem, sdk, sp_str_lit("lib"));
  sp_str_t arch_dir = spn_arch_to_str(arch);
  spn_sdk_msvc_t msvc = {
    .arch = arch,
    .include = {
      .vc = spn_path_join(mem, crt, sp_str_lit("include")),
      .ucrt = spn_path_join(mem, include, sp_str_lit("ucrt")),
      .um = spn_path_join(mem, include, sp_str_lit("um")),
      .shared = spn_path_join(mem, include, sp_str_lit("shared")),
    },
    .lib = {
      .vc = spn_path_join(mem, spn_path_join(scratch.mem, crt, sp_str_lit("lib")), arch_dir),
      .ucrt = spn_path_join(mem, spn_path_join(scratch.mem, lib, sp_str_lit("ucrt")), arch_dir),
      .um = spn_path_join(mem, spn_path_join(scratch.mem, lib, sp_str_lit("um")), arch_dir),
    },
  };
  sp_mem_end_scratch(scratch);
  return msvc;
}

spn_sdk_t spn_sdk_from_root(sp_mem_t mem, spn_sdk_kind_t kind, spn_path_t root, spn_arch_t arch) {
  switch (kind) {
    case SPN_SDK_NONE: {
      return sp_zero_struct(spn_sdk_t);
    }
    case SPN_SDK_SYSROOT:
    case SPN_SDK_MACOS: {
      return (spn_sdk_t) { .kind = kind, .root = root };
    }
    case SPN_SDK_MSVC: {
      return (spn_sdk_t) { .kind = kind, .msvc = xwin_layout(mem, root, arch) };
    }
  }
  sp_unreachable_return(sp_zero_struct(spn_sdk_t));
}

static spn_path_t absolute(sp_str_t path) {
  return (spn_path_t) { .sub = path };
}

spn_sdk_t spn_sdk_from_msvc(sp_mem_t mem, const sp_msvc_sdk_t* kits, const sp_msvc_vs_t* vs, spn_arch_t arch) {
  sp_msvc_sdk_paths_t kit = sp_msvc_sdk_render(mem, kits);
  sp_msvc_vs_paths_t tools = sp_msvc_vs_render(mem, vs);
  return (spn_sdk_t) {
    .kind = SPN_SDK_MSVC,
    .msvc = {
      .arch = arch,
      .include = {
        .vc = absolute(tools.include),
        .ucrt = absolute(kit.include_ucrt),
        .um = absolute(kit.include_um),
        .shared = absolute(kit.include_shared),
      },
      .lib = {
        .vc = absolute(tools.lib),
        .ucrt = absolute(kit.lib_ucrt),
        .um = absolute(kit.lib_um),
      },
    },
  };
}

bool spn_sdk_serves(const spn_sdk_t* sdk, spn_triple_t target) {
  if (sdk->kind != spn_sdk_kind(target)) {
    return false;
  }
  switch (sdk->kind) {
    case SPN_SDK_MACOS: return true;
    case SPN_SDK_MSVC: return sdk->msvc.arch == target.arch;
    case SPN_SDK_NONE:
    case SPN_SDK_SYSROOT: return false;
  }
  SP_UNREACHABLE_RETURN(false);
}

const spn_sdk_t* spn_sdk_find(sp_da(spn_sdk_t) sdks, spn_triple_t target) {
  sp_da_for(sdks, it) {
    if (spn_sdk_serves(&sdks[it], target)) {
      return &sdks[it];
    }
  }
  return SP_NULLPTR;
}

static sp_str_t xcrun_sdk(sp_mem_t mem) {
  sp_ps_output_t result = sp_ps_run(mem, (sp_ps_config_t) {
    .command = sp_str_lit("xcrun"),
    .args = { sp_str_lit("--show-sdk-path") },
    .io = {
      .in = { .mode = SP_PS_IO_MODE_NULL },
      .err = { .mode = SP_PS_IO_MODE_NULL },
    },
  });
  return result.status.exit_code ? sp_str_lit("") : sp_str_trim(result.out);
}

static sp_msvc_arch_t msvc_arch(spn_arch_t arch) {
  switch (arch) {
    case SPN_ARCH_X64: return SP_MSVC_ARCH_X64;
    case SPN_ARCH_ARM64: return SP_MSVC_ARCH_ARM64;
    case SPN_ARCH_WASM32:
    case SPN_ARCH_NONE: sp_unreachable_case();
  }
  SP_UNREACHABLE_RETURN(SP_MSVC_ARCH_X64);
}

static void detect_msvc(sp_mem_t mem, sp_da(spn_sdk_t)* sdks) {
  spn_arch_t arches [] = { SPN_ARCH_X64, SPN_ARCH_ARM64 };
  sp_carr_for(arches, it) {
    sp_msvc_t* found = sp_alloc_type(mem, sp_msvc_t);
    if (sp_msvc_find_ex(msvc_arch(arches[it]), found) != SP_MSVC_OK) {
      continue;
    }
    sp_da_push(*sdks, spn_sdk_from_msvc(mem, &found->sdks[0], &found->installations[0], arches[it]));
  }
}

sp_da(spn_sdk_t) spn_sdk_detect(sp_mem_t mem, sp_env_t* env, spn_triple_t host) {
  sp_da(spn_sdk_t) sdks = sp_da_new(mem, spn_sdk_t);
  sp_str_t macos = sp_env_get(env, sp_str_lit("SPN_MACOS_SDK"));
  if (sp_str_empty(macos) && host.os == SPN_OS_MACOS) {
    macos = xcrun_sdk(mem);
  }
  if (!sp_str_empty(macos)) {
    sp_da_push(sdks, ((spn_sdk_t) { .kind = SPN_SDK_MACOS, .root = absolute(sp_str_copy(mem, macos)) }));
  }
  if (host.os == SPN_OS_WINDOWS) {
    detect_msvc(mem, &sdks);
  }
  return sdks;
}

spn_sdk_t spn_sdk_resolve(sp_mem_t mem, sp_da(spn_sdk_t) sdks, const spn_toolchain_selection_t* selection) {
  spn_triple_t triple = selection->target.triple;
  if (!spn_path_empty(selection->target.sdk)) {
    return spn_sdk_from_root(mem, spn_sdk_kind(triple), selection->target.sdk, triple.arch);
  }
  const spn_sdk_t* host = spn_sdk_find(sdks, triple);
  return host ? *host : sp_zero_struct(spn_sdk_t);
}

sp_hash_t spn_sdk_hash(const spn_sdk_t* sdk) {
  switch (sdk->kind) {
    case SPN_SDK_NONE: {
      return 0;
    }
    case SPN_SDK_SYSROOT:
    case SPN_SDK_MACOS: {
      sp_hash_t parts [] = { (sp_hash_t)sdk->kind, spn_path_hash(sdk->root) };
      return sp_hash_combine(parts, sp_carr_len(parts));
    }
    case SPN_SDK_MSVC: {
      sp_hash_t parts [] = {
        (sp_hash_t)sdk->kind,
        (sp_hash_t)sdk->msvc.arch,
        spn_path_hash(sdk->msvc.include.vc),
        spn_path_hash(sdk->msvc.include.ucrt),
        spn_path_hash(sdk->msvc.include.um),
        spn_path_hash(sdk->msvc.include.shared),
        spn_path_hash(sdk->msvc.lib.vc),
        spn_path_hash(sdk->msvc.lib.ucrt),
        spn_path_hash(sdk->msvc.lib.um),
      };
      return sp_hash_combine(parts, sp_carr_len(parts));
    }
  }
  sp_unreachable_return(0);
}

