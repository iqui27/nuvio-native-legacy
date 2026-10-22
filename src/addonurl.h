// TAMANHO DA URL BASE DE UM ADDON — um numero so para o app inteiro (#201).
//
// A chave do addon vai embutida no CAMINHO da URL (addons.h), e ha addon que
// embute a configuracao INTEIRA: a do Comet e um base64 de ~830 caracteres, e a
// URL dele passa de 870. Enquanto cada modulo guardava a base num `char[600]`
// proprio, ela era cortada em silencio ja na leitura da conta, o Comet recebia
// uma configuracao pela metade e respondia o manifesto "❌ | Comet", com uma
// fonte so. Nada no log dizia que a URL tinha sido cortada.
//
// 2048 e nao 1024: a configuracao padrao do Comet ja ocupa 870, e cada servico
// de debrid a mais na mesma configuracao soma. 2048 da mais que o dobro do caso
// medido e ainda deixa <base> + caminho longe dos 4096 que rede.c aceita.
//
// QUEM GUARDA A BASE USA ESTE NUMERO. Quem monta um pedido (<base>/stream/...,
// <base>/catalog/...) usa NV_ADDON_PEDIDO_MAX e CONFERE o retorno do snprintf
// com NV_COUBE: pedido que nao coube nao e feito. URL cortada nao e uma URL
// pior, e OUTRA URL — e o addon responde a ela como se fosse valida.
//
// QUEM NAO USA, E POR QUE. Duas copias secundarias continuam em 600 porque
// alarga-las custaria memoria demais para o que rendem, e as duas tem a base
// inteira a mao por outro caminho:
//   - ColSource.base (colecoes.h), 8192 fontes: +12 MB. O que nao cabe fica
//     vazio e a leitura cai em addons_base_por_id(addonId);
//   - GCanal.base (guia.c), 3 x 900 canais: +3,9 MB. O que nao cabe fica vazio,
//     que e o caso "pergunta a fonte a todos os addons". Lembrete, fontecache,
//     spotlight e o diagnostico da Live TV recebem a base do canal e herdam
//     esse teto.
// Nas duas a regra e a mesma daqui: vazio sim, cortado nunca.
//
// NUNCA IMPRIMIR A BASE. Ela carrega a chave de debrid do dono; o log diz o
// nome do addon e o tamanho, e so.
#ifndef NV_ADDONURL_H
#define NV_ADDONURL_H

#define NV_ADDON_URL_MAX    2048
#define NV_ADDON_PEDIDO_MAX (NV_ADDON_URL_MAX + 1024)

// O snprintf que escreveu em `buf` coube inteiro? `w` e o retorno dele.
#define NV_COUBE(w, buf) ((w) >= 0 && (size_t)(w) < sizeof(buf))

#include <stdio.h>
#include <stddef.h>

// A PORTA DE ENTRADA. Toda URL de addon que chega (conta, arquivo, instalacao
// pela TV) passa por aqui antes de ser guardada. 1 = cabe. 0 = nao cabe, e a
// linha de log e a unica noticia que o dono tera disso: quem chama PULA o
// addon. `static inline` para os testes que compilam um modulo so nao
// precisarem de mais um arquivo na linha de comando.
static inline int nv_addon_url_cabe(const char *nome, size_t len) {
  if (len < NV_ADDON_URL_MAX) return 1;
  printf("[addons] %s: URL de %lu caracteres nao cabe (maximo %d): addon ignorado\n",
         nome && *nome ? nome : "Addon", (unsigned long)len, NV_ADDON_URL_MAX - 1);
  fflush(stdout);
  return 0;
}

// O PEDIDO (<base>/stream/..., /catalog/..., /manifest.json) COUBE NO BUFFER?
// `w` e o retorno do snprintf que o montou, `tam` o tamanho do buffer. Com a
// base limitada por NV_ADDON_URL_MAX e o buffer em NV_ADDON_PEDIDO_MAX isto nao
// deveria disparar; existe para que, se um id ou um caminho crescer, o pedido
// cortado NAO SAIA. Quem chama pula o pedido quando volta 0.
static inline int nv_addon_pedido_coube(const char *nome, int w, size_t tam) {
  if (w >= 0 && (size_t)w < tam) return 1;
  printf("[addons] %s: pedido de %d caracteres nao cabe (maximo %lu): pulado\n",
         nome && *nome ? nome : "Addon", w, (unsigned long)tam - 1);
  fflush(stdout);
  return 0;
}

