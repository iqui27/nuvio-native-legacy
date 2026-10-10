// O CARTAO DE NOVIDADES DA VERSAO ATUAL. Ver novidades_cartao.h.
//
// O MOTOR e o cartao da 2.0.2 (novidades202.c, aprovado pelo dono) com o que
// era de uma versao so tirado para fora: o titulo, o subtitulo, os grupos e as
// cenas da previa vem do conteudo (src/novidades/<versao>.inc, incluido mais
// abaixo). O resto e o mesmo: as medidas, o vidro, a previa com legenda e
// tracos, a cascata de entrada, as duas paginas de botoes, "Apoie o projeto"
// com os QRs, o ponteiro do Magic Remote, 320/180 ms de entrada e saida e as
// Animacoes reduzidas.
//
// ALTURAS: mede a quebra real ao abrir/trocar de idioma, com o teto declarado
// pelo conteudo. Guarda as medidas antes de paginar: o orcamento de texturas
// dos primeiros quadros nao muda as alturas nem empurra a lista ao rodape.
// CUSTO DE GPU: nenhuma passada de desfoque; artes do pacote, veus e
// retangulos pequenos (a copia desfocada do gfx_desfocado e feita uma vez).
#include "novidades_cartao.h"
#include "novidades201.h"
#include "novidades202.h"
#include "apoio.h"
#include "dvtela.h"
#include "ajustes.h"
#include "anim.h"
#include "botoes.h"
#include "dados.h"
#include "gfx.h"
#include "horafmt.h"
#include "idioma.h"
#include "layout.h"
#include "plrui.h"
#include "ponteiro.h"
#include "tex_cache.h"
#include "text.h"
#define NV_ESCALA_TELA_ATIVA
#include "escala.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define N_W         1720.0f
#define N_H          960.0f
#define N_X         ((NV_TELA_W - N_W) * 0.5f)
#define N_Y         ((NV_TELA_H - N_H) * 0.5f)
#define N_PAD         56.0f
#define N_RAIO        36.0f
#define COMPACTO_W   1160.0f
#define COMPACTO_H    912.0f   // teto; cada pagina encolhe ate a altura da lista
#define COMPACTO_PAD   40.0f
#define PV_W         760.0f
#define PV_H         (N_H - 2.0f * N_PAD)
#define PV_RAIO       28.0f
#define COL_GAP       72.0f
#define TX           (N_X + N_PAD + PV_W + COL_GAP)
#define TW           (N_X + N_W - N_PAD - TX)
#define ICONE         44.0f
#define ABRIR_MS     320.0f
#define FECHAR_MS    180.0f
#define PAGINA_MS    420.0f
// A previa: 4,2 s por cena (o conteudo pode pedir mais), 0,55 s de passagem.
#define CICLO_S        4.2f
#define TROCA_S        0.55f

// ------------------------------------------------------------ o conteudo
// Uma cena da previa: desenha em `p` (o retangulo da previa) no segundo `t`
// da cena, com alfa `a`. Com Animacoes reduzidas desenha o estado final.
typedef void (*NovCenaFn)(GfxRect p, float t, float a);

typedef struct {
  const char *grupo;     // abre um grupo (o kicker); NULL = segue o anterior
  const char *icone, *nome;
  const char *frase;     // a frase, apagada, embaixo do nome
  const char *frase2;    // opcional, logo abaixo
  int linhas, linhas2;   // teto de linhas de cada frase (0 = 1)
  unsigned plataformas;  // NOV_* (0 = todas)
} NovItem;

typedef struct {
  const char *kicker, *linha;   // a legenda no pe da previa
  NovCenaFn desenhar;
  float duracao;                // s (0 = CICLO_S)
  unsigned plataformas;         // NOV_* (0 = todas)
} NovCena;

typedef struct {
  const char *versao;     // "2.0.4" (o titulo e "Novidades da %s")
  const char *arquivo;    // a marca de "ja visto" (dados_*)
  const char *subtitulo;  // uma frase embaixo do titulo
  const NovItem *itens;
  int nItens;
  const NovCena *cenas;
  int nCenas;
  const int *artes;       // as artes do pacote (deploy/app/art/NN.jpg) que as cenas usam
  int nArtes;
} NovConteudo;

#define NOV_ARTES_MAX 6
#define NOV_ITENS_MAX 24
#define NOV_CENAS_MAX 6
#define NOV_PAGINAS_MAX 4

static int   aberto, decidido, pagina, foco = 1;
static float entrada, pag, relogio;
static char  dirArte[512] = "deploy/app/art";
static char  arte[NOV_ARTES_MAX][600];
static unsigned plataformaTeste;
// Folga entre o fim da lista e o topo do rodape (a pior pagina desenhada) e
// quantas frases precisaram de reticencias (testes).
static float folgaLista = 999.0f;
static int   frasesCortadas, itensDesenhados, medirCortes;

