// main.c – DS-wget: scan, connect, download
// State machine: SCAN → CONNECT → (WEP KEY if needed) → MAIN MENU → URL → FOLDER → DOWNLOAD

#include <nds.h>
#include <fat.h>
#include <dswifi9.h>
#include <stdio.h>
#include <string.h>
#include "gfx.h"
#include "wifi_mgr.h"
#include "keyboard.h"
#include "folder_picker.h"
#include "downloader.h"

// ── App states ────────────────────────────────────────────────────────────────
typedef enum {
    ST_SCAN,         // Scanning for APs
    ST_AP_LIST,      // Showing AP list, user picks one
    ST_WEP_KEY,      // Entering WEP key
    ST_CONNECTING,   // Waiting for association
    ST_MAIN,         // Connected: main menu
    ST_URL_INPUT,    // Touchscreen keyboard for URL
    ST_FOLDER,       // Folder picker
    ST_DOWNLOADING,  // Active download
    ST_ERROR,        // Fatal error display
} AppState;

// ── Helpers ───────────────────────────────────────────────────────────────────

static void show_header(const char *title) {
    gfx_clear_top();
    gfx_puts_top(0, 0, COL_TITLE, "=== DS-wget ===");
    gfx_hline_top(1, COL_BORDER);
    gfx_puts_top(0, 2, COL_HILITE, title);
    gfx_hline_top(3, COL_BORDER);
}

// Draw signal-strength bar: 0-100 → [####    ]
static void rssi_bar(int rssi, char *out, int outlen) {
    int bars = (rssi * 8) / 100;
    if (bars > 8) bars = 8;
    out[0] = '[';
    int i;
    for (i = 1; i <= 8; i++) out[i] = (i <= bars) ? '#' : ' ';
    out[9] = ']';
    out[10] = '\0';
    (void)outlen;
}

// ── AP list screen (top) ──────────────────────────────────────────────────────

#define AP_VISIBLE 10

static void draw_ap_list(APEntry *aps, int count, int sel, int scroll) {
    show_header("Networks Found (UP/DOWN, A=Connect, R=Rescan)");

    if (count == 0) {
        gfx_puts_top(2, 5, COL_GREY, "No networks found.");
        gfx_puts_top(2, 7, COL_GREY, "Press R to rescan.");
        return;
    }

    int row;
    for (row = 0; row < AP_VISIBLE && (scroll + row) < count; row++) {
        int idx = scroll + row;
        APEntry *ap = &aps[idx];
        int col = (idx == sel) ? COL_HILITE : COL_TEXT;

        char bar[12];
        rssi_bar(ap->rssi, bar, sizeof(bar));

        char line[34];
        snprintf(line, sizeof(line), "%-18.18s%s%s",
                 ap->ssid,
                 bar,
                 ap->encrypted ? " WEP" : " OPN");
        gfx_puts_top(0, 4 + row, col, line);
    }
    gfx_hline_top(14, COL_BORDER);
    gfx_puts_top(0, 15, COL_GREY, "B:Quit  R:Rescan  A:Connect");
}

// ── Bottom hint screen ────────────────────────────────────────────────────────

static void draw_hint_bot(const char *line1, const char *line2) {
    gfx_clear_bottom();
    gfx_puts_bot(0, 8,  COL_GREY, line1);
    gfx_puts_bot(0, 10, COL_GREY, line2);
}

// ── Main menu (top screen, connected state) ───────────────────────────────────

static void draw_main_menu(const char *ip_str) {
    show_header("Connected!");
    gfx_puts_top(0, 4, COL_OK,   ip_str);
    gfx_hline_top(5, COL_BORDER);
    gfx_puts_top(2, 7,  COL_TEXT,  "A  -  Download a file");
    gfx_puts_top(2, 9,  COL_TEXT,  "X  -  Disconnect / rescan");
    gfx_puts_top(2, 11, COL_TEXT,  "START - Quit");
    gfx_hline_top(13, COL_BORDER);
    draw_hint_bot("Touch URL bar or press A", "to start a download.");
}

// ── Entry point ───────────────────────────────────────────────────────────────

