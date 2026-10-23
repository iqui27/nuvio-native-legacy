// Episode spacing and focus animation without a window or network.
// Verify gaps at all sizes, smooth focus, reduced-motion snapping and stable
// neighbors. The outline renderer is checked by detail_epinfo.py.
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
  int v, i;

  nvt_set(dir, sizeof dir, "/nuvio-detail-epslista-XXXXXX");
  assert(mkdtemp(dir));
  assert(setenv("NUVIO_DADOS", dir, 1) == 0);
  dados_iniciar(dir);
  snprintf(caminho, sizeof caminho, "%s/ajustes.txt", dir);

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

  // 1. A MARGEM E O TOKEN, nos TRES TAMANHOS: o passo mede card + NV_DETP_EP_
  //    GAP (na escala do tamanho) e cada par de colunas vizinhas repete
  //    exatamente esse vao.
  for (v = 0; v < 3; v++) {
    char linha[48];
    float gap;
    snprintf(linha, sizeof linha, "epTamanhoLocal %d\n", v);
    gravar(linha);
    gap = epCardPasso() - epCardW();
    assert(gap > 0.0f && "o card nao pode encostar no vizinho");
    assert(fabsf(gap - NV_DETP_EP_GAP * epEscala()) < 0.01f);
    for (i = 0; i < 3; i++)
      assert(fabsf(xItem(SEC_EPISODIOS, i + 1) - xItem(SEC_EPISODIOS, i) -
                   epCardPasso()) < 0.01f);
  }
  // Refined uses the same card size setting as Modern.
  gravar("homeLayoutLocal 2\nepTamanhoLocal 1\n");
  assert(epApple());
  assert(fabsf((epCardPasso() - epCardW()) - NV_DETP_EP_GAP) < 0.01f);
  gravar("");

  // Focus traverses intermediate spring values before settling.
  aberto = 1; saindo = 0; nivel = 1;
  foco.fileira = SEC_EPISODIOS; foco.coluna = 1;
  for (i = 0; i < N_SECOES; i++) memset(animFoco[i], 0, sizeof animFoco[i]);
  detail_atualizar(1.0f / 60.0f, 1000);
  { float f1 = animFoco[SEC_EPISODIOS][1];
    assert(f1 > 0.0f && f1 < 1.0f && "a mola atravessa valores intermediarios");
    assert(animFoco[SEC_EPISODIOS][0] == 0.0f && "unfocused neighbor stays at zero");
    for (i = 0; i < 120; i++) detail_atualizar(1.0f / 60.0f, 1000 + (i + 1) * 16);
    assert(animFoco[SEC_EPISODIOS][1] > 0.99f); }

  // Reduced motion snaps focus directly to its target.
  anim_politica_reduzida = 1;
  foco.coluna = 2;
  detail_atualizar(1.0f / 60.0f, 5000);
  assert(animFoco[SEC_EPISODIOS][2] == 1.0f);
  assert(animFoco[SEC_EPISODIOS][1] == 0.0f);
  anim_politica_reduzida = 0;

  // 4. FOCAR NAO MOVE OS VIZINHOS: as posicoes das colunas sao as mesmas com
  //    o foco em 0, em 2 ou fora da fileira.
  { float x0 = xItem(SEC_EPISODIOS, 0), x1 = xItem(SEC_EPISODIOS, 1),
          x2 = xItem(SEC_EPISODIOS, 2);
    foco.coluna = 0;
    assert(xItem(SEC_EPISODIOS, 0) == x0 && xItem(SEC_EPISODIOS, 1) == x1 &&
           xItem(SEC_EPISODIOS, 2) == x2);
    foco.fileira = SEC_TEMPORADAS;
    assert(xItem(SEC_EPISODIOS, 0) == x0 && xItem(SEC_EPISODIOS, 1) == x1); }

  // #556: especiais sao acessiveis, mas nao substituem o inicio regular.
  memset(eps, 0, sizeof eps);
  eps[0].temporada = 0; eps[0].episodio = 1;
  eps[1].temporada = 0; eps[1].episodio = 2;
  eps[2].temporada = 1; eps[2].episodio = 1;
  c.temporadas[0] = 1; c.temporadas[1] = 0; c.nTemporadas = 2;
  cat_definir_tudo(&c, 1, NULL, 0); cat_definir_episodios(0, eps, 3);
  assert(primeiroEpRegular()->temporada == 1);
  temporada = 1; // aba da temporada zero, nao T1
  assert(temporadaEm(temporada) == 0 && epVisiveis() == 2);
  foco.fileira = SEC_EPISODIOS; foco.coluna = 1;
  { int t, e; assert(episodioAlvo(&t, &e, NULL) && t == 0 && e == 2); }
  c.temporada = 0; c.episodio = 2; c.progresso = 40;
  cat_definir_tudo(&c, 1, NULL, 0); cat_definir_episodios(0, eps, 3);
  foco.fileira = 0;
  { int t, e, origem; assert(episodioAlvo(&t, &e, &origem) && t == 0 && e == 2 && origem == 2); }
  cat_definir_episodios(0, eps, 2);
  assert(primeiroEpRegular()->temporada == 0);

  puts("detail_epslista: card gaps, focus spring, reduced-motion snapping, stable neighbors");
  return 0;
}
