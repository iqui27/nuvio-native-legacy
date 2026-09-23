#!/bin/bash
# #92: MKV com VARIAS legendas (ASS, SRT, ASS) — o caso dos lancamentos de
# anime "Multi-Subs". Prova que o trackNum da LG (ordinal entre as legendas)
# casa com a TrackEntry certa, e que o mkvass colhe a faixa pedida e nao a
# vizinha. Ver tests/mkv_legendas.c.
#   bash tests/mkv_legendas.sh
set -eu
cd "$(dirname "$0")/.."
FFMPEG=${FFMPEG:-/opt/homebrew/bin/ffmpeg}
[ -x "$FFMPEG" ] || { echo "mkv_legendas.sh: ffmpeg nao encontrado em $FFMPEG"; exit 1; }

DIR=$(mktemp -d /tmp/nuvio-mkvleg.XXXXXX)
trap 'kill $SRV 2>/dev/null || true; rm -rf "$DIR"' EXIT

# Tres legendas com texto que denuncia a origem: "Faixa A n", "Faixa B n"...
python3 - "$DIR" <<'PY'
import sys
d = sys.argv[1]
def t(s):
    h = int(s // 3600); m = int(s % 3600 // 60); x = s % 60
    return "%d:%02d:%05.2f" % (h, m, x)
cab = """[Script Info]
ScriptType: v4.00+
PlayResX: 640
PlayResY: 360

[V4+ Styles]
Format: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, OutlineColour, BackColour, Bold, Italic, Underline, StrikeOut, ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow, Alignment, MarginL, MarginR, MarginV, Encoding
Style: Default,Arial,28,&H00FFFFFF,&H000000FF,&H00000000,&H64000000,0,0,0,0,100,100,0,0,1,2,0,2,20,20,20,1

[Events]
Format: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text
"""
for nome in "AC":
    L = [cab]
    for i in range(12):
        a = 1.0 + i * 2.5
        L.append("Dialogue: 0,%s,%s,Default,,0,0,0,,Faixa %s %d\n" % (t(a), t(a + 1.5), nome, i))
    open("%s/%s.ass" % (d, nome), "w").write("".join(L))
S = []
for i in range(12):
    a = 1.0 + i * 2.5
    def st(s): return "00:00:%02d,%03d" % (int(s), int(round((s % 1) * 1000)))
    S.append("%d\n%s --> %s\nFaixa B %d\n" % (i + 1, st(a), st(a + 1.5), i))
open("%s/B.srt" % d, "w").write("\n".join(S))
PY

"$FFMPEG" -v error -y \
  -f lavfi -i "testsrc2=size=320x180:rate=24:duration=32" \
  -f lavfi -i "sine=frequency=440:duration=32" \
  -i "$DIR/A.ass" -i "$DIR/B.srt" -i "$DIR/C.ass" \
  -map 0:v -map 1:a -map 2:s -map 3:s -map 4:s \
  -c:v libx264 -preset ultrafast -c:a aac -c:s:0 ass -c:s:1 srt -c:s:2 ass \
  -metadata:s:s:0 language=eng -metadata:s:s:1 language=por -metadata:s:s:2 language=spa \
  "$DIR/multi.mkv"

python3 tests/servidor_range.py "$DIR" > "$DIR/porta.txt" &
SRV=$!
for _ in $(seq 1 50); do grep -q porta "$DIR/porta.txt" 2>/dev/null && break; sleep 0.1; done
PORTA=$(awk '/porta/{print $2}' "$DIR/porta.txt")
[ -n "$PORTA" ] || { echo "mkv_legendas.sh: servidor nao subiu"; exit 1; }

cc -Isrc tests/mkv_legendas.c src/mkv.c src/mkvass.c src/assrender.c src/legenda.c \
  src/rede.c src/redeurl.c src/dados.c -o /tmp/nuvio-mkv-legendas-tests -O1 -g -Wall \
  -I/opt/homebrew/include -Wno-deprecated-declarations
mkdir -p "$DIR/dados"
NUVIO_DADOS="$DIR/dados" /tmp/nuvio-mkv-legendas-tests "http://127.0.0.1:$PORTA/multi.mkv"
