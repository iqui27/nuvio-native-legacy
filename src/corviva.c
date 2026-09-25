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
    C -= 0.005f;
    if (C < 0.0f) C = 0.0f;
  }
  { float lab[3] = { L, 0, 0 }; corviva_oklab_para_srgb(lab, rgb); }
}
static void lchDe(const float rgb[3], float *L, float *C, float *h) {
  float lab[3];
  corviva_srgb_para_oklab(rgb, lab);
  *L = lab[0]; *C = sqrtf(lab[1] * lab[1] + lab[2] * lab[2]); *h = atan2f(lab[2], lab[1]);
}

// O DESTAQUE TEM DE CABER NA REGRA QUE JA EXISTE, e nao ganhar uma regra
// propria. A tinta sobre o realce e BRANCA sempre que o realce nao e branco
// (ajustes_acento_tinta, regra do dono de 21/09/2026), entao a cor extraida
// precisa segurar texto branco; e do outro lado ela e anel de foco sobre
// #0D0D0D, que abaixo de 3:1 some.
//
// A FAIXA ERA ESTREITA DEMAIS (L 0,50-0,57, croma 0,12-0,16) e foi o que o dono
// viu na TV em 25/09: "as cores muito parecidas". Com L travado em 7 centesimos
// todo destaque tinha o mesmo peso, e o laranja de pele e o vermelho do vestido
// saiam primos. Agora L vai de 0,46 a 0,68 e o croma de 0,10 a 0,24, e o que
// segura o texto e a conta de verdade: branco sobre o destaque a >= 3,2:1. E o
// piso de TEXTO GRANDE do WCAG (3:1), com folga — o texto sobre o realce e o
// rotulo de botao e de linha em foco, 26-30 px a 1080p vistos a 3 m, que e
// texto grande pela regra. O 4,5:1 anterior condenava todo destaque ao escuro.
#define CV_L_MIN 0.50f
#define CV_L_MAX 0.68f
#define CV_C_MIN 0.10f
#define CV_C_MAX 0.24f
#define CV_TEXTO_MIN 3.2f
// As DUAS regras de contraste, na conta de verdade (WCAG) e nao em L: texto
// branco por cima a >= 3,2:1 e o anel sobre #0D0D0D a >= 3:1. Juntas pedem
// luminancia relativa entre ~0,11 e ~0,28, e o laco anda L ate caber.
static void conferirContraste(float L, float C, float h, float saida[3]) {
  static const float branco[3] = { 1, 1, 1 }, fundo[3] = { 0.051f, 0.051f, 0.051f };
  int k;
  lchParaSrgb(L, C, h, saida);
  for (k = 0; k < 40 && corviva_contraste(saida, branco) < CV_TEXTO_MIN; k++) {
    L -= 0.01f; lchParaSrgb(L, C, h, saida);
  }
  for (k = 0; k < 40 && corviva_contraste(saida, fundo) < 3.0f; k++) {
    L += 0.01f; lchParaSrgb(L, C, h, saida);
  }
}
static void ajustarCor(const float bruto[3], float saida[3], float lMin, float lMax) {
  float L, C, h;
  lchDe(bruto, &L, &C, &h);
  if (L < lMin) L = lMin;
  if (L > lMax) L = lMax;
  // O balde carrega junto o tom apagado do mesmo matiz (a areia e o sofa
  // laranja caem no mesmo balde), e a media sai mais cinza que a cor que o
  // olho ve na arte: +15% de croma devolve parte do que a media tirou.
  C *= 1.15f;
  if (C < CV_C_MIN) C = CV_C_MIN;
  if (C > CV_C_MAX) C = CV_C_MAX;
  conferirContraste(L, C, h, saida);
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
// luz. O shader a pinta com alfa <= 0,5, entao o que chega a tela e metade.
static void ajustarRegiao(const float bruto[3], float saida[3]) {
  float L, C, h;
  lchDe(bruto, &L, &C, &h);
  if (L < 0.34f) L = 0.34f;
  if (L > 0.58f) L = 0.58f;
  C *= 1.6f;
  if (C > 0.16f) C = 0.16f;
  lchParaSrgb(L, C, h, saida);
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
static int distMatiz(int a, int b) {
  int d = a > b ? a - b : b - a;
  return d > CV_NB / 2 ? CV_NB - d : d;
}

int corviva_extrair(const unsigned char *px, int w, int h, int pitch,
                    CorvivaPaleta *p) {
  Celula cel[CV_NC];
  float reg[4][3];
  int regN[4];
  int gx, gy, i, j, total = 0, transp = 0, crom = 0, pele = 0;
  float matizW[CV_NB], matizWc[CV_NB];
  if (!p) return 0;
  memset(p, 0, sizeof *p);
  for (i = 0; i < 3; i++) {
    p->acento[i] = 1.0f; p->base[i] = 0.051f;
    for (j = 0; j < 3; j++) p->grad[j][i] = 1.0f;
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
      if (i * 3 < gx)      { reg[0][0] += r; reg[0][1] += g; reg[0][2] += b; regN[0]++; }
      if (i * 3 >= 2 * gx) { reg[1][0] += r; reg[1][1] += g; reg[1][2] += b; regN[1]++; }
      if (j * 3 < gy)      { reg[2][0] += r; reg[2][1] += g; reg[2][2] += b; regN[2]++; }
      if (j * 3 >= 2 * gy) { reg[3][0] += r; reg[3][1] += g; reg[3][2] += b; regN[3]++; }
      mx = r > g ? (r > b ? r : b) : (g > b ? g : b);
      mn = r < g ? (r < b ? r : b) : (g < b ? g : b);
      c = mx - mn; v = mx;
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
    if (regN[i]) { m[0] = reg[i][0] / regN[i]; m[1] = reg[i][1] / regN[i]; m[2] = reg[i][2] / regN[i]; }
    ajustarRegiao(m, p->regiao[i]);
  }
  if (crom * 25 < total) return 0;   // < 4% cromatico: P&B
  for (i = 0; i < CV_NB; i++) {
    matizW[i] = cel[i * 2].w + cel[i * 2 + 1].w;
    matizWc[i] = cel[i * 2].wc + cel[i * 2 + 1].wc;
  }
  {
    int melhor = 0, usarCheio = 0, a, z, prim, seg = -1, ter = -1;
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
    ajustarCor(bruto, p->acento, CV_L_MIN, CV_L_MAX);

    // DEGRADE: ate tres celulas. A primeira e a mais pesada do matiz vencedor
    // (ou dos vizinhos dele); as outras, as mais pesadas que sejam OUTRA cor —
    // matiz a 2+ baldes (30 graus) ou o mesmo matiz no outro brilho — e que
    // tenham ao menos 20% do peso da primeira. Menos que isso e ruido, e o
    // degrade inventa as que faltam a partir da primeira.
    prim = melhor * 2;
    for (i = -1; i <= 1; i++) {
      int kk = (melhor + i + CV_NB) % CV_NB, f;
      for (f = 0; f < 2; f++) if (cel[kk * 2 + f].w > cel[prim].w) prim = kk * 2 + f;
    }
    // E NO MAXIMO A 60 GRAUS (4 baldes) da primeira: degrade de cores
    // ANALOGAS. A primeira folha de contato (25/09) tinha laranja -> vermelho
    // -> azul no "Batman" e ouro -> azul na "Chegada" — duas cores da arte,
    // sim, mas o botao virava bandeira. Complementar fica para a luz ambiente.
    for (i = 0; i < CV_NC; i++) {
      int dm = distMatiz(i / 2, prim / 2);
      if (i == prim || cel[i].w < 0.20f * cel[prim].w || dm > 4) continue;
      if (!(dm >= 2 || (dm <= 1 && (i & 1) != (prim & 1)))) continue;
      if (seg < 0 || cel[i].w > cel[seg].w) seg = i;
    }
    if (seg >= 0) for (i = 0; i < CV_NC; i++) {
      int d1 = distMatiz(i / 2, prim / 2), d2 = distMatiz(i / 2, seg / 2);
      if (i == prim || i == seg || cel[i].w < 0.12f * cel[prim].w || d1 > 4) continue;
      if (!((d1 >= 2 || (i & 1) != (prim & 1)) && (d2 >= 2 || (i & 1) != (seg & 1)))) continue;
      if (ter < 0 || cel[i].w > cel[ter].w) ter = i;
    }
    {
      float st[3][3], L[3], C, hh;
      int n = 0, x, y;
      mediaCel(&cel[prim], bruto); ajustarCor(bruto, st[n++], 0.44f, 0.70f);
      if (seg >= 0) { mediaCel(&cel[seg], bruto); ajustarCor(bruto, st[n++], 0.44f, 0.70f); }
      if (ter >= 0) { mediaCel(&cel[ter], bruto); ajustarCor(bruto, st[n++], 0.44f, 0.70f); }
      // O QUE FALTA SE INVENTA DA PRIMEIRA: um passo mais claro e outro mais
      // escuro, com o matiz girado 10 graus para cada lado — e o que o olho le
      // como "a mesma cor com luz", e nao como duas cores.
      if (n < 3) {
        float L0, C0, h0, t2[3];
        lchDe(st[0], &L0, &C0, &h0);
        if (n == 1) {
          conferirContraste(L0 + 0.09f > 0.70f ? 0.70f : L0 + 0.09f, C0 * 0.95f, h0 - 0.17f, st[1]);
          n = 2;
        }
        conferirContraste(L0 - 0.10f < 0.40f ? 0.40f : L0 - 0.10f, C0, h0 + 0.17f, t2);
        memcpy(st[2], t2, sizeof t2);
        n = 3;
      }
      // Do mais claro ao mais escuro: o degrade anda sempre de cima-esquerda
      // (luz) para baixo-direita (sombra).
      for (x = 0; x < 3; x++) { lchDe(st[x], &L[x], &C, &hh); }
      for (x = 0; x < 3; x++) for (y = x + 1; y < 3; y++) if (L[y] > L[x]) {
        float tl = L[x], tc[3];
        L[x] = L[y]; L[y] = tl;
        memcpy(tc, st[x], sizeof tc); memcpy(st[x], st[y], sizeof tc); memcpy(st[y], tc, sizeof tc);
      }
      memcpy(p->grad, st, sizeof st);
    }

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

// ------------------------------------------------------------- quem manda
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

void corviva_definir(const char *chave, int prioridade) {
  if (!chave || !chave[0] || prioridade <= pedidoPrio) return;
  pedidoH = hashDe(chave);
  pedidoPrio = prioridade;
}
void corviva_definir_logo(const char *chave, int prioridade) {
  if (!chave || !chave[0] || prioridade <= pedidoLogoPrio) return;
  pedidoLogoH = hashDe(chave);
  pedidoLogoPrio = prioridade;
}

// ----------------------------------------------------------------- movimento
//
// NOVE CORES andam juntas, todas em OKLab: destaque, base, as tres paradas do
// degrade e as quatro luzes de regiao. Mais a forca da luz ambiente, que e um
// numero so (entra e sai com o modo imersivo).
#define CV_NCOR 9
static const float BRANCO[3] = { 1.0f, 1.0f, 1.0f };
static const float FUNDO[3]  = { 0.051f, 0.051f, 0.051f };   // NV_COR_FUNDO
float nv_cor_fundo_viva[3] = { 0.051f, 0.051f, 0.051f };
float nv_acento_viva[3] = { 1.0f, 1.0f, 1.0f };
float nv_grad_viva[3][3] = { { 1, 1, 1 }, { 1, 1, 1 }, { 1, 1, 1 } };
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
    pendLogoH = (pedidoLogoPrio == pedidoPrio) ? pedidoLogoH : 0;
  }
  pedidoPrio = 0; pedidoLogoPrio = 0;
  if (pendH && pendH != cenaH && relogio - pendDesde >= CV_ASSENTAR_MS) {
    CorvivaPaleta p;
    // Sem paleta ainda (a arte nao terminou de decodificar): espera, com a cor
    // anterior de pe. Assim que o fio de decode anotar, este teste passa.
    if (buscar(pendH, &p)) {
      cenaH = pendH; cena = p; temCena = 1;
      cenaLogoH = 0; temLogo = 0;
      travar(); sujo = 1; soltar();
    }
  }
  // O LOGO chega depois (url resolvida pela sessao, decode proprio): segue o
  // titulo em cena e e procurado todo quadro ate aparecer — uma busca de 256
  // inteiros, so enquanto falta.
  if (cenaH && cenaH == pendH && pendLogoH != cenaLogoH) {
    cenaLogoH = pendLogoH; temLogo = 0;
    travar(); sujo = 1; soltar();
  }
  if (cenaLogoH && !temLogo) temLogo = buscar(cenaLogoH, &cenaLogo);

  // A FONTE DA COR: o logo, quando o ajuste pede e ele tem cor (logo branco ou
  // preto nao tem, e ai vale a arte); senao a arte.
  fonte = (usarLogo && temLogo && cenaLogo.ok) ? &cenaLogo : &cena;
  fonteOk = modo != CORVIVA_DESLIGADA && temCena && fonte->ok;
  if (!fonteOk && modo != CORVIVA_DESLIGADA && temLogo && cenaLogo.ok) {
    fonte = &cenaLogo; fonteOk = 1;   // arte P&B com logo colorido
  }
  memcpy(novo[0], fonteOk ? fonte->acento : BRANCO, sizeof novo[0]);
  memcpy(novo[1], (fonteOk && modo >= CORVIVA_ESTILIZADA && cena.ok) ? cena.base : FUNDO, sizeof novo[1]);
  for (k = 0; k < 3; k++) memcpy(novo[2 + k], fonteOk ? fonte->grad[k] : BRANCO, sizeof novo[0]);
  for (k = 0; k < 4; k++) memcpy(novo[5 + k], temCena ? cena.regiao[k] : FUNDO, sizeof novo[0]);
  grad = fonteOk && modo >= CORVIVA_GRADIENTE;
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
}

// ------------------------------------------------------------- corviva.txt
//
// Texto e nao binario, como todo arquivo de dados deste app: da para abrir no
// ssh e ler. "leve" (dados_gravar_leve): e cache re-obtivel, e no Tizen a
// descarga para o IndexedDB pode esperar o relogio longo. Formato 2 (25/09):
// "p" + hash + ok + transparente + as nove cores; linhas do formato 1 sao
// ignoradas (e cache: a arte refaz a paleta no proximo decode).
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
    if (sscanf(p, "p %x %d %d %7s %7s %7s %7s %7s %7s %7s %7s %7s", &h, &ok, &tr,
               c[0], c[1], c[2], c[3], c[4], c[5], c[6], c[7], c[8]) == 12 && h) {
      CorvivaPaleta pal;
      int k, bom = 1;
      memset(&pal, 0, sizeof pal);
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
    if (buscar(atual, &pal)) { cenaH = pendH = atual; cena = pal; temCena = 1; }
    if (atualLogo && buscar(atualLogo, &cenaLogo)) { cenaLogoH = pendLogoH = atualLogo; temLogo = 1; }
  }
  printf("[cor] %d paleta(s) de corviva.txt%s\n", n, temCena ? ", com a ultima cena" : "");
  fflush(stdout);
}

void corviva_gravar_se_preciso(int forcar) {
  static char buf[CV_TAB * 80 + 64];
  size_t w = 0;
  int i, k;
  if (!sujo) return;
  if (!forcar && relogio - ultGravacao < 20000.0) return;
  ultGravacao = relogio;
  w += (size_t)snprintf(buf + w, sizeof buf - w, "cena %08x %08x\n", cenaH, cenaLogoH);
  travar();
  // Do mais antigo ao mais novo, para o anel voltar na mesma ordem.
  for (k = 0; k < CV_TAB; k++) {
    const Entrada *e = &tab[(tabProx + k) % CV_TAB];
    char c[9][8];
    int j;
    if (!e->h) continue;
    corHex(e->p.acento, c[0]); corHex(e->p.base, c[1]);
    for (j = 0; j < 3; j++) corHex(e->p.grad[j], c[2 + j]);
    for (j = 0; j < 4; j++) corHex(e->p.regiao[j], c[5 + j]);
    i = snprintf(buf + w, sizeof buf - w, "p %08x %d %d %s %s %s %s %s %s %s %s %s\n",
                 e->h, e->p.ok, e->p.transparente,
                 c[0], c[1], c[2], c[3], c[4], c[5], c[6], c[7], c[8]);
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
