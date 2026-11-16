#pragma once
// Name-card feature, included by epd.c (kept separate so it can also be compiled on a PC for previews).
// ---- Name-card mode (BWR154): fixed artwork in flash, only date + time drawn live ----
#define CARD_MODE 1
// 1 = also send the red plane on partial refreshes (keeps the red artwork intact);
// 0 = old behaviour (blank red plane on partial)
#define CARD_PARTIAL_RED 1
// Force a full refresh every N minutes (0 = only the hourly one). A fallback build uses a
// small value if the partial waveform ever bleeds the red artwork.
#ifndef CARD_FULL_EVERY
#define CARD_FULL_EVERY 0
#endif

static void card_px(uint8_t *b, int x, int y)
{
    int yy = y + BWR_154_SRC_OFFSET;
    b[(151 - x) * BWR_154_SRC_BYTES + (yy >> 3)] &= ~(0x80 >> (yy & 7));
}

static void card_text(uint8_t *b, const char *s, int x, int y)
{
    for (; *s; s++, x += 6)
    {
        uint8_t c = (uint8_t)*s;
        if (c < 32 || c > 126)
            c = 32;
        const uint8_t *g = &card_font[(c - 32) * 5];
        int cx, r;
        for (cx = 0; cx < 5; cx++)
            for (r = 0; r < 8; r++)
                if ((g[cx] >> r) & 1)
                    card_px(b, x + cx, y + r);
    }
}

// days since 1970-01-01 -> month (0..11) and day (1..31) (Howard Hinnant's civil_from_days)
static void card_date(uint32_t days, int *mon, int *day)
{
    int32_t z = (int32_t)days + 719468;
    int32_t era = z / 146097;
    int32_t doe = z - era * 146097;
    int32_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    int32_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    int32_t mp = (5 * doy + 2) / 153;
    *day = doy - (153 * mp + 2) / 5 + 1;
    *mon = (mp < 10) ? mp + 2 : mp - 10; // 0 = Jan
}


// ---- Editable card text (name / title), saved in flash, sent from the web page ----
// Slot 0 = name (top-left, black ink on white), slot 1 = banner title (white ink on black).
// The page renders the text with the on-chip fonts and sends 8-row column bitmaps
// (bit0 = top row); an empty slot keeps the text that is part of the artwork.
#define CARD_OVR_ADDR 0x50000 // free flash sector (OTA staging ends at 0x3FEFF, settings at 0x78100)
#define CARD_OVR_MAGIC 0xCA4D0001u
#define CARD_OVR_MAXW 90
typedef struct
{
    uint8_t x, w;
    uint8_t data[CARD_OVR_MAXW];
} card_slot_t;
typedef struct
{
    uint32_t magic;
    card_slot_t slot[2];
    // HP / EXP / BATT settings (stats_valid == 1 when set from the web page)
    uint8_t stats_valid;
    uint8_t hp;        // hearts kept (0..3), lost ones lose their red fill
    uint8_t exp_mode;  // 0 = manual exp_pct, 1 = work day: full at 08:00, empty at 17:00
    uint8_t exp_pct;   // 0..100
    uint8_t batt_mode; // 0 = real battery level, 1 = manual batt_pct
    uint8_t batt_pct;  // 0..100
    uint16_t exp_start; // EXP window start, minutes after midnight (invalid/erased = 08:00)
    uint16_t exp_end;   // EXP window end, minutes after midnight (invalid/erased = 17:00)
    uint16_t lv_start;  // unused since LV became the work week (kept so older records stay valid)
    uint8_t refresh_min; // redraw the card every N minutes (1 if unset/invalid)
    uint8_t led_off;     // 1 = no status LED blink (anything else, incl. erased 0xFF, = blink)
} card_ovr_t;
RAM card_ovr_t card_stage; // staged between 0x07 writes and the 0x08 commit
RAM uint8_t card_stage_valid = 0;

static void card_stage_load(void)
{
    if (card_stage_valid)
        return;
    flash_read_page(CARD_OVR_ADDR, sizeof(card_stage), (uint8_t *)&card_stage);
    if (card_stage.magic != CARD_OVR_MAGIC)
        memset(&card_stage, 0, sizeof(card_stage));
    card_stage.magic = CARD_OVR_MAGIC;
    card_stage_valid = 1;
}

