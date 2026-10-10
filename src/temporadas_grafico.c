// O grafico de temporadas da pagina de serie. Ver temporadas_grafico.h.
#include "temporadas_grafico.h"
#include "catalogo.h"
#include "vistoep.h"
#include "extras.h"
#include "svdesenho.h"
#include "notasui.h"
#include "plrui.h"
#include "gfx.h"
#include "text.h"
#include "layout.h"
#include "ajustes.h"
#include "idioma.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

// --- MEDIDAS (1920x1080) -----------------------------------------------------
// Cabecalho no mesmo TXT_HEADLINE dos "Numeros da temporada", e dois cartoes do
// mesmo material das Notas: o grafico (com a coluna de numeros a esquerda) e,
// quando ha amigos com posicao, o cartao deles a direita.
#define TG_CAB       78.0f
#define TG_CARD_H   300.0f
#define TG_GAP       24.0f
#define TG_RAIO      30.0f
#define TG_PAD       32.0f
#define TG_AMG_W    540.0f
#define TG_STAT_W   300.0f
#define TG_LIN_H     54.0f
#define TG_LIN_AV    42.0f
#define TG_ANIM_MS  750u
#define TG_ANIM_PASSO 45u     // atraso entre colunas vizinhas

// --- CONTA (pura) --------------------------------------------------------------

static int naoExibido(int t, int e, int agT, int agE) {
  if (agT <= 0 || agE <= 0) return 0;
  if (t != agT) return t > agT;
  return e >= agE;
}

static int depois(int t1, int e1, int t2, int e2) {
  return t1 > t2 || (t1 == t2 && e1 > e2);
}

int tgraf_coluna(const TgDados *d, int numero) {
  int i;
  if (!d) return -1;
  for (i = 0; i < d->n; i++) if (d->t[i].numero == numero) return i;
  return -1;
}

int tgraf_montar(TgDados *d, const TgEp *eps, int n, int sabe, int agT, int agE,
                 int especiais, const AmigosTitulo *amg) {
  int i, j;
  char imdb[sizeof d->imdb];
  memcpy(imdb, d->imdb, sizeof imdb);
  memset(d, 0, sizeof *d);
  memcpy(d->imdb, imdb, sizeof imdb);
  d->sabe = sabe;
  for (i = 0; i < n; i++) {
    const TgEp *e = &eps[i];
    TgTemp *t;
    int c;
    if (e->temporada < 0 || e->episodio < 1) continue;
    if (e->temporada == 0 && !especiais) continue;
    c = tgraf_coluna(d, e->temporada);
    if (c < 0) {
      if (d->n >= TG_TEMP_MAX) continue;
      // Insere na ordem (as listas do meta quase sempre ja vem ordenadas, mas
      // nada garante: o Cinemeta lista na ordem que o addon publicou).
      for (c = d->n; c > 0 && d->t[c - 1].numero > e->temporada; c--) d->t[c] = d->t[c - 1];
      memset(&d->t[c], 0, sizeof d->t[c]);
      d->t[c].numero = e->temporada;
      d->n++;
    }
    t = &d->t[c];
    t->total++;
    // Nao exibido GANHA de visto (a mesma ordem de contarTemporada em detail.c):
    // o Trakt aceita marcar o que nem estreou, e contar isso faria a coluna
    // passar de 100%.
    if (naoExibido(e->temporada, e->episodio, agT, agE)) continue;
    t->exibidos++;
    if (sabe && e->visto == 1) {
      t->vistos++;
      if (e->episodio > t->ultVisto) t->ultVisto = e->episodio;
      if (depois(e->temporada, e->episodio, d->meuT, d->meuE)) {
        d->meuT = e->temporada; d->meuE = e->episodio;
      }
    }
  }
  for (i = 0; i < d->n; i++) {
    TgTemp *t = &d->t[i];
    t->completa = t->exibidos > 0 && t->vistos >= t->exibidos;
    d->total += t->total; d->exibidos += t->exibidos; d->vistos += t->vistos;
    d->completas += t->completa;
  }
  if (!amg) return d->n;
  d->totalAmg = amg->total;
  for (i = 0; i < amg->n && d->nAmg < AMT_MAX; i++) {
    const AmigoTit *a = &amg->a[i];
    TgAmigo *g;
    // Sem posicao (filme, ou so reagiu) nao entra: o grafico e de onde cada
    // um esta. Especial so com especiais no grafico.
    if (a->temporada < 0 || a->episodio <= 0) continue;
    if (a->temporada == 0 && !especiais) continue;
    g = &d->amg[d->nAmg++];
    memset(g, 0, sizeof *g);
    snprintf(g->id, sizeof g->id, "%s", a->id);
    snprintf(g->nome, sizeof g->nome, "%s", a->nome);
    snprintf(g->avatar, sizeof g->avatar, "%s", a->avatar);
    g->temporada = a->temporada; g->episodio = a->episodio;
    g->reacao = a->reacao; g->nota = a->nota; g->agora = a->agora;
    // A FRENTE so quando sabemos onde VOCE esta: sem mapa nao ha "sua frente".
    g->frente = sabe && depois(a->temporada, a->episodio, d->meuT, d->meuE);
    g->col = tgraf_coluna(d, a->temporada);
  }
  // Ordem: os da frente primeiro; dentro de cada grupo, o mais adiantado antes.
  // Insercao estavel (AMT_MAX e 8): empate fica na ordem de amigostitulo.
  for (i = 1; i < d->nAmg; i++) {
    TgAmigo x = d->amg[i];
    for (j = i; j > 0; j--) {
      const TgAmigo *p = &d->amg[j - 1];
      int antes = x.frente > p->frente ||
                  (x.frente == p->frente && depois(x.temporada, x.episodio, p->temporada, p->episodio));
      if (!antes) break;
      d->amg[j] = d->amg[j - 1];
    }
    d->amg[j] = x;
  }
  for (i = 0; i < d->nAmg; i++) { if (d->amg[i].frente) d->nFrente++; else d->nAtras++; }
  return d->n;
}

