/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "analog-rytm-mk2/machines.h"
#include "analog-rytm-mk2/smpl.h"
CORE_MACHINES;
SMPL_MACHINES;
struct smck_state {
    uint32_t magic;             /* 'SMCK' */
    uint32_t calls[8];          /* engine calls per voice, both machines */
    uint32_t trigs[8];          /* trig 1 seen per voice */
    int16_t words[8];           /* the last call's words 17..24 */
    uint32_t last_machine;      /* 1 SC FREE, 2 SC REQ */
    uint32_t nkeys;             /* key events seen */
    uint32_t keys[16];          /* the last 16: code << 16 | flags, ring by nkeys */
};
volatile struct smck_state smck = {0x534d434b};
static void eng(int32_t v, int32_t t, const int16_t *w, int32_t trig, uint32_t which)
{
    int k;
    (void)t;
    if ((uint32_t)v >= 8)
        return;
    smck.calls[v]++;
    if (trig)
        smck.trigs[v]++;
    for (k = 0; k < 8; k++)
        smck.words[k] = w[k];
    smck.last_machine = which;
}
static void e_free(int32_t v, int32_t t, const int16_t *w, int32_t trig) { eng(v, t, w, trig, 1); }
static void e_req(int32_t v, int32_t t, const int16_t *w, int32_t trig) { eng(v, t, w, trig, 2); }
static void pct(char *buf, int32_t v)
{
    int n = (v >> 8) * 100 / 127;
    int i = 0;
    if (n >= 100) buf[i++] = '1';
    if (n >= 10) buf[i++] = '0' + n / 10 % 10;
    buf[i++] = '0' + n % 10;
    buf[i++] = '%';
    buf[i] = 0;
}
const struct core_machine smck_src = {
    CORE_MACHINE(0), CORE_ENGINE_BD_CLASSIC, 0, CORE_VOICE_BD, 0, "BD SMPLREQ", "BDSR",
    {{"Level", "LEV", 0, 127, 100, 0}, {"Tune", "TUN", 0, 127, 64, 1},
     {"Decay", "DEC", 0, 127, 32, 0}, {"Hold", "HLD", 0, 127, 32, 0},
     {"Sweep Time", "SWT", 0, 127, 64, 0}, {"Sweep Depth", "SWD", 0, 127, 42, 0},
     {"Waveform", "WAV", 0, 2, 1, 0}, {"Transient", "TRA", 0, 55, 17, 0}}};
const struct smpl_machine smck_free = {
    SMPL_MACHINE(0), "SC FREE", "SCFR", 0xff, SMPL_FREE, 0, 0, e_free,
    {{"HRM", "Harmonics", 0, 0, 127 << 8, 64 << 8, 0},
     {"TIM", "Timbre", 0, 0, 127 << 8, 32 << 8, pct},
     {"MRF", "Morph", 0, 0, 127 << 8, 0, 0},
     {0, 0, 0, 0, 0, 0, 0},
     {"STA", "Start", 0, 0, 0, 0, 0},
     {"MOD", "Model", 94, 0, 15 << 8, 3 << 8, 0},
     {"LOP", "Loop", 0, 0, 0, 0, 0},
     {"LEV", "Level", 0, 0, 127 << 8, 100 << 8, 0}}};
const struct smpl_machine smck_req = {
    SMPL_MACHINE(1), "SC REQ", "SCRQ", CORE_VOICE_BD, SMPL_REQUIRED, 0, CORE_MACHINE(0), e_req,
    {{"AAA", "Knob A", 0, 0, 0, 0, 0}, {"BBB", "Knob B", 0, 0, 0, 0, 0},
     {"CCC", "Knob C", 0, 0, 0, 0, 0}, {"DDD", "Knob D", 0, 0, 0, 0, 0},
     {"EEE", "Knob E", 0, 0, 0, 0, 0}, {"FFF", "Knob F", 0, 0, 0, 0, 0},
     {"GGG", "Knob G", 0, 0, 0, 0, 0}, {"HHH", "Knob H", 0, 0, 0, 0, 0}}};

/* ev_key: log every key event's code and flags, take none */
int smck_key(void *brain, void *ev)
{
    const uint8_t *e = (const uint8_t *)ev;
    uint32_t code = *(const uint32_t *)(e + 12), flags = *(const uint32_t *)(e + 16);
    (void)brain;
    smck.keys[smck.nkeys & 15] = code << 16 | (flags & 0xffff);
    smck.nkeys++;
    return 0;
}
