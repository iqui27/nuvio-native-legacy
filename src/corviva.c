// Cor viva: o destaque (e, no estilizado, o fundo) seguindo a arte do titulo
// em cena. O porque de cada peca esta em corviva.h; aqui ficam as medidas.
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
  for (k = 0; k < 40; k++) {
    float lab[3] = { L, C * cosf(h), C * sinf(h) }, l3[3];
    linDeLab(lab, l3);
    if ((l3[0] >= -0.001f && l3[0] <= 1.001f && l3[1] >= -0.001f &&
         l3[1] <= 1.001f && l3[2] >= -0.001f && l3[2] <= 1.001f) || C <= 0.0f) {
      rgb[0] = srgbDeLin(l3[0]); rgb[1] = srgbDeLin(l3[1]); rgb[2] = srgbDeLin(l3[2]);
      return;
    }
    C -= 0.006f;
    if (C < 0.0f) C = 0.0f;
  }
  { float lab[3] = { L, 0, 0 }; corviva_oklab_para_srgb(lab, rgb); }
}

// O DESTAQUE TEM DE CABER NA REGRA QUE JA EXISTE, e nao ganhar uma regra
// propria. A tinta sobre o realce e BRANCA sempre que o realce nao e branco
// (ajustes_acento_tinta, regra do dono de 21/09/2026), entao a cor extraida
// precisa segurar texto branco: contraste >= 4,5:1 exige luminancia relativa
// <= 0,183 — em OKLab, L perto de 0,57. E do outro lado ela e anel de foco
// sobre #0D0D0D, e um anel abaixo de 3:1 some: isso pede L >= ~0,50.
//
// Por isso a faixa e estreita de proposito: L em [0,50; 0,57], croma em
// [0,12; 0,16]. O croma minimo tira do destaque o bege e o cinza-azulado
// (uma arte dessaturada ainda da uma cor que se le como COR); o maximo segura o
// neon — um vermelho de cartaz a croma 0,25 na TV vira um alarme.
#define CV_L_MIN 0.50f
#define CV_L_MAX 0.57f
#define CV_C_MIN 0.12f
#define CV_C_MAX 0.16f
static void ajustarAcento(const float bruto[3], float saida[3]) {
  static const float branco[3] = { 1, 1, 1 };
  float lab[3], L, C, h;
  int k;
  corviva_srgb_para_oklab(bruto, lab);
  L = lab[0]; C = sqrtf(lab[1] * lab[1] + lab[2] * lab[2]); h = atan2f(lab[2], lab[1]);
  if (L < CV_L_MIN) L = CV_L_MIN;
  if (L > CV_L_MAX) L = CV_L_MAX;
  // O balde carrega junto o tom apagado do mesmo matiz (a areia e o sofa
  // laranja caem no mesmo balde), e a media dele sai mais cinza que a cor que
  // o olho ve na arte: +25% de croma devolve o que a media tirou.
  C *= 1.25f;
  if (C < CV_C_MIN) C = CV_C_MIN;
  if (C > CV_C_MAX) C = CV_C_MAX;
  lchParaSrgb(L, C, h, saida);
  // O gamute pode ter cortado croma e a conta de L em OKLab e aproximada para
  // a luminancia WCAG: confere a regra de verdade e escurece ate ela valer.
  for (k = 0; k < 20 && corviva_contraste(saida, branco) < 4.6f; k++) {
    L -= 0.01f;
    lchParaSrgb(L, C, h, saida);
  }
}

// A BASE do estilizado: o #0D0D0D (L ~0,16) com um sopro do matiz da arte.
// Croma no maximo 0,032 e L 0,175: e o bastante para a tela inteira ler
// "azul-noite" ou "vinho" ao lado de outra, e pouco para mudar o contraste dos
// cinzas que foram calibrados um a um contra #0D0D0D (ver TEMA_ACENTO em
// ajustes.c) — o cinza #8A8A8A cai de 5,5:1 para 5,1:1, ainda folgado.
#define CV_BASE_L     0.175f
#define CV_BASE_C_MAX 0.032f
static void ajustarBase(const float bruto[3], float saida[3]) {
  float lab[3], C, h;
  corviva_srgb_para_oklab(bruto, lab);
  C = sqrtf(lab[1] * lab[1] + lab[2] * lab[2]);
  h = atan2f(lab[2], lab[1]);
  C *= 0.35f;
  if (C > CV_BASE_C_MAX) C = CV_BASE_C_MAX;
  lchParaSrgb(CV_BASE_L, C, h, saida);
}

