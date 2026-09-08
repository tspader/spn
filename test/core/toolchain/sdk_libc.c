#include "toolchain.h"

#define SDK_LIBC_TOOLCHAIN_DIR "/T"

typedef struct {
  spn_sdk_kind_t kind;
  test_path_t root;
  const c8* content;
} expect_t;

typedef struct {
  const c8* name;
  fixture_sdk_t sdk;
  expect_t expect;
} test_t;

static const test_t tests [] = {
  {
    .name = "none_stays",
    .sdk = { SPN_SDK_NONE },
  },
  {
    .name = "sysroot_stays",
    .sdk = { SPN_SDK_SYSROOT, { "/S" } },
    .expect = { SPN_SDK_SYSROOT, { "/S" } },
  },
  {
    .name = "macos_names_the_sdk_headers",
    .sdk = { SPN_SDK_MACOS, { "/S" } },
    .expect = {
      SPN_SDK_LIBC_MACOS, { "/S" },
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
      SPN_SDK_LIBC_MSVC, .content =
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
      SPN_SDK_LIBC_MSVC, .content =
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
  spn_path_roots_set(&roots, sp_test_arena(t), SPN_PATH_ROOT_TOOLCHAIN, sp_str_lit(SDK_LIBC_TOOLCHAIN_DIR));
  return roots;
}

static spn_path_t libc_file(const spn_sdk_t* sdk) {
  switch (sdk->kind) {
    case SPN_SDK_LIBC_MACOS: return sdk->libc_macos.file;
    case SPN_SDK_LIBC_MSVC: return sdk->libc_msvc.file;
    case SPN_SDK_NONE:
    case SPN_SDK_SYSROOT:
    case SPN_SDK_MACOS:
    case SPN_SDK_MSVC: sp_unreachable_case();
  }
  sp_unreachable_return(sp_zero_struct(spn_path_t));
}

sp_test_each(sdk_libc, to_libc, test_t, tests) {
  sp_mem_t mem = sp_test_arena(t);
  spn_path_roots_t roots = test_roots(t);
  spn_sdk_t sdk = fixture_sdk(mem, it->sdk);
  sp_must_eq(t, (u32)SPN_OK, (u32)spn_sdk_to_libc(mem, &roots, &sdk));
  sp_must_eq(t, (u32)it->expect.kind, (u32)sdk.kind);
  switch (sdk.kind) {
    case SPN_SDK_NONE: {
      return SP_OK;
    }
    case SPN_SDK_SYSROOT: {
      return test_check_path(t, sdk.root, it->expect.root);
    }
    case SPN_SDK_LIBC_MACOS: {
      if (test_check_path(t, sdk.libc_macos.root, it->expect.root)) {
        return SP_ERR;
      }
      break;
    }
    case SPN_SDK_LIBC_MSVC: {
      break;
    }
    case SPN_SDK_MACOS:
    case SPN_SDK_MSVC: {
      sp_unreachable_case();
    }
  }
  spn_path_t file = libc_file(&sdk);
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
  sp_must_eq(t, (u32)SPN_OK, (u32)spn_sdk_to_libc(mem, &roots, &a));
  sp_must_eq(t, (u32)SPN_OK, (u32)spn_sdk_to_libc(mem, &roots, &b));
  sp_expect_eq(t, it->expect.same, spn_path_equal(libc_file(&a), libc_file(&b)));
  return SP_OK;
}

sp_test(sdk_libc, rewrite_is_idempotent) {
  sp_mem_t mem = sp_test_arena(t);
  spn_path_roots_t roots = test_roots(t);
  spn_sdk_t first = fixture_sdk(mem, (fixture_sdk_t) { SPN_SDK_MACOS, { "/S" } });
  spn_sdk_t second = first;
  sp_must_eq(t, (u32)SPN_OK, (u32)spn_sdk_to_libc(mem, &roots, &first));
  sp_must_eq(t, (u32)SPN_OK, (u32)spn_sdk_to_libc(mem, &roots, &second));
  sp_expect(t, spn_path_equal(libc_file(&first), libc_file(&second)));
  return SP_OK;
}
