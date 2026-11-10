#include "openseekproto.h"
#include "osp1.h"
#include "js.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>

static int hostCar(char c) { return isalnum((unsigned char)c) || c == '.' || c == '-'; }

int openseek_normalizar_endereco(const char *in, char *base, size_t n) {
  char host[128];
  const char *p, *h, *hf;
  long porta = OS_PORTA_PADRAO;
  size_t nh;
  int v6 = 0;
  if (!base || n < 16) return 0;
  base[0] = 0;
  if (!in) return 0;
  while (*in == ' ' || *in == '\t') in++;
  p = in;
  if (!strncasecmp(p, "https://", 8)) p += 8;
  else if (strstr(p, "://")) return 0;           // http:// e o resto: nunca
  h = p;
  if (*h == '[') {                               // IPv6 entre colchetes
    hf = strchr(h, ']');
    if (!hf) return 0;
    for (const char *q = h + 1; q < hf; q++)
      if (!isxdigit((unsigned char)*q) && *q != ':' && *q != '.') return 0;
    hf++; v6 = 1;
  } else {
    hf = h;
    while (hostCar(*hf)) hf++;
  }
  nh = (size_t)(hf - h);
  if (nh == 0 || nh >= sizeof host || (v6 && nh < 4)) return 0;
  memcpy(host, h, nh); host[nh] = 0;
  if (!v6 && (host[0] == '-' || host[0] == '.' || host[nh - 1] == '-')) return 0;
  p = hf;
  if (*p == ':') {
    char *f;
    porta = strtol(p + 1, &f, 10);
    if (f == p + 1 || porta < 1 || porta > 65535) return 0;
    p = f;
  }
  if (*p == '/') p++;
  while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
  if (*p) return 0;                              // caminho, query, @, espaco...
  for (size_t i = 0; i < nh; i++) host[i] = (char)tolower((unsigned char)host[i]);
  return (size_t)snprintf(base, n, "https://%s:%ld", host, porta) < n;
}

int openseek_na_origem(const char *url, const char *base) {
  size_t k;
  if (!url || !base || !*base) return 0;
  k = strlen(base);
  return !strncmp(url, base, k) && url[k] == '/';
}

// Copia a URL sem os parametros `tira` (lista "t\0exp\0\0") e sem fragmento.
static int semParams(const char *url, char *dst, size_t n) {
  const char *q = strchr(url, '?'), *fim = url + strcspn(url, "#");
  size_t k;
  if (!q || q > fim) q = fim;
  k = (size_t)(q - url);
  if (k >= n) return 0;
  memcpy(dst, url, k);
  if (q < fim) {
    const char *i = q + 1;
    int primeiro = 1;
    while (i < fim) {
      const char *f = i;
      size_t len;
      while (f < fim && *f != '&') f++;
      len = (size_t)(f - i);
      if (!(len >= 2 && !strncmp(i, "t=", 2)) && !(len >= 4 && !strncmp(i, "exp=", 4)) && len) {
        if (k + len + 2 >= n) return 0;
        dst[k++] = primeiro ? '?' : '&';
        memcpy(dst + k, i, len); k += len; primeiro = 0;
      }
      i = f < fim ? f + 1 : f;
    }
  }
  if (k >= n) return 0;
  dst[k] = 0;
  return 1;
}
int openseek_chave(const char *url, char *dst, size_t n) {
  char *c;
  if (!url || !dst || !n) return 0;
  dst[0] = 0;
  if (!semParams(url, dst, n)) { dst[0] = 0; return 0; }
  // /v1/c/<cap>/: a capacidade depende do exp (CONTRACT-v1) e muda a cada
  // renovacao; o conteudo e o mesmo (preview_id + indice, imutavel).
  c = strstr(dst, "/v1/c/");
  if (c) {
    char *seg = c + 6, *fimSeg = strchr(seg, '/');
    if (fimSeg && fimSeg > seg) { *seg = '_'; memmove(seg + 1, fimSeg, strlen(fimSeg) + 1); }
  }
  return 1;
}

long long openseek_url_exp(const char *url) {
  const char *p;
  if (!url) return 0;
  p = strstr(url, "?exp=");
  if (!p) p = strstr(url, "&exp=");
  if (p) {
    char *fim;
    long long e = strtoll(p + 5, &fim, 10);
    if (fim > p + 5 && (*fim == 0 || *fim == '&' || *fim == '#') && e > 0) return e;
  }
  return 0;
}

int openseek_jpeg_dimensoes(const unsigned char *b, size_t n, int *w, int *h) {
  size_t i = 2;
  if (!b || n < 4 || b[0] != 0xFF || b[1] != 0xD8) return 0;
  while (i + 4 <= n) {
    unsigned m, len;
    if (b[i] != 0xFF) return 0;
    while (i < n && b[i] == 0xFF) i++;
    if (i >= n) return 0;
    m = b[i++];
    if (m == 0xD8 || (m >= 0xD0 && m <= 0xD7) || m == 0x01) continue;
    if (m == 0xD9 || m == 0xDA) return 0;        // sem SOF antes dos dados
    if (i + 2 > n) return 0;
    len = (unsigned)b[i] << 8 | b[i + 1];
    if (len < 2 || i + len > n) return 0;
    if ((m >= 0xC0 && m <= 0xCF) && m != 0xC4 && m != 0xC8 && m != 0xCC) {
      if (len < 7) return 0;
      if (h) *h = (int)((unsigned)b[i + 3] << 8 | b[i + 4]);
      if (w) *w = (int)((unsigned)b[i + 5] << 8 | b[i + 6]);
      return 1;
    }
    i += len;
  }
  return 0;
}

