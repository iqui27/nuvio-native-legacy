// Cor viva (src/corviva.c): extracao em imagens sinteticas, regra de contraste,
// debounce, prioridade, interpolacao e o corviva.txt. So o modulo e este
// arquivo — dados.c e trocado pelos dois stubs abaixo, em memoria.
#include "../src/corviva.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int falhas;
static void ok(int c, const char *o) { printf("%s %s\n", c ? "ok   " : "FALHA", o); if (!c) falhas++; }

// --- dados.c em memoria
static char *arquivo;
char *dados_ler(const char *nome) {
  (void)nome;
  if (!arquivo) return NULL;
  { char *c = malloc(strlen(arquivo) + 1); strcpy(c, arquivo); return c; }
}
int dados_gravar_leve(const char *nome, const char *conteudo) {
  (void)nome;
  free(arquivo);
  arquivo = malloc(strlen(conteudo) + 1);
  strcpy(arquivo, conteudo);
  return 1;
}

// --- imagens
static unsigned char img[1280 * 720 * 4];
static unsigned rnd = 12345;
static int aleat(int n) { rnd = rnd * 1103515245u + 12345u; return (int)((rnd >> 16) % (unsigned)n); }
static void pinta(int w, int h, int x0, int y0, int x1, int y1, int r, int g, int b, int a, int ruido) {
  int x, y;
  for (y = y0; y < y1; y++) for (x = x0; x < x1; x++) {
    unsigned char *q = img + ((size_t)y * w + x) * 4;
    int d = ruido ? aleat(2 * ruido + 1) - ruido : 0;
    int v[3] = { r + d, g + d, b + d }, k;
    for (k = 0; k < 3; k++) q[k] = (unsigned char)(v[k] < 0 ? 0 : v[k] > 255 ? 255 : v[k]);
    q[3] = (unsigned char)a;
  }
  (void)h;
}
static float matiz(const float rgb[3]) {
  float lab[3];
  corviva_srgb_para_oklab(rgb, lab);
  return atan2f(lab[2], lab[1]) * 57.29578f;
}
static float difMatiz(float a, float b) {
  float d = fabsf(a - b);
  while (d > 360.0f) d -= 360.0f;
  return d > 180.0f ? 360.0f - d : d;
}

static const float BRANCO[3] = { 1, 1, 1 }, FUNDO[3] = { 0.051f, 0.051f, 0.051f };

static void regraDeContraste(const CorvivaPaleta *p, const char *nome) {
  char m[160];
  float cb = corviva_contraste(p->acento, BRANCO), cf = corviva_contraste(p->acento, FUNDO);
  float lumGama = 0.2126f * p->acento[0] + 0.7152f * p->acento[1] + 0.0722f * p->acento[2];
  snprintf(m, sizeof m, "%s: texto branco sobre o destaque %.2f:1 (>= 4,5)", nome, cb);
  ok(cb >= 4.5f, m);
  snprintf(m, sizeof m, "%s: destaque sobre #0D0D0D %.2f:1 (>= 3, anel de foco)", nome, cf);
  ok(cf >= 3.0f, m);
  // ajustes_acento_tinta: tinta escura so acima de 0,88 — o destaque tem de
  // cair no ramo da tinta BRANCA, senao o 4,5:1 acima nao e o que aparece.
  snprintf(m, sizeof m, "%s: cai no ramo da tinta branca (lum %.2f <= 0,88)", nome, lumGama);
  ok(lumGama <= 0.88f, m);
  { float c = corviva_contraste(p->base, FUNDO);
    snprintf(m, sizeof m, "%s: base tingida perto do #0D0D0D (%.2f:1 <= 1,25)", nome, c);
    ok(c <= 1.25f, m); }
}

static void quadros(int n, float dt, int modo, int red, const char *chave, int prio) {
  int i;
  for (i = 0; i < n; i++) {
    if (chave) corviva_definir(chave, prio);
    corviva_quadro(dt, modo, red);
  }
}
static int igual3(const float a[3], const float b[3], float tol) {
  return fabsf(a[0] - b[0]) <= tol && fabsf(a[1] - b[1]) <= tol && fabsf(a[2] - b[2]) <= tol;
}

