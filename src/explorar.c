// Explorar: o ceu das historias da pessoa.
//
// CONCEITO. Cada coisa que ela viu e uma estrela (disco com o cartaz); as
// linhas ligam o que as historias tem em comum; entre duas estrelas nasce uma
// historia nova que CRUZA as duas ("porque voce viu X e Y"), e a mais forte
// fica no centro como a proxima. Quem atravessa duas estrelas (diretor, ator)
// vira um fio proprio, com o proximo titulo dessa pessoa. Os temas que se
// repetem aparecem como nebulosas de texto; uma linha do tempo embaixo mostra
// quantas decadas o gosto atravessa; e um dado de "sorte guiada" sorteia entre
// o resto dos candidatos, pelo mesmo gosto.
//
// OS DADOS vem de mapa.c (fio proprio + cache). Esta tela so copia o retrato
// quando a revisao muda, e desenha.
//
// REMOTO: as setas andam pelo ceu por DIRECAO (o no mais proximo naquele
// sentido), OK abre a pagina do titulo (ou gira o dado), Voltar sai. Esquerda
// sem nada a esquerda devolve a barra lateral, como nas outras telas.
//
// CUSTO, medido pelo contador de gfx: um ceu procedural de tela cheia (que ja
// e o fundo, nao uma camada a mais), linhas partidas em pedacos de 120 px para
// o envelope do GFX_LINHA nao virar area cheia, e nenhuma alocacao por quadro.
#include "explorar.h"
#include "mapa.h"
#include "catalogo.h"
#include "descoberta.h"
#include "tex_cache.h"
#include "gfx.h"
#include "text.h"
#include "anim.h"
#include "ajustes.h"
#include "idioma.h"
#include "layout.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define EX_NOS_MAX     24
#define EX_LINHAS_MAX  48
#define EX_PAINEL_X    1372.0f
#define EX_PAINEL_W    470.0f
#define EX_CEU_DIR     1300.0f
#define EX_ANEL_DIR    1150.0f
#define EX_TEMPO_Y     1004.0f
#define EX_PEDACO      120.0f
#define EX_TEX_LARG    160.0f
#define EX_GIRO_S      1.6f

enum { NO_SEM = 0, NO_PONTE, NO_FIO, NO_SORTE, NO_SORTEADO };
enum { LI_ANEL = 0, LI_PONTE, LI_FIO, LI_SORTE };

typedef struct {
  int tipo, ref;
  float x, y;
  float raio;       // para o layout e a navegacao
  float atraso;     // entrada escalonada, do centro para fora
  float foco;       // 0..1, animado
  unsigned chave;
} ExNo;

typedef struct { int a, b, tipo; } ExLinha;

static Mapa mapa;
static unsigned mapaRev;
static ExNo nos[EX_NOS_MAX];
static int nNos;
static ExLinha linhas[EX_LINHAS_MAX];
static int nLinhas;
static int foco;
static float tempo, entrada, painelA, camX, camY;
static float centroX, centroY, raioX, raioY;
static int sair, pediuAbrir, pediuIndice, abrindo;
static int girando, sorteado = -1, giroFinal, giroMostra;
static float giroT;
static unsigned sorteio = 0x9e3779b9u;
static int mostrouPainel = -1;
static char curiosidadeTxt[220];   // calculada em montar(), nao por quadro

// --- utilidades ---------------------------------------------------------------

static unsigned hashTexto(const char *s, unsigned h) {
  for (; s && *s; s++) { h ^= (unsigned char)*s; h *= 16777619u; }
  return h;
}

static float suave(float t) {            // ease-out-quart
  t = anim_clamp(t, 0.0f, 1.0f);
  t = 1.0f - t;
  return 1.0f - t * t * t * t;
}

static void acento(float *r, float *g, float *b) { ajustes_acento(r, g, b); }

// Cor de texto clara puxada para o acento, fixa por sessao (texto com cor nova
// e rasterizacao nova: a cor muda so quando o tema muda).
static void tintaAcento(int *r, int *g, int *b) {
  float ar, ag, ab;
  acento(&ar, &ag, &ab);
  *r = (int)((ar * 0.55f + 0.45f) * 255.0f);
  *g = (int)((ag * 0.55f + 0.45f) * 255.0f);
  *b = (int)((ab * 0.55f + 0.45f) * 255.0f);
}

static const MapaObra *obraDoNo(const ExNo *n) {
  if (!n) return NULL;
  switch (n->tipo) {
    case NO_SEM:      return &mapa.sem[n->ref];
    case NO_PONTE:    return &mapa.pontes[n->ref].obra;
    case NO_FIO:      return mapa.fios[n->ref].temProxima ? &mapa.fios[n->ref].proxima : NULL;
    case NO_SORTEADO: return sorteado >= 0 ? &mapa.sorte[sorteado] : NULL;
  }
  return NULL;
}

static const char *posterDoNo(const ExNo *n) {
  const MapaObra *o;
  if (n->tipo == NO_FIO) return mapa.fios[n->ref].foto;
  if (n->tipo == NO_SORTE)
    return (girando && mapa.nSorte > 0) ? mapa.sorte[giroMostra].poster : NULL;
  o = obraDoNo(n);
  return o && o->poster[0] ? o->poster : NULL;
}

// --- layout --------------------------------------------------------------------

static int addNo(int tipo, int ref, float x, float y, float raio) {
  ExNo *n;
  if (nNos >= EX_NOS_MAX) return -1;
  n = &nos[nNos];
  memset(n, 0, sizeof *n);
  n->tipo = tipo; n->ref = ref; n->x = x; n->y = y; n->raio = raio;
  n->chave = hashTexto(tipo == NO_FIO ? mapa.fios[ref].nome :
                       tipo == NO_SEM ? mapa.sem[ref].titulo :
                       tipo == NO_PONTE ? mapa.pontes[ref].obra.titulo : "sorte",
                       2166136261u + (unsigned)tipo * 131u);
  return nNos++;
}

static void addLinha(int a, int b, int tipo) {
  if (a < 0 || b < 0 || a == b || nLinhas >= EX_LINHAS_MAX) return;
  linhas[nLinhas].a = a; linhas[nLinhas].b = b; linhas[nLinhas].tipo = tipo;
  nLinhas++;
}

static void limitar(ExNo *n) {
  float x0 = ajustes_conteudo_x() + n->raio + 10.0f;
  float x1 = EX_CEU_DIR - n->raio - 10.0f;
  n->x = anim_clamp(n->x, x0, x1);
  n->y = anim_clamp(n->y, 250.0f + n->raio, 905.0f - n->raio);
}

static void curiosidade(char *dst, size_t n);
static void rotuloEspacado(const char *s, float x, float y, float a);
static void disco(float x, float y, float raio, float r, float g, float b, float a);

