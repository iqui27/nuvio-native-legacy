#!/bin/bash
# The Samsung DTS loopback server (src/dts/dts_hls_tpk.c) for real on the host:
# FFmpeg built with the .tpk component set (NUVIO_DTS_TARGET=tpk
# tools/build-dts-ffmpeg.sh, prefix in NUVIO_DTS_ROOT, default /opt/nuvio-dts),
# fixtures made by the system ffmpeg CLI (libx264 + the experimental DTS
# encoder), source served by tests/dts_tpk_servidor_origem.py.
set -eu
cd "$(dirname "$0")/.."
ROOT=${NUVIO_DTS_ROOT:-/opt/nuvio-dts}
if [ ! -f "$ROOT/lib/libavformat.a" ] || ! command -v ffmpeg >/dev/null || ! command -v ffprobe >/dev/null; then
  echo "dts_tpk_servidor: skipped (needs FFmpeg for the .tpk in $ROOT and the ffmpeg/ffprobe CLI)"
  exit 0
fi
work=$(mktemp -d /tmp/nuvio-dts-tpk-servidor.XXXXXX)
pid=''
trap 'if [ -n "$pid" ]; then kill "$pid" 2>/dev/null || true; fi; rm -rf "$work"' EXIT
mkdir -p "$work/src"
# 30 s, keyframe every 2 s (15 segments of 2 s), DTS 5.1, Matroska with Cues.
ffmpeg -v error -f lavfi -i testsrc2=s=640x360:r=24 -f lavfi -i "sine=f=440:r=48000" -t 30 \
  -filter_complex "[1:a]pan=5.1|FL=c0|FR=c0|FC=c0|LFE=c0|BL=c0|BR=c0[a]" -map 0:v -map "[a]" \
  -c:v libx264 -preset ultrafast -g 48 -keyint_min 48 -sc_threshold 0 -pix_fmt yuv420p \
  -c:a dca -strict -2 -b:a 768k "$work/src/fx.mkv"
# 12 s, AAC only (no DTS at all): the video-only mode takes any audio.
ffmpeg -v error -f lavfi -i testsrc2=s=320x180:r=24 -f lavfi -i "sine=f=440:r=48000" -t 12 \
  -map 0:v -map 1:a -c:v libx264 -preset ultrafast -g 48 -keyint_min 48 -sc_threshold 0 -pix_fmt yuv420p \
  -c:a aac -ac 2 "$work/src/aac.mkv"
# The donor (audio from another source): the same title from another release,
# 1.5 s longer at the start; track 0 E-AC3 5.1 Polish, silent but for a beep at
# 10.5 s of its own time (= 9.0 s of fx.mkv's); track 1 AAC English.
ffmpeg -v error -f lavfi -i testsrc2=s=320x180:r=24 -f lavfi -i "aevalsrc='if(between(t,10.5,10.7),sin(2*PI*1000*t),0)':s=48000" \
  -f lavfi -i "sine=f=330:r=48000" -t 31.5 \
  -filter_complex "[1:a]pan=5.1|FL=c0|FR=c0|FC=c0|LFE=c0|BL=c0|BR=c0[e]" -map 0:v -map "[e]" -map 2:a \
  -c:v libx264 -preset ultrafast -g 48 -keyint_min 48 -sc_threshold 0 -pix_fmt yuv420p \
  -c:a:0 eac3 -c:a:1 aac -ac:a:1 2 -metadata:s:a:0 language=pol -metadata:s:a:1 language=eng "$work/src/doador.mkv"
# A PAL donor: the same 30 s at 25 fps, played 25/24 faster (28.8 s), its
# beep at 8.64 s of its own time (= 9.0 s of fx.mkv's at the file's speed).
ffmpeg -v error -f lavfi -i testsrc2=s=320x180:r=25 -f lavfi -i "aevalsrc='if(between(t,8.64,8.832),sin(2*PI*1000*t),0)':s=48000" -t 28.8 \
  -filter_complex "[1:a]pan=5.1|FL=c0|FR=c0|FC=c0|LFE=c0|BL=c0|BR=c0[e]" -map 0:v -map "[e]" \
  -c:v libx264 -preset ultrafast -g 50 -keyint_min 50 -sc_threshold 0 -pix_fmt yuv420p \
  -c:a eac3 -metadata:s:a:0 language=pol "$work/src/pal.mkv"