static unsigned plataforma(void) {
  if (plataformaTeste) return plataformaTeste;
#if defined(NV_WEBOS)
  return NOV_LG;
#elif defined(NV_TPK)
  return NOV_TPK;
#elif defined(__EMSCRIPTEN__)
  return NOV_WGT;
#elif defined(NV_ANDROID)
  return NOV_ANDROID;
#else
  return NOV_OUTRAS;
#endif
}
static int vale(unsigned mask) { return !mask || (mask & plataforma()); }

// ------------------------------------------------------------- pecas comuns
// (tambem das cenas do conteudo)
static float sai3(float t) { float u = 1.0f - anim_clamp(t, 0.0f, 1.0f); return 1.0f - u * u * u; }
static float janela(float t, float ini, float dur) { return anim_clamp((t - ini) / dur, 0.0f, 1.0f); }
// As cenas de uma versao podem nao usar todas as pecas.
__attribute__((unused)) static float lerp(float a, float b, float t) { return a + (b - a) * t; }

// O vidro escuro das ilhas: miolo quase opaco e um fio claro na borda.
__attribute__((unused)) static void vidro(GfxRect r, float raioPx, float a) {
  gfx_cor(r, raioPx / r.h, 0.070f, 0.073f, 0.082f, 0.90f * a);
  gfx_anel(r, raioPx / r.h, 1.0f, 1, 1, 1, 0.09f * a);
}

static const char *artePorNumero(int n);

// Uma arte do pacote em `r`, sempre em cover (a previa e mais alta que a
// arte; o "contain" do GFX_CARD deixaria faixas vazias).
static void novArte(int numero, GfxRect r, float raioPx, float a) {
  const char *url = artePorNumero(numero);
  GLuint t;
  if (a <= 0.004f || !url) return;
  t = tex_obter_hero(url);
  if (!t) return;
  gfx_tex_aspect_atual = tex_aspecto(url);
  if (gfx_tex_aspect_atual <= 0.0f) gfx_tex_aspect_atual = 16.0f / 9.0f;
  gfx_card_forcar_cover_atual = 1.0f;
  gfx_rect(r, t, GFX_CARD, 0, 0, 0, raioPx / r.h, 1, 1, 1, a);
  gfx_card_forcar_cover_atual = 0.0f;
  gfx_tex_aspect_atual = 0.0f;
}

// A mesma arte tao desfocada que so fica a cor (a copia 96x54 do
// gfx_desfocado, gerada uma vez): o fundo das telas de espera do player.
__attribute__((unused)) static void novArteDesfocada(int numero, GfxRect r, float raioPx, float a) {
  const char *url = artePorNumero(numero);
  GLuint t, tb;
  if (a <= 0.004f || !url) return;
  t = tex_obter_hero(url);
  if (!t) return;
  tb = gfx_desfocado(t, url);
  if (!tb) { novArte(numero, r, raioPx, a); return; }
  gfx_rect(r, tb, GFX_CARD, 0, 0, 0, raioPx / r.h, 1, 1, 1, a);
}

// ======================================================= O CONTEUDO DA VERSAO
// Para a proxima versao: um src/novidades/<versao>.inc novo, trocado aqui.
#include "novidades/204.inc"
#define CT NOV_ATUAL

static const char *artePorNumero(int n) {
  int i;
  for (i = 0; i < CT.nArtes && i < NOV_ARTES_MAX; i++)
    if (CT.artes[i] == n) return arte[i][0] ? arte[i] : NULL;
  return NULL;
}

void novcartao_dir(const char *d) {
  int i;
  if (d && d[0]) snprintf(dirArte, sizeof dirArte, "%s", d);
  for (i = 0; i < CT.nArtes && i < NOV_ARTES_MAX; i++)
    snprintf(arte[i], sizeof arte[i], "%s/%02d.jpg", dirArte, CT.artes[i]);
}

static void pedirArtes(void) {
  int i;
  if (CT.nArtes > 0 && !arte[0][0]) novcartao_dir(NULL);
  for (i = 0; i < CT.nArtes && i < NOV_ARTES_MAX; i++) if (arte[i][0]) tex_obter_hero(arte[i]);
}

int novcartao_previa_pronta(void) {
  int i;
  for (i = 0; i < CT.nArtes && i < NOV_ARTES_MAX; i++) if (arte[i][0] && !tex_obter_hero(arte[i])) return 0;
  return 1;
}

// ------------------------------------------------------- itens e paginas
// Medidas fixas (a 2.0.2): nome 34 + uma linha de frase 30.
#define NOME_H     34.0f
#define FRASE_LH   30.0f
#define FRASE_GAP   6.0f
#define GRUPO_H    34.0f
#define VAO_MIN    12.0f
#define VAO_MAX    24.0f
#define FOLGA_MIN  28.0f   // entre o fim da lista e o rodape
#define TITULO_H   60.0f
#define SUB_H      36.0f

// Os itens desta plataforma, na ordem; o kicker vai para o primeiro visivel
// do grupo (um grupo cujo primeiro item e de outra plataforma nao perde o
// titulo). `pg` = pagina de cada um.
static int nVis, vis[NOV_ITENS_MAX], pgVis[NOV_ITENS_MAX], nPagNov = 1;
static const char *kickerVis[NOV_ITENS_MAX];
static int nCen, cen[NOV_CENAS_MAX];
static float alturaPag[NOV_PAGINAS_MAX];
static int linhasItem[NOV_ITENS_MAX], linhasItem2[NOV_ITENS_MAX];
static int idiomaMontado = -1;
static unsigned plataformaMontada;
static float vaoMin = 999.0f, vaoMax;
static int discordDesenhado;

