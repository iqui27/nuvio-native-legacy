// Cartao de NOVIDADES DA 1.7: a ilha do relogio, o Spotlight e o layout Apple TV.
//
// O MOLDE E O DA 1.4.8, o da 1.6.0 e o da 1.6.6 (que nunca saiu e virou este):
// a esquerda uma PREVIA VIVA, desenhada com as pecas de verdade do app
// (botoes.h, badges.h, a marca do guia, a tecla da central de salvosintro.h)
// nas medidas dos componentes reais — a ilha com as molas e os tamanhos de
// ilha.c, o modal dela, o painel de Salvos que nasce dela, a caixa do
// Spotlight, a barra e o carrossel da Dinamica; a direita o titulo e uma lista
// CURTA (so o essencial, pedido do dono), cada linha com icone, nome e o que
// mudou; no rodape duas pilulas. Tres cenas trocam sozinhas.
//
// POR QUE COPIAS NA ESCALA DA PREVIA e nao os modulos de verdade: ilha.c,
// spotlight.c e salvospainel.c sao estado global da tela (fila de avisos,
// cartoes, foco, consulta) com posicoes de tela cheia (o modal tem 1120 px, a
// caixa do Spotlight 1480). Chamar os de verdade daqui mexeria no estado do
// app debaixo do cartao. As copias usam as mesmas fontes, cores, raios, a
// mesma mola subamortecida (ILHA_MOLA_* de ilha.c, ja nas velocidades mais
// lentas) e os mesmos textos (as chaves de i18n sao as deles).
//
// DADOS, NAO CODIGO. As cenas (CENAS) e as linhas (ITENS) sao tabelas: cada
// recurso e UMA entrada, e apagar uma linha reorganiza o cartao sozinho.
//
// CUSTO (LG C9): texto so por txt_linha/txt_bloco, que guardam a textura por
// (estilo, cor, texto) — cor de texto NUNCA muda por quadro, so o alfa. A lista
// abre pelo portao de texto (textogate.h). A cena SEGUINTE e desenhada uma vez
// numa tesoura de 1 px, com alfa quase nulo, para as linhas dela ja existirem
// quando ela entrar. Cada arquivo de arte e pedido numa largura so.
//
// A MARCA E "novidades-170-ui.txt" (N170_ARQ). O numero da versao mora so em
// N170_VERSAO (novidades170.h). Quem viu o cartao da 1.6.6 nao existe: ele
// nunca saiu.
#include "novidades170.h"
#include "ajustes.h"
#include "anim.h"
#include "badges.h"
#include "botoes.h"
#include "dados.h"
#include "gfx.h"
#include "idioma.h"
#include "idiomacod.h"
#include "layout.h"
#include "menu.h"
#include "ponteiro.h"
#include "salvosintro.h"
#include "teclado.h"
#include "tex_cache.h"
#include "text.h"
#include "textogate.h"
#define NV_ESCALA_TELA_ATIVA   // mede pela tela do fator ativo (escala.h)
#include "escala.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define N170_ARQ          "novidades-170-ui.txt"
#define N170_W            1720.0f
#define N170_H            1000.0f
#define N170_X            ((NV_TELA_W - N170_W) * 0.5f)
#define N170_Y            ((NV_TELA_H - N170_H) * 0.5f)
#define N170_PAD            52.0f
#define N170_RAIO           32.0f
// A previa: a coluna da esquerda inteira, acima do rodape.
#define N170_PV_W          760.0f
#define N170_PV_H          (N170_H - 2.0f * N170_PAD - BOTAO_H_PRIMARIO - 28.0f)
#define N170_PV_RAIO        26.0f
#define N170_COL_GAP        64.0f
#define N170_TXT_X        (N170_X + N170_PAD + N170_PV_W + N170_COL_GAP)
#define N170_TXT_W        (N170_X + N170_W - N170_PAD - N170_TXT_X)
#define N170_ICONE          40.0f
#define N170_ABRIR_MS      280.0f
#define N170_FECHAR_MS     160.0f
#define N170_TRANSICAO_S     0.55f
// Margem da cena: o selo com o nome e os tracos do ciclo ocupam o alto.
#define N170_M              48.0f
// Largura em que toda arte de FUNDO e pedida (um recorte "cover" de um quadro
// 16:9 da altura da previa): um so tamanho = uma textura por arquivo.
#define N170_ARTE_W       1280.0f
#define N170_CAPA_W         90.0f   // cartazes e fotos pequenas
#define N170_LOGO_W        330.0f
#define N170_EP_W          480.0f   // o still do episodio (a largura de ilha.c)

enum { B_DEPOIS = 0, B_SPOT = 1, B_N };

// De que assunto cada cena e cada linha falam: a linha da cena na tela ganha o
// disco no realce. Um id e nao um indice, para apagar uma entrada sem desalinhar.
enum { ID_NADA = 0, ID_ILHA, ID_SPOT, ID_SALVOS, ID_ATV, ID_LIVE, ID_CONSERTOS };

static int   aberto, decidido, foco = B_SPOT, naPrevia, pedido;
static int   cena, cenaAntiga;
static float entrada, transicao = 1.0f, relogioCena, tempoAntiga;
static char  dirArte[512] = "deploy/app/art";
static TextoGate gateLista;
static unsigned aquecida;   // bit por cena: as linhas dela ja estao no cache

// ------------------------------------------------------------------ a arte
//
// Tudo do pacote (deploy/app/art, ver catalogo.txt), sem rede: fundos NN.jpg,
// cartazes poster/NN.jpg, logos logo/NN.png (o mesmo NN = o mesmo titulo),
// stills ep/NN_T_EE.jpg e fotos elenco/NN_k.jpg.
enum {
  // fundos (N170_ARTE_W)
  A_HOME, A_R1, A_R2, A_R3, A_CE, A_C0, A_C1, A_C2, A_MERCY, A_MANIAC,
  A_FIM_FUNDO,
  // cartazes e fotos (N170_CAPA_W)
  A_P_FALLOUT = A_FIM_FUNDO, A_P_HAIL, A_P_LOST, A_P_WIDOW, A_P_3BODY, A_P_MRK,
  A_P_MANIAC, A_P_MARTIAN, A_P_MARSH, A_F_MERYL, A_F_MARK,
  A_FIM_CAPA,
  // logos (N170_LOGO_W)
  A_L_HOME = A_FIM_CAPA, A_L_C0, A_L_C1, A_L_C2, A_L_FALLOUT,
  A_FIM_LOGO,
  A_EP_FALLOUT = A_FIM_LOGO,   // N170_EP_W
  A_N
};
static const char *const ARQ[A_N] = {
  "03.jpg", "13.jpg", "12.jpg", "07.jpg", "06.jpg", "07.jpg", "13.jpg", "12.jpg", "16.jpg", "39.jpg",
  "poster/00.jpg", "poster/13.jpg", "poster/21.jpg", "poster/01.jpg", "poster/15.jpg", "poster/38.jpg",
  "poster/39.jpg", "poster/12.jpg", "poster/28.jpg", "elenco/07_0.jpg", "elenco/11_2.jpg",
  "logo/03.png", "logo/07.png", "logo/13.png", "logo/12.png", "logo/00.png",
  "ep/00_1_03.jpg",
};
static char arq[A_N][600];

static void montarCaminhos(void) {
  int i;
  for (i = 0; i < A_N; i++) snprintf(arq[i], sizeof arq[i], "%s/%s", dirArte, ARQ[i]);
}
static float larguraDe(int i) {
  return i < A_FIM_FUNDO ? N170_ARTE_W : i < A_FIM_CAPA ? N170_CAPA_W
       : i < A_FIM_LOGO ? N170_LOGO_W : N170_EP_W;
}
static GLuint tex(int i) { return tex_obter_larg(arq[i], larguraDe(i)); }
static void pedirArtes(void) { int i; for (i = 0; i < A_N; i++) tex(i); }

// Raio em pixels -> fracao do menor lado (a unidade de gfx_rect).
static float rr(float px, GfxRect r) { float m = r.w < r.h ? r.w : r.h; return m > 0.0f ? px / m : 0.0f; }

// Arte de fundo em "cover", com o veu de leitura (esquerda e base) assado na
// mesma passada: GFX_VITRINE, o modo dos destaques da home.
static void arte(int f, GfxRect r, float raioPx, float veu, float a) {
  GLuint t;
  if (a <= 0.003f) return;
  t = tex(f);
  if (!t) { gfx_cor(r, rr(raioPx, r), 0.10f, 0.11f, 0.13f, a); return; }
  gfx_tex_aspect_atual = tex_aspecto(arq[f]);
  if (gfx_tex_aspect_atual <= 0.0f) gfx_tex_aspect_atual = 16.0f / 9.0f;
  gfx_rect(r, t, GFX_VITRINE, veu, 0.35f, 0.0f, rr(raioPx, r), 0, 0, 0, a);
  gfx_tex_aspect_atual = 0.0f;
}

// Cartaz, still ou foto num cartao (cover), com esqueleto enquanto nao chega.
static void capa(int i, GfxRect r, float raioPx, float a) {
  GLuint t;
  if (a <= 0.003f) return;
  t = tex(i);
  if (!t) { gfx_cor(r, rr(raioPx, r), NV_COR_ESQUELETO_R, NV_COR_ESQUELETO_G, NV_COR_ESQUELETO_B, a); return; }
  gfx_tex_aspect_atual = tex_aspecto(arq[i]);
  gfx_card_forcar_cover_atual = 1.0f;
  gfx_rect(r, t, GFX_CARD, 0, 0, 0, rr(raioPx, r), 0, 0, 0, a);
  gfx_card_forcar_cover_atual = 0.0f;
  gfx_tex_aspect_atual = 0.0f;
}

// O logo do titulo com a base em `yBase`, cabendo em maxW x maxH.
static void logoTitulo(int l, float x, float yBase, float maxW, float maxH, float a) {
  GLuint t;
  float ap, w, h;
  if (a <= 0.003f || l < 0) return;
  t = tex(l);
  if (!t) return;
  ap = tex_aspecto(arq[l]);
  if (ap <= 0.0f) ap = 3.0f;
  w = maxW; h = w / ap;
  if (h > maxH) { h = maxH; w = h * ap; }
  gfx_tex_aspect_atual = 0.0f;
  gfx_rect((GfxRect){ x, yBase - h, w, h }, t,
           tex_marca_escura(arq[l]) ? GFX_MARCA : GFX_TEXTO, 0, 0, 0, 0.0f, 1, 1, 1, a);
}

// Tesoura da cena atual (a previa, ou a interseccao com ela).
static GfxRect clipCena;
static void recorte(GfxRect r) { gfx_recorte(r.x, r.y, r.w, r.h); }
static void recorteDentro(GfxRect r) {
  float x0 = fmaxf(r.x, clipCena.x), y0 = fmaxf(r.y, clipCena.y);
  float x1 = fminf(r.x + r.w, clipCena.x + clipCena.w), y1 = fminf(r.y + r.h, clipCena.y + clipCena.h);
  gfx_recorte(x0, y0, fmaxf(0.0f, x1 - x0), fmaxf(0.0f, y1 - y0));
}
static void recorteVolta(void) { recorte(clipCena); }

// Passagem de 0 a 1 com saida suave a partir de `ini`, durando `dur` segundos.
static float passo(float t, float ini, float dur) {
  return anim_suave(anim_clamp((t - ini) / dur, 0.0f, 1.0f));
}
// A MOLA da barra, como funcao do tempo (a previa e determinista: a captura
// pula para qualquer instante): passa ~6 % do alvo e assenta.
static float mola(float t, float ini, float dur) {
  float u = (t - ini) / dur;
  if (u <= 0.0f) return 0.0f;
  if (u >= 1.0f) return 1.0f;
  return 1.0f - expf(-7.0f * u) * cosf(8.0f * u);
}
// A MOLA SUBAMORTECIDA DA ILHA (ilha.c, molaIlhaWZ) em forma fechada: a
// resposta ao degrau de x'' = w^2 (1 - x) - 2 z w x'. Mesmos w e zeta da
// ilha de verdade, entao o "pulo" da previa e o da tela.
#define N170_ILHA_W   10.0f    // = ILHA_MOLA_W
#define N170_ILHA_Z    0.72f   // = ILHA_MOLA_Z
#define N170_MODAL_W   7.5f    // = MODAL_MOLA_W
#define N170_MODAL_Z   0.80f   // = MODAL_MOLA_Z
static float molaSub(float t, float ini, float w, float z) {
  float u = t - ini, wd, k;
  if (u <= 0.0f) return 0.0f;
  wd = w * sqrtf(1.0f - z * z);
  k = z / sqrtf(1.0f - z * z);
  return 1.0f - expf(-z * w * u) * (cosf(wd * u) + k * sinf(wd * u));
}
static float mistura(float a, float b, float s) { return a + (b - a) * s; }
static GfxRect misturaRect(GfxRect a, GfxRect b, float s) {
  GfxRect r = { mistura(a.x, b.x, s), mistura(a.y, b.y, s), mistura(a.w, b.w, s), mistura(a.h, b.h, s) };
  return r;
}

