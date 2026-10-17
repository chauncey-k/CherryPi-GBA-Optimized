from pathlib import Path
import subprocess,tempfile
base=Path(__file__).resolve().parents[1]
text=(base/'source/gpsp/gui.c').read_text()
def function(name):
    start=text.index('  void '+name+'()');brace=text.index('{',start);level=1;end=brace+1
    while level:
        if text[end]=='{':level+=1
        if text[end]=='}':level-=1
        end+=1
    return text[start:end]
harness=r'''
#include <assert.h>
#include <stddef.h>
#include <string.h>
static int first_load,repeat,return_value,ok,changed,chosen;
static unsigned savestate_slot;
static char current_savestate_filename[512];
static void *original_screen,*current_menu;
static void get_savestate_filename_noshot(unsigned slot,char *p){(void)slot;strcpy(p,"state.svs");}
static void menu_exit(void){repeat=0;}
static void menu_change_state(void){changed++;}
static int save_state(char *p,void *s){(void)p;(void)s;return ok;}
static int load_state(char *p){(void)p;return ok;}
static int load_file(const char **ext,char *p){(void)ext;strcpy(p,"state.svs");return 0;}
static void choose_menu(void *m){(void)m;chosen++;}
FUNCTIONS
int main(void){
  repeat=1;ok=0;menu_load_state();assert(repeat==1 && return_value==0);
  menu_save_state();assert(repeat==1 && changed==1);
  menu_load_state_file();assert(repeat==1 && chosen==1);
  ok=1;menu_load_state();assert(repeat==0 && return_value==1);
  repeat=1;menu_save_state();assert(repeat==0);
  first_load=1;repeat=1;menu_load_state();assert(repeat==1);
  return 0;
}
'''
with tempfile.TemporaryDirectory() as directory:
    d=Path(directory);(d/'test.c').write_text(harness.replace('FUNCTIONS','\n'.join(function(n) for n in ('menu_save_state','menu_load_state','menu_load_state_file'))))
    subprocess.run(['gcc','-std=gnu99','-Wall','-Wextra','-Werror',str(d/'test.c'),'-o',str(d/'test')],check=True)
    subprocess.run([str(d/'test')],check=True)
print('PASS actual GUI actions: failed load/save stays in menu, success exits, unloaded game guarded')
