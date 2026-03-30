#include "err.h"
#include "app/types.h"
#include "pkg/types.h"
#include "target/types.h"

#include "graph/graph.h"
#include "session/session.h"
#include "unit/package.h"
#include "task/build/nodes/nodes.h"
#include "task/build/build.h"

static spn_err_union_t spn_bg_error_to_union(spn_build_graph_t* graph);
static spn_err_t add_link_edges(spn_session_t* session, spn_build_graph_t* graph, spn_pkg_unit_t* unit);
static spn_bg_id_t get_or_put_user_file(spn_pkg_unit_t* ctx, spn_build_graph_t* graph, sp_str_t path);
static spn_err_t add_package(spn_build_graph_t* graph, spn_pkg_unit_t* unit);

spn_err_union_t prepare_build_graph(spn_app_t* app) {
  spn_session_t* session = &app->session;
  spn_build_graph_t* graph = &session->build.graph;

  // phase 1: add each package to the graph
  sp_om_for(session->units.packages, it) {
    spn_pkg_unit_t* unit = sp_om_at(session->units.packages, it);
    if (add_package(graph, unit) != SPN_OK) {
      return spn_bg_error_to_union(graph);
    }
  }
  if (add_package(graph, spn_session_find_root(session)) != SPN_OK) {
    return spn_bg_error_to_union(graph);
  }

  // phase 2: link dependent packages
  sp_om_for(session->units.packages, it) {
    if (add_link_edges(session, graph, sp_om_at(session->units.packages, it)) != SPN_OK) {
      return spn_bg_error_to_union(graph);
    }
  }
  if (add_link_edges(session, graph, spn_session_find_root(session)) != SPN_OK) {
    return spn_bg_error_to_union(graph);
  }

  return spn_result(SPN_OK);
}

spn_err_union_t spn_bg_error_to_union(spn_build_graph_t* graph) {
  if (!graph->error.some) {
    return (spn_err_union_t) {
      .kind = SPN_ERR_BUILD_GRAPH,
      .build_graph = {
        .kind = SPN_BUILD_GRAPH_ERR_UNKNOWN,
      },
    };
  }

  switch (graph->error.value.kind) {
    case SPN_BG_OK: {
      return (spn_err_union_t) {
        .kind = SPN_ERR_BUILD_GRAPH,
        .build_graph = {
          .kind = SPN_BUILD_GRAPH_ERR_UNKNOWN,
        },
      };
    }
    case SPN_BG_ERR_MISSING_INPUT: {
      return (spn_err_union_t) {
        .kind = SPN_ERR_BUILD_GRAPH,
        .build_graph = {
          .kind = SPN_BUILD_GRAPH_ERR_MISSING_INPUT,
          .file = spn_bg_file_id_to_str(graph, graph->error.value.missing_input.file_id),
        },
      };
    }
    case SPN_BG_ERR_DUPLICATE_OUTPUT: {
      return (spn_err_union_t) {
        .kind = SPN_ERR_BUILD_GRAPH,
        .build_graph = {
          .kind = SPN_BUILD_GRAPH_ERR_DUPLICATE_OUTPUT,
          .file = spn_bg_file_id_to_str(graph, graph->error.value.duplicate_output.file),
          .command_a = spn_bg_cmd_id_to_str(graph, graph->error.value.duplicate_output.cmds.a),
          .command_b = spn_bg_cmd_id_to_str(graph, graph->error.value.duplicate_output.cmds.b),
        },
      };
    }
  }

  return (spn_err_union_t) {
    .kind = SPN_ERR_BUILD_GRAPH,
    .build_graph = {
      .kind = SPN_BUILD_GRAPH_ERR_UNKNOWN,
    },
  };
}