int tgraf_existe(const TgDados *d) {
  return d && d->n > 0 && ((d->sabe && d->vistos > 0) || d->nAmg > 0);
}

void tgraf_frase_amigos(const TgDados *d, char *dst, size_t tam) {
  char n1[64], n2[64];
  dst[0] = 0;
  if (!d || d->nAmg <= 0) return;
  if (d->nFrente > 0) {
    amigostitulo_primeiro_nome(d->amg[0].nome, n1, sizeof n1);
    if (d->nFrente == 1) snprintf(dst, tam, i18n("%s está na sua frente"), n1);
    else if (d->nFrente == 2) {
      amigostitulo_primeiro_nome(d->amg[1].nome, n2, sizeof n2);
      snprintf(dst, tam, i18n("%s e %s estão na sua frente"), n1, n2);
    } else snprintf(dst, tam, i18n("%s e mais %d estão na sua frente"), n1, d->nFrente - 1);
    return;
  }
  // Ninguem na frente: com mapa, voce lidera; sem mapa, so onde eles estao.
  if (!d->sabe) { snprintf(dst, tam, "%s", i18n("Onde seus amigos estão")); return; }
  amigostitulo_primeiro_nome(d->amg[0].nome, n1, sizeof n1);
  if (d->nAtras == 1) snprintf(dst, tam, i18n("Você está na frente de %s"), n1);
  else snprintf(dst, tam, i18n("Você está na frente de %d amigos"), d->nAtras);
}

// --- DADOS DA PAGINA (com revisao) ------------------------------------------

static TgDados dados;
static int     dIdx = -1;
static unsigned dCat, dVisto, dAmg;
static int     dAgT, dAgE, dNEps, dValido;
static TgEp    epsBuf[1024];

