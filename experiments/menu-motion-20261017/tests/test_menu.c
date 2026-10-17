/* Host tests use real menu, font, library and exec; hardware APIs are mocked. */
#define main launcher_main
#include "../source/launcher.c"
#undef main
#include <assert.h>
#include <sys/wait.h>
#include <sys/stat.h>
static Uint32 ticks;
static unsigned scan_calls;
int __real_library_scan(Library *, const char *);
int __wrap_library_scan(Library *l,const char *dir) { scan_calls++; return __real_library_scan(l,dir); }
static int fail_settings,preview_count,poweroff_count;
static int scenario,event_index;
static unsigned present_count,last_present;
static u16 pixels[320*240];
static SDL_Surface surface={pixels,640};
int SDL_Init(unsigned flags) { (void)flags; return 0; }
void SDL_Quit(void) {}
const char *SDL_GetError(void) { return "mock"; }
Uint32 SDL_GetTicks(void) { return ticks; }
void SDL_Delay(unsigned ms) { ticks+=ms; }
int SDL_FillRect(SDL_Surface *s,SDL_Rect *r,Uint32 color) {
    assert(r->x>=0 && r->y>=0 && r->w>=0 && r->h>=0 && r->x+r->w<=320 && r->y+r->h<=240);
    int x,y; for(y=r->y;y<r->y+r->h;y++) for(x=r->x;x<r->x+r->w;x++) ((u16*)s->pixels)[y*320+x]=color; return 0;
}
void cherrypi_input_init(void) {}
void cherrypi_platform_close(void) {}
int cherrypi_platform_test(void) { return 0; }
int cherrypi_poll_event(SDL_Event *e,int menu) {
    static const struct { unsigned at;int type;SDLKey key; } sequence[]={
        {0,SDL_KEYDOWN,SDLK_LALT},{0,SDL_KEYDOWN,SDLK_DOWN},{0,SDL_KEYUP,SDLK_DOWN},
        {0,SDL_KEYDOWN,SDLK_RIGHT},{1000,SDL_KEYUP,SDLK_RIGHT},{1100,SDL_QUIT,SDLK_UNKNOWN}
    };
    static const struct { unsigned at;int type;SDLKey key; } rapid[]={
        {10,SDL_KEYDOWN,SDLK_DOWN},{20,SDL_KEYDOWN,SDLK_UP},
        {30,SDL_KEYDOWN,SDLK_RCTRL},{40,SDL_KEYDOWN,SDLK_DOWN},
        {50,SDL_KEYDOWN,SDLK_RIGHT},{60,SDL_KEYDOWN,SDLK_LCTRL},
        {70,SDL_KEYDOWN,SDLK_LALT},{80,SDL_KEYDOWN,SDLK_ESCAPE},
        {400,SDL_QUIT,SDLK_UNKNOWN}
    };
    (void)menu;
    if(scenario==2) {
        if(event_index>=9 || ticks<rapid[event_index].at)return 0;
        e->type=rapid[event_index].type;e->key.keysym.sym=rapid[event_index].key;event_index++;return 1;
    }
    if(!scenario || event_index>=6 || ticks<sequence[event_index].at) return 0;
    e->type=sequence[event_index].type;e->key.keysym.sym=sequence[event_index].key;event_index++;return 1;
}
SDL_Surface *cherrypi_video_mode(int w,int h,int d,Uint32 flags) { (void)w;(void)h;(void)d;(void)flags;return &surface; }
int cherrypi_present(SDL_Surface *s) { (void)s;present_count++;last_present=ticks;return 0; }
int cherrypi_apply_settings(int v,int b) { assert(v>=0 && v<=100 && b>=0 && b<=10);return !fail_settings; }
int cherrypi_preview_volume(int v) { assert(v>=0 && v<=100);preview_count++;return !fail_settings; }
int cherrypi_set_brightness(int b) { assert(b>=1 && b<=10);return !fail_settings; }
int cherrypi_request_poweroff(void) { poweroff_count++;return !fail_settings; }
const char *cherrypi_settings_error(void) { return "simulated write failure"; }
static void put(const char *path,const char *s) { FILE *f=fopen(path,"w");assert(f);assert(fputs(s,f)>=0);assert(!fclose(f)); }
static void snapshot(const char *name) {
    const char *dir=getenv("PREVIEW_DIR");char path[PATH_MAX];int i;FILE *f;
    if(!dir)return;
    snprintf(path,sizeof(path),"%s/%s.ppm",dir,name);f=fopen(path,"wb");assert(f);
    fprintf(f,"P6\n320 240\n255\n");
    for(i=0;i<320*240;i++) { unsigned char rgb[]={((pixels[i]>>11)&31)*255/31,((pixels[i]>>5)&63)*255/63,(pixels[i]&31)*255/31};fwrite(rgb,1,3,f); }
    fclose(f);
}
int main(void) {
    Library l={0}; char p[PATH_MAX],expected[PATH_MAX]; int i,status; pid_t pid;
    mkdir("roms",0755);mkdir("roms/sub",0755);mkdir("backlight",0755);mkdir("run",0755);
    assert(getcwd(p,sizeof(p)));snprintf(expected,sizeof(expected),"%s/roms/sub/Space ' $(test).gba",p);
    put(expected,"ROM");put("roms/Beta.GBA","ROM");put("roms/ignore.txt","ignore");
    put("config","VOLUME=45\nROMDIR=\"/missing\"\n");
    put("backlight/max_brightness","10\n");put("backlight/brightness","5\n");config(); assert(volume==45 && brightness==5 && maximum==10);
    snprintf(romdir,sizeof(romdir),"%s/roms",p);assert(library_scan(&games,romdir));assert(games.count==2);
    assert(!strcmp(library_name(games.paths[0]),"Beta.GBA"));
    assert(!recent_record(HISTORY_FILE,"/bad\nname.gba"));
    { static const char *names[]={"Advance Adventure","Color Puzzle","Drift Circuit","Forest Quest","Island Stories","Moon Patrol","Pocket Garden","A surprisingly long adventure title","Sky Runner","Star Workshop","Tiny Tactics","Valley Explorer"};
      for(i=0;i<12;i++) { char name[PATH_MAX];snprintf(name,sizeof(name),"%s/roms/%02d - %s.gba",p,i,names[i]);put(name,"ROM");assert(recent_record(HISTORY_FILE,name)); } }
    assert(recent_load(&l,HISTORY_FILE));assert(l.count==10);
    assert(recent_record(HISTORY_FILE,l.paths[4]));library_free(&l);assert(recent_load(&l,HISTORY_FILE));assert(l.count==10);library_free(&l);
    screen=&surface;view=HOME;row=0;render();snapshot("home");
    row=2;key(SDLK_LALT);assert(view==SETTINGS);
    row=1;key(SDLK_RIGHT);assert(brightness==6 && dirty);render();assert(!dirty);snapshot("settings");
    for(i=0;i<20;i++) key(SDLK_RIGHT);assert(brightness==10);
    for(i=0;i<20;i++) key(SDLK_LEFT);assert(brightness==1);
    key(SDLK_LCTRL);assert(view==HOME && brightness==5);
    row=2;key(SDLK_LALT);row=0;key(SDLK_RIGHT);assert(volume==50 && preview_count>0);
    fail_settings=1;key(SDLK_LALT);assert(view==SETTINGS && strstr(message,"failure"));render();snapshot("settings-error");
    fail_settings=0;key(SDLK_LALT);assert(view==HOME && volume==50);
    view=HOME;row=3;key(SDLK_LALT);assert(view==POWEROFF && !confirm_row);
    key(SDLK_LALT);assert(view==HOME && !poweroff_count);
    row=3;key(SDLK_LALT);key(SDLK_DOWN);message[0]=0;render();snapshot("poweroff");assert(key(SDLK_LALT)==1 && poweroff_count==1);
    view=RECENT;row=0;key(SDLK_LCTRL);assert(view==HOME);
    view=GAMES;row=0;render();snapshot("games");
    { int lit=0;for(i=0;i<320*30;i++) if(pixels[i]==UI_TEXT) lit++;assert(lit>10); }
    { char label[64];fit_label(label,"Long game title that will not fit inside this menu.gba",39,1);assert(strlen(label)==39 && !strcmp(label+36,"..."));fit_label(label,"Beta.GBA",39,1);assert(!strcmp(label,"Beta"));fit_label(label,"Pok\xc3\xa9mon.gba",39,1);assert(!strcmp(label,"Pok?mon")); }
    /* Preview all eight rows, including long titles; navigation still wraps. */
    library_free(&games);assert(library_scan(&games,romdir));view=GAMES;row=7;render();snapshot("games-page");
    key(SDLK_DOWN);assert(row==8);key(SDLK_UP);assert(row==7);
    view=RECENT;row=0;render();snapshot("recent-empty");
    put(EMULATOR_WRAPPER,"#!/bin/sh\nprintf '%s\\n' \"$1\" > run/received-rom\nexit 0\n");assert(!chmod(EMULATOR_WRAPPER,0755));
    pid=fork();assert(pid>=0);if(!pid) { int r=launch(expected);_exit(r<0?1:2); }
    assert(waitpid(pid,&status,0)==pid && WIFEXITED(status) && !WEXITSTATUS(status));
    { char actual[PATH_MAX];FILE *f=fopen("run/received-rom","r");assert(f && fgets(actual,sizeof(actual),f));fclose(f);actual[strcspn(actual,"\n")]=0;assert(!strcmp(actual,expected)); }
    assert(!access("run/cherrypi-launcher-game",F_OK));
    library_free(&games);library_free(&recent);
    view=HOME;row=2;scenario=1;ticks=0;dirty=1;first_frame=0;
    { unsigned before=scan_calls; assert(launcher_main()==130);assert(scan_calls==before); }
    assert(first_frame);assert(!access("run/cherrypi-boot-timing.log",R_OK));
    assert(brightness==10);assert(event_index==6);
    view=HOME;row=0;scenario=0;library_loaded=0;
    { unsigned before=scan_calls;key(SDLK_LALT);assert(scan_calls==before+1 && library_loaded && view==GAMES); }
    /* Rapid input must take effect before any animation finishes; idle stops presenting. */
    library_free(&games);library_free(&recent);
    view=HOME;row=0;scenario=2;event_index=0;ticks=0;dirty=1;first_frame=0;present_count=0;
    assert(launcher_main()==130 && event_index==9 && view==HOME && row==0);
    assert(last_present<=200 && present_count<=14 && !motion.page && !motion.focus);
    scenario=0;first_frame=1;message[0]=0;view=HOME;row=0;ticks=2000;motion_reset(&motion);render();snapshot("page-before");
    key(SDLK_RCTRL);assert(view==SETTINGS && motion.page);
    for(i=0;i<=3;i++) { char name[32];ticks=2000+i*40;motion_tick(&motion,ticks);render();snprintf(name,sizeof(name),"page-%03d",i*40);snapshot(name); }
    assert(!motion.page && body_offset==0);
    ticks=2200;key(SDLK_DOWN);assert(row==1 && motion.focus);
    for(i=0;i<4;i++) { static const int at[]={0,40,80,100};char name[32];ticks=2200+at[i];motion_tick(&motion,ticks);render();snprintf(name,sizeof(name),"focus-%03d",at[i]);snapshot(name); }
    assert(!motion.focus && focus_mix==256);
    puts("PASS: ROM scan/history, settings preview/save/cancel, bounds, shutdown confirmation, exact exec argument, rendered font");return 0;
}
