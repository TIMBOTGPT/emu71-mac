#!/bin/bash
# usage: tools/run_disk.sh prog.bas [NAME]
# Puts prog.bas on a virtual HP-IL disk (a LIF image), starts the drive partner and the emulator,
# then on the 71: RESTORE IO, COPY NAME:TAPE TO NAME, TRANSFORM NAME INTO BASIC, RUN NAME.
# Extra emulator settings can be passed in the environment (EMU71_DUMP, EMU71_VIDSHOT, ...).
cd "$(dirname "$0")/.."
SRC=$1; NAME=${2:-$(basename "$SRC" .bas | tr a-z A-Z | cut -c1-8)}
IMG=${IMG:-/tmp/emu71_disk.dat}
python3 tools/mklexrom.py lex/video.lex /tmp/video.rom >/dev/null
python3 tools/lifimg.py init "$IMG" && python3 tools/lifimg.py put "$IMG" "$NAME" "$SRC" || exit 1
O=$((RANDOM%2000+50000)); I=$((O+1))
python3 tools/il_disk.py "$IMG" $O $I > /tmp/il_disk.log 2>&1 &
DP=$!; sleep 1
K=$(python3 tools/keys.py "RESTORE IO" "~40" "COPY $NAME:TAPE TO $NAME" "~100" "TRANSFORM $NAME INTO BASIC" "~100" "RUN $NAME")
EMU71_NOSTATE=1 EMU71_LEXROM=/tmp/video.rom EMU71_KEYS="$K" EMU71_KEYMS=60 EMU71_FASTAT=2.5 ./emu71 --vid --hpil 127.0.0.1:$O:$I
kill $DP 2>/dev/null
