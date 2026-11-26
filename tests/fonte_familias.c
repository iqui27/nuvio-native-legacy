// Fontes sob demanda (src/fontefamilias.c, 16/11/2026): os nomes da tabela
// 'name' (os mesmos que o fontselect do libass usa), as familias que um
// documento ASS pede e a decisao de entregar ou nao cada fonte anexada.
// Nomes esperados conferidos com o fontTools nas fontes de deploy/app/fonts.
#include "../src/fontefamilias.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int falhas;
static void ok(int c, const char *o) { printf("%s %s\n", c ? "ok   " : "FALHA", o); if (!c) falhas++; }

static unsigned char *ler(const char *c, size_t *n) {
  FILE *f = fopen(c, "rb"); unsigned char *b; long t;
  if (!f) return NULL;
  fseek(f, 0, SEEK_END); t = ftell(f); fseek(f, 0, SEEK_SET);
  b = malloc((size_t)t); if (b && fread(b, 1, (size_t)t, f) != (size_t)t) { free(b); b = NULL; }
  fclose(f); *n = (size_t)t; return b;
}

typedef struct { char v[32][128]; int n; } Nomes;
static void junta(const char *nome, void *u) { Nomes *s = u; if (s->n < 32) snprintf(s->v[s->n++], 128, "%s", nome); }
static int tem(const Nomes *s, const char *n) { int i; for (i = 0; i < s->n; i++) if (!strcmp(s->v[i], n)) return 1; return 0; }

/* Uma colecao .ttc com duas fontes: cabecalho ttcf e os deslocamentos das
 * tabelas de cada uma somados ao lugar dela no arquivo. */
static unsigned char *ttc(const unsigned char *a, size_t na, const unsigned char *b, size_t nb, size_t *n) {
  size_t oa = 20, ob = (oa + na + 3) & ~(size_t)3, i, k;
  unsigned char *t = calloc(ob + nb, 1);
  const unsigned char *src[2] = { a, b }; size_t off[2] = { oa, ob }, tam[2] = { na, nb };
  memcpy(t, "ttcf", 4); t[5] = 1; t[11] = 2;
  for (k = 0; k < 2; k++) {
    unsigned char *f = t + off[k]; unsigned nt;
    t[12 + k * 4] = (unsigned char)(off[k] >> 24); t[13 + k * 4] = (unsigned char)(off[k] >> 16);
    t[14 + k * 4] = (unsigned char)(off[k] >> 8); t[15 + k * 4] = (unsigned char)off[k];
    memcpy(f, src[k], tam[k]);
    nt = (unsigned)f[4] << 8 | f[5];
    for (i = 0; i < nt; i++) {
      unsigned char *r = f + 12 + i * 16 + 8;
      unsigned long o = (unsigned long)r[0] << 24 | (unsigned long)r[1] << 16 | (unsigned long)r[2] << 8 | r[3];
      o += off[k];
      r[0] = (unsigned char)(o >> 24); r[1] = (unsigned char)(o >> 16); r[2] = (unsigned char)(o >> 8); r[3] = (unsigned char)o;
    }
  }
  *n = ob + nb;
  return t;
}

