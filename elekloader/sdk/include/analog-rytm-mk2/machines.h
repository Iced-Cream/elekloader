/* SPDX-License-Identifier: GPL-2.0-or-later */
/* core-ar2 0.5: machines mods add (docs/ADAPTING.md, "Machines (core-ar2 0.5,
 * Analog Rytm MKII)"). The Rytm has 53 machine slots; 34..52 are hidden
 * factory machines (mask 0: no voice may pick them). A mod takes one of
 * slots 34..52 (44..52 from core 0.13) through the table core_machines,
 * contributed in mod.json:
 *
 *     "contribute": [
 *       {"to": "core_machines", "order": 50, "data": "00000000",
 *        "relocs": [[0, "abs32", "sym:my_machine", 0]]}],
 *     "resources": {"core": "0.5"}
 *
 * elekloader names the entry's slot as the resource "machine:<slot>" (the
 * profile's slot_claims), so two mods that take the same slot are refused.
 * At boot, before the OS reads them, core writes each entry's engine, home
 * track, voice mask and names into the machine table, and its knobs' labels,
 * maximum and default into the slot's parameter records. A record keeps its
 * id, its slot in the sound (indices 9..16, LEV first) and its MIDI CC and
 * NRPN, so a sound stores the knobs as it stores any machine's. A knob
 * whose name is 0 keeps the stock record as it is. Slots 35..42 (the hidden
 * "2" machines) have all eight records; 34 (BT IMPROVED) and 43 (FUNC. GEN.)
 * have five (LEV and four more), and a knob with no record is ignored.
 *
 * From core 0.13 the TEST machines' slots 44..52 are offered too (ask for
 * "core": "0.13"). Their records have no MIDI of their own: core gives each
 * record a mod knob uses CC 16 + k and NRPN 1:k (k = sound index - 9), as
 * the hidden machines 34..42 have. They have fewer records:
 *
 *     44 BD TEST   9..16 (all eight)     49 MT TEST   9..12
 *     45 SD TEST   9..12, 15, 16         50 HH TEST   9..13, 16
 *     46 RS TEST   9..16 (all eight)     51 CY TEST   9..16 (all eight)
 *     47 BT TEST   9..13                 52 CB TEST   10..12, 16 (no LEV)
 *     48 LT TEST   9..12
 *
 * Mods that used these records as spare parameters (fx-sends, duck,
 * voice-dac) move off them for 0.13; one that still uses a slot's records
 * declares "machine:<n>" in its resources, so the linker refuses it next to
 * a machine in that slot. The value's text and the knob's turn can be
 * changed through core_params (pages.h).
 *
 * A kit or sound that names the machine loads on a build without the mod (or
 * on stock) with that sound's machine set to DISABLE: stock's kit load does
 * that for any machine its voice may not run.
 *
 * The engine is called once per 32-frame audio block for each physical voice
 * running the machine, as stock's are (the audio interrupt):
 *
 *     void engine(int32_t voice, void *frame, uint16_t *blk, int32_t pitch)
 *
 * frame is the analog control frame (0x800063c0), blk the track's parameter
 * block (word k is sound index 9 + k, so word 0 is LEV), pitch the note. An
 * engine may run a stock engine (CORE_ENGINE_*) with a block of its own.
 *
 * From core 0.13 the engine table holds core's trampoline, which calls the
 * mod's engine after latching, for the voice, what the interrupt's trig
 * intake set for its owner track this block (the intake runs before the
 * engines):
 *
 *     core_voice_trigged(voice)   1 when the track trigged this block, else 0
 *     core_voice_velocity(voice)  the track's velocity word (0x800067c4 + 2t),
 *                                 before accent
 *
 * Call them from the engine, for its own voice: they read the latch of the
 * voice's last call, so outside it they hold that call's values. Plain
 * reads, no locks.
 */
#ifndef ELEKLOADER_AR2_MACHINES_H
#define ELEKLOADER_AR2_MACHINES_H

#include <stdint.h>

typedef void (*core_engine)(int32_t voice, void *frame, uint16_t *blk, int32_t pitch);

struct core_machine_knob {
    const char *name;       /* long name ("Decay Time"); 0: the record as stock has it */
    const char *label;      /* the knob's three letters ("DEC") */
    uint8_t min, max;       /* the range, in the record's units (0..127 for most) */
    uint8_t def;            /* the default */
    uint8_t centred;        /* 1: shown around the middle of the range (stock's Tune) */
};

