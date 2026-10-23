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
static float croma(const float rgb[3]) {
  float lab[3];
  corviva_srgb_para_oklab(rgb, lab);
  return sqrtf(lab[1] * lab[1] + lab[2] * lab[2]);
}
static float difMatiz(float a, float b) {
  float d = fabsf(a - b);
  while (d > 360.0f) d -= 360.0f;
  return d > 180.0f ? 360.0f - d : d;
}

// O acento Branco (#f4f2ee): sem titulo, arte cinza ou modo desligado.
static const float BRANCO[3] = { 0xf4 / 255.0f, 0xf2 / 255.0f, 0xee / 255.0f };
static const float BRANCO_PURO[3] = { 1, 1, 1 }, FUNDO[3] = { 0.051f, 0.051f, 0.051f };
static const float TINTA[3] = { 0x12 / 255.0f, 0x13 / 255.0f, 0x16 / 255.0f };

// AS TRAVAS DOS ACENTOS (03/10/2026): a tinta que corviva_tokens escolhe (a
// que contrasta mais) le a 4,5:1 ou mais no destaque e nas duas pontas do
// degrade, e as duas pontas sao da MESMA familia (a tinta nao troca no meio da
// pilula). Profundo: o branco a >= 4,6.
static void regraDeContraste(const CorvivaPaleta *p, const char *nome) {
  char m[200];
  CorvivaTokens t, t2;
  float c;
  corviva_tokens(p->acento, &t);
  c = corviva_contraste(p->acento, t.tintaBranca ? BRANCO_PURO : TINTA);
  snprintf(m, sizeof m, "%s: tinta %s sobre o destaque %.2f:1 (>= %s)", nome,
           t.tintaBranca ? "branca" : "escura", c, t.tintaBranca ? "4,6" : "4,5");
  ok(c >= (t.tintaBranca ? 4.58f : 4.5f), m);
  corviva_tokens(p->grad[2], &t2);
  c = corviva_contraste(p->grad[2], t.tintaBranca ? BRANCO_PURO : TINTA);
  snprintf(m, sizeof m, "%s: a mesma tinta na 2a parada do degrade %.2f:1", nome, c);
  ok(c >= 4.5f && t2.tintaBranca == t.tintaBranca, m);
  c = corviva_contraste(t.marca, TINTA);
  snprintf(m, sizeof m, "%s: marca sobre a ilha %.2f:1 (>= 7)", nome, c);
  ok(c >= 7.0f, m);
}

