#include "musica.h"
#include "rede.h"
#include "js.h"
#include "dados.h"
#include "perfis.h"
#include "sync.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef NV_REC_URL
#define NV_REC_URL ""
#endif

#define MUS_TAB 8

typedef struct {
  char chave[24];
  char titulo[128];
  int  ano, temp, estado;
  long quando;
  MusAlbum alb;
} Entrada;
static Entrada tab[MUS_TAB];
static int nTab, prox;
static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;

static void chaveDe(char *dst, unsigned tam, const char *imdb, int temp) {
  snprintf(dst, tam, "%s:%d", imdb, temp);
}

static Entrada *achar(const char *chave) {
  int i;
  for (i = 0; i < nTab; i++) if (!strcmp(tab[i].chave, chave)) return &tab[i];
  return NULL;
}

static Entrada *reservar(const char *chave) {
  Entrada *e = achar(chave);
  if (e) return e;
  if (nTab < MUS_TAB) e = &tab[nTab++];
  else { e = &tab[prox]; prox = (prox + 1) % MUS_TAB; }
  memset(e, 0, sizeof *e);
  snprintf(e->chave, sizeof e->chave, "%s", chave);
  return e;
}

int mus_pagina(int atual, int faixas, int direcao) {
  int ultima = faixas > 0 ? (faixas - 1) / 2 : 0;
  int nova = atual + direcao;
  return nova < 0 ? 0 : (nova > ultima ? ultima : nova);
}

int mus_interpretar(const char *json, MusAlbum *out) {
  const char *a, *fim, *p;
  memset(out, 0, sizeof *out);
  if (!json || js_tem(json, NULL, "vazio") || !js_tem(json, NULL, "album")) return 0;
  a = strstr(json, "\"album\"");
  a = a ? strchr(a, '{') : NULL;
  if (!a) return 0;
  fim = js_fim(a);
  if (!fim) return 0;
  js_texto(a, fim, "titulo", out->titulo, sizeof out->titulo);
  js_texto(a, fim, "artista", out->artista, sizeof out->artista);
  js_texto(a, fim, "capa", out->capa, sizeof out->capa);
  js_texto(a, fim, "apple", out->apple, sizeof out->apple);
  if (!out->titulo[0]) return 0;
  for (p = js_array(json, NULL, "faixas"); p && out->n < MUS_FAIXAS_MAX; p = js_prox(js_fim(p))) {
    const char *f = js_fim(p);
    int k = out->n;
    if (!f) break;
    out->faixa[k].n = (int)js_num(p, f, "n", k + 1);
    js_texto(p, f, "titulo", out->faixa[k].titulo, sizeof out->faixa[k].titulo);
    js_texto(p, f, "artista", out->faixa[k].artista, sizeof out->faixa[k].artista);
    out->faixa[k].dur = (int)js_num(p, f, "dur", 0);
    if (out->faixa[k].titulo[0]) out->n++;
  }
  return out->n > 0;
}

static void codificar(char *dst, unsigned tam, const char *s) {
  unsigned k = 0;
  for (; *s && k + 4 < tam; s++) {
    unsigned char c = (unsigned char)*s;
    if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
        c == '-' || c == '_' || c == '.' || c == '~') dst[k++] = (char)c;
    else k += (unsigned)snprintf(dst + k, tam - k, "%%%02X", c);
  }
  dst[k] = 0;
}

void mus_url_celular(char *dst, unsigned tam, const char *imdb,
                     const char *titulo, int ano, int temp) {
  char t[256];
  if (!NV_REC_URL[0]) { if (tam) dst[0] = 0; return; }
  codificar(t, sizeof t, titulo ? titulo : "");
  snprintf(dst, tam, "%s/m/#id=%s&t=%s&y=%d", NV_REC_URL, imdb, t, ano);
  if (temp > 0) {
    size_t n = strlen(dst);
    snprintf(dst + n, tam - n, "&s=%d", temp);
  }
}

typedef struct { char imdb[16], titulo[128]; int ano, temp; } Pedido;

static void *buscar(void *arg) {
  Pedido pd = *(Pedido *)arg;
  char url[600], t[256], chave[24];
  char *corpo = NULL;
  MusAlbum alb;
  int achou = 0;
  Entrada *e;
  free(arg);
  if (NV_REC_URL[0]) {
    codificar(t, sizeof t, pd.titulo);
    snprintf(url, sizeof url, "%s/musica/v1/%s?t=%s&y=%d", NV_REC_URL, pd.imdb, t, pd.ano);
    if (pd.temp > 0) { size_t n = strlen(url); snprintf(url + n, sizeof url - n, "&s=%d", pd.temp); }
    corpo = rede_baixar(url, 15);
  }
  if (corpo) achou = mus_interpretar(corpo, &alb);
  chaveDe(chave, sizeof chave, pd.imdb, pd.temp);
  pthread_mutex_lock(&mtx);
  e = reservar(chave);
  e->quando = (long)time(NULL);
  if (achou) { e->alb = alb; e->estado = MUS_PRONTO; }
  else e->estado = corpo ? (js_tem(corpo, NULL, "vazio") ? MUS_VAZIO : MUS_ERRO) : MUS_ERRO;
  pthread_mutex_unlock(&mtx);
  free(corpo);
  return NULL;
}

