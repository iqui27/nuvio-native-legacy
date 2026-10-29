#!/bin/bash
# DESMARCAR GANHA (Silo, tt14688458): um episodio que a pessoa desmarcou na TV
# nao volta marcado porque o Trakt ou a conta Nuvio ainda dizem "visto", nem
# depois da poda do jornal da conta, nem depois de fechar o app.
# Fontes falsas em tests/vistonao.c; vistoep/contapend/contalib/js de verdade.
#
#   bash tests/vistonao.sh
#
# NO COMMIT PAI (sem src/vistonao.c) este script COMPILA e FALHA nas
# assercoes: e a prova do defeito.
set -euo pipefail
cd "$(dirname "$0")/.."
tmp="$(mktemp -d "${TMPDIR:-/tmp}/nuvio-vistonao-XXXXXX")"
trap 'rm -rf "$tmp"' EXIT
extra=()
if [ -f src/vistonao.c ]; then extra=(-DTEM_VISTONAO src/vistonao.c); fi
cc -O1 -g -Wall -Wextra -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -Wno-deprecated-declarations -Wno-macro-redefined \
  ${extra[@]+"${extra[@]}"} src/vistoep.c src/contapend.c src/contalib.c \
  src/js.c src/jsw.c tests/vistonao.c -o "$tmp/vistonao" -lpthread
# Diagnostico separado: falhas conhecidas de reconciliacao, sem fingir que
# reproduzem a desmarcacao remota nao registrada no log da TV.
if [ "${1:-}" = --site ]; then "$tmp/vistonao" --site; exit; fi
falhou=0
"$tmp/vistonao" | tee "$tmp/saida.txt" || falhou=1

# O LOG POR FONTE, que e o que se le na TV: uma linha por titulo por leitura,
# dizendo quantos a fonte marcou e quantos a desmarcacao barrou (e quais).
exige() {
  if ! grep -Eq "$1" "$tmp/saida.txt"; then
    echo "  FALHOU: falta no log a linha: $1"; falhou=1
  else
    echo "  ok      log: $1"
  fi
}
echo
echo "log por fonte:"
exige '^\[vistoep\] tt14688458: trakt \+28 \(bloqueados 0'
exige '^\[vistoep\] tt14688458: trakt \+24 \(bloqueados 4: T2E7 T2E8 T2E9 T2E10; remoto mais novo 0\)'
exige '^\[vistoep\] tt14688458: trakt \+25 \(bloqueados 3: T2E8 T2E9 T2E10; remoto mais novo 1\)'
# Confirmed V entries stay as evidence (#548), so the journal rejects this
# stale row before vistoep_fonte; its blocks are logged separately.
exige '^\[vistoep\] conta: \+1 episodios \(bloqueados 0; remoto mais novo 0\) \| 1 seguros pelo jornal \(intencao de visto preservada\)$'

# NENHUMA FONTE ESCREVE "VISTO" NO MAPA SEM PASSAR PELO JUIZ. vistoep_definir e
# do gesto local (player, tela); leitor de rede usa vistoep_fonte. O Simkl nao
# le episodio nenhum para o mapa — se um dia ler, e por vistoep_fonte.
echo
echo "fontes passam pelo juiz:"
for f in src/extras.c src/contalib.c src/contapend.c src/simkl.c src/trakt.c; do
  if grep -n 'vistoep_definir *(' "$f" >/dev/null; then
    echo "  FALHOU: $f escreve no mapa com vistoep_definir (sem consultar a desmarcacao)"
    falhou=1
  else
    echo "  ok      $f"
  fi
done
[ "$falhou" -eq 0 ]
