#include "sp.h"
#include "utest.h"

#include "fixture.h"

static void fixture_selection(
  s32* utest_result,
  spn_toolchain_catalog_t* catalog,
  const c8* build,
  spn_triple_t target,
  spn_triple_t host,
  spn_toolchain_selection_t* out,
  spn_toolchain_select_err_t* err
) {
  sp_mem_t mem = sp_mem_arena_as_allocator(ctx_get()->arena);
  spn_toolchain_query_t query = {
    .build = sp_str_view(build),
    .script = sp_str_lit("zig"),
    .target = target,
    .host = host,
  };
  *err = spn_toolchain_select(catalog, query, mem, out);
}

UTEST(select, system_for_host_pulls_zig_for_scripts) {
  spn_toolchain_catalog_t catalog = SP_NULLPTR;
  fixture_catalog(utest_result, &catalog, HOST_X64_LINUX);

  spn_toolchain_selection_t selection = sp_zero;
  spn_toolchain_select_err_t err = sp_zero;
  fixture_selection(utest_result, &catalog, "system", HOST_X64_LINUX, HOST_X64_LINUX, &selection, &err);

  ASSERT_EQ((u32)SPN_TOOLCHAIN_SELECT_OK, (u32)err.status);
  ASSERT_TRUE(selection.build);
  ASSERT_TRUE(selection.script);
  EXPECT_STR(selection.build->name, "system");
  EXPECT_STR(selection.script->name, "zig");
  EXPECT_EQ(2u, (u32)sp_da_size(selection.required));
}

UTEST(select, zig_as_build_toolchain_dedups) {
  spn_toolchain_catalog_t catalog = SP_NULLPTR;
  fixture_catalog(utest_result, &catalog, HOST_X64_LINUX);

  spn_toolchain_selection_t selection = sp_zero;
  spn_toolchain_select_err_t err = sp_zero;
  fixture_selection(utest_result, &catalog, "zig", HOST_X64_LINUX, HOST_X64_LINUX, &selection, &err);

  ASSERT_EQ((u32)SPN_TOOLCHAIN_SELECT_OK, (u32)err.status);
  ASSERT_EQ(1u, (u32)sp_da_size(selection.required));
  EXPECT_TRUE(selection.build == selection.script);
  EXPECT_TRUE(selection.build == selection.required[0]);
}

UTEST(select, unknown_build_toolchain) {
  spn_toolchain_catalog_t catalog = SP_NULLPTR;
  fixture_catalog(utest_result, &catalog, HOST_X64_LINUX);

  spn_toolchain_selection_t selection = sp_zero;
  spn_toolchain_select_err_t err = sp_zero;
  fixture_selection(utest_result, &catalog, "gcc-13", HOST_X64_LINUX, HOST_X64_LINUX, &selection, &err);

  ASSERT_EQ((u32)SPN_TOOLCHAIN_SELECT_ERR_UNKNOWN, (u32)err.status);
  EXPECT_STR(err.name, "gcc-13");
}

UTEST(select, undeclared_targets_are_host_only) {
  spn_toolchain_catalog_t catalog = SP_NULLPTR;
  fixture_catalog(utest_result, &catalog, HOST_X64_LINUX);

  spn_toolchain_selection_t selection = sp_zero;
  spn_toolchain_select_err_t err = sp_zero;
  fixture_selection(utest_result, &catalog, "system", TARGET_WIN_GNU, HOST_X64_LINUX, &selection, &err);

  ASSERT_EQ((u32)SPN_TOOLCHAIN_SELECT_ERR_TARGET, (u32)err.status);
  EXPECT_STR(err.name, "system");
  EXPECT_EQ((u32)SPN_OS_WINDOWS, (u32)err.target.os);
}

