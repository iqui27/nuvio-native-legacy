#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# #215: o app sobrevive ao servidor da conta fora do ar (504/429/sem resposta).
# Ver o cabecalho de tests/contaoffline.c. Sem rede.
#
#   bash tests/contaoffline.sh
set -eu
cd "$(dirname "$0")/.."
flags=(-O1 -g -Isrc -pthread -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
       -Wall -Wno-deprecated-declarations -Wno-macro-redefined)
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
bin=${TMPDIR:-/tmp}/nuvio-contaoffline-tests
cc "${flags[@]}" src/sync.c tests/stub_contapend.c src/catordem.c src/catordemcache.c src/contacache.c \
  src/js.c src/jsw.c tests/contaoffline.c -o "$bin"
dir="$(mktemp -d)"
trap 'rm -rf "$dir"' EXIT
export NV_T_DIR="$dir"

sessao() {
  local saida
  if ! saida=$("$bin" "$@" 2>&1); then echo "$saida"; echo "contaoffline.sh: FALHOU"; exit 1; fi
  echo "$saida" | grep -E '^(--|  )|\[sync\] servidor da conta' || true
  SAIDA="$saida"
}
precisa() { echo "$SAIDA" | grep -qF "$1" || { echo "FALHOU: faltou no log: $1"; exit 1; }; }

# 1. Primeira abertura com o servidor fora: nada salvo.
sessao 504
precisa '[sync] servidor da conta indisponivel (HTTP 504) e nenhuma copia de addons neste aparelho'
# 2. Servidor bom: a resposta vira copia.
sessao 200
# 3. Servidor fora (o log da TCL): os addons voltam da copia.
sessao 504
precisa '[sync] servidor da conta indisponivel (HTTP 504): usando a copia de '
sessao 429
precisa '[sync] servidor da conta indisponivel (HTTP 429): usando a copia de '
sessao 0
precisa '[sync] servidor da conta indisponivel (sem resposta): usando a copia de '
# 4. So as RPCs caem (429), a tabela de addons responde: cada superficie
#    cai para a sua copia.
sessao parcial
precisa '[sync] servidor da conta indisponivel (HTTP 429): usando a copia de '
# 5. Fora e depois volta.
sessao 502 - volta
# 6. A copia e da conta: outra pessoa nesta TV nao a usa; logout apaga.
sessao 504 conta-b
sessao 200 - sair
sessao 504
precisa 'nenhuma copia de addons'
# 7. #201: URL de addon longa chega inteira; a que passa do teto de leitura
#    (16 KB desde o #203; de 2 a 16 KB vira apelido, ver addonurl.h) e recusada
#    e contada no log, e a URL em si nunca aparece nele.
sessao longa
precisa '[sync] addons: 1 da conta ficaram de fora (maximo 128, ou URL grande demais)'
# A edicao local nao sobe mais (#360 mao unica): o que se confere e que NENHUMA
# pendencia e gravada (assert "#201" no .c) — nao ha mais linha de envio.
if echo "$SAIDA" | grep -q 'SEGREDO'; then echo "FALHOU: URL de addon no log"; exit 1; fi
echo "contaoffline.sh: ok"
