#!/usr/bin/env python3
"""Turn BASIC text lines into an EMU71_KEYS list of VK codes (ON first, ENTER after each line).
usage: keys.py 'SCALE -3.14,3.14,-1,1' 'GRAPH' ..."""
import sys
vk = {' ': 32, '.': 190, ',': 188, '-': 189, '=': 187, '/': 191, ';': 186}
shifted = {'(': 57, ')': 48, '*': 56, '+': 187, '"': 222, ':': 186, '@': 50, '$': 52}
out = [27, 27]
for line in sys.argv[1:]:
    if line.startswith('~'):                      # '~N' = N harmless shift taps (about 0.3 s each at 150 ms)
        out += [16] * int(line[1:]); continue
    for ch in line.upper():
        out.append('s%d' % shifted[ch] if ch in shifted else vk.get(ch, ord(ch)))
    out.append(13)
print(','.join(str(v) for v in out))
