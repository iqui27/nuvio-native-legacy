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
#include <SDL2/SDL_ttf.h>
#include <math.h>
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
#define CU_PW 184.0f
#define CU_PGAP 16.0f
#define CU_LISTA_W 452.0f
#define NV_TELA_W 1920.0f
#define TXT_G21B 0
#define BT_PAD 28.0f
#define BT_ICONE 22.0f
#define BT_VAO 12.0f
static TTF_Font *font;
static int person=1, count=11;
static float rail;
static const char *i18n(const char *s) { return s; }
static int txt_largura(int style,const char *s) {
  int w,h; (void)style; assert(TTF_SizeUTF8(font,s,&w,&h)==0); return w;
}
static float ajustes_rail_largura_fixa(void) { return rail; }
static int caca_n(void) { return 3; }
static int caca_info(int i, CacaInfo *c) { (void)i; memset(c,0,sizeof *c); c->tipo=person ? CACA_DIRECAO : CACA_TRILHA; return 1; }
static int caca_filme(int i,int j,CacaFilme *f) { (void)i;(void)j;(void)f;return 0; }
static int caca_alternar(int i,int b) { (void)i;(void)b;return 0; }
static void caca_alternar_escopo(int i,int b) { (void)i;(void)b; }
static void caca_parar(int i) { (void)i; }
static int nPost(void) { return count; }
static float colunasPost(void);
static int nPills(void) { return 2; }
static int nSug(int *i) { (void)i;return 0; }
static void eventoTrilhas(SDL_Keycode k,int o,int v) { (void)k;(void)o;(void)v; }
'''
# Header declares the real API; doubles use those same public signatures.
code = code.replace('static int caca_', 'int caca_').replace('static void caca_', 'void caca_')
code += part(hunt, 'enum { Z_SUG', 'static float colunasPost(void);')
code += part(Path('src/plrui.c').read_text(), 'float plrui_botao_largura(', 'static float botao(')
code += part(hunt, 'static int ehPessoa(', 'void cacaui_abrir(')
code += part(hunt, '// Share the rendered wrapping geometry', 'void cacaui_atualizar(')
code += part(hunt, 'static float xEsq(', 'static void texto(')
code += r'''
static void key(int k) { SDL_Event e={0};e.type=SDL_KEYDOWN;e.key.keysym.sym=k;cacaui_evento(&e); }
int main(void) {
  assert(TTF_Init()==0);
  font=TTF_OpenFont("deploy/app/fonts/Montserrat-Bold.ttf",21);assert(font);
  /* Real font widths, production button wrapping and poster geometry. */
  for(person=0;person<=1;person++) for(int layout=0;layout<3;layout++) {
    rail=layout*100.0f;
    int cols=(int)colunasPost(), ids[CU_BOT_MAX], nb=botoes(ids);
    float xs[CU_BOT_MAX]; int rows[CU_BOT_MAX];acaoPosicoes(xs,rows);
    for(count=1;count<=11;count++) for(int page=0;page*cols<count;page++) {
      int visible=count-page*cols; if(visible>cols)visible=cols;
      for(int col=0;col<visible;col++) {
        zona=Z_POST;pLin=pLin0=page;pCol=col;
        int nearest=0;float x=col*(CU_PW+CU_PGAP)+CU_PW*.5f;
        for(int j=1;j<nb;j++) if(rows[j]==0 && fabsf(xs[j]-x)<fabsf(xs[nearest]-x))nearest=j;
        key(SDLK_DOWN);assert(zona==Z_BOT && bot==nearest);
      }
      for(int b=0;b<nb;b++) {
        zona=Z_BOT;bot=b;pLin=pLin0=page;pCol=visible-1;
        int nearest=0;
        for(int col=1;col<visible;col++)
          if(fabsf(col*(CU_PW+CU_PGAP)+CU_PW*.5f-xs[b])<
             fabsf(nearest*(CU_PW+CU_PGAP)+CU_PW*.5f-xs[b]))nearest=col;
        key(SDLK_UP);
        if(rows[b]==0)assert(zona==Z_POST && pCol==nearest && pLin==page);
        else {
          int target=-1;
          for(int j=0;j<nb;j++)if(rows[j]==rows[b]-1 &&
            (target<0 || fabsf(xs[j]-xs[b])<fabsf(xs[target]-xs[b])))target=j;
          assert(zona==Z_BOT && bot==target);
        }
        zona=Z_BOT;bot=b;key(SDLK_DOWN);
        int target=-1;
        for(int j=0;j<nb;j++)if(rows[j]==rows[b]+1 &&
          (target<0 || fabsf(xs[j]-xs[b])<fabsf(xs[target]-xs[b])))target=j;
        assert(zona==Z_BOT && bot==(target<0 ? b : target));
        zona=Z_BOT;bot=b;key(SDLK_RIGHT);
        assert(zona==Z_BOT && bot==((b+1<nb && rows[b+1]==rows[b]) ? b+1 : b));
        zona=Z_BOT;bot=b;key(SDLK_LEFT);
        if(b>0 && rows[b-1]==rows[b])assert(zona==Z_BOT && bot==b-1);
        else assert(zona==Z_LISTA);
      }
      for(int col=0;col<visible;col++) {
        zona=Z_POST;pLin=pLin0=page;pCol=col;key(SDLK_RIGHT);
        int index=page*cols+col, expected=index+1<count ? index+1 : index;
        assert(zona==Z_POST && pLin*cols+pCol==expected);
        zona=Z_POST;pLin=pLin0=page;pCol=col;key(SDLK_LEFT);
        if(index)assert(zona==Z_POST && pLin*cols+pCol==index-1);
        else assert(zona==Z_LISTA);
      }
    }
  }
  person=1;count=11;rail=0;pCol=pLin=pLin0=bot=sug=sel=0;
  zona=Z_SUG;key(SDLK_RIGHT);assert(sug==1);key(SDLK_DOWN);assert(zona==Z_LISTA);
  key(SDLK_RIGHT);assert(zona==Z_POST);
  int cols=(int)colunasPost();
  for(int i=0;i<cols-1;i++){key(SDLK_RIGHT);assert(pCol==i+1 && pLin==0);}
  key(SDLK_RIGHT);assert(pLin==1 && pCol==0);
  key(SDLK_DOWN);assert(zona==Z_BOT && bot==0);
  key(SDLK_UP);assert(zona==Z_POST && pCol==0 && pLin==1);
  for(int i=0;i<11;i++)key(SDLK_LEFT);assert(zona==Z_LISTA);
  key(SDLK_UP);assert(zona==Z_SUG);key(SDLK_ESCAPE);assert(sair);
  TTF_CloseFont(font);TTF_Quit();
  puts("PASS: production Hunt handler and wrapping, nearest vertical targets, every horizontal hop");
}
'''
with tempfile.TemporaryDirectory() as d:
    src = Path(d,'test.c');src.write_text(code)
    subprocess.run(['cc','-O1','-Isrc','-I/opt/homebrew/include',str(src),'-L/opt/homebrew/lib','-lSDL2','-lSDL2_ttf','-o',d+'/test'],check=True)
    subprocess.run([d+'/test'],check=True)
# Trail's public regression exercises its real handler and return path.
subprocess.run(['bash','tests/trilhas.sh'],check=True)
