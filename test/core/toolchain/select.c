#include "toolchain.h"

#define SELECT_MAX_CHECKS 3
#define SELECT_MAX_ABIS 3
#define SELECT_MAX_CANDIDATES 3

#define TARGET_LINUX_MUSL     { SPN_ARCH_X64, SPN_OS_LINUX, SPN_ABI_MUSL }
#define TARGET_ARM_LINUX_MUSL { SPN_ARCH_ARM64, SPN_OS_LINUX, SPN_ABI_MUSL }
#define X64_LINUX             { SPN_ARCH_X64, SPN_OS_LINUX }
#define X64_WINDOWS           { SPN_ARCH_X64, SPN_OS_WINDOWS }
#define X64_MACOS             { SPN_ARCH_X64, SPN_OS_MACOS }
#define X64_FREESTANDING      { SPN_ARCH_X64, SPN_OS_FREESTANDING }
#define ARM_LINUX             { SPN_ARCH_ARM64, SPN_OS_LINUX }
#define ARM_MACOS             { SPN_ARCH_ARM64, SPN_OS_MACOS }
#define WASM                  { SPN_ARCH_WASM32, SPN_OS_WASI }

typedef struct {
  spn_err_t err;
  const c8* name;
  spn_triple_t triple;
  spn_triple_t targets [FIXTURE_MAX_TARGETS];
  const c8* candidates [SELECT_MAX_CANDIDATES];
  spn_abi_t abis [SELECT_MAX_ABIS];
} expect_t;

typedef struct {
  spn_triple_t target;
  spn_abi_t abis [SELECT_MAX_ABIS];
  expect_t expect;
} check_t;

typedef struct {
  const c8* name;
  spn_triple_t targets [FIXTURE_MAX_TARGETS];
  check_t checks [SELECT_MAX_CHECKS];
} complete_test_t;

typedef struct {
  const c8* name;
  const c8* file;
  const c8* toolchain;
  spn_triple_t target;
  spn_abi_t abis [SELECT_MAX_ABIS];
  spn_triple_t host;
  expect_t expect;
} resolve_test_t;

static const complete_test_t complete_tests [] = {
  {
    .name = "single_abi_must_be_supported",
    .targets = { HOST_X64_LINUX, TARGET_LINUX_MUSL },
    .checks = {
      { .target = X64_LINUX, .abis = { SPN_ABI_GNU }, .expect = { .triple = HOST_X64_LINUX } },
      { .target = X64_LINUX, .abis = { SPN_ABI_MUSL }, .expect = { .triple = TARGET_LINUX_MUSL } },
      { .target = X64_LINUX, .abis = { SPN_ABI_MSVC }, .expect = { .err = SPN_ERR_TOOLCHAIN_TARGET, .targets = { HOST_X64_LINUX, TARGET_LINUX_MUSL } } },
    },
  },
  {
    .name = "first_supported_abi_wins",
    .targets = { HOST_X64_LINUX, TARGET_LINUX_MUSL },
    .checks = {
      { .target = X64_LINUX, .abis = { SPN_ABI_MUSL, SPN_ABI_GNU }, .expect = { .triple = TARGET_LINUX_MUSL } },
      { .target = X64_LINUX, .abis = { SPN_ABI_GNU, SPN_ABI_MUSL }, .expect = { .triple = HOST_X64_LINUX } },
    },
  },
  {
    .name = "later_abis_fall_back",
    .targets = { HOST_X64_LINUX },
    .checks = {
      { .target = X64_LINUX, .abis = { SPN_ABI_MUSL, SPN_ABI_GNU }, .expect = { .triple = HOST_X64_LINUX } },
    },
  },
  {
    .name = "arch_and_os_must_match",
    .targets = { HOST_ARM_MACOS },
    .checks = {
      { .target = ARM_MACOS, .abis = { SPN_ABI_APPLE }, .expect = { .triple = HOST_ARM_MACOS } },
      { .target = X64_MACOS, .abis = { SPN_ABI_APPLE }, .expect = { .err = SPN_ERR_TOOLCHAIN_TARGET, .targets = { HOST_ARM_MACOS } } },
      { .target = ARM_LINUX, .abis = { SPN_ABI_GNU }, .expect = { .err = SPN_ERR_TOOLCHAIN_TARGET, .targets = { HOST_ARM_MACOS } } },
    },
  },
  {
    .name = "no_abis_needs_one_when_reachable",
    .targets = { HOST_ARM_LINUX, TARGET_ARM_LINUX_MUSL },
    .checks = {
      { .target = ARM_LINUX, .expect = { .err = SPN_ERR_TARGET_ABI, .abis = { SPN_ABI_GNU, SPN_ABI_MUSL } } },
    },
  },
  {
    .name = "no_abis_is_target_error_when_unreachable",
    .targets = { TARGET_WASM },
    .checks = {
      { .target = X64_WINDOWS, .expect = { .err = SPN_ERR_TOOLCHAIN_TARGET, .targets = { TARGET_WASM } } },
    },
  },
  {
    .name = "bound_targets_are_selectable",
    .checks = {
      { .target = X64_FREESTANDING, .abis = { SPN_ABI_BARE }, .expect = { .triple = TARGET_X64_BARE } },
    },
  },
};

