#pragma once

#include "spn_test.h"

#include "sp/sp_graph.h"

#define GRAPH_MAX_NODES 16
#define GRAPH_MAX_EDGES 6
#define GRAPH_MAX_TOUCH 16

typedef enum {
  NODE_KIND_FILE,
  NODE_KIND_COMMAND,
} node_kind_t;

typedef struct {
  const c8* id;
  node_kind_t kind;
  const c8* inputs [GRAPH_MAX_EDGES];
  const c8* outputs [GRAPH_MAX_EDGES];
} graph_node_t;

typedef struct {
  graph_node_t nodes [GRAPH_MAX_NODES];
} graph_def_t;

typedef struct {
  spn_bg_id_t handle;
  node_kind_t kind;
} graph_ref_t;

typedef struct {
  spn_build_graph_t* graph;
  const graph_def_t* def;
  sp_str_ht(graph_ref_t) refs;
} built_graph_t;

extern const graph_def_t simple_linear_graph;
extern const graph_def_t long_linear_graph;
extern const graph_def_t fork_join_graph;
extern const graph_def_t split_join_graph;
extern const graph_def_t diamond_graph;
extern const graph_def_t asymmetric_fork_graph;
extern const graph_def_t multi_output_graph;
extern const graph_def_t comb_graph;
extern const graph_def_t no_input_graph;

sp_err_t graph_setup(sp_test_t* t);
built_graph_t build_graph(sp_test_t* t, const graph_def_t* def);
graph_ref_t* graph_ref(built_graph_t* b, const c8* id);
void touch_file(sp_str_t path);
void touch_node(built_graph_t* b, const c8* id);
void apply_touches(built_graph_t* b, const c8* const* touch);
