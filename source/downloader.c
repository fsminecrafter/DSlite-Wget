// downloader.c – HTTP/1.0 file downloader using raw BSD sockets (dswifi)
// Supports: GET request, reads Content-Type for extension fallback,
//           streams body to FAT file with a live progress bar.

#include <nds.h>
#include <dswifi9.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "downloader.h"
#include "gfx.h"

#define RECV_BUF  1024   // bytes per recv() call – kept small for DS heap

// ── URL parsing ───────────────────────────────────────────────────────────────

typedef struct {
    char host[128];
    int  port;
    char path[384];   // includes leading '/'
} ParsedURL;

// Returns 1 on success
static int parse_url(const char *url, ParsedURL *out) {
    // Must start with "http://"
    if (strncmp(url, "http://", 7) != 0) return 0;
    const char *rest = url + 7;

    // Find path (first '/' after host[:port])
    const char *slash = strchr(rest, '/');
    size_t host_len;
    if (slash) {
        host_len = slash - rest;
        strncpy(out->path, slash, sizeof(out->path) - 1);
        out->path[sizeof(out->path) - 1] = '\0';
    } else {
        host_len = strlen(rest);
        strncpy(out->path, "/", sizeof(out->path) - 1);
    }

    // Separate host and optional port
    char host_port[132];
    if (host_len >= sizeof(host_port)) host_len = sizeof(host_port) - 1;
    strncpy(host_port, rest, host_len);
    host_port[host_len] = '\0';

    char *colon = strchr(host_port, ':');
    if (colon) {
        *colon = '\0';
        out->port = atoi(colon + 1);
    } else {
        out->port = 80;
    }
    strncpy(out->host, host_port, sizeof(out->host) - 1);
    out->host[sizeof(out->host) - 1] = '\0';
    return 1;
}

// ── Extension from Content-Type ───────────────────────────────────────────────

static void ext_from_content_type(const char *ct, char *ext, int ext_len) {
    // Strip parameters e.g. "text/html; charset=utf-8"
    char base[64];
    strncpy(base, ct, sizeof(base) - 1);
    base[sizeof(base) - 1] = '\0';
    char *semi = strchr(base, ';');
    if (semi) *semi = '\0';

    // Trim trailing whitespace
    int i = strlen(base) - 1;
    while (i >= 0 && (base[i] == ' ' || base[i] == '\t')) base[i--] = '\0';

    // Map MIME → extension
    struct { const char *mime; const char *ext; } map[] = {
        { "text/plain",             "txt"  },
        { "text/html",              "html" },
        { "text/css",               "css"  },
        { "text/csv",               "csv"  },
        { "text/xml",               "xml"  },
        { "application/json",       "json" },
        { "application/xml",        "xml"  },
        { "application/zip",        "zip"  },
        { "application/octet-stream","bin" },
        { "image/png",              "png"  },
        { "image/jpeg",             "jpg"  },
        { "image/gif",              "gif"  },
        { "image/bmp",              "bmp"  },
        { "audio/mpeg",             "mp3"  },
        { "audio/wav",              "wav"  },
        { "video/mp4",              "mp4"  },
        { NULL, NULL }
    };

    int j;
    for (j = 0; map[j].mime; j++) {
        if (strcasecmp(base, map[j].mime) == 0) {
            strncpy(ext, map[j].ext, ext_len - 1);
            ext[ext_len - 1] = '\0';
            return;
        }
    }
    // Fallback
    strncpy(ext, "txt", ext_len - 1);
    ext[ext_len - 1] = '\0';
}

// ── Filename from URL path ────────────────────────────────────────────────────

// Writes just the basename (last segment) into out.
static void basename_from_path(const char *path, char *out, int outlen) {
    const char *last = strrchr(path, '/');
    const char *base = last ? last + 1 : path;
    if (*base == '\0') base = "index";
    strncpy(out, base, outlen - 1);
    out[outlen - 1] = '\0';
}

// ── HTTP GET ──────────────────────────────────────────────────────────────────

