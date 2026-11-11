/* SPDX-License-Identifier: GPL-2.0-or-later */
/* savenew-check: core-a4mk2 0.6's CORE_SAVE_NEW on the Analog Four MKII
 * (OS 1.56), which no panel action reaches (README).
 *
 * Stock's project load request is built by 0x4003f7cc(slot, callback,
 * create_if_empty, ask_save) and run by the load job 0x4003daf0. For an empty
 * slot with create_if_empty set, the job makes a new project
 * (0x400a9f1e(model, -1): ev_project with CORE_PROJECT_NEW) and saves it into
 * the slot with the whole save 0x400a771a, called from 0x4003dc20: the save
 * core reports as CORE_SAVE_NEW. Every stock caller of 0x4003f7cc passes
 * create_if_empty 0 and ask_save 1 (LOAD FROM 0x4003faec, the two LOAD
 * PROJECT lists 0x400bf74e and 0x400e1304), so on stock the branch is dead.
 * This mod makes the same request itself, with ask_save 1 as stock does:
 *
 *   encoder J  create_if_empty 1: the new project, saved into slot 128
 *              (CORE_SAVE_NEW, slot 127);
 *   encoder I  create_if_empty 0, as stock: the job finds the slot empty and
 *              fails; no save call, no slot written (the control).
 *
 * Both only while SAVE NEW CHECK is up (page 53, the second AMP page on a
 * synth track), only when slot 128 is empty (0x40072fa2(storage, 127) == 0;
 * otherwise the request would load it, so it is refused and counted), and at
 * most once a visit: the first event of a turn arms the request, ev_tick (the
 * UI task) makes it, and the encoder is armed again only after the page has
 * been away.
 *
 * Like save-check it keeps one project field, sn_saves, adds 1 to it in every
 * call before a save, and draws along the bottom edge (y counts from the
 * bottom row), two pixels a step from the left:
 *
 *   y 0..1   the calls before a save so far (mod 32)
 *   y 4..5   the calls after a save so far, the same
 *   y 8..9   sn_saves as the project now holds it (mod 32)
 *   y 12..13 the last call's how: its kind (1..3 dots at x 0, 3, 6), the
 *            slot's 8 bits from x 15 (3 pixels apart, most significant
 *            first), a dot at x 45 when it failed, then one dot per I request
 *            (x 52, 55, 58, 61), one per J request (x 70, 73, 76, 79), a dot
 *            at x 100 when a request was refused (slot 128 full), a dot at x
 *            110 after an ev_project CORE_PROJECT_NEW, a dot at x 120 when the
 *            last call was the one before, and at x 126 always (the mod draws).
 *
 * So J on an empty 128 gives three kind dots and slot bits 0111 1111, with
 * sn_saves at 1 (the new project's field, 0, plus the call before), and the
 * x 110 dot; LOAD FROM 128 later shows that 1. I gives its dot and nothing
 * else changes.
 *
 * In RAM after the magic SNC1 (sn), for a readback: the requests made (I, J),
 * refused, the request calls' returns, the calls before and after, the last
 * how, ev_project's count and last how, sn_saves at the last tick, slot 128's
 * state at the last request and the ticks.
 */
#include <stdint.h>
#include "analog-four-mk2/store.h"
#include "analog-four-mk2/pages.h"

typedef void (*fillrect_t)(void *bmp, int x0, int y0, int x1, int y1, int colour);
#define FILLRECT ((fillrect_t)FILLRECT_AT)

/* stock 1.56 */
typedef int (*load_request_t)(int slot, void *callback, int create_if_empty, int ask_save);
#define LOAD_REQUEST ((load_request_t)0x4003f7cc)
#define STORAGE ((void *(*)(void))0x40146ce6)
#define SLOT_FULL ((int (*)(void *, int))0x40072fa2)   /* low byte: 1 when the slot holds a project */

#define SLOT 127                         /* the screen's 128 */
#define ID_I 0x220
#define ID_J 0x221
#define PAGE 53
#define ENC_I 8
#define ENC_J 9
#define ENC_INDEX(ev) (*(const int32_t *)((const uint8_t *)(ev) + 12))

STORE_FIELD(sn_saves);

/* an empty std::function (stock's LOAD FROM passes one: the manager word, +8, 0) */
static uint32_t no_callback[4];

struct {
    char magic[4];
    volatile uint32_t ticks;
    volatile uint32_t req_i, req_j, refused;
    volatile int32_t ret_i, ret_j;
    volatile uint32_t pre, post, how, last_pre;
    volatile uint32_t proj_n, proj_how, proj_new;
    volatile uint32_t saves;
    volatile uint32_t slot_full;         /* slot 128 at the last request: 0 empty, 1 full */
    volatile uint8_t shown, armed_i, armed_j, pending;   /* pending: 1 I, 2 J */
} sn = {.magic = {'S', 'N', 'C', '1'}, .armed_i = 1, .armed_j = 1};

static volatile uint32_t got_at;         /* the tick of the last get for I or J */
static volatile int dirty = 1;

