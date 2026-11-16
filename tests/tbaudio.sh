#!/bin/bash
# Original audio with TorBox burn-in (src/dts/tbaudio_tpk.c) for real on the
# host: system FFmpeg (libav* dev + the ffmpeg/ffprobe CLI with libx264,
# libopus and the experimental DTS encoder), a fake "TorBox" HLS made from the
# original by ffmpeg (MPEG-TS and fMP4, last #EXTINF overreported like TorBox),
# served by tests/tbaudio_origem.py.
set -eu
cd "$(dirname "$0")/.."
if ! pkg-config --exists libavformat libavcodec libswresample libavutil || ! command -v ffmpeg >/dev/null || ! command -v ffprobe >/dev/null; then
  echo "tbaudio: skipped (needs the FFmpeg dev libraries and the ffmpeg/ffprobe CLI)"
  exit 0
fi
work=$(mktemp -d /tmp/nuvio-tbaudio.XXXXXX)
pids=()
trap 'for p in "${pids[@]}"; do kill "$p" 2>/dev/null || true; done; rm -rf "$work"' EXIT
mkdir -p "$work/src/tb" "$work/src/tb4" "$work/out"
# An ASS track whose first line is at 6 s, one line every 0.2 s: its Cues
# outnumber the video's (log 14/11: seeks went through such a track, and a
# seek to 0 s, before its first entry, left the reader at mid-file).
python3 - "$work/src/s.ass" <<'PY'
import sys
t = lambda x: "0:%02d:%05.2f" % (int(x) // 60, x % 60)
h = ("[Script Info]\nScriptType: v4.00+\n\n[V4+ Styles]\nFormat: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, "
     "OutlineColour, BackColour, Bold, Italic, Underline, StrikeOut, ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline, "
     "Shadow, Alignment, MarginL, MarginR, MarginV, Encoding\nStyle: Default,Arial,20,&H00FFFFFF,&H000000FF,&H00000000,"
     "&H00000000,0,0,0,0,100,100,0,0,1,1,0,2,10,10,10,1\n\n[Events]\nFormat: Layer, Start, End, Style, Name, MarginL, "
     "MarginR, MarginV, Effect, Text\n")
open(sys.argv[1], "w").write(h + "".join("Dialogue: 0,%s,%s,Default,,0,0,0,,line %d\n" % (t(6 + i * 0.2), t(6.15 + i * 0.2), i)
                                         for i in range(120)))
PY
ffmpeg -v error -f lavfi -i testsrc2=s=320x180:r=24 -f lavfi -i "sine=f=440:r=48000" -f lavfi -i "sine=f=660:r=48000" -i "$work/src/s.ass" -t 32 \
  -filter_complex "[1:a]asplit=4[a0][a1][a2][a3];[a1]pan=5.1|FL=c0|FR=c0|FC=c0|LFE=c0|BL=c0|BR=c0[a51];[a3]pan=5.1|FL=c0|FR=c0|FC=c0|LFE=c0|BL=c0|BR=c0[d51];[2:a]aformat=channel_layouts=stereo[f]" \
  -map 0:v -map "[a0]" -map "[a51]" -map "[f]" -map "[a2]" -map "[d51]" -map 3 -c:s ass \
  -c:v libx264 -preset ultrafast -g 48 -keyint_min 48 -sc_threshold 0 -pix_fmt yuv420p \
  -c:a:0 aac -ac:a:0 2 -c:a:1 ac3 -c:a:2 flac -c:a:3 libopus -ac:a:3 2 -c:a:4 dca -strict -2 -b:a:4 768k \
  -metadata:s:a:0 language=eng -metadata:s:a:1 language=jpn -metadata:s:a:2 language=eng -metadata:s:a:3 language=fre \
  "$work/src/orig.mkv"
# mkvmerge-style Opus (TV, 16/11): Xiph-laced blocks of 7 frames, no DefaultDuration.
# Without an Opus parser (the TV's FFmpeg) only each block's first frame has a timestamp.
ffmpeg -v error -i "$work/src/orig.mkv" -map 0:v -map 0:a:3 -c copy -write_crc32 0 "$work/src/opus_plain.mkv"
python3 tests/mkv_lacear.py "$work/src/opus_plain.mkv" "$work/src/opus_laced.mkv" A_OPUS 7
for t in ts fmp4; do
  d=tb; [ $t = fmp4 ] && d=tb4
  ffmpeg -v error -i "$work/src/orig.mkv" -map 0:v -map 0:a:0 -c:v libx264 -preset ultrafast \
    -force_key_frames "expr:gte(t,n_forced*5)" -c:a aac -ac 2 -f hls -hls_time 5 -hls_playlist_type vod \
    -hls_segment_type $([ $t = ts ] && echo mpegts || echo fmp4) -hls_fmp4_init_filename init.mp4 -hls_segment_filename "$work/src/$d/s%d.$([ $t = ts ] && echo ts || echo m4s)" \
    "$work/src/$d/real.m3u8"
  # TorBox lists every segment as 5 s, the last one too.
  sed 's/^#EXTINF:[0-9.]*,/#EXTINF:5.000000,/' "$work/src/$d/real.m3u8" > "$work/src/$d/playlist.m3u8"
done
printf '#EXTM3U\n#EXT-X-STREAM-INF:BANDWIDTH=2000000,RESOLUTION=320x180\nplaylist.m3u8\n' > "$work/src/tb4/master.m3u8"
python3 tests/tbaudio_origem.py "$work/src" "$work/port" "$work/origem.log" & pids+=($!)
for _ in $(seq 50); do [ -s "$work/port" ] && break; sleep 0.1; done
port=$(cat "$work/port")
${CC:-gcc} -O1 -g -Wall -Wextra -Wno-unused-parameter -Wno-sign-compare -Wno-misleading-indentation -Wno-format-truncation \
  -DNV_TBAUDIO_TEST -DNV_DTS_FFMPEG -Isrc tests/tbaudio.c src/dts/tbaudio_tpk.c src/tbrelogio.c src/dts/dts_engine.c src/rede.c src/redeurl.c \
  $(pkg-config --cflags --libs libavformat libavcodec libswresample libavutil) -lz -lm -lpthread -ldl -o "$work/t"
fail=0
check() { if eval "$2"; then echo "ok   $1"; else echo "FAIL $1"; fail=1; fi; }
inicio() { ffprobe -v error -select_streams "$2" -show_entries stream=start_time -of csv=p=0 "$1" | head -1; }
perto() { awk -v a="$1" -v b="$2" -v t="$3" 'BEGIN{d=a-b; if (d<0) d=-d; exit !(d<=t)}'; }
codec() { ffprobe -v error -select_streams a -show_entries stream=codec_name,channels -of csv=p=0 "$1" | head -1; }

"$work/t" puros "$work/src/tb/s0.ts" "$work/src/tb4/init.mp4" "$work/src/tb4/s0.m4s" "$work/out/strip.ts" || fail=1
check "TS strip: ffprobe sees the video only" "[ \"\$(ffprobe -v error -show_entries stream=codec_type -of csv=p=0 '$work/out/strip.ts' | grep -v '^\$' | sort -u | tr '\n' ' ')\" = 'video ' ]"

sessao() {   # <playlist path> <modo> <ordinal> [original file, default orig.mkv]
  rm -f "$work/url"
  "$work/t" sessao "http://127.0.0.1:$port/$1?token=secret" "http://127.0.0.1:$port/${4:-orig.mkv}" "$2" "$3" "$work/url" > "$work/s.log" 2>&1 &
  sp=$!; pids+=($sp)
  for _ in $(seq 50); do [ -s "$work/url" ] && break; sleep 0.1; done
  U=$(cat "$work/url"); B=${U%/*}
}
fim() { kill "$sp" 2>/dev/null || true; wait "$sp" 2>/dev/null || true; }

# Modes: 0 Passthrough (TorBox's video without its audio + the original audio),
# 1 TorBox audio (no session: tba_abrir refuses).
# 0. TorBox audio: no session.
"$work/t" sessao "http://127.0.0.1:$port/tb/playlist.m3u8?token=secret" "http://127.0.0.1:$port/orig.mkv" 1 0 "$work/url1" > "$work/s1.log" 2>&1 || true
check "TorBox audio: no Nuvio stream" "[ ! -s '$work/url1' ]"
# 1. Passthrough over MPEG-TS: every codec, on TorBox's timeline, 2 s segments.
sessao tb/playlist.m3u8 0 0
curl -s "$U" > "$work/out/master.m3u8"
# The very first request: segment 0, right after the index read moved the reader to mid-file.
curl -s -o "$work/out/a0_0.ts" "$B/a0_0.ts"
check "segment 0 first: audio from 0 s ($(inicio "$work/out/a0_0.ts" a 2>/dev/null || echo none))" \
  "[ -s '$work/out/a0_0.ts' ] && perto $(inicio "$work/out/a0_0.ts" a 2>/dev/null || echo 99) $(inicio "$work/src/tb/s0.ts" v) 0.1"
check "master: one rendition per audio track (5)" "[ \$(grep -c '^#EXT-X-MEDIA:TYPE=AUDIO' '$work/out/master.m3u8') = 5 ]"
check "master: the chosen track is DEFAULT" "grep -q 'DEFAULT=YES.*URI=\"a0.m3u8\"' '$work/out/master.m3u8'"
curl -s "$B/v.m3u8" > "$work/out/v.m3u8"
check "video playlist: TorBox's segments, absolute URLs" "grep -q '^http://127.0.0.1:$port/tb/s0.ts' '$work/out/v.m3u8'"
check "S3: the last #EXTINF matches the file (2.0 s, TorBox said 5)" "tail -3 '$work/out/v.m3u8' | grep -qE '^#EXTINF:2\\.0[0-9]*,'"
check "no URL of the source in the log" "! grep -q 'token=secret\\|orig.mkv' '$work/s.log'"
check "master: points at Nuvio's video playlist, no CODECS" "grep -q '^vs.m3u8' '$work/out/master.m3u8' && ! grep -q CODECS '$work/out/master.m3u8'"
curl -s "$B/a0.m3u8" > "$work/out/a0.m3u8"
check "audio playlist: 2 s segments (16 for 32 s)" "grep -q '^#EXT-X-TARGETDURATION:2' '$work/out/a0.m3u8' && [ \$(grep -c '^a0_' '$work/out/a0.m3u8') = 16 ]"
check "seek index read at preparation, seeks through the video (stream 0)" "grep -qE 'seek index read in [0-9]+ ms: seeks go through stream 0 \\([1-9][0-9]* entries\\)' '$work/s.log'"
for k in 0 1 4; do curl -s -o "$work/out/a${k}_2.ts" "$B/a${k}_5.ts"; done
curl -s -o "$work/out/a3i.mp4" "$B/a3_init.mp4"; curl -s -o "$work/out/a3_2.m4s" "$B/a3_5.m4s"
cat "$work/out/a3i.mp4" "$work/out/a3_2.m4s" > "$work/out/opus.mp4"
curl -s -o "$work/out/a2i.mp4" "$B/a2_init.mp4"; curl -s -o "$work/out/a2_2.m4s" "$B/a2_5.m4s"
cat "$work/out/a2i.mp4" "$work/out/a2_2.m4s" > "$work/out/flac.mp4"
v2=$(inicio "$work/src/tb/s2.ts" v)
check "AAC copied, starts with TorBox's video segment ($(inicio "$work/out/a0_2.ts" a) vs $v2)" "[ '$(codec "$work/out/a0_2.ts")' = 'aac,2' ] && perto $(inicio "$work/out/a0_2.ts" a) $v2 0.05"
check "AC3 5.1 copied ($(codec "$work/out/a1_2.ts"))" "[ '$(codec "$work/out/a1_2.ts")' = 'ac3,6' ] && perto $(inicio "$work/out/a1_2.ts" a) $v2 0.05"
check "Opus as is in fMP4 (init + segment: $(codec "$work/out/opus.mp4"))" "[ '$(codec "$work/out/opus.mp4")' = 'opus,2' ] && perto $(inicio "$work/out/opus.mp4" a) $v2 0.1"
check "Opus rendition is fMP4 in its playlist" "curl -s '$B/a3.m3u8' | grep -q '^#EXT-X-MAP:URI=\"a3_init.mp4\"' && curl -s '$B/a3.m3u8' | grep -q '^a3_0.m4s'"
check "Opus packet diagnostic logged" "grep -q 'Opus audio 3 seg 5: [0-9]* packets, durations 20.0-20.0 ms' '$work/s.log'"
check "DTS converted to AC3 5.1 ($(codec "$work/out/a4_2.ts"))" "[ '$(codec "$work/out/a4_2.ts")' = 'ac3,6' ] && perto $(inicio "$work/out/a4_2.ts" a) $v2 0.05"
check "FLAC as is in fMP4 (init + segment: $(codec "$work/out/flac.mp4"))" "[ '$(codec "$work/out/flac.mp4")' = 'flac,2' ] && perto $(inicio "$work/out/flac.mp4" a) $v2 0.1"
curl -s -o "$work/out/back.ts" "$B/a0_1.ts"
check "a seek back (segment 1 after 5) works ($(inicio "$work/out/back.ts" a 2>/dev/null || echo none))" \
  "[ -s '$work/out/back.ts' ] && perto $(inicio "$work/out/back.ts" a 2>/dev/null || echo 0) $(awk "BEGIN{print 2+$(inicio "$work/src/tb/s0.ts" v)}") 0.05"
timeout 120 ffmpeg -v error -i "$U" -map 0:v:0 -map 0:a:0 -t 12 -c copy "$work/out/play.ts" || true
check "a player reads the master: video + audio, starts together" \
  "[ -s '$work/out/play.ts' ] && perto $(inicio "$work/out/play.ts" v 2>/dev/null || echo 9) $(inicio "$work/out/play.ts" a 2>/dev/null || echo 0) 0.05"
check "the session got ready" "grep -q 'ready in' '$work/s.log'"
# A request the player gives up on is dropped, and the next one is not kept waiting.
curl -s -m 0.3 -o /dev/null "$B/a4_14.ts" || true
t0=$(date +%s%N); curl -s -o "$work/out/after.ts" "$B/a4_3.ts"; t1=$(date +%s%N)
check "after a hang-up the next segment comes ($(( (t1 - t0) / 1000000 )) ms)" "[ -s '$work/out/after.ts' ] && [ \$(( (t1 - t0) / 1000000 )) -lt 5000 ]"
fim


# 1b. The same session, the video side: TorBox's segments without their audio.
sessao tb/playlist.m3u8 0 0
curl -s "$B/vs.m3u8" | grep -q '^v_0.ts' && curl -s -o "$work/out/v2.ts" "$B/v_2.ts"
check "strip: video segment without audio, same start as TorBox's" \
  "[ \"\$(ffprobe -v error -show_entries stream=codec_type -of csv=p=0 '$work/out/v2.ts' | grep -v '^\$' | sort -u | tr '\n' ' ')\" = 'video ' ] && perto $(inicio "$work/out/v2.ts" v 2>/dev/null || echo 0) $(inicio "$work/src/tb/s2.ts" v) 0.001"
timeout 120 ffmpeg -v error -i "$U" -map 0:v:0 -map 0:a:0 -t 12 -c copy "$work/out/plays.ts" || true
check "strip: a player reads it, video + Nuvio's audio together" \
  "[ -s '$work/out/plays.ts' ] && perto $(inicio "$work/out/plays.ts" v 2>/dev/null || echo 9) $(inicio "$work/out/plays.ts" a 2>/dev/null || echo 0) 0.05"
fim

# 3b. Laced Opus without timestamps on every frame: all frames reach the segment.
sessao tb/playlist.m3u8 0 0 opus_laced.mkv
curl -s -o "$work/out/li.mp4" "$B/a0_init.mp4"; curl -s -o "$work/out/l5.m4s" "$B/a0_5.m4s"
cat "$work/out/li.mp4" "$work/out/l5.m4s" > "$work/out/laced.mp4"
lp=$(ffprobe -v error -select_streams a -count_packets -show_entries stream=nb_read_packets -of csv=p=0 "$work/out/laced.mp4" 2>/dev/null || echo 0)
ld=$(ffprobe -v error -select_streams a -show_entries packet=pts_time -of csv=p=0 "$work/out/laced.mp4" 2>/dev/null | awk 'NR==1{a=$1} {z=$1} END{printf "%.3f", z-a+0.02}')
check "laced Opus: 100 packets in a 2 s segment ($lp), 2.0 s of audio ($ld)" "[ '$lp' -ge 99 ] && [ '$lp' -le 101 ] && perto '$ld' 2.0 0.03"
check "laced Opus: the segment starts with TorBox's video ($(inicio "$work/out/laced.mp4" a))" "perto $(inicio "$work/out/laced.mp4" a) $v2 0.1"
check "laced Opus: every packet 20 ms in the log" "grep -q 'Opus audio 0 seg 5: 100 packets, durations 20.0-20.0 ms, ' '$work/s.log'"
fim


# 4. fMP4 TorBox behind a master playlist.
sessao tb4/master.m3u8 0 1
curl -s -o "$work/out/m1.ts" "$B/a1_2.ts"
check "fMP4 source: first PTS from tfdt, audio placed" "grep -q \"TorBox's first video PTS: 0.000 s\" '$work/s.log' && [ '$(codec "$work/out/m1.ts")' = 'ac3,6' ]"
fim

[ $fail = 0 ] && echo "tbaudio: ok" || { echo "tbaudio: FAIL"; exit 1; }
