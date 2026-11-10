/* SPDX-License-Identifier: GPL-2.0-or-later */
/* screen-check: core-a4mk2 0.6's screen calls on the Analog Four MKII (OS
 * 1.56), on screen: screen-check-ar2 with the A4's keys (mod.json's cflags:
 * K_MODE, K_RIGHT, K_HOLD, K_BOX, K_NO). K_MODE (taken) steps through four
 * modes; mode 0 changes nothing the unit does but take K_MODE.
 *
 *   0  off: stock, untouched; only K_MODE is taken.
 *   1  FONTS: the screen cleared, then font n's sample (n from 0; K_RIGHT,
 *      taken, steps it through 0..16) drawn on the frame at glass top row
 *      2, its width (core_text_width) as a 1-pixel line under it, and at
 *      the bottom, in font 16 and inverted, "F<n> W<w> H<h> D<d>" drawn
 *      into the mod's own bitmap (core_bitmap_init) and then copied onto
 *      the frame. The sample has a '%' in it, drawn as is.
 *   2  CALLS: stock's screen, with the bottom bar (rows 55..63) the mod's:
 *      "OV<bits> LV<ticks> K<n> R<r> P<h> <done>", in font 16, inverted:
 *      OV core_stock_overlay, LV core_level_readout, K the key events held, R
 *      the last replay's return plus 1 (- before any),
 *      P the progress box's handle (- for none) and its fill.
 *      - K_HOLD (FILTERS): its press and release are taken and kept (K
 *        counts them), then handed to stock through core_stock_key 20 and
 *        22 ticks later: stock's FILTERS page comes a second late. R shows
 *        what the last replay returned (0 on the A4 once stock had it).
 *      - K_BOX (taken) opens a progress box "~T PROGRESS" that fills over
 *        40 ticks and closes 20 ticks after it is full; K_NO while it is
 *        open is taken and closes it at once.
 *   3  COVER: mode 2 over a checkerboard covering the whole screen, but for
 *      the track level readout's field while core_level_readout runs
 *      (core_level_field_save / _put).
 *
 * In RAM after the magic SCC1 (sc): the mode, the font, the last bar's
 * values (overlay bits, level ticks, keys held, box handle and fill), the
 * replays and their returns, and for the font shown its width, height and
 * descent, for a readback.
 */
#include <stdint.h>
#include "analog-four-mk2/screen.h"

#if !defined(K_MODE) || !defined(K_RIGHT) || !defined(K_HOLD) || !defined(K_BOX) || !defined(K_NO)
#error "mod.json's cflags give the A4's key codes"
#endif
#define KEY(ev) (*(const int32_t *)((const uint8_t *)(ev) + 12))
#define FLAGS(ev) (*(const int32_t *)((const uint8_t *)(ev) + 16))
#define DOWN 1

static volatile int mode, font;
static unsigned ticks;

/* the held K_HOLD events, replayed in order */
static uint8_t held[2][CORE_KEY_EVENT_SIZE];
static unsigned held_at[2];
static int nheld, nreplayed, replay_ret[2];

static int box = -1, box_done;
static unsigned box_full_at;

static uint8_t own_bm[CORE_BITMAP_SIZE];

struct {
    char magic[4];
    int32_t mode, font;
    int32_t overlay, level, held, box, box_done;
    int32_t replays, last_ret;
    int32_t f_width, f_height, f_descent;
    uint32_t ticks;
} sc = {{'S', 'C', 'C', '1'}};
static uint32_t own_bits[256], own_mask[256];

/* ---- small helpers: the frame's pixels, 8 bytes a column, row r from the
 * top in byte 7 - r / 8, bit 7 - r % 8 ---- */
static uint8_t *px(void *bmp)
{
    return *(uint8_t **)((uint8_t *)bmp + 16);
}

static void set_row(uint8_t *p, int x0, int x1, int r, int on)
{
    int x;
    for (x = x0; x <= x1; x++) {
        uint8_t *b = p + 8 * x + 7 - r / 8, m = (uint8_t)(1 << (r % 8));
        *b = on ? (uint8_t)(*b | m) : (uint8_t)(*b & ~m);
    }
}

static int utoa(char *o, unsigned v)
{
    char t[10];
    int n = 0, i = 0;
    do t[n++] = (char)('0' + v % 10); while ((v /= 10) && n < 10);
    while (n)
        o[i++] = t[--n];
    return i;
}

static char *put(char *o, const char *s)
{
    while (*s)
        *o++ = *s++;
    return o;
}

/* ---- events ---- */

int ck_key(void *brain, void *ev)
{
    int k = KEY(ev), down = FLAGS(ev) & DOWN;
    (void)brain;
    if (k == K_MODE) {
        if (down)
            mode = (mode + 1) & 3;
        return 1;
    }
    if (mode == 1 && k == K_RIGHT) {
        if (down)
            font = (font + 1) % CORE_FONTS;
        return 1;
    }
    if (mode >= 2 && k == K_HOLD && nheld < 2) {
        const uint8_t *s = ev;
        int i;
        for (i = 0; i < CORE_KEY_EVENT_SIZE; i++)
            held[nheld][i] = s[i];
        held_at[nheld] = ticks + 20 + 2 * (unsigned)nheld;
        nheld++;
        return 1;
    }
    if (mode >= 2 && k == K_BOX) {
        if (down && box < 0) {
            box = core_progress_open("~T PROGRESS");
            box_done = 0;
            box_full_at = 0;
        }
        return 1;
    }
    if (box >= 0 && k == K_NO) {
        if (down) {
            core_progress_close(box);
            box = -1;
        }
        return 1;
    }
    return 0;
}

