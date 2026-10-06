// Plex client. Contract, secrets and limits in plex.h; research in
// docs/plans/media-servers-1.8/README.md.
//
// Routes (all JSON, "Accept: application/json"):
//   plex.tv  POST /api/v2/pins?strong=false        -> {id, code, expiresIn}
//            GET  /api/v2/pins/{id}                -> {authToken} once linked
//            GET  /api/v2/resources                -> servers + connections
//            GET  /api/v2/user                     -> display name
//   server   GET  /identity, /library/sections, /library/sections/{k}/all,
//            /library/metadata/{rk}[/allLeaves], /:/timeline,
//            /video/:/transcode/universal/{decision,start.m3u8,stop}
// None of this was exercised against a real Plex Media Server here: the
// shapes follow Plex's public API documentation and community-documented
// behaviour, and are pinned by fixture tests (tests/plex.c).
#include "plex.h"
#include "dados.h"
#include "js.h"
#include "jsraiz.h"
#include "jsw.h"
#include "jellyfin.h"   // jellyfin_backend(): same backend naming everywhere
#include "perfis.h"
#include "srvidx.h"
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>

#ifndef NV_PX_VERSAO
#define NV_PX_VERSAO "1.8.0"
#endif
#define PX_ARQ_FMT "plex-p%d.txt"
// plex.tv. A build-time override exists only so tests/plex.sh can point it at a
// local fake; no environment variable or setting can redirect it.
#ifndef NV_PX_TV
#define NV_PX_TV "https://plex.tv"
#endif
#define PX_TV NV_PX_TV
#define PX_PRAZO_MS 15000u
#define PX_PRAZO_LISTA_MS 20000u
#define PX_PRAZO_SONDA_MS 4000u
#define PX_CORPO_MAX (6u * 1024u * 1024u)
#define PX_SESSOES 16
#define PX_FILA_CTL 8
#define PX_FILA_REL 32
#define PX_PAGINA_INDICE 200
#define PX_TAXA_MAX 20000   // kbps ceiling asked of the universal transcoder

// ------------------------------------------------------------ PIN machine
void pxpin_iniciar(PxPin *p) {
  apagarSegredo(p, sizeof *p);
  p->est = PXP_OCIOSO;
}

int pxpin_criado(PxPin *p, unsigned long long agora, const char *id, const char *codigo,
                 int expiraSeg) {
  pxpin_iniciar(p);
  if (!id || !*id || !codigo || !*codigo) return 0;
  copiar(p->id, sizeof p->id, id);
  copiar(p->codigo, sizeof p->codigo, codigo);
  p->expiraMs = agora + (unsigned long long)(expiraSeg > 0 ? expiraSeg : PXPIN_TETO_PADRAO_S) * 1000ull;
  p->proximoMs = agora + PXPIN_INTERVALO_MS;
  p->est = PXP_ESPERANDO;
  return 1;
}

int pxpin_vencido(const PxPin *p, unsigned long long agora) {
  return p->est == PXP_ESPERANDO && agora >= p->proximoMs;
}

void pxpin_relogio(PxPin *p, unsigned long long agora) {
  if (p->est == PXP_ESPERANDO && agora >= p->expiraMs) p->est = PXP_EXPIRADO;
}

void pxpin_resultado(PxPin *p, unsigned long long agora, int resp, const char *token) {
  if (p->est != PXP_ESPERANDO) return;   // a late answer never revives a finished pin
  if (resp == 1 && token && *token) {
    copiar(p->token, sizeof p->token, token);
    p->est = PXP_LIGADO;
    return;
  }
  if (resp == 0 || resp == 1) { p->falhas = 0; p->proximoMs = agora + PXPIN_INTERVALO_MS; }
  else if (resp == PX_ERR_EXPIRADO) p->est = PXP_EXPIRADO;
  else if (++p->falhas >= PXPIN_FALHAS_MAX) p->est = PXP_ERRO;
  else p->proximoMs = agora + PXPIN_INTERVALO_MS;
  pxpin_relogio(p, agora);
}

void pxpin_cancelar(PxPin *p) {
  if (p->est == PXP_ESPERANDO) p->est = PXP_CANCELADO;
  if (p->est != PXP_LIGADO) apagarSegredo(p->token, sizeof p->token);
}

// ------------------------------------------------------------ small helpers
long long px_ms(double seg) {
  if (!(seg > 0.0)) return 0;
  if (seg > 9.0e9) seg = 9.0e9;
  return (long long)(seg * 1000.0 + 0.5);
}

const char *px_metodo_nome(int m) {
  return m == PX_METODO_DIRETO ? "DirectPlay" : m == PX_METODO_STREAM ? "DirectStream" : "Transcode";
}

static int soDigitos(const char *s, size_t max) {
  size_t n = 0;
  for (; s && *s; s++, n++) if (*s < '0' || *s > '9') return 0;
  return n > 0 && n <= max;
}

// Percent-encodes everything but unreserved characters (used for the "url="
// and "key=" values: a '/' inside a query value must be %2F).
static void codificar(char *dst, size_t tam, const char *s) {
  size_t k = 0;
  for (; s && *s && k + 4 < tam; s++) {
    unsigned char c = (unsigned char)*s;
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') dst[k++] = (char)c;
    else k += (size_t)snprintf(dst + k, tam - k, "%%%02X", c);
  }
  if (tam) dst[k] = 0;
}

// Values inside X-Plex-* headers: printable ASCII, no CR/LF.
static void campoCab(char *dst, size_t tam, const char *src) {
  size_t k = 0;
  for (; src && *src && k + 1 < tam; src++) {
    unsigned char c = (unsigned char)*src;
    dst[k++] = (c < 32 || c == 127 || c >= 128) ? '_' : (char)c;
  }
  if (tam) dst[k] = 0;
}

static const char *nomeBackend(void) { return jellyfin_backend_nome(jellyfin_backend()); }

static int hexLimpo(const char *s, size_t max) {
  size_t n = 0;
  for (; s && *s; s++, n++) {
    char c = *s;
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) return 0;
  }
  return n > 0 && n <= max;
}

// ratingKey / section key: Plex sends them as strings, some builds as numbers.
static int chaveTxt(const char *ini, const char *fim, const char *chave, char *dst, size_t tam) {
  const char *v = valorRaiz(ini, fim, chave);
  if (!v) return 0;
  if (*v == '"') return js_cadeia(v, dst, tam) && soDigitos(dst, 20);
  if (*v >= '0' && *v <= '9') {
    long long n = (long long)strtod(v, NULL);
    snprintf(dst, tam, "%lld", n);
    return 1;
  }
  return 0;
}

static int arrayDe(const char *corpo, size_t n, const char *chave, const char **ai, const char **af) {
  const char *mi, *mf;
  if (!blocoRaiz(corpo, corpo + n, "MediaContainer", &mi, &mf)) return 0;
  return blocoRaiz(mi, mf, chave, ai, af);
}

// ------------------------------------------------------------------ parsers
int px_ler_pin(const char *corpo, size_t n, char *id, size_t ni, char *codigo, size_t nc,
               int *expiraSeg, char *token, size_t nt) {
  const char *fim = corpo + n;
  double idn, exp;
  char tmp[160] = "";
  if (id && ni) id[0] = 0;
  if (codigo && nc) codigo[0] = 0;
  if (token && nt) token[0] = 0;
  if (expiraSeg) *expiraSeg = 0;
  if (!corpo) return PX_ERR_FORMATO;
  idn = numRaiz(corpo, fim, "id", -1);
  if (idn < 0 || idn > 9.0e15) return PX_ERR_FORMATO;
  if (id && ni) snprintf(id, ni, "%.0f", idn);
  if (codigo && nc) txtRaiz(corpo, fim, "code", codigo, nc);
  exp = numRaiz(corpo, fim, "expiresIn", 0);
  if (expiraSeg && exp > 0 && exp < 86400.0 * 7) *expiraSeg = (int)exp;
  if (txtRaiz(corpo, fim, "authToken", tmp, sizeof tmp) && tmp[0] && token && nt) {
    if (strlen(tmp) >= nt) { apagarSegredo(tmp, sizeof tmp); return PX_ERR_FORMATO; }
    copiar(token, nt, tmp);
    apagarSegredo(tmp, sizeof tmp);
    return 1;
  }
  apagarSegredo(tmp, sizeof tmp);
  return 0;
}

int px_ler_recursos(const char *corpo, size_t n, PxServidor *out, int max) {
  const char *fim = corpo + n, *p = corpo;
  int ns = 0;
  while (p < fim && *p != '[') p++;
  if (p >= fim) return PX_ERR_FORMATO;
  while (ns < max && (p = elemento(p, fim)) != NULL) {
    const char *f = depoisElemento(p), *ci, *cf, *q;
    char prov[96] = "";
    PxServidor *s = &out[ns];
    if (!f) break;
    if (*p == '{') {
      txtRaiz(p, f, "provides", prov, sizeof prov);
      if (strstr(prov, "server")) {
        memset(s, 0, sizeof *s);
        txtRaiz(p, f, "name", s->nome, sizeof s->nome);
        txtRaiz(p, f, "clientIdentifier", s->id, sizeof s->id);
        txtRaiz(p, f, "productVersion", s->versao, sizeof s->versao);
        txtRaiz(p, f, "accessToken", s->token, sizeof s->token);
        s->dono = boolRaiz(p, f, "owned", 0);
        if (hexLimpo(s->id, 47) && blocoRaiz(p, f, "connections", &ci, &cf)) {
          q = ci + 1;
          while (s->nCon < PX_CON_MAX && (q = elemento(q, cf)) != NULL) {
            const char *g = depoisElemento(q);
            PxConexao *k = &s->con[s->nCon];
            char proto[8] = "";
            if (!g) break;
            if (*q == '{') {
              memset(k, 0, sizeof *k);
              txtRaiz(q, g, "uri", k->uri, sizeof k->uri);
              txtRaiz(q, g, "address", k->addr, sizeof k->addr);
              txtRaiz(q, g, "protocol", proto, sizeof proto);
              k->porta = (int)numRaiz(q, g, "port", 0);
              k->local = boolRaiz(q, g, "local", 0);
              k->relay = boolRaiz(q, g, "relay", 0);
              k->https = !strcmp(proto, "https") || !strncmp(k->uri, "https://", 8);
              if (!strncmp(k->uri, "http://", 7) || !strncmp(k->uri, "https://", 8)) s->nCon++;
            }
            q = g;
          }
        }
        if (s->id[0] && s->nCon > 0) ns++;
      }
    }
    p = f;
  }
  return ns;
}

static int ipv4(const char *s) {
  int pontos = 0;
  if (!s || !*s) return 0;
  for (; *s; s++) {
    if (*s == '.') pontos++;
    else if (*s < '0' || *s > '9') return 0;
  }
  return pontos == 3;
}

static int jaTem(char out[][320], int n, const char *u) {
  int i;
  for (i = 0; i < n; i++) if (!strcmp(out[i], u)) return 1;
  return 0;
}

int px_candidatas(const PxServidor *s, char out[][320], int max) {
  int n = 0, passo, i;
  // 0: LAN, plain http to the raw address (works even where the router blocks
  //    plex.direct DNS answers: "DNS rebinding protection");
  // 1: LAN, the advertised uri; 2: remote direct; 3: relay (capped by Plex).
  for (passo = 0; passo < 4; passo++) {
    for (i = 0; i < s->nCon && n < max; i++) {
      const PxConexao *k = &s->con[i];
      char u[320];
      if (passo == 0) {
        if (!k->local || k->relay || !ipv4(k->addr) || k->porta <= 0 || k->porta > 65535) continue;
        snprintf(u, sizeof u, "http://%s:%d", k->addr, k->porta);
      } else {
        if (passo == 1 && !(k->local && !k->relay)) continue;
        if (passo == 2 && (k->local || k->relay)) continue;
        if (passo == 3 && !k->relay) continue;
        snprintf(u, sizeof u, "%s", k->uri);
        { size_t l = strlen(u); while (l && u[l - 1] == '/') u[--l] = 0; }
      }
      if (!jaTem(out, n, u)) { snprintf(out[n], 320, "%s", u); n++; }
    }
  }
  return n;
}

int px_ler_identidade(const char *corpo, size_t n, char *id, size_t ni, char *versao, size_t nv) {
  const char *mi, *mf;
  if (id && ni) id[0] = 0;
  if (versao && nv) versao[0] = 0;
  if (!blocoRaiz(corpo, corpo + n, "MediaContainer", &mi, &mf)) return PX_ERR_FORMATO;
  if (id) txtRaiz(mi, mf, "machineIdentifier", id, ni);
  if (versao) txtRaiz(mi, mf, "version", versao, nv);
  return id && id[0] ? PX_OK : PX_ERR_FORMATO;
}

int px_ler_usuario(const char *corpo, size_t n, char *nome, size_t tam) {
  const char *fim = corpo + n;
  if (tam) nome[0] = 0;
  // Display name only: "title" is the profile name, "username" the login
  // handle. The e-mail field is deliberately never read.
  if (!txtRaiz(corpo, fim, "title", nome, tam) || !nome[0]) txtRaiz(corpo, fim, "username", nome, tam);
  return nome[0] ? PX_OK : PX_ERR_FORMATO;
}

int px_ler_bibliotecas(const char *corpo, size_t n, PxBiblioteca *out, int max) {
  const char *ai, *af, *p;
  int k = 0;
  if (!arrayDe(corpo, n, "Directory", &ai, &af)) return 0;
  p = ai + 1;
  while (k < max && (p = elemento(p, af)) != NULL) {
    const char *f = depoisElemento(p);
    char id[24] = "", tipo[16] = "", nome[96] = "";
    if (!f) break;
    if (*p == '{') {
      chaveTxt(p, f, "key", id, sizeof id);
      txtRaiz(p, f, "type", tipo, sizeof tipo);
      txtRaiz(p, f, "title", nome, sizeof nome);
      if (id[0] && (!strcmp(tipo, "movie") || !strcmp(tipo, "show"))) {
        copiar(out[k].id, sizeof out[k].id, id);
        copiar(out[k].nome, sizeof out[k].nome, nome);
        copiar(out[k].tipo, sizeof out[k].tipo, tipo[0] == 'm' ? "movie" : "series");
        k++;
      }
    }
    p = f;
  }
  return k;
}

