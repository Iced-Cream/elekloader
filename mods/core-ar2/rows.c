/* SPDX-License-Identifier: GPL-2.0-or-later */
/* core-ar2 0.8: mod p-locks as lock rows (README, "Mod p-locks"), the second
 * part of the lock service. Every address is OS 1.74's.
 *
 * A pattern keeps its p-locks in a pool of 72 rows of 66 bytes, [code]
 * [track][64 step bytes] (a step byte with bit 7 set has no lock); stock's
 * codes are 0..41, a mod parameter's is its lock code (42..127, lock.c). The
 * rows are only the stored form: stock edits and plays the live lock store
 * and rebuilds the rows from it (0x40009216), which drops every row it does
 * not know. So core keeps each pattern object's mod rows itself (a shadow,
 * in the same row form, on the heap) and follows stock with it:
 *
 *   - row load (0x40008f98): the object's shadow is the rows with codes
 *     42..127 (stock skips them);
 *   - rebuild (0x40009216): after stock's rows, the shadow's, in code order
 *     then track order, each where one still fits under stock's 71 (stock
 *     skips a pair that does not fit and carries on, and so does core), so
 *     stock's locks always win;
 *   - a copy of a whole pattern object (memcpy of 85,760 bytes: pattern
 *     switch, SAVE and RELOAD PATTERN, copy, paste, undo, transfers) copies
 *     the source's shadow to the target; pattern init clears it;
 *   - the sound parameter set's lock calls (vtable +60 get, +64 set, +68
 *     clear, +72 turn) take a mod parameter's knob (its page id, lock.c)
 *     with a trig held: the lock goes into the shadow and, as stock writes
 *     its own lock edits, into the stored pattern's rows;
 *   - playback (0x4009a954, a trig's locks): core notes the trig's mod
 *     locks per track, which mods read with core_lock_value.
 *
 * The objects are the 129 live patterns and, for anything else a pattern is
 * copied into (clipboard and undo buffers, transfer buffers), a few entries
 * keyed by address. Not yet followed (core-ar2 README): track, page and
 * trig operations, the SysEx dump guard, a mod parameter above 127.
 */

#include <stdint.h>

#define NEW ((unsigned (*)(unsigned))0x40082a40)         /* operator new; d0 */
#define DELETE ((void (*)(void *))0x40082a4c)
#define APP ((unsigned (*)(void))0x4017fa0a)
#define APP_TRACKS ((unsigned (*)(unsigned))0x400ae492)
#define CURRENT_TRACK ((int (*)(unsigned))0x400b693e)
#define CURRENT_PATTERN ((unsigned (*)(unsigned))0x400af0ac)
#define TRACK_REC ((unsigned (*)(unsigned, int))0x400ab29e)
#define TRACK_LENGTH ((int (*)(unsigned))0x400c0524)
#define STOCK_LOCK_GET ((int (*)(void *, int, int))0x400a98b4)
#define STOCK_LOCK_SET ((int (*)(void *, int, int, int))0x400a9926)
#define STOCK_LOCK_CLEAR ((int (*)(void *, int, int))0x400a99c2)
#define STOCK_LOCK_TURN ((int (*)(void *, int, int, int))0x400a8560)

#define LIVE_PATTERNS 0x407c89e3u               /* the live project +24: 129 objects */
#define PATTERN_SIZE 85760u
#define LIVE_LOCKS 0x2e91                       /* the live lock store in an object */
#define TRACK_LOCKS 5674
#define STEP_LOCKS 88
#define STORED_PATTERNS (0x405c8f20u + 0x18)     /* the stored project's 129 slots */
#define STORED_PATTERN 13101
#define STORED_ROWS 0x2091
#define ROWS 72
#define ROW 66
#define MAX_USED 71
#define FIRST_CODE 42
#define LAST_CODE 127
#define CODES (LAST_CODE - FIRST_CODE + 1)
#define BUFS 8

struct shadow {
    int n;
    unsigned char row[][ROW];
};

struct core_lock_param;
const struct core_lock_param *core_lock_by_id(int id, unsigned *code);
unsigned char *core_lock_sound(void *set);
int core_lock_stock_takes(int id, const unsigned char *s);
int core_lock_code(unsigned field);
unsigned core_lock_field(const struct core_lock_param *p);
int core_lock_min(const struct core_lock_param *p);
int core_lock_max(const struct core_lock_param *p);
unsigned core_store_get(unsigned field, void *item, int index);

