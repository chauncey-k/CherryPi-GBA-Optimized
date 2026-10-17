/* Host tests use real menu, font, library and exec; hardware APIs are mocked. */
#define main launcher_main
#include "../source/launcher.c"
#undef main
#include <assert.h>
#include <sys/wait.h>
#include <sys/stat.h>
static Uint32 ticks;
static int fail_settings,preview_count,poweroff_count;
static int scenario,event_index;
static u16 pixels[320*240];
static SDL_Surface surface={pixels,640};
int SDL_Init(unsigned flags) { (void)flags; return 0; }
void SDL_Quit(void) {}
const char *SDL_GetError(void) { return "mock"; }
Uint32 SDL_GetTicks(void) { return ticks; }
void SDL_Delay(unsigned ms) { ticks+=ms; }
int SDL_FillRect(SDL_Surface *s,SDL_Rect *r,Uint32 color) {
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
    (void)menu;
    if(!scenario || event_index>=6 || ticks<sequence[event_index].at) return 0;
    e->type=sequence[event_index].type;e->key.keysym.sym=sequence[event_index].key;event_index++;return 1;
}
SDL_Surface *cherrypi_video_mode(int w,int h,int d,Uint32 flags) { (void)w;(void)h;(void)d;(void)flags;return &surface; }
int cherrypi_present(SDL_Surface *s) { (void)s;return 0; }
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
    for(i=0;i<12;i++) { char name[PATH_MAX];snprintf(name,sizeof(name),"%s/roms/%02d.gba",p,i);put(name,"ROM");assert(recent_record(HISTORY_FILE,name)); }
    assert(recent_load(&l,HISTORY_FILE));assert(l.count==10);
    assert(recent_record(HISTORY_FILE,l.paths[4]));library_free(&l);assert(recent_load(&l,HISTORY_FILE));assert(l.count==10);library_free(&l);
    screen=&surface;view=HOME;row=0;render();snapshot("home");
    row=2;key(SDLK_LALT);assert(view==SETTINGS);
    row=1;key(SDLK_RIGHT);assert(brightness==6 && dirty);render();assert(!dirty);snapshot("settings");
    for(i=0;i<20;i++) key(SDLK_RIGHT);assert(brightness==10);
    for(i=0;i<20;i++) key(SDLK_LEFT);assert(brightness==1);
    key(SDLK_LCTRL);assert(view==HOME && brightness==5);
    row=2;key(SDLK_LALT);row=0;key(SDLK_RIGHT);assert(volume==50 && preview_count>0);
    fail_settings=1;key(SDLK_LALT);assert(view==SETTINGS && strstr(message,"failure"));
    fail_settings=0;key(SDLK_LALT);assert(view==HOME && volume==50);
    view=HOME;row=3;key(SDLK_LALT);assert(view==POWEROFF && !confirm_row);
    key(SDLK_LALT);assert(view==HOME && !poweroff_count);
    row=3;key(SDLK_LALT);key(SDLK_DOWN);assert(key(SDLK_LALT)==1 && poweroff_count==1);
    view=RECENT;row=0;key(SDLK_LCTRL);assert(view==HOME);
    view=GAMES;row=0;render();snapshot("games");
    { int lit=0;for(i=0;i<320*30;i++) if(pixels[i]==0xffff) lit++;assert(lit>10); }
    put(EMULATOR_WRAPPER,"#!/bin/sh\nprintf '%s\\n' \"$1\" > run/received-rom\nexit 0\n");assert(!chmod(EMULATOR_WRAPPER,0755));
    pid=fork();assert(pid>=0);if(!pid) { int r=launch(expected);_exit(r<0?1:2); }
    assert(waitpid(pid,&status,0)==pid && WIFEXITED(status) && !WEXITSTATUS(status));
    { char actual[PATH_MAX];FILE *f=fopen("run/received-rom","r");assert(f && fgets(actual,sizeof(actual),f));fclose(f);actual[strcspn(actual,"\n")]=0;assert(!strcmp(actual,expected)); }
    assert(!access("run/cherrypi-launcher-game",F_OK));
    library_free(&games);library_free(&recent);
    view=HOME;row=2;scenario=1;ticks=0;dirty=1;
    assert(launcher_main()==130);assert(brightness==10);assert(event_index==6);
    puts("PASS: ROM scan/history, settings preview/save/cancel, bounds, shutdown confirmation, exact exec argument, rendered font");return 0;
}
