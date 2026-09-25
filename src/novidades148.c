// Cartao de NOVIDADES DA 1.4.8.
//
// Pedido do dono: "o what's new com todas as mudancas [...] coloque um sneak
// peek do match color com a logo, tipo uma logo e os componentes em
// gradiente". Entao o cartao tem UM elemento que manda, a PREVIA: uma pagina de
// titulo em miniatura (fundo, logo, ano, Reproduzir com o anel de foco, barra
// de progresso) pintada com a paleta que a cor viva tira do logo e da arte.
// A cada 4 s ela passa para outro dos tres titulos e as cores andam junto.
// Do lado, as mudancas em cinco grupos de uma ou duas linhas.
//
// NADA DE COR REIMPLEMENTADA AQUI. A paleta e a de corviva_extrair, anotada
// pelo fio de decode de tex_cache (a arte pedida como heroi, o logo por ser
// transparente) e lida com corviva_paleta; a mistura entre dois titulos e em
// OKLab com as conversoes de corviva; o degrade do botao e do anel sao os
// programas GFX_COR_GRAD e GFX_ANEL_GRAD, e a luz por tras e o GFX_AMBIENTE
// (gfx_ambiente). Esses tres programas leem as cores de globais (nv_grad_viva,
// nv_ambiente_viva): a previa VESTE as globais com a paleta dela so enquanto
// desenha e as devolve logo depois (vestir/despir). A cor da interface nao
// muda — o botao do proprio cartao continua na cor da pessoa.
//
// SEM REDE: os tres titulos vem de deploy/app/art (fundo, logo e a linha de
// catalogo.txt), os mesmos da home do pacote.
//
// A MARCA E "novidades-148.txt". Vem DEPOIS do cartao da 1.4.2 na fila de
// app.c: quem pula da 1.4.1 le as duas na ordem em que chegaram.
#include "novidades148.h"
#include "dados.h"
#include "ajustes.h"
#include "corviva.h"
#include "gfx.h"
#include "botoes.h"
#include "text.h"
#include "tex_cache.h"
#include "anim.h"
#include "layout.h"
#include "idioma.h"
#include "ponteiro.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define N148_ARQ         "novidades-148.txt"
#define N148_W           1720.0f
#define N148_H            960.0f
#define N148_X           ((NV_TELA_W - N148_W) * 0.5f)
#define N148_Y           ((NV_TELA_H - N148_H) * 0.5f)
#define N148_PAD           52.0f
#define N148_RAIO          32.0f
// A previa: a coluna da esquerda inteira, acima do rodape.
#define N148_PV_W         820.0f
#define N148_PV_H         (N148_H - 2.0f * N148_PAD - 36.0f - BOTAO_H_PRIMARIO)
#define N148_PV_RAIO       26.0f
#define N148_ARTE_H       500.0f
#define N148_COL_GAP       60.0f
#define N148_TXT_X        (N148_X + N148_PAD + N148_PV_W + N148_COL_GAP)
#define N148_TXT_W        (N148_X + N148_W - N148_PAD - N148_TXT_X)
#define N148_ICONE         44.0f
#define N148_ABRIR_MS     280.0f
#define N148_FECHAR_MS    160.0f
// O ciclo: 4 s por titulo, 700 ms de passagem (cores e arte juntas).
#define N148_CICLO_S        4.0f
#define N148_TROCA_S        0.7f

enum { B_DEPOIS = 0, B_VELOCIDADE = 1, B_COR = 2, B_N };

static int   aberto, decidido, foco = B_COR, pedido;
static float entrada, relogio, fase;
static char  dirArte[512] = "deploy/app/art";

// ---------------------------------------------------------------- os titulos
//
// Tres do pacote com logo COLORIDO e de matizes longe um do outro — o logo
// de "June & John" vai do laranja ao magenta, o de "Lost" e azul-petroleo e o
// de "Outcome" vai do laranja ao verde. Escolhidos na folha de contato da cor
// viva (tests/corviva_folha_shot.sh) rodada sobre as 40 artes do pacote.
#define N148_NT 3
static const int INDICES[N148_NT] = { 30, 21, 6 };

typedef struct {
  char fundo[600], logo[600];
  char ano[48], dur[48], idade[8];
  int  temPaleta;
  // As nove cores na ordem de corviva_quadro: destaque, base, tres paradas do
  // degrade e quatro luzes de regiao.
  float cor[9][3];
} Titulo;

