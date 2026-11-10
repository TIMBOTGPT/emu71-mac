#!/bin/bash
# usage: tools/run_prog.sh examples/sombrero_spin.bas   (types the program into a fresh 71 and RUNs it, unthrottled)
cd "$(dirname "$0")/.."
python3 tools/mklexrom.py lex/video.lex /tmp/video.rom >/dev/null
K=$(python3 - "$1" <<'PY'
import subprocess,sys
lines=open(sys.argv[1]).read().splitlines()
print(subprocess.check_output(['python3','tools/keys.py']+lines+['RUN']).decode().strip())
PY
)
EMU71_NOSTATE=1 EMU71_LEXROM=/tmp/video.rom EMU71_KEYS="$K" EMU71_KEYMS=40 EMU71_FASTAT=2.5 ./emu71 --vid
