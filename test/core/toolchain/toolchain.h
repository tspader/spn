#ifndef SPN_TEST_TOOLCHAIN_FIXTURE_H
#define SPN_TEST_TOOLCHAIN_FIXTURE_H

#include "spn_test.h"
#include "sha256/sha256.h"
#include "paths/paths.h"
#include "toolchain/toolchain.h"

#define FIXTURE_MAX_ARGS 2
#define FIXTURE_MAX_HOSTS 2
#define FIXTURE_MAX_TARGETS 4

#define HOST_X64_LINUX  { SPN_ARCH_X64, SPN_OS_LINUX, SPN_ABI_GNU }
#define HOST_ARM_LINUX  { SPN_ARCH_ARM64, SPN_OS_LINUX, SPN_ABI_GNU }
#define HOST_ARM_MACOS  { SPN_ARCH_ARM64, SPN_OS_MACOS, SPN_ABI_NONE }
#define TARGET_WIN_GNU  { SPN_ARCH_X64, SPN_OS_WINDOWS, SPN_ABI_GNU }
#define TARGET_WASM     { SPN_ARCH_WASM32, SPN_OS_WASI, SPN_ABI_NONE }

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
  spn_toolchain_source_t source;
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

static bool fixture_triple_equal(spn_triple_t a, spn_triple_t b) {
  return a.arch == b.arch && a.os == b.os && a.abi == b.abi;
}

static bool fixture_has_target(spn_toolchain_info_t* info, spn_triple_t target) {
  sp_da_for(info->targets, it) {
    if (fixture_triple_equal(info->targets[it], target)) {
      return true;
    }
  }
  return false;
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
  sp_expect(t, fixture_triple_equal(host.triple, expect.triple));

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
  sp_expect_eq(t, (u32)expect.source, (u32)info->source);

  if (fixture_check_launcher(t, info->compiler, expect.compiler)) return SP_ERR;
  if (fixture_check_launcher(t, info->cxx, expect.cxx)) return SP_ERR;
  if (fixture_check_launcher(t, info->linker, expect.linker)) return SP_ERR;
  if (fixture_check_launcher(t, info->archiver, expect.archiver)) return SP_ERR;

  u32 hosts = 0;
  sp_carr_detect_len(expect.hosts, hosts, expect.hosts[hosts].url);

  u32 targets = 0;
  sp_carr_detect_len(expect.targets, targets, !fixture_triple_empty(expect.targets[targets]));

  sp_must_eq(t, hosts, (u32)sp_da_size(info->hosts));
  sp_must_eq(t, targets, (u32)sp_da_size(info->targets));

  sp_for(it, hosts) {
    if (fixture_check_host(t, info->hosts[it], expect.hosts[it])) return SP_ERR;
  }
  sp_for(it, targets) {
    sp_expect(t, fixture_has_target(info, expect.targets[it]));
  }

  return SP_OK;
}

static sp_err_t fixture_read_json(sp_test_t* t, const c8* file, sp_str_t* json) {
  sp_mem_t mem = sp_test_arena(t);
  sp_str_t path = sp_fs_join_path(mem, sp_str_lit(TOOLCHAINS_DIR), sp_str_view(file));
  sp_must_ok(t, sp_io_read_file(mem, path, json));
  return SP_OK;
}

static sp_err_t fixture_catalog(sp_test_t* t, spn_toolchain_catalog_t* catalog, const c8* file) {
  sp_str_t json = sp_zero;
  if (fixture_read_json(t, file, &json)) return SP_ERR;
  sp_must_eq(t, (u32)SPN_OK, (u32)spn_toolchain_catalog_init(catalog, json, sp_test_arena(t)));
  return SP_OK;
}

static u32 fixture_catalog_size(spn_toolchain_catalog_t* catalog) {
  return (u32)sp_str_om_size(catalog->entries);
}

static spn_toolchain_info_t* fixture_catalog_at(spn_toolchain_catalog_t* catalog, u32 index) {
  return sp_str_om_at(catalog->entries, index);
}

static spn_toolchain_info_t fixture_local_toolchain(const c8* name, const c8* compiler) {
  return (spn_toolchain_info_t) {
    .name = sp_str_view(name),
    .driver = SPN_CC_DRIVER_GCC,
    .compiler = { .program = spn_arg_lit(sp_str_view(compiler)) },
    .linker = { .program = spn_arg_lit(sp_str_view(compiler)) },
    .archiver = { .program = spn_arg_lit(sp_str_view("ar")) },
  };
}

#endif
