/* SPDX-License-Identifier: GPL-2.0-or-later */
/* core-ar2 0.12: the silent-voice skip (README, "The silent-voice skip
 * (0.12)"). Every address is OS 1.74's.
 *
 * The render calls one machine routine per voice slot (8 slots) every block
 * through 0x419ca110, from the slot's first SYN trig until the power goes
 * off, whether the voice sounds or not. Core stops calling it once the slot
 * is silent and its call provably changes nothing the voice board gets, and
 * puts the slot's digital state back to what the boot left, so the next
 * trig starts as the first one after a power-on does.
 *
 * Silent, all of these, every block (idle.s tests the gain first):
 *   - the machine's gain, both words, below 0x1000 (2^-19 of full scale,
 *     -114 dB): the machine's digital part (noise, click) is gone;
 *   - the slot's analog amp envelope (0x4010d0fc's sequence, 0x42fb49b4 +
 *     0x20 x slot) in its decay (3) or release (5), for twice the time
 *     stock's own rate-to-time routine (0x4011e7d6) gives for that rate from
 *     full level, plus 375 blocks (0.25 s), measured against the rate the
 *     envelope has now;
 *   - nothing the call reads changed since the last block: the owner track's
 *     0x54-byte parameter block, its pitch, the owner, the idle bit, the
 *     pointers, the machine's sequence state, and no trig of the owner;
 *   - the recorded machine is a public one (0..33) and not CP CLASSIC (6),
 *     whose generator is a global one; machines mods add (34 up) keep state
 *     core can't know.
 * Then core makes the call itself for QUIET_BLOCKS blocks and checks that it
 * changes nothing in the control frame (0x800063c0, 0x3d8 bytes) or the
 * slot's sequence state and timer (0x8000fef8 + 0x20 x slot, +0 and +4),
 * leaves both gain words as they were, and leaves the slot's click buffer
 * (0x80006bf8 + 0x80 x slot) at zero. Only one kind of frame write passes:
 * a byte set from 0 in a place the render clears after each send (byte +4
 * of entries 0x15..0x4c, 8 bytes apart, 0x401214d8): the same bytes every
 * call, at most REPLAY of them, which core writes again in each skipped
 * block so the frame sent is stock's. A call that changes anything else
 * (SY CHIP's arpeggio) is left to stock for BLOCKED_BLOCKS blocks before
 * core looks again. While a slot is skipped, core makes the call and checks
 * it again every PROBE_BLOCKS blocks: a machine whose words drift slower
 * than the quiet window sees (XT CLASSIC's ring-out, 1 in about 1000
 * blocks) goes back to stock at its next step, so the frame sent is at most
 * PROBE_BLOCKS blocks behind stock's (43 ms).
 *
 * Skipped, the slot's generator and filter struct (0x80007e20 + 0x2c x
 * slot), gain words and resonator buffer (0x80007040 + 0x80 x slot), and
 * for slot 7 the two RS words at 0x80007f6c, are copied back from the
 * snapshot core takes at the render's first block. The engine's private
 * words of its record (+8..+0x1c) stay where stock's last call left them
 * while the slot is skipped, since some machines write the frame from them
 * on their next call (BD PLASTIC's envelope rate), and go back to the
 * snapshot's at the slot's next SYN trig (flag bit 7 of 0x8000e53c), just
 * before stock's call for that note. Any change above (a trig of the owner, SYN on
 * or off, a knob, an LFO, a kit or machine change) gives the slot back to
 * stock in that same block.
 */

#include <stdint.h>

typedef uint32_t u32a2 __attribute__((aligned(2), may_alias));

#define MACH_PTR ((volatile uint32_t *)0x419ca110)  /* + slot */
#define MACH_REC ((volatile uint32_t *)0x419ca0d0)  /* + slot: the recorded machine */
#define PARAM_PTR ((volatile uint32_t *)0x419ca0f0) /* + slot */
#define IDLE_BITS (*(volatile uint8_t *)0x419ca0cc)
#define TRACK_TRIG ((volatile uint32_t *)0x8000e508) /* + track: 1 trig, 2 stop */
#define TRACK_FLAGS ((volatile uint8_t *)0x8000ea18)
#define TRACK_PITCH ((volatile uint32_t *)0x80006388)
#define TRACK_PARAMS 0x8000f7baU                    /* + track x 0x54 */
#define PARAM_LONGS 21
#define FRAME ((volatile uint32_t *)0x800063c0)
#define FRAME_LONGS (0x3d8 / 4)
#define RECORD(s) ((volatile uint32_t *)(0x8000fef8U + 0x20U * (s)))
#define GAIN(s) ((volatile uint32_t *)(0x80007000U + 8U * (s)))
#define CLICK(s) ((volatile uint32_t *)(0x80006bf8U + 0x80U * (s)))
#define AMP_STATE(s) (*(volatile int32_t *)(0x42fb49b4U + 0x20U * (s)))
#define AMP_RATES ((const uint32_t *)0x401ef030)
#define RATE_TIME ((int (*)(int, uint32_t))0x4011e7d6)

