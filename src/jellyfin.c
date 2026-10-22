// Jellyfin client (F11). Contract and limits in jellyfin.h; research in
// docs/plans/media-servers-1.8/README.md; status in
// docs/releases/1.8.0/F11-JELLYFIN-STATUS.md.
//
// Routes are pinned to Jellyfin 10.9+ (the /Items/{id}?userId= item route and
// the POST-only /QuickConnect/Initiate exist there). Older servers answer 404
// on the item route and the title page stays with the row data.
#include "jellyfin.h"
#include "dados.h"
#include "js.h"
#include "jsraiz.h"
#include "jsw.h"
#include "perfis.h"
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

#ifndef NV_JF_VERSAO
#define NV_JF_VERSAO "1.8.0"
#endif
#define JF_PRAZO_MS 15000u
#define JF_PRAZO_LISTA_MS 20000u
#define JF_CORPO_MAX (4u * 1024u * 1024u)
#define JF_QC_INTERVALO_MS 2000u
#define JF_QC_TETO_MS (5u * 60u * 1000u)
#define JF_SESSOES 16
#define JF_FILA_CTL 8
#define JF_FILA_REL 32
#define JF_TAXA_MAX 120000000

// ------------------------------------------------------------ small helpers
long long jf_ticks(double seg) {
  if (!(seg > 0.0)) return 0;
  if (seg > 9.0e11) seg = 9.0e11;          // int64 ticks stay far from overflow
  return (long long)(seg * 10000000.0 + 0.5);
}
double jf_seg(long long ticks) { return ticks > 0 ? (double)ticks / 10000000.0 : 0.0; }

const char *jellyfin_metodo_nome(int m) {
  return m == JF_METODO_DIRETO ? "DirectPlay" : m == JF_METODO_STREAM ? "DirectStream" : "Transcode";
}

JfBackend jellyfin_backend(void) {
#if defined(__EMSCRIPTEN__)
  return JF_BACKEND_WGT;
#elif defined(NV_ANDROID)
  return JF_BACKEND_ANDROID;
#elif defined(NV_TPK)
  return JF_BACKEND_TPK;
#elif defined(__APPLE__)
  return JF_BACKEND_HOST;
#else
  return JF_BACKEND_WEBOS;
#endif
}
const char *jellyfin_backend_nome(JfBackend b) {
  switch (b) {
    case JF_BACKEND_ANDROID: return "Android TV";
    case JF_BACKEND_WEBOS: return "LG webOS";
    case JF_BACKEND_TPK: return "Samsung Tizen";
    case JF_BACKEND_WGT: return "Samsung Tizen web";
    default: return "Desktop";
  }
}

// ------------------------------------------------------------- URL / auth
// Emby serves its API under "<base>/emby/...". The person may type the base
// with or without that suffix: it is stripped here and re-added on every
// request (jf_prefixo), so a single stored base works for both spellings.
int jf_url_normalizar_tipo(int tipo, const char *entrada, char *dst, size_t tam) {
  int e = jf_url_normalizar(entrada, dst, tam);
  if (e == JF_OK && tipo == JF_TIPO_EMBY) {
    size_t n = strlen(dst);
    if (n > 5 && !strcasecmp(dst + n - 5, "/emby")) dst[n - 5] = 0;
  }
  return e;
}

int jf_url_normalizar(const char *entrada, char *dst, size_t tam) {
  const char *p = entrada ? entrada : "", *esquema = "http://", *aut, *fimAut;
  char tmp[600];
  size_t n, i;
  if (!dst || tam < 16) return JF_ERR_ENTRADA;
  dst[0] = 0;
  while (*p == ' ' || *p == '\t') p++;
  n = strlen(p);
  while (n && (p[n - 1] == ' ' || p[n - 1] == '\t')) n--;
  if (!n || n >= sizeof tmp - 8) return JF_ERR_ENTRADA;
  for (i = 0; i < n; i++) {
    unsigned char c = (unsigned char)p[i];
    if (c <= ' ' || c == 127 || c == '"' || c == '\\' || c == '?' || c == '#') return JF_ERR_ENTRADA;
  }
  if (n > 7 && !strncasecmp(p, "http://", 7)) { p += 7; n -= 7; }
  else if (n > 8 && !strncasecmp(p, "https://", 8)) { p += 8; n -= 8; esquema = "https://"; }
  else if (memchr(p, ':', n) && strstr(p, "://")) return JF_ERR_ENTRADA;   // ftp://, file://
  aut = p;
  fimAut = memchr(p, '/', n);
  if (!fimAut) fimAut = p + n;
  if (fimAut == aut || memchr(aut, '@', (size_t)(fimAut - aut))) return JF_ERR_ENTRADA;
  while (n && p[n - 1] == '/') n--;
  if (snprintf(tmp, sizeof tmp, "%s%.*s", esquema, (int)n, p) >= (int)sizeof tmp) return JF_ERR_ENTRADA;
  if (strlen(tmp) >= tam) return JF_ERR_ENTRADA;
  copiar(dst, tam, tmp);
  return JF_OK;
}

// Values inside MediaBrowser="..." pairs: printable, no quotes/commas.
static void campoAuth(char *dst, size_t tam, const char *src) {
  size_t k = 0;
  for (; src && *src && k + 1 < tam; src++) {
    unsigned char c = (unsigned char)*src;
    dst[k++] = (c < 32 || c == 127 || c == '"' || c == ',' || c == '\\' || c >= 128) ? '_' : (char)c;
  }
  dst[k] = 0;
}

const char *jf_prefixo(const JfConta *c) { return c && c->tipo == JF_TIPO_EMBY ? "/emby" : ""; }
const char *jf_tipo_nome(int tipo) { return tipo == JF_TIPO_EMBY ? "Emby" : "Jellyfin"; }

void jf_cabecalho_auth(const JfConta *c, char *dst, size_t tam) {
  char dev[64], devId[96], tok[96];
  const char *nome = c && c->tipo == JF_TIPO_EMBY ? "X-Emby-Authorization" : "Authorization";
  campoAuth(dev, sizeof dev, c && c->dispositivoNome[0] ? c->dispositivoNome : jellyfin_backend_nome(jellyfin_backend()));
  campoAuth(devId, sizeof devId, c && c->dispositivoId[0] ? c->dispositivoId : "nuvio");
  campoAuth(tok, sizeof tok, c ? c->token : "");
  if (tok[0])
    snprintf(dst, tam, "%s: MediaBrowser Client=\"Nuvio\", Device=\"%s\", "
             "DeviceId=\"%s\", Version=\"%s\", Token=\"%s\"", nome, dev, devId, NV_JF_VERSAO, tok);
  else
    snprintf(dst, tam, "%s: MediaBrowser Client=\"Nuvio\", Device=\"%s\", "
             "DeviceId=\"%s\", Version=\"%s\"", nome, dev, devId, NV_JF_VERSAO);
  apagarSegredo(tok, sizeof tok);
}

// One request. Logs operation/status/latency only.
static int pedir(const JfConta *c, const char *base, RedeJob *job, const char *metodo,
                 const char *caminho, const char *corpo, const char *operacao,
                 unsigned prazo, RedeResposta *r) {
  char url[2048], auth[512], tokh[112];
  const char *cabs[5];
  int k = 0, emby = c && c->tipo == JF_TIPO_EMBY;
  RedePedido p;
  memset(r, 0, sizeof *r);
  if (!(rede_pedido_capacidades() & REDE_CAP_JOB)) return JF_ERR_INDISPONIVEL;
  if (!base || !base[0] || snprintf(url, sizeof url, "%s%s%s", base, jf_prefixo(c), caminho) >= (int)sizeof url)
    return JF_ERR_ENTRADA;
  jf_cabecalho_auth(c, auth, sizeof auth);
  cabs[k++] = "Accept: application/json";
  cabs[k++] = auth;
  tokh[0] = 0;
  if (emby && c->token[0]) {
    // Emby also reads the token from its own header; harmless duplicate of
    // the Token= pair above, and what older Emby builds look for first.
    snprintf(tokh, sizeof tokh, "X-Emby-Token: %.90s", c->token);
    cabs[k++] = tokh;
  }
  if (corpo) cabs[k++] = "Content-Type: application/json";
  cabs[k] = NULL;
  memset(&p, 0, sizeof p);
  p.metodo = metodo;
  p.url = url;
  p.cabecalhos = cabs;
  p.corpo = corpo;
  p.n_corpo = corpo ? strlen(corpo) : 0;
  p.prazo_ms = prazo ? prazo : JF_PRAZO_MS;
  p.max_bytes = JF_CORPO_MAX;
  p.seguir = 1;
  p.job = job;
  // #290: the configured personal server may use a self-signed or LAN
  // certificate. Only this server's own origin skips verification (rede.h).
  p.tls_pessoal = 1;
  if (!strncmp(url, "https://", 8)) {
    static int avisado;
    if (!avisado) {
      avisado = 1;
      printf("[%s] TLS: certificado nao verificado (servidor pessoal)\n", emby ? "emby" : "jellyfin");
    }
  }
  rede_pedir(&p, r);
  apagarSegredo(auth, sizeof auth);
  apagarSegredo(tokh, sizeof tokh);
  // The curl code is what tells a certificate refusal (60/51), a missing CA
  // file (77) and a TLS handshake the TV cannot do (35) apart (#290).
  printf("[%s] %s: HTTP %d, %u ms%s", emby ? "emby" : "jellyfin", operacao, r->status, r->ms,
         r->erro == REDE_OK ? "" : r->erro == REDE_CANCELADO || r->erro == REDE_GERACAO
                                   ? " (cancelled)" : " (transport)");
  if (r->erro != REDE_OK && r->erro != REDE_CANCELADO && r->erro != REDE_GERACAO)
    printf(" erro=%d curl=%d %s", (int)r->erro, r->curl_erro, !strncmp(url, "https://", 8) ? "https" : "http");
  printf("\n");
  apagarSegredo(url, sizeof url);   // may carry nothing secret, but cheap
  fflush(stdout);
  if (r->erro == REDE_CANCELADO || r->erro == REDE_GERACAO) { rede_resposta_limpar(r); return JF_ERR_CANCELADO; }
  if (r->erro == REDE_INDISPONIVEL) { rede_resposta_limpar(r); return JF_ERR_INDISPONIVEL; }
  if (r->erro != REDE_OK) { rede_resposta_limpar(r); return JF_ERR_REDE; }
  if (r->status == 401 || r->status == 403) { rede_resposta_limpar(r); return JF_ERR_AUTH; }
  if (r->status < 200 || r->status >= 300) {
    int s = r->status;
    rede_resposta_limpar(r);
    return s == 404 ? JF_ERR_EXPIRADO : JF_ERR_HTTP;
  }
  return JF_OK;
}

// ----------------------------------------------------------- server/login
int jf_publico(const char *base, RedeJob *job, JfConta *c) {
  RedeResposta r;
  char id[64] = "", nome[96] = "", ver[24] = "", prod[64] = "";
  int e = pedir(c, base, job, "GET", "/System/Info/Public", NULL, "server check", 0, &r);
  if (e == JF_ERR_EXPIRADO) return JF_ERR_FORMATO;   // 404: not this kind of server root
  if (e) return e;
  txtRaiz(r.corpo, r.corpo + r.n_corpo, "Id", id, sizeof id);
  txtRaiz(r.corpo, r.corpo + r.n_corpo, "ServerName", nome, sizeof nome);
  txtRaiz(r.corpo, r.corpo + r.n_corpo, "Version", ver, sizeof ver);
  txtRaiz(r.corpo, r.corpo + r.n_corpo, "ProductName", prod, sizeof prod);
  rede_resposta_limpar(&r);
  {
    char limpo[40];
    // The two servers answer the same route and each also answers the other's
    // prefix (Jellyfin keeps a legacy /emby alias): the product name decides,
    // so a Jellyfin address typed into the Emby row (or the reverse) is refused
    // instead of silently signing in against the wrong contract.
    { char *q; for (q = prod; *q; q++) if (*q >= 'A' && *q <= 'Z') *q = (char)(*q + 32);
      if (c->tipo == JF_TIPO_EMBY ? strstr(prod, "jellyfin") != NULL : strstr(prod, "emby") != NULL)
        return JF_ERR_FORMATO; }
    if (!jfid_hex_limpo(id, limpo, 32) || strlen(limpo) < JFID_TAG) return JF_ERR_FORMATO;
    copiar(c->servidorId, sizeof c->servidorId, limpo);
  }
  copiar(c->base, sizeof c->base, base);
  copiar(c->servidorNome, sizeof c->servidorNome, nome);
  copiar(c->versao, sizeof c->versao, ver);
  return JF_OK;
}