static int compacto(void) { return CT.nCenas == 0; }
static float padLista(void) { return compacto() ? COMPACTO_PAD : N_PAD; }
static float textoX(void) { return compacto() ? (NV_TELA_W - COMPACTO_W) * 0.5f + COMPACTO_PAD : TX; }
static float textoW(void) { return compacto() ? COMPACTO_W - 2.0f * COMPACTO_PAD : TW; }

static int linhasDe(int l) { return l > 1 ? l : 1; }
static float alturaItem(const NovItem *it) {
  int i = (int)(it - CT.itens);
  float h = NOME_H + FRASE_LH * (float)linhasItem[i];
  if (it->frase2) h += FRASE_GAP + FRASE_LH * (float)linhasItem2[i];
  return h;
}

// Mesma quebra do desenho, sem criar texturas. Sem fonte (testes de estado),
// conserva o teto; a proxima abertura mede novamente.
static int medirLinhas(const char *s, int teto) {
  int n;
  teto = linhasDe(teto);
  if (teto == 1 || !txt_largura(TXT_CAPTION, "M")) return teto;
  n = (int)(txt_bloco_corta(TXT_CAPTION, i18n(s), 0, 0, 0, 0, 0,
                            textoW() - ICONE - 22.0f, FRASE_LH, 0, teto) / FRASE_LH);
  return n > 0 ? n : 1;
}

// Do topo do cartao ao primeiro kicker, e ate onde a lista pode ir.
static float topoLista(void) { return padLista() - 6.0f + TITULO_H + (CT.subtitulo ? SUB_H : 0.0f) + 34.0f; }
static float fimLista(void) { return (compacto() ? COMPACTO_H : N_H) - padLista() - BOTAO_H_PRIMARIO - FOLGA_MIN; }

static void montar(void) {
  int i, pg = 0, usado0 = 1;
  const char *kick = NULL;
  int kickUsado = 1;
  float cap = fimLista() - topoLista(), usado = 0.0f;
  if (idiomaMontado == ajustes_idioma() && plataformaMontada == plataforma()) return;
  idiomaMontado = ajustes_idioma();
  plataformaMontada = plataforma();
  nVis = 0;
  for (i = 0; i < CT.nItens && nVis < NOV_ITENS_MAX; i++) {
    if (CT.itens[i].grupo) { kick = CT.itens[i].grupo; kickUsado = 0; }
    if (!vale(CT.itens[i].plataformas)) continue;
    linhasItem[i] = medirLinhas(CT.itens[i].frase, CT.itens[i].linhas);
    linhasItem2[i] = CT.itens[i].frase2 ? medirLinhas(CT.itens[i].frase2, CT.itens[i].linhas2) : 0;
    kickerVis[nVis] = kickUsado ? NULL : kick;
    kickUsado = 1;
    vis[nVis++] = i;
  }
  // Paginas: o grupo inteiro na mesma pagina, contando os vaos minimos (entre
  // grupos o desenho poe vao + 0,8 vao).
  i = 0;
  while (i < nVis) {
    int j = i + 1, k;
    float hg = (kickerVis[i] ? GRUPO_H : 0.0f) + alturaItem(&CT.itens[vis[i]]);
    while (j < nVis && !kickerVis[j]) { hg += VAO_MIN + alturaItem(&CT.itens[vis[j]]); j++; }
    if (!usado0 && usado + 1.8f * VAO_MIN + hg > cap && pg < NOV_PAGINAS_MAX - 2) { pg++; usado0 = 1; usado = 0.0f; }
    usado += (usado0 ? 0.0f : 1.8f * VAO_MIN) + hg;
    usado0 = 0;
    for (k = i; k < j; k++) pgVis[k] = pg;
    i = j;
  }
  nPagNov = nVis ? pgVis[nVis - 1] + 1 : 1;
  if (compacto()) {
    // Reserva exatamente as medidas guardadas para o desenho.
    for (i = 0; i < nPagNov; i++) {
      int k, n = 0;
      float h = topoLista() + FOLGA_MIN + BOTAO_H_PRIMARIO + COMPACTO_PAD;
      for (k = 0; k < nVis; k++) if (pgVis[k] == i) {
        if (n++) h += VAO_MIN * (kickerVis[k] ? 1.8f : 1.0f);
        h += alturaItem(&CT.itens[vis[k]]) + (kickerVis[k] ? GRUPO_H : 0.0f);
      }
      alturaPag[i] = h;
    }
  }
  if (pagina > nPagNov) { pagina = nPagNov; pag = (float)pagina; }
  nCen = 0;
  for (i = 0; i < CT.nCenas && nCen < NOV_CENAS_MAX; i++)
    if (CT.cenas[i].desenhar && vale(CT.cenas[i].plataformas)) cen[nCen++] = i;
}

