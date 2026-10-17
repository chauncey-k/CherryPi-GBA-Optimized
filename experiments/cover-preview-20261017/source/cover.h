/* Selected-game PNG preview. GPL-2.0-or-later. */
#ifndef CHERRYPI_COVER_H
#define CHERRYPI_COVER_H
#include <stdint.h>
#include <limits.h>
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif
#define COVER_W 112
#define COVER_H 144
enum { COVER_EMPTY, COVER_WAIT, COVER_READY, COVER_MISSING, COVER_INVALID };
typedef struct {
    char rom[PATH_MAX], path[PATH_MAX];
    uint16_t pixels[COVER_W*COVER_H];
    uint32_t deadline;
    unsigned reads;
    int state, width, height;
} Cover;
/* Prefer ROM-directory/covers/basename.png; fall back to GAMES/covers/basename.png. */
void cover_select(Cover *c, const char *rom, uint32_t now);
int cover_tick(Cover *c, uint32_t now);
int cover_decode(const char *file, Cover *c);
#endif
