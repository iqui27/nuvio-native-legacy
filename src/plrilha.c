// A ilha do relogio dentro do player — ver plrilha.h.
//
// CUSTO, a mesma regra da ilha.c: uma sombra do tamanho da ilha, o miolo, a
// luz de canto, o texto do cabecalho e o corpo de quem pediu. Nada de tela
// cheia (o veu de cima do aviso sozinho e o unico, e so sem o OSD).
#include "central.h"
#include "horafmt.h"
#include "plrilha.h"
#include "plrui.h"
#include "ajustes.h"
#include "anim.h"
#include "idioma.h"
#include "layout.h"
#include "text.h"
#include "relogiofim.h"
#include "desempenho.h"
#include "velocidade.h"
#include "ponteiro.h"
#define NV_ESCALA_TELA   // o arquivo inteiro mede pela tela virtual (escala.h)
#include "escala.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

// A mola da ilha (ilha.c, ILHA_MOLA_W/Z; o Spotlight usa a mesma, SP_MOLA).
#define MOLA_W 10.0f
#define MOLA_Z 0.72f
// Mesma margem e mesmo topo da ilha da Home (dono, 12/10: simetrica com os
// selos do lado oposto em player.c, que seguem a mesma margem).
#define X_ESQ   NV_ILHA_MARGEM_D
#define Y_TOPO  NV_ILHA_Y
#define CAB_H   64.0f      // cabecalho do corpo = a pilula aberta
#define PIL_H   56.0f
#define RAIO_CORPO 36.0f
// A frase do aviso no player: mais larga que a da ilha de fora (760), porque
// os avisos de fonte do player sao frases inteiras e nao ha nada ao lado.
#define PLR_TEXTO_MAX 1300.0f

static PlrIlhaPedido ped, ult;      // o deste quadro e o ultimo com corpo
static int temPed, temUlt, escondida;
static char pedTexto[200], pedDir[80], pedIcone[32];
static char ultTexto[200], ultDir[80], ultIcone[32];
static float relA;                  // opacidade do OSD neste quadro
static double falta = -1.0;
// #202: a velocidade da reproducao (centesimos); fora de 1x vai na pilula,
// depois do "termina as", no acento.
static int velCent = VEL_NORMAL;
static float W, vW, H, vH, A, corpoA, textoA = 1.0f;
static GfxRect ultRect;
static int ultOk;
static Uint32 ultQuadro;
static int dir;
// MEDIDOR DE DESEMPENHO (desempenho.h): so na pilula da HORA sozinha (sem
// pedido nem corpo saindo). dsP = a forma deste quadro; dsA = o corpo dele.
static int dsP;
static float dsA;
#ifdef NV_SHOT_HOOKS
static time_t horaFixa;
void plrilha_shot_hora(time_t t) { horaFixa = t; }
static GfxRect shotCentro;
int plrilha_shot_centro(GfxRect *r) { *r = shotCentro; return r->w > 0; }
static float shotTxtX, shotFimX;
static int shotTemFim;
void plrilha_shot_linha(float *t, float *f, int *tf) { *t = shotTxtX; *f = shotFimX; *tf = shotTemFim; }
#endif

static float mola(float *v, float x, float alvo, float dt) {
  int k;
  if (anim_politica_reduzida || ajustes_animacoes_reduzidas()) { *v = 0.0f; return alvo; }
  if (dt > 0.05f) dt = 0.05f;
  for (k = 0; k < 4; k++) {
    float h = dt * 0.25f, ac = MOLA_W * MOLA_W * (alvo - x) - 2.0f * MOLA_Z * MOLA_W * (*v);
    *v += ac * h;
    x += *v * h;
  }
  return x;
}

static void copiar(PlrIlhaPedido *d, const PlrIlhaPedido *s, char *t, char *dr, char *ic) {
  *d = *s;
  snprintf(t, 200, "%s", s->texto ? s->texto : "");
  snprintf(dr, 80, "%s", s->direita ? s->direita : "");
  snprintf(ic, 32, "%s", s->icone ? s->icone : "");
  d->texto = t[0] ? t : NULL;
  d->direita = dr[0] ? dr : NULL;
  d->icone = ic[0] ? ic : NULL;
}

