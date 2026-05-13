#pragma once
#include <nds.h>

// Screen dimensions
#define SCREEN_W  256
#define SCREEN_H  192

// Colour palette indices (we use a simple 16-colour text palette)
#define COL_BG       0   // black
#define COL_TEXT     1   // white
#define COL_HILITE   2   // cyan  – selected item
#define COL_TITLE    3   // yellow
#define COL_ERROR    4   // red
#define COL_OK       5   // green
#define COL_GREY     6   // dark grey – inactive
#define COL_BORDER   7   // light blue

// Simple console helpers (top screen = main engine)
void gfx_init(void);
void gfx_clear_top(void);
void gfx_clear_bottom(void);
// Print at tile column x, row y with colour index col (top screen)
void gfx_puts_top(int x, int y, int col, const char *str);
// Same for bottom screen
void gfx_puts_bot(int x, int y, int col, const char *str);
// Draw a horizontal line of dashes
void gfx_hline_top(int y, int col);
void gfx_hline_bot(int y, int col);
// Small progress bar (bottom screen), 0-100
void gfx_progress_bar(int y, int percent, int col);
