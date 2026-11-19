#!/bin/bash
# #503: semente de recomendacao, enriquecimento e escolha real da arte.
# Sem SDL, GL, rede ou dados da pessoa. SANITIZE=1 habilita ASan/UBSan.
set -eu
cd "$(dirname "$0")/.."
tmp=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-arte-recomendacao.XXXXXX")
trap 'rm -rf "$tmp"' EXIT
python3 - "$tmp" <<'PY'
from pathlib import Path
import re, sys

out = Path(sys.argv[1])
tokens = re.compile(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|//[^\n]*|/\*[\s\S]*?\*/|[{};]')

def funcao(caminho, assinatura):
    texto = Path(caminho).read_text()
    achadas = []
    for inicio in re.finditer(r'(?m)^' + re.escape(assinatura), texto):
        aberta = None
        nivel = 0
        for token in tokens.finditer(texto, inicio.start()):
            valor = token[0]
            if aberta is None:
                if valor == ';':
                    break  # declaracao adiantada; procurar a definicao inteira
                if valor != '{':
                    continue
                aberta = token.start()
            if valor == '{':
                nivel += 1
            elif valor == '}':
                nivel -= 1
                if not nivel:
                    achadas.append(texto[inicio.start():token.end()] + '\n')
                    break
        else:
            raise AssertionError('funcao incompleta: ' + assinatura)
    assert len(achadas) == 1, (caminho, assinatura, len(achadas))
    return achadas[0]

fontes = [
    ('src/metaprov.c', ('void metaprov_duracao(',)),
    ('src/descoberta.c', (
        'const char *desc_genero_pt(',
        'static int ehSvg(',
        'static int urlArteInteira(',
        'static int ehCanal(',
        'static const char *rotuloTipoSing(',
        'static int deMeta(',
        'static void completarRaso(',
        'void desc_pedir_titulo_semente(',
    )),
    ('src/detail.c', (
        'static const char *arteDe(',
        'static const char *arteDeViva(',
        'static int arteDetalheEhPoster(',
    )),
]
(out / 'arte_recomendacao.inc').write_text(''.join(
    funcao(caminho, assinatura)
    for caminho, assinaturas in fontes
    for assinatura in assinaturas))
PY
flags=(-O1 -g -Isrc -I"$tmp" -Wall)
if [ "${SANITIZE:-0}" = 1 ]; then
  flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all)
  # O runtime do macOS nao oferece LeakSanitizer.
  if [ "$(uname -s)" = Darwin ]; then
    export ASAN_OPTIONS="detect_leaks=0:halt_on_error=1"
  else
    flags+=(-fno-pie -no-pie)
    export ASAN_OPTIONS="detect_leaks=1:halt_on_error=1"
  fi
  export UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1"
fi
"${CC:-cc}" "${flags[@]}" tests/arte_recomendacao.c src/artehero.c src/js.c -o "$tmp/teste"
"$tmp/teste"
