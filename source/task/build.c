#include "app/app.h"
#include "task/build/dag.h"
#include "task/task.h"

spn_task_step_t spn_task_build_graph_init(spn_app_t* app) {
  return spn_dag_build_init(app);
}

spn_task_step_t spn_task_build_graph_update(spn_app_t* app) {
  return spn_dag_build_update(app);
}