// "2015|f" -> "2015  ·  Filme", "2004|s" -> "2004  ·  Série": o ano e numero,
// o tipo passa por i18n (as chaves de spotlight.c).
static void anoTipo(const char *m, char *b, size_t n) {
  const char *bar = strchr(m, '|');
  if (!bar) { snprintf(b, n, "%s", m); return; }
  snprintf(b, n, "%.*s  \xc2\xb7  %s", (int)(bar - m), m, bar[1] == 's' ? i18n("Série") : i18n("Filme"));
}

// Ano e duracao do destaque: numeros, iguais em todo idioma.
static const char *const META_HOME = "2025  ·  2h 42min";

// A home da Dinamica atras das cenas: o destaque (logo, nota, botoes) e a
// fileira na base. `ah` apaga so o bloco do destaque.
static void homeDinamica(float x, float y, float ah, float a) {
  const float H = N170_PV_H;
  GfxRect pv = { x, y, N170_PV_W, H };
  float yb = y + H - 24.0f - 113.0f;          // topo da fileira
  float yl = yb - 152.0f;                     // base do logo
  const char *rp = i18n("Reproduzir");
  float wp = botao_largura(rp, "play", 1);
  TxtLinha meta = txt_linha(TXT_DET_META2, META_HOME, 214, 218, 226, 255);
  int i;
  arte(A_HOME, pv, N170_PV_RAIO, 0.9f, a);
  logoTitulo(A_L_HOME, x + N170_M, yl, 300.0f, 104.0f, ah);
  { float wi = badge_imdb(x + N170_M, yl + 16.0f, 77, 0, ah);
    txt_desenhar_alpha(meta, x + N170_M + wi + 16.0f, yl + 16.0f + (BADGE_H - (float)meta.h) * 0.5f, ah); }
  botao_pilula((GfxRect){ x + N170_M, yl + 62.0f, wp, BOTAO_H_SECUNDARIO }, rp, "play", 1.0f, 1, 0, ah);
  botao_disco((GfxRect){ x + N170_M + wp + 14.0f, yl + 62.0f, BOTAO_H_SECUNDARIO, BOTAO_H_SECUNDARIO },
              "mais", 0.0f, ah);
  for (i = 0; i < 3; i++)
    arte(A_R1 + i, (GfxRect){ x + N170_M + (float)i * 222.0f, yb, 200.0f, 113.0f }, 14.0f, 0.25f, a);
}

// ================================================= CENA 0: A ILHA DO RELOGIO
//
// A home atras, a ilha no canto de cima a direita (onde a Dinamica a poe). O
// relogio se abre para o episodio largado no meio (mini capa, tempo que
// falta, a barra), a tecla da central acende e a pilula CRESCE ate o modal
// (o still, Retomar/Detalhes/Fechar e "Salvos ›"). O foco anda pelos botoes,
// a direita do ultimo faz o painel de Salvos nascer do modal, ele recolhe de
// volta para a pilula e, no fim, a vez e da estreia nao vista (o ponto).
// Medidas de ilha.c; o modal na largura da previa.
#define IL_PAD_E     22.0f
#define IL_PAD_D     24.0f
#define IL_ICONE     28.0f
#define IL_VAO       12.0f
#define IL_CAPA_W    30.0f
#define IL_CAPA_H    44.0f
#define IL_CT_VAO    14.0f
#define IL_TIT_MAX  200.0f
#define IL_MD_PAD    24.0f
#define IL_MD_ARTE_W 300.0f
#define IL_MD_ARTE_H (IL_MD_ARTE_W * 9.0f / 16.0f)
#define IL_MD_RAIO   30.0f
#define IL_MD_H      (IL_MD_PAD * 2.0f + IL_MD_ARTE_H + 20.0f + BOTAO_H_SECUNDARIO)
#define IL_PN_W     520.0f
#define IL_T_CARTAO  0.6f    // o relogio se abre para o episodio
#define IL_T_TECLA   1.9f    // a tecla da central aparece embaixo
#define IL_T_MODAL   2.8f    // e e apertada: a pilula cresce ate o modal
#define IL_T_FOCO1   4.6f    // Detalhes
#define IL_T_FOCO2   5.3f    // Fechar
#define IL_T_SALVOS  6.0f    // "Salvos ›" acende
#define IL_T_PAINEL  6.35f   // a direita do ultimo: o painel nasce do modal
#define IL_T_RECOLHE 9.3f    // Voltar: ele recolhe para a pilula
#define IL_T_ESTREIA 10.6f   // a vez da estreia
#define IL_DUR      13.2f

// Nomes proprios e numeros: iguais em todo idioma.
static const char *const TIT_VIVO = "Fallout", *const TIT_ESTREIA = "Widow's Bay";
// Os tres conteudos da pilula: relogio, o episodio largado, a estreia.
enum { IL_RELOGIO = 0, IL_VIVO, IL_ESTREIA };
typedef struct { TxtLinha hora, tit, meta; float w, h; } IlhaLin;
static void ilhaLinhas(int k, IlhaLin *L) {
  char meta[96];
  memset(L, 0, sizeof *L);
  L->hora = txt_linha(TXT_PG_RELOGIO, "21:40", 244, 245, 248, 255);
  if (k == IL_RELOGIO) { L->w = IL_PAD_E + IL_PAD_D + (float)L->hora.w; L->h = NV_ILHA_H; return; }
  if (k == IL_VIVO) {
    snprintf(meta, sizeof meta, i18n("T%dE%d · %d min restantes"), 1, 3, 32);
    L->tit = txt_linha_corta(TXT_BODY, TIT_VIVO, 240, 242, 246, 255, IL_TIT_MAX);
  } else {
    char te[32];
    snprintf(te, sizeof te, i18n("T%dE%d"), 2, 5);
    snprintf(meta, sizeof meta, "%s · %s", te, i18n("hoje"));
    L->tit = txt_linha_corta(TXT_BODY, TIT_ESTREIA, 240, 242, 246, 255, IL_TIT_MAX);
  }
  L->meta = txt_linha(TXT_CAPTION2, meta, 176, 180, 190, 255);
  L->w = IL_PAD_E + IL_PAD_D + (float)L->hora.w + IL_CT_VAO * 2.0f + 1.5f +
         (k == IL_VIVO ? IL_CAPA_W : IL_ICONE) + IL_VAO + (float)L->tit.w + 10.0f + (float)L->meta.w +
         (k == IL_ESTREIA ? 22.0f : 0.0f);
  L->h = NV_ILHA_H_ABERTA;
}

// O conteudo da pilula em `r` (desenharConteudo / desenharCartao de ilha.c).
static void ilhaConteudo(int k, GfxRect r, float a) {
  IlhaLin L;
  float cw, x, yc = r.y + r.h * 0.5f, cr, cg, cb, xTexto, yt;
  if (a < 0.01f) return;
  ilhaLinhas(k, &L);
  cw = L.w - IL_PAD_E - IL_PAD_D;
  x = r.x + (r.w - cw) * 0.5f;
  txt_desenhar_alpha(L.hora, x, yc - (float)L.hora.h * 0.5f, a);
  if (k == IL_RELOGIO) return;
  ajustes_acento(&cr, &cg, &cb);
  yt = k == IL_VIVO ? yc - 3.0f : yc;
  x += (float)L.hora.w + IL_CT_VAO;
  gfx_cor((GfxRect){ x, yc - 13.0f, 1.5f, 26.0f }, 0.0f, 1.0f, 1.0f, 1.0f, 0.22f * a);
  x += 1.5f + IL_CT_VAO;
  if (k == IL_VIVO) {
    capa(A_P_FALLOUT, (GfxRect){ x, yc - IL_CAPA_H * 0.5f, IL_CAPA_W, IL_CAPA_H }, 6.0f, a);
    x += IL_CAPA_W + IL_VAO;
  } else {
    gfx_icone((GfxRect){ x, yc - IL_ICONE * 0.5f, IL_ICONE, IL_ICONE }, "lembrete", cr, cg, cb, a);
    x += IL_ICONE + IL_VAO;
  }
  xTexto = x;
  txt_desenhar_alpha(L.tit, x, yt - (float)L.tit.h * 0.5f, a);
  x += (float)L.tit.w + 10.0f;
  txt_desenhar_alpha(L.meta, x, yt - (float)L.meta.h * 0.5f, a);
  x += (float)L.meta.w;
  if (k == IL_ESTREIA) gfx_cor((GfxRect){ x + 12.0f, yc - 5.0f, 10.0f, 10.0f }, 0.5f, cr, cg, cb, a);
  else {
    GfxRect tr = { xTexto, r.y + r.h - 12.0f, x - xTexto, 3.0f };
    gfx_cor(tr, 0.5f, 1.0f, 1.0f, 1.0f, 0.14f * a);
    gfx_cor((GfxRect){ tr.x, tr.y, tr.w * 0.42f, 3.0f }, 0.5f, cr, cg, cb, a);
  }
}

// A superficie da ilha (pilula ou modal): sombra curta, vidro ou chapa, e a
// luz de canto. `k` 0..1 = o quanto ela ja e modal.
static void ilhaSuperficie(GfxRect R, float raio, float k, float a) {
  gfx_rect((GfxRect){ R.x - 16.0f - 24.0f * k, R.y - 6.0f - 10.0f * k,
                      R.w + 32.0f + 48.0f * k, R.h + 34.0f + 50.0f * k }, 0, GFX_SOMBRA,
           1.0f, 0, 0, 0.5f, 0, 0, 0, (0.34f + 0.16f * k) * a);
  gfx_vidro_ou_solido(R, raio, GFX_VIDRO_DENS + 0.08f * k, a, 0.055f, 0.058f, 0.068f, 0.86f + 0.08f * k);
  gfx_luz_canto(R, raio, R.w * 0.25f, -R.h * 0.9f, R.w * 0.85f, 1, 1, 1, 0.07f * a);
}

// O modal (desenharModal de ilha.c, na largura da previa): o still, o logo,
// T/E e o nome, o tempo que falta com a barra, os botoes e "Salvos ›".
static const char *const MD_ROT[3] = { "Retomar", "Detalhes", "Fechar" };
static const char *const MD_IC[3] = { "play", "aj_info", NULL };
static void ilhaModal(GfxRect m, float t, float a) {
  float ax = m.x + IL_MD_PAD, ay = m.y + IL_MD_PAD;
  float cx = ax + IL_MD_ARTE_W + 24.0f, cw = m.x + m.w - IL_MD_PAD - cx;
  float y = ay, by = ay + IL_MD_ARTE_H + 20.0f, cr, cg, cb, x;
  float sv = passo(t, IL_T_SALVOS, 0.2f);
  int i;
  if (a < 0.01f) return;
  ajustes_acento(&cr, &cg, &cb);
  capa(A_EP_FALLOUT, (GfxRect){ ax, ay, IL_MD_ARTE_W, IL_MD_ARTE_H }, 16.0f, a);
  logoTitulo(A_L_FALLOUT, cx, y + 56.0f, cw, 56.0f, a);
  y += 56.0f + 12.0f;
  { char b[200];
    TxtLinha l;
    snprintf(b, sizeof b, i18n("T%dE%d · %s"), 1, 3, "The Head");
    l = txt_linha_corta(TXT_CALLOUT, b, 226, 228, 234, 255, cw);
    txt_desenhar_alpha(l, cx, y, a); }
  { char b[96];
    TxtLinha l;
    float sy, bx, bw;
    snprintf(b, sizeof b, i18n("%d min restantes"), 32);
    l = txt_linha(TXT_CAPTION, b, 200, 204, 212, 255);
    sy = ay + IL_MD_ARTE_H - (float)l.h;
    txt_desenhar_alpha(l, cx, sy, a);
    bx = cx + (float)l.w + 16.0f; bw = cx + cw - bx;
    if (bw > 30.0f) {
      GfxRect tr = { bx, sy + (float)l.h * 0.5f - 2.0f, bw, 4.0f };
      gfx_cor(tr, 0.5f, 1.0f, 1.0f, 1.0f, 0.14f * a);
      gfx_cor((GfxRect){ tr.x, tr.y, tr.w * 0.42f, 4.0f }, 0.5f, cr, cg, cb, a);
    } }
  // O foco anda: Retomar, Detalhes, Fechar; depois sai para "Salvos ›".
  x = ax;
  for (i = 0; i < 3; i++) {
    float f;
    const char *rot = i18n(MD_ROT[i]);
    float w = botao_largura(rot, MD_IC[i], i == 0);
    if (x + w > m.x + m.w - IL_MD_PAD) break;   // idioma longo: o botao que nao cabe sai
    if (i == 0) f = 1.0f - passo(t, IL_T_FOCO1, 0.18f);
    else if (i == 1) f = passo(t, IL_T_FOCO1, 0.18f) * (1.0f - passo(t, IL_T_FOCO2, 0.18f));
    else f = passo(t, IL_T_FOCO2, 0.18f) * (1.0f - sv);
    botao_pilula((GfxRect){ x, by, w, BOTAO_H_SECUNDARIO }, rot, MD_IC[i], f, i == 0, 0, a);
    x += w + BOTAO_GAP;
  }
  { TxtLinha s = txt_linha(TXT_CAPTION2, i18n("Salvos"), 176, 180, 190, 255);
    TxtLinha v = txt_linha(TXT_CALLOUT, "\xE2\x80\xBA", 176, 180, 190, 255);
    TxtLinha sf = txt_linha(TXT_CAPTION2, i18n("Salvos"), 250, 251, 253, 255);
    TxtLinha vf = txt_linha(TXT_CALLOUT, "\xE2\x80\xBA", 250, 251, 253, 255);
    float xr = m.x + m.w - IL_MD_PAD, yc = by + BOTAO_H_SECUNDARIO * 0.5f;
    float x0 = xr - (float)v.w - 8.0f - (float)s.w;
    // Botoes longos (ru, de): a palavra sai e fica so a seta; sem lugar nem
    // para ela, nada — texto por cima de botao nunca.
    float sa = 1.0f;
    if (x0 < x - BOTAO_GAP + 12.0f) { sa = 0.0f; x0 = xr - (float)v.w; }
    if (x0 < x - BOTAO_GAP + 12.0f) return;
    txt_desenhar_alpha(s, x0, yc - (float)s.h * 0.5f, a * 0.9f * (1.0f - sv) * sa);
    txt_desenhar_alpha(v, xr - (float)v.w, yc - (float)v.h * 0.5f - 2.0f, a * 0.9f * (1.0f - sv));
    txt_desenhar_alpha(sf, x0, yc - (float)s.h * 0.5f, a * sv * sa);
    txt_desenhar_alpha(vf, xr - (float)v.w + 4.0f * sv, yc - (float)v.h * 0.5f - 2.0f, a * sv); }
}

