# mark-check (Analog Four MKII, core-a4mk2 0.6)

A test for [core-a4mk2](../../mods/core-a4mk2) 0.6's modified mark and field
defaults (README, "The modified mark (0.6)", "Field defaults (0.6)"); it
is a test, not for a project you keep.

MARK CHECK (page 52) is behind the AMP key on a synth track, after stock's
AMP pages:

| knob | field | what a turn does |
|---|---|---|
| A | `mk_a`, project, init 64 | a mod page knob's turn: core marks the project modified |
| I | `mk_i`, project, init 7 | taken by the mod's `ev_enc`, +1 an event, no `core_project_edited`: the mark stays |
| J | `mk_j`, project, init 0xBEEF | the same, then `core_project_edited`: the mark comes on |

Every field has an `init`, one in each scope (track 21, pattern 300,
sound 100, kit 3000, global 60). After each `ev_project` and every 20 UI
ticks the mod counts, per scope, the items whose field doesn't read its init.

The record after the magic `MKC1` holds the mark byte (0x4045c8c2) each
tick, the ticks it was seen set, the seven counts, the three project fields,
the encoder events taken, the `core_project_edited` calls, whether MARK
CHECK is up and `ev_project`'s count and last `how`. On the screen's
bottom edge (y from the bottom row): a 5x5 square at the right end while the
mark is set, a bar at y 0..1 two pixels for each item not reading its init,
and at y 4..5 a dot for each scope with one (`check.c`'s header).

```bash
# needs m68k-linux-gnu binutils and gcc (or set ELEKLOADER_CROSS to another prefix)
python -m elekloader.sdk.build mods/core-a4mk2 --stock Analog-Four_MKII_OS1.56.syx
python -m elekloader.sdk.build examples/mark-check-a4mk2 --stock Analog-Four_MKII_OS1.56.syx
```