// Artwork through the server's photo transcoder: a poster at its original size
// is several MB on a TV. The path travels percent-encoded inside url=.
static void imagem(const PxConta *c, const char *caminho, int w, int h, char *dst, size_t tam) {
  char enc[400];
  dst[0] = 0;
  if (!caminho || caminho[0] != '/' || strlen(caminho) > 300) return;
  codificar(enc, sizeof enc, caminho);
  snprintf(dst, tam, "%s/photo/:/transcode?width=%d&height=%d&minSize=1&upscale=0&url=%s&X-Plex-Token=%s",
           c->base, w, h, enc, c->token);
}

static void guidsDe(const char *ini, const char *fim, char imdb[16], long *tmdb, long *tvdb) {
  const char *gi, *gf, *p;
  imdb[0] = 0;
  *tmdb = 0;
  if (tvdb) *tvdb = 0;
  if (!blocoRaiz(ini, fim, "Guid", &gi, &gf)) return;
  p = gi + 1;
  while ((p = elemento(p, gf)) != NULL) {
    const char *f = depoisElemento(p);
    char id[80] = "";
    if (!f) break;
    if (*p == '{' && txtRaiz(p, f, "id", id, sizeof id)) {
      if (!strncmp(id, "imdb://tt", 9) && strlen(id + 7) < 12 && soDigitos(id + 9, 9)) copiar(imdb, 12, id + 7);
      else if (!strncmp(id, "tmdb://", 7) && soDigitos(id + 7, 9)) *tmdb = atol(id + 7);
      else if (tvdb && !strncmp(id, "tvdb://", 7) && soDigitos(id + 7, 9)) *tvdb = atol(id + 7);
    }
    p = f;
  }
}

// One index row from a listing item (movie, show or episode). Only ids, the
// title hash, year, quality and the watched flag are kept.
static int linhaIndice(const char *p, const char *f, const char *serieRk, SrvIdxItem *e) {
  char tipo[16] = "", titulo[200] = "", res[16] = "";
  const char *mi, *mf, *q;
  double h = 0;
  memset(e, 0, sizeof *e);
  txtRaiz(p, f, "type", tipo, sizeof tipo);
  if (!strcmp(tipo, "movie")) e->tipo = 'm';
  else if (!strcmp(tipo, "show")) e->tipo = 's';
  else if (!strcmp(tipo, "episode")) e->tipo = 'e';
  else return 0;
  if (!chaveTxt(p, f, "ratingKey", e->item, sizeof e->item)) return 0;
  guidsDe(p, f, e->imdb, &e->tmdb, &e->tvdb);
  txtRaiz(p, f, "title", titulo, sizeof titulo);
  e->ano = (short)numRaiz(p, f, "year", 0);
  e->atualizado = (long long)numRaiz(p, f, "updatedAt", 0);
  e->visto = numRaiz(p, f, "viewCount", 0) > 0;
  if (blocoRaiz(p, f, "Media", &mi, &mf) && (q = elemento(mi + 1, mf)) != NULL && *q == '{') {
    const char *qf = depoisElemento(q);
    if (qf) {
      h = numRaiz(q, qf, "height", 0);
      txtRaiz(q, qf, "videoResolution", res, sizeof res);
      if (h <= 0) h = !strcmp(res, "4k") ? 2160 : atoi(res);
    }
  }
  e->altura = (unsigned short)(h >= 2000 ? 2160 : h >= 1000 ? 1080 : h >= 700 ? 720 : h > 0 ? 480 : 0);
  if (e->tipo == 'e') {
    double t = numRaiz(p, f, "parentIndex", -1), ix = numRaiz(p, f, "index", -1);
    if (t < 0 || ix <= 0 || t > 999 || ix > 30000 || !serieRk || !*serieRk) return 0;
    e->temporada = (short)t; e->episodio = (short)ix;
    copiar(e->serie, sizeof e->serie, serieRk);
  } else e->tituloHash = srvidx_titulo_hash(titulo);
  return 1;
}

int px_ler_indice(const char *corpo, size_t n, const char *serieRk, SrvIdx *x, int *total) {
  const char *mi, *mf, *ai, *af, *p;
  int vistos = 0;
  if (total) *total = 0;
  if (!blocoRaiz(corpo, corpo + n, "MediaContainer", &mi, &mf)) return PX_ERR_FORMATO;
  if (total) {
    *total = (int)numRaiz(mi, mf, "totalSize", -1);
    if (*total < 0) *total = (int)numRaiz(mi, mf, "size", 0);
  }
  if (!blocoRaiz(mi, mf, "Metadata", &ai, &af)) return 0;
  p = ai + 1;
  while ((p = elemento(p, af)) != NULL) {
    const char *f = depoisElemento(p);
    SrvIdxItem e;
    if (!f) break;
    if (*p == '{') {
      vistos++;
      if (linhaIndice(p, f, serieRk, &e)) srvidx_por(x, &e);
    }
    p = f;
  }
  return vistos;
}

// `canonico`: a Home row item with an IMDb/TMDB Guid IS that catalogue title
// (see jellyfin.c itemDeEx); without one it keeps the opaque "px:" id.
static int itemDeEx(const PxConta *c, const char *ini, const char *fim, CatItem *it, int canonico) {
  char rk[24] = "", tipo[16] = "", caminho[320] = "", cr[16] = "", imdb[16];
  const char *bi, *bf;
  double ano, dur, r, off;
  long tmdb;
  memset(it, 0, sizeof *it);
  if (!chaveTxt(ini, fim, "ratingKey", rk, sizeof rk)) return 0;
  txtRaiz(ini, fim, "type", tipo, sizeof tipo);
  if (!strcmp(tipo, "movie")) copiar(it->tipo, sizeof it->tipo, "movie");
  else if (!strcmp(tipo, "show")) copiar(it->tipo, sizeof it->tipo, "series");
  else return 0;
  guidsDe(ini, fim, imdb, &tmdb, NULL);
  if (canonico && imdb[0]) copiar(it->imdb, sizeof it->imdb, imdb);
  else if (canonico && tmdb > 0) snprintf(it->imdb, sizeof it->imdb, "tmdb:%ld", tmdb);
  else if (!jfid_montar_p(JFID_PREFIXO_PLEX, it->imdb, sizeof it->imdb, c->servidorId, rk)) return 0;
  txtRaiz(ini, fim, "title", it->titulo, sizeof it->titulo);
  txtRaiz(ini, fim, "summary", it->sinopse, sizeof it->sinopse);
  txtRaiz(ini, fim, "contentRating", cr, sizeof cr);
  copiar(it->classificacao, sizeof it->classificacao, cr);
  ano = numRaiz(ini, fim, "year", 0);
  dur = numRaiz(ini, fim, "duration", 0);     // milliseconds
  if (ano > 0 && dur > 0 && it->tipo[0] == 'm')
    snprintf(it->meta, sizeof it->meta, "%d · %d min", (int)ano, (int)(dur / 60000.0 + 0.5));
  else if (ano > 0) snprintf(it->meta, sizeof it->meta, "%d", (int)ano);
  r = numRaiz(ini, fim, "rating", 0);
  if (r <= 0) r = numRaiz(ini, fim, "audienceRating", 0);
  if (r > 0 && r <= 10) it->nota = (int)(r * 10.0 + 0.5);
  if (blocoRaiz(ini, fim, "Genre", &bi, &bf)) {
    const char *p = bi + 1;
    size_t k = 0;
    while ((p = elemento(p, bf)) != NULL) {
      const char *f = depoisElemento(p);
      char g[64] = "";
      if (!f) break;
      if (*p == '{' && txtRaiz(p, f, "tag", g, sizeof g) && k + strlen(g) + 4 < sizeof it->genero)
        k += (size_t)snprintf(it->genero + k, sizeof it->genero - k, "%s%s", k ? " · " : "", g);
      p = f;
    }
  }
  if (txtRaiz(ini, fim, "thumb", caminho, sizeof caminho)) imagem(c, caminho, 300, 450, it->poster, sizeof it->poster);
  caminho[0] = 0;
  if (txtRaiz(ini, fim, "art", caminho, sizeof caminho)) {
    imagem(c, caminho, 1280, 720, it->backdrop, sizeof it->backdrop);
    copiar(it->backdropCatalogo, sizeof it->backdropCatalogo, it->backdrop);
  }
  if (blocoRaiz(ini, fim, "Image", &bi, &bf)) {          // PMS 1.32+: clearLogo
    const char *p = bi + 1;
    while ((p = elemento(p, bf)) != NULL) {
      const char *f = depoisElemento(p);
      char t[24] = "", u[320] = "";
      if (!f) break;
      if (*p == '{' && txtRaiz(p, f, "type", t, sizeof t) && !strcmp(t, "clearLogo") &&
          txtRaiz(p, f, "url", u, sizeof u)) { imagem(c, u, 600, 240, it->logo, sizeof it->logo); break; }
      p = f;
    }
  }
  off = numRaiz(ini, fim, "viewOffset", 0);
  if (off > 0 && dur > off) {
    it->progresso = (int)(off * 100.0 / dur);
    it->restanteMin = (int)((dur - off) / 60000.0 + 0.5);
  }
  copiar(it->origem, sizeof it->origem, "plex");
  return 1;
}
static int itemDe(const PxConta *c, const char *ini, const char *fim, CatItem *it) {
  return itemDeEx(c, ini, fim, it, 0);
}

int px_ler_itens(const PxConta *c, const char *corpo, size_t n, CatItem *out, int max, int *total) {
  const char *mi, *mf, *ai, *af, *p;
  int k = 0;
  if (total) *total = 0;
  if (!blocoRaiz(corpo, corpo + n, "MediaContainer", &mi, &mf)) return PX_ERR_FORMATO;
  if (total) {
    *total = (int)numRaiz(mi, mf, "totalSize", -1);
    if (*total < 0) *total = (int)numRaiz(mi, mf, "size", 0);
  }
  if (!blocoRaiz(mi, mf, "Metadata", &ai, &af)) return 0;
  p = ai + 1;
  while (k < max && (p = elemento(p, af)) != NULL) {
    const char *f = depoisElemento(p);
    if (!f) break;
    if (*p == '{' && itemDeEx(c, p, f, &out[k], 1)) k++;
    p = f;
  }
  return k;
}

int px_ler_detalhe(const PxConta *c, const char *corpo, size_t n, CatItem *out) {
  const char *ai, *af, *p, *bi, *bf;
  if (!arrayDe(corpo, n, "Metadata", &ai, &af)) return PX_ERR_FORMATO;
  p = elemento(ai + 1, af);
  if (!p || *p != '{') return PX_ERR_FORMATO;
  af = depoisElemento(p);
  if (!af || !itemDe(c, p, af, out)) return PX_ERR_FORMATO;
  if (blocoRaiz(p, af, "Role", &bi, &bf)) {
    const char *q = bi + 1;
    while ((q = elemento(q, bf)) != NULL && out->nElenco < CAT_ELENCO_MAX) {
      const char *f = depoisElemento(q);
      char nome[64] = "", papel[64] = "", foto[320] = "";
      if (!f) break;
      if (*q == '{' && txtRaiz(q, f, "tag", nome, sizeof nome) && nome[0]) {
        int k = out->nElenco++;
        txtRaiz(q, f, "role", papel, sizeof papel);
        copiar(out->elenco[k].nome, sizeof out->elenco[k].nome, nome);
        copiar(out->elenco[k].papel, sizeof out->elenco[k].papel, papel);
        // Cast photos are usually absolute plex.tv/metadata URLs; a server
        // path would need the token and is skipped.
        if (txtRaiz(q, f, "thumb", foto, sizeof foto) && !strncmp(foto, "https://", 8))
          copiar(out->elenco[k].foto, sizeof out->elenco[k].foto, foto);
      }
      q = f;
    }
  }
  if (blocoRaiz(p, af, "Director", &bi, &bf)) {
    const char *q = bi + 1;
    size_t kd = 0;
    while ((q = elemento(q, bf)) != NULL) {
      const char *f = depoisElemento(q);
      char nome[64] = "";
      if (!f) break;
      if (*q == '{' && txtRaiz(q, f, "tag", nome, sizeof nome) && nome[0] &&
          kd + strlen(nome) + 3 < sizeof out->direcao)
        kd += (size_t)snprintf(out->direcao + kd, sizeof out->direcao - kd, "%s%s", kd ? ", " : "", nome);
      q = f;
    }
  }
  return PX_OK;
}

int px_ler_episodios(const PxConta *c, const char *corpo, size_t n, CatEp *out, int max) {
  const char *ai, *af, *p;
  int k = 0;
  if (!arrayDe(corpo, n, "Metadata", &ai, &af)) return 0;
  p = ai + 1;
  while (k < max && (p = elemento(p, af)) != NULL) {
    const char *f = depoisElemento(p);
    char rk[24] = "", data[32] = "", thumb[320] = "";
    CatEp *ep = &out[k];
    double t, ix, dur;
    if (!f) break;
    if (*p == '{') {
      t = numRaiz(p, f, "parentIndex", -1);
      ix = numRaiz(p, f, "index", 0);
      memset(ep, 0, sizeof *ep);
      if (chaveTxt(p, f, "ratingKey", rk, sizeof rk) && t >= 0 && ix > 0 && t < 1000 && ix < 100000 &&
          jfid_montar_p(JFID_PREFIXO_PLEX, ep->vid, sizeof ep->vid, c->servidorId, rk)) {
        ep->temporada = (int)t; ep->episodio = (int)ix;
        txtRaiz(p, f, "title", ep->nome, sizeof ep->nome);
        txtRaiz(p, f, "summary", ep->sinopse, sizeof ep->sinopse);
        dur = numRaiz(p, f, "duration", 0);
        if (dur > 0) snprintf(ep->duracao, sizeof ep->duracao, "%d min", (int)(dur / 60000.0 + 0.5));
        if (txtRaiz(p, f, "originallyAvailableAt", data, sizeof data) && strlen(data) >= 10)
          snprintf(ep->data, sizeof ep->data, "%.2s/%.2s/%.4s", data + 8, data + 5, data);
        if (txtRaiz(p, f, "thumb", thumb, sizeof thumb)) imagem(c, thumb, 640, 360, ep->thumb, sizeof ep->thumb);
        k++;
      }
    }
    p = f;
  }
  return k;
}

int px_ler_episodio_rk(const char *corpo, size_t n, int temp, int ep, char *rk, size_t tam) {
  const char *ai, *af, *p;
  if (tam) rk[0] = 0;
  if (!arrayDe(corpo, n, "Metadata", &ai, &af)) return PX_ERR_FORMATO;
  p = ai + 1;
  while ((p = elemento(p, af)) != NULL) {
    const char *f = depoisElemento(p);
    if (!f) break;
    if (*p == '{' && (int)numRaiz(p, f, "parentIndex", -1) == temp && (int)numRaiz(p, f, "index", -1) == ep &&
        chaveTxt(p, f, "ratingKey", rk, tam))
      return 1;
    p = f;
  }
  return 0;
}

