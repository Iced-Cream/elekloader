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
 *         {STORE(my_depth), 449, 0, 127, "DEP", "MY DEPTH"};
 *
 * Core then lists the parameter in each sound's LFO destination menu (after
 * stock's), names it there and in the DST value, and keeps the LFO's offset
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
    int16_t min, max;           /* the values the field takes */
    const char *short_name;     /* the DST value: up to 4 characters */
    const char *name;           /* the destination menu: up to 32 */
};

/* the field's lock code (42..127), or 0 when it has none */
int core_lock_code(unsigned field);

/* the offset track's LFO (0..12) adds to the field this audio block, in 8.8
 * steps of a stock 0..127 knob (one step is 256), or 0 when the LFO's
 * destination is another parameter. Updated in the audio interrupt, before
 * ev_render_out; the mod scales it to its own range. */
int core_lfo_offset(int track, unsigned field);

#endif
