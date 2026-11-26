// Ver esportes_canal.h.
#include "esportes_canal.h"
#include "buscanorm.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

// Palavras que nao mudam QUAL canal e: qualidade, codec, copia de reserva.
static int enfeite(const char *p, int *qualidade) {
  static const struct { const char *p; int q; } tab[] = {
    {"hd",1},{"fhd",2},{"fullhd",2},{"uhd",2},{"4k",2},{"1080p",2},{"1080",2},
    {"720p",1},{"720",1},{"sd",-1},{"480p",-1},{"h264",0},{"h265",0},{"hevc",0},
    {"50fps",0},{"60fps",0},{"alt",0},{"backup",0},{"raw",0}
  };
  for (size_t i = 0; i < sizeof tab / sizeof *tab; i++)
    if (!strcmp(p, tab[i].p)) {
      if (qualidade && (tab[i].q > *qualidade || (tab[i].q < 0 && !*qualidade))) *qualidade = tab[i].q;
      return 1;
    }
  return 0;
}
// Prefixo de pais de lista IPTV ("BR: ESPN", "US | ESPN"): so na frente.
static int pais(const char *p) {
  static const char *tab[] = {"br","us","usa","uk","pt","ca","mx","ar","es","fr","de","it"};
  for (size_t i = 0; i < sizeof tab / sizeof *tab; i++) if (!strcmp(p, tab[i])) return 1;
  return 0;
}
// Nome em palavras minusculas sem acento; "+" vira "plus" (ESPN+ nao e ESPN).
// `nucleo` = as palavras que identificam o canal, coladas ("espn2").
// `frase` = as mesmas separadas por um espaco, com um em cada ponta.
static void partes(const char *nome, char *nucleo, size_t nn, char *frase, size_t nf, int *qualidade) {
  char cru[400], norm[800]; size_t w = 0;
  for (const char *s = nome ? nome : ""; *s && w + 7 < sizeof cru; s++) {
    if (*s == '+') { memcpy(cru + w, " plus ", 6); w += 6; } else cru[w++] = *s;
  }
  cru[w] = 0;
  busca_normalizar(cru, norm, sizeof norm);
  size_t a = 0, b = 0; int primeira = 1;
  nucleo[0] = 0; if (nf > 1) { frase[b++] = ' '; } frase[b] = 0;
  for (char *p = norm; *p; ) {
    char palavra[64]; size_t n = 0;
    while (*p && (unsigned char)*p < 128 && !isalnum((unsigned char)*p)) p++;
    while (*p && ((unsigned char)*p >= 128 || isalnum((unsigned char)*p))) { if (n + 1 < sizeof palavra) palavra[n++] = *p; p++; }
    palavra[n] = 0;
    if (!n) continue;
    int eraPrimeira = primeira; primeira = 0;
    if (enfeite(palavra, qualidade)) continue;
    if (eraPrimeira && pais(palavra)) {
      const char *q = p; while (*q == ' ') q++;
      if (*q) continue;   // sozinho ("BR") e o nome do canal, nao prefixo
    }
    if (a + n < nn) { memcpy(nucleo + a, palavra, n); a += n; nucleo[a] = 0; }
    if (b + n + 1 < nf) { memcpy(frase + b, palavra, n); b += n; frase[b++] = ' '; frase[b] = 0; }
  }
}

int esportes_canal_nota(const EspEvento *ev, const char *nome, int epg) {
  char nucleo[200], frase[400]; int qualidade = 0, emissora = 0;
  if (!ev || !nome || !nome[0]) return 0;
  partes(nome, nucleo, sizeof nucleo, frase, sizeof frase, &qualidade);
  for (int i = 0; i < ev->nEmissoras && nucleo[0]; i++) {
    char en[200], ef[400];
    partes(ev->emissoras[i], en, sizeof en, ef, sizeof ef, NULL);
    if (strlen(en) < 3) continue;
    if (!strcmp(en, nucleo)) { emissora = 60; break; }
    if (strstr(frase, ef) && emissora < 20) emissora = 20;
  }
  if (!epg && !emissora) return 0;
  return (epg ? 100 : 0) + emissora + qualidade + 1;
}

int esportes_canal_evento_do_titulo(const char *titulo, const char *modalidade,
                                    time_t inicio, time_t fim, EspEvento *out) {
  static const char *seps[] = {" at ", " vs. ", " vs ", " v ", " x ", " @ ", " versus "};
  if (!out) return 0;
  memset(out, 0, sizeof *out);
  if (!titulo) titulo = "";
  snprintf(out->nome, sizeof out->nome, "%s", titulo);
  snprintf(out->modalidade, sizeof out->modalidade, "%s", modalidade ? modalidade : "");
  out->inicio = inicio; out->fim = fim;
  for (size_t s = 0; s < sizeof seps / sizeof *seps; s++) {
    size_t n = strlen(seps[s]);
    for (const char *p = titulo; *p; p++) {
      size_t k = 0;
      while (k < n && p[k] && tolower((unsigned char)p[k]) == seps[s][k]) k++;
      if (k != n || p == titulo || !p[n]) continue;
      snprintf(out->participantes[0].nome, sizeof out->participantes[0].nome, "%.*s", (int)(p - titulo), titulo);
      snprintf(out->participantes[1].nome, sizeof out->participantes[1].nome, "%s", p + n);
      out->nParticipantes = 2;
      return 1;
    }
  }
  return 0;
}
