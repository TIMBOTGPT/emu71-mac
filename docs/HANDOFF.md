# Handoff: Emu71 video + virtual disk (as of 2026-11-14)

Repo: ~/Projects/emu71-mac (github.com/TIMBOTGPT/emu71-mac, branch main, last pushed commit see `git log`, video LEX commands complete as of 2026-11-14 afternoon).
Read docs/video-architecture.md first, then README.md (video and disk sections).

## What works (tested)
- `--vid [WxH]` video device, default 800x600, 16 indexed layers (src/video.c). LEX keywords in lex/video.asm:
  ALPHA BOX CIRCLE CLIP DRAW FILL FRAME GBLIT GCLEAR GLABEL GLOAD GNEW GPOP GPUSH GRAPH GRID GROLL GSHOW GSTORE GTARGET MOVE PEN PLOT POLY
  SCALE VREAD() VWRITE XAXIS YAXIS (semantics in README). GRAPH shows the window, ALPHA hides it.
  `--vid` builds and loads the LEX itself (uncommitted change, see below).
- Virtual HP-IL disk: `--disk IMAGE` starts tools/il_disk.py (AMIGO drive, device `:TAPE`, not `:DISK`) and tools/lifimg.py
  makes and edits the LIF image. F5 = open dialog (macOS osascript), puts the .bas on the image and drives the 71 through
  COPY / TRANSFORM ... INTO BASIC / RUN. F6 = save dialog, writes the last F5-loaded program back (71 listing format, 4-digit line numbers).
  Scripted tests: EMU71_LOAD, EMU71_SAVE, EMU71_AUTOF5, EMU71_AUTOF6.
- examples/sombrero_spin.bas (coloured rotating sombrero, 12 pre-rendered layers cycled with GSHOW) runs from disk (tools/run_disk.sh).