static Titulo tit[N148_NT];
static int carregados;

// Copia o campo `n` (0..) de uma linha "a|b|c" para `d`.
static void campo(const char *linha, int n, char *d, size_t tam) {
  const char *p = linha, *q;
  size_t k;
  while (n-- > 0 && p) { p = strchr(p, '|'); if (p) p++; }
  d[0] = 0;
  if (!p) return;
  q = strchr(p, '|');
  k = q ? (size_t)(q - p) : strcspn(p, "\r\n");
  if (k >= tam) k = tam - 1;
  memcpy(d, p, k);
  d[k] = 0;
}

// "2026  ·  38 min" -> "2026" e "38 min".
static void anoEDuracao(const char *s, char *ano, size_t na, char *dur, size_t nd) {
  const char *sep = strstr(s, "·");
  size_t k;
  ano[0] = dur[0] = 0;
  if (!sep) { snprintf(ano, na, "%s", s); return; }
  k = (size_t)(sep - s);
  while (k > 0 && s[k - 1] == ' ') k--;
  if (k >= na) k = na - 1;
  memcpy(ano, s, k); ano[k] = 0;
  sep += strlen("·");
  while (*sep == ' ') sep++;
  snprintf(dur, nd, "%s", sep);
}

static void carregarTitulos(void) {
  char caminho[600], linha[2048], rel[512], meta[96];
  int n = 0, i;
  FILE *f;
  if (carregados) return;
  carregados = 1;
  memset(tit, 0, sizeof tit);
  snprintf(caminho, sizeof caminho, "%s/catalogo.txt", dirArte);
  f = fopen(caminho, "r");
  // Sem catalogo a linha fica vazia e a previa desenha so fundo e logo: os
  // nomes dos arquivos seguem a numeracao do pacote.
  for (i = 0; i < N148_NT; i++) {
    snprintf(tit[i].fundo, sizeof tit[i].fundo, "%s/%02d.jpg", dirArte, INDICES[i]);
    snprintf(tit[i].logo, sizeof tit[i].logo, "%s/logo/%02d.png", dirArte, INDICES[i]);
  }
  if (!f) return;
  while (fgets(linha, sizeof linha, f)) {
    if (linha[0] == '\n' || linha[0] == '#') continue;
    for (i = 0; i < N148_NT; i++) {
      if (INDICES[i] != n) continue;
      campo(linha, 0, rel, sizeof rel);
      if (rel[0]) snprintf(tit[i].fundo, sizeof tit[i].fundo, "%s/%s", dirArte, rel);
      campo(linha, 2, rel, sizeof rel);
      if (rel[0]) snprintf(tit[i].logo, sizeof tit[i].logo, "%s/%s", dirArte, rel);
      campo(linha, 5, meta, sizeof meta);
      anoEDuracao(meta, tit[i].ano, sizeof tit[i].ano, tit[i].dur, sizeof tit[i].dur);
      campo(linha, 6, tit[i].idade, sizeof tit[i].idade);
    }
    n++;
  }
  fclose(f);
}

// A mesma escolha de fonte de corviva_quadro com "Cor da logo" ligada: o logo
// da o destaque e o degrade quando tem cor; a arte da a base e as luzes.
static void buscarPaletas(void) {
  int i, k;
  for (i = 0; i < N148_NT; i++) {
    CorvivaPaleta arte, logo;
    const CorvivaPaleta *fonte;
    int temArte, temLogo;
    if (tit[i].temPaleta) continue;
    temArte = corviva_paleta(tit[i].fundo, &arte);
    temLogo = corviva_paleta(tit[i].logo, &logo);
    if (!temArte || !temLogo) continue;
    fonte = logo.ok ? &logo : &arte;
    memcpy(tit[i].cor[0], fonte->acento, sizeof tit[i].cor[0]);
    memcpy(tit[i].cor[1], arte.base, sizeof tit[i].cor[1]);
    for (k = 0; k < 3; k++) memcpy(tit[i].cor[2 + k], fonte->grad[k], sizeof tit[i].cor[0]);
    for (k = 0; k < 4; k++) memcpy(tit[i].cor[5 + k], arte.regiao[k], sizeof tit[i].cor[0]);
    tit[i].temPaleta = 1;
  }
}

