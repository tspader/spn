#ifndef SPN_TEST_TOOLCHAIN_FIXTURE_H
#define SPN_TEST_TOOLCHAIN_FIXTURE_H

#include "spn_test.h"
#include "hash/digest/digest.h"
#include "paths/paths.h"
#include "toolchain/toolchain.h"
#include "triple/triple.h"

#define FIXTURE_MAX_ARGS 2
#define FIXTURE_MAX_HOSTS 6
#define FIXTURE_MAX_TARGETS 12

#define HOST_X64_LINUX      { SPN_ARCH_X64, SPN_OS_LINUX, SPN_ABI_GNU }
#define HOST_X64_LINUX_MUSL { SPN_ARCH_X64, SPN_OS_LINUX, SPN_ABI_MUSL }
#define HOST_ARM_LINUX      { SPN_ARCH_ARM64, SPN_OS_LINUX, SPN_ABI_GNU }
#define HOST_ARM_MACOS      { SPN_ARCH_ARM64, SPN_OS_MACOS, SPN_ABI_APPLE }
#define HOST_X64_WIN_MSVC   { SPN_ARCH_X64, SPN_OS_WINDOWS, SPN_ABI_MSVC }
#define HOST_X64_WIN_GNU    { SPN_ARCH_X64, SPN_OS_WINDOWS, SPN_ABI_GNU }
#define TARGET_WIN_GNU      { SPN_ARCH_X64, SPN_OS_WINDOWS, SPN_ABI_GNU }
#define TARGET_WASM         { SPN_ARCH_WASM32, SPN_OS_WASI, SPN_ABI_MUSL }
#define TARGET_X64_BARE     { SPN_ARCH_X64, SPN_OS_FREESTANDING, SPN_ABI_BARE }
#define TARGET_ARM_BARE     { SPN_ARCH_ARM64, SPN_OS_FREESTANDING, SPN_ABI_BARE }

typedef struct {
  const c8* program;
  const c8* args [FIXTURE_MAX_ARGS];
} fixture_launcher_t;

typedef struct {
  spn_triple_t triple;
  const c8* url;
  const c8* sha256;
  const c8* mirrors;
} fixture_host_t;

typedef struct {
  const c8* name;
  bool absent;
  const c8* version;
  spn_cc_driver_t driver;
  fixture_launcher_t compiler;
  fixture_launcher_t cxx;
  fixture_launcher_t linker;
  fixture_launcher_t archiver;
  fixture_host_t hosts [FIXTURE_MAX_HOSTS];
  spn_triple_t targets [FIXTURE_MAX_TARGETS];
} fixture_toolchain_t;

static bool fixture_triple_empty(spn_triple_t triple) {
  return !triple.arch && !triple.os && !triple.abi;
}

static sp_err_t fixture_check_launcher(sp_test_t* t, spn_toolchain_launcher_t launcher, fixture_launcher_t expect) {
  if (!expect.program) {
    return SP_OK;
  }

  sp_expect_str_eq_c(t, launcher.program.prefix, expect.program);
  sp_must_strs_eq(t, launcher.args, sp_da_size(launcher.args), expect.args);
  return SP_OK;
}

static sp_err_t fixture_check_host(sp_test_t* t, spn_toolchain_host_t host, fixture_host_t expect) {
  sp_expect(t, spn_triple_equal(host.triple, expect.triple));

  if (expect.url) {
    sp_expect_str_eq_c(t, host.artifact.url, expect.url);
  }
  if (expect.sha256) {
    sp_expect_str_eq_c(t, host.artifact.sha256, expect.sha256);
  }
  if (expect.mirrors) {
    sp_expect_str_eq_c(t, host.artifact.mirror_list, expect.mirrors);
  }

  return SP_OK;
}

static sp_err_t fixture_check_targets(sp_test_t* t, sp_da(spn_triple_t) targets, const spn_triple_t* expect, u32 count) {
  sp_must_eq(t, count, (u32)sp_da_size(targets));
  sp_for(it, count) {
    sp_expect(t, spn_triple_equal(expect[it], targets[it]));
  }
  return SP_OK;
}

static sp_err_t fixture_check_launchers(sp_test_t* t, spn_toolchain_launcher_t compiler, spn_toolchain_launcher_t cxx, spn_toolchain_launcher_t linker, spn_toolchain_launcher_t archiver, fixture_toolchain_t expect) {
  if (fixture_check_launcher(t, compiler, expect.compiler)) {
    return SP_ERR;
  }
  if (fixture_check_launcher(t, cxx, expect.cxx)) {
    return SP_ERR;
  }
  if (fixture_check_launcher(t, linker, expect.linker)) {
    return SP_ERR;
  }
  if (fixture_check_launcher(t, archiver, expect.archiver)) {
    return SP_ERR;
  }
  return SP_OK;
}

