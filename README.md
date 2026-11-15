# nowa154-esl

Custom BLE firmware and tools for the **Hanshow Nowa-154R-N** e-paper shelf label
(Telink TLSR8359, 1.54" black/white/red panel, 152x152 pixels, NFC chip).

It is a trimmed fork of [atc1441/ATC_TLSR_Paper](https://github.com/atc1441/ATC_TLSR_Paper)
with the 152x152 panel driver, red-plane support and BLE image upload ported from
[wang551/nowa154-etag](https://github.com/wang551/nowa154-etag). The NFC experiments
from that repo are **not** included.

## Status

| File in `bin/` | What it is | Tested on a real tag |
|---|---|---|
| `ATC_Paper_nowa154_v9_verified.bin` | Clock screen only, black/white, no image upload | **Yes**: text, clock, temperature and battery drawn correctly on a Nowa-154R-N |
| `ATC_Paper_nowa154_merged.bin` | This repo's current source: dashboard clock, BLE image upload with red plane, hold/resume, re-advertise fix | **Yes**: dashboard clock, and image upload (test pattern incl. red triangle and red bar) via `web_tools/nowa152_image_upload.html`. hold/restore clock and time sync also work. OTA and the re-advertise fix are not individually confirmed |
| `ATC_Paper_nowa154_bulbasaur_card.bin` | **BULBASAUR CARD**: the earlier creature artwork with the same live date/time, editable name/title, HP/EXP/BATT settings (EXP start and end time adjustable, LV counting hours from a start time, card redraw interval 1-30 min, status LED on/off) and red-safe partial refresh. Build it from source by uncommenting `CARD_ART_BULBASAUR` in `Firmware/src/card_bg.h` | **Not yet** (the earlier version without the red fix was verified) |
| `ATC_Paper_nowa154_me_card.bin` | **ME CARD** firmware (portrait artwork): fixed artwork in flash, date and 12-hour time redrawn each minute, name, title and HP/EXP/BATT editable from the web page (BLE commands 0x07-0x09, stored in flash at 0x50000) (partial refresh, full refresh each hour; the partial waveform no longer drives red pixels, so red keeps its colour). Source: `Firmware/src/card_bg.h`, `epd.c` (`CARD_MODE`) | Display and time sync **yes** (card, live time). Editable name/title **not yet** |

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

## Sending an image

Open `web_tools/nowa152_image_upload.html` in Chrome or Edge (if BLE is blocked on
`file://`, run `python -m http.server` in this folder and open `http://localhost:8000`).
Send the built-in test pattern first to check orientation; the flip buttons fix a
mismatch without reflashing. The page also sets up the live name card (name, title, HP,
EXP, LV, battery, refresh interval, LED) and embeds the atc1441 UART flasher at the bottom
for flashing firmware. The BLE OTA update was removed from the page (a failed OTA write
can only be recovered over UART); its protocol is still described in `docs/ble_protocol.md`.

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
