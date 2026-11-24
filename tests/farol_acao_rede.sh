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

# FAROL34C P1-2: semDVForcado/farol_hdr_off derruba o htipo do bind ACB para
# "none" — a sonda compila o selHtipo() real extraido de src/video.c.
python3 - "$work/farol_htipo.inc" <<'PY'
import sys
out = sys.argv[1]
src = open("src/video.c").read()
beg = src.index("static void selHtipo")
end = src.index("\n}", beg) + 2
open(out, "w").write(src[beg:end] + "\n")
PY
grep -q "static void selHtipo" src/video.c
grep -q "selHtipo(htipo" src/video.c
grep -q "montarVideoData(vd, sizeof vd" src/video.c
grep -q "tocarInterno(url, !semDVForcado)" src/video.c

cat > "$work/farol_htipo_probe.c" <<'EOF'
#include <assert.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
static int hdrOff;
int ajustes_farol_hdr_off_ativo(void) { return hdrOff; }
static char hdrTipo[24] = "none";
static char vidHdr[24];
static int vidDV;
static int vuiPrim = 9, vuiTrans = 16, vuiMatriz = 9;   // BT2020/PQ de um HDR10 real
static int semDVForcado;
#include "farol_htipo.inc"
// No prenderPlano, prim/trans/matriz entram em 2,2,2 e selHtipo so os toca nos
// ramos hdrTipo/HDR10 — DolbyVision vai sem SEI/VUI (igual ao Apple TV na C9).
static void t(int sdr, const char *hdr, const char *esp, int prim, int trans, int matriz) {
  char h[24]; int p = 2, tr = 2, m = 2;
  semDVForcado = sdr;
  snprintf(vidHdr, sizeof vidHdr, "%s", hdr);
  selHtipo(h, sizeof h, sdr || ajustes_farol_hdr_off_ativo(), &p, &tr, &m);
  assert(!strcmp(h, esp) && p == prim && tr == trans && m == matriz);
}
int main(void) {
  vidDV = 0;
  // SDR forcado: nem HDR10 nem DolbyVision chegam ao bind, VUI fica SDR.
  t(1, "HDR10", "none", 2, 2, 2);
  t(1, "DolbyVision", "none", 2, 2, 2);
  vidDV = 1;
  t(1, "qualquer", "none", 2, 2, 2);
  // Sem override: o ACB recebe o tipo real do video (DV sem VUI, HDR10 com).
  vidDV = 1; t(0, "DolbyVision", "DolbyVision", 2, 2, 2);
  vidDV = 0; t(0, "HDR10", "HDR10", 9, 16, 9);
  t(0, "SDR", "none", 2, 2, 2);
  // hdrTipo (override de teste /tmp/nuvio-acb) sobrevive sem SDR forcado...
  snprintf(hdrTipo, sizeof hdrTipo, "HDR10");
  t(0, "SDR", "HDR10", 9, 16, 9);
  // ...e nao furta o SDR forcado.
  t(1, "SDR", "none", 2, 2, 2);
  snprintf(hdrTipo, sizeof hdrTipo, "none");
  // farol_hdr_off persistente tambem nao deixa DV passar pro bind.
  hdrOff = 1; vidDV = 1;
  t(0, "DolbyVision", "none", 2, 2, 2);
  hdrOff = 0;
  puts("ok: htipo SDR forcado");
  return 0;
}
EOF
"$ccbin" -g -std=gnu11 -Wall -Werror -fsanitize=address -I "$work" \
  -o "$work/farol_htipo_probe" "$work/farol_htipo_probe.c"
"$work/farol_htipo_probe"

# FAROL34C P1-1: a prova dirigida porta as duas APIs — em 43433cbcb3 ela
# falha no executar() com a sessao retida; na ponta, reabre na fonte certa.
"$ccbin" "${flags[@]}" -DFAROL34C_API tests/farol34c_driver.c \
  src/farolacao_rede.c src/cachearte.c -lm -o "$work/teste-p1"
"$work/teste-p1" "$work/dados"

# rede.c de verdade (libcurl simulada): -Wno-* so do que o rede.c ja carrega.
rflags=(-std=gnu11 -O1 -g -Wall -Wextra -Wno-unused-function -Wno-unused-parameter -Isrc -pthread)
case "${SANITIZE:-0}" in
  1) rflags+=(-fsanitize=address,undefined -fno-omit-frame-pointer) ;;
  thread) rflags+=(-fsanitize=thread) ;;
esac
"$ccbin" "${rflags[@]}" tests/farol_acao_rede_cache.c src/redeurl.c -ldl -lm -o "$work/teste-rede"
"$work/teste-rede"
echo "farol_acao_rede: tudo ok"
