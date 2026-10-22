// Ver psestilos.h.
#include "psestilos.h"
#include "psparede.h"
#include "gfx.h"
#include "layout.h"
#include "tex_cache.h"
#include "cachearte.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// --- PAREDE (Filmes) ---------------------------------------------------------
// Medidas em tela 1920x1080: o mockup aprovado (artifact 5XkkA6NoVho9DDw1E5zKy5)
// com a parede a 1,18x e girada -0,16 rad.
#define PAR_GIRO      (-0.16f)
#define PAR_CW        245.0f
#define PAR_CH        368.0f
#define PAR_GAP        42.0f
#define PAR_COLS          9
#define PAR_LARG_TEX  250.0f
#define PAR_TROCA_S     0.9f
#define PAR_ALFA        0.62f

typedef struct {
  int  n;
  char url[PSEST_PAREDE_MAX][PSPAREDE_URL];
  GLuint tex[PSEST_PAREDE_MAX];
  int  perfil;
} ParSet;

static ParSet parAtual, parAnt;
static int    parTemAnt;
static float  parTroca = 1.0f;     // 0..1 da parede anterior para a atual
static float  tempo;

// --- LUZ ---------------------------------------------------------------------
static float  luzX = -1.0f;
static GLuint graoTex;
static unsigned semente = 0x2545f491u;
static float  graoDx, graoDy;

// --- PROJETOR ----------------------------------------------------------------
#define PJ_TOPO_X   (NV_TELA_W * 0.5f)
#define PJ_TOPO_Y    56.0f
#define PJ_ABRE     806.0f           // meia largura do feixe na base da tela
#define PJ_POEIRA      70
#define PJ_FAIXA_H     44.0f
#define PJ_FURO_PASSO  72.0f
typedef struct { float x, y, z, f; } Poeira;
static Poeira poeira[PJ_POEIRA];
static float  riscoX, riscoV;

static float aleat(void) { semente = semente * 1664525u + 1013904223u; return (float)(semente >> 8) / 16777216.0f; }
static float suave(float t) { t = t < 0 ? 0 : t > 1 ? 1 : t; return t * t * (3.0f - 2.0f * t); }

void psestilos_iniciar(void) {
  int i;
  tempo = 0.0f;
  parAtual.n = 0; parAtual.perfil = -1; parTemAnt = 0; parTroca = 1.0f;
  luzX = -1.0f;
  for (i = 0; i < PJ_POEIRA; i++) {
    poeira[i].x = aleat(); poeira[i].y = aleat(); poeira[i].z = aleat(); poeira[i].f = aleat() * 6.0f;
  }
  riscoV = 0.0f;
}

// Grao de filme: alfa aleatorio numa textura de 640x360, esticada na tela
// inteira (graos de ~3 px). Uma vez por sessao do app.
static void graoGarantir(void) {
  unsigned char *px;
  int i, n = 640 * 360;
  if (graoTex) return;
  px = malloc((size_t)n * 4);
  if (!px) return;
  for (i = 0; i < n; i++) {
    px[i * 4] = px[i * 4 + 1] = px[i * 4 + 2] = 255;
    px[i * 4 + 3] = (unsigned char)(aleat() * 255.0f);
  }
  glGenTextures(1, &graoTex);
  glBindTexture(GL_TEXTURE_2D, graoTex);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 640, 360, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
  free(px);
}

static void graoDesenhar(float alfa) {
  if (!graoTex || alfa <= 0.001f) return;
  gfx_rect((GfxRect){ -graoDx, -graoDy, NV_TELA_W + 120.0f, NV_TELA_H + 68.0f }, graoTex,
           GFX_TEXTO, 0, 0, 0, 0.0f, 1, 1, 1, alfa);
}

// A lista de URLs que a parede do perfil em foco deve mostrar.
static void parMontar(ParSet *s, const PSCena *c) {
  int i, n = c->semParede ? 0 : psparede_n(c->perfil);
  s->n = 0;
  s->perfil = c->perfil;
  if (n >= 4) {
    for (i = 0; i < n && s->n < PSEST_PAREDE_MAX; i++) {
      const char *u = psparede_url(c->perfil, i);
      if (u) snprintf(s->url[s->n++], PSPAREDE_URL, "%s", u);
    }
  } else {
    for (i = 0; i < c->muralN && s->n < PSEST_PAREDE_MAX; i++)
      if (c->mural[i] && c->mural[i][0]) snprintf(s->url[s->n++], PSPAREDE_URL, "%s", c->mural[i]);
  }
  for (i = 0; i < s->n; i++) {
    s->tex[i] = 0;
    tex_cache_marcar_larg(NV_CACHE_ARTE_GRUPO_PERFIL, s->url[i], PAR_LARG_TEX, 1, 0);
  }
}

