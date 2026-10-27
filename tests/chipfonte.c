// Native regression: the chip must leave the hero even with pointer targets populated.
#include "../src/detail.c"
#include "dados.h"
#include <assert.h>
#include <stdio.h>

static void key(SDL_Keycode code) {
  SDL_Event e = {0}; e.type = SDL_KEYDOWN; e.key.keysym.sym = code;
  detail_evento(&e);
}
int main(void) {
  SDL_Init(SDL_INIT_TIMER);
  dados_iniciar(getenv("NUVIO_DADOS")); ajustes_iniciar();
  CatItem item = {0};
  snprintf(item.tipo,sizeof item.tipo,"movie");
  snprintf(item.imdb,sizeof item.imdb,"tt-chipfonte");
  item.nElenco=1; item.elenco[0].tmdb=42;
  cat_definir_tudo(&item,1,NULL,0); idx=0;
  Stream s={0}; snprintf(s.url,sizeof s.url,"https://example.invalid/movie.mp4");
  s.altura=2160; s.dolbyVision=1; s.tamanhoBytes=UINT64_C(19756849562);
  stream_definir_alvo(item.imdb); stream_definir_lista(&s,1);
  vt_montar(&vaiTocar,item.imdb,0,0);
  aberto=1; saindo=0; nivel=0; botao=0; focoVersao=0; carro=0;
  focoServidor=-1; focoAmigos=0; servidoresDisponiveis=0;
  navigationCount=2;
  navigation[0].rect=(GfxRect){96,700,200,72}; navigation[0].row=-1; navigation[0].col=0;
  navigation[1].rect=(GfxRect){96,784,450,48}; navigation[1].row=-4; navigation[1].col=0;
  key(SDLK_DOWN); assert(focoVersao && nivel==0);
  key(SDLK_DOWN);
  int down=nivel==1 && !focoVersao;
  printf("%s: DOWN from source chip enters next section\n",down?"PASS":"FAIL");
  char text[320]; vt_chip_texto(&vaiTocar,text,sizeof text);
  assert(strstr(text,"18") && strstr(text,"GB"));
  float r,g,b; vt_estado_cor(FA_OK,&r,&g,&b); assert(g>r && g>b);
  vt_estado_cor(FA_RUIM,&r,&g,&b); assert(r>g && r>b);
  float ar,ag,ab; ajustes_acento(&ar,&ag,&ab);
  vt_estado_cor(FA_CONFERINDO,&r,&g,&b); assert(r==ar && g==ag && b==ab);
  puts("PASS: chip includes declared GB; ready green, checking accent, failed red");
  stream_automatico_excluir(0);
  assert(vt_montar(&vaiTocar,item.imdb,0,0) && vaiTocar.estado==FA_RUIM);
  puts("PASS: exhausted source remains visible as failed");
  return !down;
}
