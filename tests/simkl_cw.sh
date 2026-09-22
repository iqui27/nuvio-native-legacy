#!/bin/bash
# Simkl no "Continuar assistindo" e no "+" (issue #110).
#
# Duas metades. A primeira le o CODIGO: o rotulo de cada indice gravado de
# "Fonte do Continuar assistindo" (cwFonteLocal) e de "Onde o + salva"
# (salvosDestino) tem de continuar o mesmo — quem ja tinha "cwFonteLocal 2"
# escolheu Trakt e tem de continuar no Trakt depois de atualizar. Os numeros
# com nome (AJ_CWF_*, AJ_SALVOS_*) sao conferidos por _Static_assert no .c.
#
# A segunda roda src/simkl.c contra uma rede falsa (tests/simkl_cw.c): so
# simkl.c e js.c entram, o resto (token, nuvem, enfeite) e stub. Sem SDL
# linkado, sem rede.
set -eu
cd "$(dirname "$0")/.."

python3 - src/ajustes.c <<'PY'
import re, sys
src = open(sys.argv[1], encoding="utf-8").read()
def vetor(nome):
    m = re.search(r'static const char \*' + nome + r'\[\]\s*=\s*\{(.*?)\};', src, re.S)
    assert m, nome
    return re.findall(r'"((?:[^"\\]|\\.)*)"', m.group(1))
def qtd(nome):
    m = re.search(r'ESC\("[^\n]*?",\s*' + nome + r',\s*(\d+)\)', src)
    assert m, nome
    return int(m.group(1))
cw = vetor("V_CW_FONTE")
sv = vetor("V_SALVOS")
esperado_cw = ["Ambas", "Conta Nuvio", "Trakt", "Simkl"]
esperado_sv = ["Lista do Nuvio", "Watchlist do Trakt", "Plan to Watch do Simkl"]
assert cw == esperado_cw, cw
assert sv == esperado_sv, sv
# A contagem do ESC tem de cobrir o vetor inteiro, senao o valor novo nunca
# e alcancavel pelas setas (e limita() o jogaria fora na leitura do arquivo).
assert qtd("V_CW_FONTE") == len(cw), qtd("V_CW_FONTE")
assert qtd("V_SALVOS") == len(sv), qtd("V_SALVOS")
# As duas chaves continuam locais: nao sobem para a conta, que o app web nao
# conhece "Simkl" aqui.
m = re.search(r'static int somenteDesteAparelho\(int op\) \{(.*?)\n\}', src, re.S)
assert m and "case AJ_CW_FONTE:" in m.group(1) and "case AJ_SALVOS_DEST:" in m.group(1)
# E a fonte do CW nao le literais do web (antes lia os de V_CW, por engano).
m = re.search(r'static const char \*const \*literaisDe\(int op\) \{(.*?)\n\}', src, re.S)
assert m and "case AJ_CW_FONTE:" not in m.group(1)
print("ok  indices antigos de V_CW_FONTE e V_SALVOS com o mesmo rotulo; Simkl no fim; locais")
PY

tmp="$(mktemp -d "${TMPDIR:-/tmp}/nuvio-simkl-cw-XXXXXX")"
trap 'rm -rf "$tmp"' EXIT
cc -O1 -g -Wall -Wextra -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -Wno-deprecated-declarations -Wno-macro-redefined \
  src/simkl.c src/js.c tests/simkl_cw.c -o "$tmp/simkl_cw" -lpthread
"$tmp/simkl_cw"