const TgDados *tgraf_dados(int idx) {
  const CatItem *ci = cat_item(idx);
  int agT = extras_agenda_temporada(), agE = extras_agenda_episodio();
  int n = cat_n_episodios(idx), i, k = 0, sabe;
  AmigosTitulo at;
  int temAmg, mesmo;
  if (!ci || !ci->imdb[0] || n < 1) {
    memset(&dados, 0, sizeof dados);
    dValido = 0;
    return &dados;
  }
  // dados.imdb guarda o id SEM ":temporada:episodio"; o item vindo de Continuar
  // assistindo ou do Spotlight traz o sufixo. Comparar os dois inteiros nunca
  // batia e o grafico era remontado em toda chamada (dezenas por quadro).
  { size_t nb = strlen(dados.imdb);
    mesmo = nb > 0 && !strncmp(dados.imdb, ci->imdb, nb) && (ci->imdb[nb] == 0 || ci->imdb[nb] == ':'); }
  if (dValido && idx == dIdx && mesmo &&
      dCat == cat_revisao() && dVisto == vistoep_revisao() &&
      dAmg == amigostitulo_revisao() && dAgT == agT && dAgE == agE && dNEps == n)
    return &dados;
  sabe = vistoep_conhecido(ci->imdb);
  for (i = 0; i < n && k < (int)(sizeof epsBuf / sizeof epsBuf[0]); i++) {
    const CatEp *e = cat_episodio(idx, i);
    if (!e) continue;
    epsBuf[k].temporada = (short)e->temporada;
    epsBuf[k].episodio = (short)e->episodio;
    epsBuf[k].visto = (signed char)(sabe ? vistoep_estado(ci->imdb, e->temporada, e->episodio) : -1);
    k++;
  }
  temAmg = amigostitulo_obter(ci->imdb, &at);
  snprintf(dados.imdb, sizeof dados.imdb, "%s", ci->imdb);
  { char *dp = strchr(dados.imdb, ':'); if (dp) *dp = 0; }
  tgraf_montar(&dados, epsBuf, k, sabe, agT, agE, 0, temAmg ? &at : NULL);
  dIdx = idx; dCat = cat_revisao(); dVisto = vistoep_revisao();
  dAmg = amigostitulo_revisao(); dAgT = agT; dAgE = agE; dNEps = n; dValido = 1;
  printf("[vistoep] %s: grafico mapa_rev=%u sabe=%d vistos=%d/%d agenda=T%dE%d",
         dados.imdb, dVisto, sabe, dados.vistos, dados.exibidos, agT, agE);
  for (i = 0; i < dados.n; i++)
    printf(" T%d=%d/%d", dados.t[i].numero, dados.t[i].vistos, dados.t[i].exibidos);
  printf("\n");
  fflush(stdout);
  return &dados;
}

// --- DESENHO -----------------------------------------------------------------

static char animImdb[24];
static Uint32 animIni;

void tgraf_reiniciar(void) { animImdb[0] = 0; animIni = 0; }

float tgraf_altura(void) { return TG_CAB + TG_CARD_H; }

static float suaveSaida(float x) {
  if (x <= 0.0f) return 0.0f;
  if (x >= 1.0f) return 1.0f;
  x = 1.0f - x;
  return 1.0f - x * x * x;
}

// Fracao da animacao de crescer da coluna `c` (0..1).
static float crescer(int c, Uint32 agora) {
  Uint32 atraso = (Uint32)c * TG_ANIM_PASSO;
  if (ajustes_animacoes_reduzidas()) return 1.0f;
  if (!animIni) return 0.0f;
  if (agora - animIni < atraso) return 0.0f;
  return suaveSaida((float)(agora - animIni - atraso) / (float)TG_ANIM_MS);
}

static void textoTemp(int numero, char *dst, size_t tam) {
  snprintf(dst, tam, i18n("T%d"), numero);
}