void ck_tick(void *ctrl)
{
    int i;
    ticks++;
    while (nreplayed < nheld && (int)(ticks - held_at[nreplayed]) >= 0) {
        i = nreplayed++;
        replay_ret[i] = core_stock_key(held[i]);
        sc.replays++;
        sc.last_ret = replay_ret[i];
    }
    if (nreplayed == nheld && nheld)
        nheld = nreplayed = 0;
    if (box >= 0) {
        if (box_done < 40)
            core_progress_set(box, ++box_done, 40);
        else if (!box_full_at)
            box_full_at = ticks;
        else if (ticks - box_full_at >= 20) {
            core_progress_close(box);
            box = -1;
        }
    }
    sc.ticks = ticks;
    sc.mode = mode;
    sc.font = font;
    sc.overlay = core_stock_overlay();
    sc.level = core_level_readout();
    sc.held = nheld - nreplayed;
    sc.box = box;
    sc.box_done = box_done;
    sc.f_width = core_text_width(font, "Ab1 %d%");
    sc.f_height = core_font_height(font);
    sc.f_descent = core_font_descent(font);
    if (mode)
        *((volatile uint8_t *)ctrl + 0x20) = 1;   /* redraw: the bars move */
}

static void draw_fonts(void *bmp)
{
    static const char sample[] = "Ab1 %d%";
    char lab[40], *o = lab;
    uint8_t *p = px(bmp), *q = (uint8_t *)own_bits;
    int i, f = font, h = core_font_height(f), w = core_text_width(f, sample);
    for (i = 0; i < 1024; i++)
        p[i] = 0;
    core_text_draw(bmp, f, 2, 64 - h - 2, CORE_ALIGN_LEFT, sample);
    if (w > 0 && h + 4 < 50)
        set_row(p, 2, 2 + w - 1, h + 4, 1);
    /* the label, through the mod's own bitmap */
    for (i = 0; i < 256; i++)
        own_bits[i] = 0;
    if (!own_bm[0] && !own_bm[1] && !own_bm[2] && !own_bm[3])
        core_bitmap_init(own_bm, 128, 64, own_bits, own_mask);
    for (i = 55; i < 64; i++)
        set_row(q, 0, 127, i, 1);
    o = put(o, "F");
    o += utoa(o, (unsigned)f);
    o = put(o, " W");
    o += utoa(o, (unsigned)w);
    o = put(o, " H");
    o += utoa(o, (unsigned)h);
    o = put(o, " D");
    o += utoa(o, (unsigned)core_font_descent(f));
    *o = 0;
    core_text_draw(own_bm, CORE_FONT_MAIN, 2, 1, CORE_ALIGN_LEFT | CORE_TEXT_INVERT, lab);
    for (i = 0; i < 1024; i++)
        p[i] |= q[i];
}

static void draw_bar(void *bmp)
{
    char t[48], *o = t;
    uint8_t *p = px(bmp);
    int r;
    for (r = 55; r < 64; r++)
        set_row(p, 0, 127, r, 1);
    o = put(o, "OV");
    o += utoa(o, (unsigned)core_stock_overlay());
    o = put(o, " LV");
    o += utoa(o, (unsigned)core_level_readout());
    o = put(o, " K");
    o += utoa(o, (unsigned)(nheld - nreplayed));
    o = put(o, " R");
    if (sc.replays)
        o += utoa(o, (unsigned)(sc.last_ret + 1));  /* 1: returned 0; 0: returned -1 */
    else
        o = put(o, "-");
    o = put(o, " P");
    if (box < 0)
        o = put(o, "-");
    else {
        o += utoa(o, (unsigned)box);
        *o++ = ' ';
        o += utoa(o, (unsigned)box_done);
    }
    *o = 0;
    core_text_draw(bmp, CORE_FONT_MAIN, 2, 1, CORE_ALIGN_LEFT | CORE_TEXT_INVERT, t);
}

void ck_draw(void *bmp, void *ctrl)
{
    (void)ctrl;
    if (mode == 1)
        draw_fonts(bmp);
    else if (mode == 2)
        draw_bar(bmp);
    else if (mode == 3) {
        uint8_t keep[CORE_LEVEL_FIELD_SIZE], *p = px(bmp);
        int lev = core_level_readout(), i;
        if (lev)
            core_level_field_save(bmp, keep);
        for (i = 0; i < 1024; i++)
            p[i] = (i / 8) & 1 ? 0xaa : 0x55;
        if (lev)
            core_level_field_put(bmp, keep);
        draw_bar(bmp);
    }
}
