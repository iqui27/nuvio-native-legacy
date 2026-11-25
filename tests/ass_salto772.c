// #772 (travamentos, artefatos e saltos de 1-2 s em legendas ASS, Samsung/Tizen).
// Compila e chama o codigo de verdade — substitui o antigo grep do
// tests/ass_salto772.sh, que nao rodava nada.
//
//  [A] src/relogio.c — o relogio da legenda (#92):
//      saltos de +2, +5, +50 e -57 s a 1x, 1,5x e 2x: a leitura segue a
//      posicao nova e NUNCA volta depois; um engasgo >= 0,25 s sem nova
//      posicao do pipeline nao pode regredir r->saida; dois Relogios
//      independentes nao dividem estado.
//      Em 06fb62b32, relogio_salto_ass_passo() chamava relogio_ler(r, ultM)
//      sobre o instante PASSADO: o efeito colateral r->saida = v regravava
//      a saida para tras em qualquer engasgo >= 0,25 s (visto no probe:
//      saida 10,200 -> 10,000), e o `static double ultM` era dividido por
//      todas as instancias de Relogio.
//  [B] src/assrender.c — so quando ha libass (NV_ASS_LIBASS): o salto visto
//      no numero do VIDEO (assrender_posicao_video) derruba o quadro e o
//      pedido do instante velho mesmo com o relogio da legenda parado — o
//      caso da reconexao e da busca em voo, em que posLeg nao anda.

#include "../src/relogio.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

static int falhas;
static void ok(int c, const char *o) { printf("%s %s\n", c ? "ok   " : "FALHA", o); if (!c) falhas++; }

static void testar_salto(double taxa, double salto, const char *nome) {
  Relogio r; double pos, t, v, ant, max;
  char msg[160];
  srand(772);
  relogio_zerar(&r);
  relogio_taxa(&r, taxa);
  { double p0 = 0.0, e = -1.0, prox = 0.0;
    for (t = 0.0; t < 4.0; t += 1.0 / 60.0) {
      if (t >= prox) { p0 = 100.0 + prox * taxa; e = t + (rand() % 48) / 1000.0; prox = t + 0.2; }
      if (e >= 0.0 && t >= e) { relogio_amostra(&r, p0, t, 1); e = -1.0; }
      else relogio_amostra(&r, r.temAmostra ? r.ultPos : 100.0, t, 1);
    } }
  pos = 100.0 + 4.0 * taxa;
  // A amostra seguinte chega com a posicao depois do salto (seek, reconexao,
  // descontinuidade de PTS): o relogio pula junto e fica certo no instante.
  ant = r.saida;
  relogio_amostra(&r, pos + salto, t, 1);
  v = relogio_ler(&r, t);
  snprintf(msg, sizeof msg, "salto %s a %.1fx: leitura segue a posicao nova", nome, taxa);
  ok(fabs(v - (pos + salto)) < 0.01, msg);
  snprintf(msg, sizeof msg, "salto %s a %.1fx: sem recuo na troca", nome, taxa);
  ok(v >= ant || salto < 0.0, msg);
  // E segue interpolando a partir dal, sem degrau para tras.
  max = v;
  { double alvo = pos + salto, t2; int recuos = 0;
    double e2 = -1.0, prox2 = t + 0.2, p2 = alvo + 0.2 * taxa;
    for (t2 = t + 1.0 / 60.0; t2 < t + 3.0; t2 += 1.0 / 60.0) {
      double v2;
      if (t2 >= prox2) { p2 = alvo + (prox2 - t) * taxa; e2 = t2 + 0.03; prox2 = t2 + 0.2; }
      if (e2 >= 0.0 && t2 >= e2) { relogio_amostra(&r, p2, t2, 1); e2 = -1.0; }
      else relogio_amostra(&r, r.ultPos, t2, 1);
      v2 = relogio_ler(&r, t2);
      if (v2 < max - 1e-9) recuos++;
      if (v2 > max) max = v2;
    }
    snprintf(msg, sizeof msg, "salto %s a %.1fx: monotonico depois do pouso", nome, taxa);
    ok(recuos == 0, msg); }
}