struct core_machine {
    int32_t slot;           /* 34..43; 34..52 from core 0.13 */
    core_engine engine;
    int8_t home;            /* the track the machine list files it under: 0 BD,
                               1 SD, 4 BT, -1 none (as XT CLASSIC) */
    uint8_t voices;         /* bit v: physical voice v may run it (0 BD, 1 BT,
                               2 SD, 3 LT, 4 CH/OH, 5 MT/HT, 6 CY/CB, 7 RS/CP) */
    uint16_t flags;         /* CORE_MACHINE_*; 0 before core 0.14 */
    const char *name;       /* "BD TX-9" */
    const char *label;      /* four letters: "BDT9" */
    struct core_machine_knob knobs[8];  /* sound indices 9..16: LEV, then A..H's order */
};

/* flags (sidecar-engines) */
#define CORE_MACHINE_DESTS 0x0001   /* slots 44..52: the knobs are LFO, velocity
                                       and aftertouch destinations */

#define CORE_VOICE_BD 0x01
#define CORE_VOICE_BT 0x02
#define CORE_VOICE_SD 0x04
#define CORE_VOICE_LT 0x08
#define CORE_VOICE_HH 0x10
#define CORE_VOICE_MT 0x20
#define CORE_VOICE_CY 0x40
#define CORE_VOICE_RS 0x80

/* stock engines (OS 1.74) */
#define CORE_ENGINE_BD_CLASSIC ((core_engine)0x401105c8)
#define CORE_ENGINE_SD_CLASSIC ((core_engine)0x401179bc)
#define CORE_ENGINE_BT_CLASSIC ((core_engine)0x4011b5a0)
#define CORE_ENGINE_XT_CLASSIC ((core_engine)0x4011b850)
#define CORE_ENGINE_DISABLE ((core_engine)0x40113f68)
#define CORE_ENGINE_UT_NOISE ((core_engine)0x401140c8)   /* machine 15; reads only blk */

/* core 0.13 */
int core_voice_trigged(int32_t voice);
int core_voice_velocity(int32_t voice);

/* sidecar-engines: stock engines for any machine. Plain reads, for the
 * audio interrupt as well as the UI.
 *
 *     core_track_default_machine(t)  track t's default machine (0..52), -1 out
 *                                    of range (tracks 0..11)
 *     core_machine_engine(m)         the engine the interrupt calls for m
 *     core_machine_voices(m)         m's voice mask (bit v: voice v may run it)
 *     core_machine_knob(m, k, ...)   1 and the range and default of m's
 *                                    record at sound index 9 + k, as record
 *                                    words (value << 8); 0 when m has none
 *     core_machine_defaults(m, blk)  writes m's defaults into blk[0..7]
 *                                    (16-bit words, as the interrupt's blk
 *                                    holds them); returns bit k for each
 *                                    word written, the rest is left
 *     core_voice_engine_reset(v)     the voice's state as after a machine
 *                                    change: call it when an engine starts
 *                                    running a different stock engine on v
 *
 * A mod's engine may call a stock engine with its own voice, frame and pitch
 * and a block of its own, on a voice the engine's mask allows (the
 * interrupt does not check). The stock engine keeps its own state for the
 * voice (0x8000fef8 + 32v, its noise and wave buffers): the mod's engine
 * leaves those alone. */
int core_track_default_machine(int32_t track);
core_engine core_machine_engine(int32_t m);
int core_machine_voices(int32_t m);
int core_machine_knob(int32_t m, int32_t k, int32_t *min, int32_t *max, int32_t *def);
int core_machine_defaults(int32_t m, uint16_t *blk);
void core_voice_engine_reset(int32_t voice);

/* sidecar-engines: the sound a trig plays. The sound item stock last pushed
 * to track t's engine (tracks 0..11): the track's sound in a live kit (a
 * plain trig, a kit apply; the item core_current_sound_item gives) or a pool
 * sound (a sound lock), 0 before the first push. Plain reads, from the audio
 * interrupt or the UI task.
 *
 *     core_track_sound(t)       the item
 *     core_track_pool_sound(t)  its pool sound 0..127, or -1 for a kit's sound
 */
void *core_track_sound(int32_t track);
int core_track_pool_sound(int32_t track);

/* sidecar-engines: set sound index i (0..41) of track t's sound (0..11) in
 * the current kit as a knob turn does: value is the sound's 16-bit word
 * (display value << 8, the fine part in the low byte), clamped to the
 * record's range; the live value, the kit word, a lock while live recording,
 * the host's notification and the screen follow; out 1 also sends it as
 * PARAM OUTPUT does. A trig held is not a p-lock here. 1 when set, 0 when
 * the sound's machine has no such parameter. UI task only. */
int core_param_set(int32_t track, int32_t index, int32_t value, int32_t out);

#endif
