// Cor viva: o destaque (e o fundo, o degrade e a luz ambiente) seguindo a arte
// do titulo em cena. O porque de cada peca esta em corviva.h; aqui ficam as
// medidas.
#include "corviva.h"
#include "dados.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// --------------------------------------------------------------- cor (OKLab)
//
// OKLab e nao RGB para duas contas: a INTERPOLACAO (azul -> laranja em RGB
// passa por um cinza lamacento no meio; em OKLab a luminosidade fica de pe) e
// o LIMITE de luminosidade/croma do destaque, que em OKLab e uma coordenada so
// em vez de uma conta por canal.
static float linDeSrgb(float c) {
  return c <= 0.04045f ? c / 12.92f : powf((c + 0.055f) / 1.055f, 2.4f);
}
static float srgbDeLin(float c) {
  if (c <= 0.0f) return 0.0f;
  if (c >= 1.0f) return 1.0f;
  return c <= 0.0031308f ? c * 12.92f : 1.055f * powf(c, 1.0f / 2.4f) - 0.055f;
}
static void labDeLin(const float l3[3], float lab[3]) {
  float l = 0.4122214708f * l3[0] + 0.5363325363f * l3[1] + 0.0514459929f * l3[2];
  float m = 0.2119034982f * l3[0] + 0.6806995451f * l3[1] + 0.1073969566f * l3[2];
  float s = 0.0883024619f * l3[0] + 0.2817188376f * l3[1] + 0.6299787005f * l3[2];
  l = cbrtf(l); m = cbrtf(m); s = cbrtf(s);
  lab[0] = 0.2104542553f * l + 0.7936177850f * m - 0.0040720468f * s;
  lab[1] = 1.9779984951f * l - 2.4285922050f * m + 0.4505937099f * s;
  lab[2] = 0.0259040371f * l + 0.7827717662f * m - 0.8086757660f * s;
}
static void linDeLab(const float lab[3], float l3[3]) {
  float l = lab[0] + 0.3963377774f * lab[1] + 0.2158037573f * lab[2];
  float m = lab[0] - 0.1055613458f * lab[1] - 0.0638541728f * lab[2];
  float s = lab[0] - 0.0894841775f * lab[1] - 1.2914855480f * lab[2];
  l = l * l * l; m = m * m * m; s = s * s * s;
  l3[0] =  4.0767416621f * l - 3.3077115913f * m + 0.2309699292f * s;
  l3[1] = -1.2684380046f * l + 2.6097574011f * m - 0.3413193965f * s;
  l3[2] = -0.0041960863f * l - 0.7034186147f * m + 1.7076147010f * s;
}
void corviva_srgb_para_oklab(const float rgb[3], float lab[3]) {
  float l3[3] = { linDeSrgb(rgb[0]), linDeSrgb(rgb[1]), linDeSrgb(rgb[2]) };
  labDeLin(l3, lab);
}
void corviva_oklab_para_srgb(const float lab[3], float rgb[3]) {
  float l3[3];
  linDeLab(lab, l3);
  rgb[0] = srgbDeLin(l3[0]); rgb[1] = srgbDeLin(l3[1]); rgb[2] = srgbDeLin(l3[2]);
}
static float luminanciaRel(const float rgb[3]) {
  return 0.2126f * linDeSrgb(rgb[0]) + 0.7152f * linDeSrgb(rgb[1]) +
         0.0722f * linDeSrgb(rgb[2]);
}
float corviva_contraste(const float a[3], const float b[3]) {
  float ya = luminanciaRel(a) + 0.05f, yb = luminanciaRel(b) + 0.05f;
  return ya > yb ? ya / yb : yb / ya;
}

// L, C, h -> sRGB, baixando o croma ate caber no gamute. Um OKLCh de croma alto
// num matiz estreito (amarelo escuro, ciano escuro) cai FORA do sRGB, e o
// grampo canal a canal torceria o matiz — baixar o croma preserva o matiz e a
// luminosidade, que sao as duas coisas que o olho confere.
static void lchParaSrgb(float L, float C, float h, float rgb[3]) {
  int k;
  for (k = 0; k < 60; k++) {
    float lab[3] = { L, C * cosf(h), C * sinf(h) }, l3[3];
    linDeLab(lab, l3);
    if ((l3[0] >= -0.001f && l3[0] <= 1.001f && l3[1] >= -0.001f &&
         l3[1] <= 1.001f && l3[2] >= -0.001f && l3[2] <= 1.001f) || C <= 0.0f) {
      rgb[0] = srgbDeLin(l3[0]); rgb[1] = srgbDeLin(l3[1]); rgb[2] = srgbDeLin(l3[2]);
      return;
    }
    // x0,96 por passo, como C.lch do acentos-mockup.html: as cores dos
    // acentos foram medidas la, e a mesma reducao da o mesmo hex aqui.
    C *= 0.96f;
  }
  { float lab[3] = { L, 0, 0 }; corviva_oklab_para_srgb(lab, rgb); }
}
static void lchDe(const float rgb[3], float *L, float *C, float *h) {
  float lab[3];
  corviva_srgb_para_oklab(rgb, lab);
  *L = lab[0]; *C = sqrtf(lab[1] * lab[1] + lab[2] * lab[2]); *h = atan2f(lab[2], lab[1]);
}

// AS TRAVAS DOS ACENTOS (acentos-mockup.html, aprovado pelo dono em 03/10).
// A regra de 21/09 ("texto branco em toda cor") mandava o destaque segurar
// texto branco a 3,2:1 e por isso empurrava todo amarelo para oliva. Agora ha
// duas familias, como nos acentos fixos: CLAROS com tinta escura #121316 e
// PROFUNDOS com tinta branca, ambos a 4,5:1 ou mais na pilula.
static const float BRANCO_A[3] = { 1.0f, 1.0f, 1.0f };
static const float TINTA_ESC[3] = { 0x12 / 255.0f, 0x13 / 255.0f, 0x16 / 255.0f };   // #121316
static const float ILHA_A[3]   = { 0x12 / 255.0f, 0x13 / 255.0f, 0x16 / 255.0f };    // a ilha
static const float CINZA_A[3]  = { 0xa3 / 255.0f, 0xa1 / 255.0f, 0x9c / 255.0f };    // "HDR" de grupo
static const float BRANCO_ACENTO[3] = { 0xf4 / 255.0f, 0xf2 / 255.0f, 0xee / 255.0f }; // o acento Branco
#define CV_GRAU 0.017453293f

