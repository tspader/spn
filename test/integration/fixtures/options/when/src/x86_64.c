#include "include/spum.h"

#if GATE_X86_64 != 69
#error GATE_X86_64
#endif

#if defined(GATE_AARCH64) || defined(GATE_WASM32)
#error GATE_EXCLUSION
#endif

#if SPUM_FLAG != 69
#error SPUM_FLAG
#endif

int wiring() {
  return spum();
}
