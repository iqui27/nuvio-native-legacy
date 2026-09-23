// Cartao de NOVIDADES DA 1.4.2.
//
// Mesmo painel e mesma gramatica do cartao da 1.4 (novidades1312.c): coluna
// da esquerda com titulo e uma ilustracao nativa, coluna da direita com os
// destaques. A diferenca e o rodape: em vez de um "Continuar" solitario, um
// convite para rodar o diagnostico AGORA, com o que ele faz em uma frase.
//
// UM CARTAO SO. Quem vem de antes da 1.4 e ainda nao viu o cartao dela veria
// os dois em fila; ao fechar este, a marca da 1.4 tambem e gravada (o que
// importa daquela rodada ja esta nesta ou na propria versao).
//
// A APRESENTACAO DO DIAGNOSTICO ("Antes de comecar", diagnostico.c) NAO vem
// logo depois deste cartao. Enquanto ele esta pendente app.c nao decide a
// apresentacao global, e ao fechar este cartao ela e dispensada nesta sessao.
// "Rodar o diagnostico" grava a apresentacao como vista: a frase do rodape ja
// diz o que o teste faz (mede, ajusta, envia o relatorio) e a tela abre direto
// na escolha do objetivo, sem um terceiro aviso.
#include "novidades142.h"
#include "dados.h"
#include "ajustes.h"
#include "diagnostico.h"
#include "gfx.h"
#include "botoes.h"
#include "text.h"
#include "anim.h"
#include "layout.h"
#include "idioma.h"
#include "ponteiro.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define N142_ARQ        "novidades-142.txt"
#define N142_ARQ_14     "novidades-14-celebracao.txt"   // o de novidades1312.c
#define N142_W          1536.0f
#define N142_H           840.0f
#define N142_X          ((NV_TELA_W - N142_W) * 0.5f)
#define N142_Y          ((NV_TELA_H - N142_H) * 0.5f)
#define N142_PAD          56.0f
#define N142_FIG_W       540.0f
#define N142_COL_GAP      56.0f
#define N142_TXT_X      (N142_X + N142_PAD + N142_FIG_W + N142_COL_GAP)
#define N142_TXT_W      (N142_X + N142_W - N142_PAD - N142_TXT_X)
#define N142_FEAT_H       94.0f
#define N142_ICON         50.0f
#define N142_ABRIR_MS    280.0f
#define N142_FECHAR_MS   160.0f

enum { B_DEPOIS = 0, B_DIAGNOSTICO = 1 };

static int   aberto, decidido, foco = B_DIAGNOSTICO, pedido;
static int   vistoCache = -1;
static float entrada, fase;

static int visto(void) {
  if (vistoCache < 0) {
    char *s = dados_ler(N142_ARQ);
    vistoCache = s != NULL;
    free(s);
  }
  return vistoCache;
}

int novidades142_aberto(void) { return aberto; }

int novidades142_pendente(void) {
  return aberto || (!decidido && !visto());
}

int novidades142_pediu_diagnostico(void) {
  int p = pedido;
  pedido = 0;
  return p;
}

void novidades142_abrir(void) {
  aberto = 1;
  decidido = 1;
  foco = B_DIAGNOSTICO;
  entrada = 1.0f;
  fase = 0.0f;
}

void novidades142_primeira_vez(void) {
  if (decidido) return;
  decidido = 1;
  if (visto()) return;
  aberto = 1;
  foco = B_DIAGNOSTICO;
  entrada = 0.0f;
  fase = 0.0f;
}

static void fechar(int rodar) {
  aberto = 0;
  dados_gravar(N142_ARQ, "1\n");
  dados_gravar(N142_ARQ_14, "1\n");
  vistoCache = 1;
  // 1 = grava a apresentacao como vista (a tela abre direto no objetivo);
  // 0 = so nao a mostra nesta sessao.
  diagnostico_intro_dispensar(rodar);
  pedido = rodar;
}

