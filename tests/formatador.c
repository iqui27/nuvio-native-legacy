// Icones dos formatadores (formatador.c): cada sequencia de
// tools/icones-formatador.tsv vira o icone certo, com e sem VS16, no meio de
// texto; o que a interface ja desenha fica texto; bandeiras; tom de pele; ZWJ.
//   bash tests/formatador.sh
#include "formatador.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int pecas(const char *s, char *out, size_t cap, size_t *k) {
  *k = 0;
  return formatador_pecas(s, strlen(s), out, cap, k, NULL);
}
// "T<txt>\0" / "Iddd" / "B<cc>\0" -> forma legivel "[txt]{ddd}<cc>"
static void legivel(const char *o, size_t k, char *d, size_t tam) {
  size_t i = 0, w = 0;
  d[0] = 0;
  while (i < k) {
    if (o[i] == 'T') { w += snprintf(d + w, tam - w, "[%s]", o + i + 1); i += strlen(o + i + 1) + 2; }
    else if (o[i] == 'I') { w += snprintf(d + w, tam - w, "{%.3s}", o + i + 1); i += 4; }
    else if (o[i] == 'B') { w += snprintf(d + w, tam - w, "<%s>", o + i + 1); i += strlen(o + i + 1) + 2; }
    else if (o[i] == 'S') { w += snprintf(d + w, tam - w, "(S"); i++; }
    else if (o[i] == 'M') { w += snprintf(d + w, tam - w, "|M"); i++; }
    else if (o[i] == 'E') { w += snprintf(d + w, tam - w, ")"); i++; }
    else { fprintf(stderr, "peca invalida em %zu\n", i); exit(1); }
  }
}
static void utf8(unsigned long c, char **w) {
  unsigned char *p = (unsigned char *)*w;
  if (c < 0x80) *p++ = (unsigned char)c;
  else if (c < 0x800) { *p++ = 0xC0 | c >> 6; *p++ = 0x80 | (c & 63); }
  else if (c < 0x10000) { *p++ = 0xE0 | c >> 12; *p++ = 0x80 | (c >> 6 & 63); *p++ = 0x80 | (c & 63); }
  else { *p++ = 0xF0 | c >> 18; *p++ = 0x80 | (c >> 12 & 63); *p++ = 0x80 | (c >> 6 & 63); *p++ = 0x80 | (c & 63); }
  *w = (char *)p;
}
static int confere(const char *s, const char *esperado) {
  char o[1024], d[1024];
  size_t k;
  pecas(s, o, sizeof o, &k);
  legivel(o, k, d, sizeof d);
  if (strcmp(d, esperado)) { printf("FALHA: %s -> %s (esperado %s)\n", s, d, esperado); return 0; }
  return 1;
}
static int existe(const char *cc) { return strcmp(cc, "ZZ") != 0; }