static void parPedir(ParSet *s) {
  int i;
  for (i = 0; i < s->n; i++) s->tex[i] = tex_obter_larg_qualquer(s->url[i], PAR_LARG_TEX);
}

void psestilos_atualizar(float dt, const PSCena *c, int modo) {
  int anda = !c->reduzida && !c->parado;
  if (anda) tempo += dt;
  if (c->reduzida) tempo = 0.0f;
  graoDx = anda ? aleat() * 120.0f : 0.0f;
  graoDy = anda ? aleat() * 68.0f : 0.0f;
  if (modo == PS_FUNDO_FILMES) {
    // Foco mudou de pessoa (ou o mural chegou e a parede estava vazia): a
    // parede atual vira a anterior e sai num cross-fade.
    if (c->perfil != parAtual.perfil || parAtual.n == 0) {
      ParSet novo;
      parMontar(&novo, c);
      if (novo.n > 0 && (c->perfil != parAtual.perfil || novo.n != parAtual.n)) {
        if (parAtual.n > 0 && c->perfil != parAtual.perfil) {
          parAnt = parAtual; parTemAnt = 1; parTroca = c->reduzida ? 1.0f : 0.0f;
        }
        parAtual = novo;
      }
    }
    parPedir(&parAtual);
    if (parTemAnt) {
      parPedir(&parAnt);
      parTroca += dt / PAR_TROCA_S;
      if (parTroca >= 1.0f) { parTroca = 1.0f; parTemAnt = 0; }
    }
  }
  if (modo == PS_FUNDO_LUZ || modo == PS_FUNDO_PROJETOR) graoGarantir();
  if (modo == PS_FUNDO_LUZ) {
    if (luzX < 0.0f || c->reduzida) luzX = c->xFoco;
    else luzX += (c->xFoco - luzX) * (dt * 2.2f > 1.0f ? 1.0f : dt * 2.2f);
  }
  if (modo == PS_FUNDO_PROJETOR && anda) {
    int i;
    for (i = 0; i < PJ_POEIRA; i++) {
      Poeira *p = &poeira[i];
      p->y -= dt * (0.006f + 0.02f * p->z);
      p->x += sinf(tempo * 0.7f + p->f) * dt * 0.004f;
      if (p->y < 0.0f) { p->y = 1.0f; p->x = aleat(); }
    }
    if (aleat() < 0.012f) { riscoX = aleat() * NV_TELA_W; riscoV = 0.18f; }
    if (riscoV > 0.0f) { riscoV -= dt * 0.9f; riscoX += dt * 28.0f; }
  }
}

// --- desenho -----------------------------------------------------------------
static void veusBordas(float topo, float base, float lados, float alfa) {
  gfx_rect((GfxRect){ 0, 0, NV_TELA_W, 330.0f }, 0, GFX_VEU_TOPO, 0, 0, 0, 0, 0, 0, 0, topo * alfa);
  gfx_rect((GfxRect){ 0, NV_TELA_H - 300.0f, NV_TELA_W, 300.0f }, 0, GFX_VEU_BAIXO,
           0, 0, 0, 0, 0, 0, 0, base * alfa);
  if (lados > 0.0f) {
    gfx_rect((GfxRect){ -900.0f, -560.0f, 1400.0f, 2200.0f }, 0, GFX_SOMBRA, 1.0f, 0, 0, 0.5f,
             0, 0, 0, lados * alfa);
    gfx_rect((GfxRect){ NV_TELA_W - 500.0f, -560.0f, 1400.0f, 2200.0f }, 0, GFX_SOMBRA, 1.0f, 0, 0,
             0.5f, 0, 0, 0, lados * alfa);
  }
}

