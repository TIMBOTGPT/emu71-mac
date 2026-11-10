# External video system for Emu71 (feasibility and architecture)

Evidence tags used below. **V** = verified in this repo's source or by running the emulator. **S** = sourced from HP documents or forum posts (cited). **U** = unverified, needs a test.

## 1. Decisions, ranked

| Decision | Recommendation | Weight | Strongest counterargument | Confidence |
|---|---|---|---|---|
| Device model | Smart indirect mailbox, synchronous, host-side drawing | 85% | Not real-hardware compatible and costs zero Saturn cycles, so timing is unrealistic | High |
| Mailbox sync | Execute commands inside the write callback on the CPU thread, no I/O thread | 80% | A long fill blocks the Saturn for its duration | High |
| Numbers | Pass raw 16-nibble HP-71 REALs, convert to double on the host | 85% | Needs a correct BCD to double and double to BCD pair, WHERE round-trips need care | High |
| Math ROM for SCALE | Reject, not needed | 90% | None found. The OS already holds 12-digit BCD, the Math ROM adds nothing for this | High |
| Layer model | Explicit layer numbers 0-15 plus GTARGET/GSHOW first, RPN stack ops as sugar in phase 2 | 65% / 35% | The stack gives double buffering with no handle bookkeeping (GNEW, draw, GSWAP) | Moderate |
| Storage | 1) virtual LIF drive on the HP-IL loop, 2) raw VWRITE/VREAD plus GSTORE/GLOAD as layer persistence, 3) LEX device driver | 65% / 30% / 5% | A LIF volume stored in layer memory adds indirection over a plain host image file | Moderate |
| LEX toolchain | 1) asm71 built from source on arm64, 2) Forth/Assembler ROM inside the emulator | 70% / 30% | asm71 arm64 build is untested here (see section 6) | Moderate |
| Attach method | Generated copy of `mops.c` with one added `case`, same pattern as `tcpip.c` | 75% | The sed anchor is fragile if `mops.c` changes (it is upstream, frozen at v1.19) | High |

## 2. Brainstorm claims checked

| Claim | Verdict | Evidence |
|---|---|---|
| I/O at 20000-2FFFF | Mostly right. Soft-configured MMIO is 20000-2BFFF, card reader 2C000-2C01F, 2D000-2E0FF reserved for HP. The rest of the 32 KB page holds system RAM | S: HP-71 system assignments (jeffcalc.hp41.eu/emu71/files/hp71_system_assignments.pdf), HP Journal Jul 1984 (hpmuseum.org/journals/71b.htm) |
| Internal RAM at 30000 | Wrong as a fixed fact. RAM is configured upward from 30000, largest chip first, and the 16 KB main RAM moves. Only about 1.5 KB of system memory is fixed | S: hpmuseum.org forum threads 7843 and 13392 |
| Free space from 38000 | Wrong. There is no fixed free hole, it depends on the module set | S: same threads |
| Framebuffer at F0000 | Reject. F0000-FFFFF is reserved or used by hard-configured ROMs, sources disagree on the detail. The indirect design makes it moot | S: forum threads 13392, 4551, 7843 |
| Up to 16 modules | Unverified. This emulator has 6 ports with unlimited chaining per port, the OS limit is in IDS Vol 1 section 2.5 (configuration buffer), not read | U |
| 64/128 KB per soft block | Partly verified. The code limits one RAM chip to 64K nibbles (32 KB). A 64 KB module is two chips, 128 KB is four | V: `add_ram_module` in `src/main.c`. S: forum thread 7843 |
| 320-416 KB practical RAM | Plausible. Address space is 1M nibbles (512 KB), a forum configuration shows 384 KB from 30000 to EFFFF with HP-IL | S: forum thread 4551. U for the upper figure |
| LEX can register :VSSD | Partly true. The OS polls LEX files for unknown device types, which is how the HP-IL ROM extends CAT and COPY. Everything after that (catalog entries, file chain, timestamps, error handling) is yours | S: J-F Garnier, hpmuseum.org thread 18765. The poll numbers in IDS Vol 2 were not read (fetch failed) |
| Name clashes | See section 6. RECT clashes with a Math ROM function. The rest clash only with optional third-party LEX files | S: holyjoe.net/HP71/keywords.htm. V: ROM scan inconclusive |
| Observed addresses (mailbox 0x40000, Math 0x20000) | **Resolved (V).** The OS runs CONFIG twice at cold start. The video mailbox is first placed at 40000, then at 20000, which is its final address and inside the documented MMIO range. The 0x40000 value is the first pass | V: `VIDEO: configured at` log from `src/video.c` |

