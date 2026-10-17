/* Cherry Pi independent handheld launcher. GPL-2.0-or-later. */
#define _DEFAULT_SOURCE
#include "platform.h"
#include "settings.h"
#include "library.h"
#include "theme.h"
#include <strings.h>
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
static int library_loaded, first_frame;
/* RAM-only timing. Kernel uptime excludes the preceding ROM/SPL/U-Boot phase. */
static void boot_mark(const char *event) {
    FILE *f; double up;
    f=fopen("/proc/uptime","r"); if(!f)return;
    if(fscanf(f,"%lf",&up)!=1) { fclose(f);return; } fclose(f);
    f=fopen(RUN_DIR "/cherrypi-boot-timing.log","a"); if(!f)return;
    fprintf(f,"%s %.2f\n",event,up); fclose(f);
}
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
static void text(int x,int y,const char *s,u16 c,int maxchars,int spacing) {
    int col=0;
    while(*s && col<maxchars) {
        unsigned char ch=(unsigned char)*s++;int a,b;
        if(ch>=128) { while(((unsigned char)*s&0xc0)==0x80)s++;ch='?'; }
        if(ch<32)ch='?';
        for(b=0;b<FONT_HEIGHT;b++)for(a=0;a<FONT_WIDTH;a++)
            if(_font_bits[_font_offset[ch]+b]&(0x8000u>>a)) {
                int px=x+col*spacing+a,py=y+b;
                if(px>=0 && px<320 && py>=0 && py<240)((u16*)((char*)screen->pixels+py*screen->pitch))[px]=c;
            }
        col++;
    }
}
/* Fit by rendered characters, never by cutting UTF-8 in the middle. */
static void fit_label(char *out,const char *s,int limit,int strip_extension) {
    int n=0;const char *end=s+strlen(s),*dot=strrchr(s,'.');
    if(strip_extension && dot && (!strcasecmp(dot,".gba") || !strcasecmp(dot,".zip") || !strcasecmp(dot,".bin")))end=dot;
    while(s<end && n<limit) {
        unsigned char ch=(unsigned char)*s++;
        if(ch>=128) { while(s<end && ((unsigned char)*s&0xc0)==0x80)s++;ch='?'; }
        out[n++]=ch<32?'?':(char)ch;
    }
    if(s<end && limit>=3) { out[limit-3]='.';out[limit-2]='.';out[limit-1]='.'; }
    out[n]=0;
}
static void right_text(int right,int y,const char *s,u16 color,int spacing) {
    text(right-(int)strlen(s)*spacing,y,s,color,50,spacing);
}
static void tile(int y,int h,int selected,u16 accent) {
    rect(12,y,296,h,selected?UI_SELECTED:UI_PANEL);
    if(selected)rect(12,y,3,h,accent);
}
static void hint(int *x,const char *button,const char *action) {
    int w=(int)strlen(button)*6+8;
    rect(*x,222,w,14,UI_LINE);text(*x+4,224,button,UI_TEXT,6,6);
    *x+=w+5;text(*x,224,action,UI_MUTED,12,6);*x+=(int)strlen(action)*6+12;
}
static void footer(void) {
    int x=12;
    rect(0,216,320,24,UI_PANEL);rect(12,216,296,1,UI_LINE);
    if(view==SETTINGS) { hint(&x,"<>","Adjust");hint(&x,"A","Save");hint(&x,"B","Cancel"); }
    else if(view==POWEROFF) { hint(&x,"<>","Choose");hint(&x,"A","Confirm");hint(&x,"B","Back"); }
    else {
        hint(&x,"^v","Move");hint(&x,"A",view==HOME?"Open":"Play");
        if(view!=HOME)hint(&x,"B","Back");
        hint(&x,"M","Settings");
    }
}
static void render(void) {
    static const char *titles[]={"CHERRY PI","GAMES","RECENT GAMES","SETTINGS","POWER OFF"};
    static const char *home[]={"Games","Recent games","Settings","Power off"};
    static const char *sub[]={"Browse your library","Jump back into a game","Sound and screen","Save data and shut down"};
    char b[100],label[64];int i;
    rect(0,0,320,240,UI_BG);
    rect(12,13,3,16,UI_ACCENT);text(23,16,titles[view],UI_TEXT,36,7);
    if(view==HOME)right_text(308,17,"HANDHELD",UI_MUTED,6);
    rect(12,38,296,1,UI_LINE);rect(12,38,32,1,UI_ACCENT);
    if(view==HOME) {
        for(i=0;i<4;i++) {
            int y=48+i*35;tile(y,30,i==row,UI_ACCENT);
            snprintf(b,sizeof(b),"%02d",i+1);text(24,y+10,b,i==row?UI_ACCENT:UI_MUTED,2,6);
            text(48,y+3,home[i],UI_TEXT,35,7);text(48,y+17,sub[i],UI_MUTED,42,6);
        }
    }
    if(view==GAMES || view==RECENT) {
        Library *l=view==GAMES?&games:&recent;size_t top=(size_t)row/8*8;
        if(!l->count) {
            tile(56,126,0,UI_ACCENT);
            text(26,81,view==GAMES?"Your library is empty":"No recent games yet",UI_TEXT,39,7);
            text(26,111,view==GAMES?"Copy .gba files to GAMES/roms":"Play a game to see it here",UI_MUTED,45,6);
            text(26,139,"B returns to the home screen",UI_MUTED,45,6);
        }
        for(i=0;i<8 && top+i<l->count;i++) {
            int y=48+i*18,selected=top+i==(size_t)row;
            tile(y,17,selected,UI_ACCENT);
            fit_label(label,library_name(l->paths[top+i]),39,1);
            text(24,y+3,label,selected?UI_TEXT:UI_MUTED,39,7);
        }
        snprintf(b,sizeof(b),"%d / %zu%s",l->count?row+1:0,l->count,l->truncated?"  (limit)":"");
        right_text(308,28,b,UI_MUTED,6);
    }
    if(view==SETTINGS) {
        for(i=0;i<2;i++) {
            int y=48+i*54,percent=i?(maximum?(brightness*100+maximum/2)/maximum:0):volume;
            tile(y,46,row==i,UI_ACCENT);
            text(24,y+7,i?"Brightness":"Volume",UI_TEXT,25,7);
            if(i && !maximum)snprintf(b,sizeof(b),"N/A");else snprintf(b,sizeof(b),"%d%%",percent);
            right_text(296,y+7,b,row==i?UI_ACCENT:UI_TEXT,7);
            rect(24,y+30,272,5,UI_LINE);
            if(percent>0)rect(24,y+30,272*percent/100,5,i && !maximum?UI_LINE:UI_ACCENT);
        }
        tile(160,30,row==2,UI_ACCENT);text(24,170,"Save and return",UI_TEXT,38,7);
    }
    if(view==POWEROFF) {
        text(24,57,"Ready to power off?",UI_TEXT,39,7);
        text(24,80,"Your saves will be synced.",UI_MUTED,46,6);
        tile(112,30,!confirm_row,UI_ACCENT);text(24,122,"Cancel",UI_TEXT,38,7);
        tile(152,30,confirm_row,UI_WARN);text(24,162,"Power off",confirm_row?UI_WARN:UI_TEXT,38,7);
    }
    if(message[0]) {
        rect(12,197,296,15,UI_PANEL);rect(12,197,3,15,UI_WARN);
        fit_label(label,message,46,0);text(22,200,label,UI_WARN,46,6);
    } else {
        if(view==HOME) {
            if(library_loaded)snprintf(b,sizeof(b),"%zu games in your library",games.count);
            else snprintf(b,sizeof(b),"Ready when you are");
        } else if(view==GAMES || view==RECENT)snprintf(b,sizeof(b),"A or START to play");
        else if(view==SETTINGS)snprintf(b,sizeof(b),"Hold Left/Right to adjust");
        else snprintf(b,sizeof(b),"B to return without shutting down");
        text(16,200,b,UI_MUTED,48,6);
    }
    footer();
    if(cherrypi_present(screen)==0 && !first_frame) { first_frame=1;boot_mark("menu_ready"); }
    dirty=0;
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
            if(row==0) { boot_mark("rom_scan_begin"); if(!library_scan(&games,romdir)) notice("Cannot read some ROM directories"); boot_mark("rom_scan_end"); library_loaded=1; view=GAMES; row=0; }
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
    boot_mark("launcher_start"); config();
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
