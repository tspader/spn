#include "spn_test.h"

#include "spn/core.h"
#include "triple/triple.h"
#include "elf_emit.h"
#include "triples.h"


typedef struct {
  spn_arch_t arch;
  spn_os_t os;
  spn_abi_t abi;
} triple_expect_t;

typedef struct {
  const c8* name;
  const c8* triple;
  spn_err_t err;
  triple_expect_t expect;
} parse_t;

static const parse_t parse_tests [] = {
  { "x64_linux_gnu",           "x86_64-linux-gnu",          .expect = { SPN_ARCH_X64,   SPN_OS_LINUX,   SPN_ABI_GNU } },
  { "x64_linux_musl",          "x86_64-linux-musl",         .expect = { SPN_ARCH_X64,   SPN_OS_LINUX,   SPN_ABI_MUSL } },
  { "arm64_linux_gnu",         "aarch64-linux-gnu",         .expect = { SPN_ARCH_ARM64, SPN_OS_LINUX,   SPN_ABI_GNU } },
  { "arm64_linux_musl",        "aarch64-linux-musl",        .expect = { SPN_ARCH_ARM64, SPN_OS_LINUX,   SPN_ABI_MUSL } },
  { "x64_windows_gnu",         "x86_64-windows-gnu",        .expect = { SPN_ARCH_X64,   SPN_OS_WINDOWS, SPN_ABI_GNU } },
  { "x64_macos",               "x86_64-macos",              .expect = { SPN_ARCH_X64,   SPN_OS_MACOS } },
  { "arm64_macos_apple",       "aarch64-macos-apple",       .expect = { SPN_ARCH_ARM64, SPN_OS_MACOS,   SPN_ABI_APPLE } },
  { "arm64_freestanding_none", "aarch64-freestanding-none", .expect = { SPN_ARCH_ARM64, SPN_OS_FREESTANDING, SPN_ABI_BARE } },
  { "x64_freestanding",        "x86_64-freestanding",       .expect = { SPN_ARCH_X64,   SPN_OS_FREESTANDING } },
  { "x64_linux_no_abi",        "x86_64-linux",              .expect = { SPN_ARCH_X64,   SPN_OS_LINUX } },
  { "x64_bare",                "x86_64",                    .expect = { SPN_ARCH_X64 } },
  { "mingw_is_not_an_abi",     "x86_64-windows-mingw",      SPN_ERR_TRIPLE_INVALID },
  { "unknown_abi",             "x86_64-linux-banana",       SPN_ERR_TRIPLE_INVALID },
  { "unknown_arch",            "x86-linux",                 SPN_ERR_TRIPLE_INVALID },
  { "too_many_parts",          "x86_64-linux-gnu-extra",    SPN_ERR_TRIPLE_INVALID },
  { "empty_part",              "x86_64--gnu",               SPN_ERR_TRIPLE_INVALID },
  { "empty",                   "",                          SPN_ERR_TRIPLE_INVALID },
};

sp_test_each(triple, parse, parse_t, parse_tests) {
  spn_triple_t triple = sp_zero;
  spn_err_t err = spn_triple_parse(sp_str_view(it->triple), &triple);
  sp_expect_eq(t, (u32)it->err, (u32)err);
  if (!err) {
    sp_expect_eq(t, triple.arch, it->expect.arch);
    sp_expect_eq(t, triple.os, it->expect.os);
    sp_expect_eq(t, triple.abi, it->expect.abi);
  }
  return SP_OK;
}


typedef struct {
  const c8* value;
} str_expect_t;

typedef struct {
  const c8* name;
  spn_triple_t triple;
  str_expect_t expect;
} to_str_t;

static const to_str_t to_str_tests [] = {
  { "x64_linux_gnu",     { SPN_ARCH_X64,   SPN_OS_LINUX,   SPN_ABI_GNU },   { "x86_64-linux-gnu" } },
  { "arm64_linux_musl",  { SPN_ARCH_ARM64, SPN_OS_LINUX,   SPN_ABI_MUSL },  { "aarch64-linux-musl" } },
  { "x64_windows_gnu",   { SPN_ARCH_X64,   SPN_OS_WINDOWS, SPN_ABI_GNU },   { "x86_64-windows-gnu" } },
  { "arm64_macos",       { SPN_ARCH_ARM64, SPN_OS_MACOS },                  { "aarch64-macos" } },
  { "arm64_macos_apple", { SPN_ARCH_ARM64, SPN_OS_MACOS,   SPN_ABI_APPLE }, { "aarch64-macos-apple" } },
  { "arm64_freestanding_none", { SPN_ARCH_ARM64, SPN_OS_FREESTANDING, SPN_ABI_BARE }, { "aarch64-freestanding-none" } },
  { "x64_linux_no_abi",  { SPN_ARCH_X64,   SPN_OS_LINUX },                  { "x86_64-linux" } },
  { "arm64_bare",        { SPN_ARCH_ARM64 },                                { "aarch64" } },
  { "empty",             { SPN_ARCH_NONE },                                 { "" } },
};

