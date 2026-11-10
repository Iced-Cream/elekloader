/* SPDX-License-Identifier: GPL-2.0-or-later */
/* smpl-machines: SMPL machines on the Analog Rytm MKII (README.md;
 * analog-rytm-mk2/smpl.h for the mods that add them).
 *
 *   - which machine a sound runs: the field smpl_choice (0 AUTO, 1 stock,
 *     2 + id a machine), the SRC machine's bind, the voice; cached per track
 *     for the current kit each UI tick (smpl_tick), which the render reads;
 *   - the SMPL page: core_page_fns for stock's pages 4 and 5 (the title),
 *     core_knob_faces for records 39..46 (label, name, look, range), and
 *     core_params' text for them (the value);
 *   - the SMPL MACHINE page: one knob of the mod's own (CORE_KNOB(0)) on a
 *     page of its own behind the SAMPLE key;
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
#define WORDS(t) ((const int16_t *)(0x8000f7a8 + 0x54 * (t)))   /* a track's engine words, by sound index */
#define STOCK_PAGE_SAMPLE 4             /* the SAMPLE view's pages: 4, and 5 (the same ids) */
#define STOCK_PAGE_SAMPLE2 5

STORE_FIELD(smpl_choice);
CORE_KNOBS;
CORE_PAGES;

extern const struct smpl_machine *const smpl_machines[];   /* zero-terminated */

/* the voice each track plays on: BD SD RS CP BT LT MT HT CH OH CY CB */
static const uint8_t track_voice[12] = {0, 2, 7, 7, 1, 3, 5, 5, 4, 4, 6, 6};

static volatile uint8_t track_entry[12];         /* the table index + 1 each track runs; 0 stock */
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

const struct smpl_machine *smpl_track_machine(int32_t track)
{
    unsigned e = (unsigned)track < 12 ? track_entry[track] : 0;
    return e && (int)e <= n_machines ? smpl_machines[e - 1] : 0;
}

/* gate.s, at the render's call of the sample layer's trig intake: masks[0]
 * the voices that start a sample this block, masks[1] those that stop */
void smpl_gate(uint32_t *masks)
{
    int v;
    for (v = 0; v < 8; v++) {
        unsigned t = OWNERS[v];
        unsigned e = t < 12 ? track_entry[t] : 0;
        const struct smpl_machine *m;
        if (!e || (int)e > n_machines)
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

/* ---- the SMPL MACHINE page: AUTO, STCK, then the machines the voice may run */

static int choice_get(int32_t id, int32_t *value)
{
    unsigned char *s = core_current_sound_item();
    (void)id;
    *value = s ? (int32_t)core_store_get(STORE(smpl_choice), s, 0) << 8 : 0;
    return 1;
}

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

static int choice_delta(void *view, int32_t id, int32_t delta, int32_t func)
{
    unsigned char *s = core_current_sound_item();
    int t = core_current_track();
    unsigned c;
    (void)view;
    (void)id;
    (void)func;
    if (!s || t < 0 || t >= 12 || !delta || n_machines < 0)
        return 1;
    if (bound((signed char)s[SOUND_MACHINE], SMPL_REQUIRED))
        return 1;                               /* the SRC machine holds it */
    c = core_store_get(STORE(smpl_choice), s, 0);
    c = step_choice(c, delta > 0 ? 1 : -1, t);
    core_store_set(STORE(smpl_choice), s, 0, c);
    return 1;
}

static void put4(char *buf, const char *s)
{
    int i;
    for (i = 0; i < 4 && s && s[i]; i++)
        buf[i] = s[i];
    buf[i] = 0;
}

static int choice_text(struct core_text *t)
{
    unsigned char *s = core_current_sound_item();
    const struct smpl_machine *m;
    unsigned c;
    if (!s) {
        t->buf[0] = 0;
        return CORE_TEXT_DONE;
    }
    m = bound((signed char)s[SOUND_MACHINE], SMPL_REQUIRED);
    c = (unsigned)t->value >> 8;
    if (m)
        put4(t->buf, m->label);
    else if (c == CHOICE_AUTO)
        put4(t->buf, "AUTO");
    else if (c == CHOICE_STOCK)
        put4(t->buf, "STCK");
    else if ((m = by_id((int)c - CHOICE_FIRST)) != 0)
        put4(t->buf, m->label);
    else
        put4(t->buf, "----");
    return CORE_TEXT_DONE;
}

const struct core_param smpl_choice_param = {CORE_KNOB(0), CORE_KNOB(0), choice_get, choice_delta, choice_text};

static const char *choice_label(int32_t id)
{
    (void)id;
    return "MACH";
}

static const char *choice_name(int32_t id)
{
    (void)id;
    return "SMPL Machine";
}

const struct core_param_lock smpl_choice_lock = {CORE_KNOB(0), CORE_KNOB(0), 0, 0, 0, 0, choice_label, choice_name};

/* SMP's select box, on 0..31 */
const struct core_knob_look smpl_choice_look = {CORE_KNOB(0), CORE_KNOB(0), FIRST_REC + 2, 0, 0, 31 << 8};

const struct core_page smpl_choice_page = {CORE_PAGE(0), "SMPL MACHINE", {CORE_KNOB(0), 0, 0, 0, 0, 0, 0, 0}};

static int32_t choice_shown(void *view)
{
    (void)view;
    return n_machines > 0;
}

const struct core_view_page smpl_choice_view = {CORE_VIEW_SAMPLE, CORE_PAGE(0), choice_shown};