// O PAINEL DE SALVOS na previa (salvospainel.c): as abas, a contagem, a barra
// de opcoes do "organizar" (Ordenar, Agrupar, Estilo, Categorias) e a lista
// agrupada — o episodio para retomar e uma categoria da pessoa.
typedef struct { int capa; const char *titulo, *meta; int traduz, prog; } LinhaSalvo;
static const LinhaSalvo SALVOS_LIN[4] = {
  { A_P_FALLOUT, "Fallout",           NULL,                   0, 1 },
  { A_P_HAIL,    "Project Hail Mary", "2026|f", 1, 0 },
  { A_P_LOST,    "Lost",              "2004|s", 1, 0 },
  { A_P_WIDOW,   "Widow's Bay",       "2026|s", 1, 0 },
};
static void painelSalvos(GfxRect P, float a) {
  float x = P.x + 24.0f, y = P.y + 22.0f, w = P.w - 48.0f, cr, cg, cb;
  int i;
  if (a < 0.01f) return;
  ajustes_acento(&cr, &cg, &cb);
  // Abas e a contagem.
  { TxtLinha s = txt_linha(TXT_CALLOUT, i18n("Salvos"), 246, 247, 252, 255);
    TxtLinha av = txt_linha(TXT_CALLOUT, i18n("Avisos"), 160, 164, 176, 255);
    char b[96];
    TxtLinha c;
    snprintf(b, sizeof b, "%d %s · %d %s", 4, i18n("títulos"), 1, i18n("para retomar"));
    c = txt_linha_corta(TXT_CAPTION2, b, 160, 164, 176, 255, w - (float)s.w - (float)av.w - 40.0f);
    txt_desenhar_alpha(s, x, y, a);
    gfx_cor((GfxRect){ x - 2.0f, y + (float)s.h + 4.0f, (float)s.w + 4.0f, 3.0f }, 0.5f, cr, cg, cb, a);
    txt_desenhar_alpha(av, x + (float)s.w + 22.0f, y, a);
    txt_desenhar_alpha(c, x + w - (float)c.w, y + ((float)s.h - (float)c.h) * 0.5f, a);
    y += (float)s.h + 24.0f; }
  // A barra de opcoes: o organizar mora aqui.
  { static const char *const OPC[4] = { "Ordenar", "Agrupar", "Estilo", "Categorias" };
    float bx = x;
    for (i = 0; i < 4; i++) {
      TxtLinha l = txt_linha(TXT_CAPTION2, i18n(OPC[i]), 222, 224, 230, 255);
      GfxRect p = { bx, y, (float)l.w + 32.0f, 38.0f };
      if (p.x + p.w > x + w) break;
      gfx_cor(p, 0.5f, 1.0f, 1.0f, 1.0f, 0.09f * a);
      txt_desenhar_alpha(l, p.x + 16.0f, p.y + (p.h - (float)l.h) * 0.5f, a);
      bx += p.w + 10.0f;
    }
    y += 38.0f + 22.0f; }
  // A lista: "Continuar assistindo" e a categoria "Fim de semana".
  for (i = 0; i < 4; i++) {
    const LinhaSalvo *L = &SALVOS_LIN[i];
    GfxRect c;
    TxtLinha t, m;
    char meta[96];
    if (i == 0 || i == 1) {
      TxtLinha h = txt_linha(TXT_CW_BADGE, i18n(i == 0 ? "Continuar assistindo" : "Fim de semana"),
                             150, 156, 168, 255);
      txt_desenhar_alpha(h, x, y, a);
      y += (float)h.h + 12.0f;
    }
    c = (GfxRect){ x, y, 60.0f, 90.0f };
    capa(L->capa, c, 8.0f, a);
    if (L->meta) {
      // "2026|f": o ano e numero, o tipo passa por i18n.
      anoTipo(L->meta, meta, sizeof meta);
    } else snprintf(meta, sizeof meta, i18n("T%dE%d · %d min restantes"), 1, 3, 32);
    t = txt_linha_corta(TXT_CALLOUT, L->titulo, 240, 242, 246, 255, w - 80.0f);
    m = txt_linha_corta(TXT_CAPTION2, meta, 160, 164, 176, 255, w - 80.0f);
    txt_desenhar_alpha(t, c.x + c.w + 20.0f, y + 14.0f, a);
    txt_desenhar_alpha(m, c.x + c.w + 20.0f, y + 14.0f + (float)t.h + 4.0f, a);
    if (L->prog) {
      GfxRect tr = { c.x + c.w + 20.0f, y + 14.0f + (float)t.h + 4.0f + (float)m.h + 12.0f, 180.0f, 5.0f };
      gfx_cor(tr, 0.5f, 1.0f, 1.0f, 1.0f, 0.14f * a);
      gfx_cor((GfxRect){ tr.x, tr.y, tr.w * 0.42f, tr.h }, 0.5f, cr, cg, cb, a);
    }
    y += c.h + (i == 0 ? 22.0f : 14.0f);
  }
}

static void cenaIlha(float x, float y, float t, float a) {
  const float W = N170_PV_W, H = N170_PV_H;
  const float xd = x + W - 24.0f, yI = y + 88.0f;
  IlhaLin L0, L1, L2;
  float sM, ePn;
  GfxRect pil, M, P;
  int kAntes, kAgora;
  float tTroca;
  ilhaLinhas(IL_RELOGIO, &L0);
  ilhaLinhas(IL_VIVO, &L1);
  ilhaLinhas(IL_ESTREIA, &L2);

  // ---- a pilula: relogio -> episodio -> (modal, painel) -> episodio -> estreia.
  if (t < IL_T_ESTREIA) { kAntes = IL_RELOGIO; kAgora = IL_VIVO; tTroca = IL_T_CARTAO; }
  else { kAntes = IL_VIVO; kAgora = IL_ESTREIA; tTroca = IL_T_ESTREIA; }
  { const IlhaLin *la = kAntes == IL_RELOGIO ? &L0 : &L1, *lb = kAgora == IL_VIVO ? &L1 : &L2;
    float s = molaSub(t, tTroca, N170_ILHA_W, N170_ILHA_Z);
    float w = mistura(la->w, lb->w, s), h = mistura(la->h, lb->h, s);
    pil = (GfxRect){ xd - w, yI, w, h }; }

  // ---- a home atras; com o painel aberto, o veu dele (SP_VEU) por cima.
  ePn = t >= IL_T_PAINEL ? anim_clamp(molaSub(t, IL_T_PAINEL, N170_MODAL_W, N170_MODAL_Z), 0.0f, 1.06f)
                           * (1.0f - passo(t, IL_T_RECOLHE, 0.42f)) : 0.0f;
  homeDinamica(x, y, a, a);
  if (ePn > 0.0f)
    gfx_cor((GfxRect){ x, y, W, H }, rr(N170_PV_RAIO, (GfxRect){ x, y, W, H }), 0, 0, 0,
            0.58f * anim_clamp(ePn, 0.0f, 1.0f) * a);

  // ---- o modal: a mesma pilula crescida, na mola do modal.
  M = (GfxRect){ x + 24.0f, yI, W - 48.0f, IL_MD_H };
  sM = t < IL_T_PAINEL ? molaSub(t, IL_T_MODAL, N170_MODAL_W, N170_MODAL_Z) : 0.0f;
  if (sM > 1.06f) sM = 1.06f;
  // O painel: nasce do retangulo do modal e recolhe para o da pilula.
  P = (GfxRect){ x + W - 24.0f - IL_PN_W, yI, IL_PN_W, H - (yI - y) - 24.0f };

  if (t >= IL_T_PAINEL && t < IL_T_RECOLHE + 0.42f) {
    // O PAINEL no lugar da pilula (ilha_coberta): a forma vai do modal (ou,
    // na volta, da pilula) ate o painel; o conteudo entra na segunda metade.
    int volta = t >= IL_T_RECOLHE;
    float e = volta ? 1.0f - passo(t, IL_T_RECOLHE, 0.42f) : ePn;
    GfxRect o = volta ? pil : M;
    float ec = anim_clamp(e, 0.0f, 1.0f), rIni = o.h * 0.5f, rFim = 28.0f;
    GfxRect F = misturaRect(o, P, e);
    float raio = (rIni + (rFim - rIni) * ec) / (F.h > 1.0f ? F.h : 1.0f);
    float ac = anim_clamp((e - 0.45f) / 0.55f, 0.0f, 1.0f);
    if (volta) ac = anim_clamp((e - 0.6f) / 0.4f, 0.0f, 1.0f);
    gfx_vidro_ou_solido(F, raio, GFX_VIDRO_DENS_MODAL, a, 0.055f, 0.058f, 0.068f, 0.94f);
    recorteDentro(F);
    painelSalvos(P, a * ac);
    recorteVolta();
    // O modal que o painel cobriu ainda aparece nos primeiros quadros dele.
    if (!volta && ec < 0.35f) {
      float am = a * (1.0f - ec / 0.35f);
      recorteDentro(F);
      ilhaModal(M, t, am);
      recorteVolta();
    }
    return;
  }

  // ---- a tecla da central (AZUL na LG, CH+ onde nao ha cor) embaixo da pilula.
  { float ak = a * passo(t, IL_T_TECLA, 0.3f) * (1.0f - passo(t, IL_T_MODAL + 0.05f, 0.2f));
    if (ak > 0.003f) {
      TxtLinha ab = txt_linha(TXT_CAPTION2, i18n("abre"), 214, 218, 228, 255);
      float lado = 44.0f, aperto = 1.0f - 0.10f * passo(t, IL_T_MODAL - 0.25f, 0.12f);
      float kx = xd - (float)ab.w - 10.0f - lado, ky = pil.y + pil.h + 18.0f;
      GfxRect fundo = { kx - 14.0f, ky - 6.0f, lado + (float)ab.w + 38.0f, lado + 12.0f };
      gfx_cor(fundo, 0.5f, 0.02f, 0.02f, 0.03f, 0.62f * ak);
      sintro_tecla_atalho(kx + lado * (1.0f - aperto) * 0.5f, ky + lado * (1.0f - aperto) * 0.5f,
                          lado * aperto, ak);
      txt_desenhar_alpha(ab, kx + lado + 10.0f, ky + (lado - (float)ab.h) * 0.5f, ak);
    } }

  // ---- a pilula (ou o modal, enquanto ele cresce dela).
  { GfxRect R = pil;
    float raio = 0.5f, aPil = 1.0f, aMod = 0.0f, k = 0.0f;
    if (sM > 0.0f) {
      float tr = sM > 1.0f ? 1.0f : sM, rpx;
      R = misturaRect(pil, M, sM);
      rpx = pil.h * 0.5f + (IL_MD_RAIO - pil.h * 0.5f) * tr;
      raio = rpx / (R.h > 1.0f ? R.h : 1.0f);
      aPil = anim_clamp(1.0f - sM * 3.0f, 0.0f, 1.0f);
      aMod = anim_clamp((sM - 0.55f) / 0.40f, 0.0f, 1.0f);
      k = tr;
    }
    // Pilula de volta do painel: nasce no lugar dele, sem a mola de novo.
    ilhaSuperficie(R, raio, k, a);
    recorteDentro((GfxRect){ R.x + 6.0f, R.y, R.w - 12.0f, R.h });
    if (aPil > 0.0f) {
      // A troca de conteudo: o velho apaga rapido, o novo entra com a forma
      // perto do tamanho final (o "conteudoA" de ilha.c).
      float sai = 1.0f - passo(t, tTroca, 0.12f), entra = passo(t, tTroca + 0.20f, 0.30f);
      float volta = t >= IL_T_RECOLHE ? passo(t, IL_T_RECOLHE + 0.42f, 0.25f) : 1.0f;
      if (sai > 0.0f) ilhaConteudo(kAntes, pil, a * aPil * sai);
      ilhaConteudo(kAgora, pil, a * aPil * entra * (kAgora == IL_VIVO ? volta : 1.0f));
    }
    if (aMod > 0.0f) ilhaModal(M, t, a * aMod);
    recorteVolta(); }
}

