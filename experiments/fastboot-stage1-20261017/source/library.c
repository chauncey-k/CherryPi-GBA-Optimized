/* Bounded ROM discovery and atomic recent history. GPL-2.0-or-later. */
#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "library.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>
const char *library_name(const char *p) { const char *s = strrchr(p, '/'); return s ? s+1 : p; }
void library_free(Library *l) { size_t i; for(i=0;i<l->count;i++) free(l->paths[i]); free(l->paths); memset(l,0,sizeof(*l)); }
static int add(Library *l,const char *p,size_t max) {
    char **a; char *copy;
    if(l->count >= max) { l->truncated=1; return 1; }
    copy=strdup(p); if(!copy) return 0;
    a=realloc(l->paths,(l->count+1)*sizeof(*a));
    if(!a) { free(copy); return 0; }
    l->paths=a; l->paths[l->count++]=copy; return 1;
}
static int supported(const char *p) {
    const char *e=strrchr(p,'.');
    return e && (!strcasecmp(e,".gba") || !strcasecmp(e,".zip") || !strcasecmp(e,".bin"));
}
static int scan(Library *l,const char *directory,int depth) {
    DIR *d=opendir(directory); struct dirent *e; int ok=1;
    if(!d) return 0;
    while((e=readdir(d))) {
        char p[PATH_MAX]; struct stat s;
        if(e->d_name[0]=='.') continue;
        if(snprintf(p,sizeof(p),"%s/%s",directory,e->d_name)>=(int)sizeof(p)) { l->truncated=1; continue; }
        if(strchr(p,'\n') || strchr(p,'\r') || lstat(p,&s)) continue;
        if(S_ISDIR(s.st_mode)) {
            if(depth<8) { if(!scan(l,p,depth+1)) ok=0; } else l->truncated=1;
        } else if(S_ISREG(s.st_mode) && supported(p) && !add(l,p,LIBRARY_MAX)) { ok=0; break; }
        if(l->count>=LIBRARY_MAX) { l->truncated=1; break; }
    }
    closedir(d); return ok;
}
static int compare(const void *a,const void *b) {
    const char *pa=*(const char *const*)a,*pb=*(const char *const*)b;
    int n=strcasecmp(library_name(pa),library_name(pb)); return n ? n : strcmp(pa,pb);
}
int library_scan(Library *l,const char *directory) {
    int ok; library_free(l); ok=scan(l,directory,0);
    if(l->count>1) qsort(l->paths,l->count,sizeof(*l->paths),compare);
    return ok;
}
int recent_load(Library *l,const char *file) {
    FILE *f; char p[PATH_MAX]; library_free(l); f=fopen(file,"r");
    if(!f) return errno==ENOENT;
    while(fgets(p,sizeof(p),f)) {
        struct stat s; size_t i,n=strlen(p);
        if(n && p[n-1]=='\n') p[--n]=0;
        else if(!feof(f)) { int c; while((c=fgetc(f))!=EOF && c!='\n') {} continue; }
        if(!n || p[0]!='/' || strchr(p,'\r') || stat(p,&s) || !S_ISREG(s.st_mode) || !supported(p)) continue;
        for(i=0;i<l->count;i++) if(!strcmp(p,l->paths[i])) break;
        if(i==l->count && !add(l,p,RECENT_MAX)) { fclose(f); return 0; }
    }
    { int ok=!ferror(f); fclose(f); return ok; }
}
int recent_record(const char *file,const char *path) {
    Library l={0}; char temp[PATH_MAX]; FILE *f; int fd,ok=1; size_t i,count=1;
    if(path[0]!='/' || strchr(path,'\n') || strchr(path,'\r')) { errno=EINVAL; return 0; }
    if(!recent_load(&l,file)) { library_free(&l); return 0; }
    if(snprintf(temp,sizeof(temp),"%s.tmp.XXXXXX",file)>=(int)sizeof(temp)) { library_free(&l); errno=ENAMETOOLONG; return 0; }
    fd=mkstemp(temp); if(fd<0) { library_free(&l); return 0; }
    f=fdopen(fd,"w"); if(!f) { close(fd); unlink(temp); library_free(&l); return 0; }
    if(fprintf(f,"%s\n",path)<0) ok=0;
    for(i=0;i<l.count && count<RECENT_MAX;i++) if(strcmp(path,l.paths[i])) {
        if(fprintf(f,"%s\n",l.paths[i])<0) ok=0;
        count++;
    }
    if(fflush(f) || fsync(fd)) ok=0;
    if(fclose(f)) ok=0;
    if(ok && rename(temp,file)) ok=0;
    if(!ok) unlink(temp); else sync();
    library_free(&l); return ok;
}
