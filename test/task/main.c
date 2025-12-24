#define SP_IMPLEMENTATION
#include "sp.h"

#define SP_TEST_IMPLEMENTATION
#include "test.h"
#include "utest.h"

#define SPN_TASK_IMPLEMENTATION
#include "task.h"

UTEST_MAIN()

#define uf utest_fixture
#define ur (*utest_result)

enum {
  TASK_NONE = 0,
  TASK_A,
  TASK_B,
  TASK_C,
};

typedef struct {
  struct { s32 init; s32 update; } a;
  struct { s32 init; s32 update; } b;
  struct { s32 init; s32 update; } c;
} call_counts_t;

// Task functions that return DONE immediately
spn_task_result_t task_a_init_done(void* user) {
  call_counts_t* c = (call_counts_t*)user;
  c->a.init++;
  return SPN_TASK_DONE;
}

spn_task_result_t task_b_init_done(void* user) {
  call_counts_t* c = (call_counts_t*)user;
  c->b.init++;
  return SPN_TASK_DONE;
}

// Task functions that continue, then done on update
spn_task_result_t task_a_init_continue(void* user) {
  call_counts_t* c = (call_counts_t*)user;
  c->a.init++;
  return SPN_TASK_CONTINUE;
}

spn_task_result_t task_a_update_done(void* user) {
  call_counts_t* c = (call_counts_t*)user;
  c->a.update++;
  return SPN_TASK_DONE;
}

// Task that needs multiple updates
static s32 update_countdown = 0;

spn_task_result_t task_b_init_continue(void* user) {
  call_counts_t* c = (call_counts_t*)user;
  c->b.init++;
  update_countdown = 3;
  return SPN_TASK_CONTINUE;
}

spn_task_result_t task_b_update_countdown(void* user) {
  call_counts_t* c = (call_counts_t*)user;
  c->b.update++;
  update_countdown--;
  if (update_countdown <= 0) {
    return SPN_TASK_DONE;
  }
  return SPN_TASK_CONTINUE;
}

// Task that errors
spn_task_result_t task_c_init_error(void* user) {
  call_counts_t* c = (call_counts_t*)user;
  c->c.init++;
  return SPN_TASK_ERROR;
}

spn_task_result_t task_c_init_continue(void* user) {
  call_counts_t* c = (call_counts_t*)user;
  c->c.init++;
  return SPN_TASK_CONTINUE;
}

spn_task_result_t task_c_update_error(void* user) {
  call_counts_t* c = (call_counts_t*)user;
  c->c.update++;
  return SPN_TASK_ERROR;
}

struct task {
  spn_task_executor_t ex;
  call_counts_t counts;
};

UTEST_F_SETUP(task) {
  uf->ex = (spn_task_executor_t){0};
  uf->counts = (call_counts_t){0};
}

UTEST_F_TEARDOWN(task) {}

UTEST_F(task, empty_queue_quits_immediately) {
  // queue[0] == 0 by default (zero-initialized)
  spn_task_result_t r = spn_task_tick(&uf->ex, &uf->counts);
  EXPECT_EQ(r, SPN_TASK_DONE);
}

UTEST_F(task, single_task_init_done) {
  uf->ex.tasks[TASK_A].init = task_a_init_done;
  uf->ex.queue[0] = TASK_A;

  spn_task_result_t r = spn_task_tick(&uf->ex, &uf->counts);
  EXPECT_EQ(r, SPN_TASK_CONTINUE);
  EXPECT_EQ(uf->counts.a.init, 1);
  EXPECT_EQ(uf->ex.index, 1);

  r = spn_task_tick(&uf->ex, &uf->counts);
  EXPECT_EQ(r, SPN_TASK_DONE);
}

