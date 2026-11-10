/* SPDX-License-Identifier: GPL-2.0-or-later */
/* mark-check: core-a4mk2 0.6's modified mark and field defaults on the
 * Analog Four MKII (OS 1.56). README says what a plan reads.
 *
 * MARK CHECK (page 52) sits behind the AMP key on a synth track, after
 * stock's AMP pages. Its knobs:
 *   A  mk_a, a project field (0..127, init 64) through core_params: a turn
 *      core takes for a mod's page knob, so core marks the project modified;
 *   I  mk_i, a project field: this mod's ev_enc takes encoder I's turns while
 *      MARK CHECK is up and adds 1 to mk_i for each, without
 *      core_project_edited: the mark must stay as it was;
 *   J  mk_j, the same for encoder J, then core_project_edited: the mark must
 *      come on.
 * Knobs I and J only show the fields; their turns never reach stock.
 *
 * Every field is declared with an "init" (mod.json), so each reads its init
 * in a new project. Every 20 UI ticks and after each ev_project the mod
 * counts, per scope, the items whose field does not read its init: the 768
 * pattern tracks, 128 patterns, 1024 kit sounds (both kit arrays), 128 pool
 * sounds, 256 kits, 4 global slots and the project's three fields; and the
 * raw stored bits of mk_a (the init XORed out of a read, so 0 where it reads
 * its init).
 *
 * In RAM after the magic MKC1 (mk), for a readback: the mark byte
 * (0x4045c8c2) at each tick, the counts, the three project fields, the
 * encoder events taken, whether MARK CHECK is up, ev_project's count and
 * last how. On the screen, along the bottom edge (y counts from the bottom
 * row): a 5x5 square at the right end while the mark is set; at y 0..1 a bar
 * two pixels a step for the items not reading their init (all scopes, up to
 * 60); at y 4..5 a dot per scope with any such item (track, pattern, kit
 * sound, pool sound, kit, global, project, 4 pixels apart).
 */
#include <stdint.h>
#include "analog-four-mk2/store.h"
#include "analog-four-mk2/pages.h"

typedef void (*fillrect_t)(void *bmp, int x0, int y0, int x1, int y1, int colour);
#define FILLRECT ((fillrect_t)FILLRECT_AT)

#define MARK (*(volatile const uint8_t *)0x4045c8c2)   /* stock's modified mark */

#define LIVE 0x40653c4c                  /* the live project (store-check-a4mk2) */
#define LIVE_PAT(p) ((void *)(LIVE + 0x10 + 0x16fda * (p)))   /* 1..128 */
#define LIVE_KIT(k) ((unsigned char *)(LIVE + 0xb95cea + 0x960 * ((k) & 127) + ((k) >> 7) * 0x4b000))
#define LIVE_KIT_SOUND(k, s) ((void *)(LIVE_KIT(k) + 0x1c + 0x152 * (s)))
#define LIVE_POOL(i) ((void *)(LIVE + 0xc4bd4e + 0x152 * (i)))

STORE_FIELD(mk_t);
STORE_FIELD(mk_p);
STORE_FIELD(mk_s);
STORE_FIELD(mk_k);
STORE_FIELD(mk_g);
STORE_FIELD(mk_a);
STORE_FIELD(mk_i);
STORE_FIELD(mk_j);

/* the inits mod.json declares */
#define INIT_T 21
#define INIT_P 300
#define INIT_S 100
#define INIT_K 3000
#define INIT_G 0x3c
#define INIT_A 64
#define INIT_I 7
#define INIT_J 0xbeef

#define ID_A 0x210
#define ID_I 0x211
#define ID_J 0x212
#define PAGE 52
#define ENC_I 8
#define ENC_J 9
#define ENC_INDEX(ev) (*(const int32_t *)((const uint8_t *)(ev) + 12))

enum { S_TRACK, S_PATTERN, S_SOUND, S_POOL, S_KIT, S_GLOBAL, S_PROJECT, SCOPES };

struct {
    char magic[4];
    uint32_t ticks;
    uint8_t mark, shown, pad[2];
    uint32_t marked_ticks;               /* ticks the mark was seen set */
    uint32_t not_init[SCOPES];           /* items whose field doesn't read its init */
    uint32_t walks;
    uint32_t a, i, j;                    /* the project fields as read */
    uint32_t a_raw;                      /* mk_a's bits: a read XOR its init */
    uint32_t enc_i, enc_j;               /* encoder events taken */
    uint32_t edited_calls;               /* core_project_edited calls made */
    uint32_t proj_n, proj_how, proj_dropped;
} mk = {{'M', 'K', 'C', '1'}};

static volatile uint32_t got_at;         /* the tick of the last get for I or J */
static volatile int walk_now = 1;

/* ---- the page ---- */

