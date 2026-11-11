#!/bin/sh
# Builds the Sidecar findings test image SCF3 from this branch: the confirm image
# (core, smpl-machines 0.3, smpl-knob-c, the check mods) with the USB input gain and
# its check mod.
# usage: tests/sidecar-confirm/build.sh <stock 1.74 .syx> <out dir> [<lockfile to start from>]
# analoghealth 1.3 (its own repo) goes in <out dir>/elemods as analoghealth-1.3.elemod
set -e
S=$1; O=$2; R=$(cd "$(dirname "$0")/../.." && pwd); H=$R/tests/sidecar-combined
mkdir -p "$O/elemods" "$O/os"; E=$O/elemods
cd "$R"
for d in mods/core-ar2 mods/smpl-machines examples/voiceio-check-ar2 examples/usb-gain-check-ar2 \
         mods/smpl-knob-c "$H/check-mods/bcnt" "$R/tests/sidecar-confirm/check-mods/smck"; do
  python3 -m elekloader.sdk.build "$d" --stock "$S" --out "$E"
done
rm -rf "${E:?}"/*.work
[ -n "$3" ] && cp "$3" "$O/os/SCF3.lock.json"
python3 -m elekloader.patch --stock "$S" --mod "$E/core-0.13.elemod" --mod "$E/analoghealth-1.3.elemod" \
  --mod "$E/voiceio-check-0.1.elemod" --mod "$E/bcnt-0.1.elemod" --mod "$E/smpl-machines-0.3.elemod" \
  --mod "$E/smpl-knob-c-0.1.elemod" --mod "$E/smck-0.2.elemod" --mod "$E/usb-gain-check-0.2.elemod" \
  --out "$O/os/SCF3.syx" --lockfile "$O/os/SCF3.lock.json" --version SCF3
cd "$O" && sha256sum os/*.syx elemods/*.elemod