float corviva_contraste_y(float ya, float yb) {
  ya += 0.05f; yb += 0.05f;
  return ya > yb ? ya / yb : yb / ya;
}
// PROFUNDO: o MAIOR L em que o branco ainda le a >= 4,6:1 (folga sobre 4,5),
// descendo de 0,78 em passos de 0,004 — C.profundo do mockup.
static void profundo(float h, float c, float out[3]) {
  float L = 0.78f;
  lchParaSrgb(L, c, h, out);
  while (corviva_contraste(out, BRANCO_A) < 4.6f && L > 0.3f) { L -= 0.004f; lchParaSrgb(L, c, h, out); }
}
// AMPLITUDE (dono, 14/10/2026: "o tom dos botoes nao casa com as cores da
// logo, temos que deixar mais amplo as cores"): o croma saida de 0,17 no
// teto — a logo vermelha de Stranger Things (C ~0,25) virava um tijolo
// abafado. Agora o croma do bruto e mantido (x1,15, teto 0,24): o MATIZ e a
// SATURACAO da cor dominante ficam, e a garantia de contraste nao sai do
// lugar — o L continua sendo o da busca (profundo/claro) e nao o do bruto.
#define CV_C_MIN 0.08f
#define CV_C_MAX 0.24f
// CLARO: amarelos (matiz 70-115) com tinta escura. L sobe (nao desce) ate a
// tinta escura #121316 ler a >= 4,5:1 — a mesma folga do profundo, para o
// croma novo (ate 0,16) nao derrubar a regra.
static void claro(float h, float c, float out[3]) {
  float L = 0.86f;
  lchParaSrgb(L, c, h, out);
  while (corviva_contraste(out, TINTA_ESC) < 4.5f && L < 0.97f) { L += 0.004f; lchParaSrgb(L, c, h, out); }
}
static float grausDe(float h) { float g = h / CV_GRAU; return g < 0.0f ? g + 360.0f : g; }
void corviva_da_arte(const float bruto[3], float out[3]) {
  float L, C, h, c, g;
  lchDe(bruto, &L, &C, &h);
  (void)L;
  if (C < 0.04f) { memcpy(out, BRANCO_ACENTO, sizeof BRANCO_ACENTO); return; }
  c = C * 1.15f;
  if (c < CV_C_MIN) c = CV_C_MIN;
  if (c > CV_C_MAX) c = CV_C_MAX;
  g = grausDe(h);
  if (g >= 70.0f && g <= 115.0f) claro(h, c > 0.16f ? 0.16f : c, out);
  else profundo(h, c, out);
}
void corviva_tokens(const float fill[3], CorvivaTokens *t) {
  float L, C, h;
  int k, claro;
  lchDe(fill, &L, &C, &h);
  claro = corviva_contraste(fill, TINTA_ESC) > corviva_contraste(fill, BRANCO_A);
  t->tintaBranca = !claro;
  if (claro && corviva_contraste(fill, ILHA_A) >= 8.0f) memcpy(t->marca, fill, sizeof t->marca);
  else lchParaSrgb(0.84f, C * 0.85f < 0.14f ? C * 0.85f : 0.14f, h, t->marca);
  for (k = 0; k < 3; k++) t->hdr[k] = 0.5f * (t->marca[k] + CINZA_A[k]);
  lchParaSrgb(0.42f, C < 0.11f ? C : 0.11f, h, t->luz);
}

// A BASE do estilizado: o #0D0D0D (L ~0,16) com um sopro do matiz da arte.
// Croma no maximo 0,032 e L 0,175: e o bastante para a tela inteira ler
// "azul-noite" ou "vinho" ao lado de outra, e pouco para mudar o contraste dos
// cinzas que foram calibrados um a um contra #0D0D0D (ver TEMA_ACENTO em
// ajustes.c) — o cinza #8A8A8A cai de 5,5:1 para 5,1:1, ainda folgado.
#define CV_BASE_L     0.175f
#define CV_BASE_C_MAX 0.032f
static void ajustarBase(const float bruto[3], float saida[3]) {
  float L, C, h;
  lchDe(bruto, &L, &C, &h);
  (void)L;
  C *= 0.35f;
  if (C > CV_BASE_C_MAX) C = CV_BASE_C_MAX;
  lchParaSrgb(CV_BASE_L, C, h, saida);
}

// A LUZ DE UMA REGIAO (imersiva): a media da regiao, com o croma realcado e a
// luminosidade numa faixa de "luz de ambiente" — escura o bastante para o
// texto branco por cima continuar lendo, clara o bastante para se ver que e
// luz. O shader a pinta com alfa <= 0,72 (gfx.c, GFX_AMBIENTE).
//
// MAIS COR (dono, 28/09/2026: "hoje nao muda tanto, quero a cor fazendo mais"):
// era croma x1,6 com teto 0,16 e alfa <= 0,5, e a media de um terco da arte
// ja sai mais cinza que a arte — toda luz virava um tom sujo parecido com a
// outra. Agora a media pesa mais quem tem cor (MEDIA_REGIAO, na extracao), o
// croma sobe x2,3 com teto 0,21 e a luminosidade vai a 0,60. O texto branco
// segue lendo: a luz mais clara (L 0,60) a alfa 0,72 sobre #0D0D0D fica em L
// ~0,43, e a maior parte do texto esta em cartao e nao sobre a luz.
static void ajustarRegiao(const float bruto[3], float saida[3]) {
  float L, C, h;
  lchDe(bruto, &L, &C, &h);
  // IMERSIVA NO VIDRO (03/10/2026, acentos-mockup.html quadro 5): a luz
  // de canto e o matiz da regiao com L 0,42 e croma <= 0,11 — a L e o croma
  // ficam travados, entao Branco vira nevoa neutra e Jade nao acende a tela.
  (void)L;
  C *= 2.3f;
  if (C > 0.11f) C = 0.11f;
  lchParaSrgb(0.42f, C, h, saida);
}

// ------------------------------------------------------------------ extracao
//
// GRADE DE 32x18 PONTOS, e nao a imagem inteira: a textura ja chega reduzida
// (um destaque de 1280x720 tem 920 mil pixels), e 576 amostras dizem qual e a
// cor dominante tao bem quanto elas. Ponto e nao media de area: a media num
// bloco borra a borda entre duas cores e cria um terceiro tom que nao existe
// na arte.
//
// QUANTIZACAO POR MATIZ E BRILHO: 24 baldes de 15 graus, cada um partido em
// claro e escuro (luma 0,40). O matiz decide o destaque; a particao existe
// para o degrade — o "catch me" azul-claro sobre azul-marinho da logo de
// "Prenda-me Se For Capaz" e o mesmo matiz em dois brilhos, e sao exatamente as
// duas cores que o degrade tem de ter. k-means nao entrou: varias passadas e
// sementes para responder uma pergunta que o histograma responde numa.
//
// O PESO de cada amostra cromatica e o CROMA AO QUADRADO (quem e mais colorido
// vota muito mais) vezes a proximidade de uma luminosidade media. E, em FOTO
// (arte opaca), o TOM DE PELE vota com 12% do peso: matiz 8-42 graus com
// saturacao media. Foi o defeito do dono em 25/09 — "Prenda-me Se For Capaz":
// fundo branco, camisa azul, logo azul, e o botao saiu laranja-avermelhado
// porque o rosto e os bracos do DiCaprio somavam mais que a camisa. Se a pele
// e a UNICA cor da arte ("dominante de longe"), o peso cheio volta: melhor um
// laranja de pele que um branco padrao numa arte que so tem isso.
//
// Fora: transparente, quase preto (v < 0,14), cinza (croma < 0,10) — o que
// tira tambem o quase branco. Menos de 4% de amostras cromaticas = arte P&B,
// ok = 0 e quem usa troca pelo destaque padrao (ou pela outra arte do titulo).
//
// LOGO (arte com >= 20% de transparencia) nao tem desconto de pele: laranja em
// logo e marca, nao rosto.
//
// CUSTO MEDIDO: ~15 us por arte no Mac, sem alocacao (a grade e fixa, o tamanho
// quase nao pesa). Na TV roda no fio de decode, em prioridade baixa; o log
// `[cor] extracao: N us` da a medida do aparelho.
#define CV_GX 32
#define CV_GY 18
#define CV_NB 24
#define CV_NC (CV_NB * 2)
typedef struct { float w, r, g, b, wc, rc, gc, bc; int n; } Celula;   // *c = sem desconto de pele

