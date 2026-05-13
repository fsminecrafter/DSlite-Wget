// keyboard.c – touchscreen QWERTY keyboard on the bottom screen
// Layout: 3 rows of keys + spacebar + backspace + confirm/cancel.
// All interaction is stylus-tap driven; no hardware key repeat needed.

#include <nds.h>
#include <string.h>
#include <stdio.h>
#include "keyboard.h"
#include "gfx.h"

// ── Key layout ────────────────────────────────────────────────────────────────
// Row 0: 1234567890-
// Row 1: qwertyuiop
// Row 2: asdfghjkl.
// Row 3: zxcvbnm/
// Row 4: [SPACE]  [DEL]  [SHIFT]  [OK]  [CANCEL]

#define ROWS       4
#define KEY_W      20   // pixel width per key tile
#define KEY_H      18   // pixel height
#define KEY_START_Y 48  // top of keyboard on bottom screen (pixels)

static const char *row_keys[ROWS] = {
    "1234567890-",
    "qwertyuiop",
    "asdfghjkl.",
    "zxcvbnm/",
};

static const char *row_keys_shift[ROWS] = {
    "!@#$%^&*()_",
    "QWERTYUIOP",
    "ASDFGHJKL:",
    "ZXCVBNM?",
};

// Action key IDs returned by touch_to_key when hitting specials
#define AK_NONE    -1
#define AK_SPACE   -2
#define AK_DEL     -3
#define AK_SHIFT   -4
#define AK_OK      -5
#define AK_CANCEL  -6

// ── Drawing ───────────────────────────────────────────────────────────────────

static int s_shift = 0;

// Draw a single key label centred in a tile
static void draw_key(int px, int py, const char *label, int col) {
    gfx_puts_bot(px / 6, py / 12, col, label);
}

static void draw_keyboard(void) {
    int r, c;
    for (r = 0; r < ROWS; r++) {
        const char *keys = s_shift ? row_keys_shift[r] : row_keys[r];
        int nk = strlen(keys);
        for (c = 0; c < nk; c++) {
            int px = 2 + c * KEY_W;
            int py = KEY_START_Y + r * KEY_H;
            char buf[3] = { keys[c], 0, 0 };
            draw_key(px, py, buf, COL_TEXT);
        }
    }
    // Bottom action row (y = KEY_START_Y + 4*KEY_H)
    int ay = KEY_START_Y + ROWS * KEY_H;
    gfx_puts_bot(0,  ay / 12, COL_HILITE, "[SPACE]");
    gfx_puts_bot(9,  ay / 12, COL_ERROR,  "[DEL]");
    gfx_puts_bot(15, ay / 12, COL_GREY,   "[SHF]");
    gfx_puts_bot(20, ay / 12, COL_OK,     "[OK]");
    gfx_puts_bot(25, ay / 12, COL_ERROR,  "[X]");
}

// ── Touch hit-test ─────────────────────────────────────────────────────────────

// Returns printable char, or AK_* negative code, or AK_NONE if miss.
static int touch_to_key(int tx, int ty) {
    int r;
    for (r = 0; r < ROWS; r++) {
        int row_y = KEY_START_Y + r * KEY_H;
        if (ty < row_y || ty >= row_y + KEY_H) continue;
        const char *keys = s_shift ? row_keys_shift[r] : row_keys[r];
        int nk = strlen(keys);
        int c;
        for (c = 0; c < nk; c++) {
            int kx = 2 + c * KEY_W;
            if (tx >= kx && tx < kx + KEY_W) return (unsigned char)keys[c];
        }
    }
    // Action row
    int ay = KEY_START_Y + ROWS * KEY_H;
    if (ty >= ay && ty < ay + KEY_H) {
        if (tx <  7 * 6) return AK_SPACE;
        if (tx < 14 * 6) return AK_DEL;
        if (tx < 20 * 6) return AK_SHIFT;
        if (tx < 25 * 6) return AK_OK;
        return AK_CANCEL;
    }
    return AK_NONE;
}

// ── Public API ─────────────────────────────────────────────────────────────────

int kb_get_string(char *buf, int maxlen, const char *prompt) {
    buf[0] = '\0';
    int len = 0;
    s_shift = 0;

    while (1) {
        // Redraw bottom screen
        gfx_clear_bottom();
        gfx_puts_bot(0, 0, COL_TITLE, prompt);
        gfx_hline_bot(1, COL_BORDER);

        // Input field (row 2)
        char display[35];
        snprintf(display, sizeof(display), "> %s_", buf);
        gfx_puts_bot(0, 2, COL_HILITE, display);

        gfx_hline_bot(3, COL_BORDER);
        draw_keyboard();

        swiWaitForVBlank();

        scanKeys();
        u32 held = keysDown();

        // Hardware shortcuts
        if (held & KEY_START) return 1;   // confirm
        if (held & KEY_B)     return 0;   // cancel

        // Stylus / touch
        if (held & KEY_TOUCH) {
            touchPosition tp;
            touchRead(&tp);
            int k = touch_to_key(tp.px, tp.py);
            switch (k) {
                case AK_NONE:   break;
                case AK_SPACE:
                    if (len < maxlen - 1) { buf[len++] = ' '; buf[len] = '\0'; }
                    break;
                case AK_DEL:
                    if (len > 0) { buf[--len] = '\0'; }
                    break;
                case AK_SHIFT:
                    s_shift = !s_shift;
                    break;
                case AK_OK:
                    return 1;
                case AK_CANCEL:
                    return 0;
                default:
                    if (len < maxlen - 1) {
                        buf[len++] = (char)k;
                        buf[len]   = '\0';
                        if (s_shift) s_shift = 0; // auto-release shift
                    }
                    break;
            }
            // Debounce: wait for stylus lift
            while (keysHeld() & KEY_TOUCH) {
                swiWaitForVBlank();
                scanKeys();
            }
        }
    }
}
