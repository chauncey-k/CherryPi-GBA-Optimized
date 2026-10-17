/* Cherry Pi framebuffer and serial input adapter. GPL-2.0-or-later. */
#include "platform.h"
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <linux/fb.h>
#include <linux/input.h>
#include <linux/kd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
static SDL_Surface *physical, *canvas;
static int fbfd = -1, vtfd = -1, saved_kdmode;
static unsigned char *fbmem;
static struct fb_fix_screeninfo finfo;
static struct fb_var_screeninfo vinfo;
static uint32_t colors[65536];
static int clear_display;
static void close_framebuffer(void) {
    if (fbmem) { munmap(fbmem, finfo.smem_len); fbmem = NULL; }
    if (fbfd >= 0) { close(fbfd); fbfd = -1; }
    if (vtfd >= 0) { ioctl(vtfd, KDSETMODE, saved_kdmode); close(vtfd); vtfd = -1; }
}
static uint32_t channel(unsigned value, unsigned max, struct fb_bitfield field) {
    return ((value * ((1u << field.length) - 1) + max / 2) / max) << field.offset;
}
static void open_framebuffer(void) {
    unsigned i;
    fbfd = open("/dev/fb0", O_RDWR);
    if (fbfd < 0 || ioctl(fbfd, FBIOGET_FSCREENINFO, &finfo) ||
        ioctl(fbfd, FBIOGET_VSCREENINFO, &vinfo)) { perror("framebuffer"); exit(1); }
    if (vinfo.xres != 480 || vinfo.yres != 272 ||
        (vinfo.bits_per_pixel != 16 && vinfo.bits_per_pixel != 32) ||
        finfo.visual != FB_VISUAL_TRUECOLOR || finfo.type != FB_TYPE_PACKED_PIXELS ||
        vinfo.red.length > 8 || vinfo.green.length > 8 || vinfo.blue.length > 8 ||
        !vinfo.red.length || !vinfo.green.length || !vinfo.blue.length ||
        vinfo.red.offset + vinfo.red.length > vinfo.bits_per_pixel ||
        vinfo.green.offset + vinfo.green.length > vinfo.bits_per_pixel ||
        vinfo.blue.offset + vinfo.blue.length > vinfo.bits_per_pixel ||
        (vinfo.xoffset + 480) * (vinfo.bits_per_pixel / 8) > finfo.line_length ||
        (uint64_t)(vinfo.yoffset + 271) * finfo.line_length +
        (vinfo.xoffset + 480) * (vinfo.bits_per_pixel / 8) > finfo.smem_len) {
        fprintf(stderr, "Unsupported framebuffer layout: %ux%u %u bpp\n",
                vinfo.xres, vinfo.yres, vinfo.bits_per_pixel); exit(1);
    }
    fbmem = mmap(NULL, finfo.smem_len, PROT_READ | PROT_WRITE, MAP_SHARED, fbfd, 0);
    if (fbmem == MAP_FAILED) { fbmem = NULL; perror("mmap fb0"); exit(1); }
    atexit(close_framebuffer);
    vtfd = open("/dev/tty0", O_RDWR);
    if (vtfd >= 0) {
        if (ioctl(vtfd, KDGETMODE, &saved_kdmode) || ioctl(vtfd, KDSETMODE, KD_GRAPHICS)) {
            close(vtfd); vtfd = -1;
        }
    }
    for (i = 0; i < 65536; ++i)
        colors[i] = channel(i >> 11, 31, vinfo.red) |
                    channel((i >> 5) & 63, 63, vinfo.green) |
                    channel(i & 31, 31, vinfo.blue);
    fprintf(stderr, "fb0: %ux%u %u bpp, stride %u\n", vinfo.xres, vinfo.yres,
            vinfo.bits_per_pixel, finfo.line_length);
}
static struct termios saved_termios;
static int saved_flags, tty_ready;
static volatile sig_atomic_t interrupted;
static Uint32 deadlines[SDLK_LAST];
static int previous_menu = -1;
static int gamepad_fd = -1;
static void restore_input(void) {
    if (gamepad_fd >= 0) { close(gamepad_fd); gamepad_fd = -1; }
    if (tty_ready) {
        tcsetattr(STDIN_FILENO, TCSANOW, &saved_termios);
        fcntl(STDIN_FILENO, F_SETFL, saved_flags);
        tty_ready = 0;
    }
}
void cherrypi_platform_close(void) { restore_input(); close_framebuffer(); }
static void on_interrupt(int sig) { (void)sig; interrupted = 1; }
static void open_gamepad(void) {
    unsigned i;
    for (i = 0; i < 32; ++i) {
        char path[64], name[128] = {0};
        int fd;
        snprintf(path, sizeof(path), "/dev/input/event%u", i);
        fd = open(path, O_RDONLY | O_NONBLOCK);
        if (fd < 0) continue;
        if (ioctl(fd, EVIOCGNAME(sizeof(name)), name) >= 0 &&
            strcmp(name, "cherrypi-gamepad") == 0) {
            gamepad_fd = fd;
            fprintf(stderr, "GPIO gamepad: %s\n", path);
            return;
        }
        close(fd);
    }
    fprintf(stderr, "GPIO gamepad not found; using serial keys only\n");
}
void cherrypi_input_init(void) {
    struct termios raw;
    if (!isatty(STDIN_FILENO) || tcgetattr(STDIN_FILENO, &saved_termios)) {
        fprintf(stderr, "gpSP needs an interactive serial terminal on stdin\n");
        exit(1);
    }
    saved_flags = fcntl(STDIN_FILENO, F_GETFL);
    raw = saved_termios;
    raw.c_lflag &= ~(ICANON | ECHO);
    raw.c_iflag &= ~(IXON | IXOFF);
    raw.c_cc[VMIN] = 0; raw.c_cc[VTIME] = 0;
    if (saved_flags < 0 || tcsetattr(STDIN_FILENO, TCSANOW, &raw) ||
        fcntl(STDIN_FILENO, F_SETFL, saved_flags | O_NONBLOCK)) {
        tcsetattr(STDIN_FILENO, TCSANOW, &saved_termios);
        perror("serial raw mode"); exit(1);
    }
    tty_ready = 1;
    open_gamepad();
    atexit(restore_input);
    signal(SIGINT, on_interrupt); signal(SIGTERM, on_interrupt);
}
static SDLKey translate(unsigned char c, int menu) {
    switch (c) {
    case 'w': case 'W': return SDLK_UP;
    case 's': case 'S': return SDLK_DOWN;
    case 'a': case 'A': return SDLK_LEFT;
    case 'd': case 'D': return SDLK_RIGHT;
    case 'j': case 'J': return SDLK_LALT;
    case 'k': case 'K': return SDLK_LCTRL;
    case 'u': case 'U': return SDLK_LSHIFT;
    case 'i': case 'I': return SDLK_SPACE;
    case '\r': case '\n': return menu ? SDLK_RETURN : SDLK_TAB;
    case ' ': return SDLK_BACKSPACE;
    case 'm': case 'M': return SDLK_RCTRL;
    case 'f': case 'F': case '`': return menu ? SDLK_UNKNOWN : SDLK_BACKQUOTE;
    default: return SDLK_UNKNOWN;
    }
}
static SDLKey gamepad_key(unsigned code, int menu) {
    switch (code) {
    case KEY_UP: return SDLK_UP;
    case KEY_DOWN: return SDLK_DOWN;
    case KEY_LEFT: return SDLK_LEFT;
    case KEY_RIGHT: return SDLK_RIGHT;
    case KEY_J: return SDLK_LALT;
    case KEY_K: return SDLK_LCTRL;
    case KEY_U: return SDLK_LSHIFT;
    case KEY_I: return SDLK_SPACE;
    case KEY_ENTER: return menu ? SDLK_RETURN : SDLK_TAB;
    case KEY_SPACE: return SDLK_BACKSPACE;
    case KEY_M: return SDLK_RCTRL;
    case KEY_F: return menu ? SDLK_UNKNOWN : SDLK_BACKQUOTE;
    default: return SDLK_UNKNOWN;
    }
}
static int poll_gamepad(SDL_Event *event, int menu) {
    struct input_event input;
    while (gamepad_fd >= 0 && read(gamepad_fd, &input, sizeof(input)) == sizeof(input)) {
        SDLKey key;
        if (input.type != EV_KEY || input.value == 2) continue;
        key = gamepad_key(input.code, menu);
        if (key == SDLK_UNKNOWN) continue;
        event->type = input.value ? SDL_KEYDOWN : SDL_KEYUP;
        event->key.state = input.value ? SDL_PRESSED : SDL_RELEASED;
        event->key.keysym.sym = key;
        return 1;
    }
    return 0;
}
int cherrypi_poll_event(SDL_Event *event, int menu) {
    unsigned char c;
    int key;
    Uint32 now = SDL_GetTicks();
    memset(event, 0, sizeof(*event));
    if (interrupted) { interrupted = 0; event->type = SDL_QUIT; return 1; }
    /* Release game keys before accepting menu keys or vice versa. */
    for (key = 0; key < SDLK_LAST; ++key) {
        if (deadlines[key] && (previous_menu != menu || (Sint32)(now - deadlines[key]) >= 0)) {
            deadlines[key] = 0;
            event->type = SDL_KEYUP; event->key.state = SDL_RELEASED;
            event->key.keysym.sym = key;
            return 1;
        }
    }
    previous_menu = menu;
    if (poll_gamepad(event, menu)) return 1;
    while (read(STDIN_FILENO, &c, 1) == 1) {
        if (c == 'q' || c == 'Q') { event->type = SDL_QUIT; return 1; }
        key = translate(c, menu);
        if (key == SDLK_UNKNOWN) continue;
        int was_down = deadlines[key] != 0;
        deadlines[key] = now + 180;
        if (was_down) continue;
        event->type = SDL_KEYDOWN; event->key.state = SDL_PRESSED;
        event->key.keysym.sym = key;
        return 1;
    }
    return SDL_PollEvent(event);
}
SDL_Surface *cherrypi_video_mode(int w, int h, int depth, Uint32 flags) {
    (void)depth; (void)flags;
    if (!physical) {
        open_framebuffer();
        physical = SDL_SetVideoMode(480, 272, 16, SDL_SWSURFACE);
        if (!physical) { fprintf(stderr, "SDL framebuffer: %s\n", SDL_GetError()); exit(1); }
        fprintf(stderr, "LCD: %dx%d, %d bpp; logical %dx%d\n", physical->w, physical->h,
                physical->format->BitsPerPixel, w, h);
        if (physical->w != 480 || physical->h != 272 || physical->format->BitsPerPixel != 16 ||
            physical->format->Rmask != 0xf800 || physical->format->Gmask != 0x07e0 || physical->format->Bmask != 0x001f) {
            fprintf(stderr, "Expected 480x272 framebuffer\n"); exit(1);
        }
    }
    if (canvas) SDL_FreeSurface(canvas);
    canvas = SDL_CreateRGBSurface(SDL_SWSURFACE, w, h, 16, 0xf800, 0x07e0, 0x001f, 0);
    if (!canvas) { fprintf(stderr, "SDL canvas: %s\n", SDL_GetError()); exit(1); }
    clear_display = 1;
    SDL_FillRect(physical, NULL, 0);
    return canvas;
}
int cherrypi_present(SDL_Surface *surface) {
    unsigned y, x, left, top;
    if (surface->w > 480 || surface->h > 272 ||
        surface->format->BitsPerPixel != 16) return -1;
    left = (480 - surface->w) / 2;
    top = (272 - surface->h) / 2;
    if (SDL_MUSTLOCK(surface) && SDL_LockSurface(surface)) return -1;
    /* Clear borders once per video mode, then copy only the active canvas. */
    if (clear_display) {
        for (y = 0; y < 272; ++y)
            memset(fbmem + (y + vinfo.yoffset) * finfo.line_length +
                   vinfo.xoffset * (vinfo.bits_per_pixel / 8), 0,
                   480 * (vinfo.bits_per_pixel / 8));
        clear_display = 0;
    }
    for (y = 0; y < (unsigned)surface->h; ++y) {
        const uint16_t *src = (const uint16_t *)
            ((const unsigned char *)surface->pixels + y * surface->pitch);
        unsigned char *dst = fbmem + (y + top + vinfo.yoffset) * finfo.line_length +
                             (left + vinfo.xoffset) * (vinfo.bits_per_pixel / 8);
        if (vinfo.bits_per_pixel == 32) {
            uint32_t *out = (uint32_t *)dst;
            for (x = 0; x < (unsigned)surface->w; ++x) out[x] = colors[src[x]];
        } else {
            uint16_t *out = (uint16_t *)dst;
            for (x = 0; x < (unsigned)surface->w; ++x) out[x] = colors[src[x]];
        }
    }
    if (SDL_MUSTLOCK(surface)) SDL_UnlockSurface(surface);
    return 0;
}

