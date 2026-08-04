#include "app/app.h"
#include "ctx/types.h"
#include "task/build/dag.h"
#include "task/task.h"

spn_task_step_t spn_task_build_graph_init(spn_ctx_t* ctx) {
  return spn_dag_build_init(ctx->app);
}

spn_task_step_t spn_task_build_graph_update(spn_ctx_t* ctx) {
  return spn_dag_build_update(ctx->app);
}
