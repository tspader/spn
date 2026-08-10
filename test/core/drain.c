#include "spn_test.h"

#include "spn/host.h"

#include "core/core.h"
#include "ctx/types.h"
#include "event/event.h"

#define DRAIN_TEST_MAX_OPS 12

typedef enum {
  DRAIN_OP_NONE,
  DRAIN_OP_PUSH,
  DRAIN_OP_PULSE,
  DRAIN_OP_DRAIN,
  DRAIN_OP_DRY,
} drain_op_kind_t;

typedef struct {
  drain_op_kind_t kind;
  spn_build_event_kind_t event;
  u32 fired;
} drain_op_t;

typedef struct {
  const c8* name;
  drain_op_t ops [DRAIN_TEST_MAX_OPS];
} drain_test_t;

static const drain_test_t drain_tests [] = {
  {
    .name = "drains_in_push_order",
    .ops = {
      { .kind = DRAIN_OP_PUSH, .event = SPN_EVENT_RESOLVE_START, .fired = 1 },
      { .kind = DRAIN_OP_PUSH, .event = SPN_EVENT_COMPILE_START, .fired = 1 },
      { .kind = DRAIN_OP_DRAIN, .event = SPN_EVENT_RESOLVE_START, .fired = 1 },
      { .kind = DRAIN_OP_DRAIN, .event = SPN_EVENT_COMPILE_START, .fired = 1 },
      { .kind = DRAIN_OP_DRY, .fired = 1 },
    },
  },
  {
    .name = "dry_rearms_the_doorbell",
    .ops = {
      { .kind = DRAIN_OP_PUSH, .event = SPN_EVENT_RESOLVE_START, .fired = 1 },
      { .kind = DRAIN_OP_DRAIN, .event = SPN_EVENT_RESOLVE_START, .fired = 1 },
      { .kind = DRAIN_OP_DRY, .fired = 1 },
      { .kind = DRAIN_OP_PUSH, .event = SPN_EVENT_COMPILE_START, .fired = 2 },
    },
  },
  {
    .name = "partial_drain_keeps_coalescing",
    .ops = {
      { .kind = DRAIN_OP_PUSH, .event = SPN_EVENT_RESOLVE_START, .fired = 1 },
      { .kind = DRAIN_OP_DRAIN, .event = SPN_EVENT_RESOLVE_START, .fired = 1 },
      { .kind = DRAIN_OP_PUSH, .event = SPN_EVENT_COMPILE_START, .fired = 1 },
      { .kind = DRAIN_OP_DRAIN, .event = SPN_EVENT_COMPILE_START, .fired = 1 },
      { .kind = DRAIN_OP_DRY, .fired = 1 },
      { .kind = DRAIN_OP_PUSH, .event = SPN_EVENT_RESOLVE_START, .fired = 2 },
    },
  },
  {
    .name = "empty_drain_is_dry",
    .ops = {
      { .kind = DRAIN_OP_DRY },
      { .kind = DRAIN_OP_PUSH, .event = SPN_EVENT_RESOLVE_START, .fired = 1 },
    },
  },
  {
    .name = "pulse_holds_the_edge_until_dry",
    .ops = {
      { .kind = DRAIN_OP_PULSE, .fired = 1 },
      { .kind = DRAIN_OP_PUSH, .event = SPN_EVENT_RESOLVE_START, .fired = 1 },
      { .kind = DRAIN_OP_DRAIN, .event = SPN_EVENT_RESOLVE_START, .fired = 1 },
      { .kind = DRAIN_OP_DRY, .fired = 1 },
      { .kind = DRAIN_OP_PUSH, .event = SPN_EVENT_COMPILE_START, .fired = 2 },
    },
  },
};

static void drain_count(void* data) {
  u32* fired = (u32*)data;
  *fired += 1;
}

sp_test_each(core, drain, drain_test_t, drain_tests) {
  sp_mem_t mem = sp_test_arena(t);

  u32 fired = 0;
  spn_ctx_t ctx = sp_zero;
  ctx.wake.fn = drain_count;
  ctx.wake.data = &fired;
  ctx.events = spn_event_buffer_new(mem);
  ctx.events->wake = &ctx.wake;

  sp_carr_for(it->ops, n) {
    const drain_op_t* op = &it->ops[n];
    switch (op->kind) {
      case DRAIN_OP_NONE: {
        break;
      }
      case DRAIN_OP_PUSH: {
        spn_event_buffer_push(ctx.events, (spn_build_event_t) { .kind = op->event });
        sp_expect_eq(t, op->fired, fired);
        break;
      }
      case DRAIN_OP_PULSE: {
        spn_wake_pulse(&ctx.wake);
        sp_expect_eq(t, op->fired, fired);
        break;
      }
      case DRAIN_OP_DRAIN: {
        spn_build_event_t* event = spn_ctx_drain(&ctx);
        sp_expect(t, event != SP_NULLPTR);
        sp_expect_eq(t, event->kind, op->event);
        sp_expect_eq(t, op->fired, fired);
        break;
      }
      case DRAIN_OP_DRY: {
        sp_expect(t, spn_ctx_drain(&ctx) == SP_NULLPTR);
        sp_expect_eq(t, op->fired, fired);
        break;
      }
    }
  }

  return SP_OK;
}
