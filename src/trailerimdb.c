#include "trailerimdb.h"
#include "rede.h"
#include "js.h"
#include "dados.h"
#include <SDL2/SDL.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

#define TR_MAX 32
typedef struct {
  char imdb[16];
  char url[1024];
  char nome[64];
  long expira;        // epoch; 0 = sem trailer (resposta negativa)
  int  emVoo, respondeu;
} Entrada;
static Entrada tab[TR_MAX];
static int nTab, prox;
static SDL_mutex *mtx;

static void trancar(void)   { if (!mtx) mtx = SDL_CreateMutex(); SDL_LockMutex(mtx); }
static void destrancar(void){ SDL_UnlockMutex(mtx); }

static Entrada *achar(const char *imdb) {
  int i;
  for (i = 0; i < nTab; i++) if (!strcmp(tab[i].imdb, imdb)) return &tab[i];
  return NULL;
}
static Entrada *reservar(const char *imdb) {
  Entrada *e = achar(imdb);
  if (e) return e;
  if (nTab < TR_MAX) e = &tab[nTab++];
  else { e = &tab[prox]; prox = (prox + 1) % TR_MAX; }
  memset(e, 0, sizeof *e);
  snprintf(e->imdb, sizeof e->imdb, "%s", imdb);
  return e;
}

// "Expires=1790035589" dentro da URL assinada.
static long expiraDe(const char *url) {
  const char *p = strstr(url, "Expires=");
  return p ? atol(p + 8) : 0;
}

static void caminhoDisco(const char *imdb, char *dst, unsigned tam) {
  char pasta[512];
  dst[0] = 0;
  if (!dados_caminho(pasta, sizeof pasta, "trailer")) return;
  mkdir(pasta, 0755);
  snprintf(dst, tam, "%s/%s", pasta, imdb);
}

static int lerDisco(Entrada *e) {
  char c[600], linha[1200];
  FILE *f;
  caminhoDisco(e->imdb, c, sizeof c);
  if (!c[0] || !(f = fopen(c, "r"))) return 0;
  // linha 1: url (ou "-" para "sem trailer" com validade de 1 dia), linha 2: nome, linha 3: expira
  if (!fgets(linha, sizeof linha, f)) { fclose(f); return 0; }
  linha[strcspn(linha, "\r\n")] = 0;
  snprintf(e->url, sizeof e->url, "%s", strcmp(linha, "-") ? linha : "");
  if (fgets(linha, sizeof linha, f)) { linha[strcspn(linha, "\r\n")] = 0; snprintf(e->nome, sizeof e->nome, "%s", linha); }
  if (fgets(linha, sizeof linha, f)) e->expira = atol(linha);
  fclose(f);
  return 1;
}
static void gravarDisco(const Entrada *e) {
  char c[600];
  FILE *f;
  caminhoDisco(e->imdb, c, sizeof c);
  if (!c[0] || !(f = fopen(c, "w"))) return;
  fprintf(f, "%s\n%s\n%ld\n", e->url[0] ? e->url : "-", e->nome, e->expira);
  fclose(f);
}

static int valido(const Entrada *e) {
  // Uma hora de folga: a URL nao pode vencer no meio do trailer.
  return e->respondeu && e->expira > (long)time(NULL) + 3600;
}

// Escolhe o MP4 de maior definicao ate 1080p dentro de playbackURLs.
static int escolher(const char *json, char *url, unsigned tamUrl, char *nome, unsigned tamNome) {
  const char *no = strstr(json, "\"node\"");
  const char *fim = json + strlen(json);
  const char *p;
  int melhor = 0;
  url[0] = 0;
  if (!no) return 0;
  { const char *n2 = strstr(no, "\"name\"");
    if (n2 && n2 < fim) js_texto(n2, fim, "value", nome, tamNome); }
  p = js_array(no, fim, "playbackURLs");
  for (; p; p = js_prox(js_fim(p))) {
    const char *pe = js_fim(p);
    char mime[16], def[16], u[1024];
    int alt;
    js_texto(p, pe, "videoMimeType", mime, sizeof mime);
    js_texto(p, pe, "videoDefinition", def, sizeof def);
    if (strcmp(mime, "MP4")) continue;
    alt = atoi(def + (strncmp(def, "DEF_", 4) ? 0 : 4));
    if (alt <= 0 || alt > 1080 || alt <= melhor) continue;
    if (!js_texto(p, pe, "url", u, sizeof u)) continue;
    // js_texto troca "\/" por "/"; a URL assinada tem "~" e "_", que passam.
    snprintf(url, tamUrl, "%s", u);
    melhor = alt;
  }
  return melhor;
}