// CARTAO CENTRAL (dono, 03/10: "ao abrir o filme pode deixar o componente de
// opening source no meio da tela, centralizado, mesmo sem o relogio"): o
// pedido com `centro` nao passa pela ilha do canto. Vira um cartao no mesmo
// material, so com o corpo, no centro, que entra e sai por alfa.
static PlrIlhaPedido cen, cenUlt;
static char cenT[200], cenD[80], cenI[32], cenUT[200], cenUD[80], cenUI[32];
static int temCen;
static float cenA, cenCorpoA, cenW, cenH, cenVW, cenVH;
static Uint32 cenQuadro;
// Quanto a pilula da HORA segura depois que um corpo com `voltaRelogio` acaba,
// antes de encolher e sair (o corpo vira o relogio e ai some).
#define HORA_SEGURA_MS 800u
static Uint32 horaAte;
static int prevVolta;

void plrilha_pedir(const PlrIlhaPedido *p) {
  if (p->centro && p->w > 0.0f && p->h > 0.0f) {
    copiar(&cen, p, cenT, cenD, cenI);
    temCen = 1;
    return;
  }
  // BAIXA PRIORIDADE (guia parental): nunca derruba um pedido que ja esta no
  // quadro, e qualquer outro pedido a derruba, venha antes ou depois dela.
  if (p->baixa && temPed) return;
  if (temPed && ped.baixa) temPed = 0;
  // Quem tem corpo vence quem e so aviso: a lista aberta nao vira toast.
  if (temPed && ped.w > 0.0f && p->w <= 0.0f) return;
  copiar(&ped, p, pedTexto, pedDir, pedIcone);
  temPed = 1;
}

void plrilha_relogio(float a, double f) {
  if (a > relA) relA = a;
  falta = f;
}

void plrilha_velocidade(int cent) { velCent = cent > 0 ? cent : VEL_NORMAL; }

static char titulo[256];
void plrilha_titulo(const char *t) { snprintf(titulo, sizeof titulo, "%s", t ? t : ""); }

// Sem pedido com corpo NO QUADRO o ultimo nao vale mais (ver plrilha_desenhar):
// quem esconde a ilha antes dos pedidos (estatisticas) impediria ate a troca
// de um corpo pelo outro — a ilha nova nasceria com o cabecalho dela sobre o
// corpo antigo um quadro inteiro.
void plrilha_esconder(void) { escondida = 1; temUlt = 0; corpoA = 0.0f; }

int plrilha_direita(void) {
  return 1;   // sempre a direita (dono, 03/10)
}

int plrilha_rect(GfxRect *r) { if (ultOk && r) *r = ultRect; return ultOk; }
float plrilha_corpo_alfa(void) { return temUlt ? corpoA : 0.0f; }

// --- a linha da pilula ------------------------------------------------------------
typedef struct {
  TxtLinha txt, hora, fim, dir, vel;
  float w;                 // largura do conteudo (sem o recuo)
  float ds;                // o trecho do medidor no fim da linha (0 = nada)
  int temIcone, temTxt, temFim, temVel;
} Linha;

static void horaAgora(char *h, size_t n, char *fim, size_t nf) {
  time_t t = time(NULL);
  struct tm lt;
#ifdef NV_SHOT_HOOKS
  if (horaFixa) t = horaFixa;
#endif
  localtime_r(&t, &lt);
  hora_tela(h, n, &lt);
  fim[0] = 0;
  if (falta >= 0.0) relogio_fim_ilha(fim, nf, t, falta);
}

