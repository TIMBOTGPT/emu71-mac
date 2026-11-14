# Handoff: Emu71 video + virtual disk (as of 2026-11-14)

Repo: ~/Projects/emu71-mac (github.com/TIMBOTGPT/emu71-mac, branch main, last pushed commit a1efb75).
Read docs/video-architecture.md first, then README.md (video and disk sections).

## What works (tested)
- `--vid [WxH]` video device, default 800x600, 16 indexed layers (src/video.c). LEX keywords in lex/video.asm:
  ALPHA DRAW GCLEAR GRAPH GSHOW GTARGET MOVE PEN PLOT SCALE XAXIS YAXIS. GRAPH shows the window, ALPHA hides it.
  `--vid` builds and loads the LEX itself (uncommitted change, see below).
- Virtual HP-IL disk: `--disk IMAGE` starts tools/il_disk.py (AMIGO drive, device `:TAPE`, not `:DISK`) and tools/lifimg.py
  makes and edits the LIF image. F5 = open dialog (macOS osascript), puts the .bas on the image and drives the 71 through
  COPY / TRANSFORM ... INTO BASIC / RUN. F6 = save dialog, writes the last F5-loaded program back (71 listing format, 4-digit line numbers).
  Scripted tests: EMU71_LOAD, EMU71_SAVE, EMU71_AUTOF5, EMU71_AUTOF6.
- examples/sombrero_spin.bas (coloured rotating sombrero, 12 pre-rendered layers cycled with GSHOW) runs from disk (tools/run_disk.sh).