int jf_qc_habilitado(const JfConta *c, RedeJob *job) {
  RedeResposta r;
  if (c->tipo == JF_TIPO_EMBY) return 0;   // Emby has no Quick Connect: user + password only
  int e = pedir(c, c->base, job, "GET", "/QuickConnect/Enabled", NULL, "quick connect enabled", 0, &r), sim;
  if (e == JF_ERR_EXPIRADO) return 0;   // 404 on servers without the feature
  if (e) return e;
  sim = r.corpo && !strncmp(r.corpo + strspn(r.corpo, " \r\n\t"), "true", 4);
  rede_resposta_limpar(&r);
  return sim;
}

int jf_qc_iniciar(const JfConta *c, RedeJob *job, char *segredo, size_t ns, char *codigo, size_t nc) {
  RedeResposta r;
  if (c->tipo == JF_TIPO_EMBY) return JF_ERR_QC_DESLIGADO;
  int e = pedir(c, c->base, job, "POST", "/QuickConnect/Initiate", NULL, "quick connect initiate", 0, &r);
  if (e == JF_ERR_AUTH) return JF_ERR_QC_DESLIGADO;   // 401 when disabled
  if (e) return e;
  segredo[0] = codigo[0] = 0;
  txtRaiz(r.corpo, r.corpo + r.n_corpo, "Secret", segredo, ns);
  txtRaiz(r.corpo, r.corpo + r.n_corpo, "Code", codigo, nc);
  apagarSegredo(r.corpo, r.n_corpo);
  rede_resposta_limpar(&r);
  return segredo[0] && codigo[0] ? JF_OK : JF_ERR_FORMATO;
}

static int segredoValido(const char *s) {
  size_t n = 0;
  for (; s && *s; s++, n++)
    if (!((*s >= '0' && *s <= '9') || (*s >= 'a' && *s <= 'z') || (*s >= 'A' && *s <= 'Z'))) return 0;
  return n > 0 && n <= 128;
}

int jf_qc_conferir(const JfConta *c, RedeJob *job, const char *segredo) {
  RedeResposta r;
  char caminho[200];
  int e, ok;
  if (!segredoValido(segredo)) return JF_ERR_ENTRADA;
  snprintf(caminho, sizeof caminho, "/QuickConnect/Connect?secret=%s", segredo);
  e = pedir(c, c->base, job, "GET", caminho, NULL, "quick connect poll", 0, &r);
  apagarSegredo(caminho, sizeof caminho);
  if (e) return e;
  ok = boolRaiz(r.corpo, r.corpo + r.n_corpo, "Authenticated", 0);
  apagarSegredo(r.corpo, r.n_corpo);
  rede_resposta_limpar(&r);
  return ok;
}

// Parses an AuthenticateBy* answer (same shape on Jellyfin and Emby). Wipes
// the buffer: it holds the access token.
int jf_ler_autenticacao(JfConta *c, char *corpo, size_t n) {
  const char *ui, *uf;
  char tok[96] = "", uid[64] = "", nome[96] = "", limpo[40];
  txtRaiz(corpo, corpo + n, "AccessToken", tok, sizeof tok);
  if (blocoRaiz(corpo, corpo + n, "User", &ui, &uf)) {
    txtRaiz(ui, uf, "Id", uid, sizeof uid);
    txtRaiz(ui, uf, "Name", nome, sizeof nome);
  }
  apagarSegredo(corpo, n);
  if (!tok[0] || strlen(tok) >= sizeof c->token || !jfid_hex_limpo(uid, limpo, 32)) {
    apagarSegredo(tok, sizeof tok);
    return JF_ERR_FORMATO;
  }
  copiar(c->token, sizeof c->token, tok);
  copiar(c->usuarioId, sizeof c->usuarioId, limpo);
  copiar(c->usuarioNome, sizeof c->usuarioNome, nome);
  apagarSegredo(tok, sizeof tok);
  return JF_OK;
}
static int lerAutenticacao(JfConta *c, RedeResposta *r) {
  return jf_ler_autenticacao(c, r->corpo, r->n_corpo);
}

int jf_qc_autenticar(JfConta *c, RedeJob *job, const char *segredo) {
  RedeResposta r;
  Jsw w;
  int e;
  if (!segredoValido(segredo)) return JF_ERR_ENTRADA;
  jsw_iniciar(&w);
  jsw_obj_ini(&w); jsw_cs(&w, "Secret", segredo); jsw_obj_fim(&w);
  if (!jsw_texto_final(&w)) { jsw_livre(&w); return JF_ERR_ENTRADA; }
  e = pedir(c, c->base, job, "POST", "/Users/AuthenticateWithQuickConnect", jsw_texto_final(&w),
            "quick connect sign-in", 0, &r);
  if (w.p) apagarSegredo(w.p, w.cap);
  jsw_livre(&w);
  if (e) return e;
  e = lerAutenticacao(c, &r);
  rede_resposta_limpar(&r);
  return e;
}

int jf_autenticar_senha(JfConta *c, RedeJob *job, const char *usuario, char *senha) {
  RedeResposta r;
  Jsw w;
  int e;
  if (!usuario || !*usuario || strlen(usuario) > 128 || !senha || strlen(senha) > 256) {
    if (senha) apagarSegredo(senha, strlen(senha));
    return JF_ERR_ENTRADA;
  }
  jsw_iniciar(&w);
  jsw_obj_ini(&w); jsw_cs(&w, "Username", usuario); jsw_cs(&w, "Pw", senha); jsw_obj_fim(&w);
  apagarSegredo(senha, strlen(senha));
  if (!jsw_texto_final(&w)) { if (w.p) apagarSegredo(w.p, w.cap); jsw_livre(&w); return JF_ERR_ENTRADA; }
  e = pedir(c, c->base, job, "POST", "/Users/AuthenticateByName", jsw_texto_final(&w),
            "password sign-in", 0, &r);
  if (w.p) apagarSegredo(w.p, w.cap);
  jsw_livre(&w);
  if (e) return e;
  e = lerAutenticacao(c, &r);
  rede_resposta_limpar(&r);
  return e;
}

int jf_sair(const JfConta *c, RedeJob *job) {
  RedeResposta r;
  int e = pedir(c, c->base, job, "POST", "/Sessions/Logout", "{}", "sign out", 5000, &r);
  if (!e) rede_resposta_limpar(&r);
  return e;
}

// ----------------------------------------------------------- catalogue
static void imagem(const JfConta *c, const char *item, const char *tipo, const char *tag,
                   const char *tamanho, char *dst, size_t tam) {
  dst[0] = 0;
  if (!tag || !tag[0]) return;
  // Image routes are anonymous on Jellyfin: no token in poster URLs, so the
  // texture cache and the catalogue cache never hold a credential.
  snprintf(dst, tam, "%s%s/Items/%s/Images/%s?%s&quality=90&tag=%s", c->base, jf_prefixo(c), item, tipo,
           tamanho, tag);
}

static const char *prefixoId(const JfConta *c) {
  return c->tipo == JF_TIPO_EMBY ? JFID_PREFIXO_EMBY : JFID_PREFIXO;
}
static int itemDe(const JfConta *c, const char *ini, const char *fim, CatItem *it) {
  char id[64] = "", tipo[24] = "", limpo[40], tag[64];
  const char *bi, *bf;
  double ano, ticks;
  memset(it, 0, sizeof *it);
  txtRaiz(ini, fim, "Id", id, sizeof id);
  txtRaiz(ini, fim, "Type", tipo, sizeof tipo);
  if (!jfid_hex_limpo(id, limpo, 32)) return 0;
  if (!strcmp(tipo, "Movie")) copiar(it->tipo, sizeof it->tipo, "movie");
  else if (!strcmp(tipo, "Series")) copiar(it->tipo, sizeof it->tipo, "series");
  else return 0;
  if (!jfid_montar_p(prefixoId(c), it->imdb, sizeof it->imdb, c->servidorId, limpo)) return 0;
  txtRaiz(ini, fim, "Name", it->titulo, sizeof it->titulo);
  txtRaiz(ini, fim, "Overview", it->sinopse, sizeof it->sinopse);
  txtRaiz(ini, fim, "OfficialRating", it->classificacao, sizeof it->classificacao);
  ano = numRaiz(ini, fim, "ProductionYear", 0);
  ticks = numRaiz(ini, fim, "RunTimeTicks", 0);
  if (ano > 0 && ticks > 0 && it->tipo[0] == 'm')
    snprintf(it->meta, sizeof it->meta, "%d · %d min", (int)ano, (int)(jf_seg((long long)ticks) / 60.0 + 0.5));
  else if (ano > 0) snprintf(it->meta, sizeof it->meta, "%d", (int)ano);
  { double r = numRaiz(ini, fim, "CommunityRating", 0);
    if (r > 0 && r <= 10) it->nota = (int)(r * 10.0 + 0.5); }
  if (blocoRaiz(ini, fim, "Genres", &bi, &bf)) {
    const char *p = bi + 1;
    size_t k = 0;
    while ((p = elemento(p, bf)) != NULL) {
      char g[64];
      if (*p == '"' && js_cadeia(p, g, sizeof g) && k + strlen(g) + 4 < sizeof it->genero) {
        k += (size_t)snprintf(it->genero + k, sizeof it->genero - k, "%s%s", k ? " · " : "", g);
      }
      p = depoisElemento(p);
    }
  }
  tag[0] = 0;
  if (blocoRaiz(ini, fim, "ImageTags", &bi, &bf)) {
    if (txtRaiz(bi, bf, "Primary", tag, sizeof tag))
      imagem(c, limpo, "Primary", tag, "maxHeight=600", it->poster, sizeof it->poster);
    tag[0] = 0;
    if (txtRaiz(bi, bf, "Logo", tag, sizeof tag))
      imagem(c, limpo, "Logo", tag, "maxWidth=600", it->logo, sizeof it->logo);
  }
  if (blocoRaiz(ini, fim, "BackdropImageTags", &bi, &bf)) {
    const char *p = elemento(bi + 1, bf);
    tag[0] = 0;
    if (p && *p == '"' && js_cadeia(p, tag, sizeof tag)) {
      imagem(c, limpo, "Backdrop", tag, "maxWidth=1280", it->backdrop, sizeof it->backdrop);
      copiar(it->backdropCatalogo, sizeof it->backdropCatalogo, it->backdrop);
    }
  }
  if (blocoRaiz(ini, fim, "UserData", &bi, &bf)) {
    double pct = numRaiz(bi, bf, "PlayedPercentage", 0);
    double pos = numRaiz(bi, bf, "PlaybackPositionTicks", 0);
    if (pct > 0 && pct < 100) it->progresso = (int)pct;
    if (pos > 0 && ticks > pos) it->restanteMin = (int)(jf_seg((long long)(ticks - pos)) / 60.0 + 0.5);
  }
  copiar(it->origem, sizeof it->origem, c->tipo == JF_TIPO_EMBY ? "emby" : "jellyfin");
  return 1;
}

// Emby has no /UserViews; its per-user views route is /Users/{id}/Views.
int jf_ler_bibliotecas(const char *corpo, size_t nCorpo, JfBiblioteca *out, int max) {
  const char *ai, *af, *p;
  int n = 0;
  if (blocoRaiz(corpo, corpo + nCorpo, "Items", &ai, &af)) {
    p = ai + 1;
    while (n < max && (p = elemento(p, af)) != NULL) {
      const char *f = depoisElemento(p);
      char id[64] = "", nome[96] = "", col[32] = "", limpo[40];
      if (!f) break;
      if (*p == '{') {
        txtRaiz(p, f, "Id", id, sizeof id);
        txtRaiz(p, f, "Name", nome, sizeof nome);
        txtRaiz(p, f, "CollectionType", col, sizeof col);
        if (jfid_hex_limpo(id, limpo, 32) && (!strcmp(col, "movies") || !strcmp(col, "tvshows"))) {
          copiar(out[n].id, sizeof out[n].id, limpo);
          copiar(out[n].nome, sizeof out[n].nome, nome);
          copiar(out[n].tipo, sizeof out[n].tipo, col[0] == 'm' ? "movie" : "series");
          n++;
        }
      }
      p = f;
    }
  }
  return n;
}

