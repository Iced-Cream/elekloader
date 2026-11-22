/* SPDX-License-Identifier: GPL-2.0-or-later */
/* payload-check (Analog Rytm MKII): drive-loader-ar2 0.12's Drive payload table
 * (analog-rytm-mk2/drivepay.h), from a +Drive mod with a 225 KB .payload
 * (payload.s). On ev_tick, in the UI task:
 *
 *   tick 30       the table: drive_payload_abi, count, at, find of its own id,
 *                 info, and the edges (past the end, a bad id, a bad handle,
 *                 a read at the end and past it)
 *   ticks 31..61  DMA timer 0's counts over 30 ticks (one second)
 *   tick 70 on    pass 0, then 30 ticks later pass 1: the payload read whole
 *                 in 4 KB dl_payload_read calls, 32 calls a tick, each word
 *                 checked against the pattern and the sha256 of what came
 *                 compared with drive_payload_info's; each call timed
 *
 * The record follows the magic PCK1; done reads 3 when both passes ended. */
#include <stdint.h>
#include "analog-rytm-mk2/drivepay.h"

#ifndef DTCN0
#define DTCN0 (*(volatile uint32_t *)0xfc07000c)
#endif
#define CHUNK 4096
#define PER_TICK 32
#define BLOCK_WORDS 64
#define RANDOM 48
#define TAG 0x5041594cu

static const uint32_t table[BLOCK_WORDS - 1 - RANDOM] = {
    0xe15d9d01u, 0x8c73fe28u, 0x8e85bde5u, 0x459dd5a5u,
    0x4bda3694u, 0x4a421a4eu, 0x32edf4c4u, 0xf2fd80c7u,
    0xfad6503fu, 0x0ea70bddu, 0x22906a7au, 0xe152535du,
    0x8144ee59u, 0xc5b3986eu, 0x15f6f9c3u
};

struct pass {
    uint32_t calls, bytes;      /* dl_payload_read calls, bytes they gave */
    int32_t worst;              /* the lowest answer (an error is < 0) */
    uint32_t short_calls;       /* answers under CHUNK before the end */
    uint32_t bad_words, first_bad;
    uint32_t dt_sum, dt_max;    /* DMA timer 0 counts in the calls */
    uint32_t ticks;             /* ticks the pass took */
    uint8_t sha[32];            /* sha256 of what came */
    uint32_t sha_ok;            /* 1: it equals drive_payload_info's */
};

struct {
    char magic[4];              /* PCK1 */
    uint32_t ticks, done;       /* done: bit 0 pass 0 ended, bit 1 pass 1 */
    uint32_t abi, count, id;
    uint32_t found_at;          /* the index whose handle drive_payload_find gave, or ~0 */
    int32_t info_rc;
    uint32_t len;
    uint8_t sha[32];            /* drive_payload_info's */
    uint32_t at_past;           /* drive_payload_at(count): 0 */
    uint32_t find_bad;          /* drive_payload_find(id ^ 1): 0 */
    int32_t info_bad;           /* drive_payload_info(0): -1 */
    int32_t read_end;           /* dl_payload_read(h, len, .., 4): 0 */
    int32_t read_past;          /* dl_payload_read(h, len + 1, .., 4): -1 */
    int32_t read_bad;           /* dl_payload_read(0, 0, .., 4): -1 */
    uint32_t dt_per_s;          /* DMA timer 0 counts over 30 ticks */
    struct pass pass[2];
} pck = {{'P', 'C', 'K', '1'}};

static const struct drive_payload *h;
static uint8_t buf[CHUNK];

/* ---- sha256 ---- */
static const uint32_t K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

static struct {
    uint32_t s[8], n;           /* state, bytes so far */
    uint8_t b[64];
} sh;

#define ROR(x, k) (((x) >> (k)) | ((x) << (32 - (k))))