static void quadros(int n, float dt, int modo, int red, const char *chave, int prio) {
  int i;
  for (i = 0; i < n; i++) {
    if (chave) corviva_definir(chave, prio);
    corviva_quadro(dt, modo, 1, red);
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
  quadros(1, 1 / 60.0f, CORVIVA_SIMPLES, 0, NULL, 0);          // arranque: Branco
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
      corviva_quadro(1 / 60.0f, CORVIVA_SIMPLES, 1, 0);
    }
    corviva_acento(&a[0], &a[1], &a[2]);
    ok(corviva_retargets() == r0 && igual3(a, azul.acento, 0.002f), "detalhe ganha da home no mesmo quadro"); }
  // ninguem pede (Ajustes, guia): fica a ultima
  { float a[3]; quadros(120, 1 / 60.0f, CORVIVA_SIMPLES, 0, NULL, 0);
    corviva_acento(&a[0], &a[1], &a[2]);
    ok(igual3(a, azul.acento, 0.002f), "tela sem titulo: fica a cor do ultimo"); }
  // o estilizado saiu (virou o Fundo Frost): o fundo nunca e tingido
  quadros(40, 1 / 60.0f, CORVIVA_IMERSIVA, 0, NULL, 0);
  ok(igual3(nv_cor_fundo_viva, FUNDO, 0.002f), "fundo fica #0D0D0D em qualquer modo");
  quadros(40, 1 / 60.0f, CORVIVA_SIMPLES, 0, NULL, 0);
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


  // [10] PELE NAO GANHA DA CAMISA (a foto do dono, 25/09: "Prenda-me Se For
  // Capaz", fundo branco, camisa azul, rosto e bracos): mais pele que azul
  // na area, e o destaque tem de sair AZUL.
  { CorvivaPaleta pc;
    pinta(W, H, 0, 0, W, H, 236, 236, 240, 255, 4);        // fundo branco
    pinta(W, H, 100, 0, 230, 110, 226, 150, 112, 255, 10); // rosto/bracos (40%)
    pinta(W, H, 90, 110, 240, H, 90, 150, 215, 255, 10);   // camisa azul (19%)
    pinta(W, H, 60, 150, 260, 175, 220, 140, 100, 255, 8); // bracos cruzados
    ok(corviva_extrair(img, W, H, W * 4, &pc) == 1, "pele x camisa: tem cor");
    { float azulF[3] = { 90 / 255.0f, 150 / 255.0f, 215 / 255.0f };
      snprintf(m, sizeof m, "pele x camisa: destaque azul (%.0f graus da camisa)", difMatiz(matiz(pc.acento), matiz(azulF)));
      ok(difMatiz(matiz(pc.acento), matiz(azulF)) <= 20.0f, m); }
    // E pele sozinha continua dando cor (dominante de longe): um retrato sem
    // mais nada nao cai no branco.
    pinta(W, H, 0, 0, W, H, 20, 18, 16, 255, 3);
    pinta(W, H, 60, 20, 260, 170, 226, 150, 112, 255, 10);
    ok(corviva_extrair(img, W, H, W * 4, &pc) == 1, "so pele: ainda tem cor"); }

  // [11] LOGO azul-claro + azul-marinho com transparencia: e logo, e o
  // degrade tem as DUAS cores (a mais clara primeiro).
  { CorvivaPaleta pl;
    pinta(W, H, 0, 0, W, H, 0, 0, 0, 0, 0);                  // transparente
    pinta(W, H, 40, 30, 280, 80, 40, 160, 240, 255, 0);      // azul-claro
    pinta(W, H, 20, 90, 250, 140, 25, 60, 205, 255, 0);      // azul-marinho
    pinta(W, H, 90, 45, 200, 60, 255, 255, 255, 255, 0);     // letras brancas
    ok(corviva_extrair(img, W, H, W * 4, &pl) == 1 && pl.transparente, "logo: tem cor e e transparente");
    { float d = difMatiz(matiz(pl.grad[0]), matiz(pl.grad[2]));
      snprintf(m, sizeof m, "logo: as duas paradas sao analogas (%.0f graus, 20-60)", d);
      ok(d >= 19.0f && d <= 61.0f, m);
      ok(matiz(pl.grad[0]) < -60.0f && matiz(pl.grad[2]) < -60.0f, "logo: as duas paradas sao azuis");
      ok(pl.txOk && pl.tx[2] > 0.19f && pl.tx[2] < 0.21f, "logo: recorte da textura a 20% da largura"); }
    // Logo BRANCO: sem cor (cai na arte).
    pinta(W, H, 0, 0, W, H, 0, 0, 0, 0, 0);
    pinta(W, H, 40, 60, 280, 120, 250, 250, 250, 255, 0);
    ok(corviva_extrair(img, W, H, W * 4, &pl) == 0 && pl.transparente, "logo branco: sem cor"); }

  // [12] COR DA LOGO: com o ajuste ligado o destaque vem do logo; desligado,
  // da arte. O logo so vale junto do pedido de mesma prioridade.
  { CorvivaPaleta logoAzul;
    float a[3];
    pinta(W, H, 0, 0, W, H, 0, 0, 0, 0, 0);
    pinta(W, H, 40, 30, 280, 140, 25, 60, 205, 255, 0);
    corviva_extrair(img, W, H, W * 4, &logoAzul);
    corviva_zerar();
    corviva_anotar("arte/laranja", &laranja);
    corviva_anotar("logo/azul", &logoAzul);
    { int i; for (i = 0; i < 60; i++) {
        corviva_definir("arte/laranja", CORVIVA_DETALHE);
        corviva_definir_logo("logo/azul", CORVIVA_DETALHE);
        corviva_quadro(1 / 60.0f, CORVIVA_SIMPLES, 1, 0); } }
    corviva_acento(&a[0], &a[1], &a[2]);
    ok(igual3(a, logoAzul.acento, 0.002f), "cor da logo ligada: destaque do logo");
    { int i; for (i = 0; i < 40; i++) {
        corviva_definir("arte/laranja", CORVIVA_DETALHE);
        corviva_definir_logo("logo/azul", CORVIVA_DETALHE);
        corviva_quadro(1 / 60.0f, CORVIVA_SIMPLES, 0, 0); } }
    corviva_acento(&a[0], &a[1], &a[2]);
    ok(igual3(a, laranja.acento, 0.002f), "cor da logo desligada: destaque da arte");
    // Logo da HOME nao vale para o pedido do DETALHE no mesmo quadro.
    { int i; for (i = 0; i < 40; i++) {
        corviva_definir("arte/laranja", CORVIVA_DETALHE);
        corviva_definir_logo("logo/azul", CORVIVA_HOME);
        corviva_quadro(1 / 60.0f, CORVIVA_SIMPLES, 1, 0); } }
    corviva_acento(&a[0], &a[1], &a[2]);
    ok(igual3(a, laranja.acento, 0.002f), "logo de outra prioridade nao vale");

    // [13] GRADIENTE e IMERSIVA: o degrade liga, a luz entra; SIMPLES os tira.
    { int i; for (i = 0; i < 40; i++) {
        corviva_definir("arte/laranja", CORVIVA_DETALHE);
        corviva_definir_logo("logo/azul", CORVIVA_DETALHE);
        corviva_quadro(1 / 60.0f, CORVIVA_GRADIENTE, 1, 0); } }
    ok(nv_grad_ativo && igual3(nv_grad_viva[0], logoAzul.grad[0], 0.002f), "gradiente: degrade ligado, paradas do logo");
    ok(nv_ambiente_forca < 0.001f, "gradiente: sem luz ambiente");
    { int i; for (i = 0; i < 40; i++) corviva_quadro(1 / 60.0f, CORVIVA_IMERSIVA, 1, 0); }
    // (14/10) A luz e do LOGO quando "Cor da logo" vale (a mudanca de 05/10,
    // pedido do dono: "o imersivo da logo nao ta pegando a cor da logo"):
    // o teste antigo cobrava as regioes da ARTE e vivia falhando desde entao.
    { float luz[3] = { nv_ambiente_viva[0][0], nv_ambiente_viva[0][1], nv_ambiente_viva[0][2] };
      snprintf(m, sizeof m, "imersiva: luz do LOGO (%.0f graus do logo azul)",
               difMatiz(matiz(luz), matiz(logoAzul.acento)));
      ok(nv_ambiente_forca > 0.999f && difMatiz(matiz(luz), matiz(logoAzul.acento)) <= 12.0f, m); }
    { int i; for (i = 0; i < 40; i++) corviva_quadro(1 / 60.0f, CORVIVA_SIMPLES, 1, 0); }
    ok(!nv_grad_ativo && nv_ambiente_forca < 0.001f, "simples: degrade e luz desligados");
  }

  // [14] DA ARTE: amarelo vira CLARO (tinta escura), nao oliva; o resto e
  // PROFUNDO com o branco a 4,6:1; arte de croma baixo = Branco.
  { float f[3], lab[3]; CorvivaTokens t;
    const float amarelo[3] = { 0.95f, 0.80f, 0.20f }, vermelho[3] = { 0.76f, 0.03f, 0.02f },
                cinza[3] = { 0.5f, 0.5f, 0.52f };
    corviva_da_arte(amarelo, f); corviva_tokens(f, &t); corviva_srgb_para_oklab(f, lab);
    snprintf(m, sizeof m, "da arte: amarelo vira claro (L %.2f) com tinta escura %.1f:1", lab[0], corviva_contraste(f, TINTA));
    ok(!t.tintaBranca && lab[0] > 0.84f && corviva_contraste(f, TINTA) >= 4.5f, m);
    corviva_da_arte(vermelho, f); corviva_tokens(f, &t);
    snprintf(m, sizeof m, "da arte: vermelho e profundo, branco %.2f:1", corviva_contraste(f, BRANCO_PURO));
    ok(t.tintaBranca && corviva_contraste(f, BRANCO_PURO) >= 4.6f && corviva_contraste(f, BRANCO_PURO) < 5.0f, m);
    corviva_da_arte(cinza, f);
    ok(igual3(f, BRANCO, 0.002f), "da arte: arte cinza = Branco");
    // as cores reais medidas no mockup (acentos-mockup.html, ARTES): o hex
    // que o mockup mostra, a 2/255. (14/10: o Fallout mudou com a amplitude
    // nova — C 0,13 -> 0,16; os outros dois nao encostam no teto.)
    { static const struct { float raw[3]; float hex[3]; const char *n; } R[] = {
        { { 0xad/255.f, 0x65/255.f, 0x3b/255.f }, { 0xb1/255.f, 0x5e/255.f, 0x2b/255.f }, "Perdido em Marte" },
        { { 0x3a/255.f, 0x72/255.f, 0x77/255.f }, { 0x2a/255.f, 0x80/255.f, 0x87/255.f }, "O Nevoeiro" },
        { { 0xfc/255.f, 0xd0/255.f, 0x40/255.f }, { 0xf8/255.f, 0xcc/255.f, 0x3a/255.f }, "logo do Fallout" } };
      int k;
      for (k = 0; k < 3; k++) {
        corviva_da_arte(R[k].raw, f);
        snprintf(m, sizeof m, "da arte (%s): #%02x%02x%02x", R[k].n,
                 (int)(f[0] * 255 + .5f), (int)(f[1] * 255 + .5f), (int)(f[2] * 255 + .5f));
        ok(igual3(f, R[k].hex, 2.5f / 255.0f), m);
      } } }

  // [15] TEXTURA: tinta pelo pior pixel e veu so quando falta contraste.
  { int b; float v, c; const float base[3] = { 0.85f, 0.60f, 0.20f };
    corviva_textura_tinta(0.62f, 0.67f, base, 1.0f, &b, &v, &c);
    snprintf(m, sizeof m, "textura clara chapada: tinta escura sem veu (%.1f:1)", c);
    ok(!b && v < 0.001f && c >= 4.5f, m);
    corviva_textura_tinta(0.006f, 0.50f, base, 1.0f, &b, &v, &c);
    snprintf(m, sizeof m, "textura de alto contraste: veu %.0f%% ate %.1f:1", v * 100, c);
    ok(v > 0.0f && c >= 4.5f, m);
    corviva_textura_tinta(0.006f, 0.50f, base, 0.35f, &b, &v, &c);
    snprintf(m, sizeof m, "textura sutil (35%%): %.1f:1 com veu %.0f%%", c, v * 100);
    ok(c >= 4.5f, m); }

  // [16] LOGOS: o BOTAO (o destaque) casa com a cor da logo (dono, 14/10:
  // "o tom dos botoes nao esta casando muito bem com as cores da logo").
  // Cinco logos — vermelho (Stranger Things), amarelo, azul, branco e
  // multicolor. O croma e o DA LOGO (acima do teto antigo de 0,17) e o matiz
  // o dela; a regra de contraste segue valendo em todos.
  { static const struct { int rgb[3]; const char *n; } LOGOS[] = {
      { { 230, 20, 20 },  "vermelho" },
      { { 255, 230, 40 }, "amarelo" },
      { { 40, 90, 255 }, "azul" } };
    int k;
    for (k = 0; k < 3; k++) {
      CorvivaPaleta pl; float fonte[3];
      pinta(W, H, 0, 0, W, H, 0, 0, 0, 0, 0);
      pinta(W, H, 20, 40, W - 20, H - 40, LOGOS[k].rgb[0], LOGOS[k].rgb[1], LOGOS[k].rgb[2], 255, 6);
      fonte[0] = LOGOS[k].rgb[0] / 255.0f; fonte[1] = LOGOS[k].rgb[1] / 255.0f; fonte[2] = LOGOS[k].rgb[2] / 255.0f;
      snprintf(m, sizeof m, "logo %s: tem cor", LOGOS[k].n);
      ok(corviva_extrair(img, W, H, W * 4, &pl) == 1, m);
      snprintf(m, sizeof m, "logo %s: matiz do botao a %.1f graus do da logo (<= 12)",
               LOGOS[k].n, difMatiz(matiz(pl.acento), matiz(fonte)));
      ok(difMatiz(matiz(pl.acento), matiz(fonte)) <= 12.0f, m);
      snprintf(m, sizeof m, "logo %s: croma do botao %.3f (o da logo, acima do teto antigo %s)",
               LOGOS[k].n, croma(pl.acento), k == 1 ? "0,13" : "0,17");
      ok(croma(pl.acento) > (k == 1 ? 0.145f : 0.175f), m);
      regraDeContraste(&pl, LOGOS[k].n);
    }
    // BRANCO: sem cor, o botao fica no Branco padrao.
    { CorvivaPaleta pl;
      pinta(W, H, 0, 0, W, H, 0, 0, 0, 0, 0);
      pinta(W, H, 20, 40, W - 20, H - 40, 250, 250, 250, 255, 4);
      ok(corviva_extrair(img, W, H, W * 4, &pl) == 0, "logo branco: sem cor (botao Branco padrao)"); }
    // MULTICOLOR: tres faixas (vermelho, verde, azul); o destaque e a faixa
    // dominante, no matiz e no croma dela — nao uma media acinzentada.
    { CorvivaPaleta pl;
      pinta(W, H, 0, 0, W, H, 0, 0, 0, 0, 0);
      pinta(W, H, 0, 40, W / 3, H - 40, 230, 20, 20, 255, 6);
      pinta(W, H, W / 3, 40, 2 * W / 3, H - 40, 20, 220, 40, 255, 6);
      pinta(W, H, 2 * W / 3, 40, W, H - 40, 40, 90, 255, 255, 6);
      ok(corviva_extrair(img, W, H, W * 4, &pl) == 1, "logo multicolor: tem cor");
      { static const float F[3][3] = { { 230 / 255.0f, 20 / 255.0f, 20 / 255.0f },
                                     { 20 / 255.0f, 220 / 255.0f, 40 / 255.0f },
                                     { 40 / 255.0f, 90 / 255.0f, 255 / 255.0f } };
        float d[3]; int i, melhor = 0;
        for (i = 0; i < 3; i++) { d[i] = difMatiz(matiz(pl.acento), matiz(F[i])); if (d[i] < d[melhor]) melhor = i; }
        snprintf(m, sizeof m, "logo multicolor: uma das tres cores (%.0f graus), nao media", d[melhor]);
        ok(d[melhor] <= 12.0f, m);
        snprintf(m, sizeof m, "logo multicolor: croma %.3f acima do teto antigo 0,17", croma(pl.acento));
        ok(croma(pl.acento) > 0.175f, m);
        regraDeContraste(&pl, "multicolor"); } } }

  // [17] TEXTURA FRACA: recorte CHAPADO e fraco (ajustes o troca pelo
  // material CC0 embutido); com grao, fica o recorte da arte. A conta e a
  // mesma do tema: p90 - p10 de luminancia do recorte abaixo de 0,12.
  { CorvivaPaleta pf, pg;
    int y;
    pinta(W, H, 0, 0, W, H, 220, 30, 30, 255, 0);                       // chapado
    ok(corviva_extrair(img, W, H, W * 4, &pf) == 1, "fraca: chapada tem cor");
    for (y = 0; y < H; y += 18) pinta(W, H, 0, y, W, y + 18 < H ? y + 18 : H,
                                      (y / 18) & 1 ? 255 : 120, (y / 18) & 1 ? 80 : 10,
                                      (y / 18) & 1 ? 80 : 10, 255, 4);
    ok(corviva_extrair(img, W, H, W * 4, &pg) == 1, "fraca: grao tem cor");
    corviva_zerar();
    corviva_anotar("arte/chapada", &pf);
    corviva_anotar("arte/graada", &pg);
    quadros(40, 1 / 60.0f, CORVIVA_TEXTURA, 0, "arte/chapada", CORVIVA_HOME);
    ok(nv_textura_viva.ok && nv_textura_viva.fraca,
       "recorte chapado: fraca (troca pelo material CC0)");
    quadros(40, 1 / 60.0f, CORVIVA_TEXTURA, 0, "arte/graada", CORVIVA_HOME);
    ok(nv_textura_viva.ok && !nv_textura_viva.fraca,
       "recorte com grao: fica o material da arte"); }

  // [9] corviva.txt: o arranque seguinte ja nasce com a cor da ultima cena.
  corviva_zerar();
  corviva_anotar("https://arte/azul", &azul);
  quadros(40, 1 / 60.0f, CORVIVA_SIMPLES, 0, "https://arte/azul", CORVIVA_HOME);
  corviva_gravar_se_preciso(1);
  ok(arquivo && strstr(arquivo, "cena ") != NULL, "corviva.txt gravado");
  corviva_zerar();
  corviva_carregar();
  quadros(1, 1 / 60.0f, CORVIVA_TEXTURA, 0, NULL, 0);
  { float a[3]; corviva_acento(&a[0], &a[1], &a[2]);
    ok(igual3(a, azul.acento, 1.0f / 255.0f + 0.001f), "arranque: primeiro quadro ja com a cor da ultima cena");
    ok(nv_textura_viva.ok && !strcmp(nv_textura_viva.url, "https://arte/azul") &&
       fabsf(nv_textura_viva.janela[2] - azul.tx[2]) < 0.001f,
       "arranque: a textura (url e recorte) tambem volta do arquivo");
    { CorvivaPaleta lida; (void)lida; }
    quadros(1, 1 / 60.0f, CORVIVA_GRADIENTE, 1, NULL, 0);
    ok(igual3(nv_grad_viva[0], azul.grad[0], 1.0f / 255.0f + 0.001f), "arranque: o degrade tambem volta do arquivo"); }

  free(arquivo);
  if (falhas) { printf("%d falha(s)\n", falhas); return 1; }
  printf("tudo ok\n");
  return 0;
}
