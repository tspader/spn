#include "spn_test.h"

#include "ctx/types.h"
#include "intern/intern.h"
#include "profile/profile.h"
#include "pkg/types.h"
#include "target/types.h"
#include "toolchain/toolchain.h"
#include "triple/triple.h"
#include "when/when.h"

sp_test_suite(profile, .serial = true);

#define PROFILE_MAX_ABIS 3

#define PROFILE_HOST_LINUX_GNU  { SPN_ARCH_X64, SPN_OS_LINUX, SPN_ABI_GNU }
#define PROFILE_HOST_LINUX_MUSL { SPN_ARCH_X64, SPN_OS_LINUX, SPN_ABI_MUSL }
#define PROFILE_HOST_WIN_MSVC   { SPN_ARCH_X64, SPN_OS_WINDOWS, SPN_ABI_MSVC }
#define PROFILE_HOST_WIN_GNU    { SPN_ARCH_X64, SPN_OS_WINDOWS, SPN_ABI_GNU }
#define PROFILE_HOST_ARM_MACOS  { SPN_ARCH_ARM64, SPN_OS_MACOS, SPN_ABI_APPLE }

typedef struct {
  const c8* key;
  const c8* value;
} clause_t;

typedef struct {
  const c8* value;
  clause_t when [2];
} candidate_t;

typedef struct {
  const c8* name;
  spn_os_t os;
  spn_arch_t arch;
  candidate_t toolchain [2];
  candidate_t abi [2];
  candidate_t linkage [2];
  candidate_t standard [2];
  candidate_t mode [2];
  candidate_t opt [2];
} decl_t;

typedef struct {
  const c8* name;
  const c8* toolchain;
  spn_mode_t mode;
  spn_os_t os;
  spn_arch_t arch;
  spn_abi_t abi;
} override_t;

typedef struct {
  spn_err_t err;
  const c8* name;
  spn_triple_t target;
  spn_linkage_t linkage;
  const c8* toolchain;
  spn_c_standard_t standard;
  spn_mode_t mode;
  spn_opt_level_t opt;
  bool targeted;
} expect_t;

typedef struct {
  const c8* name;
  decl_t profile;
  decl_t derived;
  override_t overrides;
  spn_triple_t host;
  bool shared_demand;
  spn_abi_t abi;
  expect_t expect;
} test_t;