// ------------------------------------------------------------------ extracao
//
// GRADE DE 32x18 PONTOS, e nao a imagem inteira: a textura ja chega reduzida
// (um destaque de 1280x720 tem 920 mil pixels), e 576 amostras dizem qual e a
// cor dominante tao bem quanto elas. Ponto e nao media de area: a media num
// bloco borra a borda entre duas cores e cria um terceiro tom que nao existe
// na arte.
//
// QUANTIZACAO POR MATIZ em 24 baldes de 15 graus, e nao k-means: o k-means
// precisa de varias passadas sobre as amostras e de sementes, e o que se quer
// aqui nao e "as k cores da arte", e UMA cor que valha como destaque. Cada
// amostra cromatica pesa pelo CROMA AO QUADRADO (quem e mais colorido vota
// muito mais; ver a nota no laco) vezes a
// proximidade de uma luminosidade media (um azul quase preto de sombra nao e o
// azul do cartaz). O balde vencedor soma meio voto de cada vizinho, para uma
// cor que caiu na divisa entre dois baldes nao perder para uma menor inteira.
//
// Fora: pixel transparente (logo recortado), quase preto (v < 0,14), cinza
// (croma < 0,10) — o que tira tambem o quase branco. Sobra menos de 4% de
// amostras cromaticas: a arte e P&B e a resposta e "sem cor" (ok = 0), que
// quem usa troca pelo destaque padrao.
//
// CUSTO MEDIDO: 10 a 16 us por arte no Mac (as 30 artes de deploy/app/art,
// 1280x720 e 1600x900 — a grade e fixa, entao o tamanho quase nao pesa), e
// sem alocacao. Na TV roda no fio de decode, em prioridade baixa, uma vez por
// arte de tela cheia; o log `[cor] extracao: N us` da a medida do aparelho.
#define CV_GX 32
#define CV_GY 18
#define CV_NB 24
int corviva_extrair(const unsigned char *px, int w, int h, int pitch,
                    CorvivaPaleta *p) {
  float bw[CV_NB], br[CV_NB], bgc[CV_NB], bb[CV_NB];
  int bn[CV_NB];
  int gx, gy, i, j, total = 0, crom = 0, melhor = 0, povo = 0;
  float melhorSc = -1.0f;
  if (!p) return 0;
  p->ok = 0;
  p->acento[0] = p->acento[1] = p->acento[2] = 1.0f;
  p->base[0] = p->base[1] = p->base[2] = 0.051f;
  if (!px || w <= 0 || h <= 0 || pitch < w * 4) return 0;
  memset(bw, 0, sizeof bw); memset(br, 0, sizeof br);
  memset(bgc, 0, sizeof bgc); memset(bb, 0, sizeof bb); memset(bn, 0, sizeof bn);
  gx = w < CV_GX ? w : CV_GX;
  gy = h < CV_GY ? h : CV_GY;
  for (j = 0; j < gy; j++) {
    const unsigned char *ln = px + (size_t)((2 * j + 1) * h / (2 * gy)) * (size_t)pitch;
    for (i = 0; i < gx; i++) {
      const unsigned char *q = ln + (size_t)((2 * i + 1) * w / (2 * gx)) * 4;
      float r, g, b, mx, mn, c, v, hue, wt;
      int k;
      if (q[3] < 200) continue;
      total++;
      r = q[0] * (1.0f / 255.0f); g = q[1] * (1.0f / 255.0f); b = q[2] * (1.0f / 255.0f);
      mx = r > g ? (r > b ? r : b) : (g > b ? g : b);
      mn = r < g ? (r < b ? r : b) : (g < b ? g : b);
      c = mx - mn; v = mx;
      if (v < 0.14f || c < 0.10f) continue;
      if (mx == r)      hue = (g - b) / c;           // -1..1
      else if (mx == g) hue = (b - r) / c + 2.0f;    //  1..3
      else              hue = (r - g) / c + 4.0f;    //  3..5
      if (hue < 0.0f) hue += 6.0f;
      k = (int)(hue * (CV_NB / 6.0f));
      if (k < 0) k = 0;
      if (k >= CV_NB) k = CV_NB - 1;
      // CROMA AO QUADRADO: com o croma linear, a areia e a pele de um cartaz
      // (croma ~0,25, mas metade da imagem) ganhavam do sofa laranja (croma
      // ~0,6, um decimo dela) e o destaque saia bege-marrom — MEDIDO nas 30
      // artes de deploy/app/art, 11 davam um tom de pele. Ao quadrado, o
      // colorido de verdade vence a menos que seja pouco demais.
      wt = c * c * (1.0f - 1.1f * fabsf(v - 0.62f));
      if (wt < 0.005f) wt = 0.005f;
      bw[k] += wt; br[k] += r * wt; bgc[k] += g * wt; bb[k] += b * wt;
      bn[k]++;
      crom++;
    }
  }
  if (total < 8 || crom * 25 < total) return 0;   // < 4% cromatico: P&B
  for (i = 0; i < CV_NB; i++) {
    float sc = bw[i] + 0.5f * (bw[(i + CV_NB - 1) % CV_NB] + bw[(i + 1) % CV_NB]);
    if (sc > melhorSc) { melhorSc = sc; melhor = i; }
    if (bn[i] > bn[povo]) povo = i;
  }
  // Um balde que vence com peso minusculo e ruido (tres pixels de um logo
  // colorido num cartaz P&B): pede ao menos 0,25% do peso possivel (o peso e
  // croma ao quadrado, entao isso e ~1% da area em cor forte, ou ~7% em cor
  // apagada, como o verde de mata de deploy/app/art/28.jpg).
  if (melhorSc < 0.0025f * (float)total) return 0;
  {
    int a = (melhor + CV_NB - 1) % CV_NB, z = (melhor + 1) % CV_NB;
    float W = bw[melhor] + 0.5f * (bw[a] + bw[z]);
    float bruto[3];
    bruto[0] = (br[melhor]  + 0.5f * (br[a]  + br[z]))  / W;
    bruto[1] = (bgc[melhor] + 0.5f * (bgc[a] + bgc[z])) / W;
    bruto[2] = (bb[melhor]  + 0.5f * (bb[a]  + bb[z]))  / W;
    ajustarAcento(bruto, p->acento);
    // A BASE sai do balde mais POPULOSO, nao do mais pesado: o destaque e a
    // cor que salta da arte, o fundo e a que a arte mais TEM (o azul do ceu
    // atras do casaco vermelho). Quando os dois coincidem, melhor ainda.
    bruto[0] = br[povo] / bw[povo];
    bruto[1] = bgc[povo] / bw[povo];
    bruto[2] = bb[povo] / bw[povo];
    // Croma da base proporcional a quanto da arte e colorida: meio cartaz
    // cinza com um canto azul tinge menos que um cartaz todo azul.
    { float f = (float)crom / (float)total, lab[3], cc, hh, k2;
      corviva_srgb_para_oklab(bruto, lab);
      cc = sqrtf(lab[1] * lab[1] + lab[2] * lab[2]);
      hh = atan2f(lab[2], lab[1]);
      k2 = f < 0.5f ? f * 2.0f : 1.0f;
      lab[1] = cc * k2 * cosf(hh); lab[2] = cc * k2 * sinf(hh);
      corviva_oklab_para_srgb(lab, bruto); }
    ajustarBase(bruto, p->base);
  }
  p->ok = 1;
  return 1;
}

