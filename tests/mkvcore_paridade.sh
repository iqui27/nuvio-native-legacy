#!/bin/bash
# 205-rust-mkvcore: paridade C x Rust por CAMPOS + fuzz curto e limitado.
# src/mkv.c e compilado duas vezes com os simbolos publicos renomeados
# (ref_* = caminho C, rs_* = -DNV_RUST_MKVCORE + librust). Entradas: MKV valido
# em todos os limites, truncado em cada byte, tamanhos > 2 GiB, desconhecido,
# cap=0, NAL invalido, bytes aleatorios e mutacoes. Cada buffer fica colado numa
# pagina PROT_NONE: leitura/escrita fora vira SIGSEGV (rc != 0), nao "parecido".
#   bash tests/mkvcore_paridade.sh [iteracoes]
set -eu
cd "$(dirname "$0")/.."
LIB=$(bash tools/rust-mkvcore.sh)
DIR=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-mkvcore-par.XXXXXX")
trap 'rm -rf "$DIR"' EXIT
NOMES="mkv_diag mkv_quadro_tem_rpu mkv_rpu_alem mkv_faixas mkv_faixas_e_caps mkv_contentor mkv_ultimo_contentor \
mkv_contentor_definitivo mkv_contentor_nome mkv_faixas_do_trecho mkv_creditos_nomeados mkv_creditos_ultimo \
mkv_capitulos_alem mkv_intro_nomeada mkv_previa_nomeada mkv_casar_legendas mkv_tracks_alem"
ren() { for n in $NOMES; do printf -- '-D%s=%s%s ' "$n" "$1" "$n"; done; }
CF="-Isrc -O1 -g -Wall -Wno-deprecated-declarations -I/opt/homebrew/include"
cc $CF $(ren ref_) -c src/mkv.c -o "$DIR/ref.o"
cc $CF $(ren rs_) -DNV_RUST_MKVCORE -c src/mkv.c -o "$DIR/rs.o"
cc $CF -c tests/mkvcore_paridade.c -o "$DIR/t.o"
cc $CF -c src/rede.c -o "$DIR/rede.o"
cc $CF -c src/redeurl.c -o "$DIR/redeurl.o"
cc "$DIR/t.o" "$DIR/ref.o" "$DIR/rs.o" "$DIR/rede.o" "$DIR/redeurl.o" "$LIB" -o "$DIR/par" \
  -L/opt/homebrew/lib -lcurl 2>/dev/null || \
cc "$DIR/t.o" "$DIR/ref.o" "$DIR/rs.o" "$DIR/rede.o" "$DIR/redeurl.o" "$LIB" -o "$DIR/par" -lcurl
"$DIR/par" "$@"