static const test_t tests [] = {
  {
    .name = "default_pins_to_host",
    .host = PROFILE_HOST_LINUX_GNU,
    .abi = SPN_ABI_MUSL,
    .expect = {
      .name = "debug",
      .target = { SPN_ARCH_X64, SPN_OS_LINUX },
      .linkage = SPN_LIB_KIND_STATIC,
      .toolchain = "auto",
      .mode = SPN_MODE_DEBUG,
    },
  },
  {
    .name = "release_by_mode",
    .overrides = { .mode = SPN_MODE_RELEASE },
    .host = PROFILE_HOST_LINUX_GNU,
    .abi = SPN_ABI_MUSL,
    .expect = {
      .name = "release",
      .target = { SPN_ARCH_X64, SPN_OS_LINUX },
      .linkage = SPN_LIB_KIND_STATIC,
      .mode = SPN_MODE_RELEASE,
    },
  },
  {
    .name = "release_by_name",
    .overrides = { .name = "release" },
    .host = PROFILE_HOST_LINUX_GNU,
    .abi = SPN_ABI_MUSL,
    .expect = {
      .name = "release",
      .target = { SPN_ARCH_X64, SPN_OS_LINUX },
      .linkage = SPN_LIB_KIND_STATIC,
      .mode = SPN_MODE_RELEASE,
    },
  },
  {
    .name = "user_release_overlays_builtin",
    .derived = { .name = "release", .toolchain = { { "gcc" } } },
    .overrides = { .mode = SPN_MODE_RELEASE },
    .host = PROFILE_HOST_LINUX_GNU,
    .abi = SPN_ABI_MUSL,
    .expect = {
      .name = "release",
      .target = { SPN_ARCH_X64, SPN_OS_LINUX },
      .linkage = SPN_LIB_KIND_STATIC,
      .toolchain = "gcc",
      .mode = SPN_MODE_RELEASE,
    },
  },
  {
    .name = "default_by_name",
    .profile = { .name = "default", .mode = { { "release" } } },
    .overrides = { .name = "default" },
    .host = PROFILE_HOST_LINUX_GNU,
    .abi = SPN_ABI_MUSL,
    .expect = {
      .name = "default",
      .target = { SPN_ARCH_X64, SPN_OS_LINUX },
      .linkage = SPN_LIB_KIND_STATIC,
      .mode = SPN_MODE_RELEASE,
    },
  },
  {
    .name = "builtin_mode_beats_default_mode",
    .profile = { .name = "default", .mode = { { "release" } } },
    .host = PROFILE_HOST_LINUX_GNU,
    .abi = SPN_ABI_MUSL,
    .expect = {
      .name = "debug",
      .target = { SPN_ARCH_X64, SPN_OS_LINUX },
      .linkage = SPN_LIB_KIND_STATIC,
      .mode = SPN_MODE_DEBUG,
    },
  },
  {
    .name = "explicit_shared_is_kept",
    .profile = { .name = "default", .linkage = { { "shared" } } },
    .host = PROFILE_HOST_LINUX_GNU,
    .abi = SPN_ABI_GNU,
    .expect = {
      .target = { SPN_ARCH_X64, SPN_OS_LINUX },
      .linkage = SPN_LIB_KIND_SHARED,
    },
  },
  {
    .name = "explicit_static_is_kept",
    .profile = { .name = "default", .linkage = { { "static" } } },
    .host = PROFILE_HOST_LINUX_GNU,
    .abi = SPN_ABI_MUSL,
    .expect = {
      .target = { SPN_ARCH_X64, SPN_OS_LINUX },
      .linkage = SPN_LIB_KIND_STATIC,
    },
  },
  {
    .name = "shared_demand_defaults_to_shared",
    .host = PROFILE_HOST_LINUX_GNU,
    .shared_demand = true,
    .abi = SPN_ABI_GNU,
    .expect = {
      .target = { SPN_ARCH_X64, SPN_OS_LINUX },
      .linkage = SPN_LIB_KIND_SHARED,
    },
  },
  {
    .name = "explicit_static_ignores_shared_demand",
    .profile = { .name = "default", .linkage = { { "static" } } },
    .host = PROFILE_HOST_LINUX_GNU,
    .shared_demand = true,
    .abi = SPN_ABI_MUSL,
    .expect = {
      .target = { SPN_ARCH_X64, SPN_OS_LINUX },
      .linkage = SPN_LIB_KIND_STATIC,
    },
  },
  {
    .name = "shared_demand_survives_pinned_abi",
    .overrides = { .abi = SPN_ABI_MUSL },
    .host = PROFILE_HOST_LINUX_GNU,
    .shared_demand = true,
    .expect = {
      .target = { SPN_ARCH_X64, SPN_OS_LINUX, SPN_ABI_MUSL },
      .linkage = SPN_LIB_KIND_SHARED,
      .targeted = true,
    },
  },
  {
    .name = "gnu_defaults_to_shared",
    .overrides = { .abi = SPN_ABI_GNU },
    .host = PROFILE_HOST_LINUX_GNU,
    .expect = {
      .target = { SPN_ARCH_X64, SPN_OS_LINUX, SPN_ABI_GNU },
      .linkage = SPN_LIB_KIND_SHARED,
      .targeted = true,
    },
  },
  {
    .name = "musl_defaults_to_static",
    .overrides = { .abi = SPN_ABI_MUSL },
    .host = PROFILE_HOST_LINUX_GNU,
    .expect = {
      .target = { SPN_ARCH_X64, SPN_OS_LINUX, SPN_ABI_MUSL },
      .linkage = SPN_LIB_KIND_STATIC,
      .targeted = true,
    },
  },
  {
    .name = "explicit_musl_shared_is_honored",
    .profile = { .name = "default", .linkage = { { "shared" } } },
    .overrides = { .abi = SPN_ABI_MUSL },
    .host = PROFILE_HOST_LINUX_GNU,
    .expect = {
      .target = { SPN_ARCH_X64, SPN_OS_LINUX, SPN_ABI_MUSL },
      .linkage = SPN_LIB_KIND_SHARED,
      .targeted = true,
    },
  },
  {
    .name = "explicit_gnu_static_is_honored",
    .profile = { .name = "default", .linkage = { { "static" } } },
    .overrides = { .abi = SPN_ABI_GNU },
    .host = PROFILE_HOST_LINUX_GNU,
    .expect = {
      .target = { SPN_ARCH_X64, SPN_OS_LINUX, SPN_ABI_GNU },
      .linkage = SPN_LIB_KIND_STATIC,
      .targeted = true,
    },
  },
  {
    .name = "freestanding_elf_defaults_to_static",
    .overrides = { .arch = SPN_ARCH_ARM64, .os = SPN_OS_FREESTANDING, .abi = SPN_ABI_ELF },
    .host = PROFILE_HOST_LINUX_GNU,
    .shared_demand = true,
    .expect = {
      .target = { SPN_ARCH_ARM64, SPN_OS_FREESTANDING, SPN_ABI_ELF },
      .linkage = SPN_LIB_KIND_STATIC,
      .targeted = true,
    },
  },
  {
    .name = "freestanding_without_abi_stays_incomplete",
    .overrides = { .arch = SPN_ARCH_ARM64, .os = SPN_OS_FREESTANDING },
    .host = PROFILE_HOST_LINUX_GNU,
    .expect = {
      .target = { SPN_ARCH_ARM64, SPN_OS_FREESTANDING },
      .linkage = SPN_LIB_KIND_STATIC,
      .targeted = true,
    },
  },
  {
    .name = "none_defaults_to_static",
    .overrides = { .abi = SPN_ABI_BARE },
    .host = PROFILE_HOST_LINUX_GNU,
    .shared_demand = true,
    .expect = {
      .target = { SPN_ARCH_X64, SPN_OS_LINUX, SPN_ABI_BARE },
      .linkage = SPN_LIB_KIND_STATIC,
      .targeted = true,
    },
  },
  {
    .name = "wasi_defaults_to_static",
    .overrides = { .arch = SPN_ARCH_WASM32, .os = SPN_OS_WASI },
    .host = PROFILE_HOST_LINUX_GNU,
    .shared_demand = true,
    .expect = {
      .target = { SPN_ARCH_WASM32, SPN_OS_WASI, SPN_ABI_MUSL },
      .linkage = SPN_LIB_KIND_STATIC,
      .targeted = true,
    },
  },
  {
    .name = "override_os_keeps_host_arch",
    .overrides = { .os = SPN_OS_WINDOWS, .abi = SPN_ABI_GNU },
    .host = PROFILE_HOST_LINUX_GNU,
    .expect = {
      .target = { SPN_ARCH_X64, SPN_OS_WINDOWS, SPN_ABI_GNU },
      .linkage = SPN_LIB_KIND_SHARED,
      .targeted = true,
    },
  },
  {
    .name = "override_arch_keeps_host_os",
    .overrides = { .arch = SPN_ARCH_ARM64, .abi = SPN_ABI_MUSL },
    .host = PROFILE_HOST_LINUX_GNU,
    .expect = {
      .target = { SPN_ARCH_ARM64, SPN_OS_LINUX, SPN_ABI_MUSL },
      .linkage = SPN_LIB_KIND_STATIC,
      .targeted = true,
    },
  },
  {
    .name = "override_os_pins_foreign_host_arch",
    .overrides = { .os = SPN_OS_LINUX, .abi = SPN_ABI_MUSL },
    .host = PROFILE_HOST_ARM_MACOS,
    .expect = {
      .target = { SPN_ARCH_ARM64, SPN_OS_LINUX, SPN_ABI_MUSL },
      .linkage = SPN_LIB_KIND_STATIC,
      .targeted = true,
    },
  },
  {
    .name = "manifest_toolchain_applies",
    .profile = { .name = "default", .toolchain = { { "gcc" } } },
    .host = PROFILE_HOST_LINUX_GNU,
    .abi = SPN_ABI_GNU,
    .expect = {
      .target = { SPN_ARCH_X64, SPN_OS_LINUX },
      .linkage = SPN_LIB_KIND_SHARED,
      .toolchain = "gcc",
    },
  },
  {
    .name = "override_toolchain_wins",
    .profile = { .name = "default", .toolchain = { { "gcc" } } },
    .overrides = { .toolchain = "clang" },
    .host = PROFILE_HOST_LINUX_GNU,
    .abi = SPN_ABI_GNU,
    .expect = {
      .target = { SPN_ARCH_X64, SPN_OS_LINUX },
      .linkage = SPN_LIB_KIND_SHARED,
      .toolchain = "clang",
    },
  },
  {
    .name = "manifest_abi_applies_to_host",
    .profile = { .name = "default", .abi = { { "gnu" } } },
    .host = PROFILE_HOST_LINUX_GNU,
    .expect = {
      .target = { SPN_ARCH_X64, SPN_OS_LINUX, SPN_ABI_GNU },
      .linkage = SPN_LIB_KIND_SHARED,
      .targeted = true,
    },
  },
  {
    .name = "os_override_drops_manifest_abi",
    .profile = { .name = "default", .abi = { { "gnu" } } },
    .overrides = { .arch = SPN_ARCH_ARM64, .os = SPN_OS_MACOS },
    .host = PROFILE_HOST_LINUX_GNU,
    .expect = {
      .target = { SPN_ARCH_ARM64, SPN_OS_MACOS, SPN_ABI_APPLE },
      .linkage = SPN_LIB_KIND_SHARED,
      .targeted = true,
    },
  },
  {
    .name = "arch_override_keeps_manifest_abi",
    .profile = { .name = "default", .abi = { { "gnu" } } },
    .overrides = { .arch = SPN_ARCH_ARM64 },
    .host = PROFILE_HOST_LINUX_GNU,
    .expect = {
      .target = { SPN_ARCH_ARM64, SPN_OS_LINUX, SPN_ABI_GNU },
      .linkage = SPN_LIB_KIND_SHARED,
      .targeted = true,
    },
  },
  {
    .name = "override_os_with_abi_is_honored",
    .profile = { .name = "default", .abi = { { "musl" } } },
    .overrides = { .os = SPN_OS_WINDOWS, .abi = SPN_ABI_MSVC },
    .host = PROFILE_HOST_WIN_GNU,
    .expect = {
      .target = { SPN_ARCH_X64, SPN_OS_WINDOWS, SPN_ABI_MSVC },
      .linkage = SPN_LIB_KIND_SHARED,
      .targeted = true,
    },
  },
  {
    .name = "derived_os_drops_base_abi",
    .profile = { .name = "default", .abi = { { "gnu" } } },
    .derived = { .name = "mac", .os = SPN_OS_MACOS },
    .overrides = { .name = "mac" },
    .host = PROFILE_HOST_LINUX_GNU,
    .expect = {
      .name = "mac",
      .target = { SPN_ARCH_X64, SPN_OS_MACOS, SPN_ABI_APPLE },
      .linkage = SPN_LIB_KIND_SHARED,
      .targeted = true,
    },
  },
  {
    .name = "derived_abi_overlays_base_os",
    .profile = { .name = "default", .os = SPN_OS_WINDOWS },
    .derived = { .name = "msvc", .abi = { { "msvc" } } },
    .overrides = { .name = "msvc" },
    .host = PROFILE_HOST_WIN_GNU,
    .expect = {
      .target = { SPN_ARCH_X64, SPN_OS_WINDOWS, SPN_ABI_MSVC },
      .linkage = SPN_LIB_KIND_SHARED,
      .targeted = true,
    },
  },
  {
    .name = "gated_matches_host_os",
    .profile = { .name = "default", .abi = { { "gnu", { { "os", "linux" } } } } },
    .host = PROFILE_HOST_LINUX_GNU,
    .expect = {
      .target = { SPN_ARCH_X64, SPN_OS_LINUX, SPN_ABI_GNU },
      .linkage = SPN_LIB_KIND_SHARED,
      .targeted = true,
    },
  },
  {
    .name = "gated_skips_other_os",
    .profile = { .name = "default", .abi = { { "gnu", { { "os", "macos" } } } } },
    .host = PROFILE_HOST_LINUX_GNU,
    .abi = SPN_ABI_MUSL,
    .expect = {
      .target = { SPN_ARCH_X64, SPN_OS_LINUX },
      .linkage = SPN_LIB_KIND_STATIC,
    },
  },
  {
    .name = "gated_reads_override_os",
    .profile = { .name = "default", .toolchain = { { "clang", { { "os", "macos" } } } } },
    .overrides = { .os = SPN_OS_MACOS },
    .host = PROFILE_HOST_LINUX_GNU,
    .expect = {
      .target = { SPN_ARCH_X64, SPN_OS_MACOS, SPN_ABI_APPLE },
      .linkage = SPN_LIB_KIND_SHARED,
      .toolchain = "clang",
      .targeted = true,
    },
  },
  {
    .name = "gated_reads_override_arch",
    .profile = { .name = "default", .toolchain = { { "gcc", { { "arch", "aarch64" } } } } },
    .overrides = { .arch = SPN_ARCH_ARM64, .abi = SPN_ABI_MUSL },
    .host = PROFILE_HOST_LINUX_GNU,
    .expect = {
      .target = { SPN_ARCH_ARM64, SPN_OS_LINUX, SPN_ABI_MUSL },
      .linkage = SPN_LIB_KIND_STATIC,
      .toolchain = "gcc",
      .targeted = true,
    },
  },
  {
    .name = "gated_reads_default_pinned_os",
    .profile = { .name = "default", .os = SPN_OS_WINDOWS, .abi = { { "msvc", { { "os", "windows" } } } } },
    .host = PROFILE_HOST_LINUX_GNU,
    .expect = {
      .target = { SPN_ARCH_X64, SPN_OS_WINDOWS, SPN_ABI_MSVC },
      .linkage = SPN_LIB_KIND_SHARED,
      .targeted = true,
    },
  },
  {
    .name = "gated_prefers_derived_pin_over_default_pin",
    .profile = { .name = "default", .os = SPN_OS_WINDOWS, .toolchain = { { "clang", { { "os", "macos" } } } } },
    .derived = { .name = "mac", .os = SPN_OS_MACOS },
    .overrides = { .name = "mac" },
    .host = PROFILE_HOST_LINUX_GNU,
    .expect = {
      .target = { SPN_ARCH_X64, SPN_OS_MACOS, SPN_ABI_APPLE },
      .linkage = SPN_LIB_KIND_SHARED,
      .toolchain = "clang",
      .targeted = true,
    },
  },
  {
    .name = "gated_prefers_override_over_pinned_os",
    .profile = { .name = "default", .os = SPN_OS_WINDOWS, .toolchain = { { "clang", { { "os", "macos" } } } } },
    .overrides = { .os = SPN_OS_MACOS },
    .host = PROFILE_HOST_LINUX_GNU,
    .expect = {
      .target = { SPN_ARCH_X64, SPN_OS_MACOS, SPN_ABI_APPLE },
      .linkage = SPN_LIB_KIND_SHARED,
      .toolchain = "clang",
      .targeted = true,
    },
  },
  {
    .name = "gated_first_match_wins",
    .profile = { .name = "default", .abi = { { "musl", { { "os", "linux" } } }, { "gnu", { { "arch", "x86_64" } } } } },
    .host = PROFILE_HOST_LINUX_GNU,
    .expect = {
      .target = { SPN_ARCH_X64, SPN_OS_LINUX, SPN_ABI_MUSL },
      .linkage = SPN_LIB_KIND_STATIC,
      .targeted = true,
    },
  },
  {
    .name = "gated_falls_through_to_unconditional",
    .profile = { .name = "default", .abi = { { "msvc", { { "os", "windows" } } }, { "gnu" } } },
    .host = PROFILE_HOST_LINUX_GNU,
    .expect = {
      .target = { SPN_ARCH_X64, SPN_OS_LINUX, SPN_ABI_GNU },
      .linkage = SPN_LIB_KIND_SHARED,
      .targeted = true,
    },
  },
  {
    .name = "derived_gate_beats_default_gate",
    .profile = { .name = "default", .abi = { { "gnu", { { "os", "linux" } } } } },
    .derived = { .name = "fast", .abi = { { "musl", { { "os", "linux" } } } } },
    .overrides = { .name = "fast" },
    .host = PROFILE_HOST_LINUX_GNU,
    .expect = {
      .name = "fast",
      .target = { SPN_ARCH_X64, SPN_OS_LINUX, SPN_ABI_MUSL },
      .linkage = SPN_LIB_KIND_STATIC,
      .targeted = true,
    },
  },
  {
    .name = "every_gated_field_flows",
    .derived = {
      .name = "fast",
      .toolchain = { { "gcc", { { "os", "linux" } } } },
      .abi = { { "gnu", { { "os", "linux" } } } },
      .linkage = { { "static", { { "os", "linux" } } } },
      .standard = { { "c99", { { "os", "linux" } } } },
      .mode = { { "release", { { "os", "linux" } } } },
      .opt = { { "3", { { "os", "linux" } } } },
    },
    .overrides = { .name = "fast" },
    .host = PROFILE_HOST_LINUX_GNU,
    .expect = {
      .name = "fast",
      .target = { SPN_ARCH_X64, SPN_OS_LINUX, SPN_ABI_GNU },
      .linkage = SPN_LIB_KIND_STATIC,
      .toolchain = "gcc",
      .standard = SPN_C99,
      .mode = SPN_MODE_RELEASE,
      .opt = SPN_OPT_LEVEL_3,
      .targeted = true,
    },
  },
  {
    .name = "undefined_profile",
    .overrides = { .name = "missing" },
    .host = PROFILE_HOST_LINUX_GNU,
    .expect = { .err = SPN_ERR_PROFILE_UNDEFINED },
  },
  {
    .name = "foreign_arch_is_rejected",
    .overrides = { .arch = SPN_ARCH_WASM32 },
    .host = PROFILE_HOST_LINUX_GNU,
    .expect = { .err = SPN_ERR_PROFILE_ARCH },
  },
  {
    .name = "foreign_abi_is_rejected",
    .overrides = { .os = SPN_OS_MACOS, .abi = SPN_ABI_GNU },
    .host = PROFILE_HOST_LINUX_GNU,
    .expect = { .err = SPN_ERR_PROFILE_ABI },
  },
  {
    .name = "explicit_shared_on_freestanding_is_rejected",
    .profile = { .name = "default", .linkage = { { "shared" } } },
    .overrides = { .arch = SPN_ARCH_ARM64, .os = SPN_OS_FREESTANDING },
    .host = PROFILE_HOST_LINUX_GNU,
    .expect = { .err = SPN_ERR_PROFILE_LINKAGE },
  },
  {
    .name = "manifest_foreign_abi_is_rejected",
    .profile = { .name = "default", .arch = SPN_ARCH_WASM32, .os = SPN_OS_WASI, .abi = { { "gnu" } } },
    .host = PROFILE_HOST_LINUX_GNU,
    .expect = { .err = SPN_ERR_PROFILE_ABI },
  },
};