static void silence(void *userdata, Uint8 *stream, int length) {
    (void)userdata; memset(stream, 0, length);
}
int cherrypi_platform_test(void) {
    SDL_Event event;
    SDL_AudioSpec want = {0};
    SDL_Surface *test;
    int i;
    Uint32 palette[] = {0xffffff, 0xffff00, 0x00ffff, 0x00ff00, 0xff00ff, 0xff0000, 0x0000ff, 0};
    cherrypi_input_init();
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) < 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError()); return 1;
    }
    atexit(SDL_Quit);
    test = cherrypi_video_mode(320, 240, 16, 0);
    for (i = 0; i < 8; ++i) {
        SDL_Rect r = {i * 40, 0, 40, 240};
        SDL_FillRect(test, &r, SDL_MapRGB(test->format, palette[i] >> 16,
                     (palette[i] >> 8) & 255, palette[i] & 255));
    }
    if (cherrypi_present(test)) { fprintf(stderr, "Present: %s\n", SDL_GetError()); return 1; }
    want.freq = 22050; want.format = AUDIO_S16; want.channels = 2;
    want.samples = 512; want.callback = silence;
    if (SDL_OpenAudio(&want, NULL) < 0) { fprintf(stderr, "Audio: %s\n", SDL_GetError()); return 1; }
    SDL_PauseAudio(0);
    printf("Platform test: centered color bars; dummy audio open. Press WASD/J/K/U/I/Enter/Space/M; Q exits.\n");
    while (1) {
        while (cherrypi_poll_event(&event, 0)) {
            if (event.type == SDL_QUIT) { SDL_CloseAudio(); return 0; }
            if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP)
                printf("%s %s\n", event.type == SDL_KEYDOWN ? "down" : "up", SDL_GetKeyName(event.key.keysym.sym));
        }
        SDL_Delay(10);
    }
}