void card_ovr_set(uint8_t slot, uint8_t x, uint8_t w, const uint8_t *data)
{
    if (slot > 1 || w > CARD_OVR_MAXW || x + w > 152)
        return;
    card_stage_load();
    card_stage.slot[slot].x = x;
    card_stage.slot[slot].w = w;
    memcpy(card_stage.slot[slot].data, data, w);
}

void card_ovr_stats(uint8_t hp, uint8_t exp_mode, uint8_t exp_pct, uint8_t batt_mode, uint8_t batt_pct, uint16_t exp_start, uint16_t exp_end, uint16_t lv_start, uint8_t refresh_min, uint8_t led_off)
{
    card_stage_load();
    card_stage.stats_valid = 1;
    card_stage.hp = hp > 3 ? 3 : hp;
    card_stage.exp_mode = exp_mode ? 1 : 0;
    card_stage.exp_pct = exp_pct > 7 ? 255 : exp_pct; // 0..7 = manual blocks, anything else = weekday
    card_stage.batt_mode = batt_mode ? 1 : 0;
    card_stage.batt_pct = batt_pct > 100 ? 100 : batt_pct;
    card_stage.exp_start = exp_start;
    card_stage.exp_end = exp_end;
    card_stage.lv_start = lv_start;
    card_stage.refresh_min = refresh_min;
    card_stage.led_off = led_off ? 1 : 0;
}

void card_ovr_commit(void)
{
    card_stage_load();
    flash_erase_sector(CARD_OVR_ADDR);
    flash_write_page(CARD_OVR_ADDR, sizeof(card_stage), (uint8_t *)&card_stage);
}

void card_ovr_reset(void)
{
    flash_erase_sector(CARD_OVR_ADDR);
    card_stage_valid = 0;
}

// Minutes between card redraws, as saved from the web page (1 when unset).
uint8_t card_refresh_interval(void)
{
    card_ovr_t o;
    flash_read_page(CARD_OVR_ADDR, sizeof(o), (uint8_t *)&o);
    if (o.magic == CARD_OVR_MAGIC && o.stats_valid == 1 && o.refresh_min >= 1 && o.refresh_min <= 60)
        return o.refresh_min;
    return 1;
}

// Status LED blink on/off, as saved from the web page (on when unset).
uint8_t card_led_enabled(void)
{
    card_ovr_t o;
    flash_read_page(CARD_OVR_ADDR, sizeof(o), (uint8_t *)&o);
    return !(o.magic == CARD_OVR_MAGIC && o.stats_valid == 1 && o.led_off == 1);
}

static void card_px_set(uint8_t *b, int x, int y, uint8_t white)
{
    int yy = y + BWR_154_SRC_OFFSET;
    uint8_t *p = &b[(151 - x) * BWR_154_SRC_BYTES + (yy >> 3)];
    if (white)
        *p |= (0x80 >> (yy & 7));
    else
        *p &= ~(0x80 >> (yy & 7));
}

static void card_apply_slot(uint8_t *b, const card_slot_t *sl, int cx, int cy, int cw, int ch, int ty, uint8_t white_bg)
{
    int i, j;
    if (!sl->w || sl->w > CARD_OVR_MAXW)
        return;
    for (i = 0; i < cw; i++)
        for (j = 0; j < ch; j++)
            card_px_set(b, cx + i, cy + j, white_bg);
    for (i = 0; i < sl->w; i++)
        for (j = 0; j < 8; j++)
            if ((sl->data[i] >> j) & 1)
                card_px_set(b, sl->x + i, ty + j, !white_bg);
}

static void card_red_set(uint8_t *r, int x, int y, uint8_t on)
{
    int yy = y + BWR_154_SRC_OFFSET;
    uint8_t *p = &r[(151 - x) * BWR_154_SRC_BYTES + (yy >> 3)];
    if (on)
        *p |= (0x80 >> (yy & 7));
    else
        *p &= ~(0x80 >> (yy & 7));
}

static int card_workweek(uint32_t days);