static struct shadow *live_sh[129];
static struct {
    unsigned obj;
    struct shadow *sh;
} bufs[BUFS];
static unsigned buf_next;

/* the playing trig's mod locks, per track (0xff: none) */
volatile unsigned char core_lock_now[13][CODES];

static unsigned short irq_mask(void)
{
    unsigned short sr;
    __asm__ volatile("move.w %%sr,%0\n\tmove.w #0x2700,%%sr" : "=d"(sr) : : "memory");
    return sr;
}

static void irq_restore(unsigned short sr)
{
    __asm__ volatile("move.w %0,%%sr" : : "d"(sr) : "memory");
}

/* where an object's shadow is kept; a new entry for another address when
 * make (the oldest one's shadow goes) */
static struct shadow **slot(unsigned obj, int make)
{
    unsigned i;
    if (obj >= LIVE_PATTERNS && obj < LIVE_PATTERNS + 129 * PATTERN_SIZE) {
        if ((obj - LIVE_PATTERNS) % PATTERN_SIZE)
            return 0;
        return &live_sh[(obj - LIVE_PATTERNS) / PATTERN_SIZE];
    }
    for (i = 0; i < BUFS; i++)
        if (bufs[i].obj == obj)
            return &bufs[i].sh;
    if (!make)
        return 0;
    i = buf_next++ % BUFS;
    if (bufs[i].sh) {
        DELETE(bufs[i].sh);
        bufs[i].sh = 0;
    }
    bufs[i].obj = obj;
    return &bufs[i].sh;
}

static void put(struct shadow **at, struct shadow *sh)
{
    struct shadow *old;
    unsigned short sr = irq_mask();
    old = *at;
    *at = sh;
    irq_restore(sr);
    if (old)
        DELETE(old);
}

static struct shadow *make(int n)
{
    struct shadow *sh = n ? (struct shadow *)NEW(4 + ROW * n) : 0;
    if (sh)
        sh->n = n;
    return sh;
}

static void copy_bytes(unsigned char *d, const unsigned char *s, unsigned n)
{
    while (n--)
        *d++ = *s++;
}

/* a whole pattern object copied (the memcpy site, lock.s) */
void core_rows_copy(unsigned dst, unsigned src)
{
    struct shadow **from = slot(src, 0), *sh = 0, **to;
    if (from && *from && (sh = make((*from)->n)) != 0)
        copy_bytes(&sh->row[0][0], &(*from)->row[0][0], ROW * sh->n);
    if ((to = slot(dst, sh != 0)) != 0)
        put(to, sh);
    else if (sh)
        DELETE(sh);
}

/* pattern init (st_pat_init) */
void core_rows_clear(unsigned obj)
{
    struct shadow **at = slot(obj, 0);
    if (at)
        put(at, 0);
}

static int mod_code(signed char c)
{
    return c >= FIRST_CODE && c <= LAST_CODE;
}

/* row load (0x40008f98: live lock store, rows), after stock */
void core_rows_load(unsigned char *locks, const unsigned char *rows)
{
    unsigned obj = (unsigned)locks - LIVE_LOCKS;
    int i, n = 0;
    struct shadow *sh, **at;
    if (!locks || !rows)
        return;
    for (i = 0; i < ROWS; i++)
        if (mod_code(rows[ROW * i]) && rows[ROW * i + 1] < 13)
            n++;
    if (!(at = slot(obj, n != 0)))
        return;
    sh = make(n);
    if (n && !sh)
        return;
    for (i = n = 0; i < ROWS; i++)
        if (mod_code(rows[ROW * i]) && rows[ROW * i + 1] < 13)
            copy_bytes(sh->row[n++], rows + ROW * i, ROW);
    put(at, sh);
}

static int used(const unsigned char *rows)
{
    int i, u = 0;
    for (i = 0; i < ROWS; i++)
        if (rows[ROW * i] != 0xff || rows[ROW * i + 1] != 0xff)
            u++;
    return u;
}

/* rebuild (0x40009216: rows, live lock store), after stock: the shadow's
 * rows in code order, then track order, where they fit */
void core_rows_save(unsigned char *rows, unsigned char *locks)
{
    struct shadow **at, *sh;
    int u, code, t, i, k = 0;
    if (!rows || !locks || !(at = slot((unsigned)locks - LIVE_LOCKS, 0)) || !(sh = *at))
        return;
    u = used(rows);
    for (code = FIRST_CODE; code <= LAST_CODE; code++)
        for (t = 0; t < 13; t++)
            for (i = 0; i < sh->n; i++)
                if (sh->row[i][0] == code && sh->row[i][1] == t) {
                    if (u >= MAX_USED)
                        continue;
                    while (k < ROWS && (rows[ROW * k] != 0xff || rows[ROW * k + 1] != 0xff))
                        k++;
                    if (k == ROWS)
                        return;
                    copy_bytes(rows + ROW * k, sh->row[i], ROW);
                    u++;
                }
}

