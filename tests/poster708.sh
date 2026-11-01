#!/bin/bash
# #708: posteres personalizados em TODA fileira de catalogo/busca. O servico so
# aplica quando quem desenha o cartaz passa por posterprov_card_addon (ou
# posterprov_card / posterprov_card_formato). Dois caminhos liam it.poster cru:
# a arte das linhas de titulo do Spotlight (spotlight.c arteDe, o "For you") e
# as capas da lista Popular/Ver tudo do menu contextual (ctxmenu.c).
#
#   bash tests/poster708.sh
set -eu
cd "$(dirname "$0")/.."
falha=0

# 1) spotlight.c: a arte da linha de titulo tem de passar pelo provedor mesmo
#    quando ci->poster existe — o retorno direto de ci->poster era o bug.
if awk '/^static const char \*arteDe/,/^}/' src/spotlight.c \
     | grep -qE 'return ci->poster;'; then
  echo "FALHOU: spotlight.c arteDe ainda devolve ci->poster sem o provedor"
  falha=1
fi
if ! awk '/^static const char \*arteDe/,/^}/' src/spotlight.c \
     | grep -q 'posterprov_card_addon'; then
  echo "FALHOU: spotlight.c arteDe nao chama posterprov_card_addon"
  falha=1
fi

# 2) ctxmenu.c: as capas da lista (ctxlista_abrir) nao podem usar it.poster cru.
if grep -nE 'u = it\.poster\[0\]' src/ctxmenu.c; then
  echo "FALHOU: ctxmenu.c ainda desenha it.poster sem o provedor"
  falha=1
fi
if ! grep -q 'posterprov_card_addon(it\.origem, it\.imdb, it\.tmdb, it\.tipo, it\.poster)' src/ctxmenu.c; then
  echo "FALHOU: capas da lista nao passam por posterprov_card_addon"
  falha=1
fi

[ "$falha" -eq 0 ] && echo "ok  #708: todas as fileiras passam pelo servico de posteres"
exit "$falha"
