/* One bounded image cache; no I/O until selection is stable. GPL-2.0-or-later. */
#define _DEFAULT_SOURCE
#include "cover.h"
#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <stdint.h>
#ifndef COVER_DIR
#define COVER_DIR "/mnt/games/covers"
#endif
#define PNG_MEMORY_LIMIT (2u*1024u*1024u)
typedef union { struct { size_t size; } s; long double align; } Allocation;
typedef struct {
    FILE *file;
    png_structp png;
    png_infop info;
    unsigned char *rgba;
    size_t used;
} Decoder;
static png_voidp allocate(png_structp png, png_alloc_size_t size) {
    Decoder *d=png_get_mem_ptr(png); Allocation *p;
    if(size>PNG_MEMORY_LIMIT || d->used>PNG_MEMORY_LIMIT-size) return NULL;
    p=malloc(sizeof(*p)+size); if(!p)return NULL;
    p->s.size=size;d->used+=size;return p+1;
}
static void deallocate(png_structp png,png_voidp mem) {
    Decoder *d=png_get_mem_ptr(png); Allocation *p;
    if(!mem)return;
    p=(Allocation*)mem-1;d->used-=p->s.size;free(p);
}
static void png_error_quiet(png_structp png,png_const_charp message) { (void)message;png_longjmp(png,1); }
static void png_warning_quiet(png_structp png,png_const_charp message) { (void)png;(void)message; }
int cover_decode(const char *file,Cover *c) {
    Decoder *d=calloc(1,sizeof(*d));struct stat st;volatile int ok=0;
    png_uint_32 w,h;int depth,type,passes,pass,x,y;png_bytep rows[512];
    c->width=c->height=0;
    if(!d)return 0;
    d->file=fopen(file,"rb");
    if(!d->file || fstat(fileno(d->file),&st) || !S_ISREG(st.st_mode) || st.st_size<33 || st.st_size>512*1024)goto done;
    d->png=png_create_read_struct_2(PNG_LIBPNG_VER_STRING,NULL,png_error_quiet,png_warning_quiet,d,allocate,deallocate);
    if(!d->png)goto done;
    d->info=png_create_info_struct(d->png);if(!d->info)goto done;
    if(setjmp(png_jmpbuf(d->png)))goto done;
    png_set_user_limits(d->png,512,512);
    png_set_chunk_malloc_max(d->png,65536);png_set_chunk_cache_max(d->png,8);
    png_set_keep_unknown_chunks(d->png,PNG_HANDLE_CHUNK_NEVER,NULL,0);
    png_init_io(d->png,d->file);png_read_info(d->png,d->info);
    png_get_IHDR(d->png,d->info,&w,&h,&depth,&type,NULL,NULL,NULL);
    if(!w || !h || w>512 || h>512)goto done;
    if(depth==16)png_set_strip_16(d->png);
    if(type==PNG_COLOR_TYPE_PALETTE)png_set_palette_to_rgb(d->png);
    if(type==PNG_COLOR_TYPE_GRAY && depth<8)png_set_expand_gray_1_2_4_to_8(d->png);
    if(png_get_valid(d->png,d->info,PNG_INFO_tRNS))png_set_tRNS_to_alpha(d->png);
    if(type==PNG_COLOR_TYPE_GRAY || type==PNG_COLOR_TYPE_GRAY_ALPHA)png_set_gray_to_rgb(d->png);
    if(!(type&PNG_COLOR_MASK_ALPHA) && !png_get_valid(d->png,d->info,PNG_INFO_tRNS))png_set_add_alpha(d->png,255,PNG_FILLER_AFTER);
    passes=png_set_interlace_handling(d->png);png_read_update_info(d->png,d->info);
    if(png_get_rowbytes(d->png,d->info)!=(size_t)w*4)goto done;
    d->rgba=calloc((size_t)w*h,4);if(!d->rgba)goto done;
    for(y=0;y<(int)h;y++)rows[y]=d->rgba+(size_t)y*w*4;
    for(pass=0;pass<passes;pass++)png_read_rows(d->png,rows,NULL,h);
    png_read_end(d->png,NULL);
    if(w*COVER_H>h*COVER_W) { c->width=COVER_W;c->height=h*COVER_W/w; }
    else { c->height=COVER_H;c->width=w*COVER_H/h; }
    if(c->width<1)c->width=1;
    if(c->height<1)c->height=1;
    for(y=0;y<c->height;y++)for(x=0;x<c->width;x++) {
        unsigned char *p=d->rgba+((size_t)(y*h/c->height)*w+x*w/c->width)*4;
        unsigned a=p[3],r=(p[0]*a+16*(255-a)+127)/255,g=(p[1]*a+24*(255-a)+127)/255,b=(p[2]*a+24*(255-a)+127)/255;
        c->pixels[y*COVER_W+x]=(uint16_t)((r>>3)<<11|(g>>2)<<5|(b>>3));
    }
    ok=1;
done:
    if(d->png)png_destroy_read_struct(&d->png,d->info?&d->info:NULL,NULL);
    if(d->file)fclose(d->file);
    free(d->rgba);free(d);
    if(!ok)c->width=c->height=0;
    return ok;
}
void cover_select(Cover *c,const char *rom,uint32_t now) {
    if(!rom) { c->rom[0]=c->path[0]=0;c->state=COVER_EMPTY;c->width=c->height=0;return; }
    if(!strcmp(c->rom,rom))return;
    if(strlen(rom)>=sizeof(c->rom)) { cover_select(c,NULL,now);return; }
    strcpy(c->rom,rom);c->path[0]=0;c->state=COVER_WAIT;c->width=c->height=0;c->deadline=now+180;
}
int cover_tick(Cover *c,uint32_t now) {
    char stem[PATH_MAX],*slash,*dot;int found=0;
    if(c->state!=COVER_WAIT || (int32_t)(now-c->deadline)<0)return 0;
    strcpy(stem,c->rom);slash=strrchr(stem,'/');dot=strrchr(slash?slash+1:stem,'.');if(dot)*dot=0;
    if(slash) {
        *slash=0;
        if(snprintf(c->path,sizeof(c->path),"%s/covers/%s.png",stem,slash+1)<(int)sizeof(c->path)) {
            struct stat st;if(!stat(c->path,&st) && S_ISREG(st.st_mode))found=1;
        }
    }
    if(!found) {
        struct stat st;
        if(snprintf(c->path,sizeof(c->path),"%s/%s.png",COVER_DIR,slash?slash+1:stem)>=(int)sizeof(c->path))c->path[0]=0;
        else if(!stat(c->path,&st) && S_ISREG(st.st_mode))found=1;
    }
    if(!found)c->state=COVER_MISSING;
    else { c->reads++;c->state=cover_decode(c->path,c)?COVER_READY:COVER_INVALID; }
    return 1;
}