typedef struct {
  spn_abi_t abis [PROFILE_MAX_ABIS];
} query_expect_t;

typedef struct {
  const c8* name;
  spn_triple_t target;
  spn_linkage_t linkage;
  spn_sanitizer_set_t sanitizers;
  spn_triple_t host;
  query_expect_t expect;
} query_test_t;

static const query_test_t query_tests [] = {
  {
    .name = "native_unset_linkage_prefers_musl",
    .target = { SPN_ARCH_X64, SPN_OS_LINUX },
    .host = PROFILE_HOST_LINUX_GNU,
    .expect = { .abis = { SPN_ABI_MUSL, SPN_ABI_GNU } },
  },
  {
    .name = "native_static_prefers_musl",
    .target = { SPN_ARCH_X64, SPN_OS_LINUX },
    .linkage = SPN_LIB_KIND_STATIC,
    .host = PROFILE_HOST_LINUX_GNU,
    .expect = { .abis = { SPN_ABI_MUSL, SPN_ABI_GNU } },
  },
  {
    .name = "native_shared_prefers_host_libc",
    .target = { SPN_ARCH_X64, SPN_OS_LINUX },
    .linkage = SPN_LIB_KIND_SHARED,
    .host = PROFILE_HOST_LINUX_GNU,
    .expect = { .abis = { SPN_ABI_GNU, SPN_ABI_MUSL } },
  },
  {
    .name = "request_carries_sanitizers_and_linkage",
    .target = { SPN_ARCH_X64, SPN_OS_LINUX, SPN_ABI_GNU },
    .linkage = SPN_LIB_KIND_STATIC,
    .sanitizers = SPN_SANITIZER_ADDRESS | SPN_SANITIZER_UNDEFINED,
    .host = PROFILE_HOST_LINUX_GNU,
    .expect = { .abis = { SPN_ABI_GNU } },
  },
  {
    .name = "native_shared_on_musl_host_prefers_musl",
    .target = { SPN_ARCH_X64, SPN_OS_LINUX },
    .linkage = SPN_LIB_KIND_SHARED,
    .host = PROFILE_HOST_LINUX_MUSL,
    .expect = { .abis = { SPN_ABI_MUSL, SPN_ABI_GNU } },
  },
  {
    .name = "explicit_abi_is_the_only_candidate",
    .target = { SPN_ARCH_X64, SPN_OS_LINUX, SPN_ABI_GNU },
    .host = PROFILE_HOST_LINUX_GNU,
    .expect = { .abis = { SPN_ABI_GNU } },
  },
  {
    .name = "explicit_abi_ignores_linkage",
    .target = { SPN_ARCH_X64, SPN_OS_LINUX, SPN_ABI_MUSL },
    .linkage = SPN_LIB_KIND_SHARED,
    .host = PROFILE_HOST_LINUX_GNU,
    .expect = { .abis = { SPN_ABI_MUSL } },
  },
  {
    .name = "native_windows_ignores_linkage",
    .target = { SPN_ARCH_X64, SPN_OS_WINDOWS },
    .linkage = SPN_LIB_KIND_SHARED,
    .host = PROFILE_HOST_WIN_MSVC,
    .expect = { .abis = { SPN_ABI_GNU, SPN_ABI_MSVC } },
  },
  {
    .name = "cross_explicit_abi_is_the_only_candidate",
    .target = { SPN_ARCH_ARM64, SPN_OS_LINUX, SPN_ABI_MUSL },
    .host = PROFILE_HOST_LINUX_GNU,
    .expect = { .abis = { SPN_ABI_MUSL } },
  },
  {
    .name = "freestanding_has_no_candidates",
    .target = { SPN_ARCH_ARM64, SPN_OS_FREESTANDING },
    .host = PROFILE_HOST_LINUX_GNU,
  },
  {
    .name = "cross_os_with_many_abis_has_no_candidates",
    .target = { SPN_ARCH_X64, SPN_OS_WINDOWS },
    .host = PROFILE_HOST_LINUX_GNU,
  },
  {
    .name = "cross_arch_with_many_abis_has_no_candidates",
    .target = { SPN_ARCH_ARM64, SPN_OS_LINUX },
    .linkage = SPN_LIB_KIND_SHARED,
    .host = PROFILE_HOST_LINUX_GNU,
  },
};