## 3. How a device attaches (read from source)

- `MountPorts()` in `orig/mops.c` switches on `psData->sInfo.nType` and has `default: _ASSERT(FALSE)`. A new type is not mounted without a code change (V).
- `psExtPorts[]` is `static` in `mops.c`, so `main.c` cannot attach a module directly (V).
- `DismountPorts()` is generic through `pfnDetachMem`, so teardown needs no change (V).
- CONFIG walks ports in order, each daisy chain in order, and the first module whose `pfnConfig` returns TRUE takes the address. C=ID returns the first non-zero ID among unconfigured modules in the same order (V: `mops.c`).
- The HP-IL module is the template. It is a `TYPE_HPIL` entry plus a `TYPE_ROM` entry in the same port (V: `add_hpil_module`).
- Persistence is in `main.c`, not `orig/files.c` (not compiled). `STATEHDR.ram[64]` holds the spec string, `+hpil` is appended when HP-IL is on (V). Video appends `+vid<W>x<H>`, so a different video setup starts fresh.

**Plan.** `build/mops_vid.c` is generated from `orig/mops.c` by sed. It adds `#define TYPE_VIDEO 5`, an `extern` for `AllocVideoMem/AttachVideoMem`, and a `case TYPE_VIDEO:` beside `TYPE_HPIL`. `orig/` stays untouched. All device code lives in `src/video.c`.

**Module ID.** HP-IL uses `0x80F0F` (V: comment above `IoModuleId`). The digits are last-chip, MMIO device ID, MMIO flag, reserved, size. Device ID 0 is the HP-IL mailbox, 2 is the memory controller, 1 and 3-F are unassigned (S). Video uses device ID 5 (arbitrary pick) and a 128-nibble window, so ID `0x85F0C`. The size nibble follows `IoModuleId`: F=16, E=32, D=64, C=128.

**How the OS finds it.** Confirmed by J-F Garnier (S, hpmuseum.org thread 16567). MMIO is soft-configured upward from 20000. The OS keeps a system buffer of configured addresses together with each device's MMIO class from C=ID, and a ROM scans it for its class (the HP-IL ROM does this in `FNDMBX`). Class 0 is the HP-IL mailbox and 2 is the memory controller, so video uses class 5. In the ID `0x80F0F` the nibbles are last-chip, class, MMIO flag, reserved, size, so ours is `0x85F0C` (class shifted by 12 bits, my first draft shifted by 16 and silently claimed class 0, now fixed). Mailbox ordering relative to HP-IL does not matter.

**Configuration buffer format (decoded, IDS Vol. 1 section 3.4, checked against emulator dumps).** The buffer starts at `CONFST` (2F9E6). The MMIO table has ID byte `FD` (stored low nibble first, so `DF` in a dump), then a 3-nibble length (data only), then 10-nibble entries: sequence position, device number, port, 15-log2(size in 16-nibble words), 3 address nibbles, type `F`, class, reserved nibble. Observed: video alone is `0 0 5 C 000 F 5 0` (port 5, size code C, class 5, address field 0 which is 20000). With HP-IL also mounted, HP-IL reads field `008` and video `000`, which fits HP-IL at 20080 and video at 20000, so the address is 20000 + field x 16. The doc text says the field is relative to 10000, the observed values fit a base of 20000. The larger device is configured first. A LEX finds the mailbox by walking the `FD` table for class 5. The read-only signature "VIDEO71" at mailbox nibbles 70-7D is a cross-check and a fallback.

**LEX entry points (verified).** Addresses come from the entry-point table shipped with asm71 (`tools/HP71ENTR.TXT`), not from OCR of the IDS PDFs, which misread several (an OCR reading of COMCKO as 032E5 is wrong, the table says 032AA). Used so far: NUMCK 0369D, COMCK 036CD, COMCKO 032AA, COMCK+ 032AE, EOLCK 02A7E, RESPTR 03172, NTOKEN 0493B, EXPPAR 03FD9, EXPEXC 0F186, EXPEX- 0F178, EXPR 0F23C, POP1R 0E8FD, FIXP 02A6E, FIXDC 05493, DROPDC 05470, PARERR 02F08, BSERR 0939A, CONFST 2F9E6.

