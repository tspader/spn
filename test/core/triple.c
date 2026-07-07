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
    // full triples
    { "x86_64-linux-gnu",      SPN_ARCH_X64,   SPN_OS_LINUX,   SPN_ABI_GNU },
    { "x86_64-linux-musl",     SPN_ARCH_X64,   SPN_OS_LINUX,   SPN_ABI_MUSL },
    { "aarch64-linux-gnu",     SPN_ARCH_ARM64, SPN_OS_LINUX,   SPN_ABI_GNU },
    { "aarch64-linux-musl",    SPN_ARCH_ARM64, SPN_OS_LINUX,   SPN_ABI_MUSL },
    { "x86_64-windows-mingw",  SPN_ARCH_X64,   SPN_OS_WINDOWS, SPN_ABI_MINGW },
    { "aarch64-windows-mingw", SPN_ARCH_ARM64, SPN_OS_WINDOWS, SPN_ABI_MINGW },
    { "x86_64-macos",          SPN_ARCH_X64,   SPN_OS_MACOS,   SPN_ABI_NONE },
    { "aarch64-macos",         SPN_ARCH_ARM64, SPN_OS_MACOS,   SPN_ABI_NONE },

    // partial: arch-os (no abi)
    { "x86_64-linux",          SPN_ARCH_X64,   SPN_OS_LINUX,   SPN_ABI_NONE },
    { "aarch64-linux",         SPN_ARCH_ARM64, SPN_OS_LINUX,   SPN_ABI_NONE },

    // partial: arch only
    { "x86_64",                SPN_ARCH_X64,   SPN_OS_NONE,    SPN_ABI_NONE },
    { "aarch64",               SPN_ARCH_ARM64, SPN_OS_NONE,    SPN_ABI_NONE },

    // unknown abi
    { "x86_64-linux-banana",   SPN_ARCH_X64,   SPN_OS_LINUX,   SPN_ABI_NONE },

    // empty
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
    // full triples
    { { SPN_ARCH_X64,   SPN_OS_LINUX,   SPN_ABI_GNU },   "x86_64-linux-gnu" },
    { { SPN_ARCH_ARM64, SPN_OS_LINUX,   SPN_ABI_MUSL },  "aarch64-linux-musl" },
    { { SPN_ARCH_X64,   SPN_OS_WINDOWS, SPN_ABI_MINGW }, "x86_64-windows-mingw" },
    { { SPN_ARCH_ARM64, SPN_OS_MACOS,   SPN_ABI_NONE },  "aarch64-macos" },

    // partial: arch-os
    { { SPN_ARCH_X64,   SPN_OS_LINUX,   SPN_ABI_NONE },  "x86_64-linux" },

    // partial: arch only
    { { SPN_ARCH_ARM64, SPN_OS_NONE,    SPN_ABI_NONE },  "aarch64" },

    // empty
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
