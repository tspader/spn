#include "sdl.h"
#include "sdl_mixer.h"

int mixer_open(void) {
  return sdl_init();
}
