// Ver listasescrita.h.
#include "listasescrita.h"
#include "rede.h"
#include "trakt.h"
#include "idioma.h"
#include "ilha.h"
#include "ilhaacao.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

size_t lstesc_json_texto(char *dst, size_t n, const char *s) {
  size_t k = 0;
  if (!n) return 0;
  for (; s && *s; s++) {
    unsigned char c = (unsigned char)*s;
    const char *e = NULL;
    char u[8];
    if (c == '"') e = "\\\"";
    else if (c == '\\') e = "\\\\";
    else if (c == '\n') e = "\\n";
    else if (c == '\r') e = "\\r";
    else if (c == '\t') e = "\\t";
    else if (c < 0x20) { snprintf(u, sizeof u, "\\u%04x", c); e = u; }
    if (e) {
      size_t m = strlen(e);
      if (k + m + 1 > n) break;
      memcpy(dst + k, e, m); k += m;
    } else {
      if (k + 2 > n) break;
      dst[k++] = (char)c;
    }
  }
  dst[k] = 0;
  return k;
}

// Texto escapado entre aspas em `dst+k`, devolve o novo k ou -1 se nao coube.
static int aspas(char *dst, size_t n, size_t k, const char *s) {
  size_t m;
  if (k + 3 > n) return -1;
  dst[k++] = '"';
  m = lstesc_json_texto(dst + k, n - k - 1, s);
  // Cortou no meio? Entao nao coube: um corpo truncado grava lixo no Trakt.
  { size_t cheio = 0; const char *p;
    for (p = s; p && *p; p++) cheio += (*p == '"' || *p == '\\' || *p == '\n' || *p == '\r' || *p == '\t') ? 2 :
                                       ((unsigned char)*p < 0x20 ? 6 : 1);
    if (cheio != m) return -1; }
  k += m;
  dst[k++] = '"';
  dst[k] = 0;
  return (int)k;
}

int lstesc_corpo_criar(char *dst, size_t n, const char *nome, const char *desc, int privada) {
  int k;
  if (!nome || !*nome) return 0;
  k = snprintf(dst, n, "{\"name\":");
  if ((k = aspas(dst, n, (size_t)k, nome)) < 0) return 0;
  k += snprintf(dst + k, n - (size_t)k, ",\"description\":");
  if ((k = aspas(dst, n, (size_t)k, desc ? desc : "")) < 0) return 0;
  if ((size_t)k + 40 > n) return 0;
  snprintf(dst + k, n - (size_t)k, ",\"privacy\":\"%s\"}", privada ? "private" : "public");
  return 1;
}

int lstesc_corpo_descricao(char *dst, size_t n, const char *desc) {
  int k = snprintf(dst, n, "{\"description\":");
  if ((k = aspas(dst, n, (size_t)k, desc ? desc : "")) < 0 || (size_t)k + 2 > n) return 0;
  dst[k] = '}'; dst[k + 1] = 0;
  return 1;
}

int lstesc_corpo_nota(char *dst, size_t n, const char *nota) {
  int k = snprintf(dst, n, "{\"notes\":");
  if ((k = aspas(dst, n, (size_t)k, nota ? nota : "")) < 0 || (size_t)k + 2 > n) return 0;
  dst[k] = '}'; dst[k + 1] = 0;
  return 1;
}

int lstesc_corpo_item(char *dst, size_t n, const char *imdb, const char *tipo, const char *nota) {
  char id[40];
  int k;
  if (!imdb || imdb[0] != 't') return 0;
  { const char *dp = strchr(imdb, ':');   // "tt123:1:2" -> "tt123"
    size_t m = dp ? (size_t)(dp - imdb) : strlen(imdb);
    if (m >= sizeof id) return 0;
    memcpy(id, imdb, m); id[m] = 0; }
  k = snprintf(dst, n, "{\"%s\":[{\"ids\":{\"imdb\":\"%s\"}",
               tipo && !strcmp(tipo, "series") ? "shows" : "movies", id);
  if (k < 0 || (size_t)k >= n) return 0;
  if (nota && *nota) {
    k += snprintf(dst + k, n - (size_t)k, ",\"notes\":");
    if ((k = aspas(dst, n, (size_t)k, nota)) < 0) return 0;
  }
  if ((size_t)k + 4 > n) return 0;
  memcpy(dst + k, "}]}", 4);
  return 1;
}

