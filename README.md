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