static int paginaApoio(void) { return nPagNov; }

int novcartao_aberto(void) { return aberto; }
int novcartao_pagina(void) { return pagina; }
int novcartao_paginas(void) { montar(); return nPagNov + 1; }
int novcartao_foco(void) { return foco; }
int novcartao_cenas(void) { montar(); return nCen; }
int novcartao_itens_visiveis(void) { montar(); return nVis; }
const char *novcartao_versao(void) { return CT.versao; }
const char *novcartao_arquivo(void) { return CT.arquivo; }
void novcartao_teste_relogio(float s) { relogio = s; }
void novcartao_teste_esquecer(void) { decidido = 0; aberto = 0; }
void novcartao_teste_plataforma(unsigned p) { plataformaTeste = p; idiomaMontado = -1; montar(); }
float novcartao_teste_folga(void) { return folgaLista; }
int novcartao_teste_cortadas(void) { return frasesCortadas; }
int novcartao_teste_desenhados(void) { return itensDesenhados; }
float novcartao_teste_altura_item(int i) { return alturaItem(&CT.itens[vis[i]]); }
float novcartao_teste_vao_min(void) { return vaoMin; }
float novcartao_teste_vao_max(void) { return vaoMax; }
int novcartao_teste_discord(void) { return discordDesenhado; }
void novcartao_teste_medir(int sim) { medirCortes = sim; }

static float duracaoCena(int i) { float d = CT.cenas[cen[i]].duracao; return d > 0.0f ? d : CICLO_S; }
float novcartao_teste_inicio_cena(int i) {
  float t = 0.0f;
  int k;
  montar();
  for (k = 0; k < i && k < nCen; k++) t += duracaoCena(k);
  return t;
}

static void comecar(float e) {
  idiomaMontado = -1;
  montar();
  aberto = 1;
  decidido = 1;
  pagina = 0;
  pag = 0.0f;
  foco = 1;
  entrada = e;
  relogio = 0.0f;
  folgaLista = 999.0f;
  pedirArtes();
}

void novcartao_abrir(void) { comecar(0.0f); }

// Os cartoes antigos ficam vistos: nenhum abre depois deste (nem antes, no
// mesmo quadro — o app.c chama esta decisao primeiro).
static void marcarAntigos(void) {
  static const char *const ANTIGOS[] = { N202_ARQ, N201_ARQ };
  int i;
  for (i = 0; i < (int)(sizeof ANTIGOS / sizeof *ANTIGOS); i++) {
    char *s = dados_ler(ANTIGOS[i]);
    if (s) free(s);
    else dados_gravar(ANTIGOS[i], "1\n");
  }
}

static void primeiraVez(void) {
  char *s;
  if (decidido) return;
  decidido = 1;
  marcarAntigos();
  s = dados_ler(CT.arquivo);
  if (s) { free(s); return; }
  // Sem a marca do guia da 2.0 o guia abre agora (novidades20_primeira_vez,
  // no mesmo quadro): ele ja conta o app inteiro, e um segundo cartao em
  // seguida seria demais. Fica visto.
  s = dados_ler("novidades-20-guia.txt");
  if (!s) { dados_gravar(CT.arquivo, "1\n"); return; }
  free(s);
  comecar(0.0f);
}

void novcartao_decidir(int homePronta, int playerAberto, int detalheAberto) {
  if (!homePronta || playerAberto || detalheAberto) return;
  primeiraVez();
}

static void fechar(void) {
  aberto = 0;
  dados_gravar(CT.arquivo, "1\n");
}

static void irPagina(int p) {
  pagina = p;
  foco = 1;
  if (ajustes_animacoes_reduzidas()) pag = (float)p;
}

// Foco 0 = o botao da esquerda (Agora nao / Voltar), 1 = o principal
// (Continuar / Concluir).
static void ok(void) {
  if (foco == 0) {
    if (pagina == 0) fechar();
    else irPagina(pagina - 1);
  } else {
    if (pagina >= paginaApoio()) fechar();
    else irPagina(pagina + 1);
  }
}

void novcartao_evento(const SDL_Event *e) {
  SDL_Keycode k;
  if (!aberto || e->type != SDL_KEYDOWN) return;
  montar();
  k = e->key.keysym.sym;
  if (k == SDLK_LEFT)  { if (foco > 0) foco--; return; }
  if (k == SDLK_RIGHT) { if (foco < 1) foco++; return; }
  if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) { ok(); return; }
  if (k == SDLK_AC_BACK || k == SDLK_ESCAPE || k == SDLK_BACKSPACE ||
      k == SDLK_DELETE || e->key.keysym.scancode == NV_SCANCODE_BACK) {
    if (pagina) irPagina(pagina - 1);
    else fechar();
  }
}

void novcartao_atualizar(float dt, Uint32 agora) {
  int red = ajustes_animacoes_reduzidas();
  (void)agora;
  if (!aberto && entrada < 0.002f) { entrada = 0.0f; return; }
  if (aberto) {
    montar();
    pedirArtes();
    if (novcartao_previa_pronta()) relogio += dt;
  }
  if (red) { entrada = aberto ? 1.0f : 0.0f; pag = (float)pagina; return; }
  entrada = anim_rampa(entrada, aberto ? 1.0f : 0.0f, dt, aberto ? ABRIR_MS : FECHAR_MS);
  pag = anim_rampa(pag, (float)pagina, dt, PAGINA_MS);
}

