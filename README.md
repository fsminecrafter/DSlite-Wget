# DS-wget

A wget-like HTTP file downloader for the Nintendo DS Lite, written in C
using libnds and dswifi. Scan for networks, connect (open or WEP), type
a URL on the touchscreen keyboard, pick a save folder, and download.

---

## Features

- **Network scanner** — scans for 802.11b access points, shows SSID,
  signal-strength bar, and open/WEP indicator
- **WEP key entry** — touchscreen QWERTY keyboard with Shift toggle
- **HTTP/1.0 GET** — works over plain HTTP (no HTTPS; the DS has no TLS)
- **Content-Type sniffing** — picks file extension from server header;
  falls back to `.txt` if the server doesn't say
- **Live progress bar** — shows bytes received and a fill bar;
  unknown-length downloads show a spinner-style counter
- **FAT folder picker** — browse SD/CF directories; A=enter, B=up,
  START=select, SELECT=cancel
- **Two-screen UI** — top screen for status/lists, bottom for input

### Controls

| Screen | Button | Action |
|--------|--------|--------|
| AP list | UP/DOWN | move cursor |
| AP list | A | connect to selected network |
| AP list | R | re-scan |
| AP list | B | quit |
| Keyboard | stylus | tap key |
| Keyboard | SHIFT key | toggle case |
| Keyboard | OK / START | confirm |
| Keyboard | X / B | cancel |
| Folder picker | UP/DOWN | move cursor |
| Folder picker | A | enter directory |
| Folder picker | B | go up one level |
| Folder picker | START | select this folder |
| Folder picker | SELECT | cancel |
| Main menu | A | start download |
| Main menu | X | disconnect / rescan |
| Main menu | START | quit |

---

## Hardware limitations

The DS Lite's Marvell 88W8686 Wi-Fi chip supports:
- **802.11b only** (2.4 GHz, up to 11 Mbps raw)
- **Open networks** (no password)
- **WEP** (64-bit or 128-bit key)

It does **not** support WPA, WPA2, WPA3, or 5 GHz.
It does **not** have a TLS stack, so HTTPS URLs will fail.

To test locally, set up an HTTP server on a WEP or open access point.

---

## Building

### 1. Install devkitPro

Follow the official guide for your OS:
https://devkitpro.org/wiki/Getting_Started

On Linux/macOS (after installing the pacman-based devkitPro manager):

```bash
sudo dkp-pacman -S nds-dev
```

This installs devkitARM, libnds, libfat, and dswifi.

### 2. Set environment variables

```bash
export DEVKITPRO=/opt/devkitpro
export DEVKITARM=/opt/devkitpro/devkitARM
source /etc/profile.d/devkit-env.sh   # or add the above to ~/.bashrc
```

### 3. Build

```bash
cd dslite-wget
make
```

Output: `dswget.nds`

### 4. Run

Copy `dswget.nds` to the root of your flashcart SD card.
Launch it from your flashcart menu (e.g. TWiLight Menu++, Wood R4, etc.).

Make sure your SD card is formatted FAT32 and that libfat can mount it
(most modern flashcarts support this automatically).

---

## Project structure

```
dslite-wget/
├── Makefile
├── README.md
├── include/
│   ├── gfx.h            Two-screen text renderer
│   ├── wifi_mgr.h       Network scan + connect
│   ├── keyboard.h       Touchscreen QWERTY keyboard
│   ├── folder_picker.h  FAT directory browser
│   └── downloader.h     HTTP GET downloader
└── source/
    ├── main.c           App state machine
    ├── gfx.c            libnds console renderer
    ├── wifi_mgr.c       dswifi wrapper
    ├── keyboard.c       Touchscreen keyboard
    ├── folder_picker.c  FAT directory browser
    └── downloader.c     HTTP socket downloader
```

---

## Testing without a WEP router

Most modern routers dropped WEP support. Options:

- **Old router** — dig out a router from ~2005 and set it to WEP
- **Open hotspot** — any unprotected hotspot works (DS connects fine)
- **Linux soft-AP** — `hostapd` with `wep_key0` set, or `auth_algs=1`
  for open:

```ini
# /etc/hostapd/hostapd-ds.conf
interface=wlan0
driver=nl80211
ssid=DS-TEST
hw_mode=b          # 802.11b required!
channel=6
auth_algs=1        # open auth
wep_default_key=0
wep_key0=1234567890  # 40-bit WEP (5 ASCII chars) or leave blank for open
```

Run: `sudo hostapd /etc/hostapd/hostapd-ds.conf`

Serve files: `python3 -m http.server 80` in any directory.

Then on the DS enter: `http://192.168.X.X/filename.txt`

---

## Known limitations

- No redirect following (HTTP 301/302 responses will show an error)
- No chunked transfer-encoding support
- Max URL length: 383 chars
- Max filename: 63 chars
- Max simultaneous entries in folder view: 64
- Download buffer: 1 KB per recv() call (safe on DS heap; slow on large files)
- No resume support

---

## Licence

MIT — do whatever you like with it.
