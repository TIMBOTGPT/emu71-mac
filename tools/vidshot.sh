#!/bin/bash
# usage: tools/vidshot.sh prog.bas [SECS] [OUTPREFIX]
# Types prog.bas into a fresh 71 (EMU71_NOSTATE=1), RUNs it, waits SECS (default 30) seconds, then writes
#   OUTPREFIX.ppm/.png  the video layer being shown    OUTPREFIX_lcd.png  the 71 LCD    OUTPREFIX.log  VIDLOG + stderr
# OUTPREFIX defaults to /tmp/vs. Each line is followed by ATTN,ATTN, except the final RUN (see HANDOFF gotchas).
# Environment: KEYMS (default 40) key spacing in ms.
cd "$(dirname "$0")/.."
SRC=$1; SECS=${2:-30}; OUT=${3:-/tmp/vs}
python3 tools/mklexrom.py lex/video.lex /tmp/video.rom >/dev/null
K=$(python3 - "$SRC" <<'PY'
import subprocess,sys
lines=[l for l in open(sys.argv[1]).read().splitlines() if l.strip()]
k=subprocess.check_output(['python3','tools/keys.py']+lines+['RUN']).decode().strip()
print(k.replace(',13,',',13,27,27,w600,'))
PY
)
EMU71_NOSTATE=1 EMU71_LEXROM=/tmp/video.rom EMU71_KEYS="$K" EMU71_KEYMS=${KEYMS:-40} EMU71_FASTAT=2.5 \
  EMU71_VIDLOG=1 EMU71_VIDSHOT=$OUT.ppm EMU71_DUMP=$SECS EMU71_EXIT=1 ./emu71 --vid > $OUT.log 2>&1
python3 tools/lcd2png.py < $OUT.log >/dev/null && cp /tmp/lcd_0.png ${OUT}_lcd.png
python3 -c "from PIL import Image; Image.open('$OUT.ppm').save('$OUT.png')" 2>/dev/null
grep -c 'VIDEO: cmd' $OUT.log | sed 's/^/mailbox commands: /'
grep 'err [1-9]' $OUT.log | head
