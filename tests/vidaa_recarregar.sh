#!/bin/bash
# Stub de recarga do alvo VIDAA (src/video_html5.c): o wasm do VIDAA so linka
# se video_recarregar existir ali — src/video.c inteiro esta sob
# "#ifndef __EMSCRIPTEN__" e src/video_tizen.c sob "&& !defined(NV_VIDAA)",
# entao nenhum dos dois cobre o ramo __EMSCRIPTEN__ && NV_VIDAA.
#
# O stub NEGA a recarga (retorno 0): recarregarFonteVOD em app.c trata 0 como
# indisponibilidade e mostra o toast "Indisponivel agora", em vez de reabrir o
# <video> sem estado de carregamento. Recarregar de verdade (novo elemento,
# loadedmetadata, reaplicar seek e pausa) fica fora deste contrato — e o
# mesmo contrato do stub de src/video_tpk.c.
set -euo pipefail
cd "$(dirname "$0")/.."
TMP=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-vidaa-recarrega.XXXXXX")
trap 'rm -rf "$TMP"' EXIT

src=src/video_html5.c

# A definicao mora DENTRO do ramo VIDAA do arquivo: depois da guarda de abertura
# e antes do #endif que a fecha.
guarda=$(grep -n '#if defined(__EMSCRIPTEN__) && defined(NV_VIDAA)' "$src" | head -1 | cut -d: -f1)
fim=$(awk '/^#endif/{l=NR} END{print l}' "$src")
linha=$(grep -n '^int video_recarregar(void)' "$src" | cut -d: -f1)
[ -n "$guarda" ] && [ -n "$linha" ] || { echo "FAIL: video_recarregar ausente no backend VIDAA"; exit 1; }
[ "$guarda" -lt "$linha" ] && [ "$linha" -lt "$fim" ] || { echo "FAIL: video_recarregar fora do ramo VIDAA"; exit 1; }
[ "$(grep -c 'int video_recarregar(void)' "$src")" = 1 ] || { echo "FAIL: definicao duplicada em video_html5.c"; exit 1; }

# Nenhum outro backend cobre o mesmo alvo: o Tizen wgt exclui o VIDAA e o
# video.c nativo exclui o wasm inteiro.
grep -q 'defined(__EMSCRIPTEN__) && !defined(NV_VIDAA)' src/video_tizen.c
grep -q '#ifndef __EMSCRIPTEN__' src/video.c

# O chamador trata 0 como indisponibilidade (toast), nao como falha muda, e o
# contrato e o mesmo do stub do TPK.
sed -n '/static void recarregarFonteVOD/,/^}/p' src/app.c | grep -q 'Indispon'
grep -q 'video_recarregar' src/video_tpk.c

# O corpo compilado devolve 0: extrai a funcao (uma linha) do fonte real e
# linka num probe.
grep -m1 '^int video_recarregar(void)' "$src" > "$TMP/rec.c"
grep -q 'return 0' "$TMP/rec.c"
cat > "$TMP/probe.c" <<EOF
#include <assert.h>
int video_recarregar(void);
$(cat "$TMP/rec.c")
int main(void) { return video_recarregar() != 0; }
EOF
cc -O1 -Wall -Werror "$TMP/probe.c" -o "$TMP/probe"
"$TMP/probe"
echo "PASS: VIDAA define video_recarregar, que nega a recarga e ativa o toast de indisponivel"