# The automatic match: 90 s whose audio is tones gated at irregular times
# (onsets, as hits and cuts), DTS; and a donor with the same audio 3 s later
# (3 s more at its start), AC3 stereo: the right offset is -3000 ms.
som='0.5*sin(2*PI*(300+150*sin(0.3*T))*T)*gt(sin(0.05*T*T+1.3*T),0.55)+0.4*sin(2*PI*90*T)*gt(sin(2.3*T+3*sin(0.21*T)),0.85)'
ffmpeg -v error -f lavfi -i testsrc2=s=320x180:r=24 -f lavfi -i "aevalsrc='$(echo "$som" | sed 's/T/t/g')':s=48000" -t 90 \
  -filter_complex "[1:a]pan=5.1|FL=c0|FR=c0|FC=c0|LFE=c0|BL=c0|BR=c0[a]" -map 0:v -map "[a]" \
  -c:v libx264 -preset ultrafast -g 48 -keyint_min 48 -sc_threshold 0 -pix_fmt yuv420p \
  -c:a dca -strict -2 -b:a 768k "$work/src/m.mkv"
ffmpeg -v error -f lavfi -i testsrc2=s=320x180:r=24 -f lavfi -i "aevalsrc='$(echo "$som" | sed 's/T/(t-3)/g')':s=48000" -t 93 \
  -map 0:v -map 1:a -c:v libx264 -preset ultrafast -g 48 -keyint_min 48 -sc_threshold 0 -pix_fmt yuv420p \
  -c:a ac3 -ac 2 -metadata:s:a:0 language=eng "$work/src/md.mkv"
# 150 s at 64x64 for the audio-queue cap: the test binary moves the audio 70 s
# ahead of its video below, so up to 80 s of AC3 wait for their video.
# WHICH AUDIO THE MATCH COMPARES (owner 18/11): m2.mkv has the picture's
# Japanese and an English dub 0.8 s off (a wrong dub); md2.mkv only English,
# 3 s later than the picture; md3.mkv Japanese 3 s later and an English 1.5 s
# later (a dub that is off the other way).
ffmpeg -v error -f lavfi -i testsrc2=s=320x180:r=24 -f lavfi -i "aevalsrc='$(echo "$som" | sed 's/T/t/g')':s=48000" \
  -f lavfi -i "aevalsrc='$(echo "$som" | sed 's/T/(t-0.8)/g')':s=48000" -t 90 \
  -map 0:v -map 1:a -map 2:a -c:v libx264 -preset ultrafast -g 48 -keyint_min 48 -sc_threshold 0 -pix_fmt yuv420p \
  -c:a ac3 -ac 2 -metadata:s:a:0 language=jpn -metadata:s:a:1 language=eng "$work/src/m2.mkv"
ffmpeg -v error -f lavfi -i testsrc2=s=320x180:r=24 -f lavfi -i "aevalsrc='$(echo "$som" | sed 's/T/(t-3)/g')':s=48000" -t 93 \
  -map 0:v -map 1:a -c:v libx264 -preset ultrafast -g 48 -keyint_min 48 -sc_threshold 0 -pix_fmt yuv420p \
  -c:a ac3 -ac 2 -metadata:s:a:0 language=eng "$work/src/md2.mkv"
ffmpeg -v error -f lavfi -i testsrc2=s=320x180:r=24 -f lavfi -i "aevalsrc='$(echo "$som" | sed 's/T/(t-3)/g')':s=48000" \
  -f lavfi -i "aevalsrc='$(echo "$som" | sed 's/T/(t-1.5)/g')':s=48000" -t 93 \
  -map 0:v -map 1:a -map 2:a -c:v libx264 -preset ultrafast -g 48 -keyint_min 48 -sc_threshold 0 -pix_fmt yuv420p \
  -c:a ac3 -ac 2 -metadata:s:a:0 language=jpn -metadata:s:a:1 language=eng "$work/src/md3.mkv"
