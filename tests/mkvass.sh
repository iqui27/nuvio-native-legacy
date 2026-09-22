#!/bin/bash
# Legenda ASS embutida colhida por Range (#92, fase 3). Ver tests/mkvass.c.
#
# Gera o MKV AQUI (ffmpeg do Homebrew: 2 min de testsrc2 + sine + um .ass com
# 40 Dialogue, com \an8 e \pos), serve por tests/servidor_range.py e roda o
# teste contra src/mkvass.c + src/legenda.c + src/rede.c + src/dados.c —
# sem SDL. Nada de rede externa.
#   bash tests/mkvass.sh
set -eu
cd "$(dirname "$0")/.."
FFMPEG=${FFMPEG:-/opt/homebrew/bin/ffmpeg}
[ -x "$FFMPEG" ] || { echo "mkvass.sh: ffmpeg nao encontrado em $FFMPEG"; exit 1; }

DIR=$(mktemp -d /tmp/nuvio-mkvass.XXXXXX)
trap 'kill $SRV 2>/dev/null || true; if [ "${NUVIO_MKVASS_KEEP:-0}" = "1" ]; then echo "mkvass.sh: arquivos de teste mantidos em $DIR"; else rm -rf "$DIR"; fi' EXIT

# O .ass de referencia: 40 eventos, um a cada 2,9 s, dois estilos (o "Topo"
# com Alignment 8), letreiros com \an8 e um \pos com virgula no texto — que e
# o caso que quebra quem recorta o campo Text pela virgula.
python3 - "$DIR/ref.ass" <<'EOF'
import sys
L=["[Script Info]","Title: mkvass teste","ScriptType: v4.00+","PlayResX: 1280","PlayResY: 720","",
   "[V4+ Styles]",
   "Format: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, OutlineColour, BackColour, Bold, Italic, Underline, StrikeOut, ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow, Alignment, MarginL, MarginR, MarginV, Encoding",
   "Style: Default,Arial,48,&H00FFFFFF,&H000000FF,&H00000000,&H00000000,0,0,0,0,100,100,0,0,1,2,0,2,10,10,10,1",
   "Style: Topo,Arial,40,&H0000FFFF,&H000000FF,&H00000000,&H00000000,0,1,0,0,100,100,0,0,1,2,0,8,10,10,10,1",
   "","[Events]","Format: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text"]
def t(s): return "%d:%02d:%05.2f"%(int(s//3600),int(s%3600//60),s%60)
for i in range(40):
    a=1.0+i*2.9; b=a+1.75
    if i%5==0: txt="{\\an8}Letreiro %d no topo"%i; st="Topo"
    elif i%7==0: txt="{\\pos(640,100)}Posicionado %d, com virgula"%i; st="Default"
    else: txt="Fala numero %d\\Nsegunda linha"%i; st="Default"
    L.append("Dialogue: 0,%s,%s,%s,,0,0,0,,%s"%(t(a),t(b),st,txt))
long_text = "Evento longo preservado acima de 1 KiB: " + ("texto ASS 0123456789 abcdefghijklmnopqrstuvwxyz " * 32)
L.append("Dialogue: 0,%s,%s,Default,,0,0,0,,%s"%(t(118.0),t(119.75),long_text))
open(sys.argv[1],"w").write("\n".join(L)+"\n")
EOF

# 1500 kbps para o arquivo ter tamanho de verdade (uns 20 MB): a asserção de
# "< 2 % dos bytes" nao faz sentido num MKV de 200 KB.
"$FFMPEG" -v error -y \
  -f lavfi -i "testsrc2=size=640x360:rate=24:duration=120" \
  -f lavfi -i "sine=frequency=440:duration=120" \
  -i "$DIR/ref.ass" -map 0:v -map 1:a -map 2:s \
  -attach deploy/app/fonts/InterDisplay-Regular.ttf \
  -metadata:s:t:0 mimetype=font/ttf -metadata:s:t:0 filename=InterDisplay-Regular.ttf \
  -c:v libx264 -preset ultrafast -b:v 1500k -c:a aac -c:s ass "$DIR/t.mkv"
# O no-go de codec: a mesma legenda como S_TEXT/UTF8 (SRT).
"$FFMPEG" -v error -y \
  -f lavfi -i "testsrc2=size=320x180:rate=24:duration=10" \
  -i "$DIR/ref.ass" -map 0:v -map 1:s -c:v libx264 -preset ultrafast -c:s srt "$DIR/srt.mkv"
ls -la "$DIR"/*.mkv

python3 tests/servidor_range.py "$DIR" > "$DIR/porta.txt" &
SRV=$!
for _ in $(seq 1 50); do grep -q porta "$DIR/porta.txt" 2>/dev/null && break; sleep 0.1; done
PORTA=$(awk '/porta/{print $2}' "$DIR/porta.txt")
[ -n "$PORTA" ] || { echo "mkvass.sh: servidor nao subiu"; exit 1; }

cc -Isrc tests/mkvass.c src/mkvass.c src/assrender.c src/legenda.c src/rede.c src/redeurl.c src/dados.c \
  -o /tmp/nuvio-mkvass-tests -O1 -g -Wall -I/opt/homebrew/include \
  -Wno-deprecated-declarations
mkdir -p "$DIR/dados"
if [ "${NUVIO_MKVASS_LLDB:-0}" = "1" ]; then
  NUVIO_DADOS="$DIR/dados" MKV_DIR="$DIR" lldb --batch -k 'bt all' \
    -o "run http://127.0.0.1:$PORTA t.mkv $DIR/ref.ass srt.mkv ref.ass" \
    -- /tmp/nuvio-mkvass-tests
else
  NUVIO_DADOS="$DIR/dados" MKV_DIR="$DIR" /tmp/nuvio-mkvass-tests \
    "http://127.0.0.1:$PORTA" t.mkv "$DIR/ref.ass" srt.mkv ref.ass
fi