// "tt0111161", "tt0944947:2:5", "tmdb:278", "tmdb:1399:1:3" (srvidx.h).
int px_id_titulo(const char *id, char imdb[16], long *tmdb, int *temp, int *ep) {
  return srvidx_id_titulo(id, imdb, tmdb, temp, ep);
}

// --------------------------------------------------------- direct play gate
// Plex containers are "mkv", "mp4", "mpegts"...; the capability lists of
// jellyfin.c say "ts". DTS is "dca" (and "dca-ma") on Plex.
static const char *contNome(const char *c) { return !strcasecmp(c, "mpegts") ? "ts" : c; }

int px_ler_midias(const char *corpo, size_t n, PxMidiaInfo *out, int max) {
  const char *ai, *af, *p, *mi, *mf;
  int k = 0;
  if (!arrayDe(corpo, n, "Metadata", &ai, &af)) return PX_ERR_FORMATO;
  p = elemento(ai + 1, af);
  if (!p || *p != '{') return PX_ERR_FORMATO;
  af = depoisElemento(p);
  if (!af || !blocoRaiz(p, af, "Media", &mi, &mf)) return PX_ERR_FORMATO;
  p = mi + 1;
  while (k < max && (p = elemento(p, mf)) != NULL) {
    const char *f = depoisElemento(p), *pi, *pf, *q;
    PxMidiaInfo *m = &out[k];
    if (!f) break;
    memset(m, 0, sizeof *m);
    if (*p == '{') {
      txtRaiz(p, f, "container", m->cont, sizeof m->cont);
      txtRaiz(p, f, "videoCodec", m->vcod, sizeof m->vcod);
      txtRaiz(p, f, "audioCodec", m->acod, sizeof m->acod);
      // The SELECTED tracks decide: a file whose default audio is DTS but that
      // also has an AC3 track still plays the DTS one unless told otherwise.
      if (blocoRaiz(p, f, "Part", &pi, &pf) && (q = elemento(pi + 1, pf)) != NULL && *q == '{') {
        const char *qf = depoisElemento(q), *si, *sf, *w;
        if (qf && blocoRaiz(q, qf, "Stream", &si, &sf)) {
          w = si + 1;
          while ((w = elemento(w, sf)) != NULL) {
            const char *wf = depoisElemento(w);
            if (!wf) break;
            if (*w == '{' && boolRaiz(w, wf, "selected", 0)) {
              int t = (int)numRaiz(w, wf, "streamType", 0);
              if (t == 2) txtRaiz(w, wf, "codec", m->acod, sizeof m->acod);
              else if (t == 3) txtRaiz(w, wf, "codec", m->leg, sizeof m->leg);
            }
            w = wf;
          }
        }
      }
    }
    k++;
    p = f;
  }
  return k;
}

int px_video_suportado(JfBackend b, const char *vcod) {
  const char *cont, *vid, *aud;
  return jf_capacidades(b, &cont, &vid, &aud) && jf_lista_tem(vid, vcod);
}

// The TV's own table first: Plex is never asked to approve what this pipeline
// is known not to open (DTS/TrueHD on every backend here, PGS/VobSub burned
// subtitles, HEVC on the desktop build).
int px_direto_suportado(JfBackend b, const PxMidiaInfo *m) {
  const char *cont, *vid, *aud;
  if (!m || !jf_capacidades(b, &cont, &vid, &aud)) return 0;
  if (!jf_lista_tem(cont, contNome(m->cont)) || !jf_lista_tem(vid, m->vcod)) return 0;
  if (m->acod[0] && !jf_lista_tem(aud, m->acod)) return 0;
  if (m->leg[0] && (!strcasecmp(m->leg, "pgs") || !strcasecmp(m->leg, "vobsub") ||
                    !strcasecmp(m->leg, "dvd_subtitle") || !strcasecmp(m->leg, "hdmv_pgs_subtitle")))
    return 0;
  return 1;
}

// X-Plex-Client-Profile-Extra with the same table, so the server's decision
// is made against what this TV opens and not against a generic profile.
int px_perfil_extra(JfBackend b, char *dst, size_t tam) {
  const char *cont, *vid, *aud;
  char c2[96];
  size_t k = 0;
  const char *p;
  if (!jf_capacidades(b, &cont, &vid, &aud)) return 0;
  for (p = cont; *p && k + 8 < sizeof c2;) {
    size_t n = strcspn(p, ",");
    if (n == 2 && !strncmp(p, "ts", 2)) k += (size_t)snprintf(c2 + k, sizeof c2 - k, "%smpegts", k ? "," : "");
    else k += (size_t)snprintf(c2 + k, sizeof c2 - k, "%s%.*s", k ? "," : "", (int)n, p);
    p += n;
    if (*p == ',') p++;
  }
  return snprintf(dst, tam,
                  "add-transcode-target(type=videoProfile&context=streaming&protocol=hls&container=mpegts"
                  "&videoCodec=h264&audioCodec=aac,ac3)"
                  "+add-direct-play-profile(type=videoProfile&container=%s&videoCodec=%s&audioCodec=%s"
                  "&subtitleCodec=srt,ass,ssa,webvtt)", c2, vid, aud) < (int)tam;
}

// /video/:/transcode/universal/decision. directPlayDecisionCode 1000 is "Direct
// play OK"; any other code says no. Without the code, the first Part's
// "decision" decides. Nothing readable: unknown (the caller keeps its own
// table's verdict).
int px_ler_decisao(const char *corpo, size_t n) {
  const char *mi, *mf, *ai, *af, *p, *f, *bi, *bf, *q;
  double cod;
  char dec[24] = "";
  if (!corpo || !blocoRaiz(corpo, corpo + n, "MediaContainer", &mi, &mf)) return PX_DEC_DESCONHECIDO;
  cod = numRaiz(mi, mf, "directPlayDecisionCode", -1);
  if (cod >= 0) return cod == 1000 ? PX_DEC_DIRETO : PX_DEC_NAO;
  if (!blocoRaiz(mi, mf, "Metadata", &ai, &af) || !(p = elemento(ai + 1, af)) || *p != '{' ||
      !(f = depoisElemento(p)) || !blocoRaiz(p, f, "Media", &bi, &bf) || !(q = elemento(bi + 1, bf)) || *q != '{')
    return PX_DEC_DESCONHECIDO;
  f = depoisElemento(q);
  if (!f || !blocoRaiz(q, f, "Part", &bi, &bf) || !(q = elemento(bi + 1, bf)) || *q != '{' ||
      !(f = depoisElemento(q)) || !txtRaiz(q, f, "decision", dec, sizeof dec) || !dec[0])
    return PX_DEC_DESCONHECIDO;
  return !strcmp(dec, "directplay") ? PX_DEC_DIRETO : PX_DEC_NAO;
}

// --------------------------------------------------------- playback sources
static void descreverMidia(const char *mi, const char *mf, Stream *s, int metodo, const char *versaoNome) {
  char vcod[16] = "", acod[16] = "", trc[24] = "", cont[16] = "", res[16] = "";
  int altura = 0, canais = 0, dovi = 0;
  const char *pi, *pf, *q;
  double h = numRaiz(mi, mf, "height", 0);
  txtRaiz(mi, mf, "videoCodec", vcod, sizeof vcod);
  txtRaiz(mi, mf, "audioCodec", acod, sizeof acod);
  txtRaiz(mi, mf, "container", cont, sizeof cont);
  txtRaiz(mi, mf, "videoResolution", res, sizeof res);
  canais = (int)numRaiz(mi, mf, "audioChannels", 0);
  altura = (int)h;
  if (!altura) altura = !strcmp(res, "4k") ? 2160 : atoi(res);
  if (blocoRaiz(mi, mf, "Part", &pi, &pf)) {
    q = elemento(pi + 1, pf);
    if (q && *q == '{') {
      const char *qf = depoisElemento(q), *si, *sf;
      if (qf && blocoRaiz(q, qf, "Stream", &si, &sf)) {
        const char *w = si + 1;
        while ((w = elemento(w, sf)) != NULL) {
          const char *f = depoisElemento(w);
          int tipo;
          if (!f) break;
          tipo = *w == '{' ? (int)numRaiz(w, f, "streamType", 0) : 0;
          if (tipo == 1 && !trc[0]) {
            txtRaiz(w, f, "colorTrc", trc, sizeof trc);
            dovi = boolRaiz(w, f, "DOVIPresent", 0);
          }
          w = f;
        }
      }
    }
  }
  if (altura >= 2000) s->altura = 2160;
  else if (altura >= 1000) s->altura = 1080;
  else if (altura >= 700) s->altura = 720;
  else s->altura = altura;
  s->dolbyVision = dovi;
  {
    char *u;
    for (u = vcod; *u; u++) *u = (char)toupper((unsigned char)*u);
    for (u = acod; *u; u++) *u = (char)toupper((unsigned char)*u);
    for (u = cont; *u; u++) *u = (char)toupper((unsigned char)*u);
  }
  {
    const char *m = metodo == PX_METODO_DIRETO ? "Direct play" :
                    metodo == PX_METODO_STREAM ? "Direct stream (HLS)" : "Transcode (HLS)";
    if (s->altura) snprintf(s->rotulo, sizeof s->rotulo, "%dp · %s", s->altura, m);
    else copiar(s->rotulo, sizeof s->rotulo, m);
  }
  {
    size_t n = (size_t)snprintf(s->descricao, sizeof s->descricao, "%s\n%s",
                                versaoNome && *versaoNome ? versaoNome : "Plex", vcod);
    const char *faixa = dovi ? "Dolby Vision" : !strcmp(trc, "smpte2084") ? "HDR10" : !strcmp(trc, "arib-std-b67") ? "HLG" : "";
    if (n < sizeof s->descricao && faixa[0]) n += (size_t)snprintf(s->descricao + n, sizeof s->descricao - n, " · %s", faixa);
    if (n < sizeof s->descricao && acod[0])
      n += (size_t)snprintf(s->descricao + n, sizeof s->descricao - n, " · %s%s", acod,
                            canais >= 8 ? " 7.1" : canais > 2 ? " 5.1" : "");
    if (n < sizeof s->descricao && metodo == PX_METODO_DIRETO && cont[0])
      snprintf(s->descricao + n, sizeof s->descricao - n, " · %s", cont);
  }
}

static void novoSessao(char *dst, size_t tam) {
  unsigned char b[6];
  int fd = open("/dev/urandom", O_RDONLY), ok = 0, i;
  if (fd >= 0) { ok = read(fd, b, sizeof b) == (ssize_t)sizeof b; close(fd); }
  if (!ok) {
    unsigned long long x = agoraMs() ^ ((unsigned long long)getpid() << 20);
    for (i = 0; i < 6; i++) b[i] = (unsigned char)(x >> (i * 8));
  }
  snprintf(dst, tam, "nuvio%02x%02x%02x%02x%02x%02x", b[0], b[1], b[2], b[3], b[4], b[5]);
}

int px_ler_fontes(const PxConta *c, const char *rk, const char *corpo, size_t n,
                  const signed char *direto, PxPlayback *out) {
  const char *ai, *af, *p, *mi, *mf;
  double durItem;
  int idx = 0, nInfo;
  PxMidiaInfo info[PX_FONTES_MAX];
  JfBackend b = jellyfin_backend();
  memset(out, 0, sizeof *out);
  memset(info, 0, sizeof info);
  nInfo = px_ler_midias(corpo, n, info, PX_FONTES_MAX);
  if (nInfo < 0) nInfo = 0;
  if (!arrayDe(corpo, n, "Metadata", &ai, &af)) return PX_ERR_FORMATO;
  p = elemento(ai + 1, af);
  if (!p || *p != '{') return PX_ERR_FORMATO;
  af = depoisElemento(p);
  if (!af || !blocoRaiz(p, af, "Media", &mi, &mf)) return PX_ERR_FORMATO;
  durItem = numRaiz(p, af, "duration", 0);
  p = mi + 1;
  while (out->n < PX_FONTES_MAX && (p = elemento(p, mf)) != NULL) {
    const char *f = depoisElemento(p), *pi, *pf, *q;
    char key[400] = "", cont[16] = "", nomeV[96] = "";
    double dur, tam;
    int k;
    if (!f) break;
    if (*p != '{') { p = f; idx++; continue; }
    dur = numRaiz(p, f, "duration", durItem);
    txtRaiz(p, f, "container", cont, sizeof cont);
    key[0] = 0; tam = 0;
    if (blocoRaiz(p, f, "Part", &pi, &pf) && (q = elemento(pi + 1, pf)) != NULL && *q == '{') {
      const char *qf = depoisElemento(q);
      if (qf) { txtRaiz(q, qf, "key", key, sizeof key); tam = numRaiz(q, qf, "size", 0); }
    }
    snprintf(nomeV, sizeof nomeV, "Plex · v%d", idx + 1);
    // Only server-relative part keys are accepted: the key is pasted into the
    // stream URL together with the token.
    if (strncmp(key, "/library/parts/", 15) || strpbrk(key, " \"'<>\\\r\n") ) { p = f; idx++; continue; }
    for (k = 0; k < 2 && out->n < PX_FONTES_MAX; k++) {
      PxSessaoPlay *sp = &out->sessao[out->n];
      Stream *s = &out->fonte[out->n];
      int metodo, pode;
      // DIRECT PLAY ONLY WHEN IT CAN WORK: this TV's table says so AND the
      // server's decision did not refuse it (direto[idx]: 1 yes, 0 no, -1 the
      // server could not tell). Before this every Part was "Direct play", and
      // a DTS/TrueHD/PGS file gave a black screen or no sound.
      pode = idx < nInfo && px_direto_suportado(b, &info[idx]) && (!direto || direto[idx] != PX_DEC_NAO);
      if (k == 0 && !pode) continue;
      // The HLS alternative: always for the first version, and for any version
      // that cannot play directly. With a video codec this TV opens the server
      // copies the video and converts only what it must (direct stream).
      if (k == 1 && idx != 0 && pode) continue;
      metodo = k == 0 ? PX_METODO_DIRETO
             : idx < nInfo && px_video_suportado(b, info[idx].vcod) ? PX_METODO_STREAM : PX_METODO_TRANSCODE;
      memset(sp, 0, sizeof *sp);
      memset(s, 0, sizeof *s);
      copiar(sp->ratingKey, sizeof sp->ratingKey, rk);
      sp->midia = idx;
      sp->metodo = metodo;
      sp->duracaoMs = dur > 0 ? (long long)dur : 0;
      novoSessao(sp->sessaoId, sizeof sp->sessaoId);
      if (metodo == PX_METODO_DIRETO)
        snprintf(sp->url, sizeof sp->url, "%s%s?X-Plex-Token=%s", c->base, key, c->token);
      else
        snprintf(sp->url, sizeof sp->url,
                 "%s/video/:/transcode/universal/start.m3u8?path=%%2Flibrary%%2Fmetadata%%2F%s&mediaIndex=%d"
                 "&partIndex=0&protocol=hls&fastSeek=1&directPlay=0&directStream=1&subtitleSize=100"
                 "&audioBoost=100&maxVideoBitrate=%d&offset=0&X-Plex-Platform=Nuvio"
                 "&X-Plex-Product=Nuvio&X-Plex-Client-Identifier=%s&X-Plex-Session-Identifier=%s"
                 "&X-Plex-Token=%s", c->base, rk, idx, PX_TAXA_MAX, c->dispositivoId, sp->sessaoId, c->token);
      copiar(s->url, sizeof s->url, sp->url);
      copiar(s->provedor, sizeof s->provedor, "Plex");
      s->fileIdx = -1;
      s->mp4 = metodo == PX_METODO_DIRETO && (!strcmp(cont, "mp4") || !strcmp(cont, "m4v"));
      s->tamanhoBytes = metodo == PX_METODO_DIRETO && tam > 0 ? (uint64_t)tam : 0;
      s->tamanhoMB = (long)(s->tamanhoBytes / (1024u * 1024u));
      snprintf(s->bingeGroup, sizeof s->bingeGroup, "plex|%s", px_metodo_nome(metodo));
      descreverMidia(p, f, s, metodo, nomeV);
      out->n++;
    }
    p = f;
    idx++;
  }
  return PX_OK;
}

