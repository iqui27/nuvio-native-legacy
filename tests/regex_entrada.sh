#!/bin/bash
# O que se digita/cola no campo da regex chega INTEIRO ao C e da a mesma resposta
# do Nuvio oficial? Ver tests/regex_entrada.js. Precisa de node e python3.
#
#   bash tests/regex_entrada.sh
set -eu
cd "$(dirname "$0")/.."
command -v node >/dev/null 2>&1 || { echo "regex_entrada: sem node, pulado"; exit 0; }
D=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-regex-entrada.XXXXXX")
export NUVIO_DADOS="$D"
trap 'rm -rf "$D"' EXIT
# O alfabeto do campo e o teto de tamanho vem DO CODIGO (nao de uma copia aqui).
eval "$(python3 - <<'PY'
import re, shlex
s = open("src/ajustes.c", encoding="utf8").read()
m = re.search(r'FR_ALFA_REGEX =\s*"((?:[^"\\]|\\.)*)";', s)
alfa = bytes(m.group(1), "utf8").decode("unicode_escape")
# O campo pede FR_REGEX_MAX (ajustes.c frRegexAbrir), limitado a TECLADO_LONGO.
h = open("src/fonteregra.h").read()
mx = int(re.search(r'#define\s+FR_REGEX_MAX\s+(\d+)', h).group(1))
t = open("src/teclado.h").read()
mx = min(mx, int(re.search(r'#define\s+TECLADO_LONGO\s+(\d+)', t).group(1)))
print("export NV_ALFA=%s NV_MAX=%s" % (shlex.quote(alfa), mx))
PY
)"
node tests/regex_entrada.js > "$D/casos.tsv"
cc -O1 -g -Wall -Wextra -Isrc src/fonteregra.c src/regexjs.c src/dados.c src/js.c tests/fonteregra_oficial.c \
  -o "$D/entrada" -lpthread 2>/dev/null
"$D/entrada" "$D/casos.tsv" | sed 's/fonteregra_oficial/regex_entrada/'
