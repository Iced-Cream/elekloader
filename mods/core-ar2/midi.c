/* SPDX-License-Identifier: GPL-2.0-or-later */
/* core-ar2 0.8: mod parameters on MIDI, the fourth part of the lock service
 * (README, "Mod parameters on MIDI"). Every address is OS 1.74's.
 *
 * Stock's NRPNs for a track's sound use MSBs 0..3. A mod parameter is NRPN
 * MSB 4, LSB its lock code - 42, on the track's channel, its value in the
 * data entry MSB (0..127; the LSB is ignored). Stock leaves 33 CCs on a
 * track's channel unused (none of its tables maps them, and they are not the
 * performances' CCs, 0x401de1f4); the first 33 codes, 42..74, get them in
 * order (CC_POOL), so a parameter's CC, like its NRPN, follows its code:
 *
 *   - in: the track's NRPN in (0x400b2b26) and CC in (0x400b0544) give a mod
 *     parameter's to core (midi.s), which sets the parameter in the track's
 *     sound of the current kit, clamped to its range, as its knob does;
 *   - out: when a mod's knob turns a parameter (core_param_delta, pages.s),
 *     core sends it as stock's parameter out (0x400a9754) sends a stock one:
 *     when PARAM OUTPUT is on, as an NRPN or a CC as it is set (a parameter
 *     with no CC sends none), on the channel stock uses.
 */

#define MSB 4
#define FIRST_CODE 42
#define LIVE_KITS 0x412558e3u
#define LIVE_KIT_SIZE 2531
#define WORKING_KIT_INDEX ((const volatile unsigned char *)0x405c8f20 + 0x19d3f7)
#define APP ((unsigned (*)(void))0x4017fa0a)
#define MIDI_CONFIG ((unsigned (*)(unsigned))0x400ae49e)         /* (app): MIDI CONFIG */
#define PARAM_OUT ((int (*)(unsigned))0x400b56c0)                /* PARAM OUTPUT on */
#define PARAM_OUT_KIND ((int (*)(unsigned))0x400b563c)           /* 0 NRPN, 1 CC */
#define OUT_CHANNEL ((int (*)(void *))0x400a9844)                /* reads no argument */
#define SEND_NRPN ((void (*)(int, int, int, int))0x40085962)     /* (channel, nrpn, value, 1) */
#define SEND_CC ((void (*)(int, int, int))0x4008591a)            /* (channel, cc, value) */

static const unsigned char CC_POOL[] = {
    8, 9, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62,
    63, 65, 66, 67, 68, 69, 84, 85, 110, 111, 112, 113, 114, 115, 116, 119,
};
#define CCS (sizeof CC_POOL)

struct core_lock_param;
const struct core_lock_param *core_lock_by_code(unsigned code);
const struct core_lock_param *core_lock_knob(void *view, int id, int *track, unsigned *code,
                                             unsigned char **s);
unsigned core_lock_field(const struct core_lock_param *p);
int core_lock_min(const struct core_lock_param *p);
int core_lock_max(const struct core_lock_param *p);
unsigned core_store_get(unsigned field, void *item, int index);
void core_store_set(unsigned field, void *item, int index, unsigned value);

/* the parameter of a code in the track's sound of the current kit: a 7-bit
 * value, clamped to its range */
static void set(unsigned code, int value, int track)
{
    const struct core_lock_param *p = core_lock_by_code(code);
    unsigned k = (unsigned)WORKING_KIT_INDEX[0] << 24 | (unsigned)WORKING_KIT_INDEX[1] << 16 |
                 (unsigned)WORKING_KIT_INDEX[2] << 8 | WORKING_KIT_INDEX[3];
    int hi;
    if (!p || k >= 128)
        return;
    hi = core_lock_max(p) > 127 ? 127 : core_lock_max(p);
    value = value < core_lock_min(p) ? core_lock_min(p) : value > hi ? hi : value;
    core_store_set(core_lock_field(p),
                   (unsigned char *)LIVE_KITS + LIVE_KIT_SIZE * k + 42 + 152 * track, 0, value);
}

/* 0x400b2b26 (midi.s): a track's NRPN (MSB << 7 | LSB) and its 14-bit
 * value; 1 when it was a mod parameter's (MSB 4, whether a parameter has
 * the code or not) */
int core_nrpn_in(int nrpn, int value, int track)
{
    if (nrpn >> 7 != MSB || (unsigned)track >= 12)
        return 0;
    set(FIRST_CODE + (nrpn & 127), (value >> 7) & 127, track);
    return 1;
}

/* 0x400b0544 (midi.s): a track's CC; 1 when it was a mod parameter's */
int core_cc_in(int cc, int value, int track)
{
    unsigned i;
    if ((unsigned)track >= 12)
        return 0;
    for (i = 0; i < CCS; i++)
        if (CC_POOL[i] == cc) {
            if (!core_lock_by_code(FIRST_CODE + i))
                return 0;
            set(FIRST_CODE + i, value & 127, track);
            return 1;
        }
    return 0;
}

/* pages.s: a mod's knob took a turn (view, id) */
void core_midi_knob(void *view, int id)
{
    unsigned code, cfg;
    unsigned char *s;
    int track, v;
    const struct core_lock_param *p = core_lock_knob(view, id, &track, &code, &s);
    if (!p)
        return;
    cfg = MIDI_CONFIG(APP());
    if (!PARAM_OUT(cfg))
        return;
    v = (int)core_store_get(core_lock_field(p), s, 0);
    v = v < 0 ? 0 : v > 127 ? 127 : v;
    switch (PARAM_OUT_KIND(cfg)) {
    case 0:
        SEND_NRPN(OUT_CHANNEL(0), MSB << 7 | (code - FIRST_CODE), v << 7, 1);
        break;
    case 1:
        if (code - FIRST_CODE < CCS)
            SEND_CC(OUT_CHANNEL(0), CC_POOL[code - FIRST_CODE], v);
        break;
    }
}