UTEST(select, declared_target_allows_cross) {
  spn_toolchain_catalog_t catalog = SP_NULLPTR;
  fixture_catalog(utest_result, &catalog, HOST_X64_LINUX);

  spn_toolchain_t mingw = fixture_local_toolchain("mingw", "x86_64-w64-mingw32-gcc");
  spn_triple_t targets [] = { TARGET_WIN_GNU };
  mingw.targets = SP_NULLPTR;
  sp_mem_t mem = sp_mem_arena_as_allocator(ctx_get()->arena);
  mingw.targets = sp_da_new(mem, spn_triple_t);
  sp_da_push(mingw.targets, targets[0]);
  spn_toolchain_catalog_add(&catalog, mingw);

  spn_toolchain_selection_t selection = sp_zero;
  spn_toolchain_select_err_t err = sp_zero;
  fixture_selection(utest_result, &catalog, "mingw", TARGET_WIN_GNU, HOST_X64_LINUX, &selection, &err);

  ASSERT_EQ((u32)SPN_TOOLCHAIN_SELECT_OK, (u32)err.status);
  EXPECT_STR(selection.build->name, "mingw");
  EXPECT_STR(selection.script->name, "zig");
  EXPECT_EQ(2u, (u32)sp_da_size(selection.required));
}

UTEST(select, cross_toolchain_rejects_other_targets) {
  spn_toolchain_catalog_t catalog = SP_NULLPTR;
  fixture_catalog(utest_result, &catalog, HOST_X64_LINUX);

  sp_mem_t mem = sp_mem_arena_as_allocator(ctx_get()->arena);
  spn_toolchain_t mingw = fixture_local_toolchain("mingw", "x86_64-w64-mingw32-gcc");
  mingw.targets = sp_da_new(mem, spn_triple_t);
  sp_da_push(mingw.targets, TARGET_WIN_GNU);
  spn_toolchain_catalog_add(&catalog, mingw);

  spn_toolchain_selection_t selection = sp_zero;
  spn_toolchain_select_err_t err = sp_zero;
  fixture_selection(utest_result, &catalog, "mingw", HOST_X64_LINUX, HOST_X64_LINUX, &selection, &err);

  ASSERT_EQ((u32)SPN_TOOLCHAIN_SELECT_ERR_TARGET, (u32)err.status);
  EXPECT_STR(err.name, "mingw");
}

UTEST(select, script_toolchain_must_target_wasm) {
  spn_toolchain_catalog_t catalog = SP_NULLPTR;
  fixture_catalog(utest_result, &catalog, HOST_X64_LINUX);

  spn_toolchain_catalog_add(&catalog, fixture_local_toolchain("zig", "/opt/zig/zig"));

  spn_toolchain_selection_t selection = sp_zero;
  spn_toolchain_select_err_t err = sp_zero;
  fixture_selection(utest_result, &catalog, "system", HOST_X64_LINUX, HOST_X64_LINUX, &selection, &err);

  ASSERT_EQ((u32)SPN_TOOLCHAIN_SELECT_ERR_TARGET, (u32)err.status);
  EXPECT_STR(err.name, "zig");
  EXPECT_EQ((u32)SPN_ARCH_WASM32, (u32)err.target.arch);
}

UTEST(select, supports_empty_targets_match_host_only) {
  spn_toolchain_t local = fixture_local_toolchain("system", "cc");
  EXPECT_TRUE(spn_toolchain_supports(&local, HOST_X64_LINUX, HOST_X64_LINUX));
  EXPECT_FALSE(spn_toolchain_supports(&local, TARGET_WIN_GNU, HOST_X64_LINUX));
}

UTEST(select, supports_wildcard_target_fields) {
  sp_mem_t mem = sp_mem_arena_as_allocator(ctx_get()->arena);
  spn_toolchain_t toolchain = fixture_local_toolchain("cross", "cc");
  toolchain.targets = sp_da_new(mem, spn_triple_t);
  sp_da_push(toolchain.targets, ((spn_triple_t) { .os = SPN_OS_LINUX }));

  spn_triple_t x64_musl = { SPN_ARCH_X64, SPN_OS_LINUX, SPN_ABI_MUSL };
  spn_triple_t arm_gnu = { SPN_ARCH_ARM64, SPN_OS_LINUX, SPN_ABI_GNU };
  EXPECT_TRUE(spn_toolchain_supports(&toolchain, x64_musl, HOST_X64_LINUX));
  EXPECT_TRUE(spn_toolchain_supports(&toolchain, arm_gnu, HOST_X64_LINUX));
  EXPECT_FALSE(spn_toolchain_supports(&toolchain, TARGET_WIN_GNU, HOST_X64_LINUX));
}

UTEST(select, script_target_is_wasm_wasi) {
  spn_triple_t target = spn_toolchain_script_target();
  EXPECT_EQ((u32)SPN_ARCH_WASM32, (u32)target.arch);
  EXPECT_EQ((u32)SPN_OS_WASI, (u32)target.os);
}