// A COLUNA DA SERIE a esquerda (dono, 06/10: "as barras nao dao a visao de
// completou a serie"): o numero grande e a FRACAO DA SERIE ("62% da série")
// com a barra de progresso do app (plrui_trilho, a do player e dos Ajustes);
// a 100% vira "Série completa" com o check. Com o foco numa temporada, os
// numeros dela.
static void colunaNumeros(const TgDados *d, float x, float y, float w, int foco, float a) {
  char grande[24], sufixo[48], l1[96], l2[96], l3[96];
  TxtLinha lg, ls1, ls2, ls3;
  float ar, ag, ab, frac = 0.0f;
  int completa = 0;
  ajustes_acento(&ar, &ag, &ab);
  sufixo[0] = l1[0] = l2[0] = l3[0] = 0;
  if (foco >= 0 && foco < d->n) {
    const TgTemp *t = &d->t[foco];
    size_t k;
    if (d->sabe) snprintf(grande, sizeof grande, "%d/%d", t->vistos, t->exibidos);
    else snprintf(grande, sizeof grande, "%d", t->exibidos);
    snprintf(sufixo, sizeof sufixo, "%s", i18n("eps"));
    if (t->numero == 0) snprintf(l1, sizeof l1, "%s", i18n("Especiais"));
    else snprintf(l1, sizeof l1, i18n("Temporada %d"), t->numero);
    if (t->completa) snprintf(l2, sizeof l2, "%s", i18n("Temporada completa"));
    else if (d->sabe) snprintf(l2, sizeof l2, i18n("%d de %d assistidos"), t->vistos, t->exibidos);
    else snprintf(l2, sizeof l2, i18n(t->exibidos == 1 ? "%d episódio" : "%d episódios"), t->exibidos);
    k = strlen(l2);
    // A mesma clausula do resumo acima das pilulas (detail.c).
    if (t->total > t->exibidos)
      snprintf(l2 + k, sizeof l2 - k, i18n(t->total - t->exibidos == 1 ? " · %d ainda não exibido"
                                                                       : " · %d ainda não exibidos"),
               t->total - t->exibidos);
    snprintf(l3, sizeof l3, "%s", i18n("OK abre a temporada"));
    frac = t->exibidos > 0 && d->sabe ? (float)t->vistos / (float)t->exibidos : 0.0f;
    completa = d->sabe && t->completa;
  } else {
    int pct = d->exibidos > 0 ? (int)((d->vistos * 100L + d->exibidos / 2) / d->exibidos) : 0;
    completa = d->sabe && d->exibidos > 0 && d->vistos >= d->exibidos;
    if (completa) snprintf(grande, sizeof grande, "%s", i18n("Série completa"));
    else if (d->sabe) {
      snprintf(grande, sizeof grande, "%d%%", pct);
      snprintf(sufixo, sizeof sufixo, "%s", i18n("da série"));
    } else snprintf(grande, sizeof grande, "%d", d->exibidos);
    if (d->sabe) snprintf(l1, sizeof l1, i18n("%d de %d episódios"), d->vistos, d->exibidos);
    else snprintf(l1, sizeof l1, "%s", i18n("Episódios"));
    snprintf(l2, sizeof l2, i18n("Temporadas completas: %d de %d"), d->sabe ? d->completas : 0, d->n);
    if (completa) snprintf(l3, sizeof l3, "%s", i18n("Você está em dia"));
    else if (d->sabe && d->meuT > 0)
      snprintf(l3, sizeof l3, i18n("Último visto: T%dE%d"), d->meuT, d->meuE);
    frac = d->exibidos > 0 && d->sabe ? (float)d->vistos / (float)d->exibidos : 0.0f;
  }
  { float gx = x;
    // Completa: o check do app (aj_circle-check) no realce, antes do titulo.
    if (completa) {
      gfx_icone((GfxRect){ x, y + 4.0f, 40.0f, 40.0f }, "aj_circle-check", ar, ag, ab, a);
      gx += 52.0f;
    }
    lg = txt_linha_corta(completa && !(foco >= 0) ? TXT_G28B : TXT_V2_NUM, grande,
                         245, 246, 248, 255, w - (gx - x));
    txt_desenhar_alpha(lg, gx, completa && !(foco >= 0) ? y + 9.0f : y, a);
    if (sufixo[0]) {
      TxtLinha lsf = txt_linha(TXT_ILHA_SUB, sufixo, 243, 242, 239, 255);
      txt_desenhar_alpha(lsf, gx + (float)lg.w + 10.0f, y + (float)lg.h - (float)lsf.h - 8.0f, a * 0.6f);
    } }
  y += 66.0f;
  // A barra da serie (ou da temporada focada): a mesma do app.
  plrui_trilho((GfxRect){ x, y, w, 10.0f }, frac, -1.0f, 0, 0, a);
  y += 30.0f;
  ls1 = txt_linha_corta(TXT_ILHA_NOME, l1, 243, 242, 239, 255, w);
  ls2 = txt_linha_corta(TXT_ILHA_SUB, l2, 243, 242, 239, 255, w);
  ls3 = txt_linha_corta(TXT_ILHA_SUB, l3, 243, 242, 239, 255, w);
  txt_desenhar_alpha(ls1, x, y, a); y += (float)ls1.h + 8.0f;
  if (l2[0]) { txt_desenhar_alpha(ls2, x, y, a * 0.72f); y += (float)ls2.h + 6.0f; }
  if (l3[0]) txt_desenhar_alpha(ls3, x, y, a * 0.55f);
}

