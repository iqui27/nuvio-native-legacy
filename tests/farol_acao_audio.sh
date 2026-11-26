#!/bin/bash
# Farol #16: acoes de audio e memoria (CONVERTER_AUDIO com Desfazer, TROCAR_FAIXA,
# ALIVIAR_REABRIR). Ver tests/farol_acao_audio.c.
#
#   bash tests/farol_acao_audio.sh              # ASan + UBSan (padrao)
#   SANITIZE=0 bash tests/farol_acao_audio.sh   # normal
set -eu
cd "$(dirname "$0")/.."

if grep -n "_Thread_local" src/farolacao_audio.c src/farolacao_audio.h; then
  echo "farol_acao_audio: _Thread_local proibido" >&2; exit 1
fi
if grep -n "NV_DTS_FFMPEG" src/*.h; then
  echo "farol_acao_audio: NV_DTS_FFMPEG nao pode aparecer em headers" >&2; exit 1
fi
if grep -n "SDL2/SDL" src/farolacao_audio.h; then
  echo "farol_acao_audio: a decisao pura nao pode puxar SDL" >&2; exit 1
fi
# O executor so conversa pelas ops: nada de rede e nada do canal ao vivo.
if grep -nE "urlCanalRota|retentarCanal|watchdog" src/farolacao_audio.c src/farolacao_audio.h; then
  echo "farol_acao_audio: o modulo nao toca o canal ao vivo" >&2; exit 1
fi

flags=(-Wall -Wextra -O1 -g)
ccbin="${CC:-cc}"
case "${SANITIZE:-1}" in
  1) flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer) ;;
esac

work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-farol-acao-audio.XXXXXX")
trap 'rm -rf "$work"' EXIT

"$ccbin" "${flags[@]}" -I/opt/homebrew/include \
  tests/farol_acao_audio.c src/farolacao_audio.c src/memlog.c src/farolanel.c \
  src/farolregra.c src/resolverregra.c src/farolerro.c src/fontesonda.c \
  -Isrc -lm -lpthread -o "$work/farol-acao-audio"
"$work/farol-acao-audio"

# ajustes_dts_definir REAL (extraido de src/ajustes.c, nao reescrito): no
# .tpk o intervalo valido e 0..2 (0 desligado, 1 5.1, 2 Estereo AAC) e na LG
# 0..3 — fd0270f902 descartava o 2 (maximo = 1), FAROL16B.
gen="$work/farol_acao_audio_dts_gen.c"
GEN_OUT="$gen" python3 - <<'PYEOF'
import os, re
aj = open("src/ajustes.c", encoding="utf-8").read()
def hit(rx):
    m = re.search(rx, aj, re.M | re.S)
    if not m: raise SystemExit("farol_acao_audio: extracao falhou: " + rx)
    return m.group(0)
# A declaracao do intervalo por plataforma (V_DTS_SAIDA + N_DTS_SAIDA).
ndts = hit(r"#ifdef NV_TPK\nstatic const char \*V_DTS_SAIDA.*?#endif")
val = hit(r"int ajustes_dts_valor\(void\) \{.*?\}")
dfn = hit(r"void ajustes_dts_definir\(int v\) \{.*?^\}")
open(os.environ["GEN_OUT"], "w", encoding="utf-8").write(
    "// Gerado por tests/farol_acao_audio.sh: trechos reais de src/ajustes.c.\n"
    "#include <assert.h>\n"
    "#include <string.h>\n"
    "#define AJ_N 64\n"
    "#define AJLOG_CENTRAL 1\n"
    "static int ajOrigem;\n"
    "static int gravar(void) { return 1; }\n"
    "#define AJ_COM_ORIGEM(o, chamada) do { ajOrigem = (o); chamada; ajOrigem = 0; } while (0)\n"
    "static int valor[AJ_N];\n"
    "#define AJ_DTS_AC3 0\n"
    + ndts + "\n"
    + val + "\n"
    + dfn + "\n"
    "\nint main(void) {\n"
    "  int i;\n"
    "  (void)V_DTS_SAIDA;\n"
    "  for (i = 0; i <= N_DTS_SAIDA - 1; i++) {\n"
    "    valor[AJ_DTS_AC3] = -1;\n"
    "    ajustes_dts_definir(i);\n"
    "    assert(ajustes_dts_valor() == i);   // todo valor do seletor grava\n"
    "    assert(ajOrigem == 0);\n"
    "  }\n"
    "  // Fora do intervalo: nao grava (nem a regravacao do mesmo valor conta).\n"
    "  valor[AJ_DTS_AC3] = N_DTS_SAIDA - 1;\n"
    "  ajustes_dts_definir(N_DTS_SAIDA);\n"
    "  assert(ajustes_dts_valor() == N_DTS_SAIDA - 1);\n"
    "  ajustes_dts_definir(-1);\n"
    "  assert(ajustes_dts_valor() == N_DTS_SAIDA - 1);\n"
    "  return 0;\n"
    "}\n")
PYEOF

"$ccbin" "${flags[@]}" -DNV_TPK -Isrc "$gen" -o "$work/dts-tpk"
"$work/dts-tpk"
"$ccbin" "${flags[@]}" -Isrc "$gen" -o "$work/dts-webos"
"$work/dts-webos"

echo "farol_acao_audio: tudo ok"