// ------------------------------------------------------------------ a previa
static void cena(int i, GfxRect p, float t, float a) {
  if (a <= 0.004f || i < 0 || i >= nCen) return;
  CT.cenas[cen[i]].desenhar(p, t, a);
}

static void desenhaPrevia(float x, float y, float a) {
  GfxRect p = { x, y, PV_W, PV_H };
  float total = 0.0f, ini = 0.0f, dentro, dur, e;
  int i, agora = 0, antes;
  gfx_cor(p, PV_RAIO / p.h, 0.055f, 0.058f, 0.068f, a);
  if (nCen <= 0) { gfx_anel(p, PV_RAIO / p.h, 1.0f, 1, 1, 1, 0.07f * a); return; }
  for (i = 0; i < nCen; i++) total += duracaoCena(i);
  { float tc = fmodf(relogio, total);
    for (i = 0; i < nCen; i++) {
      if (tc < ini + duracaoCena(i) || i == nCen - 1) { agora = i; break; }
      ini += duracaoCena(i);
    }
    dentro = tc - ini; }
  dur = duracaoCena(agora);
  antes = (agora + nCen - 1) % nCen;
  e = (relogio < duracaoCena(0) || ajustes_animacoes_reduzidas()) ? 1.0f : sai3(dentro / TROCA_S);
  // A cena que sai fica parada no fim dela; a que entra comeca do zero.
  if (e < 1.0f) cena(antes, p, duracaoCena(antes), a * (1.0f - e));
  cena(agora, p, dentro, a * e);

  // A legenda da cena: veu escuro no pe da previa, kicker e uma linha. Troca
  // em sequencia (a velha sai na primeira metade da passagem, a nova entra na
  // segunda): duas frases cruzadas viram borrao.
  { GfxRect pe = { p.x, p.y + p.h - 230.0f, p.w, 230.0f };
    float fOld = e < 1.0f ? 1.0f - anim_clamp(e / 0.45f, 0.0f, 1.0f) : 0.0f;
    float fNew = e < 1.0f ? anim_clamp((e - 0.45f) / 0.55f, 0.0f, 1.0f) : 1.0f;
    int k;
    gfx_rect(pe, 0, GFX_VEU_CARD, 0, 0, 0, PV_RAIO / pe.h, 0.03f, 0.03f, 0.035f, a);
    for (k = 0; k < 2; k++) {
      int c = k ? agora : antes;
      float f = k ? fNew : fOld, dy = (1.0f - f) * (k ? 8.0f : -6.0f);
      const char *linha = i18n(CT.cenas[cen[c]].linha);
      TxtLinha l;
      if (f <= 0.004f) continue;
      ajustes_ui_kicker(i18n(CT.cenas[cen[c]].kicker), p.x + 44.0f, p.y + p.h - 112.0f + dy, f * a);
      l = txt_linha_corta(TXT_V2_LN_B, linha, 243, 242, 239, 255, p.w - 88.0f);
      txt_desenhar_alpha(l, p.x + 44.0f, p.y + p.h - 84.0f + dy, f * a);
      if (k && txt_largura(TXT_V2_LN_B, linha) > (int)(p.w - 88.0f)) frasesCortadas++;
    } }

  // Os tracos do ciclo, no pe a direita: o da cena enche no tempo dela. Com
  // uma cena so nao ha ciclo para mostrar.
  if (nCen > 1) {
    for (i = 0; i < nCen; i++) {
      // Na altura do kicker (curto): a frase de baixo tem a largura toda.
      GfxRect tr = { p.x + p.w - 44.0f - (float)(nCen - i) * 40.0f + 8.0f, p.y + p.h - 106.0f, 32.0f, 4.0f };
      gfx_cor(tr, 0.5f, 1, 1, 1, 0.22f * a);
      if (i == agora) {
        float pr = ajustes_animacoes_reduzidas() ? 1.0f : dentro / dur;
        tr.w *= anim_clamp(pr, 0.0f, 1.0f);
        if (tr.w > 1.0f) gfx_cor(tr, 0.5f, 1, 1, 1, 0.9f * a);
      }
    } }
  gfx_anel(p, PV_RAIO / p.h, 1.0f, 1, 1, 1, 0.07f * a);
}

// ------------------------------------------------------------- as mudancas
static float ox;   // deslocamento horizontal da pagina (a troca de pagina desliza)