int novidades148_previa_pronta(void) {
  int i;
  for (i = 0; i < N148_NT; i++) if (!tit[i].temPaleta) return 0;
  return carregados;
}

// Pede as seis texturas. A arte vai pelo pedido de HEROI: e so a ele que o fio
// de decode anota a paleta de uma arte opaca (tex_cache.c, heroiPedido).
static void pedirArtes(void) {
  int i;
  for (i = 0; i < N148_NT; i++) {
    tex_obter_hero(tit[i].fundo);
    tex_obter_larg(tit[i].logo, 440.0f);
  }
}

// ------------------------------------------------------------------- estado
void novidades148_dir(const char *d) {
  if (d && d[0]) snprintf(dirArte, sizeof dirArte, "%s", d);
  carregados = 0;
}

int novidades148_aberto(void) { return aberto; }

int novidades148_pedido(void) {
  int p = pedido;
  pedido = N148_PEDIU_NADA;
  return p;
}

static void comecar(float e) {
  aberto = 1;
  decidido = 1;
  foco = B_COR;
  entrada = e;
  relogio = 0.0f;
  fase = 0.0f;
  carregarTitulos();
  pedirArtes();
}

void novidades148_abrir(void) { comecar(1.0f); }

void novidades148_primeira_vez(void) {
  char *s;
  if (decidido) return;
  decidido = 1;
  s = dados_ler(N148_ARQ);
  if (s) { free(s); return; }
  comecar(0.0f);
}

static void fechar(int oQue) {
  aberto = 0;
  dados_gravar(N148_ARQ, "1\n");
  pedido = oQue;
}

void novidades148_evento(const SDL_Event *e) {
  SDL_Keycode k;
  if (!aberto || e->type != SDL_KEYDOWN) return;
  k = e->key.keysym.sym;
  if (k == SDLK_LEFT)  { if (foco > 0) foco--; return; }
  if (k == SDLK_RIGHT) { if (foco < B_N - 1) foco++; return; }
  if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) {
    fechar(foco == B_COR ? N148_PEDIU_COR
         : foco == B_VELOCIDADE ? N148_PEDIU_VELOCIDADE : N148_PEDIU_NADA);
    return;
  }
  if (k == SDLK_AC_BACK || k == SDLK_ESCAPE || k == SDLK_BACKSPACE ||
      k == SDLK_DELETE || e->key.keysym.scancode == NV_SCANCODE_BACK)
    fechar(N148_PEDIU_NADA);
}

void novidades148_atualizar(float dt, Uint32 agora) {
  (void)agora;
  if (!aberto && entrada < 0.002f) { entrada = 0.0f; return; }
  if (aberto) {
    pedirArtes();
    buscarPaletas();
    // O ciclo so anda com as tres paletas na mao: sem elas a previa ficaria
    // parada num titulo sem cor e depois "pularia" para o meio do ciclo.
    if (novidades148_previa_pronta()) relogio += dt;
    if (!ajustes_animacoes_reduzidas()) fase += dt;
  }
  if (ajustes_animacoes_reduzidas()) { entrada = aberto ? 1.0f : 0.0f; return; }
  entrada = anim_rampa(entrada, aberto ? 1.0f : 0.0f, dt,
                       aberto ? N148_ABRIR_MS : N148_FECHAR_MS);
}

// ------------------------------------------------------------ a paleta viva
//
// De qual titulo para qual, e quanto da passagem ja andou (0..1, com saida
// cubica, a mesma curva de corviva_quadro). Com animacoes reduzidas a troca
// continua a cada 4 s (e o conteudo da previa), mas sem passagem.
static void cicloAgora(int *de, int *para, float *e) {
  int volta = (int)floorf(relogio / N148_CICLO_S);
  float dentro = relogio - (float)volta * N148_CICLO_S;
  float t = ajustes_animacoes_reduzidas() ? 1.0f
          : anim_clamp(dentro / N148_TROCA_S, 0.0f, 1.0f);
  float u = 1.0f - t;
  *para = volta % N148_NT;
  *de = volta > 0 ? (volta - 1) % N148_NT : *para;
  *e = 1.0f - u * u * u;
}

static float paleta[9][3];
static int   temPaleta;

