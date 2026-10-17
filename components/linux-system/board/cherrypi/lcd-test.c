#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <linux/kd.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

static volatile sig_atomic_t stopped;
static void stop_test(int sig) { (void)sig; stopped = 1; }
static uint32_t component(unsigned value, struct fb_bitfield field)
{
    if (!field.length) return 0;
    return (uint32_t)(((value * ((1ULL << field.length) - 1) + 127) / 255) << field.offset);
}
static uint32_t pixel(unsigned r, unsigned g, unsigned b, const struct fb_var_screeninfo *v)
{
    return component(r, v->red) | component(g, v->green) |
           component(b, v->blue) | component(255, v->transp);
}
static int self_test(void)
{
    struct fb_var_screeninfo v = {0};
    v.red.offset=11; v.red.length=5; v.green.offset=5; v.green.length=6; v.blue.length=5;
    if (pixel(255,0,0,&v)!=0xf800 || pixel(0,255,0,&v)!=0x07e0 ||
        pixel(0,0,255,&v)!=0x001f || pixel(255,255,255,&v)!=0xffff) return 1;
    v.red.offset=16; v.red.length=8; v.green.offset=8; v.green.length=8;
    v.blue.offset=0; v.blue.length=8; v.transp.offset=24; v.transp.length=8;
    if (pixel(255,0,0,&v)!=0xffff0000 || pixel(0,0,255,&v)!=0xff0000ff) return 1;
    puts("PASS: RGB565 and ARGB8888 known-color packing");
    return 0;
}
int main(int argc, char **argv)
{
    static const unsigned colors[8][3] = {
        {255,255,255},{255,255,0},{0,255,255},{0,255,0},
        {255,0,255},{255,0,0},{0,0,255},{0,0,0}
    };
    static const char *names[8] = {"white","yellow","cyan","green","magenta","red","blue","black"};
    const char *pattern = argc == 2 ? argv[1] : "bars";
    int solid = -1, fd = -1, tty = -1, old_mode = KD_TEXT, changed_mode = 0, ret = 1;
    struct fb_var_screeninfo v;
    struct fb_fix_screeninfo f;
    unsigned cpp;
    uint8_t *map = MAP_FAILED;
    if (argc == 2 && !strcmp(pattern,"--self-test")) return self_test();
    if (argc > 2) { fprintf(stderr,"Usage: lcd-test [bars|red|green|blue|white|black|--self-test]\n"); return 2; }
    for (int i=0;i<8;i++) if (!strcmp(pattern,names[i])) solid=i;
    if (solid < 0 && strcmp(pattern,"bars")) { fprintf(stderr,"Unknown pattern: %s\n",pattern); return 2; }
    fd=open("/dev/fb0",O_RDWR);
    if (fd<0) { perror("open /dev/fb0 (check DRM/panel logs)"); goto done; }
    if (ioctl(fd,FBIOGET_VSCREENINFO,&v)<0 || ioctl(fd,FBIOGET_FSCREENINFO,&f)<0) { perror("framebuffer info"); goto done; }
    if ((v.bits_per_pixel!=16 && v.bits_per_pixel!=24 && v.bits_per_pixel!=32) ||
        f.type!=FB_TYPE_PACKED_PIXELS || f.visual!=FB_VISUAL_TRUECOLOR ||
        !v.xres || !v.yres || v.xres>4096 || v.yres>4096) {
        fprintf(stderr,"Unsupported framebuffer: %ux%u bpp=%u visual=%u\n",v.xres,v.yres,v.bits_per_pixel,f.visual); goto done;
    }
    const struct fb_bitfield fields[4]={v.red,v.green,v.blue,v.transp};
    for (unsigned i=0;i<4;i++) if (fields[i].length>8 || fields[i].offset+fields[i].length>v.bits_per_pixel || fields[i].msb_right) {
        fprintf(stderr,"Unsupported pixel bitfields\n"); goto done;
    }
    cpp=v.bits_per_pixel/8;
    uint64_t row_end=((uint64_t)v.xoffset+v.xres)*cpp;
    uint64_t end=((uint64_t)v.yoffset+v.yres-1)*f.line_length+row_end;
    if (row_end>f.line_length || end>f.smem_len) { fprintf(stderr,"Visible surface exceeds framebuffer bounds\n"); goto done; }
    map=mmap(NULL,f.smem_len,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);
    if (map==MAP_FAILED) { perror("mmap framebuffer"); goto done; }
    tty=open("/dev/tty0",O_RDWR);
    if (tty>=0 && ioctl(tty,KDGETMODE,&old_mode)==0 && ioctl(tty,KDSETMODE,KD_GRAPHICS)==0) changed_mode=1;
    signal(SIGINT,stop_test); signal(SIGTERM,stop_test); signal(SIGHUP,stop_test);
    printf("Framebuffer %ux%u, %u bpp, stride %u; pattern %s. Ctrl+C to stop.\n",v.xres,v.yres,v.bits_per_pixel,f.line_length,pattern);
    fflush(stdout);
    for (unsigned y=0;y<v.yres;y++) {
        uint8_t *row=map+((uint64_t)y+v.yoffset)*f.line_length+(uint64_t)v.xoffset*cpp;
        for (unsigned x=0;x<v.xres;x++) {
            unsigned i=solid>=0 ? (unsigned)solid : (unsigned)((uint64_t)x*8/v.xres);
            uint32_t value=pixel(colors[i][0],colors[i][1],colors[i][2],&v);
            for (unsigned byte=0;byte<cpp;byte++) row[(uint64_t)x*cpp+byte]=(uint8_t)(value>>(8*byte));
        }
    }
    while (!stopped) sleep(1);
    ret=0;
done:
    if (changed_mode) ioctl(tty,KDSETMODE,old_mode);
    if (tty>=0) close(tty);
    if (map!=MAP_FAILED) munmap(map,f.smem_len);
    if (fd>=0) close(fd);
    return ret;
}
