// FUNDO ATRAS DOS PAINEIS. Ver fundo.h.
#include "fundo.h"
#include "ajustes.h"
#include "corviva.h"
#include "layout.h"
#include "tex_cache.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int fundo_modo(void) { return ajustes_fundo(); }

// veu = 1: a arte de tela cheia e o veu do mockup saem numa passada so
// (gfx_arte_veu) quando o caminho permite; devolve 1 se o veu ja foi aplicado.
static int arte(GfxRect r, float raioPx, const char *c, float a, int veu) {
  GLuint t = (c && c[0]) ? tex_obter_larg(c, r.w > r.h * 1.78f ? r.w : r.h * 1.78f) : 0;
  int feito = 0;
  if (!t) { gfx_cor(r, raioPx / r.h, NV_COR_FUNDO_R, NV_COR_FUNDO_G, NV_COR_FUNDO_B, a); return 0; }
  gfx_tex_aspect_atual = tex_aspecto(c);
  gfx_arte_opaca_atual = tex_opaca(c);   // tela cheia opaca: a luz por baixo nao e pintada
  // O veu: 40% no meio, 62% na borda esquerda (+0,367 do que falta) e 50% na direita (+0,167).
  if (veu && raioPx <= 0.0f && a >= 0.999f)
    feito = gfx_arte_veu(r, t, 0.40f, 0.367f, 0.167f, a);
  if (!feito) gfx_rect(r, t, GFX_ARTE, 0, 0, 0, raioPx / r.h, 1, 1, 1, a);
  gfx_arte_opaca_atual = 0;
  gfx_tex_aspect_atual = 0;
  return feito;
}
static int telaCheia(GfxRect r) {
  return r.x <= 0.5f && r.y <= 0.5f && r.w >= NV_TELA_W / gfx_escala() - 1 && r.h >= NV_TELA_H / gfx_escala() - 1;
}
// O FUNDO DE TELA CHEIA PELO CAMINHO DA LUZ IMERSIVA (gfx_luz_canal). O
// desenho de sempre vai para um quadro pequeno so quando a chave muda — pelo
// MESMO assado da luz imersiva, que a C9 mostra certo — e cada quadro paga UM
// quad opaco (GFX_SNAP, a passada de tela da luz). O veu de 28% da "Arte
// borrada" vai na mesma passada.
// O Frost assa o desenho direto inteiro.
//
// ARTE BORRADA = A IMAGEM DO TITULO DESFOCADA (dono, C9, 05/10/2026: "a Arte
// borrada continua sem borrar, so escurece o fundo"). Antes ela era so a LUZ
// da arte (as quatro cores de regiao da paleta, gfx_ambiente) e dependia da
// paleta do corviva: sem paleta para aquela url (a paleta so e anotada quando
// a arte e decodificada no teto do destaque, tex_cache.c) borrada() devolvia
// 0 e o fundo caia na arte NITIDA com o veu de 40-62% — escura, sem desfoque e
// sem nenhuma linha no log. Agora a fonte e a TEXTURA da arte: a copia de
// 96x54 de gfx_desfocado (duas passadas do GFX_BLUR, a mesma do "Desfocar nao
// assistidos" que a C9 ja desenha) e assada em "cover" no quadro de 320x180;
// o bilinear das duas ampliacoes (96 -> 320 -> 1920) e o resto do desfoque.
// Formas e cores da arte ficam; detalhe e texto somem. Nada por quadro alem do
// quad de sempre: a copia e o assado so refazem quando a arte muda.
//
// CONFERENCIA DE UMA VEZ (C9, 05/10/2026: o assado do 1bcd6ebe saiu escuro na
// TV e certo no Mac). Na primeira vez que cada fundo sai opaco, um pixel da
// TELA, lido logo depois do quad, e comparado com a conta feita aqui (Frost: a
// mesma conta no CPU; Borrada: o pixel do quadro pequeno com o veu, e o quadro
// pequeno contra a copia desfocada). Errou: log "[cor] fundo ... ERRADO" e o
// desenho direto pela sessao. Uma leitura de poucos px por fundo por sessao.
#define CANAL_BORRADA 0
#define CANAL_FROST   1
#define K_BORRADA 0
#define K_FROST   1
// O veu da Borrada: 28% atras dos paineis (Ajustes); a pagina do titulo pede
// mais (fundo_borrada_veu), porque o texto do heroi fica direto sobre ela.
#define VEU_BORRADA_PADRAO 0.28f
static float VEU_BORRADA[4] = { 0.024f, 0.027f, 0.035f, VEU_BORRADA_PADRAO };
void fundo_borrada_veu(float forca) {
  VEU_BORRADA[3] = forca < 0.0f ? VEU_BORRADA_PADRAO : forca > 1.0f ? 1.0f : forca;
}
static const char *const NOME_K[2] = { "borrada", "frost" };
static int conferido[2] = { -1, -1 };   // -1 a conferir, 1 certo, 0 errado (desenho direto)
static int conferidoAssado[2] = { -1, -1 };
int fundo_conferencia(int modo) {
  return modo == FUNDO_FROST ? conferido[K_FROST] : modo == FUNDO_BORRADA ? conferido[K_BORRADA] : -1;
}

