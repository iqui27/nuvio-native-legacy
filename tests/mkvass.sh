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
cc -I/opt/homebrew/include tests/video_url.c src/video.c -o /tmp/nuvio-video-url-test
/tmp/nuvio-video-url-test
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
# #92 (webOS 25): o MESMO arquivo sem CuePoint da faixa de legenda (mkvmerge
# --cues none — o caso que era no-go e devolvia a faixa ao renderizador da TV)
# e sem Cues nenhum. mkvmerge renumera as faixas na ordem (video 1, audio 2,
# legenda 3), a mesma da fixture do ffmpeg, entao a faixa 3 continua sendo a
# legenda. Sem mkvmerge os dois casos sao pulados, com aviso.
MKVMERGE=${MKVMERGE:-/opt/homebrew/bin/mkvmerge}
SEMCUES=""; NOCUES=""; SEMREL=""
if [ -x "$MKVMERGE" ]; then
  "$MKVMERGE" -q -o "$DIR/semcues.mkv" --cues 2:none "$DIR/t.mkv" >/dev/null 2>&1 || true
  "$MKVMERGE" -q -o "$DIR/nocues.mkv" --no-cues "$DIR/t.mkv" >/dev/null 2>&1 || true
  # CuePoint da faixa SEM CueRelativePosition (mkvmerge --engage
  # no_cue_relative_position): o indice diz o Cluster, nao o bloco.
  "$MKVMERGE" -q -o "$DIR/semrel.mkv" --engage no_cue_relative_position "$DIR/t.mkv" >/dev/null 2>&1 || true
  [ -s "$DIR/semcues.mkv" ] && SEMCUES=semcues.mkv
  [ -s "$DIR/nocues.mkv" ] && NOCUES=nocues.mkv
  [ -s "$DIR/semrel.mkv" ] && SEMREL=semrel.mkv
else
  echo "mkvass.sh: mkvmerge nao encontrado em $MKVMERGE; casos de varredura pulados"
fi
# CueRelativePosition presente mas INVALIDO (aponta para o meio do Timestamp
# do Cluster): o bloco nao esta onde o indice diz. Ver tests/mkv_rel_ruim.py.
python3 tests/mkv_rel_ruim.py "$DIR/t.mkv" "$DIR/relruim.mkv" 3
# Uma SEGUNDA faixa ASS, com outro texto, para a troca de faixa com colheita
# em voo: nada da primeira pode aparecer depois da troca.
python3 - "$DIR/ref2.ass" "$DIR/ref.ass" <<'EOF'
import sys
cab=open(sys.argv[2]).read().split("[Events]")[0]
L=[cab+"[Events]","Format: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text"]
def t(s): return "%d:%02d:%05.2f"%(int(s//3600),int(s%3600//60),s%60)
for i in range(10):
    a=1.0+i*2.9
    L.append("Dialogue: 0,%s,%s,Default,,0,0,0,,Outra faixa %d"%(t(a),t(a+1.75),i))
open(sys.argv[1],"w").write("\n".join(L)+"\n")
EOF
"$FFMPEG" -v error -y \
  -f lavfi -i "testsrc2=size=320x180:rate=24:duration=30" \
  -f lavfi -i "sine=frequency=440:duration=30" \
  -i "$DIR/ref2.ass" -map 0:v -map 1:a -map 2:s \
  -c:v libx264 -preset ultrafast -c:a aac -c:s ass "$DIR/b.mkv"
ls -la "$DIR"/*.mkv

python3 tests/servidor_range.py "$DIR" > "$DIR/porta.txt" &
SRV=$!
for _ in $(seq 1 50); do grep -q porta "$DIR/porta.txt" 2>/dev/null && break; sleep 0.1; done
PORTA=$(awk '/porta/{print $2}' "$DIR/porta.txt")
[ -n "$PORTA" ] || { echo "mkvass.sh: servidor nao subiu"; exit 1; }

# Janela da VARREDURA em 30 s (padrao 120) para a fixture de 120 s poder
# provar que os bytes lidos sao proporcionais a janela, nao ao arquivo.
cc -Isrc -DMKVASS_VARRE_JANELA_SEG=30.0 tests/mkvass.c src/mkvass.c src/assrender.c src/legenda.c src/rede.c src/redeurl.c src/dados.c src/mkv.c \
  -o /tmp/nuvio-mkvass-tests -O1 -g -Wall -I/opt/homebrew/include \
  -Wno-deprecated-declarations
mkdir -p "$DIR/dados"
if [ "${NUVIO_MKVASS_LLDB:-0}" = "1" ]; then
  NUVIO_DADOS="$DIR/dados" MKV_DIR="$DIR" lldb --batch -k 'bt all' \
    -o "run http://127.0.0.1:$PORTA t.mkv $DIR/ref.ass srt.mkv ref.ass $SEMCUES $NOCUES $SEMREL relruim.mkv b.mkv" \
    -- /tmp/nuvio-mkvass-tests
else
  NUVIO_DADOS="$DIR/dados" MKV_DIR="$DIR" /tmp/nuvio-mkvass-tests \
    "http://127.0.0.1:$PORTA" t.mkv "$DIR/ref.ass" srt.mkv ref.ass "$SEMCUES" "$NOCUES" \
    "$SEMREL" relruim.mkv b.mkv
fi

# Os mesmos tempos, agora pelo LIBASS (o que a TV desenha). Ver tests/ass_tempos.c.
if command -v pkg-config >/dev/null 2>&1 && pkg-config --exists libass; then
  cc tests/ass_tempos.c -o /tmp/nuvio-ass-tempos-test $(pkg-config --cflags --libs libass)
  SC=$(grep -l "^; mkvass-estado: completo" "$DIR"/dados/mkvass-*-3.ass 2>/dev/null | head -1)
  [ -n "$SC" ] || { echo "mkvass.sh: nenhum sidecar completo para conferir no libass"; exit 1; }
  /tmp/nuvio-ass-tempos-test "$DIR/ref.ass" "$SC"
fi