// ------------------------------------------------------------ requests
// One request. Logs operation/status/latency only: never the URL (the host can
// be a private address, the query can carry a token).
static int pedirEx(const PxConta *c, const char *url, const char *token, const char *metodo,
                   const char *operacao, unsigned prazo, RedeJob *job, RedeResposta *r,
                   const char *extra);
static int pedir(const PxConta *c, const char *url, const char *token, const char *metodo,
                 const char *operacao, unsigned prazo, RedeJob *job, RedeResposta *r) {
  return pedirEx(c, url, token, metodo, operacao, prazo, job, r, NULL);
}
// `extra`: one more complete header line ("Name: value"), or NULL.
static int pedirEx(const PxConta *c, const char *url, const char *token, const char *metodo,
                   const char *operacao, unsigned prazo, RedeJob *job, RedeResposta *r,
                   const char *extra) {
  char tok[120], did[96], dev[64], cab[8][200];
  const char *cabs[11];
  int k = 0, i;
  RedePedido p;
  memset(r, 0, sizeof *r);
  if (!(rede_pedido_capacidades() & REDE_CAP_JOB)) return PX_ERR_INDISPONIVEL;
  if (!url || !*url) return PX_ERR_ENTRADA;
  campoCab(did, sizeof did, c && c->dispositivoId[0] ? c->dispositivoId : "nuvio");
  campoCab(dev, sizeof dev, c && c->dispositivoNome[0] ? c->dispositivoNome : nomeBackend());
  snprintf(cab[0], sizeof cab[0], "Accept: application/json");
  snprintf(cab[1], sizeof cab[1], "X-Plex-Product: Nuvio");
  snprintf(cab[2], sizeof cab[2], "X-Plex-Version: %s", NV_PX_VERSAO);
  snprintf(cab[3], sizeof cab[3], "X-Plex-Client-Identifier: %s", did);
  snprintf(cab[4], sizeof cab[4], "X-Plex-Platform: Nuvio");
  snprintf(cab[5], sizeof cab[5], "X-Plex-Device-Name: %s", dev);
  for (i = 0; i < 6; i++) cabs[k++] = cab[i];
  tok[0] = 0;
  if (token && *token) {
    campoCab(tok + 0, sizeof tok, token);   // sanitised copy for the header value
    snprintf(cab[6], sizeof cab[6], "X-Plex-Token: %.150s", tok);
    cabs[k++] = cab[6];
  }
  if (extra && *extra) cabs[k++] = extra;
  cabs[k] = NULL;
  memset(&p, 0, sizeof p);
  p.metodo = metodo;
  p.url = url;
  p.cabecalhos = cabs;
  p.prazo_ms = prazo ? prazo : PX_PRAZO_MS;
  p.max_bytes = PX_CORPO_MAX;
  p.seguir = 1;
  p.mesmo_host = 1;   // a redirect never takes this request to another machine
  p.job = job;
  rede_pedir(&p, r);
  apagarSegredo(tok, sizeof tok);
  apagarSegredo(cab, sizeof cab);
  printf("[plex] %s: HTTP %d, %u ms%s\n", operacao, r->status, r->ms,
         r->erro == REDE_OK ? "" : r->erro == REDE_CANCELADO || r->erro == REDE_GERACAO
                                   ? " (cancelled)" : " (transport)");
  fflush(stdout);
  if (r->erro == REDE_CANCELADO || r->erro == REDE_GERACAO) { rede_resposta_limpar(r); return PX_ERR_CANCELADO; }
  if (r->erro == REDE_INDISPONIVEL) { rede_resposta_limpar(r); return PX_ERR_INDISPONIVEL; }
  if (r->erro != REDE_OK) { rede_resposta_limpar(r); return PX_ERR_REDE; }
  if (r->status == 401 || r->status == 403) { rede_resposta_limpar(r); return PX_ERR_AUTH; }
  if (r->status < 200 || r->status >= 300) {
    int s = r->status;
    rede_resposta_limpar(r);
    return s == 404 ? PX_ERR_EXPIRADO : PX_ERR_HTTP;
  }
  return PX_OK;
}

static int pedirServidor(const PxConta *c, const char *caminho, const char *metodo, const char *op,
                         unsigned prazo, RedeJob *job, RedeResposta *r) {
  char url[1400];
  int e;
  if (!c->base[0] || snprintf(url, sizeof url, "%s%s", c->base, caminho) >= (int)sizeof url) return PX_ERR_ENTRADA;
  e = pedir(c, url, c->token, metodo, op, prazo, job, r);
  return e == PX_ERR_EXPIRADO ? PX_ERR_HTTP : e;   // 404 on a library route is just "not found"
}

int px_pin_criar(const PxConta *c, RedeJob *job, char *id, size_t ni, char *codigo, size_t nc, int *expiraSeg) {
  RedeResposta r;
  char tok[8];
  int e = pedir(c, PX_TV "/api/v2/pins?strong=false", NULL, "POST", "pin create", 0, job, &r);
  if (e) return e;
  e = px_ler_pin(r.corpo, r.n_corpo, id, ni, codigo, nc, expiraSeg, tok, sizeof tok);
  rede_resposta_limpar(&r);
  if (e < 0) return e;
  return id[0] && codigo[0] ? PX_OK : PX_ERR_FORMATO;
}

int px_pin_conferir(const PxConta *c, RedeJob *job, const char *id, char *token, size_t nt) {
  RedeResposta r;
  char url[160], idr[24], cod[8];
  int e, exp;
  if (!soDigitos(id, 18)) return PX_ERR_ENTRADA;
  snprintf(url, sizeof url, PX_TV "/api/v2/pins/%s", id);
  e = pedir(c, url, NULL, "GET", "pin poll", 0, job, &r);
  if (e) return e;
  e = px_ler_pin(r.corpo, r.n_corpo, idr, sizeof idr, cod, sizeof cod, &exp, token, nt);
  apagarSegredo(r.corpo, r.n_corpo);
  rede_resposta_limpar(&r);
  return e;
}

int px_recursos(const PxConta *c, RedeJob *job, PxServidor *out, int max) {
  RedeResposta r;
  int e = pedir(c, PX_TV "/api/v2/resources?includeHttps=1&includeRelay=1&includeIPv6=0", c->tokenConta,
                "GET", "resources", PX_PRAZO_LISTA_MS, job, &r);
  if (e) return e;
  e = px_ler_recursos(r.corpo, r.n_corpo, out, max);
  apagarSegredo(r.corpo, r.n_corpo);    // carries every server's access token
  rede_resposta_limpar(&r);
  return e;
}

// Probes the candidate connections in order and keeps the first one that
// answers /identity AS this server. The probe sends no token: /identity is
// public and the address came from plex.tv, but a token has no business on a
// probe that may hit a stale LAN address.
int px_escolher(PxConta *c, RedeJob *job, const PxServidor *s) {
  char cand[PX_CON_MAX * 2 + 2][320];
  int n = px_candidatas(s, cand, PX_CON_MAX * 2), i;
  for (i = 0; i < n; i++) {
    RedeResposta r;
    char url[360], id[64] = "", ver[24] = "";
    int e;
    snprintf(url, sizeof url, "%s/identity", cand[i]);
    e = pedir(c, url, NULL, "GET", "server probe", PX_PRAZO_SONDA_MS, job, &r);
    if (e == PX_ERR_CANCELADO) return e;
    if (e) continue;
    e = px_ler_identidade(r.corpo, r.n_corpo, id, sizeof id, ver, sizeof ver);
    rede_resposta_limpar(&r);
    if (e != PX_OK || strcasecmp(id, s->id)) continue;     // another server on that address
    copiar(c->base, sizeof c->base, cand[i]);
    copiar(c->servidorId, sizeof c->servidorId, s->id);
    copiar(c->servidorNome, sizeof c->servidorNome, s->nome);
    copiar(c->versao, sizeof c->versao, ver[0] ? ver : s->versao);
    copiar(c->token, sizeof c->token, s->token[0] ? s->token : c->tokenConta);
    return PX_OK;
  }
  return PX_ERR_SEM_SERVIDOR;
}

int px_bibliotecas(const PxConta *c, RedeJob *job, PxBiblioteca *out, int max) {
  RedeResposta r;
  int e = pedirServidor(c, "/library/sections", "GET", "libraries", PX_PRAZO_LISTA_MS, job, &r), n;
  if (e) return e;
  n = px_ler_bibliotecas(r.corpo, r.n_corpo, out, max);
  rede_resposta_limpar(&r);
  return n;
}

static void caminhoSecao(char *dst, size_t tam, const char *secao, int inicio, int limite, const char *extra) {
  snprintf(dst, tam, "/library/sections/%s/all?includeGuids=1&sort=addedAt:desc%s"
           "&X-Plex-Container-Start=%d&X-Plex-Container-Size=%d", secao, extra, inicio, limite);
}

int px_itens(const PxConta *c, RedeJob *job, const char *secao, int inicio, int limite,
             CatItem *out, int max, int *total) {
  RedeResposta r;
  char caminho[256];
  int e, n;
  if (total) *total = 0;
  if (!soDigitos(secao, 12) || inicio < 0 || limite <= 0) return PX_ERR_ENTRADA;
  if (limite > 100) limite = 100;
  caminhoSecao(caminho, sizeof caminho, secao, inicio, limite, "");
  e = pedirServidor(c, caminho, "GET", "library items", PX_PRAZO_LISTA_MS, job, &r);
  if (e) return e;
  n = px_ler_itens(c, r.corpo, r.n_corpo, out, max, total);
  rede_resposta_limpar(&r);
  return n;
}

// `desde` > 0: only items updated after it (Plex filter "updatedAt>>=",
// strictly greater). limite 0 is the cheap count check (totalSize only).
int px_indice(const PxConta *c, RedeJob *job, const char *secao, int inicio, int limite,
              long long desde, SrvIdx *x, int *total) {
  RedeResposta r;
  char caminho[320], extra[48] = "";
  int e, n;
  if (total) *total = 0;
  if (!soDigitos(secao, 12) || inicio < 0 || limite < 0 || limite > 500) return PX_ERR_ENTRADA;
  if (desde > 0) snprintf(extra, sizeof extra, "&updatedAt%%3E%%3E=%lld", desde - 1);
  caminhoSecao(caminho, sizeof caminho, secao, inicio, limite, extra);
  e = pedirServidor(c, caminho, "GET", "library index", PX_PRAZO_LISTA_MS, job, &r);
  if (e) return e;
  n = px_ler_indice(r.corpo, r.n_corpo, NULL, x, total);
  rede_resposta_limpar(&r);
  return n;
}

int px_indice_episodios(const PxConta *c, RedeJob *job, const char *serieRk, SrvIdx *x) {
  RedeResposta r;
  char caminho[160];
  int e, n;
  if (!soDigitos(serieRk, 20)) return PX_ERR_ENTRADA;
  snprintf(caminho, sizeof caminho, "/library/metadata/%s/allLeaves?includeGuids=1"
           "&X-Plex-Container-Start=0&X-Plex-Container-Size=2000", serieRk);
  e = pedirServidor(c, caminho, "GET", "episode index", PX_PRAZO_LISTA_MS, job, &r);
  if (e) return e;
  n = px_ler_indice(r.corpo, r.n_corpo, serieRk, x, NULL);
  rede_resposta_limpar(&r);
  return n;
}

int px_detalhe(const PxConta *c, RedeJob *job, const char *rk, CatItem *out) {
  RedeResposta r;
  char caminho[160];
  int e;
  if (!soDigitos(rk, 20)) return PX_ERR_ENTRADA;
  snprintf(caminho, sizeof caminho, "/library/metadata/%s?includeGuids=1", rk);
  e = pedirServidor(c, caminho, "GET", "item", 0, job, &r);
  if (e) return e;
  e = px_ler_detalhe(c, r.corpo, r.n_corpo, out);
  rede_resposta_limpar(&r);
  return e;
}

int px_episodios(const PxConta *c, RedeJob *job, const char *serieRk, CatEp *out, int max) {
  RedeResposta r;
  char caminho[160];
  int e, n;
  if (!soDigitos(serieRk, 20)) return PX_ERR_ENTRADA;
  snprintf(caminho, sizeof caminho, "/library/metadata/%s/allLeaves?X-Plex-Container-Start=0&X-Plex-Container-Size=2000",
           serieRk);
  e = pedirServidor(c, caminho, "GET", "episodes", PX_PRAZO_LISTA_MS, job, &r);
  if (e) return e;
  n = px_ler_episodios(c, r.corpo, r.n_corpo, out, max);
  rede_resposta_limpar(&r);
  return n;
}

