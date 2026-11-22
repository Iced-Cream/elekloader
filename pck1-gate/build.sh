#!/bin/bash
# PCK1: stock 1.74 + core-ar2 0.18 + drive-loader-ar2 0.12 (the payload hooks: fork PRs #62 + #63,
# branch payload-read-ar2 0d7a49c) + analoghealth 1.5 (the harness). The DRC1/DFB1 recipe.
# Run in WSL with the source at ~/pck1-src (git checkout of 0d7a49c). Emulator gate only, no unit run.
set -u
SRC=~/pck1-src
rm -rf /tmp/pck1-src && mkdir /tmp/pck1-src && (cd $SRC && git archive HEAD | tar -x -C /tmp/pck1-src && git log --oneline -3 > /tmp/pck1-src/REV)
cd /tmp/pck1-src
export PYTHONPATH=/tmp/pck1-src
S=/mnt/d/Development/Projects/ARMKII/Analog-Rytm_MKII_OS1.74.syx
W=/mnt/d/Development/Projects/ARMKII/work/drive-loader/payload-hooks
AH=/mnt/d/Development/Projects/ARMKII/work/core-ar2/core-ar2-0.18-std1-unit/build-ladder/mods/analoghealth-1.5.elemod
O=$W/PCK1
rm -rf "$O"; mkdir -p "$O"
cp REV "$O/REV"
python3 -m elekloader.sdk.build mods/core-ar2 --stock "$S" --out "$O" 2>&1 | tail -3; echo "build core exit ${PIPESTATUS[0]}"
python3 -m elekloader.sdk.build mods/drive-loader-ar2 --stock "$S" --out "$O" 2>&1 | tail -3; echo "build loader exit ${PIPESTATUS[0]}"
cp "$AH" "$O/"
cd "$O"; C=$(ls core-*.elemod); L=$(ls drive-loader-*.elemod); cd /tmp/pck1-src
python3 -m elekloader.lint --stock "$S" --json "$O/$C" > "$O/lint-core.json"; echo "lint core exit $?"
python3 -m elekloader.lint --stock "$S" --with "$O/$C" --json "$O/$L" > "$O/lint-loader.json"; echo "lint loader exit $?"
python3 -m elekloader.lint --stock "$S" --with "$O/$C" --json "$O/analoghealth-1.5.elemod" > "$O/lint-analoghealth.json"; echo "lint analoghealth exit $?"
python3 -m elekloader.patch --stock "$S" --mod "$O/$C" --mod "$O/$L" --mod "$O/analoghealth-1.5.elemod" --out "$O/PCK1.syx" --version PCK1; echo "patch exit $?"
python3 -m elekloader check "$O/PCK1.syx"; echo "check exit $?"
python3 -m elekloader.desc dump "$O/PCK1.syx" > "$O/desc.txt" 2>&1; echo "desc exit $?"
grep -o "payload skip\|drive_payload_abi\|dl_payload_read" "$O/desc.txt" | sort | uniq -c
# test mods: built, named "~T <file>". payload-check (225 KB payload) and payload-check-v2: the
# same id and version, so the same file name, with a 200 KB payload: the update put over the first
M=$O/testmods; mkdir -p "$M/v1" "$M/v2"
for T in payload-check:v1 payload-check-v2:v2; do
  D=${T%%:*}; V=${T##*:}
  rm -rf $W/testmods/$D/out
  python3 -m elekloader.sdk.build $W/testmods/$D --stock "$S" > "$M/$D.build.log" 2>&1 || { echo "BUILD $D failed"; tail -20 "$M/$D.build.log"; exit 1; }
  for f in $W/testmods/$D/out/*.elemod; do cp "$f" "$M/$V/~T $(basename $f)"; done; rm -rf $W/testmods/$D/out
done
ls -l "$M"/v1 "$M"/v2
cd /tmp/pck1-src
ck() { echo "== $1"; shift; python3 -m elekloader check "$O/PCK1.syx" "$@"; echo rc=$?; }
{
ck "payload-check v1 alone" "$M/v1/~T payload-check-0.1.elemod"
ck "payload-check v2 alone" "$M/v2/~T payload-check-0.1.elemod"
echo "== twin (link, pass 2): v1"; rm -rf "$O/twin1"; python3 -m elekloader link --device-layout "$O/PCK1.syx" "$M/v1/~T payload-check-0.1.elemod" --out "$O/twin1"; echo rc=$?; cat "$O/twin1/summary.json" 2>/dev/null | head -c 3000; echo
echo "== twin (link, pass 2): v2"; rm -rf "$O/twin2"; python3 -m elekloader link --device-layout "$O/PCK1.syx" "$M/v2/~T payload-check-0.1.elemod" --out "$O/twin2"; echo rc=$?; cat "$O/twin2/summary.json" 2>/dev/null | head -c 3000; echo
} > "$O/hostcheck.log" 2>&1; cat "$O/hostcheck.log"
for t in test_units test_link_refusals test_drive test_drive_payload test_drive_payload_skip test_drive_route test_drive_page; do python3 tests/$t.py > "$O/test-$t.log" 2>&1; echo "$t exit $? : $(tail -1 "$O/test-$t.log")"; done
ELEKLOADER_AR2_SYX="$S" python3 tests/test_rytm_mk2.py > "$O/test-test_rytm_mk2.log" 2>&1; echo "test_rytm_mk2 exit $? : $(tail -1 "$O/test-test_rytm_mk2.log")"
cd "$O" && sha256sum *.syx *.elemod testmods/*/*.elemod > SHA256SUMS && cat SHA256SUMS
