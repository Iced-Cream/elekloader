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
stock's AMP pages. Encoder J asks for slot 128 with create-if-empty 1 and
ask-to-save 1; expected: a new project saved into 128, one call before and
one after, kind NEW, slot 127, `sn_saves` 1.

Since 0.3 the request is made from the page's knob handler (`p_delta`),
which stock's view dispatch (0x40062cca) calls with the view manager's
mutex held, the context stock's LOAD FROM makes the same request in. 0.1 and
0.2 made it from `ev_tick`, without that lock, and had an encoder I control
(create-if-empty 0); I froze the UI on a unit and is gone.

J makes one request a visit to the page (the first event of a turn; the
page must be left before it makes another), only while slot 128 is empty: a
full slot would be loaded, so the mod refuses and counts it. Slot 128's
state is read without the storage lock (the byte 0x40072fa2 reads under it).
The new project the job saves is named by stock (not `~T`); a plan clears
slot 128 with the rest of 121-128.

Like [save-check](../save-check-a4mk2) the mod keeps a project field,
`sn_saves`, adds 1 to it before each save and draws its bars on the bottom
edge (y from the bottom row): y 0..1 calls before, y 4..5 calls after, y
8..9 `sn_saves`, and at y 12..13 the last call's kind (1..3 dots from x 0),
the slot's 8 bits from x 15, a failed dot at x 45, a dot at x 52 when the
request's `p_delta` ran inside the view dispatch and at x 55 when its task
held the dispatch's mutex, J requests (x 70..79) as dots, a refused dot at x
100, a dot at x 110 after a `CORE_PROJECT_NEW` since the last request, a dot
at x 120 when the last call was the one before, and a dot at x 126 always.
It draws nothing while a whole-screen stock menu is up (GLOBAL, MANAGE
PROJECTS), so a plan can read slots 127 and 128 in stock's list.

The counts are in RAM after the magic `SNC1` (`check.c`'s header), with
breadcrumbs that place a request that never returns: a stage (0 the
request's start, 1 before the slot read, 2 after it, 3 before 0x4003f7cc, 4
after it, 5 at the next draw), `sn_tick`'s nesting and the ticks that ran
inside the request, the job queue's busy byte, the requesting task and the
mutex owner it saw, and the storage and job queue singletons, so a plan can
read their mutexes' owners when a turn times out.

In rytm-emu's A4 build (12 Nov 2026) [emu], 0.3 (SN63) passed its plan's
gate from every start: J's `p_delta` ran inside the view dispatch (return
address 0x40062d3e on the stack, its vm the tick's ctrl) with the requesting
task owning vm+4; stock asked SAVE ... BEFORE LOADING (NO) and PROCEED (YES),
made a new project, saved it into 128 and core reported `CORE_SAVE_NEW` for
slot 127, with `sn_saves` 1. A fault build for each check stopped in that
check, and one that blocks the UI task at stage 3 timed the turn out as
the unit did, with SNC1 and the mutexes still readable over analoghealth. With
the job queue forced busy the request pushed the WAIT box and still
completed. 0.2 (SN62) froze a unit on its first I turn. On a unit (12 Nov
2026), 0.3 (SN63) passed the same plan: the request ran inside the view
dispatch with its mutex held, and core reported `CORE_SAVE_NEW` for slot 127.

```bash
# needs m68k-linux-gnu binutils and gcc (or set ELEKLOADER_CROSS to another prefix)
python -m elekloader.sdk.build mods/core-a4mk2 --stock Analog-Four_MKII_OS1.56.syx
python -m elekloader.sdk.build examples/savenew-check-a4mk2 --stock Analog-Four_MKII_OS1.56.syx
```
