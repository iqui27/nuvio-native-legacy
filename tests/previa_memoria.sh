#!/bin/bash
# A OPCAO "BAIXA" DA MEMORIA DO GUIA DESLIGA O PROVEDOR LOCAL NA HORA.
#
# "No aparelho" so pode existir quando o aparelho passa na MESMA regra de
# memoria do guia (memoria.h). Na versao reprovada, trocar "Memoria do guia"
# para Baixa escondia a opcao da lista mas deixava o provedor local aplicado
# ate o heartbeat do worker expirar — a thread de miniaturas seguia decodificando
# num aparelho que acabou de declarar pouca memoria.
#
# O teste extrai as funcoes de selecao de provedor de src/ajustes.c (codigo de
# producao, compilado) e dirige o cenario da troca: aplica "No aparelho", muda
# para Baixa e confere que seekr_usar_provedor ja tirou o provedor local.
#
#   bash tests/previa_memoria.sh
set -eu
cd "$(dirname "$0")/.."

CC="${CC:-cc}"
tmp="${TMPDIR:-/tmp}/previa_memoria.$$"
mkdir -p "$tmp"
trap 'rm -rf "$tmp"' EXIT

python3 tests/previa_memoria.py "$tmp/previa_memoria.c"
for alvo in TPK WEBOS; do
  "$CC" -std=c99 -Wall -Wextra -DNV_$alvo -Isrc "$tmp/previa_memoria.c" -o "$tmp/previa_$alvo"
  "$tmp/previa_$alvo"
done