int main(void) {
  size_t nInter, nMed, nAtk, nJet, nDroid, nRob, nC;
  unsigned char *inter = ler("deploy/app/fonts/InterDisplay-Regular.ttf", &nInter);
  unsigned char *med = ler("deploy/app/fonts/InterDisplay-Medium.ttf", &nMed);
  unsigned char *atk = ler("deploy/app/fonts/AtkinsonHyperlegibleNext-Regular.ttf", &nAtk);
  unsigned char *jet = ler("deploy/app/fonts/JetBrainsMonoNL-Regular.ttf", &nJet);
  unsigned char *droid = ler("deploy/app/fonts/DroidSansFallback-Subset.ttf", &nDroid);
  unsigned char *rob = ler("deploy/app/fonts/Roboto-Bold.ttf", &nRob), *col;
  FonteFamilias s = { 0 };
  if (!inter || !med || !atk || !jet || !droid || !rob) { printf("fontes ausentes\n"); return 2; }

  printf("[1] nomes da tabela 'name'\n");
  { Nomes a = { .n = 0 }; fontefam_nomes(inter, nInter, junta, &a);
    ok(tem(&a, "Inter Display") && tem(&a, "Inter Display Regular") && tem(&a, "InterDisplay-Regular"),
       "Inter Display Regular: familia, nome completo e PostScript"); }
  { Nomes a = { .n = 0 }; fontefam_nomes(med, nMed, junta, &a);
    ok(tem(&a, "Inter Display Medium") && tem(&a, "Inter Display"), "Inter Display Medium: familia (1) e familia tipografica (16)"); }
  { Nomes a = { .n = 0 }; unsigned char lixo[64] = "nao e fonte nenhuma";
    ok(fontefam_nomes(lixo, sizeof lixo, junta, &a) == 0 && fontefam_nomes(inter, 10, junta, &a) == 0, "arquivo que nao e fonte (ou cortado): 0 nomes"); }
  col = ttc(jet, nJet, rob, nRob, &nC);
  { Nomes a = { .n = 0 }; fontefam_nomes(col, nC, junta, &a);
    ok(tem(&a, "JetBrains Mono NL") && tem(&a, "Roboto") && tem(&a, "Roboto-Bold"), "colecao .ttc: as duas faces"); }

  printf("[2] familias pedidas pelo documento\n");
  { const char *doc =
      "[Script Info]\nScriptType: v4.00+\n\n[V4+ Styles]\n"
      "Format: Name, Fontname, Fontsize, PrimaryColour\n"
      "Style: Default,Inter Display,56,&H00FFFFFF\n"
      "Style: Vertical,@Atkinson Hyperlegible Next ,40,&H00FFFFFF\n\n"
      "[Events]\nFormat: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text\n"
      "Dialogue: 0,0:00:01.00,0:00:02.00,Default,,0,0,0,,Fala {\\fnROBOTO\\b1}placa{\\fn} volta\n";
    fontefam_varrer(&s, doc, strlen(doc));
    ok(s.n == 3 && fontefam_tem(&s, "inter display") && fontefam_tem(&s, "Atkinson Hyperlegible Next") && fontefam_tem(&s, "roboto"),
       "Fontname dos estilos (sem '@' e espacos) e \\fn dos eventos, sem diferenca de maiusculas"); }
  { FonteFamilias t = { 0 };
    const char *ordem = "[V4+ Styles]\nFormat: Name, Fontsize, Fontname\nStyle: X,40,Fonte Na Terceira\n[Events]\nDialogue: 0,a,b,X,,0,0,0,,{\\fs20}sem fn\n";
    fontefam_varrer(&t, ordem, strlen(ordem));
    ok(t.n == 1 && fontefam_tem(&t, "fonte na terceira"), "a coluna Fontname vem do Format, nao da posicao fixa");
    fontefam_liberar(&t); }

  printf("[3] o que vai ao libass\n");
  ok(fontefam_quer(&s, inter, nInter), "Inter Display Regular: pedida pelo estilo");
  ok(fontefam_quer(&s, med, nMed), "Inter Display Medium: entra pela familia tipografica (o libass escolhe o peso)");
  ok(fontefam_quer(&s, atk, nAtk), "Atkinson: pedida com '@' (vertical)");
  ok(fontefam_quer(&s, rob, nRob), "Roboto: pedida so por \\fn num evento");
  ok(!fontefam_quer(&s, jet, nJet) && !fontefam_quer(&s, droid, nDroid), "JetBrains e Droid: ninguem pede, ficam fora");
  ok(fontefam_quer(&s, col, nC), "colecao com uma face pedida (Roboto): entra");
  { unsigned char lixo[64] = "OTTO e depois nada que preste";
    ok(fontefam_quer(&s, lixo, sizeof lixo), "tabela 'name' ilegivel: entrega (na duvida, como antes)"); }
  { const char *lote = "Dialogue: 0,0:00:09.00,0:00:10.00,Default,,0,0,0,,{\\fnJetBrains Mono NL}codigo\n";
    fontefam_varrer(&s, lote, strlen(lote));
    ok(fontefam_quer(&s, jet, nJet), "um lote novo com \\fn passa a pedir a fonte"); }

  fontefam_liberar(&s); free(col);
  free(inter); free(med); free(atk); free(jet); free(droid); free(rob);
  printf("\n%s (%d falha%s)\n", falhas ? "FALHOU" : "fonte_familias: ok", falhas, falhas == 1 ? "" : "s");
  return falhas ? 1 : 0;
}