// A QUERY DA URL DO ADDON VIAJA EM TODO PEDIDO (paridade com o Nuvio oficial).
//
// O Nuvio web (addonRepository.canonicalizeUrl, buildStreamUrl,
// buildSubtitlesUrl, buildManifestUrl) separa "<caminho>?<query>" da URL
// instalada e monta "<caminho>/stream/<tipo>/<id>.json?<query>": a query vai em
// TODO pedido. Este app jogava a query fora (baseNormalizada, #24), e um addon
// que guarda a configuracao nela (?apikey=..., ?config=...) recebia aqui um
// pedido sem configuracao que la funciona.
//
// Regra: a BASE guardada mantem a query; quem monta um pedido usa nv_addon_url,
// que poe o caminho ANTES da query. `fmt` e o caminho com a barra inicial
// ("/stream/%s/%s.json"). Devolve o tamanho que o pedido teria (como o
// snprintf), para conferir com nv_addon_pedido_coube/NV_COUBE; pedido que nao
// coube sai "" — nunca uma URL cortada.
#include <stdarg.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>

static inline int nv_addon_url(char *dst, size_t n, const char *base, const char *fmt, ...) {
  char caminho[NV_ADDON_PEDIDO_MAX];
  const char *q;
  int lb, wp, w;
  va_list ap;
  if (!base) base = "";
  q = strchr(base, '?');
  lb = (int)(q ? (size_t)(q - base) : strlen(base));
  while (lb > 0 && base[lb - 1] == '/') lb--;
  va_start(ap, fmt);
  wp = vsnprintf(caminho, sizeof caminho, fmt, ap);
  va_end(ap);
  if (wp < 0 || (size_t)wp >= sizeof caminho) { if (dst && n) dst[0] = 0; return wp < 0 ? -1 : lb + wp; }
  if (!dst || !n) return -1;
  w = snprintf(dst, n, "%.*s%s%s", lb, base, caminho, q ? q : "");
  if (w < 0 || (size_t)w >= n) dst[0] = 0;
  return w;
}

// O ID NO CAMINHO (/stream/<tipo>/<id>.json, /meta/..., /subtitles/...).
// O Nuvio web passa o id por encodeURIComponent (buildStreamUrl, buildMetaUrl,
// encodeSubtitleId); aqui ele ia cru. Id com espaco, "/", "#", "?", "%" ou
// acento (canal de IPTV, id que e uma URL) virava outro caminho ou um pedido
// invalido. Codifica o que nao e "pchar" do RFC 3986 e deixa o resto como
// esta — ":" inclusive, que e o que todo addon ja recebe deste app
// ("tt123:1:2") e o que o Nuvio web manda nas legendas. Id comum sai igual.
// Devolve 1 se coube; 0 deixa `dst` vazio.
static inline int nv_addon_id(char *dst, size_t n, const char *id) {
  static const char HEX[] = "0123456789ABCDEF";
  const unsigned char *p = (const unsigned char *)(id ? id : "");
  size_t k = 0;
  if (!dst || !n) return 0;
  for (; *p; p++) {
    int livre = isalnum(*p) || (*p && strchr("-._~:@!$&'()*+,;=", *p));
    if (*p >= 0x80) livre = 0;
    if (k + (livre ? 1 : 3) >= n) { dst[0] = 0; return 0; }
    if (livre) dst[k++] = (char)*p;
    else { dst[k++] = '%'; dst[k++] = HEX[*p >> 4]; dst[k++] = HEX[*p & 15]; }
  }
  dst[k] = 0;
  return 1;
}

