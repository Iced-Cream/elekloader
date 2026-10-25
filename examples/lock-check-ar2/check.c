/* SPDX-License-Identifier: GPL-2.0-or-later */
/* lock-check: the smallest user of core-ar2's lockable parameters (core 0.8)
 * and their p-locks (core 0.9).
 *
 * mod.json declares two sound fields with "lock": true, so the lockfile
 * gives each a lock code, and registers them in core_lock_params. Core then
 * lists LOCK CHECK A and B at the end of every sound's LFO destination menu
 * (shown as LCA and LCB in the DST value). A bar along the bottom edge shows
 * the offset track 1's LFO gives either one (core_lfo_offset), from the
 * middle, one pixel per stock knob step; a 3x3 square at the left end shows
 * which (top: A, bottom: B).
 *
 * Behind the LFO key, a LOCK CHECK page shows A on knob A (borrowing stock's
 * id 1, as mods' pages do) for track 1's sound of the current kit: RANDOMIZE
 * PAGE and RELOAD PAGE reach it through its lock parameter's id. With a trig
 * held, knob A sets that trig's p-lock of A instead (core 0.9), and a second
 * bar above the first shows the lock of the trig track 1 plays, half a pixel
 * a step from the left (core_lock_value), none when it has no lock of A.
 */
#include "analog-rytm-mk2/store.h"
#include "analog-rytm-mk2/lock.h"
#include "analog-rytm-mk2/pages.h"

typedef void (*fillrect_t)(void *bmp, int x0, int y0, int x1, int y1, int colour);
#define FILLRECT ((fillrect_t)FILLRECT_AT)

STORE_FIELD(lc_a);
STORE_FIELD(lc_b);

const struct core_lock_param lc_param_a = {STORE(lc_a), 1, 0, 127, "LCA", "LOCK CHECK A"};
const struct core_lock_param lc_param_b = {STORE(lc_b), 0, 0, 15, "LCB", "LOCK CHECK B"};

static volatile unsigned lc_ticks;

void lc_tick(void *ctrl)
{
    if ((++lc_ticks & 3) == 0)
        *((volatile unsigned char *)ctrl + 0x20) = 1;
}

void lc_draw(void *bmp, void *ctrl)
{
    int a = core_lfo_offset(0, STORE(lc_a)), b = core_lfo_offset(0, STORE(lc_b));
    int v = (a ? a : b) / 256;
    (void)ctrl;
    /* y counts from the bottom row: 0..3 is the bottom edge */
    FILLRECT(bmp, 0, 0, 127, 3, 0);
    if (v > 60)
        v = 60;
    if (v < -60)
        v = -60;
    if (v > 0)
        FILLRECT(bmp, 64, 0, 64 + v, 1, 1);
    else if (v < 0)
        FILLRECT(bmp, 64 + v, 0, 64, 1, 1);
    if (a)
        FILLRECT(bmp, 0, 2, 2, 3, 1);
    if (b)
        FILLRECT(bmp, 0, 0, 2, 1, 1);
    FILLRECT(bmp, 0, 5, 127, 6, 0);
    if ((v = core_lock_value(0, STORE(lc_a))) >= 0)
        FILLRECT(bmp, 0, 5, v / 2, 6, 1);
}

/* track 1's sound of the current kit: the live kits as edited, the stored
 * project's working kit index */
static void *sound1(void)
{
    const volatile unsigned char *ix = (const unsigned char *)0x405c8f20 + 0x19d3f7;
    unsigned k = (unsigned)ix[0] << 24 | (unsigned)ix[1] << 16 | (unsigned)ix[2] << 8 | ix[3];
    return k < 128 ? (void *)(0x407c89cb + 0xa8cf18 + 2531 * k + 42) : 0;
}

const struct core_page lc_page = {14, "LOCK CHECK", {1, 0, 0, 0, 0, 0, 0, 0}};
const struct core_view_page lc_view_page = {CORE_VIEW_LFO, 14, 0};

static int lc_get(int32_t id, int32_t *value)
{
    void *s = sound1();
    if (id != 1 || !s)
        return 0;
    *value = core_store_get(STORE(lc_a), s, 0) << 8;
    return 1;
}

static int lc_delta(void *view, int32_t id, int32_t delta, int32_t func)
{
    void *s = sound1();
    int v;
    (void)view;
    (void)func;
    if (id != 1 || !s)
        return 0;
    v = (int)core_store_get(STORE(lc_a), s, 0) + (delta > 0 ? (delta + 255) / 256 : -((255 - delta) / 256));
    core_store_set(STORE(lc_a), s, 0, v < 0 ? 0 : v > 127 ? 127 : v);
    return 1;
}

const struct core_param lc_params = {1, 1, lc_get, lc_delta, 0};
