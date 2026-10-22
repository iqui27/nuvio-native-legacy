#!/bin/bash
# O "A SEGUIR" DO CONTINUAR ASSISTINDO RESPEITA O QUE FOI DESMARCADO NA TV
# (Silo, tt14688458): remoto diz "ultimo visto T2E10", T2E7..E10 desmarcados
# aqui -> o card mostra T2E7. Trakt, Simkl e conta Nuvio falsos (sem rede).
#
#   bash tests/cwdesmarcado.sh
#
# NO COMMIT PAI (sem desc_lapides_primeira) este script COMPILA e FALHA nas
# assercoes: e a prova do defeito.
set -euo pipefail
cd "$(dirname "$0")/.."
tmp="$(mktemp -d "${TMPDIR:-/tmp}/nuvio-cwdesm-XXXXXX")"
trap 'rm -rf "$tmp"' EXIT
extra=()
if grep -q desc_lapides_primeira src/descoberta.h; then extra=(-DTEM_AJUSTE); fi
cc -O1 -g -Wall -Wno-deprecated-declarations -Wno-macro-redefined -Wno-unused-function \
  ${extra[@]+"${extra[@]}"} -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  src/catalogo.c src/progresso.c src/cwordem.c src/cwretido.c src/vistonao.c tests/cwdesmarcado.c \
  src/cotacat.c src/js.c src/metaprov.c src/colecoes.c src/redeurl.c src/catordem.c \
  -o "$tmp/cwdesmarcado" -lpthread
falhou=0
"$tmp/cwdesmarcado" | tee "$tmp/saida.txt" || falhou=1

# O LOG que se le na TV: uma linha por item ajustado por montagem.
exige() {
  n="$(grep -Ec "$1" "$tmp/saida.txt" || true)"
  if [ "$n" -ne "$2" ]; then
    echo "  FALHOU: log '$1' apareceu $n vez(es), esperado $2"; falhou=1
  else
    echo "  ok      log ($n): $1"
  fi
}
echo
echo "log:"
P='^\[desc\] continuar assistindo: tt14688458 a seguir ajustado pela desmarcacao: '
# OFF filtra antes de ajustar/deduplicar: nao produz log de sugestao descartada.
exige "${P}T3E1 -> T2E7\$" 3
exige "${P}T2E11 -> T2E7\$" 2
exige "${P}T3E1 -> T2E9\$" 1
[ "$falhou" -eq 0 ]