# THE EDIT MAP (owner 18/11). ins.mkv: fx.mkv's 30 s with 2 s only the donor
# has at 14 s (300 Hz there); beeps at 7.0 and 19.0 s of its own time (= 7.0
# and 17.0 s of fx.mkv's). cut.mkv: 2 s the donor lacks at fx.mkv's 14-16 s;
# 500 Hz over its 12-14 s, a beep at 15.0 s of its own (= 17.0 s of fx.mkv's).
ffmpeg -v error -f lavfi -i testsrc2=s=320x180:r=24 \
  -f lavfi -i "aevalsrc='if(between(t,7.0,7.2)+between(t,19.0,19.2),sin(2*PI*1000*t),0)+if(between(t,14.0,16.0),0.5*sin(2*PI*300*t),0)':s=48000" -t 32 \
  -map 0:v -map 1:a -c:v libx264 -preset ultrafast -g 48 -keyint_min 48 -sc_threshold 0 -pix_fmt yuv420p \
  -c:a eac3 -ac 2 -metadata:s:a:0 language=eng "$work/src/ins.mkv"
ffmpeg -v error -f lavfi -i testsrc2=s=320x180:r=24 \
  -f lavfi -i "aevalsrc='if(between(t,15.0,15.2),sin(2*PI*1000*t),0)+if(between(t,12.0,14.0),0.5*sin(2*PI*500*t),0)':s=48000" -t 28 \
  -map 0:v -map 1:a -c:v libx264 -preset ultrafast -g 48 -keyint_min 48 -sc_threshold 0 -pix_fmt yuv420p \
  -c:a eac3 -ac 2 -metadata:s:a:0 language=eng "$work/src/cut.mkv"
ffmpeg -v error -f lavfi -i testsrc2=s=64x64:r=24 -f lavfi -i "sine=f=440:r=48000" -t 150 \
  -filter_complex "[1:a]pan=5.1|FL=c0|FR=c0|FC=c0|LFE=c0|BL=c0|BR=c0[a]" -map 0:v -map "[a]" \
  -c:v libx264 -preset ultrafast -g 48 -keyint_min 48 -sc_threshold 0 -pix_fmt yuv420p \
  -c:a dca -strict -2 -b:a 768k "$work/longo.mkv"
python3 tests/dts_tpk_servidor_origem.py "$work/src" "$work/port" "$work/origem.log" &
pid=$!
for _ in $(seq 50); do [ -s "$work/port" ] && break; sleep 0.1; done
port=$(cat "$work/port")
${CC:-gcc} -O1 -g -Wall -Wextra -Wno-unused-parameter -Wno-sign-compare -Wno-misleading-indentation \
  -DNV_TPK -DNV_DTS_FFMPEG -DDOA_CLIP_S=20 -I"$ROOT/include" -Isrc \
  tests/dts_tpk_servidor.c src/dts/dts_hls_tpk.c src/tpk45.c src/dts/dts_remux.c src/dts/doador_tpk.c src/dts/dts_engine.c src/rede.c src/redeurl.c \
  "$ROOT/lib/libavformat.a" "$ROOT/lib/libavcodec.a" "$ROOT/lib/libswresample.a" "$ROOT/lib/libavutil.a" \
  -lz -lm -lpthread -ldl -o "$work/test"
"$work/test" adiantar "$work/longo.mkv" "$work/src/adiantado.mkv" 70
fail=0
dur() { ffprobe -v error -show_entries format=duration -of csv=p=0 "$1"; }
check() { if eval "$2"; then echo "ok   $1"; else echo "FAIL $1"; fail=1; fi; }

