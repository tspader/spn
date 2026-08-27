#include "dag_test.h"
#include "dag/stamp.h"
#include "sim/sim.h"

#define FENCE_TEST_MAX_OPS 8
#define FENCE_TEST_TICK_NS 1000000000

typedef enum {
  FENCE_OP_DONE,
  FENCE_OP_FILE,
  FENCE_OP_STEALTH,
  FENCE_OP_PROBE,
  FENCE_OP_DIR,
  FENCE_OP_TRUST,
  FENCE_OP_TICK,
  FENCE_OP_REFRESH,
  FENCE_OP_DIGEST,
} op_kind_t;

typedef struct {
  const c8* blob;
} expect_t;

typedef struct {
  op_kind_t kind;
  const c8* path;
  const c8* blob;
  expect_t expect;
} op_t;

typedef struct {
  const c8* name;
  op_t ops [FENCE_TEST_MAX_OPS];
} test_t;

static const test_t tests [] = {
  {
    .name = "racy_record_skipped",
    .ops = {
      { .kind = FENCE_OP_PROBE },
      { .kind = FENCE_OP_FILE, .path = "F", .blob = "A" },
      { .kind = FENCE_OP_DIGEST, .path = "F", .expect = { .blob = "A" } },
      { .kind = FENCE_OP_FILE, .path = "F", .blob = "B" },
      { .kind = FENCE_OP_REFRESH },
      { .kind = FENCE_OP_DIGEST, .path = "F", .expect = { .blob = "B" } },
    }
  },
  {
    .name = "trusting_fence_serves_stale",
    .ops = {
      { .kind = FENCE_OP_TRUST },
      { .kind = FENCE_OP_FILE, .path = "F", .blob = "A" },
      { .kind = FENCE_OP_DIGEST, .path = "F", .expect = { .blob = "A" } },
      { .kind = FENCE_OP_FILE, .path = "F", .blob = "B" },
      { .kind = FENCE_OP_REFRESH },
      { .kind = FENCE_OP_DIGEST, .path = "F", .expect = { .blob = "A" } },
    }
  },
  {
    .name = "settled_record_trusted",
    .ops = {
      { .kind = FENCE_OP_FILE, .path = "F", .blob = "A" },
      { .kind = FENCE_OP_TICK },
      { .kind = FENCE_OP_PROBE },
      { .kind = FENCE_OP_DIGEST, .path = "F", .expect = { .blob = "A" } },
      { .kind = FENCE_OP_STEALTH, .path = "F", .blob = "B" },
      { .kind = FENCE_OP_REFRESH },
      { .kind = FENCE_OP_DIGEST, .path = "F", .expect = { .blob = "A" } },
    }
  },
  {
    .name = "racy_record_rehashes_stealth",
    .ops = {
      { .kind = FENCE_OP_PROBE },
      { .kind = FENCE_OP_FILE, .path = "F", .blob = "A" },
      { .kind = FENCE_OP_DIGEST, .path = "F", .expect = { .blob = "A" } },
      { .kind = FENCE_OP_STEALTH, .path = "F", .blob = "B" },
      { .kind = FENCE_OP_REFRESH },
      { .kind = FENCE_OP_DIGEST, .path = "F", .expect = { .blob = "B" } },
    }
  },
  {
    .name = "unfenced_trusts_nothing",
    .ops = {
      { .kind = FENCE_OP_FILE, .path = "F", .blob = "A" },
      { .kind = FENCE_OP_DIGEST, .path = "F", .expect = { .blob = "A" } },
      { .kind = FENCE_OP_STEALTH, .path = "F", .blob = "B" },
      { .kind = FENCE_OP_REFRESH },
      { .kind = FENCE_OP_DIGEST, .path = "F", .expect = { .blob = "B" } },
    }
  },
  {
    .name = "trip_refresh_records_settled",
    .ops = {
      { .kind = FENCE_OP_DIR },
      { .kind = FENCE_OP_FILE, .path = "F", .blob = "A" },
      { .kind = FENCE_OP_TICK },
      { .kind = FENCE_OP_DIGEST, .path = "F", .expect = { .blob = "A" } },
      { .kind = FENCE_OP_STEALTH, .path = "F", .blob = "B" },
      { .kind = FENCE_OP_REFRESH },
      { .kind = FENCE_OP_DIGEST, .path = "F", .expect = { .blob = "A" } },
    }
  },
  {
    .name = "trip_refresh_skips_open_quantum",
    .ops = {
      { .kind = FENCE_OP_DIR },
      { .kind = FENCE_OP_FILE, .path = "F", .blob = "A" },
      { .kind = FENCE_OP_DIGEST, .path = "F", .expect = { .blob = "A" } },
      { .kind = FENCE_OP_STEALTH, .path = "F", .blob = "B" },
      { .kind = FENCE_OP_REFRESH },
      { .kind = FENCE_OP_DIGEST, .path = "F", .expect = { .blob = "B" } },
    }
  },
};

sp_test_each(dag_fence, ops, test_t, tests) {
  if (!sp_str_empty(sp_os_env_get(sp_str_lit("SPN_TEST_SIM")))) {
    return sp_test_skip(t, "drives a local sim");
  }

  sp_mem_t mem = sp_test_arena(t);
  sp_sim_t sim = sp_zero;
  sp_sim_init(&sim, mem);
  sim.granularity = FENCE_TEST_TICK_NS;
  sp_sim_install(&sim);
  sp_fs_create_dir(sp_str_lit("/w"));

  spn_path_roots_t roots = sp_zero;
  spn_dag_file_cache_t files = sp_zero;
  spn_dag_file_cache_init(&files, mem, &roots);

  sp_carr_for(it->ops, ot) {
    const op_t* op = &it->ops[ot];
    if (op->kind == FENCE_OP_DONE) {
      break;
    }
    sp_str_t rendered = op->path ? sp_fmt(mem, "/w/{}", sp_fmt_cstr(op->path)).value : sp_str_lit("");
    switch (op->kind) {
      case FENCE_OP_DONE: {
        break;
      }
      case FENCE_OP_FILE: {
        sp_fs_create_file_str(rendered, sp_cstr_as_str(op->blob));
        break;
      }
      case FENCE_OP_STEALTH: {
        sp_expect(t, sp_sim_stealth_write(&sim, rendered, sp_cstr_as_str(op->blob)));
        break;
      }
      case FENCE_OP_PROBE: {
        sp_sys_timespec_t fence = sp_zero;
        sp_expect_eq(t, SPN_OK, spn_dag_stamp_probe(sp_str_lit("/w"), &fence));
        spn_dag_file_cache_fence(&files, fence);
        break;
      }
      case FENCE_OP_DIR: {
        sp_expect_eq(t, SPN_OK, spn_dag_file_cache_fence_dir(&files, sp_str_lit("/w")));
        break;
      }
      case FENCE_OP_TRUST: {
        spn_dag_file_cache_fence(&files, SPN_DAG_STAMP_TRUST_ALL);
        break;
      }
      case FENCE_OP_TICK: {
        sim.clock.tv_sec += 4;
        break;
      }
      case FENCE_OP_REFRESH: {
        spn_dag_file_cache_invalidate_all(&files);
        break;
      }
      case FENCE_OP_DIGEST: {
        spn_dag_digest_t digest = sp_zero;
        sp_expect_eq(t, SPN_OK, spn_dag_file_cache_digest(&files, spn_path_make(&roots, rendered), &digest));
        sp_expect(t, spn_dag_digest_equal(digest, dag_test_digest(op->expect.blob)));
        break;
      }
    }
  }

  sp_sim_remove(&sim);
  return SP_OK;
}