sp_test_each(triple, to_str, to_str_t, to_str_tests) {
  sp_str_t result = spn_triple_to_str(sp_test_arena(t), it->triple);
  sp_expect_str_eq_c(t, result, it->expect.value);
  return SP_OK;
}


typedef struct {
  const c8* name;
  spn_triple_t partial;
  triple_expect_t expect;
} merge_t;

static const merge_t merge_tests [] = {
  { "override_arch", { SPN_ARCH_ARM64 },                                { SPN_ARCH_ARM64, SPN_OS_LINUX,   SPN_ABI_GNU } },
  { "override_os",   { SPN_ARCH_NONE, SPN_OS_MACOS },                   { SPN_ARCH_X64,   SPN_OS_MACOS,   SPN_ABI_GNU } },
  { "override_all",  { SPN_ARCH_ARM64, SPN_OS_WINDOWS, SPN_ABI_MSVC }, { SPN_ARCH_ARM64, SPN_OS_WINDOWS, SPN_ABI_MSVC } },
  { "override_none", { SPN_ARCH_NONE },                                 { SPN_ARCH_X64,   SPN_OS_LINUX,   SPN_ABI_GNU } },
};

sp_test_each(triple, merge, merge_t, merge_tests) {
  spn_triple_t host = { SPN_ARCH_X64, SPN_OS_LINUX, SPN_ABI_GNU };
  spn_triple_t merged = spn_triple_merge(host, it->partial);
  sp_expect_eq(t, merged.arch, it->expect.arch);
  sp_expect_eq(t, merged.os, it->expect.os);
  sp_expect_eq(t, merged.abi, it->expect.abi);
  return SP_OK;
}


typedef struct {
  const c8* name;
  spn_triple_t triple;
  sp_os_lib_kind_t kind;
  str_expect_t expect;
} lib_file_name_t;

static const lib_file_name_t lib_file_name_tests [] = {
  { "linux_static",       { SPN_ARCH_X64,   SPN_OS_LINUX,   SPN_ABI_GNU },   SP_OS_LIB_STATIC, { "libfoo.a" } },
  { "linux_shared",       { SPN_ARCH_X64,   SPN_OS_LINUX,   SPN_ABI_GNU },   SP_OS_LIB_SHARED, { "libfoo.so" } },
  { "macos_static",       { SPN_ARCH_ARM64, SPN_OS_MACOS },                  SP_OS_LIB_STATIC, { "libfoo.a" } },
  { "macos_shared",       { SPN_ARCH_ARM64, SPN_OS_MACOS },                  SP_OS_LIB_SHARED, { "libfoo.dylib" } },
  { "msvc_static",        { SPN_ARCH_X64,   SPN_OS_WINDOWS, SPN_ABI_MSVC },  SP_OS_LIB_STATIC, { "foo.lib" } },
  { "msvc_shared",        { SPN_ARCH_X64,   SPN_OS_WINDOWS, SPN_ABI_MSVC },  SP_OS_LIB_SHARED, { "foo.dll" } },
  { "windows_gnu_static", { SPN_ARCH_X64,   SPN_OS_WINDOWS, SPN_ABI_GNU },   SP_OS_LIB_STATIC, { "libfoo.a" } },
  { "windows_gnu_shared", { SPN_ARCH_X64,   SPN_OS_WINDOWS, SPN_ABI_GNU },   SP_OS_LIB_SHARED, { "foo.dll" } },
  { "freestanding_static", { SPN_ARCH_ARM64, SPN_OS_FREESTANDING, SPN_ABI_BARE }, SP_OS_LIB_STATIC, { "libfoo.a" } },
};

