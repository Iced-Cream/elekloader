# store-check (Analog Four MKII)

A test for [core-a4](../../mods/core-a4)'s mod storage: the Analog Rytm MKII's
[store-check](../store-check-ar2) with the A4's objects. It declares a field
in every scope and does nothing until asked:

| field | scope | bits |
|---|---|---|
| `sc_track` | pattern track | 16 |
| `sc_pattern` | pattern | 32 |
| `sc_sound` | sound | 32: all core-a4 keeps a sound |
| `sc_kit_a`, `sc_kit_b` | kit | 32 each |
| `sc_global_a`, `sc_global_b` | global slot | 32 and 8 |
| `sc_project_a`, `sc_project_b` | project | 32 and 7 |

Selecting STORE CHECK in the settings menu (the GLOBAL key) writes a marker
into every field of every item, through `core_store_set`: the 6 tracks and
the pattern of each of the 128 patterns (not the working copy at the live
project +0x10, as on the Rytm), the 4 sounds of every kit in both of the
A4's kit arrays (the kits as edited and as last saved), every pool sound,
those kits, the four global slots and the project. Each marker depends on
the item and the field, and is never 0.

Seven columns at the top left of the screen then count the markers
`core_store_get` no longer reads back, left to right: pattern tracks,
patterns, kit sounds, pool sounds, kits, globals, project. Each grows a pixel
per bad marker, up to 16; globals have 8 markers and the project 2. To their
right, a bar two pixels longer for each `ev_project` event, a dot when the
last one was a load, and one dot for each scope it dropped (global, track,
pattern, project). A 3x3 square at the right end blinks while the mod runs.

It names the live objects and two firmware routines itself (core exports
none), at their 1.56 addresses.

Checked in an emulator (20 Oct 2026): see core-a4's README.

## Testing on a unit

After [hooks-check-a4](../hooks-check-a4) has passed. core-a4's README
("Testing on a unit") has the backup, the recovery path and how to send a
build. This test writes projects: use an empty project slot for it, and
keep the Transfer backup.

Reading the screen: seven columns at the top left (pattern tracks,
patterns, kit sounds, pool sounds, kits, globals, project), each as tall as
the number of markers that no longer read back (empty means all good); to
their right a bar for the project events (two pixels each), a dot under it
when the last event was a load, and up to four dots for the scopes it
dropped (global, track, pattern, project).

The A4's menus, for the steps below: GLOBAL opens the settings menu (STORE
CHECK is its last row, after SYSTEM); GLOBAL > PROJECT has LOAD PROJECT,
SAVE PROJECT AS and MANAGE PROJECTS; LOAD PROJECT lists the projects and
CREATE NEW. Loading asks "SAVE '...' BEFORE LOADING? Y/N" (answer NO) and
then "ANY CHANGES WILL BE LOST. PROCEED WITH LOADING? Y/N" (answer YES).

| step | how | expected (in rytm-emu, 20 Oct 2026) |
|---|---|---|
| 1. boot SC56 | send the build | every column full; one event, a load; on the first boot of this layout, all four drop dots |
| 2. mark | GLOBAL, scroll to STORE CHECK, YES | every column empty |
| 3. save | GLOBAL > PROJECT > SAVE PROJECT AS, an empty slot, YES (keep the name) | every column empty |
| 4. load another project | LOAD PROJECT, a project saved without mods (or with another build), NO, YES | every column full; two events, a load; drop dots for track, pattern and project (and global, if that project was never saved by this build) |
| 5. load it back | LOAD PROJECT, the slot from step 3, NO, YES | every column empty; three events; no drop dots |
| 6. power cycle | off and on | every column empty; one event, a load; no drop dots |
| 7. CREATE NEW | LOAD PROJECT, CREATE NEW, NO, YES | every column full; the event bar one longer, with no load dot (a new project) |

Then two checks of what reaches the stored project without a save (the A4
keeps the working project across power cycles, and stock writes its own
edits there part by part):

- **The current kit**: load the project from step 3 back, press STORE CHECK
  again, and power cycle without saving. The project column stays empty and
  the current kit's markers (and its 4 sounds') come back; the kits, kit
  sounds and pool columns still show the other kits' markers missing, as
  stock's own unsaved edits to other kits are lost too.
- **A pattern's tracks**: after STORE CHECK, press RECORD (grid recording),
  place two trigs on the pattern that is playing, press RECORD again, and
  power cycle without saving. In the emulator this carried that pattern's
  six track fields through the power cycle (stock's track save); the
  columns can't show one pattern, so on a unit this needs a mod that reads
  one field, or is left to the emulator.

**The listen test.** core-a4 keeps a sound's mod bytes in sound words that
stock sends to the engine with every parameter push, to the slot every
unused parameter goes to. Play a pattern with long notes on all four
synth tracks (and the CV tracks if used), note how it sounds, press STORE
CHECK (which sets those words in every sound to non-zero markers), and play
it again: it must sound exactly the same, and the CV outputs must be the
same. Repeat after loading the project back (step 5). Any difference is a
failure: report the build and the track.

```bash
# needs m68k-linux-gnu binutils and gcc (or set ELEKLOADER_CROSS to another prefix)
python -m elekloader.sdk.build mods/core-a4 --stock Analog-Four_MKII_OS1.56.syx
python -m elekloader.sdk.build examples/store-check-a4 --stock Analog-Four_MKII_OS1.56.syx
python -m elekloader.patch --stock Analog-Four_MKII_OS1.56.syx \
    --mod mods/core-a4/out/core-0.1.elemod --mod examples/store-check-a4/out/store-check-1.0.elemod \
    --out a4-store.syx --version SC56 --lockfile a4-store.lock.json
```

Keep `a4-store.lock.json`: a later build made with it reads the projects
this one saved.
