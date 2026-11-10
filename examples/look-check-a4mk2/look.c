/* SPDX-License-Identifier: GPL-2.0-or-later */
/* look-check (Analog Four MKII): core-a4mk2 0.6's knob looks (README).
 *
 * look-check-ar2 with its table found on the unit: at the first ev_tick
 * after stock's first draw (stock builds its UI records at boot, in RAM from
 * 0x41856c44, 84 bytes an id), the mod groups stock's ids 1..0x126 by what
 * makes a knob's look, as gen_looks.py does on the Rytm (the flags at +0,
 * the turn's steps at +4..+0x10, and the two code words of each of the four
 * delegates at +0x14, +0x24, +0x34, +0x44), and takes the first id of each
 * group as its representative L. Own knob k (id OWN + k) gets an entry in
 * core_knob_looks whose look is L: the entries are this mod's, in RAM,
 * filled before any LOOK CHECK draw, and core reads them on each lookup.
 * Two more own knobs follow the representatives: one whose record comes from
 * a ui callback (a copy of the second representative's record) and one with
 * the look of the first representative whose range is 0..127, given a range
 * of 0..12.
 *
 * LOOK CHECK (page 53) sits behind the AMP key on a synth track, after
 * stock's AMP pages. It has four pairs, A|B, C|D, E|F and G|H, on the left
 * L and on the right own knob k, and knob J chooses which four pairs: page
 * p shows pairs 4p..4p+3 (J's text reads P and p, from 0). Both knobs of a
 * pair show the same value (this mod's core_params answers both) with
 * stock's text for L. On the A4 an own id's label and name are record 0's
 * (core has no label call here), so a pair's two cells match in the knob's
 * graphic and value text, not in the label.
 *
 * In RAM after the magic LKA1 (lk): the count of representatives, the first
 * own id, the ticks, the pages, and for each own knob what core's two
 * lookups answer, read when the table is made:
 * {int16 look, int16 ok, u32 uirec, u32 want, int32 range[3], int32
 * want_range[2]}; ok is 1 when uirec and the range are what the entry
 * gives. lk.reps lists the representatives' ids. */
#include <stdint.h>
#include "analog-four-mk2/pages.h"

#define STOCK_IDS 0x127
#define MAXREP (STOCK_IDS - 1)
#define NOWN_MAX (MAXREP + 2)
#define OWN 0x220                           /* the own ids: OWN .. OWN + NOWN_MAX - 1 */
#define ID_PAGE (OWN + NOWN_MAX)            /* knob J: the page */
#define PAGE 53
#define PAIRS_A_PAGE 4
#define RANGE_MIN (0 << 8)
#define RANGE_MAX (12 << 8)

#define UIRECS ((const uint32_t *)0x41856c44)   /* 84 bytes (21 words) an id */
#define UIREC_WORDS 21
#define PARAMS ((const uint8_t *)0x40186af4)    /* 56 bytes an id */
#define P_MIN(id) (*(const int32_t *)(PARAMS + 56 * (id) + 0x08))
#define P_MAX(id) (*(const int32_t *)(PARAMS + 56 * (id) + 0x0c))
#define UIREC ((uintptr_t (*)(int32_t))0x400f6232)   /* answers in %d0 */

/* core_knob_looks: one entry an own knob, filled at the first tick */
struct core_knob_look lk_looks[NOWN_MAX];

static int16_t looks[NOWN_MAX];             /* own knob k's L */
static int32_t value[NOWN_MAX];
static uint8_t set[NOWN_MAX];
static uint8_t own_ui[84];
static volatile int made, draws;
static int32_t page_no;
static int ui_pair = -1, range_pair = -1;

struct {
    char magic[4];
    uint32_t nrep, nown, first;             /* representatives, own knobs, the first own id */
    uint32_t ticks, pages, page, bad;       /* bad: own knobs whose ok is 0 */
    int16_t ui_pair, range_pair;
    int16_t reps[MAXREP];
    struct { int16_t look, ok; uint32_t uirec, want; int32_t range[3], want_range[2]; } k[NOWN_MAX];
} lk = {{'L', 'K', 'A', '1'}};