sp_test_each(triple, lib_file_name, lib_file_name_t, lib_file_name_tests) {
  sp_str_t result = spn_triple_lib_file_name(sp_test_arena(t), it->triple, sp_str_lit("foo"), it->kind);
  sp_expect_str_eq_c(t, result, it->expect.value);
  return SP_OK;
}


typedef struct {
  const c8* name;
  spn_triple_t triple;
  str_expect_t expect;
} exe_file_name_t;

static const exe_file_name_t exe_file_name_tests [] = {
  { "linux", { SPN_ARCH_X64,    SPN_OS_LINUX,   SPN_ABI_GNU },   { "foo" } },
  { "macos", { SPN_ARCH_ARM64,  SPN_OS_MACOS },                  { "foo" } },
  { "msvc",  { SPN_ARCH_X64,    SPN_OS_WINDOWS, SPN_ABI_MSVC },  { "foo.exe" } },
  { "windows_gnu", { SPN_ARCH_X64, SPN_OS_WINDOWS, SPN_ABI_GNU }, { "foo.exe" } },
  { "wasi",  { SPN_ARCH_WASM32, SPN_OS_WASI },                   { "foo.wasm" } },
  { "freestanding", { SPN_ARCH_ARM64, SPN_OS_FREESTANDING, SPN_ABI_BARE }, { "foo.elf" } },
};

sp_test_each(triple, exe_file_name, exe_file_name_t, exe_file_name_tests) {
  sp_str_t result = spn_triple_exe_file_name(sp_test_arena(t), it->triple, sp_str_lit("foo"));
  sp_expect_str_eq_c(t, result, it->expect.value);
  return SP_OK;
}


typedef struct {
  const c8* name;
  spn_os_t os;
  spn_format_t expect;
} format_t;

static const format_t format_tests [] = {
  { "linux",        SPN_OS_LINUX,        SPN_FORMAT_ELF },
  { "freestanding", SPN_OS_FREESTANDING, SPN_FORMAT_ELF },
  { "windows",      SPN_OS_WINDOWS,      SPN_FORMAT_COFF },
  { "macos",        SPN_OS_MACOS,        SPN_FORMAT_MACHO },
  { "wasi",         SPN_OS_WASI,         SPN_FORMAT_WASM },
};

sp_test_each(triple, format, format_t, format_tests) {
  sp_expect_eq(t, (u32)it->expect, (u32)spn_os_format(it->os));
  return SP_OK;
}

sp_test(triple, host) {
  spn_triple_t host = spn_triple_host();
  sp_expect_ne(t, host.arch, SPN_ARCH_NONE);
  switch (host.os) {
    case SPN_OS_LINUX: {
      sp_expect(t, host.abi == SPN_ABI_GNU || host.abi == SPN_ABI_MUSL);
      break;
    }
    case SPN_OS_MACOS: {
      sp_expect_eq(t, host.abi, SPN_ABI_APPLE);
      break;
    }
    case SPN_OS_WINDOWS: {
      sp_expect_eq(t, host.abi, SPN_ABI_NONE);
      break;
    }
    case SPN_OS_WASI:
    case SPN_OS_FREESTANDING:
    case SPN_OS_NONE: {
      sp_unreachable_case();
    }
  }
  return SP_OK;
}


typedef struct {
  const c8* name;
  const c8* interp;
  spn_abi_t expect;
} abi_interp_t;

static const abi_interp_t abi_interp_tests [] = {
  { "gnu_x64",     "/lib64/ld-linux-x86-64.so.2",  SPN_ABI_GNU },
  { "gnu_arm64",   "/lib/ld-linux-aarch64.so.1",   SPN_ABI_GNU },
  { "musl_x64",    "/lib/ld-musl-x86_64.so.1",     SPN_ABI_MUSL },
  { "musl_arm64",  "/lib/ld-musl-aarch64.so.1",    SPN_ABI_MUSL },
  { "unknown",     "/opt/weird/loader.so",         SPN_ABI_NONE },
  { "empty",       "",                             SPN_ABI_NONE },
};

sp_test_each(triple, abi_from_interp, abi_interp_t, abi_interp_tests) {
  sp_expect_eq(t, spn_abi_from_interp(sp_str_view(it->interp)), it->expect);
  return SP_OK;
}


typedef struct {
  const c8* name;
  const c8* interp;
  spn_abi_t expect;
} host_libc_t;

static const host_libc_t host_libc_tests [] = {
  { "gnu",    "/lib64/ld-linux-x86-64.so.2", SPN_ABI_GNU },
  { "musl",   "/lib/ld-musl-x86_64.so.1",    SPN_ABI_MUSL },
  { "static", SP_NULLPTR,                    SPN_ABI_NONE },
};

