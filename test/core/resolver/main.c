#define SP_IMPLEMENTATION
#include "sp.h"

#include "sp/sp_test.h"
#include "fixture.h"
#include "sp_fuzz.h"

#include "ctx/types.h"
#include "codegen/lower.h"
#include "enum/enum.h"
#include "external/tom.h"
#include "index/cache.h"
#include "index/json.h"
#include "index/types.h"
#include "intern/intern.h"
#include "pkg/id.h"
#include "resolve/dump.h"
#include "resolve/resolve.h"
#include "resolve/types.h"
#include "spn.h"
#include "toml/loader.h"

spn_ctx_t spn;

static sp_mem_arena_t* arena;

typedef struct {
  sp_str_ht(spn_index_pkg_t) cache;
} state_t;

static state_t state;

void spn_index_cache_init(spn_index_cache_t* cache, sp_mem_t mem, sp_intern_t* intern, spn_index_arr_t* indexes) {

}

spn_index_pkg_t* spn_index_cache_get_package(spn_index_cache_t* cache, spn_pkg_name_t id) {
  sp_str_t qualified = spn_pkg_name_to_qualified(id);
  return sp_str_ht_get(state.cache, qualified);
}

spn_index_release_t* spn_index_cache_get_release(spn_index_cache_t* cache, spn_pkg_name_t id, spn_semver_t version) {
  return SP_NULLPTR;
}


static const c8* root_qualified = "test/root";

typedef struct {
  sp_str_t skip;
  spn_linkage_t linkage;
  u64 budget;
} fx_config_t;

typedef struct {
  spn_resolve_query_t query;
  sp_intern_t* intern;
} resolve_result_t;

static resolve_result_t execute_fixture(const fx_config_t* config, const spn_pkg_info_t* root, sp_intern_t* intern) {
  sp_mem_t mem = sp_mem_arena_as_allocator(arena);

  spn_index_cache_t cache = sp_zero;
  spn_pkg_registry_t registry = sp_zero;
  sp_ht_init(mem, registry);

  sp_ht_insert(registry, spn_pkg_id(intern, root->qualified), ((spn_registry_pkg_t) {
    .source = SPN_PKG_SOURCE_ROOT,
    .info = (spn_pkg_info_t*)root,
  }));

  spn_resolver_t resolver = sp_zero;
  spn_resolver_init(&resolver, mem, intern, &cache, &registry, (spn_profile_info_t) {
    .linkage = config->linkage,
    .os = SPN_OS_LINUX,
    .arch = SPN_ARCH_X64,
    .abi = SPN_ABI_GNU,
  }, root->config, config->budget);

  resolve_result_t result = sp_zero_s(resolve_result_t);
  result.intern = intern;
  spn_resolve_query_init(mem, &result.query);
  spn_resolve_query_add(&result.query, (spn_requested_dep_t) {
    .qualified = sp_str_view(root_qualified),
    .source = SPN_PKG_SOURCE_ROOT,
  });

  spn_resolve_from_solver(&resolver, &result.query);
  return result;
}


static sp_str_t fx_dir(sp_test_t* t, sp_mem_t mem) {
  sp_str_t name = sp_str_strip_left(sp_test_get_name(t), sp_str_lit("resolver."));
  return test_repo_path(mem, sp_fs_join_path(mem, sp_str_lit("test/core/resolver/fixtures"), name));
}

static s32 fx_sort_entries(const void* a, const void* b) {
  return sp_str_compare_alphabetical(((const sp_fs_entry_t*)a)->name, ((const sp_fs_entry_t*)b)->name);
}

static sp_err_t fx_load_config(sp_test_t* t, sp_mem_t mem, sp_str_t dir, fx_config_t* config) {
  sp_str_t path = sp_fs_join_path(mem, dir, sp_str_lit("fixture.toml"));
  if (!sp_fs_is_target_file(path)) {
    return SP_OK;
  }

  toml_table_t* table = spn_toml_parse(path);
  sp_must(t, table != SP_NULLPTR);

  config->skip = spn_toml_str_opt(mem, table, "skip", "");
  config->linkage = spn_linkage_from_str(spn_toml_str_opt(mem, table, "linkage", ""));

  toml_value_t budget = toml_table_int(table, "budget");
  if (budget.ok) {
    config->budget = (u64)budget.u.i;
  }

  toml_free(table);
  return SP_OK;
}

