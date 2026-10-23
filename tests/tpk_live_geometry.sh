#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-live-geometry.XXXXXXXX")
trap 'rm -rf "$work"' EXIT
python3 - "$work/check.c" <<'PY'
from pathlib import Path
import sys
s = Path('src/player.c').read_text()
def function(name):
    start = s.index('static void ' + name + '(')
    brace = s.index('{', start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (s[end] == '{') - (s[end] == '}')
        end += 1
    return s[start:end]
Path(sys.argv[1]).write_text(r'''
#include <assert.h>
#include <stdio.h>
#define NV_TPK 1
#define NV_TELA_W 1920
#define NV_TELA_H 1080
#define PLR_ASP_ORIGINAL 0
#define aspPendente 0
typedef unsigned Uint32;
typedef struct { float x,y,w,h; } PlrRect;
static int comVideo=1,janAtiva,mini,esperandoFonte,aspecto;
static int janCalls,cropCalls,qw=1280,qh=720;
static float janT; static Uint32 janEm;
static PlrRect janDe,janPara,janAgora;
static int ehCanal(void) { return 1; }
static int video_largura(void) { return qw; }
static int video_altura(void) { return qh; }
static int video_recorte_fonte(void) { return 1; }
static int modoPrecisaRecorte(int m) { return 0; }
static PlrRect miniDestino(void) { return (PlrRect){1400,50,480,270}; }
static PlrRect aspectoRect(int m) { return (PlrRect){0,0,1920,1080}; }
static PlrRect aspectoVisivel(int m) { return aspectoRect(m); }
static PlrRect destinoComRecuo(PlrRect r) { return r; }
static void video_janela(int x,int y,int w,int h) {
    janCalls++;
    if (mini) assert(x==1400 && y==50 && w==480 && h==270);
    else assert(x==0 && y==0 && w==1920 && h==1080);
}
static void video_janela_fonte(int x,int y,int w,int h,int dx,int dy,int dw,int dh) {
    cropCalls++; assert(x==0 && y==0 && w==qw && h==qh);
}
'''+function('aplicarAspecto')+'\n'+function('janelaAnimar')+r'''
int main(void) {
    /* Previous channel still has 720p metadata while source lookup runs. */
    esperandoFonte=1; aplicarAspecto();
    assert(janCalls==1 && cropCalls==0);
    esperandoFonte=0; qw=qh=0; aplicarAspecto();
    assert(janCalls==2 && cropCalls==0);
    qw=1920; qh=1080; aplicarAspecto();
    assert(cropCalls==1);
    /* Both directions settle immediately, without a stepped window animation. */
    mini=1; janelaAnimar(aspectoRect(0),miniDestino());
    assert(!janAtiva && janCalls==3);
    mini=0; janelaAnimar(miniDestino(),aspectoRect(0));
    assert(!janAtiva && cropCalls==2);
    puts("tpk live crop/rect decisions: PASS");
}
''')
PY
cc -std=c11 "$work/check.c" -o "$work/check"
"$work/check"
cat > "$work/stubs.c" <<'C'
int ajustes_audio_offset_ms(void) { return 0; }
void dts_remux_set_audio_offset_ms(int ms) { (void)ms; }
int debrid_tb_link(int usenet,int id,int file,char *url,unsigned n) {
  (void)usenet; (void)id; (void)file; (void)url; (void)n; return 0;
}
C
for flag in '' '-DNV_TPK40'; do
  cc -O1 -g -Wall -DNV_TPK $flag -Isrc -I/opt/homebrew/include $(sdl2-config --cflags) \
    "$work/stubs.c" tests/tpk-aspecto-roi.c src/video_tpk.c src/mkvfix.c -DNV_MKVFIX_SEM_REDE \
    src/vazao.c src/velocidade.c src/faixasmkv.c src/audioinfo.c src/audioescolha.c \
    src/capmkv.c src/mkv.c tests/capmkv_stub.c tests/dts_tpk_stub.c \
    -lpthread $(sdl2-config --libs) -o "$work/bridge"
  "$work/bridge"
done
