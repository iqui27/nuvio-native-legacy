#include "continuar.h"
#include "idioma.h"
#include "layout.h"
#include "text.h"
#include "anim.h"
#include "proximo.h"
#include "trakt.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

void continuar_desenhar(const CatItem *ci, GfxRect r) {
  CatItem copia;
  ProxSugestao prox;
  int idx;
  if (!ci) return;
  // Episodio semeado ja terminado: o card passa a anunciar o PROXIMO, quando a
  // regra portada do web deixa (ver proximo.h). A decisao mora aqui, e nao na
  // home, porque so muda o que este card ESCREVE — nenhuma fileira nova, nenhum
  // poster a mais para decodificar.
  //
  // O teto de PROX_MAX_BUSCAS nao precisa ser aplicado aqui: a fileira ja nasce
  // com 8 itens (trakt_continuar, em descoberta.c), bem abaixo dele.
  idx = cat_indice_por_imdb(ci->imdb);
  if (idx >= 0 &&
      prox_para_item(ci, cat_episodio(idx, 0), cat_n_episodios(idx),
                     (long long)time(NULL) * 1000LL, &prox)) {
    copia = *ci;
    copia.temporada = prox.temporada;
    copia.episodio  = prox.episodio;
    snprintf(copia.nomeEpisodio, sizeof copia.nomeEpisodio, "%s", prox.nome);
    // O selo de "restam N min" e da duracao do episodio ANTERIOR e a barra e do
    // progresso dele; nenhum dos dois descreve um episodio que nao comecou.
    copia.restanteMin = 0;
    copia.progresso = 0;
    ci = &copia;
  }

  {
  float esc = r.w / NV_DESTAQUE_W;
  float pad = NV_CW_PAD * esc, largura = r.w - pad * 2;
  gfx_rect(r, 0, GFX_VEU, 0, 0, 0, NV_RAIO_CARD, 0, 0, 0, .85f);

  // Um retangulo compacto, nao uma pilula. Nunca inventar status de estreia.
  if (ci->restanteMin > 0 || (ci->progresso == 0 && trakt_e_a_seguir(ci->imdb))) {
    char selo[48];
    int h = ci->restanteMin / 60, m = ci->restanteMin % 60;
    // "A SEGUIR" e nao "53min Restantes": o item de progresso 0 e o proximo
    // episodio de uma serie cujo ultimo terminou (issue #66) — ninguem
    // comecou a ve-lo, entao "restantes" seria mentira.
    if (ci->progresso == 0 && trakt_e_a_seguir(ci->imdb))
      snprintf(selo, sizeof selo, "%s", i18n("A seguir"));
    else if (h && m) snprintf(selo, sizeof selo, i18n("%dh %dmin Restantes"), h, m);
    else if (h) snprintf(selo, sizeof selo, i18n("%dh Restantes"), h);
    else snprintf(selo, sizeof selo, i18n("%dmin Restantes"), m);
    float px = NV_CW_BADGE_PAD_X * esc, py = NV_CW_BADGE_PAD_Y * esc;
    TxtLinha l = txt_linha_corta(TXT_CW_BADGE, selo, 242, 243, 247, 255, largura - 2*px);
    if (l.tex) {
      GfxRect b = {r.x + r.w - pad - l.w - 2*px, r.y + pad, l.w + 2*px, l.h + 2*py};
      gfx_cor(b, NV_CW_BADGE_RADIUS * esc / b.h, .055f, .055f, .065f, .84f);
      txt_desenhar_alpha(l, b.x + px, b.y + py, 1);
    }
  }

  float base = r.y + r.h - 30*esc;
  int serie = !strcmp(ci->tipo, "series") && ci->temporada > 0 && ci->episodio > 0;
  if (serie && ci->nomeEpisodio[0]) {
    TxtLinha ep = txt_linha_corta(TXT_CW_META, ci->nomeEpisodio, 230, 232, 238, 255, largura);
    base -= ep.h;
    txt_desenhar_alpha(ep, r.x + pad, base, 1);
    base -= 4*esc;
  }
  TxtLinha titulo = txt_linha_corta(TXT_CW_TITULO, ci->titulo, 247, 248, 250, 255, largura);
  base -= titulo.h;
  txt_desenhar_alpha(titulo, r.x + pad, base, 1);
  if (serie) {
    // O FORMATO PASSA PELA TABELA, porque "T" e "E" sao portugues: numa
    // interface em ingles o card dizia "T1:E1" com o destaque logo acima
    // dizendo "S1 E1" — dois nomes para a mesma coisa na mesma tela, e na foto
    // que abre o post. A chave carrega os dois marcadores de posicao.
    char te[24]; snprintf(te, sizeof te, i18n("T%d:E%d"), ci->temporada, ci->episodio);
    TxtLinha ep = txt_linha(TXT_CW_META, te, 230, 232, 238, 255);
    txt_desenhar_alpha(ep, r.x + pad, base - ep.h - 4*esc, 1);
  }

  // Linha fina, recuada da moldura. A parte vazia nao vira uma faixa cinza.
  if (ci->progresso > 0) {
    float h = NV_CW_BAR_H * esc;
    float preenchido = largura * anim_clamp(ci->progresso / 100.f, 0, 1);
    if (preenchido < h) preenchido = h;
    GfxRect barra = {r.x + pad, r.y + r.h - NV_CW_BAR_BOTTOM*esc - h, preenchido, h};
    gfx_cor(barra, .5f, .96f, .965f, .98f, .98f);
  }
  }
}