sp_test_each(triple, host_libc, host_libc_t, host_libc_tests) {
  elf_spec_t spec = { .interp = it->interp };
  sp_str_t elf = elf_emit(sp_test_arena(t), &spec);
  sp_io_reader_t backing = sp_zero;
  sp_io_seeking_reader_t reader = elf_reader(&backing, elf);
  sp_expect_eq(t, spn_host_libc(sp_test_arena(t), &reader), it->expect);
  return SP_OK;
}


typedef struct {
  const c8* name;
  spn_triple_t partial;
  struct {
    spn_triple_entry_t result;
    spn_triple_t full;
  } expect;
} entry_t;

static const entry_t entry_tests [] = {
  { "full",                    { SPN_ARCH_X64,    SPN_OS_LINUX,        SPN_ABI_GNU },   { .full = { SPN_ARCH_X64,    SPN_OS_LINUX,        SPN_ABI_GNU } } },
  { "single_abi_filled",       { SPN_ARCH_ARM64,  SPN_OS_MACOS },                       { .full = { SPN_ARCH_ARM64,  SPN_OS_MACOS,        SPN_ABI_APPLE } } },
  { "wasi_abi_filled",         { SPN_ARCH_WASM32, SPN_OS_WASI },                        { .full = { SPN_ARCH_WASM32, SPN_OS_WASI,         SPN_ABI_MUSL } } },
  { "freestanding_abi_filled", { SPN_ARCH_ARM64,  SPN_OS_FREESTANDING },                { .full = { SPN_ARCH_ARM64,  SPN_OS_FREESTANDING, SPN_ABI_BARE } } },
  { "ambiguous_abi",           { SPN_ARCH_X64,    SPN_OS_LINUX },                       { .result = SPN_TRIPLE_ENTRY_MISSING_ABI } },
  { "missing_os",              { SPN_ARCH_X64 },                                        { .result = SPN_TRIPLE_ENTRY_MISSING_OS } },
  { "missing_arch",            { SPN_ARCH_NONE,   SPN_OS_LINUX,        SPN_ABI_GNU },   { .result = SPN_TRIPLE_ENTRY_MISSING_ARCH } },
  { "foreign_abi",             { SPN_ARCH_X64,    SPN_OS_MACOS,        SPN_ABI_GNU },   { .result = SPN_TRIPLE_ENTRY_FOREIGN_ABI } },
  { "foreign_arch",            { SPN_ARCH_X64,    SPN_OS_WASI },                        { .result = SPN_TRIPLE_ENTRY_FOREIGN_ARCH } },
  { "foreign_arch_before_abi", { SPN_ARCH_WASM32, SPN_OS_LINUX },                       { .result = SPN_TRIPLE_ENTRY_FOREIGN_ARCH } },
};

sp_test_each(triple, entry, entry_t, entry_tests) {
  spn_triple_t full = sp_zero;
  spn_triple_entry_t result = spn_triple_entry(it->partial, &full);
  sp_expect_eq(t, it->expect.result, result);
  if (result == SPN_TRIPLE_ENTRY_OK) {
    sp_expect_eq(t, it->expect.full.arch, full.arch);
    sp_expect_eq(t, it->expect.full.os, full.os);
    sp_expect_eq(t, it->expect.full.abi, full.abi);
  }
  return SP_OK;
}

typedef struct {
  const c8* name;
  const c8* str;
  struct {
    spn_err_t err;
    spn_triple_t triple;
  } expect;
} parse_host_t;

static const parse_host_t parse_host_tests [] = {
  { "arch_and_os",     "x86_64-linux",     { .triple = { SPN_ARCH_X64, SPN_OS_LINUX } } },
  { "abi_is_optional", "x86_64-linux-gnu", { .triple = { SPN_ARCH_X64, SPN_OS_LINUX, SPN_ABI_GNU } } },
  { "os_is_required",  "x86_64",           { .err = SPN_ERR_TRIPLE_INVALID } },
  { "unknown_arch",    "x86-linux",        { .err = SPN_ERR_TRIPLE_INVALID } },
  { "unknown_os",      "x86_64-plan9",     { .err = SPN_ERR_TRIPLE_INVALID } },
};