/* ---- the page ---- */

static int p_get(int32_t id, int32_t *v)
{
    if (id != ID_I && id != ID_J)
        return 0;
    got_at = sn.ticks;
    *v = (int32_t)((id == ID_I ? sn.req_i : sn.req_j) & 127) << 8;
    return 1;
}

static int p_delta(void *view, int32_t id, int32_t d, int32_t func)
{
    (void)view;
    (void)d;
    (void)func;
    return id == ID_I || id == ID_J;     /* never reached: ev_enc takes them */
}

const struct core_param sn_params = {ID_I, ID_J, p_get, p_delta, 0};
const struct core_page sn_page = {PAGE, "SAVE NEW CHECK", {0, 0, 0, 0, 0, 0, 0, 0, ID_I, ID_J}};
const struct core_view_page sn_view = {CORE_TRACK_SYNTH, CORE_KEY_AMP, PAGE, 0};

/* ---- encoders I and J while SAVE NEW CHECK is up ---- */

int sn_enc(void *brain, void *ev)
{
    int e = ENC_INDEX(ev);
    (void)brain;
    if (!sn.shown || (e != ENC_I && e != ENC_J))
        return 0;
    if (e == ENC_I && sn.armed_i) {
        sn.armed_i = 0;
        sn.pending = 1;
    } else if (e == ENC_J && sn.armed_j) {
        sn.armed_j = 0;
        sn.pending = 2;
    }
    return 1;                            /* every event of the turn is taken */
}

static void request(int create)
{
    void *st = STORAGE();
    sn.slot_full = (unsigned)SLOT_FULL(st, SLOT) & 255;
    if (sn.slot_full) {
        sn.refused++;
        return;
    }
    if (create) {
        sn.req_j++;
        sn.ret_j = LOAD_REQUEST(SLOT, no_callback, 1, 1);
    } else {
        sn.req_i++;
        sn.ret_i = LOAD_REQUEST(SLOT, no_callback, 0, 1);
    }
}

/* ---- the save and project events ---- */

void sn_save(unsigned pre, unsigned how)
{
    if (pre) {
        sn.pre++;
        core_store_set(STORE(sn_saves), 0, 0, core_store_get(STORE(sn_saves), 0, 0) + 1);
    } else
        sn.post++;
    sn.how = how;
    sn.last_pre = pre;
    dirty = 1;
}

void sn_project(int how, unsigned dropped)
{
    (void)dropped;
    sn.proj_n++;
    sn.proj_how = (uint32_t)how;
    if (how == CORE_PROJECT_NEW)
        sn.proj_new = 1;
    dirty = 1;
}

void sn_tick(void *ctrl)
{
    int p;
    sn.ticks++;
    sn.shown = sn.ticks - got_at <= 8;
    if (!sn.shown) {                     /* away from the page: armed again */
        sn.armed_i = 1;
        sn.armed_j = 1;
    }
    p = sn.pending;
    if (p) {
        sn.pending = 0;
        request(p == 2);
        dirty = 1;
    }
    sn.saves = core_store_get(STORE(sn_saves), 0, 0);
    if (dirty || (sn.ticks & 3) == 0) {  /* redraw: keeps shown fresh */
        dirty = 0;
        *((volatile uint8_t *)ctrl + 0x20) = 1;
    }
}

static void bar(void *bmp, int y, unsigned n)
{
    n &= 31;
    if (n)
        FILLRECT(bmp, 0, y, 2 * (int)n - 1, y + 1, 1);
}

static void dots(void *bmp, int x, unsigned n)
{
    unsigned i;
    for (i = 0; i < n && i < 4; i++)
        FILLRECT(bmp, x + 3 * (int)i, 12, x + 3 * (int)i + 1, 13, 1);
}

void sn_draw(void *bmp, void *ctrl)
{
    unsigned how = sn.how, kind = CORE_SAVE_KIND(how), slot = CORE_SAVE_SLOT(how), i;
    (void)ctrl;
    FILLRECT(bmp, 0, 0, 127, 13, 0);
    bar(bmp, 0, sn.pre);
    bar(bmp, 4, sn.post);
    bar(bmp, 8, core_store_get(STORE(sn_saves), 0, 0));
    dots(bmp, 0, kind < 3 ? kind : 3);
    for (i = 0; i < 8; i++)
        if (slot >> (7 - i) & 1)
            FILLRECT(bmp, 15 + 3 * i, 12, 16 + 3 * i, 13, 1);
    if (how & CORE_SAVE_FAILED)
        FILLRECT(bmp, 45, 12, 46, 13, 1);
    dots(bmp, 52, sn.req_i);
    dots(bmp, 70, sn.req_j);
    if (sn.refused)
        FILLRECT(bmp, 100, 12, 101, 13, 1);
    if (sn.proj_new)
        FILLRECT(bmp, 110, 12, 111, 13, 1);
    if (sn.last_pre && sn.pre)
        FILLRECT(bmp, 120, 12, 121, 13, 1);
    FILLRECT(bmp, 126, 12, 127, 13, 1);
}