# 1. HLS as shipped: 2 s segments, whole segments, one memory line at the end.
mkdir -p "$work/t1"
"$work/test" "$port" padrao "$work/t1" > "$work/t1.log" 2>&1 || fail=1
grep -E '^(ok|FAIL)' "$work/t1.log"
check "seg0 is about 2 s ($(dur "$work/t1/seg0.ts") s)" "awk 'BEGIN{d=$(dur "$work/t1/seg0.ts"); exit !(d > 1.8 && d < 2.3)}'"
check "seg1 is about 2 s ($(dur "$work/t1/seg1.ts") s)" "awk 'BEGIN{d=$(dur "$work/t1/seg1.ts"); exit !(d > 1.8 && d < 2.3)}'"
check "the session line names the segment length" "grep -q 'segmentos de 2 s' '$work/t1.log'"
check "one memory line at the end" "[ \$(grep -c '^\[dts-tpk\] mem: ' '$work/t1.log') = 1 ]"
if [ "$(uname -s)" = Darwin ]; then
  echo "skip RSS and MemAvailable checks: /proc metrics unavailable on macOS"
  check "memory line has the rings (auto: 32 MiB here)" \
    "grep -qE '^\[dts-tpk\] mem: sessao [0-9]+, .*reader rings peak [12] = (32|64) MiB' '$work/t1.log'"
else
  check "memory line has RSS, MemAvailable and the rings (auto: 32 MiB here)" \
    "grep -qE '^\[dts-tpk\] mem: sessao [0-9]+, .*RAM [0-9]+ MB; app RSS peak [1-9][0-9]* MB .*MemAvailable low [0-9]+ MB .*reader rings peak [12] = (32|64) MiB' '$work/t1.log'"
fi
check "nothing left of the tuning test" "! grep -qiE 'tuning|seg0 cut|dts-ajuste' '$work/t1.log'"

# 2. Stop/back during the probe hangs up on the source at once (blocker 1).
"$work/test" "$port" cancelar "$work" > "$work/t3.log" 2>&1 || fail=1
grep -E '^(ok|FAIL)' "$work/t3.log"
fechou=$(sed -n 's/^closed-at //p' "$work/t3.log")
abriu=$(sed -n 's/^hold-open //p' "$work/origem.log" | head -1)
solto=$(sed -n 's/^hold-closed //p' "$work/origem.log" | head -1)
# Timestamps are rounded to milliseconds, so ordering can differ by at most 1 ms.
check "cancel: origin connection was open at close and ended within 2 s (open ${abriu:-?}, closed ${fechou:-?}, source saw ${solto:-nothing})" \
  "[ -n '$abriu' ] && [ -n '$solto' ] && [ -n '$fechou' ] && awk 'BEGIN{exit !($abriu <= $fechou + 0.001 && $solto >= $fechou - 0.001 && $solto - $fechou < 2.0)}'"
check "cancel: no 'did not open' line for a cancelled probe" "! grep -q 'a fonte nao abriu no conversor' '$work/t3.log'"

# 3. Audio far ahead of its video: the AC3 queue stops at its cap (blocker 2).
"$work/test" "$port" fila "$work" > "$work/t4.log" 2>&1 || fail=1
grep -E '^(ok|FAIL)' "$work/t4.log"
check "queue: the error names the audio queue cap" "grep -q 'Audio queue over 60 s' '$work/t4.log'"
if [ "$(uname -s)" = Darwin ]; then
  echo "skip queue RSS limit: /proc metrics unavailable on macOS"
else
  check "queue: the converter stayed under 100 MB RSS ($(sed -n 's/.*app RSS peak \([0-9]*\) MB.*/\1/p' "$work/t4.log") MB)" \
    "[ \"\$(sed -n 's/.*app RSS peak \([0-9]*\) MB.*/\1/p' '$work/t4.log')\" -lt 100 ]"
fi

# 4. Listeners: HLS on 127.0.0.1 only; no thread before the token (blocker 3).
"$work/test" "$port" escuta "$work" > "$work/t5.log" 2>&1 || fail=1
grep -E '^(ok|FAIL|skip)' "$work/t5.log"

# 5. Range is ignored: chunked bodies only (blocker 4: inverted ranges).
"$work/test" "$port" faixa "$work" > "$work/t6.log" 2>&1 || fail=1
grep -E '^(ok|FAIL)' "$work/t6.log"