static void misturar(void) {
  int de, para, k;
  float e;
  cicloAgora(&de, &para, &e);
  temPaleta = tit[para].temPaleta;
  if (!temPaleta) return;
  if (!tit[de].temPaleta) de = para;
  for (k = 0; k < 9; k++) {
    float a[3], b[3], l[3];
    corviva_srgb_para_oklab(tit[de].cor[k], a);
    corviva_srgb_para_oklab(tit[para].cor[k], b);
    l[0] = a[0] + (b[0] - a[0]) * e;
    l[1] = a[1] + (b[1] - a[1]) * e;
    l[2] = a[2] + (b[2] - a[2]) * e;
    corviva_oklab_para_srgb(l, paleta[k]);
  }
}

// VESTIR/DESPIR: as globais que os programas de degrade e de luz leem.
static float guardaGrad[3][3], guardaAmb[4][3], guardaForca, guardaTempo;
static void vestir(void) {
  memcpy(guardaGrad, nv_grad_viva, sizeof guardaGrad);
  memcpy(guardaAmb, nv_ambiente_viva, sizeof guardaAmb);
  guardaForca = nv_ambiente_forca;
  guardaTempo = nv_tempo_viva;
  memcpy(nv_grad_viva, paleta[2], sizeof guardaGrad);
  memcpy(nv_ambiente_viva, paleta[5], sizeof guardaAmb);
  nv_ambiente_forca = 1.0f;
  nv_tempo_viva = fase;
}
static void despir(void) {
  memcpy(nv_grad_viva, guardaGrad, sizeof guardaGrad);
  memcpy(nv_ambiente_viva, guardaAmb, sizeof guardaAmb);
  nv_ambiente_forca = guardaForca;
  nv_tempo_viva = guardaTempo;
}

// ------------------------------------------------------------------ a previa
static void desenhaLogo(int i, float x, float yBase, float a) {
  GLuint t;
  float ap, w, h;
  if (a <= 0.004f) return;
  t = tex_obter_larg(tit[i].logo, 440.0f);
  if (!t) return;
  ap = tex_aspecto(tit[i].logo);
  if (ap <= 0.0f) ap = 3.0f;
  h = 150.0f; w = h * ap;
  if (w > 440.0f) { w = 440.0f; h = w / ap; }
  gfx_tex_aspect_atual = 0.0f;
  gfx_rect((GfxRect){ x, yBase - h, w, h }, t,
           tex_marca_escura(tit[i].logo) ? GFX_MARCA : GFX_TEXTO,
           0, 0, 0, 0.0f, 1, 1, 1, a);
}

static void desenhaArte(int i, GfxRect r, float a) {
  GLuint t;
  if (a <= 0.004f) return;
  t = tex_obter_hero(tit[i].fundo);
  if (!t) return;
  gfx_tex_aspect_atual = tex_aspecto(tit[i].fundo);
  if (gfx_tex_aspect_atual <= 0.0f) gfx_tex_aspect_atual = 16.0f / 9.0f;
  gfx_rect(r, t, GFX_CARD, 0, 0, 0, N148_PV_RAIO / r.h, 1, 1, 1, a);
  gfx_tex_aspect_atual = 0.0f;
}

// Linha de metadados: ano, duracao e a classificacao num selo com contorno.
static void desenhaMeta(int i, float x, float y, float a) {
  char s[128];
  TxtLinha l;
  if (a <= 0.004f || !tit[i].ano[0]) return;
  if (tit[i].dur[0]) snprintf(s, sizeof s, "%s   ·   %s", tit[i].ano, tit[i].dur);
  else snprintf(s, sizeof s, "%s", tit[i].ano);
  l = txt_linha(TXT_DET_META2, s, 214, 218, 226, 255);
  txt_desenhar_alpha(l, x, y, a);
  if (tit[i].idade[0]) {
    TxtLinha b = txt_linha(TXT_CAPTION2, tit[i].idade, 230, 232, 238, 255);
    float bx = x + (float)l.w + 22.0f, bw = (float)b.w + 18.0f, bh = 30.0f;
    float by = y + ((float)l.h - bh) * 0.5f;
    gfx_rect((GfxRect){ bx, by, bw, bh }, 0, GFX_ANEL, 0, 1.5f / bh, 0, 6.0f / bh,
             1, 1, 1, 0.45f * a);
    txt_desenhar_alpha(b, bx + 9.0f, by + (bh - (float)b.h) * 0.5f, a);
  }
}

