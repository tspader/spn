#include "sdl_mixer.h"

int main() {
  if (mixer_open() != SDL_INIT_OK) return 1;
  return 0;
}
