#include "core/iwasm/include/wasm_export.h"

int main(void) {
  if (!wasm_runtime_init()) {
    return 1;
  }

  uint32_t major, minor, patch;
  wasm_runtime_get_version(&major, &minor, &patch);

  wasm_runtime_destroy();

  return major > 0 ? 0 : 1;
}