static float frase(const char *s, int linhas, float x, float y, float w, float a) {
  const char *t = i18n(s);
  if (linhas <= 1) {
    TxtLinha f = txt_linha_corta(TXT_CAPTION, t, 186, 192, 204, 255, w);
    txt_desenhar_alpha(f, x, y, 0.92f * a);
    if (txt_largura(TXT_CAPTION, t) > (int)w) frasesCortadas++;
    return FRASE_LH;
  }
  float h = txt_bloco_corta(TXT_CAPTION, t, 186, 192, 204, x, y, w, FRASE_LH, 0.92f * a, linhas);
  // So nos testes: quantas linhas a frase pede sem limite (desenho invisivel).
  if (medirCortes && txt_bloco(TXT_CAPTION, t, 0, 0, 0, -4000.0f, -4000.0f, w, FRASE_LH, 0.0f, 0) >
                     FRASE_LH * (float)linhas + 0.5f)
    frasesCortadas++;
  return h;
}

static float item(const NovItem *it, float y, float a) {
  float tx = textoX() + ox + ICONE + 22.0f, tw = textoW() - (ICONE + 22.0f);
  GfxRect d = { textoX() + ox, y - 2.0f, ICONE, ICONE };
  const char *nome = i18n(it->nome);
  TxtLinha n;
  gfx_cor(d, 0.5f, 1, 1, 1, 0.075f * a);
  gfx_icone((GfxRect){ d.x + 11.0f, d.y + 11.0f, 22.0f, 22.0f }, it->icone, 0.93f, 0.93f, 0.95f, a);
  n = txt_linha_corta(TXT_ILHA_NOME, nome, 246, 247, 250, 255, tw);
  txt_desenhar_alpha(n, tx, y, a);
  if (txt_largura(TXT_ILHA_NOME, nome) > (int)tw) frasesCortadas++;
  int i = (int)(it - CT.itens);
  float h = NOME_H + frase(it->frase, linhasItem[i], tx, y + NOME_H, tw, a);
  if (it->frase2)
    h += FRASE_GAP + frase(it->frase2, linhasItem2[i], tx, y + h + FRASE_GAP, tw, a);
  return h;
}

static float grupo(const char *g, float y, float a) {
  float kw = ajustes_ui_kicker(i18n(g), textoX() + ox, y, a);
  gfx_cor((GfxRect){ textoX() + ox + kw + 16.0f, y + 8.0f, textoW() - kw - 16.0f, 1.0f }, 0, 1, 1, 1, 0.08f * a);
  return GRUPO_H;
}

static void cabecalho(float y0, float a, float dx) {
  float y = y0 + padLista() - 6.0f;
  char t[64];
  snprintf(t, sizeof t, i18n("Novidades da %s"), CT.versao);
  { TxtLinha l = txt_linha_corta(TXT_NOV_TITULO, t, 248, 249, 252, 255, textoW());
    txt_desenhar_alpha(l, textoX() + dx, y, a);
    if (txt_largura(TXT_NOV_TITULO, t) > (int)textoW()) frasesCortadas++; }
  if (CT.subtitulo) {
    const char *s = i18n(CT.subtitulo);
    TxtLinha l = txt_linha_corta(TXT_V2_26, s, 200, 205, 215, 255, textoW());
    txt_desenhar_alpha(l, textoX() + dx, y + TITULO_H + 6.0f, 0.9f * a);
    if (txt_largura(TXT_V2_26, s) > (int)textoW()) frasesCortadas++;
  }
}

static void paginaNovidades(int pg, float y0, float a, float dx) {
  float y = y0 + topoLista(), vao, total = 0.0f, limite = y0 + (compacto() ? alturaPag[pg] - COMPACTO_PAD - BOTAO_H_PRIMARIO - FOLGA_MIN : fimLista());
  int i, g = 0, ng = 0, n = 0, k = 0;
  float fimAnterior = 0.0f;
  if (a <= 0.004f) return;
  cabecalho(y0, a, dx);
  for (i = 0; i < nVis; i++) {
    if (pgVis[i] != pg) continue;
    n++;
    total += alturaItem(&CT.itens[vis[i]]);
    if (kickerVis[i]) { ng++; total += GRUPO_H; }
  }
  if (!n) return;
  { float div = (float)(n - 1) + 0.8f * (float)(ng > 0 ? ng - 1 : 0);
    vao = div > 0.0f ? (limite - y - total) / div : VAO_MAX; }
  if (vao > VAO_MAX) vao = VAO_MAX;
  if (vao < VAO_MIN) vao = VAO_MIN;
  ox = dx;
  for (i = 0; i < nVis; i++) {
    const NovItem *it;
    float local, al, sobe, h;
    if (pgVis[i] != pg) continue;
    it = &CT.itens[vis[i]];
    h = alturaItem(it);
    // Cascata de entrada: cada linha sobe 12 px, 45 ms depois da anterior.
    local = ajustes_animacoes_reduzidas() ? 1.0f
          : anim_clamp((anim_suave(entrada) - 0.04f * (float)k) * 2.6f, 0.0f, 1.0f);
    al = a * local;
    sobe = (1.0f - local) * 12.0f;
    if (kickerVis[i]) {
      if (g++) y += vao * 0.8f;
      // Meio pixel de folga: o vao e calculado para encher ate `limite`, e a
      // soma em float pode passar dele por um fio.
      if (y + GRUPO_H + h > limite + 0.5f) break;
      y += grupo(kickerVis[i], y + sobe, al);
    }
    if (y + h > limite + 0.5f) break;
    if (medirCortes && k && !kickerVis[i]) {
      float v = y + sobe - fimAnterior;
      if (v < vaoMin) vaoMin = v;
      if (v > vaoMax) vaoMax = v;
    }
    fimAnterior = y + sobe + item(it, y + sobe, al);
    itensDesenhados++;
    y += h;
    k++;
    if (k < n) y += vao;
  }
  { float f = limite + FOLGA_MIN - y;
    if (f < folgaLista) folgaLista = f; }
  ox = 0.0f;
}

