from pathlib import Path
import subprocess,tempfile
base=Path(__file__).resolve().parents[1];source=base/'source/gpsp'
text=(source/'video.c').read_text();a=text.index('void print_string_ext(');b=text.index('\nvoid print_string(',a)
harness=r'''
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
typedef uint16_t u16;typedef uint32_t u32;typedef uint8_t u8;
#define WANT_FONT_BITS
#include "font.h"
static u32 resolution_width=320,resolution_height=240;
FUNCTION
int main(void){
  u16 *screen=calloc(320*240,2);assert(screen);
  print_string_ext("one\ntwo\nthree\nfour\nfive",0xffff,0,8,210,screen,320,0,0,10);
  print_string_ext(" ",0xffff,0,8,210,screen,320,70,0,10);
  free(screen);return 0;
}
'''
with tempfile.TemporaryDirectory() as directory:
    d=Path(directory);(d/'test.c').write_text(harness.replace('FUNCTION',text[a:b]))
    subprocess.run(['gcc','-g','-fsanitize=address,undefined','-I'+str(source),str(d/'test.c'),'-o',str(d/'test')],check=True)
    subprocess.run([str(d/'test')],check=True)
print('PASS menu text: multiline help clipped to screen under memory sanitizer')