# 6. Video only (audio from another source): no audio stream, the fixed timeline.
mkdir -p "$work/t7"
"$work/test" "$port" sovideo "$work/t7" > "$work/t7.log" 2>&1 || fail=1
grep -E '^(ok|FAIL)' "$work/t7.log"
tipos() { ffprobe -v error -show_entries stream=codec_type -of csv=p=0 "$1" | grep -v '^$' | sort -u | tr '\n' ' '; }
vini() { ffprobe -v error -select_streams v -show_entries stream=start_time -of csv=p=0 "$1" | head -1; }
for a in 0 1; do
  check "video only ($a): segments carry the video alone ($(tipos "$work/t7/sv${a}_seg0.ts"))" \
    "[ \"\$(tipos '$work/t7/sv${a}_seg0.ts')\" = 'video ' ] && [ \"\$(tipos '$work/t7/sv${a}_seg3.ts')\" = 'video ' ]"
  check "video only ($a): source time + 10 s (seg0 $(vini "$work/t7/sv${a}_seg0.ts"), seg3 $(vini "$work/t7/sv${a}_seg3.ts"))" \
    "awk 'BEGIN{a=$(vini "$work/t7/sv${a}_seg0.ts"); b=$(vini "$work/t7/sv${a}_seg3.ts"); exit !(a>9.99 && a<10.11 && b>15.99 && b<16.11)}'"
done
check "video only: the session line says so, one generator" "grep -q 'VIDEO ONLY (the audio comes from the donor server), 1 generator' '$work/t7.log'"

# 7. Audio from another source (doador_tpk.c) over the video-only converter.
mkdir -p "$work/t8"
"$work/test" "$port" doador "$work/t8" > "$work/t8.log" 2>&1 || fail=1
grep -E '^(ok|FAIL)' "$work/t8.log"
d="$work/t8"
ini() { ffprobe -v error -select_streams "$2" -show_entries stream=start_time -of csv=p=0 "$1" | head -1; }
cod() { ffprobe -v error -select_streams a -show_entries stream=codec_name,channels -of csv=p=0 "$1" | head -1; }
perto() { awk -v a="$1" -v b="$2" -v t="$3" 'BEGIN{x=a-b; if (x<0) x=-x; exit !(x<=t)}'; }
bipe() { ffmpeg -v info -copyts -i "$1" -map 0:a:0 -af silencedetect=n=-40dB:d=0.1 -f null - 2>&1 | sed -n 's/.*silence_end: \([0-9.]*\).*/\1/p' | head -1; }
npk() { ffprobe -v error -select_streams a:0 -count_packets -show_entries stream=nb_read_packets -of csv=p=0 "$1" 2>/dev/null | head -1; }
ainicio() { ffprobe -v error -select_streams a:0 -show_entries stream=start_time -of csv=p=0 "$1" 2>/dev/null | head -1; }
for f in s0e s1e s0a s0p; do
  check "silence: $f.ts carries audio ($(npk "$d/$f.ts") packets, $(cod "$d/$f.ts"), from $(ainicio "$d/$f.ts") s)" \
    "[ \"\$(npk '$d/$f.ts')\" -gt 10 ] 2>/dev/null"
done
check "silence: E-AC3 stays E-AC3 ($(cod "$d/s0e.ts"))" "[ \"\$(cod '$d/s0e.ts')\" = 'eac3,6' ]"
check "silence: segment 0 starts at the file's 0 s (+10 s) ($(ainicio "$d/s0e.ts"))" "perto '$(ainicio "$d/s0e.ts")' 10 0.05"
check "silence: E-AC3 silence then the donor's own frames decode cleanly" "[ -z \"\$(ffmpeg -v error -i '$d/s1e.ts' -f null - 2>&1)\" ]"
check "silence: AAC silence decodes cleanly" "[ -z \"\$(ffmpeg -v error -i '$d/s0a.ts' -f null - 2>&1)\" ]"
check "silence: the log says so" "grep -q 'had no audio: [0-9]* eac3 frame(s) of silence' '$work/t8.log'"
check "donor: Polish E-AC3 is the DEFAULT rendition, named after its source" \
  "grep -q 'NAME=\"Polish (eac3 6ch) from Test donor\",LANGUAGE=\"pol\",DEFAULT=YES' '$d/dm.m3u8'"
