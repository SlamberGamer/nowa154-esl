# Bring-up notes: how the 152x152 mapping was found

Kept as a record of what was tried on a real Nowa-154R-N, so the next panel variant is quicker.

1. **Symptom:** the stock firmware showed `BWR213` with a minute counter and scrambled images.
   The panel is not a 2.13": auto-detect answers the same for both controllers.
2. **Detection:** forcing model 3 with the Stellar-S3TN@ 200x200 driver did not change the screen.
3. **Serial log:** the firmware prints debug text on its TXD pad at 115200 8N1.
   Busy-pin timing showed a 13.5 s refresh, which is a real three-colour refresh, so the panel
   was being driven but the data was not landing where expected.
4. **Fill test:** a solid black fill changed only the left ~36% of the glass.
5. **Pattern test:** straight and slanted bars showed the row stride was wrong. The 200-pixel
   window overflowed what the panel accepts.
6. **Result:** window Y 295 to 144, 20 bytes per line, source mode `0x80`, 3040 bytes.
   A pattern with a top bar, a middle bar and two corner blocks then rendered cleanly and
   showed the orientation: line 0 on the right edge, bytes running top to bottom.

Gotchas that cost time:

- Variables that must survive deep sleep need the `RAM` attribute, otherwise they reset on every wake.
- The RAM image `.bin` built here has a 4-byte CRC the flasher images do not.
  `make` can also keep a stale `.bin` when timestamps are skewed; delete it before rebuilding.
- `makefile` does not track headers.
