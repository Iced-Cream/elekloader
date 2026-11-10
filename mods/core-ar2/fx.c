/* SPDX-License-Identifier: GPL-2.0-or-later */
/* core-ar2 0.14: the FX skip (README, "The FX skip (0.14)"). Every address
 * is OS 1.74's.
 *
 * The FX block 0x4010e6bc builds the two send buses, calls the delay
 * 0x4010da24 (at 0x4010e7c4), adds the delay into the reverb's bus and calls
 * the reverb 0x4010d434 (at 0x4010e81e), every block, whether anything is
 * sent or not. Both routines leave the MAC unit as they found it (MACSR 0xa0,
 * the accumulators cleared), so a call left out leaves it the same.
 *
 * Two switches (analog-rytm-mk2/fx.h), off at boot and written only by the
 * mod that offers them as settings; core_fx_skip_hold holds both off:
 *   - core_fx_skip_want: the send trim (fx.s) and the delay skip. Exact: the
 *     output is stock's, bit for bit.
 *   - core_reverb_clear_want (with core_fx_skip_want): the reverb clear and
 *     skip. Not exact: stock's reverb, once it has had any input, never
 *     decays to 0 (a limit cycle about +-117 of full scale 2^31, which leaves
 *     main L at -1), so core clears it once it is that quiet and then skips
 *     it; its next tail starts from silence, as after a power-on.
 *
 * The delay, skipped while all of these hold:
 *   - its bus (0x80007a44, 32 stereo frames) is 0;
 *   - its parameters (0x8000fbaa..0x8000fbbb) are those of its last call, and
 *     that call changed none of its state (0x8000ca00..0x8000ca0f,
 *     0x8000dff0..0x8000e12b: input gains, gate, time, filter, the tap's
 *     scratch) and wrote 0 to its output (0x80007c44) and to the staging
 *     block (*0x8000ddb0, 0x100 bytes) that the eDMA writes into the line;
 *   - for LINE_BLOCKS blocks no call has written anything but 0 into the line:
 *     the write position (0x8000e1c8) moves 32 frames a block round a line of
 *     512K frames, so the whole 4 MB line is 0, and a call would read 0.
 * A skipped block leaves the staging block as the eDMA prefetched it, from
 * the line, so 0 goes back into the line, as a call would write.
 *
 * The reverb, on its bus (0x80007b44) after the delay's add:
 *   RUN     stock's call. Input 0 for QUIET_BLOCKS blocks with the output
 *           below QUIET_PEAK (1 LSB of 24 bits) all along: CLEAR.
 *   CLEAR   no call (input 0, so the bus, its output in place, stays 0); each
 *           block clears its state words (those its boot init 0x4010dfd8
 *           clears, and its SRAM blocks and scratch, 0x8000cc3c..0x8000ccbb and
 *           0x8000d1f0..0x8000dd03, which the eDMA moves to and from the
 *           lines) and CLEAR_CHUNK bytes of its lines (0x4b000000, 0x98000
 *           bytes). Then VERIFY.
 *   VERIFY  stock's call, from the cleared state: the output must stay exactly
 *           0 for VERIFY_BLOCKS blocks (two turns of the 64 KB lines), else
 *           RUN (after MAX_FAILS failures, not again until the input moves).
 *           Then SKIP.
 *   SKIP    no call while the input is 0: from a cleared state with input 0
 *           the call changes nothing (its LFO and line positions run outside
 *           it, at 0x4010e420 and 0x4010e822, and only move reads of 0).
 *           In CLEAR and SKIP core keeps the call's tap position
 *           (0x8000cc2c) current, as the call would.
 * Input that is not 0 gives the reverb back to stock (RUN) in that block. */

#include <stdint.h>

extern volatile uint8_t core_fx_skip_want, core_reverb_clear_want;
extern volatile uint32_t core_fx_skip_hold;

#define DELAY ((void (*)(void))0x4010da24)
#define REVERB ((void (*)(void))0x4010d434)
#define DELAY_BUS ((volatile uint32_t *)0x80007a44)
#define REVERB_BUS ((volatile uint32_t *)0x80007b44)
#define DELAY_OUT ((volatile uint32_t *)0x80007c44)
#define STAGING (*(volatile uint32_t *volatile *)0x8000ddb0)
#define BUS_LONGS 64
#define DELAY_PARAMS ((volatile uint16_t *)0x8000fbaa)
#define DELAY_PARAM_N 9
#define D_STATE_A ((volatile uint32_t *)0x8000ca00)
#define D_STATE_A_N 4
#define D_STATE_B ((volatile uint32_t *)0x8000dff0)
#define D_STATE_B_N ((0x8000e12cU - 0x8000dff0U) / 4)
#define LINE_BLOCKS (16384 + 16)

#define LINES 0x4b000000U
#define LINES_BYTES 0x98000U
#define CLEAR_CHUNK 0x2000U
#define QUIET_BLOCKS 1500
#define QUIET_PEAK 256
#define VERIFY_BLOCKS 1024
#define MAX_FAILS 2

enum { RUN, CLEAR, VERIFY, SKIP };

/* blocks the FX skip left a delay or reverb call out of, since boot */
uint32_t core_fx_skips;

static uint16_t d_params[DELAY_PARAM_N];
static uint32_t d_state[D_STATE_A_N + D_STATE_B_N];
static uint32_t d_zero_blocks;
static uint8_t d_still;

static uint8_t r_mode, r_fails;
static uint32_t r_count, r_off;