// ===================================================== CENA 1: O SPOTLIGHT
//
// A caixa por cima da home (spotlight.c, na escala da previa): o campo, o
// teclado de tela a esquerda e a lista a direita. Vazia, as pesquisas
// recentes; a cada letra de "mar" a lista se refaz, agrupada (melhor
// resultado, titulos, pessoas, canais ao vivo). Depois o foco entra na lista.
#define SPV_TECLA    38.0f
#define SPV_GAP       6.0f
#define SPV_COLS      6
#define SPV_T_DIG0    1.5f   // primeira letra; uma por SPV_T_LETRA
#define SPV_T_LETRA   1.05f
#define SPV_T_LISTA   5.2f   // o foco entra na lista (melhor resultado)
#define SPV_T_DESCE   6.5f   // e desce uma linha
#define SPV_DUR       9.0f
static const char SPV_CONSULTA[] = "mar";

enum { SL_CAB, SL_TOPO, SL_TITULO, SL_PESSOA, SL_CANAL, SL_RECENTE, SL_LIMPAR };
static const float SL_ALTURA[] = { 36.0f, 120.0f, 70.0f, 70.0f, 64.0f, 54.0f, 50.0f };
typedef struct { int tipo, arte; const char *t1, *t2; int traduz1, traduz2; } SpotLin;
// t2 com "%s" = i18n do formato com o titulo de t1 da linha seguinte? Nao: as
// metas sao montadas em spotMeta pelo tipo (ver abaixo).
#define SL_FIM { -1, -1, NULL, NULL, 0, 0 }
static const SpotLin SPOT_ETAPA[4][9] = {
  { { SL_CAB, -1, "Pesquisas recentes", NULL, 1, 0 },
    { SL_RECENTE, -1, "fallout", NULL, 0, 0 },
    { SL_RECENTE, -1, "ryan gosling", NULL, 0, 0 },
    { SL_RECENTE, -1, "drama hd", NULL, 0, 0 },
    { SL_LIMPAR, -1, "Limpar pesquisas recentes", NULL, 1, 0 },
    { SL_CAB, -1, "Em alta", NULL, 1, 0 },
    { SL_TITULO, A_P_HAIL, "Project Hail Mary", "2026|f", 0, 0 },
    { SL_TITULO, A_P_3BODY, "3 Body Problem", "2024|s", 0, 0 },
    SL_FIM },
  { { SL_CAB, -1, "Melhor resultado", NULL, 1, 0 },
    { SL_TOPO, A_MERCY, "Mercy", "2026|f", 0, 0 },
    { SL_CAB, -1, "Títulos", NULL, 1, 0 },
    { SL_TITULO, A_P_MRK, "Mr. K", "2025|f", 0, 0 },
    { SL_TITULO, A_P_MANIAC, "Maniac", "2018|s", 0, 0 },
    { SL_CAB, -1, "Pessoas", NULL, 1, 0 },
    { SL_PESSOA, A_F_MERYL, "Meryl Streep", "The Devil Wears Prada 2", 0, 0 },
    SL_FIM, SL_FIM },
  { { SL_CAB, -1, "Melhor resultado", NULL, 1, 0 },
    { SL_TOPO, A_MANIAC, "Maniac", "2018|s", 0, 0 },
    { SL_CAB, -1, "Títulos", NULL, 1, 0 },
    { SL_TITULO, A_P_MARTIAN, "The Martian", "2015|f", 0, 0 },
    { SL_TITULO, A_P_MARSH, "The Marsh King's Daughter", "2023|f", 0, 0 },
    { SL_CAB, -1, "Canais ao vivo", NULL, 1, 0 },
    { SL_CANAL, -1, "Max Action", "Filmes", 0, 0 },
    SL_FIM, SL_FIM },
  { { SL_CAB, -1, "Melhor resultado", NULL, 1, 0 },
    { SL_TOPO, A_C2, "The Martian", "2015|f", 0, 0 },
    { SL_CAB, -1, "Títulos", NULL, 1, 0 },
    { SL_TITULO, A_P_MARSH, "The Marsh King's Daughter", "2023|f", 0, 0 },
    { SL_CAB, -1, "Pessoas", NULL, 1, 0 },
    { SL_PESSOA, A_F_MARK, "Mark Coles Smith", "We Bury the Dead", 0, 0 },
    { SL_CAB, -1, "Canais ao vivo", NULL, 1, 0 },
    { SL_CANAL, -1, "Cinemar HD", "Filmes", 0, 0 },
    SL_FIM },
};

// "2015|f" -> "2015  ·  Filme" (o tipo traduzido); pessoa: "Em <titulo>";
// canal: "Canal  ·  <categoria>".
static void spotMeta(const SpotLin *l, char *b, size_t n) {
  b[0] = 0;
  if (!l->t2) return;
  if (l->tipo == SL_PESSOA) { snprintf(b, n, i18n("Em %s"), l->t2); return; }
  if (l->tipo == SL_CANAL) { snprintf(b, n, "%s  \xc2\xb7  %s", i18n("Canal"), i18n(l->t2)); return; }
  anoTipo(l->t2, b, n);
}

// A linha da lista (desenhaLinha de spotlight.c). `f` 0..1 = foco.
static void spotLinha(const SpotLin *l, float x, float y, float w, float f, float a) {
  float h = SL_ALTURA[l->tipo], ar, ag, ab, tx;
  int t1 = 246, t2 = 168;
  GfxRect r = { x, y, w, h - 8.0f };
  char meta[160];
  const char *s1 = l->traduz1 ? i18n(l->t1) : l->t1;
  if (a < 0.003f) return;
  ajustes_acento(&ar, &ag, &ab);
  if (l->tipo == SL_CAB) {
    TxtLinha t = txt_linha(TXT_CW_BADGE, s1, 150, 156, 168, 255);
    txt_desenhar_alpha(t, x + 14.0f, y + h - (float)t.h - 8.0f, a);
    return;
  }
  if (f > 0.01f) {
    float q = 16.0f / r.h;
    if (ajustes_vidro()) { gfx_vidro_painel(r, q, 0.55f, f * a); gfx_vidro_foco(r, q, f, a); }
    else {
      gfx_rect((GfxRect){ r.x - 12, r.y - 12, r.w + 24, r.h + 24 }, 0, GFX_SOMBRA, 1.0f, 0, 0, 0.5f,
               ar, ag, ab, 0.22f * f * a);
      gfx_cor(r, q, ar, ag, ab, f * a);
      t1 = (int)anim_mistura(246.0f, (float)ajustes_tinta_foco(), f);
      t2 = (int)anim_mistura(168.0f, (float)ajustes_tinta_foco(), f * 0.85f);
    }
  }
  spotMeta(l, meta, sizeof meta);
  if (l->tipo == SL_TOPO) {
    GfxRect art = { r.x + 12.0f, r.y + 12.0f, 0, r.h - 24.0f };
    art.w = art.h * 16.0f / 9.0f;
    arte(l->arte, art, 12.0f, 0.0f, a);
    tx = art.x + art.w + 20.0f;
    { TxtLinha t = txt_linha_corta(TXT_CALLOUT, s1, t1, t1, t1, 255, r.x + r.w - tx - 16.0f);
      TxtLinha m = txt_linha_corta(TXT_CAPTION2, meta, t2, t2, t2, 255, r.x + r.w - tx - 16.0f);
      txt_desenhar_alpha(t, tx, r.y + 26.0f, a);
      // Sem o "OK  Abrir" da linha de verdade: na altura da previa ele cairia
      // em cima da meta, e o rodape ja diz o que o OK faz.
      txt_desenhar_alpha(m, tx, r.y + 26.0f + (float)t.h + 6.0f, a); }
    return;
  }
  { GfxRect ic = { r.x + 12.0f, r.y + 6.0f, 0, r.h - 12.0f };
    switch (l->tipo) {
      case SL_TITULO: ic.w = ic.h * 2.0f / 3.0f; capa(l->arte, ic, 6.0f, a); break;
      case SL_PESSOA:
        ic.w = ic.h;
        if (tex(l->arte)) gfx_rect(ic, tex(l->arte), GFX_AVATAR, 0, 0, 0, 0.0f, 0, 0, 0, a);
        else gfx_cor(ic, 0.5f, 0.20f, 0.205f, 0.225f, a);
        break;
      case SL_CANAL:
        ic.w = ic.h * 2.3f;
        // Sem logo no pacote: o nome do canal como marca, na caixa do logo.
        gfx_cor(ic, rr(8.0f, ic), 0.16f, 0.165f, 0.185f, a);
        { TxtLinha m = txt_linha_corta(TXT_MINI, l->t1, 214, 218, 228, 255, ic.w - 10.0f);
          txt_desenhar_alpha(m, ic.x + (ic.w - (float)m.w) * 0.5f, ic.y + (ic.h - (float)m.h) * 0.5f, a); }
        break;
      default: {
        float d = 38.0f;
        GfxRect dc = { r.x + 14.0f, r.y + (r.h - d) * 0.5f, d, d };
        int tt = (f > 0.5f && !ajustes_vidro()) ? ajustes_tinta_foco() : 210;
        gfx_cor(dc, 0.5f, 1.0f, 1.0f, 1.0f, (0.08f + 0.06f * (1.0f - f)) * a);
        gfx_icone((GfxRect){ dc.x + 9, dc.y + 9, d - 18, d - 18 }, "aj_rotate-ccw-clock",
                  tt / 255.0f, tt / 255.0f, tt / 255.0f, l->tipo == SL_LIMPAR ? 0.6f * a : a);
        ic.w = d + 2.0f;
        break; }
    }
    tx = ic.x + ic.w + 18.0f;
    if (l->tipo == SL_RECENTE || l->tipo == SL_LIMPAR) {
      TxtLinha t = txt_linha_corta(l->tipo == SL_LIMPAR ? TXT_CAPTION2 : TXT_CALLOUT, s1,
                                   t1, t1, t1, 255, r.x + r.w - tx - 16.0f);
      txt_desenhar_alpha(t, tx, r.y + (r.h - (float)t.h) * 0.5f, l->tipo == SL_LIMPAR ? 0.8f * a : a);
      return;
    }
    { TxtLinha t = txt_linha_corta(TXT_CALLOUT, s1, t1, t1, t1, 255, r.x + r.w - tx - 16.0f);
      TxtLinha m = txt_linha_corta(TXT_CAPTION2, meta, t2, t2, t2, 255, r.x + r.w - tx - 16.0f);
      float bloco = (float)t.h + 4.0f + (meta[0] ? (float)m.h : 0.0f);
      float ty = r.y + (r.h - bloco) * 0.5f;
      txt_desenhar_alpha(t, tx, ty, a);
      if (meta[0]) txt_desenhar_alpha(m, tx, ty + (float)t.h + 4.0f, a); }
  }
}

// O alfabeto do teclado de tela (o do idioma, teclado.h), uma tecla por letra.
static int spotTeclas(char tec[48][5]) {
  const unsigned char *p = (const unsigned char *)teclado_alfabeto();
  int n = 0;
  while (p && *p && n < 36) {
    int len = *p < 0x80 ? 1 : (*p >= 0xF0 ? 4 : (*p >= 0xE0 ? 3 : 2)), i;
    for (i = 0; i < len; i++) tec[n][i] = (char)p[i];
    tec[n][len] = 0;
    n++; p += len;
  }
  return n;
}
static int spotIndice(char tec[48][5], int n, char c) {
  int i;
  for (i = 0; i < n; i++) if (tec[i][0] == c && !tec[i][1]) return i;
  return -1;
}