UTEST_F(task, single_task_init_continue_update_done) {
  uf->ex.tasks[TASK_A].init = task_a_init_continue;
  uf->ex.tasks[TASK_A].update = task_a_update_done;
  uf->ex.queue[0] = TASK_A;

  // First tick: init, returns CONTINUE
  spn_task_result_t r = spn_task_tick(&uf->ex, &uf->counts);
  EXPECT_EQ(r, SPN_TASK_CONTINUE);
  EXPECT_EQ(uf->counts.a.init, 1);
  EXPECT_EQ(uf->counts.a.update, 0);
  EXPECT_EQ(uf->ex.state, SPN_TASK_STATE_RUNNING);

  // Second tick: update, returns DONE
  r = spn_task_tick(&uf->ex, &uf->counts);
  EXPECT_EQ(r, SPN_TASK_CONTINUE);
  EXPECT_EQ(uf->counts.a.update, 1);
  EXPECT_EQ(uf->ex.index, 1);

  // Third tick: queue exhausted
  r = spn_task_tick(&uf->ex, &uf->counts);
  EXPECT_EQ(r, SPN_TASK_DONE);
}

UTEST_F(task, multiple_updates_before_done) {
  uf->ex.tasks[TASK_B].init = task_b_init_continue;
  uf->ex.tasks[TASK_B].update = task_b_update_countdown;
  uf->ex.queue[0] = TASK_B;

  // init
  spn_task_result_t r = spn_task_tick(&uf->ex, &uf->counts);
  EXPECT_EQ(r, SPN_TASK_CONTINUE);
  EXPECT_EQ(uf->counts.b.init, 1);

  // 3 updates (countdown from 3)
  r = spn_task_tick(&uf->ex, &uf->counts);
  EXPECT_EQ(r, SPN_TASK_CONTINUE);
  EXPECT_EQ(uf->counts.b.update, 1);

  r = spn_task_tick(&uf->ex, &uf->counts);
  EXPECT_EQ(r, SPN_TASK_CONTINUE);
  EXPECT_EQ(uf->counts.b.update, 2);

  r = spn_task_tick(&uf->ex, &uf->counts);
  EXPECT_EQ(r, SPN_TASK_CONTINUE);
  EXPECT_EQ(uf->counts.b.update, 3);
  EXPECT_EQ(uf->ex.index, 1);

  // done
  r = spn_task_tick(&uf->ex, &uf->counts);
  EXPECT_EQ(r, SPN_TASK_DONE);
}

UTEST_F(task, two_tasks_in_sequence) {
  uf->ex.tasks[TASK_A].init = task_a_init_done;
  uf->ex.tasks[TASK_B].init = task_b_init_done;
  uf->ex.queue[0] = TASK_A;
  uf->ex.queue[1] = TASK_B;

  spn_task_result_t r = spn_task_tick(&uf->ex, &uf->counts);
  EXPECT_EQ(r, SPN_TASK_CONTINUE);
  EXPECT_EQ(uf->counts.a.init, 1);
  EXPECT_EQ(uf->ex.index, 1);

  r = spn_task_tick(&uf->ex, &uf->counts);
  EXPECT_EQ(r, SPN_TASK_CONTINUE);
  EXPECT_EQ(uf->counts.b.init, 1);
  EXPECT_EQ(uf->ex.index, 2);

  r = spn_task_tick(&uf->ex, &uf->counts);
  EXPECT_EQ(r, SPN_TASK_DONE);
}

UTEST_F(task, error_stops_execution) {
  uf->ex.tasks[TASK_A].init = task_a_init_done;
  uf->ex.tasks[TASK_C].init = task_c_init_error;
  uf->ex.tasks[TASK_B].init = task_b_init_done;
  uf->ex.queue[0] = TASK_A;
  uf->ex.queue[1] = TASK_C;
  uf->ex.queue[2] = TASK_B;

  spn_task_result_t r = spn_task_tick(&uf->ex, &uf->counts);
  EXPECT_EQ(r, SPN_TASK_CONTINUE);
  EXPECT_EQ(uf->counts.a.init, 1);

  r = spn_task_tick(&uf->ex, &uf->counts);
  EXPECT_EQ(r, SPN_TASK_ERROR);
  EXPECT_EQ(uf->counts.c.init, 1);
  EXPECT_EQ(uf->counts.b.init, 0);  // B never ran
}