spn_err_t add_link_edges(spn_session_t* session, spn_build_graph_t* graph, spn_pkg_unit_t* unit) {
  spn_pkg_t* pkg = unit->ctx.pkg;
  sp_ht_for(pkg->deps, it) {
    spn_pkg_unit_t* parent = spn_session_find_pkg(session, *sp_ht_it_getkp(pkg->deps, it));
    sp_try(spn_bg_cmd_add_input(graph, unit->nodes.build.main, parent->nodes.build.stamp.package));
  }

  return SPN_OK;
}

spn_bg_id_t get_or_put_user_file(spn_pkg_unit_t* ctx, spn_build_graph_t* graph, sp_str_t path) {
  spn_bg_id_t* existing = sp_str_ht_get(ctx->nodes.files, path);
  if (existing) return *existing;

  spn_bg_id_t id = spn_bg_add_file_ex(graph, path, SPN_BG_VIZ_CMD, ctx->ctx.name);
  sp_str_ht_insert(ctx->nodes.files, path, id);
  sp_da_push(ctx->nodes.build.user, id);
  return id;
}


//                    ┌───────┐   ┌────────────────┐   ┌────────┐
//                    │ foo.c │──▶│ compile::foo.c │──▶│ foo.o  │──┐
//                    └───────┘   └────────────────┘   └────────┘  │
//                                                                 │  ┌──────┐   ┌───────────────────┐   ┌─────────────────┐
//                                                                 ├─▶│ link │──▶│ $STORE/bin/foobar │──▶│ script::package │
//                    ┌───────┐   ┌────────────────┐   ┌────────┐  │  └──────┘   └───────────────────┘   └─────────────────┘
//                    │ bar.c │──▶│ compile::bar.c │──▶│ bar.o  │──┘     ▲
//                    └───────┘   └────────────────┘   └────────┘        │
//                                                                       │
// ┌ ─ ─ ─ ─ ─ ─ ┐    ┌─────────────┐                                    │
//   user graph   ───▶│ user::exit  │────────────────────────────────────┘
// └ ─ ─ ─ ─ ─ ─ ┘    └─────────────┘
spn_err_t add_target(spn_build_graph_t* graph, spn_pkg_unit_t* pkg, spn_target_unit_t* unit) {
  spn_target_t* target = unit->info;

  unit->nodes.link = spn_bg_add_fn_ex(graph, link_target, unit, SPN_BG_VIZ_CMD, pkg->ctx.name, sp_format("link {}", SP_FMT_STR(target->name)));
  unit->nodes.output = spn_bg_add_file_ex(graph, get_target_output_path(unit), SPN_BG_VIZ_BINARY, pkg->ctx.name);

  sp_try(spn_bg_cmd_add_output(graph, unit->nodes.link,  unit->nodes.output));
  //spn_bg_cmd_add_input(graph, unit->nodes.link, pkg->nodes.build.stamp.exit);
  //spn_bg_cmd_add_input(graph, unit->nodes.compile, pkg->nodes.build.stamp.main);
  sp_try(spn_bg_cmd_add_input(graph, pkg->nodes.build.package, unit->nodes.output));

  sp_da_for(unit->objects, it) {
    spn_compile_unit_t* obj = unit->objects[it];
    sp_try(spn_bg_cmd_add_input(graph, unit->nodes.link, obj->nodes.object));
  }

  if (!sp_da_empty(target->embed)) {
    unit->nodes.embed.run = spn_bg_add_fn_ex(graph, compile_embed, unit, SPN_BG_VIZ_CMD, pkg->ctx.name, sp_format("embed::{}", SP_FMT_STR(target->name)));
    unit->nodes.embed.object = spn_bg_add_file_ex(graph, get_embed_object_path(unit), SPN_BG_VIZ_DEFAULT, pkg->ctx.name);
    unit->nodes.embed.header = spn_bg_add_file_ex(graph, get_embed_header_path(unit), SPN_BG_VIZ_DEFAULT, pkg->ctx.name);

    sp_try(spn_bg_cmd_add_output(graph, unit->nodes.embed.run, unit->nodes.embed.object));
    sp_try(spn_bg_cmd_add_output(graph, unit->nodes.embed.run, unit->nodes.embed.header));
    sp_try(spn_bg_cmd_add_input(graph, unit->nodes.embed.run, pkg->nodes.build.stamp.exit));

    sp_da_for(target->embed, it) {
      spn_embed_t* embed = &target->embed[it];

      switch (embed->kind) {
        case SPN_EMBED_FILE: {
          spn_bg_id_t input = get_or_put_user_file(pkg, graph, embed->file.path);
          sp_try(spn_bg_cmd_add_input(graph, unit->nodes.embed.run, input));
          break;
        }
        case SPN_EMBED_MEM:
        case SPN_EMBED_DIR: {
          break;
        }
      }
    }

    sp_try(spn_bg_cmd_add_input(graph, unit->nodes.link, unit->nodes.embed.object));

    sp_da_for(unit->objects, it) {
      spn_compile_unit_t* obj = unit->objects[it];
      sp_try(spn_bg_cmd_add_input(graph, obj->nodes.compile, unit->nodes.embed.header));
    }
  }

  return SPN_OK;
}

