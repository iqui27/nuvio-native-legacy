#!/bin/bash
# #384: faixa ASS com mais de 8000 blocos (abertura cheia de karaoke/letreiros):
# o indice cobre a faixa inteira, o corpo fica na janela do playhead quando
# passa do teto, e "completo" so vale com tudo. Ver tests/mkvass_alem_teto.c.
#   bash tests/mkvass_alem_teto.sh
#   SANITIZE=1 bash tests/mkvass_alem_teto.sh     # ASan + UBSan
#   VEZES=20 bash tests/mkvass_alem_teto.sh       # repete (fios de colheita)
set -euo pipefail
cd "$(dirname "$0")/.."
DIR=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-mkvass-alem-teto.XXXXXX")
trap 'rm -rf "$DIR"' EXIT
base=(-Isrc -O1 -g -Wall -Wextra -Wno-deprecated-declarations)
if [ "$(uname -s)" != Darwin ]; then base+=(-pthread); fi
if [ "${SANITIZE:-0}" = 1 ]; then
  base+=(-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all)
fi
# A entrega em janela, que na TV espera 10 s, nos testes espera 300 ms.
flags=("${base[@]}" -DMKVASS_RANGES_POR_SEG=400 -DMKVASS_ENTREGA_JANELA_MS=300L)
FONTES="tests/mkvass_alem_teto.c src/mkvass.c src/legenda.c src/dados.c"
# O assrender e um duble dentro do teste: guarda o documento que iria ao libass.
cc "${flags[@]}" $FONTES -o "$DIR/padrao" -lm
# Teto de corpo pequeno (1 MiB, despejo a partir de 768 KiB): a abertura do
# arquivo de teste tem 2,5 MB, entao o despejo e obrigatorio. A entrega em
# janela, que na TV espera 10 s, aqui espera 300 ms.
cc "${flags[@]}" -DMKVASS_CORPO_MAX=1048576L -DMKVASS_CORPO_ALTO=786432L -DMKVASS_ENTREGA_JANELA_MS=300L \
  $FONTES -o "$DIR/janela" -lm
# RITMO: o teto de Ranges e o de producao (3 por segundo), sem -D. So a pausa
# e o recuo do CDN sao encurtados, como em tests/mkvass.sh ([11d]).
cc "${base[@]}" -DMKVASS_ENTREGA_JANELA_MS=300L -DMKVASS_PAUSA_CDN_INI_MS=150L -DMKVASS_PAUSA_CDN_MAX_MS=600L \
  -DMKVASS_RECUO_INI_MS=20L -DMKVASS_RECUO_MAX_MS=160L $FONTES -o "$DIR/ritmo" -lm
ruim=0
for i in $(seq 1 "${VEZES:-1}"); do
  # A prova de ritmo corre em tempo real (~40 s): so na primeira rodada.
  for prova in padrao janela $([ "$i" = 1 ] && echo ritmo); do
    rm -rf "$DIR/dados"; mkdir "$DIR/dados"
    (cd "$DIR" && NUVIO_DADOS="$DIR/dados" "./$prova" "$prova" > "$DIR/saida.txt" 2>&1) || {
      cat "$DIR/saida.txt"; echo "mkvass_alem_teto: FALHOU (prova $prova, rodada $i)"; ruim=1; continue; }
    grep -E '^(ok|FALHA|estado|playhead|maior|reabertura|arquivo|seek|12 s|freio|  )' "$DIR/saida.txt" || true
  done
  [ "$ruim" = 0 ] || exit 1
done
echo "mkvass_alem_teto: ok"
