#!/bin/bash
# #495: roteamento real do foco do relogio, sem janela, conta ou rede.
set -eu
cd "$(dirname "$0")/.."
tmp=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-relogio-foco.XXXXXX")
trap 'rm -rf "$tmp"' EXIT
python3 - "$tmp" <<'PYTHON'
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

app = Path('src/app.c').read_text()
header = Path('src/app.h').read_text()
# O enum e o estado/roteador sao os do produto, sem copiar a politica no teste.
(out / 'relogio_tela.inc').write_text(re.search(r'typedef enum \{[\s\S]*?\} Tela;', header)[0])
start = app.index('// Foco por controle na ilha:')
end = app.index('void app_evento(', start)
block = app[start:end]
getters = []
modules = ['home','explorar','busca','biblioteca','agendaui','perfil','amigoperfil','addonsui','pluginsui','ajustes','vertudo','detail']
for i, module in enumerate(modules):
    f = funcao('src/' + module + '.c', 'int ' + module + '_pediu_relogio(')
    state = re.search(r'int \w+ = (\w+);', f)[1]
    getters.append('#define ' + state + ' pedidos[' + str(i) + ']\n' + f + '#undef ' + state + '\n')
(out / 'relogio_pedidos.inc').write_text(''.join(getters))
(out / 'relogio_app.inc').write_text(block + funcao('src/app.c', 'static int relogioCamadaAberta(') + funcao('src/app.c', 'static int relogioCabe('))
(out / 'relogio_salvos.inc').write_text(funcao('src/app.c', 'static void alternarSalvos('))
(out / 'relogio_ilha.inc').write_text(''.join(funcao('src/ilha.c', f) for f in (
 'void ilha_relogio_visivel(', 'static void desenharFocoRelogio(',
 'void ilha_relogio_focar(', 'int ilha_relogio_focado(')))
# Garante a integracao no caminho de desenho/roteamento; os corpos acima
# sao executados pelo teste. Nao substitui o link completo do aplicativo.
assert '  desenharFocoRelogio(R, raio, A);' in Path('src/ilha.c').read_text()
assert app.index('ilhacart_validar_identidade();', app.index('void app_evento(')) < app.index('if (relogioFocoEvento(e))', app.index('void app_evento('))
assert 'relogioFocoPedir(relogioPedidoPagina());' in app
assert 'relogioFocoPedir(vertudo_pediu_relogio());' in app
assert 'int pedido = detail_pediu_relogio();' in app
main = Path('src/main.c').read_text()
window = main[main.index('if (e.type == SDL_WINDOWEVENT) {'):]
assert window.index('app_evento(&e);') < window.index('ponteiro_evento(&e, app_evento);')
assert re.search(r'if \(e\.window\.event == SDL_WINDOWEVENT_FOCUS_LOST \|\| e\.window\.event == SDL_WINDOWEVENT_HIDDEN\)\s+app_evento\(&e\);', main)

PYTHON
flags=(-std=c11 -O1 -g -I"$tmp" -Wall -Wextra)
if [ "${TOUCH:-0}" = 1 ]; then flags+=(-DNV_TOUCH_UI); fi
if [ "${SANITIZE:-0}" = 1 ]; then
  flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all)
  if [ "$(uname -s)" = Darwin ]; then
    export ASAN_OPTIONS="detect_leaks=0:halt_on_error=1"
  else
    flags+=(-fno-pie -no-pie)
    export ASAN_OPTIONS="detect_leaks=1:halt_on_error=1"
  fi
  export UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1"
fi
"${CC:-cc}" "${flags[@]}" tests/relogio_foco.c -o "$tmp/teste"
"$tmp/teste"
