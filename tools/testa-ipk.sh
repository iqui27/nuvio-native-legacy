#!/bin/bash
# Prova que o .ipk NAO leva credencial de pessoa — sem precisar de docker.
#
# Roda so o trecho de empacotamento do arm.sh (o mesmo palco, a mesma lista de
# exclusao, a mesma conferencia) usando o nuvio-proto que ja esta em
# deploy/app. Existe porque o ciclo normal exige compilar para ARM, e ninguem
# vai rodar um build de 30s so para conferir uma lista de arquivos.
#
# DUAS ARMADILHAS que este teste existe para nao deixar voltar:
#   1. `ares-package deploy/app` leva a pasta INTEIRA, e art/ tem o token do
#      Trakt, as URLs de addon com chave de debrid, a chave do TMDB e a do
#      mdblist (modo 0600).
#   2. o .ipk e um pacote Debian: `tar tzf pacote.ipk` lista SEM ERRO apenas
#      debian-binary / control.tar.gz / data.tar.gz. Uma conferencia escrita
#      assim passa sempre, inclusive com o segredo dentro.
#
# Conferido nos dois sentidos: com a exclusao, sai limpo; deixando trakt.txt
# entrar de proposito, o teste ACUSA e devolve 1.
set -e
cd "$(dirname "$0")/.."
ARES="../NuvioWeb-0.3.38-beta/node_modules/.bin/ares-package"
ARQ_DE_PESSOA="trakt.txt addons.txt tmdb.txt mdblist.txt ajustes.txt
               progresso.txt nuvem.txt sessao.txt perfil.txt cliente.txt home-pos.txt"
PALCO=$(mktemp -d)
trap 'rm -rf "$PALCO"' EXIT
cp -R deploy/app "$PALCO/app"
rm -rf "$PALCO/app/art/cache"
for f in $ARQ_DE_PESSOA; do rm -f "$PALCO/app/art/$f"; done

SAIDA=$(mktemp -d)
"$ARES" "$PALCO/app" -o "$SAIDA" >/dev/null
IPK=$(ls -t "$SAIDA"/*.ipk | head -1)

LISTA=$(cd "$PALCO" && ar x "$IPK" 2>/dev/null && tar tzf data.tar.gz 2>/dev/null)
[ -z "$LISTA" ] && { echo "ABORTADO: nao consegui LER o pacote para conferir"; exit 1; }

echo "pacote de teste: $(du -h "$IPK" | cut -f1)"
echo "arquivos .txt dentro de art/:"
printf '%s\n' "$LISTA" | grep -E "art/.*\.txt$" | sed 's|.*/art/|  |' | sort
VAZOU=""
for f in $ARQ_DE_PESSOA; do
  printf '%s\n' "$LISTA" | grep -q "art/$f$" && VAZOU="$VAZOU $f"
done
rm -rf "$SAIDA"
if [ -n "$VAZOU" ]; then echo "FALHOU: o pacote leva credencial ->$VAZOU"; exit 1; fi
echo "OK: nenhuma credencial no pacote"