static void testar_engasgo(double taxa) {
  Relogio r; double t = 0.0, prox = 0.0, pos = 200.0, entrega = -1.0, saida0;
  char msg[160];
  srand(773);
  relogio_zerar(&r);
  relogio_taxa(&r, taxa);
  for (t = 0.0; t < 5.0; t += 1.0 / 60.0) {
    if (t >= prox) { pos = 200.0 + prox * taxa; entrega = t + 0.02; prox = t + 0.2; }
    if (entrega >= 0.0 && t >= entrega) { relogio_amostra(&r, pos, t, 1); entrega = -1.0; }
    else relogio_amostra(&r, r.temAmostra ? r.ultPos : 200.0, t, 1);
    relogio_ler(&r, t);
  }
  // Engasgo: a leitura chega ao teto (saida sobe sem nova posicao do
  // pipeline) e o proximo quadro chega 0,55 s depois — a amostra seguinte
  // com a MESMA posicao nao pode regredir a saida. Em 06fb62b32 o
  // salto_ass_passo relia o relogio no instante velho ultM e regravava
  // r->saida no valor velho (10,2 -> 10,0 no probe). A regressao mora na
  // amostra: conferir r.saida ANTES de qualquer ler, que poderia repara-la.
  saida0 = relogio_ler(&r, t + 0.5);
  { double p = r.ultPos, t2, min;
    relogio_amostra(&r, p, t + 0.55, 1);
    snprintf(msg, sizeof msg, "engasgo 0,55 s a %.1fx: saida nao volta (%.3f >= %.3f)", taxa, r.saida, saida0);
    ok(r.saida >= saida0 - 1e-9, msg);
    // E nos 2 s seguintes, quadro a quadro com a posicao parada, idem.
    min = r.saida;
    for (t2 = t + 0.55 + 1.0 / 60.0; t2 < t + 2.55; t2 += 1.0 / 60.0) {
      relogio_amostra(&r, p, t2, 1);
      if (r.saida < min - 1e-9) min = r.saida;
      relogio_ler(&r, t2);
    }
    snprintf(msg, sizeof msg, "engasgo 2 s a %.1fx: monotonico quadro a quadro (min %.3f)", taxa, min);
    ok(min >= saida0 - 1e-9, msg); }
}

static void testar_dois_relogios(void) {
  Relogio a, b; double t; int recA = 0, recB = 0;
  double maxA = -1e300, maxB = -1e300;
  relogio_zerar(&a); relogio_zerar(&b);
  // Duas instancias com linhas do tempo e posicoes proprias, alternadas —
  // o `static ultM` de 06fb62b32 vazava o instante de uma na outra.
  for (t = 0.0; t < 3.0; t += 0.2) {
    double va, vb;
    relogio_amostra(&a, 300.0 + t, 50.0 + t, 1);  va = relogio_ler(&a, 50.0 + t);
    relogio_amostra(&b, 800.0 + t, 500.0 + t, 1); vb = relogio_ler(&b, 500.0 + t);
    if (va < maxA - 1e-9) recA++; if (va > maxA) maxA = va;
    if (vb < maxB - 1e-9) recB++; if (vb > maxB) maxB = vb;
    // Engasgo alternado nas duas (>= 0,25 s sem nova posicao).
    relogio_amostra(&a, a.ultPos, 50.0 + t + 0.4, 1); va = relogio_ler(&a, 50.0 + t + 0.4);
    relogio_amostra(&b, b.ultPos, 500.0 + t + 0.4, 1); vb = relogio_ler(&b, 500.0 + t + 0.4);
    if (va < maxA - 1e-9) recA++; if (va > maxA) maxA = va;
    if (vb < maxB - 1e-9) recB++; if (vb > maxB) maxB = vb;
  }
  ok(recA == 0 && recB == 0, "dois Relogios alternados: nenhuma saida regrediu");
}

#ifdef NV_ASS_LIBASS
/* Mesmo GL de mentira de tests/ass_pisca.c (ver tests/ass_pisca_gl.h). */
#include "../src/assrender.h"
#include "../src/gfx.h"
#include <string.h>
#include <time.h>
#include <unistd.h>

