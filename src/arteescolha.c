// A arte escolhida a mao, por titulo e por perfil. Ver arteescolha.h.
#include "arteescolha.h"
#include "dados.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  char id[40];
  char fundo[ARTEESC_URL];
  char logo[ARTEESC_URL];
  unsigned ordem;          // quando entrou/mudou: a menor sai com a tabela cheia
} ArteEsc;

static ArteEsc tab[ARTEESC_MAX];
static int nTab;
static int carregado;
static int perfil;
static unsigned relogio;

// INDICE POR HASH, refeito a cada escrita. Escrita e rara (um OK na tela de
// escolha); leitura e varias por quadro. 512 posicoes para 160 entradas deixa
// a sondagem linear em uma ou duas posicoes.
#define ARTEESC_SLOTS 512
static short slot[ARTEESC_SLOTS];

// Ate onde arteesc_esquecer varre, como FONTEPREF_PERFIS: arquivo que nao
// existe custa um dados_apagar que falha.
#define ARTEESC_PERFIS 16

static const char *arquivo(int p) {
  static char nome[48];
  snprintf(nome, sizeof nome, "arte-escolhida-p%d.txt", p);
  return nome;
}

static unsigned hashId(const char *s) {
  unsigned h = 2166136261u;
  for (; *s; s++) { h ^= (unsigned char)*s; h *= 16777619u; }
  return h;
}

static void reindexar(void) {
  int i;
  for (i = 0; i < ARTEESC_SLOTS; i++) slot[i] = -1;
  for (i = 0; i < nTab; i++) {
    unsigned h = hashId(tab[i].id) % ARTEESC_SLOTS;
    while (slot[h] >= 0) h = (h + 1) % ARTEESC_SLOTS;
    slot[h] = (short)i;
  }
}

void arteesc_chave(const char *id, char *dst, size_t n) {
  size_t i = 0;
  int doisPontos = 0, corte;
  if (!n) return;
  dst[0] = 0;
  if (!id || !id[0]) return;
  // tt corta no primeiro ':'; o resto ("kitsu:7442:3", "tmdb:m123") no segundo.
  corte = (id[0] == 't' && id[1] == 't') ? 1 : 2;
  while (id[i] && i + 1 < n) {
    if (id[i] == ':' && ++doisPontos >= corte) break;
    dst[i] = id[i];
    i++;
  }
  dst[i] = 0;
}

static int achar(const char *chave) {
  unsigned h;
  int voltas = 0;
  if (!chave[0] || !nTab) return -1;
  h = hashId(chave) % ARTEESC_SLOTS;
  while (slot[h] >= 0 && voltas++ < ARTEESC_SLOTS) {
    if (!strcmp(tab[slot[h]].id, chave)) return slot[h];
    h = (h + 1) % ARTEESC_SLOTS;
  }
  return -1;
}

static char *campo(char **p) {
  char *ini = *p, *t;
  if (!ini) return (char *)"";
  t = strchr(ini, '\t');
  if (t) { *t = 0; *p = t + 1; } else { *p = NULL; }
  return ini;
}

// Url so entra se for http(s) ou um caminho do pacote/virtual conhecido: uma
// linha estragada no disco nao pode virar pedido de textura de lixo.
static int urlValida(const char *u) {
  return u && (!strncmp(u, "https://", 8) || !strncmp(u, "http://", 7) ||
               u[0] == '/' || !strncmp(u, "deploy/", 7));
}

void arteesc_iniciar(void) {
  char *b, *linha, *prox;
  if (carregado) return;
  carregado = 1;
  nTab = 0;
  relogio = 0;
  b = dados_ler(arquivo(perfil));
  if (b) {
    for (linha = b; linha && *linha && nTab < ARTEESC_MAX; linha = prox) {
      char *p, *id, *fundo, *logo;
      char *fim = strchr(linha, '\n');
      prox = fim ? fim + 1 : NULL;
      if (fim) *fim = 0;
      if (linha[0] == '#' || !linha[0]) continue;
      p = linha;
      id = campo(&p);
      fundo = campo(&p);
      logo = p ? p : (char *)"";
      { size_t L = strlen(logo);            // CRLF de quem editou a mao
        if (L && logo[L - 1] == '\r') logo[L - 1] = 0; }
      if (!urlValida(fundo)) fundo = (char *)"";
      if (!urlValida(logo)) logo = (char *)"";
      if (!id[0] || (!fundo[0] && !logo[0])) continue;
      { ArteEsc *e = &tab[nTab++];
        memset(e, 0, sizeof *e);
        arteesc_chave(id, e->id, sizeof e->id);
        snprintf(e->fundo, sizeof e->fundo, "%s", fundo);
        snprintf(e->logo, sizeof e->logo, "%s", logo);
        e->ordem = ++relogio; }
    }
    free(b);
  }
  reindexar();
  if (nTab) { printf("[arte] %d arte(s) escolhida(s) no perfil %d\n", nTab, perfil); fflush(stdout); }
}