int main(void) {
  CorvivaPaleta azul, laranja, p;
  const int W = 320, H = 180;
  char m[200];

  // [1] quase toda azul, com sombras escuras e uma faixa cinza: sai azul.
  pinta(W, H, 0, 0, W, H, 30, 80, 180, 255, 12);
  pinta(W, H, 0, 0, W, 30, 10, 10, 14, 255, 4);
  pinta(W, H, 0, 150, W, H, 128, 128, 128, 255, 6);
  ok(corviva_extrair(img, W, H, W * 4, &azul) == 1, "azul: tem cor");
  snprintf(m, sizeof m, "azul: destaque azulado (%.2f %.2f %.2f)", azul.acento[0], azul.acento[1], azul.acento[2]);
  ok(azul.acento[2] > azul.acento[0] && azul.acento[2] > azul.acento[1], m);
  { float fonte[3] = { 30 / 255.0f, 80 / 255.0f, 180 / 255.0f };
    snprintf(m, sizeof m, "azul: matiz a %.1f graus do da arte (<= 12)", difMatiz(matiz(azul.acento), matiz(fonte)));
    ok(difMatiz(matiz(azul.acento), matiz(fonte)) <= 12.0f, m); }
  ok(azul.base[2] >= azul.base[0], "azul: a base puxa para o azul");
  regraDeContraste(&azul, "azul");

  // [2] tons de cinza: sem cor, cai no padrao.
  { int x; for (x = 0; x < W; x++) pinta(W, H, x, 0, x + 1, H, x * 255 / W, x * 255 / W, x * 255 / W, 255, 3); }
  ok(corviva_extrair(img, W, H, W * 4, &p) == 0 && !p.ok, "cinza: sem cor (usa o padrao)");
  ok(igual3(p.acento, BRANCO, 0.001f), "cinza: devolve o branco padrao");

  // [3] P&B com um logo vermelho de 1% da area: continua sem cor.
  pinta(W, H, 0, 0, W, H, 20, 20, 20, 255, 5);
  pinta(W, H, 0, 90, W, H, 230, 230, 230, 255, 5);
  pinta(W, H, 10, 10, 42, 28, 220, 30, 30, 255, 0);
  ok(corviva_extrair(img, W, H, W * 4, &p) == 0, "P&B com logo pequeno: sem cor");

  // [4] transparente (logo recortado): sem cor.
  pinta(W, H, 0, 0, W, H, 30, 80, 180, 0, 0);
  ok(corviva_extrair(img, W, H, W * 4, &p) == 0, "transparente: sem cor");

  // [5] cartaz laranja com ceu azul dessaturado: o destaque e o saturado.
  pinta(W, H, 0, 0, W, H, 90, 110, 130, 255, 8);        // ceu cinza-azulado (60%)
  pinta(W, H, 0, 110, W, H, 235, 120, 20, 255, 10);     // laranja (40%)
  ok(corviva_extrair(img, W, H, W * 4, &laranja) == 1, "laranja: tem cor");
  { float fonte[3] = { 235 / 255.0f, 120 / 255.0f, 20 / 255.0f };
    snprintf(m, sizeof m, "laranja: o destaque e o laranja saturado (%.1f graus)", difMatiz(matiz(laranja.acento), matiz(fonte)));
    ok(difMatiz(matiz(laranja.acento), matiz(fonte)) <= 15.0f, m); }
  regraDeContraste(&laranja, "laranja");

  // [6] cada matiz, claro e escuro: a regra de contraste vale para todos.
  { static const int cores[][3] = {
      { 255, 40, 40 }, { 255, 150, 0 }, { 255, 230, 40 }, { 60, 220, 60 },
      { 40, 230, 230 }, { 60, 90, 255 }, { 160, 60, 230 }, { 255, 80, 180 },
      { 90, 20, 20 }, { 20, 60, 30 }, { 255, 200, 200 }, { 200, 255, 220 } };
    int i;
    for (i = 0; i < (int)(sizeof cores / sizeof *cores); i++) {
      char nome[40];
      pinta(W, H, 0, 0, W, H, cores[i][0], cores[i][1], cores[i][2], 255, 6);
      snprintf(nome, sizeof nome, "#%02x%02x%02x", cores[i][0], cores[i][1], cores[i][2]);
      if (!corviva_extrair(img, W, H, W * 4, &p)) {
        snprintf(m, sizeof m, "%s: tem cor", nome); ok(0, m); continue;
      }
      regraDeContraste(&p, nome);
    }
  }

  // [7] custo: 1280x720, a textura de destaque da LG em modo Desempenho.
  { int i, n = 2000; clock_t c0;
    double us;
    pinta(1280, 720, 0, 0, 1280, 720, 30, 80, 180, 255, 20);
    c0 = clock();
    for (i = 0; i < n; i++) corviva_extrair(img, 1280, 720, 1280 * 4, &p);
    us = (double)(clock() - c0) * 1e6 / CLOCKS_PER_SEC / n;
    snprintf(m, sizeof m, "extracao 1280x720: %.1f us por arte no Mac (teto 500)", us);
    ok(us < 500.0, m); }

  // [8] movimento: debounce, prioridade, chegada, OKLab.
  corviva_zerar();
  corviva_anotar("https://arte/azul", &azul);
  corviva_anotar("https://arte/laranja", &laranja);
  quadros(1, 1 / 60.0f, CORVIVA_SIMPLES, 0, NULL, 0);          // arranque: branco
  { float a[3]; corviva_acento(&a[0], &a[1], &a[2]);
    ok(igual3(a, BRANCO, 0.001f), "sem titulo: destaque padrao (branco)"); }
  // alternando a cada 100 ms (rolagem rapida): nenhuma troca
  { int i;
    for (i = 0; i < 10; i++) quadros(6, 1 / 60.0f, CORVIVA_SIMPLES, 0,
                                     (i & 1) ? "https://arte/azul" : "https://arte/laranja", CORVIVA_HOME);
    ok(corviva_retargets() == 0, "rolagem rapida (100 ms por titulo): nenhuma troca de cor"); }
  // assentou no azul: troca, e em 150 + 450 ms chega
  quadros(9, 1 / 60.0f, CORVIVA_SIMPLES, 0, "https://arte/azul", CORVIVA_HOME);   // 150 ms
  ok(corviva_retargets() == 1, "150 ms parado: uma troca");
  { float a[3], lab0[3], labA[3], d0, d1;
    corviva_srgb_para_oklab(BRANCO, lab0);
    corviva_srgb_para_oklab(azul.acento, labA);
    quadros(13, 1 / 60.0f, CORVIVA_SIMPLES, 0, "https://arte/azul", CORVIVA_HOME);  // ~225 ms
    corviva_acento(&a[0], &a[1], &a[2]);
    { float l[3]; corviva_srgb_para_oklab(a, l);
      d0 = sqrtf((l[0]-lab0[0])*(l[0]-lab0[0]) + (l[1]-lab0[1])*(l[1]-lab0[1]) + (l[2]-lab0[2])*(l[2]-lab0[2]));
      d1 = sqrtf((l[0]-labA[0])*(l[0]-labA[0]) + (l[1]-labA[1])*(l[1]-labA[1]) + (l[2]-labA[2])*(l[2]-labA[2])); }
    // Saida suave: na metade do tempo ja percorreu bem mais que metade.
    snprintf(m, sizeof m, "metade do tempo: mais perto do alvo (%.3f) que da origem (%.3f)", d1, d0);
    ok(d1 < d0 && d0 > 0.02f, m);
    quadros(20, 1 / 60.0f, CORVIVA_SIMPLES, 0, "https://arte/azul", CORVIVA_HOME);
    corviva_acento(&a[0], &a[1], &a[2]);
    ok(igual3(a, azul.acento, 0.002f), "450 ms depois: exatamente o destaque do azul"); }
  // prioridade: no mesmo quadro a home pede laranja e o detalhe pede azul
  { int r0 = corviva_retargets(), i; float a[3];
    for (i = 0; i < 60; i++) {
      corviva_definir("https://arte/laranja", CORVIVA_HOME);
      corviva_definir("https://arte/azul", CORVIVA_DETALHE);
      corviva_quadro(1 / 60.0f, CORVIVA_SIMPLES, 0);
    }
    corviva_acento(&a[0], &a[1], &a[2]);
    ok(corviva_retargets() == r0 && igual3(a, azul.acento, 0.002f), "detalhe ganha da home no mesmo quadro"); }
  // ninguem pede (Ajustes, guia): fica a ultima
  { float a[3]; quadros(120, 1 / 60.0f, CORVIVA_SIMPLES, 0, NULL, 0);
    corviva_acento(&a[0], &a[1], &a[2]);
    ok(igual3(a, azul.acento, 0.002f), "tela sem titulo: fica a cor do ultimo"); }
  // estilizado: o fundo passa a ser a base; simples: volta ao #0D0D0D
  quadros(40, 1 / 60.0f, CORVIVA_ESTILIZADA, 0, NULL, 0);
  ok(igual3(nv_cor_fundo_viva, azul.base, 0.002f), "estilizado: fundo = base tingida da arte");
  quadros(40, 1 / 60.0f, CORVIVA_SIMPLES, 0, NULL, 0);
  ok(igual3(nv_cor_fundo_viva, FUNDO, 0.002f), "simples: fundo volta a #0D0D0D");
  // animacoes reduzidas: troca no primeiro quadro depois do assentar
  { float a[3];
    quadros(10, 1 / 60.0f, CORVIVA_SIMPLES, 1, "https://arte/laranja", CORVIVA_PLAYER);
    corviva_acento(&a[0], &a[1], &a[2]);
    ok(igual3(a, laranja.acento, 0.002f), "animacoes reduzidas: sem transicao"); }
  // desligado: branco
  { float a[3]; quadros(40, 1 / 60.0f, CORVIVA_DESLIGADA, 0, NULL, 0);
    corviva_acento(&a[0], &a[1], &a[2]);
    ok(igual3(a, BRANCO, 0.002f), "desligado: branco"); }
  // arte ainda sem paleta: espera, sem trocar
  { int r0 = corviva_retargets();
    quadros(60, 1 / 60.0f, CORVIVA_SIMPLES, 0, "https://arte/nao-decodificada", CORVIVA_HOME);
    ok(corviva_retargets() == r0 + 1, "arte sem paleta ainda: mantem a anterior (so a volta do desligado conta)");
    { float a[3]; corviva_acento(&a[0], &a[1], &a[2]);
      ok(igual3(a, laranja.acento, 0.002f), "...e a anterior e a do laranja"); }
    corviva_anotar("https://arte/nao-decodificada", &azul);   // o decode chegou
    quadros(40, 1 / 60.0f, CORVIVA_SIMPLES, 0, "https://arte/nao-decodificada", CORVIVA_HOME);
    { float a[3]; corviva_acento(&a[0], &a[1], &a[2]);
      ok(igual3(a, azul.acento, 0.002f), "o decode chegou: troca sozinha"); } }

  // [9] corviva.txt: o arranque seguinte ja nasce com a cor da ultima cena.
  corviva_gravar_se_preciso(1);
  ok(arquivo && strstr(arquivo, "cena ") != NULL, "corviva.txt gravado");
  corviva_zerar();
  corviva_carregar();
  quadros(1, 1 / 60.0f, CORVIVA_ESTILIZADA, 0, NULL, 0);
  { float a[3]; corviva_acento(&a[0], &a[1], &a[2]);
    ok(igual3(a, azul.acento, 1.0f / 255.0f + 0.001f), "arranque: primeiro quadro ja com a cor da ultima cena");
    ok(igual3(nv_cor_fundo_viva, azul.base, 1.0f / 255.0f + 0.001f), "arranque: e com a base dela"); }

  free(arquivo);
  if (falhas) { printf("%d falha(s)\n", falhas); return 1; }
  printf("tudo ok\n");
  return 0;
}
