#include "core/core.h"

void spn_wake_ring(spn_wake_t* wake) {
  if (!wake->fn) {
    return;
  }
  if (sp_atomic_u32_cas(&wake->signaled, 0, 1, SP_ATOMIC_SEQ_CST)) {
    wake->fn(wake->data);
  }
}

void spn_wake_pulse(spn_wake_t* wake) {
  if (!wake->fn) {
    return;
  }
  sp_atomic_u32_store(&wake->signaled, 1, SP_ATOMIC_SEQ_CST);
  wake->fn(wake->data);
}

void spn_wake_rearm(spn_wake_t* wake) {
  sp_atomic_u32_store(&wake->signaled, 0, SP_ATOMIC_SEQ_CST);
}