static void paginaApoioDesenho(float y0, float a, float dx) {
  float y = y0 + padLista() - 6.0f, x = textoX() + dx;
  int n = apoio_n(), i;
  if (a <= 0.004f) return;
  { TxtLinha l = txt_linha_corta(TXT_NOV_TITULO, i18n("Apoie o projeto"), 248, 249, 252, 255, textoW());
    txt_desenhar_alpha(l, x, y, a);
    y += (float)l.h + 22.0f; }
  y += txt_bloco(TXT_V2_26, i18n("O Nuvio Legacy é gratuito. Se ele te ajuda e você quiser apoiar quem faz o app, aponte a câmera do celular para um dos códigos."),
                 200, 205, 215, x, y, textoW(), 36.0f, 0.9f * a, 4);
  y += 44.0f;
  { float lado = 300.0f, gap = 44.0f;
    float comunidadeX = x + textoW() - lado;
    ajustes_ui_kicker(i18n("Comunidade"), comunidadeX, y - GRUPO_H, a);
    gfx_cor((GfxRect){ comunidadeX - 68.0f, y - GRUPO_H, 1, lado + 174.0f },
            0, 1, 1, 1, 0.08f * a);
    for (i = 0; i <= n; i++) {
      int q = i < n ? apoio_qual(i) : APOIO_DISCORD;
      float qx = i < n ? x + (float)i * (lado + gap) : comunidadeX;
      TxtLinha u = txt_linha_corta(TXT_V2_18, apoio_url_curta(q), 243, 242, 239, 255, lado + gap * 0.8f);
      // A luz atras do cartao claro: a peca mais importante da pagina sem
      // gritar (sombra, nao borda colorida).
      gfx_rect((GfxRect){ qx - 18.0f, y - 10.0f, lado + 36.0f, lado + 40.0f }, 0, GFX_SOMBRA, 0.8f, 0, 0, 0.2f,
               0, 0, 0, 0.5f * a);
      int desenhou = apoio_qr(q, qx, y, lado, a);
      if (q == APOIO_DISCORD) discordDesenhado = desenhou;
      // O selo (Ko-fi oficial; o do Patreon no mesmo formato) e o endereco,
      // centrados embaixo do codigo.
      apoio_rotulo(q, qx + lado * 0.5f, y + lado + 20.0f, 72.0f, 1, a);
      txt_desenhar_alpha(u, qx + (lado - (float)u.w) * 0.5f, y + lado + 20.0f + 72.0f + 10.0f, 0.6f * a);
    }
    y += lado + 20.0f + 72.0f + 10.0f + 24.0f + 34.0f;
    txt_bloco(TXT_CAPTION, i18n("Dúvidas, bugs e builds de teste."),
              186, 192, 204, comunidadeX, y, lado, 28.0f, 0.92f * a, 3); }
  { float ar, ag, ab;
    ajustes_acento_marca(&ar, &ag, &ab);
    gfx_icone((GfxRect){ x, y + 1.0f, 22.0f, 22.0f }, "aj_heart", ar, ag, ab, 0.9f * a);
    txt_bloco(TXT_CAPTION, i18n("É opcional e nada muda no app. Fica também em Ajustes › Sobre e ajuda."),
              186, 192, 204, x + 34.0f, y, textoW() - 470.0f, 28.0f, 0.92f * a, 3); }
}

static void ptFoco(int b, int nada) { (void)nada; foco = b; }

static void rodape(float y0, float altura, float a) {
  float yBase = y0 + altura - padLista();
  const char *rot[2];
  float w[2], xd = textoX() + textoW(), tracos;
  int i, nPag = nPagNov + 1;
  rot[0] = i18n(pagina ? "Voltar" : "Agora não");
  rot[1] = i18n(pagina >= paginaApoio() ? "Concluir" : "Continuar");
  for (i = 0; i < 2; i++) w[i] = botao_largura(rot[i], NULL, i == 1);
  for (i = 1; i >= 0; i--) {
    int prim = i == 1;
    GfxRect r;
    r.w = w[i];
    r.h = prim ? BOTAO_H_PRIMARIO : BOTAO_H_SECUNDARIO;
    r.x = xd - r.w;
    r.y = yBase - BOTAO_H_PRIMARIO * 0.5f - r.h * 0.5f;
    botao_pilula(r, rot[i], NULL, foco == i ? 1.0f : 0.0f, prim, 0, a);
    if (aberto) ponteiro_alvo(r.x, r.y, r.w, r.h, ptFoco, NULL, i, 0);
    xd = r.x - BOTAO_GAP;
  }
  // As paginas, como tracos no comeco do rodape. Somem quando os botoes
  // (rotulos longos, japones) chegam ate eles.
  tracos = 40.0f + (float)(nPag - 1) * (14.0f + 10.0f);
  if (xd + BOTAO_GAP > textoX() + tracos + 24.0f)
  { float x = textoX(), yc = yBase - BOTAO_H_PRIMARIO * 0.5f;
    for (i = 0; i < nPag; i++) {
      float on = 1.0f - anim_clamp(fabsf(pag - (float)i), 0.0f, 1.0f), tw = 14.0f + 26.0f * on;
      gfx_cor((GfxRect){ x, yc - 3.0f, tw, 6.0f }, 0.5f, 1, 1, 1, (0.22f + 0.68f * on) * a);
      x += tw + 10.0f;
    } }
}