static void desenhaPrevia(float x, float y, float a) {
  GfxRect pv = { x, y, N148_PV_W, N148_PV_H };
  GfxRect arte = { x, y, N148_PV_W, N148_ARTE_H };
  float *base = paleta[1], *meio = paleta[3];
  float lx = x + 52.0f;
  int de, para;
  float e, aDe, aPara;
  cicloAgora(&de, &para, &e);
  // Titulo que sai some na primeira metade da passagem; o que entra aparece a
  // partir do comeco — assim nunca ha um quadro sem arte nenhuma.
  aPara = e;
  aDe = de == para ? 0.0f : 1.0f - anim_clamp(e * 1.4f, 0.0f, 1.0f);
  if (de == para) aPara = 1.0f;

  // O "chao" da pagina: a base tingida da arte, como o tema estilizado pinta.
  gfx_cor(pv, N148_PV_RAIO / pv.h, base[0], base[1], base[2], a);
  desenhaArte(de, arte, a * aDe);
  desenhaArte(para, arte, a * aPara);
  // A arte se dissolve na base, e escurece a esquerda para o logo.
  // Duas passadas do veu: a segunda, so na metade de baixo, leva a base a
  // ~100% na borda — com uma so a arte terminava numa linha visivel.
  gfx_rect(arte, 0, GFX_VEU_CARD, 0, 0, 0, N148_PV_RAIO / arte.h,
           base[0], base[1], base[2], a);
  { GfxRect baixo = { arte.x, arte.y + arte.h * 0.55f, arte.w, arte.h * 0.45f };
    gfx_rect(baixo, 0, GFX_VEU_CARD, 0, 0, 0, N148_PV_RAIO / baixo.h,
             base[0], base[1], base[2], a); }
  gfx_luz_canto(arte, N148_PV_RAIO / arte.h, 0.0f, arte.h * 0.78f, arte.h * 0.95f,
                base[0], base[1], base[2], 0.70f * a);
  // A luz da arte subindo por baixo dos botoes: a parada do meio do degrade.
  // DEPOIS da arte e do veu, senao ela para na borda da arte e desenha uma
  // linha ali.
  gfx_luz_canto(pv, N148_PV_RAIO / pv.h, pv.w * 0.30f, pv.h * 1.05f, pv.h * 0.75f,
                meio[0], meio[1], meio[2], 0.30f * a);

  // Selo "Previa" no alto e os tres tracos do ciclo a direita.
  { TxtLinha t = txt_linha(TXT_CAPTION2, i18n("Prévia"), 236, 238, 244, 255);
    GfxRect s = { x + 28.0f, y + 28.0f, (float)t.w + 32.0f, 38.0f };
    gfx_cor(s, 0.5f, 0.02f, 0.02f, 0.03f, 0.52f * a);
    txt_desenhar_alpha(t, s.x + 16.0f, s.y + (s.h - (float)t.h) * 0.5f, a); }
  { int i;
    float dentro = relogio - floorf(relogio / N148_CICLO_S) * N148_CICLO_S;
    for (i = 0; i < N148_NT; i++) {
      float tx = x + N148_PV_W - 28.0f - (float)(N148_NT - i) * 44.0f + 8.0f;
      GfxRect tr = { tx, y + 45.0f, 36.0f, 4.0f };
      gfx_cor(tr, 0.5f, 1, 1, 1, 0.28f * a);
      if (i == para) {
        float p = ajustes_animacoes_reduzidas() ? 1.0f : dentro / N148_CICLO_S;
        tr.w *= anim_clamp(p, 0.0f, 1.0f);
        if (tr.w > 1.0f) gfx_cor(tr, 0.5f, 1, 1, 1, 0.92f * a);
      }
    } }

  // Logo e metadados em SEQUENCIA, e nao cruzados como a arte: o que sai some
  // nos primeiros 40% da passagem e so entao o outro entra. Cruzados, dois
  // logos e duas linhas de ano no mesmo lugar viravam um borrao legivel.
  { float fDe = de == para ? 0.0f : 1.0f - anim_clamp(e / 0.4f, 0.0f, 1.0f);
    float fPara = de == para ? 1.0f : anim_clamp((e - 0.4f) / 0.6f, 0.0f, 1.0f);
    desenhaLogo(de, lx, y + 440.0f, a * fDe);
    desenhaLogo(para, lx, y + 440.0f, a * fPara);
    desenhaMeta(de, lx, y + 466.0f, a * fDe);
    desenhaMeta(para, lx, y + 466.0f, a * fPara); }

  // Reproduzir: o degrade do logo no miolo, a luz dele por tras e o anel de
  // foco com a luz girando — exatamente o que "Dinamica gradiente" desenha.
  { const char *rot = i18n("Reproduzir");
    TxtLinha l = txt_linha(TXT_DET_BOTAO, rot, 255, 255, 255, 255);
    float ic = BOTAO_ICONE;
    float bw = (float)l.w + ic + BOTAO_ICONE_GAP + 2.0f * BOTAO_PAD_X;
    GfxRect b = { lx, y + 552.0f, bw, BOTAO_H_PRIMARIO };
    GfxRect anel = { b.x - 7.0f, b.y - 7.0f, b.w + 14.0f, b.h + 14.0f };
    GfxRect d1 = { b.x + b.w + 22.0f, b.y, BOTAO_H_PRIMARIO, BOTAO_H_PRIMARIO };
    GfxRect d2 = { d1.x + BOTAO_H_PRIMARIO + 16.0f, b.y, BOTAO_H_PRIMARIO, BOTAO_H_PRIMARIO };
    float gx = b.x + (b.w - ((float)l.w + ic + BOTAO_ICONE_GAP)) * 0.5f;
    gfx_rect((GfxRect){ b.x - 26.0f, b.y - 20.0f, b.w + 52.0f, b.h + 40.0f }, 0,
             GFX_SOMBRA, 0.9f, 0, 0, 0.5f, meio[0], meio[1], meio[2], 0.55f * a);
    gfx_rect(b, 0, GFX_COR_GRAD, 0, 0, 0, 0.5f, 1, 1, 1, a);
    gfx_rect(anel, 0, GFX_ANEL_GRAD, 0, 3.0f / anel.h, 0, 0.5f, 1, 1, 1, a);
    gfx_icone((GfxRect){ gx, b.y + (b.h - ic) * 0.5f, ic, ic }, "play", 1, 1, 1, a);
    txt_desenhar_alpha(l, gx + ic + BOTAO_ICONE_GAP, b.y + (b.h - (float)l.h) * 0.5f, a);
    botao_disco(d1, "mais", 0.0f, a);
    botao_disco(d2, "trailer", 0.0f, a);

    // Continuar de onde parou: trilho, preenchimento no degrade e o que falta.
    { float py = b.y + b.h + 56.0f, pw = 420.0f;
      TxtLinha f = txt_linha(TXT_CAPTION2, i18n("Faltam 24 min"), 196, 202, 214, 255);
      gfx_cor((GfxRect){ lx, py, pw, 6.0f }, 0.5f, 1, 1, 1, 0.16f * a);
      gfx_rect((GfxRect){ lx, py, pw * 0.62f, 6.0f }, 0, GFX_COR_GRAD, 0, 0, 0, 0.5f, 1, 1, 1, a);
      txt_desenhar_alpha(f, lx + pw + 20.0f, py + 3.0f - (float)f.h * 0.5f, a * 0.95f); } }
}

