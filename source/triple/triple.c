#include "triple/triple.h"
#include "enum/enum.h"

spn_triple_t spn_triple_from_str(sp_str_t str) {
  spn_triple_t result = {0};
  if (sp_str_empty(str)) return result;

  // Split on '-': arch-os-abi
  sp_str_t remaining = str;

  // First component: arch
  s32 sep = sp_str_find(remaining, sp_str_lit("-"));
  if (sep < 0) {
    result.arch = spn_arch_from_str(remaining);
    return result;
  }
  result.arch = spn_arch_from_str(sp_str_prefix(remaining, sep));
  remaining = sp_str_suffix(remaining, remaining.len - sep - 1);

  // Second component: os
  sep = sp_str_find(remaining, sp_str_lit("-"));
  if (sep < 0) {
    result.os = spn_os_from_str(remaining);
    return result;
  }
  result.os = spn_os_from_str(sp_str_prefix(remaining, sep));
  remaining = sp_str_suffix(remaining, remaining.len - sep - 1);

  // Third component: abi
  result.abi = spn_abi_from_str(remaining);

  return result;
}

sp_str_t spn_triple_to_str(spn_triple_t triple) {
  sp_str_t arch = spn_arch_to_str(triple.arch);
  sp_str_t os = spn_os_to_str(triple.os);
  sp_str_t abi = spn_abi_to_str(triple.abi);

  if (triple.abi) {
    return sp_format("{}-{}-{}", SP_FMT_STR(arch), SP_FMT_STR(os), SP_FMT_STR(abi));
  }
  if (triple.os) {
    return sp_format("{}-{}", SP_FMT_STR(arch), SP_FMT_STR(os));
  }
  return arch;
}

spn_triple_t spn_triple_host(void) {
  spn_triple_t host = {0};

#if defined(__x86_64__) || defined(_M_X64)
  host.arch = SPN_ARCH_X64;
#elif defined(__aarch64__) || defined(_M_ARM64)
  host.arch = SPN_ARCH_ARM64;
#endif

  host.os = spn_os_from_sp_os(sp_os_get_kind());

#if defined(__linux__)
  #if defined(__GLIBC__)
    host.abi = SPN_ABI_GNU;
  #elif defined(__MUSL__)
    host.abi = SPN_ABI_MUSL;
  #else
    host.abi = SPN_ABI_GNU;
  #endif
#elif defined(_WIN32)
  #if defined(__MINGW32__) || defined(__MINGW64__)
    host.abi = SPN_ABI_MINGW;
  #else
    host.abi = SPN_ABI_MSVC;
  #endif
#endif

  return host;
}

spn_triple_t spn_triple_merge(spn_triple_t base, spn_triple_t partial) {
  return (spn_triple_t) {
    .arch = partial.arch ? partial.arch : base.arch,
    .os   = partial.os   ? partial.os   : base.os,
    .abi  = partial.abi  ? partial.abi  : base.abi,
  };
}

bool spn_triple_match(spn_triple_t entry, spn_triple_t target) {
  if (entry.arch && entry.arch != target.arch) return false;
  if (entry.os   && entry.os   != target.os)   return false;
  if (entry.abi  && entry.abi  != target.abi)   return false;
  return true;
}

sp_str_t spn_triple_to_cc_target(spn_triple_t triple) {
  sp_str_t arch = spn_arch_to_str(triple.arch);
  sp_str_t os = spn_os_to_str(triple.os);

  // Clang/Zig use "gnu" where spn uses "mingw"
  sp_str_t abi;
  switch (triple.abi) {
    case SPN_ABI_MINGW: abi = sp_str_lit("gnu"); break;
    default:            abi = spn_abi_to_str(triple.abi); break;
  }

  if (triple.abi) {
    return sp_format("{}-{}-{}", SP_FMT_STR(arch), SP_FMT_STR(os), SP_FMT_STR(abi));
  }
  if (triple.os) {
    return sp_format("{}-{}", SP_FMT_STR(arch), SP_FMT_STR(os));
  }
  return arch;
}

sp_str_t spn_triple_to_autoconf(spn_triple_t triple) {
  sp_str_t arch = spn_arch_to_str(triple.arch);

  // Autoconf uses GNU 4-part triples: arch-vendor-os-abi
  // For mingw: x86_64-w64-mingw32
  // For linux: x86_64-unknown-linux-gnu
  // For macos: x86_64-apple-darwin
  switch (triple.os) {
    case SPN_OS_LINUX: {
      sp_str_t abi = spn_abi_to_str(triple.abi);
      return sp_format("{}-unknown-linux-{}", SP_FMT_STR(arch), SP_FMT_STR(abi));
    }
    case SPN_OS_WINDOWS: {
      return sp_format("{}-w64-mingw32", SP_FMT_STR(arch));
    }
    case SPN_OS_MACOS: {
      return sp_format("{}-apple-darwin", SP_FMT_STR(arch));
    }
    case SPN_OS_NONE: {
      return arch;
    }
  }
  return arch;
}

sp_str_t spn_os_to_cmake_system_name(spn_os_t os) {
  switch (os) {
    case SPN_OS_LINUX:   return sp_str_lit("Linux");
    case SPN_OS_WINDOWS: return sp_str_lit("Windows");
    case SPN_OS_MACOS:   return sp_str_lit("Darwin");
    case SPN_OS_NONE:    return sp_str_lit("");
  }
  SP_UNREACHABLE_RETURN(sp_str_lit(""));
}
