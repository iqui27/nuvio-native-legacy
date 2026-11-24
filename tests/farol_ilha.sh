#!/bin/bash
# Farol na Ilha (issue 20): a politica de silencio "quando falar", pura e com
# relogio injetado. Ver tests/farol_ilha.c e src/farolilha.h.
#   bash tests/farol_ilha.sh              (sem sanitizador)
#   SANITIZE=1 bash tests/farol_ilha.sh   (ASan+UBSan)
set -eu
cd "$(dirname "$0")/.."

# Contratos estaticos: o modulo e puro e a presenca e a Ilha que ja existe.
for f in src/farolilha.c src/farolilha.h; do
  if grep -n "_Thread_local" "$f"; then echo "farol_ilha: _Thread_local proibido ($f)" >&2; exit 1; fi
  if grep -nE '#include *[<"](SDL2/)?SDL' "$f"; then echo "farol_ilha: SDL nao entra no modulo puro ($f)" >&2; exit 1; fi
  if grep -nE '(fopen|dados_|curl_|pthread_)' "$f"; then echo "farol_ilha: disco/rede/fio no modulo puro ($f)" >&2; exit 1; fi
done
if grep -nE 'gfx_(fbo|alvo)|GFX_FBO|glGenFramebuffers|NV_TELA_W, NV_TELA_H' src/farolilha.c src/ilhasinais.c; then
  echo "farol_ilha: sem FBO nem camada de tela cheia para o Farol" >&2; exit 1
fi
grep -q 'farolilha_definir_ativo' src/farolilha.h || { echo "farol_ilha: falta o gancho da #27" >&2; exit 1; }
grep -q 'FT_PILULA_FALHA' src/ilhasinais.c || { echo "farol_ilha: aviso sem a frase de faroltexto.h" >&2; exit 1; }
grep -q 'FT_ABRE_O_FAROL' src/ilhasinais.c || { echo "farol_ilha: tecla sem 'abre o Farol'" >&2; exit 1; }
grep -q '"aj_farol"' src/ilhasinais.c || { echo "farol_ilha: aviso sem o icone aj_farol" >&2; exit 1; }
[ -f deploy/app/art/icones/aj_farol.png ] || { echo "farol_ilha: falta deploy/app/art/icones/aj_farol.png" >&2; exit 1; }
[ -f deploy/app/art/icones/lucide/farol.svg ] || { echo "farol_ilha: falta o SVG fonte do aj_farol" >&2; exit 1; }
grep -qw farol tools/icones-lucide.sh || { echo "farol_ilha: aj_farol fora de tools/icones-lucide.sh" >&2; exit 1; }

flags=(-Wall -Wextra -Werror -O1 -g)
ccbin="${CC:-cc}"
case "${SANITIZE:-0}" in
  1) flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=undefined) ;;
  thread)
    flags+=(-fsanitize=thread)
    [ -z "${CC:-}" ] && command -v clang >/dev/null 2>&1 && ccbin=clang
    ;;
esac

work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-farol-ilha.XXXXXX")
trap 'rm -rf "$work"' EXIT
"$ccbin" "${flags[@]}" tests/farol_ilha.c src/farolilha.c -Isrc -o "$work/farol-ilha"
"$work/farol-ilha"
echo "farol_ilha: tudo ok"
