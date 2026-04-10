#ifndef SPN_TASK_TYPES_H
#define SPN_TASK_TYPES_H

#include "forward/types.h"

typedef enum {
  SPN_TASK_KIND_NONE,
  SPN_TASK_RESOLVE,
  SPN_TASK_SYNC_PACKAGES,
  SPN_TASK_RUN_CONFIGURE_GRAPH,
  SPN_TASK_CREATE_UNITS,
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

#endif
