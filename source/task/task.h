#ifndef SPN_TASK_H
#define SPN_TASK_H

#include "sp.h"
#include "graph.h"

typedef struct spn_app_t spn_app_t;

typedef enum {
  SPN_TASK_KIND_NONE,
  SPN_TASK_KIND_RESOLVE,
  SPN_TASK_KIND_SYNC,
  SPN_TASK_KIND_CONFIGURE_V2,
  SPN_TASK_KIND_PREPARE_BUILD_GRAPH_V2,
  SPN_TASK_KIND_PREPARE_BUILD_GRAPH,
  SPN_TASK_KIND_RUN_BUILD_GRAPH,
  SPN_TASK_KIND_RENDER_BUILD_GRAPH,
  SPN_TASK_KIND_RUN,
  SPN_TASK_KIND_GENERATE,
  SPN_TASK_KIND_WHICH,
  SPN_TASK_KIND_COUNT,
} spn_task_kind_t;

typedef enum {
  SPN_TASK_CONTINUE,
  SPN_TASK_DONE,
  SPN_TASK_ERROR,
} spn_task_result_t;

#define SPN_TASK_MAX_QUEUE 32

typedef struct {
  s32 data[SPN_TASK_MAX_QUEUE];
  u32 len;
  u32 index;
  bool initted;
} spn_task_executor_t;

void spn_task_enqueue(spn_task_executor_t* ex, s32 kind);


spn_task_result_t spn_task_resolve(spn_app_t* app);
void              spn_task_sync_init(spn_app_t* app);
spn_task_result_t spn_task_sync_update(spn_app_t* app);
spn_task_result_t spn_task_prepare_configure_graph(spn_app_t* app);
spn_task_result_t spn_task_update_configure_graph(spn_app_t* app);
void              spn_task_run_build_graph_init(spn_app_t* app);
spn_task_result_t spn_task_run_build_graph_update(spn_app_t* app);
spn_task_result_t spn_task_prepare_build_graph_v2(spn_app_t* app);
spn_task_result_t spn_task_graph(spn_app_t* app);
spn_task_result_t spn_task_run_tests(spn_app_t* app);
spn_task_result_t spn_task_generate(spn_app_t* app);
spn_task_result_t spn_task_which(spn_app_t* app);

#endif
