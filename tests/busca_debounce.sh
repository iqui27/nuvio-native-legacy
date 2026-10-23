#!/bin/bash
# BUSCA (#368): debounce de rede, fileiras anteriores sem piscar, 1 lote de catalogo.
#   bash tests/busca_debounce.sh
set -euo pipefail
cd "$(dirname "$0")/.."
tmp="$(mktemp -d "${TMPDIR:-/tmp}/nuvio-busca-deb-XXXXXX")"
trap 'rm -rf "$tmp"' EXIT
flags=(-DNV_TRAKT_CLIENT_ID='"chave-de-teste"' -O1 -g -Isrc
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2
  -Wno-deprecated-declarations -Wno-macro-redefined)
redir=()
for f in desc_buscar desc_busca_n_alvos desc_busca_alvo_n desc_busca_alvo_item desc_busca_alvo_titulo \
         desc_busca_alvo_addon desc_busca_alvo_nuvio desc_busca_n_fontes desc_busca_base_oculta \
         desc_busca_n desc_busca_chegou cat_acrescentar_lote; do
  redir+=("-D$f=teste_$f")
done
# cat_acrescentar_lote: o stub chama o REAL, entao o nome do stub e teste_cat_lote
redir=("${redir[@]/-Dcat_acrescentar_lote=teste_cat_acrescentar_lote/-Dcat_acrescentar_lote=teste_cat_lote}")
cc -c src/busca.c -DNV_BUSCA_TESTE "${redir[@]}" "${flags[@]}" -o "$tmp/busca.o"
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/busca.c|src/novidades1312.c) continue;; esac
  sources+=("$source")
done
cc "${sources[@]}" "$tmp/busca.o" tests/busca_debounce.c -x c - "${flags[@]}" \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL -o "$tmp/t" <<'EOS'
int novidades1312_aberto(void) { return 0; }
int novidades1312_primeira_vez(void) { return 0; }
void novidades1312_atualizar(float dt, unsigned int agora) { (void)dt; (void)agora; }
void novidades1312_desenhar(unsigned int agora) { (void)agora; }
void novidades1312_evento(const void *e) { (void)e; }
EOS
dados="$(mktemp -d "${TMPDIR:-/tmp}/nuvio-busca-deb-dados-XXXXXX")"
NUVIO_DADOS="$dados" SDL_AUDIODRIVER=dummy "$tmp/t"
rm -rf "$dados"
