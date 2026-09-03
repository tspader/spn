#include "toolchain.h"

typedef struct {
  const c8* name;
  fixture_toolchain_t expect;
} test_t;

static const test_t tests [] = {
  {
    .name = "zig",
    .expect = {
      .version = "0.16.0",
      .driver = SPN_CC_DRIVER_ZIG,
      .compiler = { .program = "zig", .args = { "cc" } },
      .cxx = { .program = "zig", .args = { "c++" } },
      .linker = { .program = "zig", .args = { "cc" } },
      .archiver = { .program = "zig", .args = { "ar" } },
      .hosts = {
        { .triple = { SPN_ARCH_X64, SPN_OS_LINUX }, .url = "https://ziglang.org/download/0.16.0/zig-x86_64-linux-0.16.0.tar.xz" },
        { .triple = { SPN_ARCH_ARM64, SPN_OS_LINUX }, .url = "https://ziglang.org/download/0.16.0/zig-aarch64-linux-0.16.0.tar.xz" },
        { .triple = { SPN_ARCH_X64, SPN_OS_MACOS }, .url = "https://ziglang.org/download/0.16.0/zig-x86_64-macos-0.16.0.tar.xz" },
        { .triple = { SPN_ARCH_ARM64, SPN_OS_MACOS }, .url = "https://ziglang.org/download/0.16.0/zig-aarch64-macos-0.16.0.tar.xz" },
        { .triple = { SPN_ARCH_X64, SPN_OS_WINDOWS }, .url = "https://ziglang.org/download/0.16.0/zig-x86_64-windows-0.16.0.zip" },
        { .triple = { SPN_ARCH_ARM64, SPN_OS_WINDOWS }, .url = "https://ziglang.org/download/0.16.0/zig-aarch64-windows-0.16.0.zip" },
      },
      .targets = {
        { SPN_ARCH_WASM32, SPN_OS_WASI, SPN_ABI_MUSL },
        { SPN_ARCH_X64, SPN_OS_LINUX, SPN_ABI_GNU },
        { SPN_ARCH_X64, SPN_OS_LINUX, SPN_ABI_MUSL },
        { SPN_ARCH_ARM64, SPN_OS_LINUX, SPN_ABI_GNU },
        { SPN_ARCH_ARM64, SPN_OS_LINUX, SPN_ABI_MUSL },
        { SPN_ARCH_X64, SPN_OS_MACOS, SPN_ABI_APPLE },
        { SPN_ARCH_ARM64, SPN_OS_MACOS, SPN_ABI_APPLE },
        { SPN_ARCH_X64, SPN_OS_WINDOWS, SPN_ABI_GNU },
        { SPN_ARCH_ARM64, SPN_OS_WINDOWS, SPN_ABI_GNU },
        { SPN_ARCH_X64, SPN_OS_FREESTANDING, SPN_ABI_BARE },
        { SPN_ARCH_ARM64, SPN_OS_FREESTANDING, SPN_ABI_BARE },
      },
    },
  },
  {
    .name = "msvc",
    .expect = {
      .driver = SPN_CC_DRIVER_MSVC,
      .compiler = { .program = "cl" },
      .cxx = { .program = "cl" },
      .linker = { .program = "cl" },
      .archiver = { .program = "lib" },
      .hosts = {
        { .triple = { SPN_ARCH_X64, SPN_OS_WINDOWS } },
        { .triple = { SPN_ARCH_ARM64, SPN_OS_WINDOWS } },
      },
      .targets = {
        { SPN_ARCH_X64, SPN_OS_WINDOWS, SPN_ABI_MSVC },
        { SPN_ARCH_ARM64, SPN_OS_WINDOWS, SPN_ABI_MSVC },
      },
    },
  },
  {
    .name = "clang",
    .expect = {
      .driver = SPN_CC_DRIVER_CLANG,
      .compiler = { .program = "clang" },
      .cxx = { .program = "clang++" },
      .linker = { .program = "clang" },
      .archiver = { .program = "ar" },
      .hosts = {
        { .triple = { SPN_ARCH_X64, SPN_OS_LINUX } },
        { .triple = { SPN_ARCH_ARM64, SPN_OS_LINUX } },
        { .triple = { SPN_ARCH_X64, SPN_OS_MACOS } },
        { .triple = { SPN_ARCH_ARM64, SPN_OS_MACOS } },
      },
    },
  },
  {
    .name = "gcc",
    .expect = {
      .driver = SPN_CC_DRIVER_GCC,
      .compiler = { .program = "gcc" },
      .cxx = { .program = "g++" },
      .linker = { .program = "gcc" },
      .archiver = { .program = "ar" },
    },
  },
};

static bool builtins_is_sha256(sp_str_t str) {
  return str.len == 64 && test_str_is_hex(str);
}

static sp_err_t builtins_decls(sp_test_t* t, sp_da(spn_toolchain_decl_t)* decls) {
  sp_str_t json = sp_zero;
  if (spn_test_builtin_json(t, &json)) {
    return SP_ERR;
  }
  sp_must_eq(t, (u32)SPN_OK, (u32)spn_toolchain_decls_parse(sp_test_arena(t), json, decls));
  return SP_OK;
}

sp_test_each(builtins, entries, test_t, tests) {
  sp_da(spn_toolchain_decl_t) decls = SP_NULLPTR;
  if (builtins_decls(t, &decls)) {
    return SP_ERR;
  }
  return fixture_check_decl(t, fixture_decl(decls, it->name), it->expect);
}

sp_test(builtins, declared_order) {
  spn_toolchain_catalog_t catalog = sp_zero;
  if (spn_test_builtin_catalog(t, &catalog, (spn_triple_t) HOST_X64_LINUX)) {
    return SP_ERR;
  }

  const c8* order [] = { "zig", "msvc", "clang", "gcc" };
  sp_must_eq(t, (u32)sp_carr_len(order), fixture_catalog_size(&catalog));
  sp_carr_for(order, it) {
    sp_expect_str_eq_c(t, fixture_catalog_at(&catalog, it)->name, order[it]);
  }
  return SP_OK;
}

sp_test(builtins, well_formed) {
  sp_da(spn_toolchain_decl_t) decls = SP_NULLPTR;
  if (builtins_decls(t, &decls)) {
    return SP_ERR;
  }
  sp_must(t, !sp_da_empty(decls));

  sp_da_for(decls, it) {
    spn_toolchain_decl_t* decl = &decls[it];
    sp_expect(t, !spn_arg_empty(decl->cxx.program));

    switch (decl->source) {
      case SPN_TOOLCHAIN_SOURCE_LOCAL: {
        break;
      }
      case SPN_TOOLCHAIN_SOURCE_DISTRIBUTION: {
        sp_expect(t, !sp_str_empty(decl->version));
        sp_da_for(decl->hosts, ht) {
          spn_artifact_t artifact = decl->hosts[ht].artifact;
          sp_expect(t, sp_str_starts_with(artifact.url, sp_str_lit("https://")));
          sp_expect(t, builtins_is_sha256(artifact.sha256));
          sp_expect(t, !sp_str_empty(artifact.mirror_list));
        }
        break;
      }
      case SPN_TOOLCHAIN_SOURCE_MIXED: {
        sp_unreachable_case();
      }
    }
  }

  return SP_OK;
}
