#ifndef IMAGEIO_H
#define IMAGEIO_H

#include <SDL.h>
#include <png.h>
#include <assert.h>

extern SDL_Surface *loadPNG(const char* Path, uint32_t MaxWidth, uint32_t MaxHeight);

#endif
