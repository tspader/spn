#include "task/build/dag.h"

#include "app/app.h"
#include "session/session.h"
#include "spn.h"
#include "task/task.h"

static spn_err_t spn_dag_build_describe(spn_dag_build_t* build) {
  (void)build;
  return SPN_OK;
}

static spn_err_t spn_dag_build_run(spn_dag_build_t* build) {
  (void)build;
  return SPN_OK;
}

static spn_err_t spn_dag_build_report(spn_dag_build_t* build) {
  (void)build;
  return SPN_OK;
}

spn_task_step_t spn_dag_build_init(spn_app_t* app) {
  spn_session_t* session = &app->session;

  spn_dag_build_t* build = sp_alloc_type(session->mem, spn_dag_build_t);
  build->session = session;
  build->mem = session->mem;
  session->dag = build;

  if (spn_dag_build_describe(build)) {
    return spn_task_fail(SPN_ERROR);
  }
  if (spn_dag_build_run(build)) {
    return spn_task_fail(SPN_ERROR);
  }
  return spn_task_continue();
}

spn_task_step_t spn_dag_build_update(spn_app_t* app) {
  spn_dag_build_t* build = app->session.dag;
  if (spn_dag_build_report(build)) {
    return spn_task_fail(SPN_ERROR);
  }
  return spn_task_done();
}
