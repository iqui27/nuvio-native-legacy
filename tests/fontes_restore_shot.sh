#!/bin/bash
# Foreground native comparison; one shared build, only streams.c differs.
set -euo pipefail
cd "$(dirname "$0")/.."
out="${1:-/Volumes/ExternalSSD/tmp/rev/21-21-fontes}"
work=$(mktemp -d "${TMPDIR:-/tmp}/nv-fontes-restore.XXXXXX")
trap 'rm -rf "$work"' EXIT
mkdir -p "$out" "$work/data" "$work/obj"
flags=(-DNV_SHOT_HOOKS -Isrc -O1 -I/opt/homebrew/include -I/opt/homebrew/include/SDL2
  -Wno-deprecated-declarations -Wno-macro-redefined)
export NV_FONTES_OBJECTS="$work/obj"
# xargs waits for every compiler; no background captures or watchers.
sources=()
for source in src/*.c src/dts/*.c; do
  [[ "$source" != src/main.c && "$source" != src/streams.c ]] && sources+=("$source")
done
printf '%s\n' "${sources[@]}" tests/fontes_restore_shot.c |
  xargs -P 8 -I{} sh -c 'cc "$@" -c "$0" -o "$NV_FONTES_OBJECTS/$(basename "$0").o"' '{}' "${flags[@]}"
for revision in integracao/2.0.5 5423a59d4 fixed; do
  label=${revision##*/}
  [[ "$revision" != 5423a59d4 ]] || label=HEAD
  if [[ "$revision" == fixed ]]; then cp src/streams.c "$work/streams.c"
  else git show "$revision:src/streams.c" > "$work/streams.c"
  fi
  if [[ "$revision" == integracao/2.0.5 ]]; then
    # Current callers link against new APIs. The reference renderer never calls these.
    python3 - "$work/streams.c" <<'PY'
import sys
from pathlib import Path
p = Path(sys.argv[1])
p.write_text(p.read_text() + '''
void stream_folha_servidor(int provider) { (void)provider; }
const char *stream_idioma(const Stream *s) { return s ? idiomaDa(s) : NULL; }
int stream_texto_tem(const Stream *s, const char *p) { return s && strstr(s->descricao,p)!=NULL; }
int stream_incompativel(int i) { (void)i; return 0; }
void stream_inspecao_ordem(int *r, int n) { for(int i=0;i<n;i++) r[i]=i+1; }
int stream_play_candidata(void) { return stream_automatico(); }
void stream_folha_canais_abrir(const char *s) { (void)s; }
int stream_folha_canais_aberta(void) { return 0; }
void stream_folha_canais_fechar(void) {}
float (*stream_folha_sala_h)(void);
void (*stream_folha_sala_desenhar)(float, float, float, float);
int stream_fonte_estado(int i) { (void)i; return 0; }
''')
PY
  fi
  cc "${flags[@]}" -c "$work/streams.c" -o "$work/streams.o"
  cc "$work/obj/"*.o "$work/streams.o" -o "$work/shot" -L/opt/homebrew/lib \
    -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL
  for scale in ${FONTES_SCALES:-85 100}; do
    for material in glass solid; do
      # HEAD reproduces the retained Live TV scope; fixed restores the VOD actions.
      retained=""
      [[ "$revision" == integracao/2.0.5 ]] || retained=1
      NUVIO_DADOS="$work/data" NUVIO_TESTE_DIR="$work/data" FONTES_RETAINED_LIVE="$retained" \
        "$work/shot" "$out/$label-$scale-$material" "$scale" "$material"
      if [[ "$revision" == fixed && "${FONTES_SERVER:-1}" == 1 ]]; then
        NUVIO_DADOS="$work/data" NUVIO_TESTE_DIR="$work/data" \
          "$work/shot" "$out/server-$scale-$material" "$scale" "$material" server
      fi
    done
  done
done
for scale in ${FONTES_SCALES:-85 100}; do
  for material in glass solid; do
    magick "$out/2.0.5-$scale-$material-list.png" "$out/HEAD-$scale-$material-list.png" "$out/fixed-$scale-$material-list.png" \
      +append "$out/compare-$scale-$material.png"
    magick "$out/fixed-$scale-$material-clock.png" "$out/fixed-$scale-$material-open-016.png" \
      "$out/fixed-$scale-$material-open-096.png" "$out/fixed-$scale-$material-open-224.png" \
      "$out/fixed-$scale-$material-list.png" "$out/fixed-$scale-$material-close-016.png" \
      "$out/fixed-$scale-$material-close-096.png" "$out/fixed-$scale-$material-close-224.png" \
      "$out/fixed-$scale-$material-closed.png" -resize 640x360 +append "$out/motion-$scale-$material.png"
  done
done
