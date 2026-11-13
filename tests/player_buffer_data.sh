#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
tmp=$(mktemp -d);trap 'rm -rf "$tmp"' EXIT
python3 - "$tmp/render.c" <<'PY'
from pathlib import Path
import re,sys
s=Path('src/player.c').read_text();a=s.index('static void bufferDadosDesenhar(');b=s.index('\n}',a)+2
margin=re.search(r'^#define PLR_MARGEM\s+[^\n]+',s,re.M);assert margin
Path(sys.argv[1]).write_text(margin[0]+'\n'+s[a:b])
assert 'plrui_barra(bx, yBarra, bw, frac, bufFrac' in s
PY
cat > "$tmp/test.c" <<'C'
#include <assert.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "layout.h"
#include "text.h"
static int enabled,reads,texts,draws,measures,wraps,line_width=100;
static int color[4];
static const char *sample="Buffer: 128.0 / 128 MiB total (256 MiB selected) · HTTP: 4 (DTS/DV)";
static int ajustes_buffer_dados(void) { return enabled; }
static void video_buffer_test_text(char *s,size_t n) {
 reads++;int count=snprintf(s,n,"%s",sample);assert(count>=0 && (size_t)count<n);
}
TxtLinha txt_linha(TxtEstilo f,const char *s,int r,int g,int b,int a) {
 assert(f==TXT_G19M && !strcmp(s,sample));
 color[0]=r;color[1]=g;color[2]=b;color[3]=a;texts++;
 return (TxtLinha){.w=line_width,.h=20};
}
void txt_desenhar_alpha(TxtLinha l,float x,float y,float a) {
 (void)l;assert(x==900 && y==466 && a==1);draws++;
}
float txt_bloco_dir(TxtEstilo f,const char *s,int r,int g,int b,float right,float y,
                    float width,float leading,float alpha,int max_lines) {
 assert(f==TXT_G19M && !strcmp(s,sample));
 assert(r==color[0] && g==color[1] && b==color[2]);
 assert(right==1000 && width>0 && right-width>=NV_ILHA_MARGEM_D);
 // Long text stays clear of the title's left 60% and keeps every token.
 assert(right-width>=NV_ILHA_MARGEM_D+(right-NV_ILHA_MARGEM_D)*0.6f);
 assert(leading>=20 && max_lines==0);
 if(alpha==0) {assert(y==0);measures++;}
 else {
  assert(fabsf(alpha-color[3]/255.0f)<0.00001f);
  assert(y>=0 && y+48<=500-14);wraps++;
 }
 return 48;
}
#include "render.c"
int main(void) {
 bufferDadosDesenhar(1000,500,1);assert(!reads && !texts && !draws);
 enabled=1;bufferDadosDesenhar(1000,500,1);assert(reads==1 && texts==1 && draws==1);
 assert(!measures && !wraps);
 line_width=700;bufferDadosDesenhar(1000,500,1);
 assert(reads==2 && texts==2 && draws==1 && measures==1 && wraps==1);
 enabled=0;bufferDadosDesenhar(1000,500,1);assert(reads==2 && texts==2 && draws==1 && measures==1 && wraps==1);
 return 0;
}
C
flags=();if [[ ${SANITIZE:-0} == 1 ]];then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer);fi
cc -std=c11 -Wall -Wextra -Werror "${flags[@]}" -Isrc "$tmp/test.c" -lm -o "$tmp/test"
"$tmp/test"
echo 'PASS player buffer data: OFF has no telemetry/text/draw, short line fits, complete selected-size/HTTP label wraps clear of title, progress retained'
