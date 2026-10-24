#include "conquistasarte.h"
#include "conquistasregra.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

typedef struct { const char *objeto; CqTinta cor; } Objeto;
// Uma tabela so: objeto -> tinta do acento, escolhida para casar com o material da peca.
static const Objeto OBJ[] = {
  {"director", {226,176,84}},  {"actor", {214,122,150}},   {"decade", {92,172,212}},
  {"genre", {232,150,72}},     {"noir", {150,160,190}},    {"fantasy", {150,120,225}},
  {"horror", {238,128,52}},    {"saga", {218,96,92}},      {"studio", {86,190,170}},
  {"top250", {236,196,72}},    {"country", {110,190,120}}, {"language", {90,170,235}},
  {"secret", {214,76,92}},     {"community", {232,140,100}}, {"trail", {98,196,150}},
  {"list", {120,160,236}},     {"explorer", {76,196,200}}, {"award", {244,200,84}},
  {"international", {84,150,230}}, {"palm", {104,190,110}}, {"season", {200,130,230}},
  {"silent", {205,155,110}},   {"movement", {232,100,150}}, {"discovery", {120,220,130}},
};
#define N_OBJ ((int)(sizeof OBJ / sizeof OBJ[0]))
static const CqTinta CINZA = {150, 156, 168};
static const char *const PADRAO[CQ_FAM_N] = { "trail", "director", "explorer", "award", "community", "season" };
// Trilhas curadas: pedaco do slug -> objeto.
static const char *const PALAVRA[][2] = {
  {"ghibli","studio"}, {"studio","studio"}, {"cult","decade"}, {"80","decade"}, {"90","decade"}, {"decad","decade"},
  {"noir","noir"}, {"fantas","fantasy"}, {"terror","horror"}, {"horror","horror"}, {"internacional","international"}, {"oscar","award"},
  {"melhor","award"}, {"palma","palm"}, {"cannes","palm"}, {"silenc","silent"}, {"mudo","silent"},
  {"pais","country"}, {"mundo","country"}, {"idioma","language"}, {"lingua","language"}, {"lista","list"},
  {"secret","secret"}, {"saga","saga"},
};

static int pref(const char *s, const char *p) { return !strncmp(s, p, strlen(p)); }

const char *cqarte_objeto(const char *chave, int familia) {
  size_t i;
  char low[CQ_CHAVE_MAX + 1];
  if (!chave) chave = "";
  if (pref(chave, "dir:")) return "director";
  if (pref(chave, "ator:")) return "actor";
  if (pref(chave, "saga:")) return "saga";
  if (pref(chave, "top:") || pref(chave, "trakt:")) return "top250";
  if (pref(chave, "trilha:")) {
    snprintf(low, sizeof low, "%s", chave + 7);
    for (i = 0; low[i]; i++) low[i] = (char)tolower((unsigned char)low[i]);
    for (i = 0; i < sizeof PALAVRA / sizeof PALAVRA[0]; i++)
      if (strstr(low, PALAVRA[i][0])) return PALAVRA[i][1];
  }
  return PADRAO[familia < 0 || familia >= CQ_FAM_N ? 0 : familia];
}

const char *cqarte_material(int nivel) {
  return nivel <= 0 ? "locked" : nivel == 1 ? "bronze" : nivel <= 3 ? "silver" : "gold";
}

int cqarte_arquivo(const char *chave, int familia, int nivel, char *dst, size_t n) {
  const char *o = cqarte_objeto(chave, familia), *m = cqarte_material(nivel);
  if (!strcmp(o, "director") && chave && pref(chave, "dir:")) {
    int c = tolower((unsigned char)chave[4]);   // inicial pronta so para N e K
    if (c == 'n' || c == 'k') return snprintf(dst, n, "director-%c-%s.png", c, m) < (int)n;
  }
  return snprintf(dst, n, "%s-%s.png", o, m) < (int)n;
}

CqTinta cqarte_tinta(const char *chave, int familia, int nivel) {
  const char *o = cqarte_objeto(chave, familia);
  int i;
  if (nivel <= 0) return CINZA;
  for (i = 0; i < N_OBJ; i++) if (!strcmp(OBJ[i].objeto, o)) return OBJ[i].cor;
  return CINZA;
}