static void spotDica(float *x, float y, const char *tecla, const char *acao, float a) {
  TxtLinha t = txt_linha(TXT_CAPTION2, tecla, 222, 224, 230, 255);
  TxtLinha r = txt_linha(TXT_CAPTION2, acao, 150, 154, 163, 255);
  txt_desenhar_alpha(t, *x, y, a);
  txt_desenhar_alpha(r, *x + (float)t.w + 10.0f, y, a);
  *x += (float)t.w + 10.0f + (float)r.w + 30.0f;
}

static void cenaSpot(float x, float y, float t, float a) {
  const float W = N170_PV_W, H = N170_PV_H;
  float e = passo(t, 0.15f, 0.45f), dy = (1.0f - e) * -18.0f, ar, ag, ab;
  GfxRect P = { x + 18.0f, y + 84.0f + dy, W - 36.0f, H - 84.0f - 18.0f };
  GfxRect campo = { P.x + 22.0f, P.y + 22.0f, P.w - 44.0f, 64.0f };
  float corpoY = campo.y + campo.h + 24.0f, rodY = P.y + P.h - 40.0f;
  float kbX = P.x + 22.0f, kbW = SPV_COLS * SPV_TECLA + (SPV_COLS - 1) * SPV_GAP;
  float lx = kbX + kbW + 24.0f, lw = P.x + P.w - 22.0f - lx;
  char tec[48][5];
  int nTec = spotTeclas(tec), nFil = (nTec + SPV_COLS - 1) / SPV_COLS, i, f, c;
  int nDig = 0, etapa;
  float naLista = passo(t, SPV_T_LISTA, 0.2f);
  ajustes_acento(&ar, &ag, &ab);
  for (i = 0; i < (int)sizeof SPV_CONSULTA - 1; i++)
    if (t >= SPV_T_DIG0 + SPV_T_LETRA * (float)i) nDig = i + 1;
  etapa = nDig;

  // ---- a tela de tras e o veu do Spotlight.
  homeDinamica(x, y, a, a);
  gfx_cor((GfxRect){ x, y, W, H }, rr(N170_PV_RAIO, (GfxRect){ x, y, W, H }), 0, 0, 0.01f, 0.62f * e * a);
  a *= e;
  if (a < 0.003f) return;

  // ---- a folha.
  { float raio = rr(30.0f, P);
    gfx_rect((GfxRect){ P.x - 40.0f, P.y - 34.0f, P.w + 80.0f, 300.0f }, 0, GFX_SOMBRA,
             1.0f, 0, 0, 0.5f, ar, ag, ab, 0.10f * a);
    gfx_vidro_ou_solido(P, raio, GFX_VIDRO_DENS_MODAL, a, 0.058f, 0.062f, 0.074f, 0.94f);
    if (!ajustes_vidro()) gfx_luz_canto(P, raio, P.w * 0.18f, -40.0f, 560.0f, ar, ag, ab, 0.07f * a);
    gfx_vidro_aro(P, raio, 1.5f, 1.0f, 1.0f, 1.0f, 0.10f * a); }

  // ---- o campo: a lupa, o que ja foi digitado e o cursor.
  { float lum = 0.12f + 0.03f * (1.0f - naLista), tx;
    if (ajustes_vidro()) gfx_vidro_painel(campo, 0.5f, 0.7f, a);
    else gfx_cor(campo, 0.5f, lum, lum + 0.005f, lum + 0.016f, a);
    gfx_icone((GfxRect){ campo.x + 26.0f, campo.y + (campo.h - 28.0f) * 0.5f, 28.0f, 28.0f },
              "menu_search", 0.75f, 0.76f, 0.80f, a);
    tx = campo.x + 26.0f + 28.0f + 18.0f;
    if (nDig) {
      char q[8];
      TxtLinha l;
      snprintf(q, sizeof q, "%.*s", nDig, SPV_CONSULTA);
      l = txt_linha(TXT_TITULO3, q, 246, 247, 251, 255);
      txt_desenhar_alpha(l, tx, campo.y + (campo.h - (float)l.h) * 0.5f, a);
      tx += (float)l.w + 5.0f;
      // O cursor pisca enquanto o teclado tem o foco.
      if (naLista < 0.5f && fmodf(t, 1.0f) < 0.5f)
        gfx_cor((GfxRect){ tx, campo.y + 16.0f, 3.0f, campo.h - 32.0f }, 0.5f, ar, ag, ab, 0.95f * a);
    } else {
      TxtLinha l = txt_linha_corta(TXT_CALLOUT, i18n("Buscar filmes, séries, pessoas e canais"),
                                   255, 255, 255, 255, campo.x + campo.w - 24.0f - tx);
      txt_desenhar_alpha(l, tx, campo.y + (campo.h - (float)l.h) * 0.5f, 0.42f * a);
    } }

  // ---- o teclado: o foco anda ate a proxima letra e "aperta".
  { int alvo = -1, ant = -1;
    float s = 1.0f, aperto = 0.0f;
    for (i = 0; i < (int)sizeof SPV_CONSULTA - 1; i++) {
      float tl = SPV_T_DIG0 + SPV_T_LETRA * (float)i;
      if (t >= tl - 0.55f) { ant = alvo; alvo = spotIndice(tec, nTec, SPV_CONSULTA[i]);
                             s = passo(t, tl - 0.55f, 0.3f);
                             aperto = t >= tl - 0.08f && t < tl + 0.14f ? 1.0f : 0.0f; }
    }
    if (alvo < 0 && ant < 0) { alvo = 0; s = 1.0f; }
    for (f = 0; f <= nFil; f++) {
      int cols = f < nFil ? (nTec - f * SPV_COLS < SPV_COLS ? nTec - f * SPV_COLS : SPV_COLS) : 3;
      for (c = 0; c < cols; c++) {
        int idx = f * SPV_COLS + c;
        float k = 0.0f, esc;
        GfxRect b, q;
        if (f < nFil) {
          b = (GfxRect){ kbX + (float)c * (SPV_TECLA + SPV_GAP), corpoY + (float)f * (SPV_TECLA + SPV_GAP),
                         SPV_TECLA, SPV_TECLA };
          if (idx == alvo) k = s;
          else if (idx == ant) k = 1.0f - s;
        } else {
          float cw = (kbW - 2.0f * SPV_GAP) / 3.0f;
          b = (GfxRect){ kbX + (float)c * (cw + SPV_GAP), corpoY + (float)f * (SPV_TECLA + SPV_GAP), cw, SPV_TECLA };
        }
        k *= 1.0f - naLista;
        esc = 1.0f + 0.08f * k - (idx == alvo ? 0.06f * aperto : 0.0f);
        q = (GfxRect){ b.x - b.w * (esc - 1) * 0.5f, b.y - b.h * (esc - 1) * 0.5f, b.w * esc, b.h * esc };
        gfx_cor(q, 0.16f, 0.13f, 0.138f, 0.158f, 0.95f * a);
        if (k > 0.01f) {
          gfx_rect((GfxRect){ q.x - 8, q.y - 8, q.w + 16, q.h + 16 }, 0, GFX_SOMBRA, 1.0f, 0, 0, 0.5f,
                   ar, ag, ab, 0.28f * k * a);
          gfx_cor(q, 0.16f, ar, ag, ab, k * a);
        }
        { int tom = (int)anim_mistura(226.0f, (float)ajustes_tinta_foco(), k);
          static const char *const CMD[3] = { "espaço", "apagar", "limpar" };
          TxtLinha l = f < nFil ? txt_linha(TXT_CALLOUT, tec[idx], tom, tom, tom, 255)
                                : txt_linha_corta(TXT_MINI, i18n(CMD[c]), tom, tom, tom, 255, q.w - 6.0f);
          txt_desenhar_alpha(l, q.x + (q.w - (float)l.w) * 0.5f, q.y + (q.h - (float)l.h) * 0.5f, a); }
      }
    } }

  // ---- a lista: a etapa que sai apaga, a nova entra linha a linha.
  { float tEt = etapa > 0 ? SPV_T_DIG0 + SPV_T_LETRA * (float)(etapa - 1) : 0.0f;
    float sai = etapa > 0 ? 1.0f - passo(t, tEt, 0.12f) : 0.0f;
    int focoL = -1, focoAnt = -1;
    float fs = 0.0f;
    if (t >= SPV_T_LISTA) {
      focoL = 1; fs = naLista;
      if (t >= SPV_T_DESCE) { focoAnt = 1; focoL = 3; fs = passo(t, SPV_T_DESCE, 0.18f); }
    }
    recorteDentro((GfxRect){ lx - 16.0f, corpoY - 8.0f, lw + 32.0f, rodY - 14.0f - corpoY + 8.0f });
    if (sai > 0.0f) {
      float yy = corpoY;
      for (i = 0; i < 9 && SPOT_ETAPA[etapa - 1][i].t1; i++) {
        spotLinha(&SPOT_ETAPA[etapa - 1][i], lx, yy, lw, 0.0f, a * sai);
        yy += SL_ALTURA[SPOT_ETAPA[etapa - 1][i].tipo];
      }
    }
    { float yy = corpoY;
      for (i = 0; i < 9 && SPOT_ETAPA[etapa][i].t1; i++) {
        float en = etapa > 0 ? passo(t, tEt + 0.10f + 0.04f * (float)i, 0.22f) : passo(t, 0.35f + 0.04f * (float)i, 0.25f);
        float fo = i == focoL ? fs : i == focoAnt ? 1.0f - fs : 0.0f;
        spotLinha(&SPOT_ETAPA[etapa][i], lx, yy + (1.0f - en) * 10.0f, lw, fo, a * en);
        yy += SL_ALTURA[SPOT_ETAPA[etapa][i].tipo];
      }
    }
    recorteVolta(); }

  // ---- o rodape: as dicas do controle, do teclado ou da lista.
  { float dx = P.x + 26.0f;
    if (naLista < 0.5f) {
      spotDica(&dx, rodY, "OK", i18n("Digitar"), a * (1.0f - naLista * 2.0f));
      spotDica(&dx, rodY, "\xe2\x86\x92", i18n("Resultados"), a * (1.0f - naLista * 2.0f));
    } else {
      spotDica(&dx, rodY, "OK", i18n("Abrir"), a * (naLista * 2.0f - 1.0f));
      spotDica(&dx, rodY, "\xe2\x86\x90", i18n("Teclado"), a * (naLista * 2.0f - 1.0f));
    }
    spotDica(&dx, rodY, i18n("Voltar"), i18n("Fechar"), a); }
}

// ============================================ CENA 2: O LAYOUT APPLE TV
//
// A barra (a pilula "‹ (casa) Inicio" que cresce ate o painel flutuante, com o
// foco descendo pelos itens e o Streaming) e, emendado, o carrossel: o cartao
// da fileira abre no cartao grande, o trailer toca dentro e a tira anda.
#define N170_P_W      300.0f
#define N170_P_CAB     70.0f
#define N170_P_LIN     56.0f
#define N170_P_PIL     50.0f
#define N170_P_ROT     34.0f
#define N170_P_COLX    26.0f    // centro da coluna de icones, a partir da pilula
#define N170_P_ROTX    54.0f    // rotulo, a partir da pilula
#define N170_PIL_H     50.0f
#define N170_PIL_CIRC  38.0f
#define N170_SETA      22.0f
#define N170_ABRE_T     0.75f
#define N170_ANDA_T     1.45f
#define N170_ANDA_S     0.34f
#define N170_FECHA_T    4.05f
#define N170_CARRO_T    4.45f   // daqui em diante, o carrossel
#define N170_CARRO_X    0.40f   // a passagem entre os dois

static const int BARRA_DEST[6] = { MENU_INICIO, MENU_BUSCAR, MENU_EXPLORAR, MENU_GUIA,
                                   MENU_BIBLIOTECA, MENU_AJUSTES };
static const char *const BARRA_ICONE[6] = { "menu_home", "menu_search", "portal", "menu_guide",
                                            "menu_library", "menu_settings" };
static const char *const PASTA_NOME[2] = { "Sci-Fi", "Drama" };
static const int PASTA_CAPA[2] = { A_P_3BODY, A_P_LOST };
#define N170_FOCOS 8   // seis itens e as duas pastas

// Topo de cada foco dentro do painel (0 = topo do painel).
static float yFoco(int i) {
  if (i < 6) return N170_P_CAB + (float)i * N170_P_LIN;
  return N170_P_CAB + 6.0f * N170_P_LIN + N170_P_ROT + (float)(i - 6) * N170_P_LIN;
}
static float painelAltura(void) { return yFoco(N170_FOCOS - 1) + N170_P_LIN + 10.0f; }