check "donor: E-AC3 5.1 copied ($(cod "$d/d4.ts")), segment 4 at 18 s ($(ini "$d/d4.ts" a))" "[ '$(cod "$d/d4.ts")' = 'eac3,6' ] && perto $(ini "$d/d4.ts" a) 18 0.05"
check "donor: the beep (10.5 s of its own time) lands on 9.0 s of the file's = 19.0 s ($(bipe "$d/d4.ts"))" "perto $(bipe "$d/d4.ts" || echo 0) 19.0 0.05"
check "donor: the file's own DTS as AC3 5.1 ($(cod "$d/f3.ts")), segment 3 at 16 s ($(ini "$d/f3.ts" a))" "[ '$(cod "$d/f3.ts")' = 'ac3,6' ] && perto $(ini "$d/f3.ts" a) 16 0.05"
check "back on the donor: the same beep at 19.0 s ($(bipe "$d/d4b.ts"))" "perto $(bipe "$d/d4b.ts" || echo 0) 19.0 0.05"
check "offset -1000 ms: the next segment carries it, the beep at 19.5 s ($(bipe "$d/d4c.ts"))" "perto $(bipe "$d/d4c.ts" || echo 0) 19.5 0.05"
check "donor: seek ahead and back (segment 12 $(ini "$d/d12.ts" a), segment 1 $(ini "$d/d1.ts" a))" \
  "perto $(ini "$d/d12.ts" a || echo 0) 34 0.05 && perto $(ini "$d/d1.ts" a || echo 0) 12 0.05"
check "donor: the converter's segment carries the video alone, at file time + 10 s ($(ini "$d/v3.ts" v))" \
  "[ \"\$(ffprobe -v error -show_entries stream=codec_type -of csv=p=0 '$d/v3.ts' | grep -v '^\$' | sort -u | tr '\\n' ' ')\" = 'video ' ] && perto $(ini "$d/v3.ts" v) 16 0.01"
check "donor: a player gets the converter's video and the donor's audio together ($(ini "$d/dplay.ts" v) / $(ini "$d/dplay.ts" a))" \
  "[ '$(cod "$d/dplay.ts")' = 'eac3,6' ] && perto $(ini "$d/dplay.ts" v || echo 9) $(ini "$d/dplay.ts" a || echo 0) 0.1"
check "one audio connection: the file gave its back at the start, the donor when the file's track was asked, then each took its own again" \
  "grep -q 'original file gave its connection back in [0-9]* ms (not the starting track' '$work/t8.log' && grep -q 'donor gave its connection back in [0-9]* ms (the player moved' '$work/t8.log' && grep -q 'audio 0 seg 3 .*seeked, connection taken again' '$work/t8.log' && grep -q 'donor audio 0 seg 4 .*seeked, connection taken again' '$work/t8.log'"
check "the 10 s line: connections, reader rings, each source's state, the offset" \
  "grep -qE '10s connections: .*reader rings alive [0-9]+ = [0-9]+ MiB .*; file (connected|released), gave back [0-9]+, took again [0-9]+; donor (connected|released), .*donor offset -1000 ms' '$work/t8.log'"
check "donor missing: the master offers the file's track alone" "[ \$(grep -c '^#EXT-X-MEDIA:TYPE=AUDIO' '$d/dx.m3u8') = 1 ] && grep -q 'donor not offered' '$work/t8.log'"
check "donor missing: dropped after 10 s, not the reader's 30 s of retries ($(sed -n 's/.*ready in \([0-9]*\) ms: .*audio file 0$/\1/p' "$work/t8.log" | tail -1) ms)" \
  "grep -q 'donor not offered: donor: no answer in 10 s' '$work/t8.log' && [ \"\$(sed -n 's/.*ready in \([0-9]*\) ms: .*audio file 0\$/\1/p' '$work/t8.log' | tail -1)\" -lt 13000 ]"
