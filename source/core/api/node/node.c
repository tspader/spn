#include "sp.h"
#include "macro/macro.h"
#include "spn.h"

#include "api/api.h"
#include "api/types.h"
#include "ctx/types.h"
#include "event/types.h"
#include "session/types.h"
#include "unit/types.h"

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
  sp_da_init(mem, node.output_dirs);
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

void spn_node_add_output(spn_node_t* node, const c8* output) {
  spn_user_node_t* info = spn_node_deref(node->ref);
  SPN_API_LOG(node->ref.pkg, "spn_node_add_output", "{}, {}", SP_FMT_STR(info->tag), SP_FMT_CSTR(output));
  spn_path_t made = spn_api_tree_path(node->ref.pkg, "spn_node_add_output", output);
  if (spn_path_empty(made)) {
    return;
  }
  sp_da_push(info->outputs, made);
}

static bool output_dir_rejected(spn_pkg_unit_t* unit, spn_path_t dir) {
  if (!spn_path_within(dir, unit->paths.include).within) {
    return false;
  }
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  sp_str_t message = sp_fmt(scratch.mem, "spn_node_add_output_dir: {} contains the include tree", SP_FMT_STR(spn_path_str(&spn.roots, scratch.mem, dir))).value;
  bool trapped = spn_wasm_trap_active(unit, message);
  sp_mem_end_scratch(scratch);
  sp_assert(trapped);
  return true;
}

void spn_node_add_output_dir(spn_node_t* node, const c8* dir) {
  spn_user_node_t* info = spn_node_deref(node->ref);
  SPN_API_LOG(node->ref.pkg, "spn_node_add_output_dir", "{}, {}", SP_FMT_STR(info->tag), SP_FMT_CSTR(dir));
  spn_path_t made = spn_api_tree_path(node->ref.pkg, "spn_node_add_output_dir", dir);
  if (spn_path_empty(made) || output_dir_rejected(node->ref.pkg, made)) {
    return;
  }
  sp_da_push(info->output_dirs, made);
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
