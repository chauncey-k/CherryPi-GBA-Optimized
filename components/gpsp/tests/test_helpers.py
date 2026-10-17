from pathlib import Path
import subprocess,tempfile
base=Path(__file__).resolve().parents[1];source=base/'source/gpsp'
def function(file,signature):
    text=(source/file).read_text();a=text.index(signature);b=text.index('{',a);level=1;end=b+1
    while level:
        if text[end]=='{':level+=1
        elif text[end]=='}':level-=1
        end+=1
    return text[a:end]
harness=r'''
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
typedef uint16_t u16;typedef uint32_t u32;
#define CHERRYPI
static unsigned auto_save_state=1,quit_calls,automatic_writes;
static void quit(void){quit_calls++;if(auto_save_state)automatic_writes++;}
EXIT
static u16 pixels[320*240];
#define get_screen_pixels() pixels
#define get_screen_pitch() 320
COPY
static char gamepak_filename[512];
static int quick_calls,advanced_calls;
static u32 cherrypi_quick_menu(u16 *p){(void)p;quick_calls++;return 1;}
static u32 menu_advanced(u16 *p){(void)p;advanced_calls++;return 0;}
WRAPPER
static u32 key,isAutoFireOnA,isAutoFireOnB;
static u16 io_registers[1];
#define REG_P1 0
RELEASE
int main(void){
  unsigned x,y;u16 *image;
  quit_without_autosave();assert(auto_save_state==1 && quit_calls==1 && automatic_writes==0);
  auto_save_state=0;quit_without_autosave();assert(auto_save_state==0 && automatic_writes==0);
  for(y=0;y<240;y++)for(x=0;x<320;x++)pixels[y*320+x]=y*320+x;
  image=copy_screen();assert(image);
  for(y=0;y<160;y++)for(x=0;x<240;x++)assert(image[y*240+x]==pixels[y*320+x]);
  assert(!menu(image) && advanced_calls==1);strcpy(gamepak_filename,"Test.gba");assert(menu(image)==1 && quick_calls==1);
  key=isAutoFireOnA=isAutoFireOnB=1;cherrypi_release_game_keys();assert(!key && !isAutoFireOnA && !isAutoFireOnB && io_registers[0]==0x3ff);
  free(image);return 0;
}
'''
with tempfile.TemporaryDirectory() as directory:
    d=Path(directory);(d/'test.c').write_text(harness.replace('EXIT',function('main.c','void quit_without_autosave(void)')).replace('COPY',function('video.c','u16 *copy_screen()')).replace('WRAPPER',function('gui.c','u32 menu(u16 *original_screen)')).replace('RELEASE',function('input.c','void cherrypi_release_game_keys(void)')))
    subprocess.run(['gcc','-g','-fsanitize=address,undefined',str(d/'test.c'),'-o',str(d/'test')],check=True)
    subprocess.run([str(d/'test')],check=True)
print('PASS actual helpers: direct exit skips automatic state once; captured rows respect framebuffer pitch')
