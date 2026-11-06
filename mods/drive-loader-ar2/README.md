# drive-loader (Analog Rytm MKII)

The +Drive mods loader's flashed part, for core-ar2 0.11 and newer on OS 1.74. It imports
`core_desc`, so the linker writes the build descriptor into every build that has it
(docs/FORMAT.md, "The build descriptor"), and it exports the staging buffer as `drive_staging`
and `drive_staging_size` (512 KB, its `.bss`), which the descriptor's area record names.

From 0.1 it holds the `/mods` route. Linking the chosen mods at boot comes later, in this
mod's own files.

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
  loader's handler with stock's `0x4002da4a`. That calls the handler's `register_route`, which
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
| pattern compile; list table move / grow; entry table move / grow | `0x4002f1d0`; `0x4017de50` / `0x4017dfbe`; `0x4017de9e` / `0x4017e1e6` |
| std::string ctor, dtor, the empty string | `0x401bcff0`, `0x401bb540`, `0x419f2a0c` |
| operator new, delete | `0x40082a40`, `0x40082a4c` |
| ekFS file ctor (0x2c bytes; 0 read, 1 write), dtor, read, write, node, node size | `0x4007dcfa`, `0x4007d956`, `0x4007d822`, `0x4007d860`, `0x4007d9d2`, `0x4007d9ec` |
| ekFS delete, rename | `0x4007da78`, `0x4007da8c` |
| Directory ctor (0x130 bytes), dtor, count, child, shared_ptr release | `0x4007ed48`, `0x4007ee02`, `0x4007e1bc`, `0x4007e794`, `0x40170bd8` |
| mutex lock, unlock | `0x400010bc`, `0x400011ee` |

```bash
python -m elekloader.sdk.build mods/core-ar2 --stock Analog-Rytm_MKII_OS1.74.syx
python -m elekloader.sdk.build mods/drive-loader-ar2 --stock Analog-Rytm_MKII_OS1.74.syx
python -m elekloader.patch --stock Analog-Rytm_MKII_OS1.74.syx --mod core-0.11.elemod --mod drive-loader-0.1.elemod --out drive-loader.syx --version 1.74
```
