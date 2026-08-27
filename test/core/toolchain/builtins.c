#include "toolchain.h"
#include "triple/triple.h"

#define BUILTINS_MAX_HOSTS 6
#define BUILTINS_MAX_TARGETS 4

typedef struct {
  bool absent;
  spn_cc_driver_t driver;
  spn_toolchain_source_t source;
  bool host_only;
  bool any_host;
  spn_triple_t hosts [BUILTINS_MAX_HOSTS];
  spn_triple_t targets [BUILTINS_MAX_TARGETS];
} builtins_expect_t;

typedef struct {
  const c8* name;
  builtins_expect_t expect;
} builtins_test_t;

static const builtins_test_t tests [] = {
  {
    .name = "zig",
    .expect = {
      .driver = SPN_CC_DRIVER_ZIG,
      .source = SPN_TOOLCHAIN_SOURCE_DISTRIBUTION,
      .hosts = {
        { SPN_ARCH_X64, SPN_OS_LINUX, SPN_ABI_GNU },
        { SPN_ARCH_ARM64, SPN_OS_LINUX, SPN_ABI_GNU },
        { SPN_ARCH_X64, SPN_OS_MACOS },
        { SPN_ARCH_ARM64, SPN_OS_MACOS },
        { SPN_ARCH_X64, SPN_OS_WINDOWS, SPN_ABI_GNU },
        { SPN_ARCH_ARM64, SPN_OS_WINDOWS, SPN_ABI_GNU },
      },
      .targets = {
        { SPN_ARCH_WASM32, SPN_OS_WASI, SPN_ABI_MUSL },
        { SPN_ARCH_X64, SPN_OS_LINUX, SPN_ABI_GNU },
        { SPN_ARCH_ARM64, SPN_OS_MACOS, SPN_ABI_APPLE },
        { SPN_ARCH_X64, SPN_OS_WINDOWS, SPN_ABI_GNU },
      },
    },
  },
  {
    .name = "msvc",
    .expect = {
      .driver = SPN_CC_DRIVER_MSVC,
      .hosts = {
        { SPN_ARCH_X64, SPN_OS_WINDOWS },
        { SPN_ARCH_ARM64, SPN_OS_WINDOWS },
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
      .host_only = true,
      .hosts = {
        { SPN_ARCH_X64, SPN_OS_LINUX },
        { SPN_ARCH_ARM64, SPN_OS_LINUX },
        { SPN_ARCH_X64, SPN_OS_MACOS },
        { SPN_ARCH_ARM64, SPN_OS_MACOS },
      },
    },
  },
  {
    .name = "gcc",
    .expect = {
      .driver = SPN_CC_DRIVER_GCC,
      .host_only = true,
      .any_host = true,
    },
  },
  {
    .name = "system",
    .expect = { .absent = true },
  },
};

static sp_err_t builtins_catalog(sp_test_t* t, spn_toolchain_catalog_t* catalog) {
  sp_mem_t mem = sp_test_arena(t);
  sp_str_t path = test_repo_path(mem, sp_str_lit("source/core/toolchain/toolchains.json"));

  sp_str_t json = sp_zero;
  sp_must_ok(t, sp_io_read_file(mem, path, &json));
  sp_must_eq(t, (u32)SPN_OK, (u32)spn_toolchain_catalog_init(catalog, json, mem));
  return SP_OK;
}

static bool builtins_is_sha256(sp_str_t str) {
  return str.len == 64 && test_str_is_hex(str);
}

static bool builtins_has_host(spn_toolchain_info_t* info, spn_triple_t host) {
  sp_da_for(info->hosts, it) {
    if (spn_triple_match(info->hosts[it].triple, host)) {
      return true;
    }
  }
  return false;
}

sp_test_each(builtins, entries, builtins_test_t, tests) {
  spn_toolchain_catalog_t catalog = sp_zero;
  if (builtins_catalog(t, &catalog)) return SP_ERR;

  spn_toolchain_info_t* info = spn_toolchain_catalog_get(&catalog, sp_str_view(it->name));
  if (it->expect.absent) {
    sp_expect(t, !info);
    return SP_OK;
  }
  sp_must(t, info);
  sp_expect_eq(t, (u32)it->expect.driver, (u32)info->driver);
  sp_expect_eq(t, (u32)it->expect.source, (u32)info->source);
  if (it->expect.host_only) {
    sp_expect(t, sp_da_empty(info->targets));
  }
  if (it->expect.any_host) {
    sp_expect(t, sp_da_empty(info->hosts));
  }

  sp_carr_for(it->expect.hosts, at) {
    if (fixture_triple_empty(it->expect.hosts[at])) {
      break;
    }
    sp_expect(t, builtins_has_host(info, it->expect.hosts[at]));
  }

  sp_carr_for(it->expect.targets, at) {
    if (fixture_triple_empty(it->expect.targets[at])) {
      break;
    }
    sp_expect(t, fixture_has_target(info, it->expect.targets[at]));
  }

  return SP_OK;
}

sp_test(builtins, declared_order) {
  spn_toolchain_catalog_t catalog = sp_zero;
  if (builtins_catalog(t, &catalog)) return SP_ERR;

  const c8* order [] = { "zig", "msvc", "clang", "gcc" };
  sp_must_eq(t, (u32)sp_carr_len(order), fixture_catalog_size(&catalog));
  sp_carr_for(order, it) {
    sp_expect_str_eq_c(t, fixture_catalog_at(&catalog, it)->name, order[it]);
  }
  return SP_OK;
}

sp_test(builtins, well_formed) {
  spn_toolchain_catalog_t catalog = sp_zero;
  if (builtins_catalog(t, &catalog)) return SP_ERR;
  sp_must(t, fixture_catalog_size(&catalog));

  sp_om_for(catalog.entries, it) {
    spn_toolchain_info_t* info = sp_om_at(catalog.entries, it);

    sp_expect(t, spn_toolchain_catalog_get(&catalog, info->name) == info);
    sp_expect(t, !sp_str_empty(info->name));
    sp_expect_ne(t, (u32)SPN_CC_DRIVER_NONE, (u32)info->driver);
    sp_expect(t, !spn_arg_empty(info->compiler.program));
    sp_expect(t, !spn_arg_empty(info->linker.program));
    sp_expect(t, !spn_arg_empty(info->archiver.program));
    sp_expect(t, spn_toolchain_has_cxx(info));

    if (info->source == SPN_TOOLCHAIN_SOURCE_DISTRIBUTION) {
      sp_expect(t, !sp_str_empty(info->version));
    }

    sp_da_for(info->hosts, ht) {
      spn_toolchain_host_t host = info->hosts[ht];
      sp_expect_ne(t, (u32)SPN_ARCH_NONE, (u32)host.triple.arch);
      sp_expect_ne(t, (u32)SPN_OS_NONE, (u32)host.triple.os);
      switch (info->source) {
        case SPN_TOOLCHAIN_SOURCE_DISTRIBUTION: {
          sp_expect(t, sp_str_starts_with(host.artifact.url, sp_str_lit("https://")));
          sp_expect(t, builtins_is_sha256(host.artifact.sha256));
          sp_expect(t, !sp_str_empty(host.artifact.mirror_list));
          break;
        }
        case SPN_TOOLCHAIN_SOURCE_LOCAL: {
          sp_expect(t, sp_str_empty(host.artifact.url));
          sp_expect(t, sp_str_empty(host.artifact.sha256));
          break;
        }
      }
    }

    sp_da_for(info->targets, tt) {
      sp_expect_ne(t, (u32)SPN_ARCH_NONE, (u32)info->targets[tt].arch);
      sp_expect_ne(t, (u32)SPN_OS_NONE, (u32)info->targets[tt].os);
      sp_expect_ne(t, (u32)SPN_ABI_NONE, (u32)info->targets[tt].abi);
    }
  }

  return SP_OK;
}
