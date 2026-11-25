#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
task_tmp=$(mktemp -d "${TMPDIR:-/tmp}/dts-reuse.XXXXXX")
trap 'rm -rf "$task_tmp"' EXIT
prefix=${NUVIO_DTS_ROOT:-/opt/homebrew}
flags=(-std=gnu11 -O1 -g -Wall -Wextra -Werror -Wno-misleading-indentation)
if [[ $(uname -s) == Darwin ]]; then flags+=(-Wno-implicit-const-int-float-conversion); fi
if [[ ${SANITIZE:-0} == 1 ]]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc "${flags[@]}" -DNV_DTS_WEBOS_RECOVERY -I"$prefix/include" tests/dts_reuse_engine.c \
  -o "$task_tmp/test" -L"$prefix/lib" -lavformat -lavcodec -lswresample -lavutil -lz -lm -lpthread -ldl
ffmpeg -hide_banner -loglevel error \
  -f lavfi -i 'color=size=64x64:rate=25:duration=4' \
  -f lavfi -i 'sine=frequency=300:sample_rate=44100:duration=4' \
  -f lavfi -i 'sine=frequency=900:sample_rate=48000:duration=4' \
  -map 0:v -map 1:a -map 2:a -map 1:a -map 2:a \
  -c:v libx264 -bf 0 -g 25 -c:a dca -strict -2 -b:a 768000 \
  -c:a:2 ac3 -c:a:3 eac3 "$task_tmp/input.mkv"
"$task_tmp/test" "$task_tmp/input.mkv" "$task_tmp"
for pass in 0 1 2 3 4 7 8; do
  ffmpeg -hide_banner -loglevel error -i "$task_tmp/$pass.aac" -f f32le -c:a pcm_f32le "$task_tmp/$pass.f32"
done
python3 - "$task_tmp" <<'PY'
import array, math, pathlib, sys
root=pathlib.Path(sys.argv[1])
for stage, tone in ((0,300),(1,900),(2,300),(3,900),(4,300),(7,300),(8,300)):
    samples=array.array('f'); samples.frombytes((root/f'{stage}.f32').read_bytes())
    # Skip encoder priming and use 0.25 s of the resulting stereo audio.
    mono=samples[4800*2:16800*2:2]
    def energy(hz):
        return abs(sum(x*complex(math.cos(2*math.pi*hz*i/48000), math.sin(2*math.pi*hz*i/48000)) for i,x in enumerate(mono)))**2
    assert len(mono)==12000 and energy(tone)>100*energy(1200-tone), (stage,tone)
print('Decoded AAC contains the selected track tone after every zero/backward/forward change.')
PY