static void montar(void) {
  int ordem[MAPA_SEM_MAX], noSem[MAPA_SEM_MAX], i, j, k, n = mapa.nSem;
  unsigned chaveFoco = (foco >= 0 && foco < nNos) ? nos[foco].chave : 0;
  float x0 = ajustes_conteudo_x();
  nNos = nLinhas = 0;
  // A elipse deixa uma coluna livre a direita (x > EX_ANEL_DIR): e ali que
  // moram o dado e o sorteado, sem disputar lugar com as estrelas.
  centroX = (x0 + EX_ANEL_DIR) * 0.5f;
  centroY = 575.0f;
  raioX = (EX_ANEL_DIR - x0) * 0.5f - 80.0f;
  raioY = 270.0f;

  // Sementes numa elipse, em ordem de ano: o anel tambem le como um relogio.
  for (i = 0; i < n; i++) ordem[i] = i;
  for (i = 1; i < n; i++) {
    int t = ordem[i];
    for (j = i; j > 0 && mapa.sem[ordem[j - 1]].ano > mapa.sem[t].ano; j--) ordem[j] = ordem[j - 1];
    ordem[j] = t;
  }
  for (i = 0; i < n; i++) {
    float ang = n == 1 ? 3.14159265f : n == 2 ? (i ? 0.0f : 3.14159265f)
              : -2.3f + 6.2831853f * (float)i / (float)n;
    noSem[ordem[i]] = addNo(NO_SEM, ordem[i], centroX + cosf(ang) * raioX,
                            centroY + sinf(ang) * raioY, 50.0f);
  }
  for (i = 0; i < n && n >= 2; i++) {
    if (n == 2 && i == 1) break;
    addLinha(noSem[ordem[i]], noSem[ordem[(i + 1) % n]], LI_ANEL);
  }

  // Pontes: a primeira no centro; as outras entre as duas sementes que cruzam,
  // puxadas para dentro do anel.
  for (k = 0; k < mapa.nPontes; k++) {
    const MapaPonte *p = &mapa.pontes[k];
    float mx, my, x, y;
    int no;
    if (p->a >= n || p->b >= n) continue;
    mx = (nos[noSem[p->a]].x + nos[noSem[p->b]].x) * 0.5f;
    my = (nos[noSem[p->a]].y + nos[noSem[p->b]].y) * 0.5f;
    if (k == 0) { x = centroX; y = centroY - 20.0f; }
    else { x = centroX + (mx - centroX) * 0.56f; y = centroY + (my - centroY) * 0.56f; }
    no = addNo(NO_PONTE, k, x, y, k == 0 ? 118.0f : 62.0f);
    addLinha(noSem[p->a], no, LI_PONTE);
    if (p->b != p->a) addLinha(noSem[p->b], no, LI_PONTE);
  }

  // Fios: por fora do anel, na direcao das sementes que a pessoa atravessa.
  for (k = 0; k < mapa.nFios; k++) {
    const MapaFio *f = &mapa.fios[k];
    float sx = 0, sy = 0, dx, dy, d;
    int no;
    for (i = 0; i < f->n; i++) { sx += nos[noSem[f->sementes[i]]].x; sy += nos[noSem[f->sementes[i]]].y; }
    sx /= (float)(f->n ? f->n : 1); sy /= (float)(f->n ? f->n : 1);
    dx = (sx - centroX) / raioX; dy = (sy - centroY) / raioY;
    d = sqrtf(dx * dx + dy * dy);
    if (d < 0.2f) { dx = k ? 0.9f : -0.9f; dy = -0.4f; d = 1.0f; }
    no = addNo(NO_FIO, k, centroX + dx / d * raioX * 1.16f, centroY + dy / d * raioY * 1.22f, 40.0f);
    for (i = 0; i < f->n; i++) addLinha(noSem[f->sementes[i]], no, LI_FIO);
  }

  // O dado da sorte, embaixo do centro; o sorteado nasce ao lado dele.
  if (mapa.nSorte > 0) {
    // Canto de baixo a direita: fora da elipse, e a ultima parada das setas.
    int dado = addNo(NO_SORTE, 0, EX_CEU_DIR - 75.0f, 862.0f, 42.0f);
    if (sorteado >= 0 && sorteado < mapa.nSorte) {
      int s = addNo(NO_SORTEADO, sorteado, EX_CEU_DIR - 75.0f, 672.0f, 58.0f);
      nos[s].chave = hashTexto(mapa.sorte[sorteado].titulo, 77u);
      addLinha(dado, s, LI_SORTE);
    }
  }

  // Relaxamento: afasta o que ficou encostado. Sementes e centro nao se
  // movem; roda so aqui, nunca por quadro.
  for (k = 0; k < 48; k++)
    for (i = 0; i < nNos; i++)
      for (j = 0; j < nNos; j++) {
        ExNo *a = &nos[i], *b = &nos[j];
        float dx, dy, d, min;
        if (i == j || a->tipo == NO_SEM || a->tipo == NO_SORTE || a->tipo == NO_SORTEADO ||
            (a->tipo == NO_PONTE && a->ref == 0)) continue;
        dx = a->x - b->x; dy = a->y - b->y;
        d = sqrtf(dx * dx + dy * dy);
        // A folga conta o rotulo que fica embaixo de cada no.
        min = a->raio + b->raio + 66.0f;
        if (d >= min) continue;
        if (d < 1.0f) { dx = 1.0f; dy = 0.3f; d = 1.04f; }
        a->x += dx / d * (min - d) * 0.5f;
        a->y += dy / d * (min - d) * 0.5f;
        limitar(a);
      }
  for (i = 0; i < nNos; i++) {
    float dx = nos[i].x - centroX, dy = nos[i].y - centroY;
    nos[i].atraso = 0.05f + sqrtf(dx * dx + dy * dy) / 1500.0f;
  }

  foco = 0;
  for (i = 0; i < nNos; i++) if (nos[i].tipo == NO_PONTE && nos[i].ref == 0) foco = i;
  for (i = 0; i < nNos; i++) if (chaveFoco && nos[i].chave == chaveFoco) foco = i;
  if (foco >= nNos) foco = 0;
  curiosidade(curiosidadeTxt, sizeof curiosidadeTxt);
}

// --- navegacao -----------------------------------------------------------------

static int vizinho(float dx, float dy) {
  int j, melhor = -1;
  float bs = 1e9f;
  if (foco < 0 || foco >= nNos) return -1;
  for (j = 0; j < nNos; j++) {
    float vx, vy, frente, lado, sc;
    if (j == foco) continue;
    vx = nos[j].x - nos[foco].x; vy = nos[j].y - nos[foco].y;
    frente = vx * dx + vy * dy;
    if (frente < 24.0f) continue;
    lado = fabsf(vx * dy - vy * dx);
    if (lado > frente * 2.6f) continue;
    sc = frente + lado * 2.2f;
    if (sc < bs) { bs = sc; melhor = j; }
  }
  return melhor;
}

