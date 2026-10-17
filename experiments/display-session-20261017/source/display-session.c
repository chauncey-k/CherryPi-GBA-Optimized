/* Keep the LCD virtual console graphical for the appliance session.
 * GPIO evdev and the serial terminal remain independent. GPL-2.0-or-later. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <termios.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/kd.h>
#include <linux/vt.h>
#ifndef STATE_FILE
#define STATE_FILE "/run/cherrypi-display-session.state"
#endif
struct saved { int vt, display, keyboard; };
static int error(const char *what) { perror(what); return 1; }
static int valid(const struct saved *s) {
    return s->vt >= 1 && s->vt <= 63 &&
        (s->display == KD_TEXT || s->display == KD_GRAPHICS) &&
        s->keyboard >= K_RAW && s->keyboard <= K_OFF;
}
/* 0 absent, 1 valid, -1 invalid/unreadable. Never follow state symlinks. */
static int read_state(struct saved *s) {
    int fd, n; FILE *f; char tail; struct stat st;
    fd=open(STATE_FILE,O_RDONLY|O_NOFOLLOW|O_CLOEXEC);
    if(fd<0) return errno==ENOENT ? 0 : -1;
    if(fstat(fd,&st) || !S_ISREG(st.st_mode) || st.st_size>128) {
        close(fd);errno=EINVAL;return -1;
    }
    f=fdopen(fd,"r");if(!f) { close(fd);return -1; }
    n=fscanf(f,"CPDISPLAY1 %d %d %d %c",&s->vt,&s->display,&s->keyboard,&tail);
    if(fclose(f) || n!=3 || !valid(s)) { errno=EINVAL;return -1; }
    return 1;
}
static int save_state(const struct saved *s) {
    char name[PATH_MAX]; int fd, ok, e; FILE *f;
    if(snprintf(name,sizeof(name),"%s.tmp.XXXXXX",STATE_FILE)>=(int)sizeof(name)) {errno=ENAMETOOLONG;return -1;}
    fd=mkstemp(name);if(fd<0)return -1;
    f=fdopen(fd,"w");if(!f){e=errno;close(fd);unlink(name);errno=e;return -1;}
    ok=fprintf(f,"CPDISPLAY1 %d %d %d\n",s->vt,s->display,s->keyboard)>0;
    if(fflush(f) || fsync(fd))ok=0;
    e=errno;if(fclose(f)){ok=0;e=errno;}
    if(ok && !rename(name,STATE_FILE))return 0;
    if(ok)e=errno;
    unlink(name);errno=e;return -1;
}
static int open_vt(int vt) {
    char name[32];snprintf(name,sizeof(name),"/dev/tty%d",vt);
    return open(name,O_RDWR|O_NOCTTY|O_CLOEXEC);
}
static int active_vt(void) {
    struct vt_stat st;int fd=open_vt(0),result;
    if(fd<0)return -1;
    result=ioctl(fd,VT_GETSTATE,&st);close(fd);
    if(result<0)return -1;
    if(st.v_active<1 || st.v_active>63){errno=EINVAL;return -1;}
    return st.v_active;
}
static int restore(int fd,const struct saved *s) {
    int bad=0,e=0;
    (void)tcflush(fd,TCIFLUSH);
    if(ioctl(fd,KDSETMODE,s->display)<0){bad=1;e=errno;}
    if(ioctl(fd,KDSKBMODE,s->keyboard)<0){bad=1;e=errno;}
    if(bad)errno=e;
    return bad ? -1 : 0;
}
static int acquire(void) {
    struct saved s;int fd,found=read_state(&s),e;
    if(found<0)return error("LCD state read");
    if(!found) {
        s.vt=active_vt();if(s.vt<0)return error("LCD active VT");
    }
    fd=open_vt(s.vt);if(fd<0)return error("LCD VT open");
    if(!found) {
        if(ioctl(fd,KDGETMODE,&s.display)<0 || ioctl(fd,KDGKBMODE,&s.keyboard)<0 || !valid(&s)) {
            close(fd);return error("LCD modes read");
        }
        if(save_state(&s)<0){close(fd);return error("LCD state save");}
    }
    if(ioctl(fd,KDSETMODE,KD_GRAPHICS)<0 || ioctl(fd,KDSKBMODE,K_OFF)<0) {
        e=errno;
        if(!restore(fd,&s))unlink(STATE_FILE);
        close(fd);errno=e;return error("LCD graphics session");
    }
    /* Clear only the LCD's old terminal buffer while fbcon is suspended. */
    (void)tcflush(fd,TCIFLUSH);
    (void)write(fd,"\033[2J\033[H",7);
    close(fd);
    printf("LCD session: tty%d graphics, console keyboard off\n",s.vt);
    return 0;
}
static int release(void) {
    struct saved s;int fd,found=read_state(&s),result;
    if(!found)return 0;
    if(found<0)return error("LCD state read");
    fd=open_vt(s.vt);if(fd<0)return error("LCD VT open");
    result=restore(fd,&s);close(fd);
    if(result<0)return error("LCD modes restore");
    if(unlink(STATE_FILE)<0)return error("LCD state remove");
    printf("LCD session restored: tty%d\n",s.vt);return 0;
}
static int status(void) {
    struct saved s;int vt=active_vt(),fd,display,keyboard,found;
    if(vt<0)return error("LCD active VT");
    fd=open_vt(vt);if(fd<0)return error("LCD VT open");
    if(ioctl(fd,KDGETMODE,&display)<0 || ioctl(fd,KDGKBMODE,&keyboard)<0) {close(fd);return error("LCD status");}
    close(fd);found=read_state(&s);
    if(found<0)return error("LCD state read");
    printf("LCD tty%d display=%s keyboard=%s saved_session=%s\n",vt,
        display==KD_GRAPHICS?"graphics":"text",keyboard==K_OFF?"off":"enabled",found?"yes":"no");
    return 0;
}
int main(int argc,char **argv) {
    if(geteuid()!=0){fputs("Root required\n",stderr);return 1;}
    if(argc==2) {
        if(!strcmp(argv[1],"acquire"))return acquire();
        if(!strcmp(argv[1],"release"))return release();
        if(!strcmp(argv[1],"status"))return status();
    }
    fputs("Usage: cherrypi-display-session acquire|release|status\n",stderr);return 2;
}
