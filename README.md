# Emu71 for macOS (Apple Silicon)

A native arm64 port of [Emu71](https://hp.giesselink.com/emu71.htm) v1.19, Christoph Giesselink's HP-71B emulator,
with an SDL2 front end replacing the Win32 user interface. The original Saturn CPU, memory, timer and keyboard
core is used unchanged (`orig/`) behind a thin Win32 compatibility shim (`shim/`, `stubs/`).

Rosetta is being removed from macOS, so this runs natively on Apple Silicon.

## What works

- Boots the HP-71B, full keyboard (mouse and Mac keyboard, US layout), LCD, annunciators
- Math and Forth/Assembler ROM modules mounted, plus extra RAM modules (about 147 KB free with the default 2x64K)
- State saved on quit and restored on start (`~/Library/Application Support/Emu71/state.bin`)
- Scalable skins: PNG skins at any integer multiple of the reference sheet; window keeps its aspect ratio

Not yet: HP-IL, sound, .e71 documents, menus/port configuration, non-US keymaps.

## ROM images (not included)

The HP-71B ROM images are copyrighted by HP and are **not** in this repository. Put your own copies in `roms/`:

    ROM71.BIN  MATHROM.BIN  FORTHROM.BIN

## Build

    brew install sdl2
    make
    ./bundle.sh        # produces Emu71.app (embeds SDL2, bundles skin/ and roms/)

Run from the build directory with `./emu71 --skin skin --roms roms`, or open `Emu71.app`.

Options: `--ram 3:64,4:64` (extra RAM modules as port:KB, default 128K in ports 3 and 4; `--ram none` for stock memory;
changing it starts the calculator fresh), `--kml REAL71P.KML` (skin layout), `--image file.png` (skin image), `--scale 1.5` (initial window scale),
`--refw N` (reference sheet width for unusual images).

## HP-IL (disk/printer loop over TCP/IP)

`--hpil localhost:60001:60000` mounts the HP-IL module (`Hpilrom.bin`, port 5) and connects the virtual loop to
an external HP-IL emulator such as [pyILPER](https://github.com/bug400/pyilper) (host:out-port:in-port, matching
pyILPER's defaults: it listens on 60001 and sends to 60000). Start the loop partner first, then the calculator;
the module is initialised at power-on and needs a partner that answers (a broken loop at power-up stalls the
ROM, restart the app). `tools/il_echo.py` is a minimal empty-loop partner for testing. Then use
`RESTORE IO`, `CAT :TAPE`, `COPY`, with LIF disk images served by pyILPER. Changing `--hpil` starts the
calculator fresh. Verified against pyILPER 1.9.1: `RESTORE IO`, `CAT :TAPE`, and `COPY T1 TO :TAPE` wrote a TEXT file into the LIF image on the Mac.
Test hooks: `EMU71_KEYS` (comma separated Windows VK codes, `s` prefix = shifted), `EMU71_DUMP`/`EMU71_EVERY`/`EMU71_STOP` (ASCII LCD dumps), `tools/lcd2png.py`.

## Skins

`skin/REAL71P.*` (default), `REAL71N.*` and `REAL71L.*` are original scalable designs generated from
`tools/make_skin.py` (outputs SVG, KML and, via `tools/render_skin.sh`, a 3x PNG; rendering uses headless Chrome).
Edit the SVG in Illustrator/Affinity, export the whole sheet as PNG at an integer multiple of 992 px wide.
`tools/make_logo.py` builds the logo from outlined Montserrat (SIL OFL). Set `LOGO=/path/to/image tools/render_skin.sh pro`
to place a different logo on the plate.

The layout (key positions and electrical matrix) comes from Giesselink's `REAL71BL.KML`; his bitmap skins are not
redistributed here. `KBD-US.KMI` is his keyboard script.

## Credits and licence

- Emu71 and the layout/keyboard scripts: Copyright (C) Christoph Giesselink, GPL. Emu71 is based on Emu48 by
  Christoph Giesselink and Sebastien Carlier.
- This port: GNU General Public License, version 2 or (at your option) any later version. See `LICENSE`.
- `src/stb_image.h`: public domain / MIT (Sean Barrett).
- HP and HP-71B are trademarks of their owners; this project is not affiliated with or endorsed by HP.

## External video and virtual disk (experimental)

`--vid [WxH]` adds a separate video window (default 800x600, 16 indexed layers) driven by a mailbox device and a LEX
(`lex/video.asm`): ALPHA BOX CIRCLE CLIP DRAW FILL FRAME GBLIT GCLEAR GLABEL GLOAD GNEW GPOP GPUSH GRAPH GRID GROLL GSHOW GSTORE
GTARGET MOVE PEN PLOT POLY SCALE VREAD() VWRITE XAXIS YAXIS. Semantics: FILL 0/1 switches BOX, CIRCLE, POLY between outline and fill.
POLY x,y,m (m: 0 start, 1 vertex, 2 last vertex and close). GRID x0,y0,xt,yt. CIRCLE radius is in x-scale units. GNEW n clears layer n,
GPUSH n / GPOP save and restore the target, GROLL n rotates layers 0..n-1 (layer i takes layer i+1). GBLIT s,dx,dy,t copies layer s onto
the target shifted by dx,dy pixels (t=1: colour 0 transparent). VWRITE p,n,v sets n pixels from linear offset p to v, VREAD(p) returns one.
GLABEL "text" draws 5x7 text at the pen. GSTORE/GLOAD "NAME" (up to 8 of A-Z 0-9 _ -) save and load the target layer with its palette
under `~/.emu71/video/` (override with EMU71_VIDDIR).
`GRAPH` shows the window, `ALPHA` hides it, `GTARGET n` picks the layer to draw on and `GSHOW n` the one displayed.
Details are in `docs/video-architecture.md`. For development, `EMU71_LEXROM=/tmp/video.rom` loads the LEX
(`tools/mklexrom.py lex/video.lex /tmp/video.rom`).

Programs can be loaded from a virtual HP-IL disk instead of being typed:

    tools/run_disk.sh examples/sombrero_spin.bas SPIN

This builds a LIF image (`tools/lifimg.py`), starts the drive partner (`tools/il_disk.py`, address `:TAPE` on the loop,
use `--hpil host:out:in`) and has the 71 run `RESTORE IO`, `COPY SPIN:TAPE TO SPIN`, `TRANSFORM SPIN INTO BASIC`,
`RUN SPIN`. Notes: HP-71B statements are separated by `@`, and the device is `:TAPE`. Saving a program back to the host
is not implemented yet.

### Load and save with the OS file dialogs

    ./emu71 --vid --disk ~/hp71disk.dat

`--disk IMAGE` starts the virtual drive (creating the LIF image if needed). In the emulator window:

- **F5** opens the macOS file chooser. The chosen text `.bas` file is put on the disk, then the 71 is driven through
  `COPY`, `TRANSFORM ... INTO BASIC` and `RUN` (the 71 file name is the first 8 letters or digits of the host name).
- **F6** opens a save panel. The 71 program last loaded with F5 is converted back to text, copied to the drive and
  written to the chosen host file (line numbers come out as the 71 lists them, for example `0010`).

The emulator runs unthrottled while it does this. `EMU71_LOAD`, `EMU71_SAVE`, `EMU71_AUTOF5` and `EMU71_AUTOF6`
bypass the dialogs for scripted tests. F6 only knows the program loaded by F5; saving a program typed by hand
needs a named file first.