static long uploads, gerados;
void teste_glGenTextures(GLsizei n, GLuint *t) { static GLuint prox = 1; int i; for (i = 0; i < n; i++) t[i] = prox++; gerados += n; }
void teste_glDeleteTextures(GLsizei n, const GLuint *t) { (void)n; (void)t; }
void teste_glBindTexture(GLenum a, GLuint t) { (void)a; (void)t; }
void teste_glTexImage2D(GLenum a, GLint b, GLint c, GLsizei w, GLsizei h, GLint d, GLenum e, GLenum f, const void *p) {
  (void)a; (void)b; (void)c; (void)w; (void)h; (void)d; (void)e; (void)f; (void)p; uploads++; }
void teste_glTexParameteri(GLenum a, GLenum b, GLint c) { (void)a; (void)b; (void)c; }
float gfx_tex_aspect_atual;
void gfx_tex_esquecer(GLuint tex) { (void)tex; }
void gfx_rect(GfxRect r, GLuint tex, GfxModo modo, float foco, float raio, float borda,
              float bordaA, float cr, float cg, float cb, float a) {
  (void)r; (void)tex; (void)modo; (void)foco; (void)raio; (void)borda; (void)bordaA;
  (void)cr; (void)cg; (void)cb; (void)a; }

#define CAB "[Script Info]\nScriptType: v4.00+\nPlayResX: 640\nPlayResY: 360\n\n" \
  "[V4+ Styles]\nFormat: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, OutlineColour, BackColour, Bold, Italic, Underline, StrikeOut, ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow, Alignment, MarginL, MarginR, MarginV, Encoding\n" \
  "Style: Default,Inter Display,28,&H00FFFFFF,&H000000FF,&H00000000,&H64000000,0,0,0,0,100,100,0,0,1,2,0,2,20,20,20,1\n\n" \
  "[Events]\nFormat: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text\n"
static const char *DOC = CAB
  "Dialogue: 0,0:00:01.00,0:00:03.00,Default,,0,0,0,,Fala do inicio\n"
  "Dialogue: 0,0:00:20.00,0:00:22.00,Default,,0,0,0,,Fala dos vinte segundos\n";

static char *ler(const char *c, size_t *n) {
  FILE *f = fopen(c, "rb"); char *b; long t;
  if (!f) return NULL;
  fseek(f, 0, SEEK_END); t = ftell(f); fseek(f, 0, SEEK_SET);
  b = malloc((size_t)t); if (b && fread(b, (size_t)t, 1, f) != 1) { free(b); b = NULL; }
  fclose(f); *n = (size_t)t; return b;
}

static void quadro(void) { usleep(16667); }

static void testar_salto_ass(void) {
  size_t tf; char *fonte = ler("deploy/app/fonts/InterDisplay-Regular.ttf", &tf);
  unsigned g = 7;
  int k, n;
  if (!fonte) { printf("fonte ausente\n"); falhas++; return; }
  assrender_geracao(g); assrender_limpar_fontes();
  assrender_adicionar_fonte("InterDisplay-Regular.ttf", fonte, tf);
  free(fonte); /* adicionar_fonte copia os bytes */
  ok(assrender_carregar(DOC, strlen(DOC), g) == 1, "documento carregado");

  // O player informa a posicao do VIDEO a cada quadro; a primeira so registra.
  assrender_posicao_video(21.0);
  for (k = 0, n = 0; k < 60 && n <= 0; k++) {
    quadro();
    n = assrender_desenhar(21.0 + k / 60.0, 0, 1.0f, 0, 0, 1920, 1080);
  }
  ok(n > 0, "fala de 20-22 s na tela antes do salto");

  // O VIDEO pula para 2,0 s (seek, reconexao, PTS descontinuo) e o relogio
  // da legenda AINDA mostra 21 s — a busca em voo/reconexao segurou a
  // leitura. O estado ASS sai pelo numero do video.
  assrender_posicao_video(2.0);
  n = assrender_desenhar(21.0, 0, 1.0f, 0, 0, 1920, 1080);
  ok(n == 0, "quadro de 21 s some no salto do VIDEO mesmo com o relogio parado");

  // Um passo comum (0,3 s) NAO e salto: a fala segue visivel, sem apagar.
  { unsigned g2 = g + 1;
    assrender_geracao(g2); assrender_limpar_fontes(); assrender_carregar(DOC, strlen(DOC), g2);
    for (k = 0, n = 0; k < 60 && n <= 0; k++) {
      quadro();
      assrender_posicao_video(20.0 + k / 60.0);
      n = assrender_desenhar(20.5 + k / 60.0, 0, 1.0f, 0, 0, 1920, 1080); }
    assrender_posicao_video(20.3);
    n = assrender_desenhar(20.5, 0, 1.0f, 0, 0, 1920, 1080);
    ok(n > 0, "passo de 0,3 s no video nao e salto (fala segue visivel)"); }

  // Na posicao nova a fala certa entra.
  for (k = 0, n = 0; k < 60 && n <= 0; k++) {
    quadro();
    assrender_posicao_video(2.0 + k / 60.0);
    n = assrender_desenhar(2.0 + k / 60.0, 0, 1.0f, 0, 0, 1920, 1080); }
  ok(n > 0, "fala de 1-3 s aparece na posicao nova");
}

