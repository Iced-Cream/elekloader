# screen-check (Analog Four MKII, core-a4mk2 0.6)

[screen-check-ar2](../screen-check-ar2) on the A4, a test for
[core-a4mk2](../../mods/core-a4mk2) 0.6's screen calls (README, "Screen
calls (0.6)"), on screen. The keys are mod.json's cflags: FILL (taken)
steps through the four modes (off, FONTS, CALLS, COVER), RIGHT steps the
font, FLTR is the key held back and replayed through `core_key_native`, QPER
opens the progress box and NO closes it.
`check.c`'s header says what each mode draws. The CALLS bar adds `R`, the
last replay's return plus 1 (the A4's `core_key_native` returns 0 once
stock had the key). The same values are in RAM after the magic `SCC1`.
It is a test, not for a project you keep.

In rytm-emu's A4 build (11 Nov 2026) [emu], SC60 passed its plan's gate
from every start. Mode 0 left the screen stock's, pixel for pixel. Every
font drew, its width line as long as `core_text_width` said:

| font | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 | 13 | 14 | 15 | 16 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| width of the sample | 54 | 41 | 55 | 90 | 118 | 26 | 49 | 48 | 27 | 41 | 42 | 29 | 25 | 26 | 34 | 56 | 31 |
| height | 8 | 9 | 9 | 22 | 22 | 5 | 15 | 10 | 6 | 6 | 12 | 6 | 6 | 6 | 6 | 11 | 5 |
| descent | 0 | 0 | 0 | 0 | 0 | 0 | 2 | 0 | 0 | 0 | 2 | 1 | 1 | 1 | 0 | 0 | 0 |

Fonts 0, 3, 4, 7 and 9 have no lower case; 10 is symbols. FLTR's page came
about a second late with R1; the progress box opened (OV2), filled and
closed on NO; TEMPO read OV1, and KIT, SND, TRK, PTN, the song menu, GLOBAL
and FUNC + KIT OV4. In COVER the kept field was rows 0..8, columns 0..69,
for about 2 s after a LEVEL turn, but it held stock's header: the emulator
draws no level readout there.

```bash
# needs m68k-linux-gnu binutils and gcc (or set ELEKLOADER_CROSS to another prefix)
python -m elekloader.sdk.build mods/core-a4mk2 --stock Analog-Four_MKII_OS1.56.syx
python -m elekloader.sdk.build examples/screen-check-a4mk2 --stock Analog-Four_MKII_OS1.56.syx
```