// ┌──────────┐
// │ spn.toml │──┐
// └──────────┘  │  ┌─────────────┐   ┌ ─ ─ ─ ─ ─ ─ ─ ─ ┐   ┌─────────────┐   ┌──────────────────┐
//               ├─▶│ user::main  │──▶    user graph     ──▶│ user::exit  │──▶│ script::package  │
// ┌──────────┐  │  └─────────────┘   └ ─ ─ ─ ─ ─ ─ ─ ─ ┘   └─────────────┘   └──────────────────┘
// │  spn.c   │──┘         │                                       ▲
// └──────────┘            └───────────────────────────────────────┘
//
spn_err_t add_package(spn_build_graph_t* graph, spn_pkg_unit_t* unit) {
  spn_pkg_nodes_t* nodes = &unit->nodes.build;
  spn_pkg_t* pkg = unit->ctx.pkg;

  nodes->manifest = spn_bg_add_file_ex(graph, pkg->paths.manifest, SPN_BG_VIZ_MANIFEST, unit->ctx.name);
  nodes->script = spn_bg_add_file_ex(graph, pkg->paths.script, SPN_BG_VIZ_MANIFEST, unit->ctx.name);
  nodes->package = spn_bg_add_fn_ex(graph, run_package_hook, unit, SPN_BG_VIZ_CMD, unit->ctx.name, sp_str_lit("script::package"));
  nodes->stamp.package = spn_bg_add_file_ex(graph, unit->paths.stamp.package, SPN_BG_VIZ_STAMP, unit->ctx.name);
  nodes->stamp.main = spn_bg_add_file_ex(graph, unit->paths.stamp.main, SPN_BG_VIZ_CMD, unit->ctx.name);
  nodes->stamp.exit = spn_bg_add_file_ex(graph, unit->paths.stamp.exit, SPN_BG_VIZ_CMD, unit->ctx.name);
  nodes->main = spn_bg_add_fn(graph, stamp_enter, unit);
  nodes->exit = spn_bg_add_fn(graph, stamp_exit, unit);

  sp_try(spn_bg_cmd_add_input(graph, nodes->main, nodes->manifest));
  sp_try(spn_bg_cmd_add_input(graph, nodes->main, nodes->script));
  sp_try(spn_bg_cmd_add_output(graph, nodes->main, nodes->stamp.main));
  sp_try(spn_bg_cmd_add_input(graph, nodes->exit, nodes->stamp.main));
  sp_try(spn_bg_cmd_add_output(graph, nodes->exit, nodes->stamp.exit));
  sp_try(spn_bg_cmd_add_input(graph, nodes->package, nodes->stamp.exit));
  sp_try(spn_bg_cmd_add_output(graph, nodes->package, nodes->stamp.package));

  // user nodes
  // pass 1: create all command nodes
  sp_da_for(unit->nodes.all, it) {
    spn_user_node_t* node = &unit->nodes.all[it];
    node->id = spn_bg_add_fn_ex(graph, run_user_fn, node, SPN_BG_VIZ_CMD, unit->ctx.name, node->tag);
    sp_da_push(unit->nodes.build.user, node->id);
  }

  // pass 2: create all file in/outputs + create a stamp file for phonies
  sp_da_for(unit->nodes.all, it) {
    spn_user_node_t* node = &unit->nodes.all[it];

    // if no outputs, depend on the exit node's stamp
    if (sp_da_empty(node->outputs)) {
      sp_da_push(node->outputs, spn_pkg_unit_get_node_stamp_file(unit, node));
    }

    sp_da_for(node->outputs, o) {
      spn_bg_id_t file = get_or_put_user_file(unit, graph, node->outputs[o]);
      sp_try(spn_bg_cmd_add_output(graph, node->id, file));
      sp_try(spn_bg_cmd_add_input(graph, nodes->exit, file));
    }

    // if no inputs, depend on the main node's stamp
    if (sp_da_empty(node->inputs) && sp_da_empty(node->deps)) {
      sp_try(spn_bg_cmd_add_input(graph, node->id, nodes->stamp.main));
    }

    sp_da_for(node->inputs, i) {
      spn_bg_id_t file = get_or_put_user_file(unit, graph, node->inputs[i]);
      sp_try(spn_bg_cmd_add_input(graph, node->id, file));
    }
  }

  // pass 3: now that all file nodes exist, set up links for command inputs
  sp_da_for(unit->nodes.all, it) {
    spn_user_node_t* node = &unit->nodes.all[it];

    // depend on the outputs of your command inputs
    sp_da_for(node->deps, dit) {
      spn_user_node_t* dep = spn_find_user_node(node->deps[dit]);
      sp_da_for(dep->outputs, oit) {
        spn_bg_id_t output = get_or_put_user_file(unit, graph, dep->outputs[oit]);
        sp_try(spn_bg_cmd_add_input(graph, node->id, output));
      }
    }
  }

  // object files
  // @spader gate this on user node exit?
  sp_om_for(unit->objects, it) {
    spn_compile_unit_t* obj = sp_om_at(unit->objects, it);
    obj->nodes.source = spn_bg_add_file_ex(graph, obj->paths.source, SPN_BG_VIZ_DEFAULT, pkg->name);
    obj->nodes.compile = spn_bg_add_fn_ex(graph, compile_object, obj, SPN_BG_VIZ_CMD, pkg->name, sp_format("compile::{}", SP_FMT_STR(obj->paths.source)));
    obj->nodes.object = spn_bg_add_file_ex(graph, obj->paths.object, SPN_BG_VIZ_DEFAULT, pkg->name);
    sp_try(spn_bg_cmd_add_output(graph, obj->nodes.compile, obj->nodes.object));
    sp_try(spn_bg_cmd_add_input(graph, obj->nodes.compile, obj->nodes.source));
    sp_try(spn_bg_cmd_add_input(graph, obj->nodes.compile, unit->nodes.build.stamp.exit));
  }

  sp_om_for(unit->targets, it) {
    spn_target_unit_t* target = sp_om_at(unit->targets, it);

    switch (target->info->kind) {
      case SPN_TARGET_STATIC_LIB:
      case SPN_TARGET_SHARED_LIB: {
        if (!sp_da_empty(target->info->source)) {
          sp_try(add_target(graph, unit, target));
        }
        break;
      }
      case SPN_TARGET_OBJECT:
      case SPN_TARGET_EXE:
      case SPN_TARGET_JIT: {
        sp_try(add_target(graph, unit, target));
        break;
      }
      case SPN_TARGET_NONE: {
        break;
      }
    }
  }

  return SPN_OK;
}