static int indicePorImdb(const char *imdb) {
  int i, n = cat_n();
  if (!imdb || !imdb[0]) return -1;
  for (i = 0; i < n; i++) {
    const CatItem *ci = cat_item(i);
    if (ci && !strcmp(ci->imdb, imdb)) return i;
  }
  return -1;
}

static void abrirObra(const MapaObra *o) {
  int idx;
  if (!o || !o->titulo[0]) return;
  idx = indicePorImdb(o->imdb);
  if (idx < 0 && o->catIndice >= 0) {
    const CatItem *ci = cat_item(o->catIndice);
    if (ci && !strcmp(ci->titulo, o->titulo)) idx = o->catIndice;
  }
  if (idx >= 0) { pediuIndice = idx; pediuAbrir = 1; return; }
  // Fora do catalogo: a descoberta busca o meta num fio e o roteador (app.c,
  // trocaDeTituloSeSolicitada) abre quando ele entrar.
  if (desc_titulo_buscando()) return;
  if (o->tmdb > 0) desc_pedir_titulo_tmdb(o->tmdb, !strcmp(o->tipo, "series") ? "tv" : "movie");
  else if (o->imdb[0]) desc_pedir_titulo(o->imdb);
  else return;
  abrindo = 1;
}

static unsigned proximoSorteio(void) {
  sorteio = sorteio * 1664525u + 1013904223u + (unsigned)SDL_GetTicks();
  return sorteio >> 8;
}

static void girar(void) {
  int i, total = 0, alvo;
  if (mapa.nSorte <= 0 || girando) return;
  // Peso decrescente: os primeiros do vetor sao os mais alinhados ao gosto,
  // mas o ultimo ainda pode sair — e sorte, nao ranking.
  for (i = 0; i < mapa.nSorte; i++) total += mapa.nSorte - i + 2;
  alvo = (int)(proximoSorteio() % (unsigned)total);
  for (i = 0; i < mapa.nSorte; i++) {
    alvo -= mapa.nSorte - i + 2;
    if (alvo < 0) break;
  }
  giroFinal = i < mapa.nSorte ? i : 0;
  if (giroFinal == sorteado && mapa.nSorte > 1) giroFinal = (giroFinal + 1) % mapa.nSorte;
  giroT = 0.0f;
  girando = 1;
  giroMostra = 0;
}

static void terminarGiro(void) {
  int i;
  girando = 0;
  sorteado = giroFinal;
  montar();
  for (i = 0; i < nNos; i++) if (nos[i].tipo == NO_SORTEADO) { foco = i; nos[i].atraso = 0.0f; }
  entrada = 10.0f;
}

// --- desenho: primitivas -----------------------------------------------------

static void linha(float x0, float y0, float x1, float y1, float esp, float halo,
                  float r, float g, float b, float a) {
  float dx = x1 - x0, dy = y1 - y0;
  float len = sqrtf(dx * dx + dy * dy);
  int n, i;
  if (len < 1.0f || a <= 0.004f) return;
  n = (int)ceilf(len / EX_PEDACO);
  for (i = 0; i < n; i++) {
    float ax = x0 + dx * (float)i / (float)n, ay = y0 + dy * (float)i / (float)n;
    float bx = x0 + dx * (float)(i + 1) / (float)n, by = y0 + dy * (float)(i + 1) / (float)n;
    GfxRect q;
    q.x = fminf(ax, bx) - halo; q.y = fminf(ay, by) - halo;
    q.w = fabsf(bx - ax) + 2.0f * halo; q.h = fabsf(by - ay) + 2.0f * halo;
    gfx_rect(q, 0, GFX_LINHA, esp / q.h, ((bx - ax) * (by - ay) < 0.0f) ? 1.0f : 0.0f, 0,
             halo / q.h, r, g, b, a);
  }
}

static void brilho(float x, float y, float raio, float r, float g, float b, float a) {
  if (a <= 0.004f) return;
  gfx_rect((GfxRect){ x - raio, y - raio, raio * 2.0f, raio * 2.0f }, 0, GFX_SOMBRA,
           1.0f, 0, 0, 0.5f, r, g, b, a);
}

static void disco(float x, float y, float raio, float r, float g, float b, float a) {
  gfx_rect((GfxRect){ x - raio, y - raio, raio * 2.0f, raio * 2.0f }, 0, GFX_DISCO,
           0, 0, 0, 0, r, g, b, a);
}

static int avatar(const char *url, float x, float y, float raio, float a) {
  GLuint t = url && url[0] ? tex_obter_larg(url, EX_TEX_LARG) : 0;
  if (!t) return 0;
  gfx_tex_aspect_atual = tex_aspecto(url);
  gfx_rect((GfxRect){ x - raio, y - raio, raio * 2.0f, raio * 2.0f }, t, GFX_AVATAR,
           0, 0, 0, 0, 1, 1, 1, a);
  gfx_tex_aspect_atual = 0.0f;
  return 1;
}

static void cartaz(const char *url, GfxRect r, float focado, float a) {
  GLuint t = url && url[0] ? tex_obter_larg(url, EX_TEX_LARG) : 0;
  if (t) {
    gfx_tex_aspect_atual = tex_aspecto(url);
    gfx_rect(r, t, GFX_CARD, focado, 0, 0, 0.05f, 1, 1, 1, a);
    gfx_tex_aspect_atual = 0.0f;
  } else {
    gfx_cor(r, 0.05f, 0.10f, 0.11f, 0.15f, a);
  }
}

static void rotuloCentrado(TxtEstilo e, const char *s, int r, int g, int b,
                           float cx, float y, float maxW, float a) {
  TxtLinha l;
  if (!s || !s[0] || a <= 0.01f) return;
  l = txt_linha_corta(e, s, r, g, b, 255, maxW);
  txt_desenhar_alpha(l, cx - l.w * 0.5f, y, a);
}

// --- desenho: ceu -----------------------------------------------------------------

static void desenharCeu(void) {
  float ar, ag, ab, cr, cg, cb;
  const ExNo *f = (foco >= 0 && foco < nNos) ? &nos[foco] : NULL;
  const char *p = f ? posterDoNo(f) : NULL;
  acento(&ar, &ag, &ab);
  cr = ar; cg = ag; cb = ab;
  if (p && tex_cor_fundo(p, &cr, &cg, &cb)) {
    cr = cr * 0.6f + ar * 0.4f; cg = cg * 0.6f + ag * 0.4f; cb = cb * 0.6f + ab * 0.4f;
  }
  gfx_rect((GfxRect){ 0, 0, NV_TELA_W, NV_TELA_H }, 0, GFX_CEU, tempo,
           tempo * 0.35f - camX * 0.03f, -camY * 0.03f, 0, cr, cg, cb, 1.0f);
}

