#include "analog-rytm-mk2/machines.h"
struct mchk_state {
    uint32_t magic;                 /* 'MCHK' */
    uint32_t calls[8], trigs[8];
    int32_t vel[8];                 /* velocity at the last trig */
    int32_t vel_now[8];             /* velocity at the last call */
    uint16_t lev[8];                /* blk word 0 at the last call */
    uint16_t tun[8];                /* blk word 1 */
};
volatile struct mchk_state mchk = {0x4d43484b};
static void note(int32_t v, uint16_t *blk)
{
    if ((uint32_t)v >= 8) return;
    mchk.calls[v]++;
    mchk.vel_now[v] = core_voice_velocity(v);
    mchk.lev[v] = blk[0];
    mchk.tun[v] = blk[1];
    if (core_voice_trigged(v)) { mchk.trigs[v]++; mchk.vel[v] = core_voice_velocity(v); }
}
static void e44(int32_t v, void *f, uint16_t *blk, int32_t p) { note(v, blk); CORE_ENGINE_BD_CLASSIC(v, f, blk, p); }
static void e52(int32_t v, void *f, uint16_t *blk, int32_t p) { note(v, blk); CORE_ENGINE_DISABLE(v, f, blk, p); }
const struct core_machine mchk_44 = {
    44, e44, 0, CORE_VOICE_BD, 0, "BD SLOT44", "BD44",
    {{"Level", "LEV", 0, 127, 100, 0}, {"Tune", "TUN", 0, 127, 64, 1},
     {"Decay", "DEC", 0, 127, 32, 0}, {"Hold", "HLD", 0, 127, 32, 0},
     {"Sweep Time", "SWT", 0, 127, 64, 0}, {"Sweep Depth", "SWD", 0, 127, 42, 0},
     {"Waveform", "WAV", 0, 2, 1, 0}, {"Transient", "TRA", 0, 55, 17, 0}}};
const struct core_machine mchk_52 = {
    52, e52, -1, CORE_VOICE_CY, 0, "CB SLOT52", "CB52",
    {{0}, {"Tune", "TUN", 0, 127, 64, 1}, {"Two", "TWO", 0, 127, 10, 0},
     {"Three", "THR", 0, 127, 20, 0}, {0}, {0}, {0}, {"Eight", "EGT", 0, 127, 80, 0}}};
