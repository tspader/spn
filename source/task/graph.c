#include "app.h"

#include "gen.h"
#include "graph.h"
#include "external/cc.h"
#include "sp/io.h"
#include "sp/macro.h"
#include "sp/str.h"

void spn_bg_render_file_to_builder(sp_str_builder_t* builder, u32 id, sp_str_t file) {
  sp_str_t prefix = SP_ZERO_INITIALIZE();
  if (sp_str_starts_with(file, spn.paths.cache)) {
    prefix = sp_str_lit("$CACHE");
    file = sp_str_strip_left(file, spn.paths.cache);
  }
  else if (sp_str_starts_with(file, spn.paths.project)) {
    prefix = sp_str_lit("$ROOT");
    file = sp_str_strip_left(file, spn.paths.project);
  }

  sp_mem_scratch_t scratch = sp_mem_begin_scratch();
  if (!sp_str_empty(prefix)) {
    file = sp_str_concat(prefix, file);
  }

  sp_str_builder_new_line(builder);
  sp_str_builder_append_fmt(builder, "F{}({})",
    SP_FMT_U32(id),
    SP_FMT_QSTR(file)
  );

  sp_mem_end_scratch(scratch);
}

void spn_bg_render_file_id_to_builder(spn_build_graph_t* graph, sp_str_builder_t* builder, spn_bg_id_t id) {
  spn_bg_file_t* file = spn_bg_find_file(graph, id);
  spn_bg_render_file_to_builder(builder, id.index, file->path);
}

void spn_bg_render_cmd_id_to_builder(spn_build_graph_t* graph, sp_str_builder_t* builder, spn_bg_id_t id) {
  spn_bg_cmd_t* cmd = spn_bg_find_command(graph, id);

  sp_str_builder_new_line(builder);
  sp_str_builder_append_fmt(builder, "C{}({})",
    SP_FMT_U32(cmd->id.index),
    SP_FMT_QSTR(cmd->tag)
  );
}

void spn_bg_begin_subgraph(sp_str_builder_t* builder, sp_str_t id, sp_str_t name) {
  sp_str_builder_new_line(builder);
  sp_str_builder_append_fmt(builder, "subgraph {}[{}]", SP_FMT_STR(id), SP_FMT_QSTR(name));
  sp_str_builder_indent(builder);
}

void spn_bg_end_subgraph(sp_str_builder_t* builder) {
  sp_str_builder_dedent(builder);
  sp_str_builder_new_line(builder);
  sp_str_builder_append_cstr(builder, "end");
}

