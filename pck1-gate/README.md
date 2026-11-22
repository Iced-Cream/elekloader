# Payload hooks gate (PCK1): drive-loader-ar2 0.12, emulator only

Source: fork Iced-Cream/elekloader branch `payload-read-ar2` at **0d7a49c** (PR #63, stacked on #62
`payload-api-ar2` 14ebee0). Build `build.sh` (WSL, clone at `~/pck1-src`): stock 1.74 + core-ar2 0.18
+ drive-loader-ar2 0.12 + analoghealth 1.5. No unit, no claim, nothing pushed.

## Test mods (`testmods/`)
- **payload-check** (v1): a +Drive mod with a 225 KB `.payload` (900 blocks of sidecar-check's
  pattern). `check.c`'s header says what it does. At tick 30 it walks the table and tries the edges,
  ticks 31-61 calibrate DMA timer 0 against one second, from tick 70 it reads its payload whole twice
  in 4 KB `dl_payload_read` calls (32 a tick), checking each word and the sha256.
- **payload-check-v2**: the same id and version (so the same file name), a 200 KB payload (800
  blocks): the update put over v1 while v1 runs.
- Why 225 KB and not 530 KB: the live elkbench's mods-put refuses a file over 512 KB
  (modsteps.py MAX_FILE), and a payload goes into the .elemod as hex, twice its size. The big-put
  steps live only on the scratch branch mods-over-stage (handoff "Big-file elkbench steps", owner's
  call). The read time scales with the length; the 530 KB in-place decode is host-tested.

Expected host values: payload id `163e1d60` (FNV-1a "payload-check").
v1 len 230400, sha256 `ce765c3ef5d7463787e2a771b6614532ec51922c71edc001f61987d102bc811f`.
v2 len 204800, sha256 `5e9d34db1fc3da0ef2fc95acb9faa662185c002f9a95c49ca7cb16b9d0327e32`.

## The record `pck` (240 bytes; address from `PCK1/twin1/summary.json`, symbol `pck`)

| off | field | expected v1 | expected v2 |
|---|---|---|---|
| 0 | magic | `PCK1` | |
| 8 | done | 3 | 3 |
| 12 | abi | 1 | 1 |
| 16 | count | 1 | 1 |
| 20 | id | 0x163e1d60 | |
| 24 | found_at | 0 | 0 |
| 28 | info_rc | 0 | 0 |
| 32 | len | 230400 | 204800 |
| 36 | sha (32) | ce765c3e...811f | 5e9d34db...7e32 |
| 68 | at_past, find_bad | 0, 0 | |
| 76 | info_bad, read_end, read_past, read_bad | -1, 0, -1, -1 | |
| 92 | dt_per_s | DMA timer 0 counts in one second | |
| 96 | pass 0 (72 bytes) | | |
| 168 | pass 1 (72 bytes) | | |

A pass: +0 calls (v1 57, v2 50), +4 bytes (= len), +8 worst answer (v1 1024, v2 4096; negative
is an error), +12 short calls (0), +16 bad words (0), +20 first bad (0xffffffff), +24 dt_sum,
+28 dt_max, +32 ticks, +36 sha (32), +68 sha_ok (1). Read time = dt_sum / dt_per_s seconds; KB/s
= len / 1024 / that. All values big-endian.

## Chain (fresh emulator at F1)
1. F1 PCK1 over stock 1.74.
2. P1 put v1 into `~T PAY`, mods-active it.
3. F2 PCK1 over PCK1: the boot links payload-check.
4. P2 readout: wait ~15 s, expect-mods 1 loaded payload-check, `/mods/.status` has
   `payloads: 1 kept, 0 lost` and `payload 163e1d60 230400 /mods/~T PAY/~T payload-check-0.1.elemod`,
   stats twice, dump 240 bytes of `pck` (judge the table above). On the MODS page, DELETE on the file
   shows the refusal (MOD IN USE / IN USE: RESTART FIRST), shot.
5. In use over USB (session emulator, the `data` tool): `clear` of the file and of `~T PAY` are
   refused and `ls` still lists them.
6. P3 put v2 over it: the live pin's datarpc expects `saved`, so the step fails with the route's
   `pending restart: ~T payload-check-0.1.elemod` (expected, judged like DRC1 M2); `.status` then
   has `pending /mods/~T PAY/~T payload-check-0.1.elemod`; the record is unchanged (v1 still runs).
7. F3 PCK1 over PCK1: the boot swaps the update in.
8. P4 readout: `.status` `updates at boot: 1 applied, 0 failed`, no pending line, the payload line
   with 204800; dump `pck` (twin2's address; judge the v2 column). Then mods-active restore.
9. F4 PCK1 over PCK1 (nothing loaded, nothing held).
10. P5 mods-remove `~T PAY` (no `.new` or `.old` left).
11. F5 stock 1.74 over PCK1.
