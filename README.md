# nowa154-esl

Custom BLE firmware and tools for the **Hanshow Nowa-154R-N** e-paper shelf label
(Telink TLSR8359, 1.54" black/white/red panel, 152x152 pixels, NFC chip).

It is a trimmed fork of [atc1441/ATC_TLSR_Paper](https://github.com/atc1441/ATC_TLSR_Paper)
with the 152x152 panel driver, red-plane support and BLE image upload ported from
[wang551/nowa154-etag](https://github.com/wang551/nowa154-etag). The NFC experiments
from that repo are **not** included.

## The card

A 1.54" e-ink tag turned into a live pixel-art name card. The scene, speech bubble and
decorations change by time of day, HP hearts drain through the work day, EXP fills one block
per weekday, and LV is the Intel-style work week. Previews below are rendered from the real
firmware drawing code (the tag itself shows black, white and red only).

| MONIN (05:00-11:59) | NOON (12:00-15:59) | EVENIN (16:00-20:59) | NITE (21:00-04:59) |
|---|---|---|---|
| ![MONIN](docs/img/me_monin.png) | ![NOON](docs/img/me_noon.png) | ![EVENIN](docs/img/me_evenin.png) | ![NITE](docs/img/me_nite.png) |

- **HP**: three hearts that drain from the top, half a heart at a time, across the work window (default 08:00-17:00)
- **EXP**: seven blocks, Monday fills one, Sunday fills all seven
- **LV**: work week number (weeks start Sunday, WW01 is the week containing 1 January)
- **Portraits**: replace the face of each scene separately from the web page (four uploaders, stored in the tag's flash); the rest of the artwork stays
- **Themes**: the web page ships Clean (default), Terminal and Windows 98

The earlier Bulbasaur artwork is still in the repo (`CARD_ART_BULBASAUR` in `Firmware/src/card_bg.h`):

![Bulbasaur card scenes](docs/img/bulbasaur_scenes.png)

## Status

| File in `bin/` | What it is | Tested on a real tag |
|---|---|---|
| `ATC_Paper_nowa154_v9_verified.bin` | Clock screen only, black/white, no image upload | **Yes**: text, clock, temperature and battery drawn correctly on a Nowa-154R-N |
| `ATC_Paper_nowa154_merged.bin` | This repo's current source: dashboard clock, BLE image upload with red plane, hold/resume, re-advertise fix | **Yes**: dashboard clock, and image upload (test pattern incl. red triangle and red bar) via `web_tools/nowa152_image_upload.html`. hold/restore clock and time sync also work. OTA and the re-advertise fix are not individually confirmed |
| `ATC_Paper_nowa154_bulbasaur_card.bin` | **BULBASAUR CARD**: the earlier creature artwork with the same live date/time, editable name/title, HP hearts that drain from the top in half-heart steps over a work window, 7 weekday EXP blocks (Mon = 1 ... Sun = 7), a time-of-day scene with a speech bubble (MONIN / NOON / EVENIN / NITE), BATT, LV = Intel-style work week (Sunday start, WW01 contains 1 January), card redraw interval 1-30 min, status LED on/off, and red-safe partial refresh. Build it from source by uncommenting `CARD_ART_BULBASAUR` in `Firmware/src/card_bg.h` | **Not yet** (the earlier version without the red fix was verified; the weekday EXP, half-heart HP and greeting scenes are new) |
| `ATC_Paper_nowa154_me_card.bin` | **ME CARD** firmware (portrait artwork): fixed artwork in flash, date and 12-hour time redrawn each minute, name, title and HP/EXP/BATT editable from the web page (BLE commands 0x07-0x0A, stored in flash at 0x50000), hearts drain from the top in half steps, 7 weekday EXP blocks, LV = work week, four per-scene portrait uploads (MONIN / NOON / EVENIN / NITE, stored in flash at 0x51000-0x54000; BLE 0x0B-0x0D), and a MONIN / NOON / EVENIN / NITE scene that changes at 05:00, 12:00, 16:00 and 21:00 (scene art and red hearts change with the hourly full refresh) (partial refresh, full refresh each hour; the partial waveform no longer drives red pixels, so red keeps its colour). Source: `Firmware/src/card_bg.h`, `epd.c` (`CARD_MODE`) | Display and time sync **yes** (card, live time). Editable name/title **not yet** |

If the merged build misbehaves, flash the v9 file to get back to a working screen.

## What the panel really is

The glass is **152x152**, not 200x200 (the Stellar-S3TN@ settings do not fit) and not
the 2.13" layout the original auto-detect picks. The controller has 296 gate lines;
the 152 lines of this glass sit in the back half. Findings used by the driver:

- RAM Y window 295 down to 144, data entry mode `0x01`
- RAM X window 20 bytes per line (152 pixels plus an 8-pixel source offset)
- the stream is 152 lines x 20 bytes = 3040 bytes; line 0 lands on the **right** edge
  of the glass and bytes run top to bottom, most significant bit first
- the first 8 source rows are hidden, so drawing happens on a 152x160 canvas shifted down by 8

Details: [docs/screen_adaptation.md](docs/screen_adaptation.md),
[docs/BRINGUP.md](docs/BRINGUP.md).

## Changes compared with upstream ATC_TLSR_Paper

- 1.54" 152x152 BWR driver, forced as the display model (auto-detect cannot tell it from the 2.13")
- red plane support (`0x26` RAM) and a hold flag so a BLE image is not overwritten by the clock
- BLE commands `0x05` (hold/resume clock) and `0x06` (select write plane); see [docs/ble_protocol.md](docs/ble_protocol.md)
- tag keeps advertising after a BLE disconnect (upstream never restarted advertising)
- temperature is signed (negative values were wrong)
- text drawing clips at negative x instead of refusing
- dashboard layout for the square screen

## Flashing

1. Wire a USB-UART adapter: adapter TX to the tag's SWS pad, GND to GND, and power the tag
   (3 V coin cell). Photo: [docs/USB_UART_Flashing_connection.jpg](docs/USB_UART_Flashing_connection.jpg).
2. Open the [web flasher](https://atc1441.github.io/ATC_TLSR_Paper_UART_Flasher.html) in Chrome.
3. Click **Unlock** once, then write a file from `bin/`.
4. Unplug the UART wires afterwards. A connected TX line blocks BLE.

## Web tool (`web_tools/nowa152_image_upload.html`)

One HTML file, no install. Open it in Chrome or Edge (Web Bluetooth). If BLE is blocked on
`file://`, run `python -m http.server` in this folder and open `http://localhost:8000`.
Themes: Clean (default), Terminal, Windows 98. Layout is two columns with a collapsed UART flasher panel.

**Connection**
- Connect device / Reconnect / Disconnect, with a live connection status
- Sync time to device (sets the tag's clock; the card shows date and 12-hour time)

**Image upload**
- Choose any image, or Generate test pattern (check orientation first)
- Black / white / red conversion with Floyd-Steinberg, Bayer 4x4 or fixed threshold, adjustable threshold
- Fit mode (cover or contain), rotate 90 degrees, flip horizontal / vertical, invert; the flips fix a
  mirrored or rotated panel without reflashing
- Send to screen with a progress bar (black plane, then red plane)

**Name card**
- Generate card now: the page draws the exact card the firmware would draw, for any scene and time (scene preview selector: auto, MONIN, NOON, EVENIN, NITE)
- Save name + title to tag (bottom banner text), Back to artwork text to restore the printed banner
- HP: auto (drains across the work window, default 08:00-17:00, start and end editable) or fixed 3 / 2 / 1 / 0 hearts
- EXP: auto weekday (Monday 1 block ... Sunday 7 blocks) or manual 0-7 blocks
- BATT: measured or fixed percentage
- LV: Intel-style work week (weeks start Sunday, WW01 contains 1 January)
- Redraw interval 1-30 minutes and status LED on/off
- Save HP / EXP / BATT to tag sends all of the above in one frame

**Portrait per time of day**
- Four uploaders: MONIN (05:00-11:59), NOON (12:00-15:59), EVENIN (16:00-20:59), NITE (21:00-04:59)
- Each one replaces only the middle face of that scene (53x78 pixels, black/white/red); the rest of the artwork stays
- Pick, Send and Reset per scene; the portrait is kept in the tag's flash and survives power loss
- The tag redraws fully after a change (about 15 seconds); red only appears on a full refresh

**UART flasher**
- Load flasher here embeds the atc1441 UART flasher so firmware can be written from the same page
- The BLE OTA update was removed from the page (a failed OTA write can only be recovered over UART); its protocol is still described in `docs/ble_protocol.md`

After flashing, press **Save name + title to tag** again, because flashing wipes the saved record.

## Building

The compiler toolchains are not stored in this repo (about 180 MB). Copy the `tc32_linux`
and/or `tc32_windows` folders from the upstream
[ATC_TLSR_Paper](https://github.com/atc1441/ATC_TLSR_Paper) `Firmware` folder into
`Firmware/`, install Python, then:

- Windows: run `makeit.exe` in `Firmware/`
- Linux: run `make` in `Firmware/`

`make` does not track header changes. After editing a `.h` file, delete `Firmware/out`
and `Firmware/ATC_Paper.bin` before rebuilding.

The build appends a 4-byte CRC. The flasher images in `bin/` have it removed
(the file length matches the size field at offset `0x18`), the same layout as upstream's files.

## Credits and licenses

See [NOTICE.md](NOTICE.md). This repo is MIT licensed; see `LICENSE`.
