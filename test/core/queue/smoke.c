#include "unit.h"

#include "queue/queue.h"

#define SMOKE_MAX_PRODUCERS 16
#define SMOKE_PING_ROUNDS 8192
#define SMOKE_RECYCLE_PRODUCERS 4
#define SMOKE_RECYCLE_NODES 8
#define SMOKE_RECYCLE_ROUNDS 8192

sp_test_suite(queue, .serial = true);

typedef struct {
  sp_queue_node_t node;
  u32 producer;
  u32 seq;
} smoke_item_t;

typedef struct {
  sp_queue_t* queue;
  smoke_item_t* items;
  u32 id;
  u32 count;
} smoke_producer_t;

static s32 smoke_produce(void* data) {
  smoke_producer_t* producer = (smoke_producer_t*)data;
  sp_for(it, producer->count) {
    producer->items[it].producer = producer->id;
    producer->items[it].seq = (u32)it;
    sp_queue_push(producer->queue, &producer->items[it].node);
  }
  return 0;
}

static sp_queue_node_t* smoke_wait(sp_queue_t* queue) {
  sp_queue_node_t* node = sp_zero;
  while (!node) {
    node = sp_queue_wait(queue);
  }
  return node;
}

typedef struct {
  const c8* name;
  u32 producers;
  u32 items;
  bool pop;
} stress_test_t;

static const stress_test_t stress_tests [] = {
  {
    .name = "spsc",
    .producers = 1,
    .items = 8192,
  },
  {
    .name = "mpsc_wait",
    .producers = 4,
    .items = 8192,
  },
  {
    .name = "mpsc_pop",
    .producers = 4,
    .items = 8192,
    .pop = true,
  },
  {
    .name = "oversubscribed",
    .producers = 16,
    .items = 2048,
  },
};

sp_test_each(queue, stress, stress_test_t, stress_tests) {
  sp_mem_t mem = sp_test_arena(t);

  sp_queue_t queue = sp_zero;
  sp_queue_init(&queue);

  smoke_item_t* items = sp_alloc_n(mem, smoke_item_t, it->producers * it->items);
  smoke_producer_t* producers = sp_alloc_n(mem, smoke_producer_t, it->producers);
  sp_thread_t* threads = sp_alloc_n(mem, sp_thread_t, it->producers);

  sp_for(p, it->producers) {
    producers[p] = (smoke_producer_t) {
      .queue = &queue,
      .items = &items[p * it->items],
      .id = (u32)p,
      .count = it->items,
    };
    sp_thread_init(&threads[p], smoke_produce, &producers[p]);
  }

  u32 expected [SMOKE_MAX_PRODUCERS] = sp_zero;
  u32 received = 0;
  while (received < it->producers * it->items) {
    sp_queue_node_t* node = it->pop ? sp_queue_pop(&queue) : sp_queue_wait(&queue);
    if (!node) {
      continue;
    }
    smoke_item_t* item = (smoke_item_t*)node;
    sp_must_eq(t, expected[item->producer], item->seq);
    expected[item->producer]++;
    received++;
  }

  sp_for(p, it->producers) {
    sp_thread_join(&threads[p]);
  }

  sp_expect(t, (void*)sp_queue_pop(&queue) == SP_NULLPTR);
  return SP_OK;
}

typedef struct {
  sp_queue_t* ping;
  sp_queue_t* pong;
} smoke_relay_t;

static s32 smoke_relay(void* data) {
  smoke_relay_t* relay = (smoke_relay_t*)data;
  sp_for(it, SMOKE_PING_ROUNDS) {
    sp_queue_push(relay->pong, smoke_wait(relay->ping));
  }
  return 0;
}

sp_test(queue, ping_pong) {
  sp_queue_t ping = sp_zero;
  sp_queue_t pong = sp_zero;
  sp_queue_init(&ping);
  sp_queue_init(&pong);

  smoke_relay_t relay = { .ping = &ping, .pong = &pong };
  sp_thread_t thread = sp_zero;
  sp_thread_init(&thread, smoke_relay, &relay);

  sp_queue_node_t node = sp_zero;
  sp_for(it, SMOKE_PING_ROUNDS) {
    sp_queue_push(&ping, &node);
    sp_must(t, (void*)&node == (void*)smoke_wait(&pong));
  }

  sp_thread_join(&thread);
  return SP_OK;
}

typedef struct {
  sp_queue_node_t node;
  sp_atomic_u32_t idle;
  u32 producer;
  u32 seq;
} recycle_item_t;

typedef struct {
  sp_queue_t* queue;
  recycle_item_t* pool;
} recycle_producer_t;

static s32 recycle_produce(void* data) {
  recycle_producer_t* producer = (recycle_producer_t*)data;
  sp_for(it, SMOKE_RECYCLE_ROUNDS) {
    recycle_item_t* item = &producer->pool[it % SMOKE_RECYCLE_NODES];
    while (!sp_atomic_u32_load(&item->idle, SP_ATOMIC_ACQUIRE)) {
    }
    sp_atomic_u32_store(&item->idle, 0, SP_ATOMIC_RELAXED);
    item->seq = (u32)it;
    sp_queue_push(producer->queue, &item->node);
  }
  return 0;
}

