#!/bin/bash
# Farol — produtores player/LG (issue 7; PLANO.md 2.2, 5). Duas provas:
#  (a) estatica: cada ponto da tabela tem a chamada farol_ev em src/player.c /
#      src/video.c e nenhum argumento e url, titulo, playUrl, c->titulo ou
#      literal com %s — o anel so aceita tokens;
#  (b) harness: o trecho de tratamento de erro do uMS (numeroDe, farolUmsErro)
#      e o leitor do modelo (farolTvModeloLer) sao extraidos de src/video.c e
#      rodam contra respostas JSON reais — errorCode 200, 204, 40403 e
#      "is not running" — conferindo os eventos no anel (tests/farol_produtores_lg.c).
set -eu
cd "$(dirname "$0")/.."

falhou=0
falha() { echo "farol_produtores_lg: FALHA - $1" >&2; falhou=1; }

# (a) cada ponto listado tem a chamada; nenhum argumento proibido.
python3 - <<'PYEOF' || falhou=1
import re, sys
pts = {
    "src/player.c": ["FE_ABRIR", "FE_PRONTO", "FE_ERRO_FONTE",
                     "FE_RECUSA_SINC", "FE_PARADO"],
    "src/video.c":  ["FE_ERRO_NATIVO", "FE_AUDIO_RECUSADO",
                     "FE_PIPELINE_MORREU", "FE_RECONEXAO"],
}
proib = re.compile(r"url|titulo|%s", re.I)
ruim = 0
for arq, eventos in pts.items():
    src = open(arq, encoding="utf-8").read()
    chamadas = re.findall(r"farol_ev\s*\((.*?)\)\s*;", src, re.S)
    if not chamadas:
        print("farol_produtores_lg: nenhum farol_ev em %s" % arq, file=sys.stderr)
        ruim = 1
        continue
    juntos = " ".join(chamadas)
    for ev in eventos:
        if ev not in juntos:
            print("farol_produtores_lg: %s sem chamada %s" % (arq, ev), file=sys.stderr)
            ruim = 1
    for c in chamadas:
        if proib.search(c):
            print("farol_produtores_lg: argumento proibido em farol_ev(%s) de %s"
                  % (c.replace("\n", " "), arq), file=sys.stderr)
            ruim = 1
sys.exit(ruim)
PYEOF

# (b) harness: extrai o trecho de producao e roda contra os JSONs reais.
awk '/^static double numeroDe\(const char \*p, const char \*chave\) \{/,/^}/' \
    src/video.c > tests/farol_ums.inc
awk '/^static void farolUmsErro\(const char \*p, double posicaoS, char \*erroTxt, size_t nErro\) \{/,/^}/' \
    src/video.c >> tests/farol_ums.inc
awk '/^static void farolTvModeloLer\(const char \*json, char \*out, size_t nOut\) \{/,/^}/' \
    src/video.c >> tests/farol_ums.inc
grep -q "farolUmsErro" tests/farol_ums.inc || falha "extracao de video.c vazia"
grep -q "farolTvModeloLer" tests/farol_ums.inc || falha "leitor de modelo nao extraido"
trap 'rm -f tests/farol_ums.inc' EXIT

bin="${TMPDIR:-/tmp}/nuvio-farol-prod-lg-test"
cc -Wall -Wextra -O1 -g -Itests -Isrc \
   tests/farol_produtores_lg.c src/farolanel.c src/farolerro.c src/fontesonda.c \
   -o "$bin"
"$bin"
rm -f "$bin"

# O harness roda uma vez por compilacao; com ASan+UBSan quando houver suporte.
case "${SANITIZE:-1}" in
  1)
    cc -fsanitize=address,undefined -fno-omit-frame-pointer \
       -Wall -Wextra -O1 -g -Itests -Isrc \
       tests/farol_produtores_lg.c src/farolanel.c src/farolerro.c src/fontesonda.c \
       -o "$bin"
    "$bin"
    rm -f "$bin"
    ;;
esac

[ "$falhou" = 0 ] || exit 1
echo "farol_produtores_lg: tudo ok"