// -------------------------------------------------------------- os grupos
typedef struct { const char *titulo, *l1, *l2; } N148Grupo;

static const N148Grupo GRUPOS[] = {
  { "Cor viva",
    "Quatro estilos: Dinâmica, estilizada, gradiente e imersiva.",
    "Em Ajustes › Cor de destaque. Cor da logo já vem ligada." },
  { "Guia de TV",
    "Prévia grande, grade na largura toda e logos dos canais.",
    "Mini guia por cima do vídeo e lembrete de programa." },
  { "Teste de velocidade",
    "Em Ajustes › Diagnóstico, abaixo de Diagnóstico e otimização.",
    "Mede seus addons e servidores e diz o maior arquivo que toca sem parar, por filme de 2 h e por episódio." },
  { "Legendas e canais",
    "Legenda ASS de anime carrega antes do vídeo e não cai mais.",
    "Canal lento para abrir, como o 4K, não é mais pulado." },
  { "Salvos",
    "Segure OK: Mais informações, Remover ou Marcar como assistido.",
    NULL }
};
#define N148_NG ((int)(sizeof GRUPOS / sizeof GRUPOS[0]))

// O desenho pequeno de cada grupo, num disco discreto. O da cor viva e o
// proprio degrade da previa: a paleta muda ali tambem.
static void icone(int g, float x, float y, float a) {
  GfxRect d = { x, y, N148_ICONE, N148_ICONE };
  float s = 24.0f, o = (N148_ICONE - s) * 0.5f;
  GfxRect ic = { x + o, y + o, s, s };
  if (g == 0) {
    if (temPaleta) {
      vestir();
      gfx_rect(d, 0, GFX_COR_GRAD, 0, 0, 0, 0.5f, 1, 1, 1, a);
      despir();
    } else {
      gfx_cor(d, 0.5f, 1, 1, 1, 0.08f * a);
    }
    return;
  }
  gfx_cor(d, 0.5f, 1, 1, 1, 0.08f * a);
  switch (g) {
    case 1: gfx_icone(ic, "menu_guide", 0.92f, 0.93f, 0.96f, a); break;
    case 2:  // tres barras de medida subindo
      gfx_cor((GfxRect){ x + 13, y + 24, 5, 9 }, 0.3f, 0.92f, 0.93f, 0.96f, 0.55f * a);
      gfx_cor((GfxRect){ x + 20, y + 18, 5, 15 }, 0.3f, 0.92f, 0.93f, 0.96f, 0.75f * a);
      gfx_cor((GfxRect){ x + 27, y + 11, 5, 22 }, 0.3f, 0.92f, 0.93f, 0.96f, a);
      break;
    case 3: gfx_icone(ic, "legenda", 0.92f, 0.93f, 0.96f, a); break;
    default: gfx_icone(ic, "menu_library", 0.92f, 0.93f, 0.96f, a); break;
  }
}