int px_achar_episodio(const PxConta *c, RedeJob *job, const char *serieRk, int temp, int ep, char *rk, size_t tam) {
  RedeResposta r;
  char caminho[160];
  int e;
  if (!soDigitos(serieRk, 20)) return PX_ERR_ENTRADA;
  snprintf(caminho, sizeof caminho, "/library/metadata/%s/allLeaves?X-Plex-Container-Start=0&X-Plex-Container-Size=2000",
           serieRk);
  e = pedirServidor(c, caminho, "GET", "episode lookup", PX_PRAZO_LISTA_MS, job, &r);
  if (e) return e;
  e = px_ler_episodio_rk(r.corpo, r.n_corpo, temp, ep, rk, tam);
  rede_resposta_limpar(&r);
  return e;
}

// Asks the server whether version `midia` of item `rk` can play directly on
// this backend. PX_DEC_* or PX_ERR_CANCELADO. A server without the route
// (old PMS) or any other failure is PX_DEC_DESCONHECIDO: the TV's own table
// still decides, the source list is never lost to a decision request.
int px_decisao(const PxConta *c, RedeJob *job, const char *rk, int midia, JfBackend b) {
  RedeResposta r;
  char url[1400], perfil[600], cab[640], sessao[24];
  int e;
  if (!soDigitos(rk, 20) || midia < 0 || midia > 99 || !c->base[0]) return PX_ERR_ENTRADA;
  if (!px_perfil_extra(b, perfil, sizeof perfil)) return PX_DEC_DESCONHECIDO;
  snprintf(cab, sizeof cab, "X-Plex-Client-Profile-Extra: %s", perfil);
  novoSessao(sessao, sizeof sessao);
  if (snprintf(url, sizeof url,
               "%s/video/:/transcode/universal/decision?path=%%2Flibrary%%2Fmetadata%%2F%s&mediaIndex=%d"
               "&partIndex=0&protocol=hls&directPlay=1&directStream=1&directStreamAudio=1&fastSeek=1&hasMDE=1"
               "&maxVideoBitrate=%d&X-Plex-Client-Profile-Name=Generic&X-Plex-Session-Identifier=%s",
               c->base, rk, midia, PX_TAXA_MAX, sessao) >= (int)sizeof url)
    return PX_ERR_ENTRADA;
  e = pedirEx(c, url, c->token, "GET", "playback decision", PX_PRAZO_SONDA_MS * 2, job, &r, cab);
  if (e == PX_ERR_CANCELADO) return e;
  if (e) return PX_DEC_DESCONHECIDO;
  e = px_ler_decisao(r.corpo, r.n_corpo);
  rede_resposta_limpar(&r);
  return e;
}

int px_fontes(const PxConta *c, RedeJob *job, const char *rk, PxPlayback *out) {
  RedeResposta r;
  char caminho[160];
  signed char dir[PX_FONTES_MAX];
  PxMidiaInfo info[PX_FONTES_MAX];
  JfBackend b = jellyfin_backend();
  int e, nm, i;
  memset(out, 0, sizeof *out);
  if (!soDigitos(rk, 20)) return PX_ERR_ENTRADA;
  snprintf(caminho, sizeof caminho, "/library/metadata/%s", rk);
  e = pedirServidor(c, caminho, "GET", "playback info", 0, job, &r);
  if (e) return e;
  nm = px_ler_midias(r.corpo, r.n_corpo, info, PX_FONTES_MAX);
  for (i = 0; i < PX_FONTES_MAX; i++) dir[i] = PX_DEC_NAO;
  // One decision per version this TV could open as is; a version the table
  // already refuses costs no request.
  for (i = 0; i < nm; i++) {
    if (!px_direto_suportado(b, &info[i])) continue;
    e = px_decisao(c, job, rk, i, b);
    if (e == PX_ERR_CANCELADO) { rede_resposta_limpar(&r); return e; }
    dir[i] = (signed char)(e == PX_DEC_NAO ? PX_DEC_NAO : e == PX_DEC_DIRETO ? PX_DEC_DIRETO : PX_DEC_DESCONHECIDO);
    printf("[plex] version %d: %s\n", i + 1, dir[i] == PX_DEC_DIRETO ? "direct play approved"
           : dir[i] == PX_DEC_NAO ? "direct play refused by the server" : "server decision unknown");
  }
  e = px_ler_fontes(c, rk, r.corpo, r.n_corpo, dir, out);
  rede_resposta_limpar(&r);
  return e;
}

// Timeline check-in. "stopped" ends the session; for a transcode the explicit
// stop frees the encoder even if the server missed it.
int px_reportar(const PxConta *c, RedeJob *job, int evento, const PxSessaoPlay *s, double posSeg) {
  RedeResposta r;
  char caminho[700], chave[80];
  const char *estado = evento == PX_REL_FIM ? "stopped" : evento == PX_REL_PAUSA ? "paused" : "playing";
  int e;
  if (!soDigitos(s->ratingKey, 20)) return PX_ERR_ENTRADA;
  snprintf(chave, sizeof chave, "%%2Flibrary%%2Fmetadata%%2F%s", s->ratingKey);
  snprintf(caminho, sizeof caminho,
           "/:/timeline?ratingKey=%s&key=%s&state=%s&time=%lld&duration=%lld&playbackTime=%lld&hasMDE=1"
           "&X-Plex-Session-Identifier=%s", s->ratingKey, chave, estado, px_ms(posSeg), s->duracaoMs,
           px_ms(posSeg), s->sessaoId);
  e = pedirServidor(c, caminho, "GET",
                    evento == PX_REL_INICIO ? "playback start" : evento == PX_REL_FIM ? "playback stopped" : "playback progress",
                    8000, job, &r);
  if (!e) rede_resposta_limpar(&r);
  if (!e && evento == PX_REL_FIM && s->metodo != PX_METODO_DIRETO && s->sessaoId[0]) {
    char cam[200];
    snprintf(cam, sizeof cam, "/video/:/transcode/universal/stop?session=%s", s->sessaoId);
    if (!pedirServidor(c, cam, "GET", "release transcode", 5000, job, &r)) rede_resposta_limpar(&r);
  }
  return e;
}

// ======================================================= app integration
typedef struct {
  int tipo;
  unsigned geracao;
  unsigned entrada;   // cancelEntrada when queued: a cancel BEFORE the worker starts must still win
  char a[64];
} Tarefa;
enum { T_PIN = 1, T_RESOLVER, T_BIBLIOTECAS, T_FONTES };

typedef struct {
  int evento;
  unsigned geracao;
  double pos;
  PxSessaoPlay s;
} Relato;

typedef struct {
  int ativo, iniciado, pausado;
  double ultPos;
  unsigned geracao;
  unsigned long long ultimoMs, criadoMs;
  PxSessaoPlay s;
} Sessao;

static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t sinalCtl = PTHREAD_COND_INITIALIZER, sinalRel = PTHREAD_COND_INITIALIZER;
static pthread_once_t uma = PTHREAD_ONCE_INIT;
static pthread_t fioCtl, fioRel;
static int fiosVivos, parar;
static RedeGrupo *grupo;
static unsigned geracao = 1;
static RedeJob *jobCtl;
static unsigned cancelEntrada;

static Tarefa filaCtl[PX_FILA_CTL];
static int iniCtl, nCtl;
static Relato filaRel[PX_FILA_REL];
static int iniRel, nRel, relEmVoo;

static PxConta conta;
static int perfilLido = -1;
static PxEstado estado = PX_EST_SEM_CONTA;
static char detalhe[160];
static int ultimoErro, nServ, reresolveu;

static CatItem *snapItens;
static CatFileira snapFils[PX_FIL_MAX];
static int nSnapFils, nSnapItens;
static unsigned snapVersao;
// Matching index (srvidx.h) of the connected server, this profile only.
static SrvIdx idx;
static char idxTag[JFID_TAG + 1];
#define PX_EPS_BUSCADOS 16
#define PX_EPS_VALIDADE_MS (30u * 60u * 1000u)
static struct { char serie[SRVIDX_ITEM]; unsigned long long ms; } epsBuscados[PX_EPS_BUSCADOS];
static int nEpsBuscados;

static char fontesAlvo[JFID_MAX];
static int fontesEstado = PX_FONTES_NADA;
static Stream *fontesLista;
static int nFontes;
static Sessao sessoes[PX_SESSOES];

int plex_disponivel(void) {
  return jellyfin_backend() != JF_BACKEND_WGT && (rede_pedido_capacidades() & REDE_CAP_JOB);
}

static void *trabalharCtl(void *u);
static void *trabalharRel(void *u);

static void iniciar(void) {
  grupo = rede_grupo_criar();
  if (pthread_create(&fioCtl, NULL, trabalharCtl, NULL) == 0) {
    if (pthread_create(&fioRel, NULL, trabalharRel, NULL) == 0) fiosVivos = 2;
    else fiosVivos = 1;
  }
}
static void garantir(void) { pthread_once(&uma, iniciar); }

// -------------------------------------------------------------- storage
static void arquivo(char *dst, size_t tam, int perfil) { snprintf(dst, tam, PX_ARQ_FMT, perfil); }

static void linha(char *dst, size_t tam, size_t *k, const char *chave, const char *v) {
  char limpo[400];
  size_t i = 0;
  for (; v && *v && i + 1 < sizeof limpo; v++) limpo[i++] = (*v == '\n' || *v == '\r') ? ' ' : *v;
  limpo[i] = 0;
  if (*k < tam) *k += (size_t)snprintf(dst + *k, tam - *k, "%s=%s\n", chave, limpo);
  apagarSegredo(limpo, sizeof limpo);
}

// 0600 + atomic rename, like jellyfin-p<N>.txt. Holds both tokens.
static int gravarConta(const PxConta *c, int perfil) {
  char nome[40], caminho[640], tmp[660], txt[2048];
  size_t k = 0;
  int fd, ok = 0;
  arquivo(nome, sizeof nome, perfil);
  if (!dados_caminho(caminho, sizeof caminho, nome)) return 0;
  linha(txt, sizeof txt, &k, "base", c->base);
  linha(txt, sizeof txt, &k, "sid", c->servidorId);
  linha(txt, sizeof txt, &k, "snome", c->servidorNome);
  linha(txt, sizeof txt, &k, "ver", c->versao);
  linha(txt, sizeof txt, &k, "unome", c->usuario);
  linha(txt, sizeof txt, &k, "dev", c->dispositivoId);
  linha(txt, sizeof txt, &k, "tokc", c->tokenConta);
  linha(txt, sizeof txt, &k, "toks", c->token);
  if (k >= sizeof txt) { apagarSegredo(txt, sizeof txt); return 0; }
  snprintf(tmp, sizeof tmp, "%s.tmp", caminho);
  dados_fs_travar();
  fd = open(tmp, O_WRONLY | O_CREAT | O_TRUNC, 0600);
  if (fd >= 0) {
    ok = write(fd, txt, k) == (ssize_t)k;
    if (close(fd) != 0) ok = 0;
    if (ok && rename(tmp, caminho) != 0) ok = 0;
    if (!ok) unlink(tmp);
  }
  dados_fs_liberar();
  apagarSegredo(txt, sizeof txt);
  return ok;
}

static void lerCampo(const char *txt, const char *chave, char *dst, size_t tam) {
  size_t nk = strlen(chave);
  const char *p = txt;
  dst[0] = 0;
  while (p && *p) {
    if (!strncmp(p, chave, nk) && p[nk] == '=') {
      const char *v = p + nk + 1, *e = strchr(v, '\n');
      size_t n = e ? (size_t)(e - v) : strlen(v);
      if (n >= tam) n = tam - 1;
      memcpy(dst, v, n); dst[n] = 0;
      return;
    }
    p = strchr(p, '\n');
    if (p) p++;
  }
}

static void novoDispositivo(char *dst, size_t tam) {
  unsigned char b[8];
  int fd = open("/dev/urandom", O_RDONLY), ok = 0, i;
  if (fd >= 0) { ok = read(fd, b, sizeof b) == (ssize_t)sizeof b; close(fd); }
  if (!ok) {
    unsigned long long x = agoraMs() ^ ((unsigned long long)getpid() << 20) ^ (unsigned long long)time(NULL);
    for (i = 0; i < 8; i++) b[i] = (unsigned char)(x >> (i * 8));
  }
  snprintf(dst, tam, "nuvio-%02x%02x%02x%02x%02x%02x%02x%02x-p%d",
           b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7], perfis_ativo());
}

// Under trava. The index always belongs to the connected server (8 hex of
// its machineIdentifier); another server or none drops it and loads that
// server's file, if any.
static void idxConferirLocked(void) {
  char tag[JFID_TAG + 1];
  int i;
  snprintf(tag, sizeof tag, "%.8s", conta.servidorId);
  for (i = 0; tag[i]; i++) if (tag[i] >= 'A' && tag[i] <= 'F') tag[i] = (char)(tag[i] + 32);
  if (!strcmp(tag, idxTag)) return;
  srvidx_limpar(&idx);
  nEpsBuscados = 0;
  copiar(idxTag, sizeof idxTag, tag);
  if (tag[0] && perfilLido >= 0) {
    int n = srvidx_ler(&idx, perfilLido, "px", tag);
    if (n > 0) printf("[plex] index from disk: %d row(s)\n", n);
  }
}

static void carregarLocked(void) {
  char nome[40], *txt;
  int p = perfis_ativo();
  perfilLido = p;
  memset(&conta, 0, sizeof conta);
  nServ = 0; reresolveu = 0;
  arquivo(nome, sizeof nome, p);
  txt = dados_ler(nome);
  if (txt) {
    lerCampo(txt, "base", conta.base, sizeof conta.base);
    lerCampo(txt, "sid", conta.servidorId, sizeof conta.servidorId);
    lerCampo(txt, "snome", conta.servidorNome, sizeof conta.servidorNome);
    lerCampo(txt, "ver", conta.versao, sizeof conta.versao);
    lerCampo(txt, "unome", conta.usuario, sizeof conta.usuario);
    lerCampo(txt, "dev", conta.dispositivoId, sizeof conta.dispositivoId);
    lerCampo(txt, "tokc", conta.tokenConta, sizeof conta.tokenConta);
    lerCampo(txt, "toks", conta.token, sizeof conta.token);
    apagarSegredo(txt, strlen(txt));
    free(txt);
  }
  if (!conta.dispositivoId[0]) novoDispositivo(conta.dispositivoId, sizeof conta.dispositivoId);
  copiar(conta.dispositivoNome, sizeof conta.dispositivoNome, nomeBackend());
  if (conta.tokenConta[0] && conta.token[0] && conta.base[0] && conta.servidorId[0]) estado = PX_EST_CONECTADO;
  else estado = PX_EST_SEM_CONTA;
  detalhe[0] = 0;
  srvidx_limpar(&idx);
  idxTag[0] = 0;
  nEpsBuscados = 0;
  idxConferirLocked();
}