void spn_bg_render_pkg_to_mermaid(spn_session_t* b, spn_bg_dirty_t* dirty, spn_pkg_unit_t* unit, sp_io_writer_t* io) {
  spn_build_ctx_t* ctx = &unit->ctx;
  spn_build_graph_t* graph = &b->build.graph;
  spn_pkg_nodes_t* nodes = &unit->nodes.build;

  sp_str_builder_t builder = {
    .writer = io,
    .indent = {
      .level = 1,
      .word = sp_str_lit("  ")
    }
  };

  spn_bg_begin_subgraph(&builder, ctx->name, sp_str_lit(" "));
  // sp_str_builder_new_line(&builder);
  // sp_str_builder_append_fmt(&builder, "T_{}({}):::title", SP_FMT_STR(ctx->name), SP_FMT_QSTR(ctx->name));
  spn_bg_render_file_id_to_builder(graph, &builder, nodes->manifest);
  spn_bg_render_file_id_to_builder(graph, &builder, nodes->script);
  spn_bg_render_cmd_id_to_builder(graph, &builder, nodes->package);
  spn_bg_render_file_id_to_builder(graph, &builder, nodes->stamp.package);

  // spn_bg_begin_subgraph(&builder, sp_format("{}::user", SP_FMT_STR(ctx->name)), sp_str_lit(" "));
  // sp_str_builder_new_line(&builder);
  // sp_str_builder_append_fmt(&builder, "T_{}_user(\"user\"):::title", SP_FMT_STR(ctx->name));
  spn_bg_render_cmd_id_to_builder(graph, &builder, nodes->main);
  spn_bg_render_file_id_to_builder(graph, &builder, nodes->stamp.main);
  spn_bg_render_cmd_id_to_builder(graph, &builder, nodes->exit);
  spn_bg_render_file_id_to_builder(graph, &builder, nodes->stamp.exit);

  sp_da_for(unit->nodes.all, it) {
    spn_user_node_t* node = &unit->nodes.all[it];
    spn_bg_render_cmd_id_to_builder(graph, &builder, node->id);
  }

  sp_str_ht_for(unit->nodes.files, it) {
    spn_bg_id_t file_id = *sp_str_ht_it_getp(unit->nodes.files, it);
    spn_bg_render_file_id_to_builder(graph, &builder, file_id);
  }

  // spn_bg_begin_subgraph(&builder, sp_format("{}::{}", SP_FMT_STR(ctx->name), SP_FMT_CSTR("source")), sp_str_lit("source"));
  // sp_om_for(unit->targets, it) {
  //   spn_target_unit_t* target = sp_om_at(unit->targets, it);
  //   sp_da_for(target->nodes.source, s) {
  //     spn_bg_render_file_id_to_builder(graph, &builder, target->nodes.source[s]);
  //   }
  // }
  //
  // spn_bg_end_subgraph(&builder);

  sp_om_for(unit->objects, it) {
    spn_compile_unit_t* object = sp_om_at(unit->objects, it);
    spn_bg_render_file_id_to_builder(graph, &builder, object->nodes.source);
    spn_bg_render_cmd_id_to_builder(graph, &builder, object->nodes.compile);
    spn_bg_render_file_id_to_builder(graph, &builder, object->nodes.object);
  }

  sp_om_for(unit->targets, it) {
    spn_target_unit_t* target = sp_om_at(unit->targets, it);

    // spn_bg_begin_subgraph(&builder, sp_format("{}::{}", SP_FMT_STR(ctx->name), SP_FMT_STR(target->target->name)), sp_str_lit(" "));
      // sp_str_builder_new_line(&builder);
      // sp_str_builder_append_fmt(&builder, "T_{}_{}({}):::title", SP_FMT_STR(ctx->name), SP_FMT_STR(target->target->name), SP_FMT_QSTR(target->target->name));
      spn_bg_render_cmd_id_to_builder(graph, &builder, target->nodes.link);
      spn_bg_render_file_id_to_builder(graph, &builder, target->nodes.output);
      sp_da_for(target->nodes.source, s) {
        spn_bg_render_file_id_to_builder(graph, &builder, target->nodes.source[s]);
      }
    // spn_bg_end_subgraph(&builder);
  }
  // spn_bg_end_subgraph(&builder);
  spn_bg_end_subgraph(&builder);

  sp_str_builder_new_line(&builder);
}

