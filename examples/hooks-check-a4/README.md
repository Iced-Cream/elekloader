# hooks-check (Analog Four MKII)

A test for [core-a4](../../mods/core-a4): the Analog Rytm MKII's
[hooks-check](../hooks-check-ar2) (the same `hooks.c` and `label.s`) built
with the A4's `fill_rect` (0x40059f1a) and `std::string` constructor
(0x4018471e), each found by masked matching against the Rytm 1.74's and
checked by its callers. What each bar and square means is in
[hooks-check-ar2's README](../hooks-check-ar2/README.md).

Not yet run on a unit. In rytm-emu's A4 build (20 Oct 2026, build HC56,
with an A4 "modified image" fallback added to the emulator): the tick ran
at about 20 Hz and its square blinked, each key press counted twice (down
and up) and each repeat once, the encoder counted every detent the
emulator delivered, the two render counts never differed, and SETTINGS
(GLOBAL) listed HOOK TEST last, after SYSTEM, whose selection counted.
hello-marker-a4 (HM56) differed from stock only in its 3x3 corner on every
screen taken.

## Testing on a unit

After [hello-marker-a4](../hello-marker-a4) has booted. core-a4's README
("Testing on a unit") has the backup, the recovery path and how to send a
build. On the A4 the bars draw over the top 16 rows of the screen (the top
bar's text shows through them), and the squares are at the top right:

| what | where | moves when |
|---|---|---|
| `ev_tick` | 3x3 square, top right | blinks about once a second, all the time |
| `ev_key` | the first bar, at the very top | any key: press and release add one each, a repeat one |
| `ev_enc` | the second bar | any encoder detent (DATA ENTRY A..J and LEVEL) |
| `ev_settings` | the HOOK TEST row, last in the GLOBAL menu, and the third bar | the row is listed after SYSTEM; selecting it adds one |
| `ev_render_in` | the fourth bar | always: one pixel per 64 audio blocks, wrapping |
| `ev_render_out` | a second square, below the first | never: only if the two render counts ever differ |

1. Send the HC56 build. It boots with the bars at the top.
2. Watch for ten seconds without touching anything: the tick square blinks
   and the render bar keeps moving; no second square.
3. Press a few keys (TRIG keys, TRACK keys, the page keys): the key bar
   grows two pixels a press, and each key does what it does on stock.
4. Turn each DATA ENTRY knob and the LEVEL knob: the encoder bar grows a
   pixel a detent, and each knob edits its parameter as on stock.
5. Press GLOBAL: the menu lists PROJECT, GLOBAL SLOT, MIDI CONFIG, CV
   CONFIG, SYSEX DUMP, AUDIO ROUTING, (OUTBOX 8 if connected), CONTROL,
   SYSTEM and HOOK TEST last. Scroll to HOOK TEST and press YES twice: the
   settings bar grows two pixels and nothing else happens. The other rows
   open as on stock.
6. Play a pattern through the effects for a few minutes, with the CV outputs
   in use if you have them: the sound and the CV are stock's, the render bar
   moves steadily, and no second square appears.
7. Power cycle: everything comes back as it was.

```bash
# needs m68k-linux-gnu binutils and gcc (or set ELEKLOADER_CROSS to another prefix)
python -m elekloader.sdk.build mods/core-a4 --stock Analog-Four_MKII_OS1.56.syx
python -m elekloader.sdk.build examples/hooks-check-a4 --stock Analog-Four_MKII_OS1.56.syx
python -m elekloader.patch --stock Analog-Four_MKII_OS1.56.syx \
    --mod mods/core-a4/out/core-0.1.elemod --mod examples/hooks-check-a4/out/hooks-check-0.1.elemod \
    --out a4-hooks.syx --version HC56
```
