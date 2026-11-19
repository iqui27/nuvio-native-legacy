#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
export TMPDIR=/Volumes/ExternalSSD/tmp/jobtmp
dir=$(mktemp -d "$TMPDIR/rw-hero.XXXXXX")
trap 'rm -rf "$dir"' EXIT
python3 - "$dir/test.c" <<'PY'
from pathlib import Path
import sys
s=Path('src/detail.c').read_text(); a=s.index('        const CatItem *ci = cat_item(idx);', s.index('if (acao == ACAO_PRIMARIO) {', s.index('void detail_evento'))); b=s.index('\n      } else if (acao == ACAO_INICIO)',a)
Path(sys.argv[1]).write_text('''#include <assert.h>
#include <stddef.h>
typedef struct { char *imdb; } CatItem;
static CatItem item={"tt1"}; static int idx, dur, pedFontes, pedReproduzir, modal, ativa, alvo;
#define NV_HOLD_MS 500
#define RW_ORIGEM_TITULO 1
const CatItem *cat_item(int i){return &item;}
void extras_progresso_serie(int *v,int *t){*v=*t=2;}
int ehSerie(void){return 1;}
int rewatch_ativa(const char *id){return ativa;}
int episodioAlvo(int *t,int *e,int *p){return alvo;}
void rewatchui_abrir(const CatItem *ci,int o,int t,int e){modal=1;}
void ok(void){
'''+s[a:b]+'''
}
int main(void){ativa=1; alvo=0; ok(); assert(modal && !pedReproduzir);
modal=pedReproduzir=0; alvo=1; ok(); assert(!modal && pedReproduzir);
modal=pedReproduzir=0; ativa=0; ok(); assert(modal && !pedReproduzir);}
''')
PY
cc "$dir/test.c" -o "$dir/test"
"$dir/test"
