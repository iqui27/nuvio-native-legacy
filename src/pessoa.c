#include "pessoa.h"
#include "descoberta.h"
#include "rede.h"
#include "js.h"
#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define TMDB "https://api.themoviedb.org/3"
#define IMG "https://image.tmdb.org/t/p/w342"

static FmCredito cred[PES_MAX];
static int nCred;
static char nome[80], foto[200], area[24], nasc[16], morte[16], lugar[120], outros[240], wiki[24];
static char *bio;
static long idPedido, idEmCurso, idAtual;
static char nomeSemente[80], fotoSemente[200];
static int pronta, fioVivo, bioIngles;
static unsigned revisao;
static pthread_t fio;
static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;

// known_for_department comes in English from TMDB even with language=pt-BR.
static const char *traduzArea(const char *s) {
  if (!strcmp(s, "Acting"))    return "Atua\xc3\xa7\xc3\xa3" "o";
  if (!strcmp(s, "Directing")) return "Dire\xc3\xa7\xc3\xa3" "o";
  if (!strcmp(s, "Writing"))   return "Roteiro";
  if (!strcmp(s, "Production"))return "Produ\xc3\xa7\xc3\xa3" "o";
  if (!strcmp(s, "Sound"))     return "Som";
  if (!strcmp(s, "Camera"))    return "Fotografia";
  return s;
}

static int temNumero(const char *p, const char *f, const char *chave, const char *const *alvos, int n) {
  const char *a = js_array(p, f, chave);
  if (!a) return 0;
  const char *e = js_fim(a);
  for (int i = 0; i < n; i++) {
    size_t l = strlen(alvos[i]);
    for (const char *q = a; q + l <= e; q++) if (!memcmp(q, alvos[i], l)) return 1;
  }
  return 0;
}

// One cast or crew array of combined_credits into raw rows.
static int linhas(const char *corpo, const char *chave, int papel, FmBruto *b, int n, int max) {
  static const char *const talk[] = { "10767", "10763", "10764" };
  const char *p = js_array(corpo, NULL, chave);
  while (p && n < max) {
    const char *f = js_fim(p);
    FmBruto *r = &b[n];
    char data[16] = "", tipo[8] = "", job[40] = "", caminho[128] = "";
    memset(r, 0, sizeof *r);
    if (papel == FM_DIR || papel == FM_ROT) {
      js_texto(p, f, "job", job, sizeof job);
      if (!strcmp(job, "Director")) r->papel = FM_DIR;
      else if (!strcmp(job, "Writer") || !strcmp(job, "Screenplay") || !strcmp(job, "Story") ||
               !strcmp(job, "Teleplay") || !strcmp(job, "Creator")) r->papel = FM_ROT;
    } else r->papel = FM_ATUA;
    if (r->papel) {
      if (!js_texto(p, f, "title", r->titulo, sizeof r->titulo)) js_texto(p, f, "name", r->titulo, sizeof r->titulo);
      js_texto(p, f, "character", r->personagem, sizeof r->personagem);
      if (!js_texto(p, f, "release_date", data, sizeof data)) js_texto(p, f, "first_air_date", data, sizeof data);
      if (strlen(data) >= 4) r->ano = atoi(data);
      if (strlen(data) == 10) memcpy(r->lancamento, data, 11);
      js_texto(p, f, "media_type", tipo, sizeof tipo);
      r->tv = !strcmp(tipo, "tv");
      r->tmdb = (long)js_num(p, f, "id", 0.0);
      r->ordem = (int)js_num(p, f, "order", 99);
      r->votos = (int)js_num(p, f, "vote_count", 0);
      r->eps = (int)js_num(p, f, "episode_count", 0);
      r->talk = temNumero(p, f, "genre_ids", talk, 3);
      if (js_texto(p, f, "poster_path", caminho, sizeof caminho) && caminho[0] == '/')
        snprintf(r->poster, sizeof r->poster, IMG "%s", caminho);
      if (r->tmdb && r->titulo[0]) n++;
    }
    p = js_prox(f);
  }
  return n;
}

