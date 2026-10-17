#include "../source/cover.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>
int main(void) {
    Cover c={0};unsigned before;int i;
    const char *valid[]={"rgb","rgba","palette","gray","gray16","interlaced","wide","tall"};
    for(i=0;i<8;i++) { char p[80];snprintf(p,sizeof(p),"fixtures/%s.png",valid[i]);assert(cover_decode(p,&c));assert(c.width>0 && c.width<=COVER_W && c.height>0 && c.height<=COVER_H); }
    assert(cover_decode("fixtures/rgb.png",&c));assert(c.width==112 && c.height==112 && c.pixels[0]==0xf800);
    assert(cover_decode("fixtures/rgba.png",&c));assert(c.pixels[0]==0x10c3);
    assert(!cover_decode("fixtures/oversized.png",&c));assert(!c.width);
    assert(!cover_decode("fixtures/truncated.png",&c));
    assert(!cover_decode("fixtures/bad-crc.png",&c));
    assert(!cover_decode("fixtures/huge-file.png",&c));
    assert(!cover_decode("fixtures",&c));
    cover_select(&c,"roms/Test.gba",100);before=c.reads;
    assert(c.state==COVER_WAIT && !cover_tick(&c,279) && c.reads==before);
    assert(cover_tick(&c,280) && c.state==COVER_READY && c.reads==before+1);
    assert(!strcmp(c.path,"roms/covers/Test.png"));assert(c.pixels[0]==0xf800);
    cover_select(&c,"roms/Test.gba",1000);assert(!cover_tick(&c,2000) && c.reads==before+1);
    cover_select(&c,"roms/sub/Global.gba",2000);assert(cover_tick(&c,2180) && c.state==COVER_READY && !strcmp(c.path,"covers/Global.png"));
    cover_select(&c,"roms/sub/Bad.gba",3000);assert(cover_tick(&c,3180) && c.state==COVER_INVALID);
    cover_select(&c,"roms/Missing.gba",4000);assert(cover_tick(&c,4180) && c.state==COVER_MISSING);
    before=c.reads;assert(!cover_tick(&c,5000) && c.reads==before);
    cover_select(&c,"roms/Test.gba",5000);cover_select(&c,"roms/Missing.gba",5100);
    assert(!cover_tick(&c,5180) && c.reads==before);assert(cover_tick(&c,5280) && c.reads==before);
    cover_select(&c,NULL,6000);assert(c.state==COVER_EMPTY && !cover_tick(&c,7000));
    cover_select(&c,"roms/Test.gba",UINT32_MAX-90);assert(!cover_tick(&c,20));assert(cover_tick(&c,90));
    puts("PASS PNG formats/alpha/aspect/bounds/corruption; local/global lookup; debounce/cache/missing/wraparound");
    return 0;
}
