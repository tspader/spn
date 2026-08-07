#define SP_IMPLEMENTATION
#include "sp.h"

#include "sp/sp_test.h"
#include "fixture.h"

#include "ctx/types.h"
#include "codegen/lower.h"
#include "enum/enum.h"
#include "external/tom.h"
#include "index/cache.h"
#include "index/index.h"
#include "index/types.h"
#include "intern/intern.h"
#include "pkg/id.h"
#include "resolve/dump.h"
#include "resolve/resolve.h"
#include "resolve/types.h"
#include "spn/core.h"
#include "toml/loader.h"

static sp_mem_arena_t* arena;

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

static resolve_result_t execute_fixture(const fx_config_t* config, const spn_pkg_info_t* root, sp_str_t dir, sp_intern_t* intern) {
  sp_mem_t mem = sp_mem_arena_as_allocator(arena);

  spn_index_arr_t indexes = sp_da_new(mem, spn_index_info_t);

  sp_str_t dir_index_location = sp_fs_join_path(mem, dir, sp_str_lit("index"));
  if (sp_fs_is_dir(dir_index_location)) {
    sp_da_push(indexes, ((spn_index_info_t) {
      .location = dir_index_location,
      .protocol = SPN_INDEX_PROTOCOL_DIR,
    }));
  }

  sp_da_push(indexes, ((spn_index_info_t) {
    .location = dir,
  }));

  spn_index_cache_t cache = sp_zero;
  spn_index_cache_init(&cache, mem, intern, &indexes);

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


static sp_str_t get_fixture_dir(sp_test_t* t, sp_mem_t mem) {
  sp_str_t name = sp_str_strip_left(sp_test_get_name(t), sp_str_lit("resolver."));
  return test_repo_path(mem, sp_fs_join_path(mem, sp_str_lit("test/core/resolver/fixtures"), name));
}

static s32 sort_dir_entries(const void* a, const void* b) {
  return sp_str_compare_alphabetical(((const sp_fs_entry_t*)a)->name, ((const sp_fs_entry_t*)b)->name);
}

static sp_err_t load_config(sp_test_t* t, sp_mem_t mem, sp_str_t dir, fx_config_t* config) {
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

static sp_err_t load_manifest(sp_test_t* t, sp_mem_t mem, sp_str_t dir, spn_pkg_info_t* root) {
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

  sp_str_t dir = get_fixture_dir(t, mem);

  fx_config_t config = sp_zero;
  sp_try(load_config(t, mem, dir, &config));
  if (!sp_str_empty(config.skip)) {
    return sp_test_skip(t, "{}", sp_fmt_str(config.skip));
  }

  spn_pkg_info_t root = sp_zero;
  sp_try(load_manifest(t, mem, dir, &root));

  resolve_result_t result = execute_fixture(&config, &root, dir, spn.intern);
  sp_str_t dump = dump_query(mem, &result);
  sp_str_t golden = sp_fmt(mem, "goldens/{}.json", sp_fmt_str(sp_test_get_name(t))).value;
  sp_expect_golden(t, golden, dump);

  return SP_OK;
}

s32 main(s32 argc, const c8** argv) {
  arena = sp_mem_arena_new(sp_mem_os_new());
  sp_mem_t mem = sp_mem_arena_as_allocator(arena);

  spn.intern = sp_intern_new(sp_mem_os_new());

  sp_da(sp_fs_entry_t) entries = sp_fs_collect(mem, test_repo_path(mem, sp_str_lit("test/core/resolver/fixtures")));
  sp_da_sort(entries, sort_dir_entries);

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
