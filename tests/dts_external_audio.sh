#!/bin/sh
# External audio: video of one MKV with the audio of another (engine level).
# Requires the ffmpeg CLI and the host FFmpeg libraries of
# tools/build-dts-ffmpeg.sh (NUVIO_DTS_HOST=1 CC=gcc), like tests/dts_engine.sh.
set -eu
cd "$(dirname "$0")/.."
PREFIX=${NUVIO_DTS_ROOT:-${TMPDIR:-/tmp}/nuvio-dts-host}
TMP=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-ext-audio.XXXXXXXX")
trap 'rm -rf "$TMP"' EXIT
SAN=""
if [ "${SANITIZE:-0}" = 1 ]; then SAN="-fsanitize=address,undefined -fno-omit-frame-pointer"; fi
if [ "$(uname -s)" = Darwin ]; then
  # Homebrew FFmpeg 9: real codecs/demuxers, separate from the TV's 7.1.5 pin.
  DTS_CFLAGS=$(pkg-config --cflags libavformat libavcodec libswresample libavutil)
  DTS_LIBS=$(pkg-config --libs libavformat libavcodec libswresample libavutil)
else
  DTS_CFLAGS="-I$PREFIX/include"
  DTS_LIBS="-L$PREFIX/lib -Wl,--start-group -lavformat -lavcodec -lswresample -lavutil -Wl,--end-group"
fi
# shellcheck disable=SC2086
${CC:-cc} $SAN -std=gnu11 -Wall -Wextra -Werror -DNV_DTS_FFMPEG -DNV_DTS_WEBOS_RECOVERY $DTS_CFLAGS \
  src/dts/dts_engine.c tests/dts_external_audio.c -o "$TMP/ext" $DTS_LIBS -lm -lpthread
printf '1\n00:00:00,500 --> 00:00:01,500\nExternal audio fixture\n' > "$TMP/sub.srt"
# Main source: 25 fps video, its own DTS track (never read) and a subtitle.
ffmpeg -hide_banner -loglevel error \
  -f lavfi -i 'color=size=64x64:rate=25:duration=4' \
  -f lavfi -i 'sine=frequency=300:sample_rate=48000:duration=4' -i "$TMP/sub.srt" \
  -map 0:v -map 1:a -map 2:s -c:v libx264 -bf 0 -g 25 -c:a dca -strict -2 -ac 2 -c:s srt \
  -metadata:s:a:0 language=eng "$TMP/main.mkv"
# Another version: its own video, E-AC-3 English and AC-3 5.1 Polish.
ffmpeg -hide_banner -loglevel error \
  -f lavfi -i 'color=c=red:size=64x64:rate=25:duration=4' \
  -f lavfi -i 'sine=frequency=500:sample_rate=48000:duration=4' \
  -f lavfi -i 'aevalsrc=0.1*sin(2*PI*700*t)|0.1*sin(2*PI*700*t)|0.1*sin(2*PI*700*t)|0.1*sin(2*PI*100*t)|0.1*sin(2*PI*700*t)|0.1*sin(2*PI*700*t):s=48000:d=4:c=5.1' \
  -map 0:v -map 1:a -map 2:a -c:v libx264 -bf 0 -g 25 -c:a:0 eac3 -ac:a:0 2 -c:a:1 ac3 \
  -metadata:s:a:0 language=eng -metadata:s:a:1 language=pol "$TMP/other.mkv"
# A version with only DTS: refused for now.
ffmpeg -hide_banner -loglevel error \
  -f lavfi -i 'color=size=64x64:rate=25:duration=4' \
  -f lavfi -i 'sine=frequency=300:sample_rate=48000:duration=4' \
  -map 0:v -map 1:a -c:v libx264 -c:a dca -strict -2 -ac 2 "$TMP/dts.mkv"
# A 1 s clip with usable audio: what a debrid serves for a file not in cache.
ffmpeg -hide_banner -loglevel error \
  -f lavfi -i 'color=size=64x64:rate=25:duration=1' \
  -f lavfi -i 'sine=frequency=300:sample_rate=48000:duration=1' \
  -map 0:v -map 1:a -c:v libx264 -c:a ac3 "$TMP/short.mkv"