// HP hearts, EXP blocks and BATT bar. Anything that gains or loses red only shows after the
// next full refresh (the partial waveform leaves red pixels alone on purpose), so these
// values are applied together with the hourly / on-save full refresh.
static void card_apply_stats(uint8_t *b, uint8_t *r, const card_ovr_t *o, uint32_t time_is, uint16_t battery_mv)
{
    int i, x, y;
    static const uint8_t heart_y[3] = {58, 74, 92};
    if (o->stats_valid != 1)
        return;
    // HP: 3 hearts = 6 half-heart steps. Auto mode (exp_mode = 1) drains them over the
    // start..end window (full at the start, empty at the end); manual mode keeps o->hp hearts.
    {
        int u = (o->hp > 3 ? 3 : o->hp) * 2;
        if (o->exp_mode)
        {
            int st = (o->exp_start < 1440) ? o->exp_start : 480; // default 08:00
            int en = (o->exp_end < 1440) ? o->exp_end : 1020;    // default 17:00
            int now = (int)((time_is / 60) % 1440), dur, el;
            if (st == en)
                en = (st + 1) % 1440;
            if (st < en) // same-day window: full before the start, empty after the end
            {
                dur = en - st;
                el = now <= st ? 0 : (now >= en ? dur : now - st);
            }
            else // overnight window (end earlier than start): empty outside of it
            {
                dur = en - st + 1440;
                el = (now - st + 1440) % 1440;
                if (el >= dur)
                    el = dur;
            }
            u = 6 - (el * 6) / dur;
        }
        for (i = 0; i < 3; i++) // hearts drain from the top; the outline stays, the red fill goes
        {
            int k = 2 - i; // heart i = 0 is the top one and empties first
            int x0 = (u >= 2 * k + 2) ? 99 : ((u == 2 * k + 1) ? 17 : 9); // full: nothing, half: right half, empty: all
            for (y = heart_y[i]; y < heart_y[i] + 14; y++)
                for (x = x0; x <= 24; x++)
                    card_red_set(r, x, y, 0);
        }
    }
    // EXP: 7 blocks, one per weekday (Monday = 1 block ... Sunday = 7). exp_pct 0..7 = manual blocks.
    {
        int blocks = (int)(((time_is / 86400) + 3) % 7) + 1; // 1970-01-01 was a Thursday
        if (o->exp_pct <= 7)
            blocks = o->exp_pct;
        for (y = 115; y <= 125; y++)
            for (x = 56; x <= 106; x++)
                card_px_set(b, x, y, 1);
        for (i = 0; i < 7; i++)
            for (y = 117; y <= 123; y++)
                for (x = 57 + i * 7; x <= 61 + i * 7; x++)
                {
                    int edge = (y == 117 || y == 123 || x == 57 + i * 7 || x == 61 + i * 7);
                    if (i < blocks || edge)
                        card_px_set(b, x, y, 0);
                }
    }
    // LV = Intel work week number (two digits, drawn over the artwork's number)
    {
        int ww = card_workweek((uint32_t)(time_is / 86400));
        int d, r, cx;
        for (y = 8; y <= 18; y++)
            for (x = 127; x <= 143; x++)
                card_px_set(b, x, y, 1);
        for (d = 0; d < 2; d++)
        {
            int digit = (d == 0) ? ww / 10 : ww % 10;
            if (d == 0 && digit == 0)
                continue; // single digit: right aligned
            const uint8_t *g = &card_font8[('0' + digit - 32) * 7];
            for (cx = 0; cx < 7; cx++)
                for (r = 0; r < 8; r++)
                    if ((g[cx] >> r) & 1)
                        card_px_set(b, 128 + d * 8 + cx, 9 + r, 0);
        }
    }
    // BATT bar interior x 126..137, y 26..29
    {
        int pct = o->batt_mode ? o->batt_pct : (battery_mv ? get_battery_level(battery_mv) : 100);
        if (pct > 100)
            pct = 100;
        int w = (pct * 12 + 50) / 100;
        for (x = 126; x <= 137; x++)
            for (y = 26; y <= 29; y++)
            {
                card_px_set(b, x, y, 1);
                card_red_set(r, x, y, 0);
            }
        for (x = 126; x < 126 + w; x++)
            for (y = 26; y <= 29; y++)
            {
                if (pct < 20)
                    card_red_set(r, x, y, 1);
                else
                    card_px_set(b, x, y, 0);
            }
    }
}