static const resolve_test_t resolve_tests [] = {
  {
    .name = "auto_picks_first_in_declared_order",
    .file = "auto.json",
    .target = X64_WINDOWS,
    .abis = { SPN_ABI_GNU },
    .expect = { .name = "A", .triple = TARGET_WIN_GNU },
  },
  {
    .name = "auto_takes_first_supported_abi",
    .file = "auto.json",
    .target = X64_LINUX,
    .abis = { SPN_ABI_MUSL, SPN_ABI_GNU },
    .expect = { .name = "B", .triple = TARGET_LINUX_MUSL },
  },
  {
    .name = "auto_scans_entries_before_abis",
    .file = "order.json",
    .target = X64_LINUX,
    .abis = { SPN_ABI_MUSL, SPN_ABI_GNU },
    .expect = { .name = "A", .triple = HOST_X64_LINUX },
  },
  {
    .name = "auto_skips_unsupported_host",
    .file = "auto.json",
    .target = ARM_LINUX,
    .abis = { SPN_ABI_MUSL, SPN_ABI_GNU },
    .host = HOST_ARM_LINUX,
    .expect = { .name = "C", .triple = TARGET_ARM_LINUX_MUSL },
  },
  {
    .name = "auto_with_empty_catalog",
    .file = "empty.json",
    .target = X64_LINUX,
    .abis = { SPN_ABI_GNU },
    .expect = { .err = SPN_ERR_TOOLCHAIN_NONE },
  },
  {
    .name = "auto_with_no_capable_toolchain",
    .file = "auto.json",
    .target = WASM,
    .abis = { SPN_ABI_MUSL },
    .expect = { .err = SPN_ERR_TOOLCHAIN_NONE },
  },
  {
    .name = "auto_without_abis_needs_one",
    .file = "auto.json",
    .target = X64_WINDOWS,
    .expect = { .err = SPN_ERR_TARGET_ABI, .abis = { SPN_ABI_GNU, SPN_ABI_MSVC } },
  },
  {
    .name = "auto_without_abis_rejects_unreachable_target",
    .file = "auto.json",
    .target = { SPN_ARCH_WASM32, SPN_OS_LINUX },
    .expect = { .err = SPN_ERR_TOOLCHAIN_NONE },
  },
  {
    .name = "unknown_name_lists_capable_toolchains",
    .file = "auto.json",
    .toolchain = "Z",
    .target = X64_WINDOWS,
    .abis = { SPN_ABI_GNU },
    .expect = { .err = SPN_ERR_TOOLCHAIN_UNKNOWN, .candidates = { "A", "C" } },
  },
  {
    .name = "target_error_lists_capable_toolchains",
    .file = "auto.json",
    .toolchain = "A",
    .target = X64_LINUX,
    .abis = { SPN_ABI_GNU },
    .expect = { .err = SPN_ERR_TOOLCHAIN_TARGET, .targets = { TARGET_WIN_GNU }, .candidates = { "B", "D" } },
  },
  {
    .name = "host_error_lists_capable_toolchains",
    .file = "restricted.json",
    .toolchain = "A",
    .target = ARM_MACOS,
    .abis = { SPN_ABI_APPLE },
    .host = HOST_ARM_MACOS,
    .expect = { .err = SPN_ERR_TOOLCHAIN_HOST, .candidates = { "B" } },
  },
  {
    .name = "target_error_precedes_host_error",
    .file = "distribution.json",
    .toolchain = "A",
    .target = X64_WINDOWS,
    .abis = { SPN_ABI_GNU },
    .host = HOST_ARM_LINUX,
    .expect = { .err = SPN_ERR_TOOLCHAIN_TARGET, .targets = { TARGET_WASM, TARGET_LINUX_MUSL } },
  },
};

static spn_abi_list_t abi_list(const spn_abi_t abis [SELECT_MAX_ABIS]) {
  spn_abi_list_t list = sp_zero;
  sp_carr_detect_len(abis, list.count, abis[list.count]);
  sp_for(it, list.count) {
    list.items[it] = abis[it];
  }
  return list;
}