// O resultado do teste em miniatura, na linha do titulo do grupo: uma conta
// de verdade de vazao.c (mediana de 40 Mbps -> maximo de 36 Mbps -> 32 GB num
// filme de 2 h), para a pessoa saber que tipo de resposta o teste da.
static void amostraVelocidade(float xDir, float y, float a) {
  char mbps[16];
  TxtLinha n;
  snprintf(mbps, sizeof mbps, "%d Mbps", 40);
  n = txt_linha(TXT_CAPTION2, mbps, 244, 246, 250, 255);
  TxtLinha t = txt_linha(TXT_CAPTION2, i18n("até 32 GB por filme"), 190, 197, 210, 255);
  float h = 38.0f, w = 20.0f + (float)n.w + 14.0f + (float)t.w + 20.0f;
  GfxRect r = { xDir - w, y, w, h };
  gfx_cor(r, 0.5f, 1, 1, 1, 0.07f * a);
  txt_desenhar_alpha(n, r.x + 20.0f, y + (h - (float)n.h) * 0.5f, a);
  txt_desenhar_alpha(t, r.x + 20.0f + (float)n.w + 14.0f, y + (h - (float)t.h) * 0.5f, a * 0.95f);
}

static float grupo(int g, float y, float a) {
  float tx = N148_TXT_X + N148_ICONE + 22.0f;
  float tw = N148_TXT_X + N148_TXT_W - tx;
  float local = ajustes_animacoes_reduzidas()
      ? 1.0f : anim_clamp((a - 0.05f * (float)g) * 3.0f, 0.0f, 1.0f);
  float yy = y + (1.0f - local) * 14.0f, al = a * local, h;
  icone(g, N148_TXT_X, yy - 4.0f, al);
  { TxtLinha t = txt_linha_corta(TXT_CALLOUT, i18n(GRUPOS[g].titulo), 246, 247, 252, 255, tw);
    txt_desenhar_alpha(t, tx, yy, al); }
  if (g == 2) amostraVelocidade(N148_TXT_X + N148_TXT_W, yy - 3.0f, al);
  h = 42.0f;
  h += txt_bloco(TXT_CAPTION, i18n(GRUPOS[g].l1), 188, 195, 209,
                 tx, yy + h, tw, 29.0f, al * 0.96f, 2);
  if (GRUPOS[g].l2)
    h += txt_bloco(TXT_CAPTION, i18n(GRUPOS[g].l2), 188, 195, 209,
                   tx, yy + h, tw, 29.0f, al * 0.96f, 2);
  return h;
}

static void ponteiroFoco(int b, int nada) { (void)nada; foco = b; }