// AS TEMPORADAS EM LINHAS, na gramatica das listas do app: rotulo "T3", a
// barra de progresso (plrui_trilho, realce), "6/10 eps" a direita e o check
// quando completa. A linha focada leva o fundo de foco das listas
// (plrui_linha_foco). Muitas temporadas: ate 4 colunas de linhas.
#define TG_LIN_TEMP_H 46.0f
static void desenhaGrafico(const TgDados *d, GfxRect card, int foco, int selNumero,
                           float a, Uint32 agora) {
  float ax = card.x + TG_PAD + TG_STAT_W + TG_PAD, aw = card.x + card.w - TG_PAD - ax;
  float ay = card.y + 26.0f, ah = card.h - 52.0f;
  float ar, ag, ab, colW, linH;
  int i, nCol, porCol;
  if (d->n <= 0) return;
  ajustes_acento(&ar, &ag, &ab);
  // Fio separador da coluna da serie.
  gfx_cor((GfxRect){ ax - TG_PAD * 0.5f - 1.0f, card.y + 28.0f, 1.0f, card.h - 56.0f }, 0.0f,
          1.0f, 1.0f, 1.0f, a * 0.08f);
  porCol = (int)(ah / TG_LIN_TEMP_H);
  if (porCol < 1) porCol = 1;
  nCol = (d->n + porCol - 1) / porCol;
  if (nCol > 4) { nCol = 4; porCol = (d->n + 3) / 4; }
  linH = ah / (float)porCol;
  if (linH > TG_LIN_TEMP_H + 8.0f) linH = TG_LIN_TEMP_H + 8.0f;
  colW = (aw - (float)(nCol - 1) * 24.0f) / (float)nCol;
  for (i = 0; i < d->n; i++) {
    const TgTemp *t = &d->t[i];
    int col = i / porCol, lin = i % porCol, focado = (i == foco), escolhida = (t->numero == selNumero);
    float x = ax + (float)col * (colW + 24.0f), y = ay + (float)lin * linH;
    float g = crescer(i, agora), yc = y + linH * 0.5f;
    char rot[16], cont[32];
    TxtLinha lr, lc;
    float tx, tw, fExib, fVisto, ck = t->completa && d->sabe ? 26.0f : 0.0f;
    GfxRect tr;
    if (focado) plrui_linha_foco((GfxRect){ x - 12.0f, y + 2.0f, colW + 24.0f, linH - 4.0f }, 14.0f, a);
    textoTemp(t->numero, rot, sizeof rot);
    lr = txt_linha(TXT_G20B, rot, escolhida && !focado ? (int)(ar * 255) : 243,
                   escolhida && !focado ? (int)(ag * 255) : 242,
                   escolhida && !focado ? (int)(ab * 255) : 239, 255);
    txt_desenhar_alpha(lr, x, yc - (float)lr.h * 0.5f, a * (focado || escolhida ? 1.0f : 0.8f));
    if (d->sabe) snprintf(cont, sizeof cont, i18n("%d/%d eps"), t->vistos, t->exibidos);
    else snprintf(cont, sizeof cont, i18n(t->exibidos == 1 ? "%d episódio" : "%d episódios"), t->exibidos);
    lc = txt_linha(TXT_ILHA_SUB, cont, 243, 242, 239, 255);
    tx = x + 58.0f;
    tw = colW - 58.0f - (float)lc.w - 18.0f - ck;
    if (tw < 40.0f) tw = 40.0f;
    txt_desenhar_alpha(lc, x + colW - ck - (float)lc.w, yc - (float)lc.h * 0.5f,
                       a * (focado ? 1.0f : 0.72f));
    if (ck > 0.0f)
      gfx_icone((GfxRect){ x + colW - 20.0f, yc - 10.0f, 20.0f, 20.0f }, "aj_check", ar, ag, ab, a);
    // A barra: o que foi ao ar no trilho do app; o que nao estreou e um
    // trecho mais apagado no fim (sem preenchimento possivel).
    fExib = t->total > 0 ? (float)t->exibidos / (float)t->total : 0.0f;
    fVisto = t->exibidos > 0 && d->sabe ? (float)t->vistos / (float)t->exibidos : 0.0f;
    tr = (GfxRect){ tx, yc - 4.0f, tw * fExib, 8.0f };
    if (tr.w > 0.5f) plrui_trilho(tr, fVisto * g, -1.0f, 0, 0, a);
    if (fExib < 1.0f) {
      GfxRect fut = { tx + tw * fExib + (fExib > 0.0f ? 4.0f : 0.0f), yc - 4.0f, 0, 8.0f };
      fut.w = tx + tw - fut.x;
      if (fut.w > 2.0f) gfx_cor(fut, 0.5f, 1.0f, 1.0f, 1.0f, a * 0.06f);
    }
  }
}