// Circulo de pasta de Streaming: a capa recortada em circulo (cover).
static void circuloPasta(int p, float cx, float cy, float d, float a) {
  GfxRect c = { cx - d * 0.5f, cy - d * 0.5f, d, d };
  capa(PASTA_CAPA[p], c, d * 0.5f, a);
}

static void cenaBarra(float x, float y, float t, float a) {
  const float H = N170_PV_H;
  float s = mola(t, N170_ABRE_T, 0.62f) * (1.0f - passo(t, N170_FECHA_T, 0.34f));
  float px = x + 24.0f, py = y + 88.0f;
  const char *rotIni = menu_rotulo(MENU_INICIO);
  TxtLinha lIni = txt_linha(TXT_CALLOUT, rotIni, 245, 245, 248, 255);
  GfxRect P = { px + N170_SETA, py, 7.0f + N170_PIL_CIRC + 12.0f + (float)lIni.w + 22.0f, N170_PIL_H };
  GfxRect Q = { px, py, N170_P_W, painelAltura() };
  GfxRect R = misturaRect(P, Q, s);
  float raio = mistura(P.h * 0.5f, 30.0f, s);
  float ar, ag, ab;
  int i;
  ajustes_acento(&ar, &ag, &ab);

  // ---- a home da Dinamica atras. Aberta, o painel fica por cima do bloco do
  // destaque: ele recua para nao ler como texto por cima de texto.
  homeDinamica(x, y, a * (1.0f - 0.85f * anim_clamp(s, 0.0f, 1.0f)), a);

  // ---- o veu so do lado esquerdo, como tvSombra: faixas de cor chapada.
  if (s > 0.01f) {
    float sx = x + 360.0f;
    gfx_cor((GfxRect){ x, y, 360.0f, H }, 0.0f, 0, 0, 0, 0.26f * s * a);
    for (i = 1; i <= 12; i++, sx += 20.0f)
      gfx_cor((GfxRect){ sx, y, 20.0f, H }, 0.0f, 0, 0, 0, 0.26f * s * a * (1.0f - (float)i / 13.0f));
  }

  // ---- a superficie: escura translucida, tingida de leve pelo realce.
  { float vr = 0.100f + ar * 0.05f, vg = 0.104f + ag * 0.05f, vb = 0.118f + ab * 0.05f;
    gfx_cor(R, raio / R.h, vr, vg, vb, 0.90f * a);
    gfx_anel(R, raio / R.h, 1.2f, 1.0f, 1.0f, 1.0f, 0.12f * a); }

  // ---- fechada: a seta solta sobre a arte, o circulo e a secao.
  { float ap = a * (1.0f - anim_clamp(s * 3.0f, 0.0f, 1.0f));
    if (ap > 0.01f) {
      TxtLinha seta = txt_linha(TXT_HEADLINE, "\xE2\x80\xB9", 245, 245, 248, 255);
      TxtLinha sombra = txt_linha(TXT_HEADLINE, "\xE2\x80\xB9", 0, 0, 0, 255);
      float cy = P.y + P.h * 0.5f, sx = px + (N170_SETA - (float)seta.w) * 0.5f - 3.0f;
      float sy = cy - (float)seta.h * 0.5f - 2.0f;
      GfxRect c = { P.x + 6.0f, cy - N170_PIL_CIRC * 0.5f, N170_PIL_CIRC, N170_PIL_CIRC };
      txt_desenhar_alpha(sombra, sx + 1.0f, sy + 2.0f, ap * 0.35f);
      txt_desenhar_alpha(seta, sx, sy, ap * 0.9f);
      gfx_cor(c, 0.5f, 1, 1, 1, 0.22f * ap);
      gfx_icone((GfxRect){ c.x + 8.0f, c.y + 8.0f, 22.0f, 22.0f }, "menu_home", 0.97f, 0.97f, 0.98f, ap);
      txt_desenhar_alpha(lIni, c.x + c.w + 12.0f, cy - (float)lIni.h * 0.5f, ap);
    } }

  // ---- aberta: cabecalho, itens e as pastas, presos ao retangulo que cresce.
  { float ac = a * anim_clamp((s - 0.22f) / 0.7f, 0.0f, 1.0f);
    if (ac > 0.01f) {
      float pos = anim_clamp((t - N170_ANDA_T) / N170_ANDA_S, 0.0f, (float)(N170_FOCOS - 1));
      int k = (int)pos;
      float m = passo(pos - (float)k, 0.0f, 0.55f);
      float fy = mistura(yFoco(k), yFoco(k + 1 < N170_FOCOS ? k + 1 : k), m);
      const int TINTA = 22;
      recorteDentro(R);
      { float cy = py + N170_P_CAB * 0.5f + 6.0f;
        GfxRect av = { px + 10.0f + N170_P_COLX - 18.0f, cy - 18.0f, 36.0f, 36.0f };
        TxtLinha ini = txt_linha(TXT_CAPTION2, "N", 255, 255, 255, 255);
        TxtLinha nome = txt_linha(TXT_BODY, i18n("Sua conta"), 240, 240, 240, 255);
        TxtLinha rel = txt_linha(TXT_CAPTION2, "21:40", 200, 200, 200, 255);
        gfx_cor(av, 0.5f, 0.20f, 0.48f, 0.95f, ac);
        txt_desenhar_alpha(ini, av.x + (av.w - (float)ini.w) * 0.5f, av.y + (av.h - (float)ini.h) * 0.5f, ac);
        txt_desenhar_alpha(nome, px + 10.0f + N170_P_ROTX, cy - (float)nome.h * 0.5f, ac);
        txt_desenhar_alpha(rel, px + N170_P_W - 20.0f - (float)rel.w, cy - (float)rel.h * 0.5f, ac); }
      { GfxRect pill = { px + 8.0f, py + fy + (N170_P_LIN - N170_P_PIL) * 0.5f, N170_P_W - 16.0f, N170_P_PIL };
        if (fy > yFoco(0) + 1.0f)
          gfx_cor((GfxRect){ pill.x, py + yFoco(0) + (N170_P_LIN - N170_P_PIL) * 0.5f, pill.w, N170_P_PIL },
                  0.5f, 1, 1, 1, 0.09f * ac);
        gfx_cor(pill, 0.5f, 0.95f, 0.95f, 0.96f, ac); }
      { TxtLinha st = txt_linha(TXT_CAPTION2, "Streaming", 150, 152, 160, 255);
        txt_desenhar_alpha(st, px + 10.0f + N170_P_COLX - 12.0f, py + yFoco(6) - (float)st.h - 8.0f, ac); }
      for (i = 0; i < N170_FOCOS; i++) {
        float ly = py + yFoco(i), cy = ly + N170_P_LIN * 0.5f;
        float f = anim_clamp(1.0f - fabsf(fy - yFoco(i)) / (N170_P_PIL * 0.8f), 0.0f, 1.0f);
        const char *rot = i < 6 ? menu_rotulo(BARRA_DEST[i]) : PASTA_NOME[i - 6];
        TxtLinha l = txt_linha(TXT_CALLOUT, rot, 235, 235, 235, 255);
        TxtLinha le = txt_linha(TXT_CALLOUT, rot, TINTA, TINTA, TINTA, 255);
        float ccx = px + 8.0f + N170_P_COLX, lx = px + 8.0f + N170_P_ROTX;
        if (i < 6) {
          GfxRect ic = { ccx - 11.0f, cy - 11.0f, 22.0f, 22.0f };
          if (f < 1.0f) gfx_icone(ic, BARRA_ICONE[i], 1.0f, 1.0f, 1.0f, ac * 0.85f * (1.0f - f));
          if (f > 0.0f) gfx_icone(ic, BARRA_ICONE[i], 0.08f, 0.08f, 0.08f, ac * f);
        } else circuloPasta(i - 6, ccx, cy, 36.0f, ac);
        if (f < 1.0f) txt_desenhar_alpha(l, lx, cy - (float)l.h * 0.5f, ac * (1.0f - f));
        if (f > 0.0f) txt_desenhar_alpha(le, lx, cy - (float)le.h * 0.5f, ac * f);
      }
      recorteVolta();
    } }
}

// O carrossel: o cartao em foco da fileira ABRE no cartao grande, com os
// vizinhos espiando dos lados; o trailer toca dentro e a tira anda. E a
// GFX_JANELA de detail.c: a arte em "cover" num quadro 16:9 da altura da
// previa, vista por uma janela arredondada.
#define N170_CAR_ABRE   0.55f
#define N170_CAR_ABRE_S 0.80f
#define N170_TRAILER_T  1.85f
#define N170_ANDA_CAR   4.10f
#define N170_CAR_VAO    26.0f
#define N170_CAR_DUR    6.0f

typedef struct { int fundo, logo; const char *meta; int imdb; } TituloCar;
static const TituloCar CAR[4] = {
  { A_CE, -1,     "",                  0 },
  { A_C0, A_L_C0, "2026  ·  1h 59min", 75 },
  { A_C1, A_L_C1, "2026  ·  2h 37min", 0 },
  { A_C2, A_L_C2, "2015  ·  2h 21min", 80 },
};

static void janela(int f, GfxRect r, GfxRect quadro, float zoom, float raioPx, float a) {
  GLuint t = tex(f);
  float cx = r.x + r.w * 0.5f, cy = r.y + r.h * 0.5f;
  if (a <= 0.003f) return;
  if (!t) { gfx_cor(r, rr(raioPx, r), 0.16f, 0.17f, 0.19f, a); return; }
  gfx_janela_atual[2] = r.w / quadro.w / zoom;
  gfx_janela_atual[3] = r.h / quadro.h / zoom;
  gfx_janela_atual[0] = (cx - quadro.x) / quadro.w - gfx_janela_atual[2] * 0.5f;
  gfx_janela_atual[1] = (cy - quadro.y) / quadro.h - gfx_janela_atual[3] * 0.5f;
  gfx_tex_aspect_atual = tex_aspecto(arq[f]);
  if (gfx_tex_aspect_atual <= 0.0f) gfx_tex_aspect_atual = 16.0f / 9.0f;
  gfx_rect(r, t, GFX_JANELA, 0.86f, 0.0f, 0.0f, raioPx / r.h, 1, 1, 1, a);
  gfx_tex_aspect_atual = 0.0f;
  gfx_janela_atual[0] = gfx_janela_atual[1] = 0.0f;
  gfx_janela_atual[2] = gfx_janela_atual[3] = 1.0f;
}

// Logo, linha do filme e os botoes no canto de baixo do cartao.
static void conteudoCartao(const TituloCar *c, GfxRect r, float a) {
  const char *rp = i18n("Reproduzir");
  float wp = botao_largura(rp, "play", 1), by = r.y + r.h - 40.0f - 58.0f, my, x0 = r.x + 40.0f;
  static const char *const IC[3] = { "mais", "visto", "trailer" };
  int i;
  if (a <= 0.003f) return;
  my = by - 22.0f - BADGE_H;
  logoTitulo(c->logo, x0, my - 18.0f, 290.0f, 120.0f, a);
  { float xm = x0;
    TxtLinha l = txt_linha(TXT_DET_META2, c->meta, 214, 218, 226, 255);
    if (c->imdb) xm += badge_imdb(xm, my, c->imdb, 0, a) + 14.0f;
    txt_desenhar_alpha(l, xm, my + (BADGE_H - (float)l.h) * 0.5f, a); }
  botao_pilula((GfxRect){ x0, by, wp, 58.0f }, rp, "play", 1.0f, 1, 0, a);
  for (i = 0; i < 3; i++)
    botao_disco((GfxRect){ x0 + wp + 14.0f + (float)i * 70.0f, by, 58.0f, 58.0f }, IC[i], 0.0f, a);
}

