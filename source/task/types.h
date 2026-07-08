#ifndef SPN_TASK_TYPES_H
#define SPN_TASK_TYPES_H

#include "error/types.h"
#include "forward/types.h"

typedef enum {
  SPN_TASK_NONE,
  SPN_TASK_SYNC_INDEXES,
  SPN_TASK_RESOLVE,
  SPN_TASK_SYNC_PACKAGES,
  SPN_TASK_CONFIGURE_GRAPH,
  SPN_TASK_CREATE_UNITS,
  SPN_TASK_BUILD_GRAPH,
  SPN_TASK_RENDER_GRAPH,
  SPN_TASK_RUN,
  SPN_TASK_GENERATE,
  SPN_TASK_WHICH,
  SPN_TASK_UPDATE,
  SPN_TASK_INIT,
  SPN_TASK_ADD,
  SPN_TASK_CLEAN,
  SPN_TASK_PUBLISH,
  SPN_TASK_COUNT,
} spn_task_kind_t;

typedef enum {
  SPN_TASK_CONTINUE,
  SPN_TASK_DONE,
} spn_task_status_t;

typedef struct {
  spn_task_status_t status;
  spn_err_union_t err;
} spn_task_step_t;

#define spn_task_done()     ((spn_task_step_t) { .status = SPN_TASK_DONE })
#define spn_task_continue() ((spn_task_step_t) { .status = SPN_TASK_CONTINUE })
#define spn_task_fail(kind_, ...) ((spn_task_step_t) { .err = { .kind = (kind_), ##__VA_ARGS__ } })
#define spn_task_fail_with(union_) ((spn_task_step_t) { .err = (union_) })

#define spn_try_step(expr) do { \
  spn_err_union_t _spn_step_err = (expr); \
  if (_spn_step_err.kind) return spn_task_fail_with(_spn_step_err); \
} while (0)

typedef spn_task_step_t (*spn_task_fn_t)(spn_app_t*);

typedef struct {
  const c8* name;
  spn_task_fn_t init;
  spn_task_fn_t update;
} spn_task_desc_t;

#define SPN_TASK_MAX_QUEUE 32

typedef struct {
  spn_task_kind_t data[SPN_TASK_MAX_QUEUE];
  u32 len;
  u32 index;
  bool initted;
} spn_task_executor_t;

#endif