// -------------------------------------------------------------- memoria
//
// 256 artes, anel simples (a mais antiga sai). Chave = FNV-1a da url. So as
// artes de TELA CHEIA passam por aqui (tex_cache.c anota so o que foi pedido
// no teto do destaque), entao 256 e horas de navegacao.
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
  travar();
  for (i = 0; i < CV_TAB; i++) if (tab[i].h == h) { *p = tab[i].p; achou = 1; break; }
  soltar();
  return achou;
}

// ------------------------------------------------------------- quem manda
static unsigned int pedidoH;      // o pedido de maior prioridade DESTE quadro
static int pedidoPrio;
static unsigned int pendH;        // o que esta pedido, esperando assentar
static double pendDesde;
static unsigned int cenaH;        // o titulo cuja cor vale agora
static CorvivaPaleta cena;
static int temCena;
static double relogio;            // ms, somando os dt do laco
static double ultGravacao = -1e9;

// 150 ms parado antes de trocar: quem rola o destaque depressa passa por um
// titulo a cada ~120 ms (repeticao de tecla da TV), e cada um deles mudaria a
// cor da tela inteira. O destaque da home ja espera o foco assentar para
// trocar a ARTE; isto segura a COR ate a arte nova ter ficado.
#define CV_ASSENTAR_MS 150.0
// 450 ms, saida suave (cubica). O crossfade do destaque leva ~400 ms: a cor
// chega junto com a arte, nem antes nem muito depois.
#define CV_DURACAO_S   0.45f