static void *buscar(void *arg) {
  char imdb[16];
  char consulta[512], url[1024], nome[64] = "Trailer";
  char *corpo;
  static const char *const cabs[] = {
    "Accept: application/json",
    "Content-Type: application/json",
    "Referer: https://www.imdb.com/",
    NULL };
  Entrada *e;
  snprintf(imdb, sizeof imdb, "%s", (const char *)arg);
  free(arg);
  // %7B = {  %22 = "  %28 = (  %29 = )  %3A = :  %7D = }
  snprintf(consulta, sizeof consulta,
    "https://api.graphql.imdb.com/?query=%%7Btitle%%28id%%3A%%22%s%%22%%29%%7BprimaryVideos%%28first%%3A1%%29%%7Bedges%%7Bnode%%7Bname%%7Bvalue%%7DplaybackURLs%%7Burl%%20videoMimeType%%20videoDefinition%%7D%%7D%%7D%%7D%%7D%%7D",
    imdb);
  corpo = rede_baixar_com(consulta, 15, cabs);
  url[0] = 0;
  { int alt = corpo ? escolher(corpo, url, sizeof url, nome, sizeof nome) : 0;
    printf("[trailer] imdb %s: %s%s\n", imdb,
           !corpo ? "sem resposta" : alt ? "MP4 " : "sem trailer",
           alt ? (alt >= 1080 ? "1080p" : "720p") : "");
    fflush(stdout); }
  trancar();
  e = reservar(imdb);
  e->emVoo = 0; e->respondeu = 1;
  snprintf(e->url, sizeof e->url, "%s", url);
  snprintf(e->nome, sizeof e->nome, "%s", nome);
  // Sem resposta: nao grava (tenta de novo na proxima abertura). Sem trailer:
  // vale um dia. Com trailer: vale ate a assinatura vencer.
  e->expira = !corpo ? 0 : url[0] ? expiraDe(url) : (long)time(NULL) + 86400;
  if (corpo) gravarDisco(e);
  destrancar();
  free(corpo);
  return NULL;
}

void trailerimdb_pedir(const char *imdb) {
  Entrada *e;
  pthread_t f;
  char *arg;
  if (!imdb || strncmp(imdb, "tt", 2)) return;
  trancar();
  e = reservar(imdb);
  if (!e->respondeu && !e->emVoo && lerDisco(e)) e->respondeu = 1;
  if (e->emVoo || valido(e)) { destrancar(); return; }
  e->emVoo = 1; e->respondeu = 0;
  destrancar();
  arg = strdup(imdb);
  if (!arg) return;
  if (pthread_create(&f, NULL, buscar, arg) == 0) pthread_detach(f);
  else { free(arg); trancar(); e->emVoo = 0; destrancar(); }
}

const char *trailerimdb_url(const char *imdb, const char **nome) {
  Entrada *e;
  const char *r = NULL;
  if (!imdb || !imdb[0]) return NULL;
  trancar();
  e = achar(imdb);
  if (e && valido(e) && e->url[0]) { r = e->url; if (nome) *nome = e->nome; }
  destrancar();
  return r;
}

int trailerimdb_respondeu(const char *imdb) {
  Entrada *e;
  int r;
  if (!imdb || !imdb[0]) return 1;
  trancar();
  e = achar(imdb);
  r = !e || e->respondeu;
  destrancar();
  return r;
}