// --- JSON --------------------------------------------------------------------
typedef struct { char *d; size_t n, k; int ok; } Buf;
static void bAdd(Buf *b, const char *s, size_t len) {
  if (!b->ok || len >= b->n - b->k) { b->ok = 0; return; }
  memcpy(b->d + b->k, s, len); b->k += len; b->d[b->k] = 0;
}
static void bTxt(Buf *b, const char *s) { bAdd(b, s, strlen(s)); }
static void bFmt(Buf *b, const char *fmt, long long v) {
  char t[32]; int k = snprintf(t, sizeof t, fmt, v); if (k > 0) bAdd(b, t, (size_t)k);
}
// Cadeia JSON com aspas. Controle -> \u00XX; o resto (UTF-8) passa.
static void bStr(Buf *b, const char *s, size_t len) {
  size_t i;
  bAdd(b, "\"", 1);
  for (i = 0; i < len && b->ok; i++) {
    unsigned char c = (unsigned char)s[i];
    if (c == '"' || c == '\\') { char e[2] = { '\\', (char)c }; bAdd(b, e, 2); }
    else if (c < 0x20 || c == 0x7f) { char e[8]; snprintf(e, sizeof e, "\\u%04x", c); bAdd(b, e, 6); }
    else bAdd(b, (const char *)&c, 1);
  }
  bAdd(b, "\"", 1);
}

static const char *CABS_OK[] = { "User-Agent", "Referer", "Origin" };

size_t openseek_json_previa(char *dst, size_t n, const OpenSeekFonte *f) {
  Buf b = { dst, n, 0, 1 };
  int primeiro = 1;
  if (!dst || n < 2 || !f || !f->url || strncmp(f->url, "http", 4)) return 0;
  dst[0] = 0;
  bTxt(&b, "{\"source\":{\"url\":");
  bStr(&b, f->url, strlen(f->url));
  bTxt(&b, ",\"headers\":{");
  // Allowlist (CONTRACT-v1): User-Agent, Referer, Origin. Um de cada.
  for (int k = 0; k < 3; k++) {
    const char *l = f->cabecalhos;
    size_t nome = strlen(CABS_OK[k]);
    while (l && *l) {
      const char *fim = l + strcspn(l, "\n");
      if ((size_t)(fim - l) > nome && !strncasecmp(l, CABS_OK[k], nome) && l[nome] == ':') {
        const char *v = l + nome + 1, *vf = fim;
        while (v < vf && (*v == ' ' || *v == '\t')) v++;
        while (vf > v && (vf[-1] == '\r' || vf[-1] == ' ' || vf[-1] == '\t')) vf--;
        if (vf > v) {
          if (!primeiro) bTxt(&b, ",");
          bStr(&b, CABS_OK[k], nome); bTxt(&b, ":"); bStr(&b, v, (size_t)(vf - v));
          primeiro = 0;
          break;
        }
      }
      l = *fim ? fim + 1 : fim;
    }
  }
  bTxt(&b, "}");
  if (f->durMs > 0) bFmt(&b, ",\"duration_ms\":%lld", f->durMs);
  bTxt(&b, "},\"hints\":{");
  {
    const char *im = f->imdb && !strncmp(f->imdb, "tt", 2) ? f->imdb : "";
    bTxt(&b, "\"imdb_id\":"); bStr(&b, im, strcspn(im, ":"));
  }
  bFmt(&b, ",\"season\":%lld", f->temporada > 0 ? f->temporada : 0);
  bFmt(&b, ",\"episode\":%lld", f->episodio > 0 ? f->episodio : 0);
  bFmt(&b, ",\"video_size\":%lld", (long long)(f->tamanho > 0x7fffffffffffffffULL ? 0 : f->tamanho));
  bTxt(&b, ",\"video_hash\":"); bStr(&b, f->videoHash ? f->videoHash : "", f->videoHash ? strlen(f->videoHash) : 0);
  bTxt(&b, ",\"filename\":"); bStr(&b, f->arquivo ? f->arquivo : "", f->arquivo ? strlen(f->arquivo) : 0);
  bTxt(&b, "},\"at_ms\":");
  if (f->atMs >= 0) bFmt(&b, "%lld", f->atMs); else bTxt(&b, "null");
  bFmt(&b, ",\"playhead_ms\":%lld", f->playheadMs > 0 ? f->playheadMs : 0);
  bTxt(&b, ",\"layout\":\"hybrid\"}");
  if (!b.ok || b.k > 16384) { dst[0] = 0; return 0; }   // corpo <= 16 KiB
  return b.k;
}

// --- fonte alternativa (opcional) ---------------------------------------------
static void bCabs(Buf *b, const char *cabs) {   // mesma allowlist do POST de criacao
  int primeiro = 1;
  bTxt(b, "{");
  for (int k = 0; k < 3; k++) {
    const char *l = cabs;
    size_t nome = strlen(CABS_OK[k]);
    while (l && *l) {
      const char *fim = l + strcspn(l, "\n");
      if ((size_t)(fim - l) > nome && !strncasecmp(l, CABS_OK[k], nome) && l[nome] == ':') {
        const char *v = l + nome + 1, *vf = fim;
        while (v < vf && (*v == ' ' || *v == '\t')) v++;
        while (vf > v && (vf[-1] == '\r' || vf[-1] == ' ' || vf[-1] == '\t')) vf--;
        if (vf > v) {
          if (!primeiro) bTxt(b, ",");
          bStr(b, CABS_OK[k], nome); bTxt(b, ":"); bStr(b, v, (size_t)(vf - v));
          primeiro = 0;
          break;
        }
      }
      l = *fim ? fim + 1 : fim;
    }
  }
  bTxt(b, "}");
}