typedef struct {
  spn_linkage_t linkage;
  spn_cc_driver_t driver;
  spn_ld_family_t linker;
} finalize_expect_t;

typedef struct {
  const c8* name;
  spn_triple_t target;
  spn_cc_driver_t driver;
  bool lld;
  finalize_expect_t expect;
} finalize_test_t;

static const finalize_test_t finalize_tests [] = {
  { .name = "gnu_is_shared",   .target = { SPN_ARCH_X64, SPN_OS_LINUX, SPN_ABI_GNU },          .driver = SPN_CC_DRIVER_GCC,   .expect = { .linkage = SPN_LIB_KIND_SHARED, .driver = SPN_CC_DRIVER_GCC,   .linker = SPN_LD_FAMILY_GNU } },
  { .name = "musl_is_static",  .target = { SPN_ARCH_X64, SPN_OS_LINUX, SPN_ABI_MUSL },         .driver = SPN_CC_DRIVER_GCC,   .expect = { .linkage = SPN_LIB_KIND_STATIC, .driver = SPN_CC_DRIVER_GCC,   .linker = SPN_LD_FAMILY_GNU } },
  { .name = "msvc_is_shared",  .target = { SPN_ARCH_X64, SPN_OS_WINDOWS, SPN_ABI_MSVC },       .driver = SPN_CC_DRIVER_MSVC,  .expect = { .linkage = SPN_LIB_KIND_SHARED, .driver = SPN_CC_DRIVER_MSVC,  .linker = SPN_LD_FAMILY_MSVC } },
  { .name = "apple_is_shared", .target = { SPN_ARCH_ARM64, SPN_OS_MACOS, SPN_ABI_APPLE },      .driver = SPN_CC_DRIVER_CLANG, .expect = { .linkage = SPN_LIB_KIND_SHARED, .driver = SPN_CC_DRIVER_CLANG, .linker = SPN_LD_FAMILY_LD64 } },
  { .name = "bare_is_static",  .target = { SPN_ARCH_X64, SPN_OS_FREESTANDING, SPN_ABI_BARE },  .driver = SPN_CC_DRIVER_GCC,   .expect = { .linkage = SPN_LIB_KIND_STATIC, .driver = SPN_CC_DRIVER_GCC,   .linker = SPN_LD_FAMILY_GNU } },
  { .name = "elf_is_static",   .target = { SPN_ARCH_ARM64, SPN_OS_FREESTANDING, SPN_ABI_ELF }, .driver = SPN_CC_DRIVER_GCC,   .expect = { .linkage = SPN_LIB_KIND_STATIC, .driver = SPN_CC_DRIVER_GCC,   .linker = SPN_LD_FAMILY_GNU } },
  { .name = "records_driver",  .target = { SPN_ARCH_X64, SPN_OS_LINUX, SPN_ABI_GNU },          .driver = SPN_CC_DRIVER_ZIG,   .expect = { .linkage = SPN_LIB_KIND_SHARED, .driver = SPN_CC_DRIVER_ZIG,   .linker = SPN_LD_FAMILY_LLD } },
  { .name = "records_linker",  .target = { SPN_ARCH_X64, SPN_OS_LINUX, SPN_ABI_GNU },          .driver = SPN_CC_DRIVER_GCC,   .lld = true, .expect = { .linkage = SPN_LIB_KIND_SHARED, .driver = SPN_CC_DRIVER_GCC, .linker = SPN_LD_FAMILY_LLD } },
};

