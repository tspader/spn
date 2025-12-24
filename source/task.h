#ifndef SPN_TASK_H
#define SPN_TASK_H

#define SPN_TASK_MAX_KINDS 32
#define SPN_TASK_MAX_QUEUE 32

typedef enum {
  SPN_TASK_STATE_NEW,
  SPN_TASK_STATE_RUNNING,
} spn_task_state_t;

typedef enum {
  SPN_TASK_CONTINUE,
  SPN_TASK_DONE,
  SPN_TASK_ERROR,
} spn_task_result_t;

sp_typedef_fn(spn_task_result_t, spn_task_fn_t, void*);

typedef struct {
  spn_task_fn_t init;
  spn_task_fn_t update;
} spn_task_vtable_t;

typedef struct {
  spn_task_vtable_t tasks[SPN_TASK_MAX_KINDS];
  s32 queue[SPN_TASK_MAX_QUEUE];
  u32 len;
  u32 index;
  spn_task_state_t state;
} spn_task_executor_t;

spn_task_result_t spn_task_tick(spn_task_executor_t* ex, void* user);
void spn_task_enqueue(spn_task_executor_t* ex, s32 kind);

#endif

#ifdef SPN_TASK_IMPLEMENTATION

spn_task_result_t spn_task_tick(spn_task_executor_t* ex, void* user) {
  s32 kind = ex->queue[ex->index];
  if (kind == 0) {
    return SPN_TASK_DONE;
  }

  spn_task_vtable_t* vt = &ex->tasks[kind];
  spn_task_fn_t fn = SP_NULLPTR;

  switch (ex->state) {
    case SPN_TASK_STATE_NEW: {
      fn = vt->init;
      break;
    }
    case SPN_TASK_STATE_RUNNING: {
      fn = vt->update;
      break;
    }
  }

  if (!fn) {
    if (ex->state == SPN_TASK_STATE_NEW) {
      ex->state = SPN_TASK_STATE_RUNNING;
      return SPN_TASK_CONTINUE;
    } else {
      ex->index++;
      ex->state = SPN_TASK_STATE_NEW;
      return SPN_TASK_CONTINUE;
    }
  }

  spn_task_result_t result = fn(user);

  switch (result) {
    case SPN_TASK_ERROR: {
      return SPN_TASK_ERROR;
    }
    case SPN_TASK_DONE: {
      ex->index++;
      ex->state = SPN_TASK_STATE_NEW;
      return SPN_TASK_CONTINUE;
    }
    case SPN_TASK_CONTINUE: {
      if (ex->state == SPN_TASK_STATE_NEW) {
        ex->state = SPN_TASK_STATE_RUNNING;
      }
      return SPN_TASK_CONTINUE;
    }
  }

  return SPN_TASK_CONTINUE;
}

void spn_task_enqueue(spn_task_executor_t* ex, s32 kind) {
  SP_ASSERT(ex->len < SPN_TASK_MAX_QUEUE);
  ex->queue[ex->len++] = kind;
}

#endif