static void mediaCel(const Celula *c, float out[3]) {
  float W = c->w > 1e-6f ? c->w : 1e-6f;
  out[0] = c->r / W; out[1] = c->g / W; out[2] = c->b / W;
}
static void pesoCheio(Celula *c) {
  c->w = c->wc; c->r = c->rc; c->g = c->gc; c->b = c->bc;
}

// TEXTURA: O RECORTE (acentos-mockup.html, quadros 8-9). Uma grade propria de
// 24 colunas (linhas na proporcao da imagem) pontua cada regiao:
//   LOGO -> alfa x (0,35 + croma): o miolo opaco e com cor do glifo, numa
//           janela de 20% da largura;
//   ARTE -> croma x (1 - |L - 0,5|) - 1,5 x borda: a regiao mais colorida e
//           de meio-tom, numa janela de 30% — a borda (diferenca de L com o
//           vizinho) desconta, senao O Nevoeiro escolhia o caixilho preto da
//           janela e a pilula saia com uma faixa preta no meio.
// A janela tem a proporcao da pilula (3,2:1). Depois, 32x10 pontos DENTRO do
// recorte dao a luminancia nos percentis 10 e 90 e a cor media; no logo, o
// transparente entra composto sobre o destaque (o que fica por baixo).
// Nada por quadro: sai na mesma passada da paleta e vai junto no corviva.txt.
#define TX_GX 24
#define TX_GYMAX 24
static float yLin(float r, float g, float b) {
  return 0.2126f * linDeSrgb(r) + 0.7152f * linDeSrgb(g) + 0.0722f * linDeSrgb(b);
}
static int cmpF(const void *a, const void *b) {
  float x = *(const float *)a, y = *(const float *)b;
  return x < y ? -1 : x > y;
}
static void recorteTextura(const unsigned char *px, int w, int h, int pitch, CorvivaPaleta *p) {
  float sc[TX_GYMAX][TX_GX], Lg[TX_GYMAX][TX_GX], frac = p->transparente ? 0.20f : 0.30f;
  float melhor = -1e9f, cwN, chN, cx, cy, ys[320], soma[3] = { 0, 0, 0 }, yAc;
  int gy, i, j, ww, hh, bx = 0, by = 0, n = 0;
  gy = (int)(TX_GX * (float)h / (float)w + 0.5f);
  if (gy < 3) gy = 3;
  if (gy > TX_GYMAX) gy = TX_GYMAX;
  for (j = 0; j < gy; j++) for (i = 0; i < TX_GX; i++) {
    const unsigned char *q = px + (size_t)((2 * j + 1) * h / (2 * gy)) * (size_t)pitch
                                + (size_t)((2 * i + 1) * w / (2 * TX_GX)) * 4;
    float rgb[3] = { q[0] / 255.0f, q[1] / 255.0f, q[2] / 255.0f }, L, C, hh2, a = q[3] / 255.0f;
    lchDe(rgb, &L, &C, &hh2);
    Lg[j][i] = a < 0.5f ? -1.0f : L;
    sc[j][i] = p->transparente ? a * (0.35f + C) : C * (1.0f - fabsf(L - 0.5f));
  }
  if (!p->transparente)
    for (j = 0; j < gy; j++) for (i = 0; i < TX_GX; i++) {
      float b = 0.0f;
      if (i + 1 < TX_GX) b = fabsf(Lg[j][i] - Lg[j][i + 1]);
      if (j + 1 < gy && fabsf(Lg[j][i] - Lg[j + 1][i]) > b) b = fabsf(Lg[j][i] - Lg[j + 1][i]);
      sc[j][i] -= 1.5f * b;
    }
  cwN = frac;
  chN = frac * (float)w / (3.2f * (float)h);
  if (chN > 1.0f) { cwN *= 1.0f / chN; chN = 1.0f; }
  ww = (int)(cwN * TX_GX + 0.5f); if (ww < 1) ww = 1;
  hh = (int)(chN * gy + 0.5f);    if (hh < 1) hh = 1;
  if (hh > gy) hh = gy;
  for (j = 0; j + hh <= gy; j++) for (i = 0; i + ww <= TX_GX; i++) {
    float t = 0.0f; int a2, b2;
    for (b2 = 0; b2 < hh; b2++) for (a2 = 0; a2 < ww; a2++) t += sc[j + b2][i + a2];
    if (t > melhor) { melhor = t; bx = i; by = j; }
  }
  cx = (bx + ww * 0.5f) / TX_GX; cy = (by + hh * 0.5f) / gy;
  cx -= cwN * 0.5f; cy -= chN * 0.5f;
  if (cx < 0.0f) cx = 0.0f;
  if (cy < 0.0f) cy = 0.0f;
  if (cx + cwN > 1.0f) cx = 1.0f - cwN;
  if (cy + chN > 1.0f) cy = 1.0f - chN;
  p->tx[0] = cx; p->tx[1] = cy; p->tx[2] = cwN; p->tx[3] = chN;
  yAc = yLin(p->acento[0], p->acento[1], p->acento[2]);
  for (j = 0; j < 10; j++) for (i = 0; i < 32; i++) {
    int xx = (int)((cx + cwN * (i + 0.5f) / 32.0f) * w), yy = (int)((cy + chN * (j + 0.5f) / 10.0f) * h);
    const unsigned char *q;
    float a, r, g, b;
    if (xx >= w) xx = w - 1;
    if (yy >= h) yy = h - 1;
    q = px + (size_t)yy * (size_t)pitch + (size_t)xx * 4;
    a = q[3] / 255.0f;
    r = q[0] / 255.0f; g = q[1] / 255.0f; b = q[2] / 255.0f;
    ys[n++] = a * yLin(r, g, b) + (1.0f - a) * yAc;
    soma[0] += a * r + (1.0f - a) * p->acento[0];
    soma[1] += a * g + (1.0f - a) * p->acento[1];
    soma[2] += a * b + (1.0f - a) * p->acento[2];
  }
  qsort(ys, (size_t)n, sizeof *ys, cmpF);
  p->txY[0] = ys[n / 10];
  p->txY[1] = ys[(n * 9) / 10];
  for (i = 0; i < 3; i++) p->txMedia[i] = soma[i] / (float)n;
  p->txOk = 1;
}