/* the snapshot's regions: gains + resonator buffers, the structs + RS words */
#define SNAP_A 0x80007000U
#define SNAP_A_LONGS ((0x40 + 0x400) / 4)
#define SNAP_B 0x80007e20U
#define SNAP_B_LONGS (0x160 / 4)
#define SNAP_C 0x8000fef8U
#define SNAP_C_LONGS (0x100 / 4)
#define TRACK_TRIG_FLAGS ((volatile uint32_t *)0x8000e53c) /* + track: bit 7 SYN */
#define CLEARED_FIRST (0x15 * 8 + 4)            /* frame offsets the render clears */
#define CLEARED_LAST (0x4c * 8 + 4)
#define REPLAY 4

#define QUIET_BLOCKS 16
#define PROBE_BLOCKS 64
#define BLOCKED_BLOCKS 256
#define MARGIN_BLOCKS 375
#define LAST_PUBLIC 33
#define CP_CLASSIC 6

enum { WATCH = 0, SKIPPED = 1, BLOCKED = 2 };
enum { IN_N = PARAM_LONGS + 7 };

/* per slot: idle.s clears both longs when the voice sounds.
 *   m: count in bits 0-15, mode in 16-23, bit 24 inputs recorded
 *   amp: blocks in the decay in bits 0-23, the amp state + 1 in 24-31 */
struct voice_state {
    uint32_t m, amp;
};
struct voice_state core_voice_state[8];
static uint32_t inputs[8][IN_N];
static uint32_t amp_rate[8];
static uint32_t snap_a[SNAP_A_LONGS], snap_b[SNAP_B_LONGS], snap_c[SNAP_C_LONGS];
static uint8_t record_due;                 /* slots whose record waits for a SYN trig */
/* per slot: the frame bytes its quiet call sets, as offset << 8 | value */
static uint32_t replay[8][REPLAY], replay_new[REPLAY];
static uint8_t replays[8];
static uint32_t frame_before[FRAME_LONGS];
static uint8_t snapped;
/* blocks skipped, for the emulator checks and a meter */
uint32_t core_voice_skips;

/* ev_render_in, after core_load_in: the boot's state, before any machine */
void core_voice_boot(void)
{
    if (snapped)
        return;
    for (int i = 0; i < SNAP_A_LONGS; i++)
        snap_a[i] = ((volatile uint32_t *)SNAP_A)[i];
    for (int i = 0; i < SNAP_B_LONGS; i++)
        snap_b[i] = ((volatile uint32_t *)SNAP_B)[i];
    for (int i = 0; i < SNAP_C_LONGS; i++)
        snap_c[i] = ((volatile uint32_t *)SNAP_C)[i];
    snapped = 1;
}

static void reset_slot(uint32_t s)
{
    volatile uint32_t *a = (volatile uint32_t *)SNAP_A, *b = (volatile uint32_t *)SNAP_B;
    a[2 * s] = snap_a[2 * s];
    a[2 * s + 1] = snap_a[2 * s + 1];
    for (uint32_t i = 0x10 + 0x20 * s; i < 0x30 + 0x20 * s; i++)
        a[i] = snap_a[i];
    for (uint32_t i = 11 * s; i < 11 * s + 11; i++)
        b[i] = snap_b[i];
    if (s == 7) {                       /* 0x80007f6c, 0x80007f70 */
        b[0x53] = snap_b[0x53];
        b[0x54] = snap_b[0x54];
    }
}

static int small(uint32_t g)
{
    return g + 0xfffU <= 0x1ffeU;
}

/* the decay's time is up (or not in a decay) */
static int amp_done(uint32_t s, uint32_t owner, struct voice_state *v)
{
    int32_t st = AMP_STATE(s);
    if (st != 3 && st != 5) {
        v->amp = 0;
        return 0;
    }
    const uint8_t *p = (const uint8_t *)(TRACK_PARAMS + 0x54U * owner);
    uint32_t rate = AMP_RATES[(int8_t)p[st == 3 ? 0x24 : 0x28]] & 0xfffff;
    uint32_t tag = (uint32_t)(st + 1) << 24;
    if ((v->amp & 0xff000000U) != tag || amp_rate[s] != rate) {
        v->amp = tag;
        amp_rate[s] = rate;
        return 0;
    }
    uint32_t n = v->amp & 0xffffffU;
    uint32_t need = 2U * ((uint32_t)RATE_TIME(0x7f, rate) >> 4) + MARGIN_BLOCKS;
    if (n < need) {
        v->amp = tag | (n + 1);
        return 0;
    }
    return 1;
}

static uint32_t pitch_of(uint32_t owner)
{
    return (TRACK_FLAGS[owner] & 1) ? TRACK_PITCH[owner] : 0x3c0000U;
}

static void idle_clear(uint32_t s)
{
    if (IDLE_BITS & (1U << s))
        *(volatile uint16_t *)(PARAM_PTR[s] + 4) = 0;
}

static void set(struct voice_state *v, uint32_t mode, uint32_t count, uint32_t rec)
{
    v->m = rec | mode << 16 | count;
}

