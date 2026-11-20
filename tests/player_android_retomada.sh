#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
dir=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-android-retomada.XXXXXX")
trap 'rm -rf "$dir"' EXIT
export NUVIO_DADOS="$dir/dados"
mkdir -p "$NUVIO_DADOS"
flags=(-O1 -g -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2
  -Wno-deprecated-declarations -Wno-macro-redefined)
pure=()
if [ "${SANITIZE:-0}" = 1 ]; then pure+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
# A regra numerica tem sanitizadores sem carregar o SDL do sistema.
cc "${flags[@]}" ${pure[@]+"${pure[@]}"} -ffunction-sections -fdata-sections \
  src/player.c tests/player_retomada_regra.c -Wl,-dead_strip -o "$dir/regra"
"$dir/regra"
# O app completo precisa do SDL instalado; o teste de integracao e normal,
# como os testes existentes do player. JNI e exercitado separadamente.
renomes=()
for nome in tocar parar pausar buscar pausa_confirmada tocando pronto ativo falhou conflito_recurso pos duracao url_atual; do
  renomes+=("-Dvideo_$nome=nv_base_video_$nome")
done
cc "${flags[@]}" "${renomes[@]}" -c src/video.c -o "$dir/video.o"
cc "${flags[@]}" -DNV_ANDROID -c src/player.c -o "$dir/player.o"
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/player.c|src/video.c) continue;; esac
  sources+=("$source")
done
cc "${flags[@]}" "${sources[@]}" tests/player_android_retomada.c tests/player_android_coto.c "$dir/player.o" "$dir/video.o" \
  -o "$dir/test" -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL
"$dir/test"
