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
  int ultima = faixas > 0 ? (faixas - 1) / MUS_POR_PAGINA : 0;
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
  out->ano = (int)js_num(a, fim, "ano", 0);
  if (!out->titulo[0]) return 0;
  for (p = js_array(json, NULL, "faixas"); p && out->n < MUS_FAIXAS_MAX; p = js_prox(js_fim(p))) {
    const char *f = js_fim(p);
    int k = out->n;
    if (!f) break;
    out->faixa[k].n = (int)js_num(p, f, "n", k + 1);
    js_texto(p, f, "titulo", out->faixa[k].titulo, sizeof out->faixa[k].titulo);
    js_texto(p, f, "artista", out->faixa[k].artista, sizeof out->faixa[k].artista);
    out->faixa[k].dur = (int)js_num(p, f, "dur", 0);
    js_texto(p, f, "previa", out->faixa[k].previa, sizeof out->faixa[k].previa);
    if (strncmp(out->faixa[k].previa, "https://", 8)) out->faixa[k].previa[0] = 0;
    if (out->faixa[k].titulo[0]) out->n++;
  }
  return out->n > 0;
}

void mus_previa_parar(MusPrevia *p) {
  p->ativa = p->pronta = 0; p->faixa = -1;
}

int mus_previa_iniciar(MusPrevia *p, const MusAlbum *a, int faixa, int shuffle,
                       unsigned seed, unsigned agora, int ocupado) {
  mus_previa_parar(p); p->n = p->pos = 0;
  if (ocupado || !a || faixa >= a->n || faixa < -1) return -1;
  for (int i = 0; i < a->n && i < MUS_FAIXAS_MAX; i++)
    if (a->faixa[i].previa[0] && (faixa < 0 || faixa == i)) p->ordem[p->n++] = i;
  if (!p->n) return -1;
  if (shuffle) for (int i = p->n - 1; i > 0; i--) {
    seed = seed * 1664525u + 1013904223u;
    int j = (int)(seed % (unsigned)(i + 1)), t = p->ordem[i];
    p->ordem[i] = p->ordem[j]; p->ordem[j] = t;
  }
  p->ativa = 1; p->desde = agora; p->faixa = p->ordem[0];
  return p->faixa;
}

int mus_previa_passo(MusPrevia *p, unsigned agora, double segundos, int pronta, int evento) {
  if (!p->ativa) return -1;
  if (evento >= MUS_PREV_ERRO || (!p->pronta && agora - p->desde > 15000u)) {
    mus_previa_parar(p); return -1;
  }
  if (pronta && !p->pronta) { p->pronta = 1; p->desde = agora; }
  if (evento == MUS_PREV_FIM || segundos >= 30.0 ||
      (p->pronta && agora - p->desde >= 30000u)) {
    if (++p->pos >= p->n) { mus_previa_parar(p); return -1; }
    p->faixa = p->ordem[p->pos]; p->pronta = 0; p->desde = agora;
  }
  return p->faixa;
}

void mus_cores(const unsigned char *px, int w, int h, int pitch, unsigned char *out) {
  // Bounded box sampling preserves separate fire/blue regions, once per decode.
  for (int y = 0; y < 18; y++) for (int x = 0; x < 32; x++) {
    unsigned sum[3] = {0}; int n = 0;
    if (px && w > 0 && h > 0 && pitch >= w * 4)
      for (int dy = -3; dy <= 3; dy++) for (int dx = -3; dx <= 3; dx++) {
        // Match the mock's 100px overscan before its static 90px blur.
        int sx = ((x * 1920 / 32 + 100 + dx * 60) * w) / 2120;
        int sy = ((y * 1080 / 18 + 100 + dy * 60) * h) / 1280;
        if (sx < 0) sx = 0; if (sx >= w) sx = w - 1;
        if (sy < 0) sy = 0; if (sy >= h) sy = h - 1;
        const unsigned char *q = px + sy * pitch + sx * 4;
        static const int weight[7] = {1,4,7,10,7,4,1};
        int wt = weight[dx+3] * weight[dy+3];
        for (int c = 0; c < 3; c++) sum[c] += q[c] * wt;
        n += wt;
      }
    float lum = n ? (sum[0] * .213f + sum[1] * .715f + sum[2] * .072f) / n : 13;
    float veil = .52f * (1.0f - .7f * y / 17.0f);
    for (int c = 0; c < 3; c++) {
      float v = n ? (lum + (sum[c] / (float)n - lum) * 1.65f) * veil : 13;
      if (n) v += (c == 0 ? 5 : c == 1 ? 8 : 15) * .7f * y / 17.0f;
      out[(y*32+x)*4+c] = (unsigned char)(v < 0 ? 0 : v > 255 ? 255 : v);
    }
    out[(y*32+x)*4+3] = 255;
  }
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
  // Sem a URL (ela leva o segredo do worker): id, se o pedido saiu e quantas
  // faixas vieram — o "nao aparece a secao" se distingue entre nao-pedido,
  // erro de rede e resposta vazia.
  printf("[musica] %s temp=%d pedido=%d faixas=%d\n", pd.imdb, pd.temp,
         NV_REC_URL[0] != 0, achou ? alb.n : 0);
  fflush(stdout);
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