static sp_err_t check_failure(sp_test_t* t, sp_mem_t mem, spn_toolchain_catalog_t* catalog, spn_toolchain_query_t query, const expect_t* expect) {
  sp_da(spn_event_t) errs = spn_test_drain_errs(mem);
  sp_must_eq(t, 1, sp_da_size(errs));
  sp_expect_eq(t, errs[0].err.kind, expect->err);

  if (expect->err == SPN_ERR_TARGET_ABI) {
    spn_err_completion_t* completion = &errs[0].err.completion;
    sp_expect(t, spn_triple_equal(completion->target, query.target));
    sp_expect(t, spn_triple_equal(completion->host, catalog->host));
    spn_abi_list_t abis = abi_list(expect->abis);
    sp_must_eq(t, abis.count, (u32)sp_da_size(completion->candidates));
    sp_for(it, abis.count) {
      sp_expect_eq(t, (u32)abis.items[it], (u32)completion->candidates[it]);
    }
    return SP_OK;
  }

  spn_err_toolchain_t* err = &errs[0].err.toolchain;
  sp_expect_str_eq(t, err->name, query.toolchain.name);
  sp_expect(t, spn_triple_equal(err->target, query.target));
  sp_expect(t, spn_triple_equal(err->host, catalog->host));

  u32 targets = 0;
  sp_carr_detect_len(expect->targets, targets, !fixture_triple_empty(expect->targets[targets]));
  if (fixture_check_targets(t, err->targets, expect->targets, targets)) {
    return SP_ERR;
  }

  sp_must_strs_eq(t, err->candidates, sp_da_size(err->candidates), expect->candidates);
  return SP_OK;
}

static spn_err_t query_catalog(spn_toolchain_catalog_t* catalog, spn_toolchain_query_t query, spn_toolchain_selection_t* selection) {
  if (!query.abis.count) {
    return spn_toolchain_incomplete(catalog, query);
  }
  return spn_toolchain_select(catalog, query, selection);
}

static sp_err_t check_selection(sp_test_t* t, const spn_toolchain_selection_t* selection, const c8* name, spn_triple_t triple) {
  sp_must(t, selection->toolchain);
  sp_expect_str_eq_c(t, selection->toolchain->name, name);
  sp_expect(t, spn_triple_equal(selection->triple, triple));
  return SP_OK;
}

sp_test_each(select, complete, complete_test_t, complete_tests, .setup = spn_test_ctx_setup) {
  sp_mem_t mem = sp_test_arena(t);

  spn_toolchain_decl_t toolchain = fixture_local_toolchain("A", "cc");
  u32 declared = 0;
  sp_carr_detect_len(it->targets, declared, !fixture_triple_empty(it->targets[declared]));
  if (declared) {
    toolchain.targets = sp_da_new(mem, spn_triple_t);
    sp_for(at, declared) {
      sp_da_push(toolchain.targets, it->targets[at]);
    }
  }

  spn_toolchain_catalog_t catalog = sp_zero;
  spn_toolchain_catalog_init(&catalog, (spn_triple_t) HOST_X64_LINUX, mem);
  spn_toolchain_catalog_add(&catalog, toolchain);

  u32 checks = 0;
  sp_carr_detect_len(it->checks, checks, it->checks[checks].expect.err || !fixture_triple_empty(it->checks[checks].expect.triple));
  sp_for(at, checks) {
    const check_t* check = &it->checks[at];
    spn_toolchain_query_t query = {
      .toolchain = { SPN_TOOLCHAIN_REF_NAMED, sp_str_lit("A") },
      .target = check->target,
      .abis = abi_list(check->abis),
    };
    spn_toolchain_selection_t selection = sp_zero;
    spn_err_t err = query_catalog(&catalog, query, &selection);
    sp_must_eq(t, (u32)check->expect.err, (u32)err);

    if (err) {
      if (check_failure(t, mem, &catalog, query, &check->expect)) {
        return SP_ERR;
      }
    } else {
      if (check_selection(t, &selection, "A", check->expect.triple)) {
        return SP_ERR;
      }
    }
  }

  return SP_OK;
}

sp_test_each(select, resolve, resolve_test_t, resolve_tests, .setup = spn_test_ctx_setup) {
  sp_mem_t mem = sp_test_arena(t);

  spn_triple_t host = fixture_triple_empty(it->host) ? (spn_triple_t) HOST_X64_LINUX : it->host;
  spn_toolchain_catalog_t catalog = sp_zero;
  if (fixture_catalog(t, &catalog, it->file, host)) {
    return SP_ERR;
  }

  spn_toolchain_query_t query = {
    .toolchain = { .kind = SPN_TOOLCHAIN_REF_AUTO },
    .target = it->target,
    .abis = abi_list(it->abis),
  };
  if (it->toolchain) {
    query.toolchain = (spn_toolchain_ref_t) { SPN_TOOLCHAIN_REF_NAMED, sp_cstr_as_str(it->toolchain) };
  }
  spn_toolchain_selection_t selection = sp_zero;
  spn_err_t err = query_catalog(&catalog, query, &selection);
  sp_must_eq(t, (u32)it->expect.err, (u32)err);

  if (err) {
    return check_failure(t, mem, &catalog, query, &it->expect);
  }
  return check_selection(t, &selection, it->expect.name, it->expect.triple);
}
