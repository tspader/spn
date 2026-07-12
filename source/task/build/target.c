#include "task/build/target.h"

#include "graph/graph.h"
#include "task/build/build.h"
#include "task/build/nodes/nodes.h"

spn_err_t spn_build_add_object_nodes(spn_build_graph_t* graph, spn_target_unit_t* target) {
  sp_da_for(target->objects, it) {
    spn_compile_unit_t* object = target->objects[it];
    if (object->nodes.compile.occupied) {
      continue;
    }

    object->nodes.source = spn_bg_add_file(graph, object->paths.file);
    object->nodes.compile = spn_bg_add_fn(graph, compile_object, object);
    object->nodes.object = spn_bg_add_file(graph, object->paths.object);
    spn_try(spn_bg_cmd_add_input(graph, object->nodes.compile, object->nodes.source));
    spn_try(spn_bg_cmd_add_output(graph, object->nodes.compile, object->nodes.object));
  }
  return SPN_OK;
}

spn_err_t spn_build_add_target_nodes(spn_build_graph_t* graph, spn_target_unit_t* target) {
  spn_try(spn_build_add_object_nodes(graph, target));
  if (target->nodes.link.occupied) {
    return SPN_OK;
  }

  target->nodes.link = spn_bg_add_fn(graph, link_target, target);
  target->nodes.output = spn_bg_add_file(graph, get_target_output_path(target->session->mem, target));
  spn_try(spn_bg_cmd_add_output(graph, target->nodes.link, target->nodes.output));
  sp_da_for(target->objects, it) {
    spn_try(spn_bg_cmd_add_input(graph, target->nodes.link, target->objects[it]->nodes.object));
  }
  return SPN_OK;
}
