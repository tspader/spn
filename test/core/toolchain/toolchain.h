#ifndef SPN_TEST_TOOLCHAIN_FIXTURE_H
#define SPN_TEST_TOOLCHAIN_FIXTURE_H

#include "spn_test.h"
#include "arg.h"
#include "triples.h"
#include "hash/digest/digest.h"
#include "paths/paths.h"
#include "enum/enum.h"
#include "toolchain/toolchain.h"
#include "triple/triple.h"

#define FIXTURE_MAX_ARGS 2
#define FIXTURE_MAX_HOSTS 6
#define FIXTURE_MAX_TARGETS 16
#define FIXTURE_MAX_SDKS 2

#define SAN_GCC_LINUX   (SPN_SANITIZER_ADDRESS | SPN_SANITIZER_THREAD | SPN_SANITIZER_UNDEFINED | SPN_SANITIZER_LEAK)
#define SAN_CLANG_LINUX (SPN_SANITIZER_ADDRESS | SPN_SANITIZER_THREAD | SPN_SANITIZER_UNDEFINED | SPN_SANITIZER_MEMORY | SPN_SANITIZER_LEAK)
#define SAN_GCC_MACOS   (SPN_SANITIZER_ADDRESS | SPN_SANITIZER_THREAD | SPN_SANITIZER_UNDEFINED)
#define SAN_CLANG_MACOS (SPN_SANITIZER_ADDRESS | SPN_SANITIZER_THREAD | SPN_SANITIZER_UNDEFINED | SPN_SANITIZER_LEAK)
#define SAN_ZIG_UT      (SPN_SANITIZER_UNDEFINED | SPN_SANITIZER_THREAD)
#define SAN_ZIG_U       (SPN_SANITIZER_UNDEFINED)

typedef struct {
  const c8* name;
  const c8* path;
  spn_path_root_t root;
  const c8* args [FIXTURE_MAX_ARGS];
} fixture_launcher_t;

typedef struct {
  spn_triple_t triple;
  const c8* url;
  const c8* sha256;
  const c8* mirrors;
} fixture_host_t;

typedef struct {
  spn_triple_t triple;
  test_path_t sdk;
  spn_sanitizer_set_t sanitizers;
} fixture_target_t;

typedef struct {
  spn_sdk_kind_t kind;
  test_path_t root;
  spn_arch_t arch;
  test_path_t bin;
} fixture_sdk_t;

typedef struct {
  spn_sdk_kind_t kind;
  test_path_t root;
  test_path_t vc;
} fixture_sdk_expect_t;

typedef struct {
  spn_triple_t triple;
  fixture_sdk_expect_t sdk;
  spn_sanitizer_set_t sanitizers;
} fixture_row_t;

typedef struct {
  test_path_t root;
  spn_arch_t arch;
} fixture_msvc_t;

typedef struct {
  test_path_t macos;
  fixture_msvc_t msvc [FIXTURE_MAX_SDKS];
} fixture_sdks_t;

typedef struct {
  const c8* name;
  bool absent;
  bool host;
  const c8* version;
  spn_cc_driver_t driver;
  fixture_launcher_t compiler;
  fixture_launcher_t cxx;
  fixture_launcher_t archiver;
  bool lld;
  const c8* link_args [FIXTURE_MAX_ARGS];
  fixture_host_t hosts [FIXTURE_MAX_HOSTS];
  fixture_target_t targets [FIXTURE_MAX_TARGETS];
  fixture_row_t rows [FIXTURE_MAX_TARGETS];
} fixture_toolchain_t;

static bool fixture_triple_empty(spn_triple_t triple) {
  return !triple.arch && !triple.os && !triple.abi;
}

static bool fixture_target_empty(fixture_target_t target) {
  return fixture_triple_empty(target.triple);
}

static spn_arg_t fixture_arg(fixture_launcher_t launcher) {
  if (launcher.path) {
    return spn_arg_path((spn_path_t) { .root = launcher.root, .sub = sp_cstr_as_str(launcher.path) });
  }
  return spn_arg_lit(sp_cstr_as_str(launcher.name));
}

static spn_path_t fixture_path(test_path_t path) {
  if (!path.path) {
    return sp_zero_struct(spn_path_t);
  }
  return (spn_path_t) { .root = path.root, .sub = sp_cstr_as_str(path.path) };
}

