#include "epd_ble_service.h"

#include <stdint.h>
#include "tl_common.h"
#include "stack/ble/ble.h"

#include "epd.h"
#include "ble.h"

#define ASSERT_MIN_LEN(val, min_len) \
	if (val < min_len)               \
	{                                \
		return 0;                    \
	}

extern unsigned char epd_buffer[epd_buffer_size];
// Red plane storage aliases epd_temp (the clock's OBD canvas): the two paths
// never overlap — the clock only redraws while the image is not held, and every
// upload starts with 0x00 clearing both planes. A dedicated 3 KB buffer would
// eat most of the ~4 KB SRAM left below _end_bss_ for the stack.
extern unsigned char epd_temp[epd_buffer_size];
unsigned int byte_pos = 0;
uint8_t epd_plane = 0; // 0x03 write target: 0 = b/w plane (epd_buffer), 1 = red plane (epd_temp)
extern void epd_resume_clock(void);
extern void card_portrait_set(uint8_t scene, uint16_t off, const uint8_t *d, uint16_t n);
extern void card_portrait_commit(uint8_t scene);
extern void card_portrait_reset(uint8_t scene);

int epd_ble_handle_write(void *p)
{
	rf_packet_att_write_t *req = (rf_packet_att_write_t *)p;
	uint8_t *payload = &req->value;
	unsigned int payload_len = req->l2capLen - 3;

	ASSERT_MIN_LEN(payload_len, 1);

	switch (payload[0])
	{
	// Clear both EPD planes (b/w fill byte + red blanked) and reset write state.
	case 0x00:
		ASSERT_MIN_LEN(payload_len, 2);
		memset(epd_buffer, payload[1], sizeof(epd_buffer));
		memset(epd_temp, 0, sizeof(epd_temp));
		epd_plane = 0;
		byte_pos = 0;
		ble_set_connection_speed(40);
		return 0;
	// Push both planes to display.
	case 0x01:
		ble_set_connection_speed(200);
		EPD_Display(epd_buffer, epd_temp, epd_buffer_size, 1);
		return 0;
	// Set byte_pos.
	case 0x02:
		ASSERT_MIN_LEN(payload_len, 3);
		byte_pos = payload[1] << 8 | payload[2];
		return 0;
	// Write data to the image buffer selected by epd_plane.
	case 0x03:
		if (byte_pos + payload_len - 1 >= sizeof(epd_buffer) + 1)
		{
			return 0;
		}
		memcpy((epd_plane ? epd_temp : epd_buffer) + byte_pos, payload + 1, payload_len - 1);
		byte_pos += payload_len - 1;
		return 0;
	case 0x04: // decode & display a TIFF image
		epd_display_tiff(epd_buffer, byte_pos);
		return 0;
	case 0x05: // hold custom image: 1 = keep BLE image (skip clock), 0 = resume clock. Volatile, reboot clears it.
		ASSERT_MIN_LEN(payload_len, 2);
		epd_hold_image = payload[1] ? 1 : 0;
		if (!epd_hold_image)
			epd_resume_clock();
		return 0;
	case 0x06: // select 0x03 write target plane (1 = red) and rewind byte_pos.
		// Firmware without this command ignores it and keeps writing the b/w
		// plane, so the red upload degrades to a no-op there.
		ASSERT_MIN_LEN(payload_len, 2);
		epd_plane = payload[1] ? 1 : 0;
		byte_pos = 0;
		return 0;
	case 0x07: // card text slot: [07, slot, x, w, w column bytes]
		ASSERT_MIN_LEN(payload_len, 4);
		if (payload_len - 4 >= payload[3])
			card_ovr_set(payload[1], payload[2], payload[3], payload + 4);
		return 0;
	case 0x0A: // card stats: [0A, hp 0..3, exp_mode, exp_pct, batt_mode, batt_pct, exp_start(2), exp_end(2), lv_start(2; 0xFFFF = off), refresh_min, led_off(1 = LED blink disabled)]
		ASSERT_MIN_LEN(payload_len, 6);
		if (payload_len >= 14)
			card_ovr_stats(payload[1], payload[2], payload[3], payload[4], payload[5],
						   (payload[6] << 8) | payload[7], (payload[8] << 8) | payload[9], (payload[10] << 8) | payload[11], payload[12], payload[13]);
		else if (payload_len >= 13)
			card_ovr_stats(payload[1], payload[2], payload[3], payload[4], payload[5],
						   (payload[6] << 8) | payload[7], (payload[8] << 8) | payload[9], (payload[10] << 8) | payload[11], payload[12], 0);
		else if (payload_len >= 12)
			card_ovr_stats(payload[1], payload[2], payload[3], payload[4], payload[5],
						   (payload[6] << 8) | payload[7], (payload[8] << 8) | payload[9], (payload[10] << 8) | payload[11], 1, 0);
		else if (payload_len >= 10)
			card_ovr_stats(payload[1], payload[2], payload[3], payload[4], payload[5],
						   (payload[6] << 8) | payload[7], (payload[8] << 8) | payload[9], 0xFFFF, 1, 0);
		else
			card_ovr_stats(payload[1], payload[2], payload[3], payload[4], payload[5], 480, 1020, 0xFFFF, 1, 0);
		return 0;
	case 0x0B: // portrait data: [0B, scene 0..3, offset hi, offset lo, bytes...] (black bitmap, then red bitmap)
		ASSERT_MIN_LEN(payload_len, 5);
		card_portrait_set(payload[1], (payload[2] << 8) | payload[3], payload + 4, payload_len - 4);
		return 0;
	case 0x0C: // save the staged portrait for a scene and redraw the card (full refresh)
		ASSERT_MIN_LEN(payload_len, 2);
		card_portrait_commit(payload[1]);
		epd_hold_image = 0;
		epd_resume_clock();
		return 0;
	case 0x0D: // forget the saved portrait of a scene (back to the built-in artwork)
		ASSERT_MIN_LEN(payload_len, 2);
		card_portrait_reset(payload[1]);
		epd_hold_image = 0;
		epd_resume_clock();
		return 0;
	case 0x08: // save staged card text to flash and redraw the card (full refresh)
		card_ovr_commit();
		epd_hold_image = 0;
		epd_resume_clock();
		return 0;
	case 0x09: // forget saved card text, back to the artwork text
		card_ovr_reset();
		epd_hold_image = 0;
		epd_resume_clock();
		return 0;
	default:
		return 0;
	}

	return 0;
}