static void bCand(Buf *b, const OpenSeekAltCand *c) {
  size_t nome = c->nome ? strnlen(c->nome, 200) : 0;       // o servidor aceita ate 240
  bTxt(b, "{\"cid\":"); bStr(b, c->cid, strnlen(c->cid, sizeof c->cid));
  bTxt(b, ",\"title_key\":"); bStr(b, c->titleKey, strnlen(c->titleKey, sizeof c->titleKey));
  bFmt(b, ",\"height\":%lld", c->altura > 0 && c->altura < 8192 ? c->altura : 0);
  bFmt(b, ",\"duration_ms\":%lld", c->durMs > 0 && c->durMs < 43200000LL ? c->durMs : 0);
  bFmt(b, ",\"size\":%lld", (long long)(c->tamanho > 0x7fffffffffffffffULL ? 0 : c->tamanho));
  bTxt(b, ",\"codec\":"); bStr(b, c->codec ? c->codec : "", c->codec ? strnlen(c->codec, 32) : 0);
  bTxt(b, ",\"container\":"); bStr(b, c->container ? c->container : "", c->container ? strnlen(c->container, 32) : 0);
  bTxt(b, ",\"name\":"); bStr(b, c->nome ? c->nome : "", nome);
  bTxt(b, c->emCache ? ",\"cached\":true" : ",\"cached\":false");
  bTxt(b, c->resolvida ? ",\"resolved\":true}" : ",\"resolved\":false}");
}

size_t openseek_json_previa_alt(char *dst, size_t n, const OpenSeekFonte *f,
                                const OpenSeekAltCand *play, const OpenSeekAltCand *c, int nc) {
  size_t len = openseek_json_previa(dst, n, f);
  if (!len || !play || !c || nc <= 0 || dst[len - 1] != '}') return len;   // sem candidatos: corpo de sempre
  if (nc > OS_ALT_MAX) nc = OS_ALT_MAX;
  Buf b = { dst, n, len - 1, 1 };                           // reabre antes do '}' final
  dst[len - 1] = 0;
  bTxt(&b, ",\"alt\":{\"play\":"); bCand(&b, play);
  bTxt(&b, ",\"candidates\":[");
  for (int i = 0; i < nc; i++) { if (i) bTxt(&b, ","); bCand(&b, &c[i]); }
  bTxt(&b, "],\"telemetry\":true}}");
  if (!b.ok) { dst[len - 1] = '}'; dst[len] = 0; return len; }   // nao coube: corpo sem o bloco
  return b.k;
}

size_t openseek_json_alt_fonte(char *dst, size_t n, const char *cid, const char *url, const char *cabs) {
  Buf b = { dst, n, 0, 1 };
  if (!dst || n < 2 || !cid || !*cid || !url || strncmp(url, "http", 4)) return 0;
  dst[0] = 0;
  bTxt(&b, "{\"cid\":"); bStr(&b, cid, strnlen(cid, 24));
  bTxt(&b, ",\"source\":{\"url\":"); bStr(&b, url, strlen(url));
  bTxt(&b, ",\"headers\":"); bCabs(&b, cabs ? cabs : "");
  bTxt(&b, "}}");
  return b.ok ? b.k : 0;
}

size_t openseek_json_telemetria(char *dst, size_t n, long bufferMs, int rebuffering, const char *host, int conns) {
  Buf b = { dst, n, 0, 1 };
  if (!dst || n < 2) return 0;
  dst[0] = 0;
  bTxt(&b, "{");
  if (bufferMs >= 0) bFmt(&b, "\"buffer_ms\":%lld,", bufferMs > 600000 ? 600000 : bufferMs);
  bTxt(&b, rebuffering ? "\"rebuffering\":true" : "\"rebuffering\":false");
  if (host && *host && strlen(host) < 255) { bTxt(&b, ",\"host\":"); bStr(&b, host, strlen(host)); }
  bFmt(&b, ",\"conns\":%lld}", conns < 0 ? 0 : conns > 64 ? 64 : conns);
  return b.ok ? b.k : 0;
}

int openseek_ler_alt(const char *json, char *estado, size_t nEstado, char *want, size_t nWant) {
  const char *a, *fim;
  if (estado && nEstado) estado[0] = 0;
  if (want && nWant) want[0] = 0;
  if (!json || !(a = strstr(json, "\"alt\":{"))) return 0;
  a += 6;                                                  // no '{' do bloco (sem objetos aninhados)
  fim = strchr(a, '}');
  if (!fim) return 0;
  if (estado && nEstado) js_texto(a, fim, "state", estado, nEstado);
  if (want && nWant) js_texto(a, fim, "want", want, nWant);
  return 1;
}

size_t openseek_json_prioridade(char *dst, size_t n, long atMs, long playheadMs) {
  Buf b = { dst, n, 0, 1 };
  if (!dst || n < 2) return 0;
  dst[0] = 0;
  bTxt(&b, "{\"at_ms\":");
  if (atMs >= 0) bFmt(&b, "%lld", atMs); else bTxt(&b, "null");
  bFmt(&b, ",\"playhead_ms\":%lld}", playheadMs > 0 ? playheadMs : 0);
  return b.ok ? b.k : 0;
}

size_t openseek_json_claim(char *dst, size_t n, const char *pairId,
                           const char *device, const char *prova) {
  Buf b = { dst, n, 0, 1 };
  if (!dst || n < 2 || !pairId || !device || !prova) return 0;
  dst[0] = 0;
  bTxt(&b, "{\"pair_id\":"); bStr(&b, pairId, strlen(pairId));
  bTxt(&b, ",\"device_name\":"); bStr(&b, device, strlen(device));
  bTxt(&b, ",\"client_proof\":"); bStr(&b, prova, strlen(prova));
  bTxt(&b, "}");
  return b.ok ? b.k : 0;
}

static int soHex(const char *s, size_t n) {
  size_t i;
  if (strlen(s) != n) return 0;
  for (i = 0; i < n; i++) if (!isxdigit((unsigned char)s[i]) || isupper((unsigned char)s[i])) return 0;
  return 1;
}