static spn_sdk_t fixture_sdk(sp_mem_t mem, fixture_sdk_t sdk) {
  switch (sdk.kind) {
    case SPN_SDK_NONE: return sp_zero_struct(spn_sdk_t);
    case SPN_SDK_SYSROOT: return spn_sdk_sysroot(fixture_path(sdk.root));
    case SPN_SDK_MACOS: return spn_sdk_macos(mem, fixture_path(sdk.root));
    case SPN_SDK_MSVC: {
      spn_sdk_t msvc = spn_sdk_msvc(mem, fixture_path(sdk.root), sdk.arch);
      msvc.msvc.bin = fixture_path(sdk.bin);
      return msvc;
    }
  }
  sp_unreachable_return(sp_zero_struct(spn_sdk_t));
}

static spn_sdk_host_t fixture_sdks(sp_mem_t mem, fixture_sdks_t sdks) {
  spn_sdk_host_t host = { .msvc = sp_da_new(mem, spn_sdk_msvc_t) };
  if (sdks.macos.path) {
    host.macos = spn_sdk_macos(mem, fixture_path(sdks.macos)).macos;
  }
  sp_carr_for(sdks.msvc, it) {
    if (!sdks.msvc[it].root.path) {
      break;
    }
    sp_da_push(host.msvc, spn_sdk_msvc(mem, fixture_path(sdks.msvc[it].root), sdks.msvc[it].arch).msvc);
  }
  return host;
}

static sp_err_t fixture_check_sdk(sp_test_t* t, spn_sdk_t sdk, fixture_sdk_expect_t expect) {
  sp_must_eq(t, (u32)expect.kind, (u32)sdk.kind);
  switch (sdk.kind) {
    case SPN_SDK_NONE: {
      return SP_OK;
    }
    case SPN_SDK_SYSROOT: {
      return test_check_path(t, sdk.root, expect.root);
    }
    case SPN_SDK_MACOS: {
      return test_check_path(t, sdk.macos.root, expect.root);
    }
    case SPN_SDK_MSVC: {
      return test_check_path(t, sdk.msvc.lib.vc, expect.vc);
    }
  }
  sp_unreachable_return(SP_ERR);
}

static spn_toolchain_target_t fixture_target(fixture_target_t target) {
  return (spn_toolchain_target_t) { .triple = target.triple, .sdk = fixture_path(target.sdk), .sanitizers = target.sanitizers };
}

