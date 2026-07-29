#include "fixture.h"

#define BUILTINS_MAX_HOSTS 6
#define BUILTINS_MAX_TARGETS 4

typedef struct {
  spn_cc_driver_t driver;
  spn_toolchain_source_t source;
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
      .driver = SPN_CC_DRIVER_CLANG,
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
        { SPN_ARCH_WASM32, SPN_OS_WASI },
        { SPN_ARCH_X64, SPN_OS_LINUX, SPN_ABI_GNU },
        { SPN_ARCH_ARM64, SPN_OS_MACOS },
        { SPN_ARCH_X64, SPN_OS_WINDOWS, SPN_ABI_GNU },
      },
    },
  },
  {
    .name = "msvc",
    .expect = {
      .driver = SPN_CC_DRIVER_MSVC,
      .targets = {
        { SPN_ARCH_X64, SPN_OS_WINDOWS, SPN_ABI_MSVC },
        { SPN_ARCH_ARM64, SPN_OS_WINDOWS, SPN_ABI_MSVC },
      },
    },
  },
  {
    .name = "system",
    .expect = {
      .driver = SPN_CC_DRIVER_GCC,
    },
  },
};

static sp_err_t builtins_catalog(sp_test_t* t, spn_toolchain_catalog_t* catalog) {
  sp_mem_t mem = sp_test_arena(t);
  sp_str_t path = test_repo_path(mem, sp_str_lit("source/toolchain/toolchains.json"));

  sp_str_t json = sp_zero;
  sp_must_ok(t, sp_io_read_file(mem, path, &json));
  sp_must_eq(t, (u32)SPN_OK, (u32)spn_toolchain_catalog_init(catalog, json, mem));
  return SP_OK;
}

static bool builtins_is_sha256(sp_str_t str) {
  if (str.len != 64) {
    return false;
  }
  sp_for(it, str.len) {
    c8 c = str.data[it];
    bool hex = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
    if (!hex) {
      return false;
    }
  }
  return true;
}

sp_test_each(builtins, entries, builtins_test_t, tests) {
  spn_toolchain_catalog_t catalog = sp_zero;
  if (builtins_catalog(t, &catalog)) return SP_ERR;

  spn_toolchain_info_t* info = spn_toolchain_catalog_get(&catalog, sp_str_view(it->name));
  sp_must(t, info);
  sp_expect_eq(t, (u32)it->expect.driver, (u32)info->driver);
  sp_expect_eq(t, (u32)it->expect.source, (u32)info->source);

  sp_carr_for(it->expect.hosts, at) {
    if (fixture_triple_empty(it->expect.hosts[at])) {
      break;
    }
    sp_expect(t, !sp_opt_is_null(spn_toolchain_select_artifact(info->hosts, it->expect.hosts[at])));
  }

  sp_carr_for(it->expect.targets, at) {
    if (fixture_triple_empty(it->expect.targets[at])) {
      break;
    }
    sp_expect(t, fixture_has_target(info, it->expect.targets[at]));
  }

  return SP_OK;
}

sp_test(builtins, zig_is_declared_first) {
  spn_toolchain_catalog_t catalog = sp_zero;
  if (builtins_catalog(t, &catalog)) return SP_ERR;

  sp_must(t, fixture_catalog_size(&catalog));
  sp_expect_str_eq_c(t, fixture_catalog_at(&catalog, 0)->name, "zig");
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
    sp_expect(t, !sp_str_empty(info->compiler.program));
    sp_expect(t, !sp_str_empty(info->linker.program));
    sp_expect(t, !sp_str_empty(info->archiver.program));
    sp_expect(t, spn_toolchain_has_cxx(info));

    if (info->source == SPN_TOOLCHAIN_SOURCE_DISTRIBUTION) {
      sp_expect(t, !sp_str_empty(info->version));
    }

    sp_da_for(info->hosts, ht) {
      spn_toolchain_host_t host = info->hosts[ht];
      sp_expect_ne(t, (u32)SPN_ARCH_NONE, (u32)host.triple.arch);
      sp_expect_ne(t, (u32)SPN_OS_NONE, (u32)host.triple.os);
      sp_expect(t, sp_str_starts_with(host.artifact.url, sp_str_lit("https://")));
      sp_expect(t, builtins_is_sha256(host.artifact.sha256));
      sp_expect(t, !sp_str_empty(host.artifact.mirror_list));
    }

    sp_da_for(info->targets, tt) {
      sp_expect(t, !fixture_triple_empty(info->targets[tt]));
    }
  }

  return SP_OK;
}