int jf_bibliotecas(const JfConta *c, RedeJob *job, JfBiblioteca *out, int max) {
  RedeResposta r;
  char caminho[160];
  int n, e;
  if (c->tipo == JF_TIPO_EMBY) snprintf(caminho, sizeof caminho, "/Users/%s/Views", c->usuarioId);
  else snprintf(caminho, sizeof caminho, "/UserViews?userId=%s", c->usuarioId);
  e = pedir(c, c->base, job, "GET", caminho, NULL, "libraries", JF_PRAZO_LISTA_MS, &r);
  if (e) return e;
  n = jf_ler_bibliotecas(r.corpo, r.n_corpo, out, max);
  rede_resposta_limpar(&r);
  return n;
}

int jf_ler_itens(const JfConta *c, const char *corpo, size_t nCorpo, CatItem *out, int max,
                 int *total) {
  const char *ai, *af, *p;
  int n = 0;
  if (total) *total = (int)numRaiz(corpo, corpo + nCorpo, "TotalRecordCount", 0);
  if (blocoRaiz(corpo, corpo + nCorpo, "Items", &ai, &af)) {
    p = ai + 1;
    while (n < max && (p = elemento(p, af)) != NULL) {
      const char *f = depoisElemento(p);
      if (!f) break;
      if (*p == '{' && itemDe(c, p, f, &out[n])) n++;
      p = f;
    }
  }
  return n;
}

int jf_itens(const JfConta *c, RedeJob *job, const char *bib, int inicio, int limite,
             CatItem *out, int max, int *total) {
  RedeResposta r;
  char caminho[512], limpo[40], raiz[96];
  int n, e;
  if (total) *total = 0;
  if (!jfid_hex_limpo(bib, limpo, 32) || inicio < 0 || limite <= 0) return JF_ERR_ENTRADA;
  if (limite > 100) limite = 100;
  // Emby takes the user in the path; Jellyfin 10.9+ in the query.
  if (c->tipo == JF_TIPO_EMBY) snprintf(raiz, sizeof raiz, "/Users/%s/Items?", c->usuarioId);
  else snprintf(raiz, sizeof raiz, "/Items?userId=%s&", c->usuarioId);
  snprintf(caminho, sizeof caminho,
           "%sParentId=%s&Recursive=true&IncludeItemTypes=Movie,Series"
           "&SortBy=DateCreated,SortName&SortOrder=Descending&Fields=Overview,Genres"
           "&EnableImageTypes=Primary,Backdrop,Logo&ImageTypeLimit=1&EnableUserData=true"
           "&StartIndex=%d&Limit=%d", raiz, limpo, inicio, limite);
  e = pedir(c, c->base, job, "GET", caminho, NULL, "library items", JF_PRAZO_LISTA_MS, &r);
  if (e) return e;
  n = jf_ler_itens(c, r.corpo, r.n_corpo, out, max, total);
  rede_resposta_limpar(&r);
  return n;
}

int jf_ler_detalhe(const JfConta *c, const char *corpo, size_t nCorpo, CatItem *out) {
  const char *bi, *bf, *p;
  if (!itemDe(c, corpo, corpo + nCorpo, out)) return JF_ERR_FORMATO;
  if (blocoRaiz(corpo, corpo + nCorpo, "People", &bi, &bf)) {
    size_t kd = 0;
    p = bi + 1;
    while ((p = elemento(p, bf)) != NULL) {
      const char *f = depoisElemento(p);
      char tipo[24] = "", nome[64] = "", papel[64] = "", pid[64] = "", tag[64] = "", pl[40];
      if (!f) break;
      if (*p == '{') {
        txtRaiz(p, f, "Type", tipo, sizeof tipo);
        txtRaiz(p, f, "Name", nome, sizeof nome);
        if (!strcmp(tipo, "Actor") && out->nElenco < CAT_ELENCO_MAX && nome[0]) {
          int k = out->nElenco++;
          txtRaiz(p, f, "Role", papel, sizeof papel);
          txtRaiz(p, f, "Id", pid, sizeof pid);
          txtRaiz(p, f, "PrimaryImageTag", tag, sizeof tag);
          copiar(out->elenco[k].nome, sizeof out->elenco[k].nome, nome);
          copiar(out->elenco[k].papel, sizeof out->elenco[k].papel, papel);
          if (jfid_hex_limpo(pid, pl, 32))
            imagem(c, pl, "Primary", tag, "maxHeight=300", out->elenco[k].foto, sizeof out->elenco[k].foto);
        } else if (!strcmp(tipo, "Director") && nome[0] && kd + strlen(nome) + 3 < sizeof out->direcao) {
          kd += (size_t)snprintf(out->direcao + kd, sizeof out->direcao - kd, "%s%s", kd ? ", " : "", nome);
        }
      }
      p = f;
    }
  }
  return JF_OK;
}

int jf_detalhe(const JfConta *c, RedeJob *job, const char *itemId, CatItem *out) {
  RedeResposta r;
  char caminho[200], limpo[40];
  int e;
  if (!jfid_hex_limpo(itemId, limpo, 32)) return JF_ERR_ENTRADA;
  // Emby has no /Items/{id}?userId=; the per-user item route exists on both.
  if (c->tipo == JF_TIPO_EMBY) snprintf(caminho, sizeof caminho, "/Users/%s/Items/%s", c->usuarioId, limpo);
  else snprintf(caminho, sizeof caminho, "/Items/%s?userId=%s", limpo, c->usuarioId);
  e = pedir(c, c->base, job, "GET", caminho, NULL, "item", 0, &r);
  if (e) return e;
  e = jf_ler_detalhe(c, r.corpo, r.n_corpo, out);
  rede_resposta_limpar(&r);
  return e;
}

int jf_ler_episodios(const JfConta *c, const char *corpo, size_t nCorpo, CatEp *out, int max) {
  const char *ai, *af, *p;
  int n = 0;
  if (blocoRaiz(corpo, corpo + nCorpo, "Items", &ai, &af)) {
    p = ai + 1;
    while (n < max && (p = elemento(p, af)) != NULL) {
      const char *f = depoisElemento(p), *ti, *tf;
      char id[64] = "", el[40], data[32] = "", tag[64] = "";
      CatEp *ep = &out[n];
      double t, ix, tk;
      if (!f) break;
      if (*p == '{') {
        memset(ep, 0, sizeof *ep);
        txtRaiz(p, f, "Id", id, sizeof id);
        t = numRaiz(p, f, "ParentIndexNumber", 0);
        ix = numRaiz(p, f, "IndexNumber", 0);
        if (jfid_hex_limpo(id, el, 32) && t >= 0 && ix > 0 && t < 1000 && ix < 100000 &&
            jfid_montar_p(prefixoId(c), ep->vid, sizeof ep->vid, c->servidorId, el)) {
          ep->temporada = (int)t; ep->episodio = (int)ix;
          txtRaiz(p, f, "Name", ep->nome, sizeof ep->nome);
          txtRaiz(p, f, "Overview", ep->sinopse, sizeof ep->sinopse);
          tk = numRaiz(p, f, "RunTimeTicks", 0);
          if (tk > 0) snprintf(ep->duracao, sizeof ep->duracao, "%d min", (int)(jf_seg((long long)tk) / 60.0 + 0.5));
          if (txtRaiz(p, f, "PremiereDate", data, sizeof data) && strlen(data) >= 10)
            snprintf(ep->data, sizeof ep->data, "%.2s/%.2s/%.4s", data + 8, data + 5, data);
          if (blocoRaiz(p, f, "ImageTags", &ti, &tf) && txtRaiz(ti, tf, "Primary", tag, sizeof tag))
            imagem(c, el, "Primary", tag, "maxWidth=640", ep->thumb, sizeof ep->thumb);
          n++;
        }
      }
      p = f;
    }
  }
  return n;
}

int jf_episodios(const JfConta *c, RedeJob *job, const char *serieId, CatEp *out, int max) {
  RedeResposta r;
  char caminho[256], limpo[40];
  int n, e;
  if (!jfid_hex_limpo(serieId, limpo, 32)) return JF_ERR_ENTRADA;
  snprintf(caminho, sizeof caminho,
           "/Shows/%s/Episodes?userId=%s&Fields=Overview&EnableImageTypes=Primary"
           "&ImageTypeLimit=1&EnableUserData=true", limpo, c->usuarioId);
  e = pedir(c, c->base, job, "GET", caminho, NULL, "episodes", JF_PRAZO_LISTA_MS, &r);
  if (e) return e;
  n = jf_ler_episodios(c, r.corpo, r.n_corpo, out, max);
  rede_resposta_limpar(&r);
  return n;
}

// ------------------------------------------------------- device profiles
// What each pipeline is declared to open. CONSERVATIVE AND UNPROVEN on TVs:
// unknown capability is not announced (it would only trade a transcode for a
// black screen). DTS/TrueHD are absent everywhere: LG 2019+ and Samsung 2018+
// sets dropped DTS, and Android passthrough depends on the HDMI sink.
int jf_perfil_dispositivo(JfBackend b, char *json, size_t tam) {
  const char *nome, *cont, *vid, *aud;
  switch (b) {
    case JF_BACKEND_ANDROID: nome = "Nuvio Android TV"; cont = "mp4,m4v,mkv,webm,mov,ts";
      vid = "h264,hevc,vp9"; aud = "aac,mp3,ac3,eac3,opus,flac,vorbis"; break;
    case JF_BACKEND_WEBOS: nome = "Nuvio LG webOS"; cont = "mp4,m4v,mkv,mov,ts";
      vid = "h264,hevc"; aud = "aac,mp3,ac3,eac3"; break;
    case JF_BACKEND_TPK: nome = "Nuvio Samsung Tizen"; cont = "mp4,m4v,mkv,mov,ts";
      vid = "h264,hevc"; aud = "aac,mp3,ac3,eac3"; break;
    case JF_BACKEND_WGT: return 0;   // no strict HTTP: never negotiated
    default: nome = "Nuvio Desktop"; cont = "mp4,m4v"; vid = "h264"; aud = "aac,mp3"; break;
  }
  return snprintf(json, tam,
    "{\"Name\":\"%s\",\"MaxStreamingBitrate\":%d,\"MaxStaticBitrate\":%d,"
    "\"DirectPlayProfiles\":[{\"Type\":\"Video\",\"Container\":\"%s\",\"VideoCodec\":\"%s\",\"AudioCodec\":\"%s\"}],"
    "\"TranscodingProfiles\":[{\"Type\":\"Video\",\"Container\":\"ts\",\"Protocol\":\"hls\",\"Context\":\"Streaming\","
    "\"VideoCodec\":\"h264\",\"AudioCodec\":\"aac,ac3\",\"MaxAudioChannels\":\"6\",\"MinSegments\":1,"
    "\"BreakOnNonKeyFrames\":true}],"
    "\"ContainerProfiles\":[],"
    "\"CodecProfiles\":[{\"Type\":\"Video\",\"Codec\":\"h264\",\"Conditions\":[{\"Condition\":\"LessThanEqual\","
    "\"Property\":\"VideoLevel\",\"Value\":\"51\",\"IsRequired\":false}]}],"
    "\"SubtitleProfiles\":[{\"Format\":\"srt\",\"Method\":\"External\"},{\"Format\":\"subrip\",\"Method\":\"External\"},"
    "{\"Format\":\"vtt\",\"Method\":\"External\"},{\"Format\":\"ass\",\"Method\":\"External\"},"
    "{\"Format\":\"ssa\",\"Method\":\"External\"}]}",
    nome, JF_TAXA_MAX, JF_TAXA_MAX, cont, vid, aud) < (int)tam;
}

static void anexarApiKey(char *url, size_t tam, const char *token) {
  size_t n = strlen(url);
  if (strstr(url, "api_key=") || strstr(url, "ApiKey=")) return;
  snprintf(url + n, tam - n, "%sapi_key=%s", strchr(url, '?') ? "&" : "?", token);
}

