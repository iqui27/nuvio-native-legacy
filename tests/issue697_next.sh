#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
dir=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-epclose.XXXXXX")
trap 'rm -rf "$dir"' EXIT
mkdir -p "$dir/obj" "$dir/dados" "$dir/tmp"
root=$PWD
flags=(-O1 -I"$root/src" -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -Wno-deprecated-declarations -Wno-macro-redefined)
renomes=()
for nome in tocar parar pausar buscar pausa_confirmada tocando pronto ativo falhou conflito_recurso pos duracao url_atual terminou creditos; do renomes+=("-Dvideo_$nome=nv_base_video_$nome"); done
stubs=()
for nome in trakt_scrobble_alvo trakt_marcar trakt_scrobble simkl_scrobble visto_episodios visto_titulo servidores_reproducao_fim wsenvio_emitir recomenda_atividade_fim atividade_player_saiu diariofontes_player_saiu sync_sujar_progresso; do stubs+=("-D$nome=stub_$nome"); done
cc "${flags[@]}" "${renomes[@]}" -c src/video.c -o "$dir/video.o"
cc "${flags[@]}" "${stubs[@]}" -c src/player.c -o "$dir/player.o"
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/app.c|src/player.c|src/video.c) continue;; esac
  sources+=("$root/$source")
done
(cd "$dir/obj" && cc "${flags[@]}" -c "${sources[@]}")
cc "${flags[@]}" -c tests/issue697_app.c -o "$dir/app.o"
cc "${flags[@]}" -c tests/issue697_next.c -o "$dir/test.o"
cc "$dir"/obj/*.o "$dir/app.o" "$dir/test.o" "$dir/player.o" "$dir/video.o" -o "$dir/test" -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL
run() {
  env -i PATH=/usr/bin:/bin HOME="$dir" NUVIO_DADOS="$dir/dados" TMPDIR="$dir/tmp" sandbox-exec -p '(version 1)(allow default)(deny network*)' "$@"
}
run "$dir/test"
# Only app.c comes from before the existing fix; other objects are HEAD.
git show ca4c99014^:src/app.c > "$dir/app-before.c"
cc "${flags[@]}" -DISSUE697_APP=\""$dir/app-before.c"\" -c tests/issue697_app.c -o "$dir/app-before.o"
cc "$dir"/obj/*.o "$dir/app-before.o" "$dir/test.o" "$dir/player.o" "$dir/video.o" -o "$dir/test-before" -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL
run "$dir/test-before" 1
if run "$dir/test-before" 2 > "$dir/before-2.log" 2>&1; then
  printf 'FAIL: historical removed-title scenario unexpectedly passed\n'; exit 1
fi
if ! grep -q 'FAIL .*actual && !strcmp(actual->imdb' "$dir/before-2.log"; then
  printf 'FAIL: historical scenario failed for an unrelated reason\n'
  grep 'FAIL ' "$dir/before-2.log" || true
  exit 1
fi
grep '^wrong title:' "$dir/before-2.log"
printf 'PASS historical removed-title scenario: wrong title detected\n'