static float montar(const PlrIlhaPedido *p, Linha *L) {
  char h[16], fim[RELOGIO_FIM_MAX];
  float w = 0.0f;
  memset(L, 0, sizeof *L);
  horaAgora(h, sizeof h, fim, sizeof fim);
  L->temIcone = p && p->icone;
  L->temTxt = p && p->texto;
  L->temFim = (!p || !p->semFim) && fim[0];
  L->hora = txt_linha(TXT_ILHA_NOME, h, 243, 242, 239, 255);
  if (L->temIcone) w += 24.0f + 12.0f;
  if (p && p->respira) w += 10.0f + 14.0f;
  if (L->temTxt) {
    if (p->tituloLeve) L->txt = txt_linha_corta(PLR_TIT_ESTILO, p->texto, PLR_TIT_R, PLR_TIT_G, PLR_TIT_B, 255, PLR_TEXTO_MAX);
    else L->txt = txt_linha_corta(TXT_ILHA_NOME, p->texto, 243, 242, 239, 255, PLR_TEXTO_MAX);
    w += (float)L->txt.w + 12.0f;
  }
  if (p && p->pontos > 0) w += 4.0f + p->pontos * 8.0f + (p->pontos - 1) * 7.0f + 12.0f;
  if (L->temTxt || (p && p->pontos > 0)) w += 1.0f + 12.0f;   // separador
  w += (float)L->hora.w;
  if (L->temFim) {
    L->fim = txt_linha(TXT_G19M, fim, 243, 242, 239, 153);
    w += 12.0f + 1.0f + 12.0f + (float)L->fim.w;
  }
  if (velCent != VEL_NORMAL) {
    char v[16];
    float ar, ag, ab;
    vel_rotulo(v, sizeof v, velCent);
    plrui_decimal(v);
    ajustes_acento(&ar, &ag, &ab);
    L->vel = txt_linha(TXT_G19M, v, (int)(ar * 255.0f), (int)(ag * 255.0f), (int)(ab * 255.0f), 255);
    L->temVel = 1;
    w += 12.0f + 1.0f + 12.0f + (float)L->vel.w;
  }
  if (p && p->direita) L->dir = txt_linha(TXT_G19M, p->direita, 243, 242, 239, 140);
  L->ds = dsP ? desempenho_linha_w(dsP) : 0.0f;
  w += L->ds;
  L->w = w;
  return w;
}

// tOff/tK: o texto sai da linha (#205, `insetEsq`): ele desenha em x+tOff e
// abre mao do seu espaco na razao tK (1 = fora da linha; o resto encolhe para
// a direita, onde o relogio continua encostado).
static void desenharLinha(const PlrIlhaPedido *p, const Linha *L, float x, float yc, float a, float tOff, float tK) {
  float cr = 1, cg = 1, cb = 1;
  if (a < 0.01f) return;
  if (L->temIcone) {
    if (p->corIcone == 1) { cr = 0.941f; cg = 0.725f; cb = 0.290f; }
    else if (p->corIcone == 2) ajustes_acento(&cr, &cg, &cb);
    gfx_icone((GfxRect){ x, yc - 12.0f, 24.0f, 24.0f }, p->icone, cr, cg, cb, a);
    x += 24.0f + 12.0f;
  }
  if (p && p->respira) {
    plrui_respira(x + 5.0f, yc, 10.0f, SDL_GetTicks(), a);
    x += 10.0f + 14.0f;
  }
  if (L->temTxt) {
    txt_desenhar_alpha(L->txt, x + tOff, yc - (float)L->txt.h * 0.5f, a);
#ifdef NV_SHOT_HOOKS
    shotTxtX = x + tOff;
#endif
    x += ((float)L->txt.w + 12.0f) * (1.0f - tK);
  }
  if (p && p->pontos > 0) {
    float ar, ag, ab;
    int i;
    ajustes_acento(&ar, &ag, &ab);
    x += 4.0f;
    for (i = 0; i < p->pontos; i++) {
      GfxRect d = { x, yc - 4.0f, 8.0f, 8.0f };
      if (i == p->ponto) gfx_cor(d, 0.5f, ar, ag, ab, a);
      else gfx_cor(d, 0.5f, 1, 1, 1, 0.25f * a);
      x += 8.0f + 7.0f;
    }
    x += 12.0f - 7.0f;
  }
  if (L->temTxt || (p && p->pontos > 0)) { plrui_sep(x, yc, a); x += 1.0f + 12.0f; }
  txt_desenhar_alpha(L->hora, x, yc - (float)L->hora.h * 0.5f, a);
  x += (float)L->hora.w;
  if (L->temFim) {
    x += 12.0f;
    plrui_sep(x, yc, a);
    x += 1.0f + 12.0f;
    txt_desenhar_alpha(L->fim, x, yc - (float)L->fim.h * 0.5f + 1.0f, a);
    x += (float)L->fim.w;
  }
  if (L->temVel) {
    x += 12.0f;
    plrui_sep(x, yc, a);
    x += 1.0f + 12.0f;
    txt_desenhar_alpha(L->vel, x, yc - (float)L->vel.h * 0.5f + 1.0f, a);
    x += (float)L->vel.w;
  }
  if (L->ds > 0.0f) desempenho_linha(dsP, x, yc, a);
#ifdef NV_SHOT_HOOKS
  shotFimX = x; shotTemFim = L->temFim;
#endif
}