// A NORMALIZACAO DA URL INSTALADA, uma so para o app (addons, guia, colecoes).
// A mesma do Nuvio oficial (canonicalizeUrl + normalizeAddonUrl):
//   - espaco em volta fora; "stremio://" vira "https://";
//   - "/manifest.json" no fim do CAMINHO sai, sem caixa ("/Manifest.json");
//   - barra final do caminho sai ("<base>//stream" e outra URL para alguns);
//   - a QUERY FICA (ver nv_addon_url); "?" sozinho nao e query.
// O que nao cabe em `tam` sai "" (vazio sim, cortado nunca).
static inline void nv_addon_base(const char *url, char *dst, size_t tam) {
  const char *ini, *fim, *q;
  size_t k;
  int w;
  if (!dst || !tam) return;
  dst[0] = 0;
  if (!url) return;
  ini = url;
  while (*ini && isspace((unsigned char)*ini)) ini++;
  fim = ini + strlen(ini);
  while (fim > ini && isspace((unsigned char)fim[-1])) fim--;
  q = memchr(ini, '?', (size_t)(fim - ini));
  if (!q) q = fim;
  w = !strncasecmp(ini, "stremio://", 10)
      ? snprintf(dst, tam, "https://%.*s", (int)(q - ini - 10), ini + 10)
      : snprintf(dst, tam, "%.*s", (int)(q - ini), ini);
  if (w < 0 || (size_t)w >= tam) { dst[0] = 0; return; }
  k = (size_t)w;
  while (k && dst[k - 1] == '/') dst[--k] = 0;
  if (k > 14 && !strcasecmp(dst + k - 14, "/manifest.json")) { k -= 14; dst[k] = 0; }
  while (k && dst[k - 1] == '/') dst[--k] = 0;
  if (fim - q > 1) {
    w = snprintf(dst + k, tam - k, "%.*s", (int)(fim - q), q);
    if (w < 0 || (size_t)w >= tam - k) dst[0] = 0;
  }
}

// URL DE ADDON GRANDE DEMAIS PARA OS BUFFERS FIXOS (#203).
//
// 19 pessoas tinham addon com URL de 2 a 8 KB (AIOLists, StremioLabAR) que era
// IGNORADO. Subir NV_ADDON_URL_MAX para 8 KB custaria dezenas de MB em vetores
// de fileira/canal/lista, entao a URL grande fica numa tabela no heap (um
// malloc por addon grande) e o resto do app guarda um APELIDO curto:
//   <esquema>://<host>/~nv<16 hex do hash da URL>
// O host fica de proposito: log por host, agrupamento e rede_url_publica
// continuam funcionando. O apelido e DETERMINISTICO (hash da URL), entao uma
// cache em disco que o guarde continua valendo na proxima abertura.
// Quem fala com a rede (rede.c, no unico ponto do curl OPT_URL) EXPANDE o
// apelido de volta para a URL inteira — normalizada, com o caminho do pedido
// no lugar certo e a query do addon no fim. Quem envia a lista para a conta
// (sync.c) troca o apelido pela URL EXATA que chegou.
//
// A tabela e global e fica fora do `static` por um simbolo fraco: este header
// entra em varias unidades de traducao e todas precisam ver a mesma tabela.
#include <pthread.h>
#include <stdlib.h>
#include <stdint.h>

#define NV_LONGA_MAX 128
typedef struct {
  pthread_mutex_t m; int n;
  char *bruto[NV_LONGA_MAX], *norm[NV_LONGA_MAX], apelido[NV_LONGA_MAX][96];
} NvLonga;
__attribute__((weak)) NvLonga nv_longa = { PTHREAD_MUTEX_INITIALIZER, 0, {0}, {0}, {{0}} };

// Registra `bruto` (qualquer tamanho) e escreve o apelido em `dst`.
// 0 = nao deu (sem esquema, tabela cheia, colisao, `dst` pequeno).
static inline int nv_longa_registrar(const char *bruto, char *dst, size_t n) {
  size_t lb = strlen(bruto), host;
  const char *ini, *e;
  char *norm, apelido[96];
  uint64_t h = 1469598103934665603ULL;
  int i, ok = 0;
  size_t k;
  norm = (char *)malloc(lb + 16);
  if (!norm) return 0;
  nv_addon_base(bruto, norm, lb + 16);
  ini = strstr(norm, "://");
  if (!ini) { free(norm); return 0; }
  e = ini + 3; e += strcspn(e, "/?#"); host = (size_t)(e - norm);
  for (k = 0; k < lb; k++) { h ^= (unsigned char)bruto[k]; h *= 1099511628211ULL; }
  if (host > 60 || snprintf(apelido, sizeof apelido, "%.*s/~nv%016llx", (int)host, norm,
                            (unsigned long long)h) >= (int)sizeof apelido ||
      strlen(apelido) >= n) { free(norm); return 0; }
  pthread_mutex_lock(&nv_longa.m);
  for (i = 0; i < nv_longa.n; i++)
    if (!strcmp(nv_longa.apelido[i], apelido)) { ok = !strcmp(nv_longa.bruto[i], bruto); break; }
  if (i == nv_longa.n && i < NV_LONGA_MAX) {
    nv_longa.bruto[i] = (char *)malloc(lb + 1);
    if (nv_longa.bruto[i]) {
      memcpy(nv_longa.bruto[i], bruto, lb + 1);
      nv_longa.norm[i] = norm; norm = NULL;
      snprintf(nv_longa.apelido[i], sizeof nv_longa.apelido[i], "%s", apelido);
      nv_longa.n++; ok = 1;
    }
  }
  pthread_mutex_unlock(&nv_longa.m);
  free(norm);
  if (ok) snprintf(dst, n, "%s", apelido);
  return ok;
}