void novidades142_evento(const SDL_Event *e) {
  SDL_Keycode k;
  if (!aberto || e->type != SDL_KEYDOWN) return;
  k = e->key.keysym.sym;
  if (k == SDLK_LEFT) { foco = B_DEPOIS; return; }
  if (k == SDLK_RIGHT) { foco = B_DIAGNOSTICO; return; }
  if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) {
    fechar(foco == B_DIAGNOSTICO);
    return;
  }
  if (k == SDLK_AC_BACK || k == SDLK_ESCAPE || k == SDLK_BACKSPACE ||
      k == SDLK_DELETE || e->key.keysym.scancode == NV_SCANCODE_BACK)
    fechar(0);
}

void novidades142_atualizar(float dt, Uint32 agora) {
  (void)agora;
  if (!aberto && entrada < 0.002f) { entrada = 0.0f; return; }
  if (ajustes_animacoes_reduzidas()) {
    entrada = aberto ? 1.0f : 0.0f;
    fase = 0.0f;
    return;
  }
  entrada = anim_rampa(entrada, aberto ? 1.0f : 0.0f, dt,
                       aberto ? N142_ABRIR_MS : N142_FECHAR_MS);
  if (aberto) {
    fase += dt * 1.6f;
    if (fase > 6.2831853f) fase -= 6.2831853f;
  }
}

// A mesma paleta de quatro cores da ilustracao da 1.4.
static const float PALETA[4][3] = {
  { 0.98f, 0.37f, 0.36f },  // coral
  { 1.00f, 0.74f, 0.18f },  // amarelo
  { 0.24f, 0.82f, 0.72f },  // turquesa
  { 0.62f, 0.46f, 0.96f }   // violeta
};

// Segmento curto com GFX_LINHA (ver gfx.h): o envelope e o proprio segmento
// mais o halo, e todos aqui sao pequenos — nao precisa partir em pedacos como
// a constelacao da Explorar.
static void linha(float x0, float y0, float x1, float y1, float esp, float halo,
                  float r, float g, float b, float a) {
  GfxRect q;
  if (a <= 0.004f) return;
  q.x = fminf(x0, x1) - halo; q.y = fminf(y0, y1) - halo;
  q.w = fabsf(x1 - x0) + 2.0f * halo; q.h = fabsf(y1 - y0) + 2.0f * halo;
  gfx_rect(q, 0, GFX_LINHA, esp / q.h, ((x1 - x0) * (y1 - y0) < 0.0f) ? 1.0f : 0.0f,
           0, halo / q.h, r, g, b, a);
}

// O cursor do Magic Remote como ponteiro.c o desenha: disco branco, miolo
// escuro e sombra. `sobre` cresce o disco como quando ha alvo embaixo.
static void cursor(float px, float py, float d, float a) {
  gfx_cor((GfxRect){ px - d * 0.5f - 3.0f, py - d * 0.5f - 1.0f, d + 6.0f, d + 6.0f },
          0.5f, 0, 0, 0, 0.45f * a);
  gfx_cor((GfxRect){ px - d * 0.5f, py - d * 0.5f, d, d }, 0.5f, 1, 1, 1, 0.96f * a);
  d -= d * 0.38f;
  gfx_cor((GfxRect){ px - d * 0.5f, py - d * 0.5f, d, d }, 0.5f,
          0.10f, 0.11f, 0.13f, 0.75f * a);
}

