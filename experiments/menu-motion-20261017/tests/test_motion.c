#include "../source/motion.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    Motion m={0};unsigned t,frames=1;
    assert(sizeof(m)<=32 && !motion_tick(&m,90000));
    motion_begin(&m,1000,1,1);assert(motion_offset(&m,1000)==8);
    assert(motion_offset(&m,1040)>0 && motion_offset(&m,1040)<8);
    for(t=1010;t<=1120;t+=10)frames+=motion_tick(&m,t);
    assert(frames==4 && !m.page && !motion_offset(&m,1120));
    for(t=1130;t<5000;t+=10)assert(!motion_tick(&m,t));
    motion_begin(&m,5000,1,-1);assert(motion_offset(&m,5000)==-8);
    motion_begin(&m,5010,1,1);assert(motion_offset(&m,5010)==8);
    motion_begin(&m,5020,0,0);assert(motion_focus(&m,5020)==0 && motion_focus(&m,5070)==128);
    motion_begin(&m,5030,0,0);assert(motion_focus(&m,5030)==0);
    assert(motion_tick(&m,5130) && !m.focus && !m.page);
    motion_begin(&m,UINT32_MAX-50,1,1);
    assert(motion_offset(&m,10)>0 && motion_tick(&m,70) && !m.page);
    motion_begin(&m,UINT32_MAX-50,0,0);assert(motion_focus(&m,0)>0);
    assert(motion_tick(&m,50) && !m.focus);
    motion_reset(&m);assert(!motion_tick(&m,10000));
    puts("PASS bounded frame count, easing/endpoints, reversal/retarget, idle and tick wraparound");
    return 0;
}