int corviva_extrair(const unsigned char *px, int w, int h, int pitch,
                    CorvivaPaleta *p) {
  Celula cel[CV_NC];
  float reg[4][3], regN[4];
  int gx, gy, i, j, total = 0, transp = 0, crom = 0, pele = 0;
  float matizW[CV_NB], matizWc[CV_NB];
  if (!p) return 0;
  memset(p, 0, sizeof *p);
  for (i = 0; i < 3; i++) {
    p->acento[i] = BRANCO_ACENTO[i]; p->base[i] = 0.051f;
    for (j = 0; j < 3; j++) p->grad[j][i] = BRANCO_ACENTO[i];
    for (j = 0; j < 4; j++) p->regiao[j][i] = 0.051f;
  }
  if (!px || w <= 0 || h <= 0 || pitch < w * 4) return 0;
  memset(cel, 0, sizeof cel); memset(reg, 0, sizeof reg); memset(regN, 0, sizeof regN);
  gx = w < CV_GX ? w : CV_GX;
  gy = h < CV_GY ? h : CV_GY;
  for (j = 0; j < gy; j++) {
    const unsigned char *ln = px + (size_t)((2 * j + 1) * h / (2 * gy)) * (size_t)pitch;
    for (i = 0; i < gx; i++) {
      const unsigned char *q = ln + (size_t)((2 * i + 1) * w / (2 * gx)) * 4;
      float r, g, b, mx, mn, c, v, hue, wt, luma;
      int k, faixa;
      if (q[3] < 200) { transp++; continue; }
      total++;
      r = q[0] * (1.0f / 255.0f); g = q[1] * (1.0f / 255.0f); b = q[2] * (1.0f / 255.0f);
      // Regioes da luz ambiente: tercos da esquerda, direita, topo e base.
      // Toda amostra opaca conta, cinza inclusive — a luz de um ceu cinza e
      // cinza, e inventar cor ali seria mentir sobre a arte.
      mx = r > g ? (r > b ? r : b) : (g > b ? g : b);
      mn = r < g ? (r < b ? r : b) : (g < b ? g : b);
      c = mx - mn; v = mx;
      // MEDIA_REGIAO: cada amostra pesa 0,25 + 8 x croma^2, e nao 1. A media
      // simples de um terco da arte puxa para o cinza (o vermelho do vestido
      // dilui no fundo escuro); com o peso, quem tem cor manda na luz, e uma
      // regiao sem cor nenhuma continua cinza — o peso base garante isso.
      { float wr = 0.25f + 8.0f * c * c;
        if (i * 3 < gx)      { reg[0][0] += r * wr; reg[0][1] += g * wr; reg[0][2] += b * wr; regN[0] += wr; }
        if (i * 3 >= 2 * gx) { reg[1][0] += r * wr; reg[1][1] += g * wr; reg[1][2] += b * wr; regN[1] += wr; }
        if (j * 3 < gy)      { reg[2][0] += r * wr; reg[2][1] += g * wr; reg[2][2] += b * wr; regN[2] += wr; }
        if (j * 3 >= 2 * gy) { reg[3][0] += r * wr; reg[3][1] += g * wr; reg[3][2] += b * wr; regN[3] += wr; } }
      if (v < 0.14f || c < 0.10f) continue;
      if (mx == r)      hue = (g - b) / c;
      else if (mx == g) hue = (b - r) / c + 2.0f;
      else              hue = (r - g) / c + 4.0f;
      if (hue < 0.0f) hue += 6.0f;
      k = (int)(hue * (CV_NB / 6.0f));
      if (k < 0) k = 0;
      if (k >= CV_NB) k = CV_NB - 1;
      luma = 0.299f * r + 0.587f * g + 0.114f * b;
      faixa = luma < 0.40f ? 1 : 0;
      wt = c * c * (1.0f - 1.1f * fabsf(v - 0.62f));
      if (wt < 0.005f) wt = 0.005f;
      { Celula *ce = &cel[k * 2 + faixa];
        float s = c / v, graus = hue * 60.0f, wp = wt;
        if (graus >= 8.0f && graus <= 42.0f && s >= 0.18f && s <= 0.64f && v >= 0.30f) {
          wp = wt * 0.12f; pele++;
        }
        ce->w += wp; ce->r += r * wp; ce->g += g * wp; ce->b += b * wp;
        ce->wc += wt; ce->rc += r * wt; ce->gc += g * wt; ce->bc += b * wt;
        ce->n++; }
      crom++;
    }
  }
  if (total < 8) return 0;
  p->transparente = transp * 5 >= (transp + total);     // >= 20% transparente
  if (p->transparente) {
    // Logo: sem desconto de pele.
    for (i = 0; i < CV_NC; i++) pesoCheio(&cel[i]);
  }
  // Regioes (valem mesmo sem cor: o cinza vira luz cinza e fraca).
  for (i = 0; i < 4; i++) {
    float m[3] = { 0.051f, 0.051f, 0.051f };
    if (regN[i] > 0.0f) { m[0] = reg[i][0] / regN[i]; m[1] = reg[i][1] / regN[i]; m[2] = reg[i][2] / regN[i]; }
    ajustarRegiao(m, p->regiao[i]);
  }
  if (crom * 25 < total) return 0;   // < 4% cromatico: P&B
  for (i = 0; i < CV_NB; i++) {
    matizW[i] = cel[i * 2].w + cel[i * 2 + 1].w;
    matizWc[i] = cel[i * 2].wc + cel[i * 2 + 1].wc;
  }
  {
    int melhor = 0, usarCheio = 0, a, z;
    float sc, melhorSc = -1.0f, W, bruto[3];
    for (i = 0; i < CV_NB; i++) {
      sc = matizW[i] + 0.5f * (matizW[(i + CV_NB - 1) % CV_NB] + matizW[(i + 1) % CV_NB]);
      if (sc > melhorSc) { melhorSc = sc; melhor = i; }
    }
    // Pele como unica cor: o desconto derrubou tudo abaixo do piso. Volta ao
    // peso cheio (a arte so tem isso).
    if (melhorSc < 0.0025f * (float)total) {
      melhorSc = -1.0f;
      for (i = 0; i < CV_NB; i++) {
        sc = matizWc[i] + 0.5f * (matizWc[(i + CV_NB - 1) % CV_NB] + matizWc[(i + 1) % CV_NB]);
        if (sc > melhorSc) { melhorSc = sc; melhor = i; }
      }
      if (melhorSc < 0.0025f * (float)total) return 0;
      usarCheio = 1;
      for (i = 0; i < CV_NC; i++) pesoCheio(&cel[i]);
    }
    (void)usarCheio; (void)pele;
    a = (melhor + CV_NB - 1) % CV_NB; z = (melhor + 1) % CV_NB;
    W = cel[melhor*2].w + cel[melhor*2+1].w + 0.5f * (cel[a*2].w + cel[a*2+1].w + cel[z*2].w + cel[z*2+1].w);
    bruto[0] = (cel[melhor*2].r + cel[melhor*2+1].r + 0.5f * (cel[a*2].r + cel[a*2+1].r + cel[z*2].r + cel[z*2+1].r)) / W;
    bruto[1] = (cel[melhor*2].g + cel[melhor*2+1].g + 0.5f * (cel[a*2].g + cel[a*2+1].g + cel[z*2].g + cel[z*2+1].g)) / W;
    bruto[2] = (cel[melhor*2].b + cel[melhor*2+1].b + 0.5f * (cel[a*2].b + cel[a*2+1].b + cel[z*2].b + cel[z*2+1].b)) / W;
    corviva_da_arte(bruto, p->acento);

    // GRADIENTE (acentos-mockup.html, C.grad): DUAS paradas da mesma familia.
    // A primeira e o Da arte; a segunda e a proxima cor da arte (a celula mais
    // pesada, de croma >= 0,04) que esteja a 20-60 graus da primeira — cor
    // ANALOGA, nunca bandeira. Nenhuma assim (arte de um matiz so) = o mesmo
    // matiz girado 24 graus. A parada do meio e a metade em OKLab, para o
    // shader de tres paradas (GFX_COR_GRAD) desenhar a reta entre as duas.
    { float La, ca, ha, hb, cb, B[3], la[3], lb[3], lm[3];
      int k, x, usados[CV_NC];
      lchDe(p->acento, &La, &ca, &ha);
      hb = ha + 24.0f * CV_GRAU; cb = ca;
      memset(usados, 0, sizeof usados);
      for (k = 0; k < CV_NC; k++) {
        int m = -1;
        float L2, c2, h2, d;
        for (x = 0; x < CV_NC; x++)
          if (!usados[x] && cel[x].w > 0.0f && (m < 0 || cel[x].w > cel[m].w)) m = x;
        if (m < 0) break;
        usados[m] = 1;
        mediaCel(&cel[m], bruto);
        lchDe(bruto, &L2, &c2, &h2);
        d = fabsf(grausDe(h2) - grausDe(ha));
        if (d > 180.0f) d = 360.0f - d;
        if (c2 >= 0.04f && d >= 20.0f && d <= 60.0f) {
          hb = h2; cb = c2 * 1.15f;
          if (cb < CV_C_MIN) cb = CV_C_MIN;
          if (cb > CV_C_MAX) cb = CV_C_MAX;
          break;
        }
      }
      if (corviva_contraste(p->acento, TINTA_ESC) > corviva_contraste(p->acento, BRANCO_A))
        lchParaSrgb(0.80f, cb > 0.16f ? 0.16f : cb, hb, B);
      else profundo(hb, cb, B);
      corviva_srgb_para_oklab(p->acento, la);
      corviva_srgb_para_oklab(B, lb);
      for (k = 0; k < 3; k++) lm[k] = 0.5f * (la[k] + lb[k]);
      memcpy(p->grad[0], p->acento, sizeof p->grad[0]);
      corviva_oklab_para_srgb(lm, p->grad[1]);
      memcpy(p->grad[2], B, sizeof p->grad[2]); }

    // A BASE sai da celula mais POPULOSA (contagem, nao peso): o destaque e a
    // cor que salta da arte, o fundo e a que a arte mais TEM (o azul do ceu
    // atras do casaco vermelho). Croma proporcional a quanto da arte e colorida.
    { int povo = 0; float f, cc, hh2, Lb, lab[3];
      // Celula que e quase so PELE nao vale como fundo: na foto do dono o rosto
      // somava mais amostras que a camisa, e a base saia marrom sob um botao
      // azul. So conta quem manteve ao menos metade do peso depois do desconto.
      for (i = 1; i < CV_NC; i++) {
        int bom = cel[i].w * 2.0f >= cel[i].wc, bomP = cel[povo].w * 2.0f >= cel[povo].wc;
        if ((bom && !bomP) || (bom == bomP && cel[i].n > cel[povo].n)) povo = i;
      }
      mediaCel(&cel[povo], bruto);
      f = (float)crom / (float)total;
      lchDe(bruto, &Lb, &cc, &hh2);
      cc *= f < 0.5f ? f * 2.0f : 1.0f;
      lab[0] = Lb; lab[1] = cc * cosf(hh2); lab[2] = cc * sinf(hh2);
      corviva_oklab_para_srgb(lab, bruto);
      ajustarBase(bruto, p->base); }
  }
  p->ok = 1;
  recorteTextura(px, w, h, pitch, p);
  return 1;
}

