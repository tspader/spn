#include "toolchain.h"

#define SDK_LIBC_TOOLCHAIN_DIR "/T"

typedef struct {
  const c8* content;
} render_expect_t;

typedef struct {
  const c8* name;
  fixture_sdk_t sdk;
  render_expect_t expect;
} render_test_t;

static const render_test_t render_tests [] = {
  {
    .name = "macos_names_the_sdk_headers",
    .sdk = { SPN_SDK_MACOS, { "/S" } },
    .expect.content =
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
    .expect.content =
      "include_dir=/X/sdk/include/ucrt\n"
      "sys_include_dir=/X/crt/include\n"
      "crt_dir=/X/sdk/lib/ucrt/x86_64\n"
      "msvc_lib_dir=/X/crt/lib/x86_64\n"
      "kernel32_lib_dir=/X/sdk/lib/um/x86_64\n"
      "gcc_dir=\n",
  },
  {
    .name = "rooted_dirs_render_absolute",
    .sdk = { SPN_SDK_MSVC, { "aa/X", SPN_PATH_ROOT_TOOLCHAIN }, SPN_ARCH_X64 },
    .expect.content =
      "include_dir=" SDK_LIBC_TOOLCHAIN_DIR "/aa/X/sdk/include/ucrt\n"
      "sys_include_dir=" SDK_LIBC_TOOLCHAIN_DIR "/aa/X/crt/include\n"
      "crt_dir=" SDK_LIBC_TOOLCHAIN_DIR "/aa/X/sdk/lib/ucrt/x86_64\n"
      "msvc_lib_dir=" SDK_LIBC_TOOLCHAIN_DIR "/aa/X/crt/lib/x86_64\n"
      "kernel32_lib_dir=" SDK_LIBC_TOOLCHAIN_DIR "/aa/X/sdk/lib/um/x86_64\n"
      "gcc_dir=\n",
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

typedef struct {
  const c8* name;
  fixture_sdk_t sdk;
} write_test_t;

static const write_test_t write_tests [] = {
  { .name = "macos", .sdk = { SPN_SDK_MACOS, { "/S" } } },
  { .name = "msvc",  .sdk = { SPN_SDK_MSVC, { "/X" }, SPN_ARCH_X64 } },
};

static spn_path_roots_t roots_with(sp_mem_t mem, spn_path_root_t root, sp_str_t dir) {
  spn_path_roots_t roots = sp_zero;
  spn_path_roots_set(&roots, mem, root, dir);
  return roots;
}

static sp_str_t render(sp_mem_t mem, const spn_path_roots_t* roots, const spn_sdk_t* sdk) {
  sp_io_dyn_mem_writer_t w = sp_zero;
  sp_io_dyn_mem_writer_init(mem, &w);
  spn_sdk_render_libc(&w.base, roots, sdk);
  return sp_io_dyn_mem_writer_as_str(&w);
}

sp_test_each(sdk_libc, render, render_test_t, render_tests) {
  sp_mem_t mem = sp_test_arena(t);
  spn_path_roots_t roots = roots_with(mem, SPN_PATH_ROOT_TOOLCHAIN, sp_str_lit(SDK_LIBC_TOOLCHAIN_DIR));
  spn_sdk_t sdk = fixture_sdk(mem, it->sdk);
  sp_expect_str_eq_c(t, render(mem, &roots, &sdk), it->expect.content);
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
  sp_expect_eq(t, it->expect.same, spn_path_equal(pa, pb));
  return SP_OK;
}

sp_test_each(sdk_libc, write, write_test_t, write_tests) {
  sp_mem_t mem = sp_test_arena(t);
  spn_path_roots_t roots = roots_with(mem, SPN_PATH_ROOT_CACHE, sp_test_dir(t));
  spn_sdk_t sdk = fixture_sdk(mem, it->sdk);
  sp_must_eq(t, (u32)SPN_OK, (u32)spn_sdk_libc_write(mem, &roots, &sdk));
  sp_str_t path = spn_path_str(&roots, mem, spn_sdk_libc_path(mem, &roots, &sdk));
  sp_str_t content = sp_zero;
  sp_must_ok(t, sp_io_read_file(mem, path, &content));
  sp_expect_str_eq(t, content, render(mem, &roots, &sdk));
  sp_expect_eq(t, (u32)SPN_OK, (u32)spn_sdk_libc_write(mem, &roots, &sdk));
  return SP_OK;
}
