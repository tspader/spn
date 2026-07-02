#include "sp.h"
#include "utest.h"

#include "fixture.h"

UTEST(catalog, builtin_with_host_match) {
  spn_toolchain_catalog_t catalog = sp_zero;
  fixture_catalog(utest_result, &catalog, HOST_X64_LINUX);

  spn_toolchain_t* zig = spn_toolchain_catalog_get(&catalog, sp_str_lit("zig"));
  ASSERT_TRUE(zig);

  EXPECT_STR(zig->name, "zig");
  EXPECT_STR(zig->version, "0.15.2");
  EXPECT_EQ((u32)SPN_CC_DRIVER_CLANG, (u32)zig->driver);
  EXPECT_STR(zig->compiler.program, "zig");
  ASSERT_EQ(1u, (u32)sp_da_size(zig->compiler.args));
  EXPECT_STR(zig->compiler.args[0], "cc");
  ASSERT_EQ(1u, (u32)sp_da_size(zig->linker.args));
  EXPECT_STR(zig->linker.args[0], "cc");
  EXPECT_STR(zig->archiver.args[0], "ar");

  ASSERT_FALSE(sp_opt_is_null(zig->artifact));
  spn_artifact_t artifact = sp_opt_get(zig->artifact);
  EXPECT_STR(artifact.url, "https://tc.example.com/zig-x86_64-linux.tar.xz");
  EXPECT_STR(artifact.sha256, "aaaa");
  EXPECT_STR(artifact.mirror_list, "https://mirrors.example.com/list.txt");

  ASSERT_EQ(4u, (u32)sp_da_size(zig->targets));
  EXPECT_EQ((u32)SPN_ARCH_WASM32, (u32)zig->targets[0].arch);
  EXPECT_EQ((u32)SPN_OS_WASI, (u32)zig->targets[0].os);
}

UTEST(catalog, host_selects_matching_artifact) {
  spn_toolchain_catalog_t catalog = sp_zero;
  fixture_catalog(utest_result, &catalog, HOST_ARM_MACOS);

  spn_toolchain_t* zig = spn_toolchain_catalog_get(&catalog, sp_str_lit("zig"));
  ASSERT_TRUE(zig);
  ASSERT_FALSE(sp_opt_is_null(zig->artifact));
  EXPECT_STR(sp_opt_get(zig->artifact).url, "https://tc.example.com/zig-aarch64-macos.tar.xz");
  EXPECT_STR(sp_opt_get(zig->artifact).sha256, "bbbb");
}

UTEST(catalog, unmatched_host_has_no_artifact) {
  spn_toolchain_catalog_t catalog = sp_zero;
  fixture_catalog(utest_result, &catalog, HOST_ARM_LINUX);

  spn_toolchain_t* zig = spn_toolchain_catalog_get(&catalog, sp_str_lit("zig"));
  ASSERT_TRUE(zig);
  EXPECT_TRUE(sp_opt_is_null(zig->artifact));
}

UTEST(catalog, system_entry_is_local) {
  spn_toolchain_catalog_t catalog = sp_zero;
  fixture_catalog(utest_result, &catalog, HOST_X64_LINUX);

  spn_toolchain_t* system = spn_toolchain_catalog_get(&catalog, sp_str_lit("system"));
  ASSERT_TRUE(system);
  EXPECT_TRUE(sp_opt_is_null(system->artifact));
  EXPECT_EQ((u32)SPN_CC_DRIVER_GCC, (u32)system->driver);
  EXPECT_STR(system->compiler.program, "cc");
  EXPECT_EQ(0u, (u32)sp_da_size(system->targets));
}

UTEST(catalog, add_overrides_builtin) {
  spn_toolchain_catalog_t catalog = sp_zero;
  fixture_catalog(utest_result, &catalog, HOST_X64_LINUX);

  spn_toolchain_catalog_add(&catalog, fixture_local_toolchain("zig", "/opt/zig/zig"));

  spn_toolchain_t* zig = spn_toolchain_catalog_get(&catalog, sp_str_lit("zig"));
  ASSERT_TRUE(zig);
  EXPECT_STR(zig->compiler.program, "/opt/zig/zig");
  EXPECT_TRUE(sp_opt_is_null(zig->artifact));
}

UTEST(catalog, add_new_entry) {
  spn_toolchain_catalog_t catalog = sp_zero;
  fixture_catalog(utest_result, &catalog, HOST_X64_LINUX);

  spn_toolchain_catalog_add(&catalog, fixture_local_toolchain("mingw", "x86_64-w64-mingw32-gcc"));

  spn_toolchain_t* mingw = spn_toolchain_catalog_get(&catalog, sp_str_lit("mingw"));
  ASSERT_TRUE(mingw);
  EXPECT_STR(mingw->compiler.program, "x86_64-w64-mingw32-gcc");

  EXPECT_TRUE(spn_toolchain_catalog_get(&catalog, sp_str_lit("zig")));
  EXPECT_TRUE(spn_toolchain_catalog_get(&catalog, sp_str_lit("system")));
}

UTEST(catalog, unknown_name_is_null) {
  spn_toolchain_catalog_t catalog = sp_zero;
  fixture_catalog(utest_result, &catalog, HOST_X64_LINUX);

  EXPECT_FALSE(spn_toolchain_catalog_get(&catalog, sp_str_lit("gcc-13")));
}

UTEST(catalog, embedded_builtins_parse) {
  sp_mem_t mem = sp_mem_arena_as_allocator(ctx_get()->arena);
  ctx_paths_t paths = ctx_get_paths(ctx_get());
  sp_str_t path = sp_fs_join_path(mem, paths.repo, sp_str_lit("source/toolchain/toolchains.json"));

  sp_str_t json = sp_zero;
  ASSERT_EQ(0, (s32)sp_io_read_file(mem, path, &json));

  spn_toolchain_catalog_t catalog = sp_zero;
  ASSERT_EQ(SPN_OK, spn_toolchain_catalog_init(&catalog, json, HOST_X64_LINUX, mem));

  spn_toolchain_t* zig = spn_toolchain_catalog_get(&catalog, sp_str_lit("zig"));
  ASSERT_TRUE(zig);
  ASSERT_FALSE(sp_opt_is_null(zig->artifact));
  EXPECT_TRUE(sp_str_contains(sp_opt_get(zig->artifact).url, sp_str_lit("ziglang.org")));
  EXPECT_EQ(64u, sp_opt_get(zig->artifact).sha256.len);

  bool targets_wasm = false;
  sp_da_for(zig->targets, it) {
    if (zig->targets[it].arch == SPN_ARCH_WASM32 && zig->targets[it].os == SPN_OS_WASI) targets_wasm = true;
  }
  EXPECT_TRUE(targets_wasm);

  spn_toolchain_t* system = spn_toolchain_catalog_get(&catalog, sp_str_lit("system"));
  ASSERT_TRUE(system);
  EXPECT_TRUE(sp_opt_is_null(system->artifact));
}

UTEST(catalog, malformed_json_errors) {
  sp_mem_t mem = sp_mem_arena_as_allocator(ctx_get()->arena);
  spn_toolchain_catalog_t catalog = sp_zero;
  EXPECT_EQ(SPN_ERROR, spn_toolchain_catalog_init(&catalog, sp_str_lit("{ not json"), HOST_X64_LINUX, mem));
}