void spn_render_to_mermaid(spn_app_t* app, sp_io_writer_t* io) {
  struct {
    sp_str_t text;
    sp_str_t stroke;
    sp_str_t link;
    sp_str_t clean;
    sp_str_t dirty;
    sp_str_t outer_bg;
    sp_str_t inner_bg;
  } color = {
    .text     = sp_str_lit("#ffffff"),
    .stroke   = sp_str_lit("#101010"),
    .link     = sp_str_lit("#505050"),
    .clean    = sp_str_lit("#3d633d"),
    .dirty    = sp_str_lit("#694141"),
    .outer_bg = sp_str_lit("#191924"),
    .inner_bg = sp_str_lit("#28283b"),
  };

  sp_io_write_str(io, sp_str_lit("%%{init: {'theme': 'base', 'themeVariables': { 'fontSize': '96px', 'fontFamily': 'monospace'}}}%%\n"));
  sp_io_write_str(io, sp_str_lit("graph TD\n"));

  sp_io_write_str(io, spn_bg_mermaid_class_ex(sp_str_lit("dirty"), color.dirty, color.stroke, color.text, sp_str_lit("32px")));
  sp_io_write_str(io, spn_bg_mermaid_class_ex(sp_str_lit("clean"), color.clean, color.stroke, color.text, sp_str_lit("32px")));
  sp_io_write_str(io, sp_format("  classDef title fill:{},stroke-width:0,color:{},font-size:96px\n",
    SP_FMT_STR(color.outer_bg), SP_FMT_STR(color.text)));
  sp_io_write_str(io, sp_format("  linkStyle default stroke:{},stroke-width:2px\n", SP_FMT_STR(color.link)));

  spn_session_t* session = &app->session;
  spn_build_graph_t* graph = &session->build.graph;
  spn_bg_dirty_t* dirty = spn_bg_compute_dirty(graph);

  // packages
  sp_om_for(session->units.packages, it) {
    spn_pkg_unit_t* unit = sp_om_at(session->units.packages, it);
    spn_bg_render_pkg_to_mermaid(session, dirty, unit, io);
  }

  spn_bg_render_pkg_to_mermaid(session, dirty, spn_session_find_root(session), io);

  // edges
  sp_da_for(graph->commands, it) {
    spn_bg_cmd_t* cmd = &graph->commands[it];

    sp_da_for(cmd->consumes, i) {
      u32 id = cmd->consumes[i].index;
      sp_io_write_str(io, sp_format("  F{} --> C{}\n",
        SP_FMT_U32(id), SP_FMT_U32(cmd->id.index)));
    }

    sp_da_for(cmd->produces, i) {
      u32 id = cmd->produces[i].index;
      sp_io_write_str(io, sp_format("  C{} --> F{}\n",
        SP_FMT_U32(cmd->id.index), SP_FMT_U32(id)));
    }
  }

  // subgraph styles
  sp_om_for(session->units.packages, it) {
    spn_pkg_unit_t* unit = sp_om_at(session->units.packages, it);
    sp_str_t name = unit->ctx.name;
    sp_io_write_str(io, sp_format("  style {} fill:{},stroke:{},color:{}\n",
      SP_FMT_STR(name), SP_FMT_STR(color.outer_bg), SP_FMT_STR(color.stroke), SP_FMT_STR(color.text)));
    sp_io_write_str(io, sp_format("  style {}::user fill:{},stroke:{}\n",
      SP_FMT_STR(name), SP_FMT_STR(color.inner_bg), SP_FMT_STR(color.outer_bg)));
    sp_om_for(unit->targets, t) {
      spn_target_unit_t* target = sp_om_at(unit->targets, t);
      sp_io_write_str(io, sp_format("  style {}::{} fill:{},stroke:{}\n",
        SP_FMT_STR(name), SP_FMT_STR(target->info->name), SP_FMT_STR(color.inner_bg), SP_FMT_STR(color.outer_bg)));
    }
  }

  spn_pkg_unit_t* root = spn_session_find_root(session);
  sp_str_t root_name = root->ctx.name;
  sp_io_write_str(io, sp_format("  style {} fill:{},stroke:{},color:{}\n",
    SP_FMT_STR(root_name), SP_FMT_STR(color.outer_bg), SP_FMT_STR(color.stroke), SP_FMT_STR(color.text)));
  sp_io_write_str(io, sp_format("  style {}::user fill:{},stroke:{}\n",
    SP_FMT_STR(root_name), SP_FMT_STR(color.inner_bg), SP_FMT_STR(color.outer_bg)));
  sp_om_for(root->targets, t) {
    spn_target_unit_t* target = sp_om_at(root->targets, t);
    sp_io_write_str(io, sp_format("  style {}::{} fill:{},stroke:{}\n",
      SP_FMT_STR(root_name), SP_FMT_STR(target->info->name), SP_FMT_STR(color.inner_bg), SP_FMT_STR(color.outer_bg)));
  }

  sp_da_for(graph->commands, it) {
    spn_bg_cmd_t* cmd = &graph->commands[it];
    sp_io_write_str(io, sp_format("class C{} {}",
      SP_FMT_U32(cmd->id.index),
      SP_FMT_CSTR(spn_bg_is_cmd_dirty(dirty, cmd->id) ? "dirty" : "clean")
    ));
    sp_io_write_new_line(io);

  }

  sp_da_for(graph->files, it) {
    spn_bg_file_t* file = &graph->files[it];
    sp_io_write_str(io, sp_format("class F{} {}",
      SP_FMT_U32(file->id.index),
      SP_FMT_CSTR(spn_bg_is_file_dirty(dirty, file->id) ? "dirty" : "clean")
    ));
    sp_io_write_new_line(io);
  }
}

spn_task_result_t spn_task_graph(spn_app_t* app) {
  spn_bg_dirty_t* dirty = spn.cli.graph.dirty ? spn_bg_compute_dirty(&app->session.build.graph) : NULL;
  spn_pkg_unit_t* root = spn_session_find_root(&app->session);
  sp_str_t work_dir = root->ctx.paths.work;
  sp_str_t store_dir = root->ctx.paths.store;

  if (sp_str_valid(spn.cli.graph.output)) {
    sp_io_writer_t writer = sp_io_writer_from_file(spn.cli.graph.output, SP_IO_WRITE_MODE_OVERWRITE);
    //spn_bg_to_mermaid(&app->builder.build.graph, dirty, &writer, app->paths.dir, spn.paths.cache, work_dir, store_dir);
    spn_render_to_mermaid(app, &writer);
    sp_io_writer_close(&writer);
  }
  else {
    //spn_bg_to_mermaid(&app->builder.build.graph, dirty, &spn.logger.out, app->paths.dir, spn.paths.cache, work_dir, store_dir);
    spn_render_to_mermaid(app, &spn.logger.out);
  }

  return SPN_TASK_DONE;
}
