#include "continuar.h"
#include "ajustes.h"
#include "idioma.h"
#include "layout.h"
#include "recomenda.h"
#include "badges.h"
#include "text.h"
#include "anim.h"
#include "revela.h"
#include "proximo.h"
#include "rewatch.h"
#include "trakt.h"
#include "simkl.h"
#include "cwordem.h"
#include "descoberta.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

// A lista de episodios do card terminado. O catalogo so recebe episodios
// quando o titulo e ABERTO (app.c), entao o card do Continuar — uma copia sem
// lista — ficava para sempre no episodio que acabou. So o card TERMINADO pede
// (costuma ser um so), e no maximo uma vez a cada 30 s por titulo: sem meta a
// resposta volta vazia, e pedir a cada quadro so disputaria o fio com o
// detalhe.
//
// UM RELOGIO POR TITULO. Era um so ("ultimo"): com DOIS cards terminados na
// fileira eles se revezavam no "ultimo", o limite de 30 s nunca valia e os
// dois pediam de novo a cada resposta — 47 pedidos em ~7 min no registro
// 25007 (webOS 1.7.4, tt39304754 e tt33044444 alternando).
#define CW_PEDIDOS 8
static void pedirEpisodios(int idx, const char *imdb) {
  static struct { char imdb[64]; time_t quando; } ped[CW_PEDIDOS];
  time_t agora = time(NULL);
  int i, vaga = 0;
  desc_episodios_pendente();
  if (desc_episodios_carregando(idx)) return;
  for (i = 0; i < CW_PEDIDOS; i++) {
    if (!strcmp(ped[i].imdb, imdb)) {
      if (agora - ped[i].quando < 30) return;
      vaga = i;
      break;
    }
    if (ped[i].quando < ped[vaga].quando) vaga = i;   // o mais antigo cede a vaga
  }
  snprintf(ped[vaga].imdb, sizeof ped[vaga].imdb, "%s", imdb);
  ped[vaga].quando = agora;
  printf("[cw] %s terminado sem lista de episodios: pedindo para achar o proximo\n", imdb);
  desc_episodios(idx, 0);
}

