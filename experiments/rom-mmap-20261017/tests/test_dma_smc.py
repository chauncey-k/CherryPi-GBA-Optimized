"""Compile and execute the real DMA write macros with RAM/tag fixtures."""
from pathlib import Path
import subprocess, tempfile
base = Path(__file__).resolve().parents[1]
lines = (base/'source/gpsp/memory.c').read_text().splitlines(True)
def macro(name):
    i = next(i for i,line in enumerate(lines) if line.startswith('#define '+name+'('))
    result = [lines[i]]
    while result[-1].rstrip().endswith('\\'):
        i += 1; result.append(lines[i])
    return ''.join(result)
source = r'''
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
static u8 iwram[0x10000] __attribute__((aligned(16)));
static u8 ewram[0x20000] __attribute__((aligned(16)));
static u8 *memory_map_write[8192];
/* Model ARM32 wraparound when a metadata offset is negative. */
#define address16(base,offset) (*(u16*)((u8*)(base)+(int32_t)(offset)))
#define address32(base,offset) (*(u32*)((u8*)(base)+(int32_t)(offset)))
'''
for name in ('dma_segmented_load_dest','dma_ewram_check_region','dma_write_iwram','dma_write_ewram'):
    source += macro(name)
for kind in ('iwram','ewram'):
    for width in (16,32):
        source += f'''
static u32 {kind}_{width}(u32 dest_ptr, u32 read_value, u32 smc_trigger) {{
'''
        if kind == 'ewram':
            source += '''
u32 dest_new_region, dest_current_region=(dest_ptr>>15)^1;
u8 *dest_address_block=NULL;
'''
        source += f'dma_write_{kind}(dest,{width}); return smc_trigger; }}\n'
source += r'''
int main(void) {
    unsigned kind,w,page,k,t,s,count=0;
    const u32 tags[]={0,0x0101,0xffff};
    const u32 offsets[]={0,0x100,0x7ffc};
    for(kind=0;kind<2;kind++) for(w=16;w<=32;w+=16)
    for(page=0;page<2;page++) for(k=0;k<3;k++) for(t=0;t<3;t++) for(s=0;s<3;s++) {
        u32 offset=offsets[k], old=w==16?0x3456:0x12343456;
        u32 value=s==0?old:s==1?(old^1):(w==16?old|0xabcd0000:old);
        u32 ptr=(kind?0x02000000:0x03000000)+page*0x8000+offset;
        u8 *tag_area=kind?ewram+page*0x10000:iwram;
        u8 *data=tag_area+0x8000;
        u32 result,mask=w==16?0xffff:0xffffffff;
        memset(iwram,0,sizeof(iwram)); memset(ewram,0,sizeof(ewram));
        memory_map_write[ptr>>15]=data;
        memcpy(tag_area+offset,&tags[t],w/8); memcpy(data+offset,&old,w/8);
        if(kind) result=w==16?ewram_16(ptr,value,0):ewram_32(ptr,value,0);
        else result=w==16?iwram_16(ptr,value,0):iwram_32(ptr,value,0);
        assert(result==(((value&mask)!=old)?tags[t]:0));
        old=0; memcpy(&old,data+offset,w/8); assert(old==(value&mask));
        if(kind) result=w==16?ewram_16(ptr,value,0x8000):ewram_32(ptr,value,0x8000);
        else result=w==16?iwram_16(ptr,value,0x8000):iwram_32(ptr,value,0x8000);
        assert(result==0x8000); /* unchanged transfer must not clear a previous SMC */
        count++;
    }
    printf("PASS actual DMA macros: %u cases; IWRAM/EWRAM, page changes, tags, same/changed values, width truncation and accumulated SMC\n",count);
}
'''
with tempfile.TemporaryDirectory() as tmp:
    tmp = Path(tmp)
    (tmp/'test.c').write_text(source)
    subprocess.run(['gcc','-std=gnu99','-Wall','-Wextra','-Werror','-g','-fsanitize=address,undefined',str(tmp/'test.c'),'-o',str(tmp/'test')],check=True)
    subprocess.run([str(tmp/'test')],check=True)
