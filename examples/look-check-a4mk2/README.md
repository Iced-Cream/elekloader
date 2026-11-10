# look-check (Analog Four MKII, core-a4mk2 0.6)

A test for [core-a4mk2](../../mods/core-a4mk2) 0.6's knob looks
(`core_knob_looks`; README, "Knob looks (0.6)"), look-check-ar2's on the A4
with its table found on the unit rather than generated from a RAM dump.

At the first tick after stock's first draw, the mod groups stock's ids
1..0x126 by what makes a knob's look (the UI record's flags, turn steps and
the code words of its four delegates, as look-check-ar2's `gen_looks.py`
does) and gives own knob k (id 0x220 + k) a `core_knob_looks` entry whose
look is group k's first id. Two more own knobs follow: one whose record
comes from a `ui` callback (a copy of the second group's) and one with the
look of the first group whose range is 0..127, given a range of 0..12.

LOOK CHECK (page 53, behind the AMP key on a synth track) shows four pairs,
A|B to G|H, a stock id on the left and its own knob on the right, with the
same value and stock's text for both; knob J chooses the four (`P` and the
page, from 0). An own id's label is record 0's on the A4, so a pair's cells
match in the graphic and the value text, not the label.

The record after the magic `LKA1` holds the groups' ids, the own knobs, the
page, and for each own knob what core's UI record and range lookups answer
against what its entry gives (`ok`, and `bad` the count of those not
ok). It is a test, not for a project you keep.

```bash
# needs m68k-linux-gnu binutils and gcc (or set ELEKLOADER_CROSS to another prefix)
python -m elekloader.sdk.build mods/core-a4mk2 --stock Analog-Four_MKII_OS1.56.syx
python -m elekloader.sdk.build examples/look-check-a4mk2 --stock Analog-Four_MKII_OS1.56.syx
```
