#include <SDL.h>

int main(int num_args, char** args) {
  SDL_version version;
  SDL_GetVersion(&version);
  if (version.major != 2) return 1;
  if (SDL_Init(SDL_INIT_TIMER | SDL_INIT_EVENTS)) return 2;
  SDL_Quit();
  return 0;
}
