"""Exercise the actual C loader, private mapping lifetime and guest mirrors."""
from pathlib import Path
import subprocess, tempfile
b=Path(__file__).resolve().parents[1]
s=(b/'source/gpsp/memory.c').read_text()
helpers=s[s.index('#ifdef CHERRYPI\n/* The private mapping'):s.index('\nchar gamepak_title[13]')]
allocator=s[s.index('void init_gamepak_buffer()'):s.index('\nvoid init_memory()',s.index('void init_gamepak_buffer()'))]
maps=s[s.index('void init_memory_gamepak()'):s.index('\nvoid init_gamepak_buffer()')]
prefix=r'''
#define _DEFAULT_SOURCE
#define CHERRYPI 1
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <errno.h>
typedef uint32_t u32; typedef int32_t s32; typedef uint8_t u8;
typedef struct { u32 page_timestamp,physical_index; } gamepak_swap_entry_type;
u8 *gamepak_rom,*memory_regions[16],*memory_map_read[0x2000];
u32 gamepak_ram_buffer_size,gamepak_ram_pages,gamepak_size;
gamepak_swap_entry_type *gamepak_memory_map;
FILE *gamepak_file_large;
void init_gamepak_buffer(void);
static u32 file_len(FILE *f) { struct stat st; assert(!fstat(fileno(f),&st));return st.st_size; }
#define file_open(tag,name,mode) FILE *tag=fopen(name,"rb")
#define file_check_valid(f) ((f)!=NULL)
#define file_close(f) assert(!fclose(f))
#define file_length(n,f) file_len(f)
#define file_read(f,p,n) assert(fread(p,1,n,f)==(n))
#define map_null(type,start,end) for(map_offset=(start)/0x8000;map_offset<(end)/0x8000;map_offset++)memory_map_read[map_offset]=NULL
#define map_region(type,start,end,mirror,region) for(map_offset=(start)/0x8000;map_offset<(end)/0x8000;map_offset++)memory_map_read[map_offset]=(region)+(map_offset%(mirror))*0x8000
static int fail_map;
static void *test_mmap(void *a,size_t n,int p,int flags,int fd,off_t o) {
 if(fail_map) { errno=ENOMEM;return MAP_FAILED; }
 return mmap(a,n,p,flags,fd,o);
}
#define mmap test_mmap
'''
main=r'''
int main(int argc,char **argv) {
 assert(argc==2);char *path=argv[1];
 FILE *f=fopen(path,"wb");assert(f);
 for(unsigned page=0;page<1024;page++) {
  u32 data[8192];for(unsigned i=0;i<8192;i++)data[i]=(page<<16)^i^0x183712ab;
  assert(fwrite(data,1,sizeof(data),f)==sizeof(data));
 }
 fclose(f);init_gamepak_buffer();assert(!cp_rom_mapping_size);
 assert(load_gamepak_raw(path)==32*1024*1024);
 assert(cp_rom_mapping_size==32*1024*1024 && !gamepak_memory_map && !gamepak_file_large);
 gamepak_size=cp_rom_mapping_size;cp_refresh_rom_regions();init_memory_gamepak();
 for(unsigned page=0;page<1024;page++) {
  for(unsigned alias=0x1000;alias<=0x1800;alias+=0x400)
   assert(memory_map_read[alias+page]==gamepak_rom+page*0x8000);
  u32 *data=(u32 *)(gamepak_rom+page*0x8000);
  for(unsigned i=0;i<8192;i++)assert(data[i]==((page<<16)^i^0x183712ab));
 }
 assert(memory_regions[8]==gamepak_rom && memory_regions[9]==gamepak_rom+0xFFFFFF);
 /* RTC writes stay private; source bytes never change. */
 u32 original=((u32*)gamepak_rom)[0xC4/4];
 memset(gamepak_rom+0xC4,0x55,12);
 f=fopen(path,"rb");assert(f);assert(!fseek(f,0xC4,SEEK_SET));u32 disk;
 assert(fread(&disk,4,1,f)==1 && disk==original);fclose(f);
 /* Simulate clean page eviction without changing guest pointers. */
 u8 *stable=gamepak_rom;assert(!madvise(stable+0x8000,31*1024*1024,MADV_DONTNEED));
 for(unsigned page=1;page<1024;page++)
  assert(*(u32*)(stable+page*0x8000)==((page<<16)^0x183712ab));
 assert(gamepak_rom==stable && gamepak_rom[0xC4]==0x55);
 /* Failed mmap keeps the previous valid mapping, with no stale FILE*. */
 fail_map=1;assert(load_gamepak_raw(path)==-1);assert(gamepak_rom==stable);
 fail_map=0;
 for(unsigned i=0;i<8;i++)assert(load_gamepak_raw(path)==32*1024*1024);
 cp_reset_heap_rom();assert(!cp_rom_mapping_size && gamepak_ram_buffer_size==24*1024*1024);
 assert(gamepak_memory_map && gamepak_ram_pages==768);
 /* Unaligned small raw files use bounded heap fallback. */
 f=fopen(path,"wb");assert(f);for(unsigned i=0;i<0x8001;i++)fputc(i&255,f);fclose(f);
 assert(load_gamepak_raw(path)==0x8001 && !cp_rom_mapping_size);
 for(unsigned i=0;i<0x8001;i++)assert(gamepak_rom[i]==(i&255));
 cp_release_rom_buffer();unlink(path);
 puts("PASS actual ROM loader: 32MiB/mirrors, clean-page reclaim, private RTC writes, mapping failure, reload, heap transition");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);c=p/'test.c';exe=p/'test';c.write_text(prefix+helpers+maps+allocator+main)
 subprocess.run(['gcc','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer',str(c),'-o',str(exe)],check=True)
 subprocess.run([str(exe),str(p/'synthetic.gba')],check=True)