static void descreverFonte(const char *msi, const char *msf, Stream *s, int metodo,
                           const char *nomeFonte, const char *container, const char *servidor) {
  const char *p = msi + 1;
  char vcod[16] = "", acod[16] = "", faixa[24] = "";
  int altura = 0, canais = 0;
  while ((p = elemento(p, msf)) != NULL) {
    const char *f = depoisElemento(p);
    char tipo[16] = "";
    if (!f) break;
    if (*p == '{') {
      txtRaiz(p, f, "Type", tipo, sizeof tipo);
      if (!strcmp(tipo, "Video") && !vcod[0]) {
        txtRaiz(p, f, "Codec", vcod, sizeof vcod);
        // Jellyfin 10.9+ says VideoRangeType (HDR10, DOVI...); Emby says VideoRange.
        if (!txtRaiz(p, f, "VideoRangeType", faixa, sizeof faixa)) txtRaiz(p, f, "VideoRange", faixa, sizeof faixa);
        altura = (int)numRaiz(p, f, "Height", 0);
      } else if (!strcmp(tipo, "Audio") && !acod[0]) {
        txtRaiz(p, f, "Codec", acod, sizeof acod);
        canais = (int)numRaiz(p, f, "Channels", 0);
      }
    }
    p = f;
  }
  if (altura >= 2000) s->altura = 2160;
  else if (altura >= 1000) s->altura = 1080;
  else if (altura >= 700) s->altura = 720;
  else s->altura = altura;
  s->dolbyVision = strstr(faixa, "DOVI") != NULL;
  {
    char *u;
    for (u = vcod; *u; u++) if (*u >= 'a' && *u <= 'z') *u = (char)(*u - 32);
    for (u = acod; *u; u++) if (*u >= 'a' && *u <= 'z') *u = (char)(*u - 32);
  }
  {
    const char *m = metodo == JF_METODO_DIRETO ? "Direct play" :
                    metodo == JF_METODO_STREAM ? "Direct stream" : "Transcode (HLS)";
    if (s->altura) snprintf(s->rotulo, sizeof s->rotulo, "%dp · %s", s->altura, m);
    else copiar(s->rotulo, sizeof s->rotulo, m);
  }
  {
    // Second line of the card: version name, codecs, range, audio, container.
    // The media PATH is never used (it is a private filesystem path).
    char up[16] = "";
    size_t i, n;
    int hdr = faixa[0] && strcmp(faixa, "SDR");
    if (container && metodo == JF_METODO_DIRETO) {
      for (i = 0; container[i] && container[i] != ',' && i + 1 < sizeof up; i++)
        up[i] = (char)(container[i] >= 'a' && container[i] <= 'z' ? container[i] - 32 : container[i]);
      up[i] = 0;
    }
    n = (size_t)snprintf(s->descricao, sizeof s->descricao, "%s\n%s",
                         nomeFonte && *nomeFonte ? nomeFonte : servidor, vcod);
    if (n < sizeof s->descricao && hdr) n += (size_t)snprintf(s->descricao + n, sizeof s->descricao - n, " · %s", faixa);
    if (n < sizeof s->descricao && acod[0])
      n += (size_t)snprintf(s->descricao + n, sizeof s->descricao - n, " · %s%s", acod,
                            canais >= 8 ? " 7.1" : canais > 2 ? " 5.1" : "");
    if (n < sizeof s->descricao && up[0]) snprintf(s->descricao + n, sizeof s->descricao - n, " · %s", up);
  }
}

// MediaSource ids: Jellyfin uses 32 hex; Emby uses "mediasource_<n>" (or hex
// for extra versions). Both are URL-safe tokens; anything else is refused
// because the id is pasted into stream URLs and check-in bodies.
static int fonteIdValido(const char *id) {
  size_t n = 0;
  for (; id && *id; id++, n++) {
    char c = *id;
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c == '-'))
      return 0;
  }
  return n > 0 && n < 64;
}

// Parses a PlaybackInfo answer. `itemLimpo` is the (hex) item id asked for.
int jf_ler_playbackinfo(const JfConta *c, const char *itemLimpo, const char *corpo, size_t nCorpo,
                        JfPlayback *out) {
  char sessao[64] = "";
  const char *ai, *af, *p, *pre = jf_prefixo(c), *nomeSrv = jf_tipo_nome(c->tipo);
  memset(out, 0, sizeof *out);
  txtRaiz(corpo, corpo + nCorpo, "PlaySessionId", sessao, sizeof sessao);
  if (!blocoRaiz(corpo, corpo + nCorpo, "MediaSources", &ai, &af)) return JF_ERR_FORMATO;
  p = ai + 1;
  while (out->n < JF_FONTES_MAX && (p = elemento(p, af)) != NULL) {
    const char *f = depoisElemento(p), *msi, *msf;
    char fid[64] = "", cont[32] = "", nomeF[160] = "", tr[1536] = "";
    int direto, stream, transc, k;
    if (!f) break;
    if (*p != '{') { p = f; continue; }
    txtRaiz(p, f, "Id", fid, sizeof fid);
    txtRaiz(p, f, "Container", cont, sizeof cont);
    txtRaiz(p, f, "Name", nomeF, sizeof nomeF);
    direto = boolRaiz(p, f, "SupportsDirectPlay", 0);
    stream = boolRaiz(p, f, "SupportsDirectStream", 0);
    transc = txtRaiz(p, f, "TranscodingUrl", tr, sizeof tr) && tr[0] == '/';
    if (!fonteIdValido(fid)) { p = f; continue; }
    for (k = 0; k < 2 && out->n < JF_FONTES_MAX; k++) {
      JfSessaoPlay *sp = &out->sessao[out->n];
      Stream *s = &out->fonte[out->n];
      int metodo;
      if (k == 0 && !direto) continue;
      if (k == 1 && !transc) continue;
      metodo = k == 0 ? JF_METODO_DIRETO : (stream ? JF_METODO_STREAM : JF_METODO_TRANSCODE);
      memset(sp, 0, sizeof *sp);
      memset(s, 0, sizeof *s);
      copiar(sp->itemId, sizeof sp->itemId, itemLimpo);
      copiar(sp->fonteId, sizeof sp->fonteId, fid);
      copiar(sp->sessaoId, sizeof sp->sessaoId, sessao);
      sp->metodo = metodo;
      if (metodo == JF_METODO_DIRETO)
        snprintf(sp->url, sizeof sp->url,
                 "%s%s/Videos/%s/stream?static=true&MediaSourceId=%s&PlaySessionId=%s&DeviceId=%s",
                 c->base, pre, itemLimpo, fid, sessao, c->dispositivoId);
      else if (c->tipo == JF_TIPO_EMBY && strncmp(tr, "/emby/", 6))
        // Emby's TranscodingUrl is relative to the /emby API root.
        snprintf(sp->url, sizeof sp->url, "%s/emby%s", c->base, tr);
      else
        snprintf(sp->url, sizeof sp->url, "%s%s", c->base, tr);
      anexarApiKey(sp->url, sizeof sp->url, c->token);
      copiar(s->url, sizeof s->url, sp->url);
      copiar(s->provedor, sizeof s->provedor, nomeSrv);
      s->fileIdx = -1;
      s->mp4 = metodo == JF_METODO_DIRETO && (!strcmp(cont, "mp4") || !strcmp(cont, "m4v"));
      s->tamanhoBytes = metodo == JF_METODO_DIRETO ? (uint64_t)numRaiz(p, f, "Size", 0) : 0;
      s->tamanhoMB = (long)(s->tamanhoBytes / (1024u * 1024u));
      snprintf(s->bingeGroup, sizeof s->bingeGroup, "%s|%s", c->tipo == JF_TIPO_EMBY ? "emby" : "jellyfin",
               jellyfin_metodo_nome(metodo));
      if (blocoRaiz(p, f, "MediaStreams", &msi, &msf))
        descreverFonte(msi, msf, s, metodo, nomeF, cont, nomeSrv);
      else
        copiar(s->rotulo, sizeof s->rotulo, metodo == JF_METODO_DIRETO ? "Direct play" : "Transcode (HLS)");
      out->n++;
    }
    p = f;
  }
  return JF_OK;
}

int jf_playbackinfo(const JfConta *c, RedeJob *job, const char *itemId, JfBackend b, JfPlayback *out) {
  RedeResposta r;
  char caminho[200], limpo[40], perfil[2048];
  Jsw w;
  int e;
  memset(out, 0, sizeof *out);
  if (!jfid_hex_limpo(itemId, limpo, 32)) return JF_ERR_ENTRADA;
  if (!jf_perfil_dispositivo(b, perfil, sizeof perfil)) return JF_ERR_INDISPONIVEL;
  jsw_iniciar(&w);
  jsw_obj_ini(&w);
  jsw_cs(&w, "UserId", c->usuarioId);
  jsw_chave(&w, "DeviceProfile"); jsw_bruto(&w, perfil);
  jsw_ci(&w, "MaxStreamingBitrate", JF_TAXA_MAX);
  // ALWAYS FROM 0. A transcode started at StartTimeTicks restarts the
  // timeline at the offset, and the player's own resume seek would apply the
  // offset twice. Seeking inside a full HLS/direct timeline keeps one clock.
  jsw_ci(&w, "StartTimeTicks", 0);
  jsw_cb(&w, "EnableDirectPlay", 1);
  jsw_cb(&w, "EnableDirectStream", 1);
  jsw_cb(&w, "EnableTranscoding", 1);
  jsw_cb(&w, "AllowVideoStreamCopy", 1);
  jsw_cb(&w, "AllowAudioStreamCopy", 1);
  jsw_cb(&w, "AutoOpenLiveStream", 0);
  jsw_obj_fim(&w);
  snprintf(caminho, sizeof caminho, "/Items/%s/PlaybackInfo?userId=%s", limpo, c->usuarioId);
  e = jsw_texto_final(&w)
        ? pedir(c, c->base, job, "POST", caminho, jsw_texto_final(&w), "playback info", 0, &r)
        : JF_ERR_ENTRADA;
  jsw_livre(&w);
  if (e) return e;
  e = jf_ler_playbackinfo(c, limpo, r.corpo, r.n_corpo, out);
  apagarSegredo(r.corpo, r.n_corpo);   // TranscodingUrl may embed the token
  rede_resposta_limpar(&r);
  return e;
}

int jf_reportar(const JfConta *c, RedeJob *job, int evento, const JfSessaoPlay *s, double posSeg) {
  RedeResposta r;
  Jsw w;
  const char *caminho, *op;
  int e;
  jsw_iniciar(&w);
  jsw_obj_ini(&w);
  jsw_cs(&w, "ItemId", s->itemId);
  jsw_cs(&w, "MediaSourceId", s->fonteId);
  if (s->sessaoId[0]) jsw_cs(&w, "PlaySessionId", s->sessaoId);
  jsw_ci(&w, "PositionTicks", jf_ticks(posSeg));
  if (evento != JF_REL_FIM) {
    jsw_cb(&w, "IsPaused", evento == JF_REL_PAUSA);
    jsw_cb(&w, "CanSeek", 1);
    jsw_cs(&w, "PlayMethod", jellyfin_metodo_nome(s->metodo));
    if (evento == JF_REL_PAUSA) jsw_cs(&w, "EventName", "pause");
    else if (evento == JF_REL_RETOMA) jsw_cs(&w, "EventName", "unpause");
    else if (evento == JF_REL_PROGRESSO) jsw_cs(&w, "EventName", "timeupdate");
  }
  jsw_obj_fim(&w);
  switch (evento) {
    case JF_REL_INICIO: caminho = "/Sessions/Playing"; op = "playback start"; break;
    case JF_REL_FIM: caminho = "/Sessions/Playing/Stopped"; op = "playback stopped"; break;
    default: caminho = "/Sessions/Playing/Progress"; op = "playback progress"; break;
  }
  e = jsw_texto_final(&w) ? pedir(c, c->base, job, "POST", caminho, jsw_texto_final(&w), op, 8000, &r)
                          : JF_ERR_ENTRADA;
  jsw_livre(&w);
  if (!e) rede_resposta_limpar(&r);
  // END OF SESSION: Stopped already ends it; the explicit release frees a
  // transcoder even if the server missed the stop (Jellyfin keeps encodes
  // alive while the session looks open).
  if (!e && evento == JF_REL_FIM && s->metodo != JF_METODO_DIRETO && s->sessaoId[0]) {
    char cam[300];
    snprintf(cam, sizeof cam, "/Videos/ActiveEncodings?deviceId=%s&playSessionId=%s",
             c->dispositivoId, s->sessaoId);
    if (!pedir(c, c->base, job, "DELETE", cam, NULL, "release transcode", 5000, &r))
      rede_resposta_limpar(&r);
  }
  return e;
}

