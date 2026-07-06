#include "sdl_mixer.h"

int main() {
  if (SDL_HAS_GL != 1) return 1;
  if (sdl_gl() != 1) return 2;
  if (mixer_gl() != 1) return 3;
  return 0;
}