// ---- Time-of-day scene: four full backgrounds (card_art_*.h) picked by the hour ----
// MONIN 05:00-11:59, NOON 12:00-15:59, EVENIN 16:00-20:59, NITE 21:00-04:59.
// The scenes carry red, so they change with the full refresh at the top of the hour.
static int card_scene(uint32_t time_is)
{
    int h = (int)((time_is / 3600) % 24);
    return (h >= 5 && h < 12) ? 0 : ((h >= 12 && h < 16) ? 1 : ((h >= 16 && h < 21) ? 2 : 3));
}

// Intel-style work week: weeks start on Sunday, WW01 is the week that contains 1 January.
static int card_workweek(uint32_t days)
{
    int32_t z = (int32_t)days + 719468;
    int32_t era = z / 146097;
    int32_t doe = z - era * 146097;
    int32_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    int32_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100); // 0 = 1 March
    int32_t mp = (5 * doy + 2) / 153;
    int32_t y = yoe + era * 400 + (mp >= 10 ? 1 : 0);
    int leap = ((y % 4 == 0) && (y % 100 != 0 || y % 400 == 0)) ? 1 : 0;
    int32_t d0 = (mp >= 10) ? doy - 306 : doy + 59 + leap; // days since 1 January of year y
    int32_t jan1 = (int32_t)days - d0;                      // day number of 1 January
    int32_t dow1 = (jan1 + 4) % 7;                          // weekday of 1 January, Sunday = 0
    int32_t ylen = 365 + leap;
    int32_t dow = ((int32_t)days + 4) % 7;
    if ((int32_t)days - dow + 6 >= jan1 + ylen) // this week contains 1 January of the next year
        return 1;
    return (int)((d0 + dow1) / 7) + 1;
}

// ---- Per-scene portraits, uploaded from the web page ----
// One flash sector per scene (0x51000 + scene * 0x1000). Layout: black-ink bitmap, red bitmap
// (row-major, MSB first, 53 x 78 px each), the magic word sits at offset 1040 and is written last.
// The portrait replaces the middle of the card; the bubble, hearts and scene art stay.
#define CARD_PORTRAIT_ADDR 0x51000
#define CARD_PORTRAIT_MAGIC 0xCA505201u
#define CARD_PORTRAIT_X 52
#define CARD_PORTRAIT_Y 36
#define CARD_PORTRAIT_W 53
#define CARD_PORTRAIT_H 78
#define CARD_PORTRAIT_PLANE ((CARD_PORTRAIT_W * CARD_PORTRAIT_H + 7) / 8) // 517
#define CARD_PORTRAIT_MAGIC_OFF 1040
// Staging reuses the red-plane buffer epd_temp (RAM is tight). The web page holds the redraw (0x05 01)
// while it uploads, and the commit (0x0C) releases it again.
#define card_pstage epd_temp
#define CARD_PORTRAIT_BYTES (2 * CARD_PORTRAIT_PLANE)

void card_portrait_set(uint8_t scene, uint16_t off, const uint8_t *d, uint16_t n)
{
    if (scene > 3 || (uint32_t)off + n > CARD_PORTRAIT_BYTES)
        return;
    memcpy(card_pstage + off, d, n);
}

void card_portrait_commit(uint8_t scene)
{
    uint32_t addr = CARD_PORTRAIT_ADDR + (uint32_t)scene * 0x1000, magic = CARD_PORTRAIT_MAGIC;
    uint16_t o, n;
    if (scene > 3)
        return;
    flash_erase_sector(addr);
    for (o = 0; o < CARD_PORTRAIT_BYTES; o += 256)
    {
        n = CARD_PORTRAIT_BYTES - o;
        if (n > 256)
            n = 256;
        flash_write_page(addr + o, n, card_pstage + o);
    }
    flash_write_page(addr + CARD_PORTRAIT_MAGIC_OFF, 4, (uint8_t *)&magic);
}

void card_portrait_reset(uint8_t scene)
{
    if (scene <= 3)
        flash_erase_sector(CARD_PORTRAIT_ADDR + (uint32_t)scene * 0x1000);
}

