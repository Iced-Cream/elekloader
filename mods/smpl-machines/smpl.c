/* SPDX-License-Identifier: GPL-2.0-or-later */
/* smpl-machines: SMPL machines on the Analog Rytm MKII (README.md;
 * analog-rytm-mk2/smpl.h for the mods that add them).
 *
 *   - which machine a sound runs: the field smpl_choice (0 AUTO, 1 stock,
 *     2 + id a machine), the SRC machine's bind, the voice. The render reads
 *     it from the sound the audio side last put on each track (gate.s, at
 *     0x4011f60a: the kit's sound, or a sound lock's pool sound), and the UI
 *     from the current kit's sounds, cached each UI tick (smpl_tick);
 *   - the SMPL page: core_page_fns for stock's pages 4 and 5 (the title),
 *     core_knob_faces for records 39..46 (label, name, look, range), and
 *     core_params' text for them (the value);
 *   - the choice: a SMPL MACHINE row under MACHINE in the sound setup menu
 *     (gate.s, at the menu's build, 0x400ef184);
 *   - the render (gate.s, the call of the sample layer's trig intake): a
 *     voice whose track runs a machine starts no sample and stops any it
 *     plays, and the machine's engine gets the words. */
#include <stdint.h>
#include "analog-rytm-mk2/pages.h"
#include "analog-rytm-mk2/store.h"
#include "analog-rytm-mk2/moments.h"
#include "analog-rytm-mk2/machines.h"
#include "analog-rytm-mk2/smpl.h"

#define FIRST_REC 39                    /* the SMPL page's records: sound indices 17..24 */
#define LAST_REC 46
#define MAX_MACHINES 30                 /* the pool's ids: 0..29 (choice 2..31) */
#define CHOICE_AUTO 0
#define CHOICE_STOCK 1
#define CHOICE_FIRST 2
#define SOUND_SIZE 152                  /* a live sound */
#define SOUND_MACHINE 104               /* its SRC machine, a byte */
#define KIT_SOUNDS 42                   /* a live kit's first sound */
#define OWNERS ((const volatile uint8_t *)0x40291138)   /* the track each voice plays, by voice */
#define PLAYING ((const volatile int8_t *)0x8000ea00)   /* the SRC machine the audio side plays, by track */
#define WORDS(t) ((const int16_t *)(0x8000f7a8 + 0x54 * (t)))   /* a track's engine words, by sound index */
#define STOCK_PAGE_SAMPLE 4             /* the SAMPLE view's pages: 4, and 5 (the same ids) */
#define STOCK_PAGE_SAMPLE2 5

STORE_FIELD(smpl_choice);

extern const struct smpl_machine *const smpl_machines[];   /* zero-terminated */

/* the voice each track plays on: BD SD RS CP BT LT MT HT CH OH CY CB */
static const uint8_t track_voice[12] = {0, 2, 7, 7, 1, 3, 5, 5, 4, 4, 6, 6};

static volatile uint8_t track_entry[12];         /* the table index + 1 each track runs; 0 stock */
static const unsigned char *volatile applied[12]; /* the sound the audio side last put on each track */
static int n_machines = -1;                       /* -1 until the first tick */
static struct core_knob_face faces[MAX_MACHINES][8];
static struct core_page pages[MAX_MACHINES];
static uint8_t blank_ui[84];
static int blank_ready;

static int count_machines(void)
{
    int n = 0;
    while (n < MAX_MACHINES && smpl_machines[n])
        n++;
    return n;
}

static const struct smpl_machine *bound(int machine, int bind)
{
    int i;
    for (i = 0; i < n_machines; i++)
        if (smpl_machines[i]->bind == bind && smpl_machines[i]->src == machine)
            return smpl_machines[i];
    return 0;
}

static int index_of(const struct smpl_machine *m)
{
    int i;
    for (i = 0; i < n_machines; i++)
        if (smpl_machines[i] == m)
            return i;
    return -1;
}

static int fits(const struct smpl_machine *m, int track)
{
    return m && track >= 0 && track < 12 && (m->voices >> track_voice[track] & 1);
}

static const struct smpl_machine *by_id(int id)
{
    int i;
    for (i = 0; i < n_machines; i++)
        if (smpl_machines[i]->id == id)
            return smpl_machines[i];
    return 0;
}

/* the machine sound s (a live sound item of track t) runs, or 0 for stock */
static const struct smpl_machine *machine_of(const unsigned char *s, int track)
{
    const struct smpl_machine *m;
    unsigned c;
    int src;
    if (!s || n_machines <= 0)
        return 0;
    src = (signed char)s[SOUND_MACHINE];
    if ((m = bound(src, SMPL_REQUIRED)) != 0)
        return fits(m, track) ? m : 0;
    c = core_store_get(STORE(smpl_choice), (void *)s, 0);
    if (c == CHOICE_AUTO)
        m = bound(src, SMPL_DEFAULT);
    else if (c == CHOICE_STOCK)
        m = 0;
    else
        m = by_id((int)c - CHOICE_FIRST);
    return fits(m, track) ? m : 0;
}