int main(int argc, char **argv) {
  int ok = 1, n = 0, nTexto = 0;
  char linha[512];
  FILE *f = fopen(argc > 1 ? argv[1] : "tools/icones-formatador.tsv", "r");
  assert(f);
  while (fgets(linha, sizeof linha, f)) {
    char seq[128], tipo[16], s1[64], s2[64], esp[64], *w;
    char *p;
    if (linha[0] == '#' || sscanf(linha, "%127s %15s", seq, tipo) != 2) continue;
    // a<seq>b e a<seq com FE0F depois do primeiro codepoint>b
    for (int vs = 0; vs < 2; vs++) {
      char buf[64]; w = buf; *w++ = 'a';
      int primeiro = 1;
      for (p = strtok(strcpy(s1, seq), "-"); p; p = strtok(NULL, "-")) {
        utf8(strtoul(p, NULL, 16), &w);
        if (vs && primeiro) utf8(0xFE0F, &w);
        primeiro = 0;
      }
      *w++ = 'b'; *w = 0;
      if (!strcmp(tipo, "texto")) {
        char o[256]; size_t k;
        pecas(buf, o, sizeof o, &k);
        if (o[0] != 'T' || k != strlen(o + 1) + 2) { printf("FALHA texto %s\n", seq); ok = 0; }
        nTexto += !vs;
        continue;
      }
      // o indice esperado e o da propria sequencia (o utf8 da tabela)
      int idx = -1;
      for (int i = 0; i < formatador_icones_n(); i++) {
        char sem[64]; w = sem;
        for (p = strtok(strcpy(s2, seq), "-"); p; p = strtok(NULL, "-")) utf8(strtoul(p, NULL, 16), &w);
        *w = 0;
        if (!strcmp(formatador_icone_utf8(i), sem)) { idx = i; break; }
      }
      if (idx < 0) { printf("FALHA: %s fora da tabela\n", seq); ok = 0; continue; }
      snprintf(esp, sizeof esp, "[a]{%03d}[b]", idx);
      ok &= confere(buf, esp);
      n += !vs;
    }
  }
  fclose(f);
  printf("formatador: %d icones, %d ficam texto, tabela com %d\n", n, nTexto, formatador_icones_n());
  assert(n == formatador_icones_n());
  // Nomes dos dois estilos.
  for (int i = 0; i < formatador_icones_n(); i++) {
    int cor;
    const char *a = formatador_icone_nome(i, 0, &cor), *b = formatador_icone_nome(i, 1, &cor);
    assert(a && !strncmp(a, "formatador/", 11) && b && !strncmp(b, "aj_", 3) && cor == 0);
  }
  // Formatador de verdade (AIOStreams): pecas, separadores aparados.
  {
    char o[512], d[512]; size_t k; size_t len;
    int hd = formatador_icone_em("💾", &len);
    char esp[128];
    pecas("💾 11.1 GB | 30.1 Mbps |", o, sizeof o, &k); legivel(o, k, d, sizeof d);
    snprintf(esp, sizeof esp, "{%03d}[11.1 GB | 30.1 Mbps]", hd);
    if (strcmp(d, esp)) { printf("FALHA aiostreams: %s\n", d); ok = 0; }
    // Emoji fora da lista some como antes; o texto fica.
    pecas("🦄 Unicorn", o, sizeof o, &k); legivel(o, k, d, sizeof d);
    if (strcmp(d, "[Unicorn]")) { printf("FALHA desconhecido: %s\n", d); ok = 0; }
    // Tom de pele que a tabela nao tem cai no emoji base e leva o tom junto.
    int danca = formatador_icone_em("💃", &len), dancaTom = formatador_icone_em("💃🏻", &len);
    assert(danca >= 0 && dancaTom >= 0 && danca != dancaTom);
    assert(formatador_icone_em("💃🏽", &len) == danca && len == strlen("💃🏽"));
    // ZWJ desconhecido: 🕵️‍♀️ vira o 🕵 e o "‍♀️" vai junto (nada de " · ").
    int det = formatador_icone_em("🕵", &len);
    assert(formatador_icone_em("🕵️‍♀️", &len) == det && len == strlen("🕵️‍♀️"));
    assert(formatador_icone_em("🕵🏼‍♂️", &len) != det);
    // Bandeiras: par regional, subdivisao, sem PNG volta ao codigo de idioma.
    pecas("🇬🇧 🇧🇷 Dual", o, sizeof o, &k); legivel(o, k, d, sizeof d);
    if (strcmp(d, "(S<GB>|M[EN])(S<BR>|M[PT])[Dual]")) { printf("FALHA bandeiras: %s\n", d); ok = 0; }
    pecas("🏴󠁧󠁢󠁥󠁮󠁧󠁿 English", o, sizeof o, &k); legivel(o, k, d, sizeof d);
    if (strcmp(d, "(S<GB-ENG>|M[EN])[English]")) { printf("FALHA inglaterra: %s\n", d); ok = 0; }
    // Legenda e audio em texto viram bandeira no Padrao; o Minimalista fica com o texto.
    {
      int leg = formatador_icone_em("💬", &len), voz = formatador_icone_em("🗣", &len), dsk = formatador_icone_em("💾", &len);
      char esp[256];
      pecas("💬 EN, PT-BR, JA, Commentary", o, sizeof o, &k); legivel(o, k, d, sizeof d);
      snprintf(esp, sizeof esp, "{%03d}(S<GB><BR><JP>[Commentary]|M[EN, PT-BR, JA, Commentary])", leg);
      if (strcmp(d, esp)) { printf("FALHA legenda: %s\n", d); ok = 0; }
      pecas("🗣 English | Portuguese (Brazil) | Latino 💾 4 GB", o, sizeof o, &k); legivel(o, k, d, sizeof d);
      snprintf(esp, sizeof esp, "{%03d}(S<GB><BR><MX>|M[English | Portuguese (Brazil) | Latino]){%03d}[4 GB]", voz, dsk);
      if (strcmp(d, esp)) { printf("FALHA audio: %s\n", d); ok = 0; }
      // Minusculas sao palavras ("it", "no"), e fora de icone de idioma nada muda.
      pecas("💬 it is fine", o, sizeof o, &k); legivel(o, k, d, sizeof d);
      snprintf(esp, sizeof esp, "{%03d}[it is fine]", leg);
      if (strcmp(d, esp)) { printf("FALHA minusculas: %s\n", d); ok = 0; }
      pecas("💾 EN 4 GB", o, sizeof o, &k); legivel(o, k, d, sizeof d);
      snprintf(esp, sizeof esp, "{%03d}[EN 4 GB]", dsk);
      if (strcmp(d, esp)) { printf("FALHA fora de idioma: %s\n", d); ok = 0; }
      // Nome nativo vira a mesma bandeira do nome em ingles (dono, 18/11:
      // "العربية" = Arabia Saudita); caixa ASCII dobrada, o resto como veio.
      pecas("💬 العربية | English", o, sizeof o, &k); legivel(o, k, d, sizeof d);
      snprintf(esp, sizeof esp, "{%03d}(S<SA><GB>|M[العربية | English])", leg);
      if (strcmp(d, esp)) { printf("FALHA nativo arabe: %s\n", d); ok = 0; }
      pecas("🗣 Español, ESPAÑOL, español, 日本語", o, sizeof o, &k); legivel(o, k, d, sizeof d);
      snprintf(esp, sizeof esp, "{%03d}(S<ES><ES><ES><JP>|M[Español, ESPAÑOL, español, 日本語])", voz);
      if (strcmp(d, esp)) { printf("FALHA nativos: %s\n", d); ok = 0; }
      // Separador depois de texto RTL vira peca propria (senao o bidi da peca
      // joga a barra para o outro lado); no LTR fica colado como antes.
      pecas("💬 مترجم | English", o, sizeof o, &k); legivel(o, k, d, sizeof d);
      snprintf(esp, sizeof esp, "{%03d}(S[مترجم][|]<GB>|M[مترجم | English])", leg);
      if (strcmp(d, esp)) { printf("FALHA separador rtl: %s\n", d); ok = 0; }
      pecas("💬 مترجم |", o, sizeof o, &k); legivel(o, k, d, sizeof d);   // separador no fim sai
      snprintf(esp, sizeof esp, "{%03d}[مترجم]", leg);
      if (strcmp(d, esp)) { printf("FALHA separador rtl no fim: %s\n", d); ok = 0; }
      pecas("💬 Commentary | English", o, sizeof o, &k); legivel(o, k, d, sizeof d);
      snprintf(esp, sizeof esp, "{%03d}(S[Commentary |]<GB>|M[Commentary | English])", leg);
      if (strcmp(d, esp)) { printf("FALHA separador ltr: %s\n", d); ok = 0; }
      printf("legenda: %s\n", "ok");
    }
    k = 0; formatador_pecas("🇿🇿 x", strlen("🇿🇿 x"), o, sizeof o, &k, existe); legivel(o, k, d, sizeof d);
    if (strcmp(d, "[ZZ x]")) { printf("FALHA sem png: %s\n", d); ok = 0; }
    // Simbolos do Tamtaro (monocromaticos) viram icone; ★ e ✓ ficam texto.
    pecas("⯪☆ ✦ HDR ★ ✓", o, sizeof o, &k); legivel(o, k, d, sizeof d);
    printf("tamtaro: %s\n", d);
    assert(d[0] == '{' && strstr(d, "[HDR ★ ✓]"));
    // Saida cheia: nunca corta peca no meio.
    char pequeno[12]; k = 0;
    int np = formatador_pecas("💾 11.1 GB | 30.1 Mbps", strlen("💾 11.1 GB | 30.1 Mbps"), pequeno, sizeof pequeno, &k, NULL);
    assert(np == 1 && k == 4);
  }
  puts(ok ? "formatador: OK" : "formatador: FALHOU");
  return ok ? 0 : 1;
}
