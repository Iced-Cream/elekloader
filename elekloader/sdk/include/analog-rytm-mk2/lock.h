/* SPDX-License-Identifier: GPL-2.0-or-later */
/* core-ar2 0.8: lockable mod parameters (docs/ADAPTING.md, "Lockable
 * parameters (core-ar2 0.8, Analog Rytm MKII)").
 *
 * A sound-scope storage field declared with "lock": true gets a lock code
 * (42..127) from the build's lockfile. A mod registers it once, through the
 * table core_lock_params:
 *
 *     "storage": [{"sym": "my_depth", "scope": "sound", "bits": 7, "lock": true}],
 *     "contribute": [
 *       {"to": "core_lock_params", "order": 50, "data": "00000000",
 *        "relocs": [[0, "abs32", "sym:my_depth_param", 0]]}]
 *
 * and in C:
 *
 *     STORE_FIELD(my_depth);
 *     const struct core_lock_param my_depth_param =
 *         {STORE(my_depth), 449, 0, 127, "DEP", "My Depth", CORE_LOCK_CATEGORY("MINE")};
 *
 * Core then lists the parameter in each sound's LFO destination menu (after
 * stock's, as category:name, "MOD" when the category is 0), names it there
 * and in the DST value, and keeps the LFO's offset
 * for it, which the mod reads with core_lfo_offset and adds to the value it
 * uses. With id, the page knob that shows the parameter (core_params),
 * RANDOMIZE PAGE gives the knob a value in min..max and RELOAD PAGE takes it
 * back from the saved kit, as they do stock knobs.
 */
#ifndef ELEKLOADER_AR2_LOCK_H
#define ELEKLOADER_AR2_LOCK_H

#include <stdint.h>

struct core_lock_param {
    unsigned field;             /* STORE(sym): a lockable sound field */
    int32_t id;                 /* its page knob's id, or 0 */
    int16_t min, max;           /* the values the field takes (locks: to 16383) */
    const char *short_name;     /* the DST value and the knob's label: up to 4 characters */
    const char *name;           /* the destination menu and the knob's pop-up: up to 32, in
                                 * proper case as stock's */
    const char *category;       /* before the colon in the menu: up to 4 characters, upper
                                 * case as stock's (SAMP, FILT, AMP), or 0 for MOD */
};

/* Core hands stock at most 4 characters of a short name or a category and
 * 32 of a name, so no length a mod gives can overrun stock's buffers. A
 * category written with this is refused by the compiler when it is longer
 * than 4 characters. */
#define CORE_LOCK_CATEGORY(s) ((s) + 0 * sizeof(char[sizeof(s) <= 5 ? 1 : -1]))

/* the field's lock code (42..127), or 0 when it has none */
int core_lock_code(unsigned field);

/* the offset track's LFO (0..12) adds to the field this audio block, in 8.8
 * steps of a stock 0..127 knob (one step is 256), or 0 when the LFO's
 * destination is another parameter. Updated in the audio interrupt, before
 * ev_render_out; the mod scales it to its own range. */
int core_lfo_offset(int track, unsigned field);

/* core-ar2 0.8: the field's p-lock on the trig track (0..12) last played,
 * in min..max, or -1 when that trig has none. Set with the trig's other locks,
 * before it sounds; the mod uses it in place of the sound's value for that
 * trig. A p-lock is set like a stock one: a trig held, the parameter's knob
 * (its id) turned. It is a lock row in the pattern, coded by the field's
 * lock code, so it shares the pattern's 72 rows with stock's (71 used at
 * most, stock's first); a parameter whose max is above 127 takes two rows
 * per track and keeps its whole value. */
int core_lock_value(int track, unsigned field);

/* core-ar2 0.8: the field's lock in the active scene of the kit track
 * (0..11) plays, in min..max, or -1 when the scene has none (or no scene is
 * active). A scene lock is set like a stock one: the scene's pad held in
 * scene edit, the parameter's knob turned. It is an entry with the field's
 * lock code in the kit's 48 scene locks, shared with stock's. Above 127 it
 * moves in coarse steps (max shifted right into 0..127), as the kit's file
 * keeps only a lock's whole step. The mod uses it in place of the sound's
 * value while the scene is active. */
int core_scene_value(int track, unsigned field);

/* core-ar2 0.8: the offset the performances give the field on track (0..11)
 * now, in 8.8 steps of the field (one step is 256): each performance's depth
 * for it times its pad's pressure, as stock adds a performance's to a stock
 * parameter. The depth is set like a stock one, the performance's pad held
 * in its edit and the knob turned; it is an entry in the kit's 48
 * performance locks, in coarse steps above 127 (as core_scene_value). The
 * mod adds it to the value it uses, as it adds core_lfo_offset. */
int core_perf_offset(int track, unsigned field);

#endif
