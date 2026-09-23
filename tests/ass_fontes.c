// Queda da C9 em 22/09/2026 (#92): o app abortava em
//   ass_render.c:3069: ass_start_frame: Assertion
//   `render_priv->library->num_fontdata > render_priv->num_emfonts' failed.
// logo depois da SEGUNDA entrega de legenda do mkvass. Cada entrega fazia
// assrender_limpar_fontes (ass_clear_fonts) e anexava as fontes de novo; a
// conta do libass encolhia e o proximo quadro abortava.
//
// Aqui o src/assrender.c de verdade, com o libass do Homebrew, repete o que o
// mkvass faz a cada lote e a cada troca de faixa/episodio, e renderiza um
// quadro depois de cada passo. Com o codigo antigo este binario ABORTA no
// passo [2] (conferido antes da correcao).
#include "../src/assrender.h"
#include "../src/gfx.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// assrender.c desenha por gfx_rect; aqui nao ha GL, e nada disso e chamado.
float gfx_tex_aspect_atual;
void gfx_tex_esquecer(GLuint tex) { (void)tex; }
void gfx_rect(GfxRect r, GLuint tex, GfxModo modo, float foco,
              float parx, float pary, float raio,
              float cr, float cg, float cb, float ca) {
  (void)r; (void)tex; (void)modo; (void)foco; (void)parx; (void)pary; (void)raio;
  (void)cr; (void)cg; (void)cb; (void)ca;
}

static int falhas;
static void ok(int c, const char *o) { printf("  %-60s %s\n", o, c ? "ok" : "FALHOU"); if (!c) falhas++; }

static char *ler(const char *c, size_t *n) {
  FILE *f = fopen(c, "rb"); long t; char *b;
  if (!f) return NULL;
  fseek(f, 0, SEEK_END); t = ftell(f); fseek(f, 0, SEEK_SET);
  b = malloc((size_t)t);
  if (fread(b, 1, (size_t)t, f) != (size_t)t) { fclose(f); free(b); return NULL; }
  fclose(f); *n = (size_t)t; return b;
}

static const char *DOC =
  "[Script Info]\nScriptType: v4.00+\nPlayResX: 640\nPlayResY: 360\n\n"
  "[V4+ Styles]\nFormat: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, OutlineColour, BackColour, Bold, Italic, Underline, StrikeOut, ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow, Alignment, MarginL, MarginR, MarginV, Encoding\n"
  "Style: Default,Inter Display,28,&H00FFFFFF,&H000000FF,&H00000000,&H64000000,0,0,0,0,100,100,0,0,1,2,0,2,20,20,20,1\n\n"
  "[Events]\nFormat: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text\n"
  "Dialogue: 0,0:00:00.00,0:00:09.00,Default,,0,0,0,,Fala que precisa aparecer\n";

static const char *NOMES[3] = { "InterDisplay-Regular.ttf", "InterDisplay-Bold.ttf", "InterDisplay-Medium.ttf" };
static char *fonte[3]; static size_t tamFonte[3];
static unsigned g;

// O que mkvass.c:entregarCorpoSeAtual faz a cada lote: limpa, anexa, carrega.
static int entrega(int nFontes) {
  int i;
  assrender_geracao(++g);
  assrender_limpar_fontes();
  for (i = 0; i < nFontes; i++) assrender_adicionar_fonte(NOMES[i % 3], fonte[i % 3], tamFonte[i % 3]);
  assrender_limpar();
  if (!assrender_carregar(DOC, strlen(DOC), g)) return -2;
  return assrender_quadro_cpu(1.0);
}

int main(void) {
  int i, n;
  for (i = 0; i < 3; i++) {
    char c[256]; snprintf(c, sizeof c, "deploy/app/fonts/%s", NOMES[i]);
    fonte[i] = ler(c, &tamFonte[i]);
    if (!fonte[i]) { printf("fonte ausente: %s\n", c); return 2; }
  }
  printf("[1] primeira entrega com 3 fontes anexadas\n");
  n = entrega(3); ok(n > 0, "quadro com imagens");
  printf("[2] segunda entrega, mesmas 3 fontes (o lote seguinte do mkvass)\n");
  n = entrega(3); ok(n > 0, "quadro com imagens (antes: abortava aqui)");
  printf("[3] troca para faixa/episodio com MENOS fontes\n");
  n = entrega(1); ok(n > 0, "1 fonte: quadro sem abortar");
  n = entrega(0); ok(n > 0, "0 fontes (legenda externa): quadro sem abortar");
  printf("[4] mil lotes seguidos nao fazem a biblioteca crescer sem fim\n");
  for (i = 0; i < 1000 && n > 0; i++) n = entrega(3);
  ok(n > 0, "1000 entregas, ultimo quadro com imagens");
  printf("[5] teto: 200 fontes DIFERENTES reiniciam a biblioteca sem abortar\n");
  { char *lixo = malloc(4096);
    assrender_geracao(++g); assrender_limpar_fontes();
    for (i = 0; i < 200; i++) {
      char nome[32]; memset(lixo, i, 4096); snprintf(nome, sizeof nome, "falsa-%d.ttf", i);
      assrender_adicionar_fonte(nome, lixo, 4096);
      if (i % 50 == 49) { assrender_limpar(); assrender_carregar(DOC, strlen(DOC), g);
                          assrender_quadro_cpu(1.0); }
    }
    free(lixo); }
  n = entrega(3); ok(n > 0, "depois do reinicio, quadro com imagens");
  printf("\n%s (%d falha%s)\n", falhas ? "FALHOU" : "tudo ok", falhas, falhas == 1 ? "" : "s");
  return falhas ? 1 : 0;
}