static sp_err_t fixture_check_launcher(sp_test_t* t, spn_toolchain_launcher_t launcher, fixture_launcher_t expect) {
  if (!expect.name && !expect.path) {
    return SP_OK;
  }
  if (test_check_arg(t, launcher.program, (test_arg_t) { .name = expect.name, .path = expect.path, .root = expect.root })) {
    return SP_ERR;
  }
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

static sp_err_t fixture_check_triples(sp_test_t* t, sp_da(spn_triple_t) triples, const spn_triple_t* expect, u32 count) {
  sp_must_eq(t, count, (u32)sp_da_size(triples));
  sp_for(it, count) {
    sp_expect(t, spn_triple_equal(expect[it], triples[it]));
  }
  return SP_OK;
}

static sp_err_t fixture_check_targets(sp_test_t* t, sp_da(spn_toolchain_target_t) targets, const fixture_target_t* expect, u32 count) {
  sp_must_eq(t, count, (u32)sp_da_size(targets));
  sp_for(it, count) {
    spn_toolchain_target_t want = fixture_target(expect[it]);
    sp_expect(t, spn_triple_equal(want.triple, targets[it].triple));
    sp_expect_eq(t, want.sanitizers, targets[it].sanitizers);
    if (test_check_path(t, targets[it].sdk, expect[it].sdk)) {
      return SP_ERR;
    }
  }
  return SP_OK;
}

static sp_err_t fixture_check_rows(sp_test_t* t, sp_da(spn_toolchain_row_t) rows, const fixture_row_t* expect, u32 count) {
  sp_mem_t mem = sp_test_arena(t);
  sp_must_eq(t, count, (u32)sp_da_size(rows));
  sp_for(it, count) {
    sp_expect_str_eq(t, spn_triple_to_str(mem, rows[it].triple), spn_triple_to_str(mem, expect[it].triple));
    sp_expect_eq(t, expect[it].sanitizers, rows[it].sanitizers);
    if (fixture_check_sdk(t, rows[it].sdk, expect[it].sdk)) {
      return SP_ERR;
    }
  }
  return SP_OK;
}

static sp_err_t fixture_check_expected_rows(sp_test_t* t, sp_da(spn_toolchain_row_t) rows, const fixture_row_t* expect) {
  u32 count = 0;
  while (count < FIXTURE_MAX_TARGETS && !fixture_triple_empty(expect[count].triple)) {
    count++;
  }
  return fixture_check_rows(t, rows, expect, count);
}

static sp_err_t fixture_check_link_args(sp_test_t* t, sp_da(sp_str_t) link_args, const c8* const* expect) {
  sp_must_strs_eq(t, link_args, sp_da_size(link_args), expect);
  return SP_OK;
}

static sp_err_t fixture_check_launchers(sp_test_t* t, spn_toolchain_launcher_t compiler, spn_toolchain_launcher_t cxx, spn_toolchain_launcher_t archiver, fixture_toolchain_t expect) {
  if (fixture_check_launcher(t, compiler, expect.compiler)) {
    return SP_ERR;
  }
  if (fixture_check_launcher(t, cxx, expect.cxx)) {
    return SP_ERR;
  }
  if (fixture_check_launcher(t, archiver, expect.archiver)) {
    return SP_ERR;
  }
  return SP_OK;
}

static sp_err_t fixture_check_declared_targets(sp_test_t* t, sp_da(spn_toolchain_target_t) targets, fixture_toolchain_t expect) {
  u32 count = 0;
  sp_carr_detect_len(expect.targets, count, !fixture_target_empty(expect.targets[count]));
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
  sp_expect_eq(t, expect.lld, decl->lld);
  sp_expect_eq(t, expect.host, decl->host_row);
  if (fixture_check_launchers(t, decl->compiler, decl->cxx, decl->archiver, expect)) {
    return SP_ERR;
  }
  if (fixture_check_link_args(t, decl->link_args, expect.link_args)) {
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
  sp_expect_eq(t, expect.lld, info->lld);
  if (fixture_check_launchers(t, info->compiler, info->cxx, info->archiver, expect)) {
    return SP_ERR;
  }
  if (fixture_check_link_args(t, info->link_args, expect.link_args)) {
    return SP_ERR;
  }

  return fixture_check_expected_rows(t, info->rows, expect.rows);
}

static sp_err_t fixture_read_toml(sp_test_t* t, const c8* file, sp_str_t* toml) {
  sp_mem_t mem = sp_test_arena(t);
  sp_str_t path = sp_fs_join_path(mem, sp_str_lit(TOOLCHAINS_DIR), sp_cstr_as_str(file));
  sp_must_ok(t, sp_io_read_file(mem, path, toml));
  return SP_OK;
}

static sp_err_t fixture_decls(sp_test_t* t, const c8* file, sp_da(spn_toolchain_decl_t)* decls, sp_da(spn_codegen_issue_t)* issues) {
  sp_str_t toml = sp_zero;
  if (fixture_read_toml(t, file, &toml)) {
    return SP_ERR;
  }
  spn_test_lower_toolchains(t, toml, SPN_PATH_ROOT_NONE, decls, issues);
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

static sp_err_t fixture_catalog(sp_test_t* t, spn_toolchain_catalog_t* catalog, const c8* file, spn_triple_t host, spn_sdk_host_t sdks) {
  sp_da(spn_toolchain_decl_t) decls = SP_NULLPTR;
  sp_da(spn_codegen_issue_t) issues = SP_NULLPTR;
  if (fixture_decls(t, file, &decls, &issues)) {
    return SP_ERR;
  }
  sp_must(t, sp_da_empty(issues));
  spn_toolchain_catalog_init(catalog, host, sdks, sp_test_arena(t));
  sp_da_for(decls, it) {
    spn_toolchain_catalog_add(catalog, decls[it]);
  }
  return SP_OK;
}

static u32 fixture_catalog_size(spn_toolchain_catalog_t* catalog) {
  return (u32)sp_str_om_size(catalog->entries);
}

static spn_toolchain_info_t* fixture_catalog_at(spn_toolchain_catalog_t* catalog, u32 index) {
  return sp_str_om_at(catalog->entries, index);
}

static spn_toolchain_decl_t fixture_local_toolchain(const c8* name, fixture_launcher_t compiler) {
  return (spn_toolchain_decl_t) {
    .name = sp_cstr_as_str(name),
    .driver = SPN_CC_DRIVER_GCC,
    .compiler = { .program = fixture_arg(compiler) },
    .archiver = { .program = spn_arg_lit(sp_cstr_as_str("ar")) },
    .host_row = true,
  };
}

#endif