sp_test_each(triple, parse_host, parse_host_t, parse_host_tests) {
  spn_triple_t triple = sp_zero;
  spn_err_t err = spn_triple_parse_host(sp_cstr_as_str(it->str), &triple);
  sp_expect_eq(t, (u32)it->expect.err, (u32)err);
  if (!err) {
    sp_expect(t, spn_triple_equal(it->expect.triple, triple));
  }
  return SP_OK;
}

#define TRIPLE_MAX_ABIS 3

typedef struct {
  const c8* name;
  spn_os_t os;
  spn_abi_t expect [TRIPLE_MAX_ABIS];
} os_abis_t;

static const os_abis_t os_abis_tests [] = {
  { "linux",   SPN_OS_LINUX,   { SPN_ABI_GNU, SPN_ABI_MUSL } },
  { "windows", SPN_OS_WINDOWS, { SPN_ABI_GNU, SPN_ABI_MSVC } },
  { "macos",   SPN_OS_MACOS,   { SPN_ABI_APPLE } },
  { "wasi",    SPN_OS_WASI,    { SPN_ABI_MUSL } },
  { "freestanding", SPN_OS_FREESTANDING, { SPN_ABI_BARE } },
  { "none",    SPN_OS_NONE },
};

#define TRIPLE_MAX_ARCHS 3

typedef struct {
  const c8* name;
  spn_os_t os;
  spn_arch_t expect [TRIPLE_MAX_ARCHS];
} os_archs_t;

static const os_archs_t os_archs_tests [] = {
  { "linux",        SPN_OS_LINUX,        { SPN_ARCH_X64, SPN_ARCH_ARM64 } },
  { "windows",      SPN_OS_WINDOWS,      { SPN_ARCH_X64, SPN_ARCH_ARM64 } },
  { "macos",        SPN_OS_MACOS,        { SPN_ARCH_X64, SPN_ARCH_ARM64 } },
  { "wasi",         SPN_OS_WASI,         { SPN_ARCH_WASM32 } },
  { "freestanding", SPN_OS_FREESTANDING, { SPN_ARCH_X64, SPN_ARCH_ARM64 } },
  { "none",         SPN_OS_NONE },
};

sp_test_each(triple, os_archs, os_archs_t, os_archs_tests) {
  u32 expected = 0;
  sp_carr_detect_len(it->expect, expected, it->expect[expected]);

  const spn_arch_t* archs = SP_NULLPTR;
  u32 count = spn_os_archs(it->os, &archs);
  sp_must_eq(t, expected, count);
  sp_for(at, count) {
    sp_expect_eq(t, it->expect[at], archs[at]);
  }
  return SP_OK;
}

typedef struct {
  const c8* name;
  spn_triple_t triple;
  bool expect;
} dynamic_t;

static const dynamic_t dynamic_tests [] = {
  { "linux",        HOST_X64_LINUX,  true },
  { "windows",      TARGET_WIN_GNU,  true },
  { "macos",        HOST_ARM_MACOS,  true },
  { "wasi",         TARGET_WASM },
  { "freestanding", TARGET_ARM_BARE },
  { "linux_none",   TARGET_X64_LINUX_NONE },
};

sp_test_each(triple, dynamic, dynamic_t, dynamic_tests) {
  sp_expect_eq(t, it->expect, spn_triple_dynamic(it->triple));
  return SP_OK;
}

typedef struct {
  const c8* name;
  spn_triple_t triple;
  bool expect;
} pic_t;

static const pic_t pic_tests [] = {
  { "linux",        HOST_X64_LINUX,  true },
  { "macos",        HOST_ARM_MACOS,  true },
  { "windows",      TARGET_WIN_GNU },
  { "wasi",         TARGET_WASM },
  { "freestanding", TARGET_ARM_BARE },
  { "linux_none",   TARGET_X64_LINUX_NONE },
};

sp_test_each(triple, pic, pic_t, pic_tests) {
  sp_expect_eq(t, it->expect, spn_triple_pic(it->triple));
  return SP_OK;
}

sp_test_each(triple, os_abis, os_abis_t, os_abis_tests) {
  u32 expected = 0;
  sp_carr_detect_len(it->expect, expected, it->expect[expected]);

  const spn_abi_t* abis = SP_NULLPTR;
  u32 count = spn_os_abis(it->os, &abis);
  sp_must_eq(t, expected, count);
  sp_for(at, count) {
    sp_expect_eq(t, it->expect[at], abis[at]);
  }
  return SP_OK;
}