// A ILUSTRACAO: a TV da 1.4, agora com o ceu das historias na tela — cinco
// estrelas ligadas — e o cursor do controle pousado numa delas. Conta tres
// destaques de uma vez sem depender de arte de catalogo.
static void desenhaFigura(float x, float y, float w, float h, float a, float t) {
  static const float NOS[5][2] = {
    { -78.0f, -30.0f }, { -24.0f, -58.0f }, { 30.0f, -14.0f },
    { 86.0f, -46.0f }, { -2.0f, 38.0f }
  };
  static const int ELOS[5][2] = { {0,1}, {1,2}, {2,3}, {2,4}, {0,4} };
  float ar, ag, ab;
  float cx = x + w * 0.5f;
  float cy = y + h * 0.52f + sinf(t * 1.4f) * 3.0f;
  float sx = cx - 6.0f, sy = cy - 10.0f;   // centro da tela da TV
  int i;
  ajustes_acento(&ar, &ag, &ab);

  gfx_cor((GfxRect){ x, y, w, h }, 28.0f / h, 0.035f, 0.055f, 0.105f, a);
  gfx_rect((GfxRect){ cx - 200.0f, cy - 200.0f, 400.0f, 400.0f },
           0, GFX_DISCO, 0, 0, 0, 0, ar, ag, ab, 0.075f * a);
  // SEM GFX_ANEL aqui: o anel e `abs(d) < espessura` no SDF do proprio
  // retangulo, e a metade de fora e cortada pelo quad — num circulo grande isso
  // aparece como quatro trechos retos. Um segundo disco, violeta e fraco, faz
  // o mesmo papel de moldura.
  gfx_rect((GfxRect){ cx - 176.0f, cy - 176.0f, 352.0f, 352.0f },
           0, GFX_DISCO, 0, 0, 0, 0,
           PALETA[3][0], PALETA[3][1], PALETA[3][2], 0.10f * a);

  // Estrelas soltas fora da TV: o ceu continua alem da moldura.
  { static const float SOLTAS[8][3] = {
      { -176.0f, -120.0f, 5.0f }, { 150.0f, -150.0f, 4.0f }, { 188.0f, 30.0f, 6.0f },
      { -190.0f, 70.0f, 4.0f }, { -120.0f, 168.0f, 5.0f }, { 128.0f, 150.0f, 4.0f },
      { 40.0f, -178.0f, 3.0f }, { -60.0f, -170.0f, 4.0f } };
    for (i = 0; i < 8; i++) {
      float tw = 0.55f + 0.45f * (0.5f + 0.5f * sinf(t * 2.0f + (float)i * 1.7f));
      float s = SOLTAS[i][2];
      gfx_cor((GfxRect){ cx + SOLTAS[i][0] - s * 0.5f, cy + SOLTAS[i][1] - s * 0.5f, s, s },
              0.5f, 0.86f, 0.90f, 1.0f, tw * a);
    } }

  // Antena, moldura coral, tela escura.
  gfx_cor((GfxRect){ cx - 5.0f, cy - 128.0f, 10.0f, 30.0f }, 0.5f,
          PALETA[1][0], PALETA[1][1], PALETA[1][2], a);
  gfx_cor((GfxRect){ cx - 28.0f, cy - 136.0f, 56.0f, 9.0f }, 0.5f,
          PALETA[1][0], PALETA[1][1], PALETA[1][2], a);
  gfx_cor((GfxRect){ cx - 158.0f, cy - 106.0f, 316.0f, 196.0f }, 0.10f,
          PALETA[0][0], PALETA[0][1], PALETA[0][2], a);
  gfx_cor((GfxRect){ cx - 144.0f, cy - 92.0f, 288.0f, 168.0f }, 0.075f,
          0.030f, 0.034f, 0.070f, a);
  gfx_rect((GfxRect){ cx - 150.0f, cy - 120.0f, 260.0f, 200.0f },
           0, GFX_SOMBRA, 0.35f, 0, 0, 0, ar, ag, ab, 0.18f * a);

  // A constelacao: elos primeiro, estrelas por cima.
  for (i = 0; i < 5; i++) {
    const float *p0 = NOS[ELOS[i][0]], *p1 = NOS[ELOS[i][1]];
    linha(sx + p0[0], sy + p0[1], sx + p1[0], sy + p1[1], 2.2f, 7.0f,
          ar, ag, ab, 0.80f * a);
  }
  // O realce da estrela em que o cursor pousou: disco do acento por baixo
  // da estrela branca (o foco da Explorar), e nao um anel — ver acima.
  gfx_cor((GfxRect){ sx + NOS[2][0] - 17.0f, sy + NOS[2][1] - 17.0f, 34.0f, 34.0f },
          0.5f, ar, ag, ab, a);
  for (i = 0; i < 5; i++) {
    float s = i == 2 ? 20.0f : 12.0f;
    const float *c = PALETA[i & 3];
    gfx_rect((GfxRect){ sx + NOS[i][0] - s, sy + NOS[i][1] - s, s * 2.0f, s * 2.0f },
             0, GFX_SOMBRA, 0.5f, 0, 0, 0, c[0], c[1], c[2], 0.35f * a);
    gfx_cor((GfxRect){ sx + NOS[i][0] - s * 0.5f, sy + NOS[i][1] - s * 0.5f, s, s },
            0.5f, i == 2 ? 1.0f : c[0], i == 2 ? 1.0f : c[1], i == 2 ? 1.0f : c[2], a);
  }

  // Pes da TV.
  gfx_cor((GfxRect){ cx - 110.0f, cy + 98.0f, 44.0f, 20.0f }, 0.22f,
          PALETA[3][0], PALETA[3][1], PALETA[3][2], a);
  gfx_cor((GfxRect){ cx + 66.0f, cy + 98.0f, 44.0f, 20.0f }, 0.22f,
          PALETA[3][0], PALETA[3][1], PALETA[3][2], a);

  // O cursor chega pela diagonal e pousa ao lado da estrela em foco.
  cursor(sx + NOS[2][0] + 24.0f + sinf(t) * 4.0f,
         sy + NOS[2][1] + 26.0f + cosf(t * 1.3f) * 3.0f, 30.0f, a);
}

