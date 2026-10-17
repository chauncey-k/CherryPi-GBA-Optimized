#ifndef CHERRYPI_PLATFORM_H
#define CHERRYPI_PLATFORM_H
#include <SDL.h>
void cherrypi_input_init(void);
int cherrypi_platform_test(void);
int cherrypi_poll_event(SDL_Event *event, int menu);
SDL_Surface *cherrypi_video_mode(int w, int h, int depth, Uint32 flags);
int cherrypi_present(SDL_Surface *surface);
#endif
