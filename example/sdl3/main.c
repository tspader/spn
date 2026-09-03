#include <SDL3/SDL.h>

int main(void) {
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    SDL_Log("SDL_Init: %s", SDL_GetError());
    return 1;
  }

  SDL_Window* window = NULL;
  SDL_Renderer* renderer = NULL;
  if (!SDL_CreateWindowAndRenderer("spn + sdl3", 800, 450, 0, &window, &renderer)) {
    SDL_Log("SDL_CreateWindowAndRenderer: %s", SDL_GetError());
    return 1;
  }

  SDL_FRect box = { 0, 175, 100, 100 };
  float velocity = 4;

  bool running = true;
  while (running) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_EVENT_QUIT) {
        running = false;
      }
    }

    box.x += velocity;
    if (box.x < 0 || box.x > 700) velocity = -velocity;

    SDL_SetRenderDrawColor(renderer, 30, 30, 40, 255);
    SDL_RenderClear(renderer);
    SDL_SetRenderDrawColor(renderer, 240, 90, 60, 255);
    SDL_RenderFillRect(renderer, &box);
    SDL_RenderPresent(renderer);
  }

  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}