static int temaLigado(const MapaTema *t) {
  const ExNo *f = (foco >= 0 && foco < nNos) ? &nos[foco] : NULL;
  if (!f) return 0;
  if (f->tipo == NO_SEM) return (t->mascara >> f->ref) & 1u;
  if (f->tipo == NO_PONTE)
    return ((t->mascara >> mapa.pontes[f->ref].a) & 1u) && ((t->mascara >> mapa.pontes[f->ref].b) & 1u);
  return 0;
}

// Temas que se repetem, numa linha sob o titulo. Os ligados ao no em foco
// acendem; e o cruzamento lido em palavras, sem disputar espaco com o ceu.
static void desenharTemas(void) {
  int i, tr, tg, tb;
  float x = ajustes_conteudo_x(), e = suave((entrada - 0.5f) / 0.8f);
  if (mapa.nTemas <= 0 || e <= 0.01f) return;
  tintaAcento(&tr, &tg, &tb);
  rotuloEspacado(i18n("TEMAS"), x, 206.0f, 0.9f * e);
  x += txt_tracking(TXT_CAPTION2, i18n("TEMAS"), tr, tg, tb, -1, 0, 0, 2.4f) + 22.0f;
  for (i = 0; i < mapa.nTemas; i++) {
    int aceso = temaLigado(&mapa.temas[i]);
    TxtLinha l = aceso ? txt_linha(TXT_BODY, mapa.temas[i].nome, tr, tg, tb, 255)
                       : txt_linha(TXT_BODY, mapa.temas[i].nome, 178, 182, 196, 255);
    if (x + l.w > EX_CEU_DIR) break;
    disco(x + 5.0f, 206.0f + 12.0f, aceso ? 5.0f : 3.5f, aceso ? tr / 255.0f : 0.6f,
          aceso ? tg / 255.0f : 0.62f, aceso ? tb / 255.0f : 0.7f, e);
    txt_desenhar_alpha(l, x + 18.0f, 206.0f - 2.0f, (aceso ? 1.0f : 0.72f) * e);
    x += 18.0f + l.w + 30.0f;
  }
}

static int linhaAcesa(const ExLinha *l) { return l->a == foco || l->b == foco; }

static void desenharLinhas(int reduzida) {
  float ar, ag, ab;
  int i;
  acento(&ar, &ag, &ab);
  for (i = 0; i < nLinhas; i++) {
    const ExLinha *l = &linhas[i];
    const ExNo *a = &nos[l->a], *b = &nos[l->b];
    float k = suave((entrada - fmaxf(a->atraso, b->atraso) - 0.25f) / 0.6f);
    float x0 = a->x + camX, y0 = a->y + camY;
    float x1 = x0 + (b->x + camX - x0) * k, y1 = y0 + (b->y + camY - y0) * k;
    int aceso = linhaAcesa(l);
    float cr, cg, cb, al, esp;
    if (k <= 0.0f) continue;
    if (l->tipo == LI_ANEL)      { cr = 0.74f; cg = 0.78f; cb = 0.92f; al = aceso ? 0.36f : 0.13f; esp = 1.3f; }
    else if (l->tipo == LI_FIO)  { cr = 0.56f; cg = 0.64f; cb = 1.00f; al = aceso ? 0.85f : 0.20f; esp = 1.6f; }
    else                         { cr = ar;    cg = ag;    cb = ab;    al = aceso ? 0.90f : 0.22f; esp = aceso ? 2.4f : 1.5f; }
    linha(x0, y0, x1, y1, esp, aceso ? 11.0f : 6.0f, cr, cg, cb, al);
    // Cometa: a historia correndo PARA o no em foco. Sem animacao, a linha
    // acesa ja diz o mesmo.
    if (aceso && !reduzida && k >= 1.0f && l->tipo != LI_ANEL) {
      float t = fmodf(tempo * 0.55f + (float)i * 0.37f, 1.0f);
      float fx, fy, px, py, v;
      const ExNo *de = l->a == foco ? b : a, *para = l->a == foco ? a : b;
      fx = de->x + camX; fy = de->y + camY;
      px = fx + (para->x + camX - fx) * t; py = fy + (para->y + camY - fy) * t;
      v = sinf(t * 3.14159265f);
      brilho(px, py, 20.0f, cr, cg, cb, 0.85f * v);
      disco(px, py, 3.2f, 0.96f, 0.97f, 1.0f, 0.95f * v);
    }
  }
}

// --- desenho: nos -------------------------------------------------------------

