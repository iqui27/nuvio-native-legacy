#!/bin/bash
# #788 revisao: prova com ASan nos carregadores reais. Extrai colpinItens,
# desc_colecao_inicial e os parsers de producao de src/descoberta.c
# (producao.inc) e dirige uma fonte Discover filtrada com transporte
# simulado:
#   discover, Ocultar vistos desligado -> 20 itens da primeira pagina;
#   discover, os 24 primeiros vistos   -> a reserva nao vista e consumida:
#     a fileira publicada pula os vistos do prefixo e o 25o inedito aparece
#     (discover pagina de 20: ids 25-40 = 16). Sem escrever fora do buffer
#     (rc 0, sem global-buffer-overflow).
set -eu
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-colpin-vistos-f.XXXXXX")
trap 'rm -rf "$work"' EXIT
awk '
 /^#define (COLPIN_MORTAS|TMDB |VT_VISTOS_PAGINAS_MAX)/ { print }
 /^static (int ehCanal|int colpinItens|const char \*rotuloTipoSing|int deMetaTmdb|int deMetaTrakt|void tmdbFiltros|const char \*tmdbMontarUrl|const char \*colecaoTextoFim|int colecaoArray|char \*colecaoBaixar|int colecaoTeto|int colecaoTipo)\(/ || /^int desc_colecao_inicial\(/ {
     extracting = 1
     printf "#line %d \"src/descoberta.c\"\n", NR
 }
 /Somente ids da pagina sao guardados/ { ids = 1 }
 ids && /^typedef struct/ { extracting = 1 }
 extracting { print }
 extracting && /^}/ { extracting = 0; ids = 0 }
 ' src/descoberta.c > "$work/producao.inc"
cc -g -O1 -fsanitize=address -fno-omit-frame-pointer -Isrc -I"$work" \
  tests/colpin_vistos_filtros.c src/js.c -lm -o "$work/test"
"$work/test" discover 0 20
"$work/test" discover 1 16
echo ok colpin_vistos_filtros