Observed in the emulator (V): with the video module mounted the OS buffer in system RAM grows by exactly 10 nibbles near 2FA42, all later chain pointers shift by 10, and the class value appears in the entry as the nibble right after the `F` MMIO flag. Changing the class changes only that nibble. **The full entry layout (address and size fields) is not decoded yet (U).** Two ways to finish it: read `FNDMBX` in the HP-IL IDS Vol 2 source listing (the PDF fetch failed here), or compare against an HP-IL entry by mounting `--hpil`. Fallback that needs no buffer format, ranked 40% against the buffer scan at 60%: the mailbox carries a read-only signature in its reserved nibbles and the LEX probes 20000-2BFFF in 128-nibble steps. It works because we control the hardware, and it would not suit a real module.

## 4. Mailbox register map (128 nibbles, nibble offsets in hex)

| Offset | Size | Name | Access | Meaning |
|---|---|---|---|---|
| 00-0F | 16 | R0 | RW | REAL slot 0 (HP-71 REAL, 16 nibbles) |
| 10-1F | 16 | R1 | RW | REAL slot 1 |
| 20-2F | 16 | R2 | RW | REAL slot 2 |
| 30-3F | 16 | R3 | RW | REAL slot 3 |
| 40-43 | 4 | I0 | RW | signed 16-bit, little-endian nibbles |
| 44-47 | 4 | I1 | RW | |
| 48-4B | 4 | I2 | RW | |
| 4C-4F | 4 | I3 | RW | |
| 50-5F | 16 | DATA | RW | text for GLABEL, RGB for COLORMAP, bulk bytes for VWRITE/VREAD, file name for GSTORE/GLOAD |
| 60 | 1 | STAT | R | b0 reserved (always 0, sync device), b1 error, b2 window visible, b3 dirty |
| 61 | 1 | ERR | R | error code of last command (0 = ok) |
| 62 | 1 | VER | R | protocol version (1) |
| 63 | 1 | TGT | RW | target layer 0-F |
| 64-65 | 2 | LEN | RW | byte count for DATA |
| 66-6A | 5 | LPTR | RW | 20-bit linear pointer into target layer, auto-increments on VPOKE/VPEEK/VWRITE/VREAD |
| 6B-6C | 2 | COL | RW | colour index 0-255 (0 = transparent when overlaying) |
| 6D-6E | 2 | PAL | RW | palette index |
| 6F | 1 | FLAGS | RW | b0 transparent-0 overlay, b1 autoincrement off |
| 70-7D | 14 | reserved | | read as 0 |
| 7E-7F | 2 | CMD | W | command byte, **the write that covers nibble 7F triggers execution** |