static sp_err_t fx_load_index(sp_test_t* t, sp_mem_t mem, sp_str_t dir, sp_da(sp_str_t)* names) {
  sp_da(sp_fs_entry_t) namespaces = sp_fs_collect(mem, dir);
  sp_da_sort(namespaces, fx_sort_entries);
  sp_da_for(namespaces, it) {
    if (namespaces[it].kind != SP_FS_KIND_DIR) continue;

    sp_da(sp_fs_entry_t) files = sp_fs_collect(mem, namespaces[it].path);
    sp_da_sort(files, fx_sort_entries);
    sp_da_for(files, jt) {
      if (!sp_str_ends_with(files[jt].name, sp_str_lit(".jsonl"))) continue;

      sp_str_t blob = sp_zero;
      sp_must_ok(t, sp_io_read_file(mem, files[jt].path, &blob));

      spn_pkg_name_t id = {
        .namespace = namespaces[it].name,
        .name = sp_str_strip_right(files[jt].name, sp_str_lit(".jsonl")),
      };
      spn_index_pkg_t pkg = sp_zero;
      sp_must_eq(t, SPN_OK, spn_index_parse_pkg(mem, id, blob, &pkg));

      sp_str_t qualified = spn_pkg_name_to_qualified(id);
      sp_str_ht_insert(state.cache, qualified, pkg);
      sp_da_push(*names, qualified);
    }
  }
  return SP_OK;
}

static sp_err_t fx_load_root(sp_test_t* t, sp_mem_t mem, sp_str_t dir, spn_pkg_info_t* root) {
  sp_str_t manifest = sp_fs_join_path(mem, dir, sp_str_lit("spn.toml"));
  sp_must(t, sp_fs_is_target_file(manifest));

  spn_toml_loader_t loader = sp_zero;
  spn_toml_loader_init(&loader, mem, spn.intern);
  sp_must_eq(t, SPN_OK, spn_codegen_load_pkg(&loader, manifest, root));
  sp_must_eq(t, 0, sp_da_size(loader.issues));
  return SP_OK;
}

static sp_str_t dump_query(sp_mem_t mem, resolve_result_t* result) {
  spn_cg_resolve_t dump = spn_resolve_dump(mem, result->intern, &result->query);
  return spn_resolve_write(mem, &dump);
}

static sp_err_t run_fixture(sp_test_t* t) {
  sp_mem_t mem = sp_mem_arena_as_allocator(arena);
  state = sp_zero_s(state_t);
  sp_str_ht_init(mem, state.cache);

  sp_str_t dir = fx_dir(t, mem);

  fx_config_t config = sp_zero;
  sp_try(fx_load_config(t, mem, dir, &config));
  if (!sp_str_empty(config.skip)) {
    return sp_test_skip(t, "{}", sp_fmt_str(config.skip));
  }

  sp_da(sp_str_t) names = sp_da_new(mem, sp_str_t);
  sp_da_push(names, sp_str_lit(""));
  sp_da_push(names, sp_str_view(root_qualified));
  sp_try(fx_load_index(t, mem, dir, &names));

  spn_pkg_info_t root = sp_zero;
  sp_try(fx_load_root(t, mem, dir, &root));
  sp_da_for(root.deps, it) {
    sp_da_push(names, root.deps[it].qualified);
  }

  sp_fuzz_prng_t prng = sp_fuzz_stream(names);

  resolve_result_t canonical = execute_fixture(&config, &root, sp_intern_new(sp_mem_os_new()));
  sp_str_t canonical_dump = dump_query(mem, &canonical);
  sp_str_t golden = sp_fmt(mem, "goldens/{}.json", sp_fmt_str(sp_test_get_name(t))).value;
  sp_expect_golden(t, golden, canonical_dump);

  // Picks may never depend on intern state: resolve against perturbed interns
  // and require structurally identical results every round
  sp_for(round, 8) {
    resolve_result_t shaken = execute_fixture(&config, &root, sp_fuzz_perturbed_intern(&prng, names));
    sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
    sp_str_t shaken_dump = sp_str_copy(mem, dump_query(scratch.mem, &shaken));
    sp_mem_end_scratch(scratch);
    sp_must_str_eq(t, shaken_dump, canonical_dump);
  }

  return SP_OK;
}

s32 main(s32 argc, const c8** argv) {
  arena = sp_mem_arena_new(sp_mem_os_new());
  sp_mem_t mem = sp_mem_arena_as_allocator(arena);

  spn.intern = sp_intern_new(sp_mem_os_new());
  sp_fuzz_seed_init();

  sp_da(sp_fs_entry_t) entries = sp_fs_collect(mem, test_repo_path(mem, sp_str_lit("test/core/resolver/fixtures")));
  sp_da_sort(entries, fx_sort_entries);

  sp_da(sp_test_decl_t) decls = sp_da_new(mem, sp_test_decl_t);
  sp_da_for(entries, it) {
    if (entries[it].kind != SP_FS_KIND_DIR) {
      continue;
    }
    if (sp_str_starts_with(entries[it].name, sp_str_lit("."))) {
      continue;
    }
    sp_da_push(decls, ((sp_test_decl_t) {
      .name = sp_str_to_cstr(mem, entries[it].name),
      .fn = run_fixture,
    }));
  }
  sp_da_push(decls, sp_zero_s(sp_test_decl_t));

  sp_test_suite_t suites [] = {
    { .name = "resolver", .tests = decls, .serial = true },
    sp_zero,
  };

  s32 result = sp_test_main(argc, argv, suites);

  sp_mem_arena_destroy(arena);
  return result;
}
