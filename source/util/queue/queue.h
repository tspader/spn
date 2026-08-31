#ifndef SP_QUEUE_H
#define SP_QUEUE_H

#include "sp.h"

typedef struct sp_queue_node_t sp_queue_node_t;
struct sp_queue_node_t {
  sp_queue_node_t* next;
};

typedef struct {
  sp_atomic_ptr_t stack;
  sp_queue_node_t* fifo;
  sp_atomic_u32_t pushes;
  sp_atomic_u32_t poked;
} sp_queue_t;

SP_API void             sp_queue_init(sp_queue_t* queue);
SP_API void             sp_queue_push(sp_queue_t* queue, sp_queue_node_t* node);
SP_API sp_queue_node_t* sp_queue_pop(sp_queue_t* queue);
SP_API sp_queue_node_t* sp_queue_wait(sp_queue_t* queue);
SP_API void             sp_queue_wake(sp_queue_t* queue);

#if defined(SP_IMPLEMENTATION) && !defined(SP_QUEUE_IMPLEMENTATION)
  #define SP_QUEUE_IMPLEMENTATION
#endif

#ifdef SP_QUEUE_IMPLEMENTATION

void sp_queue_init(sp_queue_t* queue) {
  *queue = sp_zero_s(sp_queue_t);
}

void sp_queue_push(sp_queue_t* queue, sp_queue_node_t* node) {
  sp_queue_node_t* head = (sp_queue_node_t*)sp_atomic_ptr_load(&queue->stack, SP_ATOMIC_RELAXED);
  while (true) {
    node->next = head;
    if (sp_atomic_ptr_cas(&queue->stack, head, node, SP_ATOMIC_RELEASE)) {
      break;
    }
    head = (sp_queue_node_t*)sp_atomic_ptr_load(&queue->stack, SP_ATOMIC_RELAXED);
  }

  sp_atomic_u32_add(&queue->pushes, 1, SP_ATOMIC_RELEASE);
  sp_sys_futex_wake(&queue->pushes);
}

sp_queue_node_t* sp_queue_pop(sp_queue_t* queue) {
  if (!queue->fifo) {
    sp_queue_node_t* batch = (sp_queue_node_t*)sp_atomic_ptr_exchange(&queue->stack, SP_NULLPTR, SP_ATOMIC_ACQUIRE);
    while (batch) {
      sp_queue_node_t* next = batch->next;
      batch->next = queue->fifo;
      queue->fifo = batch;
      batch = next;
    }
  }

  sp_queue_node_t* node = queue->fifo;
  if (node) {
    queue->fifo = node->next;
  }
  return node;
}

sp_queue_node_t* sp_queue_wait(sp_queue_t* queue) {
  sp_queue_node_t* node = sp_queue_pop(queue);
  if (node) {
    return node;
  }

  u32 generation = sp_atomic_u32_load(&queue->pushes, SP_ATOMIC_ACQUIRE);
  node = sp_queue_pop(queue);
  if (node) {
    return node;
  }

  if (sp_atomic_u32_exchange(&queue->poked, 0, SP_ATOMIC_ACQUIRE)) {
    return SP_NULLPTR;
  }

  sp_sys_futex_wait(&queue->pushes, generation, SP_NULLPTR);
  return sp_queue_pop(queue);
}

void sp_queue_wake(sp_queue_t* queue) {
  sp_atomic_u32_store(&queue->poked, 1, SP_ATOMIC_RELEASE);
  sp_atomic_u32_add(&queue->pushes, 1, SP_ATOMIC_RELEASE);
  sp_sys_futex_wake(&queue->pushes);
}

#endif

#endif