static spn_when_t clauses_to_when(sp_mem_t mem, const clause_t* clauses, u32 count) {
  spn_when_t when = { .clauses = sp_da_new(mem, spn_when_clause_t) };
  sp_for(it, count) {
    if (!clauses[it].key) {
      break;
    }
    sp_da_push(when.clauses, ((spn_when_clause_t) {
      .key = sp_cstr_as_str(clauses[it].key),
      .value = spn_option_value_str(sp_cstr_as_str(clauses[it].value)),
    }));
  }
  return when;
}

static spn_gated_list_t candidates_to_list(sp_mem_t mem, const candidate_t* candidates, u32 count) {
  spn_gated_list_t list = sp_da_new(mem, spn_gated_str_t);
  sp_for(it, count) {
    if (!candidates[it].value) {
      break;
    }
    sp_da_push(list, ((spn_gated_str_t) {
      .value = sp_cstr_as_str(candidates[it].value),
      .when = clauses_to_when(mem, candidates[it].when, sp_carr_len(candidates[it].when)),
    }));
  }
  return list;
}

#define candidates(mem, field) candidates_to_list(mem, field, sp_carr_len(field))

static spn_profile_decl_t desc_to_decl(sp_mem_t mem, const decl_t* d) {
  return (spn_profile_decl_t) {
    .name = sp_cstr_as_str(d->name),
    .os = d->os,
    .arch = d->arch,
    .toolchain = candidates(mem, d->toolchain),
    .abi = candidates(mem, d->abi),
    .linkage = candidates(mem, d->linkage),
    .standard = candidates(mem, d->standard),
    .mode = candidates(mem, d->mode),
    .opt = candidates(mem, d->opt),
  };
}

