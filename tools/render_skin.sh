#!/bin/zsh
# usage: render_skin.sh dark|light   (run from anywhere)
cd "$(dirname "$0")/.." || exit 1
T=${1:-dark}; S=N; [ "$T" = light ] && S=L; [ "$T" = pro ] && S=P
python3 tools/make_skin.py $T || exit 1
H=$(python3 tools/make_skin.py $T | awk '{print $3}')
"/Applications/Google Chrome.app/Contents/MacOS/Google Chrome" --headless --disable-gpu --hide-scrollbars \
  --force-device-scale-factor=3 --window-size=992,$((H+80)) --screenshot=/tmp/skin_raw.png \
  "file://$PWD/skin/REAL71$S.svg" >/dev/null 2>&1
python3 - "$S" "$H" <<'EOF'
import sys
from PIL import Image
s, h = sys.argv[1], int(sys.argv[2])
Image.open('/tmp/skin_raw.png').convert('RGB').crop((0, 0, 2976, h * 3)).save(f'skin/REAL71{s}.png')
print('wrote skin/REAL71%s.png' % s)
EOF