// -------------------------------------------------------------- memoria
//
// 256 artes, anel simples (a mais antiga sai). Chave = FNV-1a da url. So as
// artes de TELA CHEIA e os LOGOS passam por aqui (tex_cache.c anota so o que
// foi pedido no teto do destaque ou tem transparencia), entao 256 e horas de
// navegacao.
#define CV_TAB 256
typedef struct { unsigned int h; CorvivaPaleta p; } Entrada;
static Entrada tab[CV_TAB];
static int tabProx;
static volatile int trava;
static int sujo;
static void travar(void) { while (__sync_lock_test_and_set(&trava, 1)) { } }
static void soltar(void) { __sync_lock_release(&trava); }

static unsigned int hashDe(const char *s) {
  unsigned int h = 2166136261u;
  if (!s) return 0;
  while (*s) { h ^= (unsigned char)*s++; h *= 16777619u; }
  return h ? h : 1u;   // 0 e "nenhuma"
}

static void guardar(unsigned int h, const CorvivaPaleta *p) {
  int i;
  for (i = 0; i < CV_TAB; i++) if (tab[i].h == h) { tab[i].p = *p; return; }
  tab[tabProx].h = h; tab[tabProx].p = *p;
  tabProx = (tabProx + 1) % CV_TAB;
}

void corviva_anotar(const char *chave, const CorvivaPaleta *p) {
  unsigned int h = hashDe(chave);
  if (!h || !p || !chave[0]) return;
  travar();
  guardar(h, p);
  sujo = 1;
  soltar();
}

static int buscar(unsigned int h, CorvivaPaleta *p) {
  int i, achou = 0;
  if (!h) return 0;
  travar();
  for (i = 0; i < CV_TAB; i++) if (tab[i].h == h) { *p = tab[i].p; achou = 1; break; }
  soltar();
  return achou;
}

int corviva_paleta(const char *chave, CorvivaPaleta *p) {
  return p && chave && chave[0] && buscar(hashDe(chave), p);
}

// ------------------------------------------------------------- quem manda
// As URLs andam junto dos hashes so para a TEXTURA: o desenho precisa da
// textura do titulo em cena (tex_cache, pela url), e a paleta so guarda o hash.
#define CV_URL 512
static char pedidoUrl[CV_URL], pedidoLogoUrl[CV_URL], pendUrl[CV_URL], pendLogoUrl[CV_URL];
static char cenaUrl[CV_URL], cenaLogoUrl[CV_URL];
static unsigned int pedidoH, pedidoLogoH;   // os pedidos de maior prioridade DESTE quadro
static int pedidoPrio, pedidoLogoPrio;
static unsigned int pendH, pendLogoH;       // o que esta pedido, esperando assentar
static double pendDesde;
static unsigned int cenaH, cenaLogoH;       // o titulo cuja cor vale agora
static CorvivaPaleta cena, cenaLogo;
static int temCena, temLogo;
static double relogio;                      // ms, somando os dt do laco
static double ultGravacao = -1e9;

// 150 ms parado antes de trocar: quem rola o destaque depressa passa por um
// titulo a cada ~120 ms (repeticao de tecla da TV), e cada um deles mudaria a
// cor da tela inteira.
#define CV_ASSENTAR_MS 150.0
// 450 ms, saida suave (cubica). O crossfade do destaque leva ~400 ms: a cor
// chega junto com a arte, nem antes nem muito depois.
#define CV_DURACAO_S   0.45f

int corviva_cena_paleta(CorvivaPaleta *p) {
  if (!p || !temCena || !cena.ok) return 0;
  *p = cena;
  return 1;
}