// O Frost direto em (u, v) de uma tela cheia: base, degrade de cima e as tres
// luzes (GFX_LUZ: queda suave ao quadrado).
static void refFrost(const float *ac, float u, float v, int luz, float o[3]) {
  static const float L[3][3] = { { 0.14f, 0.08f, 1.05f }, { 0.92f, 0.96f, 0.90f }, { 0.70f, 0.30f, 0.60f } };
  static const float B[3] = { 0.043f, 0.047f, 0.055f }, T[3] = { 0.082f, 0.086f, 0.102f };
  const float A = 1920.0f / 1080.0f;
  float g = v / 0.70f;
  int i, j;
  g = 1.0f - (g < 0.0f ? 0.0f : g > 1.0f ? 1.0f : g);
  for (j = 0; j < 3; j++) o[j] = B[j] * (1.0f - g) + T[j] * g;
  if (!luz) return;
  for (i = 0; i < 3; i++) {
    float px = (u - L[i][0]) * A, py = v - L[i][1], t = 1.0f - sqrtf(px * px + py * py) / L[i][2], sv, al;
    t = t < 0.0f ? 0.0f : t > 1.0f ? 1.0f : t;
    sv = t * t * (3.0f - 2.0f * t);
    al = sv * sv * ac[9 + i];
    for (j = 0; j < 3; j++) o[j] = o[j] * (1.0f - al) + ac[i * 3 + j] * al;
  }
}
static int nivel(float c) { return (int)(c * 255.0f + 0.5f); }
// Ponto onde a luz do Frost mais pesa entre os candidatos. Devolve o desvio
// (niveis) entre "com luz" e "sem luz" e preenche u, v e as duas referencias.
static int pontoDeLuz(const float *ac, float *u, float *v, int ref[3], int base[3]) {
  static const float CF[3][2] = { { 0.14f, 0.08f }, { 0.92f, 0.94f }, { 0.5f, 0.5f } };
  int i, j, melhor = -1;
  for (i = 0; i < 3; i++) {
    float c[3], b[3];
    int dev = 0;
    refFrost(ac, CF[i][0], CF[i][1], 1, c); refFrost(ac, CF[i][0], CF[i][1], 0, b);
    for (j = 0; j < 3; j++) { int d = abs(nivel(c[j]) - nivel(b[j])); if (d > dev) dev = d; }
    if (dev > melhor) {
      melhor = dev; *u = CF[i][0]; *v = CF[i][1];
      for (j = 0; j < 3; j++) { ref[j] = nivel(c[j]); base[j] = nivel(b[j]); }
    }
  }
  return melhor;
}
// DESPEJO DE UMA VEZ (dono, 05/10/2026: ver na TV o que foi mesmo desenhado).
// Na PRIMEIRA vez por execucao que cada fundo de tela cheia sai opaco, grava
// /tmp/nuvio-fundo-<frost|borrada>.bmp (a tela logo depois do fundo, em meia
// resolucao) e /tmp/nuvio-fundo-<...>-assado.bmp (o quadro pequeno de 320x180,
// quando ha um), e loga o pixel conferido. Sem arquivo de pedido: um pedido que
// o app nao conseguia apagar derrubou a C9 para 9 fps. A marca e posta ANTES de
// gravar, entao uma gravacao que falha tambem nao repete; sem /tmp gravavel
// (Android) nada e lido. NUVIO_DUMP_FUNDO_DIR troca o /tmp (testes no Mac).
static const char *dumpDir(void) {
  const char *d = getenv("NUVIO_DUMP_FUNDO_DIR");
  return d && d[0] ? d : "/tmp";
}
static int despejado[2];
static int despejoDevido(int k, float a) {
  if (despejado[k] || a * gfx_opacidade_grupo < 0.999f) return 0;
  if (access(dumpDir(), W_OK) != 0) { despejado[k] = 1; return 0; }
  return 1;
}
static void despejar(int k, GLuint t) {
  char cam[640];
  snprintf(cam, sizeof cam, "%s/nuvio-fundo-%s.bmp", dumpDir(), NOME_K[k]);
  printf("[fundo-dump] %s: %s", cam, gfx_tela_bmp(cam) ? "gravado" : "FALHOU");
  if (t) {
    snprintf(cam, sizeof cam, "%s/nuvio-fundo-%s-assado.bmp", dumpDir(), NOME_K[k]);
    printf(", %s: %s", cam, gfx_luz_canal_bmp(t, cam) ? "gravado" : "FALHOU");
  }
  printf("\n");
  fflush(stdout);
}
static int podeConferir(int k, GLuint t, float a) {
  return t && conferido[k] < 0 && conferidoAssado[k] != gfx_n_fundo_assados &&
         a * gfx_opacidade_grupo >= 0.999f && !gfx_efeitos_leves() && !gfx_modos_desligados;
}
// Depois do quad do Frost (t = o quadro pequeno, 0 no desenho direto): a
// conferencia de uma vez e o despejo de uma vez.
static void depoisDoFrost(GLuint t, const float *ac, float a) {
  const int k = K_FROST;
  int despejar_ = despejoDevido(k, a), conferir = podeConferir(k, t, a), dev, ref[3], base[3], j, erro = 0, tol;
  unsigned char tela[3] = { 0, 0, 0 }, pq[3] = { 0, 0, 0 };
  float u = 0.5f, v = 0.5f;
  if (!conferir && !despejar_) return;
  dev = pontoDeLuz(ac, &u, &v, ref, base);
  tol = 5 + dev / 5;
  if (!gfx_tela_px(u, v, tela)) return;   // dentro de um snapshot: nada foi lido, fica para o proximo
  if (despejar_) despejado[k] = 1;
  if (t) gfx_luz_canal_px(t, u, v, pq);
  for (j = 0; j < 3; j++) { int d = abs((int)tela[j] - ref[j]); if (d > erro) erro = d; }
  if (conferir) {
    if (dev < 12) conferidoAssado[k] = gfx_n_fundo_assados;   // luz fraca demais: confere no proximo assado
    else conferido[k] = erro <= tol;
  }
  if ((conferir && dev >= 12) || despejar_) {
    printf("[cor] fundo frost %s: em %.2f,%.2f esperado %d,%d,%d (sem luz %d,%d,%d); tela %d,%d,%d; "
           "quadro pequeno %d,%d,%d%s\n",
           !t ? "direto" : conferido[k] == 1 ? "conferido" : conferido[k] == 0 ? "CONFERIDO ERRADO (desenho direto)" : "lido",
           u, v, ref[0], ref[1], ref[2], base[0], base[1], base[2], tela[0], tela[1], tela[2], pq[0], pq[1], pq[2],
           t ? "" : " (sem quadro pequeno)");
    fflush(stdout);
  }
  if (despejar_) despejar(k, t);
}

