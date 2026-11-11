# save-check (Analog Four MKII, core-a4mk2 0.6)

[save-check-ar2](../save-check-ar2) on the A4: a probe for
[core-a4mk2](../../mods/core-a4mk2)'s `ev_project_save` (README, "The
project save (0.6)"). It keeps one project field, `sv_saves`, adds 1 to it
in every call before a save, and draws the same bars as the Rytm's (its
README has the table). The counts are also in RAM after the magic `SVC1`:
the calls before and after, the last `how`, whether the last call was the
one before, and `sv_saves` as the project holds it at the last tick.

In rytm-emu's A4 build (11 Nov 2026) [emu], SV60 (core 0.6, analoghealth
1.2, this mod) passed its plan's gate from every start: SAVE PROJECT AS onto
the active slot gave one kind dot (SAME), onto another two (OTHER), each
with the slot's bits and `sv_saves` one longer, and LOAD FROM showed the
count the slot was saved with. On 1.56 the PROJECT menu has no plain SAVE
PROJECT row; an empty slot offers INIT NEW (no call), SAVE TO and TOGGLE, so
no panel action gives `CORE_SAVE_NEW`. The before and after bars count
since boot, so a plan compares rows 50..55 (the field, the kind, the slot
and the dots), not those two.

```bash
# needs m68k-linux-gnu binutils and gcc (or set ELEKLOADER_CROSS to another prefix)
python -m elekloader.sdk.build mods/core-a4mk2 --stock Analog-Four_MKII_OS1.56.syx
python -m elekloader.sdk.build examples/save-check-a4mk2 --stock Analog-Four_MKII_OS1.56.syx
```