/* ---- playback: the trig's locks (0x4009a954: track, out, the step's
 * locks, ...) ---- */
void core_rows_trig(int track, const unsigned char *step)
{
    unsigned off, obj, t, s, i;
    struct shadow *sh;
    if ((unsigned)track >= 13)
        return;
    for (i = 0; i < CODES; i++)
        core_lock_now[track][i] = 0xff;
    off = (unsigned)step - LIVE_PATTERNS;
    if ((unsigned)step < LIVE_PATTERNS || off >= 129 * PATTERN_SIZE)
        return;
    obj = off / PATTERN_SIZE;
    off = off % PATTERN_SIZE - LIVE_LOCKS;
    if (off >= 13 * TRACK_LOCKS || off % TRACK_LOCKS >= 64 * STEP_LOCKS)
        return;
    t = off / TRACK_LOCKS;
    s = off % TRACK_LOCKS / STEP_LOCKS;
    if (!(sh = live_sh[obj]))
        return;
    for (i = 0; i < (unsigned)sh->n; i++)
        if (sh->row[i][1] == t && !(sh->row[i][2 + s] & 0x80))
            core_lock_now[track][sh->row[i][0] - FIRST_CODE] = sh->row[i][2 + s];
}

int core_lock_value(int track, unsigned field)
{
    int code = core_lock_code(field), v;
    if ((unsigned)track >= 13 || code < FIRST_CODE)
        return -1;
    v = core_lock_now[track][code - FIRST_CODE];
    return v == 0xff ? -1 : v;
}

/* ---- editing: a mod parameter's knob with a trig held ---- */

/* the live pattern and track a lock call works on, found as stock's
 * (0x400a9926 -> 0x400c058c -> 0x400ac6dc): the current pattern's track
 * record, whose +44 answers the lock store (vtable +40) and +64 is the
 * track; the step within the track's length */
static int where(int step, unsigned *obj, int *track)
{
    unsigned app = APP(), rec, pat, locks;
    rec = TRACK_REC(CURRENT_PATTERN(app), CURRENT_TRACK(APP_TRACKS(app)));
    pat = *(unsigned *)(rec + 44);
    *track = *(int *)(rec + 64);
    if ((unsigned)step >= 64 || !pat || (unsigned)*track >= 13 || TRACK_LENGTH(rec) < step)
        return 0;
    locks = (*(unsigned (**)(unsigned))(*(unsigned *)pat + 40))(pat);
    *obj = locks - LIVE_LOCKS;
    return *obj >= LIVE_PATTERNS && *obj < LIVE_PATTERNS + 129 * PATTERN_SIZE &&
           !((*obj - LIVE_PATTERNS) % PATTERN_SIZE);
}

static int find(const struct shadow *sh, int code, int track)
{
    int i;
    for (i = 0; sh && i < sh->n; i++)
        if (sh->row[i][0] == code && sh->row[i][1] == track)
            return i;
    return -1;
}

static int lock_get(unsigned obj, int track, int step, int code)
{
    struct shadow *sh = live_sh[(obj - LIVE_PATTERNS) / PATTERN_SIZE];
    int i = find(sh, code, track);
    return i < 0 || (sh->row[i][2 + step] & 0x80) ? -1 : sh->row[i][2 + step];
}

/* the stored pattern's row for (code, track): an empty one when make and
 * stock's 71 leave room */
static unsigned char *stored_row(unsigned obj, int code, int track, int make)
{
    unsigned char *rows = (unsigned char *)STORED_PATTERNS +
                          STORED_PATTERN * ((obj - LIVE_PATTERNS) / PATTERN_SIZE) + STORED_ROWS;
    int i;
    for (i = 0; i < ROWS; i++)
        if (rows[ROW * i] == code && rows[ROW * i + 1] == track)
            return rows + ROW * i;
    if (!make || used(rows) >= MAX_USED)
        return 0;
    for (i = 0; i < ROWS; i++)
        if (rows[ROW * i] == 0xff && rows[ROW * i + 1] == 0xff) {
            rows[ROW * i] = code;
            rows[ROW * i + 1] = track;
            for (code = 0; code < 64; code++)
                rows[ROW * i + 2 + code] = 0xff;
            return rows + ROW * i;
        }
    return 0;
}

