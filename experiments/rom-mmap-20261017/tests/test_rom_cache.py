"""Compile actual allocator/pager bodies with ASan/UBSan and a synthetic 32MiB ROM."""
from pathlib import Path
import subprocess,tempfile
base=Path(__file__).resolve().parents[1]
text=(base/'source/gpsp/memory.c').read_text()
alloc=text[text.index('void init_gamepak_buffer()'):text.index('\nvoid init_memory()',text.index('void init_gamepak_buffer()'))]
pager=text[text.index('// Picks a page to evict'):text.index('\nvoid init_gamepak_buffer()')]
prefix=r'''
#define _DEFAULT_SOURCE
#define CHERRYPI 1
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <sys/wait.h>
typedef uint32_t u32;
typedef uint8_t u8;
typedef struct { u32 page_timestamp,physical_index; } gamepak_swap_entry_type;
u8 *gamepak_rom,*memory_map_read[0x2000];
u32 gamepak_ram_buffer_size,gamepak_ram_pages,gamepak_size;
gamepak_swap_entry_type *gamepak_memory_map;
FILE *gamepak_file_large;
enum { RTC_DISABLED, RTC_ENABLED };
int rtc_state=RTC_DISABLED;
u8 rtc_registers[12];
#define file_seek(f,o,w) assert(!fseek(f,o,w))
#define file_read(f,p,n) assert(fread(p,1,n,f)==(n))
#define map_null(type,start,end) for(map_offset=(start)/0x8000;map_offset<(end)/0x8000;map_offset++)memory_map_read[map_offset]=NULL
#define map_region(type,start,end,mirror,region) for(map_offset=(start)/0x8000;map_offset<(end)/0x8000;map_offset++)memory_map_read[map_offset]=(region)+(map_offset%(mirror))*0x8000
static unsigned mode,calls,frees;
static void *test_malloc(size_t n) {
 calls++;
 if(mode==1 && n>16u*1024u*1024u)return NULL;
 if(mode==2 && calls==2)return NULL;
 if(mode==3)return NULL;
 return malloc(n);
}
static void test_free(void *p) { frees++;free(p); }
#define malloc test_malloc
#define free test_free
'''
tests=r'''
#undef malloc
#undef free
int main(void) {
 u8 page[32768];u32 p,i;pid_t child;int status;
 init_gamepak_buffer();assert(gamepak_ram_buffer_size==24u*1024u*1024u);assert(gamepak_ram_pages==768);
 gamepak_size=32u*1024u*1024u;gamepak_file_large=tmpfile();assert(gamepak_file_large);
 for(p=0;p<1024;p++) { for(i=0;i<sizeof(page);i++)page[i]=(u8)(p*13u+i*17u+(p>>8));assert(fwrite(page,1,sizeof(page),gamepak_file_large)==sizeof(page)); }
 assert(!fflush(gamepak_file_large));init_memory_gamepak();
 /* Fill and overflow the 768-entry cache, then revisit evicted pages.
  * Verify all 3 GBA ROM mirrors contain the selected page's exact bytes. */
 for(i=0;i<2300;i++) {
  p=(i*37u)%1024;u8 *data=memory_map_read[0x1000+p];
  if(!data)data=load_gamepak_page(p);
  assert(memory_map_read[0x1400+p]==data && memory_map_read[0x1800+p]==data);
  for(u32 j=0;j<32768;j++)assert(data[j]==(u8)(p*13u+j*17u+(p>>8)));
 }
 /* Every published mapping must still belong to that physical ROM page. */
 for(p=0;p<1024;p++)if(memory_map_read[0x1000+p]) {
  u8 *data=memory_map_read[0x1000+p];assert(data[0]==(u8)(p*13u+(p>>8)));
  assert(memory_map_read[0x1400+p]==data && memory_map_read[0x1800+p]==data);
 }
 rtc_state=RTC_ENABLED;memset(rtc_registers,0xa7,sizeof(rtc_registers));
 assert(!memcmp(load_gamepak_page(0)+0xc4,rtc_registers,sizeof(rtc_registers)));
 free(gamepak_rom);free(gamepak_memory_map);fclose(gamepak_file_large);
 mode=1;calls=0;init_gamepak_buffer();assert(gamepak_ram_buffer_size==16u*1024u*1024u);free(gamepak_rom);free(gamepak_memory_map);
 mode=2;calls=frees=0;init_gamepak_buffer();assert(gamepak_ram_buffer_size==16u*1024u*1024u && frees==1);free(gamepak_rom);free(gamepak_memory_map);
 child=fork();assert(child>=0);if(!child) { mode=3;init_gamepak_buffer();_exit(99); }
 assert(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==EXIT_FAILURE);
 puts("PASS ROM cache: default24MiB, 2300 synthetic 32MiB-ROM page reads/evictions/three mirrors/RTC, bounded allocation and metadata failure fallback");
 return 0;
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(prefix+pager+alloc+tests)
 subprocess.run(['gcc','-std=gnu99','-Wall','-Wextra','-Werror','-O1','-g','-fsanitize=address,undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
