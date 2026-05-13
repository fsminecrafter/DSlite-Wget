// folder_picker.c – navigate FAT directories on bottom screen
// A = enter directory, B = go up, START = select this folder, SELECT = cancel

#include <nds.h>
#include <fat.h>
#include <dirent.h>
#include <string.h>
#include <stdio.h>
#include "folder_picker.h"
#include "gfx.h"

#define MAX_ENTRIES  64
#define VISIBLE_ROWS 14   // how many entries fit on screen at once

typedef struct {
    char name[256];
    int  is_dir;
} DirEntry;

static DirEntry s_entries[MAX_ENTRIES];
static int      s_count = 0;

// Fill s_entries from path, directories first, no files shown (folders only)
static void load_dir(const char *path) {
    s_count = 0;
    DIR *d = opendir(path);
    if (!d) return;

    struct dirent *ent;
    while ((ent = readdir(d)) != NULL && s_count < MAX_ENTRIES) {
        // Skip hidden entries and "." but keep ".."
        if (ent->d_name[0] == '.' && strcmp(ent->d_name, "..") != 0) continue;
        if (ent->d_type != DT_DIR) continue;  // folders only

        strncpy(s_entries[s_count].name, ent->d_name, 255);
        s_entries[s_count].name[255] = '\0';
        s_entries[s_count].is_dir    = 1;
        s_count++;
    }
    closedir(d);
}

static void draw_picker(const char *cur_path, int sel, int scroll) {
    gfx_clear_bottom();
    gfx_puts_bot(0, 0, COL_TITLE, "Select Folder");
    gfx_hline_bot(1, COL_BORDER);

    // Current path (truncated to fit)
    char pathbuf[33];
    int plen = strlen(cur_path);
    if (plen > 32) {
        // Show tail
        snprintf(pathbuf, sizeof(pathbuf), "~%s", cur_path + plen - 31);
    } else {
        strncpy(pathbuf, cur_path, 32);
        pathbuf[32] = '\0';
    }
    gfx_puts_bot(0, 2, COL_GREY, pathbuf);
    gfx_hline_bot(3, COL_BORDER);

    // Entry list
    int row;
    for (row = 0; row < VISIBLE_ROWS && (scroll + row) < s_count; row++) {
        int idx = scroll + row;
        int col = (idx == sel) ? COL_HILITE : COL_TEXT;

        // Prefix: "> " for selected, "  " otherwise; "/" suffix for dirs
        char line[34];
        const char *prefix = (idx == sel) ? "> " : "  ";
        snprintf(line, sizeof(line), "%s%.28s/", prefix, s_entries[idx].name);
        gfx_puts_bot(0, 4 + row, col, line);
    }

    // If directory is empty
    if (s_count == 0) {
        gfx_puts_bot(2, 4, COL_GREY, "(empty)");
    }

    gfx_hline_bot(18, COL_BORDER);
    gfx_puts_bot(0, 19, COL_GREY, "A:Enter B:Up ST:Select SL:Cancel");
}

int folder_pick(char *out_path, int maxlen) {
    // Start at SD card root
    char cur_path[512];
    strncpy(cur_path, "fat:/", sizeof(cur_path));
    cur_path[sizeof(cur_path) - 1] = '\0';

    int sel    = 0;
    int scroll = 0;

    load_dir(cur_path);

    while (1) {
        draw_picker(cur_path, sel, scroll);
        swiWaitForVBlank();

        scanKeys();
        u32 keys = keysDown();

        if (keys & KEY_SELECT) return 0;  // cancel

        if (keys & KEY_START) {
            // Select current directory
            strncpy(out_path, cur_path, maxlen - 1);
            out_path[maxlen - 1] = '\0';
            return 1;
        }

        if (keys & KEY_DOWN) {
            if (sel < s_count - 1) {
                sel++;
                if (sel >= scroll + VISIBLE_ROWS) scroll++;
            }
        }

        if (keys & KEY_UP) {
            if (sel > 0) {
                sel--;
                if (sel < scroll) scroll--;
            }
        }

        if (keys & KEY_A) {
            if (s_count == 0) continue;

            const char *chosen = s_entries[sel].name;

            if (strcmp(chosen, "..") == 0) {
                // Go up: strip last path component
                char *slash = strrchr(cur_path, '/');
                if (slash && slash != cur_path) {
                    // Check if we'd go above "fat:/"
                    if (slash - cur_path <= 5) {
                        // Already at root level, stay
                    } else {
                        *slash = '\0';
                    }
                }
            } else {
                // Enter subdirectory
                int cur_len = strlen(cur_path);
                // Append /name if not already ending in /
                if (cur_path[cur_len - 1] != '/') {
                    strncat(cur_path, "/", sizeof(cur_path) - cur_len - 1);
                }
                strncat(cur_path, chosen, sizeof(cur_path) - strlen(cur_path) - 1);
            }

            load_dir(cur_path);
            sel    = 0;
            scroll = 0;
        }

        if (keys & KEY_B) {
            // Same as ".." — go up one level
            char *slash = strrchr(cur_path, '/');
            if (slash && (slash - cur_path) > 5) {
                *slash = '\0';
            } else {
                strncpy(cur_path, "fat:/", sizeof(cur_path));
            }
            load_dir(cur_path);
            sel    = 0;
            scroll = 0;
        }
    }
}
