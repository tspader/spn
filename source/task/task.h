#ifndef SPN_TASK_H
#define SPN_TASK_H

#include "cli/types.h"
#include "forward/types.h"
#include "task/types.h"

void spn_task_enqueue(spn_task_executor_t* ex, s32 kind);

///////////
// TASKS //
///////////
spn_task_result_t spn_task_resolve(spn_app_t* app);
spn_task_result_t spn_task_sync_init(spn_app_t* app);
spn_task_result_t spn_task_sync_update(spn_app_t* app);
spn_task_result_t spn_task_init_configure_graph(spn_app_t* app);
spn_task_result_t spn_task_update_configure_graph(spn_app_t* app);
void              spn_task_init_build_graph(spn_app_t* app);
spn_task_result_t spn_task_run_build_graph(spn_app_t* app);
spn_task_result_t spn_task_create_units(spn_app_t* app);
spn_task_result_t spn_task_graph(spn_app_t* app);
spn_task_result_t spn_task_run(spn_app_t* app);
spn_task_result_t spn_task_run_tests(spn_app_t* app);
spn_task_result_t spn_task_generate(spn_app_t* app);
spn_task_result_t spn_task_which(spn_app_t* app);
spn_task_result_t spn_task_update(spn_app_t* app);
spn_task_result_t spn_task_init(spn_app_t* app);
spn_task_result_t spn_task_add(spn_app_t* app);
spn_task_result_t spn_task_clean(spn_app_t* app);
spn_task_result_t spn_task_publish(spn_app_t* app);

#endif
