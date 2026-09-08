#include "toolchain.h"

typedef struct {
  const c8* name;
  fixture_sdk_t sdk;
  const c8* expect;
} render_test_t;

static const render_test_t render_tests [] = {
  {
    .name = "macos_names_the_sdk_headers",
    .sdk = { SPN_SDK_MACOS, { "/S" } },
    .expect =
      "include_dir=/S/usr/include\n"
      "sys_include_dir=/S/usr/include\n"
      "crt_dir=\n"
      "msvc_lib_dir=\n"
      "kernel32_lib_dir=\n"
      "gcc_dir=\n",
  },
  {
    .name = "msvc_names_every_role",
    .sdk = { SPN_SDK_MSVC, { "/X" }, SPN_ARCH_X64 },
    .expect =
      "include_dir=/X/sdk/include/ucrt\n"
      "sys_include_dir=/X/crt/include\n"
      "crt_dir=/X/sdk/lib/ucrt/x86_64\n"
      "msvc_lib_dir=/X/crt/lib/x86_64\n"
      "kernel32_lib_dir=/X/sdk/lib/um/x86_64\n"
      "gcc_dir=\n",
  },
};

typedef struct {
  const c8* name;
  fixture_sdk_t a;
  fixture_sdk_t b;
  bool same;
} path_test_t;

static const path_test_t path_tests [] = {
  { .name = "same_content_same_file",    .a = { SPN_SDK_MACOS, { "/S" } }, .b = { SPN_SDK_MACOS, { "/S" } }, .same = true },
  { .name = "other_root_other_file",     .a = { SPN_SDK_MACOS, { "/S" } }, .b = { SPN_SDK_MACOS, { "/T" } } },
  { .name = "other_kind_other_file",     .a = { SPN_SDK_MACOS, { "/S" } }, .b = { SPN_SDK_MSVC, { "/S" }, SPN_ARCH_X64 } },
  { .name = "other_arch_other_file",     .a = { SPN_SDK_MSVC, { "/X" }, SPN_ARCH_X64 }, .b = { SPN_SDK_MSVC, { "/X" }, SPN_ARCH_ARM64 } },
};

sp_test_each(sdk_libc, render, render_test_t, render_tests) {
  sp_mem_t mem = sp_test_arena(t);
  spn_sdk_t sdk = fixture_sdk(mem, it->sdk);
  sp_io_dyn_mem_writer_t w;
  sp_io_dyn_mem_writer_init(mem, &w);
  spn_sdk_render_libc(&w.base, &spn.roots, &sdk);
  sp_expect_str_eq_c(t, sp_io_dyn_mem_writer_as_str(&w), it->expect);
  return SP_OK;
}

sp_test_each(sdk_libc, path, path_test_t, path_tests) {
  sp_mem_t mem = sp_test_arena(t);
  spn_sdk_t a = fixture_sdk(mem, it->a);
  spn_sdk_t b = fixture_sdk(mem, it->b);
  spn_path_t pa = spn_sdk_libc_path(mem, &spn.roots, &a);
  spn_path_t pb = spn_sdk_libc_path(mem, &spn.roots, &b);
  sp_expect_eq(t, (u32)SPN_PATH_ROOT_CACHE, (u32)pa.root);
  sp_expect(t, sp_str_starts_with(pa.sub, sp_str_lit("libc/")));
  sp_expect(t, sp_str_ends_with(pa.sub, sp_str_lit(".txt")));
  sp_expect_eq(t, it->same, spn_path_equal(pa, pb));
  return SP_OK;
}
