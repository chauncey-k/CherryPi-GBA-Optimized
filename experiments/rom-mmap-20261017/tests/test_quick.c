#define _GNU_SOURCE
#define QUICK_MENU_TEST
#define QUICK_CONFIG "config"
#define QUICK_BACKLIGHT "backlight"
#include "../source/gpsp/cherrypi/quick_menu.c"
#include <assert.h>
#include <setjmp.h>
#include <unistd.h>
#include <fcntl.h>
#define WANT_FONT_BITS
#include "../source/gpsp/font.h"
u32 savestate_slot;
int cherrypi_menu_active,screen_scale;
static int save_calls,load_calls,quit_calls,persist_calls,preview_calls,fail_save,fail_load,fail_settings,fail_preview;
static int released,audio_paused,advanced_called,advanced_result,painted,video_large,video_small;
static int last_saved=-1,last_loaded=-1;
static char error[160];
static jmp_buf exit_target;
static gui_action_type sequence[64];static int sequence_count,sequence_index;
static u16 capture[240*160],pixels[320*240];
static int shown_error;
static SDL_audiostatus previous_audio=SDL_AUDIO_PLAYING;
static void slot_file(int slot) {
  char p[64];int fd;snprintf(p,sizeof(p),"slot%d.svs",slot);fd=creat(p,0600);assert(fd>=0);assert(!ftruncate(fd,CP_STATE_PAYLOAD+CP_STATE_HEADER));assert(!close(fd));
}
static void put(const char *p,const char *s) { FILE *f=fopen(p,"w");assert(f);fputs(s,f);assert(!fclose(f)); }
SDL_audiostatus SDL_GetAudioStatus(void) { return previous_audio; }
void SDL_PauseAudio(int n) { audio_paused=n; }
void video_resolution_large(void) { video_large++; }
void video_resolution_small(void) { video_small++; }
void set_gba_resolution(int n) { (void)n; }
void cherrypi_release_game_keys(void) { released++; }
void cherrypi_menu_repeat_reset(void) {}
gui_action_type get_gui_input(void) { assert(sequence_index<sequence_count);return sequence[sequence_index++]; }
void clear_screen(u16 color) { int i;for(i=0;i<320*240;i++)pixels[i]=color; }
void print_string(const char *s,u16 fg,u16 bg,u32 x,u32 y) {
  unsigned i=0;int a,b;(void)bg;
  if(strstr(s,"simulated"))shown_error=1;
  while(*s) {
    unsigned char ch=*s++;if(ch>=128 || ch<32)ch='?';
    for(b=0;b<10;b++)for(a=0;a<6;a++)if(x+i*6+a<320 && y+b<240 && (_font_bits[_font_offset[ch]+b] & (0x8000u>>a)))pixels[(y+b)*320+x+i*6+a]=fg;
    i++;
  }
}
void blit_to_screen(u16 *p,u32 w,u32 h,u32 x,u32 y) {
  u32 row;assert(x+w<=320 && y+h<=240);
  for(row=0;row<h;row++)memcpy(pixels+(y+row)*320+x,p+row*w,w*2);
}
void cherrypi_menu_present(void) { painted++; }
void get_savestate_filename_noshot(u32 slot,char *p) { snprintf(p,512,"slot%u.svs",slot); }
u32 menu_advanced(u16 *p) { assert(p==capture);advanced_called++;return advanced_result; }
int save_config_file(void) { return 0; }
void save_controllers(void) {}
void quit_without_autosave(void) { quit_calls++;longjmp(exit_target,1); }
const char *cp_state_message(void) { return error; }
void cp_state_clear_message(void) { error[0]=0; }
int cp_state_fail(const char *s) { snprintf(error,sizeof(error),"%s",s);return 0; }
int cp_state_preview(const char *p,void *image,void *stamp,size_t n) {
  struct stat s;time_t now=1700000000;int i;
  assert(n==sizeof(now));if(stat(p,&s) || s.st_size!=CP_STATE_PAYLOAD+CP_STATE_HEADER)return 0;
  for(i=0;i<240*160;i++)((u16*)image)[i]=((i/240)%32)<<11 | (i%240)%32;
  memcpy(stamp,&now,n);return 1;
}
int save_state(char *p,u16 *image) { assert(image==capture);save_calls++;sscanf(p,"slot%d.svs",&last_saved);if(fail_save)return cp_state_fail("simulated save failure");slot_file(last_saved);return 1; }
int load_state(char *p) { struct stat s;load_calls++;sscanf(p,"slot%d.svs",&last_loaded);if(fail_load || stat(p,&s))return cp_state_fail("simulated load failure");return 1; }
int cherrypi_apply_settings(int v,int b) { assert(v>=0 && v<=100 && b>=0 && b<=10);persist_calls++;return !fail_settings; }
const char *cherrypi_settings_error(void) { return "simulated settings failure"; }
int cherrypi_preview_volume(int v) { assert(v>=0 && v<=100);preview_calls++;return !fail_preview; }
int cherrypi_set_brightness(int b) { assert(b>=1 && b<=10);preview_calls++;return !fail_preview; }
static void init(QuickMenu *q) { memset(q,0,sizeof(*q));q->capture=capture;q->volume=q->saved_volume=45;q->brightness=q->saved_brightness=5;q->maximum=10; }
static void snapshot(const char *name) {
  const char *out=getenv("PREVIEW_DIR");FILE *f;char path[1024];int i;
  if(!out)return;snprintf(path,sizeof(path),"%s/%s.ppm",out,name);f=fopen(path,"wb");assert(f);fprintf(f,"P6\n320 240\n255\n");
  for(i=0;i<320*240;i++) { unsigned char rgb[]={((pixels[i]>>11)&31)*255/31,((pixels[i]>>5)&63)*255/63,(pixels[i]&31)*255/31};fwrite(rgb,1,3,f); }fclose(f);
}
static void queue(const gui_action_type *actions,int n) { memcpy(sequence,actions,n*sizeof(*actions));sequence_count=n;sequence_index=0; }
int main(void) {
  QuickMenu q;int calls,i,result;
  mkdir("backlight",0755);put("backlight/max_brightness","10\n");put("backlight/brightness","5\n");put("config","VOLUME=45\n");
  { gui_action_type s[]={CURSOR_EXIT};queue(s,1);assert(!cherrypi_quick_menu(capture));assert(!audio_paused && released==1 && !cherrypi_menu_active && video_large==1 && video_small==1);snapshot("quick-main"); }
  init(&q);q.row=1;action(&q,CURSOR_SELECT);assert(q.view==SAVE_SLOT && q.slot_state[0]==EMPTY);
  assert(action(&q,CURSOR_SELECT)==1 && save_calls==1 && last_saved==0);
  init(&q);q.row=1;action(&q,CURSOR_SELECT);assert(q.preview_valid);render(&q);snapshot("quick-slots");
  calls=save_calls;action(&q,CURSOR_SELECT);assert(q.view==OVERWRITE && !q.choice);render(&q);snapshot("quick-overwrite");
  action(&q,CURSOR_SELECT);assert(save_calls==calls && q.view==SAVE_SLOT);
  action(&q,CURSOR_SELECT);action(&q,CURSOR_DOWN);assert(action(&q,CURSOR_SELECT)==1 && save_calls==calls+1);
  init(&q);open_slots(&q,SAVE_SLOT,0);action(&q,CURSOR_DOWN);fail_save=1;calls=save_calls;assert(!action(&q,CURSOR_SELECT));assert(q.view==SAVE_SLOT && strstr(q.message,"failure") && save_calls==calls+1);render(&q);snapshot("quick-error");fail_save=0;
  init(&q);open_slots(&q,LOAD_SLOT,0);assert(action(&q,CURSOR_SELECT)==2 && last_loaded==0);
  init(&q);q.volume=50;open_slots(&q,LOAD_SLOT,0);fail_settings=1;calls=load_calls;
  assert(!action(&q,CURSOR_SELECT) && load_calls==calls && q.view==LOAD_SLOT);fail_settings=0;
  init(&q);open_slots(&q,LOAD_SLOT,0);action(&q,CURSOR_DOWN);assert(!action(&q,CURSOR_SELECT) && strstr(q.message,"failure"));
  action(&q,CURSOR_RIGHT);assert(q.slot==6);action(&q,CURSOR_RIGHT);assert(q.slot==1);action(&q,CURSOR_UP);action(&q,CURSOR_UP);assert(q.slot==9);
  init(&q);q.row=3;for(i=0;i<30;i++)action(&q,CURSOR_RIGHT);assert(q.volume==100);for(i=0;i<30;i++)action(&q,CURSOR_LEFT);assert(q.volume==0);
  q.row=4;for(i=0;i<30;i++)action(&q,CURSOR_RIGHT);assert(q.brightness==10);for(i=0;i<30;i++)action(&q,CURSOR_LEFT);assert(q.brightness==1);
  fail_preview=1;action(&q,CURSOR_RIGHT);assert(q.brightness==1 && strstr(q.message,"failure"));fail_preview=0;
  fail_settings=1;assert(!action(&q,CURSOR_EXIT) && strstr(q.message,"failure"));fail_settings=0;assert(action(&q,CURSOR_EXIT)==1 && persist_calls>0);
  init(&q);q.row=5;action(&q,CURSOR_SELECT);assert(q.view==EXIT_CONFIRM && !q.choice);render(&q);snapshot("quick-exit");
  calls=quit_calls;action(&q,CURSOR_SELECT);assert(q.view==MAIN && quit_calls==calls);
  q.row=5;action(&q,CURSOR_SELECT);action(&q,CURSOR_DOWN);action(&q,CURSOR_SELECT);assert(q.view==SAVE_SLOT && q.exit_after_save);
  action(&q,CURSOR_EXIT);assert(q.view==EXIT_CONFIRM && !q.choice);action(&q,CURSOR_DOWN);action(&q,CURSOR_SELECT);action(&q,CURSOR_DOWN);
  calls=save_calls;
  if(!setjmp(exit_target)) { action(&q,CURSOR_SELECT);assert(0); }
  assert(save_calls==calls+1 && last_saved==1 && quit_calls==1);
  init(&q);q.row=5;action(&q,CURSOR_SELECT);action(&q,CURSOR_DOWN);action(&q,CURSOR_DOWN);calls=save_calls;
  if(!setjmp(exit_target)) { action(&q,CURSOR_SELECT);assert(0); }
  assert(save_calls==calls && quit_calls==2);
  init(&q);open_slots(&q,SAVE_SLOT,1);q.slot=2;fail_save=1;calls=quit_calls;
  assert(!action(&q,CURSOR_SELECT) && quit_calls==calls && q.view==SAVE_SLOT);fail_save=0;
  strcpy(error,"simulated prior state failure");shown_error=0;
  { gui_action_type s[]={CURSOR_EXIT};queue(s,1);assert(!cherrypi_quick_menu(capture));assert(shown_error && !error[0]); }
  { gui_action_type s[]={CURSOR_UP,CURSOR_SELECT};queue(s,2);advanced_result=1;result=cherrypi_quick_menu(capture);assert(result==1 && advanced_called==1 && !cherrypi_menu_active); }
  { gui_action_type s[]={CURSOR_EXIT};queue(s,1);previous_audio=SDL_AUDIO_PAUSED;assert(!cherrypi_quick_menu(capture));assert(audio_paused && !cherrypi_menu_active); }
  assert(painted>5 && preview_calls>10);
  puts("PASS quick menu: resume, slots/preview, overwrite default cancel, save/load failure retry, pages, settings bounds/failure, exit choices, single save-and-exit, advanced return and audio/key cleanup");
  return 0;
}
