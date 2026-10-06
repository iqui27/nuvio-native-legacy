// NOMES DO DIARIO: id -> titulo, ano, poster (diario.h, "nomes"). Separado do
// diario.c porque e compartilhado entre perfis e porque nao tem regra nenhuma:
// e um cache. Strings alocadas no tamanho certo (a capa costuma ter ~90 bytes,
// e um vetor fixo de 512 por item gastaria 2 MB a toa no teto).
#include "diario.h"
#include "dados.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { char *id, *titulo, *poster; int ano; unsigned uso; } Nome;

static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
static Nome *nomes;
static int n, cap, lido, sujo;
static unsigned relogio;

static char *dup(const char *s) {
  size_t l = strlen(s ? s : "");
  char *d = malloc(l + 1);
  if (d) memcpy(d, s ? s : "", l + 1);
  return d;
}

static int achar(const char *id) {
  int i;
  for (i = 0; i < n; i++) if (!strcmp(nomes[i].id, id)) return i;
  return -1;
}

static int pore(const char *id, const char *titulo, int ano, const char *poster) {
  int i = achar(id);
  if (i < 0) {
    if (n >= DIARIO_NOMES_MAX) {             // sai o usado ha mais tempo
      int k, velho = 0;
      for (k = 1; k < n; k++) if (nomes[k].uso < nomes[velho].uso) velho = k;
      free(nomes[velho].id); free(nomes[velho].titulo); free(nomes[velho].poster);
      nomes[velho] = nomes[--n];
    }
    if (n == cap) {
      int c = cap ? cap * 2 : 128;
      Nome *v = realloc(nomes, sizeof(Nome) * (size_t)c);
      if (!v) return 0;
      nomes = v; cap = c;
    }
    i = n++;
    memset(&nomes[i], 0, sizeof nomes[i]);
    nomes[i].id = dup(id);
    nomes[i].titulo = dup(""); nomes[i].poster = dup("");
  }
  if (titulo && titulo[0] && strcmp(nomes[i].titulo, titulo)) { free(nomes[i].titulo); nomes[i].titulo = dup(titulo); sujo = 1; }
  if (poster && poster[0] && strcmp(nomes[i].poster, poster)) { free(nomes[i].poster); nomes[i].poster = dup(poster); sujo = 1; }
  if (ano > 0 && nomes[i].ano != ano) { nomes[i].ano = ano; sujo = 1; }
  nomes[i].uso = ++relogio;
  return 1;
}

static void ler(void) {
  char *b, *s;
  if (lido) return;
  lido = 1;
  b = dados_ler("diario-nomes.txt");
  if (!b) return;
  for (s = b; s && *s;) {
    char *nl = strchr(s, '\n'), *c[4];
    int k = 0;
    if (nl) *nl = 0;
    c[0] = s;
    while (k < 3 && (c[k + 1] = strchr(c[k], '\t')) != NULL) { *c[k + 1]++ = 0; k++; }
    if (k == 3 && c[0][0] && c[0][0] != '#' && strlen(c[0]) < DIARIO_ID_MAX)
      pore(c[0], c[1], atoi(c[2]), c[3]);
    s = nl ? nl + 1 : NULL;
  }
  free(b);
  sujo = 0;
}

static void limpo(char *d, size_t t, const char *s) {
  size_t i = 0;
  for (; s && *s && i + 1 < t; s++) d[i++] = (*s == '\t' || *s == '\n' || *s == '\r') ? ' ' : *s;
  d[i] = 0;
}

void diario_nome_definir(const char *idEnt, const char *titulo, int ano, const char *poster) {
  char id[DIARIO_ID_MAX], tl[160], pl[512];
  diario_separar_id(idEnt, id, sizeof id, NULL, NULL);
  if (!id[0]) return;
  limpo(tl, sizeof tl, titulo);
  limpo(pl, sizeof pl, poster);
  pthread_mutex_lock(&trava);
  ler();
  pore(id, tl, ano, pl);
  pthread_mutex_unlock(&trava);
}

int diario_nome(const char *idEnt, char *titulo, size_t tt, int *ano, char *poster, size_t tp) {
  char id[DIARIO_ID_MAX];
  int i, ok = 0;
  diario_separar_id(idEnt, id, sizeof id, NULL, NULL);
  pthread_mutex_lock(&trava);
  ler();
  i = achar(id);
  if (i >= 0) {
    if (titulo && tt) snprintf(titulo, tt, "%s", nomes[i].titulo);
    if (poster && tp) snprintf(poster, tp, "%s", nomes[i].poster);
    if (ano) *ano = nomes[i].ano;
    ok = nomes[i].titulo[0] != 0;
  } else {
    if (titulo && tt) titulo[0] = 0;
    if (poster && tp) poster[0] = 0;
    if (ano) *ano = 0;
  }
  pthread_mutex_unlock(&trava);
  return ok;
}

// Chamado por diario_gravar_agora / diario_passo (diario.c), sem a trava dele.
void diario_nomes_gravar(void) {
  char *b = NULL;
  size_t c = 0, k = 0;
  int i;
  pthread_mutex_lock(&trava);
  if (sujo) {
    for (i = 0; i < n; i++) c += strlen(nomes[i].id) + strlen(nomes[i].titulo) + strlen(nomes[i].poster) + 24;
    b = malloc(c + 32);
    if (b) {
      k = (size_t)snprintf(b, c + 32, "# nuvio diario nomes v1\n");
      for (i = 0; i < n; i++)
        k += (size_t)snprintf(b + k, c + 32 - k, "%s\t%s\t%d\t%s\n", nomes[i].id, nomes[i].titulo, nomes[i].ano, nomes[i].poster);
      sujo = 0;
    }
  }
  pthread_mutex_unlock(&trava);
  if (b) { dados_gravar("diario-nomes.txt", b); free(b); }
}
