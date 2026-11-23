#!/bin/bash
# O CAMINHO DE CANCELAMENTO DO MOTOR REAL sob o thread sanitizer.
#
# O provider desliga o worker com um store atomico em job->parar; o motor lia
# o mesmo flag com um dereference plain (*cancelar) entre pacotes — data race
# real que o TSan media com rc 66. Este teste roda o mini_motor_quadro de
# verdade (FFmpeg, container MPEG-TS gerado na hora) numa thread enquanto a
# principal alterna o flag com o store do provider.
#
#   bash tests/miniatura_cancel.sh            # normal
#   SANITIZE=1 bash tests/miniatura_cancel.sh # ASan+UBSan
#   TSAN=1 bash tests/miniatura_cancel.sh     # Clang ThreadSanitizer
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-miniatura-cancel.XXXXXX")
trap 'rm -rf "$work"' EXIT

flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Wall -Isrc)
libs=(-lpthread -lm)
if pkg-config --exists libavcodec libavformat libavutil; then
  flags+=($(pkg-config --cflags libavcodec libavformat libavutil))
  libs+=($(pkg-config --libs libavcodec libavformat libavutil))
else
  libs+=(-lavcodec -lavformat -lavutil)
fi

if [ "${TSAN:-0}" = 1 ]; then
  # GCC TSan is known broken here — always Clang.
  clang "${flags[@]}" -fsanitize=thread -DNV_MINIATURAS -DNV_DTS_FFMPEG \
        src/miniatura_motor.c tests/miniatura_cancel.c "${libs[@]}" -o "$work/miniatura_cancel"
elif [ "${SANITIZE:-0}" = 1 ]; then
  cc "${flags[@]}" -fsanitize=address,undefined -fno-omit-frame-pointer \
     -DNV_MINIATURAS -DNV_DTS_FFMPEG src/miniatura_motor.c tests/miniatura_cancel.c \
     "${libs[@]}" -o "$work/miniatura_cancel"
else
  cc "${flags[@]}" -DNV_MINIATURAS -DNV_DTS_FFMPEG \
     src/miniatura_motor.c tests/miniatura_cancel.c "${libs[@]}" -o "$work/miniatura_cancel"
fi

# 40 s of H.264 in MPEG-TS, a keyframe every 2 s — enough material for the
# worker loop to decode real tiles while the flag flips.
ffmpeg -hide_banner -loglevel error -y \
  -f lavfi -i "testsrc=duration=40:size=640x360:rate=12" \
  -c:v libx264 -preset ultrafast -pix_fmt yuv420p -g 24 -bf 0 -f mpegts "$work/fonte.ts"

"$work/miniatura_cancel" "$work/fonte.ts"