static int on(void)
{
    return core_fx_skip_want && !core_fx_skip_hold;
}

static int zero(volatile uint32_t *p, int n)
{
    uint32_t a = 0;
    for (int i = 0; i < n; i++)
        a |= p[i];
    return a == 0;
}

/* the 0x8000e1c8 counter: blocks since a call last wrote anything but 0 */
static void d_count(int zero_written)
{
    if (!zero_written)
        d_zero_blocks = 0;
    else if (d_zero_blocks < LINE_BLOCKS)
        d_zero_blocks++;
}

/* 0x4010e7c4: in place of `jsr 0x4010da24` */
void core_fx_delay(void)
{
    if (!on() || (core_fx_skip_want & 4) /* TEST ONLY: 4 = no delay skip */ || !zero(DELAY_BUS, BUS_LONGS)) {
        d_still = 0;
        d_zero_blocks = 0;
        DELAY();
        return;
    }
    int same = d_still;
    for (int i = 0; i < DELAY_PARAM_N; i++) {
        uint16_t w = DELAY_PARAMS[i];
        same &= d_params[i] == w;
        d_params[i] = w;
    }
    if (same && d_zero_blocks >= LINE_BLOCKS) {
        core_fx_skips++;
        return;
    }
    for (int i = 0; i < D_STATE_A_N; i++)
        d_state[i] = D_STATE_A[i];
    for (uint32_t i = 0; i < D_STATE_B_N; i++)
        d_state[D_STATE_A_N + i] = D_STATE_B[i];
    DELAY();
    int written0 = zero(STAGING, BUS_LONGS) && zero(DELAY_OUT, BUS_LONGS);
    int still = written0;
    for (int i = 0; i < D_STATE_A_N && still; i++)
        still = d_state[i] == D_STATE_A[i];
    for (uint32_t i = 0; i < D_STATE_B_N && still; i++)
        still = d_state[D_STATE_A_N + i] == D_STATE_B[i];
    d_still = (uint8_t)still;
    d_count(written0);
}

static void clear(uint32_t from, uint32_t to)
{
    volatile uint32_t *p = (volatile uint32_t *)from, *e = (volatile uint32_t *)to;
    while (p < e) {
        p[0] = 0;
        p[1] = 0;
        p[2] = 0;
        p[3] = 0;
        p += 4;
    }
}

/* the reverb's state words, not its lines: what its boot init clears, its
 * scratch and its SRAM blocks (all multiples of 16 bytes) */
static void clear_state(void)
{
    clear(0x8000ca10U, 0x8000cc10U);
    clear(0x8000cc3cU, 0x8000ccfcU);
    clear(0x8000cd00U, 0x8000cd10U);    /* the input filter, cd08 and cd0c */
    clear(0x8000d1f0U, 0x8000dd00U);
    ((volatile uint32_t *)0x8000dd00U)[0] = 0;
}

static uint32_t peak(volatile uint32_t *p, int n)
{
    uint32_t m = 0;
    for (int i = 0; i < n; i++) {
        int32_t v = (int32_t)p[i];
        uint32_t a = v < 0 ? -(uint32_t)v : (uint32_t)v;
        if (a > m)
            m = a;
    }
    return m;
}

/* the reverb's tap position, as its call works it out (0x4010d7fa..0x4010d820)
 * from the LFO (0x4010e420 steps it every block) and 0x8000cd14: the call
 * steps its taps from the last call's position (0x8000cc2c) to this one, so a
 * block without the call keeps it current, or the next call would step its
 * taps across the whole time it was left out */
static void tap_position(void)
{
    int32_t lfo = *(volatile int32_t *)0x42fb4ac4;
    int32_t base = (int32_t)(*(volatile uint32_t *)0x8000cd14 << 18);
    *(volatile int32_t *)0x8000cc2c = ((lfo >> 10) + base) >> 18;
}

static void run(void)
{
    r_mode = RUN;
    r_count = 0;
}

/* 0x4010e81e: in place of `jsr 0x4010d434` */
void core_fx_reverb(void)
{
    if (!on() || !core_reverb_clear_want) {
        run();
        r_fails = 0;
        REVERB();
        return;
    }
    int in0 = zero(REVERB_BUS, BUS_LONGS);
    switch (r_mode) {
    case SKIP:
        if (in0) {
            tap_position();
            core_fx_skips++;
            return;
        }
        run();
        REVERB();
        return;
    case CLEAR:
        if (!in0) {
            run();
            REVERB();
            return;
        }
        tap_position();
        clear_state();
        clear(LINES + r_off, LINES + r_off + CLEAR_CHUNK);
        r_off += CLEAR_CHUNK;
        if (r_off >= LINES_BYTES) {
            r_mode = VERIFY;
            r_count = 0;
        }
        return;
    case VERIFY:
        REVERB();
        if (!in0) {
            run();
            return;
        }
        if (!zero(REVERB_BUS, BUS_LONGS)) {
            r_fails++;
            run();
            return;
        }
        if (++r_count >= VERIFY_BLOCKS)
            r_mode = SKIP;
        return;
    default:
        REVERB();
        if (!in0) {
            r_count = 0;
            r_fails = 0;
            return;
        }
        if (r_fails >= MAX_FAILS)
            return;
        if (peak(REVERB_BUS, BUS_LONGS) >= QUIET_PEAK) {
            r_count = 0;
            return;
        }
        if (++r_count >= QUIET_BLOCKS) {
            r_mode = CLEAR;
            r_off = 0;
        }
        return;
    }
}
