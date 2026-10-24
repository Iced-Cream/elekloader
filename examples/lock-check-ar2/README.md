# lock-check (Analog Rytm MKII)

A test for [core-ar2](../../mods/core-ar2)'s lockable parameters (core 0.8).
It declares two sound fields with `"lock": true` and registers them in
`core_lock_params`:

| field | bits | code (from a new lockfile) | DST menu | DST value | range |
|---|---|---|---|---|---|
| `lc_a` | 7 | 42 | MOD:LOCK CHECK A | LCA | 0..127, page knob id 1 |
| `lc_b` | 4 | 43 | MOD:LOCK CHECK B | LCB | 0..15 |

A bar along the bottom edge shows the offset track 1's LFO gives either one
(`core_lfo_offset`), from the middle, a pixel per stock knob step; a small
square at the left end shows which (top A, bottom B). Behind the LFO key a
LOCK CHECK page shows `lc_a` of track 1's sound in the current kit on knob A
(borrowing the dead record id 1), so RANDOMIZE PAGE ([LFO]+[YES]) and
RELOAD PAGE ([LFO]+[NO]) can be tried on it.

```bash
python -m elekloader.sdk.build mods/core-ar2 --stock Analog-Rytm_MKII_OS1.74.syx
python -m elekloader.sdk.build examples/lock-check-ar2 --stock Analog-Rytm_MKII_OS1.74.syx
python -m elekloader.patch --stock Analog-Rytm_MKII_OS1.74.syx \
    --mod mods/core-ar2/out/core-0.8.elemod --mod examples/lock-check-ar2/out/lock-check-1.0.elemod \
    --out rytm-lock.syx --version LK08
```

The knob's text is stock's for id 1 (ERR): the example shows only the value.
