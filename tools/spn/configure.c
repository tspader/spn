#define SP_IMPLEMENTATION
#include "sp.h"
#include "spn.h"

#define SPN_CODEGEN_MAX_UNITS 8

typedef struct {
  const c8* target;
  const c8* units [SPN_CODEGEN_MAX_UNITS];
} consumer_t;

static const consumer_t consumers [] = {
  { "spn",           { "abi", "config", "errors", "events", "fuzz", "manifest", "release", "source_deps" } },
  { "smoke" },
  { "winvm" },
  { "core",          { "config", "errors", "events", "manifest", "release", "source_deps" } },
  { "resolver",      { "config", "errors", "events", "manifest", "release", "resolve" } },
  { "integration",   { "config", "errors", "events", "source_deps" } },
  { "render",        { "config", "errors", "events", "source_deps" } },
  { "fuzz_dag",      { "fuzz" } },
  { "fuzz_resolver", { "config", "errors", "events", "manifest", "release" } },
  { "toolchain",     { "config", "errors", "events" } },
  { "unit",          { "config", "errors", "events", "source_deps" } },
};

static s32 sort_paths(const void* a, const void* b) {
  const sp_fs_entry_t* ea = (const sp_fs_entry_t*)a;
  const sp_fs_entry_t* eb = (const sp_fs_entry_t*)b;
  return sp_str_compare_alphabetical(ea->path, eb->path);
}

static const c8* host_path(spn_t* spn, sp_mem_t mem, sp_str_t guest) {
  sp_str_t relative = sp_str_strip_left(guest, sp_str_lit("/source/"));
  return spn_get_subdir(spn, SPN_DIR_SOURCE, sp_str_to_cstr(mem, relative));
}

static void add_inputs(spn_t* spn, sp_mem_t mem, spn_node_t* node, sp_str_t dir) {
  sp_da(sp_fs_entry_t) entries = sp_zero;
  sp_fs_collect_recursive(mem, dir, &entries);
  sp_da_sort(entries, sort_paths);
  sp_da_for(entries, it) {
    if (entries[it].kind != SP_FS_KIND_FILE) {
      continue;
    }
    spn_node_add_input(node, host_path(spn, mem, entries[it].path));
  }
}

// Must match the union formats dispatched in tools/gen/run.c; union schemas
// render their public header into gen/include/spn/ instead of gen/codegen/gen/.
static bool is_union_schema(sp_str_t name) {
  return sp_str_equal_cstr(name, "errors") || sp_str_equal_cstr(name, "events");
}

static const c8* gen_path(sp_mem_t mem, const c8* dir, sp_str_t name, const c8* suffix) {
  return sp_fmt_mem_cstr(mem, "gen/{}/{}{}", sp_fmt_cstr(dir), sp_fmt_str(name), sp_fmt_cstr(suffix));
}

static void add_codegen(spn_t* spn, spn_config_t* config) {
  sp_mem_t mem = sp_mem_heap_as_allocator(sp_mem_heap_new());

  spn_node_t* node = spn_add_node(config, "codegen");
  spn_node_set_fn(node, "codegen");

  add_inputs(spn, mem, node, sp_str_lit("/source/source/core/codegen/schema"));
  add_inputs(spn, mem, node, sp_str_lit("/source/tools/gen/templates"));

  spn_node_add_output(node, SPN_DIR_WORK, gen_path(mem, "codegen/gen", sp_str_lit("common"), ".gen.h"));
  spn_node_add_output(node, SPN_DIR_WORK, gen_path(mem, "codegen/gen", sp_str_lit("abi"), ".gen.h"));
  spn_node_add_output(node, SPN_DIR_WORK, gen_path(mem, "codegen/gen", sp_str_lit("abi"), ".gen.c"));
  spn_node_add_output(node, SPN_DIR_WORK, gen_path(mem, "include/spn", sp_str_lit("err"), ".h"));

  sp_da(sp_fs_entry_t) schemas = sp_zero;
  sp_fs_collect(mem, sp_str_lit("/source/source/core/codegen/schema"), &schemas);
  sp_da_sort(schemas, sort_paths);
  sp_da_for(schemas, it) {
    sp_fs_entry_t* entry = &schemas[it];
    if (!sp_str_ends_with(entry->name, sp_str_lit(".jtd.json"))) {
      continue;
    }
    if (sp_str_equal_cstr(entry->name, "common.jtd.json")) {
      continue;
    }
    sp_str_t name = sp_str_strip_right(entry->name, sp_str_lit(".jtd.json"));
    spn_node_add_output(node, SPN_DIR_WORK, gen_path(mem, "codegen/gen", name, ".gen.c"));
    if (is_union_schema(name)) {
      spn_node_add_output(node, SPN_DIR_WORK, gen_path(mem, "include/spn", name, ".h"));
    } else {
      spn_node_add_output(node, SPN_DIR_WORK, gen_path(mem, "codegen/gen", name, ".gen.h"));
      spn_node_add_output(node, SPN_DIR_WORK, gen_path(mem, "codegen/gen", name, ".jtd.json"));
    }
  }

  const c8* include = spn_get_subdir(spn, SPN_DIR_WORK, "gen/include");
  const c8* gen = spn_get_subdir(spn, SPN_DIR_WORK, "gen/codegen/gen");
  sp_carr_for(consumers, it) {
    const consumer_t* consumer = &consumers[it];
    spn_target_t* target = spn_get_target(spn, consumer->target);
    spn_target_add_include(target, include);
    spn_target_add_include(target, gen);
    sp_carr_for(consumer->units, ut) {
      if (!consumer->units[ut]) {
        break;
      }
      spn_target_add_source(target, spn_get_subdir(spn, SPN_DIR_WORK, gen_path(mem, "codegen/gen", sp_str_view(consumer->units[ut]), ".gen.c")));
    }
  }
  spn_target_add_flag(spn_get_target(spn, "core"), sp_fmt_mem_cstr(mem, "-DSCHEMA_GEN_DIR=\"{}\"", sp_fmt_cstr(gen)));
}

SPN_EXPORT
spn_err_t configure(spn_t* spn, spn_config_t* config) {
  spn_target_t* target = spn_get_target(spn, "spn");
  spn_target_embed_file_ex(target, "include/spn.h", "include_spn_h", "u8", "u64");
  spn_target_embed_file_ex(target, "include/spn/core.h", "include_spn_core_h", "u8", "u64");
  spn_target_embed_file_ex(target, spn_get_subdir(spn, SPN_DIR_WORK, "gen/include/spn/err.h"), "include_spn_err_h", "u8", "u64");
  spn_target_embed_file_ex(target, "source/core/toolchain/toolchains.toml", "toolchains_toml", "u8", "u64");
  spn_target_embed_dir_ex(target, "assets/init", "init", "u8", "u64");

  add_codegen(spn, config);
  return SPN_OK;
}
