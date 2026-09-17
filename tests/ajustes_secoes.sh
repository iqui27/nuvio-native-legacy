#!/bin/bash
# TODA OPCAO DE AJUSTES TEM DE CABER NUMA CATEGORIA.
#
# A tela de Ajustes desenha as linhas percorrendo as FAIXAS declaradas em
# SECOES[]; uma opcao fora de toda faixa nao e desenhada por ninguem, continua
# recebendo foco e continua desenhando o painel dela na coluna da direita. Foi
# o que aconteceu com "Arredondamento do cartaz" e "Memória usada por imagens":
# o dono viu o painel do cache de imagens a direita e nenhuma linha a esquerda.
#
# O teste le o CODIGO, nao o binario: nao precisa de SDL, de GL nem de linkar a
# tela inteira, e falha por leitura de texto — que e exatamente o nivel em que o
# defeito existia.
#
#   bash tests/ajustes_secoes.sh
set -eu
cd "$(dirname "$0")/.."
python3 tests/ajustes_secoes.py src/ajustes.c
