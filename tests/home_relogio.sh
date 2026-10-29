#!/bin/bash
# #495: complete actual Home/Add-ons event handlers and real spatial focus.
set -eu
cd "$(dirname "$0")/.."
tmp=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-home-relogio.XXXXXX")
trap 'rm -rf "$tmp"' EXIT
python3 - "$tmp" <<'PY'
from pathlib import Path
import re,sys
out=Path(sys.argv[1])
tokens=re.compile(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|//[^\n]*|/\*[\s\S]*?\*/|[{};]')
def function(path,signature):
    source=Path(path).read_text(); found=[]
    for start in re.finditer(r'(?m)^'+re.escape(signature),source):
        level=0; opened=False
        for token in tokens.finditer(source,start.start()):
            value=token[0]
            if not opened:
                if value==';': break
                if value!='{': continue
                opened=True
            if value=='{': level+=1
            elif value=='}':
                level-=1
                if not level:
                    found.append(source[start.start():token.end()]+'\n'); break
    assert len(found)==1,(path,signature,len(found))
    return found[0]
home=Path('src/home.c').read_text(); header=Path('src/home.h').read_text()
enum=re.search(r'typedef enum \{[\s\S]*?\} TipoFileira;',header)[0]
row=re.search(r'typedef struct \{[\s\S]*?\} Fileira;',home)[0]
(out/'home_relogio_tipos.inc').write_text(enum+'\n'+row+'\n')
(out/'home_relogio_evento.inc').write_text(''.join(function('src/home.c',name) for name in (
    'static int fileiraItemIndice(', 'static int fileiraEhCanal(',
    'static int primeiraFileiraNavegavel(', 'static int focoVertical(', 'void home_evento(')))
(out/'home_relogio_addons.inc').write_text(function('src/addonsui.c','void addonsui_evento('))
PY
flags=(-std=c11 -O1 -g -Isrc -I"$tmp" -Wall -Wextra)
if [ "${TOUCH:-0}" = 1 ]; then flags+=(-DNV_TOUCH_UI); fi
if [ "${SANITIZE:-0}" = 1 ]; then
  flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all)
  if [ "$(uname -s)" = Darwin ]; then export ASAN_OPTIONS="detect_leaks=0:halt_on_error=1";
  else flags+=(-fno-pie -no-pie); export ASAN_OPTIONS="detect_leaks=1:halt_on_error=1"; fi
  export UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1"
fi
"${CC:-cc}" "${flags[@]}" tests/home_relogio.c src/focus.c -lm -o "$tmp/teste"
"$tmp/teste"
