#define SP_IMPLEMENTATION
#include "sp.h"
#include "spn.h"

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
  sp_da(sp_fs_entry_t) entries = sp_fs_collect_recursive(mem, dir);
  sp_da_sort(entries, sort_paths);
  sp_da_for(entries, it) {
    if (entries[it].kind != SP_FS_KIND_FILE) {
      continue;
    }
    spn_node_add_input(node, host_path(spn, mem, entries[it].path));
  }
}

static void add_output(spn_t* spn, sp_mem_t mem, spn_node_t* node, sp_str_t name, const c8* suffix) {
  sp_str_t file = sp_fmt(mem, "source/codegen/gen/{}{}", sp_fmt_str(name), sp_fmt_cstr(suffix)).value;
  spn_node_add_output(node, spn_get_subdir(spn, SPN_DIR_SOURCE, sp_str_to_cstr(mem, file)));
}

static void add_codegen(spn_t* spn, spn_config_t* config) {
  sp_mem_t mem = sp_mem_heap_as_allocator(sp_mem_heap_new());

  spn_node_t* node = spn_add_node(config, "codegen");
  spn_node_set_fn(node, "codegen");

  add_inputs(spn, mem, node, sp_str_lit("/source/source/codegen/schema"));
  add_inputs(spn, mem, node, sp_str_lit("/source/tools/templates"));

  add_output(spn, mem, node, sp_str_lit("common"), ".gen.h");
  add_output(spn, mem, node, sp_str_lit("abi"), ".gen.h");
  add_output(spn, mem, node, sp_str_lit("abi"), ".gen.c");

  sp_da(sp_fs_entry_t) schemas = sp_fs_collect(mem, sp_str_lit("/source/source/codegen/schema"));
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
    add_output(spn, mem, node, name, ".gen.h");
    add_output(spn, mem, node, name, ".gen.c");
    add_output(spn, mem, node, name, ".jtd.json");
  }
}

SPN_EXPORT
spn_err_t configure(spn_t* spn, spn_config_t* config) {
  spn_target_t* target = spn_get_target(spn, "spn");
  spn_target_embed_file_ex(target, "include/spn.h", "include_spn_h", "u8", "u64");
  spn_target_embed_file_ex(target, "source/toolchain/toolchains.json", "toolchains_json", "u8", "u64");
  spn_target_embed_dir_ex(target, "assets/init", "init", "u8", "u64");

  add_codegen(spn, config);
  return SPN_OK;
}