static spn_profile_override_t desc_to_override(const override_t* d) {
  return (spn_profile_override_t) {
    .name = d->name ? sp_cstr_as_str(d->name) : (sp_str_t) sp_zero,
    .toolchain = d->toolchain ? sp_cstr_as_str(d->toolchain) : (sp_str_t) sp_zero,
    .mode = d->mode,
    .triple = { .arch = d->arch, .os = d->os, .abi = d->abi },
  };
}

sp_test_each(profile, resolve, test_t, tests, .setup = spn_test_ctx_setup) {
  sp_mem_t mem = spn.mem;

  spn_profile_override_t overrides = desc_to_override(&it->overrides);

  spn_pkg_info_t pkg = sp_zero;
  sp_str_om_init(pkg.profiles);
  if (it->profile.name) {
    spn_profile_decl_t decl = desc_to_decl(mem, &it->profile);
    sp_str_om_insert(pkg.profiles, decl.name, decl);
  }
  if (it->derived.name) {
    spn_profile_decl_t decl = desc_to_decl(mem, &it->derived);
    sp_str_om_insert(pkg.profiles, decl.name, decl);
  }
  if (it->shared_demand) {
    sp_str_om_insert(pkg.libs, sp_str_lit("L"), ((spn_target_info_t) { .name = sp_str_lit("L"), .linkages = { .shared = true } }));
  }

  spn_profile_info_t result = sp_zero;
  spn_err_t err = spn_profile_resolve(&overrides, it->host, &pkg, &result);
  sp_must_eq(t, (u32)it->expect.err, (u32)err);
  if (err) {
    sp_da(spn_event_t) errs = spn_test_drain_errs(mem);
    sp_must_eq(t, 1, sp_da_size(errs));
    sp_expect_eq(t, errs[0].err.kind, err);
    return SP_OK;
  }

  if (it->expect.name) {
    sp_expect_str_eq_c(t, result.name, it->expect.name);
  }
  sp_expect(t, spn_triple_equal(it->expect.target, (spn_triple_t) { result.arch, result.os, result.abi }));
  sp_expect_eq(t, it->expect.targeted, result.targeted);
  if (it->expect.toolchain) {
    spn_toolchain_ref_t toolchain = spn_toolchain_ref_from_str(sp_cstr_as_str(it->expect.toolchain));
    sp_expect_eq(t, (u32)toolchain.kind, (u32)result.toolchain.kind);
    sp_expect_str_eq(t, toolchain.name, result.toolchain.name);
  }
  if (it->expect.standard) {
    sp_expect_eq(t, (u32)it->expect.standard, (u32)result.standard);
  }
  if (it->expect.mode) {
    sp_expect_eq(t, (u32)it->expect.mode, (u32)result.mode);
  }
  if (it->expect.opt) {
    sp_expect_eq(t, (u32)it->expect.opt, (u32)result.opt);
  }

  spn_toolchain_info_t info = { .driver = SPN_CC_DRIVER_ZIG };
  spn_toolchain_selection_t selection = { .toolchain = &info, .row.triple = { result.arch, result.os, it->abi ? it->abi : result.abi } };
  spn_profile_finalize(&result, &selection);
  sp_expect_eq(t, (u32)it->expect.linkage, (u32)result.linkage);
  return SP_OK;
}