REAL layout (V): nibbles 0-2 exponent as BCD (10's complement when negative, `988` = -12), nibbles 3-14 mantissa with nibble 14 the most significant digit, nibble 15 sign (0 positive, 9 negative). Checked by typing `X=3.14159265359` and `Y=-2.71828182845E-12` in BASIC and searching RAM (`EMU71_FIND`). Both values stored exactly as decoded by `real_dec`. A positive non-zero exponent was not tested separately.

Execution model. `pfnWriteIO` receives `(a, d, ps)` and may cover several nibbles (V: `WriteIOHpilMem`). When the range includes nibble 7F, the command runs inline and `STAT/ERR` are valid on the next read. There is no thread and no handshake, which removes about two thirds of the HP-IL module's complexity. The SDL window is created and drawn only on the main thread (V: `main()` owns the SDL loop, the CPU runs on another thread), so `GRAPH` sets a flag and the main loop pumps it.

## 5. Command set

| Code | Name | Inputs | Outputs |
|---|---|---|---|
| 00 | NOP | | |
| 01 | RESET | | clears layers, palette, scale |
| 02 | GRAPH | | shows window |
| 03 | ALPHA | | hides window |
| 04 | GCLEAR | TGT, COL | |
| 05 | PEN | COL | |
| 06 | PENUP | | pen up |
| 10 | SCALE | R0..R3 = xmin,xmax,ymin,ymax | |
| 11 | LIMIT | I0..I3 = pixel rectangle | |
| 12 | CLIP | FLAGS b2 | |
| 13 | MOVE | R0,R1 | |
| 14 | DRAW | R0,R1 | |
| 15 | IMOVE | R0,R1 | |
| 16 | IDRAW | R0,R1 | |
| 17 | PLOT | R0,R1,I0 mode | |
| 18 | WHERE | | R0,R1 (back-converted) |
| 20 | BOX (see section 6) | R0..R3, I0 fill | |
| 21 | CIRCLE | R0,R1,R2, I0 fill | |
| 22-24 | POLY begin / vertex / fill | R0,R1 | |
| 30-33 | GLABEL / LDIR / CSIZE / LORG | DATA,LEN or R0/I0 | |
| 40-43 | XAXIS / YAXIS / GRID / FRAME | R0..R3 | |
| 50 | PGET | I0,I1 | COL |
| 51 | PSET | I0,I1,COL | |
| 60 | COLORMAP | PAL, DATA = RGB | |
| 70-77 | GNEW / GFREE / GTARGET / GPUSH / GPOP / GSWAP / GROLL / GSHOW | TGT, I0 | TGT |
| 80-84 | LSEEK / VPOKE / VPEEK / VWRITE / VREAD | LPTR, COL, DATA, LEN | COL, DATA |
| 90-92 | GBLIT / GSTORE / GLOAD | source layer, rects, file name in DATA | |

Drawing details: Bresenham for lines, midpoint circle, scanline polygon fill, 5x7 bitmap font scaled by CSIZE. Layers are 8-bit indexed with a 256-entry palette. Default size 800x600 (override with --vid WxH), 16 layers numbered 0-15 (TGT is one nibble), allocated on first use at 480 KB each, 7.7 MB if all are used. GSTORE/GLOAD are restricted to one sandbox folder on the host. File names are 8 characters because DATA is 16 nibbles.

## 6. LEX keywords: names, clashes, toolchain

Clash check against the LEX keyword listing (S: holyjoe.net/HP71/keywords.htm):

| Our name | Existing keyword | Where | Action |
|---|---|---|---|
| RECT | RECT(#) function | Math ROM, ID 02 (a ROM you mount in port 1) | **Rename the statement** (BOX). Verify the new name against the listing first |
| ALPHA | ALPHA function | CNSTLEX, third-party, ID 94 | Clash only if loaded |
| PEN, PENUP, MOVE, DRAW, IMOVE, IDRAW, CIRCLE, LDIR, CSIZE, LORG, SCALE, LIMIT, LOCATE, CLIP, XAXIS, YAXIS, FRAME | all present in one other library (ID 68) | optional third-party plot LEX | Clash only if that library is loaded, note in README |
| MOVE, DRAW, LDIR, CSIZE | PLOTLEX, ID 5F | AMPI third-party | Same |
| GRAPH, GCLEAR, PLOT, WHERE, AREA, POLY, GLABEL, GRID, COLORMAP, G*, V* | none found | | Free |

My own ROM scan (`ROM71`, Math, Forth, HP-IL images) is not evidence of absence. It found PRINT, COPY, CAT but missed BEEP and FRAC, so recall was 3 of 6 controls. The Forth hits for MOVE, LIMIT and FILL are Forth dictionary words, not BASIC keywords, and do not clash.

LEX ID. Use a scratch ID (5C, 5D or 5E) for development, they are listed as scratch in the HP assignments (S). Do not reuse HP or third-party IDs.

Phase 1 mapping: each keyword parses its arguments with OS routines, copies REALs into R0-R3 as raw nibbles, writes CMD. WHERE copies R0/R1 back to the stack. SCALE, clipping and every primitive stay on the host.

**Toolchain.**

1. asm71 (bug400/asm71, GPLv3). Source is Free Pascal (`src/asm71.pas`) with CMake, the README says "completely compatible with the assembler of the HP-71B Forth/Assembler ROM", and its doc covers `ENTRY`, `CHAR`, `KEY`, `TOKEN` and the `LEX`/`BIN` pseudo-ops (V: read from the cloned `doc/asm71.html`). Prebuilt macOS binaries are Intel only (S: GitHub page). An arm64 build needs `fpc` from Homebrew. **Not built or run here.** The build of the cloned source was blocked by the session's permission classifier, so it needs your go-ahead.
2. The Forth/Assembler ROM in the emulator. **Finding (V):** `main.c` mounted only `FORTHROM.BIN` (the soft-configured LEX half). With the same keystrokes, `FORTH` followed by `1 2 SWAP DROP .` gave `ERR: Excess Chars`, so Forth did not start. After mounting `HRDFORTH.BIN` as a hard-configured module at E0000 (`add_hrd_module` in `src/main.c`), the same input gave `2 OK {0}`. Forth now works. `ASSEMBLE` itself was **not** tested.

Loading a LEX. Verified: the pyILPER loop reads and writes (this repo). Untested: COPY of a LEX from a LIF image into a port RAM module. End state to aim for: ship the video LEX as a soft-configured ROM image directly after the mailbox in the same port, exactly as `Hpilrom.bin` follows the HP-IL mailbox. That needs the LIF and file headers stripped and the ROM file-chain layout checked (U).

## 7. Storage in detail

1. **Virtual LIF drive on the HP-IL loop (65%).** The HP-IL ROM already handles catalog, copy, file chain and errors. The device only has to speak HP-IL mass storage, as J-F Garnier points out (S, thread 18765). Stage A0 is pyILPER's virtual disk, which already works here (V). Stage A1 is a built-in drive answering frames in-process, backed by a host `.dat` image. Backing it with a layer buys nothing over a host file.
2. **Raw VWRITE/VREAD with GSTORE/GLOAD (30%).** Cheap and fast, no file system, no CAT/COPY. Right for scratch data and graphics, wrong as an SSD. It complements option 1, it does not replace it.
3. **LEX driver for :VSSD (5%).** Possible through the unknown-device poll, but you reimplement the part HP-IL gives you for free, and a mistake corrupts the file chain in RAM. Reject until 1 proves inadequate.

What would change the ranking. If the HP-IL ROM cannot be mounted (port budget, or you want video without HP-IL), option 3 moves up. If you want the volume to be visible or editable as pixels, option 1 backed by a layer becomes sensible.

## 8. What could go wrong

- LEX mailbox discovery fails because the config buffer layout is not what I assume. The prototype dumps the buffer before any LEX work.
- REAL nibble layout differs from my assumption. One `PEEK$` test settles it.
- Saved state from an older module set. Handled by the `+vid` spec string, but a stale `state.bin` must be removed once when upgrading.
- A second SDL window on macOS must be created on the main thread. A window close on the video window must hide it, not quit the app.
- Unusually large MMIO window plus HP-IL plus cards in the 20000-2BFFF page. 128 nibbles is small, risk is low.
- A keyword name clash changes behaviour silently when optional third-party LEX files are loaded.

## 9. Tested and not tested

Tested: source reading of `mops.c`, `satmem.c`, `hpil.c`, `main.c`, `portmem.c`. Emulator: Forth start fails without the hard ROM and works with it. Keyword listing and memory map checked against the cited sources.

Prototype status (V): `src/video.c` builds and `EMU71_VIDTEST` passes 0 failures. It drives the mailbox through the module's own `pfnWriteIO`/`pfnReadIO`. Covered: REAL encode/decode round trip, read-only registers, bad command error, SCALE, MOVE/DRAW/PLOT, WHERE, PGET, VPOKE/VPEEK on a hidden layer, out-of-range error, and the sine plot (PNG checked by eye). The SDL window runs for 7 seconds without crashing, its appearance was not checked.

Not tested: a LEX or any Saturn code calling the mailbox, the decoded layout of a configuration buffer entry, state save/restore with `--vid` (not run, it would overwrite the saved calculator state), asm71 build and use, `ASSEMBLE` of a LEX, REAL layout, config buffer scan, LEX load from a LIF image, the IDS poll list for device specifiers.

## Phase 1 status (LEX keywords)

`lex/video.asm` (LEX ID 5C, built with asm71) provides ALPHA, DRAW, GCLEAR, GRAPH, MOVE, PEN, PLOT, SCALE, XAXIS, YAXIS. The LEX finds the mailbox by walking the configuration buffer `FD` table for class 5, lets the OS evaluate the numeric arguments (first through EXPEXC, the rest through EXPR, comma token skipped), copies the REALs into mailbox slots R0..R3 and writes the command. `tools/mklexrom.py` wraps the asm71 output as a 16 KB soft-configured ROM image, mounted with `EMU71_LEXROM=path` (development hook, adds `+lex` to the saved-state spec). `tools/keys.py` turns BASIC lines into an `EMU71_KEYS` list. `EMU71_VIDLOG=1` logs each mailbox command and `EMU71_VIDSHOT=path` writes the shown layer as a PPM at exit.

Tested: a BASIC program (`OPTION ANGLE RADIANS`, GRAPH, SCALE -3.14,3.14,-1,1, XAXIS, YAXIS, PEN 3, MOVE, `FOR X=-3.14 TO 3.14 STEP .1 : DRAW X,SIN(X)`) draws the expected sine curve with axes. Not tested: error paths from BASIC (wrong argument count, string argument), program listing/decompile of the new statements, keyword clashes with other LEX files loaded together, GLABEL, state save and restore with the LEX mounted, installing the LEX from a LIF file instead of the ROM hook. Note the HP-71B defaults to degrees, so a radians demo needs `OPTION ANGLE RADIANS`.
