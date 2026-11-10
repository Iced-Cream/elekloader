#!/bin/sh
# Builds the combined Sidecar test images from this branch.
# usage: tests/sidecar-combined/build.sh <stock 1.74 .syx> <out dir>
set -e
S=$1; O=$2; R=$(cd "$(dirname "$0")/../.." && pwd); H=$R/tests/sidecar-combined
mkdir -p "$O/elemods" "$O/os"; E=$O/elemods
cd "$R"
for d in mods/core-ar2 mods/smpl-machines examples/pad-check-ar2 examples/voiceio-check-ar2 \
         examples/drive-check-ar2 examples/test-looks-check-ar2 \
         "$H/check-mods/echk" "$H/check-mods/bcnt" "$H/check-mods/smck"; do
  python3 -m elekloader.sdk.build "$d" --stock "$S" --out "$E"
done
rm -rf "$E"/*.work
# analoghealth 1.3 (its own repo) goes in $E as analoghealth-1.3.elemod
C="--mod $E/core-0.13.elemod --mod $E/analoghealth-1.3.elemod"
python3 -m elekloader.patch --stock "$S" $C --mod "$E/pad-check-0.1.elemod" --mod "$E/voiceio-check-0.1.elemod" \
  --mod "$E/drive-check-0.1.elemod" --mod "$E/bcnt-0.1.elemod" --mod "$E/smpl-machines-0.1.elemod" \
  --mod "$E/smck-0.1.elemod" --out "$O/os/SCB1.syx" --lockfile "$O/os/SCB1.lock.json" --version SCB1
cp "$O/os/SCB1.lock.json" "$O/os/SCB2.lock.json"
python3 -m elekloader.patch --stock "$S" $C --mod "$E/pad-check-0.1.elemod" --mod "$E/echk-0.1.elemod" \
  --mod "$E/drive-check-0.1.elemod" --out "$O/os/SCB2.syx" --lockfile "$O/os/SCB2.lock.json" --version SCB2
cp "$O/os/SCB2.lock.json" "$O/os/SCB3.lock.json"
python3 -m elekloader.patch --stock "$S" $C --mod "$E/test-looks-check-0.1.elemod" \
  --out "$O/os/SCB3.syx" --lockfile "$O/os/SCB3.lock.json" --version SCB3
cd "$O" && sha256sum os/*.syx elemods/*.elemod
