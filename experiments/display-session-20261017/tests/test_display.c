/* Exercise the real state/mode code; no real tty or input device is opened. */
#define STATE_FILE "session.state"
#define main display_main
#include "../source/display-session.c"
#undef main
#include <assert.h>
#include <stdarg.h>
static int display=KD_TEXT,keyboard=K_UNICODE,fail_request,fail_rename;
static int tty_opens,flushes,clears;static uid_t user;
int __real_open(const char *,int,...);
int __real_close(int);
ssize_t __real_write(int,const void *,size_t);
int __real_rename(const char *,const char *);
int __wrap_open(const char *name,int flags,...) {
    mode_t mode=0;
    if(flags&O_CREAT){va_list ap;va_start(ap,flags);mode=(mode_t)va_arg(ap,int);va_end(ap);}
    if(!strcmp(name,"/dev/tty0")){tty_opens++;return 100;}
    if(!strcmp(name,"/dev/tty1")){tty_opens++;return 101;}
    assert(strncmp(name,"/dev/",5));return __real_open(name,flags,mode);
}
int __wrap_close(int fd){return fd==100 || fd==101 ? 0 : __real_close(fd);}
uid_t __wrap_geteuid(void){return user;}
int __wrap_ioctl(int fd,unsigned long request,...) {
    va_list ap;assert(fd==100 || fd==101);va_start(ap,request);
    if((int)request==fail_request){fail_request=0;va_end(ap);errno=EIO;return -1;}
    switch(request) {
    case VT_GETSTATE:va_arg(ap,struct vt_stat*)->v_active=1;break;
    case KDGETMODE:*va_arg(ap,int*)=display;break;
    case KDGKBMODE:*va_arg(ap,int*)=keyboard;break;
    case KDSETMODE:display=va_arg(ap,int);break;
    case KDSKBMODE:keyboard=va_arg(ap,int);break;
    default:assert(0);
    }
    va_end(ap);return 0;
}
int __wrap_tcflush(int fd,int queue){assert(fd==101 && queue==TCIFLUSH);flushes++;return 0;}
ssize_t __wrap_write(int fd,const void *p,size_t n){
    if(fd==101){assert(n==7 && !memcmp(p,"\033[2J\033[H",7));clears++;return n;}
    assert(fd!=0);return __real_write(fd,p,n);
}
int __wrap_rename(const char *a,const char *b){if(fail_rename){errno=EACCES;return -1;}return __real_rename(a,b);}
static void state_is(int d,int k){struct saved s;assert(read_state(&s)==1 && s.display==d && s.keyboard==k);}
int main(void) {
    char *args[]={"display","acquire",NULL};struct saved s;
    assert(!release() && tty_opens==0);
    assert(!acquire());assert(display==KD_GRAPHICS && keyboard==K_OFF);state_is(KD_TEXT,K_UNICODE);
    assert(!acquire());state_is(KD_TEXT,K_UNICODE); /* no overwrite on restart */
    assert(clears==2 && flushes==2);assert(!status());
    /* Menu and game see GRAPHICS as their saved mode across their exits. */
    display=KD_GRAPHICS;assert(!acquire());state_is(KD_TEXT,K_UNICODE);
    assert(!release());assert(display==KD_TEXT && keyboard==K_UNICODE);assert(read_state(&s)==0);
    assert(!release());
    fail_request=KDSKBMODE;assert(acquire()==1);
    assert(display==KD_TEXT && keyboard==K_UNICODE && read_state(&s)==0);
    fail_request=KDSETMODE;assert(acquire()==1);
    assert(display==KD_TEXT && keyboard==K_UNICODE && read_state(&s)==0);
    fail_rename=1;assert(acquire()==1);fail_rename=0;
    assert(display==KD_TEXT && keyboard==K_UNICODE && read_state(&s)==0);
    assert(!acquire());fail_request=KDSETMODE;assert(release()==1);assert(read_state(&s)==1);
    assert(!release());assert(display==KD_TEXT && keyboard==K_UNICODE);
    FILE *f=fopen(STATE_FILE,"w");assert(f);fputs("CPDISPLAY1 999 0 3\n",f);fclose(f);
    assert(acquire()==1 && release()==1);assert(display==KD_TEXT && keyboard==K_UNICODE);unlink(STATE_FILE);
    assert(!symlink("nonexistent-target",STATE_FILE));assert(acquire()==1);unlink(STATE_FILE);
    display=KD_GRAPHICS;keyboard=K_RAW;assert(!acquire());state_is(KD_GRAPHICS,K_RAW);
    assert(!release());assert(display==KD_GRAPHICS && keyboard==K_RAW);
    user=1;assert(display_main(2,args)==1);
    puts("PASS actual LCD session: graphical handoff, keyboard suppression, idempotent acquire/release, rollback/retry, invalid state, serial/evdev untouched");
    return 0;
}