static int p_get(int32_t id, int32_t *v)
{
    unsigned f = id == ID_A ? STORE(mk_a) : id == ID_I ? STORE(mk_i) : id == ID_J ? STORE(mk_j) : 0;
    if (!f)
        return 0;
    if (id != ID_A)
        got_at = mk.ticks;
    *v = (int32_t)(core_store_get(f, 0, 0) & (id == ID_A ? 127 : 0xffff)) << 8;
    if (id != ID_A)                      /* I and J show their count, mod 128 */
        *v &= 127 << 8;
    return 1;
}

static int p_delta(void *view, int32_t id, int32_t d, int32_t func)
{
    int32_t v;
    (void)view;
    (void)func;
    if (id != ID_A)
        return id == ID_I || id == ID_J; /* never reached: ev_enc takes them */
    v = (int32_t)core_store_get(STORE(mk_a), 0, 0) + (d >> 8);
    core_store_set(STORE(mk_a), 0, 0, (unsigned)(v < 0 ? 0 : v > 127 ? 127 : v));
    return 1;                            /* core marks the project */
}

const struct core_param mk_params = {ID_A, ID_J, p_get, p_delta, 0};
const struct core_page mk_page = {PAGE, "MARK CHECK", {ID_A, 0, 0, 0, 0, 0, 0, 0, ID_I, ID_J}};
const struct core_view_page mk_view = {CORE_TRACK_SYNTH, CORE_KEY_AMP, PAGE, 0};

/* ---- encoders I and J while MARK CHECK is up ---- */

int mk_enc(void *brain, void *ev)
{
    int e = ENC_INDEX(ev);
    (void)brain;
    if (!mk.shown || (e != ENC_I && e != ENC_J))
        return 0;
    if (e == ENC_I) {
        mk.enc_i++;
        core_store_set(STORE(mk_i), 0, 0, core_store_get(STORE(mk_i), 0, 0) + 1);
    } else {
        mk.enc_j++;
        core_store_set(STORE(mk_j), 0, 0, core_store_get(STORE(mk_j), 0, 0) + 1);
        core_project_edited();
        mk.edited_calls++;
    }
    return 1;
}

/* ---- the defaults walk ---- */

static unsigned bad(unsigned f, void *item, int index, unsigned init)
{
    return core_store_get(f, item, index) != init;
}

static void walk(void)
{
    int a, b;
    uint32_t n[SCOPES] = {0};
    for (a = 1; a < 129; a++) {
        for (b = 0; b < 6; b++)
            n[S_TRACK] += bad(STORE(mk_t), LIVE_PAT(a), b, INIT_T);
        n[S_PATTERN] += bad(STORE(mk_p), LIVE_PAT(a), 0, INIT_P);
    }
    for (a = 0; a < 256; a++) {
        for (b = 0; b < 4; b++)
            n[S_SOUND] += bad(STORE(mk_s), LIVE_KIT_SOUND(a, b), 0, INIT_S);
        n[S_KIT] += bad(STORE(mk_k), LIVE_KIT(a), 0, INIT_K);
    }
    for (a = 0; a < 128; a++)
        n[S_POOL] += bad(STORE(mk_s), LIVE_POOL(a), 0, INIT_S);
    for (a = 0; a < 4; a++)
        n[S_GLOBAL] += bad(STORE(mk_g), 0, a, INIT_G);
    n[S_PROJECT] = bad(STORE(mk_a), 0, 0, INIT_A) + bad(STORE(mk_i), 0, 0, INIT_I) +
                   bad(STORE(mk_j), 0, 0, INIT_J);
    for (a = 0; a < SCOPES; a++)
        mk.not_init[a] = n[a];
    mk.walks++;
}

void mk_project(int how, unsigned dropped)
{
    mk.proj_n++;
    mk.proj_how = (uint32_t)how;
    mk.proj_dropped = dropped;
    walk_now = 1;
}

void mk_tick(void *ctrl)
{
    mk.ticks++;
    mk.mark = MARK;
    if (mk.mark)
        mk.marked_ticks++;
    mk.shown = mk.ticks - got_at <= 8;
    mk.a = core_store_get(STORE(mk_a), 0, 0);
    mk.i = core_store_get(STORE(mk_i), 0, 0);
    mk.j = core_store_get(STORE(mk_j), 0, 0);
    mk.a_raw = mk.a ^ INIT_A;
    if (walk_now || mk.ticks % 20 == 0) {
        walk_now = 0;
        walk();
    }
    if ((mk.ticks & 3) == 0)
        *((volatile uint8_t *)ctrl + 0x20) = 1;   /* redraw: keeps shown fresh */
}

void mk_draw(void *bmp, void *ctrl)
{
    uint32_t total = 0;
    int s;
    (void)ctrl;
    FILLRECT(bmp, 0, 0, 127, 5, 0);
    for (s = 0; s < SCOPES; s++) {
        total += mk.not_init[s];
        if (mk.not_init[s])
            FILLRECT(bmp, 4 * s, 4, 4 * s + 1, 5, 1);
    }
    if (total > 60)
        total = 60;
    if (total)
        FILLRECT(bmp, 0, 0, 2 * (int)total - 1, 1, 1);
    if (mk.mark)
        FILLRECT(bmp, 123, 0, 127, 4, 1);
}