void corviva_definir(const char *chave, int prioridade) {
  if (!chave || !chave[0] || prioridade <= pedidoPrio) return;
  pedidoH = hashDe(chave);
  pedidoPrio = prioridade;
  snprintf(pedidoUrl, sizeof pedidoUrl, "%s", chave);
}
void corviva_definir_logo(const char *chave, int prioridade) {
  if (!chave || !chave[0] || prioridade <= pedidoLogoPrio) return;
  pedidoLogoH = hashDe(chave);
  pedidoLogoPrio = prioridade;
  snprintf(pedidoLogoUrl, sizeof pedidoLogoUrl, "%s", chave);
}

// ----------------------------------------------------------------- movimento
//
// NOVE CORES andam juntas, todas em OKLab: destaque, base, as tres paradas do
// degrade e as quatro luzes de regiao. Mais a forca da luz ambiente, que e um
// numero so (entra e sai com o modo imersivo).
#define CV_NCOR 9
// Sem titulo (ou arte cinza): o acento Branco (#f4f2ee), e nao o #ffffff —
// e a mesma cor do acento fixo 0.
static const float BRANCO[3] = { 0xf4 / 255.0f, 0xf2 / 255.0f, 0xee / 255.0f };
static const float FUNDO[3]  = { 0.051f, 0.051f, 0.051f };   // NV_COR_FUNDO
float nv_cor_fundo_viva[3] = { 0.051f, 0.051f, 0.051f };
float nv_acento_viva[3] = { 0xf4 / 255.0f, 0xf2 / 255.0f, 0xee / 255.0f };
float nv_grad_viva[3][3] = { { 1, 1, 1 }, { 1, 1, 1 }, { 1, 1, 1 } };
float nv_luz_viva[3] = { 0.30f, 0.30f, 0.29f };
CorvivaTextura nv_textura_viva;
int   nv_grad_ativo;
float nv_ambiente_viva[4][3];
float nv_ambiente_forca;
float nv_tempo_viva;
static float deL[CV_NCOR][3], paraL[CV_NCOR][3];   // OKLab
static float alvo[CV_NCOR][3];                     // sRGB
static float deForca, paraForca, alvoForca = -1.0f;
static int alvoGrad;
static float t = 1.0f;
static int iniciado, retargets;

static float *corAtual(int i) {
  if (i == 0) return nv_acento_viva;
  if (i == 1) return nv_cor_fundo_viva;
  if (i < 5)  return nv_grad_viva[i - 2];
  return nv_ambiente_viva[i - 5];
}

void corviva_acento(float *r, float *g, float *b) {
  if (r) *r = nv_acento_viva[0];
  if (g) *g = nv_acento_viva[1];
  if (b) *b = nv_acento_viva[2];
}
// A cor do LOGO do titulo em cena, a mesma fonte que a Imersiva usa com "Cor da
// logo" (corviva_quadro: logo de cor, senao a arte). 0 = ainda sem paleta.
int corviva_logo_acento(float *r, float *g, float *b) {
  const CorvivaPaleta *p = (temLogo && cenaLogo.ok) ? &cenaLogo : (temCena && cena.ok) ? &cena : NULL;
  if (!p) return 0;
  *r = p->acento[0]; *g = p->acento[1]; *b = p->acento[2];
  return 1;
}
int corviva_retargets(void) { return retargets; }

