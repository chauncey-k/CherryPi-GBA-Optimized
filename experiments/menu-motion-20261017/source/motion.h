/* Bounded, nonblocking UI motion. No image buffers or timer threads. GPL-2.0-or-later. */
#ifndef CHERRYPI_MOTION_H
#define CHERRYPI_MOTION_H
#include <stdint.h>
#include <string.h>
enum { MOTION_PAGE_MS=120, MOTION_FOCUS_MS=100, MOTION_FRAME_MS=40 };
typedef struct {
    uint32_t page_at, focus_at, frame_at;
    int page, focus, direction;
} Motion;
static void motion_reset(Motion *m) { memset(m,0,sizeof(*m)); }
static void motion_begin(Motion *m,uint32_t now,int page,int direction) {
    m->frame_at=now;
    if(page) { m->page=1;m->page_at=now;m->direction=direction;m->focus=0; }
    else { m->focus=1;m->focus_at=now; }
}
static int motion_tick(Motion *m,uint32_t now) {
    int finished=0;
    if(m->page && (uint32_t)(now-m->page_at)>=MOTION_PAGE_MS) { m->page=0;finished=1; }
    if(m->focus && (uint32_t)(now-m->focus_at)>=MOTION_FOCUS_MS) { m->focus=0;finished=1; }
    if(finished || ((m->page || m->focus) && (uint32_t)(now-m->frame_at)>=MOTION_FRAME_MS)) {
        m->frame_at=now;return 1;
    }
    return 0;
}
static int motion_offset(const Motion *m,uint32_t now) {
    unsigned elapsed=(uint32_t)(now-m->page_at),left;
    if(!m->page || elapsed>=MOTION_PAGE_MS)return 0;
    left=MOTION_PAGE_MS-elapsed;
    return m->direction*(int)((8*left*left+MOTION_PAGE_MS*MOTION_PAGE_MS-1)/(MOTION_PAGE_MS*MOTION_PAGE_MS));
}
static unsigned motion_focus(const Motion *m,uint32_t now) {
    unsigned elapsed=(uint32_t)(now-m->focus_at);
    if(!m->focus || elapsed>=MOTION_FOCUS_MS)return 256;
    return elapsed*256/MOTION_FOCUS_MS;
}
#endif
