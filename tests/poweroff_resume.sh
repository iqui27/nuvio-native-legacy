#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nv-poweroff.XXXXXX")
trap 'rm -rf "$work"' EXIT
flags=(-O1 -g -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -Wno-deprecated-declarations -Wno-macro-redefined)
renames=()
for name in tocar parar pausar buscar pausa_confirmada tocando pronto ativo falhou conflito_recurso pos duracao url_atual; do
  renames+=("-Dvideo_$name=nv_base_video_$name")
done
cc "${flags[@]}" "${renames[@]}" -c src/video.c -o "$work/video.o"
cc "${flags[@]}" -Dclock_gettime=nv_test_clock -Dsessao_usuario=nv_test_usuario -Ddesc_episodios_garantir=nv_test_episodes -Dsync_sujar_progresso=nv_test_sync -Dtrakt_scrobble=nv_test_trakt -Dsimkl_scrobble=nv_test_simkl -c src/player.c -o "$work/player.o"
# Expose the existing local row builder without adding a production API.
python3 - "$work/discovery.c" <<'PY'
import pathlib, sys
source = pathlib.Path('src/descoberta.c').read_text()
source = source.replace('static int continuarLocal(', 'int nv_test_continuarLocal(', 1)
source = source.replace('continuarLocal(daConta,', 'nv_test_continuarLocal(daConta,')
pathlib.Path(sys.argv[1]).write_text(source)
PY
cc "${flags[@]}" -Dtrakt_enfeitar_lote=nv_test_enrich -Dtrakt_enfeitar_lote_sessao=nv_test_enrich_session -c "$work/discovery.c" -o "$work/discovery.o"
cc "${flags[@]}" -Dfsync=nv_test_fsync -Drename=nv_test_rename -c src/dados.c -o "$work/data.o"
cc "${flags[@]}" -Drede_postar_st=nv_base_rede_postar_st -c src/rede.c -o "$work/network.o"
cc "${flags[@]}" -Dsimklauth_token=nv_test_simkl_token -Dsimklauth_credencial_copiar=nv_test_simkl_cred -Dnuvem_simkl_cliente=nv_test_simkl_client -Dnuvem_simkl_app=nv_test_simkl_app -DSMK_SCR_ESPACO_MS=0 -c src/simkl.c -o "$work/simkl.o"
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/player.c|src/video.c|src/descoberta.c|src/dados.c|src/rede.c|src/simkl.c) continue;; esac
  sources+=("$source")
done
cc "${flags[@]}" "${sources[@]}" tests/poweroff_resume.c "$work/player.o" "$work/video.o" "$work/discovery.o" "$work/data.o" "$work/network.o" "$work/simkl.o" \
  -o "$work/test" -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL
for mode in periodic background cadence; do
  mkdir "$work/$mode"
  NUVIO_DADOS="$work/$mode" "$work/test" "$mode"
done