/* the track a live sound item is in the current kit, or -1 */
static int track_of(const unsigned char *s)
{
    const unsigned char *kit = core_current_kit_item();
    unsigned o;
    if (!kit || !s || s < kit + KIT_SOUNDS)
        return -1;
    o = (unsigned)(s - kit - KIT_SOUNDS);
    return o % SOUND_SIZE || o / SOUND_SIZE >= 12 ? -1 : (int)(o / SOUND_SIZE);
}

static const struct smpl_machine *current_machine(void)
{
    return machine_of(core_current_sound_item(), core_current_track());
}

static void build(void)
{
    int i, k;
    n_machines = count_machines();
    for (i = 0; i < n_machines; i++) {
        const struct smpl_machine *m = smpl_machines[i];
        pages[i].page = STOCK_PAGE_SAMPLE;
        pages[i].name = m->name;
        for (k = 0; k < 8; k++) {
            const struct smpl_knob *n = &m->knobs[k];
            struct core_knob_face *f = &faces[i][k];
            pages[i].ids[k] = FIRST_REC + k;
            f->label = n->label ? n->label : "";
            f->name = n->name ? n->name : f->label;
            f->look = n->look;
            f->ui = n->label ? 0 : blank_ui;
            f->min = n->min;
            f->max = n->max;
            f->def = n->def;
        }
    }
}

/* ev_tick: the faces once (stock's UI records are built by then), and the
 * track cache the render reads */
void smpl_tick(void)
{
    const unsigned char *kit;
    int t;
    if (n_machines < 0) {
        if (!blank_ready) {                     /* a copy of TUN's UI record that draws nothing */
            const uint8_t *src = core_knob_stock_ui(FIRST_REC);
            for (t = 0; t < 84; t++)
                blank_ui[t] = src[t];
            blank_ui[0] = blank_ui[1] = blank_ui[2] = blank_ui[3] = 0;
            blank_ready = 1;
        }
        build();
    }
    kit = core_current_kit_item();
    for (t = 0; t < 12; t++) {
        const struct smpl_machine *m = kit ? machine_of(kit + KIT_SOUNDS + SOUND_SIZE * t, t) : 0;
        track_entry[t] = (uint8_t)(index_of(m) + 1);
    }
}

/* gate.s, at the entry of the audio side's routine that puts a sound on a
 * track (0x4011f60a: a kit's sound, or a sound lock's pool sound) */
void smpl_apply(const unsigned char *sound, int32_t track)
{
    if ((uint32_t)track < 12)
        applied[track] = sound;
}

/* the machine track t plays now: the sound the audio side put on it, while
 * that sound's SRC machine is the one playing (stock refuses one that doesn't
 * fit the track), else the current kit's */
static int entry_now(unsigned t)
{
    const unsigned char *s = applied[t];
    if (s && n_machines > 0 && (signed char)s[SOUND_MACHINE] == PLAYING[t])
        return index_of(machine_of(s, (int)t)) + 1;
    return track_entry[t];
}

const struct smpl_machine *smpl_track_machine(int32_t track)
{
    int e = (uint32_t)track < 12 ? entry_now((unsigned)track) : 0;
    return e > 0 && e <= n_machines ? smpl_machines[e - 1] : 0;
}

/* gate.s, at the render's call of the sample layer's trig intake: masks[0]
 * the voices that start a sample this block, masks[1] those that stop */
void smpl_gate(uint32_t *masks)
{
    int v;
    for (v = 0; v < 8; v++) {
        unsigned t = OWNERS[v];
        int e = t < 12 ? entry_now(t) : 0;
        const struct smpl_machine *m;
        if (e <= 0 || e > n_machines)
            continue;
        m = smpl_machines[e - 1];
        if (m->engine)
            m->engine(v, (int32_t)t, WORDS(t) + 17, (int32_t)(masks[0] >> v & 1));
        masks[0] &= ~(1u << v);
        masks[1] |= 1u << v;
    }
}

/* ---- the SMPL page ------------------------------------------------------ */

static const struct core_page *smpl_info(int32_t page)
{
    const struct smpl_machine *m = current_machine();
    int i = index_of(m);
    (void)page;
    return i < 0 ? 0 : &pages[i];
}

const struct core_page_fn smpl_page_fn4 = {STOCK_PAGE_SAMPLE, smpl_info};
const struct core_page_fn smpl_page_fn5 = {STOCK_PAGE_SAMPLE2, smpl_info};

static const struct core_knob_face *smpl_face(int32_t id, void *sound)
{
    const struct smpl_machine *m;
    int i;
    if (id < FIRST_REC || id > LAST_REC)
        return 0;
    m = sound ? machine_of(sound, track_of(sound)) : current_machine();
    i = index_of(m);
    return i < 0 ? 0 : &faces[i][id - FIRST_REC];
}

const struct core_knob_faces smpl_faces = {FIRST_REC, LAST_REC, smpl_face};

static int put_num(char *buf, int32_t v)
{
    char tmp[8];
    int n = 0, i = 0;
    if (v < 0) {
        buf[n++] = '-';
        v = -v;
    }
    do {
        tmp[i++] = '0' + v % 10;
        v /= 10;
    } while (v && i < 7);
    while (i)
        buf[n++] = tmp[--i];
    buf[n] = 0;
    return n;
}

