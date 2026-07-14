#define SP_IMPLEMENTATION
#include "sp.h"

#define UTEST_IMPLEMENTATION
#include "utest.h"

#include "test.h"
#include "spn.h"
#include "triple/triple.h"

UTEST_MAIN();


typedef struct {
  const c8* triple;
  spn_arch_t arch;
  spn_os_t os;
  spn_abi_t abi;
} from_str_t;

UTEST(triple, from_str) {
  from_str_t tests [] = {
    { "x86_64-linux-gnu",      SPN_ARCH_X64,   SPN_OS_LINUX,   SPN_ABI_GNU },
    { "x86_64-linux-musl",     SPN_ARCH_X64,   SPN_OS_LINUX,   SPN_ABI_MUSL },
    { "aarch64-linux-gnu",     SPN_ARCH_ARM64, SPN_OS_LINUX,   SPN_ABI_GNU },
    { "aarch64-linux-musl",    SPN_ARCH_ARM64, SPN_OS_LINUX,   SPN_ABI_MUSL },
    { "x86_64-windows-mingw",  SPN_ARCH_X64,   SPN_OS_WINDOWS, SPN_ABI_MINGW },
    { "aarch64-windows-mingw", SPN_ARCH_ARM64, SPN_OS_WINDOWS, SPN_ABI_MINGW },
    { "x86_64-macos",          SPN_ARCH_X64,   SPN_OS_MACOS,   SPN_ABI_NONE },
    { "aarch64-macos",         SPN_ARCH_ARM64, SPN_OS_MACOS,   SPN_ABI_NONE },
    { "x86_64-linux",          SPN_ARCH_X64,   SPN_OS_LINUX,   SPN_ABI_NONE },
    { "aarch64-linux",         SPN_ARCH_ARM64, SPN_OS_LINUX,   SPN_ABI_NONE },
    { "x86_64",                SPN_ARCH_X64,   SPN_OS_NONE,    SPN_ABI_NONE },
    { "aarch64",               SPN_ARCH_ARM64, SPN_OS_NONE,    SPN_ABI_NONE },
    { "x86_64-linux-banana",   SPN_ARCH_X64,   SPN_OS_LINUX,   SPN_ABI_NONE },
    { "",                      SPN_ARCH_NONE,  SPN_OS_NONE,    SPN_ABI_NONE },
  };

  sp_carr_for(tests, it) {
    spn_triple_t triple = spn_triple_from_str(sp_str_view(tests[it].triple));
    EXPECT_EQ(triple.arch, tests[it].arch);
    EXPECT_EQ(triple.os, tests[it].os);
    EXPECT_EQ(triple.abi, tests[it].abi);
  }
}


typedef struct {
  spn_triple_t triple;
  const c8* expected;
} to_str_t;

UTEST(triple, to_str) {
  to_str_t tests [] = {
    { { SPN_ARCH_X64,   SPN_OS_LINUX,   SPN_ABI_GNU },   "x86_64-linux-gnu" },
    { { SPN_ARCH_ARM64, SPN_OS_LINUX,   SPN_ABI_MUSL },  "aarch64-linux-musl" },
    { { SPN_ARCH_X64,   SPN_OS_WINDOWS, SPN_ABI_MINGW }, "x86_64-windows-mingw" },
    { { SPN_ARCH_ARM64, SPN_OS_MACOS,   SPN_ABI_NONE },  "aarch64-macos" },
    { { SPN_ARCH_X64,   SPN_OS_LINUX,   SPN_ABI_NONE },  "x86_64-linux" },
    { { SPN_ARCH_ARM64, SPN_OS_NONE,    SPN_ABI_NONE },  "aarch64" },
    { { SPN_ARCH_NONE,  SPN_OS_NONE,    SPN_ABI_NONE },  "" },
  };

  sp_carr_for(tests, it) {
    sp_str_t result = spn_triple_to_str(sp_mem_os_new(), tests[it].triple);
    EXPECT_TRUE(sp_str_equal_cstr(result, tests[it].expected));
  }
}


UTEST(triple, merge) {
  spn_triple_t host = { SPN_ARCH_X64, SPN_OS_LINUX, SPN_ABI_GNU };

  // override just arch
  {
    spn_triple_t partial = { SPN_ARCH_ARM64, 0, 0 };
    spn_triple_t merged = spn_triple_merge(host, partial);
    EXPECT_EQ(merged.arch, SPN_ARCH_ARM64);
    EXPECT_EQ(merged.os, SPN_OS_LINUX);
    EXPECT_EQ(merged.abi, SPN_ABI_GNU);
  }

  // override just os
  {
    spn_triple_t partial = { 0, SPN_OS_MACOS, 0 };
    spn_triple_t merged = spn_triple_merge(host, partial);
    EXPECT_EQ(merged.arch, SPN_ARCH_X64);
    EXPECT_EQ(merged.os, SPN_OS_MACOS);
    EXPECT_EQ(merged.abi, SPN_ABI_GNU);
  }

  // override everything
  {
    spn_triple_t partial = { SPN_ARCH_ARM64, SPN_OS_WINDOWS, SPN_ABI_MINGW };
    spn_triple_t merged = spn_triple_merge(host, partial);
    EXPECT_EQ(merged.arch, SPN_ARCH_ARM64);
    EXPECT_EQ(merged.os, SPN_OS_WINDOWS);
    EXPECT_EQ(merged.abi, SPN_ABI_MINGW);
  }

  // override nothing
  {
    spn_triple_t partial = { 0, 0, 0 };
    spn_triple_t merged = spn_triple_merge(host, partial);
    EXPECT_EQ(merged.arch, SPN_ARCH_X64);
    EXPECT_EQ(merged.os, SPN_OS_LINUX);
    EXPECT_EQ(merged.abi, SPN_ABI_GNU);
  }
}