int lstesc_corpo_ordem(char *dst, size_t n, const long *ids, int k) {
  size_t o;
  int i;
  if (!ids || k < 1) return 0;
  o = (size_t)snprintf(dst, n, "{\"rank\":[");
  for (i = 0; i < k; i++) {
    int m;
    if (ids[i] <= 0 || o >= n) return 0;
    m = snprintf(dst + o, n - o, "%s%ld", i ? "," : "", ids[i]);
    if (m < 0 || o + (size_t)m >= n) return 0;
    o += (size_t)m;
  }
  if (o + 3 > n) return 0;
  memcpy(dst + o, "]}", 3);
  return 1;
}

// --- envio ------------------------------------------------------------------
typedef struct {
  int qual;
  char metodo[8], url[120];
  char corpo[2400];   // ate ~200 ids na ordem; o nome/nota cabem folgados
} Envio;

static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
static volatile int estado, ultimo, vivo;
static LstescEnvio envioTeste;

static int envioReal(const char *metodo, const char *url, const char *corpo, char **resp) {
  const char *cab[5];
  char aut[200], chave[140];
  int status = 0, k;
#ifndef __EMSCRIPTEN__
  RedePedido q;
  RedeResposta r;
  int ok;
#endif
  *resp = NULL;
  if (!trakt_cabecalhos(cab, aut, sizeof aut, chave, sizeof chave)) return 0;
  for (k = 0; cab[k]; k++) ;
  cab[k++] = "Content-Type: application/json";
  cab[k] = NULL;
#ifdef __EMSCRIPTEN__
  if (!strcmp(metodo, "PUT"))
    *resp = rede_putar_st(url, 20, cab, corpo, &status);
  else
    *resp = rede_postar_st(url, 20, cab, corpo, &status);
#else
  memset(&q, 0, sizeof q);
  q.metodo = metodo; q.url = url; q.cabecalhos = cab;
  q.corpo = corpo; q.n_corpo = strlen(corpo);
  q.prazo_ms = 20000;
  memset(&r, 0, sizeof r);
  ok = rede_pedir(&q, &r);
  status = ok ? r.status : 0;
  if (ok && r.corpo) { *resp = strdup(r.corpo); }
  rede_resposta_limpar(&r);
#endif
  return status;
}

static void *fio(void *u) {
  Envio *e = (Envio *)u;
  char *resp = NULL;
  int st = (envioTeste ? envioTeste : envioReal)(e->metodo, e->url, e->corpo, &resp);
  int ok = st >= 200 && st < 300;
  printf("[listas] escrita %d %s -> %s (HTTP %d)\n", e->qual, e->metodo, ok ? "confirmada" : "falhou", st);
  fflush(stdout);
  free(resp);
  free(e);
  pthread_mutex_lock(&trava);
  estado = ok ? LSTESC_OK : LSTESC_FALHA;
  vivo = 0;
  pthread_mutex_unlock(&trava);
  return NULL;
}

static int disparar(int qual, const char *metodo, const char *url, const char *corpo) {
  Envio *e;
  pthread_t f;
  if (!envioTeste) { const char *cab[4]; char a[200], c[140];
    if (!trakt_cabecalhos(cab, a, sizeof a, c, sizeof c)) return 0; }
  pthread_mutex_lock(&trava);
  if (vivo) { pthread_mutex_unlock(&trava); return 0; }
  e = (Envio *)calloc(1, sizeof *e);
  if (!e) { pthread_mutex_unlock(&trava); return 0; }
  e->qual = qual;
  snprintf(e->metodo, sizeof e->metodo, "%s", metodo);
  snprintf(e->url, sizeof e->url, "%s", url);
  snprintf(e->corpo, sizeof e->corpo, "%s", corpo);
  vivo = 1; estado = LSTESC_PENDENTE; ultimo = qual;
  pthread_mutex_unlock(&trava);
  if (pthread_create(&f, NULL, fio, e) != 0) {
    free(e);
    pthread_mutex_lock(&trava); vivo = 0; estado = LSTESC_OCIOSO; pthread_mutex_unlock(&trava);
    return 0;
  }
  pthread_detach(f);
  return 1;
}

