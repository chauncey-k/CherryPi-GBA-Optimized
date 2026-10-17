#define _GNU_SOURCE
#include "state_io.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
static char rom_name[512]="/roms/Test.gba";
static unsigned cpu=42;
static unsigned char memory[64];
static int fail_write,fail_sync,fail_rename,fail_close,short_write;
ssize_t __real_write(int,const void*,size_t);
int __real_fsync(int);
int __real_rename(const char*,const char*);
int __real_close(int);
ssize_t __wrap_write(int fd,const void *p,size_t n) {
    if(fail_write) { errno=ENOSPC;return -1; }
    if(short_write && n>7)n=7;
    return __real_write(fd,p,n);
}
int __wrap_fsync(int fd) { if(fail_sync){errno=EIO;return -1;}return __real_fsync(fd); }
int __wrap_rename(const char *a,const char *b) { if(fail_rename){errno=EROFS;return -1;}return __real_rename(a,b); }
int __wrap_close(int fd) { int r=__real_close(fd);if(fail_close){errno=EIO;return -1;}return r; }
static void pack(const char *field) {
    assert(cp_state_begin(CP_STATE_PREVIEW+8));
    cp_state_pack(&cpu,sizeof(cpu),field);
    cp_state_pack(rom_name,sizeof(rom_name),"gamepak_filename");
    cp_state_pack(memory,sizeof(memory),"memory");
    assert(cp_state_layout_ok());
}
static void put(const char *p,const void *b,size_t n) { FILE *f=fopen(p,"wb");assert(f);assert(fwrite(b,1,n,f)==n);assert(!fclose(f)); }
static unsigned char *read_file(const char *p,size_t *n) {
    FILE *f=fopen(p,"rb");long size;unsigned char *b;assert(f);assert(!fseek(f,0,SEEK_END));size=ftell(f);assert(size>=0);rewind(f);
    b=malloc(size+1);assert(b);assert(fread(b,1,size,f)==(size_t)size);fclose(f);*n=size;return b;
}
static void unchanged(const unsigned char *old,size_t n) { size_t m;unsigned char *p=read_file("state.svs",&m);assert(m==n && !memcmp(p,old,n));free(p); }
static void rejects(const char *path,const char *word) {
    unsigned before=cpu;char name[512];memcpy(name,rom_name,512);pack("cpu");
    assert(!cp_state_stage(path,rom_name));assert(strstr(cp_state_message(),word));
    cp_state_commit();assert(cpu==before && !memcmp(name,rom_name,512));
}
int main(void) {
    unsigned char *good,*bad,preview[CP_STATE_PREVIEW];size_t n;unsigned long long timestamp;
    put("rom.gba","first ROM content",17);assert(cp_state_set_rom("rom.gba"));
    memset(memory,0x5a,sizeof(memory));pack("cpu");assert(cp_state_save("state.svs"));
    good=read_file("state.svs",&n);assert(n==CP_STATE_PAYLOAD+CP_STATE_HEADER);
    cpu=99;memset(memory,0xaa,sizeof(memory));pack("cpu");assert(cp_state_stage("state.svs",rom_name));
    assert(cpu==99 && memory[0]==0xaa);cp_state_commit();assert(cpu==42 && memory[0]==0x5a);
    puts("PASS staged restore: no machine mutation before commit");
    rejects("missing.svs","No savestate");
    put("truncated.svs",good,100);rejects("truncated.svs","incomplete");
    bad=malloc(n);assert(bad);memcpy(bad,good,n);bad[100000]^=1;put("bad.svs",bad,n);rejects("bad.svs","checksum");
    memcpy(bad,good,n);bad[8]=2;put("bad.svs",bad,n);rejects("bad.svs","version/header");
    pack("different_schema");assert(!cp_state_stage("state.svs",rom_name));assert(strstr(cp_state_message(),"layout"));
    put("rom.gba","other ROM content",17);assert(cp_state_set_rom("rom.gba"));rejects("state.svs","different ROM");
    put("rom.gba","first ROM content",17);assert(cp_state_set_rom("rom.gba"));
    strcpy(rom_name,"/other/Other.gba");rejects("state.svs","different game");strcpy(rom_name,"/roms/Test.gba");
    puts("PASS missing/truncated/version/checksum/layout/ROM mismatch: live machine unchanged");
    put("legacy.svs",good+CP_STATE_HEADER,CP_STATE_LEGACY);pack("cpu");assert(cp_state_stage("legacy.svs",rom_name));assert(cp_state_legacy());
    strcpy(rom_name,"/moved/Test.gba");pack("cpu");assert(cp_state_stage("legacy.svs",rom_name));
    /* Old relative-name states use the same filename slot, with no header. */
    memcpy(bad,good+CP_STATE_HEADER,CP_STATE_PAYLOAD);memset(bad+CP_STATE_PREVIEW+8+4,0,512);strcpy((char*)bad+CP_STATE_PREVIEW+8+4,"Test.gba");
    put("relative.svs",bad,CP_STATE_LEGACY);pack("cpu");assert(cp_state_stage("relative.svs",rom_name));
    memset(bad+CP_STATE_PREVIEW+8+4,'x',512);put("badlegacy.svs",bad,CP_STATE_LEGACY);rejects("badlegacy.svs","name");
    put("new-cut-to-legacy.svs",good,CP_STATE_LEGACY);rejects("new-cut-to-legacy.svs","incomplete");
    puts("PASS legacy relative/absolute/moved paths and invalid filename rejection");
    strcpy(rom_name,"/roms/Test.gba");cpu=77;pack("cpu");
    fail_write=1;assert(!cp_state_save("state.svs"));fail_write=0;unchanged(good,n);
    fail_sync=1;assert(!cp_state_save("state.svs"));fail_sync=0;unchanged(good,n);
    fail_rename=1;assert(!cp_state_save("state.svs"));fail_rename=0;unchanged(good,n);
    fail_close=1;assert(!cp_state_save("state.svs"));fail_close=0;unchanged(good,n);
    assert(!cp_state_save("missing-dir/state.svs"));unchanged(good,n);
    puts("PASS disk-full/write/sync/close/rename/create failures preserve original bytes");
    short_write=1;assert(cp_state_save("short.svs"));short_write=0;pack("cpu");assert(cp_state_stage("short.svs",rom_name));
    cp_state_fail("keep last operation error");assert(cp_state_preview("state.svs",preview,&timestamp,8));assert(!strcmp(cp_state_message(),"keep last operation error"));
    assert(!cp_state_preview("bad.svs",preview,&timestamp,8));assert(!strcmp(cp_state_message(),"keep last operation error"));
    puts("PASS short-write retry, checked preview and preserved error message");
    pack("cpu");cp_state_pack(memory,CP_STATE_PAYLOAD,"overflow");assert(!cp_state_layout_ok());
    {
        unsigned tail=123;size_t gap_size=CP_STATE_PAYLOAD-(CP_STATE_PREVIEW+8)-sizeof(cpu)-sizeof(rom_name)-sizeof(memory)-sizeof(tail);
        unsigned char *gap=calloc(gap_size,1),*bytes;size_t length;assert(gap);
        pack("cpu");cp_state_pack(gap,gap_size,"gap");cp_state_pack(&tail,sizeof(tail),"legacy_video_tail");
        assert(cp_state_save("full.svs"));bytes=read_file("full.svs",&length);
        put("old-short-tail.svs",bytes+CP_STATE_HEADER,CP_STATE_LEGACY);
        tail=456;pack("cpu");cp_state_pack(gap,gap_size,"gap");cp_state_pack(&tail,sizeof(tail),"legacy_video_tail");
        assert(cp_state_stage("old-short-tail.svs",rom_name));cp_state_commit();assert(tail==456);
        pack("cpu");cp_state_pack(gap,gap_size,"gap");cp_state_pack(&tail,sizeof(tail),"legacy_video_tail");
        assert(cp_state_stage("full.svs",rom_name));cp_state_commit();assert(tail==123);
        free(bytes);free(gap);
        puts("PASS exact legacy 4-byte omission compatibility; new format restores all bytes");
    }
    free(good);free(bad);return 0;
}
