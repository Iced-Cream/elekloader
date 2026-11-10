# save-check (Analog Four MKII, core-a4mk2 0.6)

[save-check-ar2](../save-check-ar2) on the A4: a probe for
[core-a4mk2](../../mods/core-a4mk2)'s `ev_project_save` (README, "The
project save (0.6)"). It keeps one project field, `sv_saves`, adds 1 to it
in every call before a save, and draws the same bars as the Rytm's (its
README has the table). The counts are also in RAM after the magic `SVC1`:
the calls before and after, the last `how`, whether the last call was the
one before, and `sv_saves` as the project holds it at the last tick.

```bash
# needs m68k-linux-gnu binutils and gcc (or set ELEKLOADER_CROSS to another prefix)
python -m elekloader.sdk.build mods/core-a4mk2 --stock Analog-Four_MKII_OS1.56.syx
python -m elekloader.sdk.build examples/save-check-a4mk2 --stock Analog-Four_MKII_OS1.56.syx
```