int openseek_ler_start(const char *json, OpenSeekStart *s) {
  double it;
  if (!json || !s) return 0;
  memset(s, 0, sizeof *s);
  if (!js_texto_raiz(json, "pair_id", s->pairId, sizeof s->pairId) || !soHex(s->pairId, 32) ||
      !js_texto_raiz(json, "salt", s->salt, sizeof s->salt) || !soHex(s->salt, 32) ||
      !js_texto_raiz(json, "spki_pin", s->pino, sizeof s->pino) || !rede_pino_valido(s->pino) ||
      strchr(s->pino, ';')) { memset(s, 0, sizeof *s); return 0; }
  it = js_num(json, NULL, "iterations", -1);
  if (it < 1 || it > 1e9) { memset(s, 0, sizeof *s); return 0; }
  s->iter = (long)it;
  return 1;
}

int openseek_ler_claim(const char *json, OpenSeekClaim *c) {
  size_t i, n;
  if (!json || !c) return 0;
  memset(c, 0, sizeof *c);
  if (!js_texto_raiz(json, "device_id", c->deviceId, sizeof c->deviceId) || !soHex(c->deviceId, 16) ||
      !js_texto_raiz(json, "token", c->token, sizeof c->token) ||
      !js_texto_raiz(json, "server_proof", c->prova, sizeof c->prova) || !soHex(c->prova, 64)) {
    memset(c, 0, sizeof *c); return 0;
  }
  snprintf(c->papel, sizeof c->papel, "owner");
  js_texto_raiz(json, "role", c->papel, sizeof c->papel);               // ausente (servidor antigo) = dono
  if (strcmp(c->papel, "owner") && strcmp(c->papel, "guest")) snprintf(c->papel, sizeof c->papel, "owner");
  n = strlen(c->token);
  if (n < 32 || n > 60) { memset(c, 0, sizeof *c); return 0; }
  for (i = 0; i < n; i++)
    if (!isalnum((unsigned char)c->token[i]) && c->token[i] != '-' && c->token[i] != '_') {
      memset(c, 0, sizeof *c); return 0;
    }
  return 1;
}

static int estadoDe(const char *s) {
  if (!strcmp(s, "preparing")) return OS_PREPARANDO;
  if (!strcmp(s, "ready")) return OS_PRONTO;
  if (!strcmp(s, "complete")) return OS_COMPLETO;
  if (!strcmp(s, "failed")) return OS_FALHOU;
  return 0;
}

int openseek_ler_previa(const char *json, const char *base, OpenSeekPrevia *p) {
  char st[24] = "";
  double v;
  if (!json || !p || !base) return 0;
  memset(p, 0, sizeof *p);
  if (!js_texto_raiz(json, "preview_id", p->id, sizeof p->id) || !soHex(p->id, 32) ||
      !js_texto_raiz(json, "state", st, sizeof st) || !(p->estado = estadoDe(st))) {
    memset(p, 0, sizeof *p); return 0;
  }
  js_texto_raiz(json, "error", p->erro, sizeof p->erro);
  // vtt_url absoluta e NA ORIGEM PAREADA: o pino so cobre ela (NUVIO-REVIEW 4).
  if (!seekr_ler_lookup_base(json, base, p->vtt, sizeof p->vtt) || !openseek_na_origem(p->vtt, base))
    p->vtt[0] = 0;
  if (!p->vtt[0] && p->estado != OS_FALHOU) { memset(p, 0, sizeof *p); return 0; }
  p->exp = openseek_url_exp(p->vtt);
  v = js_num(json, NULL, "vtt_version", 0);
  p->versao = v >= 0 && v < 2e9 ? (long)v : 0;
  v = js_num(json, NULL, "refresh_after_ms", 1000);
  p->refreshMs = v >= 100 && v <= 600000 ? (long)v : 1000;
  return 1;
}

int openseek_ler_versao(const char *json, long *versao, int *estado) {
  char st[24] = "";
  double v;
  if (!json || !js_tem(json, NULL, "vtt_version")) return 0;
  v = js_num(json, NULL, "vtt_version", -1);
  if (v < 0 || v >= 2e9) return 0;
  if (versao) *versao = (long)v;
  if (estado) *estado = js_texto_raiz(json, "state", st, sizeof st) ? estadoDe(st) : 0;
  return 1;
}

int openseek_mapear(int http, int redeErro, int curlErro, const OpenSeekPrevia *p,
                    int nCues, int *motivo) {
  int m = OSM_NENHUM, e;
  if (redeErro == REDE_CANCELADO || redeErro == REDE_GERACAO) return -1;
  if (redeErro == REDE_INDISPONIVEL || redeErro == REDE_ENTRADA) {
    // Sem libcurl ou sem CURLOPT_PINNEDPUBLICKEY neste aparelho: nada sai.
    m = OSM_SEM_PINO; e = OS_SEEKR_REDE_INDISPONIVEL;
  } else if (redeErro != REDE_OK && curlErro == REDE_CURL_PINO_DIFERENTE) {
    m = OSM_PINO_MUDOU; e = OS_SEEKR_REDE_INDISPONIVEL;
  } else if (redeErro != REDE_OK || http == 0) {
    m = OSM_REDE; e = OS_SEEKR_REDE_INDISPONIVEL;
  } else if (http == 401) {
    m = OSM_RECUSADO; e = OS_SEEKR_CHAVE_RECUSADA;
  } else if (http == 403) {
    m = OSM_EXPIRADO; e = OS_SEEKR_REDE_INDISPONIVEL;   // capacidade vencida: renovar
  } else if (http == 429 || http == 503) {
    m = OSM_OCUPADO; e = OS_SEEKR_LIMITE_PROVEDOR;
  } else if (http == 400 || http == 413 || http == 404) {
    m = OSM_FONTE; e = OS_SEEKR_SEM_PREVIA;
  } else if (http >= 500 || http < 200 || http >= 300) {
    m = OSM_REDE; e = OS_SEEKR_REDE_INDISPONIVEL;
  } else if (!p) {
    m = OSM_REDE; e = OS_SEEKR_REDE_INDISPONIVEL;          // 2xx ilegivel
  } else if (p->estado == OS_FALHOU) {
    m = OSM_FONTE; e = OS_SEEKR_SEM_PREVIA;
  } else if (p->estado == OS_PREPARANDO && nCues <= 0) {
    // VTT vazio enquanto prepara = carregando, nunca "sem previa".
    m = OSM_PREPARANDO; e = OS_SEEKR_BUSCANDO;
  } else e = OS_SEEKR_PRONTO;
  if (motivo) *motivo = m;
  return e;
}