int pessoa_ler(long id, const char *corpo, const char *hoje) {
  char n[80] = "", f[160] = "", a[24] = "", nc[16] = "", mo[16] = "", lu[120] = "", wk[24] = "", caminho[128] = "";
  char ou[240] = "";
  FmBruto *raw = calloc(FM_MAX * 2, sizeof *raw);
  FmCredito *out = calloc(PES_MAX, sizeof *out);
  int k, nb;
  size_t bioTam = strlen(corpo) + 1;
  char *b = calloc(1, bioTam);
  if (!raw || !out || !b) { free(raw); free(out); free(b); return 0; }
  js_texto_raiz(corpo, "name", n, sizeof n);
  js_texto_raiz(corpo, "biography", b, bioTam);
  js_texto_raiz(corpo, "known_for_department", a, sizeof a);
  js_texto_raiz(corpo, "birthday", nc, sizeof nc);
  js_texto_raiz(corpo, "deathday", mo, sizeof mo);
  js_texto_raiz(corpo, "place_of_birth", lu, sizeof lu);
  js_texto(corpo, NULL, "wikidata_id", wk, sizeof wk);
  if (js_texto_raiz(corpo, "profile_path", caminho, sizeof caminho) && caminho[0] == '/') snprintf(f, sizeof f, IMG "%s", caminho);
  { const char *p = js_array(corpo, NULL, "also_known_as");
    size_t u = 0;
    while (p) {
      char t[60] = "";
      js_cadeia(p, t, sizeof t);
      if (t[0] && u + strlen(t) + 4 < sizeof ou) u += (size_t)snprintf(ou + u, sizeof ou - u, "%s%s", u ? " \xc2\xb7 " : "", t);
      { const char *e = p + 1;
        while (*e) { if (*e == '\\' && e[1]) e += 2; else if (*e++ == '"') break; }
        p = js_prox(e); }
    } }
  nb = linhas(corpo, "cast", FM_ATUA, raw, 0, FM_MAX * 2);
  nb = linhas(corpo, "crew", FM_DIR, raw, nb, FM_MAX * 2);
  k = fm_juntar(raw, nb, hoje, out, PES_MAX);
  pthread_mutex_lock(&trava);
  if (idPedido && idPedido != id) {
    pthread_mutex_unlock(&trava);
    free(raw); free(out); free(b);
    return 0;
  }
  idAtual = id;
  snprintf(nome, sizeof nome, "%s", n[0] ? n : nomeSemente);
  snprintf(foto, sizeof foto, "%s", f[0] ? f : fotoSemente);
  free(bio);
  bio = b;
  snprintf(area, sizeof area, "%s", a[0] ? traduzArea(a) : "");
  snprintf(nasc, sizeof nasc, "%s", nc);
  snprintf(morte, sizeof morte, "%s", mo);
  snprintf(lugar, sizeof lugar, "%s", lu);
  snprintf(outros, sizeof outros, "%s", ou);
  snprintf(wiki, sizeof wiki, "%s", wk);
  bioIngles = 0;
  memcpy(cred, out, (size_t)k * sizeof *out);
  nCred = k;
  pronta = 1;
  revisao++;
  pthread_mutex_unlock(&trava);
  free(raw); free(out);
  return 1;
}

