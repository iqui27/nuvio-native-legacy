#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
tmp=$(mktemp -d);trap 'rm -rf "$tmp"' EXIT
python3 - "$tmp/render.c" <<'PY'
from pathlib import Path
import sys
s=Path('src/player.c').read_text();a=s.index('static void bufferDadosDesenhar(');b=s.index('\n}',a)+2
Path(sys.argv[1]).write_text(s[a:b])
assert 'plrui_barra(bx, yBarra, bw, frac, bufFrac' in s
PY
cat > "$tmp/test.c" <<'C'
#include <assert.h>
#include <stddef.h>
enum {TXT_G19M};
typedef struct {int w,h;} TxtLinha;
static int enabled,reads,texts,draws;
static int ajustes_buffer_dados(void) { return enabled; }
static void video_buffer_test_text(char *s,size_t n) { assert(n>=32);reads++;s[0]='B';s[1]=0; }
static TxtLinha txt_linha(int f,const char *s,int r,int g,int b,int a) {
 (void)f;(void)s;(void)r;(void)g;(void)b;(void)a;texts++;return (TxtLinha){100,20};
}
static void txt_desenhar_alpha(TxtLinha l,float x,float y,float a) {
 (void)l;assert(x==900 && y==466 && a==1);draws++;
}
#include "render.c"
int main(void) {
 bufferDadosDesenhar(1000,500,1);assert(!reads && !texts && !draws);
 enabled=1;bufferDadosDesenhar(1000,500,1);assert(reads==1 && texts==1 && draws==1);
 enabled=0;bufferDadosDesenhar(1000,500,1);assert(reads==1 && texts==1 && draws==1);
 return 0;
}
C
cc -std=c11 -Wall -Wextra -Werror "$tmp/test.c" -o "$tmp/test"
"$tmp/test"
echo 'PASS player buffer data: no telemetry/text/draw while off, opt-in render, buffered progress retained'
