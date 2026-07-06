#include "engine.h"
#include "sdl_mixer.h"

int engine_start(void) {
  return mixer_open();
}