/* [C] A sonda do revisor (evidencias/sol-f64r2/probe-ass772.c): o video
 * salta e o RELOGIO da legenda fica parado na origem por 24 quadros — a
 * busca em voo do TPK segura posLeg. A fala da cena de origem NAO pode
 * voltar em nenhum desses quadros; a do destino entra quando o relogio
 * alcanca. 1x e 2x, ida e volta, ASS e SRT via plainass_document. */
static int desenhar_rel(Relogio *r, double mono) {
  return assrender_desenhar(relogio_ler(r, mono), 0, 1.0f, 0, 0, 1920, 1080);
}
static int esperar_fala_rel(Relogio *r, double mono) {
  int q;
  for (q = 0; q < 90; q++) {
    int n = desenhar_rel(r, mono);
    if (n > 0) return n;
    quadro();
  }
  return 0;
}
static void verificar_salto_parado(double taxa, int sentido, unsigned g, int srt) {
  const LegendaCue cues[2] = {
    { .inicio = 1, .fim = 3, .texto = "كلام البداية" },
    { .inicio = 20, .fim = 22, .texto = "كلام المشهد التالي" },
  };
  double anterior = sentido > 0 ? 2.0 : 21.0;
  double destino = sentido > 0 ? 21.0 : 2.0;
  double passo = 0.15 * taxa;
  size_t tf; char *fonte;
  Relogio relogio;
  int antes, passoNormal, imediato, velhos = 0, novo, q;
  char msg[224];
  const char *rotulo = srt ? "SRT/plainass" : "ASS";
  fonte = ler("deploy/app/fonts/InterDisplay-Regular.ttf", &tf);
  if (!fonte) { printf("fonte ausente\n"); falhas++; return; }
  assrender_geracao(g); assrender_limpar_fontes();
  assrender_adicionar_fonte("InterDisplay-Regular.ttf", fonte, tf);
  free(fonte);
  if (srt) {
    PlainAssStyle estilo; char *doc;
    fonte = ler("deploy/app/fonts/NotoNaskhArabic-Regular.ttf", &tf);
    if (!fonte) { printf("fonte ausente\n"); falhas++; return; }
    assrender_adicionar_fonte("NotoNaskhArabic-Regular.ttf", fonte, tf);
    free(fonte);
    memset(&estilo, 0, sizeof estilo);
    estilo.size = 28; estilo.rgb = 0xffffff; estilo.marginV = 20;
    snprintf(estilo.font, sizeof estilo.font, "Noto Naskh Arabic");
    doc = plainass_document(cues, 2);
    if (!doc || !assrender_carregar_texto(doc, strlen(doc), g)) {
      free(doc); ok(0, "documento SRT/plainass carregado"); return; }
    free(doc);
    assrender_definir_texto_estilo(&estilo);
  } else if (!assrender_carregar(DOC, strlen(DOC), g)) {
    ok(0, "documento ASS carregado"); return;
  }

  relogio_zerar(&relogio); relogio_taxa(&relogio, taxa);
  relogio_amostra(&relogio, anterior - passo, 99.85, 1);
  assrender_posicao_video(anterior - passo);
  relogio_amostra(&relogio, anterior, 100, 1);
  assrender_posicao_video(anterior);
  antes = esperar_fala_rel(&relogio, 100);
  snprintf(msg, sizeof msg, "%s %.1fx %+.0fs: fala visivel antes do salto",
           rotulo, taxa, destino - anterior);
  ok(antes > 0, msg);

  // Passo comum: a fala tem de estar na tela (0,15 s a 1x, 0,30 s a 2x).
  assrender_posicao_video(anterior + passo);
  passoNormal = desenhar_rel(&relogio, 100);
  snprintf(msg, sizeof msg, "%s %.1fx %+.0fs: passo comum de %.2f s mantem imagens",
           rotulo, taxa, destino - anterior, passo);
  ok(passoNormal > 0, msg);

  // O video salta; o relogio da legenda segue PARADO na origem 24 quadros.
  assrender_posicao_video(destino);
  imediato = desenhar_rel(&relogio, 100);
  snprintf(msg, sizeof msg, "%s %.1fx %+.0fs: fala da origem some no quadro do salto",
           rotulo, taxa, destino - anterior);
  ok(imediato == 0, msg);
  for (q = 1; q <= 24; q++) {
    quadro();
    assrender_posicao_video(destino);
    relogio_amostra(&relogio, anterior, 100 + q / 60.0, 0);
    if (desenhar_rel(&relogio, 100 + q / 60.0) > 0) velhos++;
  }
  snprintf(msg, sizeof msg, "%s %.1fx %+.0fs: zero fala antiga em 24 quadros de relogio parado",
           rotulo, taxa, destino - anterior);
  ok(velhos == 0, msg);

  relogio_amostra(&relogio, destino, 100.5, 0);
  novo = esperar_fala_rel(&relogio, 100.5);
  snprintf(msg, sizeof msg, "%s %.1fx %+.0fs: fala do destino entra quando o relogio alcanca",
           rotulo, taxa, destino - anterior);
  ok(novo > 0, msg);
}
static void testar_salto_parado(int srt) {
  static const double taxas[2] = { 1.0, 2.0 };
  static unsigned g = 100;
  int t, s;
  for (t = 0; t < 2; t++)
    for (s = -1; s <= 1; s += 2)
      verificar_salto_parado(taxas[t], s, ++g, srt);
}
#endif