void novidades148_desenhar(Uint32 agora) {
  float a = anim_suave(entrada), dy, y0;
  float ar, ag, ab;
  (void)agora;
  if (entrada < 0.002f) return;
  ajustes_acento(&ar, &ag, &ab);
  misturar();

  gfx_cor((GfxRect){ 0, 0, NV_TELA_W, NV_TELA_H }, 0.0f, 0, 0, 0, 0.76f * entrada);
  // A LUZ DA ARTE VAZANDO pela tela em volta do cartao — o tema imersivo, com
  // a paleta da previa.
  if (temPaleta) { vestir(); gfx_ambiente(0.95f * a); despir(); }

  dy = (1.0f - a) * 34.0f;
  y0 = N148_Y + dy;
  gfx_cor((GfxRect){ N148_X, y0, N148_W, N148_H }, N148_RAIO / N148_H,
          0.050f, 0.053f, 0.062f, 0.92f * a);
  gfx_rect((GfxRect){ N148_X, y0, N148_W, N148_H }, 0, GFX_ANEL, 0, 1.2f / N148_H, 0,
           N148_RAIO / N148_H, 1, 1, 1, 0.06f * a);

  if (temPaleta) {
    // A luz da previa derramando no cartao em volta dela.
    { float *c = paleta[3], m = 110.0f;
      gfx_rect((GfxRect){ N148_X + N148_PAD - m, y0 + N148_PAD - m * 0.6f,
                          N148_PV_W + 2.0f * m, N148_PV_H + 1.6f * m },
               0, GFX_SOMBRA, 1.0f, 0, 0, 0.35f, c[0], c[1], c[2], 0.42f * a); }
    vestir();
    desenhaPrevia(N148_X + N148_PAD, y0 + N148_PAD, a);
    despir();
  } else {
    gfx_cor((GfxRect){ N148_X + N148_PAD, y0 + N148_PAD, N148_PV_W, N148_PV_H },
            N148_PV_RAIO / N148_PV_H, 0.075f, 0.078f, 0.090f, a);
  }

  txt_bloco(TXT_TITULO2, i18n("Novidades da 1.4.8"), 248, 249, 252,
            N148_TXT_X, y0 + N148_PAD - 6.0f, N148_TXT_W, 62.0f, a, 1);
  { int g;
    float y = y0 + N148_PAD + 104.0f;
    for (g = 0; g < N148_NG; g++) y += grupo(g, y, a) + 24.0f; }

  // RODAPE: os tres botoes da tabela (botoes.h) encostados a direita, o
  // primario por ultimo e com o foco ao abrir — OK sozinho abre a cor.
  { float yBase = y0 + N148_H - N148_PAD;
    const char *rotC = i18n("Experimentar a cor viva");
    const char *rotV = i18n("Testar a velocidade");
    const char *rotD = i18n("Agora não");
    float wC = botao_largura(rotC, NULL, 1), wV = botao_largura(rotV, NULL, 0);
    float wD = botao_largura(rotD, NULL, 0);
    GfxRect bC = { N148_X + N148_W - N148_PAD - wC, yBase - BOTAO_H_PRIMARIO,
                   wC, BOTAO_H_PRIMARIO };
    GfxRect bV = { bC.x - BOTAO_GAP - wV, yBase - BOTAO_H_PRIMARIO * 0.5f - BOTAO_H_SECUNDARIO * 0.5f,
                   wV, BOTAO_H_SECUNDARIO };
    GfxRect bD = { bV.x - BOTAO_GAP - wD, bV.y, wD, BOTAO_H_SECUNDARIO };
    botao_pilula(bD, rotD, NULL, foco == B_DEPOIS ? 1.0f : 0.0f, 0, 0, a);
    botao_pilula(bV, rotV, NULL, foco == B_VELOCIDADE ? 1.0f : 0.0f, 0, 0, a);
    botao_pilula(bC, rotC, NULL, foco == B_COR ? 1.0f : 0.0f, 1, 0, a);
    if (aberto) {
      ponteiro_alvo(bD.x, bD.y, bD.w, bD.h, ponteiroFoco, NULL, B_DEPOIS, 0);
      ponteiro_alvo(bV.x, bV.y, bV.w, bV.h, ponteiroFoco, NULL, B_VELOCIDADE, 0);
      ponteiro_alvo(bC.x, bC.y, bC.w, bC.h, ponteiroFoco, NULL, B_COR, 0);
    } }
}
