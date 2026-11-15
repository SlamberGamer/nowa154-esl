# Notice

This project builds on work by others. Their license terms apply to the parts taken from them.

- **atc1441/ATC_TLSR_Paper** (MIT): the base firmware, BLE service, web flasher/uploader concept.
  Its license text is kept at `Firmware/src/LICENSE`.
- **wang551/nowa154-etag** (MIT, Copyright (c) 2026 wang551): reverse-engineered 152x152 panel
  geometry, the BWR154 driver with red plane, BLE commands `0x05`/`0x06`, the advertising fix,
  signed temperature, text clipping, the dashboard layout, `web_tools/nowa152_image_upload.html`
  and `docs/screen_adaptation.md`, `docs/ble_protocol.md`, `docs/hardware_notes.md`.
  Source: https://github.com/wang551/nowa154-etag. The NFC experiment code from that repo is not included.
- **OneBitDisplay / TIFF_G4** by Larry Bank (bundled in `Firmware/src`, original licenses apply).
- The Telink SDK in `Firmware/components` is distributed under its original license.

The `LICENSE` file is the MIT text from wang551/nowa154-etag, kept as required.
Add your own copyright line for your changes if you wish.

## web_tools/nowa152_image_upload.html
Originally by wang551 (MIT). The UI, log messages and comments were translated from Chinese to English; the BLE OTA section was removed, the theme was redone (terminal, Windows 98 and clean themes, switchable), and the page embeds atc1441's UART flasher (https://atc1441.github.io/ATC_TLSR_Paper_UART_Flasher.html) in an iframe; that flasher is not copied into this repo.
