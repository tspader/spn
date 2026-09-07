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
      .archiver = { .program = "zig", .args = { "ar" } },
      .linkers = {
        [SPN_LD_FLAVOR_ELF] = SPN_LD_FAMILY_LLD,
        [SPN_LD_FLAVOR_MINGW] = SPN_LD_FAMILY_LLD,
        [SPN_LD_FLAVOR_MACHO] = SPN_LD_FAMILY_LLD,
        [SPN_LD_FLAVOR_WASM] = SPN_LD_FAMILY_LLD,
      },
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
      .archiver = { .program = "lib" },
      .linkers = {
        [SPN_LD_FLAVOR_MSVC] = SPN_LD_FAMILY_MSVC,
      },
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
      .archiver = { .program = "ar" },
      .linkers = {
        [SPN_LD_FLAVOR_ELF] = SPN_LD_FAMILY_GNU,
        [SPN_LD_FLAVOR_MINGW] = SPN_LD_FAMILY_GNU,
        [SPN_LD_FLAVOR_MSVC] = SPN_LD_FAMILY_MSVC,
        [SPN_LD_FLAVOR_MACHO] = SPN_LD_FAMILY_LD64,
        [SPN_LD_FLAVOR_WASM] = SPN_LD_FAMILY_LLD,
      },
      .hosts = {
        { .triple = { SPN_ARCH_X64, SPN_OS_LINUX } },
        { .triple = { SPN_ARCH_ARM64, SPN_OS_LINUX } },
        { .triple = { SPN_ARCH_X64, SPN_OS_MACOS } },
        { .triple = { SPN_ARCH_ARM64, SPN_OS_MACOS } },
      },
    },
  },
  {
    .name = "llvm",
    .expect = {
      .driver = SPN_CC_DRIVER_CLANG,
      .compiler = { .program = "clang" },
      .cxx = { .program = "clang++" },
      .archiver = { .program = "llvm-ar" },
      .linkers = {
        [SPN_LD_FLAVOR_ELF] = SPN_LD_FAMILY_LLD,
        [SPN_LD_FLAVOR_MINGW] = SPN_LD_FAMILY_GNU,
        [SPN_LD_FLAVOR_MSVC] = SPN_LD_FAMILY_MSVC,
        [SPN_LD_FLAVOR_MACHO] = SPN_LD_FAMILY_LLD,
        [SPN_LD_FLAVOR_WASM] = SPN_LD_FAMILY_LLD,
      },
      .link_args = { "-fuse-ld=lld" },
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
      .archiver = { .program = "ar" },
      .linkers = {
        [SPN_LD_FLAVOR_ELF] = SPN_LD_FAMILY_GNU,
        [SPN_LD_FLAVOR_MINGW] = SPN_LD_FAMILY_GNU,
        [SPN_LD_FLAVOR_MACHO] = SPN_LD_FAMILY_LD64,
      },
    },
  },
};

typedef struct {
  spn_triple_t targets [FIXTURE_MAX_TARGETS];
} bind_expect_t;

typedef struct {
  const c8* name;
  const c8* toolchain;
  spn_triple_t host;
  bind_expect_t expect;
} bind_test_t;

static const bind_test_t bind_tests [] = {
  {
    .name = "clang_on_linux",
    .toolchain = "clang",
    .host = HOST_X64_LINUX,
    .expect = { .targets = { HOST_X64_LINUX } },
  },
  {
    .name = "clang_on_macos",
    .toolchain = "clang",
    .host = HOST_X64_MACOS,
    .expect = { .targets = { HOST_X64_MACOS } },
  },
  {
    .name = "clang_on_windows",
    .toolchain = "clang",
    .host = HOST_X64_WIN_GNU,
    .expect = { .targets = { HOST_X64_WIN_GNU } },
  },
  {
    .name = "gcc_on_linux",
    .toolchain = "gcc",
    .host = HOST_X64_LINUX,
    .expect = { .targets = { HOST_X64_LINUX, TARGET_X64_BARE } },
  },
  {
    .name = "gcc_on_macos",
    .toolchain = "gcc",
    .host = HOST_X64_MACOS,
    .expect = { .targets = { HOST_X64_MACOS } },
  },
  {
    .name = "gcc_on_windows",
    .toolchain = "gcc",
    .host = HOST_X64_WIN_GNU,
    .expect = { .targets = { HOST_X64_WIN_GNU } },
  },
  {
    .name = "gcc_on_msvc_host",
    .toolchain = "gcc",
    .host = HOST_X64_WIN_MSVC,
    .expect = { .targets = {} },
  },
  {
    .name = "zig_keeps_declared_targets_on_macos",
    .toolchain = "zig",
    .host = HOST_X64_MACOS,
    .expect = {
      .targets = {
        TARGET_WASM,
        HOST_X64_LINUX,
        HOST_X64_LINUX_MUSL,
        HOST_ARM_LINUX,
        { SPN_ARCH_ARM64, SPN_OS_LINUX, SPN_ABI_MUSL },
        HOST_X64_MACOS,
        HOST_ARM_MACOS,
        HOST_X64_WIN_GNU,
        { SPN_ARCH_ARM64, SPN_OS_WINDOWS, SPN_ABI_GNU },
        TARGET_X64_BARE,
        TARGET_ARM_BARE,
      },
    },
  },
  {
    .name = "msvc_keeps_declared_targets_on_windows",
    .toolchain = "msvc",
    .host = HOST_X64_WIN_MSVC,
    .expect = {
      .targets = {
        HOST_X64_WIN_MSVC,
        { SPN_ARCH_ARM64, SPN_OS_WINDOWS, SPN_ABI_MSVC },
      },
    },
  },
  {
    .name = "msvc_keeps_declared_targets_on_linux",
    .toolchain = "msvc",
    .host = HOST_X64_LINUX,
    .expect = {
      .targets = {
        HOST_X64_WIN_MSVC,
        { SPN_ARCH_ARM64, SPN_OS_WINDOWS, SPN_ABI_MSVC },
      },
    },
  },
};

sp_test_each(builtins, bind, bind_test_t, bind_tests) {
  spn_toolchain_catalog_t catalog = sp_zero;
  if (spn_test_builtin_catalog(t, &catalog, it->host)) {
    return SP_ERR;
  }

  spn_toolchain_info_t* info = spn_toolchain_catalog_get(&catalog, sp_cstr_as_str(it->toolchain));
  sp_must(t, info);

  u32 targets = 0;
  sp_carr_detect_len(it->expect.targets, targets, !fixture_triple_empty(it->expect.targets[targets]));
  return fixture_check_targets(t, info->targets, it->expect.targets, targets);
}

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

  const c8* order [] = { "zig", "msvc", "clang", "llvm", "gcc" };
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
