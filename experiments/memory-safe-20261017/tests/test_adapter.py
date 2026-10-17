"""Exercise actual emulator load/save adapter and audio validation with tiny RAM."""
from pathlib import Path
import subprocess,tempfile
base=Path(__file__).resolve().parents[1];source=base/'source/gpsp'
memory=(source/'memory.c').read_text()
adapter=memory[memory.index('static int state_capture_layout(void)'):memory.index('#define memory_savestate_builder(type)')]
sound=(source/'sound.h').read_text();types=sound[sound.index('typedef enum'):sound.index('extern direct_sound_struct')]
sound_impl=(source/'sound.c').read_text();sound_check=sound_impl[sound_impl.index('int cherrypi_sound_state_valid(void)'):]
harness=r'''
#include "state_io.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <time.h>
typedef uint8_t u8;typedef int8_t s8;typedef uint16_t u16;typedef uint32_t u32;
typedef u32 fixed8_24;typedef u32 fixed16_16;
#define BUFFER_SIZE 65536
TYPES
static char gamepak_filename[512]="/roms/Test.gba";
static unsigned cpu,ram[16],reg[2],palette_ram[512],palette_ram_converted[512];
static unsigned current_debug_state,instruction_count,oam_update,gbc_sound_update;
static direct_sound_struct direct_sound_channel[2];
static gbc_sound_struct gbc_sound_channel[4];
static s8 square_pattern_duty[4][8];
static unsigned gbc_sound_buffer_index,sound_buffer_base;
static int sound_mutex,locked,flushes;
static void SDL_LockMutex(int m){(void)m;assert(!locked);locked=1;}
static void SDL_UnlockMutex(int m){(void)m;assert(locked);locked=0;}
static void flush_translation_cache_ram(void){flushes++;}
static void flush_translation_cache_rom(void){flushes++;}
static void flush_translation_cache_bios(void){flushes++;}
#define STEP 1
#define CHANGED_PC_STATUS 0
#define convert_palette(n) (n)
static void serialize(void){
  assert(locked);
  cp_state_pack(&cpu,sizeof(cpu),"cpu");cp_state_pack(ram,sizeof(ram),"ram");
  cp_state_pack(gamepak_filename,sizeof(gamepak_filename),"gamepak_filename");
  cp_state_pack(direct_sound_channel,sizeof(direct_sound_channel),"direct_sound_channel");
  cp_state_pack(gbc_sound_channel,sizeof(gbc_sound_channel),"gbc_sound_channel");
  cp_state_pack(&gbc_sound_buffer_index,sizeof(gbc_sound_buffer_index),"gbc_sound_buffer_index");
  cp_state_pack(&sound_buffer_base,sizeof(sound_buffer_base),"sound_buffer_base");
}
#define savestate_block(type) serialize()
SOUND_CHECK
ADAPTER
int main(void){
  u16 *screen=calloc(1,CP_STATE_PREVIEW);FILE *f=fopen("rom.gba","wb");assert(f);fputs("test ROM",f);fclose(f);
  assert(cp_state_set_rom("rom.gba"));
  cpu=42;ram[0]=55;assert(save_state("good.svs",screen));
  cpu=99;ram[0]=88;assert(load_state("good.svs"));assert(cpu==42 && ram[0]==55 && flushes==3 && !locked);
  assert(gbc_sound_channel[0].sample_data==square_pattern_duty[2]);
  assert(!strcmp(gamepak_filename,"/roms/Test.gba"));
  cpu=77;ram[0]=66;assert(!load_state("missing.svs"));assert(cpu==77 && ram[0]==66 && flushes==3 && !locked);
  strcpy(gamepak_filename,"/other/Test.gba");assert(load_state("good.svs"));assert(!strcmp(gamepak_filename,"/other/Test.gba"));
  direct_sound_channel[0].fifo_base=32;assert(save_state("bad-audio.svs",screen));
  direct_sound_channel[0].fifo_base=0;cpu=13;assert(!load_state("bad-audio.svs"));assert(cpu==13 && direct_sound_channel[0].fifo_base==0);
  sound_buffer_base=BUFFER_SIZE;assert(save_state("bad-index.svs",screen));sound_buffer_base=0;
  assert(!load_state("bad-index.svs"));assert(cpu==13 && sound_buffer_base==0);
  assert(!save_state("good.svs",NULL));
  free(screen);return 0;
}
'''
with tempfile.TemporaryDirectory() as directory:
    d=Path(directory);(d/'test.c').write_text(harness.replace('TYPES',types).replace('SOUND_CHECK',sound_check).replace('ADAPTER',adapter))
    subprocess.run(['gcc','-std=gnu99','-g','-fsanitize=address,undefined','-include','stdint.h','-include','stddef.h','-I'+str(source/'cherrypi'),str(d/'test.c'),str(source/'cherrypi/state_io.c'),str(source/'sha1.c'),'-o',str(d/'test')],check=True)
    subprocess.run([str(d/'test')],cwd=d,check=True)
print('PASS actual adapter: staged load, legacy path preservation, cache/pointer fixups, rejected audio indices, unchanged live state on failure')