UTEST_F(task, null_init_skips_to_update) {
  // No init, only update
  uf->ex.tasks[TASK_A].update = task_a_update_done;
  uf->ex.queue[0] = TASK_A;

  // First tick: no init, transitions to RUNNING
  spn_task_result_t r = spn_task_tick(&uf->ex, &uf->counts);
  EXPECT_EQ(r, SPN_TASK_CONTINUE);
  EXPECT_EQ(uf->ex.state, SPN_TASK_STATE_RUNNING);

  // Second tick: update runs
  r = spn_task_tick(&uf->ex, &uf->counts);
  EXPECT_EQ(r, SPN_TASK_CONTINUE);
  EXPECT_EQ(uf->counts.a.update, 1);

  r = spn_task_tick(&uf->ex, &uf->counts);
  EXPECT_EQ(r, SPN_TASK_DONE);
}

UTEST_F(task, null_update_completes_after_init) {
  // Only init, no update
  uf->ex.tasks[TASK_A].init = task_a_init_continue;
  uf->ex.queue[0] = TASK_A;

  // First tick: init runs, returns CONTINUE
  spn_task_result_t r = spn_task_tick(&uf->ex, &uf->counts);
  EXPECT_EQ(r, SPN_TASK_CONTINUE);
  EXPECT_EQ(uf->counts.a.init, 1);
  EXPECT_EQ(uf->ex.state, SPN_TASK_STATE_RUNNING);

  // Second tick: no update fn, task completes
  r = spn_task_tick(&uf->ex, &uf->counts);
  EXPECT_EQ(r, SPN_TASK_CONTINUE);
  EXPECT_EQ(uf->ex.index, 1);

  r = spn_task_tick(&uf->ex, &uf->counts);
  EXPECT_EQ(r, SPN_TASK_DONE);
}

UTEST_F(task, error_from_update) {
  uf->ex.tasks[TASK_C].init = task_c_init_continue;
  uf->ex.tasks[TASK_C].update = task_c_update_error;
  uf->ex.tasks[TASK_A].init = task_a_init_done;
  uf->ex.queue[0] = TASK_C;
  uf->ex.queue[1] = TASK_A;

  // init succeeds
  spn_task_result_t r = spn_task_tick(&uf->ex, &uf->counts);
  EXPECT_EQ(r, SPN_TASK_CONTINUE);
  EXPECT_EQ(uf->counts.c.init, 1);

  // update errors
  r = spn_task_tick(&uf->ex, &uf->counts);
  EXPECT_EQ(r, SPN_TASK_ERROR);
  EXPECT_EQ(uf->counts.c.update, 1);
  EXPECT_EQ(uf->counts.a.init, 0);  // A never ran
}

UTEST_F(task, null_vtable_entry) {
  // queue references kind with no init or update
  uf->ex.queue[0] = TASK_A;
  // vtable[TASK_A] is all zeros

  // first tick: no init, transitions to RUNNING
  spn_task_result_t r = spn_task_tick(&uf->ex, &uf->counts);
  EXPECT_EQ(r, SPN_TASK_CONTINUE);
  EXPECT_EQ(uf->ex.state, SPN_TASK_STATE_RUNNING);

  // second tick: no update, task completes
  r = spn_task_tick(&uf->ex, &uf->counts);
  EXPECT_EQ(r, SPN_TASK_CONTINUE);
  EXPECT_EQ(uf->ex.index, 1);

  // queue exhausted
  r = spn_task_tick(&uf->ex, &uf->counts);
  EXPECT_EQ(r, SPN_TASK_DONE);
}