// ======================================================= app integration
typedef struct {
  int tipo;
  unsigned geracao;
  unsigned entrada;   // cancelEntrada when queued: a cancel BEFORE the worker starts must still win
  char a[600];
  char b[288];
} Tarefa;
enum { T_VERIFICAR = 1, T_QC, T_SENHA, T_BIBLIOTECAS, T_FONTES, T_SAIR };

typedef struct {
  int evento;
  unsigned geracao;
  double pos;
  JfSessaoPlay s;
} Relato;

typedef struct {
  int ativo, iniciado, pausado;
  double ultPos;
  unsigned geracao;
  unsigned long long ultimoMs, criadoMs;
  JfSessaoPlay s;
} Sessao;


// One integration instance per server kind. Instance 0 is Jellyfin
// (jellyfin_* API), instance 1 is Emby (emby_* API); each owns its token file,
// workers, generation, snapshot and check-in sessions, so signing out of one
// never disturbs the other.
typedef struct JfInst {
  int tipo;                       // JF_TIPO_*
  const char *arqFmt;             // "jellyfin-p%d.txt" / "emby-p%d.txt"
  const char *log;                // "jellyfin" / "emby"
  const char *prefixoFila;        // Home row key prefix: "jellyfin_" / "emby_"
  const char *prefixoId;          // "jf:" / "em:"
  pthread_mutex_t trava;
  pthread_cond_t sinalCtl, sinalRel;
  int iniciou;
  pthread_t fioCtl, fioRel;
  int fiosVivos, parar;
  RedeGrupo *grupo;
  unsigned geracao;
  RedeJob *jobCtl;                // control job in flight (cancel on demand)
  unsigned cancelEntrada;         // bumps on "cancel sign-in"
  Tarefa filaCtl[JF_FILA_CTL];
  int iniCtl, nCtl;
  Relato filaRel[JF_FILA_REL];
  int iniRel, nRel, relEmVoo;
  JfConta conta;
  int perfilLido;
  JfEstado estado;
  char detalhe[160];
  int qcPermitido;
  CatItem *snapItens;
  CatFileira snapFils[JF_FIL_MAX];
  int nSnapFils, nSnapItens;
  unsigned snapVersao;
  char fontesAlvo[JFID_MAX];
  int fontesEstado;
  Stream *fontesLista;
  int nFontes;
  Sessao sessoes[JF_SESSOES];
  int ultimoErro;
  char curto[160], nomeUsuario[96];
} JfInst;

static JfInst instJf = { .tipo = JF_TIPO_JELLYFIN, .arqFmt = "jellyfin-p%d.txt", .log = "jellyfin",
  .prefixoFila = "jellyfin_", .prefixoId = JFID_PREFIXO, .trava = PTHREAD_MUTEX_INITIALIZER,
  .sinalCtl = PTHREAD_COND_INITIALIZER, .sinalRel = PTHREAD_COND_INITIALIZER, .geracao = 1,
  .perfilLido = -1, .estado = JF_EST_SEM_SERVIDOR, .qcPermitido = -1, .fontesEstado = JF_FONTES_NADA };
static JfInst instEm = { .tipo = JF_TIPO_EMBY, .arqFmt = "emby-p%d.txt", .log = "emby",
  .prefixoFila = "emby_", .prefixoId = JFID_PREFIXO_EMBY, .trava = PTHREAD_MUTEX_INITIALIZER,
  .sinalCtl = PTHREAD_COND_INITIALIZER, .sinalRel = PTHREAD_COND_INITIALIZER, .geracao = 1,
  .perfilLido = -1, .estado = JF_EST_SEM_SERVIDOR, .qcPermitido = -1, .fontesEstado = JF_FONTES_NADA };
static pthread_mutex_t travaInicio = PTHREAD_MUTEX_INITIALIZER;

// Forward declarations of the per-instance implementations.
static void jfi_carregar(JfInst *I);
static void jfi_perfil_trocou(JfInst *I);
static void jfi_esquecer(JfInst *I);
static void jfi_esquecer_todos(JfInst *I);
static void jfi_encerrar(JfInst *I);
static JfEstado jfi_estado(JfInst *I, char *det, size_t tam);
static int jfi_conectado(JfInst *I);
static int jfi_qc_permitido(JfInst *I);
static int jfi_ultimo_erro(JfInst *I);
static int jfi_definir_servidor(JfInst *I, const char *url);
static int jfi_entrar_quick_connect(JfInst *I);
static int jfi_entrar_senha(JfInst *I, const char *usuario, char *senha);
static void jfi_cancelar_entrada(JfInst *I);
static void jfi_recarregar_bibliotecas(JfInst *I);
static unsigned jfi_fileiras_versao(JfInst *I);
static int jfi_chave_fileira(JfInst *I, const char *chave);
static int jfi_fileiras_copiar(JfInst *I, CatItem *itens, int maxItens, CatFileira *fils, int maxFils, int *nItens);
static int jfi_relatorios_pendentes(JfInst *I);
static void jfi_reproducao_tick(JfInst *I, const char *url, double pos, double dur, int tocando);
static void jfi_reproducao_fim(JfInst *I, const char *url, double pos, double dur);
static int jfi_fontes_pedir(JfInst *I, const char *alvo);
static int jfi_fontes_colher(JfInst *I, const char *alvo, Stream **lista, int *n);
static int jfi_ficha(JfInst *I, CatItem *item, CatEp *eps, int maxEps);


int jellyfin_disponivel(void) {
  return jellyfin_backend() != JF_BACKEND_WGT && (rede_pedido_capacidades() & REDE_CAP_JOB);
}

static void *trabalharCtl(void *u);
static void *trabalharRel(void *u);

static void iniciar(JfInst *I) {
  I->grupo = rede_grupo_criar();
  if (pthread_create(&I->fioCtl, NULL, trabalharCtl, I) == 0) {
    if (pthread_create(&I->fioRel, NULL, trabalharRel, I) == 0) I->fiosVivos = 2;
    else I->fiosVivos = 1;
  }
}
// First use of an instance starts its two workers; later calls are a lock and
// a flag (no pthread_once: there is one start per instance).
static void garantir(JfInst *I) {
  pthread_mutex_lock(&travaInicio);
  if (!I->iniciou) { I->iniciou = 1; iniciar(I); }
  pthread_mutex_unlock(&travaInicio);
}

// -------------------------------------------------------------- storage
static void arquivo(JfInst *I, char *dst, size_t tam, int perfil) { snprintf(dst, tam, I->arqFmt, perfil); }

static void linha(char *dst, size_t tam, size_t *k, const char *chave, const char *v) {
  char limpo[600];
  size_t i = 0;
  for (; v && *v && i + 1 < sizeof limpo; v++) limpo[i++] = (*v == '\n' || *v == '\r') ? ' ' : *v;
  limpo[i] = 0;
  if (*k < tam) *k += (size_t)snprintf(dst + *k, tam - *k, "%s=%s\n", chave, limpo);
  apagarSegredo(limpo, sizeof limpo);
}

// 0600 and atomic rename: the token file is never world-readable on POSIX
// backends (dados_gravar uses fopen's default mode).
static int gravarConta(JfInst *I, const JfConta *c, int perfil) {
  char nome[40], caminho[640], tmp[660], txt[2048];
  size_t k = 0;
  int fd, ok = 0;
  arquivo(I, nome, sizeof nome, perfil);
  if (!dados_caminho(caminho, sizeof caminho, nome)) return 0;
  linha(txt, sizeof txt, &k, "base", c->base);
  linha(txt, sizeof txt, &k, "sid", c->servidorId);
  linha(txt, sizeof txt, &k, "snome", c->servidorNome);
  linha(txt, sizeof txt, &k, "ver", c->versao);
  linha(txt, sizeof txt, &k, "uid", c->usuarioId);
  linha(txt, sizeof txt, &k, "unome", c->usuarioNome);
  linha(txt, sizeof txt, &k, "dev", c->dispositivoId);
  linha(txt, sizeof txt, &k, "token", c->token);
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
  int fd = open("/dev/urandom", O_RDONLY), ok = 0;
  if (fd >= 0) { ok = read(fd, b, sizeof b) == (ssize_t)sizeof b; close(fd); }
  if (!ok) {
    unsigned long long x = agoraMs() ^ ((unsigned long long)getpid() << 20) ^ (unsigned long long)time(NULL);
    int i;
    for (i = 0; i < 8; i++) { b[i] = (unsigned char)(x >> (i * 8)); }
  }
  snprintf(dst, tam, "nuvio-%02x%02x%02x%02x%02x%02x%02x%02x-p%d",
           b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7], perfis_ativo());
}

// Under I->trava. Reads the active profile's file into `I->conta`.
static void carregarLocked(JfInst *I) {
  char nome[40], *txt;
  int p = perfis_ativo();
  I->perfilLido = p;
  memset(&I->conta, 0, sizeof I->conta);
  I->conta.tipo = I->tipo;
  I->qcPermitido = -1;
  arquivo(I, nome, sizeof nome, p);
  txt = dados_ler(nome);
  if (txt) {
    lerCampo(txt, "base", I->conta.base, sizeof I->conta.base);
    lerCampo(txt, "sid", I->conta.servidorId, sizeof I->conta.servidorId);
    lerCampo(txt, "snome", I->conta.servidorNome, sizeof I->conta.servidorNome);
    lerCampo(txt, "ver", I->conta.versao, sizeof I->conta.versao);
    lerCampo(txt, "uid", I->conta.usuarioId, sizeof I->conta.usuarioId);
    lerCampo(txt, "unome", I->conta.usuarioNome, sizeof I->conta.usuarioNome);
    lerCampo(txt, "dev", I->conta.dispositivoId, sizeof I->conta.dispositivoId);
    lerCampo(txt, "token", I->conta.token, sizeof I->conta.token);
    apagarSegredo(txt, strlen(txt));
    free(txt);
  }
  if (!I->conta.dispositivoId[0]) novoDispositivo(I->conta.dispositivoId, sizeof I->conta.dispositivoId);
  copiar(I->conta.dispositivoNome, sizeof I->conta.dispositivoNome, jellyfin_backend_nome(jellyfin_backend()));
  if (I->conta.token[0] && I->conta.usuarioId[0] && I->conta.servidorId[0]) I->estado = JF_EST_CONECTADO;
  else if (I->conta.base[0] && I->conta.servidorId[0]) I->estado = JF_EST_SERVIDOR_OK;
  else I->estado = JF_EST_SEM_SERVIDOR;
  I->detalhe[0] = 0;
}

// Under I->trava. Drops everything bound to the previous identity.
static void soltarSnapshotLocked(JfInst *I) {
  int i;
  I->nSnapFils = I->nSnapItens = 0;
  I->snapVersao++;
  free(I->fontesLista); I->fontesLista = NULL; I->nFontes = 0;
  I->fontesAlvo[0] = 0; I->fontesEstado = JF_FONTES_NADA;
  for (i = 0; i < JF_SESSOES; i++) {
    apagarSegredo(I->sessoes[i].s.url, sizeof I->sessoes[i].s.url);
    I->sessoes[i].ativo = 0;
  }
  // Queued check-ins carry the previous token: drop them with the identity.
  for (i = 0; i < JF_FILA_REL; i++) apagarSegredo(&I->filaRel[i], sizeof I->filaRel[i]);
  I->nRel = 0;
}

// Under I->trava: new generation, every in-flight request of the old one is
// cancelled at its next progress callback and its result fenced out.
static void avancarLocked(JfInst *I) {
  I->geracao++;
  if (I->grupo) rede_grupo_avancar(I->grupo);
  if (I->jobCtl) rede_job_cancelar(I->jobCtl);
  I->nCtl = 0;
  soltarSnapshotLocked(I);
}

static void conferirPerfilLocked(JfInst *I) {
  if (I->perfilLido != perfis_ativo()) { avancarLocked(I); carregarLocked(I); }
}

static void jfi_carregar(JfInst *I) {
  garantir(I);
  pthread_mutex_lock(&I->trava);
  carregarLocked(I);
  pthread_mutex_unlock(&I->trava);
  if (jfi_conectado(I)) jfi_recarregar_bibliotecas(I);
}

