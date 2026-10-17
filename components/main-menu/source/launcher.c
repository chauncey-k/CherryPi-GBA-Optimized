/* Cherry Pi independent handheld launcher. GPL-2.0-or-later. */
#define _DEFAULT_SOURCE
#include "platform.h"
#include "settings.h"
#include "library.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>
typedef uint16_t u16;
#define WANT_FONT_BITS
#include "font.h"
#ifndef CONFIG_FILE
#define CONFIG_FILE "/etc/cherrypi-autostart.conf"
#endif
#ifndef BACKLIGHT_DIR
#define BACKLIGHT_DIR "/sys/class/backlight/backlight"
#endif
#ifndef HISTORY_FILE
#define HISTORY_FILE "/root/.cherrypi-recent"
#endif
#ifndef RUN_DIR
#define RUN_DIR "/run"
#endif
#ifndef EMULATOR_WRAPPER
#define EMULATOR_WRAPPER "/root/gpsp/run-gpsp.sh"
#endif
enum { HOME, GAMES, RECENT, SETTINGS, POWEROFF };
static SDL_Surface *screen;
static Library games, recent;
static int view=HOME, row, return_view, return_row, volume=45, brightness, maximum;
static int saved_volume,saved_brightness,dirty=1,confirm_row;
static char romdir[PATH_MAX]="/mnt/games/roms", message[160];
static unsigned notice_deadline;
static void notice(const char *s) { snprintf(message,sizeof(message),"%s",s); notice_deadline=SDL_GetTicks()+5000; dirty=1; }
static int read_number(const char *path) { int n=-1; FILE *f=fopen(path,"r"); if(f) { if(fscanf(f,"%d",&n)!=1) n=-1; fclose(f); } return n; }
static void config(void) {
    FILE *f=fopen(CONFIG_FILE,"r"); char line[PATH_MAX+32];
    if(f) { while(fgets(line,sizeof(line),f)) {
        char *e; long n;
        line[strcspn(line,"\r\n")]=0;
        if(!strncmp(line,"VOLUME=",7)) { n=strtol(line+7,&e,10); if(e!=line+7 && !*e && n>=0 && n<=100) volume=n; }
        if(!strncmp(line,"ROMDIR=",7) && line[7]) {
            char *p=line+7; size_t len=strlen(p);
            if(len>=2 && ((p[0]=='\'' && p[len-1]=='\'') || (p[0]=='"' && p[len-1]=='"'))) { p++; p[len-2]=0; }
            if(p[0]=='/' && strlen(p)<sizeof(romdir)) strcpy(romdir,p);
        }
    } fclose(f); }
    if(access(romdir,R_OK)) snprintf(romdir,sizeof(romdir),"/mnt/games");
    maximum=read_number(BACKLIGHT_DIR "/max_brightness");
    brightness=read_number(BACKLIGHT_DIR "/brightness");
    if(maximum<1 || brightness<1 || brightness>maximum) { brightness=0; maximum=0; }
}
static void rect(int x,int y,int w,int h,u16 c) { SDL_Rect r={x,y,w,h}; SDL_FillRect(screen,&r,c); }
static void text(int x,int y,const char *s,u16 c,int maxchars) {
    int col=0;
    while(*s && col<maxchars) {
        unsigned char ch=(unsigned char)*s++; int a,b;
        if(ch>=128) { while(((unsigned char)*s & 0xc0)==0x80) s++; ch='?'; }
        if(ch<32) ch='?';
        for(b=0;b<FONT_HEIGHT;b++) for(a=0;a<FONT_WIDTH;a++)
            if(_font_bits[_font_offset[ch]+b] & (0x8000u>>a)) {
                int px=x+col*6+a,py=y+b;
                if(px>=0 && px<320 && py>=0 && py<240) ((u16*)((char*)screen->pixels+py*screen->pitch))[px]=c;
            }
        col++;
    }
}
static void item(int i,const char *label,int selected) {
    int y=44+i*18; if(selected) rect(8,y-3,304,17,0x2451);
    text(14,y,label,selected?0xffff:0xc618,48);
}
static void render(void) {
    static const char *home[]={"Games","Recent games","Settings","Power off"};
    static const char *titles[]={"CHERRY PI","GAMES","RECENT GAMES","SETTINGS","POWER OFF"};
    char b[100]; int i;
    rect(0,0,320,240,0x0842); text(12,12,titles[view],0xffff,48); rect(8,30,304,1,0x4a69);
    if(view==HOME) { for(i=0;i<4;i++) item(i,home[i],i==row); snprintf(b,sizeof(b),"%zu games",games.count); text(14,142,b,0x9cf3,48); }
    if(view==GAMES || view==RECENT) {
        Library *l=view==GAMES?&games:&recent; size_t top=(size_t)row/8*8;
        if(!l->count) { text(14,48,view==GAMES?"No games found":"No recent games",0xffff,48); text(14,70,"Copy ROMs into GAMES/roms",0x9cf3,48); }
        for(i=0;i<8 && top+i<l->count;i++) item(i,library_name(l->paths[top+i]),top+i==(size_t)row);
        snprintf(b,sizeof(b),"%d / %zu%s",l->count?row+1:0,l->count,l->truncated?" (limit reached)":""); text(14,190,b,0x9cf3,48);
    }
    if(view==SETTINGS) {
        snprintf(b,sizeof(b),"Volume       %d%%",volume); item(0,b,row==0);
        if(maximum) snprintf(b,sizeof(b),"Brightness   %d%%",(brightness*100+maximum/2)/maximum);
        else snprintf(b,sizeof(b),"Brightness   unavailable");
        item(1,b,row==1); item(2,"Save and return",row==2);
        text(14,119,"Hold Left/Right to adjust",0x9cf3,48);
        text(14,137,"A: save   B: cancel",0x9cf3,48);
    }
    if(view==POWEROFF) { text(14,48,"Save data and shut down?",0xffff,48); item(2,"Cancel",confirm_row==0); item(3,"Power off",confirm_row==1); }
    if(message[0]) text(12,207,message,0xffe0,49);
    text(12,226,view==SETTINGS?"Left/Right: adjust   A: save   B: cancel":"A/Start: select   B: back   M: settings",0x9cf3,50);
    cherrypi_present(screen); dirty=0;
}
static void enter_settings(void) { return_view=view; return_row=row; saved_volume=volume; saved_brightness=brightness; view=SETTINGS; row=0; dirty=1; }
static void save_settings(void) {
    if(!cherrypi_apply_settings(volume,brightness)) { notice(cherrypi_settings_error()); return; }
    view=return_view; row=return_row; notice("Settings saved");
}
static void cancel_settings(void) {
    if(!cherrypi_preview_volume(saved_volume)) { notice(cherrypi_settings_error()); return; }
    if(maximum && !cherrypi_set_brightness(saved_brightness)) { notice(cherrypi_settings_error()); return; }
    volume=saved_volume; brightness=saved_brightness; view=return_view; row=return_row; notice("Changes cancelled");
}
static int launch(const char *path) {
    FILE *f;
    if(access(path,R_OK)) { notice("Game unavailable; re-open Games to scan"); return 0; }
    if(!recent_record(HISTORY_FILE,path)) { notice("Cannot save recent history"); fprintf(stderr,"Recent: %s\n",strerror(errno)); return 0; }
    f=fopen(RUN_DIR "/cherrypi-launcher-game","w");
    if(!f) { notice("Cannot create game launch marker"); return 0; }
    { int ok=fprintf(f,"%s\n",path)>=0; if(fclose(f)) ok=0;
      if(!ok) { unlink(RUN_DIR "/cherrypi-launcher-game"); notice("Cannot write game launch marker"); return 0; } }
    fprintf(stderr,"Launching: %s\n",path);
    cherrypi_platform_close(); SDL_Quit();
    execl(EMULATOR_WRAPPER,EMULATOR_WRAPPER,path,(char*)NULL);
    perror("exec emulator"); return -1;
}
static int key(SDLKey k) {
    int count=4;
    if(view==GAMES) count=games.count;
    if(view==RECENT) count=recent.count;
    if(view==SETTINGS) count=3;
    if(k==SDLK_RCTRL && view!=SETTINGS && view!=POWEROFF) { enter_settings(); return 0; }
    if(k==SDLK_LCTRL || k==SDLK_ESCAPE) {
        if(view==SETTINGS) cancel_settings(); else { view=HOME; row=0; dirty=1; } return 0;
    }
    if(view==POWEROFF) {
        if(k==SDLK_UP || k==SDLK_DOWN || k==SDLK_LEFT || k==SDLK_RIGHT) { confirm_row=!confirm_row; dirty=1; }
        if(k==SDLK_LALT || k==SDLK_RETURN) {
            if(!confirm_row) { view=HOME; row=3; dirty=1; }
            else if(cherrypi_request_poweroff()) return 1;
            else notice(cherrypi_settings_error());
        } return 0;
    }
    if(k==SDLK_UP || k==SDLK_DOWN) { if(count) row=(row+(k==SDLK_UP?count-1:1))%count; dirty=1; }
    if(view==SETTINGS && (k==SDLK_LEFT || k==SDLK_RIGHT)) {
        int delta=k==SDLK_RIGHT?1:-1,n;
        if(row==0) { n=volume+delta*5; if(n<0)n=0;if(n>100)n=100; if(cherrypi_preview_volume(n)) volume=n; else notice(cherrypi_settings_error()); }
        if(row==1 && maximum) { n=brightness+delta*(maximum/10?maximum/10:1); if(n<1)n=1;if(n>maximum)n=maximum; if(cherrypi_set_brightness(n)) brightness=n; else notice(cherrypi_settings_error()); }
        dirty=1;
    }
    if(k==SDLK_LALT || k==SDLK_RETURN) {
        if(view==HOME) {
            if(row==0) { if(!library_scan(&games,romdir)) notice("Cannot read some ROM directories"); view=GAMES; row=0; }
            else if(row==1) { if(!recent_load(&recent,HISTORY_FILE)) notice("Cannot read recent history"); view=RECENT;row=0; }
            else if(row==2) enter_settings();
            else { view=POWEROFF; confirm_row=0; }
            dirty=1;
        } else if(view==SETTINGS) save_settings();
        else { Library *l=view==GAMES?&games:&recent; if(l->count && launch(l->paths[row])<0) return -1; }
    }
    return 0;
}
int main(void) {
    SDL_Event event; SDLKey held=SDLK_UNKNOWN; Uint32 repeat_at=0; int result=0;
    config(); library_scan(&games,romdir);
    if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER)<0) { fprintf(stderr,"SDL: %s\n",SDL_GetError()); return 1; }
    cherrypi_input_init(); screen=cherrypi_video_mode(320,240,16,0);
    if(!screen) { fprintf(stderr,"Video: %s\n",SDL_GetError()); cherrypi_platform_close(); SDL_Quit(); return 1; }
    { FILE *f=fopen(RUN_DIR "/cherrypi-launcher-error","r"); if(f) { char b[160]; if(fgets(b,sizeof(b),f)) { b[strcspn(b,"\r\n")]=0; notice(b); } fclose(f); unlink(RUN_DIR "/cherrypi-launcher-error"); } }
    for(;;) {
        Uint32 now=SDL_GetTicks();
        if(message[0] && (int32_t)(now-notice_deadline)>=0) { message[0]=0; dirty=1; }
        while(cherrypi_poll_event(&event,1)) {
            if(event.type==SDL_QUIT) { result=130; goto done; }
            if(event.type==SDL_KEYUP && event.key.keysym.sym==held) held=SDLK_UNKNOWN;
            if(event.type==SDL_KEYDOWN) {
                SDLKey k=event.key.keysym.sym;
                if(k==SDLK_UP || k==SDLK_DOWN || k==SDLK_LEFT || k==SDLK_RIGHT) { held=k;repeat_at=now+350; } else held=SDLK_UNKNOWN;
                result=key(k); if(result) goto done;
            }
        }
        if(held!=SDLK_UNKNOWN && view!=POWEROFF && (int32_t)(now-repeat_at)>=0) { result=key(held); repeat_at=now+120; if(result) goto done; }
        if(dirty) render();
        SDL_Delay(10);
    }
done:
    library_free(&games); library_free(&recent); cherrypi_platform_close(); SDL_Quit();
    return result==1?0:(result<0?1:result);
}
