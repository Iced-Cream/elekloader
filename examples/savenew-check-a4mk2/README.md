# savenew-check (Analog Four MKII, core-a4mk2 0.6)

A test for [core-a4mk2](../../mods/core-a4mk2)'s `CORE_SAVE_NEW` (README,
"The project save (0.6)"), which no panel action reaches on 1.56: stock's
project load request (0x4003f7cc) carries a create-if-empty flag that every
stock caller sets to 0. This mod makes the same request itself, with the
flag set, so the load job takes its dead branch: a new project
(`ev_project`, `CORE_PROJECT_NEW`), then the whole save into the empty slot,
which core reports as `CORE_SAVE_NEW`. It is a test, not for a project you
keep.

SAVE NEW CHECK (page 53) is behind the AMP key on a synth track, after
stock's AMP pages:

| encoder | request for slot 128 | expected |
|---|---|---|
| I | create-if-empty 0, ask-to-save 1 (stock's LOAD FROM) | the job finds the slot empty and fails: no save call, the slot stays empty |
| J | create-if-empty 1, ask-to-save 1 | a new project saved into 128: one call before and one after, kind NEW, slot 127, `sn_saves` 1 |

Each encoder makes one request a visit to the page (the first event of a
turn; the page must be left before it makes another), only while slot 128
is empty: a full slot would be loaded, so the mod refuses and counts it. The
new project the job saves is named by stock (not `~T`); a plan clears slot
128 with the rest of 121-128.

Like [save-check](../save-check-a4mk2) the mod keeps a project field,
`sn_saves`, adds 1 to it before each save and draws its bars on the bottom
edge (y from the bottom row): y 0..1 calls before, y 4..5 calls after, y
8..9 `sn_saves`, and at y 12..13 the last call's kind (1..3 dots from x 0),
the slot's 8 bits from x 15, a failed dot at x 45, the I requests (x 52..61)
and J requests (x 70..79) as dots, a refused dot at x 100, a dot at x 110
after a `CORE_PROJECT_NEW` since the last request, a dot at x 120 when the last call was the one
before, and a dot at x 126 always. The counts are in RAM after the magic
`SNC1` (`check.c`'s header). Since 0.2 it draws nothing while a
whole-screen stock menu is up (GLOBAL, MANAGE PROJECTS), so a plan can read
slots 127 and 128 in stock's list.

In rytm-emu's A4 build (12 Nov 2026) [emu], 0.2 (SN62) passed its plan's
gate from every start: J asked SAVE ... BEFORE LOADING (NO) and PROCEED
(YES), then stock made a new project, saved it into 128 (named NEW
PROJECT, then the active slot) and core reported `CORE_SAVE_NEW` for slot
127, with `sn_saves` 1 and the new-project dot; I asked nothing and saved
nothing, and 128 stayed empty. A fault build for each check (J passing 0,
NEW drawn as OTHER, `sn_saves` not counted, the empty-slot guard inverted)
stopped in that check. The before and after bars count since boot, so a
plan compares rows 50..55. Not yet run on a unit.

```bash
# needs m68k-linux-gnu binutils and gcc (or set ELEKLOADER_CROSS to another prefix)
python -m elekloader.sdk.build mods/core-a4mk2 --stock Analog-Four_MKII_OS1.56.syx
python -m elekloader.sdk.build examples/savenew-check-a4mk2 --stock Analog-Four_MKII_OS1.56.syx
```