static int own_ix(int32_t id) { return (uint32_t)(id - OWN) < lk.nown ? id - OWN : -1; }

/* ---- the groups ---- */

static int same_look(const uint32_t *a, const uint32_t *b)
{
    static const uint8_t w[] = {0, 1, 2, 3, 4, 0x14 / 4 + 2, 0x14 / 4 + 3, 0x24 / 4 + 2, 0x24 / 4 + 3,
                                0x34 / 4 + 2, 0x34 / 4 + 3, 0x44 / 4 + 2, 0x44 / 4 + 3};
    unsigned i;
    for (i = 0; i < sizeof w; i++)
        if (a[w[i]] != b[w[i]])
            return 0;
    return 1;
}

static void group(void)
{
    int id, r, n = 0;
    for (id = 1; id < STOCK_IDS; id++) {
        const uint32_t *rec = UIRECS + UIREC_WORDS * id;
        for (r = 0; r < n; r++)
            if (same_look(rec, UIRECS + UIREC_WORDS * lk.reps[r]))
                break;
        if (r == n && n < MAXREP)
            lk.reps[n++] = (int16_t)id;
    }
    lk.nrep = (uint32_t)n;
    for (r = 0; r < n; r++)
        looks[r] = lk.reps[r];
    ui_pair = n;
    looks[n] = n > 1 ? lk.reps[1] : lk.reps[0];
    range_pair = n + 1;
    looks[n + 1] = lk.reps[0];
    for (r = 0; r < n; r++)
        if (P_MIN(lk.reps[r]) == 0 && P_MAX(lk.reps[r]) == 127 << 8) {
            looks[n + 1] = lk.reps[r];
            break;
        }
    lk.nown = (uint32_t)n + 2;
    lk.ui_pair = (int16_t)ui_pair;
    lk.range_pair = (int16_t)range_pair;
    lk.pages = (lk.nown + PAIRS_A_PAGE - 1) / PAIRS_A_PAGE;
}

/* ---- the values ---- */

static int32_t lo(int k) { return k == range_pair ? RANGE_MIN : P_MIN(looks[k]); }
static int32_t hi(int k) { return k == range_pair ? RANGE_MAX : P_MAX(looks[k]); }
static int32_t dial(int32_t v) { return (int32_t)((uint32_t)(v - RANGE_MIN) * (127 << 8) / (RANGE_MAX - RANGE_MIN)); }

/* the pair of a knob on the page shown: own knob k, or its L */
static int pair_of(int32_t id)
{
    int i = own_ix(id), j, k;
    if (i >= 0)
        return i;
    for (j = 0; j < PAIRS_A_PAGE; j++) {
        k = PAIRS_A_PAGE * page_no + j;
        if ((uint32_t)k < lk.nown && looks[k] == id)
            return k;
    }
    return -1;
}

static int p_get(int32_t id, int32_t *v)
{
    int k;
    if (id == ID_PAGE) {
        *v = page_no << 8;
        return 1;
    }
    if (!made || (k = pair_of(id)) < 0)
        return 0;
    if (!set[k]) {
        value[k] = lo(k) + ((hi(k) - lo(k)) * 3 / 8 & ~0xff);
        set[k] = 1;
    }
    *v = value[k];
    if (k == range_pair && own_ix(id) < 0)  /* the stock knob: where core draws the own one */
        *v = dial(*v);
    return 1;
}

static int p_delta(void *view, int32_t id, int32_t d, int32_t func)
{
    int k;
    int32_t v;
    (void)view;
    (void)func;
    if (id == ID_PAGE) {
        if (made) {
            page_no += d > 0 ? 1 : d < 0 ? -1 : 0;
            page_no = page_no < 0 ? 0 : page_no >= (int32_t)lk.pages ? (int32_t)lk.pages - 1 : page_no;
            lk.page = (uint32_t)page_no;
        }
        return 1;
    }
    if (!made || (k = pair_of(id)) < 0)
        return 0;
    p_get(id, &v);
    v = value[k] + d;
    value[k] = v < lo(k) ? lo(k) : v > hi(k) ? hi(k) : v;
    return 1;
}

