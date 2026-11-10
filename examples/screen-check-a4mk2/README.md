# screen-check (Analog Four MKII, core-a4mk2 0.6)

[screen-check-ar2](../screen-check-ar2) on the A4, a test for
[core-a4mk2](../../mods/core-a4mk2) 0.6's screen calls (README, "Screen
calls (0.6)"), on screen. The keys are mod.json's cflags: `K_MODE` (taken)
steps through the four modes (off, FONTS, CALLS, COVER), `K_RIGHT` steps
the font, FILTERS is the key held back and replayed through
`core_stock_key`, `K_BOX` opens the progress box and `K_NO` closes it.
`check.c`'s header says what each mode draws. The CALLS bar adds `R`, the
last replay's return plus 1 (the A4's `core_stock_key` returns 0 once
stock had the key). The same values are in RAM after the magic `SCC1`.
It is a test, not for a project you keep.

```bash
# needs m68k-linux-gnu binutils and gcc (or set ELEKLOADER_CROSS to another prefix)
python -m elekloader.sdk.build mods/core-a4mk2 --stock Analog-Four_MKII_OS1.56.syx
python -m elekloader.sdk.build examples/screen-check-a4mk2 --stock Analog-Four_MKII_OS1.56.syx
```