// OS AMIGOS FICAM FORA DO GRAFICO (dono, 06/10: "so as barras; pessoas/social
// algo separado, menos poluido"): um cartao calmo ao lado, sem aro nem rosto
// apagado — ate tres linhas (rosto, nome, "Na sua frente"/"Atrás de você" e a
// posicao T5E3) e "+N".
static void desenhaAmigos(const TgDados *d, GfxRect card, float a, Uint32 agora) {
  char frase[160];
  float x = card.x + TG_PAD, w = card.w - 2.0f * TG_PAD, y = card.y + 28.0f;
  TxtLinha lf;
  int i, mostra = d->nAmg < 3 ? d->nAmg : 3;
  tgraf_frase_amigos(d, frase, sizeof frase);
  lf = txt_linha_corta(TXT_ILHA_NOME, frase, 243, 242, 239, 255, w);
  txt_desenhar_alpha(lf, x, y, a);
  y += (float)lf.h + 18.0f;
  for (i = 0; i < mostra; i++) {
    const TgAmigo *g = &d->amg[i];
    char ep[24];
    const char *st = !d->sabe ? "" : g->frente ? i18n("Na sua frente") : i18n("Atrás de você");
    TxtLinha ln, le, ls;
    float tx = x + TG_LIN_AV + 16.0f;
    GfxRect av = { x, y + (TG_LIN_H - TG_LIN_AV) * 0.5f, TG_LIN_AV, TG_LIN_AV };
    svd_avatar(av, g->avatar, g->nome, g->id, a);
    if (g->agora) svd_ponto_vivo(av.x + av.w - 8.0f, av.y + av.h - 8.0f, 14.0f, 3.0f, a, agora);
    snprintf(ep, sizeof ep, i18n("T%dE%d"), g->temporada, g->episodio);
    le = txt_linha(TXT_G20B, ep, 243, 242, 239, 255);
    ln = txt_linha_corta(TXT_ILHA_NOME, g->nome, 243, 242, 239, 255, w - TG_LIN_AV - 16.0f - (float)le.w - 20.0f);
    ls = txt_linha_corta(TXT_ILHA_HORA, st, 243, 242, 239, 255, w - TG_LIN_AV - 16.0f);
    { float bloco = (float)ln.h + (st[0] ? 2.0f + (float)ls.h : 0.0f);
      float ty = y + (TG_LIN_H - bloco) * 0.5f;
      txt_desenhar_alpha(ln, tx, ty, a);
      if (st[0]) txt_desenhar_alpha(ls, tx, ty + (float)ln.h + 2.0f, a * 0.55f); }
    txt_desenhar_alpha(le, x + w - (float)le.w, y + (TG_LIN_H - (float)le.h) * 0.5f, a * 0.8f);
    y += TG_LIN_H;
  }
  if (d->nAmg > mostra) {
    char b[16];
    TxtLinha lb;
    snprintf(b, sizeof b, "+%d", d->nAmg - mostra);
    lb = txt_linha(TXT_ILHA_SUB, b, 243, 242, 239, 255);
    txt_desenhar_alpha(lb, x + TG_LIN_AV + 16.0f, y + 6.0f, a * 0.55f);
  }
}

void tgraf_desenhar(const TgDados *d, float x, float y, float w, int foco,
                    int selNumero, float a, Uint32 agora) {
  float alt = tgraf_altura();
  GfxRect cg, ca;
  int temAmg;
  if (!tgraf_existe(d) || a <= 0.001f) return;
  if (y >= NV_TELA_H || y + alt <= 0.0f) return;
  // A animacao comeca quando o bloco aparece pela primeira vez nesta serie.
  if (strcmp(animImdb, d->imdb)) {
    snprintf(animImdb, sizeof animImdb, "%s", d->imdb);
    animIni = agora ? agora : 1u;
  }
  { TxtLinha lt = txt_linha(TXT_HEADLINE, i18n("Seu progresso"), 245, 248, 255, 255);
    txt_desenhar_alpha(lt, x, y, a); }
  temAmg = d->nAmg > 0;
  cg = (GfxRect){ x, y + TG_CAB, temAmg ? w - TG_AMG_W - TG_GAP : w, TG_CARD_H };
  notasui_painel(cg, TG_RAIO, a);
  colunaNumeros(d, cg.x + TG_PAD, cg.y + 30.0f, TG_STAT_W - 8.0f, foco, a);
  desenhaGrafico(d, cg, foco, selNumero, a, agora);
  if (temAmg) {
    ca = (GfxRect){ x + w - TG_AMG_W, y + TG_CAB, TG_AMG_W, TG_CARD_H };
    notasui_painel(ca, TG_RAIO, a);
    desenhaAmigos(d, ca, a, agora);
  }
}