static void desenharNo(int i, int reduzida) {
  ExNo *n = &nos[i];
  float e = reduzida ? 1.0f : suave((entrada - n->atraso) / 0.5f);
  float f = n->foco, x = n->x + camX, y = n->y + camY;
  float ar, ag, ab, esc = 0.6f + 0.4f * e;
  int tr, tg, tb;
  if (e <= 0.01f) return;
  acento(&ar, &ag, &ab);
  tintaAcento(&tr, &tg, &tb);

  if (n->tipo == NO_SEM) {
    const MapaObra *o = &mapa.sem[n->ref];
    float r = (46.0f + 12.0f * f) * esc;
    brilho(x, y, r + 34.0f + 10.0f * f, 0.62f, 0.70f, 1.0f, (0.16f + 0.30f * f) * e);
    disco(x, y, r + 3.0f + f * 1.5f, f > 0.5f ? ar : 0.86f, f > 0.5f ? ag : 0.88f,
          f > 0.5f ? ab : 0.95f, (0.38f + 0.62f * f) * e);
    if (!avatar(o->poster, x, y, r, e)) disco(x, y, r, 0.10f, 0.11f, 0.16f, e);
    rotuloCentrado(TXT_CAPTION2, o->titulo, 232, 234, 240, x, y + r + 12.0f, 210.0f,
                   (0.62f + 0.38f * f) * e);
  } else if (n->tipo == NO_PONTE) {
    const MapaObra *o = &mapa.pontes[n->ref].obra;
    int centro = n->ref == 0;
    float h = (centro ? 216.0f : 104.0f) * (1.0f + 0.16f * f) * esc, w = h * 2.0f / 3.0f;
    GfxRect r = { x - w * 0.5f, y - h * 0.5f, w, h };
    brilho(x, y, h * 0.62f + 30.0f, ar, ag, ab, (centro ? 0.42f : 0.20f + 0.28f * f) * e);
    if (f > 0.02f) gfx_cor((GfxRect){ r.x - 3, r.y - 3, r.w + 6, r.h + 6 }, 0.07f, ar, ag, ab, 0.95f * f * e);
    cartaz(o->poster, r, f, e);
    if (centro) {
      TxtLinha l = txt_linha(TXT_CAPTION2, i18n("SUA PRÓXIMA HISTÓRIA"), tr, tg, tb, 255);
      txt_desenhar_alpha(l, x - l.w * 0.5f, r.y - 34.0f, e);
      rotuloCentrado(TXT_CAPTION2, o->titulo, 240, 241, 245, x, r.y + r.h + 12.0f, 260.0f, e);
    } else if (f > 0.05f) {
      rotuloCentrado(TXT_CAPTION2, o->titulo, 240, 241, 245, x, r.y + r.h + 10.0f, 230.0f, f * e);
    }
  } else if (n->tipo == NO_FIO) {
    const MapaFio *fi = &mapa.fios[n->ref];
    float r = (34.0f + 10.0f * f) * esc;
    brilho(x, y, r + 30.0f, 0.50f, 0.58f, 1.0f, (0.22f + 0.30f * f) * e);
    disco(x, y, r + 3.0f, 0.56f, 0.64f, 1.0f, (0.55f + 0.45f * f) * e);
    if (!avatar(fi->foto, x, y, r, e)) {
      char ini[8];
      TxtLinha l;
      disco(x, y, r, 0.12f, 0.13f, 0.22f, e);
      snprintf(ini, sizeof ini, "%.*s", (fi->nome[0] & 0x80) ? 2 : 1, fi->nome);
      l = txt_linha(TXT_HEADLINE, ini, 214, 222, 255, 255);
      txt_desenhar_alpha(l, x - l.w * 0.5f, y - l.h * 0.5f, e);
    }
    rotuloCentrado(TXT_CAPTION2, fi->nome, 196, 206, 255, x, y + r + 10.0f, 220.0f, (0.7f + 0.3f * f) * e);
  } else if (n->tipo == NO_SORTE) {
    float r = (40.0f + 8.0f * f) * esc;
    float giro = girando ? giroT / EX_GIRO_S : 0.0f;
    brilho(x, y, r + 30.0f + 16.0f * sinf(giro * 3.14159265f), ar, ag, ab, (0.20f + 0.35f * f) * e);
    disco(x, y, r + 3.0f, ar, ag, ab, (0.45f + 0.55f * f) * e);
    if (!girando || !avatar(posterDoNo(n), x, y, r, e)) {
      TxtLinha l;
      disco(x, y, r, 0.09f, 0.09f, 0.13f, e);
      l = txt_linha(TXT_TITULO3, "?", 246, 246, 250, 255);
      txt_desenhar_alpha(l, x - l.w * 0.5f, y - l.h * 0.5f, e);
    }
    rotuloCentrado(TXT_CAPTION2, i18n("Sorte guiada"), tr, tg, tb, x, y + r + 10.0f, 200.0f, (0.7f + 0.3f * f) * e);
  } else if (n->tipo == NO_SORTEADO && sorteado >= 0) {
    const MapaObra *o = &mapa.sorte[sorteado];
    float h = 112.0f * (1.0f + 0.16f * f) * esc, w = h * 2.0f / 3.0f;
    GfxRect r = { x - w * 0.5f, y - h * 0.5f, w, h };
    brilho(x, y, h * 0.62f + 26.0f, ar, ag, ab, (0.26f + 0.26f * f) * e);
    if (f > 0.02f) gfx_cor((GfxRect){ r.x - 3, r.y - 3, r.w + 6, r.h + 6 }, 0.07f, ar, ag, ab, 0.95f * f * e);
    cartaz(o->poster, r, f, e);
    rotuloCentrado(TXT_CAPTION2, o->titulo, 240, 241, 245, x, r.y + r.h + 10.0f, 220.0f, (0.6f + 0.4f * f) * e);
  }
}

// --- desenho: linha do tempo -----------------------------------------------------

static void desenharTempo(void) {
  float x0 = ajustes_conteudo_x(), x1 = EX_CEU_DIR - 20.0f, y = EX_TEMPO_Y;
  int d0, d1, passo, d, i, tr, tg, tb;
  float ar, ag, ab;
  float e = suave((entrada - 0.4f) / 0.8f);
  if (mapa.anoMax <= 0 || mapa.anoMin <= 0 || e <= 0.01f) return;
  acento(&ar, &ag, &ab);
  tintaAcento(&tr, &tg, &tb);
  d0 = (mapa.anoMin / 10) * 10;
  d1 = (mapa.anoMax / 10) * 10 + 10;
  if (d1 - d0 < 30) { d0 -= 10; d1 += 10; }
  passo = (d1 - d0) > 70 ? 20 : 10;
#define XANO(a) (x0 + (x1 - x0) * ((float)(a) - (float)d0) / (float)(d1 - d0))
  gfx_cor((GfxRect){ x0, y, (x1 - x0) * e, 1.0f }, 0.0f, 0.60f, 0.64f, 0.76f, 0.40f);
  for (d = d0; d <= d1; d += passo) {
    char s[8];
    TxtLinha l;
    float x = XANO(d);
    snprintf(s, sizeof s, "%d", d);
    gfx_cor((GfxRect){ x, y - 5.0f, 1.0f, 11.0f }, 0.0f, 0.60f, 0.64f, 0.76f, 0.45f * e);
    l = txt_linha(TXT_CAPTION2, s, 150, 156, 172, 255);
    txt_desenhar_alpha(l, x - l.w * 0.5f, y + 14.0f, e);
  }
  for (i = 0; i < nNos; i++) {
    const MapaObra *o = obraDoNo(&nos[i]);
    float x, f = nos[i].foco;
    int semente = nos[i].tipo == NO_SEM;
    if (!o || !o->ano) continue;
    x = XANO(o->ano);
    if (f > 0.05f) {
      // O fio do no ate o seu ano: a mesma estrela vista no tempo.
      float nx = nos[i].x + camX, ny = nos[i].y + camY + 60.0f;
      linha(nx, ny, x, y - 12.0f, 1.2f, 8.0f, ar, ag, ab, 0.38f * f * e);
      brilho(x, y, 24.0f, ar, ag, ab, 0.7f * f * e);
      { char s[8]; TxtLinha l;
        snprintf(s, sizeof s, "%d", o->ano);
        l = txt_linha(TXT_CAPTION2, s, tr, tg, tb, 255);
        txt_desenhar_alpha(l, x - l.w * 0.5f, y - 42.0f, f * e); }
    }
    disco(x, y, (semente ? 5.0f : 4.0f) + 3.0f * f,
          semente ? 0.92f : ar, semente ? 0.94f : ag, semente ? 1.0f : ab, (0.75f + 0.25f * f) * e);
  }
#undef XANO
}

// --- desenho: painel ---------------------------------------------------------------

static void rotuloEspacado(const char *s, float x, float y, float a) {
  int tr, tg, tb;
  tintaAcento(&tr, &tg, &tb);
  txt_tracking(TXT_CAPTION2, s, tr, tg, tb, x, y, a, 2.4f);
}