static void soltarSnapshotLocked(void) {
  int i;
  nSnapFils = nSnapItens = 0;
  snapVersao++;
  free(fontesLista); fontesLista = NULL; nFontes = 0;
  fontesAlvo[0] = 0; fontesEstado = PX_FONTES_NADA;
  for (i = 0; i < PX_SESSOES; i++) {
    apagarSegredo(sessoes[i].s.url, sizeof sessoes[i].s.url);
    sessoes[i].ativo = 0;
  }
  for (i = 0; i < PX_FILA_REL; i++) apagarSegredo(&filaRel[i], sizeof filaRel[i]);
  nRel = 0;
}

static void avancarLocked(void) {
  geracao++;
  if (grupo) rede_grupo_avancar(grupo);
  if (jobCtl) rede_job_cancelar(jobCtl);
  nCtl = 0;
  soltarSnapshotLocked();
}

static void conferirPerfilLocked(void) {
  if (perfilLido != perfis_ativo()) { avancarLocked(); carregarLocked(); }
}

static int enfileirarLocked(int tipo, const char *a) {
  Tarefa *t;
  if (!fiosVivos || nCtl >= PX_FILA_CTL) return 0;
  t = &filaCtl[(iniCtl + nCtl) % PX_FILA_CTL];
  memset(t, 0, sizeof *t);
  t->tipo = tipo;
  t->geracao = geracao;
  t->entrada = cancelEntrada;
  copiar(t->a, sizeof t->a, a);
  nCtl++;
  pthread_cond_signal(&sinalCtl);
  return 1;
}

void plex_recarregar_bibliotecas(void) {
  garantir();
  pthread_mutex_lock(&trava);
  conferirPerfilLocked();
  if (estado == PX_EST_CONECTADO) enfileirarLocked(T_BIBLIOTECAS, "");
  pthread_mutex_unlock(&trava);
}

void plex_carregar(void) {
  garantir();
  pthread_mutex_lock(&trava);
  carregarLocked();
  pthread_mutex_unlock(&trava);
  if (plex_conectado()) plex_recarregar_bibliotecas();
}

void plex_perfil_trocou(void) {
  int recarregar;
  garantir();
  pthread_mutex_lock(&trava);
  avancarLocked();
  carregarLocked();
  recarregar = estado == PX_EST_CONECTADO;
  pthread_mutex_unlock(&trava);
  printf("[plex] profile changed: in-flight requests dropped\n");
  if (recarregar) plex_recarregar_bibliotecas();
}

void plex_esquecer(void) {
  char nome[40];
  garantir();
  pthread_mutex_lock(&trava);
  conferirPerfilLocked();
  avancarLocked();
  cancelEntrada++;
  arquivo(nome, sizeof nome, perfis_ativo());
  dados_apagar(nome);
  srvidx_apagar(perfis_ativo(), "px");
  apagarSegredo(&conta, sizeof conta);
  carregarLocked();
  pthread_mutex_unlock(&trava);
  // plex.tv keeps listing the device until the person removes it at
  // plex.tv/devices: there is no per-device revoke call we rely on.
  printf("[plex] signed out on this profile\n");
}

void plex_esquecer_todos(void) {
  char nome[40];
  int i;
  garantir();
  pthread_mutex_lock(&trava);
  avancarLocked();
  cancelEntrada++;
  for (i = 1; i <= 32; i++) { arquivo(nome, sizeof nome, i); dados_apagar(nome); }
  for (i = 0; i <= 32; i++) srvidx_apagar(i, "px");
  apagarSegredo(&conta, sizeof conta);
  carregarLocked();
  pthread_mutex_unlock(&trava);
}

void plex_encerrar(void) {
  int vivos;
  pthread_mutex_lock(&trava);
  parar = 1;
  avancarLocked();
  vivos = fiosVivos;
  pthread_cond_broadcast(&sinalCtl);
  pthread_cond_broadcast(&sinalRel);
  pthread_mutex_unlock(&trava);
  if (vivos >= 1) pthread_join(fioCtl, NULL);
  if (vivos >= 2) pthread_join(fioRel, NULL);
  pthread_mutex_lock(&trava);
  fiosVivos = 0;
  free(snapItens); snapItens = NULL;
  free(fontesLista); fontesLista = NULL;
  if (grupo) { rede_grupo_cancelar(grupo); rede_grupo_soltar(grupo); grupo = NULL; }
  apagarSegredo(&conta, sizeof conta);
  srvidx_limpar(&idx);
  idxTag[0] = 0;
  pthread_mutex_unlock(&trava);
}

// ----------------------------------------------------------- UI snapshot
PxEstado plex_estado(char *det, size_t tam) {
  PxEstado e;
  garantir();
  pthread_mutex_lock(&trava);
  conferirPerfilLocked();
  e = estado;
  if (det && tam) copiar(det, tam, e == PX_EST_CONECTADO ? conta.servidorNome : detalhe);
  pthread_mutex_unlock(&trava);
  return e;
}
const char *plex_servidor_nome(void) {
  static char s[96];
  pthread_mutex_lock(&trava);
  copiar(s, sizeof s, conta.servidorNome);
  pthread_mutex_unlock(&trava);
  return s;
}
const char *plex_usuario(void) {
  static char s[96];
  pthread_mutex_lock(&trava);
  copiar(s, sizeof s, estado == PX_EST_CONECTADO ? conta.usuario : "");
  pthread_mutex_unlock(&trava);
  return s;
}
int plex_conectado(void) {
  int c;
  pthread_mutex_lock(&trava);
  c = estado == PX_EST_CONECTADO && conta.token[0];
  pthread_mutex_unlock(&trava);
  return c;
}
int plex_ultimo_erro(void) {
  int e;
  pthread_mutex_lock(&trava);
  e = ultimoErro;
  pthread_mutex_unlock(&trava);
  return e;
}
int plex_n_servidores(void) {
  int n;
  pthread_mutex_lock(&trava);
  n = nServ;
  pthread_mutex_unlock(&trava);
  return n;
}

static const char *textoErro(int e) {
  switch (e) {
    case PX_ERR_REDE: return "server did not answer";
    case PX_ERR_AUTH: return "sign-in refused";
    case PX_ERR_HTTP: return "server error";
    case PX_ERR_FORMATO: return "not a Plex answer";
    case PX_ERR_INDISPONIVEL: return "unavailable on this platform";
    case PX_ERR_EXPIRADO: return "code expired";
    case PX_ERR_SEM_SERVIDOR: return "no reachable Plex server on this account";
    default: return "failed";
  }
}
static void definirEstadoLocked(PxEstado e, const char *det) {
  estado = e;
  copiar(detalhe, sizeof detalhe, det);
}
static void definirErroLocked(int e) {
  estado = PX_EST_ERRO;
  ultimoErro = e;
  detalhe[0] = 0;
  printf("[plex] action failed: %s\n", textoErro(e));
}

int plex_entrar(void) {
  int ok = 0;
  garantir();
  if (!plex_disponivel()) return 0;
  pthread_mutex_lock(&trava);
  conferirPerfilLocked();
  if (estado != PX_EST_CODIGO && estado != PX_EST_ENTRANDO && estado != PX_EST_CONECTADO) {
    cancelEntrada++;
    definirEstadoLocked(PX_EST_CODIGO, "");
    ok = enfileirarLocked(T_PIN, "");
    if (!ok) definirEstadoLocked(PX_EST_SEM_CONTA, "");
  }
  pthread_mutex_unlock(&trava);
  return ok;
}

void plex_cancelar_entrada(void) {
  pthread_mutex_lock(&trava);
  cancelEntrada++;
  if (jobCtl) rede_job_cancelar(jobCtl);
  if (estado == PX_EST_CODIGO || estado == PX_EST_ENTRANDO)
    definirEstadoLocked(PX_EST_SEM_CONTA, "");
  pthread_mutex_unlock(&trava);
}

int plex_proximo_servidor(void) {
  int ok = 0;
  garantir();
  pthread_mutex_lock(&trava);
  conferirPerfilLocked();
  if (estado == PX_EST_CONECTADO && conta.tokenConta[0]) {
    avancarLocked();                   // the old server's rows/sessions go with it
    definirEstadoLocked(PX_EST_ENTRANDO, "");
    ok = enfileirarLocked(T_RESOLVER, "next");
  }
  pthread_mutex_unlock(&trava);
  return ok;
}

unsigned plex_fileiras_versao(void) {
  unsigned v;
  pthread_mutex_lock(&trava);
  conferirPerfilLocked();
  v = snapVersao;
  pthread_mutex_unlock(&trava);
  return v;
}

int plex_chave_fileira(const char *chave) { return chave && !strncmp(chave, "plex_", 5); }

int plex_fileiras_copiar(CatItem *itens, int maxItens, CatFileira *fils, int maxFils, int *nItens) {
  int r, nf = 0, ni = 0;
  if (nItens) *nItens = 0;
  pthread_mutex_lock(&trava);
  conferirPerfilLocked();
  for (r = 0; r < nSnapFils && nf < maxFils; r++) {
    const CatFileira *f = &snapFils[r];
    if (ni + f->n > maxItens) break;
    memcpy(itens + ni, snapItens + f->ini, sizeof(CatItem) * (size_t)f->n);
    fils[nf] = *f;
    fils[nf].ini = ni;
    ni += f->n;
    nf++;
  }
  pthread_mutex_unlock(&trava);
  if (nItens) *nItens = ni;
  return nf;
}

int plex_casamento_ativo(void) {
  int ok;
  pthread_mutex_lock(&trava);
  ok = estado == PX_EST_CONECTADO && conta.token[0] && idx.n > 0;
  pthread_mutex_unlock(&trava);
  return ok;
}
int plex_indice_n(void) {
  int n;
  pthread_mutex_lock(&trava);
  n = idx.n;
  pthread_mutex_unlock(&trava);
  return n;
}

// ------------------------------------------------------ worker: control
static RedeJob *abrirTarefa(const Tarefa *t, PxConta *c) {
  RedeJob *j = NULL;
  pthread_mutex_lock(&trava);
  if (t->geracao == geracao && grupo) {
    *c = conta;
    j = rede_job_criar(grupo);
    jobCtl = j;
  }
  pthread_mutex_unlock(&trava);
  return j;
}
static int aindaVale(const Tarefa *t, RedeJob *j) {
  return t->geracao == geracao && rede_job_estado(j) == REDE_OK;
}
static void fecharTarefa(RedeJob *j) {
  pthread_mutex_lock(&trava);
  if (jobCtl == j) jobCtl = NULL;
  pthread_mutex_unlock(&trava);
  rede_job_soltar(j);
}

// plex.tv said 401: the account token is dead. Keep nothing but the device id.
static void expirouLocked(void) {
  avancarLocked();
  apagarSegredo(conta.tokenConta, sizeof conta.tokenConta);
  apagarSegredo(conta.token, sizeof conta.token);
  conta.base[0] = 0;
  gravarConta(&conta, perfilLido);
  definirEstadoLocked(PX_EST_EXPIROU, "session expired");
}

static void publicarConta(const Tarefa *t, RedeJob *j, PxConta *c, int nServidores) {
  pthread_mutex_lock(&trava);
  if (aindaVale(t, j)) {
    copiar(c->dispositivoNome, sizeof c->dispositivoNome, conta.dispositivoNome);
    conta = *c;
    nServ = nServidores;
    reresolveu = 0;
    gravarConta(&conta, perfilLido);
    idxConferirLocked();
    definirEstadoLocked(PX_EST_CONECTADO, conta.servidorNome);
    enfileirarLocked(T_BIBLIOTECAS, "");
  }
  pthread_mutex_unlock(&trava);
}

// Finds the server on the account and a reachable connection. `ciclar`: take
// the server after the current one instead of the current/default one.
static int resolverServidor(PxConta *c, RedeJob *j, int ciclar, int *nServidores) {
  PxServidor *srv = malloc(sizeof *srv * PX_SERVIDORES_MAX);
  int n, i, atual = -1, ini, e = PX_ERR_SEM_SERVIDOR, tentativas;
  if (!srv) return PX_ERR_REDE;
  n = px_recursos(c, j, srv, PX_SERVIDORES_MAX);
  if (n < 0) { free(srv); return n; }
  if (nServidores) *nServidores = n;
  if (n == 0) { free(srv); return PX_ERR_SEM_SERVIDOR; }
  for (i = 0; i < n; i++) if (c->servidorId[0] && !strcasecmp(srv[i].id, c->servidorId)) atual = i;
  if (ciclar && atual >= 0) ini = (atual + 1) % n;
  else if (atual >= 0) ini = atual;
  else { ini = 0; for (i = 0; i < n; i++) if (srv[i].dono) { ini = i; break; } }
  // Cycling tries exactly one server; the default walks the list until one answers.
  for (tentativas = ciclar && atual >= 0 ? 1 : n, i = 0; i < tentativas; i++) {
    e = px_escolher(c, j, &srv[(ini + i) % n]);
    if (e != PX_ERR_SEM_SERVIDOR) break;
  }
  apagarSegredo(srv, sizeof *srv * PX_SERVIDORES_MAX);
  free(srv);
  return e;
}

// Display name from plex.tv. Best effort: a failure leaves the name empty.
static void buscarUsuario(PxConta *c, RedeJob *j) {
  RedeResposta r;
  if (pedir(c, PX_TV "/api/v2/user", c->tokenConta, "GET", "account", 0, j, &r) == PX_OK) {
    px_ler_usuario(r.corpo, r.n_corpo, c->usuario, sizeof c->usuario);
    apagarSegredo(r.corpo, r.n_corpo);
    rede_resposta_limpar(&r);
  }
}