void corviva_definir(const char *chave, int prioridade) {
  if (!chave || !chave[0] || prioridade <= pedidoPrio) return;
  pedidoH = hashDe(chave);
  pedidoPrio = prioridade;
}

// ----------------------------------------------------------------- movimento
static const float BRANCO[3] = { 1.0f, 1.0f, 1.0f };
static const float FUNDO[3]  = { 0.051f, 0.051f, 0.051f };   // NV_COR_FUNDO
static float corA[3] = { 1.0f, 1.0f, 1.0f };
float nv_cor_fundo_viva[3] = { 0.051f, 0.051f, 0.051f };
static float deA[3], deB[3], paraA[3], paraB[3];   // OKLab
static float alvoA[3] = { 1, 1, 1 }, alvoB[3] = { 0.051f, 0.051f, 0.051f };   // sRGB
static float t = 1.0f;
static int iniciado, retargets;

void corviva_acento(float *r, float *g, float *b) {
  if (r) *r = corA[0];
  if (g) *g = corA[1];
  if (b) *b = corA[2];
}
int corviva_retargets(void) { return retargets; }

void corviva_quadro(float dt, int modo, int reduzido) {
  const float *nA, *nB;
  if (dt < 0.0f) dt = 0.0f;
  relogio += (double)dt * 1000.0;

  // O pedido do quadro que ACABOU de ser desenhado vira o pendente.
  if (pedidoPrio) {
    if (pedidoH != pendH) { pendH = pedidoH; pendDesde = relogio; }
    pedidoPrio = 0;
  }
  if (pendH && pendH != cenaH && relogio - pendDesde >= CV_ASSENTAR_MS) {
    CorvivaPaleta p;
    // Sem paleta ainda (a arte nao terminou de decodificar): espera, com a cor
    // anterior de pe. Assim que o fio de decode anotar, este teste passa.
    if (buscar(pendH, &p)) {
      cenaH = pendH; cena = p; temCena = 1;
      travar(); sujo = 1; soltar();
    }
  }

  if (modo != CORVIVA_DESLIGADA && temCena && cena.ok) {
    nA = cena.acento;
    nB = modo == CORVIVA_ESTILIZADA ? cena.base : FUNDO;
  } else {
    nA = BRANCO;
    nB = FUNDO;
  }
  if (memcmp(nA, alvoA, sizeof alvoA) || memcmp(nB, alvoB, sizeof alvoB) || !iniciado) {
    float cur[3];
    memcpy(alvoA, nA, sizeof alvoA); memcpy(alvoB, nB, sizeof alvoB);
    corviva_srgb_para_oklab(corA, cur);            memcpy(deA, cur, sizeof cur);
    corviva_srgb_para_oklab(nv_cor_fundo_viva, cur); memcpy(deB, cur, sizeof cur);
    corviva_srgb_para_oklab(alvoA, paraA);
    corviva_srgb_para_oklab(alvoB, paraB);
    // Primeiro quadro (cor do arranque) e animacoes reduzidas: sem transicao.
    t = (!iniciado || reduzido) ? 1.0f : 0.0f;
    if (iniciado) retargets++;
    iniciado = 1;
    if (t >= 1.0f) {
      memcpy(corA, alvoA, sizeof corA);
      memcpy(nv_cor_fundo_viva, alvoB, sizeof nv_cor_fundo_viva);
    }
  }
  if (t < 1.0f) {
    float e, u, la[3], lb[3];
    int k;
    t += dt / CV_DURACAO_S;
    if (reduzido || t > 1.0f) t = 1.0f;
    u = 1.0f - t;
    e = 1.0f - u * u * u;
    for (k = 0; k < 3; k++) {
      la[k] = deA[k] + (paraA[k] - deA[k]) * e;
      lb[k] = deB[k] + (paraB[k] - deB[k]) * e;
    }
    if (t >= 1.0f) {
      memcpy(corA, alvoA, sizeof corA);
      memcpy(nv_cor_fundo_viva, alvoB, sizeof nv_cor_fundo_viva);
    } else {
      corviva_oklab_para_srgb(la, corA);
      corviva_oklab_para_srgb(lb, nv_cor_fundo_viva);
    }
  }
}