# An MP4 version, Polish AC-3, whose picture is one large keyframe a second
# and tiny frames between: its audio samples gather into about one chunk per
# second, as in a coarsely interleaved WEB-DL, and audio only skips the
# keyframes.
ffmpeg -hide_banner -loglevel error -f lavfi -i 'nullsrc=s=1280x720:d=0.04,geq=random(1)*255:128:128' \
  -frames:v 1 "$TMP/noise.png"
ffmpeg -hide_banner -loglevel error -loop 1 -framerate 25 -t 20 -i "$TMP/noise.png" \
  -f lavfi -i 'sine=frequency=600:sample_rate=48000:duration=20' \
  -map 0:v -map 1:a -c:v libx264 -preset ultrafast -qp 10 -g 25 -bf 0 -pix_fmt yuv420p \
  -c:a ac3 -ac 2 -metadata:s:a:0 language=pol -movflags +faststart "$TMP/other.mp4"
# A Polish dub in AAC at 44.1 kHz, the format of the WEB-DLs in the owner's log.
ffmpeg -hide_banner -loglevel error \
  -f lavfi -i 'color=size=64x64:rate=24000/1001:duration=4' \
  -f lavfi -i 'sine=frequency=400:sample_rate=44100:duration=4' \
  -map 0:v -map 1:a -c:v libx264 -c:a aac -ac 2 -ar 44100 -metadata:s:a:0 language=pol "$TMP/aac44.mkv"
# Long versions for the seek cost: a 60 s source, and a 60 s other version with
# a heavy picture, with its index (Cues) and without one (a stream muxed to a
# pipe has none). Seeking to 50 s must not read the file from the start.
ffmpeg -hide_banner -loglevel error \
  -f lavfi -i 'color=size=64x64:rate=25:duration=60' \
  -f lavfi -i 'sine=frequency=300:sample_rate=48000:duration=60' \
  -map 0:v -map 1:a -c:v libx264 -bf 0 -g 25 -c:a ac3 "$TMP/main60.mkv"
ffmpeg -hide_banner -loglevel error \
  -f lavfi -i 'nullsrc=s=320x180:d=60,geq=random(1)*255:128:128' \
  -f lavfi -i 'sine=frequency=500:sample_rate=48000:duration=60' \
  -map 0:v -map 1:a -c:v libx264 -preset ultrafast -qp 30 -g 25 -bf 0 -pix_fmt yuv420p \
  -c:a ac3 -metadata:s:a:0 language=pol "$TMP/long.mkv"