typedef struct {
  spn_triple_t triple;
  sp_os_lib_kind_t kind;
  const c8* expected;
} lib_file_name_t;

UTEST(triple, lib_file_name) {
  lib_file_name_t tests [] = {
    { { SPN_ARCH_X64,   SPN_OS_LINUX,   SPN_ABI_GNU },   SP_OS_LIB_STATIC, "libfoo.a" },
    { { SPN_ARCH_X64,   SPN_OS_LINUX,   SPN_ABI_GNU },   SP_OS_LIB_SHARED, "libfoo.so" },
    { { SPN_ARCH_ARM64, SPN_OS_MACOS,   SPN_ABI_NONE },  SP_OS_LIB_STATIC, "libfoo.a" },
    { { SPN_ARCH_ARM64, SPN_OS_MACOS,   SPN_ABI_NONE },  SP_OS_LIB_SHARED, "libfoo.dylib" },
    { { SPN_ARCH_X64,   SPN_OS_WINDOWS, SPN_ABI_MSVC },  SP_OS_LIB_STATIC, "foo.lib" },
    { { SPN_ARCH_X64,   SPN_OS_WINDOWS, SPN_ABI_MSVC },  SP_OS_LIB_SHARED, "foo.dll" },
    { { SPN_ARCH_X64,   SPN_OS_WINDOWS, SPN_ABI_MINGW }, SP_OS_LIB_STATIC, "libfoo.a" },
    { { SPN_ARCH_X64,   SPN_OS_WINDOWS, SPN_ABI_MINGW }, SP_OS_LIB_SHARED, "foo.dll" },
    { { SPN_ARCH_X64,   SPN_OS_WINDOWS, SPN_ABI_GNU },   SP_OS_LIB_STATIC, "libfoo.a" },
  };

  sp_carr_for(tests, it) {
    sp_str_t result = spn_triple_lib_file_name(sp_mem_os_new(), tests[it].triple, sp_str_lit("foo"), tests[it].kind);
    EXPECT_TRUE(sp_str_equal_cstr(result, tests[it].expected));
  }
}

typedef struct {
  spn_triple_t triple;
  const c8* expected;
} exe_file_name_t;

UTEST(triple, exe_file_name) {
  exe_file_name_t tests [] = {
    { { SPN_ARCH_X64,   SPN_OS_LINUX,   SPN_ABI_GNU },   "foo" },
    { { SPN_ARCH_ARM64, SPN_OS_MACOS,   SPN_ABI_NONE },  "foo" },
    { { SPN_ARCH_X64,   SPN_OS_WINDOWS, SPN_ABI_MSVC },  "foo.exe" },
    { { SPN_ARCH_X64,   SPN_OS_WINDOWS, SPN_ABI_MINGW }, "foo.exe" },
    { { SPN_ARCH_WASM32, SPN_OS_WASI,   SPN_ABI_NONE },  "foo.wasm" },
  };

  sp_carr_for(tests, it) {
    sp_str_t result = spn_triple_exe_file_name(sp_mem_os_new(), tests[it].triple, sp_str_lit("foo"));
    EXPECT_TRUE(sp_str_equal_cstr(result, tests[it].expected));
  }
}


typedef struct {
  spn_os_version_t a;
  spn_os_version_t b;
  struct {
    bool less;
  } expect;
} os_version_less_t;

UTEST(triple, os_version_less) {
  os_version_less_t tests [] = {
    { .a = { 12, 4 }, .b = { 13 },    .expect = { .less = true } },
    { .a = { 13 },    .b = { 13, 1 }, .expect = { .less = true } },
    { .a = { 13, 1 }, .b = { 13, 1 } },
    { .a = { 13, 1 }, .b = { 12, 4 } },
    { .b = { 1 },     .expect = { .less = true } },
  };

  sp_carr_for(tests, it) {
    EXPECT_EQ(spn_os_version_less(tests[it].a, tests[it].b), tests[it].expect.less);
  }
}

typedef struct {
  spn_os_version_t version;
  struct {
    bool present;
  } expect;
} os_version_present_t;

UTEST(triple, os_version_present) {
  os_version_present_t tests [] = {
    { .version = { 13 },   .expect = { .present = true } },
    { .version = { 0, 4 }, .expect = { .present = true } },
    { .version = { 0 } },
  };

  sp_carr_for(tests, it) {
    EXPECT_EQ(spn_os_version_present(tests[it].version), tests[it].expect.present);
  }
}


UTEST(triple, host) {
  spn_triple_t host = spn_triple_host();

  // should have non-NONE values for all fields on any real platform
  EXPECT_NE(host.arch, SPN_ARCH_NONE);
  EXPECT_NE(host.os, SPN_OS_NONE);

  // roundtrip: to_str then from_str should match
  sp_str_t str = spn_triple_to_str(sp_mem_os_new(), host);
  spn_triple_t roundtrip = spn_triple_from_str(str);
  EXPECT_EQ(roundtrip.arch, host.arch);
  EXPECT_EQ(roundtrip.os, host.os);
  EXPECT_EQ(roundtrip.abi, host.abi);
}
