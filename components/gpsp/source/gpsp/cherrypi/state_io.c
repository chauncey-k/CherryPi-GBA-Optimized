/* Native staged load and atomic save. GPL-2.0-or-later. */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "state_io.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <limits.h>
#include "../sha1.h"
typedef struct { void *field; size_t offset, size; } Segment;
static unsigned char snapshot[CP_STATE_PAYLOAD], incoming[CP_STATE_PAYLOAD];
static Segment segments[160];
static size_t used, count;
static uint32_t schema;
static int layout_bad, staged, legacy, rom_valid;
static unsigned char rom_hash[20];
static char message[144];
static const unsigned char magic[8]={'C','P','S','T','A','T','E','2'};
static uint32_t get32(const unsigned char *p) { return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24; }
static void put32(unsigned char *p,uint32_t n) { p[0]=n;p[1]=n>>8;p[2]=n>>16;p[3]=n>>24; }
static uint32_t crc(const void *data,size_t size) {
    const unsigned char *p=data;uint32_t n=~0u;size_t i;int j;
    for(i=0;i<size;i++) { n^=p[i];for(j=0;j<8;j++) n=(n>>1)^(0xedb88320u & (0u-(n&1))); }
    return ~n;
}
const char *cp_state_message(void) { return message; }
void cp_state_clear_message(void) { message[0]=0; }
int cp_state_fail(const char *text) { staged=0;snprintf(message,sizeof(message),"%s",text);fprintf(stderr,"Savestate: %s\n",message);return 0; }
static int io_fail(const char *operation) {
    int e=errno;char b[144];snprintf(b,sizeof(b),"%s: %s (%d)",operation,strerror(e),e);return cp_state_fail(b);
}
int cp_state_begin(size_t prefix) {
    used=prefix;count=0;schema=2166136261u;layout_bad=prefix>CP_STATE_PAYLOAD;staged=legacy=0;message[0]=0;
    memset(snapshot,0,sizeof(snapshot));return !layout_bad;
}
void cp_state_pack(void *field,size_t size,const char *name) {
    const unsigned char *p=(const unsigned char*)name;unsigned char sizes[8];size_t i;
    if(layout_bad)return;
    if(!field || size>CP_STATE_PAYLOAD-used || count>=sizeof(segments)/sizeof(segments[0])) { layout_bad=1;return; }
    segments[count++]=(Segment){field,used,size};
    memcpy(snapshot+used,field,size);
    while(*p) { schema^=*p++;schema*=16777619u; }
    put32(sizes,(uint32_t)used);put32(sizes+4,(uint32_t)size);
    for(i=0;i<8;i++) { schema^=sizes[i];schema*=16777619u; }
    used+=size;
}
unsigned char *cp_state_snapshot(void) { return snapshot; }
int cp_state_layout_ok(void) { return !layout_bad && count && used<=CP_STATE_PAYLOAD ? 1:cp_state_fail("State layout is incompatible"); }
int cp_state_legacy(void) { return legacy; }
int cp_state_set_rom(const char *path) {
    FILE *f;sha1nfo sha;unsigned char buffer[32768];size_t n;int ok;
    rom_valid=0;f=fopen(path,"rb");if(!f)return io_fail("ROM identity open");
    sha1_init(&sha);
    while((n=fread(buffer,1,sizeof(buffer),f))) sha1_write(&sha,buffer,n);
    ok=!ferror(f);if(fclose(f))ok=0;
    if(!ok)return io_fail("ROM identity read");
    memcpy(rom_hash,sha1_result(&sha),20);rom_valid=1;return 1;
}
/* One bounded read, including close, precedes every state mutation. */
static int read_file(const char *path,unsigned char *data,unsigned char header[CP_STATE_HEADER],int *old) {
    FILE *f=fopen(path,"rb");struct stat s;int ok;
    if(!f) { if(errno==ENOENT)return cp_state_fail("No savestate in this slot");return io_fail("State open"); }
    if(fstat(fileno(f),&s)) { int e=errno;fclose(f);errno=e;return io_fail("State stat"); }
    if(s.st_size!=CP_STATE_LEGACY && s.st_size!=CP_STATE_PAYLOAD+CP_STATE_HEADER) { fclose(f);return cp_state_fail("State incomplete or incompatible"); }
    *old=s.st_size==CP_STATE_LEGACY;
    if(*old) {
        ok=fread(data,1,CP_STATE_LEGACY,f)==CP_STATE_LEGACY;
        /* Never accept a truncated new-format file as a legacy state. */
        if(ok && !memcmp(data,magic,8)) { fclose(f);return cp_state_fail("State incomplete or incompatible"); }
    } else {
        ok=fread(header,1,CP_STATE_HEADER,f)==CP_STATE_HEADER;
        if(ok)ok=fread(data,1,CP_STATE_PAYLOAD,f)==CP_STATE_PAYLOAD;
    }
    if(ok && (fgetc(f)!=EOF || ferror(f)))ok=0;
    if(fclose(f))ok=0;
    return ok?1:cp_state_fail("State read failed; game unchanged");
}
static int check_header(const unsigned char *data,const unsigned char *header) {
    if(memcmp(header,magic,8) || get32(header+8)!=1 || get32(header+60)!=crc(header,60))return cp_state_fail("State version/header invalid");
    if(get32(header+20)!=crc(data,CP_STATE_PAYLOAD))return cp_state_fail("State checksum failed");
    return 1;
}
const void *cp_state_field(const void *field,size_t size) {
    size_t i;for(i=0;i<count;i++)if(segments[i].field==field && segments[i].size==size)return incoming+segments[i].offset;
    return NULL;
}
static const char *leaf(const char *p) { const char *s=strrchr(p,'/');return s?s+1:p; }
int cp_state_stage(const char *path,const char *live_name) {
    unsigned char header[CP_STATE_HEADER];const char *saved;
    staged=0;if(!cp_state_layout_ok())return 0;
    /* The old ARM time64 writer omitted the final 4 bytes. Its reader kept
     * that part of affine_reference_y unchanged; preserve that legacy rule. */
    memcpy(incoming,snapshot,sizeof(incoming));
    if(!read_file(path,incoming,header,&legacy))return 0;
    if(!legacy) {
        if(!check_header(incoming,header))return 0;
        if(get32(header+12)!=used || get32(header+16)!=schema)return cp_state_fail("State layout is incompatible");
        if(!rom_valid || memcmp(rom_hash,header+24,20))return cp_state_fail("State belongs to a different ROM");
    }
    saved=cp_state_field(live_name,512);
    if(!saved || !memchr(saved,0,512) || !saved[0])return cp_state_fail("State ROM name is invalid");
    if(strcmp(leaf(saved),leaf(live_name)))return cp_state_fail("State belongs to a different game");
    staged=1;return 1;
}
void cp_state_commit(void) {
    size_t i;if(!staged)return;
    for(i=0;i<count;i++)memcpy(segments[i].field,incoming+segments[i].offset,segments[i].size);
    staged=0;
}
static int write_all(int fd,const void *buffer,size_t size) {
    const unsigned char *p=buffer;
    while(size) { ssize_t n=write(fd,p,size);if(n<0 && errno==EINTR)continue;if(n<=0){if(!n)errno=EIO;return 0;}p+=n;size-=n; }
    return 1;
}
int cp_state_save(const char *path) {
    unsigned char header[CP_STATE_HEADER]={0};char temp[PATH_MAX],parent[PATH_MAX],*slash;
    int fd,dirfd,e;struct stat old;
    if(!cp_state_layout_ok())return 0;
    if(!rom_valid)return cp_state_fail("ROM identity unavailable; save cancelled");
    if(snprintf(temp,sizeof(temp),"%s.tmp.XXXXXX",path)>=(int)sizeof(temp))return cp_state_fail("State path too long");
    memcpy(header,magic,8);put32(header+8,1);put32(header+12,used);put32(header+16,schema);
    put32(header+20,crc(snapshot,sizeof(snapshot)));memcpy(header+24,rom_hash,20);put32(header+60,crc(header,60));
    fd=mkstemp(temp);if(fd<0)return io_fail("Save create");
    if(fchmod(fd,stat(path,&old)?0600:old.st_mode&0777) || !write_all(fd,header,sizeof(header)) || !write_all(fd,snapshot,sizeof(snapshot)) || fsync(fd)) {
        e=errno;close(fd);unlink(temp);errno=e;return io_fail("Save write/sync");
    }
    if(close(fd)) { e=errno;unlink(temp);errno=e;return io_fail("Save close"); }
    if(rename(temp,path)) { e=errno;unlink(temp);errno=e;return io_fail("Save replace"); }
    snprintf(parent,sizeof(parent),"%s",path);slash=strrchr(parent,'/');
    if(slash) { if(slash==parent)slash[1]=0;else *slash=0; } else strcpy(parent,".");
    dirfd=open(parent,O_RDONLY|O_DIRECTORY);
    if(dirfd<0 || fsync(dirfd)) {
        fprintf(stderr,"Savestate: replacement complete; directory sync unavailable: %s\n",strerror(errno));
        snprintf(message,sizeof(message),"Saved; directory sync unavailable");
    }
    if(dirfd>=0)close(dirfd);
    return 1;
}
int cp_state_preview(const char *path,void *pixels,void *timestamp,size_t time_size) {
    unsigned char header[CP_STATE_HEADER];char previous[sizeof(message)];int old,ok=0;
    memcpy(previous,message,sizeof(message));
    if(time_size<=16 && read_file(path,incoming,header,&old) && (old || check_header(incoming,header))) {
        memcpy(pixels,incoming,CP_STATE_PREVIEW);memcpy(timestamp,incoming+CP_STATE_PREVIEW,time_size);ok=1;
    }
    /* Previewing a slot must not overwrite the last save/load failure. */
    memcpy(message,previous,sizeof(message));return ok;
}
