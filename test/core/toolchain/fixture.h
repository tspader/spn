#ifndef SPN_TEST_TOOLCHAIN_FIXTURE_H
#define SPN_TEST_TOOLCHAIN_FIXTURE_H

#include "sp.h"
#include "test.h"
#include "toolchain/toolchain.h"

#define EXPECT_STR(actual, cstr) EXPECT_TRUE(sp_str_equal((actual), sp_str_view(cstr)))

static const c8* FIXTURE_TOOLCHAINS_JSON =
  "{"
  "  \"toolchain\": ["
  "    {"
  "      \"name\": \"zig\","
  "      \"version\": \"0.15.2\","
  "      \"driver\": \"clang\","
  "      \"compiler\": { \"program\": \"zig\", \"args\": [\"cc\"] },"
  "      \"linker\":   { \"program\": \"zig\", \"args\": [\"cc\"] },"
  "      \"archiver\": { \"program\": \"zig\", \"args\": [\"ar\"] },"
  "      \"mirrors\": \"https://mirrors.example.com/list.txt\","
  "      \"host\": {"
  "        \"x86_64-linux\":  { \"url\": \"https://tc.example.com/zig-x86_64-linux.tar.xz\",  \"sha256\": \"aaaa\" },"
  "        \"aarch64-macos\": { \"url\": \"https://tc.example.com/zig-aarch64-macos.tar.xz\", \"sha256\": \"bbbb\" }"
  "      },"
  "      \"target\": ["
  "        { \"arch\": \"wasm32\", \"os\": \"wasi\" },"
  "        { \"arch\": \"x86_64\", \"os\": \"linux\", \"abi\": \"gnu\" },"
  "        { \"arch\": \"x86_64\", \"os\": \"linux\", \"abi\": \"musl\" },"
  "        { \"arch\": \"x86_64\", \"os\": \"windows\", \"abi\": \"gnu\" }"
  "      ]"
  "    },"
  "    {"
  "      \"name\": \"system\","
  "      \"driver\": \"gcc\","
  "      \"compiler\": { \"program\": \"cc\" },"
  "      \"linker\":   { \"program\": \"cc\" },"
  "      \"archiver\": { \"program\": \"ar\" }"
  "    }"
  "  ]"
  "}";

static const spn_triple_t HOST_X64_LINUX  = { SPN_ARCH_X64, SPN_OS_LINUX, SPN_ABI_GNU };
static const spn_triple_t HOST_ARM_MACOS  = { SPN_ARCH_ARM64, SPN_OS_MACOS, SPN_ABI_NONE };
static const spn_triple_t HOST_ARM_LINUX  = { SPN_ARCH_ARM64, SPN_OS_LINUX, SPN_ABI_GNU };
static const spn_triple_t TARGET_WIN_GNU  = { SPN_ARCH_X64, SPN_OS_WINDOWS, SPN_ABI_GNU };

static void fixture_catalog(s32* utest_result, spn_toolchain_catalog_t* catalog, spn_triple_t host) {
  sp_mem_t mem = sp_mem_arena_as_allocator(ctx_get()->arena);
  spn_err_t err = spn_toolchain_catalog_init(catalog, sp_str_view(FIXTURE_TOOLCHAINS_JSON), host, mem);
  ASSERT_EQ(SPN_OK, err);
}

static spn_toolchain_t fixture_local_toolchain(const c8* name, const c8* compiler) {
  return (spn_toolchain_t) {
    .name = sp_str_view(name),
    .driver = SPN_CC_DRIVER_GCC,
    .compiler = { .program = sp_str_view(compiler) },
    .linker = { .program = sp_str_view(compiler) },
    .archiver = { .program = sp_str_view("ar") },
  };
}

#endif
