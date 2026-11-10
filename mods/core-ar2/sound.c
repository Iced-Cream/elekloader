/* SPDX-License-Identifier: GPL-2.0-or-later */
/* core-ar2 (sidecar-engines): the sound a trig plays (README, "The sound a
 * trig plays"). sound.s files, for each track, the sound item stock pushed
 * to the engine last: the track's sound in a live kit (a plain trig, a kit
 * apply) or a pool sound (a sound lock). Plain reads, from the audio
 * interrupt or the UI task. */
#include <stdint.h>

#define TRACKS 12
#define POOL ((uint8_t *)0x41313c47)        /* pool sound i: POOL + 0x98 i */
#define POOL_SOUNDS 128
#define SOUND_SIZE 0x98

void *volatile core_trig_items[TRACKS];

void *core_track_sound(int32_t track)
{
    return (uint32_t)track < TRACKS ? core_trig_items[track] : 0;
}

int core_track_pool_sound(int32_t track)
{
    uint8_t *s = core_track_sound(track);
    uint32_t o = (uint32_t)(s - POOL);
    if (!s || (uint32_t)s < (uint32_t)POOL || o % SOUND_SIZE || o / SOUND_SIZE >= POOL_SOUNDS)
        return -1;
    return o / SOUND_SIZE;
}