## Committed 2026-11-14 (nothing pending except the stray untracked files below)
- src/main.c (bd6b324): no ATTN after RUN in the F5 sequence; --vid auto-loads the video LEX; F6 names a hand-typed workfile with NAME.
- lex/video.asm + lex/video.lex (c29c9b7): CHK routine reads the device result code (mailbox ERRC, nibble #61) after every command and raises a 71 error:
  device err 2 (no such layer) -> Insufficient Memory (#0018), any other nonzero -> Invalid Arg (#000B).
  Checked on screen 2026-11-14 (tests/cases/err_*): PEN 300, GSHOW 20, GTARGET 99, SCALE 5,5,5,5 and more all give ERR:Invalid Arg.
  CORRECTION: an earlier note here said GTARGET 99 gave Insufficient Memory. It does not, layer numbers above 15 are a range error (device err 3),
  so the Insufficient Memory branch (device err 2) cannot be reached from any keyword today. Rebuild: copy lex/video.asm to a scratch dir, run ~/Projects/asm71/build/asm71 video.asm, copy video.lex back.
- Untracked, not committed, safe to delete if Mark agrees: examples/lorenz2.bas (uses illegal names), examples/lorenz_SX.bak, lex/video.lex.orig.
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
- Error numbers (ERRN) measured in the emulator: 24 Insufficient Memory, 30 Stmt Not Found, 11 Invalid Arg (measured with ON ERROR GOTO in a program, ERRN typed directly after a restart is unreliable), 1..4 are math warnings (underflow, exponent(0), TAN=Inf).
- Video LEX now raises a real 71 error after each command (CHK in lex/video.asm): device err 2 (no such layer) -> Insufficient Memory (24), other nonzero -> Invalid Arg (11). SQR(-1) and FIX 20 raise no error on the 71.
- Load log: /tmp/*.log from `EMU71_VIDLOG=1 tools/run_disk.sh` shows which video commands arrived; disk load takes about 90 s before RUN output appears.

## Still to do
1. DONE: lorenz.bas fixed and committed with the main.c changes (bd6b324).
2. DONE 2026-11-14 (committed in bd6b324): F6 on a hand-typed program now asks for a name, runs `NAME <n>` on the workfile, then saves. Tested
   with a two-line program (output matched). NAME fails if a 71 file of that name already exists (no PURGE on purpose).
   Also tested 2026-11-14: LIST/decompile round trip of all 12 keywords incl. IF..THEN DRAW ELSE MOVE is correct (only a cosmetic trailing
   space after a final no-arg statement). Error paths: too few/many args, string arg, GCLEAR with an arg are all rejected cleanly (71 shows
   ERR:Excessive Chars for too many) and later statements still work. Device range errors (GTARGET 99, PEN 300) were silent because XQ in
   lex/video.asm never read the result (confirmed). FIXED and committed in c29c9b7 (CHK, see above).
   Test harness note: after a syntax error the 71 keeps the line in its buffer, so send ATTN twice (27,27) between key-script lines.
3. DONE 2026-11-14: all mailbox commands have keywords and were checked on screen (examples/shapes.bas, layers.bas, labels.bas, pixels.bas,
   store.bas). Error paths checked on screen: PEN 300, GSHOW 20, GPOP on an empty stack, GROLL 1, GLOAD of a missing file, GSTORE with a
   name over 8 chars, VREAD(500000) all give ERR:Invalid Arg. LIST decompile checked for GSTORE "AB" and X=VREAD(5)+1.
   Not covered: CLIP has no visible effect because there is no LIMIT keyword yet (host command 0x11 takes I0..I3, not REALs). Lowercase
   GLABEL text cannot be typed through tools/keys.py (it uppercases), so the lowercase glyphs are unchecked. FILL is an addition (BOX, CIRCLE
   and POLY need a fill switch and the mailbox only has four REAL slots). LDIR/CSIZE/LORG, COLORMAP and WHERE have host commands but no keyword.
4. DONE 2026-11-14 (tests/cases/clash_pen, state_restore, lex_install_lif):
   - Keyword clash with another LEX: tested with a throwaway LEX (tests/lex/clash.asm, ID 5D, defines PEN and ZZAP) in ports 3 and 4 via the new
     EMU71_LEXROM2 hook. The LEX in the lower port wins PEN in both orders (it is port order, not LEX ID, because ID 5D beat ID 5C when it sat in port 3),
     and the other LEX keeps all its other keywords (ZZAP, GRAPH, CIRCLE worked). Evidence is two load orders, so confidence is moderate-high for "first
     mounted wins". Math ROM (port 1) and Forth ROM (port 2) are always mounted and have coexisted with the video LEX in every run, but they define no
     clashing keyword (RECT is not used). Real third-party LEX files (PLOTLEX, ID 68 library) were not available and are untested.
   - State save/restore with the LEX mounted works: variable, program with LEX statements, its LIST decompile and the mailbox mapping all survive
     (run with a scratch HOME so the real state.bin stays untouched, `mkdir -p $HOME/Library/Application\ Support` first because state_path creates only the last
     folder). Layer pixels live on the host and are NOT in state.bin, the picture is blank after a restore until the program redraws (GSTORE/GLOAD persist it).
   - LEX install from a LIF file works with no LEX ROM mounted (EMU71_LEXROM=none): tools/lifimg.py putlif adds lex/video.lex (type E208) to the virtual disk,
     `RESTORE IO`, `COPY VIDEO:TAPE TO VIDEO` makes the keywords live. The COPY wait of 90 s in the test is a generous guess, the real time was not measured.
5. DONE 2026-11-14: tests/run_tests.py with tests/cases/*.t and tests/golden (see below). Still open: tools/run_disk.sh hardcodes EMU71_KEYMS=60, make it overridable.

## LEX authoring notes learned 2026-11-14 (afternoon)
- asm71 needs KEY names in alphabetical order with ascending tokens, or the OS silently does not recognise the late keywords (FILL gave a syntax
  error, BOX gave Excess Chars). Regenerate the ENTRY/KEY tables from the KEY names, tokens 1..N follow the sorted order (they were renumbered, which
  only matters for tokenized programs in saved state, .bas files are text).
- GONC/GOYES reach about 127 nibbles. Far jumps to NOVID failed with "Jump or value too large", so the new code has inline error stubs.
- String arguments: EXPEXC then REVPOP (POP1S returns the text reversed, seen as "DLROW OL"). A(A) = length in nibbles, D1 = first character.
  Parse is `P=0 / GOVLNG =STRNGP`, decompile reuses FIXDC. GLABEL sends 8 characters per command (#34 starts at the pen, #30 per chunk).
- Numeric function (VREAD): `NIBHEX 8` / `NIBHEX 11` (one numeric parameter, min 1 max 1) before the entry, CHAR #F, POP1N gives the REAL in A
  (DEC mode, so SETHEX at once), save D0/D1, result REAL read back from mailbox R0, restore D0/D1, GOVLNG =FNRTN4.
- Typing a program is slow: a 29-line program needs about 130 s before RUN output exists, so tools/vidshot.sh needs SECS of about 4.5 s per line.
  EMU71_DUMP fires at a fixed time, an early dump shows a half-typed line.
- tools/vidshot.sh prog.bas SECS OUTPREFIX types, runs, writes OUTPREFIX.png (video layer), _lcd.png and .log (EMU71_VIDLOG).

## Regression tests (tests/)
- `python3 tests/run_tests.py [--slow] [--update] [NAME ...]` runs tests/cases/*.t through EMU71_KEYS and compares the mailbox log, the LCD dump and the
  video layer (PNG, pixel exact) with tests/golden. Hand-written `expect_log` / `expect_nolog` regexes check the meaning independently of the goldens.
  The fast set takes about 10 minutes (measured 4 to 10 minutes) (typing is slow, two timer ticks per key). `lex_install_lif` is slow (about 3 minutes) and only runs with --slow or by name.
  `--update` rewrites goldens, render them with tests/show_golden.py and look before committing. Goldens were checked by eye on 2026-11-14 and the five PNGs
  are pixel-identical to the program renders (examples/shapes, layers, labels, pixels, store) that were checked on screen first.
- New emulator hooks for tests: EMU71_DUMPIDLE=N (dump N s after the last scripted key), EMU71_LEXROM2 (second LEX, port 4), EMU71_LEXROM=none.
- A wake-up pause is needed after the leading ATTNs (`27,27,w1500`), at KEYMS=60 the first typed character was lost without it.
- Weak spots: parse-error cases (err_pen_noarg, err_pen_extra, err_pen_string) only prove the line was rejected and nothing reached the device, the LCD shows the
  kept line, not the error text. Lowercase text cannot be typed. CLIP has no observable effect yet. lorenz/sombrero are not in the suite (minutes each).
- The LCD compare blanks any fully set 5x7 cell (the blinking block cursor), a repeat run showed state_restore.2 flaking on it. Run 2 of the suite passed 20 of 21 before that fix, state_restore passed after it. A full second pass after the fix was not repeated. lex_install_lif passed in 152 s.