typedef struct { const char *titulo, *descricao; } N142Destaque;

static const N142Destaque DESTAQUES[] = {
  { "Cursor do Magic Remote",
    "Aponte e clique pelas telas com o controle da LG." },
  { "Explorar nova",
    "O céu das suas histórias: o que você viu, ligado ao que vem a seguir." },
  { "Legendas ASS de anime",
    "Desenhadas pelo próprio app, com estilo e no tempo certo." },
  { "Artes mais rápidas",
    "E destaque com arte diferente de verdade: Apple TV, TMDB e fanart.tv." },
  { "Diagnóstico e otimização automática",
    "O app mede esta TV e se ajusta a ela." }
};

// Um desenho pequeno por destaque, no disco de realce a 18 % da 1.4.
static void miniGrafico(float x, float y, int tipo, float ar, float ag, float ab, float a) {
  float xr = 0.93f, xg = 0.95f, xb = 0.99f;
  gfx_rect((GfxRect){ x, y, N142_ICON, N142_ICON }, 0, GFX_DISCO, 0, 0, 0, 0,
           ar, ag, ab, 0.18f * a);
  switch (tipo) {
    case 0:  // cursor sobre um alvo
      gfx_cor((GfxRect){ x + 10, y + 12, 22, 16 }, 4.0f / 16.0f, ar, ag, ab, 0.45f * a);
      cursor(x + 32.0f, y + 32.0f, 16.0f, a);
      break;
    case 1:  // tres estrelas ligadas
      linha(x + 13, y + 32, x + 24, y + 16, 1.6f, 3.0f, xr, xg, xb, 0.75f * a);
      linha(x + 24, y + 16, x + 37, y + 30, 1.6f, 3.0f, xr, xg, xb, 0.75f * a);
      gfx_cor((GfxRect){ x + 10, y + 29, 7, 7 }, 0.5f, PALETA[2][0], PALETA[2][1], PALETA[2][2], a);
      gfx_cor((GfxRect){ x + 20, y + 12, 9, 9 }, 0.5f, xr, xg, xb, a);
      gfx_cor((GfxRect){ x + 34, y + 27, 7, 7 }, 0.5f, PALETA[1][0], PALETA[1][1], PALETA[1][2], a);
      break;
    case 2:  // falas coloridas
      gfx_cor((GfxRect){ x + 10, y + 14, 30, 4 }, 0.5f, xr, xg, xb, a);
      gfx_cor((GfxRect){ x + 10, y + 22, 20, 4 }, 0.5f, PALETA[0][0], PALETA[0][1], PALETA[0][2], a);
      gfx_cor((GfxRect){ x + 10, y + 30, 27, 4 }, 0.5f, PALETA[2][0], PALETA[2][1], PALETA[2][2], a);
      gfx_cor((GfxRect){ x + 10, y + 38, 14, 4 }, 0.5f, PALETA[1][0], PALETA[1][1], PALETA[1][2], a);
      break;
    case 3:  // tres artes diferentes, lado a lado
      gfx_cor((GfxRect){ x + 8, y + 15, 12, 20 }, 3.0f / 20.0f, PALETA[3][0], PALETA[3][1], PALETA[3][2], a);
      gfx_cor((GfxRect){ x + 19, y + 11, 13, 28 }, 3.0f / 28.0f, xr, xg, xb, a);
      gfx_cor((GfxRect){ x + 31, y + 15, 12, 20 }, 3.0f / 20.0f, PALETA[2][0], PALETA[2][1], PALETA[2][2], a);
      break;
    default: // barras de medida subindo
      gfx_cor((GfxRect){ x + 11, y + 28, 6, 11 }, 0.3f, xr, xg, xb, 0.60f * a);
      gfx_cor((GfxRect){ x + 20, y + 21, 6, 18 }, 0.3f, xr, xg, xb, 0.80f * a);
      gfx_cor((GfxRect){ x + 29, y + 13, 6, 26 }, 0.3f, ar, ag, ab, a);
      gfx_cor((GfxRect){ x + 10, y + 41, 27, 2 }, 0.5f, xr, xg, xb, 0.40f * a);
      break;
  }
}

