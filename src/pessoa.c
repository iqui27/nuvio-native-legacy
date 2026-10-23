#include "pessoa.h"
#include "descoberta.h"
#include "rede.h"
#include "js.h"
#include "idbase.h"
int ajustes_tmdb_elenco(void);
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
static unsigned revisao, geracao;
static int falhou;
static char ctxImdb[24], ctxTipo[16];
typedef struct { long id; unsigned geracao; char nome[80], foto[200], imdb[24], tipo[16]; } Pedido;
static pthread_t fio;
static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;

void pessoa_contexto_titulo(const char *imdb, const char *tipo) {
  pthread_mutex_lock(&trava);
  snprintf(ctxImdb, sizeof ctxImdb, "%s", imdb ? imdb : "");
  snprintf(ctxTipo, sizeof ctxTipo, "%s", tipo ? tipo : "");
  pthread_mutex_unlock(&trava);
}

static void codificar(const char *s, char *dst, size_t cap) {
  size_t z = 0;
  for (const unsigned char *c = (const unsigned char *)s; *c && z + 4 < cap; c++) {
    if ((*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z') || (*c >= '0' && *c <= '9'))
      dst[z++] = *c;
    else if (*c == ' ') dst[z++] = '+';
    else { snprintf(dst + z, 4, "%%%02X", *c); z += 3; }
  }
  dst[z] = 0;
}

static char normDobra(unsigned char c) {
  switch (c) {
    case 0xA0: case 0xA1: return 'a'; case 0xA2: return 'a'; case 0xA3: return 'a';
    case 0xA7: return 'c'; case 0xA9: return 'e'; case 0xAA: return 'e';
    case 0xAD: return 'i'; case 0xB3: return 'o'; case 0xB5: return 'o';
    case 0xBA: return 'u'; default: return 0;
  }
}

static void normNome(const char *s, char *dst, size_t tam) {
  const unsigned char *p = (const unsigned char *)(s ? s : "");
  size_t k = 0;
  while (*p && k + 1 < tam) {
    unsigned char c = *p++;
    char o = 0;
    if (c >= 'A' && c <= 'Z') o = (char)(c + 32);
    else if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) o = (char)c;
    else if (c == 0xC3 && *p && (o = normDobra(*p))) p++;
    else if (c >= 0x80) { dst[k++] = (char)c; continue; }
    if (o) dst[k++] = o;
  }
  dst[k] = 0;
}

static int mesmoNome(const char *a, const char *b) {
  char na[64], nb[64];
  if (!a[0] || !b[0]) return 0;
  if (!strcmp(a, b)) return 1;
  normNome(a, na, sizeof na);
  normNome(b, nb, sizeof nb);
  return na[0] && !strcmp(na, nb);
}

static long resolverId(const char *nomeAlvo, const char *imdb, const char *tipo) {
  const char *chave = desc_chave_tmdb();
  char url[700], enc[300], *corpo = NULL;
  long idPessoa = 0, idTitulo = 0;
  int serie = !strcmp(tipo, "series");
  if (!chave || !chave[0] || !ajustes_tmdb_elenco() || !nomeAlvo[0]) return 0;
  if (idbase_e_imdb(imdb)) {
    snprintf(url, sizeof url, "%s/find/%s?api_key=%s&external_source=imdb_id",
             TMDB, imdb, chave);
    corpo = rede_baixar(url, 20);
    if (corpo) {
      const char *p = js_array(corpo, NULL, serie ? "tv_results" : "movie_results");
      if (p) idTitulo = (long)js_num(p, js_fim(p), "id", 0);
      free(corpo);
    }
  }
  if (idTitulo > 0) {
    snprintf(url, sizeof url, "%s/%s/%ld/credits?api_key=%s&language=%s",
             TMDB, serie ? "tv" : "movie", idTitulo, chave, desc_tmdb_idioma());
    corpo = rede_baixar(url, 20);
    if (corpo) {
      const char *p = js_array(corpo, NULL, "cast");
      while (p && !idPessoa) {
        const char *f = js_fim(p);
        char nome[96] = "";
        js_texto(p, f, "name", nome, sizeof nome);
        if (mesmoNome(nome, nomeAlvo)) idPessoa = (long)js_num(p, f, "id", 0);
        p = js_prox(f);
      }
      free(corpo);
    }
  }
  if (!idPessoa) {
    codificar(nomeAlvo, enc, sizeof enc);
    snprintf(url, sizeof url, "%s/search/person?api_key=%s&language=%s&query=%s",
             TMDB, chave, desc_tmdb_idioma(), enc);
    corpo = rede_baixar(url, 20);
    if (corpo) {
      const char *r = js_array(corpo, NULL, "results");
      int correspondencias = 0;
      while (r) {
        const char *fr = js_fim(r);
        char nome[96] = "";
        js_texto(r, fr, "name", nome, sizeof nome);
        if (mesmoNome(nome, nomeAlvo)) {
          if (++correspondencias > 1) { idPessoa = 0; break; }
          idPessoa = (long)js_num(r, fr, "id", 0);
        }
        r = js_prox(fr);
      }
      free(corpo);
    }
  }
  return idPessoa;
}

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

static void buscarPedido(const Pedido *pedido) {
  long id = pedido->id;
  const char *chave = desc_chave_tmdb();
  char url[700], *corpo;
  if (!chave || !*chave) return;
  if (id <= 0) id = resolverId(pedido->nome, pedido->imdb, pedido->tipo);
  pthread_mutex_lock(&trava);
  int atual = pedido->geracao == geracao;
  if (atual && id > 0) idPedido = id;
  pthread_mutex_unlock(&trava);
  if (!atual || id <= 0) return;
  snprintf(url, sizeof url, "%s/person/%ld?api_key=%s&language=%s&append_to_response=combined_credits,external_ids", TMDB, id, chave, desc_tmdb_idioma());
  corpo = rede_baixar(url, 20);
  if (!corpo) return;
  char hoje[11]; time_t agora = time(NULL); struct tm local;
  localtime_r(&agora, &local); strftime(hoje, sizeof hoje, "%Y-%m-%d", &local);
  pthread_mutex_lock(&trava); atual = pedido->geracao == geracao; pthread_mutex_unlock(&trava);
  if (atual && pessoa_ler(id, corpo, hoje) && (!bio || !bio[0])) {
    snprintf(url, sizeof url, "%s/person/%ld?api_key=%s&language=en-US", TMDB, id, chave);
    char *en = rede_baixar(url, 20);
    if (en) {
      pthread_mutex_lock(&trava);
      if (pedido->geracao == geracao) {
        char *trad = calloc(1, strlen(en) + 1);
        if (trad) { js_texto_raiz(en, "biography", trad, strlen(en) + 1); free(bio); bio = trad; bioIngles = bio[0] != 0; revisao++; }
      }
      pthread_mutex_unlock(&trava); free(en);
    }
  }
  free(corpo);
}
static void *buscar(void *arg) {
  (void)arg;
  pthread_mutex_lock(&trava);
  for (;;) {
    Pedido pedido = { .id = idPedido, .geracao = geracao };
    snprintf(pedido.nome, sizeof pedido.nome, "%s", nomeSemente);
    snprintf(pedido.foto, sizeof pedido.foto, "%s", fotoSemente);
    snprintf(pedido.imdb, sizeof pedido.imdb, "%s", ctxImdb);
    snprintf(pedido.tipo, sizeof pedido.tipo, "%s", ctxTipo);
    pthread_mutex_unlock(&trava);
    buscarPedido(&pedido);
    pthread_mutex_lock(&trava);
    if (pedido.geracao != geracao) continue;
    falhou = !pronta;
    fioVivo = 0;
    pthread_mutex_unlock(&trava);
    return NULL;
  }
}

void pessoa_pedir(long tmdbId, const char *nomeConhecido, const char *fotoConhecida) {
  if (tmdbId <= 0 && !(nomeConhecido && nomeConhecido[0])) return;
  pthread_mutex_lock(&trava);
  if (tmdbId > 0 && idPedido == tmdbId && pronta) { pthread_mutex_unlock(&trava); return; }
  geracao++;
  idPedido = tmdbId;
  nCred = 0; pronta = 0; falhou = 0; bioIngles = 0; revisao++;
  free(bio); bio = NULL;
  area[0] = nasc[0] = morte[0] = lugar[0] = outros[0] = wiki[0] = 0;
  // O nome e a foto que o elenco ja tinha entram na hora, para a tela abrir
  // com conteudo em vez de vazia enquanto a rede responde.
  snprintf(nomeSemente, sizeof nomeSemente, "%s", nomeConhecido ? nomeConhecido : "");
  snprintf(fotoSemente, sizeof fotoSemente, "%s", fotoConhecida ? fotoConhecida : "");
  snprintf(nome, sizeof nome, "%s", nomeSemente);
  snprintf(foto, sizeof foto, "%s", fotoSemente);
  // One worker drains the latest selection; older results cannot publish.
  if (!fioVivo) {
    fioVivo = 1;
    if (pthread_create(&fio, NULL, buscar, NULL) != 0) {
      fioVivo = 0;
      falhou = 1;
    } else pthread_detach(fio);
  }
  pthread_mutex_unlock(&trava);
}

int pessoa_falhou(void) { pthread_mutex_lock(&trava); int f = falhou; pthread_mutex_unlock(&trava); return f; }
int pessoa_pronta(void) { return pronta; }
long pessoa_id(void) { return idAtual; }
unsigned pessoa_revisao(void) { return revisao; }
const char *pessoa_nome(void) { return nome; }
const char *pessoa_foto(void) { return foto; }
char *pessoa_bio(void) {
  pthread_mutex_lock(&trava);
  char *snapshot = strdup(bio ? bio : "");
  pthread_mutex_unlock(&trava);
  return snapshot;
}
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