## Uncommitted on the Mac
- (2026-11-14, committed) src/main.c: no ATTN after RUN in the F5 sequence; --vid auto-loads the video LEX; F6 names a hand-typed workfile with NAME.
- (2026-11-14, NOT committed, work in progress) lex/video.asm + lex/video.lex: new CHK routine reads the device result code (mailbox ERRC, nibble #61)
  after every command and raises a 71 error, so bad layer/colour numbers are no longer silent. Verified: GTARGET 99 now gives an error on the 71.
  Still wrong: the error number. 30 is "Stmt Not Found" (placeholder). Measured numbers so far: 24 = Insufficient Memory (used for device error 2, no such
  layer), 30 = Stmt Not Found, 80 = raised by `DELAY -1` (message not yet read). Need the number whose text is "Invalid Arg", then replace #001E in CHK
  and rebuild. lex/video.lex.orig is the previous binary. Rebuild: copy lex/video.asm to a scratch dir, run ~/Projects/asm71/build/asm71 video.asm,
  copy video.lex back (verified: asm71 reproduces the old binary byte for byte apart from the LIF date stamp).
- asm71 (bug400/asm71, GPLv3) was cloned to ~/Projects/asm71 and built with `fpc -O2 -FEbuild -FUbuild -obuild/asm71 src/asm71.pas` (arm64, works).
- examples/lorenz.bas: WORKING as of 2026-11-14 (rotating coloured attractor, seen on screen). Cause was not the numbers: the SX,SY
  variable names broke TRANSFORM at load, so MOVE/DRAW lines were dropped (see gotchas). Renamed to X,Y. Committed in bd6b324.

## Known gotchas
- HP-71B: statements separated by `@` (not `::`), no GET (use TRANSFORM), do not start key scripts with NEW (garbles input),
  workfile is named lowercase `workfile` (cannot be typed), default angle mode is degrees (OPTION ANGLE RADIANS).
- Key injection: tools/keys.py makes VK lists; EMU71_KEYMS default 60 ms; `~N` pauses; runtime queue in main.c supports `wMS` waits.
- Test runs: always EMU71_NOSTATE=1 (state.bin was overwritten once and is unrecovered; Time Machine snapshot
  2026-11-10 14:12 could restore it). LCD text is read by piping EMU71_DUMP output through tools/lcd2png.py.
- il_disk needs fresh random ports after a killed emulator.
- (2026-11-14) Variable names in LEX statement arguments: loading from disk (TRANSFORM) printed `TFM WRN Lnn:syntax` and dropped the line for
  `MOVE SX,SY` / `MOVE SX,301`. In a 20-name test only names containing a digit (S1, X1, Y1) got through; all 17 alpha-only two-letter names
  (SX SY SZ SA SB XS YS TX TY PX PY AB CD ZZ EX NX XY) failed. Single letters (S,T,X,Y) work (sombrero uses S,T).
  CAUSE (resolved 2026-11-14): not an emulator or LEX bug. HP-71B BASIC variable names are one letter plus an optional digit (A, X1), so
  two-letter alphabetic names are illegal on a real 71 and any line using them fails TRANSFORM/line entry with a syntax warning. Applies to
  every statement, not just MOVE/DRAW. Fixed lorenz.bas by renaming SX,SY to X,Y
  (original kept as examples/lorenz_SX.bak). A blank LCD with all annunciators dim is the 71 being off (seen at the end of runs); the cause of the power-off is not confirmed.
- Key script harness (EMU71_KEYS via tools/keys.py), learned 2026-11-14: (1) `~N` taps the f-shift key, so N must be EVEN, an odd count leaves f-shift on and
  garbles the next line (annunciator `f` lit). (2) After a rejected line the 71 keeps the text in its buffer, so send ATTN twice (27,27) after each line, e.g.
  `| sed 's/,13,/,13,27,27,w600,/g'`, but NOT after the final RUN line, ATTN there suspends the program (annunciator SUSP). (3) `$` has no mapping in keys.py
  and the shell expands `$(`, so avoid string functions in scripted lines. (4) Wait about 90 s for a run, 150 ms per key (EMU71_KEYMS=150).
- Error numbers (ERRN) measured in the emulator: 24 Insufficient Memory, 30 Stmt Not Found, 11 Invalid Arg, 1..4 are math warnings (underflow, exponent(0), TAN=Inf).
- Video LEX now raises a real 71 error after each command (CHK in lex/video.asm): device err 2 (no such layer) -> Insufficient Memory (24), other nonzero -> Invalid Arg (11). Verified on screen for GTARGET 99 and SCALE 5,5,5,5. SQR(-1) and FIX 20 raise no error on the 71.
- Load log: /tmp/*.log from `EMU71_VIDLOG=1 tools/run_disk.sh` shows which video commands arrived; disk load takes about 90 s before RUN output appears.

## Still to do
1. DONE: lorenz.bas fixed and committed with the main.c changes (bd6b324).
2. DONE 2026-11-14 (committed in bd6b324): F6 on a hand-typed program now asks for a name, runs `NAME <n>` on the workfile, then saves. Tested
   with a two-line program (output matched). NAME fails if a 71 file of that name already exists (no PURGE on purpose).
   Also tested 2026-11-14: LIST/decompile round trip of all 12 keywords incl. IF..THEN DRAW ELSE MOVE is correct (only a cosmetic trailing
   space after a final no-arg statement). Error paths: too few/many args, string arg, GCLEAR with an arg are all rejected cleanly (71 shows
   ERR:Excessive Chars for too many) and later statements still work. Device range errors (GTARGET 99, PEN 300) were silent because XQ in
   lex/video.asm never read the result (confirmed). The CHK fix is written and works but is uncommitted until its error number is right (see Uncommitted).
   Test harness note: after a syntax error the 71 keeps the line in its buffer, so send ATTN twice (27,27) between key-script lines.
3. Remaining mailbox commands: GLABEL (string arg), CLIP, BOX, CIRCLE, POLY, GRID, FRAME, layer stack ops
   (GNEW/GPUSH/GPOP/GROLL), VWRITE/VREAD, GBLIT, GSTORE/GLOAD.
4. Still untested: keyword clashes with other LEX files (PEN, MOVE, DRAW, SCALE, XAXIS, YAXIS, ALPHA), state save/restore with the LEX mounted,
   installing the LEX from a LIF file instead of the ROM hook. (Error paths and LIST/decompile are done, see item 2.)
   Finish the CHK error number (see Uncommitted), then commit lex/video.asm and lex/video.lex.
5. Tests with the EMU71_KEYS harness (deliverable 5).
