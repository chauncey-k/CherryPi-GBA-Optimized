/* Cherry Pi handheld menu, GPL-2.0-or-later. */
#ifdef QUICK_MENU_TEST
#include "quick_test_api.h"
#else
#include "../common.h"
#endif
#include "quick_menu.h"
#include "settings.h"
#include "state_io.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <time.h>
#ifndef QUICK_CONFIG
#define QUICK_CONFIG "/etc/cherrypi-autostart.conf"
#endif
#ifndef QUICK_BACKLIGHT
#define QUICK_BACKLIGHT "/sys/class/backlight/backlight"
#endif
enum { MAIN, SAVE_SLOT, LOAD_SLOT, OVERWRITE, EXIT_CONFIRM };
enum { EMPTY, SAVED, INVALID, UNAVAILABLE };
typedef struct {
  int view,row,slot,choice,exit_after_save;
  int volume,brightness,maximum,saved_volume,saved_brightness;
  int slot_state[10],preview_valid;
  char paths[10][512],date[32],clock[32],message[160];
  u16 *capture;
} QuickMenu;
static u16 preview[240*160],thumbnail[144*96],highlight[304*18];
#define BG 0x0842
#define FG 0xffff
#define DIM 0x9cf3
#define SELECTED 0x2451
#define WARN 0xffe0
static int number(const char *path,int fallback) {
  FILE *f=fopen(path,"r");int n=fallback;
  if(f) { if(fscanf(f,"%d",&n)!=1)n=fallback;fclose(f); }return n;
}
static int initial_volume(void) {
  FILE *f=fopen(QUICK_CONFIG,"r");char line[256];int n=45,value;
  if(f) { while(fgets(line,sizeof(line),f))if(sscanf(line,"VOLUME=%d",&value)==1 && value>=0 && value<=100)n=value;fclose(f); }return n;
}
static void notice(QuickMenu *q,const char *s) { snprintf(q->message,sizeof(q->message),"%s",s); }
static void label(const char *s,int x,int y,u16 color,int selected) {
  char clipped[50];snprintf(clipped,sizeof(clipped),"%s",s);
  print_string(clipped,color,selected?SELECTED:BG,x,y);
}
static void bar(int y) { blit_to_screen(highlight,304,18,8,y-3); }
static int persist(QuickMenu *q) {
  if(q->volume==q->saved_volume && q->brightness==q->saved_brightness)return 1;
  if(!cherrypi_apply_settings(q->volume,q->brightness)) { notice(q,cherrypi_settings_error());return 0; }
  q->saved_volume=q->volume;q->saved_brightness=q->brightness;return 1;
}
static void refresh_preview(QuickMenu *q) {
  time_t stamp;struct tm *t;int x,y;
  q->preview_valid=0;q->date[0]=q->clock[0]=0;
  if(q->slot_state[q->slot]!=SAVED)return;
  if(!cp_state_preview(q->paths[q->slot],preview,&stamp,sizeof(stamp))) { q->slot_state[q->slot]=INVALID;return; }
  for(y=0;y<96;y++)for(x=0;x<144;x++)thumbnail[y*144+x]=preview[(y*160/96)*240+x*240/144];
  q->preview_valid=1;t=localtime(&stamp);
  if(!t || !strftime(q->date,sizeof(q->date),"%Y-%m-%d",t) || !strftime(q->clock,sizeof(q->clock),"%H:%M:%S",t)) {
    snprintf(q->date,sizeof(q->date),"Time unavailable");q->clock[0]=0;
  }
}
static void open_slots(QuickMenu *q,int view,int exit_after_save) {
  int i;struct stat s;
  q->view=view;q->exit_after_save=exit_after_save;q->slot=savestate_slot<10?savestate_slot:0;q->message[0]=0;
  for(i=0;i<10;i++) {
    get_savestate_filename_noshot(i,q->paths[i]);
    if(lstat(q->paths[i],&s))q->slot_state[i]=errno==ENOENT?EMPTY:UNAVAILABLE;
    else q->slot_state[i]=S_ISREG(s.st_mode) && (s.st_size==CP_STATE_LEGACY || s.st_size==CP_STATE_PAYLOAD+CP_STATE_HEADER)?SAVED:INVALID;
  }
  refresh_preview(q);
}
static void render(QuickMenu *q) {
  static const char *names[]={"Continue game","Save state (snapshot)","Load state (snapshot)","Volume","Brightness","Exit to main menu","Advanced options"};
  static const char *states[]={"Empty","Saved","Invalid","Unavailable"};
  char line[96];int i;
  clear_screen(BG);
  if(q->view==MAIN) {
    label("GAME MENU",12,10,FG,0);
    for(i=0;i<7;i++) {
      int y=36+i*20;if(q->row==i)bar(y);
      if(i==3)snprintf(line,sizeof(line),"Volume           %d%%",q->volume);
      else if(i==4 && q->maximum)snprintf(line,sizeof(line),"Brightness       %d%%",(q->brightness*100+q->maximum/2)/q->maximum);
      else if(i==4)snprintf(line,sizeof(line),"Brightness       unavailable");
      else snprintf(line,sizeof(line),"%s",names[i]);
      label(line,14,y,q->row==i?FG:DIM,q->row==i);
    }
    label("Left/Right: adjust   A: select",12,214,DIM,0);
    label("B/Menu: continue",12,228,DIM,0);
  } else if(q->view==SAVE_SLOT || q->view==LOAD_SLOT) {
    int top=q->slot/5*5;
    label(q->view==LOAD_SLOT?"LOAD STATE (.svs)":(q->exit_after_save?"SAVE STATE AND EXIT":"SAVE STATE (.svs)"),12,10,FG,0);
    for(i=0;i<5;i++) {
      int slot=top+i,y=45+i*22;
      if(slot==q->slot) { u16 row_pixels[148*18];int p;for(p=0;p<148*18;p++)row_pixels[p]=SELECTED;blit_to_screen(row_pixels,148,18,8,y-3); }
      snprintf(line,sizeof(line),"Slot %d  %s",slot,states[q->slot_state[slot]]);
      label(line,13,y,slot==q->slot?FG:DIM,slot==q->slot);
    }
    if(q->preview_valid)blit_to_screen(thumbnail,144,96,166,42);
    else label(q->slot_state[q->slot]==EMPTY?"No save state":"No valid preview",167,80,DIM,0);
    label(q->date,166,149,DIM,0);label(q->clock,166,163,DIM,0);
    snprintf(line,sizeof(line),"Page %d / 2",q->slot/5+1);label(line,13,163,DIM,0);
    label("Up/Down: slot   Left/Right: page",12,214,DIM,0);
    label(q->view==LOAD_SLOT?"A: load state   B: back":"A: save state   B: back",12,228,DIM,0);
  } else if(q->view==OVERWRITE) {
    label("OVERWRITE SAVE STATE?",12,12,FG,0);
    snprintf(line,sizeof(line),"Replace slot %d emulator snapshot (.svs)?",q->slot);label(line,14,48,DIM,0);
    if(q->choice==0)bar(85);else bar(113);
    label("Cancel",14,85,q->choice==0?FG:DIM,q->choice==0);
    label("Overwrite state",14,113,q->choice==1?FG:DIM,q->choice==1);
    label("A: confirm   B: cancel",12,228,DIM,0);
  } else {
    static const char *exit_names[]={"Cancel","Save state and exit...","Exit without new state"};
    label("EXIT TO MAIN MENU?",12,12,FG,0);
    label("Choose whether to create a save state.",14,48,DIM,0);
    for(i=0;i<3;i++) { int y=85+i*28;if(q->choice==i)bar(y);label(exit_names[i],14,y,q->choice==i?FG:DIM,q->choice==i); }
    label("A: confirm   B: cancel",12,228,DIM,0);
  }
  label("State (.svs): emulator snapshot",12,176,DIM,0);
  label("In-game (.sav): game's own Save menu",12,188,DIM,0);
  label(q->message,12,202,WARN,0);
  cherrypi_menu_present();
}
static int save_selected(QuickMenu *q) {
  if(!persist(q))return 0;
  if(!save_state(q->paths[q->slot],q->capture)) { notice(q,cp_state_message());q->view=SAVE_SLOT;return 0; }
  savestate_slot=q->slot;
  if(q->exit_after_save) {
    save_config_file();save_controllers();quit_without_autosave();
    notice(q,"Exit did not complete");return 0;
  }
  return 1;
}
/* Returns 1 to resume, 2 after loading a state, 3 to open advanced menu. */
static int action(QuickMenu *q,gui_action_type a) {
  int d,n;
  if(a==CURSOR_NONE)return 0;
  if(a==CURSOR_BACK || a==CURSOR_EXIT || a==CURSOR_DEVICE) {
    if(q->view==MAIN)return persist(q)?1:0;
    if(q->view==OVERWRITE)q->view=SAVE_SLOT;
    else if(q->view==SAVE_SLOT && q->exit_after_save) { q->view=EXIT_CONFIRM;q->choice=0; }
    else { q->view=MAIN;q->exit_after_save=0; }
    q->message[0]=0;return 0;
  }
  if(q->view==MAIN) {
    if(a==CURSOR_UP || a==CURSOR_DOWN)q->row=(q->row+(a==CURSOR_UP?6:1))%7;
    if(a==CURSOR_LEFT || a==CURSOR_RIGHT) {
      d=a==CURSOR_RIGHT?1:-1;
      if(q->row==3) {
        n=q->volume+d*5;if(n<0)n=0;if(n>100)n=100;
        if(cherrypi_preview_volume(n)) { q->volume=n;q->message[0]=0; }else notice(q,cherrypi_settings_error());
      }
      if(q->row==4 && q->maximum) {
        n=q->brightness+d;if(n<1)n=1;if(n>q->maximum)n=q->maximum;
        if(cherrypi_set_brightness(n)) { q->brightness=n;q->message[0]=0; }else notice(q,cherrypi_settings_error());
      }
    }
    if(a==CURSOR_SELECT) {
      if(q->row==0)return persist(q)?1:0;
      if(q->row==1)open_slots(q,SAVE_SLOT,0);
      if(q->row==2)open_slots(q,LOAD_SLOT,0);
      if(q->row==3 || q->row==4) { if(persist(q))notice(q,"Settings saved"); }
      if(q->row==5) { q->view=EXIT_CONFIRM;q->choice=0;q->message[0]=0; }
      if(q->row==6 && persist(q))return 3;
    }
  } else if(q->view==SAVE_SLOT || q->view==LOAD_SLOT) {
    if(a==CURSOR_UP || a==CURSOR_DOWN || a==CURSOR_LEFT || a==CURSOR_RIGHT) {
      d=a==CURSOR_UP?-1:a==CURSOR_DOWN?1:a==CURSOR_LEFT?-5:5;
      q->slot=(q->slot+d+10)%10;q->message[0]=0;refresh_preview(q);
    }
    if(a==CURSOR_SELECT) {
      if(q->view==LOAD_SLOT) {
        if(!persist(q))return 0;
        if(load_state(q->paths[q->slot])) { savestate_slot=q->slot;return 2; }
        notice(q,cp_state_message());return 0;
      }
      /* Recheck existence at confirmation time, not just when the list opened. */
      { struct stat s;
        if(!lstat(q->paths[q->slot],&s)) { q->view=OVERWRITE;q->choice=0;q->message[0]=0; }
        else if(errno==ENOENT)return save_selected(q);
        else notice(q,"Cannot check slot; save cancelled");
      }
    }
  } else if(q->view==OVERWRITE) {
    if(a==CURSOR_UP || a==CURSOR_DOWN || a==CURSOR_LEFT || a==CURSOR_RIGHT)q->choice=!q->choice;
    if(a==CURSOR_SELECT) {
      if(!q->choice)q->view=SAVE_SLOT;
      else return save_selected(q);
    }
  } else {
    if(a==CURSOR_UP || a==CURSOR_DOWN)q->choice=(q->choice+(a==CURSOR_UP?2:1))%3;
    if(a==CURSOR_SELECT) {
      if(q->choice==0) { q->view=MAIN;q->exit_after_save=0; }
      else if(q->choice==1)open_slots(q,SAVE_SLOT,1);
      else if(persist(q)) {
        save_config_file();save_controllers();quit_without_autosave();notice(q,"Exit did not complete");
      }
    }
  }
  return 0;
}
uint32_t cherrypi_quick_menu(uint16_t *game_capture) {
  QuickMenu q;SDL_audiostatus previous=SDL_GetAudioStatus();int result=0,i;
  memset(&q,0,sizeof(q));q.capture=game_capture;
  if(!game_capture) { cp_state_fail("Cannot capture game screen");return 0; }
  q.volume=q.saved_volume=initial_volume();
  q.maximum=number(QUICK_BACKLIGHT "/max_brightness",0);
  q.brightness=number(QUICK_BACKLIGHT "/brightness",0);
  if(q.maximum<1 || q.maximum>65535 || q.brightness<1 || q.brightness>q.maximum)q.maximum=q.brightness=0;
  q.saved_brightness=q.brightness;notice(&q,cp_state_message());
  for(i=0;i<304*18;i++)highlight[i]=SELECTED;
  cherrypi_menu_active++;cherrypi_menu_repeat_reset();
  SDL_PauseAudio(1);video_resolution_large();
  while(!result) { render(&q);result=action(&q,get_gui_input()); }
  if(result==3)result=menu_advanced(game_capture)?2:1;
  set_gba_resolution(screen_scale);video_resolution_small();
  cherrypi_release_game_keys();cherrypi_menu_repeat_reset();cherrypi_menu_active--;
  cp_state_clear_message();
  SDL_PauseAudio(previous==SDL_AUDIO_PLAYING?0:1);
  return result==2?1:0;
}