// Primeiro contato sem pino: o corpo so pode ser {} ou {"invite":"<id 32 hex | etiqueta de 4>"}: o NOME do convite
// (publico), nunca o segredo nem a prova. Um codigo digitado de 16 nao traz pino, e o convite precisa nomear-se aqui.
static int corpoPrimeiroContatoOk(const void *corpo, size_t n) {
  const char *c = corpo;
  static const char P[] = "{\"invite\":\"";
  size_t i, k;
  if (!c || n == 0) return 1;
  if (n == 2 && !memcmp(c, "{}", 2)) return 1;
  if (n < sizeof P - 1 + 4 + 2 || memcmp(c, P, sizeof P - 1)) return 0;
  k = n - 2;                                              // termina com "}
  if (c[k] != '"' || c[k + 1] != '}') return 0;
  k -= sizeof P - 1;
  if (k != 4 && k != 32) return 0;
  for (i = 0; i < k; i++) {
    char ch = c[sizeof P - 1 + i];
    if (!((ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z'))) return 0;
  }
  return 1;
}
int openseek_pedido(OpenSeekPedido *o, const char *metodo, const char *url,
                    const char *token, const char *pino, int semPino,
                    const void *corpo, size_t nCorpo, unsigned prazoMs) {
  int k = 0;
  if (!o) return 0;
  memset(o, 0, sizeof *o);
  if (!url || strncmp(url, "https://", 8) || strlen(url) >= sizeof o->url) return 0;
  // SR-3: sem pino so o primeiro contato, que nao leva segredo algum.
  if (pino && *pino) { if (!rede_pino_valido(pino)) return 0; }
  else if (!semPino || (token && *token) || (corpo && nCorpo && !corpoPrimeiroContatoOk(corpo, nCorpo))) return 0;
  // SR-4: nada de URL de video em query/caminho; o unico caminho com query e
  // o long-poll de status, que so leva numeros.
  strcpy(o->url, url);
  if (token && *token) {
    if (strlen(token) > 60 || strpbrk(token, "\r\n ")) return 0;
    snprintf(o->auth, sizeof o->auth, "Authorization: Bearer %s", token);
    o->cabs[k++] = o->auth;
  }
  if (corpo && nCorpo) o->cabs[k++] = "Content-Type: application/json";
  o->cabs[k] = NULL;
  o->p.metodo = metodo;
  o->p.url = o->url;
  o->p.cabecalhos = k ? o->cabs : NULL;
  o->p.corpo = corpo;
  o->p.n_corpo = nCorpo;
  o->p.prazo_ms = prazoMs ? prazoMs : OS_PRAZO_API_MS;
  o->p.max_bytes = OS_TILE_BYTES_MAX;
  o->p.seguir = 0;
  o->p.tls_pessoal = 1;
  o->p.pino_publico = pino && *pino ? pino : NULL;
  return 1;
}

// --- LRU -----------------------------------------------------------------------
void openseek_lru_ini(OsLru *l, int n) {
  memset(l, 0, sizeof *l);
  l->n = n < 1 ? 1 : n > OS_LRU_MAX ? OS_LRU_MAX : n;
}
int openseek_lru_tem(const OsLru *l, const char *chave) {
  int i;
  if (!l || !chave || !*chave) return -1;
  for (i = 0; i < l->n; i++)
    if (l->it[i].ocupado && !strcmp(l->it[i].chave, chave)) return i;
  return -1;
}
int openseek_lru_achar(OsLru *l, const char *chave) {
  int i = openseek_lru_tem(l, chave);
  if (i >= 0) l->it[i].uso = ++l->relogio;
  return i;
}
int openseek_lru_lugar(OsLru *l, const char *chave, const char *const *prot, int np) {
  int i, k, melhor = -1;
  if (!chave || strlen(chave) >= sizeof l->it[0].chave) return -1;
  i = openseek_lru_achar(l, chave);
  if (i >= 0) return i;
  for (i = 0; i < l->n; i++) {
    int protegido = 0;
    if (!l->it[i].ocupado) { melhor = i; break; }
    for (k = 0; k < np; k++) if (prot[k] && !strcmp(prot[k], l->it[i].chave)) protegido = 1;
    if (protegido) continue;
    if (melhor < 0 || l->it[i].uso < l->it[melhor].uso) melhor = i;
  }
  if (melhor < 0) melhor = 0;
  snprintf(l->it[melhor].chave, sizeof l->it[melhor].chave, "%s", chave);
  l->it[melhor].ocupado = 1;
  l->it[melhor].uso = ++l->relogio;
  return melhor;
}

static long cueCentroMs(const SeekrCue *q) {
  return q->rotulo >= 0 ? q->rotulo : (q->ini + q->fim) / 2;
}

int openseek_cue_visivel(const SeekrVtt *v, long posMs, int *esparso) {
  int c, lo, hi, melhor;
  long distMelhor;
  if (esparso) *esparso = 0;
  if (!v || v->nCues <= 0 || posMs < 0) return -1;
  c = seekr_vtt_cue_estrita(v, posMs);
  if (c >= 0) return c;
  lo = 0; hi = v->nCues - 1;
  while (lo < hi) {
    int m = (lo + hi + 1) / 2;
    if (v->cues[m].ini <= posMs) lo = m; else hi = m - 1;
  }
  melhor = lo;
  distMelhor = labs(cueCentroMs(&v->cues[lo]) - posMs);
  if (lo + 1 < v->nCues) {
    long d2 = labs(cueCentroMs(&v->cues[lo + 1]) - posMs);
    if (d2 < distMelhor) { melhor = lo + 1; distMelhor = d2; }
  }
  if (distMelhor > OS_SPARSE_TOL_MS) return -1;
  if (esparso) *esparso = 1;
  return melhor;
}

static void filaAdd(const SeekrVtt *v, int c, int maxOut, int *out, int *n) {
  if (c < 0 || c >= v->nCues || *n >= maxOut) return;
  for (int k = 0; k < *n; k++) if (out[k] == c) return;
  out[(*n)++] = c;
}

int openseek_fila_busca(const SeekrVtt *v, long posMs, long alvoMs, int dir,
                        int maxOut, int *out) {
  int n = 0, i, ia, base;
  if (!v || v->nCues <= 0 || maxOut <= 0 || !out) return 0;
  dir = dir < 0 ? -1 : 1;
  i = openseek_cue_visivel(v, posMs, NULL);
  if (i >= 0) {
    filaAdd(v, i, maxOut, out, &n);
    filaAdd(v, i + dir, maxOut, out, &n);
    filaAdd(v, i - dir, maxOut, out, &n);
  }
  ia = alvoMs >= 0 ? openseek_cue_visivel(v, alvoMs, NULL) : -1;
  if (ia >= 0 && ia != i) {
    filaAdd(v, ia, maxOut, out, &n);
    filaAdd(v, ia + dir, maxOut, out, &n);
    filaAdd(v, ia - dir, maxOut, out, &n);
  }
  base = ia >= 0 ? ia : i >= 0 ? i : seekr_vtt_cue(v, alvoMs >= 0 ? alvoMs : posMs);
  if (base >= 0) {
    if (i < 0 && ia < 0) {
      filaAdd(v, base, maxOut, out, &n);
      filaAdd(v, base + dir, maxOut, out, &n);
      filaAdd(v, base - dir, maxOut, out, &n);
    }
    for (int passo = 2; passo <= maxOut && n < maxOut; passo++) {
      filaAdd(v, base + passo * dir, maxOut, out, &n);
      if (passo <= 4) filaAdd(v, (i >= 0 ? i : base) - passo * dir, maxOut, out, &n);
    }
  }
  return n;
}

int openseek_escolher_prefetch(const SeekrVtt *v, const char (*folhaChaves)[240],
                               const OsLru *jl, const char (*falhas)[240], int nFalhas,
                               long anchorMs, int dir) {
  int melhor = -1, fUlt = -1, fUltOk = 0;
  long long melhorScore = 0;
  long anchor = anchorMs >= 0 ? anchorMs : 0;
  if (!v || v->nCues <= 0 || v->nFolhas <= 0 || !jl) return -1;
  dir = dir < 0 ? -1 : 1;
  for (int c = 0; c < v->nCues; c++) {
    const SeekrCue *q = &v->cues[c];
    int f = q->folha, rel = 0;
    long long dist = 0, score;
    if (f < 0 || f >= v->nFolhas) continue;
    if (anchor < q->ini) { dist = (long long)q->ini - anchor; rel = 1; }
    else if (anchor >= q->fim) { dist = (long long)anchor - q->fim; rel = -1; }
    score = (rel == 0 || rel == dir) ? dist : dist * 2 + 10000LL;
    if (melhor >= 0 && score >= melhorScore) continue;
    if (f != fUlt) {
      char kBuf[240];
      const char *k = folhaChaves ? folhaChaves[f] : NULL;
      if (!k || !*k) {
        if (!openseek_chave(v->folhas[f], kBuf, sizeof kBuf)) { fUlt = f; fUltOk = 0; continue; }
        k = kBuf;
      }
      fUlt = f;
      fUltOk = openseek_lru_tem(jl, k) < 0;
      for (int j = 0; fUltOk && j < nFalhas; j++)
        if (falhas[j][0] && !strcmp(falhas[j], k)) fUltOk = 0;
    }
    if (!fUltOk) continue;
    melhor = f;
    melhorScore = score;
    if (melhorScore == 0) break;
  }
  return melhor;
}


// --- BIBLIOTECA COMPARTILHADA (convidado) ------------------------------------------------
int openseek_papel_de(const char *t) { return t && !strcmp(t, "guest") ? OS_PAPEL_CONVIDADO : OS_PAPEL_DONO; }

static int hexVal(int c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}
// Valor de `chave` na query de uma string de convite, com %XX decodificado. 1 se achou e coube.
static int queryValor(const char *q, const char *chave, char *dst, size_t n) {
  size_t k = strlen(chave);
  const char *p = q;
  dst[0] = 0;
  while (p && *p) {
    const char *fim = strchr(p, '&');
    size_t len = fim ? (size_t)(fim - p) : strlen(p), j = 0, i;
    if (len > k && !strncmp(p, chave, k) && p[k] == '=') {
      for (i = k + 1; i < len; i++) {
        int c = (unsigned char)p[i];
        if (c == '%' && i + 2 < len + 0 && hexVal(p[i + 1]) >= 0 && hexVal(p[i + 2]) >= 0) {
          c = hexVal(p[i + 1]) * 16 + hexVal(p[i + 2]); i += 2;
        }
        if (j + 1 >= n) { dst[0] = 0; return 0; }
        dst[j++] = (char)c;
      }
      dst[j] = 0;
      return 1;
    }
    p = fim ? fim + 1 : NULL;
  }
  return 0;
}
int openseek_ler_entrada(const char *texto, OpenSeekEntrada *e) {
  static const char PREF[] = "openseek://join/1?";
  char tag[OSP1_CONVITE_TAG + 1], seg[OSP1_CONVITE_SEGREDO + 1], c8[OSP1_CODE_LEN + 1], h[OS_BASE_MAX], tmp[OS_BASE_MAX];
  if (!e) return 0;
  memset(e, 0, sizeof *e);
  if (!texto) return 0;
  while (*texto == ' ' || *texto == '\t') texto++;
  if (!strncmp(texto, PREF, sizeof PREF - 1)) {
    const char *q = texto + sizeof PREF - 1;
    char id[64], c[40], p[OS_PINO_MAX + 8];
    if (strlen(q) > 600) return 0;
    if (!queryValor(q, "i", id, sizeof id) || !soHex(id, 32)) return 0;
    if (!queryValor(q, "c", c, sizeof c)) return 0;
    {                                                                          // o segredo da string tem 12 simbolos (com tracos)
      char junto[48 + 8];
      if (strlen(c) > 40) return 0;
      snprintf(junto, sizeof junto, "AAAA%s", c);
      if (!osp1_normalizar_convite(junto, tag, seg)) return 0;
    }
    if (queryValor(q, "h", h, sizeof h) && h[0]) {
      if (!openseek_normalizar_endereco(h, tmp, sizeof tmp)) return 0;
      snprintf(e->base, sizeof e->base, "%s", tmp);
    }
    if (queryValor(q, "p", p, sizeof p) && p[0]) {
      if (strlen(p) >= sizeof e->pino || !rede_pino_valido(p) || strchr(p, ';')) return 0;
      snprintf(e->pino, sizeof e->pino, "%s", p);
    }
    if (strlen(id) >= sizeof e->convite) return 0;
    snprintf(e->convite, sizeof e->convite, "%s", id);
    snprintf(e->segredo, sizeof e->segredo, "%s", seg);
    e->forma = 2;
    return 1;
  }
  if (osp1_normalizar_convite(texto, tag, seg)) {                              // codigo digitado: etiqueta + segredo
    snprintf(e->convite, sizeof e->convite, "%s", tag);
    snprintf(e->segredo, sizeof e->segredo, "%s", seg);
    e->forma = 2;
    return 1;
  }
  if (osp1_normalizar_codigo(texto, c8)) {                                     // codigo de 8 da janela do dono
    snprintf(e->segredo, sizeof e->segredo, "%s", c8);
    e->forma = 1;
    return 1;
  }
  return 0;
}

size_t openseek_json_start(char *dst, size_t n, const char *convite) {
  Buf b = { dst, n, 0, 1 };
  if (!dst || n < 4) return 0;
  if (!convite || !*convite) { bTxt(&b, "{}"); }
  else {
    size_t i;
    for (i = 0; convite[i]; i++)
      if (!isalnum((unsigned char)convite[i])) { dst[0] = 0; return 0; }     // so id hex ou etiqueta: nada a escapar
    bTxt(&b, "{\"invite\":"); bStr(&b, convite, strlen(convite)); bTxt(&b, "}");
  }
  if (!b.ok) { dst[0] = 0; return 0; }
  dst[b.k] = 0;
  return b.k;
}

static int imdbValido(const char *s) {
  size_t i, n;
  if (!s || s[0] != 't' || s[1] != 't') return 0;
  n = strlen(s);
  if (n < 3 || n > 12) return 0;
  for (i = 2; i < n; i++) if (s[i] < '0' || s[i] > '9') return 0;
  return 1;
}
size_t openseek_json_procura(char *dst, size_t n, const char *imdb, int t, int e, long durMs,
                             unsigned long long tamanho) {
  Buf b = { dst, n, 0, 1 };
  if (!dst || n < 8 || !imdbValido(imdb) || durMs <= 0) { if (dst && n) dst[0] = 0; return 0; }
  bTxt(&b, "{\"imdb_id\":"); bStr(&b, imdb, strlen(imdb));
  bFmt(&b, ",\"season\":%lld", t > 0 ? (long long)t : 0LL);
  bFmt(&b, ",\"episode\":%lld", e > 0 ? (long long)e : 0LL);
  bFmt(&b, ",\"duration_ms\":%lld", (long long)durMs);
  if (tamanho > 0 && tamanho < 0x7fffffffffffffffULL) bFmt(&b, ",\"size_bytes\":%lld", (long long)tamanho);
  bTxt(&b, ",\"layout\":\"hybrid\"}");
  if (!b.ok) { dst[0] = 0; return 0; }
  dst[b.k] = 0;
  return b.k;
}

size_t openseek_json_contribuir(char *dst, size_t n, const OpenSeekFonte *f) {
  Buf b = { dst, n, 0, 1 };
  int primeiro = 1;
  if (!dst || n < 2 || !f || !f->url || strncmp(f->url, "https://", 8) || !imdbValido(f->imdb) || f->durMs < 60000) {
    if (dst && n) dst[0] = 0;
    return 0;
  }
  dst[0] = 0;
  bTxt(&b, "{\"source\":{\"url\":");
  bStr(&b, f->url, strlen(f->url));
  bTxt(&b, ",\"headers\":{");
  for (int k = 0; k < 3; k++) {                         // a mesma lista de cabecalhos permitidos do POST /v1/previews
    const char *l = f->cabecalhos;
    size_t nome = strlen(CABS_OK[k]);
    while (l && *l) {
      const char *fim = l + strcspn(l, "\n");
      if ((size_t)(fim - l) > nome && !strncasecmp(l, CABS_OK[k], nome) && l[nome] == ':') {
        const char *v = l + nome + 1, *vf = fim;
        while (v < vf && (*v == ' ' || *v == '\t')) v++;
        while (vf > v && (vf[-1] == '\r' || vf[-1] == ' ' || vf[-1] == '\t')) vf--;
        if (vf > v) {
          if (!primeiro) bTxt(&b, ",");
          bStr(&b, CABS_OK[k], nome); bTxt(&b, ":"); bStr(&b, v, (size_t)(vf - v));
          primeiro = 0;
          break;
        }
      }
      l = *fim ? fim + 1 : fim;
    }
  }
  bTxt(&b, "}},\"imdb_id\":"); bStr(&b, f->imdb, strcspn(f->imdb, ":"));
  bFmt(&b, ",\"season\":%lld", f->temporada > 0 ? (long long)f->temporada : 0LL);
  bFmt(&b, ",\"episode\":%lld", f->episodio > 0 ? (long long)f->episodio : 0LL);
  bFmt(&b, ",\"duration_ms\":%lld}", (long long)f->durMs);
  if (!b.ok || b.k > 16384) { dst[0] = 0; return 0; }
  return b.k;
}

int openseek_ler_pode_contribuir(const char *json) {
  return json && (strstr(json, "\"contribute\":true") || strstr(json, "\"contribute\": true"));
}

size_t openseek_json_offset(char *dst, size_t n, long offsetMs) {
  Buf b = { dst, n, 0, 1 };
  if (!dst || n < 8 || offsetMs < -21600000L || offsetMs > 21600000L) { if (dst && n) dst[0] = 0; return 0; }
  bFmt(&b, "{\"offset_ms\":%lld}", (long long)offsetMs);
  if (!b.ok) { dst[0] = 0; return 0; }
  dst[b.k] = 0;
  return b.k;
}

static int jsBooleano(const char *ini, const char *fim, const char *chave) {   // 1 so para true
  char pad[48];
  const char *p;
  snprintf(pad, sizeof pad, "\"%s\":", chave);
  p = strstr(ini, pad);
  if (!p || (fim && p >= fim)) return 0;
  p += strlen(pad);
  while (*p == ' ') p++;
  return !strncmp(p, "true", 4);
}
int openseek_ler_procura(const char *json, const char *base, OpenSeekCompart *c) {
  const char *el, *fim;
  char sub[200];
  int n = 0;
  double v;
  if (!json || !base || !c) return 0;
  memset(c, 0, sizeof *c);
  el = js_array(json, NULL, "candidates");
  if (!el || *el != '{') return 0;
  fim = js_fim(el);
  if (!fim) return 0;
  for (const char *q = el; q; q = js_prox(js_fim(q))) { if (*q != '{') break; n++; if (n > 8) break; }
  c->nCandidatos = n;
  if (!js_texto(el, fim, "preview_id", c->id, sizeof c->id) || !soHex(c->id, 32)) { memset(c, 0, sizeof *c); return 0; }
  if (!js_texto(el, fim, "vtt_url", c->vtt, sizeof c->vtt) || !openseek_na_origem(c->vtt, base)) {
    memset(c, 0, sizeof *c); return 0;
  }
  c->exp = openseek_url_exp(c->vtt);
  js_texto(el, fim, "kind", c->tipo, sizeof c->tipo);
  c->verificado = jsBooleano(el, fim, "verified");
  c->aproximado = !(c->verificado && !jsBooleano(el, fim, "approximate"));     // nunca "exato" por omissao
  if (js_bruto(el, fim, "mapping", sub, sizeof sub)) {
    c->a = js_num(sub, NULL, "a", 1.0);
    c->b = js_num(sub, NULL, "b", 0.0);
  } else c->a = 1.0;
  if (!(c->a > 0.5 && c->a < 2.0) || c->b < -2.2e7 || c->b > 2.2e7) { memset(c, 0, sizeof *c); return 0; }
  if (js_bruto(el, fim, "source", sub, sizeof sub)) {
    v = js_num(sub, NULL, "duration_ms", 0); c->fonteMs = v > 0 && v < 1e10 ? (long)v : 0;
    v = js_num(sub, NULL, "tiles", 0);       c->tiles = v > 0 && v < 1e7 ? (long)v : 0;
    v = js_num(sub, NULL, "n_tiles", 0);     c->nTiles = v > 0 && v < 1e7 ? (long)v : 0;
  }
  return 1;
}

const char *openseek_rotulo_compart(const OpenSeekCompart *c) {
  if (!c || !c->id[0]) return "";
  if (!strcmp(c->tipo, "user_adjusted")) return "Miniaturas compartilhadas, ajustadas por você";
  if (c->verificado && !c->aproximado) return "Miniaturas compartilhadas, verificadas";
  if (!strcmp(c->tipo, "same_length")) return "Miniaturas compartilhadas de outra versão (aproximadas)";
  if (!strcmp(c->tipo, "speed_adjusted")) return "Miniaturas compartilhadas, ajustadas à velocidade (aproximadas)";
  return "Miniaturas compartilhadas de outra versão (aproximadas; pode ser preciso ajustar)";
}

long openseek_offset_apos(double b, long deslocaMs) {
  // t_cache = a * t_convidado + b; miniaturas MAIS TARDE no convidado = b menor.
  double nb = b - (double)deslocaMs;
  if (nb < -21600000.0) nb = -21600000.0;
  if (nb > 21600000.0) nb = 21600000.0;
  return (long)(nb < 0 ? nb - 0.5 : nb + 0.5);
}
