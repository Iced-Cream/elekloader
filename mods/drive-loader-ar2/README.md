# drive-loader (Analog Rytm MKII)

The +Drive mods loader's flashed part, for core-ar2 0.11 and newer on OS 1.74. It imports
`core_desc`, so the linker writes the build descriptor into every build that has it
(docs/FORMAT.md, "The build descriptor"), and it exports the staging buffer as `drive_staging`
and `drive_staging_size` (512 KB, its `.bss`), which the descriptor's area record names.

From 0.1 it holds the `/mods` route, links the mods `/mods/active` names into the +Drive
mods' area at boot, and adds the MODS page in SETTINGS > GLOBAL, which writes `/mods/active`.
The rules (what a +Drive mod may hold, the checks, the order, the refusal
codes and skip reasons) are docs/FORMAT.md's, "+Drive mods".

## The /mods route

Stock's Data router (the one Transfer and Elektroid use for `/projects` and `/soundbanks`) gets
a third `RouteTypeHandler`, so the unit's Data root listing shows `mods`:

| path | what |
|---|---|
| `/mods` | the files in the +Drive's `/mods` folder (`DL_DIR` in `fs.h`), then `.build` and `.status` |
| `/mods/<name>` | read, upload or Clear one file. Uploads take `*.elemod` and `active` only. Other files there can be read and cleared |
| `/mods/.build` | read only: the build descriptor, byte for byte (`python -m elekloader.desc dump`) |
| `/mods/.status` | read only: plain text, the route's last commit or delete, then the boot linker's records |

Each transfer goes through stock's data container (31-byte header, body, 12-byte footer with
CRC-32). An upload's header must say family 2, file type `0x454d4f44` (`'EMOD'`), not
compressed, and a payload of at most 512 KB. Anything else is refused at the header, and
nothing is written. The router checks the CRC at the footer, and only a transfer that passes
every check reaches the commit: the body goes to `<name>.tmp`, the old file is deleted, and the
tmp file is renamed over it.

A build for a unit test run sets `-DDL_TEST_DIR` (mod.json `cflags`), which makes the folder
`/~T MODS`. The test rules allow only `~T` items.

### How it hooks stock

- One site, `0x400a005e`, in the resolver's getter (`0x4009febc`): after stock adds its two
  handlers, `dl_route_site` sets the getter's guard (the instruction it displaces) and adds the
  loader's handler with stock's `0x4002da4a`, which takes a `shared_ptr` to it (one with no control block: the handler is never freed). That calls the handler's `register_route`, which
  adds `/mods` to the list table and `/mods/*` to the entry table.
- `/mods/*` resolves to a RAM entry (kind 1) over the staging buffer. Its functions do the rest:
  `+0x1c` refuses with "busy" while a loader part holds the buffer (`dl_staging_take`); `+0x2c`
  fills the buffer from the file once a read stream exists; `+0x6c` checks the container's
  header; `+0x4c` is stock's after-change call. WriteClose calls it with the header after a
  successful upload, which is when the route commits. Clear calls it with no header, which is
  when the route deletes the file.
- The route can't tell a read, an upload and a Clear apart when it resolves a path, so the
  entry carries every function, each closure holding the file's name.
- Stock's Rename refuses RAM entries, so `/mods` has no rename. A new upload under the same
  name replaces the file.
- One transfer at a time per Data handler: stock drops an open transfer when the next one
  opens, and a dropped upload never commits.
- Calls come from the SysEx receive tasks. The loader's mutex (`dl_lock`) covers the staging
  buffer and each commit, delete and fill. ekFS takes its own mutex inside each call.

### Firmware calls

All addresses are OS 1.74's. They were read in Ghidra.

