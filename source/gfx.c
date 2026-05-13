// gfx.c – simple two-screen text renderer using libnds consoles
#include <nds.h>
#include <stdio.h>
#include <string.h>
#include "gfx.h"

// We use the built-in libnds PrintConsole for simplicity.
// Top screen = main display (ARM9 main engine)
// Bottom screen = sub display (ARM9 sub engine)

static PrintConsole topConsole;
static PrintConsole botConsole;

void gfx_init(void) {
    videoSetMode(MODE_0_2D);
    videoSetModeSub(MODE_0_2D);

    vramSetBankA(VRAM_A_MAIN_BG);
    vramSetBankC(VRAM_C_SUB_BG);

    consoleInit(&topConsole, 3, BgType_Text4bpp, BgSize_T_256x256, 31, 0, true, true);
    consoleInit(&botConsole, 3, BgType_Text4bpp, BgSize_T_256x256, 31, 0, false, true);

    // Palette: index 0=BG(black), 1=white, 2=cyan, 3=yellow, 4=red, 5=green, 6=grey, 7=light-blue
    // libnds default console uses BG_PALETTE[n*16] for each colour set;
    // we just tweak colour 0 of the first 8 foreground slots.
    BG_PALETTE[0]  = RGB15(0,0,0);       // black bg
    BG_PALETTE[1]  = RGB15(31,31,31);    // white
    BG_PALETTE[2]  = RGB15(0,31,31);     // cyan
    BG_PALETTE[3]  = RGB15(31,31,0);     // yellow
    BG_PALETTE[4]  = RGB15(31,0,0);      // red
    BG_PALETTE[5]  = RGB15(0,31,0);      // green
    BG_PALETTE[6]  = RGB15(15,15,15);    // grey
    BG_PALETTE[7]  = RGB15(10,20,31);    // light blue

    BG_PALETTE_SUB[0] = RGB15(0,0,0);
    BG_PALETTE_SUB[1] = RGB15(31,31,31);
    BG_PALETTE_SUB[2] = RGB15(0,31,31);
    BG_PALETTE_SUB[3] = RGB15(31,31,0);
    BG_PALETTE_SUB[4] = RGB15(31,0,0);
    BG_PALETTE_SUB[5] = RGB15(0,31,0);
    BG_PALETTE_SUB[6] = RGB15(15,15,15);
    BG_PALETTE_SUB[7] = RGB15(10,20,31);
}

void gfx_clear_top(void) {
    consoleSelect(&topConsole);
    consoleClear();
}

void gfx_clear_bottom(void) {
    consoleSelect(&botConsole);
    consoleClear();
}

// libnds console uses ANSI escape codes for colour:
// \x1b[3Xm  = set foreground colour (X = 0-7, maps to our palette)
static const char *col_esc[] = {
    "\x1b[30m", // 0 black (bg – rarely used for text)
    "\x1b[37m", // 1 white
    "\x1b[36m", // 2 cyan
    "\x1b[33m", // 3 yellow
    "\x1b[31m", // 4 red
    "\x1b[32m", // 5 green
    "\x1b[90m", // 6 dark grey (bright black)
    "\x1b[34m", // 7 blue
};

void gfx_puts_top(int x, int y, int col, const char *str) {
    consoleSelect(&topConsole);
    // Position cursor: \x1b[row;colH  (1-based)
    iprintf("\x1b[%d;%dH%s%s", y + 1, x + 1,
            (col >= 0 && col < 8) ? col_esc[col] : col_esc[1], str);
}

void gfx_puts_bot(int x, int y, int col, const char *str) {
    consoleSelect(&botConsole);
    iprintf("\x1b[%d;%dH%s%s", y + 1, x + 1,
            (col >= 0 && col < 8) ? col_esc[col] : col_esc[1], str);
}

void gfx_hline_top(int y, int col) {
    char line[33];
    memset(line, '-', 32);
    line[32] = '\0';
    gfx_puts_top(0, y, col, line);
}

void gfx_hline_bot(int y, int col) {
    char line[33];
    memset(line, '-', 32);
    line[32] = '\0';
    gfx_puts_bot(0, y, col, line);
}

void gfx_progress_bar(int y, int percent, int col) {
    char bar[33];
    int filled = (percent * 30) / 100;
    int i;
    bar[0] = '[';
    for (i = 1; i <= 30; i++) bar[i] = (i <= filled) ? '#' : ' ';
    bar[31] = ']';
    bar[32] = '\0';
    gfx_puts_bot(0, y, col, bar);
    char pct[8];
    siprintf(pct, " %3d%%", percent);
    gfx_puts_bot(0, y + 1, col, pct);
}