// ------------------------------------------------------------- corviva.txt
//
// Texto e nao binario, como todo arquivo de dados deste app: da para abrir no
// ssh e ler. "leve" (dados_gravar_leve): e cache re-obtivel, e no Tizen a
// descarga para o IndexedDB pode esperar o relogio longo.
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
  unsigned int atual = 0;
  int n = 0;
  if (!txt) return;
  for (p = txt; *p; p = fim) {
    char a[16], b[16];
    unsigned h; int ok;
    fim = strchr(p, '\n');
    if (!fim) fim = p + strlen(p); else *fim++ = 0;
    if (sscanf(p, "cena %x", &h) == 1) { atual = h; continue; }
    if (sscanf(p, "%x %d %15s %15s", &h, &ok, a, b) == 4 && h) {
      CorvivaPaleta pal;
      pal.ok = ok ? 1 : 0;
      if (!lerHex(a, pal.acento) || !lerHex(b, pal.base)) continue;
      travar(); guardar(h, &pal); soltar();
      n++;
    }
  }
  free(txt);
  if (atual) {
    CorvivaPaleta pal;
    if (buscar(atual, &pal)) { cenaH = pendH = atual; cena = pal; temCena = 1; }
  }
  printf("[cor] %d paleta(s) de corviva.txt%s\n", n, temCena ? ", com a ultima cena" : "");
  fflush(stdout);
}

void corviva_gravar_se_preciso(int forcar) {
  static char buf[CV_TAB * 28 + 64];
  size_t w = 0;
  int i, k;
  if (!sujo) return;
  if (!forcar && relogio - ultGravacao < 20000.0) return;
  ultGravacao = relogio;
  w += (size_t)snprintf(buf + w, sizeof buf - w, "cena %08x\n", cenaH);
  travar();
  // Do mais antigo ao mais novo, para o anel voltar na mesma ordem.
  for (k = 0; k < CV_TAB; k++) {
    const Entrada *e = &tab[(tabProx + k) % CV_TAB];
    char a[8], b[8];
    if (!e->h) continue;
    corHex(e->p.acento, a); corHex(e->p.base, b);
    i = snprintf(buf + w, sizeof buf - w, "%08x %d %s %s\n", e->h, e->p.ok, a, b);
    if (i < 0 || (size_t)i >= sizeof buf - w) break;
    w += (size_t)i;
  }
  sujo = 0;
  soltar();
  dados_gravar_leve("corviva.txt", buf);
}

void corviva_zerar(void) {
  travar();
  memset(tab, 0, sizeof tab); tabProx = 0; sujo = 0;
  soltar();
  pedidoH = pendH = cenaH = 0; pedidoPrio = 0; temCena = 0;
  relogio = 0; pendDesde = 0; ultGravacao = -1e9;
  memcpy(corA, BRANCO, sizeof corA);
  memcpy(nv_cor_fundo_viva, FUNDO, sizeof nv_cor_fundo_viva);
  memcpy(alvoA, BRANCO, sizeof alvoA); memcpy(alvoB, FUNDO, sizeof alvoB);
  t = 1.0f; iniciado = 0; retargets = 0;
}
