#include "toolchain.h"

typedef struct {
  const c8* name;
  spn_triple_t target;
  spn_sdk_kind_t expect;
} test_t;

static const test_t tests [] = {
  { .name = "linux_gnu",    .target = HOST_X64_LINUX,      .expect = SPN_SDK_SYSROOT },
  { .name = "linux_musl",   .target = HOST_X64_LINUX_MUSL, .expect = SPN_SDK_SYSROOT },
  { .name = "windows_gnu",  .target = TARGET_WIN_GNU,      .expect = SPN_SDK_SYSROOT },
  { .name = "wasi",         .target = TARGET_WASM,         .expect = SPN_SDK_SYSROOT },
  { .name = "windows_msvc", .target = TARGET_WIN_MSVC,     .expect = SPN_SDK_MSVC },
  { .name = "macos",        .target = HOST_ARM_MACOS,      .expect = SPN_SDK_MACOS },
  { .name = "freestanding", .target = TARGET_X64_BARE },
};

sp_test_each(sdk, kind, test_t, tests) {
  sp_expect_eq(t, (u32)it->expect, (u32)spn_sdk_kind(it->target));
  return SP_OK;
}