static float veuTela(float y);
static void parDesenhar(const ParSet *s, float alfa) {
  int col, k, leve = gfx_efeitos_leves();   // veus de borda no fragmento so no leve
  float passoY = PAR_CH + PAR_GAP;
  if (s->n <= 0 || alfa <= 0.001f) return;
  gfx_girar(PAR_GIRO, NV_TELA_W * 0.5f, NV_TELA_H * 0.5f);
  for (col = 0; col < PAR_COLS; col++) {
    int dir = col % 2 ? 1 : -1;
    float vel = 20.0f + (float)(col % 3) * 8.0f;
    float off = fmodf(tempo * vel * (float)dir, passoY);
    float x = NV_TELA_W * 0.5f + ((float)col - PAR_COLS * 0.5f) * (PAR_CW + PAR_GAP);
    float dist = fabsf((float)col - (PAR_COLS - 1) * 0.5f) / ((PAR_COLS - 1) * 0.5f);
    // As colunas das pontas apagam AQUI (0,95 no meio, 0,30 na borda), e nao
    // mais por dois veus radiais de 1400x2200 por cima: na C9 a tela ficava em
    // 34 fps e, sem esses dois quads, em 60 (medido peca por peca, 05/10/2026).
    float a = (0.95f - dist * dist * 0.65f) * alfa;
    if (off < 0.0f) off += passoY;
    for (k = -3; k < 3; k++) {
      float y = NV_TELA_H * 0.5f + (float)k * passoY + off - passoY * 0.5f;
      int idx = ((col * 3 + k + 30) % s->n + s->n) % s->n;
      GLuint t = s->tex[idx];
      // Corte grosso: o cartao girado que nem encosta na tela nao vai a GPU.
      float cx = x + PAR_CW * 0.5f - NV_TELA_W * 0.5f, cy = y + PAR_CH * 0.5f - NV_TELA_H * 0.5f;
      float rx = cx * cosf(PAR_GIRO) - cy * sinf(PAR_GIRO) + NV_TELA_W * 0.5f;
      float ry = cx * sinf(PAR_GIRO) + cy * cosf(PAR_GIRO) + NV_TELA_H * 0.5f;
      if (rx < -260.0f || rx > NV_TELA_W + 260.0f || ry < -260.0f || ry > NV_TELA_H + 260.0f) continue;
      if (!t) {
        // Sem textura o quad nao passa pelo GFX_CARD: com os veus no fragmento
        // (leve) o veu de borda entra aqui pelo centro do cartao (veuTela).
        float v = leve ? veuTela(ry) : 1.0f;
        gfx_cor((GfxRect){ x, y, PAR_CW, PAR_CH }, 0.06f, 0.10f * v, 0.10f * v, 0.12f * v, a * 0.6f);
        continue;
      }
      gfx_tex_aspect_atual = tex_aspecto(s->url[idx]);
      gfx_card_forcar_cover_atual = 1.0f;
      gfx_card_veu_tela_atual = leve ? 1.0f : 0.0f;   // os veus de borda, no fragmento (gfx.h)
      gfx_rect((GfxRect){ x, y, PAR_CW, PAR_CH }, t, GFX_CARD, 0, 0, 0, 0.06f, 1, 1, 1, a);
      gfx_card_veu_tela_atual = 0.0f;
      gfx_card_forcar_cover_atual = 0.0f;
      gfx_tex_aspect_atual = 0.0f;
    }
  }
  gfx_sem_girar();
}

// O que sobra da cor em y (pixels de tela) depois dos dois veus de borda da
// parede — a mesma conta do GFX_CARD com gfx_card_veu_tela_atual (gfx.h).
static float veuTela(float y) {
  float sy = y / NV_TELA_H, vt, tb, gb, g;
  vt = sy / (330.0f / NV_TELA_H); vt = vt < 0.0f ? 0.0f : vt > 1.0f ? 1.0f : vt;
  g = (vt - 1.0f) / (0.15f - 1.0f); g = g < 0.0f ? 0.0f : g > 1.0f ? 1.0f : g;
  vt = g * g * (3.0f - 2.0f * g) * 0.92f;
  tb = (sy - 780.0f / NV_TELA_H) / (300.0f / NV_TELA_H); tb = tb < 0.0f ? 0.0f : tb > 1.0f ? 1.0f : tb;
  gb = tb * tb * (3.0f - 2.0f * tb); gb = gb * gb * 0.95f;
  return (1.0f - vt) * (1.0f - gb);
}