static void jfi_perfil_trocou(JfInst *I) {
  int recarregar;
  garantir(I);
  pthread_mutex_lock(&I->trava);
  avancarLocked(I);
  carregarLocked(I);
  recarregar = I->estado == JF_EST_CONECTADO;
  pthread_mutex_unlock(&I->trava);
  printf("[%s] profile changed: in-flight requests dropped\n", I->log);
  if (recarregar) jfi_recarregar_bibliotecas(I);
}

static int enfileirarLocked(JfInst *I, int tipo, const char *a, const char *b) {
  Tarefa *t;
  if (!I->fiosVivos || I->nCtl >= JF_FILA_CTL) return 0;
  t = &I->filaCtl[(I->iniCtl + I->nCtl) % JF_FILA_CTL];
  memset(t, 0, sizeof *t);
  t->tipo = tipo;
  t->geracao = I->geracao;
  t->entrada = I->cancelEntrada;
  copiar(t->a, sizeof t->a, a);
  copiar(t->b, sizeof t->b, b);
  I->nCtl++;
  pthread_cond_signal(&I->sinalCtl);
  return 1;
}

static void jfi_esquecer(JfInst *I) {
  char nome[40];
  garantir(I);
  pthread_mutex_lock(&I->trava);
  conferirPerfilLocked(I);
  // Best-effort revoke on the server, with the old token captured by value in
  // the task (base/token in b/a never touch logs). Queued AFTER advancing so
  // the new generation does not cancel it.
  avancarLocked(I);
  if (I->conta.token[0] && I->conta.base[0]) {
    char a[600];
    snprintf(a, sizeof a, "%s\n%s\n%s", I->conta.base, I->conta.token, I->conta.dispositivoId);
    enfileirarLocked(I, T_SAIR, a, "");
    apagarSegredo(a, sizeof a);
  }
  arquivo(I, nome, sizeof nome, perfis_ativo());
  dados_apagar(nome);
  apagarSegredo(&I->conta, sizeof I->conta);
  carregarLocked(I);
  pthread_mutex_unlock(&I->trava);
  printf("[%s] signed out on this profile\n", I->log);
}

static void jfi_esquecer_todos(JfInst *I) {
  char nome[40];
  int i;
  garantir(I);
  pthread_mutex_lock(&I->trava);
  avancarLocked(I);
  for (i = 1; i <= 32; i++) { arquivo(I, nome, sizeof nome, i); dados_apagar(nome); }
  apagarSegredo(&I->conta, sizeof I->conta);
  carregarLocked(I);
  pthread_mutex_unlock(&I->trava);
}

static void jfi_encerrar(JfInst *I) {
  int vivos;
  pthread_mutex_lock(&I->trava);
  I->parar = 1;
  avancarLocked(I);
  vivos = I->fiosVivos;
  pthread_cond_broadcast(&I->sinalCtl);
  pthread_cond_broadcast(&I->sinalRel);
  pthread_mutex_unlock(&I->trava);
  if (vivos >= 1) pthread_join(I->fioCtl, NULL);
  if (vivos >= 2) pthread_join(I->fioRel, NULL);
  pthread_mutex_lock(&I->trava);
  I->fiosVivos = 0;
  free(I->snapItens); I->snapItens = NULL;
  free(I->fontesLista); I->fontesLista = NULL;
  if (I->grupo) { rede_grupo_cancelar(I->grupo); rede_grupo_soltar(I->grupo); I->grupo = NULL; }
  apagarSegredo(&I->conta, sizeof I->conta);
  pthread_mutex_unlock(&I->trava);
}

// ----------------------------------------------------------- UI snapshot
static JfEstado jfi_estado(JfInst *I, char *det, size_t tam) {
  JfEstado e;
  garantir(I);
  pthread_mutex_lock(&I->trava);
  conferirPerfilLocked(I);
  e = I->estado;
  if (det && tam) copiar(det, tam, I->detalhe);
  pthread_mutex_unlock(&I->trava);
  return e;
}

const char *jfi_servidor_curto(JfInst *I) {
  const char *b;
  pthread_mutex_lock(&I->trava);
  b = I->conta.base;
  if (!strncmp(b, "https://", 8)) b += 8;
  else if (!strncmp(b, "http://", 7)) b += 7;
  copiar(I->curto, sizeof I->curto, b);
  pthread_mutex_unlock(&I->trava);
  return I->curto;
}
const char *jfi_usuario(JfInst *I) {
  pthread_mutex_lock(&I->trava);
  copiar(I->nomeUsuario, sizeof I->nomeUsuario, I->estado == JF_EST_CONECTADO ? I->conta.usuarioNome : "");
  pthread_mutex_unlock(&I->trava);
  return I->nomeUsuario;
}
static int jfi_conectado(JfInst *I) {
  int c;
  pthread_mutex_lock(&I->trava);
  c = I->estado == JF_EST_CONECTADO && I->conta.token[0];
  pthread_mutex_unlock(&I->trava);
  return c;
}
static int jfi_qc_permitido(JfInst *I) {
  int q;
  pthread_mutex_lock(&I->trava);
  q = I->qcPermitido;
  pthread_mutex_unlock(&I->trava);
  return q;
}

static const char *textoErro(JfInst *I, int e);
static void definirEstadoLocked(JfInst *I, JfEstado e, const char *det) {
  I->estado = e;
  copiar(I->detalhe, sizeof I->detalhe, det);
}
// Failure for the Settings row: the UI translates the code; the log gets the
// English reason (no secret, no address).
static void definirErroLocked(JfInst *I, int e) {
  I->estado = JF_EST_ERRO;
  I->ultimoErro = e;
  I->detalhe[0] = 0;
  printf("[%s] action failed: %s\n", I->log, textoErro(I, e));
}
static int jfi_ultimo_erro(JfInst *I) {
  int e;
  pthread_mutex_lock(&I->trava);
  e = I->ultimoErro;
  pthread_mutex_unlock(&I->trava);
  return e;
}

static int jfi_definir_servidor(JfInst *I, const char *url) {
  char norm[512];
  int ok;
  garantir(I);
  if (!jellyfin_disponivel()) return 0;
  if (jf_url_normalizar_tipo(I->tipo, url, norm, sizeof norm) != JF_OK) {
    pthread_mutex_lock(&I->trava);
    definirErroLocked(I, JF_ERR_ENTRADA);
    pthread_mutex_unlock(&I->trava);
    return 0;
  }
  pthread_mutex_lock(&I->trava);
  conferirPerfilLocked(I);
  // A NEW SERVER IS A NEW IDENTITY. The old token, rows and sessions belong
  // to the previous server and are dropped before the check even starts.
  avancarLocked(I);
  if (strcmp(norm, I->conta.base)) {
    char dev[80];
    copiar(dev, sizeof dev, I->conta.dispositivoId);
    apagarSegredo(&I->conta, sizeof I->conta);
    I->conta.tipo = I->tipo;
    copiar(I->conta.dispositivoId, sizeof I->conta.dispositivoId, dev);
    copiar(I->conta.dispositivoNome, sizeof I->conta.dispositivoNome, jellyfin_backend_nome(jellyfin_backend()));
  }
  definirEstadoLocked(I, JF_EST_VERIFICANDO, "");
  ok = enfileirarLocked(I, T_VERIFICAR, norm, "");
  pthread_mutex_unlock(&I->trava);
  return ok;
}

static int jfi_entrar_quick_connect(JfInst *I) {
  int ok = 0;
  garantir(I);
  pthread_mutex_lock(&I->trava);
  conferirPerfilLocked(I);
  if (I->tipo == JF_TIPO_JELLYFIN && I->conta.base[0] && I->conta.servidorId[0] &&
      I->estado != JF_EST_QC_CODIGO && I->estado != JF_EST_ENTRANDO) {
    I->cancelEntrada++;
    definirEstadoLocked(I, JF_EST_ENTRANDO, "");
    ok = enfileirarLocked(I, T_QC, "", "");
  }
  pthread_mutex_unlock(&I->trava);
  return ok;
}

static int jfi_entrar_senha(JfInst *I, const char *usuario, char *senha) {
  int ok = 0;
  garantir(I);
  pthread_mutex_lock(&I->trava);
  conferirPerfilLocked(I);
  if (I->conta.base[0] && I->conta.servidorId[0] && usuario && *usuario && senha &&
      strlen(usuario) < 128 && strlen(senha) < 256 && I->estado != JF_EST_ENTRANDO) {
    I->cancelEntrada++;
    definirEstadoLocked(I, JF_EST_ENTRANDO, "");
    ok = enfileirarLocked(I, T_SENHA, usuario, senha);
    // The queue slot holds the password until the worker wipes it.
  }
  pthread_mutex_unlock(&I->trava);
  if (senha) apagarSegredo(senha, strlen(senha));
  return ok;
}

static void jfi_cancelar_entrada(JfInst *I) {
  pthread_mutex_lock(&I->trava);
  I->cancelEntrada++;
  if (I->jobCtl) rede_job_cancelar(I->jobCtl);
  if (I->estado == JF_EST_QC_CODIGO || I->estado == JF_EST_ENTRANDO)
    definirEstadoLocked(I, I->conta.servidorId[0] ? JF_EST_SERVIDOR_OK : JF_EST_SEM_SERVIDOR, "");
  pthread_mutex_unlock(&I->trava);
}

static void jfi_recarregar_bibliotecas(JfInst *I) {
  garantir(I);
  pthread_mutex_lock(&I->trava);
  conferirPerfilLocked(I);
  if (I->estado == JF_EST_CONECTADO) enfileirarLocked(I, T_BIBLIOTECAS, "", "");
  pthread_mutex_unlock(&I->trava);
}

static unsigned jfi_fileiras_versao(JfInst *I) {
  unsigned v;
  pthread_mutex_lock(&I->trava);
  conferirPerfilLocked(I);
  v = I->snapVersao;
  pthread_mutex_unlock(&I->trava);
  return v;
}

static int jfi_chave_fileira(JfInst *I, const char *chave) { return chave && !strncmp(chave, I->prefixoFila, strlen(I->prefixoFila)); }

static int jfi_fileiras_copiar(JfInst *I, CatItem *itens, int maxItens, CatFileira *fils, int maxFils, int *nItens) {
  int r, nf = 0, ni = 0;
  if (nItens) *nItens = 0;
  pthread_mutex_lock(&I->trava);
  conferirPerfilLocked(I);
  for (r = 0; r < I->nSnapFils && nf < maxFils; r++) {
    const CatFileira *f = &I->snapFils[r];
    if (ni + f->n > maxItens) break;
    memcpy(itens + ni, I->snapItens + f->ini, sizeof(CatItem) * (size_t)f->n);
    fils[nf] = *f;
    fils[nf].ini = ni;
    ni += f->n;
    nf++;
  }
  pthread_mutex_unlock(&I->trava);
  if (nItens) *nItens = ni;
  return nf;
}

// ------------------------------------------------------ worker: control
// Snapshot of what a task needs, taken under I->trava; the network runs on the
// copy. Returns the job to use (NULL if the generation already moved).
static RedeJob *abrirTarefa(JfInst *I, const Tarefa *t, JfConta *c) {
  RedeJob *j = NULL;
  pthread_mutex_lock(&I->trava);
  if (t->geracao == I->geracao && I->grupo) {
    *c = I->conta;
    j = rede_job_criar(I->grupo);
    I->jobCtl = j;
  }
  pthread_mutex_unlock(&I->trava);
  return j;
}
// 1 when the task may still publish (same generation, job not cancelled).
static int aindaVale(JfInst *I, const Tarefa *t, RedeJob *j) {
  return t->geracao == I->geracao && rede_job_estado(j) == REDE_OK;
}
static void fecharTarefa(JfInst *I, RedeJob *j) {
  pthread_mutex_lock(&I->trava);
  if (I->jobCtl == j) I->jobCtl = NULL;
  pthread_mutex_unlock(&I->trava);
  rede_job_soltar(j);
}

static const char *textoErro(JfInst *I, int e) {
  switch (e) {
    case JF_ERR_REDE: return "server did not answer";
    case JF_ERR_AUTH: return "sign-in refused";
    case JF_ERR_ENTRADA: return "invalid address";
    case JF_ERR_HTTP: return "server error";
    case JF_ERR_FORMATO: return I->tipo == JF_TIPO_EMBY ? "not an Emby server" : "not a Jellyfin server";
    case JF_ERR_INDISPONIVEL: return "unavailable on this platform";
    case JF_ERR_EXPIRADO: return "code expired";
    case JF_ERR_QC_DESLIGADO: return "Quick Connect is off on this server";
    default: return "failed";
  }
}

