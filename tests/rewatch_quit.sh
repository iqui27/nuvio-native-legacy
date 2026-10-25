#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
export TMPDIR=/Volumes/ExternalSSD/tmp/jobtmp
dir=$(mktemp -d "$TMPDIR/rw-quit.XXXXXX")
trap 'rm -rf "$dir"' EXIT
python3 - "$dir/test.c" <<'PY'
from pathlib import Path
import sys
s=Path('src/app.c').read_text(); a=s.index('  if (rewatchui_aberta())'); b=max(s.index('\n',a),s.index('\n',s.index('if (e->type == SDL_QUIT)')))+1
# Include both routing statements regardless of their order.
p=s.rfind('\n',0,a-1)+1
if 'SDL_QUIT' in s[p:a]: a=p
Path(sys.argv[1]).write_text('''#include <assert.h>
#define SDL_QUIT 256
typedef struct { int type; } SDL_Event;
static int sair, modal=1, consumidos;
int rewatchui_aberta(void){return modal;}
void rewatchui_evento(const SDL_Event *e){consumidos++;}
void evento(const SDL_Event *e){
'''+s[a:b]+'''
}
int main(void){SDL_Event e={SDL_QUIT}; evento(&e); assert(sair && !consumidos);}
''')
PY
cc "$dir/test.c" -o "$dir/test"
"$dir/test"
