#ifndef SPN_GRAPH_GRAPH_H
#define SPN_GRAPH_GRAPH_H

#include "graph/types.h"

spn_build_graph_t* spn_bg_new();
spn_bg_id_t        spn_bg_add_file(spn_build_graph_t* graph, sp_str_t path);
spn_bg_id_t        spn_bg_add_file_ex(spn_build_graph_t* graph, sp_str_t path, spn_bg_viz_kind_t viz, sp_str_t package);
spn_bg_id_t        spn_bg_add_file_c(spn_build_graph_t* graph, spn_bg_file_config_t config);
spn_bg_id_t        spn_bg_add_fn(spn_build_graph_t* graph, spn_bg_fn_t fn, void* user_data);
spn_bg_id_t        spn_bg_add_fn_ex(spn_build_graph_t* graph, spn_bg_fn_t fn, void* user_data, spn_bg_viz_kind_t viz, sp_str_t package, sp_str_t tag);
spn_bg_id_t        spn_bg_add_fn_c(spn_build_graph_t* graph, spn_bg_cmd_config_t config);
void               spn_bg_file_set_metadata(spn_build_graph_t* graph, spn_bg_id_t id, sp_str_t package, spn_bg_viz_kind_t viz);
void               spn_bg_file_set_metadata_ex(spn_build_graph_t* graph, spn_bg_id_t id, spn_bg_node_metadata_t metadata);
void               spn_bg_cmd_set_metadata(spn_build_graph_t* graph, spn_bg_id_t id, sp_str_t tag, sp_str_t package, spn_bg_viz_kind_t viz);
void               spn_bg_cmd_set_metadata_ex(spn_build_graph_t* graph, spn_bg_id_t id, spn_bg_node_metadata_t metadata);
sp_da(spn_bg_id_t) spn_bg_find_outputs(spn_build_graph_t* graph);
spn_bg_file_t*     spn_bg_find_file(spn_build_graph_t* graph, spn_bg_id_t id);
bool               spn_bg_is_file_input(spn_bg_file_t* file);
spn_err_t          spn_bg_file_set_producer(spn_build_graph_t* g, spn_bg_id_t file, spn_bg_id_t cmd);
sp_str_t           spn_bg_file_id_to_str(spn_build_graph_t* graph, spn_bg_id_t id);
sp_str_t           spn_bg_file_to_str(spn_bg_file_t* file);
spn_bg_cmd_t*      spn_bg_find_command(spn_build_graph_t* graph, spn_bg_id_t id);
void               spn_bg_tag_command_c(spn_build_graph_t* graph, spn_bg_id_t id, const c8* tag);
void               spn_bg_cmd_set_fn(spn_build_graph_t* g, spn_bg_id_t id, spn_bg_fn_t fn, void* ud);
spn_err_t          spn_bg_cmd_add_output(spn_build_graph_t* g, spn_bg_id_t cmd, spn_bg_id_t file);
spn_err_t          spn_bg_cmd_add_input(spn_build_graph_t* g, spn_bg_id_t cmd, spn_bg_id_t file);
sp_str_t           spn_bg_cmd_id_to_str(spn_build_graph_t* graph, spn_bg_id_t id);
sp_str_t           spn_bg_cmd_to_str(spn_bg_cmd_t* cmd);
sp_str_t           spn_bg_err_to_str(spn_build_graph_t* graph, spn_bg_err_t err);
spn_bg_it_t        spn_bg_it_new(spn_bg_it_config_t config);
spn_bg_node_t      spn_bg_it_next(spn_bg_it_t* it);
void               spn_bg_it_add_children(spn_bg_it_t* it, spn_bg_node_t node);
bool               spn_bg_it_done(spn_bg_it_t* it);
sp_str_t           spn_bg_dfs(spn_bg_it_config_t config);
sp_str_t           spn_bg_bfs(spn_bg_it_config_t config);
void               spn_bg_to_mermaid(spn_build_graph_t* graph, spn_bg_dirty_t* dirty, sp_io_writer_t* stream, sp_str_t project_dir, sp_str_t cache_dir, sp_str_t work_dir, sp_str_t store_dir);
spn_bg_dirty_t*    spn_bg_dirty_new();
spn_bg_dirty_t*    spn_bg_compute_dirty(spn_build_graph_t* graph);
bool               spn_bg_is_file_dirty(spn_bg_dirty_t* dirty, spn_bg_id_t id);
bool               spn_bg_is_cmd_dirty(spn_bg_dirty_t* dirty, spn_bg_id_t id);
spn_bg_visited_t*  spn_bg_visited_new();
bool               spn_bg_visited_mark(spn_bg_visited_t* visited, spn_bg_node_t id);
spn_bg_executor_t* spn_bg_executor_new(spn_build_graph_t* graph, spn_bg_dirty_t* dirty, spn_bg_executor_config_t config);
void               spn_bg_executor_run(spn_bg_executor_t* ex);
void               spn_bg_executor_join(spn_bg_executor_t* ex);
void               spn_bg_executor_free(spn_bg_executor_t* ex);
spn_bg_dirty_t*    spn_bg_compute_forced_dirty(spn_build_graph_t* graph);
sp_str_t           spn_bg_mermaid_class_ex(sp_str_t name, sp_str_t fill, sp_str_t stroke, sp_str_t color, sp_str_t font_size);

#endif