// Under I->trava, after a 401 on an authenticated call: the token is dead. Keep
// the server so the person only signs in again.
static void expirouLocked(JfInst *I) {
  avancarLocked(I);
  apagarSegredo(I->conta.token, sizeof I->conta.token);
  I->conta.usuarioId[0] = 0;
  gravarConta(I, &I->conta, I->perfilLido);
  definirEstadoLocked(I, JF_EST_EXPIROU, "session expired");
}

static void publicarConta(JfInst *I, const Tarefa *t, RedeJob *j, JfConta *c) {
  int qc = -1;
  pthread_mutex_lock(&I->trava);
  if (aindaVale(I, t, j)) {
    copiar(c->dispositivoNome, sizeof c->dispositivoNome, I->conta.dispositivoNome);
    I->conta = *c;
    gravarConta(I, &I->conta, I->perfilLido);
    definirEstadoLocked(I, JF_EST_CONECTADO, I->conta.servidorNome);
    qc = enfileirarLocked(I, T_BIBLIOTECAS, "", "");
  }
  pthread_mutex_unlock(&I->trava);
  (void)qc;
}

static void tarefaVerificar(JfInst *I, const Tarefa *t) {
  JfConta c;
  RedeJob *j = abrirTarefa(I, t, &c);
  int e, qc;
  if (!j) return;
  e = jf_publico(t->a, j, &c);
  qc = e == JF_OK ? jf_qc_habilitado(&c, j) : 0;
  pthread_mutex_lock(&I->trava);
  if (aindaVale(I, t, j)) {
    if (e == JF_OK) {
      char det[160];
      I->conta = c;
      I->qcPermitido = qc > 0;
      gravarConta(I, &I->conta, I->perfilLido);
      snprintf(det, sizeof det, "%s · %s", c.servidorNome[0] ? c.servidorNome : jf_tipo_nome(I->tipo), c.versao);
      definirEstadoLocked(I, JF_EST_SERVIDOR_OK, det);
    } else definirErroLocked(I, e);
  }
  pthread_mutex_unlock(&I->trava);
  fecharTarefa(I, j);
}

static void tarefaSenha(JfInst *I, Tarefa *t) {
  JfConta c;
  RedeJob *j = abrirTarefa(I, t, &c);
  int e;
  if (!j) { apagarSegredo(t->b, sizeof t->b); return; }
  e = jf_autenticar_senha(&c, j, t->a, t->b);   // wipes t->b
  apagarSegredo(t->b, sizeof t->b);
  if (e == JF_OK) publicarConta(I, t, j, &c);
  else {
    pthread_mutex_lock(&I->trava);
    if (aindaVale(I, t, j)) definirErroLocked(I, e);
    pthread_mutex_unlock(&I->trava);
  }
  apagarSegredo(&c, sizeof c);
  fecharTarefa(I, j);
}

static void tarefaQc(JfInst *I, const Tarefa *t) {
  JfConta c;
  RedeJob *j = abrirTarefa(I, t, &c);
  char segredo[160], codigo[16];
  unsigned meu, inicio;
  int e;
  if (!j) return;
  meu = t->entrada;
  e = jf_qc_iniciar(&c, j, segredo, sizeof segredo, codigo, sizeof codigo);
  if (e) {
    pthread_mutex_lock(&I->trava);
    if (aindaVale(I, t, j)) {
      if (e == JF_ERR_QC_DESLIGADO) I->qcPermitido = 0;
      definirErroLocked(I, e);
    }
    pthread_mutex_unlock(&I->trava);
    fecharTarefa(I, j);
    return;
  }
  pthread_mutex_lock(&I->trava);
  if (aindaVale(I, t, j) && meu == I->cancelEntrada) definirEstadoLocked(I, JF_EST_QC_CODIGO, codigo);
  pthread_mutex_unlock(&I->trava);
  inicio = (unsigned)agoraMs();
  // POLLING ENDS on approval, expiry (404), cancel, profile switch or the
  // 5-minute ceiling: no Quick Connect poll survives leaving the screen.
  for (;;) {
    unsigned k;
    int vivo;
    for (k = 0; k < JF_QC_INTERVALO_MS / 100u; k++) {
      struct timespec d = { 0, 100 * 1000000L };
      pthread_mutex_lock(&I->trava);
      vivo = aindaVale(I, t, j) && meu == I->cancelEntrada && !I->parar;
      pthread_mutex_unlock(&I->trava);
      if (!vivo) goto fim;
      nanosleep(&d, NULL);
    }
    if ((unsigned)agoraMs() - inicio > JF_QC_TETO_MS) { e = JF_ERR_EXPIRADO; break; }
    e = jf_qc_conferir(&c, j, segredo);
    if (e == 1) { e = jf_qc_autenticar(&c, j, segredo); break; }
    if (e < 0) break;
  }
  if (e == JF_OK) publicarConta(I, t, j, &c);
  else {
    pthread_mutex_lock(&I->trava);
    if (aindaVale(I, t, j) && meu == I->cancelEntrada) definirErroLocked(I, e);
    pthread_mutex_unlock(&I->trava);
  }
fim:
  apagarSegredo(segredo, sizeof segredo);
  apagarSegredo(&c, sizeof c);
  fecharTarefa(I, j);
}

static void tarefaBibliotecas(JfInst *I, const Tarefa *t) {
  JfConta c;
  RedeJob *j = abrirTarefa(I, t, &c);
  JfBiblioteca libs[JF_FIL_MAX];
  CatItem *itens;
  CatFileira fils[JF_FIL_MAX];
  int nl, nf = 0, ni = 0, e = JF_OK, i;
  if (!j) return;
  // Limit checked before allocating: JF_FIL_MAX x JF_POR_FILEIRA items.
  itens = calloc((size_t)JF_FIL_MAX * JF_POR_FILEIRA, sizeof *itens);
  if (!itens) { fecharTarefa(I, j); return; }
  nl = jf_bibliotecas(&c, j, libs, JF_FIL_MAX);
  if (nl < 0) e = nl;
  for (i = 0; i < nl && e == JF_OK; i++) {
    int total, n = jf_itens(&c, j, libs[i].id, 0, JF_POR_FILEIRA, itens + ni, JF_POR_FILEIRA, &total);
    if (n < 0) { e = n; break; }
    if (!n) continue;
    memset(&fils[nf], 0, sizeof fils[nf]);
    snprintf(fils[nf].chave, sizeof fils[nf].chave, "%s%s", I->prefixoFila, libs[i].id);
    snprintf(fils[nf].titulo, sizeof fils[nf].titulo, "%s · %s", libs[i].nome, jf_tipo_nome(I->tipo));
    copiar(fils[nf].tipo, sizeof fils[nf].tipo, libs[i].tipo);
    fils[nf].ini = ni; fils[nf].n = n;
    ni += n; nf++;
  }
  pthread_mutex_lock(&I->trava);
  if (aindaVale(I, t, j)) {
    if (e == JF_ERR_AUTH) expirouLocked(I);
    else if (e == JF_OK) {
      free(I->snapItens);
      I->snapItens = itens; itens = NULL;
      memcpy(I->snapFils, fils, sizeof(CatFileira) * (size_t)nf);
      I->nSnapFils = nf; I->nSnapItens = ni;
      I->snapVersao++;
    }
  }
  pthread_mutex_unlock(&I->trava);
  free(itens);
  apagarSegredo(&c, sizeof c);
  fecharTarefa(I, j);
}

static void tarefaFontes(JfInst *I, const Tarefa *t) {
  JfConta c;
  RedeJob *j = abrirTarefa(I, t, &c);
  char tag[JFID_TAG + 1], item[JFID_ITEM + 1];
  JfPlayback *pb;
  int e;
  if (!j) return;
  pb = malloc(sizeof *pb);
  if (!pb || !jfid_partes_p(I->prefixoId, t->a, tag, item)) e = JF_ERR_ENTRADA;
  else if (strncmp(c.servidorId, tag, JFID_TAG)) e = JF_ERR_OUTRO_SERVIDOR;
  else e = jf_playbackinfo(&c, j, item, jellyfin_backend(), pb);
  pthread_mutex_lock(&I->trava);
  if (aindaVale(I, t, j) && !strcmp(I->fontesAlvo, t->a)) {
    if (e == JF_ERR_AUTH) { expirouLocked(I); }
    else if (e != JF_OK) I->fontesEstado = JF_FONTES_FALHOU;
    else {
      int i, k;
      free(I->fontesLista);
      I->fontesLista = pb->n ? malloc(sizeof(Stream) * (size_t)pb->n) : NULL;
      I->nFontes = I->fontesLista ? pb->n : 0;
      if (I->nFontes) memcpy(I->fontesLista, pb->fonte, sizeof(Stream) * (size_t)I->nFontes);
      I->fontesEstado = JF_FONTES_PRONTO;
      // Register check-in sessions by URL; evict the oldest never started.
      for (i = 0; i < I->nFontes; i++) {
        int alvo = -1;
        unsigned long long velho = ~0ull;
        for (k = 0; k < JF_SESSOES; k++) {
          if (!I->sessoes[k].ativo) { alvo = k; break; }
          if (!I->sessoes[k].iniciado && I->sessoes[k].criadoMs < velho) { velho = I->sessoes[k].criadoMs; alvo = k; }
        }
        if (alvo < 0) break;
        memset(&I->sessoes[alvo], 0, sizeof I->sessoes[alvo]);
        I->sessoes[alvo].ativo = 1;
        I->sessoes[alvo].geracao = I->geracao;
        I->sessoes[alvo].criadoMs = agoraMs();
        I->sessoes[alvo].s = pb->sessao[i];
      }
    }
  }
  pthread_mutex_unlock(&I->trava);
  if (pb) { apagarSegredo(pb, sizeof *pb); free(pb); }
  apagarSegredo(&c, sizeof c);
  fecharTarefa(I, j);
}

static void tarefaSair(JfInst *I, Tarefa *t) {
  JfConta c;
  char *b, *tok, *dev;
  RedeJob *j;
  memset(&c, 0, sizeof c);
  c.tipo = I->tipo;
  b = t->a; tok = strchr(b, '\n');
  if (tok) { *tok++ = 0; dev = strchr(tok, '\n'); if (dev) *dev++ = 0; } else dev = NULL;
  copiar(c.base, sizeof c.base, b);
  copiar(c.token, sizeof c.token, tok);
  copiar(c.dispositivoId, sizeof c.dispositivoId, dev);
  apagarSegredo(t->a, sizeof t->a);
  pthread_mutex_lock(&I->trava);
  j = (t->geracao == I->geracao && I->grupo) ? rede_job_criar(I->grupo) : NULL;
  pthread_mutex_unlock(&I->trava);
  if (j && c.token[0]) jf_sair(&c, j);
  if (j) rede_job_soltar(j);
  apagarSegredo(&c, sizeof c);
}

static void *trabalharCtl(void *u) {
  JfInst *I = u;
  for (;;) {
    Tarefa t;
    pthread_mutex_lock(&I->trava);
    while (!I->parar && !I->nCtl) pthread_cond_wait(&I->sinalCtl, &I->trava);
    if (I->parar) { pthread_mutex_unlock(&I->trava); break; }
    t = I->filaCtl[I->iniCtl];
    apagarSegredo(&I->filaCtl[I->iniCtl], sizeof I->filaCtl[I->iniCtl]);
    I->iniCtl = (I->iniCtl + 1) % JF_FILA_CTL;
    I->nCtl--;
    pthread_mutex_unlock(&I->trava);
    switch (t.tipo) {
      case T_VERIFICAR: tarefaVerificar(I, &t); break;
      case T_QC: tarefaQc(I, &t); break;
      case T_SENHA: tarefaSenha(I, &t); break;
      case T_BIBLIOTECAS: tarefaBibliotecas(I, &t); break;
      case T_FONTES: tarefaFontes(I, &t); break;
      case T_SAIR: tarefaSair(I, &t); break;
      default: break;
    }
    apagarSegredo(&t, sizeof t);
  }
  return NULL;
}

