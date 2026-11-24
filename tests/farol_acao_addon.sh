#!/bin/bash
# Farol #17: acoes de add-on e de debrid (isolar por 24 h, fontes, debrid).
#   1. tests/farol_acao_addon.c       — farolacao_addon.c com o anel de verdade,
#      relogio injetado, arquivo de quarentena real em NUVIO_DADOS, teto de 3,
#      unico de fontes, Desfazer, ilha, pedidos de fontes/debrid ao app.
#   2. tests/farol_acao_addon_addons.c — o addons.c real: o isolado sai da busca
#      de fontes (tambem da consulta capturada) ate vencer ou ser desfeito,
#      "De lado ate amanha" na lista e nos Ajustes, guarda do ultimo sob corrida.
#
#   bash tests/farol_acao_addon.sh
#   SANITIZE=0 bash tests/farol_acao_addon.sh   # sem ASan+UBSan
#   SANITIZE=1 bash tests/farol_acao_addon.sh   # com (o padrao e 0)
set -eu
cd "$(dirname "$0")/.."

# Zero _Thread_local nos arquivos novos; o modulo nao puxa SDL por conta propria.
if grep -n "_Thread_local" src/farolacao_addon.c src/farolacao_addon.h tests/farol_acao_addon.c tests/farol_acao_addon_addons.c; then
  echo "farol_acao_addon: _Thread_local proibido" >&2; exit 1
fi
if grep -n "SDL2/SDL" src/farolacao_addon.c src/farolacao_addon.h; then
  echo "farol_acao_addon: SDL nao entra direto em farolacao_addon" >&2; exit 1
fi
# O "de lado" so aparece por chave de traducao (FT_ADDON_DE_LADO), nunca por literal.
if grep -n 'i18n("De lado\|"De lado até' src/addons.c src/farolacao_addon.c; then
  echo "farol_acao_addon: texto cru no lugar de FT_ADDON_DE_LADO" >&2; exit 1
fi

flags=(-std=gnu11 -Wall -Wextra -O1 -g -pthread -DRESOLVER_TESTE -Isrc -Itests
       -I/opt/homebrew/include -I/opt/homebrew/include/SDL2
       -ffunction-sections -fdata-sections)
if [[ $(uname -s) == Darwin ]]; then flags+=(-Wl,-dead_strip); else flags+=(-Wl,--gc-sections); fi
if command -v pkg-config >/dev/null && pkg-config --exists sdl2; then
  read -r -a sdk_flags <<< "$(pkg-config --cflags sdl2)"
  flags+=("${sdk_flags[@]}")
fi
ccbin="${CC:-cc}"
case "${SANITIZE:-0}" in
  1) flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer) ;;
  thread)
    flags+=(-fsanitize=thread)
    [ -z "${CC:-}" ] && command -v clang >/dev/null 2>&1 && ccbin=clang
    ;;
esac

work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-farol-acao-addon.XXXXXX")
trap 'rm -rf "$work"' EXIT
mkdir -p "$work/dados"

"$ccbin" "${flags[@]}" \
  tests/farol_acao_addon.c src/farolacao_addon.c src/addonstats.c \
  src/farol.c src/farolanel.c src/farolregra.c src/resolverregra.c src/farolerro.c src/fontesonda.c \
  -lm -o "$work/acao-addon"
NUVIO_DADOS="$work/dados" "$work/acao-addon"

# As duas funcoes dos Ajustes que mostram a quarentena (linha e painel), de
# verdade: extraidas de src/ajustes_ux_ilha.inc para o teste com addons.c real.
awk '/^static const char \*ajAddon(Apoio|NestaTv)\(int i\) \{$/,/^\}$/' src/ajustes_ux_ilha.inc \
  > "$work/ajustes_addon_estado.inc"
if [ "$(grep -c '^static const char \*ajAddon' "$work/ajustes_addon_estado.inc")" != 2 ]; then
  echo "farol_acao_addon: ajAddonApoio/ajAddonNestaTv sumiram de src/ajustes_ux_ilha.inc" >&2; exit 1
fi
if ! grep -q 'ajAddonApoio(i)' src/ajustes_ux_ilha.inc || ! grep -q 'ajAddonNestaTv(foco)' src/ajustes_ux_ilha.inc; then
  echo "farol_acao_addon: a lista ou o painel dos Ajustes nao usam o estado da quarentena" >&2; exit 1
fi

"$ccbin" "${flags[@]}" -I"$work" \
  tests/farol_acao_addon_addons.c src/farolacao_addon.c \
  src/js.c src/addonmodo.c src/jsw.c src/watchstate.c src/addonstats.c src/fontefalha.c \
  src/idcanal.c src/linguas.c src/legextras.c src/farolanel.c \
  -lm -o "$work/acao-addon-addons"
NUVIO_DADOS="$work/dados" "$work/acao-addon-addons"

echo "farol_acao_addon: tudo ok"
