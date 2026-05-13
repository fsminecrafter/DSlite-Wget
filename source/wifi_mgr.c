// wifi_mgr.c – network scanning and connection via dswifi
#include <nds.h>
#include <dswifi9.h>
#include <netinet/in.h>
#include <string.h>
#include "wifi_mgr.h"
#include "gfx.h"

// dswifi needs timer IRQ + VBlank IRQ to run its internal state machine.
// The user must call Wifi_Update() from VBlank; we hook it here.

static volatile int s_connected = 0;

static void vblank_handler(void) {
    Wifi_Update();
}

static void wifi_connect_cb(int status) {
    switch (status) {
        case ASSOCSTATUS_ASSOCIATED:
            s_connected = 1;
            break;
        case ASSOCSTATUS_CANNOTCONNECT:
        case ASSOCSTATUS_DISCONNECTED:
            s_connected = -1;
            break;
        default:
            break;
    }
}

// Init ARM7 wifi sync once (idempotent due to static flag)
static int s_wifi_inited = 0;
static void ensure_wifi_init(void) {
    if (s_wifi_inited) return;
    irqSet(IRQ_VBLANK, vblank_handler);
    irqEnable(IRQ_VBLANK);
    Wifi_InitDefault(INIT_ONLY); // don't autoconnect
    s_wifi_inited = 1;
}

int wifi_scan(APEntry *ap_list) {
    ensure_wifi_init();

    // Ask dswifi to start scanning
    Wifi_ScanMode();

    // Wait ~3 seconds for beacons (180 frames at 60fps)
    int frame;
    for (frame = 0; frame < 180; frame++) {
        swiWaitForVBlank();
    }

    int raw_count = Wifi_GetNumAP();
    if (raw_count > MAX_APS) raw_count = MAX_APS;

    int count = 0;
    int i;
    for (i = 0; i < raw_count; i++) {
        Wifi_AccessPoint ap;
        Wifi_GetAPData(i, &ap);

        // Skip APs with empty SSID
        if (ap.ssid_len == 0) continue;

        APEntry *e = &ap_list[count];
        memcpy(e->raw, &ap, sizeof(Wifi_AccessPoint));

        // Copy SSID safely
        int slen = ap.ssid_len;
        if (slen > 32) slen = 32;
        memcpy(e->ssid, ap.ssid, slen);
        e->ssid[slen] = '\0';

        // RSSI: dswifi stores it as a raw byte; convert to 0-100 range.
        // Typical values: 0=no signal, 255=full. We normalise linearly.
        e->rssi = (ap.rssi * 100) / 255;

        // Check WEP cap bit (bit 4 of ap.flags according to dswifi source)
        e->encrypted = (ap.flags & WFLAG_AP_WEP) ? 1 : 0;

        count++;
    }
    return count;
}

// Shared connection routine
static int do_connect(Wifi_AccessPoint *ap, int wep, const char *key) {
    ensure_wifi_init();
    s_connected = 0;

    Wifi_ConnectAP(ap, wep ? WEPMODE_128BIT : WEPMODE_NONE,
                   0,  // key index
                   wep ? (u8 *)key : NULL);

    Wifi_SetIP(0, 0, 0, 0, 0); // request DHCP

    // Register status callback
    Wifi_AssociationStatus = wifi_connect_cb; // direct function pointer set

    // Wait up to 10 seconds
    int ticks = 0;
    while (s_connected == 0 && ticks < 600) {
        swiWaitForVBlank();
        ticks++;
    }
    return (s_connected == 1) ? 1 : 0;
}

int wifi_connect_open(const APEntry *ap) {
    return do_connect((Wifi_AccessPoint *)&ap->raw, 0, NULL);
}

int wifi_connect_wep(const APEntry *ap, const char *key) {
    return do_connect((Wifi_AccessPoint *)&ap->raw, 1, key);
}

void wifi_disconnect(void) {
    Wifi_DisconnectAP();
    s_connected = 0;
}

int wifi_is_connected(void) {
    return s_connected == 1;
}
