#!/bin/bash
# Real player_encerrar close path linked with the whole app (pattern of
# player_android_retomada.sh). Fake video pipeline; outbound account/network
# calls renamed to counting stubs in player.c only; sandbox denies network.
set -euo pipefail
cd "$(dirname "$0")/.."
dir=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-epclose.XXXXXX")
trap 'rm -rf "$dir"' EXIT
mkdir -p "$dir/dados" "$dir/tmp"
root=$PWD
flags=(-O1 -g -I"$root/src" -I/opt/homebrew/include -I/opt/homebrew/include/SDL2
  -Wno-deprecated-declarations -Wno-macro-redefined)
renomes=()
for nome in tocar parar pausar buscar pausa_confirmada tocando pronto ativo falhou conflito_recurso pos duracao url_atual terminou creditos; do
  renomes+=("-Dvideo_$nome=nv_base_video_$nome")
done
stubs=()
for nome in trakt_scrobble_alvo trakt_marcar trakt_scrobble simkl_scrobble visto_episodios visto_titulo \
  servidores_reproducao_fim wsenvio_emitir recomenda_atividade_fim atividade_player_saiu \
  diariofontes_player_saiu sync_sujar_progresso; do
  stubs+=("-D$nome=stub_$nome")
done
mutar=(-Dcat_salvar_progresso_ep=mut_cat_salvar_progresso_ep -Dvistoep_definir=mut_vistoep_definir)
cc "${flags[@]}" "${renomes[@]}" -c src/video.c -o "$dir/video.o"
cc "${flags[@]}" "${stubs[@]}" -c src/player.c -o "$dir/player.o"
cc "${flags[@]}" "${stubs[@]}" "${mutar[@]}" -c src/player.c -o "$dir/player_mut.o"
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/player.c|src/video.c) continue;; esac
  sources+=("$root/$source")
done
libs=(-L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL)
cc "${flags[@]}" -c tests/ep_close_proof.c -o "$dir/t.o"
cc "${flags[@]}" -DNV_EPCLOSE_MUTATE -c tests/ep_close_proof.c -o "$dir/t_mut.o"
mkdir -p "$dir/obj"
(cd "$dir/obj" && cc "${flags[@]}" -c "${sources[@]}")
cc "$dir"/obj/*.o "$dir/t.o" "$dir/player.o" "$dir/video.o" -o "$dir/test" "${libs[@]}"
cc "$dir"/obj/*.o "$dir/t_mut.o" "$dir/player_mut.o" "$dir/video.o" -o "$dir/test_mut" "${libs[@]}"
run() {
  env -i PATH=/usr/bin:/bin HOME="$dir" NUVIO_DADOS="$dir/dados" TMPDIR="$dir/tmp" \
    sandbox-exec -p '(version 1)(allow default)(deny network*)' "$1"
}
run "$dir/test" > "$dir/out" 2>&1 || { grep -E "^(ok|FAIL|stubs)" "$dir/out"; echo "ep_close_proof: FAIL"; exit 1; }
grep -E '^(ok|stubs|ep close)' "$dir/out"
rm -rf "$dir/dados" && mkdir -p "$dir/dados"
if run "$dir/test_mut" > "$dir/out_mut" 2>&1; then echo "ep_close_proof: mutation survived"; exit 1; fi
grep -m1 '^FAIL' "$dir/out_mut" | sed 's/^/mutation caught: /'
echo "ep_close_proof: PASS"