static void metaObra(const MapaObra *o, char *dst, size_t n) {
  char nota[24] = "";
  if (o->nota > 0) snprintf(nota, sizeof nota, "   ·   TMDB %d,%d", o->nota / 10, o->nota % 10);
  if (o->ano) snprintf(dst, n, "%s   ·   %d%s", i18n(!strcmp(o->tipo, "series") ? "Série" : "Filme"), o->ano, nota);
  else snprintf(dst, n, "%s%s", i18n(!strcmp(o->tipo, "series") ? "Série" : "Filme"), nota);
}

static void frasePonte(const MapaPonte *p, char *dst, size_t n) {
  int so = p->a == p->b;
  switch (p->elo) {
    case MAPA_ELO_TEMA:   snprintf(dst, n, i18n(so ? "Também fala de %s." : "As duas falam de %s."), i18n(p->motivo)); break;
    case MAPA_ELO_PESSOA: snprintf(dst, n, i18n("O fio entre elas: %s."), p->motivo); break;
    case MAPA_ELO_GENERO: snprintf(dst, n, i18n(so ? "O mesmo gosto por %s." : "As duas passam por %s."), i18n(p->motivo)); break;
    case MAPA_ELO_DUPLA:  snprintf(dst, n, "%s", i18n("O TMDB chega aqui a partir das duas.")); break;
    case MAPA_ELO_DECADA: snprintf(dst, n, i18n("As duas são dos anos %s."), p->motivo); break;
    default:              snprintf(dst, n, "%s", i18n("Vizinha do que você gosta.")); break;
  }
}

static float painelObra(const MapaObra *o, float x, float y, float a, int sinLinhas) {
  char meta[96];
  float h;
  h = txt_bloco(TXT_TITULO3, o->titulo, 246, 246, 248, x, y, EX_PAINEL_W, 56.0f, a, 2);
  y += h + 8.0f;
  metaObra(o, meta, sizeof meta);
  { TxtLinha l = txt_linha_corta(TXT_HERO_META, meta, 186, 190, 202, 255, EX_PAINEL_W);
    txt_desenhar_alpha(l, x, y, a); }
  y += 40.0f;
  if (o->sinopse[0] && sinLinhas > 0) {
    y += 8.0f;
    y += txt_bloco(TXT_CAPTION, o->sinopse, 214, 217, 226, x, y, EX_PAINEL_W, 31.0f, a * 0.92f, sinLinhas);
  }
  return y;
}

static void acaoPainel(const char *s, float a) {
  float ar, ag, ab;
  GfxRect r = { EX_PAINEL_X, 916.0f, 0, 54.0f };
  TxtLinha ok, l;
  acento(&ar, &ag, &ab);
  ok = txt_linha(TXT_CAPTION2, "OK", ajustes_tinta_foco(), ajustes_tinta_foco(), ajustes_tinta_foco(), 255);
  l = txt_linha(TXT_BODY, s, 240, 241, 245, 255);
  r.w = ok.w + 28.0f + 18.0f + l.w + 22.0f;
  gfx_cor(r, 0.5f, 0.14f, 0.15f, 0.19f, 0.86f * a);
  gfx_cor((GfxRect){ r.x + 8.0f, r.y + 9.0f, ok.w + 26.0f, 36.0f }, 0.5f, ar, ag, ab, a);
  txt_desenhar_alpha(ok, r.x + 21.0f, r.y + (r.h - ok.h) * 0.5f, a);
  txt_desenhar_alpha(l, r.x + ok.w + 50.0f, r.y + (r.h - l.h) * 0.5f, a);
}

static void listaTitulos(const char *rotulo, const int *sem, int n, float x, float *y, float a) {
  int i;
  if (n <= 0) return;
  rotuloEspacado(rotulo, x, *y, a);
  *y += 34.0f;
  for (i = 0; i < n && i < 4; i++) {
    TxtLinha l = txt_linha_corta(TXT_BODY, mapa.sem[sem[i]].titulo, 226, 228, 236, 255, EX_PAINEL_W - 28.0f);
    disco(x + 5.0f, *y + l.h * 0.5f, 4.0f, 0.86f, 0.88f, 0.96f, 0.9f * a);
    txt_desenhar_alpha(l, x + 22.0f, *y, a);
    *y += 36.0f;
  }
  *y += 10.0f;
}