int lstesc_criar(const char *nome, const char *desc, int privada) {
  char c[1200];
  if (!lstesc_corpo_criar(c, sizeof c, nome, desc, privada)) return 0;
  return disparar(LSTESC_CRIAR, "POST", "https://api.trakt.tv/users/me/lists", c);
}

int lstesc_descricao(long id, const char *desc) {
  char c[900], u[100];
  if (id <= 0 || !lstesc_corpo_descricao(c, sizeof c, desc)) return 0;
  snprintf(u, sizeof u, "https://api.trakt.tv/users/me/lists/%ld", id);
  return disparar(LSTESC_DESCRICAO, "PUT", u, c);
}

int lstesc_adicionar(long id, const char *imdb, const char *tipo, const char *nota) {
  char c[900], u[100];
  if (id <= 0 || !lstesc_corpo_item(c, sizeof c, imdb, tipo, nota)) return 0;
  snprintf(u, sizeof u, "https://api.trakt.tv/users/me/lists/%ld/items", id);
  return disparar(LSTESC_ADICIONAR, "POST", u, c);
}

int lstesc_ordenar(long id, const long *itemIds, int k) {
  char c[2400], u[100];
  if (id <= 0 || !lstesc_corpo_ordem(c, sizeof c, itemIds, k)) return 0;
  snprintf(u, sizeof u, "https://api.trakt.tv/users/me/lists/%ld/items/reorder", id);
  return disparar(LSTESC_ORDENAR, "POST", u, c);
}

int lstesc_nota(long id, long itemId, const char *nota) {
  char c[900], u[120];
  if (id <= 0 || itemId <= 0 || !lstesc_corpo_nota(c, sizeof c, nota)) return 0;
  snprintf(u, sizeof u, "https://api.trakt.tv/users/me/lists/%ld/items/%ld", id, itemId);
  return disparar(LSTESC_NOTA, "PUT", u, c);
}

int lstesc_estado(void) { return estado; }
int lstesc_qual(void) { return ultimo; }
int lstesc_consumir(void) {
  int e;
  pthread_mutex_lock(&trava);
  e = estado;
  if (e == LSTESC_OK || e == LSTESC_FALHA) estado = LSTESC_OCIOSO;
  pthread_mutex_unlock(&trava);
  return e;
}
void lstesc_teste_envio(LstescEnvio f) { envioTeste = f; }
void lstesc_teste_esperar(void) { while (vivo) usleep(1000); }

// --- aviso na ilha ----------------------------------------------------------
static int  avisoPendente;
static char avisoTitulo[160], avisoLista[128], avisoCapa[300];

int lstesc_adicionar_titulo(const CatItem *ci, const LstLista *l) {
  if (!ci || !l || !lstesc_adicionar(l->traktId, ci->imdb, ci->tipo, NULL)) return 0;
  snprintf(avisoTitulo, sizeof avisoTitulo, "%s", ci->titulo);
  snprintf(avisoLista, sizeof avisoLista, "%s", l->titulo);
  snprintf(avisoCapa, sizeof avisoCapa, "%s", ci->poster);
  avisoPendente = 1;
  return 1;
}

void lstesc_passo(void) {
  int e;
  char frase[260];
  IlhaAcao a;
  if (!avisoPendente || lstesc_estado() == LSTESC_PENDENTE || lstesc_estado() == LSTESC_OCIOSO) return;
  e = lstesc_consumir();
  avisoPendente = 0;
  memset(&a, 0, sizeof a);
  if (e == LSTESC_OK) snprintf(frase, sizeof frase, i18n("Adicionado a %s"), avisoLista);
  else snprintf(frase, sizeof frase, "%s", i18n("Não foi possível adicionar à lista"));
  a.icone = e == LSTESC_OK ? "aj_bookmark" : NULL;
  a.frase = frase; a.titulo = avisoTitulo; a.onde = "";
  a.thumb = avisoCapa; a.tipo = e == LSTESC_OK ? ILHA_ACENTO : ILHA_ERRO;
  a.voar = e == LSTESC_OK;
  ilhaacao_feita(&a);
}