check "PAL donor at its speed: converted to AC3 ($(cod "$d/p4.ts")), the beep at 19.0 s ($(bipe "$d/p4.ts"))" \
  "[ '$(cod "$d/p4.ts")' = 'ac3,6' ] && perto $(bipe "$d/p4.ts" || echo 0) 19.0 0.06"
check "PAL donor, speed from the frame rates by itself: the beep at 19.0 s ($(bipe "$d/p4a.ts"))" \
  "perto $(bipe "$d/p4a.ts" || echo 0) 19.0 0.06"
check "PAL donor further on: still in place, segment 10's audio at 30 s ($(ini "$d/p10.ts" a))" "perto $(ini "$d/p10.ts" a || echo 0) 30 0.06"
check "donor: no URL of either file in the log" "! grep -q 'token=secret\\|doador.mkv\\|nothere.mkv\\|fx.mkv\\|pal.mkv\\|md.mkv\\|/m.mkv' '$work/t8.log'"

# 8. The automatic match over the converter's tap and the donor's segments.
mkdir -p "$work/t9"
"$work/test" "$port" dopasuj "$work/t9" > "$work/t9.log" 2>&1 || fail=1
grep -E '^(ok|FAIL)' "$work/t9.log"
check "match: no URL of either file in the log" "! grep -q 'md.mkv\\|/m.mkv' '$work/t9.log'"
mkdir -p "$work/t10"
"$work/test" "$port" faixasmatch "$work/t10" > "$work/t10.log" 2>&1 || fail=1
grep -E '^(ok|FAIL)' "$work/t10.log"
grep -E '\[doador\] match (tracks|\(two tracks\))' "$work/t10.log" | sed 's/^/     /'
check "match tracks: original on both sides, the donor's Japanese from the generator's reads" "grep -q 'the original language (ja) on both sides' '$work/t10.log' && grep -q \"from the donor's reads, not the playing track\" '$work/t10.log'"
check "match tracks: two file tracks scored together" "grep -q 'match (two tracks): ' '$work/t10.log'"

# 9. The edit map (owner 18/11): per segment, no player seek; silence where
# the donor lacks content; a stretch's own refine changes only its segments.
mkdir -p "$work/t11"
"$work/test" "$port" mapa "$work/t11" > "$work/t11.log" 2>&1 || fail=1
grep -E '^(ok|FAIL)' "$work/t11.log"
d="$work/t11"
check "map: stretch 1 unchanged by the map (segments 2 and 3 byte-identical)" "cmp -s '$d/u2.ts' '$d/m2.ts' && cmp -s '$d/u3.ts' '$d/m3.ts'"
check "map: the beep before the insert at 7.0 s of the file (+10) ($(bipe "$d/m3.ts"))" "perto $(bipe "$d/m3.ts" || echo 0) 17.0 0.05"
check "map: segment 7 crosses the edit, a donor seek ('crosses the edit' in the log)" "grep -q 'seg 7 crosses the edit at 14.0 s: offset +0 -> -2000 ms' '$work/t11.log'"
check "map: the beep after the insert at the file's 17.0 s (+10) ($(bipe "$d/m8.ts"))" "perto $(bipe "$d/m8.ts" || echo 0) 27.0 0.05"
maxv() { ffmpeg -v info -i "$1" -map 0:a:0 -af "${2:+$2,}volumedetect" -f null - 2>&1 | sed -n 's/.*max_volume: \(-*[0-9.inf]*\) dB.*/\1/p' | tail -1; }
check "map: the insert's 300 Hz tone never plays (segment 7 is silent: $(maxv "$d/m7.ts") dB)" "awk 'BEGIN{exit !($(maxv "$d/m7.ts" | sed 's/^-inf$/-200/') < -50)}'"
check "map: after a seek back and ahead each segment has its stretch's offset" "cmp -s '$d/m3.ts' '$d/b3.ts' && cmp -s '$d/m8.ts' '$d/b8.ts'"
check "map: -500 ms by hand shifts stretch 1 too (the beep 0.5 s earlier, $(bipe "$d/h3.ts"))" "perto $(bipe "$d/h3.ts" || echo 0) 16.5 0.05"
check "map: the cut: segment 7 is silence, no repeat of the 500 Hz before it ($(maxv "$d/c7.ts") dB; segment 6 has it: $(maxv "$d/c6.ts") dB)" \
  "awk 'BEGIN{exit !($(maxv "$d/c7.ts" | sed 's/^-inf$/-200/') < -50 && $(maxv "$d/c6.ts" | sed 's/^-inf$/-200/') > -20)}' && grep -q 'seg 7: the file has 2.00 s the donor lacks before 16.0 s: silence there, no repeat' '$work/t11.log'"