static void desenharPainel(void) {
  const ExNo *n = (foco >= 0 && foco < nNos) ? &nos[foco] : NULL;
  float x = EX_PAINEL_X, y = 268.0f, a = painelA;
  int tr, tg, tb;
  if (!n) return;
  tintaAcento(&tr, &tg, &tb);

  if (n->tipo == NO_PONTE) {
    const MapaPonte *p = &mapa.pontes[n->ref];
    char frase[160];
    rotuloEspacado(i18n(n->ref == 0 ? "SUA PRÓXIMA HISTÓRIA" : "UMA PONTE ENTRE HISTÓRIAS"), x, y, a);
    y += 42.0f;
    // As duas de onde ela vem, em miniatura: o "porque voce viu" desenhado.
    { GfxRect m = { x, y, 58.0f, 87.0f };
      TxtLinha pq = txt_linha(TXT_CAPTION2, i18n("Porque você viu"), 170, 174, 188, 255);
      float tx = x + (p->a != p->b ? 140.0f : 74.0f);
      cartaz(mapa.sem[p->a].poster, m, 0, a);
      if (p->a != p->b) {
        TxtLinha mais = txt_linha(TXT_HEADLINE, "+", tr, tg, tb, 255);
        txt_desenhar_alpha(mais, x + 62.0f + (14.0f - mais.w * 0.5f), y + 26.0f, a);
        m.x += 80.0f;
        cartaz(mapa.sem[p->b].poster, m, 0, a);
      }
      txt_desenhar_alpha(pq, tx, y + 4.0f, a);
      { TxtLinha t1 = txt_linha_corta(TXT_BODY, mapa.sem[p->a].titulo, 232, 234, 240, 255, EX_PAINEL_W - (tx - x));
        txt_desenhar_alpha(t1, tx, y + 32.0f, a); }
      if (p->a != p->b) {
        TxtLinha t2 = txt_linha_corta(TXT_BODY, mapa.sem[p->b].titulo, 232, 234, 240, 255, EX_PAINEL_W - (tx - x));
        txt_desenhar_alpha(t2, tx, y + 62.0f, a);
      } }
    y += 120.0f;
    frasePonte(p, frase, sizeof frase);
    { TxtLinha fl = txt_linha_corta(TXT_CALLOUT, frase, tr, tg, tb, 255, EX_PAINEL_W);
      txt_desenhar_alpha(fl, x, y, a); }
    y += 56.0f;
    painelObra(&p->obra, x, y, a, 5);
    acaoPainel(i18n(abrindo ? "Abrindo…" : "Abrir título"), a);
  } else if (n->tipo == NO_SEM) {
    const MapaObra *o = &mapa.sem[n->ref];
    static const char *const ORIGEM[] = { "VOCÊ VIU", "VOCÊ ESTÁ VENDO", "NA SUA LISTA", "EM ALTA NO SEU CATÁLOGO" };
    char temas[200] = "";
    int i, pontes[MAPA_PONTE_MAX], np = 0;
    rotuloEspacado(i18n(ORIGEM[mapa.semOrigem[n->ref] & 3]), x, y, a);
    y += 42.0f;
    y = painelObra(o, x, y, a, 4) + 18.0f;
    for (i = 0; i < mapa.nTemas; i++)
      if ((mapa.temas[i].mascara >> n->ref) & 1u) {
        size_t k = strlen(temas);
        snprintf(temas + k, sizeof temas - k, "%s%s", k ? "   ·   " : "", i18n(mapa.temas[i].nome));
      }
    if (temas[0]) {
      rotuloEspacado(i18n("TEMAS QUE SE REPETEM"), x, y, a);
      y += 34.0f;
      { TxtLinha l = txt_linha_corta(TXT_BODY, temas, tr, tg, tb, 255, EX_PAINEL_W);
        txt_desenhar_alpha(l, x, y, a); }
      y += 50.0f;
    }
    for (i = 0; i < mapa.nPontes; i++)
      if (mapa.pontes[i].a == n->ref || mapa.pontes[i].b == n->ref) pontes[np++] = i;
    if (np > 0 && y < 820.0f) {
      rotuloEspacado(i18n("LEVA A"), x, y, a);
      y += 34.0f;
      for (i = 0; i < np && i < 3 && y < 880.0f; i++) {
        char s[160];
        TxtLinha l;
        snprintf(s, sizeof s, "→  %s", mapa.pontes[pontes[i]].obra.titulo);
        l = txt_linha_corta(TXT_BODY, s, 226, 228, 236, 255, EX_PAINEL_W);
        txt_desenhar_alpha(l, x, y, a);
        y += 36.0f;
      }
    }
    acaoPainel(i18n(abrindo ? "Abrindo…" : "Abrir título"), a);
  } else if (n->tipo == NO_FIO) {
    const MapaFio *f = &mapa.fios[n->ref];
    int serie = f->n > 0 && !strcmp(mapa.sem[f->sementes[0]].tipo, "series");
    rotuloEspacado(i18n("O FIO DE UMA PESSOA"), x, y, a);
    y += 42.0f;
    y += txt_bloco(TXT_TITULO3, f->nome, 246, 246, 248, x, y, EX_PAINEL_W, 56.0f, a, 2) + 6.0f;
    { TxtLinha l = txt_linha(TXT_HERO_META, i18n(f->direcao ? (serie ? "Criação" : "Direção") : "Atuação"),
                             186, 190, 202, 255);
      txt_desenhar_alpha(l, x, y, a); }
    y += 52.0f;
    listaTitulos(i18n("ATRAVESSA"), f->sementes, f->n, x, &y, a);
    if (f->temProxima) {
      rotuloEspacado(i18n("PRÓXIMO NO FIO"), x, y, a);
      y += 38.0f;
      cartaz(f->proxima.poster, (GfxRect){ x, y, 96.0f, 144.0f }, 0, a);
      { char meta[96];
        float tx = x + 116.0f, w = EX_PAINEL_W - 116.0f;
        float h = txt_bloco(TXT_HEADLINE, f->proxima.titulo, 244, 245, 248, tx, y, w, 44.0f, a, 2);
        metaObra(&f->proxima, meta, sizeof meta);
        { TxtLinha l = txt_linha_corta(TXT_CAPTION2, meta, 176, 180, 194, 255, w);
          txt_desenhar_alpha(l, tx, y + h + 6.0f, a); } }
      acaoPainel(i18n(abrindo ? "Abrindo…" : "Abrir o próximo no fio"), a);
    }
  } else if (n->tipo == NO_SORTE) {
    char s[160];
    rotuloEspacado(i18n("SORTE GUIADA"), x, y, a);
    y += 42.0f;
    y += txt_bloco(TXT_TITULO3, i18n("Deixe o seu gosto escolher"), 246, 246, 248, x, y, EX_PAINEL_W, 56.0f, a, 2) + 14.0f;
    snprintf(s, sizeof s, i18n("Um sorteio entre %d histórias que cruzam os seus temas, fora das que já estão no céu."),
             mapa.nSorte);
    txt_bloco(TXT_CAPTION, s, 214, 217, 226, x, y, EX_PAINEL_W, 31.0f, a, 4);
    acaoPainel(i18n(girando ? "Girando…" : sorteado >= 0 ? "Girar de novo" : "Girar"), a);
  } else if (n->tipo == NO_SORTEADO && sorteado >= 0) {
    rotuloEspacado(i18n("O SORTEIO ESCOLHEU"), x, y, a);
    y += 42.0f;
    painelObra(&mapa.sorte[sorteado], x, y, a, 7);
    acaoPainel(i18n(abrindo ? "Abrindo…" : "Abrir título"), a);
  }
}

// Uma curiosidade por dia, tirada do proprio cruzamento.
static void curiosidade(char *dst, size_t n) {
  char fatos[5][200];
  int nf = 0, i, dec[16] = { 0 };
  time_t agora = time(NULL);
  struct tm *tm = localtime(&agora);
  dst[0] = 0;
  if (mapa.nFios > 0)
    snprintf(fatos[nf++], sizeof fatos[0], i18n("%s atravessa %d histórias que você viu."),
             mapa.fios[0].nome, mapa.fios[0].n);
  if (mapa.nTemas > 0)
    snprintf(fatos[nf++], sizeof fatos[0], i18n("“%s” volta em %d das suas histórias."),
             i18n(mapa.temas[0].nome), mapa.temas[0].n);
  if (mapa.anoMax - mapa.anoMin >= 15 && mapa.anoMin > 0)
    snprintf(fatos[nf++], sizeof fatos[0], i18n("Seu gosto atravessa %d anos de histórias: de %d a %d."),
             mapa.anoMax - mapa.anoMin, mapa.anoMin, mapa.anoMax);
  if (mapa.nPontes > 0 && mapa.pontes[0].a != mapa.pontes[0].b)
    snprintf(fatos[nf++], sizeof fatos[0], i18n("%s e %s se cruzam em %s."),
             mapa.sem[mapa.pontes[0].a].titulo, mapa.sem[mapa.pontes[0].b].titulo,
             mapa.pontes[0].obra.titulo);
  for (i = 0; i < mapa.nSem; i++)
    if (mapa.sem[i].ano >= 1900) { int d = (mapa.sem[i].ano - 1900) / 10; if (d < 16) dec[d]++; }
  for (i = 0; i < 16 && nf < 5; i++)
    if (dec[i] >= 3) {
      snprintf(fatos[nf++], sizeof fatos[0], i18n("A década de %d aparece em %d das suas histórias."),
               1900 + i * 10, dec[i]);
      break;
    }
  if (!nf) return;
  snprintf(dst, n, "%s", fatos[(tm ? tm->tm_yday : 0) % nf]);
}

