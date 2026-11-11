# payload-check (Analog Four MKII, core-a4mk2 0.6)

A test for [core-a4mk2](../../mods/core-a4mk2) 0.6's payloads and its 37.7 MB
mods' area (README, "Payloads and the mods' area (0.6)"):

- a 512 KB payload of a known pattern (`payload.s`, sidecar-check-ar2's
  part 6, with `BLOCKS` 2048), found through `core_payload` by this mod's
  id and read back whole at the first tick and every 200 ticks;
- an 8 MB `.bss` array, filled with a pattern 64 KB a tick from the first
  tick and then read back 64 KB a tick, round and round.

The record after the magic `PKC1` says where the code, the data, the array
and the payload landed (`in_area` 1 when all are inside 0x45650000 to
0x47C00000), the payload's reads and the array's passes and wrong words. On
the bottom edge: a dot at x 120 once the payload read back right, a 3x3
square at the right end once the whole array also read back right with
everything in the area, and a bar at the left when a word read back wrong.

In rytm-emu's A4 build (11 Nov 2026) [emu], PK60 passed its plan's gate
from every start: the payload at 0x4565ad20 (512 KB) and the 8 MB array
from 0x456ddce0 read back right after boot, CREATE NEW, a save and a load,
with everything inside the area.

```bash
# needs m68k-linux-gnu binutils and gcc (or set ELEKLOADER_CROSS to another prefix)
python -m elekloader.sdk.build mods/core-a4mk2 --stock Analog-Four_MKII_OS1.56.syx
python -m elekloader.sdk.build examples/payload-check-a4mk2 --stock Analog-Four_MKII_OS1.56.syx
```
