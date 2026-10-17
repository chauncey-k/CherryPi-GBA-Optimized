/* Static Midnight Mint palette and spacing. GPL-2.0-or-later. */
#ifndef CHERRYPI_THEME_H
#define CHERRYPI_THEME_H
#define RGB565(r,g,b) (((r)>>3)<<11|((g)>>2)<<5|((b)>>3))
enum {
    UI_BG=RGB565(12,20,32), UI_PANEL=RGB565(22,34,48),
    UI_SELECTED=RGB565(32,61,72), UI_LINE=RGB565(45,65,80),
    UI_TEXT=RGB565(239,246,247), UI_MUTED=RGB565(155,177,194),
    UI_ACCENT=RGB565(112,224,192), UI_WARN=RGB565(255,201,112),
    UI_DARK=RGB565(12,28,32)
};
#endif
