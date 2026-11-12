/* SPDX-License-Identifier: GPL-2.0-or-later */
/* savenew-check 0.3: core-a4mk2 0.6's CORE_SAVE_NEW on the Analog Four MKII
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
 *
 * Encoder J on SAVE NEW CHECK (page 53, the second AMP page on a synth track)
 * makes the same request with create_if_empty 1 and ask_save 1, for slot 128
 * (CORE_SAVE_NEW, slot 127). 0.3 makes it from the page's knob handler
 * (p_delta), which stock's view dispatch 0x40062cca reaches with the
 * view-manager mutex (vm+4) held, as it reaches LOAD FROM: stock's own context
 * for the request. ev_enc only counts J's events and passes them on. 0.1 and
 * 0.2 made it from ev_tick, with no lock, and had an encoder I control
 * (create_if_empty 0); T277 froze on the first I turn, so 0.3 has no I.
 *
 * The request is made only when slot 128 is empty, read without the storage
 * lock as the byte 0x40072fa2 reads under it (storage + 0xc + slot * 0x18; one
 * byte can't tear), and at most once a visit to the page: the first J event
 * arms nothing more until the page has been away.
 *
 * Breadcrumbs (SNC1, below) say where a request stopped if it never returns:
 * stage 0 p_delta's entry for the request, 1 before the slot read, 2 after it,
 * 3 before 0x4003f7cc, 4 after it, 5 at the next draw; the nesting of sn_tick
 * (the WAIT push 0x4006293a re-enters every ev_tick handler) and how many
 * ticks ran inside the request; the job queue's byte +0x1c at the request
 * (0x4001e99c: 0 = busy, then 0x4003f7cc pushes the WAIT box); the task that
 * made it ([0x4287c5a4]) and the owner of vm+4 then; the return address of
 * 0x40062cca's handler call found on the stack, and the vm in that frame; the
 * storage and job queue singletons ([0x418717a8], [0x418717b8]), whose mutexes
 * (storage+0x24e78, queue+4) a plan reads with vm+4 and the heap lock
 * 0x417df8c8 when a turn times out. A mutex (0x40001cbc) is {count, owner
 * task, first waiter, last waiter}.
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
 *            first), a dot at x 45 when it failed; a dot at x 52 when the
 *            request's p_delta ran inside 0x40062cca (its handler call's
 *            return address on the stack) and at x 55 when its task held the
 *            dispatch's vm+4 there; one dot per J request (x 70, 73, 76, 79),
 *            a dot at x 100 when a request was refused (slot 128 full), a dot
 *            at x 110 after an ev_project CORE_PROJECT_NEW since the last
 *            request made, a dot at x 120 when the last call was the one
 *            before, and at x 126 always (the mod draws).
 *
 * Nothing is drawn while a whole-screen stock menu is up (GLOBAL, PROJECT,
 * MANAGE PROJECTS: core_stock_overlay's CORE_OVERLAY_SCREEN), so a plan reads
 * stock's slot list as stock draws it.
 *
 * So J on an empty 128 gives the x 52 and x 55 dots, three kind dots and slot
 * bits 0111 1111, with sn_saves at 1 (the new project's field, 0, plus the
 * call before), and the x 110 dot; LOAD FROM 128 later shows that 1.
 */
#include <stdint.h>
#include "analog-four-mk2/store.h"
#include "analog-four-mk2/pages.h"
#include "analog-four-mk2/screen.h"

typedef void (*fillrect_t)(void *bmp, int x0, int y0, int x1, int y1, int colour);
#define FILLRECT ((fillrect_t)FILLRECT_AT)

