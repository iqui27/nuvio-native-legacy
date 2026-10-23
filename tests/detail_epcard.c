// OS CARDS DE EPISODIO DA MODERNA (#205), sem janela e sem rede.
//
// Variant C: unchanged 16:9 image, 624 px medium card, title/ratings row,
// metadata row, five synopsis lines, and navigation at all three card sizes.
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

  // 1. BASE DO EPISODIO NA MESMA ALTURA DO WATCHED. O selo Assistido e fixo
  //    (NV_DETP_EP_VISTO_*); o do numero e que desce para as bases cairem
  //    juntas — e nao ao contrario, que e o que o dono pediu.
  assert(NV_DETP_EPM_SELO_Y + NV_DETP_EP_SELO_H ==
         NV_DETP_EP_VISTO_Y + NV_DETP_EP_VISTO_H);

  // All sizes retain their image geometry and reserve the same seven panel rows.
  for (v = 0; v < 3; v++) {
    char linha[48];
    snprintf(linha, sizeof linha, "epTamanhoLocal %d\n", v);
    gravar(linha);
    assert(ajustes_ep_tamanho() == v);
    assert(ajustes_home_layout() == HOME_LAYOUT_MODERNA);
    h[v] = epCardH(); th[v] = epThumbH(); w[v] = epCardW(); p[v] = epCardPasso();
    // As bases continuam casando em qualquer tamanho: nenhum dos dois selos
    // escala com o card.
    assert(NV_DETP_EPM_SELO_Y + NV_DETP_EP_SELO_H ==
           NV_DETP_EP_VISTO_Y + NV_DETP_EP_VISTO_H);
    assert(epSinLinhas() == 5);
    assert(w[v] == 640.0f * epEscala() && th[v] == 360.0f * epEscala());
    assert(h[v] - th[v] == 264.0f);
    assert(NV_DETP_EPM_TIT_DY + 36 <= NV_DETP_EPM_META_DY);
    assert(NV_DETP_EPM_META_DY + 28 <= NV_DETP_EPM_SIN_DY);
    assert(h[v] - th[v] >= NV_DETP_EPM_SIN_DY + 5.0f * NV_DETP_EP_LD_SIN);
    assert(epRowY() == ANC(NV_DETP_EP_Y + 24.0f - 40.0f));
    // A secao mede o card que sera desenhado: o empilhamento acompanha.
    assert(alturaSecao(SEC_EPISODIOS) == h[v]);
    // Colunas lado a lado no passo certo (card + a margem do #205).
    assert(p[v] > w[v]);
    assert(fabsf(p[v] - w[v] - NV_DETP_EP_GAP * epEscala()) < 0.01f);
    // O Medio e o card de sempre mais a margem nova; os passos crescem com o
    // tamanho porque a margem acompanha a escala.
    assert(fabsf(xItem(SEC_EPISODIOS, 3) - xItem(SEC_EPISODIOS, 2) - p[v]) < 0.01f);
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
  assert(h[0] < h[1] && h[1] < h[2]);
  assert(th[0] < th[1] && th[1] < th[2]);
  assert(w[0] < w[1] && w[1] < w[2]);
  assert(p[0] < p[1] && p[1] < p[2]);
  assert(w[1] == 640.0f && th[1] == 360.0f && h[1] == 624.0f);
  assert(p[1] == 640.0f + NV_DETP_EP_GAP);

  // 4. O TITULO UM DEGRAU ABAIXO na Moderna: TXT_CALLOUT (28), e nao o
  //    TXT_PLR_CORPO (32) do Padrao.
  assert(epTituloEstilo() == TXT_CALLOUT);

  // Refined shares Modern episode metrics in every card size.
  for (v = 0; v < 3; v++) {
    char settings[80];
    snprintf(settings, sizeof settings, "homeLayoutLocal 2\nepTamanhoLocal %d\n", v);
    gravar(settings);
    assert(epCardW() == w[v] && epCardH() == h[v]);
    assert(epThumbH() == th[v] && epCardPasso() == p[v]);
    assert(epTituloEstilo() == TXT_CALLOUT && epSinLinhas() == 5);
  }

  // Standard restores the old Apple TV metrics regardless of Modern card size.
  for (v = 0; v < 3; v++) {
    char settings[80];
    snprintf(settings, sizeof settings, "homeLayoutLocal 1\nepTamanhoLocal %d\n", v);
    gravar(settings);
    assert(ajustes_home_layout() == HOME_LAYOUT_PADRAO);
    assert(epCardW() == 420 && epThumbH() == 236 && epCardH() == 464);
    assert(epCardPasso() == 420 + NV_DETP_EP_GAP);
    assert(epRowY() == ANC(NV_DETP_EP_Y + 24.0f));
    assert(epSinLinhas() == 4 && epTituloEstilo() == TXT_DET_META2);
    assert(alturaSecao(SEC_EPISODIOS) == epCardH());
    assert(epCardH() - epThumbH() == 228);
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
  puts("detail_epcard: Modern/Refined metrics and navigation preserved; Standard Apple TV cards restored");
  return 0;
}