int main(void) {
  static const double taxas[] = { 1.0, 1.5, 2.0 };
  int i;
  printf("[A] relogio: saltos do pipeline, engasgo, instancias\n");
  for (i = 0; i < 3; i++) {
    testar_salto(taxas[i], +2.0, "+2 s");
    testar_salto(taxas[i], +5.0, "+5 s");
    testar_salto(taxas[i], +50.0, "+50 s");
    testar_salto(taxas[i], -57.0, "-57 s");
    testar_engasgo(taxas[i]);
  }
  testar_dois_relogios();
#ifdef NV_ASS_LIBASS
  /* SRT/plainass PRIMEIRO: neste ambiente, renderizar uma faixa authored e
   * DEPOIS uma plainass no mesmo renderer derruba o libass 0.17.1 em
   * fribidi_get_bracket_types — reproduzido tambem na base c18c21756, fora
   * do escopo do #772. Com a trilha simples primeiro a sequencia e limpa. */
  printf("[B] assrender+plainass: 24 quadros com o relogio parado (SRT)\n");
  testar_salto_parado(1);
  printf("[C] assrender: salto visto na posicao do video\n");
  testar_salto_ass();
  printf("[D] assrender: 24 quadros com o relogio parado (ASS)\n");
  testar_salto_parado(0);
#else
  printf("[B] sem libass: parte do renderer pulada\n");
#endif
  printf("\n%s (%d falha%s)\n", falhas ? "FALHOU" : "ass_salto772: ok", falhas, falhas == 1 ? "" : "s");
  return falhas ? 1 : 0;
}