static void put_num(char *o, unsigned v)
{
    char t[10];
    int n = 0;
    do t[n++] = (char)('0' + v % 10); while ((v /= 10) && n < 10);
    while (n)
        *o++ = t[--n];
    *o = 0;
}

static int p_text(struct core_text *t)
{
    int k;
    if (t->id == ID_PAGE) {
        t->buf[0] = 'P';
        put_num(t->buf + 1, (unsigned)page_no);
        return CORE_TEXT_DONE;
    }
    if (!made || (k = pair_of(t->id)) < 0)
        return CORE_TEXT_PASS;
    if (k == range_pair && own_ix(t->id) >= 0)
        t->value = dial(t->value);
    t->id = looks[k];
    return CORE_TEXT_STOCK;
}

static const void *l_ui(int32_t id)
{
    (void)id;
    return own_ui;
}

/* range(id) into r, as stock calls it: the 12 bytes' place in %a0 */
static void range_of(int32_t id, int32_t *r)
{
    register int32_t *a0 __asm__("a0") = r;
    __asm__ volatile("move.l %1,-(%%sp)\n\tjsr 0x40007798\n\taddq.l #4,%%sp"
                     : "+a"(a0) : "g"(id) : "d0", "d1", "a1", "memory", "cc");
}

static void make(void)
{
    int k, n;
    const uint8_t *s;
    group();
    for (k = 0; k < (int)lk.nrep; k++)
        lk_looks[k] = (struct core_knob_look){OWN + k, OWN + k, looks[k], 0, 0, 0};
    s = core_knob_stock_ui(looks[ui_pair]);
    for (n = 0; n < 84; n++)
        own_ui[n] = s[n];
    lk_looks[ui_pair] = (struct core_knob_look){OWN + ui_pair, OWN + ui_pair, looks[ui_pair], l_ui, 0, 0};
    lk_looks[range_pair] = (struct core_knob_look){OWN + range_pair, OWN + range_pair, looks[range_pair], 0,
                                                   RANGE_MIN, RANGE_MAX};
    lk.first = OWN;
    lk.bad = 0;
    for (k = 0; k < (int)lk.nown; k++) {
        int32_t id = OWN + k;
        lk.k[k].look = looks[k];
        lk.k[k].uirec = UIREC(id);
        lk.k[k].want = k == ui_pair ? (uint32_t)own_ui : UIREC(looks[k]);
        range_of(id, lk.k[k].range);
        lk.k[k].want_range[0] = lo(k);
        lk.k[k].want_range[1] = hi(k);
        lk.k[k].ok = lk.k[k].uirec == lk.k[k].want && lk.k[k].range[0] == lo(k) && lk.k[k].range[1] == hi(k);
        lk.bad += !lk.k[k].ok;
    }
    made = 1;
}

void lk_draw(void *bmp, void *ctrl)
{
    (void)bmp;
    (void)ctrl;
    draws++;
}

void lk_tick(void *ctrl)
{
    (void)ctrl;
    lk.ticks++;
    if (!made && draws)
        make();
}

/* the page, its entry made afresh for the page chosen */
static struct core_page shown = {PAGE, "LOOK CHECK", {0}};

static const struct core_page *l_info(int32_t page)
{
    int j, k;
    (void)page;
    for (j = 0; j < 10; j++)
        shown.ids[j] = 0;
    if (made)
        for (j = 0; j < PAIRS_A_PAGE; j++) {
            k = PAIRS_A_PAGE * page_no + j;
            if ((uint32_t)k < lk.nown) {
                shown.ids[2 * j] = looks[k];
                shown.ids[2 * j + 1] = OWN + k;
            }
        }
    shown.ids[9] = ID_PAGE;
    return &shown;
}

const struct core_page_fn lk_page_fn = {PAGE, l_info};
const struct core_view_page lk_view = {CORE_TRACK_SYNTH, CORE_KEY_AMP, PAGE, 0};
const struct core_param lk_params_own = {OWN, ID_PAGE, p_get, p_delta, p_text};
const struct core_param lk_params_stock = {1, STOCK_IDS - 1, p_get, p_delta, p_text};