static void sha_block(const uint8_t *p)
{
    uint32_t w[64], a, b, c, d, e, f, g, hh, t1, t2;
    int i;
    for (i = 0; i < 16; i++)
        w[i] = (uint32_t)p[4 * i] << 24 | (uint32_t)p[4 * i + 1] << 16 | (uint32_t)p[4 * i + 2] << 8 | p[4 * i + 3];
    for (; i < 64; i++)
        w[i] = w[i - 16] + (ROR(w[i - 15], 7) ^ ROR(w[i - 15], 18) ^ (w[i - 15] >> 3)) + w[i - 7]
             + (ROR(w[i - 2], 17) ^ ROR(w[i - 2], 19) ^ (w[i - 2] >> 10));
    a = sh.s[0]; b = sh.s[1]; c = sh.s[2]; d = sh.s[3];
    e = sh.s[4]; f = sh.s[5]; g = sh.s[6]; hh = sh.s[7];
    for (i = 0; i < 64; i++) {
        t1 = hh + (ROR(e, 6) ^ ROR(e, 11) ^ ROR(e, 25)) + ((e & f) ^ (~e & g)) + K[i] + w[i];
        t2 = (ROR(a, 2) ^ ROR(a, 13) ^ ROR(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
        hh = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
    }
    sh.s[0] += a; sh.s[1] += b; sh.s[2] += c; sh.s[3] += d;
    sh.s[4] += e; sh.s[5] += f; sh.s[6] += g; sh.s[7] += hh;
}

static void sha_init(void)
{
    static const uint32_t iv[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                                   0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    int i;
    for (i = 0; i < 8; i++)
        sh.s[i] = iv[i];
    sh.n = 0;
}

static void sha_add(const uint8_t *p, uint32_t n)
{
    while (n--) {
        sh.b[sh.n++ & 63] = *p++;
        if (!(sh.n & 63))
            sha_block(sh.b);
    }
}

static void sha_end(uint8_t out[32])
{
    uint32_t bits = sh.n * 8, i;
    uint8_t z = 0x80;
    sha_add(&z, 1);
    z = 0;
    while ((sh.n & 63) != 56)
        sha_add(&z, 1);
    for (i = 0; i < 4; i++)
        sha_add(&z, 1);         /* bit length's high word: under 512 MB */
    for (i = 0; i < 4; i++) {
        z = (uint8_t)(bits >> (24 - 8 * i));
        sha_add(&z, 1);
    }
    for (i = 0; i < 32; i++)
        out[i] = (uint8_t)(sh.s[i / 4] >> (24 - 8 * (i % 4)));
}

/* ---- the pattern, word by word (payload.s) ---- */
static struct {
    uint32_t w, x;              /* word index, the xorshift's state */
} pat;

static uint32_t next_word(void)
{
    uint32_t b = pat.w / BLOCK_WORDS, i = pat.w % BLOCK_WORDS, v;
    if (!i)
        v = b ^ TAG;
    else if (i <= RANDOM) {
        pat.x ^= pat.x << 13;
        pat.x ^= pat.x >> 17;
        pat.x ^= pat.x << 5;
        v = pat.x;
    } else
        v = table[i - 1 - RANDOM];
    pat.w++;
    return v;
}

static uint32_t fnv(const char *s)
{
    uint32_t x = 0x811c9dc5u;
    while (*s)
        x = (x ^ (uint8_t)*s++) * 0x01000193u;
    return x;
}

static void table_check(void)
{
    uint32_t i;
    pck.abi = drive_payload_abi;
    if (pck.abi < 1)
        return;
    pck.count = drive_payload_count();
    pck.id = fnv("payload-check");
    h = drive_payload_find(pck.id);
    pck.found_at = 0xffffffffu;
    for (i = 0; i < pck.count; i++)
        if (drive_payload_at(i) == h)
            pck.found_at = i;
    pck.at_past = (uint32_t)drive_payload_at(pck.count);
    pck.find_bad = (uint32_t)drive_payload_find(pck.id ^ 1);
    pck.info_bad = drive_payload_info(0, 0, 0, 0);
    pck.read_bad = dl_payload_read(0, 0, buf, 4);
    if (!h)
        return;
    pck.info_rc = drive_payload_info(h, 0, &pck.len, pck.sha);
    pck.read_end = dl_payload_read(h, pck.len, buf, 4);
    pck.read_past = dl_payload_read(h, pck.len + 1, buf, 4);
}

static uint32_t at;             /* the running pass's next byte */
static uint32_t start;          /* the tick it started */

static void pass_start(struct pass *p)
{
    at = 0;
    pat.w = 0;
    pat.x = TAG;
    sha_init();
    p->worst = 0x7fffffff;
    p->first_bad = 0xffffffffu;
    start = pck.ticks;
}

/* -> 1 when the pass has ended */
static int pass_step(struct pass *p)
{
    uint32_t k, t0, dt, i;
    int32_t r;
    for (k = 0; k < PER_TICK; k++) {
        t0 = DTCN0;
        r = dl_payload_read(h, at, buf, CHUNK);
        dt = DTCN0 - t0;
        p->calls++;
        p->dt_sum += dt;
        if (dt > p->dt_max)
            p->dt_max = dt;
        if (r < p->worst)
            p->worst = r;
        if (r <= 0)
            break;
        if (r < CHUNK && at + (uint32_t)r < pck.len)
            p->short_calls++;
        sha_add(buf, (uint32_t)r);
        for (i = 0; i + 4 <= (uint32_t)r; i += 4) {
            uint32_t got = (uint32_t)buf[i] << 24 | (uint32_t)buf[i + 1] << 16 | (uint32_t)buf[i + 2] << 8 | buf[i + 3];
            if (got != next_word() && !p->bad_words++)
                p->first_bad = at + i;
        }
        at += (uint32_t)r;
        p->bytes += (uint32_t)r;
        if (at >= pck.len)
            break;
    }
    if (k < PER_TICK || at >= pck.len) {
        sha_end(p->sha);
        p->sha_ok = 1;
        for (i = 0; i < 32; i++)
            if (p->sha[i] != pck.sha[i])
                p->sha_ok = 0;
        p->ticks = pck.ticks - start + 1;
        return 1;
    }
    return 0;
}

static uint32_t cal0, next_start = 70;
static int running = -1;

void pck_tick(void *ctrl)
{
    uint32_t t = ++pck.ticks;
    (void)ctrl;
    if (t == 30)
        table_check();
    else if (t == 31)
        cal0 = DTCN0;
    else if (t == 61)
        pck.dt_per_s = DTCN0 - cal0;
    if (!h || pck.info_rc || pck.done == 3)
        return;
    if (running < 0 && t == next_start) {
        running = pck.done ? 1 : 0;
        pass_start(&pck.pass[running]);
    }
    if (running >= 0 && pass_step(&pck.pass[running])) {
        pck.done |= 1u << running;
        running = -1;
        next_start = t + 30;
    }
}
