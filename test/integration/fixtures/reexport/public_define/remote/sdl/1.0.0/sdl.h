#ifndef SDL_H
#define SDL_H

#ifdef SDL_OPENGL
#define SDL_HAS_GL 1
#else
#define SDL_HAS_GL 0
#endif

int sdl_gl(void);

#endif