int dl_download(const char *url, const char *dest_dir) {
    ParsedURL pu;

    // --- Status display setup ---
    gfx_clear_top();
    gfx_puts_top(0, 0, COL_TITLE, "DS-wget  Downloading");
    gfx_hline_top(1, COL_BORDER);

    char msg[64];
    snprintf(msg, sizeof(msg), "URL: %.54s", url);
    gfx_puts_top(0, 2, COL_TEXT, msg);

    if (!parse_url(url, &pu)) {
        gfx_puts_top(0, 4, COL_ERROR, "ERROR: Only http:// supported");
        swiDelay(120 * 0x1000);
        return 0;
    }

    snprintf(msg, sizeof(msg), "Host: %s:%d", pu.host, pu.port);
    gfx_puts_top(0, 4, COL_TEXT, msg);
    gfx_puts_top(0, 5, COL_GREY, "Resolving...");

    // --- DNS ---
    struct hostent *he = gethostbyname(pu.host);
    if (!he) {
        gfx_puts_top(0, 5, COL_ERROR, "ERROR: DNS failed");
        swiDelay(120 * 0x1000);
        return 0;
    }

    gfx_puts_top(0, 5, COL_GREY, "Connecting...");

    // --- Connect ---
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        gfx_puts_top(0, 5, COL_ERROR, "ERROR: socket() failed");
        swiDelay(120 * 0x1000);
        return 0;
    }

    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port   = htons(pu.port);
    memcpy(&sa.sin_addr, he->h_addr_list[0], he->h_length);

    if (connect(sock, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
        gfx_puts_top(0, 5, COL_ERROR, "ERROR: connect() failed");
        closesocket(sock);
        swiDelay(120 * 0x1000);
        return 0;
    }

    gfx_puts_top(0, 5, COL_GREY, "Sending request...");

    // --- Send HTTP/1.0 GET ---
    char req[640];
    snprintf(req, sizeof(req),
        "GET %s HTTP/1.0\r\n"
        "Host: %s\r\n"
        "User-Agent: DS-wget/1.0\r\n"
        "Connection: close\r\n"
        "\r\n",
        pu.path, pu.host);

    send(sock, req, strlen(req), 0);

    // --- Read response headers ---
    // We read byte-by-byte until we see \r\n\r\n (simple but sufficient)
    char header_buf[2048];
    int  hlen = 0;
    int  header_done = 0;

    while (!header_done && hlen < (int)sizeof(header_buf) - 1) {
        char c;
        int r = recv(sock, &c, 1, 0);
        if (r <= 0) break;
        header_buf[hlen++] = c;
        if (hlen >= 4 &&
            header_buf[hlen-4] == '\r' && header_buf[hlen-3] == '\n' &&
            header_buf[hlen-2] == '\r' && header_buf[hlen-1] == '\n') {
            header_done = 1;
        }
    }
    header_buf[hlen] = '\0';

    // Parse status line
    int http_status = 0;
    sscanf(header_buf, "HTTP/%*s %d", &http_status);
    if (http_status != 200) {
        snprintf(msg, sizeof(msg), "ERROR: HTTP %d", http_status);
        gfx_puts_top(0, 5, COL_ERROR, msg);
        closesocket(sock);
        swiDelay(180 * 0x1000);
        return 0;
    }

    // Extract Content-Length
    long content_length = -1;
    char *cl_hdr = strcasestr(header_buf, "Content-Length:");
    if (cl_hdr) {
        content_length = atol(cl_hdr + 15);
    }

    // Extract Content-Type
    char content_type[64] = "application/octet-stream";
    char *ct_hdr = strcasestr(header_buf, "Content-Type:");
    if (ct_hdr) {
        sscanf(ct_hdr + 13, " %63[^\r\n]", content_type);
    }

    // Derive filename
    char basename[64];
    basename_from_path(pu.path, basename, sizeof(basename));

    // If basename has no extension, append one from Content-Type
    char *dot = strrchr(basename, '.');
    char ext[8] = "";
    if (!dot) {
        ext_from_content_type(content_type, ext, sizeof(ext));
    }

    // Build destination path
    char dest_path[512];
    if (ext[0]) {
        snprintf(dest_path, sizeof(dest_path), "%s/%s.%s", dest_dir, basename, ext);
    } else {
        snprintf(dest_path, sizeof(dest_path), "%s/%s", dest_dir, basename);
    }

    snprintf(msg, sizeof(msg), "File: %.54s", dest_path);
    gfx_puts_top(0, 6, COL_TEXT, msg);

    if (content_length > 0) {
        snprintf(msg, sizeof(msg), "Size: %ld bytes", content_length);
        gfx_puts_top(0, 7, COL_TEXT, msg);
    } else {
        gfx_puts_top(0, 7, COL_GREY, "Size: unknown");
    }

    // --- Open output file ---
    FILE *fp = fopen(dest_path, "wb");
    if (!fp) {
        gfx_puts_top(0, 8, COL_ERROR, "ERROR: Cannot create file");
        closesocket(sock);
        swiDelay(120 * 0x1000);
        return 0;
    }

    gfx_puts_top(0, 8, COL_OK, "Downloading...");

    // Bottom screen: progress
    gfx_clear_bottom();
    gfx_puts_bot(0, 0, COL_TITLE, "Progress");
    gfx_hline_bot(1, COL_BORDER);

    // --- Stream body ---
    char recv_buf[RECV_BUF];
    long bytes_received = 0;
    int  n;

    while ((n = recv(sock, recv_buf, RECV_BUF, 0)) > 0) {
        fwrite(recv_buf, 1, n, fp);
        bytes_received += n;

        // Update progress bar
        int pct = 0;
        if (content_length > 0) {
            pct = (int)((bytes_received * 100L) / content_length);
            if (pct > 100) pct = 100;
        } else {
            // Spin: cycle 0-99
            pct = (int)(bytes_received / 512) % 100;
        }

        char prog_msg[40];
        snprintf(prog_msg, sizeof(prog_msg), "%ld bytes", bytes_received);
        gfx_puts_bot(0, 3, COL_TEXT, prog_msg);
        gfx_progress_bar(5, pct, COL_OK);

        swiWaitForVBlank();
    }

    fclose(fp);
    closesocket(sock);

    // Final status
    gfx_progress_bar(5, 100, COL_OK);
    snprintf(msg, sizeof(msg), "Done! %ld bytes saved.", bytes_received);
    gfx_puts_top(0, 9, COL_OK, msg);
    gfx_puts_top(0, 11, COL_GREY, "Press any button to continue");

    // Wait for key press
    while (1) {
        swiWaitForVBlank();
        scanKeys();
        if (keysDown()) break;
    }

    return 1;
}
