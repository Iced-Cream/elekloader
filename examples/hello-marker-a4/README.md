# hello-marker (Analog Four MKII)

The smallest mod that runs code on the Analog Four MKII: the shared
[hello-marker](../hello-marker) source (`hello.c`) built with the A4's
`fill_rect` (0x40059f1a), on [core-a4](../../mods/core-a4). It draws a small
white square in the top right corner of every screen, through `ev_draw`, and
changes nothing else. A template to start from (docs/ADAPTING.md).

In rytm-emu's A4 build (20 Oct 2026, build HM56): it booted as stock and
differed from stock only at its square (OLED x 125..127, rows 0..2) on the
main page, the OSC1 page, after edits and in the settings menu. Not yet run
on a unit.

## Testing on a unit

The first build to send: it shows that a build boots and that the mods' code
runs at all. core-a4's README ("Testing on a unit") has the backup, the
recovery path and how to send a build.

1. Build and patch as below, with `--version HM56`, and send `a4-hello.syx`
   with GLOBAL > SYSTEM > OS UPGRADE and Transfer over USB.
2. The unit reboots into the build. Expected: it starts as stock does, with
   a 3x3 white square in the top right corner.
3. Visit a few screens (the SYNTH pages with OSC1, OSC2, FILTERS, AMP, ENV,
   LFO; KIT; the GLOBAL menu; a pattern playing): the square stays on every
   one, and nothing else differs from stock.
4. Power cycle: the build comes back with the square.

If it doesn't boot, recover from the STARTUP menu (hold FUNC at power-on,
TRIG 4, the stock .syx over MIDI DIN).

```bash
# needs m68k-linux-gnu binutils and gcc (or set ELEKLOADER_CROSS to another prefix)
python -m elekloader.sdk.build mods/core-a4 --stock Analog-Four_MKII_OS1.56.syx
python -m elekloader.sdk.build examples/hello-marker-a4 --stock Analog-Four_MKII_OS1.56.syx
python -m elekloader.patch --stock Analog-Four_MKII_OS1.56.syx \
    --mod mods/core-a4/out/core-0.1.elemod --mod examples/hello-marker-a4/out/hello-marker-1.0.elemod \
    --out a4-hello.syx --version HM56
```