int main(void) {
    // --- Hardware init ---
    defaultExceptionHandler();

    // Init FAT (libfat) for SD / CF access
    if (!fatInitDefault()) {
        // Non-fatal: downloads won't save but we can still run
        // (user will see file errors later)
    }

    gfx_init();

    // Show splash
    show_header("DS-wget v1.0");
    gfx_puts_top(2, 5,  COL_TEXT, "HTTP downloader for DS Lite");
    gfx_puts_top(2, 7,  COL_TEXT, "WEP + Open networks");
    gfx_puts_top(2, 9,  COL_GREY, "Initialising WiFi...");
    draw_hint_bot("Starting up...", "Please wait.");
    swiDelay(60 * 0x1000);

    // --- State machine ---
    AppState state = ST_SCAN;
    APEntry  aps[MAX_APS];
    int      ap_count = 0;
    int      ap_sel   = 0;
    int      ap_scroll= 0;
    int      selected_ap_idx = -1;

    char     wep_key[32] = "";
    char     url_buf[384] = "http://";
    char     folder_buf[256] = "fat:/";

    while (1) {
        swiWaitForVBlank();
        scanKeys();
        u32 kd = keysDown();

        switch (state) {

        // ── SCAN ─────────────────────────────────────────────────────────────
        case ST_SCAN:
            show_header("Scanning for networks...");
            gfx_puts_top(2, 5, COL_GREY, "Please wait (~3 sec)");
            draw_hint_bot("Scanning WiFi...", "");
            swiWaitForVBlank();

            ap_count  = wifi_scan(aps);
            ap_sel    = 0;
            ap_scroll = 0;
            state     = ST_AP_LIST;
            break;

        // ── AP LIST ──────────────────────────────────────────────────────────
        case ST_AP_LIST:
            draw_ap_list(aps, ap_count, ap_sel, ap_scroll);
            draw_hint_bot("UP/DOWN: move  A: connect", "R: rescan  B: quit");

            if (kd & KEY_R) {
                state = ST_SCAN;
                break;
            }
            if (kd & KEY_B) goto quit;

            if (kd & KEY_DOWN) {
                if (ap_sel < ap_count - 1) {
                    ap_sel++;
                    if (ap_sel >= ap_scroll + AP_VISIBLE) ap_scroll++;
                }
            }
            if (kd & KEY_UP) {
                if (ap_sel > 0) {
                    ap_sel--;
                    if (ap_sel < ap_scroll) ap_scroll--;
                }
            }

            if ((kd & KEY_A) && ap_count > 0) {
                selected_ap_idx = ap_sel;
                if (aps[ap_sel].encrypted) {
                    // Need WEP key
                    wep_key[0] = '\0';
                    state = ST_WEP_KEY;
                } else {
                    state = ST_CONNECTING;
                }
            }
            break;

        // ── WEP KEY ──────────────────────────────────────────────────────────
        case ST_WEP_KEY: {
            char prompt[48];
            snprintf(prompt, sizeof(prompt), "WEP key for: %.20s",
                     aps[selected_ap_idx].ssid);
            int ok = kb_get_string(wep_key, sizeof(wep_key), prompt);
            if (!ok) {
                state = ST_AP_LIST;
            } else {
                state = ST_CONNECTING;
            }
            break;
        }

        // ── CONNECTING ───────────────────────────────────────────────────────
        case ST_CONNECTING: {
            APEntry *ap = &aps[selected_ap_idx];
            show_header("Connecting...");
            char cline[40];
            snprintf(cline, sizeof(cline), "SSID: %.28s", ap->ssid);
            gfx_puts_top(2, 5, COL_TEXT, cline);
            gfx_puts_top(2, 7, COL_GREY, "Associating...");
            draw_hint_bot("Connecting to network...", "Please wait.");
            swiWaitForVBlank();

            int ok;
            if (ap->encrypted) {
                ok = wifi_connect_wep(ap, wep_key);
            } else {
                ok = wifi_connect_open(ap);
            }

            if (ok) {
                state = ST_MAIN;
            } else {
                gfx_puts_top(2, 9, COL_ERROR, "Connection failed!");
                gfx_puts_top(2, 11, COL_GREY, "Press any key...");
                while (!keysDown()) { swiWaitForVBlank(); scanKeys(); }
                state = ST_AP_LIST;
            }
            break;
        }

        // ── MAIN MENU ────────────────────────────────────────────────────────
        case ST_MAIN: {
            // Get IP string
            struct in_addr ip;
            ip.s_addr = Wifi_GetIP();
            char ip_str[40];
            snprintf(ip_str, sizeof(ip_str), "IP: %s", inet_ntoa(ip));
            draw_main_menu(ip_str);

            if (kd & KEY_START) goto quit;

            if (kd & KEY_X) {
                wifi_disconnect();
                state = ST_SCAN;
            }

            if (kd & KEY_A) {
                // Start URL input
                strncpy(url_buf, "http://", sizeof(url_buf));
                state = ST_URL_INPUT;
            }
            break;
        }

        // ── URL INPUT ────────────────────────────────────────────────────────
        case ST_URL_INPUT: {
            show_header("Enter URL");
            int ok = kb_get_string(url_buf, sizeof(url_buf), "Enter URL:");
            if (!ok) {
                state = ST_MAIN;
            } else {
                // Move to folder picker
                strncpy(folder_buf, "fat:/", sizeof(folder_buf));
                state = ST_FOLDER;
            }
            break;
        }

        // ── FOLDER PICKER ────────────────────────────────────────────────────
        case ST_FOLDER: {
            int ok = folder_pick(folder_buf, sizeof(folder_buf));
            if (!ok) {
                state = ST_MAIN;
            } else {
                state = ST_DOWNLOADING;
            }
            break;
        }

        // ── DOWNLOADING ──────────────────────────────────────────────────────
        case ST_DOWNLOADING: {
            dl_download(url_buf, folder_buf);
            // dl_download shows its own wait-for-key
            state = ST_MAIN;
            break;
        }

        // ── ERROR ────────────────────────────────────────────────────────────
        case ST_ERROR:
            gfx_puts_top(2, 10, COL_GREY, "Press START to quit.");
            if (kd & KEY_START) goto quit;
            break;

        } // switch
    } // while

quit:
    wifi_disconnect();
    return 0;
}
