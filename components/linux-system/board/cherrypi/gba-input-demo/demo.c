/* Original GBA prototype demo. SPDX-License-Identifier: MIT
 * WASD moves the square; J/K changes its color; Enter centers it.
 * No BIOS SWI calls: tests CPU, RAM, mode-3 video, VCOUNT and KEYINPUT.
 */
typedef unsigned short u16;
#define REG16(addr) (*(volatile u16 *)(addr))
#define RGB(r,g,b) ((r) | ((g)<<5) | ((b)<<10))
#define VRAM ((volatile u16 *)0x06000000)
static void box(int x, int y, u16 color) {
    int row, col;
    for (row = 0; row < 16; ++row)
        for (col = 0; col < 16; ++col)
            VRAM[(y + row) * 240 + x + col] = color;
}
void demo_main(void) {
    int x = 112, y = 72, oldx = x, oldy = y, i;
    const u16 bg = RGB(2, 3, 6);
    REG16(0x04000208) = 0;
    REG16(0x04000000) = 0x0403;
    for (i = 0; i < 240 * 160; ++i) VRAM[i] = bg;
    /* Eight blocks along top make color/order checks easy. */
    for (i = 0; i < 240; ++i) {
        u16 color = i < 30 ? RGB(31,31,31) : i < 60 ? RGB(31,31,0) :
          i < 90 ? RGB(0,31,31) : i < 120 ? RGB(0,31,0) :
          i < 150 ? RGB(31,0,31) : i < 180 ? RGB(31,0,0) :
          i < 210 ? RGB(0,0,31) : RGB(0,0,0);
        int row;
        for (row = 0; row < 8; ++row) VRAM[row * 240 + i] = color;
    }
    while (1) {
        unsigned keys;
        u16 color = RGB(31,31,31);
        while (REG16(0x04000006) >= 160) {}
        while (REG16(0x04000006) < 160) {}
        keys = (~REG16(0x04000130)) & 0x3ff;
        oldx = x; oldy = y;
        if ((keys & 0x10) && x < 224) x += 2;
        if ((keys & 0x20) && x > 0) x -= 2;
        if ((keys & 0x40) && y > 10) y -= 2;
        if ((keys & 0x80) && y < 144) y += 2;
        if (keys & 8) { x = 112; y = 72; }
        if (keys & 1) color = RGB(31,0,0);
        if (keys & 2) color = RGB(0,31,0);
        box(oldx, oldy, bg);
        box(x, y, color);
    }
}
