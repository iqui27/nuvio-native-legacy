#!/bin/bash
# #524: real player -> progress and outgoing statistics identity.
set -eu
cd "$(dirname "$0")/.."
dir=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-player-identity524.XXXXXX")
trap 'rm -rf "$dir"' EXIT
flags=(-O1 -g -Isrc -Wno-deprecated-declarations -Wno-macro-redefined)
libs=()
if [ "$(uname -s)" = Darwin ]; then
  flags+=(-I/opt/homebrew/include -I/opt/homebrew/include/SDL2)
  libs+=(-L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL)
else
  read -r -a depflags <<< "$(pkg-config --cflags sdl2 SDL2_image SDL2_ttf glesv2 egl zlib)"
  read -r -a deplibs <<< "$(pkg-config --libs sdl2 SDL2_image SDL2_ttf glesv2 egl zlib)"
  flags+=(-DNV_LINUX_DESKTOP "${depflags[@]}")
  libs+=("${deplibs[@]}" -ldl -pthread -lm)
fi
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
renomes=()
for name in tocar parar pausar buscar pausa_confirmada tocando pronto ativo falhou conflito_recurso pos duracao url_atual; do
  renomes+=("-Dvideo_$name=nv_base_video_$name")
done
cc "${flags[@]}" "${renomes[@]}" -c src/video.c -o "$dir/video.o"
cc "${flags[@]}" -Dsessao_usuario=nv_test_usuario \
  -Ddesc_episodios_garantir=nv_identity_desc_garantir \
  -Dintro_pedir_ids=nv_identity_intro_pedir -Dintro_pedir_ids_mapeados=nv_identity_intro_mapeado -Dtrakt_marcar=nv_identity_trakt_marcar -Dtrakt_scrobble_alvo=nv_identity_scrobble_alvo \
  -Dparental_pedir=nv_identity_parental_pedir -Ddesc_refazer_continuar=nv_identity_refazer_continuar \
  -c src/player.c -o "$dir/player.o"
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/player.c|src/video.c) continue;; esac
  sources+=("$source")
done
cc "${flags[@]}" "${sources[@]}" tests/player_identity524.c "$dir/player.o" "$dir/video.o" \
  -o "$dir/test" "${libs[@]}"
failed=0
for scenario in known-series-control known-series-default known-series-special movie-control late-movie-control \
  late-series late-close-control next-opening-control retained-series-control; do
  mkdir "$dir/$scenario"
  if NUVIO_DADOS="$dir/$scenario" "$dir/test" "$scenario"; then printf 'PASS %s\n' "$scenario";
  else printf 'FAIL %s\n' "$scenario"; failed=1; fi
done
exit "$failed"