static void desenharCabecalho(void) {
  TxtLinha t = txt_linha(TXT_TITULO2, i18n("Explorar"), 246, 246, 248, 255);
  float x = ajustes_conteudo_x();
  txt_desenhar(t, x, 54.0f);
  { TxtLinha s = txt_linha(TXT_BODY, i18n("O que você viu, ligado pelo que as histórias têm em comum"),
                           180, 184, 198, 255);
    txt_desenhar(s, x, 128.0f); }
  if (mapa.carregando) {
    float pulso = ajustes_animacoes_reduzidas() ? 1.0f : 0.55f + 0.45f * sinf(tempo * 3.0f);
    TxtLinha c = txt_linha(TXT_CAPTION2, i18n("Cruzando histórias…"), 170, 176, 196, 255);
    txt_desenhar_alpha(c, x, 170.0f, pulso);
  }
  if (curiosidadeTxt[0]) {
    rotuloEspacado(i18n("CURIOSIDADE DO DIA"), EX_PAINEL_X, 62.0f, 1.0f);
    txt_bloco(TXT_CALLOUT, curiosidadeTxt, 236, 238, 244, EX_PAINEL_X, 98.0f, EX_PAINEL_W, 38.0f, 1.0f, 3);
  }
  { TxtLinha aj = txt_linha(TXT_CAPTION2, i18n("Setas   Navegar pelo céu   ·   OK   Abrir"),
                            140, 146, 162, 255);
    txt_desenhar(aj, EX_PAINEL_X, 1010.0f); }
}

// --- ciclo ------------------------------------------------------------------------

void explorar_iniciar(void) {
  nNos = nLinhas = 0;
  foco = 0;
  tempo = entrada = 0.0f;
  painelA = 0.0f;
  camX = camY = 0.0f;
  sair = pediuAbrir = abrindo = 0;
  girando = 0;
  sorteado = -1;
  mostrouPainel = -1;
  memset(&mapa, 0, sizeof mapa);
  mapaRev = 0;
  curiosidadeTxt[0] = 0;
  mapa_pedir();
  if (mapa_copiar(&mapa, &mapaRev)) montar();
}

void explorar_encerrar(void) {
  nNos = 0;
  pediuAbrir = 0;
  sair = 0;
}

void explorar_evento(const SDL_Event *e) {
  SDL_Keycode k;
  int v = -1;
  if (!e || e->type != SDL_KEYDOWN) return;
  k = e->key.keysym.sym;
  if (k == SDLK_ESCAPE || k == SDLK_AC_BACK || k == SDLK_BACKSPACE || k == SDLK_DELETE) { sair = 1; return; }
  if (girando) return;
  if (k == SDLK_LEFT)  { v = vizinho(-1, 0); if (v < 0) { sair = 1; return; } }
  if (k == SDLK_RIGHT) v = vizinho(1, 0);
  if (k == SDLK_UP)    v = vizinho(0, -1);
  if (k == SDLK_DOWN)  v = vizinho(0, 1);
  if (v >= 0) { foco = v; abrindo = 0; return; }
  if ((k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) && foco >= 0 && foco < nNos) {
    const ExNo *n = &nos[foco];
    if (n->tipo == NO_SORTE) girar();
    else abrirObra(obraDoNo(n));
  }
}

void explorar_atualizar(float dt, Uint32 agora) {
  int reduzida = ajustes_animacoes_reduzidas(), i;
  (void)agora;
  if (mapa_copiar(&mapa, &mapaRev)) {
    // O mapa cresce enquanto o TMDB responde; a entrada continua de onde
    // estava para nao reapresentar o ceu inteiro a cada semente nova.
    if (sorteado >= mapa.nSorte) sorteado = -1;
    montar();
  }
  tempo += reduzida ? 0.0f : dt;
  entrada += dt;
  if (reduzida) entrada = 10.0f;
  if (abrindo && !desc_titulo_buscando()) abrindo = 0;
  for (i = 0; i < nNos; i++) {
    float alvo = i == foco ? 1.0f : 0.0f;
    nos[i].foco = reduzida ? alvo : anim_mola(nos[i].foco, alvo, dt, 14.0f);
  }
  if (foco != mostrouPainel) { mostrouPainel = foco; if (!reduzida) painelA = 0.0f; }
  painelA = reduzida ? 1.0f : anim_mola(painelA, 1.0f, dt, 9.0f);
  if (foco >= 0 && foco < nNos) {
    // Deriva de camera: o ceu inclina de leve para o no em foco.
    float cx = (centroX - nos[foco].x) * 0.05f, cy = (centroY - nos[foco].y) * 0.05f;
    camX = reduzida ? cx : anim_mola(camX, cx, dt, 3.0f);
    camY = reduzida ? cy : anim_mola(camY, cy, dt, 3.0f);
  }
  if (girando) {
    float t;
    giroT += dt;
    if (reduzida || giroT >= EX_GIRO_S) { terminarGiro(); return; }
    // Desacelera como uma roleta: muitos cartazes no inicio, poucos no fim,
    // e o ultimo mostrado e o sorteado.
    t = 1.0f - powf(1.0f - giroT / EX_GIRO_S, 3.0f);
    giroMostra = ((giroFinal - 24 + (int)(t * 24.0f)) % mapa.nSorte + mapa.nSorte) % mapa.nSorte;
  }
}

void explorar_desenhar(Uint32 agora) {
  int reduzida = ajustes_animacoes_reduzidas(), i;
  (void)agora;
  desenharCeu();
  if (nNos == 0) {
    TxtLinha v = txt_linha(TXT_TITULO3, i18n("O céu aparece quando o catálogo carregar"), 236, 238, 244, 255);
    TxtLinha s = txt_linha(TXT_BODY, i18n("Assista a alguma coisa ou salve um título: cada história vira uma estrela."),
                           176, 180, 194, 255);
    desenharCabecalho();
    txt_desenhar(v, (NV_TELA_W - v.w) * 0.5f, 470.0f);
    txt_desenhar(s, (NV_TELA_W - s.w) * 0.5f, 546.0f);
    return;
  }
  desenharTemas();
  desenharLinhas(reduzida);
  for (i = 0; i < nNos; i++) if (i != foco) desenharNo(i, reduzida);
  if (foco >= 0 && foco < nNos) desenharNo(foco, reduzida);
  desenharTempo();
  desenharCabecalho();
  desenharPainel();
}

int explorar_quer_sair(void) {
  int v = sair;
  sair = 0;
  return v;
}

int explorar_pediu_abrir(int *indice) {
  if (!pediuAbrir) return 0;
  pediuAbrir = 0;
  if (indice) *indice = pediuIndice;
  return 1;
}
