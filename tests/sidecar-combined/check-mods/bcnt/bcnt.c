#include "analog-rytm-mk2/machines.h"
#include "analog-rytm-mk2/pages.h"
CORE_MACHINES;
CORE_PAGES;
struct bcnt_state { uint32_t magic; uint32_t calls[8]; };
volatile struct bcnt_state bcnt = {0x42434e54};   /* 'BCNT' */
static void e0(int32_t v, void *f, uint16_t *blk, int32_t p)
{ if ((uint32_t)v < 8) bcnt.calls[v]++; CORE_ENGINE_BD_CLASSIC(v, f, blk, p); }
static void e1(int32_t v, void *f, uint16_t *blk, int32_t p)
{ if ((uint32_t)v < 8) bcnt.calls[v]++; CORE_ENGINE_SD_CLASSIC(v, f, blk, p); }
const struct core_machine bcnt_m0 = {
    CORE_MACHINE(0), e0, 0, CORE_VOICE_BD, 0, "BD BYCOUNT8", "BDC8",
    {{"Level", "LEV", 0, 127, 100, 0}, {"Tune", "TUN", 0, 127, 64, 1},
     {"Decay", "DEC", 0, 127, 32, 0}, {"Hold", "HLD", 0, 127, 32, 0},
     {"Sweep Time", "SWT", 0, 127, 64, 0}, {"Sweep Depth", "SWD", 0, 127, 42, 0},
     {"Waveform", "WAV", 0, 2, 1, 0}, {"Transient", "TRA", 0, 55, 17, 0}}};
const struct core_machine bcnt_m1 = {
    CORE_MACHINE(1), e1, 1, CORE_VOICE_SD, 0, "SD BYCOUNT4", "SDC4",
    {{"Level", "LEV", 0, 127, 100, 0}, {"Tune", "TUN", 0, 127, 64, 1},
     {"Decay", "DEC", 0, 127, 40, 0}, {"Hold", "HLD", 0, 127, 20, 0}}};
const struct core_page bcnt_page = {CORE_PAGE(0), "BC PAGE", {0, 0, 0, 0, 0, 0, 0, 0}};
const struct core_view_page bcnt_view = {CORE_VIEW_COMP, CORE_PAGE(0), 0};
