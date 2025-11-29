#include <stdint.h>

typedef int32_t s32;

typedef struct {
  s32 value;
} spn_build_config_t;

spn_build_config_t spn_build_fn(void) {
  spn_build_config_t config = {0};
  config.value = 42;
  return config;
}