static void cenaCarrossel(float x, float y, float t, float a) {
  const float W = N170_PV_W, H = N170_PV_H;
  GfxRect pv = { x, y, W, H };
  GfxRect quadro = { x + (W - H * 16.0f / 9.0f) * 0.5f, y, H * 16.0f / 9.0f, H };
  GfxRect C = { x + 58.0f, y + 92.0f, W - 116.0f, H - 92.0f - 30.0f };
  float o = passo(t, N170_CAR_ABRE, N170_CAR_ABRE_S);
  float off = passo(t, N170_ANDA_CAR, 0.62f);
  float ar, ag, ab;
  int k;
  GfxRect fila[3];
  ajustes_acento(&ar, &ag, &ab);
  for (k = 0; k < 3; k++)
    fila[k] = (GfxRect){ x + (W - 300.0f) * 0.5f + (float)(k - 1) * 324.0f, y + 330.0f, 300.0f, 169.0f };

  // A home atras: a arte do titulo em foco, escurecida (o fundo da Dinamica).
  if (o < 0.999f) {
    float ah = a * (1.0f - anim_clamp(o * 3.0f, 0.0f, 1.0f));
    arte(A_C0, pv, N170_PV_RAIO, 1.0f, ah);
    gfx_cor(pv, rr(N170_PV_RAIO, pv), 0.02f, 0.02f, 0.03f, 0.55f * ah);
    for (k = 0; k < 3; k++) {
      int f = k == 0 ? A_CE : k == 1 ? A_C0 : A_C1;
      arte(f, fila[k], 14.0f, 0.2f, ah);
    }
    gfx_anel_fora(fila[1], rr(14.0f, fila[1]), 3.0f, 3.0f, 0.97f, 0.97f, 0.98f, ah);
  }
  // A folha: opaca em um terco da abertura (a home deixa de ser desenhada).
  gfx_cor(pv, rr(N170_PV_RAIO, pv), 0.105f, 0.110f, 0.125f, a * anim_clamp(o * 3.0f, 0.0f, 1.0f));

  { GfxRect h = misturaRect(fila[1], C, o);
    float raio = mistura(14.0f, 30.0f, o), passoX = h.w + N170_CAR_VAO;
    float zoom = 1.0f + 0.10f * anim_clamp((t - N170_TRAILER_T) / 2.6f, 0.0f, 1.0f);
    for (k = 0; k < 4; k++) {
      GfxRect r = { h.x + ((float)(k - 1) - off) * passoX, h.y, h.w, h.h };
      float ak = k == 1 ? a * anim_clamp(o * 2.5f, 0.0f, 1.0f)
                        : a * anim_clamp((o - 0.55f) * 2.2f, 0.0f, 1.0f);
      if (r.x >= x + W || r.x + r.w <= x) continue;
      { GfxRect qa = { r.x, r.y, r.h * 16.0f / 9.0f, r.h };
        qa.x = r.x + (r.w - qa.w) * 0.5f;
        janela(CAR[k].fundo, r, misturaRect(qa, (GfxRect){ quadro.x + (r.x - h.x), quadro.y, quadro.w, quadro.h }, o),
               k == 1 ? zoom : 1.0f, raio, ak); }
    }
    { float aIn = a * passo(o, 0.75f, 0.25f);
      GfxRect r1 = { h.x - off * passoX, h.y, h.w, h.h }, r2 = { r1.x + passoX, h.y, h.w, h.h };
      if (off < 0.5f) conteudoCartao(&CAR[1], r1, aIn * (1.0f - anim_clamp(off * 3.0f, 0.0f, 1.0f)));
      if (off > 0.4f) conteudoCartao(&CAR[2], r2, a * passo(off, 0.55f, 0.45f)); }
    { float at = a * passo(t, N170_TRAILER_T, 0.3f) * (1.0f - anim_clamp(off * 4.0f, 0.0f, 1.0f));
      if (at > 0.003f) {
        TxtLinha l = txt_linha(TXT_CAPTION2, i18n("Trailer"), 236, 238, 244, 255);
        GfxRect p = { h.x + h.w - 28.0f - 36.0f - 44.0f - (float)l.w + 14.0f, h.y + 26.0f,
                      44.0f + (float)l.w + 22.0f, 40.0f };
        gfx_cor(p, 0.5f, 0.02f, 0.02f, 0.03f, 0.62f * at);
        gfx_icone((GfxRect){ p.x + 14.0f, p.y + 9.0f, 22.0f, 22.0f }, "trailer", ar, ag, ab, at);
        txt_desenhar_alpha(l, p.x + 44.0f, p.y + (p.h - (float)l.h) * 0.5f, at);
        { float pr = anim_clamp((t - N170_TRAILER_T) / 5.0f, 0.0f, 1.0f);
          gfx_cor((GfxRect){ p.x + 14.0f, p.y + p.h - 6.0f, p.w - 28.0f, 2.0f }, 0.5f, 1, 1, 1, 0.18f * at);
          gfx_cor((GfxRect){ p.x + 14.0f, p.y + p.h - 6.0f, (p.w - 28.0f) * pr, 2.0f }, 0.5f, ar, ag, ab, at); }
      } } }
}

// As duas emendadas: a barra ate fechar, e o carrossel por cima dela.
static void cenaAppleTV(float x, float y, float t, float a) {
  // Em sequencia, nao sobrepostas: duas homes diferentes misturadas leem
  // como imagem dupla. A barra apaga na primeira metade, o carrossel acende
  // na segunda.
  float s = anim_clamp((t - N170_CARRO_T) / N170_CARRO_X, 0.0f, 1.0f);
  if (s < 0.5f) cenaBarra(x, y, t, a * (1.0f - anim_suave(s * 2.0f)));
  if (s > 0.5f) cenaCarrossel(x, y, t - N170_CARRO_T, a * anim_suave(s * 2.0f - 1.0f));
}

// ================================================================ as tabelas
typedef struct {
  const char *nome;                              // selo no alto da previa
  void (*desenhar)(float x, float y, float t, float a);
  float duracao, estatico;                       // s; o quadro das animacoes reduzidas
  int id, id2;                                   // linhas da lista que ela ilustra
} Cena;

static const Cena CENAS[] = {
  { "Ilha do relógio", cenaIlha,    IL_DUR,                         4.6f, ID_ILHA, ID_SALVOS },
  { "Spotlight",       cenaSpot,    SPV_DUR,                        5.6f, ID_SPOT, ID_NADA },
  { "Layout Apple TV", cenaAppleTV, N170_CARRO_T + N170_CAR_DUR,    2.75f, ID_ATV, ID_NADA },
};
#define N170_NC ((int)(sizeof CENAS / sizeof *CENAS))

typedef struct { int id; const char *icone, *nome, *linha; } Item;

// A LISTA: so o essencial, nome e uma frase com o caminho quando ha um.
static const Item ITENS[] = {
  { ID_ILHA,      "sino",                "Ilha do relógio",
    "O episódio pela metade e a estreia nova ficam no relógio. CH+ abre, → leva aos Salvos." },
  { ID_SPOT,      "menu_search",         "Spotlight",
    "A tecla CH− abre a busca em qualquer tela. A lista muda a cada letra." },
  { ID_SALVOS,    "aj_folders",          "Organizar Salvos",
    "Categorias suas, a ordem que quiser e o estilo da lista, no painel de Salvos." },
  { ID_ATV,       "aj_layout-dashboard", "Layout Apple TV",
    "Barra em pílula com o Streaming e títulos em carrossel. Ajustes › Layout." },
  { ID_LIVE,      "menu_guide",          "Live TV",
    "Canais HLS tocam na LG, e o diagnóstico do guia testa os canais nesta TV." },
  { ID_CONSERTOS, "check",               "Consertos",
    "Vídeo atrás do menu na Samsung, retomar pelo card e séries no seu idioma." },
};
#define N170_NI ((int)(sizeof ITENS / sizeof *ITENS))

// ------------------------------------------------------------------- estado
int novidades170_itens(void) { return N170_NI; }
int novidades170_item_largura(int i, int *limite, const char **nome) {
  float tx = N170_TXT_X + N170_ICONE + 20.0f;
  if (i < 0 || i >= N170_NI) return 0;
  if (limite) *limite = (int)(N170_TXT_X + N170_TXT_W - tx);
  if (nome) *nome = ITENS[i].nome;
  return txt_largura(TXT_CAPTION, i18n(ITENS[i].linha));
}

int novidades170_cenas(void) { return N170_NC; }
static float tempoDe(int c, float t);
float novidades170_previa_altura(void) { return N170_PV_H; }
void novidades170_cena_desenhar(int c, float x, float y, float t) {
  if (c < 0 || c >= N170_NC) return;
  if (!arq[0][0]) montarCaminhos();
  pedirArtes();
  clipCena = (GfxRect){ x, y, N170_PV_W, N170_PV_H };
  recorte(clipCena);
  gfx_cor(clipCena, rr(N170_PV_RAIO, clipCena), 0.062f, 0.066f, 0.080f, 1);
  CENAS[c].desenhar(x, y, tempoDe(c, t), 1.0f);
  gfx_sem_recorte();
}
int novidades170_cena(void) { return cena; }
int novidades170_aberto(void) { return aberto; }
int novidades170_foco_na_previa(void) { return naPrevia; }

void novidades170_dir(const char *d) {
  if (d && d[0]) snprintf(dirArte, sizeof dirArte, "%s", d);
  montarCaminhos();
}

int novidades170_pedido(void) {
  int p = pedido;
  pedido = N170_PEDIU_NADA;
  return p;
}

static void mudarCena(int nova) {
  if (nova < 0) nova = N170_NC - 1;
  if (nova >= N170_NC) nova = 0;
  if (nova == cena) return;
  tempoAntiga = relogioCena;
  cenaAntiga = cena;
  cena = nova;
  transicao = ajustes_animacoes_reduzidas() ? 1.0f : 0.0f;
  relogioCena = 0.0f;
}

void novidades170_ir(int c, float t) {
  cena = cenaAntiga = (c % N170_NC + N170_NC) % N170_NC;
  transicao = 1.0f;
  relogioCena = t;
}

static void comecar(float e) {
  aberto = decidido = 1;
  foco = B_SPOT;
  naPrevia = 0;
  cena = cenaAntiga = 0;
  entrada = e;
  transicao = 1.0f;
  relogioCena = tempoAntiga = 0.0f;
  aquecida = 0;
  textogate_reiniciar(&gateLista);
  if (!arq[0][0]) montarCaminhos();
  pedirArtes();
}

void novidades170_abrir(void) { comecar(1.0f); }

void novidades170_primeira_vez(void) {
  char *s;
  if (decidido) return;
  decidido = 1;
  s = dados_ler(N170_ARQ);
  if (s) { free(s); return; }
  comecar(0.0f);
}

static void fechar(int oQue) {
  aberto = 0;
  dados_gravar(N170_ARQ, "1\n");
  pedido = oQue;
}

// D-PAD. Duas fileiras de foco: os botoes (de fabrica, no primario) e a previa
// (cima). Na previa, esquerda/direita trocam a cena; nos botoes, andam entre eles.
void novidades170_evento(const SDL_Event *e) {
  SDL_Keycode k;
  if (!aberto || !e || e->type != SDL_KEYDOWN) return;
  k = e->key.keysym.sym;
  if (k == SDLK_UP)   { naPrevia = 1; return; }
  if (k == SDLK_DOWN) { naPrevia = 0; return; }
  if (k == SDLK_LEFT) {
    if (naPrevia) mudarCena(cena - 1);
    else if (foco > 0) foco--;
    return;
  }
  if (k == SDLK_RIGHT) {
    if (naPrevia) mudarCena(cena + 1);
    else if (foco < B_N - 1) foco++;
    return;
  }
  if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) {
    if (naPrevia) { mudarCena(cena + 1); return; }
    fechar(foco == B_SPOT ? N170_PEDIU_SPOT : N170_PEDIU_NADA);
    return;
  }
  if (k == SDLK_AC_BACK || k == SDLK_ESCAPE || k == SDLK_BACKSPACE ||
      k == SDLK_DELETE || e->key.keysym.scancode == NV_SCANCODE_BACK)
    fechar(N170_PEDIU_NADA);
}

void novidades170_atualizar(float dt, Uint32 agora) {
  (void)agora;
  if (!aberto && entrada < 0.002f) { entrada = 0.0f; return; }
  if (aberto) pedirArtes();
  if (ajustes_animacoes_reduzidas()) {
    // Sem passagem, sem movimento dentro da cena; a troca continua, seca.
    entrada = aberto ? 1.0f : 0.0f;
    transicao = 1.0f;
    if (aberto) {
      relogioCena += dt;
      if (relogioCena >= CENAS[cena].duracao) mudarCena(cena + 1);
    }
    return;
  }
  entrada = anim_rampa(entrada, aberto ? 1.0f : 0.0f, dt, aberto ? N170_ABRIR_MS : N170_FECHAR_MS);
  if (aberto) {
    if (transicao < 1.0f) {
      transicao += dt / N170_TRANSICAO_S;
      if (transicao > 1.0f) transicao = 1.0f;
      tempoAntiga += dt;
    }
    relogioCena += dt;
    if (relogioCena >= CENAS[cena].duracao) mudarCena(cena + 1);
  }
}

// ------------------------------------------------------------------ desenho
static float tempoDe(int c, float t) {
  return ajustes_animacoes_reduzidas() ? CENAS[c].estatico : t;
}

static void desenhaCena(int c, float x, float y, float t, float a) {
  if (a <= 0.003f) return;
  CENAS[c].desenhar(x, y, tempoDe(c, t), a);
}