| what | address |
|---|---|
| resolver, its getter's guard | `0x419438cc`, `0x419438c8` |
| add a handler | `0x4002da4a` |
| pattern compile (a route's entry is 0x1c bytes: the pattern's segments, a std::function; the route grows a table itself) | `0x4002f1d0` |
| std::string ctor, dtor, the empty string | `0x401bcff0`, `0x401bb540`, `0x419f2a0c` |
| operator new, delete | `0x40082a40`, `0x40082a4c` |
| ekFS file ctor (0x2c bytes; 0 read, 1 write), dtor, read, write, node, node size (less 16, for FsSample's own header) | `0x4007dcfa`, `0x4007d956`, `0x4007d822`, `0x4007d860`, `0x4007d9d2`, `0x4007d9ec` |
| ekFS delete, rename | `0x4007da78`, `0x4007da8c` |
| Directory ctor (0x130 bytes), dtor, count, child, shared_ptr release | `0x4007ed48`, `0x4007ee02`, `0x4007e1bc`, `0x4007e794`, `0x40170bd8` |
| mutex lock, unlock | `0x400010bc`, `0x400011ee` |

## Linking at boot

One more site, `0x401009e0` in `main()` (`0x401006ee`), stock `jsr 0x4009da0e`: `drive_boot`
(`flush.s`) makes that call, then runs `dl_boot` (`boot.c`). There ekFS is mounted and the audio
start and the UI task haven't run, so nothing reads the tables the link rewrites.

- **Skip.** Nothing links when the eMMC (`0x419ae2c0`) or ekFS (`0x419ae2bc`) didn't come up;
  when [NO] is held (the panel query `61 01`, 20 ms, the key bitmap `0x418d1340` through
  `0x40081c28`, then `60 00` again); or when `/mods/.armed` is there, which it deletes.
- **The link** (`dlink.c`, with the JSON reader `json.c`) reads one file at a time into the
  staging buffer and works from `core_desc` only: pass 1, the checks in id order, the layout,
  pass 2 and the tables' merge. A pass-2 refusal starts the checks again without that mod. It
  writes the status records `/mods/.status` shows.
- **The cache.** `dl_flush` pushes every data-cache line by set and way (the V4's CPUSHL takes
  an index, not an address), then sets CACR to `0xA40CE100`: stock's value without DCINVA.
- **The crash guard.** With a mod loaded, it writes `/mods/.armed`, calls each `drive_init` in
  id order, and deletes `.armed` 200 `ev_tick`s (about 10 s) later.

`tests/drive_link_host.c` builds the same linker for the computer, and
`tests/test_drive_link.py` checks it on a made-up Rytm build, and checks that the MODS page's
`dl_check` gives the same verdicts without writing RAM.

| what | address |
|---|---|
| panel lock, send, unlock; sleep (µs); key held | `0x40001bf6`, `0x400019e4`, `0x40001c06`; `0x40098c84`; `0x40081c28` |

## The MODS page

A MODS row at the bottom of SETTINGS > GLOBAL (`ev_settings`, `core_additem`) opens a page
(`page.c`, `page.s`) listing the `.elemod` files in the folder, one row each, with a tick.

- **The rows.** The first row is the state: how many mods the boot loaded, or why it skipped;
  after a change, `SAVED: REBOOT`, `NOT SAVED`, `SAVE FAILED` or `BUSY: TRY AGAIN`; with no
  files, `NO MODS ON +DRIVE`. Each file's row shows its id, alternating every half second with
  what applies first: why the set on the page is refused for it, why the file itself is (a bad
  file, another device, a newer core), `LOADED`, `BOOT:` and why the boot refused it,
  `ON NEXT BOOT`, `OFF NEXT BOOT`, or its version and size.
- **A change.** [YES] on a row toggles it, and the page runs the boot's own pass 1, checks and
  layout (`dl_check`, `dlink.c`) on the new set against `core_desc`, without writing RAM. A
  set where every ticked mod would load is written to `active` (`active.tmp`, then the old
  file deleted and the tmp renamed), one line per file, its name and CRC-32. A set that
  wouldn't is not saved, and the refused row says why. The page never loads or unloads code:
  the next boot links the saved set.
- **The boot's verdicts** come from the boot's own records, matched by file name
  (`dl_boot_choice`), so a second file with a loaded mod's id isn't shown as loaded.
- **Reading files.** The page reads each file into the staging buffer, which it takes only
  while it reads: at its opening and at each change. The route answers `busy` then, and only
  then. A separate buffer for the UI task would cost another 512 KB, or limit what the page
  can check. Every ekFS call is made under `dl_lock`.
- **Keeping off transfers.** Stock streams a read out of the buffer after the route fills it,
  and makes no call at its end, so the page can't see when a read is over. The route stamps
  the time of each transfer's start (`dl_pre` sets `dl_read_at` under `dl_lock`), and the page
  doesn't take the buffer for 10 s after one (`dl_staging_take_quiet`, 200 `ev_tick`s): it
  shows `BUSY: TRY AGAIN` and saves nothing. A page check that overlapped a read would hand the
  client the page's bytes under a good CRC (in the emulator, 6 of 90 reads during repeated
  ticks before this rule; none of 90 after it).
- **An upload that met a check.** The route counts each time a loader part takes the buffer
  (`dl_staging_epoch`, `fs.c`). An upload whose body went to the buffer while the page held it
  is refused at the commit, so a file is never written from bytes the page overwrote.

What the page doesn't see:

- Pass 2's refusals (a `pc16` out of range) are only found by the boot's link: the page saves
  such a set, and the row shows `BOOT:` and the reason after the restart.
- A transfer longer than 10 s (a large file over a slow link) can still meet a page check.

| what | address |
|---|---|
| MenuItem ctor (0x54 bytes), `Menu::addItem` | `0x40072fbc`, `0x400727f2` |
| std::function manager for a plain pointer | `0x4019411a` |
| VerticalSettingsMenuView ctor `(this, name, rows, &title, icon, height, width)`, 0x194 bytes, its View at `+0x38` | `0x400780e8` |
| the view's `shared_ptr<View>` `(parent, view + 0x38)`, returned in `%a0` | `0x4007762c` |
| show a child `(parent, &shared_ptr)`: kept in the parent's slot `+0x84`, which releases the one before | `0x40077a16` |
| text `(canvas, font, x, y, align, box, clear, fmt, ...)`, the font | `0x4007154c`, `0x40279e24` |

`tools/modsrpc.py` is a small Data RPC client (list, get, put, rm) for checking the route in an
emulator through its `--usb-midi-tcp` port.

```bash
python -m elekloader.sdk.build mods/core-ar2 --stock Analog-Rytm_MKII_OS1.74.syx
python -m elekloader.sdk.build mods/drive-loader-ar2 --stock Analog-Rytm_MKII_OS1.74.syx
python -m elekloader.patch --stock Analog-Rytm_MKII_OS1.74.syx --mod core-0.11.elemod --mod drive-loader-0.1.elemod --out drive-loader.syx --version 1.74
```