static void desenharFilmes(const PSCena *c, float alfa) {
  float k = suave(parTroca);
  gfx_cor((GfxRect){ 0, 0, NV_TELA_W, NV_TELA_H }, 0, 0, 0, 0, 1);
  if (parTemAnt) parDesenhar(&parAnt, (1.0f - k) * PAR_ALFA * alfa);
  parDesenhar(&parAtual, (parTemAnt ? k : 1.0f) * PAR_ALFA * alfa);
  // Os dois veus de borda (veusBordas 0,92 / 0,95): com EFEITOS LEVES eles
  // ja sairam DENTRO de cada capa (gfx_card_veu_tela_atual) — sobre o preto e
  // o mesmo pixel, e sao duas passadas de largura inteira a menos por quadro
  // (0,59 telas; MEDIDO na TCL Smart TV Pro, Mali-G52, presa em
  // preenchimento). No nivel 0 ficam as duas passadas de sempre: na LG C9
  // (Mali-G71) a parede com os veus no fragmento caiu de 60 para 44 fps
  // (06/10/2026) — la o fragmento do cartaz e que custa, nao a passada.
  if (!gfx_efeitos_leves()) veusBordas(0.92f, 0.95f, 0.0f, alfa);
  // A luz do perfil sobe do chao, na cor dele. Uma mancha radial de ~1 tela
  // visivel, com mistura, por quadro: com efeitos leves (nivel 1 de
  // gpunivel.h, GPU presa) ela sai primeiro — e enfeite, a parede e o disco
  // do avatar continuam marcando o perfil. No nivel 2 o GFX_SOMBRA ja saia.
  if (!gfx_efeitos_leves())
    gfx_rect((GfxRect){ NV_TELA_W * 0.5f - 1250.0f, NV_TELA_H + 54.0f - 1250.0f, 2500.0f, 2500.0f },
             0, GFX_SOMBRA, 1.0f, 0, 0, 0.5f, c->luz[0], c->luz[1], c->luz[2], 0.50f * alfa);
}

static void desenharLuz(const PSCena *c, float alfa) {
  float t = tempo, resp = c->reduzida ? 1.0f : 0.85f + 0.15f * sinf(t * 0.6f);
  float lx = luzX < 0.0f ? c->xFoco : luzX;
  gfx_cor((GfxRect){ 0, 0, NV_TELA_W, NV_TELA_H }, 0, 0.012f, 0.012f, 0.016f, 1);
  { float cy = 660.0f + sinf(t * 0.3f) * 40.0f, r = 1040.0f;
    gfx_rect((GfxRect){ lx - 120.0f - r, cy - r, r * 2.0f, r * 2.0f }, 0, GFX_SOMBRA, 1.0f, 0, 0, 0.5f,
             c->luz[0], c->luz[1], c->luz[2], 0.55f * resp * alfa); }
  { float vx = NV_TELA_W * 0.82f + cosf(t * 0.22f) * 80.0f, vy = 80.0f + sinf(t * 0.27f) * 60.0f, r = 960.0f;
    gfx_rect((GfxRect){ vx - r, vy - r, r * 2.0f, r * 2.0f }, 0, GFX_SOMBRA, 1.0f, 0, 0, 0.5f,
             c->luz[3], c->luz[4], c->luz[5], 0.30f * alfa); }
  // Horizonte: uma linha de luz que some nas pontas.
  gfx_rect((GfxRect){ NV_TELA_W * 0.5f - 820.0f, 896.0f, 1640.0f, 6.0f }, 0, GFX_SOMBRA, 1.0f, 0, 0, 0.5f,
           c->luz[0], c->luz[1], c->luz[2], 0.55f * resp * alfa);
  veusBordas(0.60f, 0.65f, 0.55f, alfa);
  graoDesenhar(0.055f * alfa);
}

