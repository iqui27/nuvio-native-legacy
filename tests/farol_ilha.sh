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

# FAROL 20B: o Voltar do aviso dispensavel so pode agir com a pilula visivel
# DE FATO — fora da ilha_coberta e atras das camadas que se desenham depois
# dela. A dispensa vive fora de ilha_evento (ilha_evento_dispensar) e o
# app_evento chama depois de todas elas.
grep -q 'ilha_evento_dispensar' src/ilha.h src/app.c || { echo "farol_ilha: ilha_evento_dispensar sem chamada em app.c" >&2; exit 1; }
if ! grep -q '^int ilha_evento_dispensar(const SDL_Event' src/ilha.c; then
  echo "farol_ilha: ilha_evento_dispensar nao definido em ilha.c" >&2; exit 1
fi
# a dispensa so pode ser armada na funcao nova (nada dela no ilha_evento
# que roda antes das camadas)
fdisp=$(grep -n '^int ilha_evento_dispensar' src/ilha.c | cut -d: -f1)
if grep -n 'avDispensou = 1' src/ilha.c | awk -F: -v f="$fdisp" '$1 < f' | grep -q .; then
  echo "farol_ilha: a dispensa do Voltar ficou antes de ilha_evento_dispensar" >&2; exit 1
fi
# Em app_evento a dispensa e o ultimo recurso: DEPOIS do player (que cobre a
# ilha), do srvsetup e dos dois blocos do spainel — e antes do roteamento por
# tela (menu/ctx), onde o Voltar tem dono.
linhaDe() { grep -n "$2" "$1" | head -1 | cut -d: -f1; }
d=$(linhaDe src/app.c 'ilha_evento_dispensar(e)')
p=$(linhaDe src/app.c 'player_visivel().*player_evento')
s=$(linhaDe src/app.c 'if (spainel_aberto()) {$')
m=$(linhaDe src/app.c 'if (menu_aberto())  *{ menu_evento')
if [ -z "$d" ] || [ -z "$p" ] || [ -z "$s" ] || [ -z "$m" ] || \
   ! { [ "$p" -lt "$d" ] && [ "$s" -lt "$d" ] && [ "$d" -lt "$m" ]; }; then
  echo "farol_ilha: ilha_evento_dispensar fora da ordem em app_evento (p=$p s=$s d=$d m=$m)" >&2; exit 1
fi

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

# O cenario do FAROL 20B de ponta a ponta, sem janela nem GL: ilha.c e
# ilhasinais.c de producao com os desenhos em stub e o relogio injetado.
"$ccbin" "${flags[@]}" -std=c11 -D_POSIX_C_SOURCE=200809L \
  -Wno-unused-parameter -Wno-format-truncation \
  tests/farol_ilha_voltar.c src/ilha.c src/ilhasinais.c src/farolilha.c src/farolregra.c \
  -Isrc -I. -lm -o "$work/farol-ilha-voltar"
"$work/farol-ilha-voltar"
echo "farol_ilha: tudo ok"
