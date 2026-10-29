"""Compile and drive the production key handlers without GL or network."""
from pathlib import Path
import subprocess
import tempfile
import sys

baseline = '--baseline' in sys.argv
trail = Path('src/trilhas.c').read_text()
hunt = (subprocess.check_output(['git','show','HEAD:src/cacaui.c'],text=True)
        if baseline else Path('src/cacaui.c').read_text())
assert 'arte_metahub_preencher' in trail, 'FAIL: uncatalogued trail steps never request a poster'

def part(s, start, end):
    return s[s.index(start):s.index(end, s.index(start))]

code = r'''
#include <SDL2/SDL.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "caca.h"
#include "artemetahub.h"
#define NV_SCANCODE_BACK 999
#define VISIVEIS 8
#define CU_PLIN 1
#define CU_SUG_MAX 3
#define CU_LIN_VIS 7
#define CU_BOT_MAX 6
#define CU_PAD 32.0f
static int caca_n(void) { return 3; }
static int caca_info(int i, CacaInfo *c) { (void)i; memset(c,0,sizeof *c); return 1; }
static int caca_filme(int i,int j,CacaFilme *f) { (void)i;(void)j;(void)f;return 0; }
static int caca_alternar(int i,int b) { (void)i;(void)b;return 0; }
static void caca_alternar_escopo(int i,int b) { (void)i;(void)b; }
static void caca_parar(int i) { (void)i; }
static int nPost(void) { return 11; }
static float colunasPost(void) { return 5; }
static int nPills(void) { return 2; }
static int nSug(int *i) { (void)i;return 0; }
static void eventoTrilhas(SDL_Keycode k,int o,int v) { (void)k;(void)o;(void)v; }
static int botoes(int *ids) { for(int i=0;i<6;i++)ids[i]=i;return 6; }
static void acaoPosicoes(float *xs,int *rows) { for(int i=0;i<6;i++){xs[i]=(i%2)*260+100; rows[i]=i/2;} }
'''
# Header declares the real API; doubles use those same public signatures.
code = code.replace('static int caca_', 'int caca_').replace('static void caca_', 'void caca_')
code += part(hunt, 'enum { Z_SUG', 'static float colunasPost(void);')
code += part(hunt, 'void cacaui_evento(', 'void cacaui_atualizar(')
code += r'''
static void key(int k) { SDL_Event e={0};e.type=SDL_KEYDOWN;e.key.keysym.sym=k;cacaui_evento(&e); }
int main(void) {
  zona=Z_SUG;key(SDLK_RIGHT);assert(sug==1);key(SDLK_DOWN);assert(zona==Z_LISTA);
  key(SDLK_RIGHT);assert(zona==Z_POST);
  for(int i=0;i<4;i++)key(SDLK_RIGHT);assert(pCol==4);
  key(SDLK_RIGHT);assert(pLin==1 && pCol==0);
  for(int i=0;i<5;i++)key(SDLK_RIGHT);assert(pLin==2 && pCol==0);
  key(SDLK_DOWN);assert(zona==Z_BOT && bot==0);
  key(SDLK_RIGHT);assert(bot==1);key(SDLK_DOWN);assert(bot==3);
  key(SDLK_DOWN);assert(bot==5);key(SDLK_LEFT);assert(bot==4);
  key(SDLK_UP);assert(bot==2);key(SDLK_UP);assert(bot==0);
  key(SDLK_UP);assert(zona==Z_POST);
  for(int i=0;i<11;i++)key(SDLK_LEFT);assert(zona==Z_LISTA);
  key(SDLK_UP);assert(zona==Z_SUG);key(SDLK_ESCAPE);assert(sair);
  puts("PASS: production Hunt handler, horizontal poster paging, wrapped actions, every zone");
}
'''
with tempfile.TemporaryDirectory() as d:
    src = Path(d,'test.c');src.write_text(code)
    subprocess.run(['cc','-O1','-Isrc','-I/opt/homebrew/include',str(src),'-o',d+'/test'],check=True)
    subprocess.run([d+'/test'],check=True)
# Trail's public regression exercises its real handler and return path.
subprocess.run(['bash','tests/trilhas.sh'],check=True)
