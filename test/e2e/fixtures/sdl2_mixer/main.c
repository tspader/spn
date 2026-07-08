#include <SDL.h>
#include <SDL_mixer.h>

int main(int num_args, char** args) {
  SDL_version compiled;
  SDL_MIXER_VERSION(&compiled);
  if (compiled.major != 2) return 1;
  Mix_Init(0);
  Mix_Quit();
  return 0;
}
