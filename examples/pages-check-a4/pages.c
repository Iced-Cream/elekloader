/* SPDX-License-Identifier: GPL-2.0-or-later */
/* pages-check: a test for core-a4 0.2's parameter routines on the Analog
 * Four MKII (OS 1.56). It contributes one core_params entry, for AMP PAN
 * (parameter 97), and changes nothing else:
 *   - text: the value is shown as two hex digits (its whole part, 8.8),
 *     "00" to "7F", where stock shows its own text;
 *   - delta: counts each turn and passes it on, so the knob still turns;
 *   - get: passes.
 * Each turn of PAN also lengthens a 2-pixel bar along the top of the screen
 * (mod 128), so a turn that reaches core shows even where stock draws no
 * text.
 */
#include <stdint.h>
#include "analog-four-mk2/pages.h"

typedef void (*fillrect_t)(void *bmp, int x0, int y0, int x1, int y1, int colour);
#ifndef FILLRECT_AT
#error "FILLRECT_AT: fill_rect's address (mod.json cflags)"
#endif
#define FILLRECT ((fillrect_t)FILLRECT_AT)

#define PAN 97

static volatile unsigned pc_turns;

static int pc_delta(void *view, int32_t id, int32_t delta, int32_t func)
{
    (void)view; (void)id; (void)delta; (void)func;
    pc_turns++;
    return 0;
}

static int pc_text(struct core_text *t)
{
    static const char hex[] = "0123456789ABCDEF";
    unsigned v = ((unsigned)t->value >> 8) & 0xff;
    t->buf[0] = hex[v >> 4];
    t->buf[1] = hex[v & 15];
    t->buf[2] = 0;
    return CORE_TEXT_DONE;
}

const struct core_param pc_params = {PAN, PAN, 0, pc_delta, pc_text};

void pc_draw(void *bmp, void *ctrl)
{
    unsigned n = pc_turns & 127;
    (void)ctrl;
    FILLRECT(bmp, 0, 60, 127, 61, 0);
    if (n)
        FILLRECT(bmp, 0, 60, (int)n - 1, 61, 1);
}