sp_test(queue, node_recycle_stress) {
  sp_mem_t mem = sp_test_arena(t);

  sp_queue_t queue = sp_zero;
  sp_queue_init(&queue);

  recycle_item_t* items = sp_alloc_n(mem, recycle_item_t, SMOKE_RECYCLE_PRODUCERS * SMOKE_RECYCLE_NODES);
  recycle_producer_t* producers = sp_alloc_n(mem, recycle_producer_t, SMOKE_RECYCLE_PRODUCERS);
  sp_thread_t* threads = sp_alloc_n(mem, sp_thread_t, SMOKE_RECYCLE_PRODUCERS);

  sp_for(it, SMOKE_RECYCLE_PRODUCERS) {
    producers[it] = (recycle_producer_t) {
      .queue = &queue,
      .pool = &items[it * SMOKE_RECYCLE_NODES],
    };
    sp_for(n, SMOKE_RECYCLE_NODES) {
      producers[it].pool[n] = (recycle_item_t) { .producer = (u32)it };
      sp_atomic_u32_store(&producers[it].pool[n].idle, 1, SP_ATOMIC_RELEASE);
    }
    sp_thread_init(&threads[it], recycle_produce, &producers[it]);
  }

  u32 expected [SMOKE_RECYCLE_PRODUCERS] = sp_zero;
  u32 received = 0;
  while (received < SMOKE_RECYCLE_PRODUCERS * SMOKE_RECYCLE_ROUNDS) {
    sp_queue_node_t* node = sp_queue_wait(&queue);
    if (!node) {
      continue;
    }
    recycle_item_t* item = (recycle_item_t*)node;
    sp_must_eq(t, expected[item->producer], item->seq);
    expected[item->producer]++;
    received++;
    sp_atomic_u32_store(&item->idle, 1, SP_ATOMIC_RELEASE);
  }

  sp_for(it, SMOKE_RECYCLE_PRODUCERS) {
    sp_thread_join(&threads[it]);
  }

  sp_expect(t, (void*)sp_queue_pop(&queue) == SP_NULLPTR);
  return SP_OK;
}

typedef struct {
  sp_queue_t* queue;
  sp_atomic_u32_t* done;
  u32 count;
} smoke_drainer_t;

static s32 smoke_drain(void* data) {
  smoke_drainer_t* drainer = (smoke_drainer_t*)data;
  while (true) {
    sp_queue_node_t* node = sp_queue_wait(drainer->queue);
    if (node) {
      drainer->count++;
      continue;
    }
    if (sp_atomic_u32_load(drainer->done, SP_ATOMIC_ACQUIRE)) {
      while ((node = sp_queue_pop(drainer->queue))) {
        drainer->count++;
      }
      break;
    }
  }
  return 0;
}

sp_test(queue, shutdown_drains_in_flight_items) {
  sp_mem_t mem = sp_test_arena(t);

  sp_queue_t queue = sp_zero;
  sp_queue_init(&queue);

  u32 count = 4096;
  smoke_item_t* items = sp_alloc_n(mem, smoke_item_t, SMOKE_RECYCLE_PRODUCERS * count);
  smoke_producer_t* producers = sp_alloc_n(mem, smoke_producer_t, SMOKE_RECYCLE_PRODUCERS);
  sp_thread_t* threads = sp_alloc_n(mem, sp_thread_t, SMOKE_RECYCLE_PRODUCERS);

  sp_atomic_u32_t done = sp_zero;
  smoke_drainer_t drainer = { .queue = &queue, .done = &done };
  sp_thread_t consumer = sp_zero;
  sp_thread_init(&consumer, smoke_drain, &drainer);

  sp_for(it, SMOKE_RECYCLE_PRODUCERS) {
    producers[it] = (smoke_producer_t) {
      .queue = &queue,
      .items = &items[it * count],
      .id = (u32)it,
      .count = count,
    };
    sp_thread_init(&threads[it], smoke_produce, &producers[it]);
  }

  sp_for(it, SMOKE_RECYCLE_PRODUCERS) {
    sp_thread_join(&threads[it]);
  }

  sp_atomic_u32_store(&done, 1, SP_ATOMIC_RELEASE);
  sp_queue_wake(&queue);
  sp_thread_join(&consumer);

  sp_expect_eq(t, SMOKE_RECYCLE_PRODUCERS * count, drainer.count);
  return SP_OK;
}

typedef struct {
  sp_queue_t* queue;
  sp_queue_node_t* received;
} smoke_consumer_t;

static s32 smoke_consume_once(void* data) {
  smoke_consumer_t* consumer = (smoke_consumer_t*)data;
  consumer->received = sp_queue_wait(consumer->queue);
  return 0;
}

sp_test(queue, single_wake_unblocks_empty_wait) {
  sp_queue_t queue = sp_zero;
  sp_queue_init(&queue);

  smoke_consumer_t consumer = { .queue = &queue };
  sp_thread_t thread = sp_zero;
  sp_thread_init(&thread, smoke_consume_once, &consumer);

  sp_queue_wake(&queue);
  sp_thread_join(&thread);

  sp_expect(t, (void*)consumer.received == SP_NULLPTR);
  return SP_OK;
}
