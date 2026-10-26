/* SPDX-License-Identifier: GPL-2.0-or-later */
/* core-ar2 0.8: mod parameters on MIDI, the fourth part of the lock service
 * (README, "Mod parameters on MIDI"). Every address is OS 1.74's.
 *
 * Stock's NRPNs for a track's sound use MSBs 0..3. A mod parameter is NRPN
 * MSB 4, LSB its lock code - 42, on the track's channel, its value in the
 * data entry MSB (0..127; the LSB is ignored):
 *
 *   - in: the track's NRPN in (0x400b2b26, midi.s) gives an NRPN of MSB 4 to
 *     core, which sets the parameter in the track's sound of the current kit,
 *     clamped to its range, as its knob does;
 *   - out: when a mod's knob turns a parameter (core_param_delta, pages.s),
 *     core sends the NRPN as stock's parameter out (0x400a9754) sends a stock
 *     one: when PARAM OUTPUT is on and set to NRPN, on the channel stock uses.
 *
 * CC has no free numbers to give mod parameters, so they have none.
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

struct core_lock_param;
const struct core_lock_param *core_lock_by_code(unsigned code);
const struct core_lock_param *core_lock_knob(void *view, int id, int *track, unsigned *code,
                                             unsigned char **s);
unsigned core_lock_field(const struct core_lock_param *p);
int core_lock_min(const struct core_lock_param *p);
int core_lock_max(const struct core_lock_param *p);
unsigned core_store_get(unsigned field, void *item, int index);
void core_store_set(unsigned field, void *item, int index, unsigned value);

/* 0x400b2b26 (midi.s): a track's NRPN (MSB << 7 | LSB) and its 14-bit
 * value; 1 when it was a mod parameter's */
int core_nrpn_in(int nrpn, int value, int track)
{
    const struct core_lock_param *p;
    unsigned k = (unsigned)WORKING_KIT_INDEX[0] << 24 | (unsigned)WORKING_KIT_INDEX[1] << 16 |
                 (unsigned)WORKING_KIT_INDEX[2] << 8 | WORKING_KIT_INDEX[3];
    int v, hi;
    if (nrpn >> 7 != MSB || (unsigned)track >= 12)
        return 0;
    if (!(p = core_lock_by_code(FIRST_CODE + (nrpn & 127))) || k >= 128)
        return 1;
    v = (value >> 7) & 127;
    hi = core_lock_max(p) > 127 ? 127 : core_lock_max(p);
    v = v < core_lock_min(p) ? core_lock_min(p) : v > hi ? hi : v;
    core_store_set(core_lock_field(p),
                   (unsigned char *)LIVE_KITS + LIVE_KIT_SIZE * k + 42 + 152 * track, 0, v);
    return 1;
}

/* pages.s: a mod's knob took a turn (view, id) */
void core_nrpn_knob(void *view, int id)
{
    unsigned code, cfg;
    unsigned char *s;
    int track, v;
    const struct core_lock_param *p = core_lock_knob(view, id, &track, &code, &s);
    if (!p)
        return;
    cfg = MIDI_CONFIG(APP());
    if (!PARAM_OUT(cfg) || PARAM_OUT_KIND(cfg) != 0)
        return;
    v = (int)core_store_get(core_lock_field(p), s, 0);
    v = v < 0 ? 0 : v > 127 ? 127 : v;
    SEND_NRPN(OUT_CHANNEL(0), MSB << 7 | (code - FIRST_CODE), v << 7, 1);
}