static void tarefaPin(const Tarefa *t) {
  PxConta c;
  RedeJob *j = abrirTarefa(t, &c);
  PxPin pin;
  char id[24], cod[8];
  unsigned meu;
  int e, exp = 0, nServidores = 0;
  if (!j) return;
  meu = t->entrada;
  e = px_pin_criar(&c, j, id, sizeof id, cod, sizeof cod, &exp);
  if (e != PX_OK) {
    pthread_mutex_lock(&trava);
    if (aindaVale(t, j) && meu == cancelEntrada) definirErroLocked(e);
    pthread_mutex_unlock(&trava);
    apagarSegredo(&c, sizeof c);
    fecharTarefa(j);
    return;
  }
  pxpin_criado(&pin, agoraMs(), id, cod, exp);
  pthread_mutex_lock(&trava);
  if (aindaVale(t, j) && meu == cancelEntrada) definirEstadoLocked(PX_EST_CODIGO, pin.codigo);
  pthread_mutex_unlock(&trava);
  // POLLING ENDS on link, expiry, cancel, profile switch or too many failures:
  // nothing polls plex.tv after the person leaves the screen.
  while (pin.est == PXP_ESPERANDO) {
    unsigned k;
    unsigned long long agora;
    for (k = 0; k < PXPIN_INTERVALO_MS / 100u; k++) {
      struct timespec d = { 0, 100 * 1000000L };
      int vivo;
      pthread_mutex_lock(&trava);
      vivo = aindaVale(t, j) && meu == cancelEntrada && !parar;
      pthread_mutex_unlock(&trava);
      if (!vivo) { pxpin_cancelar(&pin); goto fim; }
      nanosleep(&d, NULL);
    }
    agora = agoraMs();
    pxpin_relogio(&pin, agora);
    if (pin.est == PXP_ESPERANDO && pxpin_vencido(&pin, agora)) {
      char tok[96] = "";
      e = px_pin_conferir(&c, j, pin.id, tok, sizeof tok);
      pxpin_resultado(&pin, agoraMs(), e, tok);
      apagarSegredo(tok, sizeof tok);
    }
  }
  if (pin.est == PXP_LIGADO) {
    copiar(c.tokenConta, sizeof c.tokenConta, pin.token);
    pthread_mutex_lock(&trava);
    if (aindaVale(t, j) && meu == cancelEntrada) definirEstadoLocked(PX_EST_ENTRANDO, "");
    pthread_mutex_unlock(&trava);
    c.base[0] = c.servidorId[0] = 0;
    e = resolverServidor(&c, j, 0, &nServidores);
    if (e == PX_OK) { buscarUsuario(&c, j); publicarConta(t, j, &c, nServidores); }
    else {
      pthread_mutex_lock(&trava);
      if (aindaVale(t, j) && meu == cancelEntrada) definirErroLocked(e);
      pthread_mutex_unlock(&trava);
    }
  } else if (pin.est != PXP_CANCELADO) {
    pthread_mutex_lock(&trava);
    if (aindaVale(t, j) && meu == cancelEntrada)
      definirErroLocked(pin.est == PXP_EXPIRADO ? PX_ERR_EXPIRADO : PX_ERR_REDE);
    pthread_mutex_unlock(&trava);
  }
fim:
  apagarSegredo(&pin, sizeof pin);
  apagarSegredo(&c, sizeof c);
  fecharTarefa(j);
}

static void tarefaResolver(const Tarefa *t) {
  PxConta c;
  RedeJob *j = abrirTarefa(t, &c);
  int e, nServidores = 0;
  if (!j) return;
  e = resolverServidor(&c, j, !strcmp(t->a, "next"), &nServidores);
  if (e == PX_OK) publicarConta(t, j, &c, nServidores);
  else {
    pthread_mutex_lock(&trava);
    if (aindaVale(t, j)) {
      if (e == PX_ERR_AUTH) expirouLocked();
      else definirErroLocked(e);
    }
    pthread_mutex_unlock(&trava);
  }
  apagarSegredo(&c, sizeof c);
  fecharTarefa(j);
}

// THE MATCHING INDEX PASS (same policy as jellyfin.c): incremental from the
// newest updatedAt when an index exists, then a count check per library that
// pays for one full rebuild only when the totals disagree (deletions).
// Episodes (fetched on demand) survive a rebuild.
static void indexar(const Tarefa *t, RedeJob *j, const PxConta *c, const PxBiblioteca *libs, int nl) {
  SrvIdx novo;
  char tag[JFID_TAG + 1], *txt = NULL;
  long long marca;
  int nAtual, totalTodos, e = PX_OK, incremental, passada, i, perfil = -1;
  pthread_mutex_lock(&trava);
  if (!aindaVale(t, j)) { pthread_mutex_unlock(&trava); return; }
  idxConferirLocked();
  marca = idx.marca;
  nAtual = srvidx_n_tipo(&idx, 'm') + srvidx_n_tipo(&idx, 's');
  copiar(tag, sizeof tag, idxTag);
  pthread_mutex_unlock(&trava);
  if (!tag[0]) return;
  incremental = marca > 0 && nAtual > 0;
  for (passada = 0; passada < 2; passada++) {
    srvidx_iniciar(&novo);
    totalTodos = 0;
    for (i = 0; i < nl && e == PX_OK; i++) {
      int inicio = 0, total = 0, vistos;
      if (incremental) {
        e = px_indice(c, j, libs[i].id, 0, 0, 0, &novo, &total);   // the count, nothing else
        if (e < 0) break;
        e = PX_OK;
        totalTodos += total;
      }
      do {
        pthread_mutex_lock(&trava);
        if (!aindaVale(t, j)) e = PX_ERR_CANCELADO;
        pthread_mutex_unlock(&trava);
        if (e != PX_OK) break;
        vistos = px_indice(c, j, libs[i].id, inicio, PX_PAGINA_INDICE, incremental ? marca : 0, &novo, &total);
        if (vistos < 0) { e = vistos; break; }
        if (!vistos) break;
        inicio += vistos;
      } while (inicio < total && novo.n < SRVIDX_MAX);
    }
    if (e != PX_OK) break;
    pthread_mutex_lock(&trava);
    if (!aindaVale(t, j) || strcmp(tag, idxTag)) e = PX_ERR_CANCELADO;
    else if (incremental) srvidx_juntar(&idx, &novo);
    else {
      int k;
      for (k = 0; k < idx.n; k++) if (idx.v[k].tipo == 'e') srvidx_por(&novo, &idx.v[k]);
      srvidx_limpar(&idx);
      idx = novo;
      srvidx_iniciar(&novo);
    }
    nAtual = srvidx_n_tipo(&idx, 'm') + srvidx_n_tipo(&idx, 's');
    pthread_mutex_unlock(&trava);
    srvidx_limpar(&novo);
    if (e != PX_OK) break;
    printf("[plex] index: %d title(s), %s pass\n", nAtual, incremental ? "incremental" : "full");
    if (!incremental || totalTodos == nAtual) break;
    incremental = 0;
  }
  srvidx_limpar(&novo);
  if (e != PX_OK) return;
  pthread_mutex_lock(&trava);
  if (aindaVale(t, j) && !strcmp(tag, idxTag)) { txt = srvidx_serializar(&idx, tag); perfil = perfilLido; }
  pthread_mutex_unlock(&trava);
  if (txt && perfil >= 0) srvidx_gravar_texto(perfil, "px", txt);
  free(txt);
}

static void tarefaBibliotecas(const Tarefa *t) {
  PxConta c;
  RedeJob *j = abrirTarefa(t, &c);
  PxBiblioteca libs[PX_FIL_MAX];
  CatItem *itens;
  CatFileira fils[PX_FIL_MAX];
  int nl, nf = 0, ni = 0, e = PX_OK, i;
  if (!j) return;
  itens = calloc((size_t)PX_FIL_MAX * PX_POR_FILEIRA, sizeof *itens);
  if (!itens) { fecharTarefa(j); return; }
  nl = px_bibliotecas(&c, j, libs, PX_FIL_MAX);
  if (nl < 0) e = nl;
  for (i = 0; i < nl && e == PX_OK; i++) {
    int total, n = px_itens(&c, j, libs[i].id, 0, PX_POR_FILEIRA, itens + ni, PX_POR_FILEIRA, &total);
    if (n < 0) { e = n; break; }
    if (!n) continue;
    memset(&fils[nf], 0, sizeof fils[nf]);
    snprintf(fils[nf].chave, sizeof fils[nf].chave, "plex_%s", libs[i].id);
    snprintf(fils[nf].titulo, sizeof fils[nf].titulo, "%s · Plex", libs[i].nome);
    copiar(fils[nf].tipo, sizeof fils[nf].tipo, libs[i].tipo);
    fils[nf].ini = ni; fils[nf].n = n;
    ni += n; nf++;
  }
  pthread_mutex_lock(&trava);
  if (aindaVale(t, j)) {
    if (e == PX_OK) {
      // The periodic refresh usually brings the same rows: rebuild the Home
      // only when something a row shows changed.
      int igual = nf == nSnapFils && ni == nSnapItens && snapItens, q;
      for (q = 0; igual && q < nf; q++) igual = !strcmp(fils[q].chave, snapFils[q].chave) && fils[q].n == snapFils[q].n;
      for (q = 0; igual && q < ni; q++)
        igual = !strcmp(itens[q].imdb, snapItens[q].imdb) && itens[q].progresso == snapItens[q].progresso &&
                !strcmp(itens[q].poster, snapItens[q].poster);
      free(snapItens);
      snapItens = itens; itens = NULL;
      memcpy(snapFils, fils, sizeof(CatFileira) * (size_t)nf);
      nSnapFils = nf; nSnapItens = ni;
      if (!igual) snapVersao++;
      reresolveu = 0;
    } else if ((e == PX_ERR_REDE || e == PX_ERR_AUTH) && !reresolveu) {
      // The saved connection may be stale (new IP, relay, rotated server
      // token): ask plex.tv once for fresh ones before giving up.
      reresolveu = 1;
      enfileirarLocked(T_RESOLVER, "");
    }
  }
  pthread_mutex_unlock(&trava);
  free(itens);
  // Second pass, after the rows are on screen: the matching index.
  if (e == PX_OK) indexar(t, j, &c, libs, nl);
  apagarSegredo(&c, sizeof c);
  fecharTarefa(j);
}

// Registers check-in sessions for freshly listed sources (under trava).
static void registrarSessoesLocked(const PxPlayback *pb) {
  int i, k;
  for (i = 0; i < pb->n; i++) {
    int alvo = -1, igual = 0;
    unsigned long long velho = ~0ull;
    // A direct-play URL is the same every time the title is listed (unlike a
    // Jellyfin PlaySessionId), so a repeat refreshes the waiting entry and
    // never duplicates one that is already playing.
    for (k = 0; k < PX_SESSOES; k++)
      if (sessoes[k].ativo && sessoes[k].geracao == geracao && !strcmp(sessoes[k].s.url, pb->sessao[i].url)) {
        igual = 1;
        if (!sessoes[k].iniciado) { sessoes[k].s = pb->sessao[i]; sessoes[k].criadoMs = agoraMs(); }
        break;
      }
    if (igual) continue;
    for (k = 0; k < PX_SESSOES; k++) {
      if (!sessoes[k].ativo) { alvo = k; break; }
      if (!sessoes[k].iniciado && sessoes[k].criadoMs < velho) { velho = sessoes[k].criadoMs; alvo = k; }
    }
    if (alvo < 0) break;
    memset(&sessoes[alvo], 0, sizeof sessoes[alvo]);
    sessoes[alvo].ativo = 1;
    sessoes[alvo].geracao = geracao;
    sessoes[alvo].criadoMs = agoraMs();
    sessoes[alvo].s = pb->sessao[i];
  }
}

static void tarefaFontes(const Tarefa *t) {
  PxConta c;
  RedeJob *j = abrirTarefa(t, &c);
  char tag[JFID_TAG + 1], item[JFID_ITEM + 1];
  PxPlayback *pb;
  int e;
  if (!j) return;
  pb = malloc(sizeof *pb);
  if (!pb || !jfid_partes_p(JFID_PREFIXO_PLEX, t->a, tag, item)) e = PX_ERR_ENTRADA;
  else if (strncasecmp(c.servidorId, tag, JFID_TAG)) e = PX_ERR_OUTRO_SERVIDOR;
  else e = px_fontes(&c, j, item, pb);
  pthread_mutex_lock(&trava);
  if (aindaVale(t, j) && !strcmp(fontesAlvo, t->a)) {
    if (e != PX_OK) fontesEstado = PX_FONTES_FALHOU;
    else {
      free(fontesLista);
      fontesLista = pb->n ? malloc(sizeof(Stream) * (size_t)pb->n) : NULL;
      nFontes = fontesLista ? pb->n : 0;
      if (nFontes) memcpy(fontesLista, pb->fonte, sizeof(Stream) * (size_t)nFontes);
      fontesEstado = PX_FONTES_PRONTO;
      registrarSessoesLocked(pb);
    }
  }
  pthread_mutex_unlock(&trava);
  if (pb) { apagarSegredo(pb, sizeof *pb); free(pb); }
  apagarSegredo(&c, sizeof c);
  fecharTarefa(j);
}

static void *trabalharCtl(void *u) {
  (void)u;
  for (;;) {
    Tarefa t;
    pthread_mutex_lock(&trava);
    while (!parar && !nCtl) pthread_cond_wait(&sinalCtl, &trava);
    if (parar) { pthread_mutex_unlock(&trava); break; }
    t = filaCtl[iniCtl];
    apagarSegredo(&filaCtl[iniCtl], sizeof filaCtl[iniCtl]);
    iniCtl = (iniCtl + 1) % PX_FILA_CTL;
    nCtl--;
    pthread_mutex_unlock(&trava);
    switch (t.tipo) {
      case T_PIN: tarefaPin(&t); break;
      case T_RESOLVER: tarefaResolver(&t); break;
      case T_BIBLIOTECAS: tarefaBibliotecas(&t); break;
      case T_FONTES: tarefaFontes(&t); break;
      default: break;
    }
    apagarSegredo(&t, sizeof t);
  }
  return NULL;
}

// ------------------------------------------------------ worker: check-ins
static void *trabalharRel(void *u) {
  (void)u;
  for (;;) {
    Relato r;
    PxConta c;
    RedeJob *j = NULL;
    pthread_mutex_lock(&trava);
    while (!parar && !nRel) pthread_cond_wait(&sinalRel, &trava);
    if (parar) { pthread_mutex_unlock(&trava); break; }
    r = filaRel[iniRel];
    apagarSegredo(&filaRel[iniRel], sizeof filaRel[iniRel]);
    iniRel = (iniRel + 1) % PX_FILA_REL;
    nRel--;
    if (r.geracao == geracao && grupo && conta.token[0]) {
      c = conta;
      j = rede_job_criar(grupo);
      relEmVoo = 1;
    }
    pthread_mutex_unlock(&trava);
    if (j) {
      px_reportar(&c, j, r.evento, &r.s, r.pos);
      pthread_mutex_lock(&trava);
      relEmVoo = 0;
      pthread_mutex_unlock(&trava);
      rede_job_soltar(j);
      apagarSegredo(&c, sizeof c);
    }
    apagarSegredo(&r, sizeof r);
  }
  return NULL;
}

