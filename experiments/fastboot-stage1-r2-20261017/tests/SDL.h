#ifndef TEST_SDL_H
#define TEST_SDL_H
#include <stdint.h>
typedef uint32_t Uint32;
typedef uint16_t Uint16;
typedef enum { SDLK_UNKNOWN,SDLK_UP,SDLK_DOWN,SDLK_LEFT,SDLK_RIGHT,SDLK_RCTRL,SDLK_LCTRL,SDLK_ESCAPE,SDLK_LALT,SDLK_RETURN } SDLKey;
enum { SDL_QUIT=12,SDL_KEYDOWN=2,SDL_KEYUP=3,SDL_INIT_VIDEO=1,SDL_INIT_TIMER=2 };
typedef struct { int type; struct { struct { SDLKey sym; } keysym; } key; } SDL_Event;
typedef struct { int x,y,w,h; } SDL_Rect;
typedef struct { void *pixels; int pitch; } SDL_Surface;
int SDL_Init(unsigned flags);
void SDL_Quit(void);
const char *SDL_GetError(void);
Uint32 SDL_GetTicks(void);
void SDL_Delay(unsigned ms);
int SDL_FillRect(SDL_Surface *s,SDL_Rect *r,Uint32 color);
#endif
