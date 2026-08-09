#include "spn_test.h"

#include "core/core.h"

#define WAKE_TEST_MAX_OPS 8

typedef enum {
  WAKE_OP_NONE,
  WAKE_OP_RING,
  WAKE_OP_REARM,
} wake_op_kind_t;

typedef struct {
  wake_op_kind_t kind;
  u32 fired;
} wake_op_t;

typedef struct {
  const c8* name;
  bool no_fn;
  wake_op_t ops [WAKE_TEST_MAX_OPS];
} wake_test_t;

static const wake_test_t wake_tests [] = {
  {
    .name = "rings_on_first_edge_only",
    .ops = {
      { .kind = WAKE_OP_RING, .fired = 1 },
      { .kind = WAKE_OP_RING, .fired = 1 },
      { .kind = WAKE_OP_RING, .fired = 1 },
    },
  },
  {
    .name = "rearm_reopens_the_edge",
    .ops = {
      { .kind = WAKE_OP_RING, .fired = 1 },
      { .kind = WAKE_OP_REARM, .fired = 1 },
      { .kind = WAKE_OP_RING, .fired = 2 },
    },
  },
  {
    .name = "rearm_without_ring_is_idempotent",
    .ops = {
      { .kind = WAKE_OP_REARM },
      { .kind = WAKE_OP_REARM },
      { .kind = WAKE_OP_RING, .fired = 1 },
    },
  },
  {
    .name = "no_fn_never_fires",
    .no_fn = true,
    .ops = {
      { .kind = WAKE_OP_RING },
      { .kind = WAKE_OP_REARM },
      { .kind = WAKE_OP_RING },
    },
  },
};

static void wake_count(void* data) {
  u32* fired = (u32*)data;
  *fired += 1;
}

sp_test_each(core, wake, wake_test_t, wake_tests) {
  u32 fired = 0;
  spn_wake_t wake = sp_zero;
  if (!it->no_fn) {
    wake.fn = wake_count;
    wake.data = &fired;
  }

  sp_carr_for(it->ops, n) {
    const wake_op_t* op = &it->ops[n];
    switch (op->kind) {
      case WAKE_OP_NONE: {
        break;
      }
      case WAKE_OP_RING: {
        spn_wake_ring(&wake);
        sp_expect_eq(t, op->fired, fired);
        break;
      }
      case WAKE_OP_REARM: {
        spn_wake_rearm(&wake);
        sp_expect_eq(t, op->fired, fired);
        break;
      }
    }
  }

  return SP_OK;
}