/* idle.s, the gain small: 0 skips the call, else the pointer runs it */
uint32_t core_voice_quiet(uint32_t s, uint32_t owner, uint32_t ptr)
{
    struct voice_state *v = &core_voice_state[s];
    uint32_t mode = v->m >> 16 & 0xff, count = v->m & 0xffff;
    uint32_t mach = MACH_REC[s];

    if (mach > LAST_PUBLIC || mach == CP_CLASSIC || !snapped)
        return ptr;
    if (mode == BLOCKED) {
        if (count > 1)
            set(v, BLOCKED, count - 1, 0);
        else
            set(v, WATCH, 0, 0);
        return ptr;
    }

    /* the call's inputs */
    uint32_t in[IN_N];
    const u32a2 *pb = (const u32a2 *)(TRACK_PARAMS + 0x54U * owner);
    for (int i = 0; i < PARAM_LONGS; i++)
        in[i] = pb[i];
    in[PARAM_LONGS] = pitch_of(owner);
    in[PARAM_LONGS + 1] = owner;
    in[PARAM_LONGS + 2] = IDLE_BITS >> s & 1;
    in[PARAM_LONGS + 3] = mach;
    in[PARAM_LONGS + 4] = ptr;
    in[PARAM_LONGS + 5] = PARAM_PTR[s];
    in[PARAM_LONGS + 6] = RECORD(s)[0];
    int same = (v->m & 0x1000000U) && TRACK_TRIG[owner] == 0;
    for (int i = 0; i < IN_N; i++) {
        if (inputs[s][i] != in[i]) {
            same = 0;
            inputs[s][i] = in[i];
        }
    }
    if (!same) {
        if (TRACK_TRIG[owner] == 1 && (TRACK_TRIG_FLAGS[owner] & 0x80) && (record_due >> s & 1)) {
            for (uint32_t i = 8 * s + 2; i < 8 * s + 8; i++)
                ((volatile uint32_t *)SNAP_C)[i] = snap_c[i];
            record_due &= ~(1U << s);
        }
        set(v, WATCH, 0, 0x1000000U);
        return ptr;
    }
    int probe = 0;
    if (mode == SKIPPED) {
        if (count + 1 < PROBE_BLOCKS) {
            idle_clear(s);
            for (uint32_t i = 0; i < replays[s]; i++)
                ((volatile uint8_t *)FRAME)[replay[s][i] >> 8] = (uint8_t)replay[s][i];
            core_voice_skips++;
            set(v, SKIPPED, count + 1, 0x1000000U);
            return 0;
        }
        probe = 1;                      /* every PROBE_BLOCKS: the call, checked again */
    } else if (!amp_done(s, owner, v)) {
        set(v, WATCH, 0, 0x1000000U);
        return ptr;
    }

    /* the call, made here, with what it changes */
    volatile uint32_t *rec = RECORD(s), *g = GAIN(s), *ck = CLICK(s);
    uint32_t rec0 = rec[0], rec1 = rec[1], g0 = g[0], g1 = g[1];
    for (int i = 0; i < FRAME_LONGS; i++)
        frame_before[i] = FRAME[i];
    idle_clear(s);
    ((void (*)(uint32_t, volatile uint32_t *, uint32_t, uint32_t))ptr)(s, FRAME, PARAM_PTR[s], pitch_of(owner));
    int changed = rec0 != rec[0] || rec1 != rec[1];
    uint32_t n = 0;
    for (uint32_t i = 0; i < FRAME_LONGS && !changed; i++) {
        uint32_t was = frame_before[i], now = FRAME[i], off = 4 * i;
        if (was == now)
            continue;
        /* one byte, the long's first, set from 0 where the render clears */
        if ((was ^ now) & 0xffffffU || was >> 24 || off < CLEARED_FIRST
            || off > CLEARED_LAST || (off - CLEARED_FIRST) % 8 || n == REPLAY)
            changed = 1;
        else
            replay_new[n++] = off << 8 | now >> 24;
    }
    if (changed) {
        set(v, BLOCKED, BLOCKED_BLOCKS, 0);
        return 0;
    }
    int settled = g[0] == g0 && g[1] == g1 && small(g0) && small(g1);
    for (int i = 0; i < 32; i++)
        settled &= ck[i] == 0;
    if ((count || probe) && n != replays[s])
        settled = 0;
    for (uint32_t i = 0; i < n && settled && (count || probe); i++)
        settled = replay[s][i] == replay_new[i];
    replays[s] = (uint8_t)n;
    for (uint32_t i = 0; i < n; i++)
        replay[s][i] = replay_new[i];
    if (!settled) {
        set(v, WATCH, 0, 0x1000000U);
        return 0;
    }
    if (!probe && ++count < QUIET_BLOCKS) {
        set(v, WATCH, count, 0x1000000U);
        return 0;
    }
    reset_slot(s);
    record_due |= 1U << s;
    set(v, SKIPPED, 0, 0x1000000U);
    return 0;
}
