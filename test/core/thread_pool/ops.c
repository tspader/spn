#include "spn_test.h"

#include "thread_pool/thread_pool.h"

#define POOL_TEST_MAX_OPS 16

typedef enum {
  POOL_OP_NONE,
  POOL_OP_SUBMIT,
  POOL_OP_STEP,
  POOL_OP_POLL,
  POOL_OP_TRY_POLL,
} pool_op_kind_t;

typedef struct {
  pool_op_kind_t kind;
  struct {
    bool idle;
  } step;
  struct {
    u32 probe;
  } poll;
  struct {
    bool empty;
  } try_poll;
} pool_op_t;

typedef struct {
  const c8* name;
  pool_op_t ops [POOL_TEST_MAX_OPS];
  struct {
    u32 ran;
    u32 pending;
  } expect;
} pool_test_t;

typedef struct {
  u32 ran;
} pool_env_t;

typedef struct {
  pool_env_t* env;
  bool ran;
} pool_probe_t;

static void probe_run(void* data) {
  pool_probe_t* probe = (pool_probe_t*)data;
  probe->ran = true;
  probe->env->ran++;
}

static const pool_test_t pool_tests [] = {
  {
    .name = "submit_is_deferred",
    .ops = {
      { .kind = POOL_OP_SUBMIT },
      { .kind = POOL_OP_TRY_POLL, .try_poll = { .empty = true } },
    },
    .expect = { .pending = 1 }
  },
  {
    .name = "step_runs_one_job",
    .ops = {
      { .kind = POOL_OP_SUBMIT },
      { .kind = POOL_OP_STEP },
      { .kind = POOL_OP_TRY_POLL },
      { .kind = POOL_OP_TRY_POLL, .try_poll = { .empty = true } },
    },
    .expect = { .ran = 1 }
  },
  {
    .name = "step_is_idle_on_empty_queue",
    .ops = {
      { .kind = POOL_OP_STEP, .step = { .idle = true } },
    },
  },
  {
    .name = "poll_executes_queued_jobs",
    .ops = {
      { .kind = POOL_OP_SUBMIT },
      { .kind = POOL_OP_SUBMIT },
      { .kind = POOL_OP_POLL },
      { .kind = POOL_OP_POLL },
      { .kind = POOL_OP_TRY_POLL, .try_poll = { .empty = true } },
    },
    .expect = { .ran = 2 }
  },
  {
    .name = "deinit_discards_undrained_results",
    .ops = {
      { .kind = POOL_OP_SUBMIT },
      { .kind = POOL_OP_SUBMIT },
      { .kind = POOL_OP_STEP },
      { .kind = POOL_OP_STEP },
    },
    .expect = { .ran = 2 }
  },
  {
    .name = "poll_prefers_completed_over_queued",
    .ops = {
      { .kind = POOL_OP_SUBMIT },
      { .kind = POOL_OP_STEP },
      { .kind = POOL_OP_SUBMIT },
      { .kind = POOL_OP_POLL, .poll = { .probe = 1 } },
      { .kind = POOL_OP_POLL, .poll = { .probe = 2 } },
    },
    .expect = { .ran = 2 }
  },
  {
    .name = "interleaved_submit_and_drain",
    .ops = {
      { .kind = POOL_OP_SUBMIT },
      { .kind = POOL_OP_STEP },
      { .kind = POOL_OP_TRY_POLL },
      { .kind = POOL_OP_SUBMIT },
      { .kind = POOL_OP_SUBMIT },
      { .kind = POOL_OP_POLL },
      { .kind = POOL_OP_STEP },
      { .kind = POOL_OP_TRY_POLL },
      { .kind = POOL_OP_TRY_POLL, .try_poll = { .empty = true } },
    },
    .expect = { .ran = 3 }
  },
};

sp_test_each(thread_pool, ops, pool_test_t, pool_tests) {
  sp_mem_t mem = sp_test_arena(t);

  spn_thread_pool_t pool = sp_zero;
  spn_thread_pool_init(&pool, mem, (spn_thread_pool_config_t) sp_zero);

  pool_env_t env = sp_zero;
  pool_probe_t* probes = sp_alloc_n(mem, pool_probe_t, POOL_TEST_MAX_OPS);
  u32 submitted = 0;

  sp_carr_for(it->ops, n) {
    const pool_op_t* op = &it->ops[n];
    switch (op->kind) {
      case POOL_OP_NONE: {
        break;
      }
      case POOL_OP_SUBMIT: {
        pool_probe_t* probe = &probes[submitted++];
        probe->env = &env;
        spn_thread_pool_submit(&pool.executor, (spn_thread_pool_job_t) { .fn = probe_run, .data = probe });
        break;
      }
      case POOL_OP_STEP: {
        sp_expect_eq(t, !op->step.idle, spn_thread_pool_step(&pool));
        break;
      }
      case POOL_OP_POLL: {
        spn_thread_pool_job_t job = spn_thread_pool_poll(&pool.executor);
        sp_must(t, job.fn);
        sp_expect(t, ((pool_probe_t*)job.data)->ran);
        if (op->poll.probe) {
          sp_expect_eq(t, (void*)&probes[op->poll.probe - 1], job.data);
        }
        break;
      }
      case POOL_OP_TRY_POLL: {
        spn_thread_pool_job_t job = spn_thread_pool_try_poll(&pool.executor);
        sp_expect_eq(t, op->try_poll.empty, !job.fn);
        if (job.fn) {
          sp_expect(t, ((pool_probe_t*)job.data)->ran);
        }
        break;
      }
    }
  }

  sp_expect_eq(t, it->expect.ran, env.ran);
  sp_expect_eq(t, it->expect.pending, spn_thread_pool_pending(&pool));

  while (spn_thread_pool_pending(&pool)) {
    spn_thread_pool_poll(&pool.executor);
  }
  spn_thread_pool_deinit(&pool);
  return SP_OK;
}
