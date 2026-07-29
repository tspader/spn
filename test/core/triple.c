// The sp and spit implementations live alone in this TU; this binary is a
// single TU, so it doubles as its own spit_main.c
#define SP_IMPLEMENTATION
#include "sp.h"

#include "spn_test.h"

#include "spn.h"
#include "triple/triple.h"

s32 main(s32 argc, const c8** argv) {
  return sp_test_main(argc, argv, SP_NULLPTR);
}

typedef struct {
  spn_arch_t arch;
  spn_os_t os;
  spn_abi_t abi;
} triple_expect_t;

typedef struct {
  const c8* name;
  const c8* triple;
  triple_expect_t expect;
} from_str_t;

static const from_str_t from_str_tests [] = {
  { "x64_linux_gnu",      "x86_64-linux-gnu",      { SPN_ARCH_X64,   SPN_OS_LINUX,   SPN_ABI_GNU } },
  { "x64_linux_musl",     "x86_64-linux-musl",     { SPN_ARCH_X64,   SPN_OS_LINUX,   SPN_ABI_MUSL } },
  { "arm64_linux_gnu",    "aarch64-linux-gnu",     { SPN_ARCH_ARM64, SPN_OS_LINUX,   SPN_ABI_GNU } },
  { "arm64_linux_musl",   "aarch64-linux-musl",    { SPN_ARCH_ARM64, SPN_OS_LINUX,   SPN_ABI_MUSL } },
  { "x64_windows_mingw",  "x86_64-windows-mingw",  { SPN_ARCH_X64,   SPN_OS_WINDOWS, SPN_ABI_MINGW } },
  { "arm64_windows_mingw", "aarch64-windows-mingw", { SPN_ARCH_ARM64, SPN_OS_WINDOWS, SPN_ABI_MINGW } },
  { "x64_macos",          "x86_64-macos",          { SPN_ARCH_X64,   SPN_OS_MACOS } },
  { "arm64_macos",        "aarch64-macos",         { SPN_ARCH_ARM64, SPN_OS_MACOS } },
  { "x64_linux_no_abi",   "x86_64-linux",          { SPN_ARCH_X64,   SPN_OS_LINUX } },
  { "arm64_linux_no_abi", "aarch64-linux",         { SPN_ARCH_ARM64, SPN_OS_LINUX } },
  { "x64_bare",           "x86_64",                { SPN_ARCH_X64 } },
  { "arm64_bare",         "aarch64",               { SPN_ARCH_ARM64 } },
  { "unknown_abi",        "x86_64-linux-banana",   { SPN_ARCH_X64,   SPN_OS_LINUX } },
  { "empty",              "",                      { SPN_ARCH_NONE } },
};

sp_test_each(triple, from_str, from_str_t, from_str_tests) {
  spn_triple_t triple = spn_triple_from_str(sp_str_view(it->triple));
  sp_expect_eq(t, triple.arch, it->expect.arch);
  sp_expect_eq(t, triple.os, it->expect.os);
  sp_expect_eq(t, triple.abi, it->expect.abi);
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
  { "x64_windows_mingw", { SPN_ARCH_X64,   SPN_OS_WINDOWS, SPN_ABI_MINGW }, { "x86_64-windows-mingw" } },
  { "arm64_macos",       { SPN_ARCH_ARM64, SPN_OS_MACOS },                  { "aarch64-macos" } },
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
  { "override_all",  { SPN_ARCH_ARM64, SPN_OS_WINDOWS, SPN_ABI_MINGW }, { SPN_ARCH_ARM64, SPN_OS_WINDOWS, SPN_ABI_MINGW } },
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
  { "mingw_static",       { SPN_ARCH_X64,   SPN_OS_WINDOWS, SPN_ABI_MINGW }, SP_OS_LIB_STATIC, { "libfoo.a" } },
  { "mingw_shared",       { SPN_ARCH_X64,   SPN_OS_WINDOWS, SPN_ABI_MINGW }, SP_OS_LIB_SHARED, { "foo.dll" } },
  { "windows_gnu_static", { SPN_ARCH_X64,   SPN_OS_WINDOWS, SPN_ABI_GNU },   SP_OS_LIB_STATIC, { "libfoo.a" } },
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
  { "mingw", { SPN_ARCH_X64,    SPN_OS_WINDOWS, SPN_ABI_MINGW }, { "foo.exe" } },
  { "wasi",  { SPN_ARCH_WASM32, SPN_OS_WASI },                   { "foo.wasm" } },
};

sp_test_each(triple, exe_file_name, exe_file_name_t, exe_file_name_tests) {
  sp_str_t result = spn_triple_exe_file_name(sp_test_arena(t), it->triple, sp_str_lit("foo"));
  sp_expect_str_eq_c(t, result, it->expect.value);
  return SP_OK;
}


sp_test(triple, host) {
  spn_triple_t host = spn_triple_host();

  // should have non-NONE values for all fields on any real platform
  sp_expect_ne(t, host.arch, SPN_ARCH_NONE);
  sp_expect_ne(t, host.os, SPN_OS_NONE);

  // roundtrip: to_str then from_str should match
  sp_str_t str = spn_triple_to_str(sp_test_arena(t), host);
  spn_triple_t roundtrip = spn_triple_from_str(str);
  sp_expect_eq(t, roundtrip.arch, host.arch);
  sp_expect_eq(t, roundtrip.os, host.os);
  sp_expect_eq(t, roundtrip.abi, host.abi);

  return SP_OK;
}
