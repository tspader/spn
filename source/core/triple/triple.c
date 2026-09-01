#include "spn/host.h"
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

spn_err_t spn_triple_parse(sp_str_t str, spn_triple_t* triple) {
  *triple = sp_zero_s(spn_triple_t);

  sp_str_t segments [3] = sp_zero;
  u32 num_segments = 0;
  sp_str_t remaining = str;
  while (true) {
    s32 sep = sp_str_find_c8(remaining, '-');
    sp_str_t segment = sep < 0 ? remaining : sp_str_prefix(remaining, sep);
    if (sp_str_empty(segment) || num_segments == sp_carr_len(segments)) {
      return SPN_ERR_TRIPLE_INVALID;
    }
    segments[num_segments++] = segment;
    if (sep < 0) break;
    remaining = sp_str_suffix(remaining, remaining.len - sep - 1);
  }

  triple->arch = spn_arch_from_str(segments[0]);
  if (!triple->arch) {
    return SPN_ERR_TRIPLE_INVALID;
  }
  if (num_segments > 1) {
    triple->os = spn_os_from_str(segments[1]);
    if (!triple->os) {
      return SPN_ERR_TRIPLE_INVALID;
    }
  }
  if (num_segments > 2) {
    triple->abi = spn_abi_from_str(segments[2]);
    if (!triple->abi) {
      return SPN_ERR_TRIPLE_INVALID;
    }
  }
  return SPN_OK;
}

sp_str_t spn_triple_to_str(sp_mem_t mem, spn_triple_t triple) {
  sp_str_t arch = spn_arch_to_str(triple.arch);
  sp_str_t os = spn_os_to_str(triple.os);
  sp_str_t abi = spn_abi_to_str(triple.abi);

  if (triple.abi) {
    return sp_fmt(mem, "{}-{}-{}", sp_fmt_str(arch), sp_fmt_str(os), sp_fmt_str(abi)).value;
  }
  if (triple.os) {
    return sp_fmt(mem, "{}-{}", sp_fmt_str(arch), sp_fmt_str(os)).value;
  }
  return arch;
}

typedef struct {
  u8 e_ident [16];
  u16 e_type;
  u16 e_machine;
  u32 e_version;
  u64 e_entry;
  u64 e_phoff;
  u64 e_shoff;
  u32 e_flags;
  u16 e_ehsize;
  u16 e_phentsize;
  u16 e_phnum;
  u16 e_shentsize;
  u16 e_shnum;
  u16 e_shstrndx;
} elf_ehdr_t;

typedef struct {
  u32 p_type;
  u32 p_flags;
  u64 p_offset;
  u64 p_vaddr;
  u64 p_paddr;
  u64 p_filesz;
  u64 p_memsz;
  u64 p_align;
} elf_phdr_t;

#define SPN_ELF_PT_INTERP 3
#define SPN_ELF_CLASS_64 2

sp_str_t spn_elf_interp(sp_mem_t mem, sp_io_seeking_reader_t* elf) {
  sp_str_t none = sp_str_lit("");
  s64 position = 0;
  u64 bytes = 0;

  elf_ehdr_t ehdr = sp_zero;
  if (sp_io_seeking_reader_seek(elf, 0, SP_IO_SEEK_SET, &position)) {
    return none;
  }
  if (sp_io_read_all(elf->reader, &ehdr, sizeof(ehdr), &bytes) || bytes != sizeof(ehdr)) {
    return none;
  }
  if (ehdr.e_ident[0] != 0x7f || ehdr.e_ident[1] != 'E' || ehdr.e_ident[2] != 'L' || ehdr.e_ident[3] != 'F') {
    return none;
  }
  if (ehdr.e_ident[4] != SPN_ELF_CLASS_64) {
    return none;
  }

  u64 interp_offset = 0;
  u64 interp_size = 0;
  sp_for(it, ehdr.e_phnum) {
    elf_phdr_t phdr = sp_zero;
    if (sp_io_seeking_reader_seek(elf, (s64)(ehdr.e_phoff + it * ehdr.e_phentsize), SP_IO_SEEK_SET, &position)) {
      return none;
    }
    if (sp_io_read_all(elf->reader, &phdr, sizeof(phdr), &bytes) || bytes != sizeof(phdr)) {
      return none;
    }
    if (phdr.p_type == SPN_ELF_PT_INTERP && !interp_size) {
      interp_offset = phdr.p_offset;
      interp_size = phdr.p_filesz;
    }
  }

  if (!interp_size) {
    return none;
  }

  c8* data = sp_alloc(mem, interp_size);
  if (sp_io_seeking_reader_seek(elf, (s64)interp_offset, SP_IO_SEEK_SET, &position)) {
    return none;
  }
  if (sp_io_read_all(elf->reader, data, interp_size, &bytes) || bytes != interp_size) {
    return none;
  }

  u32 len = 0;
  while (len < interp_size && data[len]) {
    len++;
  }
  return sp_str(data, len);
}

