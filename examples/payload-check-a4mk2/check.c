/* SPDX-License-Identifier: GPL-2.0-or-later */
/* payload-check: core-a4mk2 0.6's payloads and its 37.7 MB mods' area
 * (0x45650000..0x47C00000) on the Analog Four MKII (OS 1.56). README says
 * what a plan reads.
 *
 * The payload: 512 KB of a known pattern (payload.s with mod.json's BLOCKS
 * 2048; sidecar-check-ar2's part 6), found through core_payload with this mod's id and read back whole at
 * the first ev_tick and then every 200 ticks (about 10 s).
 *
 * The area: an 8 MB .bss array, which the linker lays out in the mods'
 * area with the rest of the mod. The mod fills it with a pattern, 64 KB a
 * tick from the first ev_tick, and then reads it back 64 KB a tick, round
 * and round: a word that differs is one stock (or anything else) wrote
 * after the fill.
 *
 * In RAM after the magic PKC1 (pk): where the code, the data, the bss
 * array and the payload landed, and the counts. On the screen, at the
 * bottom right (y counts from the bottom row): a 3x3 square while all is
 * well (payload found and every read back right), blinking with the
 * area's passes; a bar from x 0 at y 0..1 when any word read back wrong.
 */
#include <stdint.h>
#include "analog-four-mk2/payload.h"

typedef void (*fillrect_t)(void *bmp, int x0, int y0, int x1, int y1, int colour);
#define FILLRECT ((fillrect_t)FILLRECT_AT)

#define BLOCK_WORDS 64
#define TAG 0x5041594cu
#define RANDOM 48
#define AREA_WORDS (8u << 18)                   /* 8 MB */
#define CHUNK_WORDS (16u << 10)                 /* 64 KB a tick */
#define AREA_FIRST 0x45650000u
#define AREA_END 0x47c00000u

static const uint32_t table[BLOCK_WORDS - 1 - RANDOM] = {
    0xe15d9d01u, 0x8c73fe28u, 0x8e85bde5u, 0x459dd5a5u,
    0x4bda3694u, 0x4a421a4eu, 0x32edf4c4u, 0xf2fd80c7u,
    0xfad6503fu, 0x0ea70bddu, 0x22906a7au, 0xe152535du,
    0x8144ee59u, 0xc5b3986eu, 0x15f6f9c3u
};

extern const uint32_t pk_payload[], pk_payload_end[];
static uint32_t area[AREA_WORDS];              /* .bss: in the mods' area */
static uint32_t data_word = 0x44415441u;      /* .data */

struct {
    char magic[4];                              /* PKC1 */
    uint32_t ticks;
    uint32_t code, data, bss, bss_len;          /* where the linker put them */
    uint32_t in_area;                           /* 1: all four inside 0x45650000..0x47C00000 */
    uint32_t id;                                /* core_payload_id("payload-check") */
    const void *addr;                           /* core_payload's answer */
    uint32_t len;
    const void *own;
    uint32_t own_len;
    uint32_t n_payloads;
    uint32_t verifies, words, bad, first_bad, total_bad, missing;
    uint32_t filled;                            /* area words filled */
    uint32_t passes;                            /* whole read-backs of the area done */
    uint32_t area_bad, area_first_bad;          /* words wrong so far, the first one's offset */
    uint32_t data_ok;
} pk = {{'P', 'K', 'C', '1'}};

static uint32_t at;                             /* the next word to fill or read */

static void verify(void)
{
    const uint32_t *p;
    uint32_t b, blocks, w, want, x = TAG, bad = 0, first = 0xffffffffu;
    const struct core_payload_entry *e;
    pk.id = core_payload_id("payload-check");
    pk.addr = core_payload(pk.id, &pk.len);
    pk.own = pk_payload;
    pk.own_len = (uint32_t)((const char *)pk_payload_end - (const char *)pk_payload);
    for (pk.n_payloads = 0, e = core_payloads; e->addr; e++)
        pk.n_payloads++;
    pk.missing = !pk.addr;
    pk.verifies++;
    pk.words = 0;
    if (!pk.addr || pk.len != pk.own_len || pk.len % (BLOCK_WORDS * 4)) {
        pk.bad = 0xffffffffu;
        return;
    }
    p = pk.addr;
    blocks = pk.len / (BLOCK_WORDS * 4);
    for (b = 0; b < blocks; b++)
        for (w = 0; w < BLOCK_WORDS; w++, p++) {
            if (!w)
                want = b ^ TAG;
            else if (w <= RANDOM) {
                x ^= x << 13;
                x ^= x >> 17;
                x ^= x << 5;
                want = x;
            } else
                want = table[w - 1 - RANDOM];
            if (*p != want) {
                if (!bad++)
                    first = (b * BLOCK_WORDS + w) * 4;
            }
        }
    pk.words = blocks * BLOCK_WORDS;
    pk.bad = bad;
    pk.first_bad = first;
    pk.total_bad += bad;
}

static uint32_t pattern(uint32_t i)
{
    return i * 2654435761u ^ 0x41524541u;
}

static int inside(uint32_t a, uint32_t len)
{
    return a >= AREA_FIRST && a + len <= AREA_END;
}

void pk_tick(void *ctrl)
{
    uint32_t i, end;
    if (!pk.ticks++) {
        pk.code = (uint32_t)pk_tick;
        pk.data = (uint32_t)&data_word;
        pk.bss = (uint32_t)area;
        pk.bss_len = sizeof area;
        pk.in_area = inside(pk.code, 4) && inside(pk.data, 4) && inside(pk.bss, sizeof area) &&
                     inside((uint32_t)pk_payload, 4);
    }
    pk.data_ok = data_word == 0x44415441u;
    if (pk.ticks == 1 || pk.ticks % 200 == 0)
        verify();
    end = at + CHUNK_WORDS;
    if (pk.filled < AREA_WORDS) {
        for (i = at; i < end; i++)
            area[i] = pattern(i);
        pk.filled = end;
    } else {
        for (i = at; i < end; i++)
            if (area[i] != pattern(i) && !pk.area_bad++)
                pk.area_first_bad = i * 4;
        if (end == AREA_WORDS)
            pk.passes++;
    }
    at = end == AREA_WORDS ? 0 : end;
    if ((pk.ticks & 7) == 0)
        *((volatile uint8_t *)ctrl + 0x20) = 1;
}

void pk_draw(void *bmp, void *ctrl)
{
    int ok = pk.in_area && pk.data_ok && !pk.missing && !pk.total_bad && !pk.area_bad;
    (void)ctrl;
    FILLRECT(bmp, 0, 0, 127, 2, 0);
    if (ok && !(pk.passes & 1))
        FILLRECT(bmp, 125, 0, 127, 2, 1);
    if (pk.total_bad || pk.area_bad)
        FILLRECT(bmp, 0, 0, 40, 1, 1);
}
