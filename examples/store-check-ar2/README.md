# store-check (Analog Rytm MKII)

A test for [core-ar2](../../mods/core-ar2)'s mod storage (core 0.2, OS 1.74).
It declares a field in every scope and does nothing until asked:

| field | scope | bits |
|---|---|---|
| `sc_track` | pattern track | 16 |
| `sc_pattern` | pattern | 32 |
| `sc_sound` | sound | 24 |
| `sc_kit_a`, `sc_kit_b` | kit | 32 each |
| `sc_global_a`, `sc_global_b` | global slot | 32 and 8 |
| `sc_project_a`, `sc_project_b` | project | 32 and 7 |

Selecting STORE CHECK in SETTINGS writes a marker into every field of every
item, through `core_store_set`: the 13 tracks and the pattern of each of the
128 patterns (not the working pattern, which the stock OS rebuilds from the
current one), the sounds of every kit and of the pool, every kit, the four
global slots and the project. Each marker depends on the item and the
field, and is never 0.

Seven columns at the bottom left then count the markers `core_store_get` no
longer reads back, left to right: pattern tracks, patterns, kit sounds, pool
sounds, kits, globals, project. Each grows a pixel per bad marker, up to 16.
Empty columns after a save, a power cycle or loading the project back mean
everything survived; full columns after loading another project, or with a
build whose layout can't read this one, mean the data was dropped to zeros
as it should be. A 3x3 square top right blinks while the mod runs.

It names the live pattern, kit, sound and pool addresses itself (core
exports none), so it is built for 1.74 only.

Checked in an emulator only (19 Oct 2026): see core-ar2's README. Not yet
on a unit.

```bash
# needs m68k-linux-gnu binutils and gcc (or set ELEKLOADER_CROSS to another prefix)
python -m elekloader.sdk.build mods/core-ar2 --stock Analog-Rytm_MKII_OS1.74.syx
python -m elekloader.sdk.build examples/store-check-ar2 --stock Analog-Rytm_MKII_OS1.74.syx
python -m elekloader.patch --stock Analog-Rytm_MKII_OS1.74.syx \
    --mod mods/core-ar2/out/core-0.2-os1.74.elemod --mod examples/store-check-ar2/out/store-check-1.0.elemod \
    --out rytm-store.syx --version SC01 --lockfile rytm-store.lock.json
```

Keep `rytm-store.lock.json`: a later build made with it reads the projects
this one saved.