static void card_apply_portrait(uint8_t *b, uint8_t *r, int sc)
{
    uint32_t addr = CARD_PORTRAIT_ADDR + (uint32_t)sc * 0x1000, magic = 0;
    uint8_t cache[64];
    int pass, i, j, base, have;
    flash_read_page(addr + CARD_PORTRAIT_MAGIC_OFF, 4, (uint8_t *)&magic);
    if (magic != CARD_PORTRAIT_MAGIC)
        return;
    for (pass = 0; pass < 2; pass++) // pass 0: black ink (and clears red), pass 1: red
    {
        base = -1000;
        have = 0;
        for (j = 0; j < CARD_PORTRAIT_H; j++)
            for (i = 0; i < CARD_PORTRAIT_W; i++)
            {
                int x = CARD_PORTRAIT_X + i, y = CARD_PORTRAIT_Y + j;
                int idx = j * CARD_PORTRAIT_W + i, off = (idx >> 3) + pass * CARD_PORTRAIT_PLANE;
                uint8_t bit;
                if (y < 60 && x >= 100) // keep the speech bubble
                    continue;
                if (!have || off < base || off >= base + 64)
                {
                    base = off & ~63;
                    flash_read_page(addr + base, 64, cache);
                    have = 1;
                }
                bit = (cache[off - base] >> (7 - (idx & 7))) & 1;
                if (pass == 0)
                {
                    card_px_set(b, x, y, !bit);
                    card_red_set(r, x, y, 0);
                }
                else if (bit)
                    card_red_set(r, x, y, 1);
            }
    }
}

static const char *card_months = "JANFEBMARAPRMAYJUNJULAUGSEPOCTNOVDEC";

static void card_draw(uint32_t time_is, uint16_t battery_mv, uint8_t full_or_partial)
{
    char buff[16];
    int mon, day, h, mi, i;
#if CARD_FULL_EVERY
    if (((time_is / 60) % CARD_FULL_EVERY) == 0)
        full_or_partial = 1;
#endif
    {
        int sc = card_scene(time_is);
        memcpy(epd_buffer, card_bg_black[sc], CARD_BG_SIZE);
        memcpy(epd_temp, card_bg_red[sc], CARD_BG_SIZE);
        card_apply_portrait(epd_buffer, epd_temp, sc);
    }
    {
        card_ovr_t o;
        flash_read_page(CARD_OVR_ADDR, sizeof(o), (uint8_t *)&o);
        if (o.magic == CARD_OVR_MAGIC)
        {
            card_apply_slot(epd_buffer, &o.slot[0], 8, 8, 81, 11, 9, 1);    // name on white
            card_apply_slot(epd_buffer, &o.slot[1], 32, 132, 88, 10, 133, 0); // title on black
        }
        else
            memset(&o, 0xFF, sizeof(o)); // never configured: erased fields fall back to the defaults
        if (o.magic != CARD_OVR_MAGIC || o.stats_valid != 1)
        {
            o.stats_valid = 1; // defaults: HP drains 08:00-17:00, EXP = weekday, real battery, art LV
            o.hp = 3;
            o.exp_mode = 1;
            o.exp_pct = 255;
            o.batt_mode = 0;
            o.exp_start = 480;
            o.exp_end = 1020;
            o.lv_start = 0xFFFF;
        }
        card_apply_stats(epd_buffer, epd_temp, &o, time_is, battery_mv);
    }
    card_date(time_is / 86400, &mon, &day);
    for (i = 0; i < 3; i++)
        buff[i] = card_months[mon * 3 + i];
    buff[3] = ' ';
    if (day >= 10)
    {
        buff[4] = '0' + day / 10;
        buff[5] = '0' + day % 10;
        buff[6] = 0;
    }
    else
    {
        buff[4] = '0' + day;
        buff[5] = 0;
    }
    card_text(epd_buffer, buff, 10, 24);
    h = (time_is / 3600) % 24;
    mi = (time_is / 60) % 60;
    i = 0;
    if ((h % 12 ? h % 12 : 12) >= 10)
        buff[i++] = '1';
    buff[i++] = '0' + ((h % 12 ? h % 12 : 12) % 10);
    buff[i++] = ':';
    buff[i++] = '0' + mi / 10;
    buff[i++] = '0' + mi % 10;
    buff[i++] = ' ';
    buff[i++] = h < 12 ? 'A' : 'P';
    buff[i++] = 'M';
    buff[i] = 0;
    card_text(epd_buffer, buff, 95 - (i * 6 - 1), 24); // right edge at x = 94
    EPD_Display(epd_buffer, (full_or_partial || CARD_PARTIAL_RED) ? epd_temp : NULL, CARD_BG_SIZE, full_or_partial);
}

