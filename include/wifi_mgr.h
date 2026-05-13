#pragma once
#include <nds.h>
#include <dswifi9.h>

#define MAX_APS  32

typedef struct {
    char ssid[33];          // null-terminated SSID
    Wifi_AccessPoint raw;   // full AP struct from dswifi
    int  rssi;              // signal strength 0-100
    int  encrypted;         // 1 = WEP, 0 = open
} APEntry;

// Populate ap_list, return count found (≤ MAX_APS)
int  wifi_scan(APEntry *ap_list);

// Connect to open network; returns 1 on success
int  wifi_connect_open(const APEntry *ap);

// Connect to WEP network with given key string; returns 1 on success
int  wifi_connect_wep(const APEntry *ap, const char *key);

// Disconnect & shut down
void wifi_disconnect(void);

// Already connected?
int  wifi_is_connected(void);