/* v: 0..127, or -1 to clear; 0 when there was no room */
static int lock_put(unsigned obj, int track, int step, int code, int v)
{
    struct shadow **at = &live_sh[(obj - LIVE_PATTERNS) / PATTERN_SIZE], *sh = *at, *nsh;
    unsigned char *row;
    int i = find(sh, code, track), j, any;
    if (i < 0) {
        if (v < 0)
            return 1;
        if (!(row = stored_row(obj, code, track, 1)))
            return 0;           /* stock's budget is full: no lock, as stock */
        if (!(nsh = make((sh ? sh->n : 0) + 1)))
            return 0;
        if (sh)
            copy_bytes(&nsh->row[0][0], &sh->row[0][0], ROW * sh->n);
        i = nsh->n - 1;
        nsh->row[i][0] = code;
        nsh->row[i][1] = track;
        for (j = 0; j < 64; j++)
            nsh->row[i][2 + j] = 0xff;
        put(at, nsh);
        sh = nsh;
    }
    sh->row[i][2 + step] = v < 0 ? 0xff : v;
    if ((row = stored_row(obj, code, track, 0)) != 0)
        row[2 + step] = sh->row[i][2 + step];
    for (any = j = 0; j < 64; j++)
        any |= !(sh->row[i][2 + j] & 0x80);
    if (!any) {                 /* the row's last lock: the row goes */
        if (row)
            for (j = 0; j < ROW; j++)
                row[j] = j < 2 ? 0xff : 0;
        nsh = make(sh->n - 1);
        if (sh->n > 1 && !nsh)
            return 1;
        for (j = any = 0; j < sh->n; j++)
            if (j != i)
                copy_bytes(nsh->row[any++], sh->row[j], ROW);
        put(at, nsh);
    }
    return 1;
}

/* the knob's mod parameter, or 0 for stock's */
static const struct core_lock_param *ours(void *set, int id, unsigned *code)
{
    const struct core_lock_param *p = core_lock_by_id(id, code);
    unsigned char *s;
    if (!p || !(s = core_lock_sound(set)) || core_lock_stock_takes(id, s))
        return 0;
    return p;
}

static int clamp(const struct core_lock_param *p, int v)
{
    int lo = core_lock_min(p), hi = core_lock_max(p);
    if (hi > 127)
        hi = 127;
    return v < lo ? lo : v > hi ? hi : v;
}

/* sound set vtable +60: (set, id, step) -> the lock (8.8) or -1 */
int core_lock_get_vt(void *set, int id, int step)
{
    unsigned code, obj;
    int track;
    if (!ours(set, id, &code))
        return STOCK_LOCK_GET(set, id, step);
    if (!where(step, &obj, &track))
        return -1;
    step = lock_get(obj, track, step, code);
    return step < 0 ? -1 : step << 8;
}

/* +64: (set, id, value 8.8, step) */
int core_lock_set_vt(void *set, int id, int value, int step)
{
    const struct core_lock_param *p;
    unsigned code, obj;
    int track;
    if (!(p = ours(set, id, &code)))
        return STOCK_LOCK_SET(set, id, value, step);
    if (!where(step, &obj, &track))
        return 0;
    return lock_put(obj, track, step, code, clamp(p, value >> 8));
}

/* +68: (set, id, step) */
int core_lock_clear_vt(void *set, int id, int step)
{
    unsigned code, obj;
    int track;
    if (!ours(set, id, &code))
        return STOCK_LOCK_CLEAR(set, id, step);
    if (!where(step, &obj, &track))
        return 0;
    return lock_put(obj, track, step, code, -1);
}

/* +72: (set, id, delta 8.8, step) -> the new lock (8.8): a whole step at
 * least, from the lock or, on the first turn, the sound's value */
int core_lock_turn_vt(void *set, int id, int delta, int step)
{
    const struct core_lock_param *p;
    unsigned code, obj;
    int track, v;
    if (!(p = ours(set, id, &code)))
        return STOCK_LOCK_TURN(set, id, delta, step);
    if (!where(step, &obj, &track))
        return -1;
    if ((v = lock_get(obj, track, step, code)) < 0)
        v = core_store_get(core_lock_field(p), core_lock_sound(set), 0);
    v = clamp(p, v + (delta > 0 ? (delta + 255) >> 8 : delta < 0 ? -((255 - delta) >> 8) : 0));
    lock_put(obj, track, step, code, v);
    return v << 8;
}
