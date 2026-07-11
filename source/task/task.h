#ifndef SPN_TASK_H
#define SPN_TASK_H

#include "cli/types.h"
#include "error/types.h"
#include "forward/types.h"
#include "task/types.h"

spn_task_desc_t* spn_task_get(spn_task_kind_t kind);
void             spn_task_enqueue(spn_task_executor_t* ex, spn_task_kind_t kind);
bool             spn_task_rewind(spn_task_executor_t* ex, spn_task_kind_t kind);
sp_cli_result_t  spn_task_plan_kinds(const spn_task_kind_t* kinds, u32 len);

#define spn_plan(...) \
  spn_task_plan_kinds( \
    (spn_task_kind_t[]) { __VA_ARGS__ }, \
    sp_carr_len(((spn_task_kind_t[]) { __VA_ARGS__ })) \
  )

///////////
// TASKS //
///////////
spn_task_step_t spn_task_sync_indexes_init(spn_app_t* app);
spn_task_step_t spn_task_sync_indexes_update(spn_app_t* app);
spn_task_step_t spn_task_resolve(spn_app_t* app);
spn_task_step_t spn_task_sync_packages_init(spn_app_t* app);
spn_task_step_t spn_task_sync_packages_update(spn_app_t* app);
spn_task_step_t spn_task_configure_graph_init(spn_app_t* app);
spn_task_step_t spn_task_configure_graph_update(spn_app_t* app);
spn_task_step_t spn_task_create_units(spn_app_t* app);
spn_err_union_t spn_task_create_script_units(spn_session_t* session);
spn_task_step_t spn_task_build_graph_init(spn_app_t* app);
spn_task_step_t spn_task_build_graph_update(spn_app_t* app);
spn_task_step_t spn_task_render_graph(spn_app_t* app);
spn_task_step_t spn_task_run(spn_app_t* app);
spn_task_step_t spn_task_generate(spn_app_t* app);
spn_task_step_t spn_task_which(spn_app_t* app);
spn_task_step_t spn_task_update(spn_app_t* app);
spn_task_step_t spn_task_init(spn_app_t* app);
spn_task_step_t spn_task_add(spn_app_t* app);
spn_task_step_t spn_task_clean(spn_app_t* app);
spn_task_step_t spn_task_publish(spn_app_t* app);

#endif