static void plrilha_desenharCorpo_(Uint32 agora);
// Camada ampliada (escala.h): o corpo desenha na tela virtual.
static void cartaoCentro(Uint32 agora) {
  float dt = cenQuadro ? (float)(agora - cenQuadro) / 1000.0f : 1.0f / 60.0f;
  int quer = temCen;
  cenQuadro = agora;
  if (dt > 0.1f) dt = 0.1f;
  if (temCen) copiar(&cenUlt, &cen, cenUT, cenUD, cenUI);
  // A SAIDA E A DA ILHA: a mesma mola (MOLA_W/MOLA_Z, mola()) leva a forma ao
  // disco de PIL_H*0.6 e o alfa cai com a mesma taxa (12) de quando a ilha some.
  // O corpo sai antes da forma encolher (26, a taxa do corpo da ilha).
  { float alvoW = quer ? cenUlt.w : PIL_H * 0.6f, alvoH = quer ? cenUlt.h : PIL_H * 0.6f;
    if (cenW <= 0.0f) { cenW = alvoW; cenH = alvoH; }
    cenW = mola(&cenVW, cenW, alvoW, dt);
    cenH = mola(&cenVH, cenH, alvoH, dt); }
  if (anim_politica_reduzida || ajustes_animacoes_reduzidas()) {
    cenA = quer ? 1.0f : 0.0f; cenCorpoA = cenA;
  } else {
    cenA = anim_mola(cenA, quer ? 1.0f : 0.0f, dt, quer ? 10.0f : 12.0f);
    cenCorpoA = anim_mola(cenCorpoA, quer ? 1.0f : 0.0f, dt, quer ? 10.0f : 26.0f);
  }
  temCen = 0;
#ifdef NV_SHOT_HOOKS
  shotCentro = (GfxRect){0};
#endif
  if (cenA < 0.01f) { cenA = cenCorpoA = 0.0f; cenW = cenH = 0.0f; cenVW = cenVH = 0.0f; return; }
  { const PlrIlhaPedido *c = &cenUlt;
    float w = cenW < 8.0f ? 8.0f : cenW, h = cenH < 8.0f ? 8.0f : cenH;
    GfxRect R = { (NV_TELA_W - w) * 0.5f, (NV_TELA_H - h) * 0.5f, w, h };
    GfxRect F = { (NV_TELA_W - c->w) * 0.5f, (NV_TELA_H - c->h) * 0.5f, c->w, c->h };
#ifdef NV_SHOT_HOOKS
    shotCentro = R;
#endif
    if (c->ancoraTopo) F.y = R.y;   // o conteudo desce/sobe com a forma (nao salta para o centro novo)
    // O raio acompanha a forma como na ilha: da pilula (meia altura) ao do corpo.
    float cresce = anim_clamp((h - PIL_H) / 120.0f, 0.0f, 1.0f);
    float raio = RAIO_CORPO * cresce + h * 0.5f * (1.0f - cresce);
    if (raio > h * 0.5f) raio = h * 0.5f;
    if (raio > w * 0.5f) raio = w * 0.5f;
    if (c->video) plrui_material_video(R, raio, cenA);
    else plrui_material_s(R, raio, c->modal, cenA, c->semSombra);
    gfx_recorte(R.x, R.y, R.w, R.h);
    // O corpo fica no tamanho final e o recorte da forma o corta (centrado, ou
    // preso ao topo da forma com `ancoraTopo`).
    if (c->corpo && cenCorpoA > 0.01f) c->corpo(F, cenCorpoA * cenA, c->u);
    gfx_sem_recorte(); }
}