void arteesc_definir_perfil(int p) {
  if (p < 0) p = 0;
  if (p == perfil && carregado) return;
  perfil = p;
  nTab = 0;
  carregado = 0;
  reindexar();
}

void arteesc_esquecer(void) {
  int p;
  for (p = 0; p <= ARTEESC_PERFIS; p++) dados_apagar(arquivo(p));
  nTab = 0;
  carregado = 1;   // o disco agora nao tem nada
  reindexar();
}

static void gravar(void) {
  size_t cap = (size_t)nTab * (40 + 2 * ARTEESC_URL + 4) + 64, k = 0;
  char *buf = (char *)malloc(cap);
  int i;
  if (!buf) return;
  k += (size_t)snprintf(buf + k, cap - k, "# nuvio arte v1\n");
  for (i = 0; i < nTab && k + 1 < cap; i++)
    k += (size_t)snprintf(buf + k, cap - k, "%s\t%s\t%s\n",
                          tab[i].id, tab[i].fundo, tab[i].logo);
  dados_gravar(arquivo(perfil), buf);
  free(buf);
}

const char *arteesc_fundo(const char *id) {
  char k[40];
  int i;
  if (!carregado) arteesc_iniciar();
  if (!nTab) return NULL;               // o caso da home: sai aqui
  arteesc_chave(id, k, sizeof k);
  i = achar(k);
  return i >= 0 && tab[i].fundo[0] ? tab[i].fundo : NULL;
}

const char *arteesc_logo(const char *id) {
  char k[40];
  int i;
  if (!carregado) arteesc_iniciar();
  if (!nTab) return NULL;
  arteesc_chave(id, k, sizeof k);
  i = achar(k);
  return i >= 0 && tab[i].logo[0] ? tab[i].logo : NULL;
}

// `qual` 0 = fundo, 1 = logo.
static int definir(const char *id, int qual, const char *url) {
  char k[40];
  int i;
  ArteEsc *e;
  if (!carregado) arteesc_iniciar();
  arteesc_chave(id, k, sizeof k);
  if (!k[0]) return 0;
  if (url && !url[0]) url = NULL;
  if (url && (!urlValida(url) || strlen(url) >= ARTEESC_URL)) return 0;
  i = achar(k);
  if (i < 0) {
    if (!url) return 0;                  // Automatico que ja era Automatico
    if (nTab < ARTEESC_MAX) i = nTab++;
    else {                               // cheia: sai a mais antiga
      int j;
      i = 0;
      for (j = 1; j < nTab; j++) if (tab[j].ordem < tab[i].ordem) i = j;
    }
    e = &tab[i];
    memset(e, 0, sizeof *e);
    snprintf(e->id, sizeof e->id, "%s", k);
  } else {
    const char *atual = qual ? tab[i].logo : tab[i].fundo;
    if (!strcmp(atual, url ? url : "")) return 0;
    e = &tab[i];
  }
  snprintf(qual ? e->logo : e->fundo, ARTEESC_URL, "%s", url ? url : "");
  e->ordem = ++relogio;
  if (!e->fundo[0] && !e->logo[0]) {     // as duas no Automatico: sai a linha
    tab[i] = tab[--nTab];
  }
  reindexar();
  gravar();
  return 1;
}

int arteesc_definir_fundo(const char *id, const char *url) { return definir(id, 0, url); }
int arteesc_definir_logo(const char *id, const char *url)  { return definir(id, 1, url); }

int arteesc_n(void) {
  if (!carregado) arteesc_iniciar();
  return nTab;
}