check "map: the cut: the beep after it at the file's 17.0 s (+10) ($(bipe "$d/c8.ts"))" "perto $(bipe "$d/c8.ts" || echo 0) 27.0 0.05"
check "map refine: stretch 1's segment byte-identical before and after" "cmp -s '$d/r5a.ts' '$d/r5b.ts'"
check "map refine: stretch 2's segment changed" "! cmp -s '$d/r40a.ts' '$d/r40b.ts'"
check "map refine: the log names the stretch and its change" "grep -qE 'stretch 2 refined: -2600 -> -(29[4-9][0-9]|30[0-5][0-9]) ms' '$work/t11.log'"
# 10. One piece: a donor without a map gives the same bytes as f4c5f25 (the
# commit before the map), built from git when its history is here.
mkdir -p "$work/t12/new" "$work/t12/old"
"$work/test" "$port" umapeca "$work/t12/new" > "$work/t12.log" 2>&1 || fail=1
grep -E '^(ok|FAIL)' "$work/t12.log"
if git cat-file -e f4c5f25:src/dts/doador_tpk.c 2>/dev/null; then
  # f4c5f25's own sources; this test file (its "umapeca" mode uses only what f4c5f25 had).
  mkdir -p "$work/old/tests"
  git archive f4c5f25 src | tar -x -C "$work/old"
  cp tests/dts_tpk_servidor.c "$work/old/tests/"
  ${CC:-gcc} -O1 -g -w -DNV_TPK -DNV_DTS_FFMPEG -DDOA_CLIP_S=20 -DSEM_MAPA -I"$ROOT/include" -I"$work/old/src" \
    "$work/old/tests/dts_tpk_servidor.c" "$work/old/src/dts/dts_hls_tpk.c" "$work/old/src/dts/dts_remux.c" "$work/old/src/dts/doador_tpk.c" \
    "$work/old/src/dts/dts_engine.c" "$work/old/src/rede.c" "$work/old/src/redeurl.c" \
    "$ROOT/lib/libavformat.a" "$ROOT/lib/libavcodec.a" "$ROOT/lib/libswresample.a" "$ROOT/lib/libavutil.a" \
    -lz -lm -lpthread -ldl -o "$work/test-old"
  "$work/test-old" "$port" umapeca "$work/t12/old" > "$work/t12-old.log" 2>&1 || fail=1
  iguais=0
  for i in 0 1 2 3 4 5 6 7 8; do cmp -s "$work/t12/new/p$i.ts" "$work/t12/old/p$i.ts" && iguais=$((iguais + 1)); done
  check "one piece: segments 0-8 byte-identical to f4c5f25's ($iguais of 9)" "[ $iguais = 9 ]"
else
  echo "skip one piece: f4c5f25 is not in this clone's history"
fi

cp "$work/t1.log" "$work/t3.log" "$work/t4.log" "$work/t5.log" "$work/t6.log" "${DTS_TPK_LOGS:-$work}/" 2>/dev/null || true
if [ "$fail" != 0 ]; then echo "--- t1.log"; grep '\[dts-tpk\]' "$work/t1.log" | tail -30; echo "--- t3.log"; tail -20 "$work/t3.log"; echo "--- t4.log"; grep -v '^\[dts-tpk\] #' "$work/t4.log" | tail -20; fi
[ "$fail" = 0 ] && echo "dts_tpk_servidor: ok"
exit "$fail"