/* stock 1.56 */
typedef int (*load_request_t)(int slot, void *callback, int create_if_empty, int ask_save);
#define LOAD_REQUEST ((load_request_t)0x4003f7cc)
#define STORAGE ((uint8_t *(*)(void))0x40146ce6)
#define STORAGE_PTR (*(uint8_t *const volatile *)0x418717a8)   /* 0x40146ce6's singleton */
#define QUEUE_PTR (*(uint8_t *const volatile *)0x418717b8)     /* 0x4014e852's, the job queue */
#define CUR_TASK (*(const volatile uint32_t *)0x4287c5a4)      /* the running task (0x40001cbc) */
#define SLOT_STATE 0xc                   /* storage + 0xc + slot * 0x18: 1 holds a project */
#define SLOT_SIZE 0x18
#define QUEUE_IDLE 0x1c                  /* 0x4001e99c: 0 while the worker is busy */
#define VM_OWNER 8                       /* vm+4 is the dispatch's mutex; +4 in it the owner */
#define DISPATCH_A 0x40062d3eu           /* 0x40062cca's handler calls: jsr (a0) with (a4, d3) */
#define DISPATCH_B 0x40062d54u           /*   with (a5, d3) */
#define DISPATCH_C 0x40062d5cu           /*   0x40061b48(d3) */
#define SCAN_WORDS 256

#define SLOT 127                         /* the screen's 128 */
#define ID_J 0x221
#define PAGE 53
#define ENC_J 9
#define ENC_INDEX(ev) (*(const int32_t *)((const uint8_t *)(ev) + 12))

STORE_FIELD(sn_saves);

/* an empty std::function (stock's LOAD FROM passes one: the manager word, +8, 0) */
static uint32_t no_callback[4];

struct {
    char magic[4];                       /* +0x00 SNC1 */
    volatile uint32_t ticks;             /* +0x04 */
    volatile uint32_t req_j, refused;    /* +0x08 +0x0c */
    volatile int32_t ret_j;              /* +0x10 */
    volatile uint32_t pre, post, how, last_pre;          /* +0x14 .. +0x20 */
    volatile uint32_t proj_n, proj_how, proj_new;        /* +0x24 .. +0x2c */
    volatile uint32_t saves;             /* +0x30 sn_saves at the last tick */
    volatile uint32_t slot_full;         /* +0x34 slot 128 at the last request: 0 empty, 1 full */
    /* 0.3 */
    volatile uint32_t enc_j;             /* +0x38 J events ev_enc saw on the page */
    volatile uint32_t delta_j;           /* +0x3c p_delta calls for J */
    volatile uint32_t stage;             /* +0x40 0..5, 0xff before the first request */
    volatile uint32_t depth, depth_max;  /* +0x44 +0x48 sn_tick's nesting now, deepest */
    volatile uint32_t in_req;            /* +0x4c 1 while p_delta's request runs */
    volatile uint32_t req_ticks;         /* +0x50 sn_tick calls inside the request (re-entries) */
    volatile uint32_t busy;              /* +0x54 the queue's +0x1c byte at the request, 0xff none */
    volatile uint32_t ctrl;              /* +0x58 ev_tick's ctrl (the view manager, vm) */
    volatile uint32_t storage, queue;    /* +0x5c +0x60 the singletons */
    volatile uint32_t task;              /* +0x64 [0x4287c5a4] in p_delta */
    volatile uint32_t vm_owner;          /* +0x68 [ctrl+8] in p_delta */
    volatile uint32_t disp_ret;          /* +0x6c 0x40062cca's return address found, 0 none */
    volatile uint32_t disp_vm;           /* +0x70 the vm in that frame */
    volatile uint32_t disp_depth;        /* +0x74 the words between p_delta and that frame */
    volatile uint8_t shown, armed_j;     /* +0x78 */
} sn = {.magic = {'S', 'N', 'C', '1'}, .stage = 0xff, .busy = 0xff, .armed_j = 1};

static volatile uint32_t got_at;         /* the tick of the last get for J */
static volatile int dirty = 1;

/* ---- the request (UI task, inside the dispatch's p_delta) ---- */

/* 0x40062cca's handler call on the stack: its frame is 0x30 bytes of saved
 * registers under its return address and vm (its first argument); a/b push
 * two words, c one, before the call. */
static void find_dispatch(void)
{
    uint32_t here = 0;
    const volatile uint32_t *sp = (const volatile uint32_t *)&here;
    unsigned i;
    for (i = 0; i < SCAN_WORDS; i++) {
        uint32_t w = sp[i];
        if (w == DISPATCH_A || w == DISPATCH_B || w == DISPATCH_C) {
            sn.disp_ret = w;
            sn.disp_vm = sp[i + (w == DISPATCH_C ? 15 : 16)];
            sn.disp_depth = i;
            return;
        }
    }
    sn.disp_ret = 0;
}