spn_abi_t spn_abi_from_interp(sp_str_t interp) {
  if (sp_str_find(interp, sp_str_lit("ld-musl")) >= 0) {
    return SPN_ABI_MUSL;
  }
  if (sp_str_find(interp, sp_str_lit("ld-linux")) >= 0) {
    return SPN_ABI_GNU;
  }
  return SPN_ABI_NONE;
}

spn_abi_t spn_host_libc(sp_mem_t mem, sp_io_seeking_reader_t* elf) {
  return spn_abi_from_interp(spn_elf_interp(mem, elf));
}

u32 spn_os_abis(spn_os_t os, const spn_abi_t** abis) {
  static const spn_abi_t linux_abis [] = { SPN_ABI_GNU, SPN_ABI_MUSL };
  static const spn_abi_t windows_abis [] = { SPN_ABI_GNU, SPN_ABI_MSVC };
  static const spn_abi_t macos_abis [] = { SPN_ABI_APPLE };
  static const spn_abi_t wasi_abis [] = { SPN_ABI_MUSL };
  static const spn_abi_t freestanding_abis [] = { SPN_ABI_BARE };

  switch (os) {
    case SPN_OS_LINUX: {
      *abis = linux_abis;
      return sp_carr_len(linux_abis);
    }
    case SPN_OS_WINDOWS: {
      *abis = windows_abis;
      return sp_carr_len(windows_abis);
    }
    case SPN_OS_MACOS: {
      *abis = macos_abis;
      return sp_carr_len(macos_abis);
    }
    case SPN_OS_WASI: {
      *abis = wasi_abis;
      return sp_carr_len(wasi_abis);
    }
    case SPN_OS_FREESTANDING: {
      *abis = freestanding_abis;
      return sp_carr_len(freestanding_abis);
    }
    case SPN_OS_NONE: {
      *abis = SP_NULLPTR;
      return 0;
    }
  }
  SP_UNREACHABLE_RETURN(0);
}

bool spn_triple_entry(spn_triple_t partial, spn_triple_t* full) {
  *full = sp_zero_s(spn_triple_t);
  if (!partial.arch || !partial.os) {
    return false;
  }

  const spn_abi_t* abis = SP_NULLPTR;
  u32 count = spn_os_abis(partial.os, &abis);

  if (!partial.abi) {
    if (count != 1) {
      return false;
    }
    *full = (spn_triple_t) { partial.arch, partial.os, abis[0] };
    return true;
  }

  sp_for(it, count) {
    if (abis[it] == partial.abi) {
      *full = partial;
      return true;
    }
  }
  return false;
}

