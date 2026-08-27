#include "spn_test.h"
#include "sp_sim.h"

#define SIM_TEST_MAX_OPS 8

typedef enum {
  SIM_OP_DONE,
  SIM_OP_STAT,
  SIM_OP_TICK,
  SIM_OP_AT,
  SIM_OP_FROM,
  SIM_OP_CLEAR,
} op_kind_t;

typedef struct {
  bool fail;
} op_expect_t;

typedef struct {
  op_kind_t kind;
  u64 nth;
  op_expect_t expect;
} op_t;

typedef struct {
  u64 faults;
  u64 points;
} expect_t;

typedef struct {
  const c8* name;
  op_t ops [SIM_TEST_MAX_OPS];
  expect_t expect;
} test_t;

static const test_t tests [] = {
  {
    .name = "at_fails_exactly_nth",
    .ops = {
      { .kind = SIM_OP_AT, .nth = 2 },
      { .kind = SIM_OP_STAT },
      { .kind = SIM_OP_STAT, .expect = { .fail = true } },
      { .kind = SIM_OP_STAT },
    },
    .expect = { .faults = 1, .points = 3 },
  },
  {
    .name = "from_fails_nth_onward",
    .ops = {
      { .kind = SIM_OP_FROM, .nth = 2 },
      { .kind = SIM_OP_STAT },
      { .kind = SIM_OP_STAT, .expect = { .fail = true } },
      { .kind = SIM_OP_STAT, .expect = { .fail = true } },
    },
    .expect = { .faults = 2, .points = 3 },
  },
  {
    .name = "at_beyond_never_fires",
    .ops = {
      { .kind = SIM_OP_AT, .nth = 4 },
      { .kind = SIM_OP_STAT },
      { .kind = SIM_OP_STAT },
      { .kind = SIM_OP_STAT },
    },
    .expect = { .points = 3 },
  },
  {
    .name = "nth_counts_from_arming",
    .ops = {
      { .kind = SIM_OP_STAT },
      { .kind = SIM_OP_STAT },
      { .kind = SIM_OP_AT, .nth = 1 },
      { .kind = SIM_OP_STAT, .expect = { .fail = true } },
      { .kind = SIM_OP_STAT },
    },
    .expect = { .faults = 1, .points = 4 },
  },
  {
    .name = "nth_skips_ineligible_syscalls",
    .ops = {
      { .kind = SIM_OP_AT, .nth = 1 },
      { .kind = SIM_OP_TICK },
      { .kind = SIM_OP_STAT, .expect = { .fail = true } },
    },
    .expect = { .faults = 1, .points = 1 },
  },
  {
    .name = "clear_disarms_from",
    .ops = {
      { .kind = SIM_OP_FROM, .nth = 1 },
      { .kind = SIM_OP_STAT, .expect = { .fail = true } },
      { .kind = SIM_OP_CLEAR },
      { .kind = SIM_OP_STAT },
    },
    .expect = { .faults = 1, .points = 2 },
  },
};

sp_test_each(sim_fault, ops, test_t, tests) {
  if (!sp_str_empty(sp_os_env_get(sp_str_lit("SPN_TEST_SIM")))) {
    return sp_test_skip(t, "drives a local sim");
  }

  sp_mem_t mem = sp_test_arena(t);
  sp_sim_t sim = sp_zero;
  sp_sim_init(&sim, mem);
  sp_sim_install(&sim);

  sp_carr_for(it->ops, ot) {
    const op_t* op = &it->ops[ot];
    if (op->kind == SIM_OP_DONE) {
      break;
    }
    sp_test_kv(t, "op", sp_fmt(mem, "{}", sp_fmt_uint(ot)).value);
    switch (op->kind) {
      case SIM_OP_DONE: {
        break;
      }
      case SIM_OP_STAT: {
        sp_sys_file_meta_t meta = sp_zero;
        sp_err_t err = sp_sys_get_path_metadata_s(sp_sys_get_root(0), sp_str_lit("/"), &meta);
        sp_expect_eq(t, op->expect.fail ? SP_ERR_SYS : SP_OK, err);
        break;
      }
      case SIM_OP_TICK: {
        sp_tm_now_epoch();
        break;
      }
      case SIM_OP_AT: {
        sp_sim_fault_at(&sim, op->nth);
        break;
      }
      case SIM_OP_FROM: {
        sp_sim_fault_from(&sim, op->nth);
        break;
      }
      case SIM_OP_CLEAR: {
        sp_sim_fault_clear(&sim);
        break;
      }
    }
  }
  sp_test_kv_clear(t, "op");
  sp_expect_eq(t, it->expect.faults, sim.faults);
  sp_expect_eq(t, it->expect.points, sim.fail_points);

  sp_sim_remove(&sim);
  return SP_OK;
}