// --- ARTE BORRADA ------------------------------------------------------------
typedef struct {
  GLuint d;     // a copia desfocada de 96x54 (a arte INTEIRA, esticada)
  float asp;    // w/h da arte (0 = desconhecido: estica)
} Borrada;
// O retangulo em "cover" da arte `asp` na tela cheia (pode passar das bordas).
static GfxRect coverTela(float asp) {
  float W = NV_TELA_W, H = NV_TELA_H, w = W, h = H;
  if (asp > 0.01f) { if (asp > W / H) w = H * asp; else h = W / asp; }
  return (GfxRect){ (W - w) * 0.5f, (H - h) * 0.5f, w, h };
}
static void pintarBorradaArte(const Borrada *b, float a) {
  float aspAnt = gfx_tex_aspect_atual;
  int opAnt = gfx_arte_opaca_atual;
  gfx_tex_aspect_atual = 0.0f; gfx_arte_opaca_atual = 0;
  gfx_rect(coverTela(b->asp), b->d, GFX_ARTE, 0, 0, 0, 0.0f, 1, 1, 1, a);
  gfx_tex_aspect_atual = aspAnt; gfx_arte_opaca_atual = opAnt;
}
// O ASSADO: a copia de 96x54 esticada ate a tela mostra as emendas do bilinear
// (cada texel vira uma rampa de 20 px com quina nas duas pontas, e a arte de
// contraste alto saia em faixas — dono, 05/10: "degrade duro"). Aqui ela e
// pintada 25 vezes numa grade de +-32 px, cada uma com alfa 1/n: a media de
// todas, um filtro de caixa de ~3 texels que desfaz as quinas. So no assado,
// que refaz quando a arte muda; o desenho direto continua com uma passada.
static void pintarBorrada(void *ctx) {
  const Borrada *b = ctx;
  float aspAnt = gfx_tex_aspect_atual;
  int opAnt = gfx_arte_opaca_atual, i, n = 0;
  GfxRect cv = coverTela(b->asp);
  gfx_tex_aspect_atual = 0.0f; gfx_arte_opaca_atual = 0;
  for (i = 0; i < 25; i++) {
    GfxRect r = cv;
    r.x += (float)(i % 5 - 2) * 16.0f; r.y += (float)(i / 5 - 2) * 16.0f;
    // a borda da copia deslocada nao pode deixar o clear aparecer
    r.x -= 40.0f; r.y -= 40.0f; r.w += 80.0f; r.h += 80.0f;
    gfx_rect(r, b->d, GFX_ARTE, 0, 0, 0, 0.0f, 1, 1, 1, 1.0f / (float)++n);
  }
  gfx_tex_aspect_atual = aspAnt; gfx_arte_opaca_atual = opAnt;
}
// Avisos de uma vez por sessao: porque a Borrada nao saiu pelo caminho normal.
static void avisoBorrada(int *visto, const char *msg) {
  if (*visto) return;
  *visto = 1;
  printf("[cor] fundo borrada: %s\n", msg);
  fflush(stdout);
}
// Depois do quad da Borrada (t = o quadro pequeno, 0 no desenho direto).
// Confere em tres pontos: tela == quadro pequeno com o veu (a passada de tela)
// e quadro pequeno == copia desfocada no mesmo lugar da arte (o assado). O
// desvio do quadro pequeno para o fundo liso vai ao log: um assado que so tem
// o clear (o "quase preto" do 1bcd6ebe) aparece ali e no despejo.
static void depoisDaBorrada(GLuint t, const Borrada *b, float a) {
  static const float P[3][2] = { { 0.25f, 0.30f }, { 0.50f, 0.50f }, { 0.75f, 0.70f } };
  const int k = K_BORRADA;
  int despejar_ = despejoDevido(k, a), conferir = podeConferir(k, t, a), i, j;
  int erroTela = 0, erroAssado = 0, desvio = 0, iPior = 1;
  unsigned char tela[3][3], pq[3][3], cd[3][3];
  float ref[3][3];
  GfxRect cv = coverTela(b->asp);
  if (!conferir && !despejar_) return;
  memset(pq, 0, sizeof pq); memset(cd, 0, sizeof cd);
  for (i = 0; i < 3; i++) if (!gfx_tela_px(P[i][0], P[i][1], tela[i])) return;   // snapshot: fica para o proximo
  if (despejar_) despejado[k] = 1;
  for (i = 0; i < 3; i++) {
    float au = (P[i][0] * NV_TELA_W - cv.x) / cv.w, av = (P[i][1] * NV_TELA_H - cv.y) / cv.h;
    int temPq = t && gfx_luz_canal_px(t, P[i][0], P[i][1], pq[i]);
    int temCd = gfx_desfocado_px(b->d, au, av, cd[i]);
    const float F[3] = { NV_COR_FUNDO_R, NV_COR_FUNDO_G, NV_COR_FUNDO_B };
    for (j = 0; j < 3; j++) {
      float fonte = temPq ? pq[i][j] / 255.0f : temCd ? cd[i][j] / 255.0f : F[j];
      int dt, da, dv;
      ref[i][j] = (fonte * (1.0f - VEU_BORRADA[3]) + VEU_BORRADA[j] * VEU_BORRADA[3]) * 255.0f;
      dt = abs((int)tela[i][j] - (int)(ref[i][j] + 0.5f));
      da = temPq && temCd ? abs((int)pq[i][j] - (int)cd[i][j]) : 0;
      dv = abs((int)(fonte * 255.0f + 0.5f) - nivel(F[j]));
      if (dt > erroTela) erroTela = dt;
      if (da > erroAssado) { erroAssado = da; iPior = i; }
      if (dv > desvio) desvio = dv;
    }
  }
  // A copia (96x54) e o assado (320x180) sao lidos no texel mais perto: perto
  // de uma borda da arte desfocada eles diferem alguns niveis pelo bilinear.
  if (conferir) conferido[k] = erroTela <= 6 && erroAssado <= 72;   // 72: o assado e a MEDIA de 25 copias deslocadas (pintarBorrada), nao a copia
  printf("[cor] fundo borrada %s: tela %d,%d,%d esperado %.0f,%.0f,%.0f (erro %d); quadro pequeno %d,%d,%d, "
         "copia desfocada %d,%d,%d (erro %d); desvio do fundo liso %d%s\n",
         !t ? "direto" : conferido[k] == 1 ? "conferido" : conferido[k] == 0 ? "CONFERIDO ERRADO (desenho direto)" : "lido",
         tela[iPior][0], tela[iPior][1], tela[iPior][2], ref[iPior][0], ref[iPior][1], ref[iPior][2], erroTela,
         pq[iPior][0], pq[iPior][1], pq[iPior][2], cd[iPior][0], cd[iPior][1], cd[iPior][2], erroAssado, desvio,
         t ? "" : " (sem quadro pequeno)");
  fflush(stdout);
  if (despejar_) despejar(k, t);
}
// A copia desfocada da arte `c`, ou 0 (sem textura ainda, ou o limite de copias
// por quadro). Pede a arte na largura da tela, como a arte nitida.
static GLuint borradaCopia(GfxRect r, const char *c, float *asp) {
  GLuint src = tex_obter_larg(c, r.w > r.h * 1.78f ? r.w : r.h * 1.78f);
  *asp = src ? tex_aspecto(c) : 0.0f;
  return src ? gfx_desfocado(src, c) : 0;
}
static unsigned long hashUrl(const char *s) {
  unsigned long h = 2166136261u;
  while (*s) { h ^= (unsigned char)*s++; h *= 16777619u; }
  return h & 0xffffffffu;
}
static int borrada(GfxRect r, float raioPx, const char *c, float a) {
  static int avisoSemCopia, avisoDireto;
  static GLuint ultimo;          // o ultimo quadro pequeno desenhado, e de quem
  static unsigned long ultimoH;
  Borrada b;
  unsigned long h;
  if (!c || !c[0]) return 0;
  h = hashUrl(c);
  b.d = borradaCopia(r, c, &b.asp);
  if (telaCheia(r) && raioPx <= 0.0f) {
    GLuint t = 0;
    if (!b.d) {
      // A arte ainda nao chegou, ou a copia fica para o proximo quadro: o
      // quadro pequeno desta mesma arte, se ha; senao o fundo liso com o veu.
      // PONTILHADO como o assado de sempre (abaixo): sem o ruido o degrade
      // lento da arte desfocada sai em faixas de 8 bits no OLED (dono, 11/10,
      // fotos1014/gradiente-banding.jpg).
      if (ultimo && ultimoH == h && glIsTexture(ultimo)) {
        gfx_luz_canal_desenhar(ultimo, a, VEU_BORRADA, 1);
        return 1;
      }
      avisoBorrada(&avisoSemCopia, "sem a copia desfocada da arte ainda (arte nao decodificada?): fundo liso");
      gfx_cor(r, 0.0f, NV_COR_FUNDO_R, NV_COR_FUNDO_G, NV_COR_FUNDO_B, a);
      gfx_cor(r, 0.0f, VEU_BORRADA[0], VEU_BORRADA[1], VEU_BORRADA[2], VEU_BORRADA[3] * a);
      return 1;
    }
    if (conferido[K_BORRADA] != 0) {
      float k[9];
      k[0] = (float)(b.d & 0xffffu); k[1] = (float)(b.d >> 16);
      k[2] = (float)(h & 0xffffu);   k[3] = (float)(h >> 16);
      k[4] = b.asp;
      k[5] = NV_COR_FUNDO_R; k[6] = NV_COR_FUNDO_G; k[7] = NV_COR_FUNDO_B;
      k[8] = 1.0f;   // versao do desenho: 1 = a copia desfocada
      t = gfx_luz_canal(CANAL_BORRADA, k, 9, pintarBorrada, &b);
    }
    if (t) {
      // PONTILHADO (uPar.x = 1 no GFX_SNAP, o nv_dither de gfx.c): o quadro
      // pequeno de 320x180 esticado na tela e 8 bits, e o degrade escuro da
      // arte desfocada quantizava em faixas largas no OLED — o mesmo caminho
      // do Frost (pontilhar 1) e do Cinema (dfae93b2). Ruido +-1/255, parado,
      // ~6 ALU: barato no GLES2 de webOS/Tizen/Android.
      gfx_luz_canal_desenhar(t, a, VEU_BORRADA, 1);
      ultimo = t; ultimoH = h;
    } else {
      // DESENHO DIRETO: a copia em cover na tela e o veu por cima (uma tela
      // misturada a mais). Sem quadro pequeno, ou a conferencia errou.
      avisoBorrada(&avisoDireto, conferido[K_BORRADA] == 0 ? "desenho direto (conferencia errou)"
                                                            : "desenho direto (sem quadro pequeno)");
      pintarBorradaArte(&b, a);
      gfx_cor(r, 0.0f, VEU_BORRADA[0], VEU_BORRADA[1], VEU_BORRADA[2], VEU_BORRADA[3] * a);
    }
    depoisDaBorrada(t, &b, a);
    return 1;
  }
  // PREVIAS (cartoes de Ajustes > Fundo, cantos arredondados): a copia
  // esticada no cartao (as artes do catalogo sao 16:9, como o cartao) e o veu.
  if (b.d) {
    float aspAnt = gfx_tex_aspect_atual;
    gfx_tex_aspect_atual = 0.0f;
    gfx_rect(r, b.d, GFX_ARTE, 0, 0, 0, raioPx / r.h, 1, 1, 1, a);
    gfx_tex_aspect_atual = aspAnt;
  } else gfx_cor(r, raioPx / r.h, NV_COR_FUNDO_R, NV_COR_FUNDO_G, NV_COR_FUNDO_B, a);
  gfx_cor(r, raioPx / r.h, VEU_BORRADA[0], VEU_BORRADA[1], VEU_BORRADA[2], VEU_BORRADA[3] * a);
  return 1;
}
// FROST (acentos-mockup.html, quadro 6): a luz NAO e o acento cru, e o mesmo
// matiz com L 0,42 e croma <= 0,11 (ajustes_acento_luz) — Branco vira nevoa
// neutra e Jade nao acende a tela. Fundo linear(165deg, #15161A, #0B0C0E 70%)
// e as tres luzes a 70%, 45% e 18%.
// AS TRES LUZES: cor (9) e forca (3). Fora da Imersiva, a luz do destaque nas
// tres, a 70/45/18%. NA IMERSIVA (dono, 05/10: "o fundo frost no imersivo ta
// sempre preto, nao ta com a cor imersiva") cada luz e uma REGIAO da cena — as
// mesmas cores que a Home pinta — e mais forte, porque aqui elas sao so tres
// manchas e nao a tela inteira.
static void frostLuzes(float c[12]) {
  static const float F[3] = { 0.70f, 0.45f, 0.18f }, FI[3] = { 0.90f, 0.70f, 0.45f };
  float ac[3], k = nv_ambiente_forca < 0.0f ? 0.0f : nv_ambiente_forca > 1.0f ? 1.0f : nv_ambiente_forca;
  int i, j;
  ajustes_acento_luz(&ac[0], &ac[1], &ac[2]);
  for (i = 0; i < 3; i++) {
    for (j = 0; j < 3; j++) c[i * 3 + j] = ac[j] + (nv_ambiente_viva[i][j] - ac[j]) * k;
    c[9 + i] = F[i] + (FI[i] - F[i]) * k;
  }
}
static void frost(GfxRect r, float raioPx, float a) {
  float c[12], rr = raioPx / r.h;
  frostLuzes(c);
  gfx_cor(r, rr, 0.043f, 0.047f, 0.055f, a);   // #0B0C0E
  gfx_rect(r, 0, GFX_VEU_CSS, 1.0f, 1.0f, 0.70f, rr, 0.082f, 0.086f, 0.102f, a);   // #15161A em cima
  gfx_luz_canto(r, rr, r.w * 0.14f, r.h * 0.08f, r.h * 1.05f, c[0], c[1], c[2], c[9] * a);
  gfx_luz_canto(r, rr, r.w * 0.92f, r.h * 0.96f, r.h * 0.90f, c[3], c[4], c[5], c[10] * a);
  gfx_luz_canto(r, rr, r.w * 0.70f, r.h * 0.30f, r.h * 0.60f, c[6], c[7], c[8], c[11] * a);
}
static void pintarFrost(void *ctx) {
  (void)ctx;
  GfxRect tela = { 0, 0, NV_TELA_W, NV_TELA_H };
  float k = fminf(fmaxf(nv_ambiente_forca, 0.0f), 1.0f);
  frost(tela, 0.0f, 1.0f);
  // Blend toward the immersive base inside the small bake. Switching at 0.5
  // replaced the entire background in one frame while corviva was easing.
  if (k > 0.0f) {
    gfx_cor(tela, 0, NV_COR_FUNDO_R, NV_COR_FUNDO_G, NV_COR_FUNDO_B, k);
    gfx_rect(tela, 0, GFX_AMBIENTE, 0, 0, 0, 0, 1, 1, 1, k * k);
  }
}
static void frostTela(GfxRect r, float raioPx, float a) {
  float k[20];
  GLuint t = 0;
  if (!telaCheia(r) || raioPx > 0.0f) { frost(r, raioPx, a); return; }
  // NA IMERSIVA O FROST E A LUZ DE AMBIENTE (dono, 05/10: "queria que o fundo
  // frost seguisse a mesma dinamica do fundo do layout Apple TV no filme"). E
  // o que a pagina do titulo ja pinta sem Frost: o chao do app e a luz das
  // regioes da cena por cima (gfx_ambiente, o assado da Home). As tres manchas
  // do Frost ficavam quase pretas ao lado disso.
  if (nv_ambiente_forca >= 1.0f) {
    gfx_cor(r, 0.0f, NV_COR_FUNDO_R, NV_COR_FUNDO_G, NV_COR_FUNDO_B, a);
    // Opaco (alfa 1): a luz vai pelo caminho SEM mistura do gfx_ambiente, o
    // mesmo do quadro da Home. A 0,998 era uma tela cheia misturada a mais por
    // quadro, e a TCL caia para 34 fps na pagina do titulo (log de 05/10).
    gfx_ambiente(a >= 0.999f ? 1.0f : a * 0.998f);
    return;
  }
  frostLuzes(k);
  k[12] = NV_COR_FUNDO_R; k[13] = NV_COR_FUNDO_G; k[14] = NV_COR_FUNDO_B;
  k[15] = nv_ambiente_forca;
  k[16] = nv_ambiente_forca > 0.0f ? floorf(nv_tempo_viva * 12.0f) : 0.0f;
  for (int i = 0; i < 3; i++) k[17 + i] = nv_ambiente_forca > 0.0f ? nv_ambiente_viva[3][i] : 0.0f;
  if (conferido[K_FROST] != 0) t = gfx_luz_canal(CANAL_FROST, k, 20, pintarFrost, NULL);
  // ADIADO (gfx_luz_canal_adiar): a arte de tela cheia que vier por cima (o
  // topo da pagina do titulo) le o Frost pelo uAmb e sai opaca, ou o veu de
  // rolagem vai na mesma passada dele. Nao adia quando a conferencia ou o
  // despejo de uma vez vao ler a tela logo depois do quad.
  if (t && a >= 0.999f && !podeConferir(K_FROST, t, a) && !despejoDevido(K_FROST, a) &&
      gfx_luz_canal_adiar(t, 1))
    return;
  if (t) gfx_luz_canal_desenhar(t, a, NULL, 1);
  else {
    frost(r, raioPx, a);
    if (nv_ambiente_forca > 0.0f) {
      gfx_cor(r, 0, NV_COR_FUNDO_R, NV_COR_FUNDO_G, NV_COR_FUNDO_B, nv_ambiente_forca * a);
      gfx_rect(r, 0, GFX_AMBIENTE, 0, 0, 0, 0, 1, 1, 1, nv_ambiente_forca * nv_ambiente_forca * a);
    }
  }
  if (nv_ambiente_forca <= 0.0f) depoisDoFrost(t, k, a);
}
void fundo_desenhar_modo(int modo, GfxRect r, float raioPx, const char *c, float a) {
  if (r.w < 1 || r.h < 1 || a <= 0.003f) return;
  if (modo == FUNDO_FROST) {
    frostTela(r, raioPx, a);
    return;
  }
  if (modo == FUNDO_BORRADA && borrada(r, raioPx, c, a)) return;
  if (arte(r, raioPx, c, a, raioPx <= 0.0f)) return;
  if (raioPx > 0.0f) { gfx_cor(r, raioPx / r.h, 0, 0, 0, 0.42f * a); return; }
  // O veu do mockup: linear-gradient(90deg, 62%, 40% no meio, 50%). Um
  // chapado de 40% e, por cima, as duas metades em degrade (GFX_VEU_CSS,
  // nv_dither) com o que falta para chegar a 62% e 50% nas bordas.
  // O chapado vai DENTRO das duas metades (gfx_veu_css_base): uma camada de
  // tela cheia misturada a menos por quadro, o mesmo pixel.
  gfx_veu_css_base((GfxRect){ r.x, r.y, r.w * 0.5f, r.h }, 2, 1.0f, 1.0f, 0.367f * a, 0.40f * a);
  gfx_veu_css_base((GfxRect){ r.x + r.w * 0.5f, r.y, r.w * 0.5f, r.h }, 3, 1.0f, 1.0f, 0.167f * a, 0.40f * a);
}
void fundo_desenhar(GfxRect area, const char *arteUrl, float a) {
  fundo_desenhar_modo(fundo_modo(), area, 0.0f, arteUrl, a);
}