// A pagina `i` vista do ponto `pag` da troca: a que sai desliza para a
// esquerda e some cedo; a que chega vem da direita e aparece depois (as duas
// nunca cruzam inteiras). Igual a 2.0.2 com duas paginas.
static void visPagina(int i, float *alfa, float *dx) {
  float t = pag - (float)i;
  if (t >= 0.0f) {
    *alfa = 1.0f - anim_clamp(t * 1.6f, 0.0f, 1.0f);
    *dx = -28.0f * sai3(t);
  } else {
    float u = 1.0f + t;
    *alfa = anim_clamp((u - 0.35f) / 0.65f, 0.0f, 1.0f);
    *dx = 28.0f * (1.0f - sai3(u));
  }
}

static void desenharCorpo(void) {
  float a = anim_suave(entrada), y0, altura = N_H;
  float largura = compacto() ? COMPACTO_W : N_W;
  float x0 = (NV_TELA_W - largura) * 0.5f;
  int i;
  if (entrada < 0.002f) return;
  gfx_cor((GfxRect){ 0, 0, NV_TELA_W, NV_TELA_H }, 0.0f, 0, 0, 0, 0.78f * entrada);
  if (compacto()) {
    int p = (int)pag, q = p < paginaApoio() ? p + 1 : p;
    float hp = p < paginaApoio() ? alturaPag[p] : N_H;
    float hq = q < paginaApoio() ? alturaPag[q] : N_H;
    float t = pag - (float)p;
    // A pagina maior precisa caber enquanto estiver visivel (visPagina).
    t = hp > hq ? anim_clamp((t - 0.625f) / 0.375f, 0, 1)
                : anim_clamp(t / 0.35f, 0, 1);
    altura = lerp(hp, hq, t);
  }
  y0 = (NV_TELA_H - altura) * 0.5f + (1.0f - a) * 28.0f;
  // O cartao: o vidro escuro das ilhas, um fio de luz e uma luz fria no alto.
  { GfxRect c = { x0, y0, largura, altura };
    float ar, ag, ab;
    ajustes_acento(&ar, &ag, &ab);
    gfx_rect((GfxRect){ c.x - 40.0f, c.y - 20.0f, c.w + 80.0f, c.h + 70.0f }, 0, GFX_SOMBRA, 1.0f, 0, 0, 0.12f,
             0, 0, 0, 0.6f * a);
    gfx_cor(c, N_RAIO / altura, 0.052f, 0.055f, 0.064f, 0.96f * a);
    gfx_luz_canto(c, N_RAIO / altura, c.w * 0.78f, -c.h * 0.25f, c.h * 0.95f, ar, ag, ab, 0.10f * a);
    gfx_anel(c, N_RAIO / altura, 1.2f, 1, 1, 1, 0.07f * a); }

  frasesCortadas = 0;
  itensDesenhados = 0;
  vaoMin = 999.0f; vaoMax = 0.0f; discordDesenhado = 0;
  // Na pagina de apoio a previa recua (um veu, nao transparencia: a cena
  // translucida mostraria uma camada atraves da outra): os codigos sao o assunto.
  if (!compacto()) {
    desenhaPrevia(N_X + N_PAD, y0 + N_PAD, a);
    { float rec = anim_clamp(pag - (float)(paginaApoio() - 1), 0.0f, 1.0f);
      if (rec > 0.004f)
        gfx_cor((GfxRect){ N_X + N_PAD, y0 + N_PAD, PV_W, PV_H }, PV_RAIO / PV_H, 0.052f, 0.055f, 0.064f,
                0.55f * sai3(rec) * a); }
  }

  for (i = 0; i <= paginaApoio(); i++) {
    float al, dx;
    if (fabsf(pag - (float)i) >= 1.0f) continue;
    visPagina(i, &al, &dx);
    if (i < paginaApoio()) paginaNovidades(i, y0, a * al, dx);
    else paginaApoioDesenho(y0, a * al, dx);
  }
  rodape(y0, altura, a);
}

// Cartao de tela quase cheia: ampliado so se ainda couber (escala.h).
void novcartao_desenhar(Uint32 agora) {
  (void)agora;
  ESCALA_SE_COUBER_INI(N_W, N_H);
  desenharCorpo();
  ESCALA_SE_COUBER_FIM();
}
