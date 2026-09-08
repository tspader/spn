#include "toolchain.h"

typedef struct {
  spn_sdk_kind_t kind;
  bool declarable;
} expect_t;

typedef struct {
  const c8* name;
  spn_triple_t target;
  expect_t expect;
} test_t;

static const test_t tests [] = {
  { .name = "linux_gnu",    .target = HOST_X64_LINUX,      .expect = { SPN_SDK_SYSROOT, .declarable = true } },
  { .name = "linux_musl",   .target = HOST_X64_LINUX_MUSL, .expect = { SPN_SDK_SYSROOT, .declarable = true } },
  { .name = "windows_gnu",  .target = TARGET_WIN_GNU,      .expect = { SPN_SDK_SYSROOT, .declarable = true } },
  { .name = "wasi",         .target = TARGET_WASM,         .expect = { SPN_SDK_SYSROOT, .declarable = true } },
  { .name = "windows_msvc", .target = TARGET_WIN_MSVC,     .expect = { SPN_SDK_MSVC, .declarable = true } },
  { .name = "macos",        .target = HOST_ARM_MACOS,      .expect = { SPN_SDK_MACOS, .declarable = true } },
  { .name = "freestanding", .target = TARGET_X64_BARE },
  { .name = "linux_none",   .target = TARGET_X64_LINUX_NONE },
};

sp_test_each(sdk, kind, test_t, tests) {
  spn_sdk_kind_t kind = spn_sdk_kind(it->target);
  sp_expect_eq(t, (u32)it->expect.kind, (u32)kind);
  sp_expect_eq(t, it->expect.declarable, spn_sdk_declarable(kind));
  return SP_OK;
}
