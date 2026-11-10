/* Emulator check for the sidecar-engines core branch: TEST knobs as
 * destinations (slot 44 with CORE_MACHINE_DESTS), stock engines for any
 * machine, the sound a trig plays, core_param_set. */
#include "analog-rytm-mk2/machines.h"
struct echk_state {
    uint32_t magic;                 /* 'ECHK' */
    int32_t def_machine[12];        /* core_track_default_machine(t) */
    uint32_t eng_def0;              /* core_machine_engine(def_machine[0]) */
    uint32_t eng_noise;             /* core_machine_engine(15) */
    int32_t voices15;               /* core_machine_voices(15) */
    int32_t got_def0, got_15, got_44;   /* core_machine_defaults bitmasks */
    uint16_t blk_def0[8], blk_15[8], blk_44[8];
    int32_t knob44[3];              /* core_machine_knob(44, 1, ...) */
    uint32_t calls, trigs;          /* slot 44 engine calls on voice 0 */
    uint32_t sound_item[12];        /* core_track_sound(t), at the last tick */
    int32_t pool[12];               /* core_track_pool_sound(t) */
    uint32_t ticks;
    int32_t set_ret;                /* core_param_set(1, 9, 0x5500, 0) at tick 400 */
    int32_t set_bad;                /* core_param_set(1, 45, ..) : 0 expected */
};
volatile struct echk_state echk = {0x4543484b};

static void e44(int32_t v, void *f, uint16_t *blk, int32_t p)
{
    if (v == 0) {
        echk.calls++;
        if (core_voice_trigged(v))
            echk.trigs++;
    }
    /* plays the BD track's default machine with the knobs' block */
    core_engine e = core_machine_engine(core_track_default_machine(0));
    if (e)
        e(v, f, blk, p);
}

void echk_tick(void *ctrl)
{
    (void)ctrl;
    uint32_t n = ++echk.ticks;
    if (n == 2) {
        for (int t = 0; t < 12; t++)
            echk.def_machine[t] = core_track_default_machine(t);
        echk.eng_def0 = (uint32_t)core_machine_engine(echk.def_machine[0]);
        echk.eng_noise = (uint32_t)core_machine_engine(15);
        echk.voices15 = core_machine_voices(15);
        echk.got_def0 = core_machine_defaults(echk.def_machine[0], (uint16_t *)echk.blk_def0);
        echk.got_15 = core_machine_defaults(15, (uint16_t *)echk.blk_15);
        echk.got_44 = core_machine_defaults(44, (uint16_t *)echk.blk_44);
        int32_t a, b, c;
        if (core_machine_knob(44, 1, &a, &b, &c)) {
            echk.knob44[0] = a; echk.knob44[1] = b; echk.knob44[2] = c;
        }
    }
    if (n == 400) {
        echk.set_ret = core_param_set(1, 9, 0x5500, 0);
        echk.set_bad = core_param_set(1, 45, 0x1000, 0);
    }
    for (int t = 0; t < 12; t++) {
        echk.sound_item[t] = (uint32_t)core_track_sound(t);
        echk.pool[t] = core_track_pool_sound(t);
    }
}

const struct core_machine echk_44 = {
    44, e44, 0, CORE_VOICE_BD, CORE_MACHINE_DESTS, "BD ENGINES", "BDEN",
    {{"Level", "LEV", 0, 127, 100, 0}, {"Tune", "TUN", 0, 127, 64, 1},
     {"Decay", "DEC", 0, 127, 32, 0}, {"Hold", "HLD", 0, 127, 32, 0},
     {"Sweep Time", "SWT", 0, 127, 64, 0}, {"Sweep Depth", "SWD", 0, 127, 42, 0},
     {"Waveform", "WAV", 0, 2, 1, 0}, {"Transient", "TRA", 0, 55, 17, 0}}};
