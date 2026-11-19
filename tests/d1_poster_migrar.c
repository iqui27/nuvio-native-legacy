// #739: marcador prov= de posteres.txt desambigua posterProvLocal=3.
// Espelha a migracao de src/ajustes.c (pstCarregar + posterProvMigrar) com o
// MESMO formato de arquivo, inclusive a linha prov= gravada pela versao nova.
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { PP_DESLIGADO = 0, PP_SPATIAL, PP_RPDB, PP_POSTERSPLUS, PP_MODELO };

static char pstInst[128], pstToken[400], pstExtra[120], pstChave[120], pstModelo[1024];
static int pstProvArq = -1;

static void pstCopia(char *dst, size_t n, const char *src) {
  size_t k = src ? strlen(src) : 0;
  if (k >= n) k = n - 1;
  if (k) memcpy(dst, src, k);
  dst[k] = 0;
}

// Mesmo parse de pstCarregar (ajustes.c), linha a linha chave=valor.
static void pstCarregarTexto(const char *t) {
  pstInst[0] = pstToken[0] = pstExtra[0] = pstChave[0] = pstModelo[0] = 0;
  pstProvArq = -1;
  for (const char *p = t; p && *p;) {
    const char *fim = strchr(p, '\n'), *eq;
    char v[1200];
    size_t n = fim ? (size_t)(fim - p) : strlen(p), nv;
    eq = memchr(p, '=', n);
    if (!eq) { p = fim ? fim + 1 : NULL; continue; }
    nv = n - (size_t)(eq + 1 - p);
    if (nv >= sizeof v) { p = fim ? fim + 1 : NULL; continue; }
    memcpy(v, eq + 1, nv); v[nv] = 0;
    if (nv && v[nv - 1] == '\r') v[nv - 1] = 0;
    if (!strncmp(p, "prov=", 5)) { int pv = atoi(v); pstProvArq = pv >= PP_DESLIGADO && pv <= PP_MODELO ? pv : -1; }
    else if (!strncmp(p, "inst=", 5)) pstCopia(pstInst, sizeof pstInst, v);
    else if (!strncmp(p, "token=", 6)) pstCopia(pstToken, sizeof pstToken, v);
    else if (!strncmp(p, "extra=", 6)) pstCopia(pstExtra, sizeof pstExtra, v);
    else if (!strncmp(p, "chave=", 6)) pstCopia(pstChave, sizeof pstChave, v);
    else if (!strncmp(p, "modelo=", 7)) pstCopia(pstModelo, sizeof pstModelo, v);
    p = fim ? fim + 1 : NULL;
  }
}

// Mesma regra de posterProvMigrar (ajustes.c).
static int migrar(int provLocal) {
  if (pstProvArq < 0 && provLocal == PP_POSTERSPLUS) return PP_MODELO;
  return provLocal;
}

static void arquivo(char *b, size_t n, int prov, const char *inst, const char *modelo) {
  if (prov >= 0)
    snprintf(b, n, "prov=%d\ninst=%s\ntoken=\nextra=\nchave=\nmodelo=%s\n", prov, inst, modelo);
  else
    snprintf(b, n, "inst=%s\ntoken=\nextra=\nchave=\nmodelo=%s\n", inst, modelo);
}

int main(void) {
  char b[2048];
  // Legado SEM prov=: o 3 de ajustes.txt so podia ser Modelo proprio — inst
  // velha de SpatialPosters + modelo salvo juntos nao podem virar Posters+.
  arquivo(b, sizeof b, -1, "posters.meu.com", "https://x/{imdb}.jpg");
  pstCarregarTexto(b); assert(migrar(PP_POSTERSPLUS) == PP_MODELO);
  // Legado com so inst (SpatialPosters antigo): quem tem 3 gravado sem prov=
  // migra para Modelo — a versao antiga nao gravava 3 para SpatialPosters.
  arquivo(b, sizeof b, -1, "posters.meu.com", "");
  pstCarregarTexto(b); assert(migrar(PP_POSTERSPLUS) == PP_MODELO);
  // Novo com prov=3 (Posters+): fica Posters+ mesmo com modelo antigo no arquivo.
  arquivo(b, sizeof b, PP_POSTERSPLUS, "pplus.srv", "https://x/{imdb}.jpg");
  pstCarregarTexto(b); assert(migrar(PP_POSTERSPLUS) == PP_POSTERSPLUS);
  // Novo com prov=4 (Modelo proprio) e inst residencial: fica Modelo.
  arquivo(b, sizeof b, PP_MODELO, "posters.meu.com", "https://x/{imdb}.jpg");
  pstCarregarTexto(b); assert(migrar(PP_MODELO) == PP_MODELO);
  // Nenhum campo: migracao nao mexe.
  arquivo(b, sizeof b, -1, "", "");
  pstCarregarTexto(b); assert(migrar(PP_POSTERSPLUS) == PP_MODELO);
  puts("d1_poster_migrar: ok");
  return 0;
}