// A porta de entrada de toda URL de addon: cabe em NV_ADDON_URL_MAX = fica
// como esta; senao vira apelido. 0 = nao deu (quem chama PULA e loga).
static inline int nv_addon_url_guardar(const char *nome, const char *url, char *dst, size_t n) {
  if (strlen(url) < NV_ADDON_URL_MAX && strlen(url) < n) { memcpy(dst, url, strlen(url) + 1); return 1; }
  if (nv_longa_registrar(url, dst, n)) {
    printf("[addons] %s: URL de %lu caracteres guardada fora dos buffers fixos\n",
           nome && *nome ? nome : "Addon", (unsigned long)strlen(url));
    fflush(stdout);
    return 1;
  }
  printf("[addons] %s: URL de %lu caracteres nao coube na tabela de URLs grandes: addon ignorado\n",
         nome && *nome ? nome : "Addon", (unsigned long)strlen(url));
  fflush(stdout);
  return 0;
}

// Apelido -> URL EXATA que chegou. NULL = nao e apelido conhecido. O ponteiro
// vale ate o fim do processo (a tabela nunca libera).
static inline const char *nv_longa_bruto(const char *u) {
  const char *r = NULL;
  int i;
  if (!u || !strstr(u, "/~nv")) return NULL;
  pthread_mutex_lock(&nv_longa.m);
  for (i = 0; i < nv_longa.n; i++)
    if (!strcmp(nv_longa.apelido[i], u)) { r = nv_longa.bruto[i]; break; }
  pthread_mutex_unlock(&nv_longa.m);
  return r;
}

// Pedido (<apelido>/stream/...) -> URL inteira, em memoria nova (free). NULL =
// `u` nao comeca por um apelido conhecido: use `u` como esta.
static inline char *nv_longa_expandir(const char *u) {
  const char *e, *p, *resto, *qr, *qn;
  char *out = NULL, *norm = NULL;
  size_t la, lp, lq, lr, lrp;
  int i, k;
  if (!u || !(e = strstr(u, "://"))) return NULL;
  e += 3; e += strcspn(e, "/?#");
  if (strncmp(e, "/~nv", 4)) return NULL;
  for (k = 0; k < 16; k++) if (!isxdigit((unsigned char)e[4 + k])) return NULL;
  p = e + 20; la = (size_t)(p - u);
  pthread_mutex_lock(&nv_longa.m);
  for (i = 0; i < nv_longa.n; i++)
    if (strlen(nv_longa.apelido[i]) == la && !strncmp(nv_longa.apelido[i], u, la)) {
      norm = nv_longa.norm[i]; break;
    }
  pthread_mutex_unlock(&nv_longa.m);   // `norm` nunca e liberado: seguro fora da trava
  if (!norm) return NULL;
  resto = p;
  qn = strchr(norm, '?');
  lp = qn ? (size_t)(qn - norm) : strlen(norm);
  lq = qn ? strlen(qn + 1) : 0;
  qr = strchr(resto, '?');
  lrp = qr ? (size_t)(qr - resto) : strlen(resto);
  lr = strlen(resto);
  out = (char *)malloc(lp + lr + lq + 4);
  if (!out) return NULL;
  memcpy(out, norm, lp); memcpy(out + lp, resto, lrp); k = (int)(lp + lrp);
  if (lq) {
    out[k++] = '?'; memcpy(out + k, qn + 1, lq); k += (int)lq;
    if (qr && qr[1]) { out[k++] = '&'; memcpy(out + k, qr + 1, strlen(qr + 1)); k += (int)strlen(qr + 1); }
  } else if (qr) { memcpy(out + k, qr, strlen(qr)); k += (int)strlen(qr); }
  out[k] = 0;
  return out;
}

#endif