static void request(void)
{
    uint8_t *st, *q;
    sn.stage = 1;
    st = STORAGE_PTR;
    if (!st)
        st = STORAGE();
    q = QUEUE_PTR;
    sn.storage = (uint32_t)st;
    sn.queue = (uint32_t)q;
    sn.slot_full = st[SLOT_STATE + SLOT * SLOT_SIZE];
    sn.stage = 2;
    if (sn.slot_full) {
        sn.refused++;
        return;
    }
    sn.proj_new = 0;                     /* the x 110 dot means this request's new project */
    sn.busy = q ? q[QUEUE_IDLE] : 0xff;
    sn.req_j++;
    sn.stage = 3;
    sn.ret_j = LOAD_REQUEST(SLOT, no_callback, 1, 1);
    sn.stage = 4;
}

/* ---- the page ---- */

static int p_get(int32_t id, int32_t *v)
{
    if (id != ID_J)
        return 0;
    got_at = sn.ticks;
    *v = (int32_t)(sn.req_j & 127) << 8;
    return 1;
}

static int p_delta(void *view, int32_t id, int32_t d, int32_t func)
{
    (void)view;
    (void)d;
    (void)func;
    if (id != ID_J)
        return 0;
    sn.delta_j++;
    if (!sn.armed_j)
        return 1;                        /* the rest of the turn: taken, nothing more */
    sn.armed_j = 0;
    sn.stage = 0;
    sn.task = CUR_TASK;
    sn.vm_owner = sn.ctrl ? *(const volatile uint32_t *)(sn.ctrl + VM_OWNER) : 0;
    find_dispatch();
    sn.in_req = 1;
    request();
    sn.in_req = 0;
    dirty = 1;
    return 1;
}

const struct core_param sn_params = {ID_J, ID_J, p_get, p_delta, 0};
const struct core_page sn_page = {PAGE, "SAVE NEW CHECK", {0, 0, 0, 0, 0, 0, 0, 0, 0, ID_J}};
const struct core_view_page sn_view = {CORE_TRACK_SYNTH, CORE_KEY_AMP, PAGE, 0};

/* ---- encoder J: counted, passed on to stock's dispatch ---- */

int sn_enc(void *brain, void *ev)
{
    (void)brain;
    if (sn.shown && ENC_INDEX(ev) == ENC_J)
        sn.enc_j++;
    return 0;
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
    uint32_t dp = ++sn.depth;
    if (dp > sn.depth_max)
        sn.depth_max = dp;
    if (sn.in_req)
        sn.req_ticks++;
    sn.ctrl = (uint32_t)ctrl;
    sn.ticks++;
    sn.shown = sn.ticks - got_at <= 8;
    if (!sn.shown)                       /* away from the page: armed again */
        sn.armed_j = 1;
    sn.saves = core_store_get(STORE(sn_saves), 0, 0);
    if (dirty || (sn.ticks & 3) == 0) {  /* redraw: keeps shown fresh */
        dirty = 0;
        *((volatile uint8_t *)ctrl + 0x20) = 1;
    }
    sn.depth = dp - 1;
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
    if (sn.stage == 4)
        sn.stage = 5;
    if (core_stock_overlay() & CORE_OVERLAY_SCREEN)
        return;                          /* GLOBAL's menus (MANAGE PROJECTS) stay stock's */
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
    if (sn.disp_ret)
        FILLRECT(bmp, 52, 12, 53, 13, 1);
    if (sn.disp_ret && sn.task && sn.vm_owner == sn.task && sn.disp_vm == sn.ctrl)
        FILLRECT(bmp, 55, 12, 56, 13, 1);
    dots(bmp, 70, sn.req_j);
    if (sn.refused)
        FILLRECT(bmp, 100, 12, 101, 13, 1);
    if (sn.proj_new)
        FILLRECT(bmp, 110, 12, 111, 13, 1);
    if (sn.last_pre && sn.pre)
        FILLRECT(bmp, 120, 12, 121, 13, 1);
    FILLRECT(bmp, 126, 12, 127, 13, 1);
}
