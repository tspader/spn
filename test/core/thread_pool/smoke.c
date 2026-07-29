#include "spn_test.h"

#include "thread_pool/thread_pool.h"

#define SMOKE_JOBS 64
#define SMOKE_WORKERS 4

sp_test_suite(thread_pool, .serial = true);

static sp_atomic_s32_t smoke_runs;
static sp_atomic_s32_t smoke_exits;

typedef struct {
  bool ran;
} smoke_probe_t;

static void smoke_run(void* data) {
  smoke_probe_t* probe = (smoke_probe_t*)data;
  probe->ran = true;
  sp_atomic_s32_add(&smoke_runs, 1);
}

static void smoke_exit() {
  sp_atomic_s32_add(&smoke_exits, 1);
}

sp_test(thread_pool, smoke) {
  sp_mem_t mem = sp_test_arena(t);

  sp_atomic_s32_set(&smoke_runs, 0);
  sp_atomic_s32_set(&smoke_exits, 0);

  spn_thread_pool_t pool = sp_zero;
  spn_thread_pool_init(&pool, mem, (spn_thread_pool_config_t) {
    .workers = SMOKE_WORKERS,
    .on_worker_exit = smoke_exit,
  });

  smoke_probe_t* probes = sp_alloc_n(mem, smoke_probe_t, SMOKE_JOBS);
  sp_for(it, SMOKE_JOBS) {
    spn_thread_pool_submit(&pool.executor, (spn_thread_pool_job_t) { .fn = smoke_run, .data = &probes[it] });
  }

  sp_for(it, SMOKE_JOBS) {
    spn_thread_pool_job_t job = spn_thread_pool_poll(&pool.executor);
    sp_must(t, job.fn);
  }

  sp_must_eq(t, 0, spn_thread_pool_pending(&pool));
  spn_thread_pool_deinit(&pool);

  sp_expect_eq(t, SMOKE_JOBS, sp_atomic_s32_get(&smoke_runs));
  sp_expect_eq(t, SMOKE_WORKERS, sp_atomic_s32_get(&smoke_exits));
  sp_for(it, SMOKE_JOBS) {
    sp_expect(t, probes[it].ran);
  }

  return SP_OK;
}