sp_test_each(profile, query, query_test_t, query_tests) {
  spn_profile_info_t profile = {
    .toolchain = { SPN_TOOLCHAIN_REF_NAMED, sp_str_lit("T") },
    .arch = it->target.arch,
    .os = it->target.os,
    .abi = it->target.abi,
    .linkage = it->linkage,
    .sanitizers = it->sanitizers,
  };

  spn_toolchain_query_t query = spn_profile_query(&profile, it->host);
  sp_expect_eq(t, (u32)profile.toolchain.kind, (u32)query.toolchain.kind);
  sp_expect_str_eq(t, query.toolchain.name, profile.toolchain.name);
  sp_expect(t, spn_triple_equal(query.target, it->target));
  sp_expect_eq(t, query.sanitizers, it->sanitizers);
  sp_expect_eq(t, (u32)query.linkage, (u32)it->linkage);

  u32 abis = 0;
  sp_carr_detect_len(it->expect.abis, abis, it->expect.abis[abis]);
  sp_must_eq(t, abis, query.abis.count);
  sp_for(at, abis) {
    sp_expect_eq(t, (u32)it->expect.abis[at], (u32)query.abis.items[at]);
  }
  return SP_OK;
}

sp_test_each(profile, finalize, finalize_test_t, finalize_tests) {
  spn_profile_info_t profile = sp_zero;
  spn_toolchain_info_t info = { .driver = it->driver, .lld = it->lld };
  spn_toolchain_selection_t selection = { .toolchain = &info, .row.triple = it->target };
  spn_profile_finalize(&profile, &selection);
  sp_expect_eq(t, (u32)it->expect.linkage, (u32)profile.linkage);
  sp_expect_eq(t, (u32)it->expect.driver, (u32)profile.driver);
  sp_expect_eq(t, (u32)it->expect.linker, (u32)profile.linker);
  return SP_OK;
}