int plex_relatorios_pendentes(void) {
  int n;
  pthread_mutex_lock(&trava);
  n = nRel + relEmVoo;
  pthread_mutex_unlock(&trava);
  return n;
}

static void relatarLocked(int evento, const Sessao *s, double pos) {
  Relato *r;
  if (!fiosVivos || s->geracao != geracao) return;
  if (nRel >= PX_FILA_REL) {
    int i, alvo = -1;
    for (i = 0; i < nRel; i++) {
      int k = (iniRel + i) % PX_FILA_REL;
      if (filaRel[k].evento == PX_REL_PROGRESSO) { alvo = i; break; }
    }
    if (alvo < 0) return;
    for (i = alvo; i + 1 < nRel; i++)
      filaRel[(iniRel + i) % PX_FILA_REL] = filaRel[(iniRel + i + 1) % PX_FILA_REL];
    nRel--;
  }
  r = &filaRel[(iniRel + nRel) % PX_FILA_REL];
  memset(r, 0, sizeof *r);
  r->evento = evento;
  r->geracao = s->geracao;
  r->pos = pos;
  r->s = s->s;
  nRel++;
  pthread_cond_signal(&sinalRel);
}

static Sessao *sessaoDaUrlLocked(const char *url) {
  int i;
  if (!url || !*url) return NULL;
  for (i = 0; i < PX_SESSOES; i++)
    if (sessoes[i].ativo && sessoes[i].geracao == geracao && !strcmp(sessoes[i].s.url, url))
      return &sessoes[i];
  return NULL;
}

void plex_reproducao_tick(const char *url, double pos, double dur, int tocando) {
  Sessao *s;
  unsigned long long agora;
  (void)dur;
  if (!url || strncmp(url, "http", 4)) return;
  pthread_mutex_lock(&trava);
  s = sessaoDaUrlLocked(url);
  if (!s) { pthread_mutex_unlock(&trava); return; }
  agora = agoraMs();
  if (!s->iniciado) {
    if (tocando) {
      int i;
      for (i = 0; i < PX_SESSOES; i++)
        if (&sessoes[i] != s && sessoes[i].ativo && sessoes[i].iniciado) {
          relatarLocked(PX_REL_FIM, &sessoes[i], sessoes[i].ultPos);
          apagarSegredo(sessoes[i].s.url, sizeof sessoes[i].s.url);
          sessoes[i].ativo = 0;
        }
      s->iniciado = 1; s->pausado = 0; s->ultimoMs = agora;
      relatarLocked(PX_REL_INICIO, s, pos);
    }
  } else if (!tocando != !!s->pausado) {
    s->pausado = !tocando; s->ultimoMs = agora;
    relatarLocked(s->pausado ? PX_REL_PAUSA : PX_REL_RETOMA, s, pos);
  } else if (tocando && agora - s->ultimoMs >= PX_PROGRESSO_MS) {
    s->ultimoMs = agora;
    relatarLocked(PX_REL_PROGRESSO, s, pos);
  }
  if (pos > 0) s->ultPos = pos;
  pthread_mutex_unlock(&trava);
}

void plex_reproducao_fim(const char *url, double pos, double dur) {
  Sessao *s;
  (void)dur;
  if (!url) return;
  pthread_mutex_lock(&trava);
  s = sessaoDaUrlLocked(url);
  if (s) {
    if (s->iniciado) relatarLocked(PX_REL_FIM, s, pos);
    apagarSegredo(s->s.url, sizeof s->s.url);
    s->ativo = 0;
  }
  pthread_mutex_unlock(&trava);
}

// -------------------------------------------------------------- sources
int plex_fontes_pedir(const char *alvo) {
  char tag[JFID_TAG + 1], item[JFID_ITEM + 1];
  int ok = 0;
  garantir();
  if (!jfid_partes_p(JFID_PREFIXO_PLEX, alvo, tag, item)) return 0;
  pthread_mutex_lock(&trava);
  conferirPerfilLocked();
  free(fontesLista); fontesLista = NULL; nFontes = 0;
  copiar(fontesAlvo, sizeof fontesAlvo, alvo);
  if (estado != PX_EST_CONECTADO || strncasecmp(conta.servidorId, tag, JFID_TAG)) {
    fontesEstado = PX_FONTES_FALHOU;
  } else {
    fontesEstado = PX_FONTES_PENDENTE;
    ok = enfileirarLocked(T_FONTES, alvo);
    if (!ok) fontesEstado = PX_FONTES_FALHOU;
  }
  pthread_mutex_unlock(&trava);
  return ok;
}

int plex_fontes_colher(const char *alvo, Stream **lista, int *n) {
  int e;
  if (lista) *lista = NULL;
  if (n) *n = 0;
  pthread_mutex_lock(&trava);
  conferirPerfilLocked();
  if (!alvo || strcmp(alvo, fontesAlvo)) { pthread_mutex_unlock(&trava); return PX_FONTES_FALHOU; }
  e = fontesEstado;
  if (e == PX_FONTES_PRONTO) {
    if (lista) { *lista = fontesLista; fontesLista = NULL; }
    if (n) *n = nFontes;
    nFontes = 0;
    fontesEstado = PX_FONTES_NADA;
  } else if (e == PX_FONTES_FALHOU) fontesEstado = PX_FONTES_NADA;
  pthread_mutex_unlock(&trava);
  return e;
}

// Under trava: the index row of a catalogue title, -1 none (srvidx.h rules).
static int idxAcharLocked(const char *id, const char *tipo, const char *titulo, int ano, int *temp, int *ep) {
  char imdb[16];
  long tmdb;
  int k, serie;
  if (estado != PX_EST_CONECTADO || !idxTag[0] || strncasecmp(idxTag, conta.servidorId, JFID_TAG)) return -1;
  if (!srvidx_id_titulo(id, imdb, &tmdb, temp, ep)) return -1;
  serie = tipo && (!strcmp(tipo, "series") || !strcmp(tipo, "tv"));
  k = srvidx_achar(&idx, imdb, tmdb, 0, serie ? 's' : 'm');
  if (k < 0 && titulo && *titulo) k = srvidx_achar_titulo(&idx, srvidx_titulo_hash(titulo), ano, serie ? 's' : 'm');
  return k >= 0 ? k : -1;
}

int plex_tem(const char *id, const char *tipo, SrvAchado *info) {
  int k;
  garantir();
  pthread_mutex_lock(&trava);
  conferirPerfilLocked();
  k = idxAcharLocked(id, tipo, NULL, 0, NULL, NULL);
  if (k >= 0 && info) {
    memset(info, 0, sizeof *info);
    copiar(info->servidor, sizeof info->servidor, conta.servidorNome[0] ? conta.servidorNome : "Plex");
    copiar(info->tipoSrv, sizeof info->tipoSrv, "Plex");
    info->altura = idx.v[k].altura;
    info->hdr = idx.v[k].hdr;
    info->visto = idx.v[k].visto;
    info->local = srvidx_host_local(conta.base);
  }
  pthread_mutex_unlock(&trava);
  return k >= 0;
}

static int epsRecentesLocked(const char *serie) {
  int i;
  for (i = 0; i < nEpsBuscados; i++)
    if (!strcmp(epsBuscados[i].serie, serie)) return agoraMs() - epsBuscados[i].ms < PX_EPS_VALIDADE_MS;
  return 0;
}
static void epsMarcarLocked(const char *serie) {
  int i, alvo = -1;
  for (i = 0; i < nEpsBuscados; i++) if (!strcmp(epsBuscados[i].serie, serie)) alvo = i;
  if (alvo < 0) {
    if (nEpsBuscados < PX_EPS_BUSCADOS) alvo = nEpsBuscados++;
    else { memmove(epsBuscados, epsBuscados + 1, sizeof epsBuscados[0] * (PX_EPS_BUSCADOS - 1)); alvo = PX_EPS_BUSCADOS - 1; }
  }
  copiar(epsBuscados[alvo].serie, sizeof epsBuscados[alvo].serie, serie);
  epsBuscados[alvo].ms = agoraMs();
}

// A regular catalogue title ("tt...", "tmdb:...") the server has. Runs on the
// extra-source thread of addons.c, so it may block; every wait is a RedeJob of
// the current generation. No index hit: no request at all.
int plex_consultar(const char *id, const char *tipo, const char *titulo, int ano,
                   int (*cancelado)(void *), void *ctx, Stream **saida) {
  char rk[SRVIDX_ITEM] = "", serieRk[SRVIDX_ITEM] = "", tag[JFID_TAG + 1];
  int temp = 0, ep = 0, serie, k, e = PX_OK, buscar = 0, n = 0;
  PxConta c;
  RedeJob *j = NULL;
  unsigned g = 0;
  PxPlayback *pb = NULL;
  *saida = NULL;
  garantir();
  serie = tipo && (!strcmp(tipo, "series") || !strcmp(tipo, "tv"));
  pthread_mutex_lock(&trava);
  conferirPerfilLocked();
  k = grupo ? idxAcharLocked(id, tipo, titulo, ano, &temp, &ep) : -1;
  if (k >= 0) {
    copiar(rk, sizeof rk, idx.v[k].item);
    if (serie) {
      int ke;
      // A series without an episode in the id means "the first one", as in addons.c.
      if (temp <= 0 || ep <= 0) { temp = temp > 0 ? temp : 1; ep = ep > 0 ? ep : 1; }
      copiar(serieRk, sizeof serieRk, rk);
      ke = srvidx_achar_ep(&idx, serieRk, temp, ep);
      if (ke >= 0) copiar(rk, sizeof rk, idx.v[ke].item);
      else if (epsRecentesLocked(serieRk)) rk[0] = 0;
      else buscar = 1;
    }
    if (rk[0]) { c = conta; g = geracao; j = rede_job_criar(grupo); copiar(tag, sizeof tag, idxTag); }
  }
  pthread_mutex_unlock(&trava);
  if (!j) return 0;
  if (buscar) {
    // Episodes on demand, once per show per half hour, into the index (and disk).
    SrvIdx eps;
    char *txt = NULL;
    int perfil = -1;
    srvidx_iniciar(&eps);
    e = px_indice_episodios(&c, j, serieRk, &eps);
    rk[0] = 0;
    pthread_mutex_lock(&trava);
    if (g == geracao && rede_job_estado(j) == REDE_OK && !strcmp(tag, idxTag) && e >= 0) {
      int ke;
      srvidx_juntar(&idx, &eps);
      epsMarcarLocked(serieRk);
      ke = srvidx_achar_ep(&idx, serieRk, temp, ep);
      if (ke >= 0) copiar(rk, sizeof rk, idx.v[ke].item);
      if (eps.n) { txt = srvidx_serializar(&idx, tag); perfil = perfilLido; }
    }
    pthread_mutex_unlock(&trava);
    srvidx_limpar(&eps);
    if (txt && perfil >= 0) srvidx_gravar_texto(perfil, "px", txt);
    free(txt);
    e = rk[0] ? PX_OK : PX_ERR_HTTP;
  }
  if (e == PX_OK && cancelado && cancelado(ctx)) e = PX_ERR_CANCELADO;
  if (e == PX_OK) {
    pb = malloc(sizeof *pb);
    e = pb ? px_fontes(&c, j, rk, pb) : PX_ERR_REDE;
  }
  pthread_mutex_lock(&trava);
  if (g != geracao || rede_job_estado(j) != REDE_OK) e = PX_ERR_CANCELADO;
  else if (e == PX_OK && pb->n) registrarSessoesLocked(pb);
  pthread_mutex_unlock(&trava);
  if (e == PX_OK && pb->n) {
    Stream *l = malloc(sizeof(Stream) * (size_t)pb->n);
    if (l) {
      int i;
      memcpy(l, pb->fonte, sizeof(Stream) * (size_t)pb->n);
      // The server's own name: the facade turns it into "Local · <name>".
      for (i = 0; i < pb->n; i++) copiar(l[i].provedor, sizeof l[i].provedor, c.servidorNome[0] ? c.servidorNome : "Plex");
      *saida = l;
      n = pb->n;
    }
  } else n = e == PX_ERR_CANCELADO ? -1 : 0;
  if (pb) { apagarSegredo(pb, sizeof *pb); free(pb); }
  apagarSegredo(&c, sizeof c);
  rede_job_soltar(j);
  return n;
}

// ---------------------------------------------------------- title page
int plex_ficha(CatItem *item, CatEp *eps, int maxEps) {
  char tag[JFID_TAG + 1], id[JFID_ITEM + 1];
  PxConta c;
  RedeJob *j = NULL;
  unsigned g;
  CatItem *novo;
  int ne = 0, e;
  garantir();
  if (!item || !jfid_partes_p(JFID_PREFIXO_PLEX, item->imdb, tag, id)) return -1;
  pthread_mutex_lock(&trava);
  conferirPerfilLocked();
  g = geracao;
  if (estado == PX_EST_CONECTADO && !strncasecmp(conta.servidorId, tag, JFID_TAG) && grupo) {
    c = conta;
    j = rede_job_criar(grupo);
  }
  pthread_mutex_unlock(&trava);
  if (!j) return -1;
  novo = malloc(sizeof *novo);
  if (!novo) { rede_job_soltar(j); apagarSegredo(&c, sizeof c); return -1; }
  e = px_detalhe(&c, j, id, novo);
  if (e == PX_OK && !strcmp(item->tipo, "series") && eps && maxEps > 0) {
    ne = px_episodios(&c, j, id, eps, maxEps);
    if (ne < 0) { e = ne; ne = 0; }
  }
  pthread_mutex_lock(&trava);
  if (g != geracao || rede_job_estado(j) != REDE_OK) e = PX_ERR_CANCELADO;
  pthread_mutex_unlock(&trava);
  rede_job_soltar(j);
  apagarSegredo(&c, sizeof c);
  if (e != PX_OK) { free(novo); return -1; }
  if (!novo->poster[0]) copiar(novo->poster, sizeof novo->poster, item->poster);
  if (!novo->backdrop[0]) copiar(novo->backdrop, sizeof novo->backdrop, item->backdrop);
  if (ne > 0) {
    int k, q;
    novo->nTemporadas = 0;
    for (k = 0; k < ne; k++) {
      int ja = 0, t = eps[k].temporada;
      for (q = 0; q < novo->nTemporadas; q++) if (novo->temporadas[q] == t) ja = 1;
      if (!ja && novo->nTemporadas < CAT_TEMP_MAX) novo->temporadas[novo->nTemporadas++] = t;
    }
  }
  novo->naLista = item->naLista;
  novo->naColecao = item->naColecao;
  novo->retomadoMs = item->retomadoMs;
  *item = *novo;
  free(novo);
  return ne;
}