ffmpeg -hide_banner -loglevel error -i "$TMP/long.mkv" -c copy -f matroska - < /dev/null > "$TMP/long_nocues.mkv"
# "Match automatically": a shared bed of random hits (music and effects) with
# different "speech" on each; the dub's bed is 0.7 s late.
python3 - "$TMP" <<'PY'
import random, struct, sys, wave
R = 48000
def bursts(seed, n, count, amp):
    rnd = random.Random(seed); out = [0.0] * n
    for _ in range(count):
        at = rnd.randrange(n); a = amp * (0.3 + rnd.random()); ln = rnd.randrange(R // 20, R // 4)
        for i in range(at, min(n, at + ln)):
            out[i] += a * (rnd.random() * 2 - 1) * (1 - (i - at) / ln)
    return out
def write(path, x):
    with wave.open(path, "wb") as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(R)
        w.writeframes(b"".join(struct.pack("<h", max(-32767, min(32767, int(v * 12000)))) for v in x))
n = 45 * R; d = int(0.7 * R)
bed = bursts(1, n + d, 260, 1.0)
write(sys.argv[1] + "/m.wav", [b + s for b, s in zip(bed[d:], bursts(2, n, 160, 0.5))])
write(sys.argv[1] + "/d.wav", [b + s for b, s in zip(bed[:n], bursts(3, n, 160, 0.5))])
PY
ffmpeg -hide_banner -loglevel error -f lavfi -i 'color=size=64x64:rate=25:duration=45' -i "$TMP/m.wav" \
  -map 0:v -map 1:a -c:v libx264 -bf 0 -g 25 -c:a ac3 -shortest "$TMP/match_main.mkv"
ffmpeg -hide_banner -loglevel error -f lavfi -i 'color=c=red:size=64x64:rate=25:duration=45' -i "$TMP/d.wav" \
  -map 0:v -map 1:a -c:v libx264 -bf 0 -g 25 -c:a eac3 -metadata:s:a:0 language=pol -shortest "$TMP/match_dub.mkv"
# AVI and MPEG-TS: an H.264 AVI picture source (as a DivX-era release with an
# H.264 stream), an AVI version with MP3 audio, a TS version with AC-3, and an
# old MPEG-4 Part 2 (XviD) AVI.
ffmpeg -hide_banner -loglevel error \
  -f lavfi -i 'color=size=64x64:rate=25:duration=4' \
  -f lavfi -i 'sine=frequency=300:sample_rate=48000:duration=4' \
  -map 0:v -map 1:a -c:v libx264 -bf 2 -g 25 -c:a ac3 "$TMP/main.avi"
ffmpeg -hide_banner -loglevel error \
  -f lavfi -i 'color=c=red:size=64x64:rate=25:duration=4' \
  -f lavfi -i 'sine=frequency=500:sample_rate=48000:duration=4' \
  -map 0:v -map 1:a -c:v mpeg4 -g 25 -c:a libmp3lame -ac 2 "$TMP/other.avi"
ffmpeg -hide_banner -loglevel error \
  -f lavfi -i 'color=c=red:size=64x64:rate=25:duration=4' \
  -f lavfi -i 'sine=frequency=500:sample_rate=48000:duration=4' \
  -map 0:v -map 1:a -c:v libx264 -bf 0 -g 25 -c:a ac3 -metadata:s:a:0 language=pol "$TMP/other.ts"
ffmpeg -hide_banner -loglevel error \
  -f lavfi -i 'color=size=64x64:rate=25:duration=4' \
  -f lavfi -i 'sine=frequency=300:sample_rate=48000:duration=4' \
  -map 0:v -map 1:a -c:v mpeg4 -g 25 -c:a ac3 "$TMP/xvid.avi"
# An 11-minute "film" (1 fps picture): the version a placeholder clip stands for.
ffmpeg -hide_banner -loglevel error \
  -f lavfi -i 'color=size=16x16:rate=1:duration=660' \
  -f lavfi -i 'sine=frequency=300:sample_rate=48000:duration=660' \
  -map 0:v -map 1:a -c:v libx264 -g 10 -c:a ac3 -b:a 64k -metadata:s:a:0 language=pol "$TMP/film.mkv"
# Another cut and a PAL version of a 120 s film (8 kHz beds, AC-3 at 48 kHz):
# the cut's bed starts 40 s later; the PAL one runs 25/23.976 faster.
python3 - "$TMP" <<'PY'
import array, random, sys, wave
R = 8000
def bursts(seed, seconds, count, amp):
    rnd = random.Random(seed); ev = []
    for _ in range(count):
        ev.append((rnd.random() * seconds, amp * (0.3 + rnd.random()), rnd.uniform(0.05, 0.25), rnd.random()))
    return ev
def render(path, seconds, layers):
    n = int(seconds * R); x = [0.0] * n
    for ev, warp in layers:
        for at, a, ln, ph in ev:
            t0 = warp(at)
            if t0 is None or t0 < 0 or t0 >= seconds: continue
            i0 = int(t0 * R); m = int(ln * R)
            rnd = random.Random(int(ph * 1e9))
            for i in range(i0, min(n, i0 + m)):
                x[i] += a * (rnd.random() * 2 - 1) * (1 - (i - i0) / m)
    a = array.array('h', (max(-32767, min(32767, int(v * 9000))) for v in x))
    with wave.open(path, 'wb') as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(R); w.writeframes(a.tobytes())
K = 25 / (24000 / 1001)
bed = bursts(11, 200, 1400, 1.0)
render(sys.argv[1] + '/c_main.wav', 120, [(bed, lambda t: t), (bursts(12, 120, 500, 0.5), lambda t: t)])
render(sys.argv[1] + '/c_dub.wav', 160, [(bed, lambda t: t + 40), (bursts(13, 160, 650, 0.5), lambda t: t),
                                         (bursts(14, 40, 160, 1.0), lambda t: t)])
render(sys.argv[1] + '/c_pal.wav', 120 / K, [(bed, lambda t: t / K), (bursts(15, 120, 500, 0.5), lambda t: t)])
# Another cut to follow: the "extended" picture has 25 s at 60 s that the
# "theatrical" version lacks (there, its own unrelated sound).
ext = bursts(21, 200, 1400, 1.0)
def warp_ext(at):   # the bed's event at `at` (theatrical time) in the extended timeline
    return at if at < 60 else at + 25
render(sys.argv[1] + '/f_ext.wav', 120, [(ext, warp_ext), (bursts(22, 25, 160, 1.0), lambda t: t + 60)])
render(sys.argv[1] + '/f_th.wav', 95, [(ext, lambda t: t), (bursts(23, 95, 100, 0.25), lambda t: t)])
# The same cut in other languages (owner's log 08/11, The Godfather): this
# file has a Russian dub (default) over the same effects and its English
# original; the other version is a Polish voice-over on the original (quiet)
# with loud Polish speech, and 30 s more at the end (credits), so it is taken
# for "another cut". Nothing is missing: no switch may happen.
fx = bursts(31, 150, 700, 0.6)
en = bursts(32, 150, 900, 1.0)
render(sys.argv[1] + '/x_ru.wav', 120, [(fx, lambda t: t), (bursts(33, 120, 900, 1.3), lambda t: t)])
render(sys.argv[1] + '/x_en.wav', 120, [(fx, lambda t: t), (en, lambda t: t)])
render(sys.argv[1] + '/x_pl.wav', 150, [(fx, lambda t: t), (en, lambda t: t), (bursts(34, 150, 1300, 1.6), lambda t: t)])
# Only a dub in each, sharing louder effects (scores like the log's, ~0.2-0.4).
# Dialogue scenes (both talk at the same places) drown the shared effects.
def talk(seed, count, amp):
    rnd = random.Random(seed); ev = []
    for a, b in ((22, 38), (50, 64), (72, 112)):
        for _ in range(count):
            ev.append((rnd.uniform(a, b), amp * (0.5 + rnd.random()), rnd.uniform(0.08, 0.3), rnd.random()))
    return ev
fx2 = bursts(35, 150, 900, 0.8)
render(sys.argv[1] + '/y_ru.wav', 120, [(fx2, lambda t: t), (talk(36, 260, 2.6), lambda t: t)])
render(sys.argv[1] + '/y_pl.wav', 150, [(fx2, lambda t: t), (talk(37, 260, 2.6), lambda t: t)])
# Two episode releases (owner's log 08/11, Chicago Fire): a gap 0.6 s longer
# at 70 s in the other one (and 15 s more at the end): the sync shifts by 0.6 s.
ep = bursts(41, 200, 1400, 1.0)
render(sys.argv[1] + '/s_main.wav', 120, [(ep, lambda t: t)])
render(sys.argv[1] + '/s_dub.wav', 135, [(ep, lambda t: t if t < 70 else t + 0.6), (bursts(42, 135, 120, 0.2), lambda t: t)])
# Same frame rate, a gap 0.5 s longer at 200 s: the second look (5 min after
# the first find) must move the offset, not believe a speed difference.
lg = bursts(43, 400, 2600, 1.0)
render(sys.argv[1] + '/l_main.wav', 380, [(lg, lambda t: t)])
render(sys.argv[1] + '/l_dub.wav', 380, [(lg, lambda t: t if t < 200 else t + 0.5), (bursts(44, 380, 300, 0.2), lambda t: t)])
PY
ffmpeg -hide_banner -loglevel error -f lavfi -i 'color=size=32x32:rate=24000/1001:duration=120' -i "$TMP/c_main.wav" \
  -map 0:v -map 1:a -c:v libx264 -bf 0 -g 24 -c:a ac3 -ar 48000 -b:a 96k -metadata:s:a:0 language=eng -shortest "$TMP/cut_main.mkv"
# A version with this film's English 3.2 s later and an unrelated Polish
# track (the one played): the English one is what gets compared.
ffmpeg -hide_banner -loglevel error -f lavfi -i 'color=c=green:size=32x32:rate=24000/1001:duration=124' \
  -i "$TMP/c_main.wav" -i "$TMP/c_dub.wav" \
  -filter_complex '[1:a]adelay=3200,aresample=48000[en];[2:a]aresample=48000[pl]' \
  -map 0:v -map '[en]' -map '[pl]' -c:v libx264 -bf 0 -g 24 -c:a eac3 -b:a 96k \
  -metadata:s:a:0 language=eng -metadata:s:a:1 language=pol -shortest "$TMP/pair_dub.mkv"
ffmpeg -hide_banner -loglevel error -f lavfi -i 'color=c=red:size=32x32:rate=24000/1001:duration=160' -i "$TMP/c_dub.wav" \
  -map 0:v -map 1:a -c:v libx264 -bf 0 -g 24 -c:a eac3 -ar 48000 -b:a 96k -metadata:s:a:0 language=pol -shortest "$TMP/cut_dub.mkv"
ffmpeg -hide_banner -loglevel error -f lavfi -i 'color=c=blue:size=32x32:rate=25:duration=116' -i "$TMP/c_pal.wav" \
  -map 0:v -map 1:a -c:v libx264 -bf 0 -g 25 -c:a ac3 -ar 48000 -b:a 96k -metadata:s:a:0 language=pol -shortest "$TMP/pal_dub.mkv"
ffmpeg -hide_banner -loglevel error -f lavfi -i 'color=size=32x32:rate=25:duration=120' -i "$TMP/f_ext.wav" \
  -map 0:v -map 1:a -c:v libx264 -bf 0 -g 25 -c:a ac3 -ar 48000 -b:a 96k -metadata:s:a:0 language=eng -shortest "$TMP/follow_ext.mkv"
ffmpeg -hide_banner -loglevel error -f lavfi -i 'color=c=red:size=32x32:rate=25:duration=95' -i "$TMP/f_th.wav" \
  -map 0:v -map 1:a -c:v libx264 -bf 0 -g 25 -c:a eac3 -ar 48000 -b:a 96k -metadata:s:a:0 language=eng -shortest "$TMP/follow_th.mkv"
ffmpeg -hide_banner -loglevel error -f lavfi -i 'color=size=32x32:rate=25:duration=120' -i "$TMP/x_ru.wav" -i "$TMP/x_en.wav" \
  -map 0:v -map 1:a -map 2:a -c:v libx264 -bf 0 -g 25 -c:a ac3 -ar 48000 -b:a 96k \
  -metadata:s:a:0 language=rus -metadata:s:a:1 language=eng -disposition:a:0 default -disposition:a:1 0 -shortest "$TMP/lang_main.mkv"
ffmpeg -hide_banner -loglevel error -f lavfi -i 'color=c=red:size=32x32:rate=25:duration=150' -i "$TMP/x_pl.wav" \
  -map 0:v -map 1:a -c:v libx264 -bf 0 -g 25 -c:a vorbis -strict -2 -q:a 8 -ar 48000 -ac 2 -metadata:s:a:0 language=pol -shortest "$TMP/lang_pl.mkv"
ffmpeg -hide_banner -loglevel error -f lavfi -i 'color=size=32x32:rate=25:duration=120' -i "$TMP/y_ru.wav" \
  -map 0:v -map 1:a -c:v libx264 -bf 0 -g 25 -c:a ac3 -ar 48000 -b:a 96k -metadata:s:a:0 language=rus -shortest "$TMP/dub_ru.mkv"
ffmpeg -hide_banner -loglevel error -f lavfi -i 'color=c=red:size=32x32:rate=25:duration=150' -i "$TMP/y_pl.wav" \
  -map 0:v -map 1:a -c:v libx264 -bf 0 -g 25 -c:a vorbis -strict -2 -q:a 8 -ar 48000 -ac 2 -metadata:s:a:0 language=pol -shortest "$TMP/dub_pl.mkv"
ffmpeg -hide_banner -loglevel error -f lavfi -i 'color=size=32x32:rate=25:duration=120' -i "$TMP/s_main.wav" \
  -map 0:v -map 1:a -c:v libx264 -bf 0 -g 25 -c:a ac3 -ar 48000 -b:a 96k -metadata:s:a:0 language=eng -shortest "$TMP/shift_main.mkv"
ffmpeg -hide_banner -loglevel error -f lavfi -i 'color=c=red:size=32x32:rate=25:duration=135' -i "$TMP/s_dub.wav" \
  -map 0:v -map 1:a -c:v libx264 -bf 0 -g 25 -c:a eac3 -ar 48000 -b:a 96k -metadata:s:a:0 language=eng -shortest "$TMP/shift_dub.mkv"
ffmpeg -hide_banner -loglevel error -f lavfi -i 'color=size=16x16:rate=24000/1001:duration=380' -i "$TMP/l_main.wav" \
  -map 0:v -map 1:a -c:v libx264 -bf 0 -g 48 -c:a ac3 -ar 48000 -b:a 64k -metadata:s:a:0 language=eng -shortest "$TMP/gap_main.mkv"
ffmpeg -hide_banner -loglevel error -f lavfi -i 'color=c=red:size=16x16:rate=24000/1001:duration=380' -i "$TMP/l_dub.wav" \
  -map 0:v -map 1:a -c:v libx264 -bf 0 -g 48 -c:a ac3 -ar 48000 -b:a 64k -metadata:s:a:0 language=eng -shortest "$TMP/gap_dub.mkv"
# Two English tracks (owner 09/11): the series memory keeps the chosen one.
ffmpeg -hide_banner -loglevel error -f lavfi -i 'color=c=red:size=32x32:rate=25:duration=20' -i "$TMP/x_en.wav" -i "$TMP/x_ru.wav" \
  -map 0:v -map 1:a -map 2:a -t 20 -c:v libx264 -bf 0 -g 25 -c:a ac3 -ac 2 -ar 48000 -b:a 96k \
  -metadata:s:a:0 language=eng -metadata:s:a:0 title=Main -metadata:s:a:1 language=eng -metadata:s:a:1 title=Commentary "$TMP/two_en.mkv"
# The same cut fixtures labelled as two languages: a real missing scene is still found.
ffmpeg -hide_banner -loglevel error -i "$TMP/follow_ext.mkv" -map 0 -c copy -metadata:s:a:0 language=rus "$TMP/follow_ext_ru.mkv"
ffmpeg -hide_banner -loglevel error -i "$TMP/follow_th.mkv" -map 0 -c copy -metadata:s:a:0 language=pol "$TMP/follow_th_pl.mkv"
"$TMP/ext" "$TMP/main.mkv" "$TMP/other.mkv" "$TMP/dts.mkv" "$TMP/short.mkv" "$TMP/other.mp4" "$TMP/aac44.mkv" \
  "$TMP/main60.mkv" "$TMP/long.mkv" "$TMP/long_nocues.mkv" "$TMP/match_main.mkv" "$TMP/match_dub.mkv" \
  "$TMP/main.avi" "$TMP/other.avi" "$TMP/other.ts" "$TMP/xvid.avi" "$TMP/film.mkv" \
  "$TMP/cut_main.mkv" "$TMP/cut_dub.mkv" "$TMP/pal_dub.mkv" "$TMP/pair_dub.mkv" \
  "$TMP/follow_ext.mkv" "$TMP/follow_th.mkv" "$TMP/lang_main.mkv" "$TMP/lang_pl.mkv" "$TMP/dub_ru.mkv" "$TMP/dub_pl.mkv" "$TMP/follow_ext_ru.mkv" "$TMP/follow_th_pl.mkv" "$TMP/shift_main.mkv" "$TMP/shift_dub.mkv" "$TMP/gap_main.mkv" "$TMP/gap_dub.mkv" "$TMP/two_en.mkv"
