#!/bin/bash
# Acoes de rede, cache, buffer e HDR do Farol (issue #34, Onda 2, ADENDO 2.1 C):
#   (a) RECONECTAR_REDE zera DNS/conexoes (rede_zerar_cache) e refaz o teste curto;
#   (b) LIMPAR_CACHE apaga so o cache descartavel e preserva ajustes, contas,
#       sessao, tokens, Diario e fontefalha.txt num NUVIO_DADOS temporario;
#   (c) BUFFER_MAIOR e um override temporario (ajuste gravado intacto), pede a
#       reabertura na posicao e some ao encerrar;
#   (d) DESLIGAR_HDR temporario x persistente, com Desfazer devolvendo o anterior.
# Mais o teste do src/rede.c REAL (rede_zerar_cache sem interromper fios).
#
#   bash tests/farol_acao_rede.sh
#   SANITIZE=1 bash tests/farol_acao_rede.sh      # ASan + UBSan
set -eu
cd "$(dirname "$0")/.."

if grep -n "_Thread_local" src/farolacao_rede.c src/farolacao_rede.h; then
  echo "farol_acao_rede: _Thread_local proibido" >&2; exit 1
fi
# O executor nao pode mexer na rota de canal ao vivo (fora do escopo da #34).
if grep -n -E "urlCanalRota|retentarCanal" src/farolacao_rede.c src/farolacao_rede.h; then
  echo "farol_acao_rede: canal ao vivo fora do escopo" >&2; exit 1
fi
# LIMPAR_CACHE nunca nomeia dado da pessoa: so a limpeza do cache de imagens.
if grep -n -E "ajustes\.txt|contas|sessao|token|diario|perfis|fontefalha|dados_apagar|remove\(|unlink\(" \
     src/farolacao_rede.c; then
  echo "farol_acao_rede: o modulo nao pode apagar nem nomear dado da pessoa" >&2; exit 1
fi

flags=(-std=gnu11 -O1 -g -Wall -Wextra -Werror -Wno-unused-function -Wno-misleading-indentation -Isrc -pthread)
if pkg-config --exists sdl2 2>/dev/null; then
  read -r -a sdk <<< "$(pkg-config --cflags sdl2)"
  flags+=("${sdk[@]}")
fi
ccbin="${CC:-cc}"
case "${SANITIZE:-0}" in
  1) flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer) ;;
  thread)
    flags+=(-fsanitize=thread)
    [ -z "${CC:-}" ] && command -v clang >/dev/null 2>&1 && ccbin=clang
    ;;
esac

work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-farol-acao-rede.XXXXXX")
trap 'rm -rf "$work"' EXIT
mkdir "$work/dados"

"$ccbin" "${flags[@]}" tests/farol_acao_rede.c src/farolacao_rede.c src/cachearte.c \
  -lm -o "$work/teste"
"$work/teste" "$work/dados"

# rede.c de verdade (libcurl simulada): -Wno-* so do que o rede.c ja carrega.
rflags=(-std=gnu11 -O1 -g -Wall -Wextra -Wno-unused-function -Wno-unused-parameter -Isrc -pthread)
case "${SANITIZE:-0}" in
  1) rflags+=(-fsanitize=address,undefined -fno-omit-frame-pointer) ;;
  thread) rflags+=(-fsanitize=thread) ;;
esac
"$ccbin" "${rflags[@]}" tests/farol_acao_rede_cache.c src/redeurl.c -ldl -lm -o "$work/teste-rede"
"$work/teste-rede"
echo "farol_acao_rede: tudo ok"
