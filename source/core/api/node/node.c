#include "sp.h"
#include "macro/macro.h"
#include "spn.h"

#include "api/api.h"
#include "api/types.h"
#include "ctx/types.h"
#include "event/types.h"
#include "session/types.h"
#include "unit/types.h"

#include "enum/enum.h"
#include "event/event.h"
#include "external/wasm/wasm.h"
#include "paths/paths.h"
#include "unit/unit.h"
#include "intern/intern.h"

spn_node_t* spn_add_node(spn_config_t* config, const c8* tag) {
  spn_pkg_unit_t* unit = spn_api_unit(config);
  SPN_API_LOG(unit, "spn_add_node", "{}", SP_FMT_CSTR(tag));

  sp_mem_t mem = spn.mem;
  u32 index = sp_da_size(unit->user_nodes);
  spn_user_node_t node = {
    .pkg = unit,
    .tag = spn_intern_cstr(tag),
  };
  sp_da_init(mem, node.inputs);
  sp_da_init(mem, node.outputs);
  sp_da_init(mem, node.deps);
  sp_da_push(unit->user_nodes, node);

  spn_node_t* out = sp_alloc_type(mem, spn_node_t);
  *out = (spn_node_t) {
    .ref = {
      .pkg = unit,
      .index = index,
    },
  };

  return out;
}

void spn_node_add_input(spn_node_t* node, const c8* input) {
  spn_user_node_t* info = spn_node_deref(node->ref);
  SPN_API_LOG(node->ref.pkg, "spn_node_add_input", "{}, {}", SP_FMT_STR(info->tag), SP_FMT_CSTR(input));
  spn_path_t made = spn_api_tree_path(node->ref.pkg, "spn_node_add_input", input);
  if (spn_path_empty(made)) {
    return;
  }
  sp_da_push(info->inputs, made);
}

typedef enum {
  OUTPUT_OK,
  OUTPUT_UNNAMED,
  OUTPUT_ABSOLUTE,
  OUTPUT_ROOT,
} output_problem_t;

static output_problem_t output_problem(spn_dir_t dir, sp_str_t sub) {
  if (sp_str_empty(sub)) {
    return OUTPUT_UNNAMED;
  }
  if (sp_fs_is_absolute(sub)) {
    return OUTPUT_ABSOLUTE;
  }
  switch (dir) {
    case SPN_DIR_INCLUDE:
    case SPN_DIR_VENDOR:
    case SPN_DIR_LIB:
    case SPN_DIR_SOURCE:
    case SPN_DIR_WORK:     return OUTPUT_OK;
    case SPN_DIR_NONE:
    case SPN_DIR_CACHE:
    case SPN_DIR_STORE:
    case SPN_DIR_PROJECT:
    case SPN_DIR_MANIFEST: return OUTPUT_ROOT;
  }
  SP_UNREACHABLE_RETURN(OUTPUT_ROOT);
}

static sp_str_t output_problem_str(output_problem_t problem) {
  switch (problem) {
    case OUTPUT_OK:       return sp_str_lit("");
    case OUTPUT_UNNAMED:  return sp_str_lit("must name a path under its root");
    case OUTPUT_ABSOLUTE: return sp_str_lit("must be relative to its root");
    case OUTPUT_ROOT:     return sp_str_lit("must be rooted at include, vendor, lib, source, or work");
  }
  SP_UNREACHABLE_RETURN(sp_str_lit(""));
}

static void add_output(spn_node_t* node, const c8* fn, spn_dir_t dir, const c8* path, spn_dag_artifact_kind_t kind) {
  spn_user_node_t* info = spn_node_deref(node->ref);
  spn_pkg_unit_t* unit = node->ref.pkg;
  sp_str_t sub = sp_str_view(path);
  SPN_API_LOG(unit, fn, "{}, {}, {}", SP_FMT_STR(info->tag), SP_FMT_STR(spn_dir_to_str(dir)), SP_FMT_STR(sub));
  if (spn_api_path_rejected(unit, fn, sub)) {
    return;
  }
  output_problem_t problem = output_problem(dir, sub);
  if (problem != OUTPUT_OK) {
    sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
    sp_str_t message = sp_fmt(scratch.mem, "{}: {}/{} {}", SP_FMT_CSTR(fn), SP_FMT_STR(spn_dir_to_str(dir)), SP_FMT_STR(sub), SP_FMT_STR(output_problem_str(problem))).value;
    bool trapped = spn_wasm_trap_active(unit, message);
    sp_mem_end_scratch(scratch);
    sp_assert(trapped);
    return;
  }
  sp_da_push(info->outputs, ((spn_user_output_t) {
    .dir = dir,
    .sub = sp_str_copy(spn.mem, sub),
    .kind = kind,
    .path = spn_path_join(spn.mem, spn_api_dir_path(unit, dir), sub),
  }));
}

void spn_node_add_output(spn_node_t* node, spn_dir_t dir, const c8* path) {
  add_output(node, "spn_node_add_output", dir, path, SPN_DAG_ARTIFACT_KIND_FILE);
}

void spn_node_add_output_dir(spn_node_t* node, spn_dir_t dir, const c8* path) {
  add_output(node, "spn_node_add_output_dir", dir, path, SPN_DAG_ARTIFACT_KIND_TREE);
}

void spn_node_link(spn_node_t* from, spn_node_t* to) {
  spn_user_node_t* info = spn_node_deref(to->ref);
  SPN_API_LOG(to->ref.pkg, "spn_node_link", "{} -> {}", SP_FMT_STR(spn_node_deref(from->ref)->tag), SP_FMT_STR(info->tag));
  sp_da_push(info->deps, from->ref);
}

void spn_node_set_fn(spn_node_t* node, const c8* fn) {
  spn_user_node_t* info = spn_node_deref(node->ref);
  SPN_API_LOG(node->ref.pkg, "spn_node_set_fn", "{}, {}", SP_FMT_STR(info->tag), SP_FMT_CSTR(fn));
  info->fn = spn_intern_cstr(fn);
}