void plrilha_desenhar(Uint32 agora) {
  ESCALA_INI();
  plrilha_desenharCorpo_(agora);
  cartaoCentro(agora);
  ESCALA_FIM();
}
static void plrilha_desenharCorpo_(Uint32 agora) {
  float dt = ultQuadro ? (float)(agora - ultQuadro) / 1000.0f : 1.0f / 60.0f;
  const PlrIlhaPedido *p;
  PlrIlhaPedido vazio;
  Linha L;
  float alvoW, alvoH, pilH, pad, x, conteudoW, cresce;
  int corpo, quer;
  ultQuadro = agora;
  if (dt > 0.1f) dt = 0.1f;
  memset(&vazio, 0, sizeof vazio);
  dir = plrilha_direita();

  if (temPed && ped.w > 0.0f) { copiar(&ult, &ped, ultTexto, ultDir, ultIcone); temUlt = 1; }
  else if (!corpoA) temUlt = 0;   // escondida neste quadro zerou o ultimo
  p = temPed ? &ped : &vazio;
  corpo = temPed && ped.w > 0.0f;
  // VOLTA AO RELOGIO: o corpo que pediu `voltaRelogio` acabou. Com o relogio
  // ligado a pilula da hora segura HORA_SEGURA_MS (o corpo encolhe ate ela) e so
  // depois sai pelo caminho de sempre (!quer). Desligado, nao ha pilula para a
  // qual voltar: encolhe e some no lugar, direto.
  if (prevVolta && !temPed && ajustes_relogio_ligado()) horaAte = agora + HORA_SEGURA_MS;
  prevVolta = temPed && ped.w > 0.0f && ped.voltaRelogio;
  if (temPed || !ajustes_relogio_ligado()) horaAte = 0;
  quer = !escondida && (temPed || relA > 0.01f || central_segurar_progresso(agora) > 0.0f ||
                       (horaAte && (int)(horaAte - agora) > 0));

  // O que a linha da pilula mostra: o pedido, ou (encolhendo depois de um
  // corpo) a linha dele ate o corpo apagar — a hora volta com a forma.
  if (!temPed && temUlt && corpoA > 0.02f) p = &ult;
  // O medidor entra so na hora sozinha: aviso, lista ou carregamento na ilha
  // o tiram (e ele volta quando ela fica so com a hora).
  dsP = p == &vazio && quer ? desempenho_forma() : DS_DESLIGADO;
  pilH = p->aberta || corpo ? CAB_H : PIL_H;
  pad = pilH > PIL_H + 0.5f ? 28.0f : 24.0f;
  conteudoW = montar(p, &L);
  alvoW = corpo ? ped.w : conteudoW + pad * 2.0f;
  alvoH = corpo ? CAB_H + ped.h : pilH;
  { float bw = 0.0f, bh = 0.0f;
    desempenho_corpo_tam(dsP, &bw, &bh);
    if (bh > 0.0f) { if (bw > alvoW) alvoW = bw; alvoH = CAB_H + bh; }
    dsA = (anim_politica_reduzida || ajustes_animacoes_reduzidas()) ? (bh > 0.0f ? 1.0f : 0.0f)
        : anim_mola(dsA, bh > 0.0f && fabsf(H - alvoH) < 0.2f * alvoH ? 1.0f : 0.0f, dt, 12.0f); }
  if (!quer) { alvoW = PIL_H * 0.6f; alvoH = PIL_H * 0.6f; }
  if (W <= 0.0f) { W = alvoW; H = alvoH; }
  W = mola(&vW, W, alvoW, dt);
  H = mola(&vH, H, alvoH, dt);
  A = anim_mola(A, quer ? 1.0f : 0.0f, dt, quer ? 9.0f : 12.0f);
  // Corpo: entra quando a forma chegou perto do tamanho final, sai antes de ela
  // encolher (a lista nunca e espremida dentro da pilula).
  { float alvoC = 0.0f;
    if (corpo && fabsf(H - alvoH) < 0.35f * alvoH && fabsf(W - alvoW) < 0.35f * alvoW) alvoC = 1.0f;
    corpoA = (anim_politica_reduzida || ajustes_animacoes_reduzidas())
           ? alvoC : anim_mola(corpoA, alvoC, dt, alvoC > corpoA ? 10.0f : 26.0f);
    if (!corpo && corpoA < 0.02f) { corpoA = 0.0f; temUlt = 0; } }
  textoA = anim_mola(textoA, 1.0f, dt, 12.0f);

  temPed = 0; relA = 0.0f; escondida = 0;
  if (A < 0.01f) { ultOk = 0; W = H = 0.0f; vW = vH = 0.0f; return; }

  x = dir ? NV_TELA_W - X_ESQ : X_ESQ;
  { float w = (W < H && H <= CAB_H) ? H : W, h = H < 8.0f ? 8.0f : H;
    GfxRect R = { dir ? x - w : x, Y_TOPO, w, h };
    float raioPx = h * 0.5f;
    cresce = anim_clamp((h - pilH) / 120.0f, 0.0f, 1.0f);
    if (raioPx > RAIO_CORPO * cresce + h * 0.5f * (1.0f - cresce)) raioPx = RAIO_CORPO * cresce + h * 0.5f * (1.0f - cresce);
    if (raioPx > h * 0.5f) raioPx = h * 0.5f;
    ultRect = R; ultOk = 1;
    // PONTEIRO (#99): a pilula que pediu clique (ponteiroOk), so assentada —
    // sem corpo, opaca e no tamanho final. Sem camada propria: o alvo cai na
    // camada de quem estiver por cima no player, como a pilula cai na tela.
    // O corpo de uma folha (faixas, fontes, episodios) registra os seus alvos
    // ele mesmo, na camada da folha, quando a ilha o chama abaixo.
    if (p->ponteiroOk && !corpo && A > 0.99f && fabsf(W - alvoW) < 1.0f && fabsf(H - alvoH) < 1.0f)
      ponteiro_alvo(R.x, R.y, R.w, R.h, p->ponteiroOk, NULL, 0, 0);
    if (temUlt && ult.video) plrui_material_video(R, raioPx, A);
    else plrui_material_s(R, raioPx, (temUlt ? ult.modal : 0), A, temUlt ? ult.semSombra : 0);
    central_segurar_desenhar(R, raioPx / (R.h > 1.0f ? R.h : 1.0f), A, agora);
    gfx_recorte(R.x, R.y, R.w, R.h);
    { float cabH = h < CAB_H ? h : CAB_H;
      float yc = R.y + cabH * 0.5f;
      // A linha: centrada na pilula; com corpo, encostada a DIREITA do
      // cabecalho — dono, 11/10: o relogio do lado direito nos paineis que
      // nascem da ilha. `insetEsq` (#205) tira so o TEXTO da conta: ele fica
      // no recuo pedido, a esquerda, e o relogio (com o "termina as") continua
      // encostado a direita — a linha inteira caberia de menos no painel.
      float ie, k;
      { float lx = R.x + (R.w - L.w) * 0.5f;
        int ins = p->insetEsq > 0.0f && L.temTxt;
        float wAnc = L.w - (ins ? (float)L.txt.w + 12.0f : 0.0f);
        float ex = R.x + R.w - 28.0f - wAnc;
        ie = p->insetEsq > 0.0f ? p->insetEsq : 28.0f;
        k = anim_clamp((R.w - (wAnc + pad * 2.0f)) / 40.0f, 0.0f, 1.0f);
        lx = lx + (ex - lx) * k;
        desenharLinha(p, &L, lx, yc, A * textoA, ins ? (R.x + ie - lx) * k : 0.0f, ins ? k : 0.0f); }
      if (p->direita && k > 0.0f)
        txt_desenhar_alpha(L.dir, R.x + ie, yc - (float)L.dir.h * 0.5f + 1.0f, A * k);
      // Sem `direita`, o lado esquerdo leva o titulo, cortado na largura
      // final do painel (a da mola mudaria a textura a cada quadro).
      else if (titulo[0] && k > 0.0f && !(p->insetEsq > 0.0f)) {
        float max = alvoW - L.w - 28.0f - ie - 24.0f;
        if (max > 40.0f) {
          TxtLinha t = txt_linha_corta(TXT_G19M, titulo, 243, 242, 239, 200, max);
          txt_desenhar_alpha(t, R.x + ie, yc - (float)t.h * 0.5f + 1.0f, A * k);
        }
      }
      // Fio de 1 px sob o cabecalho, so com o corpo de pe.
      if (h > CAB_H + 4.0f && dsP == DS_DESLIGADO)
        gfx_cor((GfxRect){ R.x, R.y + CAB_H, R.w, 1.0f }, 0.0f, 1, 1, 1, 0.07f * A * cresce); }
    if (temUlt && corpoA > 0.01f && ult.corpo && h > CAB_H + 4.0f) {
      GfxRect c = { R.x, R.y + CAB_H, R.w, h - CAB_H };
      ult.corpo(c, A * corpoA, ult.u);
    }
    if (dsP != DS_DESLIGADO && dsA > 0.01f && h > CAB_H + 4.0f)
      desempenho_corpo(dsP, (GfxRect){ R.x, R.y + CAB_H, R.w, h - CAB_H }, A * dsA);
    gfx_sem_recorte(); }
}