void continuar_desenhar(const CatItem *ci, GfxRect r, float raio) {
  CatItem copia;
  ProxSugestao prox;
  int idx;
  if (!ci) return;
  // Episodio ja terminado (ajuste Percentual assistido): o card passa a
  // anunciar o PROXIMO (prox_seguinte, proximo.h). A decisao mora aqui, e nao
  // na home, porque so muda o que este card ESCREVE — nenhuma fileira nova,
  // nenhum poster a mais para decodificar.
  //
  // A copia COM episodios, quando ha (cat_indice_titulo): a do card costuma
  // nao ter lista, e a do detalhe aberto antes tem.
  idx = cat_indice_titulo(ci->imdb, cat_indice_por_imdb(ci->imdb));
  if (!ci->rewatch && idx >= 0 && !strcmp(ci->tipo, "series") && ci->temporada > 0 &&
      ci->episodio > 0 && ci->progresso >= ajustes_cw_concluido()) {
    if (cat_n_episodios(idx) <= 0) pedirEpisodios(idx, ci->imdb);
    else if (prox_seguinte(ci, cat_episodio(idx, 0), cat_n_episodios(idx),
                           ajustes_cw_concluido(),
                           (long long)time(NULL) * 1000LL, &prox)) {
      copia = *ci;
      copia.temporada = prox.temporada;
      copia.episodio  = prox.episodio;
      snprintf(copia.nomeEpisodio, sizeof copia.nomeEpisodio, "%s", prox.nome);
      // O selo de "restam N min" e da duracao do episodio ANTERIOR e a barra e
      // do progresso dele; nenhum dos dois descreve um episodio que nao comecou.
      copia.restanteMin = 0;
      copia.progresso = 0;
      ci = &copia;
    }
  }

  {
  float esc = r.w / NV_DESTAQUE_W;
  float pad = NV_CW_PAD * esc, largura = r.w - pad * 2;
  // Veu so na base (gfx.h, gfx_veu_base): o nome e o episodio ficam embaixo;
  // o selo de cima tem o proprio fundo.
  //
  // O RAIO E O DA ARTE (#144). Era NV_RAIO_CARD fixo (~13 px neste cartao),
  // enquanto a arte usa o raio de Ajustes: com raio maior os cantos do veu
  // passavam por fora da curva da arte e apareciam como cantos escuros e
  // "cortados" na base do cartao.
  gfx_veu_base(r, raio, NV_CW_VEU_F, NV_CW_VEU_A);

  // Um retangulo compacto, nao uma pilula. Nunca inventar status de estreia.
  if (ci->rewatch && rewatch_ativa(ci->imdb)) {
    badge_desenhar(r.x+pad,r.y+pad,"Reassistindo",BADGE_REALCE,1);
  }
  if (!ci->rewatch && (ci->restanteMin > 0 || (ci->progresso == 0 && (trakt_e_a_seguir(ci->imdb) || simkl_e_a_seguir(ci->imdb) || cwo_conta_a_seguir(ci->imdb))))) {
    char selo[48];
    int h = ci->restanteMin / 60, m = ci->restanteMin % 60;
    // "A SEGUIR" e nao "53min Restantes": o item de progresso 0 e o proximo
    // episodio de uma serie cujo ultimo terminou (issue #66) — ninguem
    // comecou a ve-lo, entao "restantes" seria mentira.
    // O "a seguir" que AINDA NAO FOI AO AR (issue #127) diz quando estreia:
    // "A seguir" nele prometia um episodio que nao ha como tocar. Quem e
    // futuro e a decisao unica da montagem (cwo_e_futuro), a mesma que o poe
    // em "Proximos episodios".
    char quando[32];
    if (ci->progresso == 0 && cwo_e_futuro(ci->imdb) &&
        cwo_data_curta(cwo_estreia(ci->imdb), (long long)time(NULL) * 1000LL,
                       ajustes_idioma(), 0, quando, sizeof quando))
      snprintf(selo, sizeof selo, i18n("Estreia %s"), quando);
    else if (ci->progresso == 0 && (trakt_e_a_seguir(ci->imdb) || simkl_e_a_seguir(ci->imdb) || cwo_conta_a_seguir(ci->imdb)))
      snprintf(selo, sizeof selo, "%s", i18n("A seguir"));
    else if (h && m) snprintf(selo, sizeof selo, i18n("%dh %dmin Restantes"), h, m);
    else if (h) snprintf(selo, sizeof selo, i18n("%dh Restantes"), h);
    else snprintf(selo, sizeof selo, i18n("%dmin Restantes"), m);
    float bw = badge_largura(selo);
    if (bw <= largura)
      badge_desenhar(r.x + r.w - pad - bw, r.y + pad, selo,
                     BADGE_NEUTRO, 1.0f);
  }

  // O selo do IMDb, no CANTO INFERIOR DIREITO (issue #87): o card ja tinha a
  // nota no CatItem — quem nao a lia era o enfeite do Trakt — e a fileira e o
  // unico lugar da home onde ela nao aparecia. O desenho e o mesmo selo da
  // aba Salvos (rec_selo_imdb), pela base da linha do titulo e com o mesmo
  // `pad` lateral do badge de "restam". A largura dele e medida ANTES do
  // texto e sai da pista das linhas, que encurtam em vez de passar por baixo.
  float seloLarg = 0.0f;
  if (ci->nota > 0 && ajustes_notas_home())
    seloLarg = badge_imdb_largura(ci->nota);
  // Quando o selo existe as linhas de texto perdem seloLarg + uma folga de
  // 16*esc — e o preco ja sai na largura, porque rec_selo_imdb ancora pela
  // esquerda e so devolve a medida depois de desenhar.
  float livre = largura - (seloLarg > 0 ? seloLarg + 16.0f * esc : 0.0f);
  if (livre < 40.0f) livre = 40.0f;   // texto sempre ganha um minimo de pista

  float base = r.y + r.h - 30*esc;
  int serie = !strcmp(ci->tipo, "series") && ci->temporada > 0 && ci->episodio > 0;
  if (serie && ci->nomeEpisodio[0]) {
    TxtLinha ep = txt_linha_corta(TXT_CW_META, ci->nomeEpisodio, 230, 232, 238, 255, livre);
    base -= ep.h;
    txt_desenhar_alpha(ep, r.x + pad, base, 1);
    base -= 4*esc;
  }
  if (seloLarg > 0)
    badge_imdb(r.x + r.w - pad - seloLarg, base - BADGE_H, ci->nota, 0, 1.0f);
  TxtLinha titulo = txt_linha_corta(TXT_CW_TITULO, ci->titulo, 247, 248, 250, 255, livre);
  base -= titulo.h;
  txt_desenhar_alpha(titulo, r.x + pad, base, 1);
  if (serie) {
    // Sem sigla "T1:E2": quem ve o card nao sabe o que T e E querem dizer.
    // Sao duas chaves prontas da tabela ("Temporada %d", "Episodio %d")
    // compostas aqui — em ingles sai "Season 1 · Episode 2".
    char tmpT[24], tmpE[24], te[64];
    snprintf(tmpT, sizeof tmpT, i18n("Temporada %d"), ci->temporada);
    snprintf(tmpE, sizeof tmpE, i18n("Episódio %d"), ci->episodio);
    if (ci->rewatch) snprintf(te,sizeof te,i18n("Reassistindo T%dE%d"),ci->temporada,ci->episodio);
    else snprintf(te, sizeof te, "%s · %s", tmpT, tmpE);
    TxtLinha ep = txt_linha(TXT_CW_META, te, 230, 232, 238, 255);
    txt_desenhar_alpha(ep, r.x + pad, base - ep.h - 4*esc, 1);
  }

  // Linha fina, recuada da moldura. A parte vazia nao vira uma faixa cinza.
  // A barra ANDA do valor que mostrava ate o novo (revela.h): na volta do
  // player o card cresce ate onde a pessoa parou, em vez de saltar.
  // O zero tambem e registrado: o "a seguir" que comeca a ser visto cresce
  // do nada ate o valor novo.
  float prog = revela_progresso(ci->imdb, ci->temporada, ci->episodio,
                                (float)ci->progresso, SDL_GetTicks());
  if (ci->progresso > 0) {
    float h = NV_CW_BAR_H * esc;
    float preenchido = largura * anim_clamp(prog / 100.f, 0, 1);
    if (preenchido < h) preenchido = h;
    GfxRect barra = {r.x + pad, r.y + r.h - NV_CW_BAR_BOTTOM*esc - h, preenchido, h};
    gfx_cor(barra, .5f, .96f, .965f, .98f, .98f);
  }
  }
}
