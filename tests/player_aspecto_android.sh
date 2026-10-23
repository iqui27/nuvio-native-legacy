#!/bin/bash
# Cada modo de aspecto no Android manda um retangulo/crop proprio ao backend
# (ver o .c). Mesma montagem do player_android_retomada.sh: player.c real com
# NV_ANDROID, video.c do Mac como coto; video_janela/video_janela_fonte do
# coto sao renomeadas para as capturas do teste ganharem o nome de verdade.
set -euo pipefail
cd "$(dirname "$0")/.."
dir=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-aspecto-android.XXXXXX")
trap 'rm -rf "$dir"' EXIT
export NUVIO_DADOS="$dir/dados"
mkdir -p "$NUVIO_DADOS"
flags=(-O1 -g -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2
  -Wno-deprecated-declarations -Wno-macro-redefined)
pure=()
if [ "${SANITIZE:-0}" = 1 ]; then pure+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
renomes=(-Dvideo_janela=nv_base_video_janela -Dvideo_janela_fonte=nv_base_video_janela_fonte)
for nome in tocar tocar_posicao tocar_retomada parar pausar buscar pausa_confirmada tocando pronto ativo falhou conflito_recurso pos duracao url_atual retomada_inicial_estado; do
  renomes+=("-Dvideo_$nome=nv_base_video_$nome")
done
cc "${flags[@]}" "${renomes[@]}" -c src/video.c -o "$dir/video.o"
cc "${flags[@]}" -DNV_ANDROID -c src/player.c -o "$dir/player.o"
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/player.c|src/video.c) continue;; esac
  sources+=("$source")
done
cc "${flags[@]}" ${pure[@]+"${pure[@]}"} "${sources[@]}" \
  tests/player_aspecto_android.c tests/player_android_coto.c "$dir/player.o" "$dir/video.o" \
  -o "$dir/test" -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL
"$dir/test"
