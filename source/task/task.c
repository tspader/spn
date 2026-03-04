#include "app/app.h"

void spn_task_enqueue(spn_task_executor_t* ex, s32 kind) {
  sp_assert(ex->len < SPN_TASK_MAX_QUEUE);
  ex->data[ex->len++] = kind;
}
