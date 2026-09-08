#include "toolchain/sdk.h"

#include "ctx/types.h"
#include "enum/enum.h"
#include "error/error.h"
#include "paths/paths.h"
#include "triple/triple.h"

spn_sdk_kind_t spn_sdk_kind(spn_triple_t target) {
  if (target.abi == SPN_ABI_BARE) {
    return SPN_SDK_NONE;
  }
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

static spn_sdk_macos_t macos_layout(sp_mem_t mem, spn_path_t root) {
  return (spn_sdk_macos_t) {
    .root = root,
    .include = spn_path_join(mem, root, sp_str_lit("usr/include")),
    .frameworks = spn_path_join(mem, root, sp_str_lit("System/Library/Frameworks")),
  };
}

static spn_sdk_msvc_t msvc_layout(sp_mem_t mem, spn_path_t root, spn_arch_t arch) {
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch_for(mem);
  sp_str_t name = spn_arch_to_str(arch);
  spn_sdk_msvc_t msvc = {
    .arch = arch,
    .include = {
      .vc = spn_path_join(mem, root, sp_str_lit("crt/include")),
      .ucrt = spn_path_join(mem, root, sp_str_lit("sdk/include/ucrt")),
      .um = spn_path_join(mem, root, sp_str_lit("sdk/include/um")),
      .shared = spn_path_join(mem, root, sp_str_lit("sdk/include/shared")),
    },
    .lib = {
      .vc = spn_path_join(mem, root, sp_fmt(scratch.mem, "crt/lib/{}", sp_fmt_str(name)).value),
      .ucrt = spn_path_join(mem, root, sp_fmt(scratch.mem, "sdk/lib/ucrt/{}", sp_fmt_str(name)).value),
      .um = spn_path_join(mem, root, sp_fmt(scratch.mem, "sdk/lib/um/{}", sp_fmt_str(name)).value),
    },
  };
  sp_mem_end_scratch(scratch);
  return msvc;
}

spn_sdk_t spn_sdk_sysroot(spn_path_t root) {
  return (spn_sdk_t) { .kind = SPN_SDK_SYSROOT, .root = root };
}

spn_sdk_t spn_sdk_macos(sp_mem_t mem, spn_path_t root) {
  return (spn_sdk_t) { .kind = SPN_SDK_MACOS, .macos = macos_layout(mem, root) };
}

spn_sdk_t spn_sdk_msvc(sp_mem_t mem, spn_path_t root, spn_arch_t arch) {
  return (spn_sdk_t) { .kind = SPN_SDK_MSVC, .msvc = msvc_layout(mem, root, arch) };
}

static spn_path_t absolute(sp_str_t path) {
  return (spn_path_t) { .sub = path };
}

spn_sdk_msvc_t spn_sdk_from_msvc(sp_mem_t mem, const sp_msvc_sdk_t* kits, const sp_msvc_vs_t* vs, spn_arch_t arch) {
  sp_msvc_sdk_paths_t kit = sp_msvc_sdk_render(mem, kits);
  sp_msvc_vs_paths_t tools = sp_msvc_vs_render(mem, vs);
  return (spn_sdk_msvc_t) {
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
  };
}

static const spn_sdk_msvc_t* msvc_for(const spn_sdk_host_t* host, spn_arch_t arch) {
  sp_da_for(host->msvc, it) {
    if (host->msvc[it].arch == arch) {
      return &host->msvc[it];
    }
  }
  return SP_NULLPTR;
}

spn_sdk_t spn_sdk_from_host(const spn_sdk_host_t* host, spn_triple_t target) {
  spn_sdk_t none = sp_zero;
  switch (spn_sdk_kind(target)) {
    case SPN_SDK_NONE:
    case SPN_SDK_SYSROOT: {
      return none;
    }
    case SPN_SDK_MACOS: {
      return spn_path_empty(host->macos.root) ? none : (spn_sdk_t) { .kind = SPN_SDK_MACOS, .macos = host->macos };
    }
    case SPN_SDK_MSVC: {
      const spn_sdk_msvc_t* msvc = msvc_for(host, target.arch);
      return msvc ? (spn_sdk_t) { .kind = SPN_SDK_MSVC, .msvc = *msvc } : none;
    }
  }
  sp_unreachable_return(none);
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

static void detect_msvc(sp_mem_t mem, sp_da(spn_sdk_msvc_t)* msvc) {
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch_for(mem);
  sp_msvc_t* found = sp_alloc_type(scratch.mem, sp_msvc_t);
  const spn_arch_t* arches = SP_NULLPTR;
  u32 count = spn_os_archs(SPN_OS_WINDOWS, &arches);
  sp_for(it, count) {
    if (sp_msvc_find_ex(msvc_arch(arches[it]), found) != SP_MSVC_OK) {
      continue;
    }
    sp_da_push(*msvc, spn_sdk_from_msvc(mem, &found->sdks[0], &found->installations[0], arches[it]));
  }
  sp_mem_end_scratch(scratch);
}

spn_sdk_host_t spn_sdk_detect(sp_mem_t mem, const spn_path_roots_t* roots, sp_env_t* env, spn_triple_t host) {
  spn_sdk_host_t sdks = { .msvc = sp_da_new(mem, spn_sdk_msvc_t) };
  sp_str_t macos = sp_env_get(env, sp_str_lit("SPN_MACOS_SDK"));
  if (sp_str_empty(macos) && host.os == SPN_OS_MACOS) {
    macos = xcrun_sdk(mem);
  }
  if (!sp_str_empty(macos)) {
    sdks.macos = macos_layout(mem, spn_path_canonicalize(mem, roots, absolute(macos)));
  }
  if (host.os == SPN_OS_WINDOWS) {
    detect_msvc(mem, &sdks.msvc);
  }
  return sdks;
}

spn_sdk_t spn_sdk_resolve(sp_mem_t mem, const spn_sdk_host_t* host, const spn_toolchain_selection_t* selection) {
  spn_triple_t triple = selection->target.triple;
  spn_path_t root = selection->target.sdk;
  if (spn_path_empty(root)) {
    return spn_sdk_from_host(host, triple);
  }
  switch (spn_sdk_kind(triple)) {
    case SPN_SDK_SYSROOT: return spn_sdk_sysroot(root);
    case SPN_SDK_MACOS: return spn_sdk_macos(mem, root);
    case SPN_SDK_MSVC: return spn_sdk_msvc(mem, root, triple.arch);
    case SPN_SDK_NONE: sp_unreachable_case();
  }
  sp_unreachable_return(sp_zero_struct(spn_sdk_t));
}

sp_hash_t spn_sdk_hash(const spn_sdk_t* sdk) {
  switch (sdk->kind) {
    case SPN_SDK_NONE: {
      return 0;
    }
    case SPN_SDK_SYSROOT: {
      sp_hash_t parts [] = { (sp_hash_t)sdk->kind, spn_path_hash(sdk->root) };
      return sp_hash_combine(parts, sp_carr_len(parts));
    }
    case SPN_SDK_MACOS: {
      sp_hash_t parts [] = { (sp_hash_t)sdk->kind, spn_path_hash(sdk->macos.root), spn_path_hash(sdk->macos.include), spn_path_hash(sdk->macos.frameworks) };
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
