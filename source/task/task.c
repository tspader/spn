#include "app/types.h"
#include "task/task.h"

static spn_task_desc_t spn_tasks[SPN_TASK_COUNT] = {
  [SPN_TASK_SYNC_INDEXES]    = { "sync-indexes",  spn_task_sync_indexes_init,    spn_task_sync_indexes_update    },
  [SPN_TASK_RESOLVE]         = { "resolve",       SP_NULLPTR,                    spn_task_resolve                },
  [SPN_TASK_SYNC_PACKAGES]   = { "sync-packages", spn_task_sync_packages_init,   spn_task_sync_packages_update   },
  [SPN_TASK_CONFIGURE_GRAPH] = { "configure",     spn_task_configure_graph_init, spn_task_configure_graph_update },
  [SPN_TASK_CREATE_UNITS]    = { "create-units",  SP_NULLPTR,                    spn_task_create_units           },
  [SPN_TASK_BUILD_GRAPH]     = { "build",         spn_task_build_graph_init,     spn_task_build_graph_update     },
  [SPN_TASK_RENDER_GRAPH]    = { "render-graph",  SP_NULLPTR,                    spn_task_render_graph           },
  [SPN_TASK_RUN]             = { "run",           SP_NULLPTR,                    spn_task_run                    },
  [SPN_TASK_GENERATE]        = { "generate",      SP_NULLPTR,                    spn_task_generate               },
  [SPN_TASK_WHICH]           = { "which",         SP_NULLPTR,                    spn_task_which                  },
  [SPN_TASK_UPDATE]          = { "update",        SP_NULLPTR,                    spn_task_update                 },
  [SPN_TASK_INIT]            = { "init",          SP_NULLPTR,                    spn_task_init                   },
  [SPN_TASK_ADD]             = { "add",           SP_NULLPTR,                    spn_task_add                    },
  [SPN_TASK_CLEAN]           = { "clean",         SP_NULLPTR,                    spn_task_clean                  },
  [SPN_TASK_PUBLISH]         = { "publish",       SP_NULLPTR,                    spn_task_publish                },
};

spn_task_desc_t* spn_task_get(spn_task_kind_t kind) {
  sp_assert(kind > SPN_TASK_NONE && kind < SPN_TASK_COUNT);
  spn_task_desc_t* task = &spn_tasks[kind];
  sp_assert(task->update);
  return task;
}

void spn_task_enqueue(spn_task_executor_t* ex, spn_task_kind_t kind) {
  sp_assert(ex->len < SPN_TASK_MAX_QUEUE);
  ex->data[ex->len++] = kind;
}

bool spn_task_rewind(spn_task_executor_t* ex, spn_task_kind_t kind) {
  for (s32 it = (s32)ex->index; it >= 0; it--) {
    if (ex->data[it] == kind) {
      ex->index = (u32)it;
      ex->initted = false;
      return true;
    }
  }
  return false;
}

sp_cli_result_t spn_task_plan_kinds(const spn_task_kind_t* kinds, u32 len) {
  sp_for(it, len) {
    spn_task_enqueue(&app.tasks, kinds[it]);
  }
  return SP_CLI_CONTINUE;
}