static void destaque(float x, float y, float w, int i, float a) {
  float ar, ag, ab, local, yy, alpha;
  float tx = x + N142_ICON + 22.0f;
  float tw = w - N142_ICON - 22.0f;
  ajustes_acento(&ar, &ag, &ab);
  local = ajustes_animacoes_reduzidas()
      ? 1.0f : anim_clamp((a - 0.04f * (float)i) * 4.0f, 0.0f, 1.0f);
  yy = y + (1.0f - local) * 16.0f;
  alpha = a * local;
  miniGrafico(x, yy + 8.0f, i, ar, ag, ab, alpha);
  { TxtLinha t = txt_linha_corta(TXT_CALLOUT, i18n(DESTAQUES[i].titulo),
                                 246, 247, 252, 255, tw);
    txt_desenhar_alpha(t, tx, yy + 2.0f, alpha); }
  txt_bloco(TXT_CAPTION, i18n(DESTAQUES[i].descricao), 178, 186, 201,
            tx, yy + 38.0f, tw, 24.0f, alpha * 0.96f, 2);
}

static void ponteiroFoco(int b, int nada) { (void)nada; foco = b; }

void novidades142_desenhar(Uint32 agora) {
  float a = anim_suave(entrada), dy;
  float ar, ag, ab;
  (void)agora;
  if (entrada < 0.002f) return;
  ajustes_acento(&ar, &ag, &ab);

  gfx_cor((GfxRect){ 0, 0, NV_TELA_W, NV_TELA_H }, 0.0f, 0, 0, 0, 0.74f * entrada);
  dy = (1.0f - a) * 34.0f;
  gfx_cor((GfxRect){ N142_X, N142_Y + dy, N142_W, N142_H },
          30.0f / N142_H, 0.055f, 0.058f, 0.068f, 0.94f * a);
  gfx_luz_canto((GfxRect){ N142_X, N142_Y + dy, N142_W, N142_H },
                30.0f / N142_H, N142_W * 0.10f, -N142_H * 0.10f,
                N142_H * 0.65f, ar, ag, ab, 0.22f * a);
  gfx_recorte(N142_X, N142_Y + dy, N142_W, N142_H);

  { float fx = N142_X + N142_PAD;
    { TxtLinha t = txt_linha(TXT_CAPTION2, i18n("NOVO NA 1.4.2"), 155, 166, 185, 255);
      txt_desenhar_alpha(t, fx, N142_Y + dy + 56.0f, a * 0.92f); }
    txt_bloco(TXT_TITULO2, i18n("O que há de novo"), 248, 249, 252,
              fx, N142_Y + dy + 88.0f, N142_FIG_W, 58.0f, a, 2);
    txt_bloco(TXT_BODY,
              i18n("Uma versão para apontar, explorar e deixar o app sob medida para a sua TV."),
              205, 213, 226, fx, N142_Y + dy + 166.0f, N142_FIG_W, 34.0f, a * 0.98f, 3);
    desenhaFigura(fx, N142_Y + dy + 290.0f, N142_FIG_W, N142_H - 290.0f - N142_PAD,
                  a, fase); }

  { int i;
    float y0 = N142_Y + dy + 58.0f;
    for (i = 0; i < (int)(sizeof DESTAQUES / sizeof DESTAQUES[0]); i++)
      destaque(N142_TXT_X, y0 + (float)i * N142_FEAT_H, N142_TXT_W, i, a); }

  // O RODAPE: um filete, a frase do diagnostico e os dois botoes da tabela
  // (botoes.h) alinhados pela base, o primario a direita e com o foco ao
  // abrir — OK sozinho ja roda o diagnostico.
  { float yBase = N142_Y + dy + N142_H - N142_PAD;   // base dos botoes
    float yFilete = N142_Y + dy + 58.0f + 5.0f * N142_FEAT_H + 14.0f;
    const char *rotP = i18n("Rodar o diagnóstico");
    const char *rotS = i18n("Depois");
    float wP = botao_largura(rotP, NULL, 1), wS = botao_largura(rotS, NULL, 0);
    GfxRect bP = { N142_X + N142_W - N142_PAD - wP, yBase - BOTAO_H_PRIMARIO,
                   wP, BOTAO_H_PRIMARIO };
    GfxRect bS = { bP.x - BOTAO_GAP - wS, yBase - BOTAO_H_SECUNDARIO,
                   wS, BOTAO_H_SECUNDARIO };
    gfx_cor((GfxRect){ N142_TXT_X, yFilete, N142_TXT_W, 1.5f }, 0.0f,
            1.0f, 1.0f, 1.0f, 0.08f * a);
    txt_bloco(TXT_BODY,
              i18n("Leva 1 minuto: mede esta TV, ajusta o app sozinho e envia o relatório para melhorarmos o Nuvio para todo mundo."),
              214, 219, 230, N142_TXT_X, yFilete + 26.0f, N142_TXT_W, 34.0f, a * 0.98f, 3);
    botao_pilula(bS, rotS, NULL, foco == B_DEPOIS ? 1.0f : 0.0f, 0, 0, a);
    botao_pilula(bP, rotP, NULL, foco == B_DIAGNOSTICO ? 1.0f : 0.0f, 1, 0, a);
    if (aberto) {
      ponteiro_alvo(bS.x, bS.y, bS.w, bS.h, ponteiroFoco, NULL, B_DEPOIS, 0);
      ponteiro_alvo(bP.x, bP.y, bP.w, bP.h, ponteiroFoco, NULL, B_DIAGNOSTICO, 0);
    } }
  gfx_sem_recorte();
}
