#!/bin/bash
# Buffer size setting -> MiB and the free-RAM clamp. Ver o .c.
#   bash tests/buffer_tamanho.sh
set -eu
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-buffer.XXXXXX")
trap 'rm -rf "$work"' EXIT
cc -std=gnu11 -O1 -g -Wall -Wextra -Werror -Isrc src/cacheboost.c tests/buffer_tamanho.c -lpthread -lm -o "$work/t"
"$work/t" > "$work/buffer.log"

cc -std=gnu11 -O1 -g -Wall -Wextra -Werror -Isrc -I/opt/homebrew/include \
  -I/opt/homebrew/include/SDL2 -ffunction-sections -fdata-sections \
  tests/buffer_overlay.c -Wl,-dead_strip,-undefined,dynamic_lookup -o "$work/overlay"
"$work/overlay"

# Preload and playback must retain the same MiB value for player adoption.
python3 - <<'PY'
from pathlib import Path
import re

source = Path('android/app/src/main/java/space/nuvio/nativelegacy/NvPlayer.kt').read_text()
preload = source.split('private fun enfileirarPreparo(', 1)[1].split('private fun prepararMain(', 1)[0]
playback = re.search(r'fun buffer\(mb: Int\) \{ bufferPreset = mb\.coerceIn\((\d+), (\d+)\)', source)
prepared = re.search(r'buffer\.coerceIn\((\d+), (\d+)\)', preload)
assert playback and prepared, 'Missing Android buffer validation'
for mb in (-1, 0, 1, 64, 128, 256, 257):
    expected = min(max(mb, 0), 256)
    for name, validation in (('playback', playback), ('preload', prepared)):
        low, high = map(int, validation.groups())
        actual = min(max(mb, low), high)
        assert actual == expected, f'{name}: {mb} MiB became {actual}, expected {expected}'
print('buffer_tamanho: Android preload/playback MiB validation ok')
PY
