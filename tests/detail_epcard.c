// OS CARDS DE EPISODIO DA MODERNA (#205), sem janela e sem rede.
//
// Approved fixed 420x530 cards, 236 px thumbnails and navigation across
// every Home layout and legacy size preference.
//
// A escolha do tamanho e gravada como a do dono: um ajustes.txt de verdade
// em uma pasta temporaria, lido por ajustes_dir — o caminho da TV.
#include "nvtmp.h"
#include "dados.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../src/detail.c"

static char dir[NVT_MAX], caminho[700];
static void gravar(const char *txt) {
  FILE *f = fopen(caminho, "w");
  assert(f);
  fputs(txt, f);
  fclose(f);
  ajustes_dir(dir);
}

int main(void) {
  CatItem c;
  CatEp eps[6];
  SDL_Event e = {0};
  float h[3], th[3], w[3], p[3];
  int v, i;

  nvt_set(dir, sizeof dir, "/nuvio-detail-epcard-XXXXXX");
  assert(mkdtemp(dir));
  assert(setenv("NUVIO_DADOS", dir, 1) == 0);
  dados_iniciar(dir);
  snprintf(caminho, sizeof caminho, "%s/ajustes.txt", dir);

  // Uma serie de ensaio com 6 episodios na T1: a fileira existe de verdade.
  memset(&c, 0, sizeof c);
  snprintf(c.tipo, sizeof c.tipo, "series");
  snprintf(c.titulo, sizeof c.titulo, "Série de Ensaio");
  snprintf(c.imdb, sizeof c.imdb, "tt0000205");
  c.temporadas[0] = 1; c.nTemporadas = 1;
  cat_definir_tudo(&c, 1, NULL, 0); idx = 0;
  memset(eps, 0, sizeof eps);
  for (i = 0; i < 6; i++) {
    eps[i].temporada = 1;
    eps[i].episodio = i + 1;
    snprintf(eps[i].nome, sizeof eps[i].nome, "Episódio %d", i + 1);
  }
  cat_definir_episodios(0, eps, 6);

  gravar("");
  assert(ajustes_home_layout() == HOME_LAYOUT_MODERNA && ajustes_ep_tamanho() == 1);

  // Legacy size preferences must not change the approved Detail geometry.
  for (v = 0; v < 3; v++) {
    char linha[48];
    snprintf(linha, sizeof linha, "epTamanhoLocal %d\n", v);
    gravar(linha);
    assert(ajustes_ep_tamanho() == v);
    assert(ajustes_home_layout() == HOME_LAYOUT_MODERNA);
    h[v] = epCardH(); th[v] = epThumbH(); w[v] = epCardW(); p[v] = epCardPasso();
    assert(w[v] == 420 && th[v] == 236 && h[v] == 530 && p[v] == 448);
    assert(alturaSecao(SEC_EPISODIOS) == h[v]);
    assert(p[v] - w[v] == 28);
    assert(fabsf(xItem(SEC_EPISODIOS, 3) - xItem(SEC_EPISODIOS, 2) - p[v]) < .01f);
    // O foco anda pela fileira inteira neste tamanho. Um punhado de quadros
    // de atualizacao primeiro: e ali que a pagina reconta as colunas de cada
    // fileira (foco.nColunas), que e o que o focus_mover consulta.
    aberto = 1; saindo = 0; nivel = 1;
    foco.fileira = SEC_EPISODIOS;
    foco.coluna = 0;
    for (i = 0; i < 30; i++) detail_atualizar(1.0f / 60.0f, 1000 + i * 16);
    assert(secaoN(SEC_EPISODIOS) == 6 && foco.nColunas[SEC_EPISODIOS] == 6);
    e.type = SDL_KEYDOWN;
    for (i = 0; i < 5; i++) {
      e.key.keysym.sym = SDLK_RIGHT;
      detail_evento(&e);
      assert(foco.fileira == SEC_EPISODIOS && foco.coluna == i + 1);
    }
    e.key.keysym.sym = SDLK_RIGHT;   // na ponta: para
    detail_evento(&e);
    assert(foco.coluna == 5);
    e.key.keysym.sym = SDLK_LEFT;
    detail_evento(&e);
    assert(foco.coluna == 4);
  }
  assert(h[0] == h[1] && h[1] == h[2]);
  assert(w[0] == w[1] && w[1] == w[2]);
  // Dynamic retains the Apple TV cards from before 60043132c.
  for (v = 0; v < 3; v++) {
    char settings[80];
    snprintf(settings, sizeof settings, "homeLayoutLocal 2\nepTamanhoLocal %d\n", v);
    gravar(settings);
    assert(epApple() && !epModerno());
    assert(epCardW() == 420 && epCardH() == 414);
    assert(epThumbH() == 236 && epCardPasso() == 452);
    assert(epCardH() - epThumbH() == 178);
    assert(secaoN(SEC_ABAS_INFO) == (nAbasInfo() > 1 ? nAbasInfo() : 0));
    nivel = 1; foco.fileira = SEC_EPISODIOS;
    assert(secaoAlvo(SEC_TEMPORADAS) == .42f);
    aberto=1; saindo=0; foco.coluna=2;
    for (i=0;i<120;i++) detail_atualizar(1.f/60,4000+i*16);
    assert(fabsf(scrollSec[SEC_EPISODIOS]-2*452)<1);
    assert(fabsf(conteudoSec[SEC_EPISODIOS]-1286)<.01f);
    { DET_INI();
      assert(fabsf(scrollY-fmaxf(0,fminf(docFim-NV_TELA_H,
                                       1286-NV_TELA_H*NV_DETP_ALVO_FILEIRA)))<1);
      TXT_FIM(); }
    e.type=SDL_KEYDOWN; e.key.keysym.sym=SDLK_UP; detail_evento(&e);
    assert(foco.fileira==SEC_TEMPORADAS && foco.coluna==colTemp(temporada));
    e.key.keysym.sym=SDLK_DOWN; detail_evento(&e);
    assert(foco.fileira==SEC_EPISODIOS && foco.coluna==2);
  }

  // Standard retains its original landscape cards.
  for (v = 0; v < 3; v++) {
    char settings[80];
    snprintf(settings, sizeof settings, "homeLayoutLocal 1\nepTamanhoLocal %d\n", v);
    gravar(settings);
    assert(ajustes_home_layout() == HOME_LAYOUT_PADRAO);
    assert(!epApple() && !epModerno());
    assert(epCardW() == NV_DETP_EP_W && epThumbH() == NV_DETP_EP_THUMB_H);
    assert(epCardH() == NV_DETP_EP_H && epCardPasso() == NV_DETP_EP_PASSO);
    assert(epRowY() == conteudoSec[SEC_EPISODIOS]);
    assert(alturaSecao(SEC_EPISODIOS) == epCardH());
    assert(epCardH() == epThumbH());
    aberto = 1; saindo = 0; nivel = 1;
    foco.fileira = SEC_EPISODIOS; foco.coluna = 0;
    for (i = 0; i < 30; i++) detail_atualizar(1.0f / 60.0f, 2000 + i * 16);
    for (i = 1; i < 6; i++) {
      e.key.keysym.sym = SDLK_RIGHT; detail_evento(&e);
      assert(foco.fileira == SEC_EPISODIOS && foco.coluna == i);
      assert(fabsf(xItem(SEC_EPISODIOS, i) - xItem(SEC_EPISODIOS, i - 1)
                   - epCardPasso()) < .01f);
    }
    e.key.keysym.sym = SDLK_DOWN; detail_evento(&e);
    assert(foco.fileira != SEC_EPISODIOS);
    e.key.keysym.sym = SDLK_UP; detail_evento(&e);
    assert(foco.fileira == SEC_EPISODIOS);
  }
  puts("detail_epcard: fixed approved metrics and navigation across Home layouts");
  return 0;
}