spn_triple_t spn_triple_host() {
  spn_triple_t host = {0};

#if defined(SP_AMD64)
  host.arch = SPN_ARCH_X64;
#elif defined(SP_ARM64)
  host.arch = SPN_ARCH_ARM64;
#endif

  host.os = spn_os_from_sp_os(sp_os_get_kind());

#if defined(SP_LINUX)
  #if defined(__GLIBC__)
    host.abi = SPN_ABI_GNU;
  #else
    host.abi = SPN_ABI_MUSL;
  #endif

  sp_io_file_reader_t file = sp_zero;
  if (!sp_io_file_reader_from_path(&file, sp_str_lit("/bin/sh"))) {
    sp_io_seeking_reader_t elf = sp_zero;
    sp_io_seeking_reader_from_file_reader(&elf, &file);
    sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
    spn_abi_t libc = spn_host_libc(scratch.mem, &elf);
    sp_mem_end_scratch(scratch);
    sp_io_file_reader_close(&file);
    if (libc) {
      host.abi = libc;
    }
  }
#elif defined(SP_MACOS)
  host.abi = SPN_ABI_APPLE;
#elif defined(SP_WIN32)
  host.abi = SPN_ABI_GNU;
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

sp_str_t spn_triple_to_autoconf(sp_mem_t mem, spn_triple_t triple) {
  sp_str_t arch = spn_arch_to_str(triple.arch);

  // Autoconf uses GNU 4-part triples: arch-vendor-os-abi
  // For mingw: x86_64-w64-mingw32
  // For linux: x86_64-unknown-linux-gnu
  // For macos: x86_64-apple-darwin
  switch (triple.os) {
    case SPN_OS_LINUX: {
      sp_str_t abi = spn_abi_to_str(triple.abi);
      return sp_fmt(mem, "{}-unknown-linux-{}", sp_fmt_str(arch), sp_fmt_str(abi)).value;
    }
    case SPN_OS_WINDOWS: {
      return sp_fmt(mem, "{}-w64-mingw32", sp_fmt_str(arch)).value;
    }
    case SPN_OS_MACOS: {
      return sp_fmt(mem, "{}-apple-darwin", sp_fmt_str(arch)).value;
    }
    case SPN_OS_WASI: {
      return sp_fmt(mem, "{}-wasi", sp_fmt_str(arch)).value;
    }
    case SPN_OS_FREESTANDING: {
      return sp_fmt(mem, "{}-none-elf", sp_fmt_str(arch)).value;
    }
    case SPN_OS_NONE: {
      return arch;
    }
  }
  return arch;
}

sp_str_t spn_triple_lib_file_name(sp_mem_t mem, spn_triple_t triple, sp_str_t name, sp_os_lib_kind_t kind) {
  switch (kind) {
    case SP_OS_LIB_STATIC: {
      if (triple.os == SPN_OS_WINDOWS && triple.abi == SPN_ABI_MSVC) {
        return sp_fmt(mem, "{}.lib", sp_fmt_str(name)).value;
      }
      return sp_fmt(mem, "lib{}.a", sp_fmt_str(name)).value;
    }
    case SP_OS_LIB_SHARED: {
      switch (triple.os) {
        case SPN_OS_WINDOWS: return sp_fmt(mem, "{}.dll", sp_fmt_str(name)).value;
        case SPN_OS_MACOS:   return sp_fmt(mem, "lib{}.dylib", sp_fmt_str(name)).value;
        case SPN_OS_LINUX:
        case SPN_OS_WASI:
        case SPN_OS_FREESTANDING:
        case SPN_OS_NONE:    return sp_fmt(mem, "lib{}.so", sp_fmt_str(name)).value;
      }
      break;
    }
  }
  SP_UNREACHABLE_RETURN(sp_str_lit(""));
}

sp_str_t spn_triple_exe_file_name(sp_mem_t mem, spn_triple_t triple, sp_str_t name) {
  switch (triple.os) {
    case SPN_OS_WINDOWS: return sp_fmt(mem, "{}.exe", sp_fmt_str(name)).value;
    case SPN_OS_WASI:    return sp_fmt(mem, "{}.wasm", sp_fmt_str(name)).value;
    case SPN_OS_FREESTANDING: return sp_fmt(mem, "{}.elf", sp_fmt_str(name)).value;
    case SPN_OS_LINUX:
    case SPN_OS_MACOS:
    case SPN_OS_NONE:    return sp_str_copy(mem, name);
  }
  SP_UNREACHABLE_RETURN(name);
}

sp_str_t spn_os_to_cmake_system_name(spn_os_t os) {
  switch (os) {
    case SPN_OS_LINUX:   return sp_str_lit("Linux");
    case SPN_OS_WINDOWS: return sp_str_lit("Windows");
    case SPN_OS_MACOS:   return sp_str_lit("Darwin");
    case SPN_OS_WASI:   return sp_str_lit("WASI"); // @spader ? p much dead code
    case SPN_OS_FREESTANDING: return sp_str_lit("Generic");
    case SPN_OS_NONE:    return sp_str_lit("");
  }
  SP_UNREACHABLE_RETURN(sp_str_lit(""));
}