static void *buscar(void *arg) {
  char url[400], *corpo;
  long id;
  const char *chave = desc_chave_tmdb();
  (void)arg;
  pthread_mutex_lock(&trava);
  id = idEmCurso;
  pthread_mutex_unlock(&trava);
  if (!chave || !chave[0]) goto fim;
  // Same call as the web (castDetailScreen.js:204): details and combined credits at once.
  snprintf(url, sizeof url, "%s/person/%ld?api_key=%s&language=%s&append_to_response=combined_credits,external_ids",
           TMDB, id, chave, desc_tmdb_idioma());
  corpo = rede_baixar(url, 20);
  if (!corpo) goto fim;
  pthread_mutex_lock(&trava);
  int atual = id == idPedido;
  pthread_mutex_unlock(&trava);
  char hoje[11];
  time_t agora = time(NULL);
  struct tm local;
  localtime_r(&agora, &local);
  strftime(hoje, sizeof hoje, "%Y-%m-%d", &local);
  if (atual && pessoa_ler(id, corpo, hoje) && (!bio || !bio[0])) {
    char *en;
    snprintf(url, sizeof url, "%s/person/%ld?api_key=%s&language=en-US", TMDB, id, chave);
    en = rede_baixar(url, 20);
    if (en) {
      pthread_mutex_lock(&trava);
      if (id == idPedido) { char *trad = calloc(1, strlen(en) + 1); if (trad) { js_texto_raiz(en, "biography", trad, strlen(en) + 1); free(bio); bio = trad; bioIngles = bio[0] != 0; revisao++; } }
      pthread_mutex_unlock(&trava);
      free(en);
    }
  }
  printf("[pessoa] %ld -> %d creditos\n", id, nCred); fflush(stdout);
  free(corpo);
fim:
  pthread_mutex_lock(&trava);
  fioVivo = 0;
  pthread_mutex_unlock(&trava);
  return NULL;
}

void pessoa_pedir(long tmdbId, const char *nomeConhecido, const char *fotoConhecida) {
  if (tmdbId <= 0) return;
  pthread_mutex_lock(&trava);
  if (idPedido == tmdbId) { pthread_mutex_unlock(&trava); return; }
  idPedido = tmdbId;
  nCred = 0; pronta = 0; bioIngles = 0; revisao++;
  free(bio); bio = NULL;
  area[0] = nasc[0] = morte[0] = lugar[0] = outros[0] = wiki[0] = 0;
  // The name and photo the cast row already had show at once, so the screen opens with content.
  snprintf(nomeSemente, sizeof nomeSemente, "%s", nomeConhecido ? nomeConhecido : "");
  snprintf(fotoSemente, sizeof fotoSemente, "%s", fotoConhecida ? fotoConhecida : "");
  snprintf(nome, sizeof nome, "%s", nomeSemente);
  snprintf(foto, sizeof foto, "%s", fotoSemente);
  if (fioVivo) { pthread_mutex_unlock(&trava); return; }
  idEmCurso = tmdbId;
  fioVivo = 1;
  pthread_mutex_unlock(&trava);
  if (pthread_create(&fio, NULL, buscar, NULL) != 0) fioVivo = 0;
  else pthread_detach(fio);
}

int pessoa_pronta(void) { return pronta; }
long pessoa_id(void) { return idAtual; }
unsigned pessoa_revisao(void) { return revisao; }
const char *pessoa_nome(void) { return nome; }
const char *pessoa_foto(void) { return foto; }
const char *pessoa_bio(void) { return bio ? bio : ""; }
const char *pessoa_area(void) { return area; }
const char *pessoa_nascimento(void) { return nasc; }
const char *pessoa_morte(void) { return morte; }
const char *pessoa_lugar(void) { return lugar; }
const char *pessoa_outros(void) { return outros; }
const char *pessoa_wikidata(void) { return wiki; }
int pessoa_bio_ingles(void) { return bioIngles; }
const FmCredito *pessoa_creditos(void) { return cred; }
int pessoa_n_creditos(void) { return nCred; }
const char *pessoa_credito_titulo(int i) { return i >= 0 && i < nCred ? cred[i].titulo : ""; }
const char *pessoa_credito_papel(int i) { return i >= 0 && i < nCred ? cred[i].personagem : ""; }
const char *pessoa_credito_ano(int i) {
  static char b[8];
  if (i < 0 || i >= nCred || !cred[i].ano) return "";
  snprintf(b, sizeof b, "%d", cred[i].ano);
  return b;
}
const char *pessoa_credito_poster(int i) { return i >= 0 && i < nCred ? cred[i].poster : ""; }
const char *pessoa_credito_imdb(int i) { return i >= 0 && i < nCred ? cred[i].imdb : ""; }
long pessoa_credito_tmdb(int i) { return i >= 0 && i < nCred ? cred[i].tmdb : 0; }
const char *pessoa_credito_tipo(int i) { return i >= 0 && i < nCred ? (cred[i].tv ? "tv" : "movie") : ""; }