static sp_err_t fixture_check_declared_targets(sp_test_t* t, sp_da(spn_triple_t) targets, fixture_toolchain_t expect) {
  u32 count = 0;
  sp_carr_detect_len(expect.targets, count, !fixture_triple_empty(expect.targets[count]));
  if (!count) {
    return SP_OK;
  }
  return fixture_check_targets(t, targets, expect.targets, count);
}

static sp_err_t fixture_check_decl(sp_test_t* t, const spn_toolchain_decl_t* decl, fixture_toolchain_t expect) {
  if (expect.absent) {
    sp_expect(t, !decl);
    return SP_OK;
  }
  sp_must(t, decl);

  if (expect.version) {
    sp_expect_str_eq_c(t, decl->version, expect.version);
  }
  sp_expect_eq(t, (u32)expect.driver, (u32)decl->driver);
  if (fixture_check_launchers(t, decl->compiler, decl->cxx, decl->linker, decl->archiver, expect)) {
    return SP_ERR;
  }

  u32 hosts = 0;
  sp_carr_detect_len(expect.hosts, hosts, !fixture_triple_empty(expect.hosts[hosts].triple));
  sp_must_eq(t, hosts, (u32)sp_da_size(decl->hosts));
  sp_for(it, hosts) {
    if (fixture_check_host(t, decl->hosts[it], expect.hosts[it])) {
      return SP_ERR;
    }
  }

  return fixture_check_declared_targets(t, decl->targets, expect);
}

static sp_err_t fixture_check_entry(sp_test_t* t, spn_toolchain_info_t* info, fixture_toolchain_t expect) {
  if (expect.absent) {
    sp_expect(t, !info);
    return SP_OK;
  }
  sp_must(t, info);

  if (expect.version) {
    sp_expect_str_eq_c(t, info->version, expect.version);
  }
  sp_expect_eq(t, (u32)expect.driver, (u32)info->driver);
  if (fixture_check_launchers(t, info->compiler, info->cxx, info->linker, info->archiver, expect)) {
    return SP_ERR;
  }

  return fixture_check_declared_targets(t, info->targets, expect);
}

static sp_err_t fixture_read_json(sp_test_t* t, const c8* file, sp_str_t* json) {
  sp_mem_t mem = sp_test_arena(t);
  sp_str_t path = sp_fs_join_path(mem, sp_str_lit(TOOLCHAINS_DIR), sp_cstr_as_str(file));
  sp_must_ok(t, sp_io_read_file(mem, path, json));
  return SP_OK;
}

static const spn_toolchain_decl_t* fixture_decl(sp_da(spn_toolchain_decl_t) decls, const c8* name) {
  sp_da_for(decls, it) {
    if (sp_str_equal_cstr(decls[it].name, name)) {
      return &decls[it];
    }
  }
  return SP_NULLPTR;
}

static sp_err_t fixture_catalog(sp_test_t* t, spn_toolchain_catalog_t* catalog, const c8* file, spn_triple_t host) {
  sp_str_t json = sp_zero;
  if (fixture_read_json(t, file, &json)) {
    return SP_ERR;
  }
  spn_toolchain_catalog_init(catalog, host, sp_test_arena(t));
  sp_must_eq(t, (u32)SPN_OK, (u32)spn_toolchain_catalog_load(catalog, json));
  return SP_OK;
}

static u32 fixture_catalog_size(spn_toolchain_catalog_t* catalog) {
  return (u32)sp_str_om_size(catalog->entries);
}

static spn_toolchain_info_t* fixture_catalog_at(spn_toolchain_catalog_t* catalog, u32 index) {
  return sp_str_om_at(catalog->entries, index);
}

static spn_toolchain_decl_t fixture_local_toolchain(const c8* name, const c8* compiler) {
  return (spn_toolchain_decl_t) {
    .name = sp_cstr_as_str(name),
    .driver = SPN_CC_DRIVER_GCC,
    .compiler = { .program = spn_arg_lit(sp_cstr_as_str(compiler)) },
    .linker = { .program = spn_arg_lit(sp_cstr_as_str(compiler)) },
    .archiver = { .program = spn_arg_lit(sp_cstr_as_str("ar")) },
  };
}

#endif
