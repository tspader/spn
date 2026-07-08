#include "app/app.h"
#include "task/task.h"

spn_task_step_t spn_task_generate(spn_app_t* app) {
  (void)app;
  return spn_task_done();
}
