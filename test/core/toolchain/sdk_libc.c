#include "toolchain.h"

#define SDK_LIBC_TOOLCHAIN_DIR "/T"

typedef struct {
  const c8* content;
} expect_t;

typedef struct {
  const c8* name;
  fixture_sdk_t sdk;
  expect_t expect;
} test_t;

static const test_t tests [] = {
  {
    .name = "none_has_no_file",
    .sdk = { SPN_SDK_NONE },
  },
  {
    .name = "sysroot_has_no_file",
    .sdk = { SPN_SDK_SYSROOT, { "/S" } },
  },
  {
    .name = "macos_names_the_sdk_headers",
    .sdk = { SPN_SDK_MACOS, { "/S" } },
    .expect = {
      "include_dir=/S/usr/include\n"
      "sys_include_dir=/S/usr/include\n"
      "crt_dir=\n"
      "msvc_lib_dir=\n"
      "kernel32_lib_dir=\n"
      "gcc_dir=\n",
    },
  },
  {
    .name = "msvc_names_every_role",
    .sdk = { SPN_SDK_MSVC, { "/X" }, SPN_ARCH_X64 },
    .expect = {
      "include_dir=/X/sdk/include/ucrt\n"
      "sys_include_dir=/X/crt/include\n"
      "crt_dir=/X/sdk/lib/ucrt/x86_64\n"
      "msvc_lib_dir=/X/crt/lib/x86_64\n"
      "kernel32_lib_dir=/X/sdk/lib/um/x86_64\n"
      "gcc_dir=\n",
    },
  },
  {
    .name = "rooted_dirs_render_absolute",
    .sdk = { SPN_SDK_MSVC, { "aa/X", SPN_PATH_ROOT_TOOLCHAIN }, SPN_ARCH_X64 },
    .expect = {
      "include_dir=" SDK_LIBC_TOOLCHAIN_DIR "/aa/X/sdk/include/ucrt\n"
      "sys_include_dir=" SDK_LIBC_TOOLCHAIN_DIR "/aa/X/crt/include\n"
      "crt_dir=" SDK_LIBC_TOOLCHAIN_DIR "/aa/X/sdk/lib/ucrt/x86_64\n"
      "msvc_lib_dir=" SDK_LIBC_TOOLCHAIN_DIR "/aa/X/crt/lib/x86_64\n"
      "kernel32_lib_dir=" SDK_LIBC_TOOLCHAIN_DIR "/aa/X/sdk/lib/um/x86_64\n"
      "gcc_dir=\n",
    },
  },
};

typedef struct {
  bool same;
} path_expect_t;

typedef struct {
  const c8* name;
  fixture_sdk_t a;
  fixture_sdk_t b;
  path_expect_t expect;
} path_test_t;

static const path_test_t path_tests [] = {
  { .name = "same_content_same_file",   .a = { SPN_SDK_MACOS, { "/S" } }, .b = { SPN_SDK_MACOS, { "/S" } }, .expect = { .same = true } },
  { .name = "other_content_other_file", .a = { SPN_SDK_MACOS, { "/S" } }, .b = { SPN_SDK_MACOS, { "/T" } } },
};

static spn_path_roots_t test_roots(sp_test_t* t) {
  spn_path_roots_t roots = sp_zero;
  spn_path_roots_set(&roots, sp_test_arena(t), SPN_PATH_ROOT_CACHE, sp_test_dir(t));
  roots.dirs[SPN_PATH_ROOT_TOOLCHAIN] = sp_str_lit(SDK_LIBC_TOOLCHAIN_DIR);
  return roots;
}

sp_test_each(sdk_libc, write, test_t, tests) {
  sp_mem_t mem = sp_test_arena(t);
  spn_path_roots_t roots = test_roots(t);
  spn_sdk_t sdk = fixture_sdk(mem, it->sdk);
  spn_path_t file = sp_zero;
  sp_must_eq(t, (u32)SPN_OK, (u32)spn_libc_write(mem, &roots, &sdk, &file));
  if (!it->expect.content) {
    sp_expect(t, spn_path_empty(file));
    return SP_OK;
  }
  sp_expect_eq(t, (u32)SPN_PATH_ROOT_CACHE, (u32)file.root);
  sp_str_t content = sp_zero;
  sp_must_ok(t, sp_io_read_file(mem, spn_path_str(&roots, mem, file), &content));
  sp_expect_str_eq_c(t, content, it->expect.content);
  return SP_OK;
}

sp_test_each(sdk_libc, path, path_test_t, path_tests) {
  sp_mem_t mem = sp_test_arena(t);
  spn_path_roots_t roots = test_roots(t);
  spn_sdk_t a = fixture_sdk(mem, it->a);
  spn_sdk_t b = fixture_sdk(mem, it->b);
  spn_path_t file_a = sp_zero;
  spn_path_t file_b = sp_zero;
  sp_must_eq(t, (u32)SPN_OK, (u32)spn_libc_write(mem, &roots, &a, &file_a));
  sp_must_eq(t, (u32)SPN_OK, (u32)spn_libc_write(mem, &roots, &b, &file_b));
  sp_expect_eq(t, it->expect.same, spn_path_equal(file_a, file_b));
  return SP_OK;
}

sp_test(sdk_libc, rewrite_is_idempotent) {
  sp_mem_t mem = sp_test_arena(t);
  spn_path_roots_t roots = test_roots(t);
  spn_sdk_t sdk = fixture_sdk(mem, (fixture_sdk_t) { SPN_SDK_MACOS, { "/S" } });
  spn_path_t first = sp_zero;
  spn_path_t second = sp_zero;
  sp_must_eq(t, (u32)SPN_OK, (u32)spn_libc_write(mem, &roots, &sdk, &first));
  sp_must_eq(t, (u32)SPN_OK, (u32)spn_libc_write(mem, &roots, &sdk, &second));
  sp_expect(t, spn_path_equal(first, second));
  return SP_OK;
}