static int smpl_text(struct core_text *t)
{
    const struct smpl_machine *m;
    const struct smpl_knob *k;
    if (t->id < FIRST_REC || t->id > LAST_REC || !(m = current_machine()))
        return CORE_TEXT_PASS;
    k = &m->knobs[t->id - FIRST_REC];
    if (!k->label)
        t->buf[0] = 0;
    else if (k->text)
        k->text(t->buf, t->value);
    else
        put_num(t->buf, t->value >> 8);
    return CORE_TEXT_DONE;
}

const struct core_param smpl_params = {FIRST_REC, LAST_REC, 0, 0, smpl_text};

/* ---- the choice: AUTO, STOCK, then the machines the voice may run ------ */

/* the next choice from c by dir (+1 / -1) the track may take, or c */
static unsigned step_choice(unsigned c, int dir, int track)
{
    unsigned n = c;
    for (;;) {
        const struct smpl_machine *m;
        if (dir > 0 ? n >= CHOICE_FIRST + MAX_MACHINES - 1 : n == 0)
            return c;
        n += dir;
        if (n < CHOICE_FIRST)
            return n;
        m = by_id((int)n - CHOICE_FIRST);
        if (m && fits(m, track))
            return n;
    }
}

/* what the sound setup menu shows for the current sound */
static const char *choice_name(void)
{
    unsigned char *s = core_current_sound_item();
    const struct smpl_machine *m;
    unsigned c;
    if (!s)
        return "";
    if ((m = bound((signed char)s[SOUND_MACHINE], SMPL_REQUIRED)) != 0)
        return m->name;
    c = core_store_get(STORE(smpl_choice), s, 0);
    if (c == CHOICE_AUTO) {
        m = bound((signed char)s[SOUND_MACHINE], SMPL_DEFAULT);
        return m && fits(m, core_current_track()) ? m->name : "AUTO";
    }
    if (c == CHOICE_STOCK)
        return "STOCK";
    m = by_id((int)c - CHOICE_FIRST);
    return m ? m->name : "----";
}

static void choice_turn(int delta)
{
    unsigned char *s = core_current_sound_item();
    int t = core_current_track();
    unsigned c;
    if (!s || t < 0 || t >= 12 || !delta || n_machines <= 0)
        return;
    if (bound((signed char)s[SOUND_MACHINE], SMPL_REQUIRED))
        return;                                 /* the SRC machine holds it */
    c = core_store_get(STORE(smpl_choice), s, 0);
    c = step_choice(c, delta > 0 ? 1 : -1, t);
    core_store_set(STORE(smpl_choice), s, 0, c);
}

/* ---- the sound setup menu's row (gate.s) -------------------------------- */

/* stock's menu row (0x54 bytes, built by 0x40072fbc) takes four functors,
 * each {storage[2], manager, invoker}: its label, a press, its value's
 * drawing and a turn. The managers are stock's own for the MACHINE row's
 * functors, which hold nothing. */
struct functor {
    void *data[2];
    void *manager;
    void *invoker;
};

/* operator new; stock returns its pointer in %d0, where our compiler reads a
 * pointer from %a0, so it is declared as returning an integer */
#define NEW ((uint32_t (*)(uint32_t))0x400827c4)
#define ROW_INIT ((void (*)(void *, struct functor *, struct functor *, struct functor *, \
                            struct functor *, int32_t, int32_t))0x40072fbc)
#define DRAW_TEXT ((void (*)(void *, const void *, int32_t, int32_t, int32_t, int32_t, int32_t, \
                             const char *, ...))0x4007154c)
#define MENU_FONT ((const void *)0x4027aa28)
#define FMT_S ((const char *)0x402413d4)                             /* "%s" */

extern char smpl_row_label_s[];                                     /* gate.s */
const char smpl_row_text[] = "SMPL MACHINE";                       /* the row's label */

static void row_draw(void *f, void *row, void *ctx, int32_t x, int32_t y)
{
    (void)f;
    (void)row;
    DRAW_TEXT(ctx, MENU_FONT, x + 0x13, y + 2, 2, 0, 0, FMT_S, choice_name());
}

static void row_turn(void *f, void *row, int32_t delta)
{
    (void)f;
    (void)row;
    choice_turn(delta);
}

/* gate.s, as the sound setup menu is built, after its MACHINE row; returns
 * the row to add in %d0, or 0 */
uint32_t smpl_menu_row(void)
{
    struct functor label = {{0, 0}, (void *)0x400ee470, smpl_row_label_s};
    struct functor press = {{0, 0}, 0, 0};
    struct functor draw = {{0, 0}, (void *)0x400ee4c2, (void *)row_draw};
    struct functor turn = {{0, 0}, (void *)0x400ee514, (void *)row_turn};
    uint32_t row;
    if (count_machines() <= 0 || !(row = NEW(0x54)))      /* the menu is built at boot, before a tick */
        return 0;
    ROW_INIT((void *)row, &label, &press, &draw, &turn, -1, 8);
    return row;
}
