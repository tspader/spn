#include "spn_test.h"

#include "sp/queue.h"

#define QUEUE_TEST_MAX_OPS 16
#define QUEUE_TEST_MAX_NODES 8

typedef enum {
  QUEUE_OP_NONE,
  QUEUE_OP_PUSH,
  QUEUE_OP_POP,
  QUEUE_OP_WAIT,
  QUEUE_OP_WAKE,
} queue_op_kind_t;

typedef struct {
  queue_op_kind_t kind;
  struct {
    u32 node;
  } push;
  struct {
    u32 node;
    bool empty;
  } pop;
  struct {
    u32 node;
    bool empty;
  } wait;
} queue_op_t;

typedef struct {
  const c8* name;
  queue_op_t ops [QUEUE_TEST_MAX_OPS];
} queue_test_t;

static const queue_test_t queue_tests [] = {
  {
    .name = "pop_on_empty",
    .ops = {
      { .kind = QUEUE_OP_POP, .pop = { .empty = true } },
    },
  },
  {
    .name = "fifo_within_batch",
    .ops = {
      { .kind = QUEUE_OP_PUSH, .push = { .node = 1 } },
      { .kind = QUEUE_OP_PUSH, .push = { .node = 2 } },
      { .kind = QUEUE_OP_PUSH, .push = { .node = 3 } },
      { .kind = QUEUE_OP_POP, .pop = { .node = 1 } },
      { .kind = QUEUE_OP_POP, .pop = { .node = 2 } },
      { .kind = QUEUE_OP_POP, .pop = { .node = 3 } },
      { .kind = QUEUE_OP_POP, .pop = { .empty = true } },
    },
  },
  {
    .name = "fifo_across_batches",
    .ops = {
      { .kind = QUEUE_OP_PUSH, .push = { .node = 1 } },
      { .kind = QUEUE_OP_PUSH, .push = { .node = 2 } },
      { .kind = QUEUE_OP_POP, .pop = { .node = 1 } },
      { .kind = QUEUE_OP_PUSH, .push = { .node = 3 } },
      { .kind = QUEUE_OP_POP, .pop = { .node = 2 } },
      { .kind = QUEUE_OP_POP, .pop = { .node = 3 } },
      { .kind = QUEUE_OP_POP, .pop = { .empty = true } },
    },
  },
  {
    .name = "reusable_after_drain",
    .ops = {
      { .kind = QUEUE_OP_PUSH, .push = { .node = 1 } },
      { .kind = QUEUE_OP_POP, .pop = { .node = 1 } },
      { .kind = QUEUE_OP_POP, .pop = { .empty = true } },
      { .kind = QUEUE_OP_PUSH, .push = { .node = 2 } },
      { .kind = QUEUE_OP_POP, .pop = { .node = 2 } },
    },
  },
  {
    .name = "node_reusable_after_pop",
    .ops = {
      { .kind = QUEUE_OP_PUSH, .push = { .node = 1 } },
      { .kind = QUEUE_OP_POP, .pop = { .node = 1 } },
      { .kind = QUEUE_OP_PUSH, .push = { .node = 1 } },
      { .kind = QUEUE_OP_POP, .pop = { .node = 1 } },
    },
  },
  {
    .name = "wait_returns_pending_item",
    .ops = {
      { .kind = QUEUE_OP_PUSH, .push = { .node = 1 } },
      { .kind = QUEUE_OP_WAIT, .wait = { .node = 1 } },
    },
  },
  {
    .name = "wait_serves_fifo_cache",
    .ops = {
      { .kind = QUEUE_OP_PUSH, .push = { .node = 1 } },
      { .kind = QUEUE_OP_PUSH, .push = { .node = 2 } },
      { .kind = QUEUE_OP_POP, .pop = { .node = 1 } },
      { .kind = QUEUE_OP_WAIT, .wait = { .node = 2 } },
    },
  },
  {
    .name = "wake_does_not_drop_items",
    .ops = {
      { .kind = QUEUE_OP_PUSH, .push = { .node = 1 } },
      { .kind = QUEUE_OP_WAKE },
      { .kind = QUEUE_OP_POP, .pop = { .node = 1 } },
    },
  },
  {
    .name = "stale_wake_does_not_shadow_item",
    .ops = {
      { .kind = QUEUE_OP_WAKE },
      { .kind = QUEUE_OP_PUSH, .push = { .node = 1 } },
      { .kind = QUEUE_OP_WAIT, .wait = { .node = 1 } },
    },
  },
  {
    .name = "wake_makes_empty_wait_return",
    .ops = {
      { .kind = QUEUE_OP_WAKE },
      { .kind = QUEUE_OP_WAIT, .wait = { .empty = true } },
    },
  },
  {
    .name = "interleaved_push_and_pop",
    .ops = {
      { .kind = QUEUE_OP_PUSH, .push = { .node = 1 } },
      { .kind = QUEUE_OP_PUSH, .push = { .node = 2 } },
      { .kind = QUEUE_OP_POP, .pop = { .node = 1 } },
      { .kind = QUEUE_OP_PUSH, .push = { .node = 3 } },
      { .kind = QUEUE_OP_PUSH, .push = { .node = 4 } },
      { .kind = QUEUE_OP_POP, .pop = { .node = 2 } },
      { .kind = QUEUE_OP_POP, .pop = { .node = 3 } },
      { .kind = QUEUE_OP_PUSH, .push = { .node = 5 } },
      { .kind = QUEUE_OP_POP, .pop = { .node = 4 } },
      { .kind = QUEUE_OP_POP, .pop = { .node = 5 } },
      { .kind = QUEUE_OP_POP, .pop = { .empty = true } },
    },
  },
};

sp_test_each(queue, ops, queue_test_t, queue_tests) {
  sp_queue_t queue = sp_zero;
  sp_queue_init(&queue);

  sp_queue_node_t nodes [QUEUE_TEST_MAX_NODES] = sp_zero;

  sp_carr_for(it->ops, n) {
    const queue_op_t* op = &it->ops[n];
    switch (op->kind) {
      case QUEUE_OP_NONE: {
        break;
      }
      case QUEUE_OP_PUSH: {
        sp_queue_push(&queue, &nodes[op->push.node - 1]);
        break;
      }
      case QUEUE_OP_POP: {
        sp_queue_node_t* node = sp_queue_pop(&queue);
        if (op->pop.empty) {
          sp_expect(t, (void*)node == SP_NULLPTR);
        }
        else {
          sp_expect(t, (void*)&nodes[op->pop.node - 1] == (void*)node);
        }
        break;
      }
      case QUEUE_OP_WAIT: {
        sp_queue_node_t* node = sp_queue_wait(&queue);
        if (op->wait.empty) {
          sp_expect(t, (void*)node == SP_NULLPTR);
        }
        else {
          sp_expect(t, (void*)&nodes[op->wait.node - 1] == (void*)node);
        }
        break;
      }
      case QUEUE_OP_WAKE: {
        sp_queue_wake(&queue);
        break;
      }
    }
  }

  return SP_OK;
}