void mus_pedir(const char *imdb, const char *titulo, int ano, int temp) {
  char chave[24];
  Entrada *e;
  Pedido *pd;
  pthread_t f;
  if (!imdb || strncmp(imdb, "tt", 2) || !titulo || !titulo[0]) return;
  chaveDe(chave, sizeof chave, imdb, temp);
  pthread_mutex_lock(&mtx);
  e = reservar(chave);
  // Erro vale 60 s (tenta de novo ao reabrir); o resto fica na sessao.
  if (e->estado == MUS_CARREGANDO || e->estado == MUS_PRONTO || e->estado == MUS_VAZIO ||
      (e->estado == MUS_ERRO && (long)time(NULL) - e->quando < 60)) {
    pthread_mutex_unlock(&mtx);
    return;
  }
  e->estado = MUS_CARREGANDO;
  pthread_mutex_unlock(&mtx);
  pd = calloc(1, sizeof *pd);
  if (!pd) return;
  snprintf(pd->imdb, sizeof pd->imdb, "%s", imdb);
  snprintf(pd->titulo, sizeof pd->titulo, "%s", titulo);
  pd->ano = ano; pd->temp = temp;
  if (pthread_create(&f, NULL, buscar, pd) == 0) pthread_detach(f);
  else { free(pd); pthread_mutex_lock(&mtx); e->estado = MUS_ERRO; pthread_mutex_unlock(&mtx); }
}

MusEstado mus_estado(const char *imdb, int temp, MusAlbum *out) {
  char chave[24];
  Entrada *e;
  MusEstado r = MUS_NADA;
  if (!imdb) return r;
  chaveDe(chave, sizeof chave, imdb, temp);
  pthread_mutex_lock(&mtx);
  e = achar(chave);
  if (e) {
    r = (MusEstado)e->estado;
    if (out && r == MUS_PRONTO) *out = e->alb;
  }
  pthread_mutex_unlock(&mtx);
  return r;
}

// --- Guardadas: uma linha por trilha, "imdb:temp<TAB>album<TAB>artista".
static const char *arquivo(void) {
  static char nome[48];
  int p = perfis_ativo();
  if (p <= 0) snprintf(nome, sizeof nome, "musicas.txt");
  else snprintf(nome, sizeof nome, "musicas-p%d.txt", p);
  return nome;
}

static int linhaDe(const char *b, const char *chave, const char **ini) {
  size_t n = strlen(chave);
  const char *l;
  for (l = b; *l; ) {
    const char *nl = strchr(l, '\n');
    if (!strncmp(l, chave, n) && l[n] == '\t') { if (ini) *ini = l; return 1; }
    if (!nl) break;
    l = nl + 1;
  }
  return 0;
}

int mus_guardado(const char *imdb, int temp) {
  char chave[24];
  char *b;
  int r;
  if (!imdb) return 0;
  chaveDe(chave, sizeof chave, imdb, temp);
  b = dados_ler(arquivo());
  if (!b) return 0;
  r = linhaDe(b, chave, NULL);
  free(b);
  return r;
}

int mus_listar(MusSalva *out, int max) {
  char *b = dados_ler(arquivo()), *p = b;
  int n = 0;
  while (p && *p && n < max) {
    char *end = strchr(p, '\n');
    if (end) *end = 0;
    MusSalva s = {0};
    if (sscanf(p, "%15[^:]:%d\t%127[^\t]\t%95[^\n]", s.imdb, &s.temp, s.titulo, s.artista) >= 3)
      out[n++] = s;
    p = end ? end + 1 : NULL;
  }
  free(b);
  return n;
}

static void limpo(char *dst, size_t tam, const char *s) {
  size_t k = 0;
  for (; s && *s && k + 1 < tam; s++) dst[k++] = (*s == '\t' || *s == '\n' || *s == '\r') ? ' ' : *s;
  dst[k] = 0;
}

int mus_alternar_guardado(const char *imdb, int temp, const char *album, const char *artista) {
  char chave[24], a[128], r[96];
  char *b = dados_ler(arquivo());
  char *novo;
  size_t cap, k = 0;
  const char *ini = NULL;
  int ligado, linhas = 0;
  const char *l;
  chaveDe(chave, sizeof chave, imdb, temp);
  limpo(a, sizeof a, album);
  limpo(r, sizeof r, artista);
  ligado = !(b && linhaDe(b, chave, &ini));
  cap = (b ? strlen(b) : 0) + 400;
  novo = malloc(cap);
  if (!novo) { free(b); return !ligado; }
  novo[0] = 0;
  for (l = b; l && *l; ) {
    const char *nl = strchr(l, '\n');
    size_t len = nl ? (size_t)(nl - l) + 1 : strlen(l);
    if (l != ini && len > 1 && linhas < MUS_GUARDADAS_MAX - 1) {
      memcpy(novo + k, l, len); k += len; linhas++;
      if (!nl) novo[k++] = '\n';
    }
    l += len;
  }
  if (ligado) k += (size_t)snprintf(novo + k, cap - k, "%s\t%s\t%s\n", chave, a, r);
  novo[k] = 0;
  if (dados_gravar(arquivo(), novo)) sync_proteger_ajustes_locais();
  free(novo);
  free(b);
  return ligado;
}

void mus_esquecer(void) {
  pthread_mutex_lock(&mtx);
  nTab = 0; prox = 0;
  memset(tab, 0, sizeof tab);
  pthread_mutex_unlock(&mtx);
}