// ------------------------------------------------------ worker: check-ins
static void *trabalharRel(void *u) {
  JfInst *I = u;
  for (;;) {
    Relato r;
    JfConta c;
    RedeJob *j = NULL;
    int e;
    pthread_mutex_lock(&I->trava);
    while (!I->parar && !I->nRel) pthread_cond_wait(&I->sinalRel, &I->trava);
    if (I->parar) { pthread_mutex_unlock(&I->trava); break; }
    r = I->filaRel[I->iniRel];
    apagarSegredo(&I->filaRel[I->iniRel], sizeof I->filaRel[I->iniRel]);
    I->iniRel = (I->iniRel + 1) % JF_FILA_REL;
    I->nRel--;
    if (r.geracao == I->geracao && I->grupo && I->conta.token[0]) {
      c = I->conta;
      j = rede_job_criar(I->grupo);
      I->relEmVoo = 1;
    }
    pthread_mutex_unlock(&I->trava);
    if (j) {
      e = jf_reportar(&c, j, r.evento, &r.s, r.pos);
      pthread_mutex_lock(&I->trava);
      if (e == JF_ERR_AUTH && r.geracao == I->geracao && rede_job_estado(j) == REDE_OK) expirouLocked(I);
      I->relEmVoo = 0;
      pthread_mutex_unlock(&I->trava);
      rede_job_soltar(j);
      apagarSegredo(&c, sizeof c);
    }
    apagarSegredo(&r, sizeof r);
  }
  return NULL;
}

static int jfi_relatorios_pendentes(JfInst *I) {
  int n;
  pthread_mutex_lock(&I->trava);
  n = I->nRel + I->relEmVoo;
  pthread_mutex_unlock(&I->trava);
  return n;
}

// Under I->trava.
static void relatarLocked(JfInst *I, int evento, const Sessao *s, double pos) {
  Relato *r;
  if (!I->fiosVivos || s->geracao != I->geracao) return;
  if (I->nRel >= JF_FILA_REL) {
    // Full queue (server unreachable): drop the oldest PROGRESS, never a
    // start/stop, so the session keeps its boundaries.
    int i, alvo = -1;
    for (i = 0; i < I->nRel; i++) {
      int k = (I->iniRel + i) % JF_FILA_REL;
      if (I->filaRel[k].evento == JF_REL_PROGRESSO) { alvo = i; break; }
    }
    if (alvo < 0) return;
    for (i = alvo; i + 1 < I->nRel; i++)
      I->filaRel[(I->iniRel + i) % JF_FILA_REL] = I->filaRel[(I->iniRel + i + 1) % JF_FILA_REL];
    I->nRel--;
  }
  r = &I->filaRel[(I->iniRel + I->nRel) % JF_FILA_REL];
  memset(r, 0, sizeof *r);
  r->evento = evento;
  r->geracao = s->geracao;
  r->pos = pos;
  r->s = s->s;
  I->nRel++;
  pthread_cond_signal(&I->sinalRel);
}

static Sessao *sessaoDaUrlLocked(JfInst *I, const char *url) {
  int i;
  if (!url || !*url) return NULL;
  for (i = 0; i < JF_SESSOES; i++)
    if (I->sessoes[i].ativo && I->sessoes[i].geracao == I->geracao && !strcmp(I->sessoes[i].s.url, url))
      return &I->sessoes[i];
  return NULL;
}

static void jfi_reproducao_tick(JfInst *I, const char *url, double pos, double dur, int tocando) {
  Sessao *s;
  unsigned long long agora;
  (void)dur;
  if (!url || strncmp(url, "http", 4)) return;
  pthread_mutex_lock(&I->trava);
  s = sessaoDaUrlLocked(I, url);
  if (!s) { pthread_mutex_unlock(&I->trava); return; }
  agora = agoraMs();
  if (!s->iniciado) {
    // "Started" only once real playback runs, never on the probe/open.
    if (tocando) {
      int i;
      // SOURCE SWITCH inside the player: the previous session of this
      // generation is over, so it gets its stop (and transcode release)
      // before the new one starts.
      for (i = 0; i < JF_SESSOES; i++)
        if (&I->sessoes[i] != s && I->sessoes[i].ativo && I->sessoes[i].iniciado) {
          relatarLocked(I, JF_REL_FIM, &I->sessoes[i], I->sessoes[i].ultPos);
          apagarSegredo(I->sessoes[i].s.url, sizeof I->sessoes[i].s.url);
          I->sessoes[i].ativo = 0;
        }
      s->iniciado = 1; s->pausado = 0; s->ultimoMs = agora;
      relatarLocked(I, JF_REL_INICIO, s, pos);
    }
  } else if (!tocando != !!s->pausado) {
    s->pausado = !tocando; s->ultimoMs = agora;
    relatarLocked(I, s->pausado ? JF_REL_PAUSA : JF_REL_RETOMA, s, pos);
  } else if (tocando && agora - s->ultimoMs >= JF_PROGRESSO_MS) {
    s->ultimoMs = agora;
    relatarLocked(I, JF_REL_PROGRESSO, s, pos);
  }
  if (pos > 0) s->ultPos = pos;
  pthread_mutex_unlock(&I->trava);
}

static void jfi_reproducao_fim(JfInst *I, const char *url, double pos, double dur) {
  Sessao *s;
  (void)dur;
  if (!url) return;
  pthread_mutex_lock(&I->trava);
  s = sessaoDaUrlLocked(I, url);
  if (s) {
    if (s->iniciado) relatarLocked(I, JF_REL_FIM, s, pos);
    apagarSegredo(s->s.url, sizeof s->s.url);
    s->ativo = 0;
  }
  pthread_mutex_unlock(&I->trava);
}

// -------------------------------------------------------------- sources
static int jfi_fontes_pedir(JfInst *I, const char *alvo) {
  char tag[JFID_TAG + 1], item[JFID_ITEM + 1];
  int ok = 0;
  garantir(I);
  if (!jfid_partes_p(I->prefixoId, alvo, tag, item)) return 0;
  pthread_mutex_lock(&I->trava);
  conferirPerfilLocked(I);
  free(I->fontesLista); I->fontesLista = NULL; I->nFontes = 0;
  copiar(I->fontesAlvo, sizeof I->fontesAlvo, alvo);
  if (I->estado != JF_EST_CONECTADO || strncmp(I->conta.servidorId, tag, JFID_TAG)) {
    I->fontesEstado = JF_FONTES_FALHOU;
  } else {
    I->fontesEstado = JF_FONTES_PENDENTE;
    ok = enfileirarLocked(I, T_FONTES, alvo, "");
    if (!ok) I->fontesEstado = JF_FONTES_FALHOU;
  }
  pthread_mutex_unlock(&I->trava);
  return ok;
}

static int jfi_fontes_colher(JfInst *I, const char *alvo, Stream **lista, int *n) {
  int e;
  if (lista) *lista = NULL;
  if (n) *n = 0;
  pthread_mutex_lock(&I->trava);
  conferirPerfilLocked(I);
  if (!alvo || strcmp(alvo, I->fontesAlvo)) { pthread_mutex_unlock(&I->trava); return JF_FONTES_FALHOU; }
  e = I->fontesEstado;
  if (e == JF_FONTES_PRONTO) {
    if (lista) { *lista = I->fontesLista; I->fontesLista = NULL; }
    if (n) *n = I->nFontes;
    I->nFontes = 0;
    I->fontesEstado = JF_FONTES_NADA;
  } else if (e == JF_FONTES_FALHOU) I->fontesEstado = JF_FONTES_NADA;
  pthread_mutex_unlock(&I->trava);
  return e;
}

// ---------------------------------------------------------- title page
static int jfi_ficha(JfInst *I, CatItem *item, CatEp *eps, int maxEps) {
  char tag[JFID_TAG + 1], id[JFID_ITEM + 1];
  JfConta c;
  RedeJob *j = NULL;
  unsigned g;
  CatItem novo;
  int ne = 0, e;
  garantir(I);
  if (!item || !jfid_partes_p(I->prefixoId, item->imdb, tag, id)) return -1;
  pthread_mutex_lock(&I->trava);
  conferirPerfilLocked(I);
  g = I->geracao;
  if (I->estado == JF_EST_CONECTADO && !strncmp(I->conta.servidorId, tag, JFID_TAG) && I->grupo) {
    c = I->conta;
    j = rede_job_criar(I->grupo);
  }
  pthread_mutex_unlock(&I->trava);
  if (!j) return -1;
  e = jf_detalhe(&c, j, id, &novo);
  if (e == JF_OK && !strcmp(item->tipo, "series") && eps && maxEps > 0) {
    ne = jf_episodios(&c, j, id, eps, maxEps);
    if (ne < 0) { e = ne; ne = 0; }
  }
  pthread_mutex_lock(&I->trava);
  if (g != I->geracao || rede_job_estado(j) != REDE_OK) e = JF_ERR_CANCELADO;
  else if (e == JF_ERR_AUTH) expirouLocked(I);
  pthread_mutex_unlock(&I->trava);
  rede_job_soltar(j);
  apagarSegredo(&c, sizeof c);
  if (e != JF_OK) return -1;
  // Keep what the row already had when the item route omits it.
  if (!novo.poster[0]) copiar(novo.poster, sizeof novo.poster, item->poster);
  if (!novo.backdrop[0]) copiar(novo.backdrop, sizeof novo.backdrop, item->backdrop);
  if (ne > 0) {
    int k, t;
    novo.nTemporadas = 0;
    for (k = 0; k < ne; k++) {
      int ja = 0;
      t = eps[k].temporada;
      for (int q = 0; q < novo.nTemporadas; q++) if (novo.temporadas[q] == t) ja = 1;
      if (!ja && novo.nTemporadas < CAT_TEMP_MAX) novo.temporadas[novo.nTemporadas++] = t;
    }
  }
  novo.naLista = item->naLista;
  novo.naColecao = item->naColecao;
  novo.retomadoMs = item->retomadoMs;
  *item = novo;
  return ne;
}

// ====================================================== public API (2 kinds)
// jellyfin_* drives instance 0, emby_* instance 1: thin forwards, no logic.
#define JF_API(P, I)                                                                         \
  void P##_carregar(void) { jfi_carregar(I); }                                                \
  void P##_perfil_trocou(void) { jfi_perfil_trocou(I); }                                      \
  void P##_esquecer(void) { jfi_esquecer(I); }                                                \
  void P##_esquecer_todos(void) { jfi_esquecer_todos(I); }                                    \
  void P##_encerrar(void) { jfi_encerrar(I); }                                                \
  JfEstado P##_estado(char *d, size_t t) { return jfi_estado(I, d, t); }                      \
  const char *P##_servidor_curto(void) { return jfi_servidor_curto(I); }                      \
  const char *P##_usuario(void) { return jfi_usuario(I); }                                    \
  int P##_conectado(void) { return jfi_conectado(I); }                                        \
  int P##_qc_permitido(void) { return jfi_qc_permitido(I); }                                  \
  int P##_ultimo_erro(void) { return jfi_ultimo_erro(I); }                                    \
  int P##_definir_servidor(const char *u) { return jfi_definir_servidor(I, u); }              \
  int P##_entrar_quick_connect(void) { return jfi_entrar_quick_connect(I); }                  \
  int P##_entrar_senha(const char *u, char *s) { return jfi_entrar_senha(I, u, s); }          \
  void P##_cancelar_entrada(void) { jfi_cancelar_entrada(I); }                                \
  void P##_recarregar_bibliotecas(void) { jfi_recarregar_bibliotecas(I); }                    \
  unsigned P##_fileiras_versao(void) { return jfi_fileiras_versao(I); }                       \
  int P##_chave_fileira(const char *k) { return jfi_chave_fileira(I, k); }                    \
  int P##_fileiras_copiar(CatItem *it, int mi, CatFileira *f, int mf, int *n) {               \
    return jfi_fileiras_copiar(I, it, mi, f, mf, n); }                                        \
  int P##_ficha(CatItem *it, CatEp *e, int m) { return jfi_ficha(I, it, e, m); }              \
  int P##_fontes_pedir(const char *a) { return jfi_fontes_pedir(I, a); }                      \
  int P##_fontes_colher(const char *a, Stream **l, int *n) { return jfi_fontes_colher(I, a, l, n); } \
  void P##_reproducao_tick(const char *u, double p, double d, int t) { jfi_reproducao_tick(I, u, p, d, t); } \
  void P##_reproducao_fim(const char *u, double p, double d) { jfi_reproducao_fim(I, u, p, d); } \
  int P##_relatorios_pendentes(void) { return jfi_relatorios_pendentes(I); }

JF_API(jellyfin, &instJf)
JF_API(emby, &instEm)
