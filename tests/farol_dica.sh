#!/bin/bash
# Cartao de dica do Farol (P2 — lote sense, issue 48).
# Parte 1: nucleo puro (3 dias com relogio injetado, cota 1/dia/perfil,
#          avisodisp persistido, todos os 6 acao.tipo + guarda de rota #49/#50,
#          serie fora do catalogo no Diario, concorrencia sob TSan).
# Parte 2: integracao real com src/home.c + src/homeassistiu.c (#840):
#          arranque sem queimar cota, virada diaria na mesma cena, serie fora
#          do catalogo, coluna 0, foco, acao: nada, OK, Voltar longo, reduzido.
# Parte 3: despacho real de src/app.c (abrirTitulo + home_pediu_abrir +
#          home_pediu_tocar) para serie/filme fora do catalogo sem arte.
#
#   TMPDIR=... bash tests/farol_dica.sh                          (normal)
#   TMPDIR=... SANITIZE=1 bash tests/farol_dica.sh               (ASan+UBSan)
#   TMPDIR=... CC=clang SANITIZE=thread bash tests/farol_dica.sh (TSan)
set -eu
if [ -z "${TMPDIR:-}" ]; then
  echo "farol_dica: defina TMPDIR fora de /tmp" >&2
  exit 1
fi
TMPDIR="${TMPDIR%/}"
cd "$(dirname "$0")/.."

CC_BIN="${CC:-cc}"
SAN="${SANITIZE:-0}"
flags=()
if [ "$SAN" = 1 ]; then
  flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer)
elif [ "$SAN" = thread ]; then
  flags+=(-fsanitize=thread -fno-omit-frame-pointer)
fi

work="$TMPDIR/nuvio-farol-dica-work-$$"
mkdir -p "$work"
bin1="$work/core"
bin2="$work/home"
bin3="$work/app"
trap 'rm -rf "$work"' EXIT

# Parte 1: nucleo + concorrencia (roda em normal, ASan+UBSan e TSan)
"$CC_BIN" ${flags[@]+"${flags[@]}"} -DNV_FAROLDICA_PURO -DNV_MAPA_PURO \
  tests/farol_dica.c \
  src/faroldica.c src/farolcena.c src/farolpeca.c src/faroltexto.c \
  src/farolrec.c src/farolgosto.c src/farolficha.c src/farolinfo.c \
  src/farolacha.c src/avisodisp.c src/catalogo.c src/progresso.c \
  src/salvos.c src/diario.c src/diarionomes.c src/dados.c src/js.c src/mapa.c \
  -Isrc -O1 -g -pthread -lm \
  -Wall -Wno-deprecated-declarations -Wno-unused-function \
  -o "$bin1"
NUVIO_DADOS="$work/d1" "$bin1"

# Partes 2 e 3: integracao real com src/home.c + src/homeassistiu.c + despacho
# real de src/app.c (normal e ASan+UBSan; TSan pula porque home.c usa SDL/estado
# de fio unico — a concorrencia foi coberta na Parte 1).
if [ "$SAN" != thread ]; then
  if [ "$(uname -s)" = Darwin ]; then
    gc_flags=(-Wl,-dead_strip)
    sdl_inc=(-I/opt/homebrew/include -I/opt/homebrew/include/SDL2)
  else
    gc_flags=(-Wl,--gc-sections)
    sdl_inc=(-I/usr/include/SDL2)
  fi
  home_srcs=(
    tests/amigosfil_stub.c
    src/faroldica.c src/farolcena.c src/farolpeca.c src/faroltexto.c
    src/farolrec.c src/farolgosto.c src/farolficha.c src/farolinfo.c
    src/farolacha.c src/avisodisp.c src/catalogo.c src/progresso.c
    src/salvos.c src/diario.c src/diarionomes.c src/dados.c src/js.c src/mapa.c
    src/homeassistiu.c
    src/focus.c src/posterprov.c src/redeurl.c
    src/colecoes.c src/catordem.c src/fileiras.c src/homefil.c
    src/artehero.c src/cwordem.c src/cwretido.c src/curadoria.c
    src/buscanorm.c
  )

  "$CC_BIN" ${flags[@]+"${flags[@]}"} -DNV_MAPA_PURO -DCURADORIA_TESTE \
    -ffunction-sections -fdata-sections "${gc_flags[@]}" \
    tests/farol_dica_home.c "${home_srcs[@]}" \
    -Isrc "${sdl_inc[@]}" -O1 -g -pthread -lm \
    -Wall -Wno-deprecated-declarations -Wno-unused-function \
    -o "$bin2"
  NUVIO_DADOS="$work/d2" "$bin2"

  # Extrai abrirTitulo e os ramos home_pediu_abrir / home_pediu_tocar reais de src/app.c
  python3 - "$work/farol_dica_app_gen.c" "$work/home_real.inc" <<'PY'
from pathlib import Path
import sys

app = Path('src/app.c').read_text()
a_ini = app.index('static void abrirTitulo(const HomeItem *it, int exigeArte, int descartarRetido) {')
a_fim = app.index('\nstatic void abrirPorIndice(int i) {', a_ini)
abrir_fn = app[a_ini:a_fim]

d_ini = app.index('    if (tela == TELA_HOME && home_pediu_abrir()) {')
d_fim = app.index('    } else if (tela == TELA_BUSCA && busca_pediu_pessoa(&pedPessoa)) {', d_ini)
dispatch_blk = app[d_ini:d_fim] + '    }\n'

Path(sys.argv[2]).write_text(f'#include "{Path("src/home.c").resolve()}"\n')
src = Path('tests/farol_dica_app.c').read_text()
src = src.replace('/* APP_ABRIR_TITULO */', abrir_fn)
src = src.replace('/* APP_HOME_DISPATCH */', dispatch_blk)
Path(sys.argv[1]).write_text(src)
PY

  "$CC_BIN" ${flags[@]+"${flags[@]}"} -DNV_MAPA_PURO -DCURADORIA_TESTE \
    -ffunction-sections -fdata-sections "${gc_flags[@]}" \
    "$work/farol_dica_app_gen.c" "${home_srcs[@]}" \
    -I"$work" -Isrc "${sdl_inc[@]}" -O1 -g -pthread -lm \
    -Wall -Wno-deprecated-declarations -Wno-unused-function \
    -o "$bin3"
  NUVIO_DADOS="$work/d3" "$bin3"
fi

echo "farol_dica: tudo ok"