void corviva_quadro(float dt, int modo, int usarLogo, int reduzido) {
  float novo[CV_NCOR][3], forca;
  int k, grad, fonteOk;
  const CorvivaPaleta *fonte;
  if (dt < 0.0f) dt = 0.0f;
  relogio += (double)dt * 1000.0;
  nv_tempo_viva = reduzido ? 0.0f : (float)(relogio / 1000.0);

  // O pedido do quadro que ACABOU de ser desenhado vira o pendente.
  if (pedidoPrio) {
    if (pedidoH != pendH) { pendH = pedidoH; pendDesde = relogio; }
    memcpy(pendUrl, pedidoUrl, sizeof pendUrl);
    pendLogoH = (pedidoLogoPrio == pedidoPrio) ? pedidoLogoH : 0;
    if (pendLogoH) memcpy(pendLogoUrl, pedidoLogoUrl, sizeof pendLogoUrl);
  }
  pedidoPrio = 0; pedidoLogoPrio = 0;
  if (pendH && pendH != cenaH && relogio - pendDesde >= CV_ASSENTAR_MS) {
    CorvivaPaleta p;
    // Sem paleta ainda (a arte nao terminou de decodificar): espera, com a cor
    // anterior de pe. Assim que o fio de decode anotar, este teste passa.
    if (buscar(pendH, &p)) {
      cenaH = pendH; cena = p; temCena = 1;
      memcpy(cenaUrl, pendUrl, sizeof cenaUrl);
      cenaLogoH = 0; temLogo = 0;
      travar(); sujo = 1; soltar();
    }
  }
  // O LOGO chega depois (url resolvida pela sessao, decode proprio): segue o
  // titulo em cena e e procurado todo quadro ate aparecer — uma busca de 256
  // inteiros, so enquanto falta.
  if (cenaH && cenaH == pendH && pendLogoH != cenaLogoH) {
    cenaLogoH = pendLogoH; temLogo = 0;
    memcpy(cenaLogoUrl, pendLogoUrl, sizeof cenaLogoUrl);
    travar(); sujo = 1; soltar();
  }
  if (cenaLogoH && !temLogo) temLogo = buscar(cenaLogoH, &cenaLogo);

  // A FONTE DA COR: o logo, quando o ajuste pede e ele tem cor (logo branco ou
  // preto nao tem, e ai vale a arte); senao a arte.
  // TEXTURA: com logo de cor e recorte, a fonte e o logo (a cor lisa por
  // baixo do glifo tem de ser a dele), com ou sem "Cor da logo".
  { int tx = modo == CORVIVA_TEXTURA || modo == CORVIVA_TEXTURA_SUTIL;
    int logoTx = tx && temLogo && cenaLogo.ok && cenaLogo.txOk && cenaLogoUrl[0];
    fonte = ((usarLogo || logoTx) && temLogo && cenaLogo.ok) ? &cenaLogo : &cena;
    memset(&nv_textura_viva, 0, sizeof nv_textura_viva);
    if (tx && (logoTx || (temCena && cena.ok && cena.txOk && cenaUrl[0]))) {
      const CorvivaPaleta *f = logoTx ? &cenaLogo : &cena;
      CorvivaTextura *T = &nv_textura_viva;
      T->ok = 1; T->logo = logoTx;
      snprintf(T->url, sizeof T->url, "%s", logoTx ? cenaLogoUrl : cenaUrl);
      memcpy(T->janela, f->tx, sizeof T->janela);
      memcpy(T->base, f->acento, sizeof T->base);
      memcpy(T->media, f->txMedia, sizeof T->media);
      T->forca = modo == CORVIVA_TEXTURA_SUTIL ? 0.35f : 1.0f;
      corviva_textura_tinta(f->txY[0], f->txY[1], f->acento, T->forca,
                            &T->tintaBranca, &T->veu, &T->contraste);
      // (14/10, dono: "os com textura tem alguns que sao muito bons e outros
      // nao") O RECORTE CHAPADO e o que nao presta: p90 - p10 de luminancia
      // abaixo de 0,12 nao tem material nenhum (glifo liso, faixa unica) e a
      // pilula sai como cor lisa com o tema fazendo nada. Ajustes troca estes
      // pelo MATERIAL CC0 embutido (ajustes_textura_quadro).
      T->fraca = (f->txY[1] - f->txY[0]) < 0.12f;
    } }
  fonteOk = modo != CORVIVA_DESLIGADA && temCena && fonte->ok;
  if (!fonteOk && modo != CORVIVA_DESLIGADA && temLogo && cenaLogo.ok) {
    fonte = &cenaLogo; fonteOk = 1;   // arte P&B com logo colorido
  }
  memcpy(novo[0], fonteOk ? fonte->acento : BRANCO, sizeof novo[0]);
  if (fonteOk && modo == CORVIVA_SIMPLES_CLARO) {
    float L, C, h;
    lchDe(novo[0], &L, &C, &h);
    // Lift lightness and halve chroma without rotating hue; gamut mapping
    // only lowers chroma further. Existing tokens select contrasting ink.
    lchParaSrgb(L + (1.0f - L) * 0.65f, C * 0.45f, h, novo[0]);
  }
  memcpy(novo[1], FUNDO, sizeof novo[1]);   // o estilizado (base tingida) saiu
  for (k = 0; k < 3; k++) memcpy(novo[2 + k], fonteOk ? fonte->grad[k] : BRANCO, sizeof novo[0]);
  for (k = 0; k < 4; k++) memcpy(novo[5 + k], temCena ? cena.regiao[k] : FUNDO, sizeof novo[0]);
  // "Cor da logo" tambem vale para a LUZ da Imersiva (dono, 05/10: "o imersivo
  // da logo nao ta pegando a cor da logo"): as quatro luzes saiam sempre das
  // regioes da ARTE, e so o destaque seguia o logo. Com logo de cor, sao o
  // destaque e o degrade dele, na mesma faixa de luz das regioes.
  if (usarLogo && temCena && temLogo && cenaLogo.ok) {
    ajustarRegiao(cenaLogo.acento, novo[5]);
    for (k = 0; k < 3; k++) ajustarRegiao(cenaLogo.grad[k], novo[6 + k]);
  }
  grad = fonteOk && modo == CORVIVA_GRADIENTE;
  forca = (modo == CORVIVA_IMERSIVA && temCena) ? 1.0f : 0.0f;

  if (memcmp(novo, alvo, sizeof alvo) || forca != alvoForca || !iniciado) {
    memcpy(alvo, novo, sizeof alvo);
    for (k = 0; k < CV_NCOR; k++) {
      corviva_srgb_para_oklab(corAtual(k), deL[k]);
      corviva_srgb_para_oklab(alvo[k], paraL[k]);
    }
    deForca = nv_ambiente_forca; paraForca = forca; alvoForca = forca;
    // Primeiro quadro (cor do arranque) e animacoes reduzidas: sem transicao.
    t = (!iniciado || reduzido) ? 1.0f : 0.0f;
    if (iniciado) retargets++;
    iniciado = 1;
    if (t >= 1.0f) {
      for (k = 0; k < CV_NCOR; k++) memcpy(corAtual(k), alvo[k], sizeof alvo[k]);
      nv_ambiente_forca = paraForca;
    }
  }
  // O degrade LIGA no inicio da transicao e DESLIGA no fim: durante ela as
  // paradas andam junto com o destaque, entao ligar cedo nao mostra salto.
  if (grad) nv_grad_ativo = 1;
  alvoGrad = grad;
  if (t < 1.0f) {
    float e, u;
    t += dt / CV_DURACAO_S;
    if (reduzido || t > 1.0f) t = 1.0f;
    u = 1.0f - t;
    e = 1.0f - u * u * u;
    for (k = 0; k < CV_NCOR; k++) {
      if (t >= 1.0f) { memcpy(corAtual(k), alvo[k], sizeof alvo[k]); continue; }
      { float l[3] = { deL[k][0] + (paraL[k][0] - deL[k][0]) * e,
                       deL[k][1] + (paraL[k][1] - deL[k][1]) * e,
                       deL[k][2] + (paraL[k][2] - deL[k][2]) * e };
        corviva_oklab_para_srgb(l, corAtual(k)); }
    }
    nv_ambiente_forca = deForca + (paraForca - deForca) * e;
  }
  if (t >= 1.0f) nv_grad_ativo = alvoGrad;
  { static float ultimo[3] = { -1, -1, -1 };
    if (memcmp(ultimo, nv_acento_viva, sizeof ultimo)) {
      CorvivaTokens tk;
      memcpy(ultimo, nv_acento_viva, sizeof ultimo);
      corviva_tokens(nv_acento_viva, &tk);
      memcpy(nv_luz_viva, tk.luz, sizeof nv_luz_viva);
    } }
}

// A tinta da textura (txTinta do mockup). Luminancia relativa -> cinza sRGB e
// de volta: o veu e uma mistura em sRGB, como o radial-gradient do CSS.
static float sG(float y) { return y <= 0.0031308f ? 12.92f * y : 1.055f * powf(y, 1.0f / 2.4f) - 0.055f; }
void corviva_textura_tinta(float p10, float p90, const float base[3], float forca,
                           int *tintaBranca, float *veu, float *contraste) {
  float yi = 0.2126f * linDeSrgb(TINTA_ESC[0]) + 0.7152f * linDeSrgb(TINTA_ESC[1]) + 0.0722f * linDeSrgb(TINTA_ESC[2]);
  float cw, cd, cr, a = 0.0f;
  int branca;
  if (forca < 0.999f) {   // Textura sutil: o recorte a `forca` sobre a cor lisa
    float bY = sG(0.2126f * linDeSrgb(base[0]) + 0.7152f * linDeSrgb(base[1]) + 0.0722f * linDeSrgb(base[2]));
    p10 = linDeSrgb(forca * sG(p10) + (1.0f - forca) * bY);
    p90 = linDeSrgb(forca * sG(p90) + (1.0f - forca) * bY);
  }
  cw = corviva_contraste_y(1.0f, p90);
  cd = corviva_contraste_y(yi, p10);
  branca = cw >= cd;
  cr = branca ? cw : cd;
  while (cr < 4.5f && a < 0.7f) {
    a += 0.05f;
    cr = branca ? corviva_contraste_y(1.0f, linDeSrgb(sG(p90) * (1.0f - a)))
                : corviva_contraste_y(yi, linDeSrgb(sG(p10) * (1.0f - a) + a));
  }
  if (tintaBranca) *tintaBranca = branca;
  if (veu) *veu = a;
  if (contraste) *contraste = cr;
}