static void desenharProjetor(const PSCena *c, float alfa) {
  static const float AMB[3] = { 0.95f, 0.67f, 0.35f };
  const float FUNDO[3] = { 0.039f, 0.027f, 0.020f };
  float cor[3], pisca, th = atanf(PJ_ABRE / (NV_TELA_H - PJ_TOPO_Y));
  int i;
  for (i = 0; i < 3; i++) cor[i] = AMB[i] + (c->luz[i] - AMB[i]) * 0.35f;
  pisca = c->reduzida ? 1.0f : 0.93f + 0.05f * sinf(tempo * 41.0f) + 0.03f * sinf(tempo * 17.3f);
  gfx_cor((GfxRect){ 0, 0, NV_TELA_W, NV_TELA_H }, 0, FUNDO[0], FUNDO[1], FUNDO[2], 1);
  // O feixe: a luz nasce na lente e as duas laminas cortam o cone.
  { float r = 1150.0f;
    gfx_rect((GfxRect){ PJ_TOPO_X - r, PJ_TOPO_Y - r, r * 2.0f, r * 2.0f }, 0, GFX_SOMBRA, 1.0f, 0, 0, 0.5f,
             cor[0], cor[1], cor[2], 0.55f * pisca * alfa); }
  // Poeira dentro do feixe, antes das laminas: o que sai do cone some junto.
  for (i = 0; i < PJ_POEIRA; i++) {
    const Poeira *p = &poeira[i];
    float yy = PJ_TOPO_Y + (NV_TELA_H - PJ_TOPO_Y) * p->y;
    float lar = 12.0f + (PJ_ABRE - 12.0f) * p->y;
    float xx = PJ_TOPO_X + (p->x - 0.5f) * 2.0f * lar;
    float tam = 1.2f + 3.2f * p->z;
    float a = (0.25f + 0.6f * p->z) * (0.6f + 0.4f * sinf(tempo * 2.0f + p->f)) * pisca *
              (p->y * 4.0f > 1.0f ? 1.0f : p->y * 4.0f);
    gfx_cor((GfxRect){ xx - tam * 0.5f, yy - tam * 0.5f, tam, tam }, 0.5f, 1.0f, 0.93f, 0.80f, a * alfa);
  }
  // Cada lamina e so o que cobre a tela depois de girada (900 x 1700 no
  // quadro dela): 6000 px de altura pintavam 11 telas por lamina.
  gfx_girar(th, PJ_TOPO_X, PJ_TOPO_Y);
  gfx_cor((GfxRect){ PJ_TOPO_X - 900.0f, PJ_TOPO_Y - 200.0f, 900.0f, 1700.0f }, 0.002f,
          FUNDO[0], FUNDO[1], FUNDO[2], 0.92f);
  gfx_girar(-th, PJ_TOPO_X, PJ_TOPO_Y);
  gfx_cor((GfxRect){ PJ_TOPO_X, PJ_TOPO_Y - 200.0f, 900.0f, 1700.0f }, 0.002f,
          FUNDO[0], FUNDO[1], FUNDO[2], 0.92f);
  gfx_sem_girar();
  // A tela do cinema iluminada, embaixo.
  { float r = 1300.0f;
    gfx_rect((GfxRect){ NV_TELA_W * 0.5f - r, 1340.0f - r, r * 2.0f, r * 2.0f }, 0, GFX_SOMBRA, 1.0f, 0, 0,
             0.5f, cor[0], cor[1], cor[2], 0.20f * pisca * alfa); }
  // A lente.
  gfx_rect((GfxRect){ PJ_TOPO_X - 90.0f, PJ_TOPO_Y - 90.0f, 180.0f, 180.0f }, 0, GFX_SOMBRA, 1.0f, 0, 0, 0.5f,
           1.0f, 0.94f, 0.84f, 0.85f * pisca * alfa);
  // Pelicula correndo nas duas bordas.
  { float desl = fmodf(tempo * 56.0f, PJ_FURO_PASSO), x;
    gfx_cor((GfxRect){ 0, 0, NV_TELA_W, PJ_FAIXA_H }, 0, 0.07f, 0.05f, 0.035f, alfa);
    gfx_cor((GfxRect){ 0, NV_TELA_H - PJ_FAIXA_H, NV_TELA_W, PJ_FAIXA_H }, 0, 0.07f, 0.05f, 0.035f, alfa);
    for (x = -PJ_FURO_PASSO + desl; x < NV_TELA_W + PJ_FURO_PASSO; x += PJ_FURO_PASSO) {
      gfx_cor((GfxRect){ x, 12.0f, 36.0f, 20.0f }, 0.3f, 0.235f, 0.17f, 0.118f, 0.9f * alfa);
      gfx_cor((GfxRect){ x, NV_TELA_H - 32.0f, 36.0f, 20.0f }, 0.3f, 0.235f, 0.17f, 0.118f, 0.9f * alfa);
    } }
  if (riscoV > 0.0f)
    gfx_cor((GfxRect){ riscoX, PJ_FAIXA_H, 2.0f, NV_TELA_H - PJ_FAIXA_H * 2.0f }, 0,
            1.0f, 0.94f, 0.86f, riscoV * alfa);
  veusBordas(0.0f, 0.55f, 0.75f, alfa);
  graoDesenhar(0.07f * alfa);
}

void psestilos_desenhar(const PSCena *c, int modo, float alfa) {
  if (modo == PS_FUNDO_FILMES) desenharFilmes(c, alfa);
  else if (modo == PS_FUNDO_LUZ) desenharLuz(c, alfa);
  else if (modo == PS_FUNDO_PROJETOR) desenharProjetor(c, alfa);
}
