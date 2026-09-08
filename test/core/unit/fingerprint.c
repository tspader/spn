#include "unit.h"

typedef struct {
  spn_os_t os;
  spn_abi_t abi;
  const c8* sdk;
} side_t;

typedef struct {
  bool equal;
} expect_t;

typedef struct {
  const c8* name;
  side_t a;
  side_t b;
  expect_t expect;
} test_t;

static const test_t tests [] = {
  { .name = "same_sdk_same_fingerprint",    .a = { SPN_OS_LINUX, .sdk = "/S" },                 .b = { SPN_OS_LINUX, .sdk = "/S" },                 .expect = { .equal = true } },
  { .name = "sysroot_changes_fingerprint",  .a = { SPN_OS_LINUX, .sdk = "/S" },                 .b = { SPN_OS_LINUX, .sdk = "/T" } },
  { .name = "absent_sysroot_differs",       .a = { SPN_OS_LINUX },                              .b = { SPN_OS_LINUX, .sdk = "/S" } },
  { .name = "msvc_sdk_changes_fingerprint", .a = { SPN_OS_WINDOWS, SPN_ABI_MSVC, .sdk = "/X" }, .b = { SPN_OS_WINDOWS, SPN_ABI_MSVC, .sdk = "/Y" } },
  { .name = "macos_sdk_changes_fingerprint", .a = { SPN_OS_MACOS, .sdk = "/S" },                .b = { SPN_OS_MACOS, .sdk = "/T" } },
};

static sp_hash_t fingerprint(sp_mem_t mem, side_t side) {
  unit_graph_test_t graph = { .os = side.os, .abi = side.abi, .sdk = side.sdk, .pkgs = { { .name = "A" } } };
  spn_session_t* s = build_session(mem, &graph);
  return spn_unit_fingerprint(s, s->units.target, find_pkg_id(s, &graph, "A"));
}

sp_test_each(unit_fingerprint, sdk, test_t, tests, .setup = spn_test_ctx_setup) {
  sp_mem_t mem = sp_test_arena(t);
  sp_expect_eq(t, it->expect.equal, fingerprint(mem, it->a) == fingerprint(mem, it->b));
  return SP_OK;
}