// ------------------------------------------------------------- corviva.txt
//
// Texto e nao binario, como todo arquivo de dados deste app: da para abrir no
// ssh e ler. "leve" (dados_gravar_leve): e cache re-obtivel, e no Tizen a
// descarga para o IndexedDB pode esperar o relogio longo. Formato 3 (03/10):
// "p3" + hash + ok + transparente + as nove cores + o recorte da Textura, e as
// urls da cena ("url", "urllogo"). As linhas "p" do formato 2 tem o destaque
// da regra antiga (tudo segurando texto branco) e sao ignoradas, como as do 1
// (e cache: a arte refaz a paleta no proximo decode).
static void corHex(const float c[3], char *d) {
  snprintf(d, 7, "%02x%02x%02x", (int)(c[0] * 255.0f + 0.5f),
           (int)(c[1] * 255.0f + 0.5f), (int)(c[2] * 255.0f + 0.5f));
}
static int lerHex(const char *s, float c[3]) {
  unsigned v;
  if (sscanf(s, "%6x", &v) != 1) return 0;
  c[0] = ((v >> 16) & 255) / 255.0f; c[1] = ((v >> 8) & 255) / 255.0f; c[2] = (v & 255) / 255.0f;
  return 1;
}

void corviva_carregar(void) {
  char *txt = dados_ler("corviva.txt"), *p, *fim;
  unsigned int atual = 0, atualLogo = 0;
  int n = 0;
  if (!txt) return;
  for (p = txt; *p; p = fim) {
    char c[9][8];
    unsigned h, h2 = 0; int ok, tr, lidos;
    fim = strchr(p, '\n');
    if (!fim) fim = p + strlen(p); else *fim++ = 0;
    if ((lidos = sscanf(p, "cena %x %x", &h, &h2)) >= 1) { atual = h; atualLogo = lidos == 2 ? h2 : 0; continue; }
    if (!strncmp(p, "url ", 4)) { snprintf(cenaUrl, sizeof cenaUrl, "%s", p + 4); continue; }
    if (!strncmp(p, "urllogo ", 8)) { snprintf(cenaLogoUrl, sizeof cenaLogoUrl, "%s", p + 8); continue; }
    if (sscanf(p, "p3 %x %d %d %7s %7s %7s %7s %7s %7s %7s %7s %7s", &h, &ok, &tr,
               c[0], c[1], c[2], c[3], c[4], c[5], c[6], c[7], c[8]) == 12 && h) {
      CorvivaPaleta pal;
      int k, bom = 1, txOk = 0;
      float t4[4], y2[2];
      char med[8], *q = p;
      memset(&pal, 0, sizeof pal);
      // TEXTURA, depois das nove cores: ok x y w h p10 p90 media.
      for (k = 0; k < 13 && q; k++) { q = strchr(q, ' '); if (q) q++; }
      if (q && sscanf(q, "%d %f %f %f %f %f %f %7s", &txOk, &t4[0], &t4[1], &t4[2], &t4[3],
                      &y2[0], &y2[1], med) == 8 && txOk && lerHex(med, pal.txMedia)) {
        pal.txOk = 1;
        memcpy(pal.tx, t4, sizeof t4); memcpy(pal.txY, y2, sizeof y2);
      }
      pal.ok = ok ? 1 : 0; pal.transparente = tr ? 1 : 0;
      bom &= lerHex(c[0], pal.acento);
      bom &= lerHex(c[1], pal.base);
      for (k = 0; k < 3; k++) bom &= lerHex(c[2 + k], pal.grad[k]);
      for (k = 0; k < 4; k++) bom &= lerHex(c[5 + k], pal.regiao[k]);
      if (!bom) continue;
      travar(); guardar(h, &pal); soltar();
      n++;
    }
  }
  free(txt);
  if (atual) {
    CorvivaPaleta pal;
    if (buscar(atual, &pal)) { cenaH = pendH = atual; cena = pal; temCena = 1; memcpy(pendUrl, cenaUrl, sizeof pendUrl); }
    if (atualLogo && buscar(atualLogo, &cenaLogo)) { cenaLogoH = pendLogoH = atualLogo; temLogo = 1; }
  }
  printf("[cor] %d paleta(s) de corviva.txt%s\n", n, temCena ? ", com a ultima cena" : "");
  fflush(stdout);
}

void corviva_gravar_se_preciso(int forcar) {
  static char buf[CV_TAB * 140 + 2 * CV_URL + 64];
  size_t w = 0;
  int i, k;
  if (!sujo) return;
  if (!forcar && relogio - ultGravacao < 20000.0) return;
  ultGravacao = relogio;
  w += (size_t)snprintf(buf + w, sizeof buf - w, "cena %08x %08x\n", cenaH, cenaLogoH);
  if (cenaUrl[0]) w += (size_t)snprintf(buf + w, sizeof buf - w, "url %s\n", cenaUrl);
  if (cenaLogoUrl[0] && w < sizeof buf) w += (size_t)snprintf(buf + w, sizeof buf - w, "urllogo %s\n", cenaLogoUrl);
  travar();
  // Do mais antigo ao mais novo, para o anel voltar na mesma ordem.
  for (k = 0; k < CV_TAB; k++) {
    const Entrada *e = &tab[(tabProx + k) % CV_TAB];
    char c[9][8], med[8];
    int j;
    if (!e->h) continue;
    corHex(e->p.txMedia, med);
    corHex(e->p.acento, c[0]); corHex(e->p.base, c[1]);
    for (j = 0; j < 3; j++) corHex(e->p.grad[j], c[2 + j]);
    for (j = 0; j < 4; j++) corHex(e->p.regiao[j], c[5 + j]);
    i = snprintf(buf + w, sizeof buf - w, "p3 %08x %d %d %s %s %s %s %s %s %s %s %s %d %.4f %.4f %.4f %.4f %.4f %.4f %s\n",
                 e->h, e->p.ok, e->p.transparente,
                 c[0], c[1], c[2], c[3], c[4], c[5], c[6], c[7], c[8],
                 e->p.txOk, e->p.tx[0], e->p.tx[1], e->p.tx[2], e->p.tx[3],
                 e->p.txY[0], e->p.txY[1], med);
    if (i < 0 || (size_t)i >= sizeof buf - w) break;
    w += (size_t)i;
  }
  sujo = 0;
  soltar();
  dados_gravar_leve("corviva.txt", buf);
}

void corviva_zerar(void) {
  int k;
  travar();
  memset(tab, 0, sizeof tab); tabProx = 0; sujo = 0;
  soltar();
  pedidoH = pendH = cenaH = pedidoLogoH = pendLogoH = cenaLogoH = 0;
  pedidoUrl[0] = pedidoLogoUrl[0] = pendUrl[0] = pendLogoUrl[0] = cenaUrl[0] = cenaLogoUrl[0] = 0;
  memset(&nv_textura_viva, 0, sizeof nv_textura_viva);
  pedidoPrio = pedidoLogoPrio = 0; temCena = temLogo = 0;
  relogio = 0; pendDesde = 0; ultGravacao = -1e9;
  memcpy(nv_acento_viva, BRANCO, sizeof nv_acento_viva);
  memcpy(nv_cor_fundo_viva, FUNDO, sizeof nv_cor_fundo_viva);
  for (k = 0; k < 3; k++) memcpy(nv_grad_viva[k], BRANCO, sizeof BRANCO);
  for (k = 0; k < 4; k++) memcpy(nv_ambiente_viva[k], FUNDO, sizeof FUNDO);
  memset(alvo, 0, sizeof alvo);
  nv_ambiente_forca = 0; alvoForca = -1.0f; nv_grad_ativo = 0; alvoGrad = 0;
  t = 1.0f; iniciado = 0; retargets = 0;
}