// O selo com o nome da cena e os tracos do ciclo, no alto da previa. Com o
// foco na previa, o selo vira a pilula de foco dos botoes.
static GfxRect seloRect;
static void selo(float x, float y, float a) {
  float fr, fg, fb, ti = botao_cor_foco(&fr, &fg, &fb);
  int tinta = (int)(ti * 255.0f + 0.5f), i;
  const char *nome = i18n(CENAS[cena].nome);
  TxtLinha l = txt_linha(TXT_CAPTION, nome, 236, 238, 244, 255);
  TxtLinha lf = txt_linha(TXT_CAPTION, nome, tinta, tinta, tinta, 255);
  GfxRect s = { x + 24.0f, y + 24.0f, (float)l.w + 36.0f, 44.0f };
  float tA = anim_suave(transicao);
  gfx_cor(s, 0.5f, 0.02f, 0.02f, 0.03f, 0.58f * a);
  if (naPrevia) { botao_luz(s, 1.0f, a); gfx_cor(s, 0.5f, fr, fg, fb, a); }
  txt_desenhar_alpha(naPrevia ? lf : l, s.x + 18.0f, s.y + (s.h - (float)l.h) * 0.5f, a * tA);
  seloRect = s;
  { float tw = 26.0f, gap = 8.0f, lw = (float)N170_NC * (tw + gap) - gap;
    GfxRect p = { x + N170_PV_W - 24.0f - lw - 16.0f, y + 32.0f, lw + 32.0f, 28.0f };
    gfx_cor(p, 0.5f, 0.02f, 0.02f, 0.03f, 0.50f * a); }
  for (i = 0; i < N170_NC; i++) {
    float tw = 26.0f, gap = 8.0f;
    float tx = x + N170_PV_W - 24.0f - (float)(N170_NC - i) * (tw + gap) + gap;
    GfxRect tr = { tx, y + 44.0f, tw, 4.0f };
    gfx_cor(tr, 0.5f, 1, 1, 1, 0.26f * a);
    if (i < cena) gfx_cor(tr, 0.5f, 1, 1, 1, 0.62f * a);
    if (i == cena) {
      float p = anim_clamp(relogioCena / CENAS[cena].duracao, 0.0f, 1.0f);
      tr.w *= p;
      if (tr.w > 1.0f) gfx_cor(tr, 0.5f, 1, 1, 1, 0.94f * a);
    }
  }
}

// Uma linha da lista. `vivo` 0..1: a cena na previa fala desta linha.
static float linhaH[32];
static void desenhaItem(int i, float y, float vivo, int maxL, float a) {
  float fr, fg, fb, ti = botao_cor_foco(&fr, &fg, &fb);
  float tx = N170_TXT_X + N170_ICONE + 20.0f, tw = N170_TXT_X + N170_TXT_W - tx;
  GfxRect d = { N170_TXT_X, y + 4.0f, N170_ICONE, N170_ICONE };
  GfxRect ic = { d.x + 9.0f, d.y + 9.0f, 22.0f, 22.0f };
  float c = 0.90f + (ti - 0.90f) * vivo;
  gfx_cor(d, 0.5f, 1, 1, 1, 0.08f * a);
  if (vivo > 0.0f) gfx_cor(d, 0.5f, fr, fg, fb, vivo * a);
  gfx_icone(ic, ITENS[i].icone, c, c, c + (vivo > 0.5f ? 0.0f : 0.02f), a);
  { TxtLinha n = txt_linha_corta(TXT_BODY, i18n(ITENS[i].nome), 244, 246, 250, 255, tw);
    txt_desenhar_alpha(n, tx, y, a); }
  txt_bloco_corta(TXT_CAPTION, i18n(ITENS[i].linha), 176, 182, 196, tx, y + 33.0f, tw, 27.0f, a, maxL);
}

// Mede a lista (sem rasterizar) e distribui o espaco. A descricao que nao cabe
// numa linha ganha a segunda enquanto houver altura (na ordem da lista). O
// espaco entre as linhas e o que restar, entre 8 e 34 px.
static int linhaMax[32];
static void medirLista(float alto, float *gap) {
  float tx = N170_TXT_X + N170_ICONE + 20.0f, tw = N170_TXT_X + N170_TXT_W - tx;
  float soma = 60.0f * (float)N170_NI, folga;
  int i;
  folga = alto - soma - 8.0f * (float)(N170_NI - 1);
  for (i = 0; i < N170_NI; i++) {
    int larga = txt_largura(TXT_CAPTION, i18n(ITENS[i].linha)) > (int)tw;
    linhaMax[i] = 1;
    linhaH[i] = 60.0f;
    if (larga && folga >= 27.0f) {
      linhaMax[i] = 2; linhaH[i] += 27.0f; soma += 27.0f; folga -= 27.0f;
    }
  }
  *gap = N170_NI > 1 ? anim_clamp((alto - soma) / (float)(N170_NI - 1), 8.0f, 34.0f) : 0.0f;
}

static void ponteiroFoco(int b, int nada) { (void)nada; foco = b; naPrevia = 0; }
static void focarSelo(int nada, int nada2) { (void)nada; (void)nada2; naPrevia = 1; }
static void ponteiroSelo(int nada, int nada2) { (void)nada; (void)nada2; naPrevia = 1; mudarCena(cena + 1); }

static void novidades170_desenharCorpo_(Uint32 agora);
// Cartao de tela quase cheia: ampliado so se ainda couber (escala.h).
void novidades170_desenhar(Uint32 agora) {
  ESCALA_SE_COUBER_INI(N170_W, N170_H);
  novidades170_desenharCorpo_(agora);
  ESCALA_SE_COUBER_FIM();
}
static void novidades170_desenharCorpo_(Uint32 agora) {
  float a = anim_suave(entrada), dy, y0, ar, ag, ab;
  GfxRect card;
  (void)agora;
  if (entrada < 0.002f) return;
  if (aberto) ponteiro_camada();
  ajustes_acento(&ar, &ag, &ab);
  gfx_cor((GfxRect){ 0, 0, NV_TELA_W, NV_TELA_H }, 0.0f, 0, 0, 0, 0.78f * entrada);
  dy = (1.0f - a) * 34.0f;
  y0 = N170_Y + dy;
  card = (GfxRect){ N170_X, y0, N170_W, N170_H };
  gfx_vidro_ou_solido(card, N170_RAIO / N170_H, GFX_VIDRO_DENS_MODAL, a, 0.050f, 0.053f, 0.062f, 0.96f);
  gfx_rect(card, 0, GFX_ANEL, 0, 1.2f / N170_H, 0, N170_RAIO / N170_H, 1, 1, 1, 0.06f * a);
  gfx_luz_canto(card, N170_RAIO / N170_H, N170_PAD + N170_PV_W * 0.5f, -120.0f, 820.0f,
                ar, ag, ab, 0.10f * a);
  recorte(card);

  // ----- a previa: cena que sai e cena que entra, cada uma na sua tesoura.
  { float px = N170_X + N170_PAD, py = y0 + N170_PAD;
    float t = anim_suave(transicao);
    GfxRect pv = { px, py, N170_PV_W, N170_PV_H };
    clipCena = pv;
    recorte(pv);
    gfx_cor(pv, rr(N170_PV_RAIO, pv), 0.062f, 0.066f, 0.080f, a);
    if (transicao < 1.0f && cenaAntiga != cena)
      desenhaCena(cenaAntiga, px - t * 24.0f, py, tempoAntiga, a * (1.0f - anim_clamp(t / 0.5f, 0.0f, 1.0f)));
    desenhaCena(cena, px + (1.0f - t) * 24.0f, py, relogioCena,
                a * (cenaAntiga != cena ? anim_clamp((t - 0.38f) / 0.62f, 0.0f, 1.0f) : t));
    // Aquecer a cena seguinte: numa tesoura de 1 px e alfa quase nulo.
    { int prox = (cena + 1) % N170_NC;
      if (!(aquecida & (1u << prox)) && transicao >= 1.0f) {
        int pend0 = txt_pendentes;
        clipCena = (GfxRect){ px, py, 1.0f, 1.0f };
        recorte(clipCena);
        desenhaCena(prox, px, py, CENAS[prox].estatico, NV_TXTGATE_AQUECER);
        if (txt_pendentes == pend0) aquecida |= 1u << prox;
        clipCena = pv;
        recorte(pv);
      }
      if (txt_pendentes == 0) aquecida |= 1u << cena; }
    selo(px, py, a);
    if (aberto)
      ponteiro_alvo(seloRect.x, seloRect.y, seloRect.w, seloRect.h, focarSelo, ponteiroSelo, 0, 0);
    recorte(card); }

  // ----- a coluna da direita: a linha de cima, o titulo e a lista.
  { char tit[64];
    float ya = y0 + N170_PAD, ta;
    int pend0 = txt_pendentes, i;
    float gap, alto, yy;
    float fechado = textogate_aberto(&gateLista) ? 0.0f : 1.0f;
    ta = fechado > 0.0f ? NV_TXTGATE_AQUECER : a * textogate_passo(&gateLista, 0, SDL_GetTicks());
    snprintf(tit, sizeof tit, i18n("Novidades da %s"), N170_VERSAO);
    { TxtLinha k = txt_linha(TXT_CAPTION2, i18n("ILHA DO RELÓGIO E SPOTLIGHT"),
                             (int)(ar * 90.0f + 150.0f), (int)(ag * 90.0f + 150.0f),
                             (int)(ab * 90.0f + 150.0f), 255);
      txt_desenhar_alpha(k, N170_TXT_X, ya, ta); }
    txt_bloco(TXT_TITULO2, tit, 248, 249, 252, N170_TXT_X, ya + 30.0f, N170_TXT_W, 62.0f, ta, 1);
    yy = ya + 30.0f + 88.0f + 12.0f;
    alto = (y0 + N170_PAD + N170_PV_H) - yy;
    medirLista(alto, &gap);
    for (i = 0; i < N170_NI; i++) {
      float vivo = 0.0f, tA = anim_suave(transicao);
      float local = ajustes_animacoes_reduzidas() || fechado > 0.0f
                  ? 1.0f : anim_clamp((a - 0.04f * (float)i) * 3.0f, 0.0f, 1.0f);
      if (ITENS[i].id == CENAS[cena].id || ITENS[i].id == CENAS[cena].id2) vivo += tA;
      if (transicao < 1.0f && (ITENS[i].id == CENAS[cenaAntiga].id || ITENS[i].id == CENAS[cenaAntiga].id2))
        vivo += 1.0f - tA;
      if (vivo > 1.0f) vivo = 1.0f;
      desenhaItem(i, yy + (1.0f - local) * 12.0f, vivo, linhaMax[i], ta * local);
      yy += linhaH[i] + gap;
    }
    if (fechado > 0.0f) textogate_passo(&gateLista, txt_pendentes - pend0, SDL_GetTicks()); }

  // ----- o rodape: a dica do D-pad a esquerda, os dois botoes a direita.
  { float yBase = y0 + N170_H - N170_PAD;
    const char *rotS = i18n("Abrir o Spotlight");
    const char *rotD = i18n("Agora não");
    float wS = botao_largura(rotS, "menu_search", 1), wD = botao_largura(rotD, NULL, 0);
    GfxRect bS = { N170_X + N170_W - N170_PAD - wS, yBase - BOTAO_H_PRIMARIO, wS, BOTAO_H_PRIMARIO };
    GfxRect bD = { bS.x - BOTAO_GAP - wD, yBase - BOTAO_H_PRIMARIO * 0.5f - BOTAO_H_SECUNDARIO * 0.5f,
                   wD, BOTAO_H_SECUNDARIO };
    { TxtLinha h = txt_linha_corta(TXT_CAPTION2,
                     i18n(naPrevia ? "← → Trocar a prévia  ·  ↓ Botões" : "↑ Escolher a prévia"),
                     150, 156, 170, 255, bD.x - 32.0f - (N170_X + N170_PAD));
      txt_desenhar_alpha(h, N170_X + N170_PAD + 4.0f, yBase - BOTAO_H_PRIMARIO * 0.5f - (float)h.h * 0.5f,
                         a * 0.9f); }
    botao_pilula(bD, rotD, NULL, !naPrevia && foco == B_DEPOIS ? 1.0f : 0.0f, 0, 0, a);
    botao_pilula(bS, rotS, "menu_search", !naPrevia && foco == B_SPOT ? 1.0f : 0.0f, 1, 0, a);
    if (aberto) {
      ponteiro_alvo(bD.x, bD.y, bD.w, bD.h, ponteiroFoco, NULL, B_DEPOIS, 0);
      ponteiro_alvo(bS.x, bS.y, bS.w, bS.h, ponteiroFoco, NULL, B_SPOT, 0);
    } }
  gfx_sem_recorte();
}
