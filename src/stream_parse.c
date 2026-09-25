#include "streams.h"
#include "badges.h"
#include "js.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static int contem(const char *s, const char *termo) {
  for (; *s; s++) if (!strncasecmp(s, termo, strlen(termo))) return 1;
  return 0;
}

// Token isolado reconhece .DV., DV/HDR e [DV], mas nunca DVD/DVDRip.
static int token(const char *s, const char *t) {
  size_t n = strlen(t);
  const char *p;
  for (p = s; *p; p++)
    if ((p == s || !isalnum((unsigned char)p[-1])) &&
        !strncasecmp(p, t, n) && !isalnum((unsigned char)p[n])) return 1;
  return 0;
}

// CABECALHOS EXIGIDOS PELO ADDON: behaviorHints.proxyHeaders.request.
//
// MEDIDO em 17/09 contra o addon de um relato: o CDN devolve 403 sem
// Referer/Origin/User-Agent e 200 com eles. O parser entrava em behaviorHints
// so para pegar bingeGroup, entao esses cabecalhos eram descartados e QUALQUER
// addon que dependa de Referer ficava sem tocar, nos dois alvos.
//
// A MAO, e nao com js_texto: os nomes das chaves aqui sao escolhidos pelo
// ADDON (sao nomes de cabecalho HTTP), entao nao ha lista de chaves conhecidas
// para procurar — e preciso varrer os pares que existirem. So profundidade 1
// de "request", que e onde a convencao do Stremio poe os cabecalhos.
//
// Saida no formato que rede.h aceita: uma linha "Nome: valor" por cabecalho.
static void lerProxyHeaders(const char *bh, const char *fim, char *dst, unsigned tam) {
  const char *ph, *rq, *q;
  unsigned u = 0;
  dst[0] = 0;
  ph = strstr(bh, "\"proxyHeaders\"");
  if (!ph || ph >= fim) return;
  rq = strstr(ph, "\"request\"");
  if (!rq || rq >= fim) return;
  q = strchr(rq, '{');
  if (!q || q >= fim) return;
  q++;
  // Varre pares "nome":"valor" ate a chave que fecha o objeto. Valor de
  // cabecalho e string, entao um '{' aqui e coisa que nao se esperava: encerra
  // em vez de tentar adivinhar.
  while (q < fim && *q && *q != '}' && *q != '{') {
    char nome[96], valor[320];
    unsigned k;
    const char *f;
    while (q < fim && *q && *q != '"' && *q != '}') q++;
    if (q >= fim || *q != '"') break;
    q++;
    f = q;
    while (f < fim && *f && *f != '"') f++;
    if (f >= fim || *f != '"') break;
    k = (unsigned)(f - q);
    if (k >= sizeof nome) k = sizeof nome - 1;
    memcpy(nome, q, k); nome[k] = 0;
    q = f + 1;
    while (q < fim && *q && *q != ':') q++;
    if (q >= fim || *q != ':') break;
    q++;
    while (q < fim && (*q == ' ' || *q == '\t')) q++;
    if (q >= fim || *q != '"') break;   // valor nao-string: nao e cabecalho
    q++;
    f = q;
    while (f < fim && *f && *f != '"') f++;
    if (f >= fim || *f != '"') break;
    k = (unsigned)(f - q);
    if (k >= sizeof valor) k = sizeof valor - 1;
    memcpy(valor, q, k); valor[k] = 0;
    q = f + 1;
    if (nome[0] && valor[0]) {
      int esc = snprintf(dst + u, tam - u, "%s%s: %s", u ? "\n" : "", nome, valor);
      if (esc < 0 || (unsigned)esc >= tam - u) { dst[u] = 0; break; }
      u += (unsigned)esc;
    }
    while (q < fim && (*q == ' ' || *q == '\t' || *q == '\n' || *q == '\r')) q++;
    if (q < fim && *q == ',') q++;
  }
}

// "FORA DE CACHE" DITO PELO ADDON. As marcas sao as que os addons mandam de
// verdade: "⏳" e o que o AIOStreams poe no nome de toda fonte nao cacheada
// (registros 1136, 2191, 2501: "⏳ UHD", "⏳ FHD"; a cacheada vem com "⚡"), o
// Torrentio escreve "[TB download]" / "[RD download]" (a cacheada e "[TB+]"),
// e outros usam "⬇" ou a palavra "uncached". "download]" e nao "download"
// solto: "WEB-DL"/"Download" aparecem em titulo de fonte cacheada.
int stream_texto_fora_de_cache(const char *t) {
  if (!t) return 0;
  return strstr(t, "\xe2\x8f\xb3") != NULL          // U+23F3 ⏳
      || strstr(t, "\xe2\xac\x87") != NULL          // U+2B07 ⬇
      || contem(t, " download]") || contem(t, "uncached");
}

int stream_extrair(const char *json, const char *provedor, Stream **saida) {
  const char *p, *fim;
  int n = 0, cap = 0;
  Stream *v = NULL;
  *saida = NULL;
  if (!json) return 0;
  p = js_array(json, json + strlen(json), "streams");
  while (p && *p == '{') {
    Stream s = {0};
    char titulo[2048] = "", texto[5000];
    fim = js_fim(p);
    if (!fim || fim <= p) break;
    js_texto(p, fim, "url", s.url, sizeof s.url);
    if (!s.url[0]) js_texto(p, fim, "externalUrl", s.url, sizeof s.url);
    s.fileIdx = -1;
    // Torrent puro: sem url, com infoHash (no topo ou dentro de clientResolve,
    // como o AIOStreams manda). A url nasce depois, no debrid.
    if (!s.url[0] || strncmp(s.url, "http", 4)) {
      const char *cr = strstr(p, "\"clientResolve\"");
      s.url[0] = 0;
      if (!js_texto(p, fim, "infoHash", s.infoHash, sizeof s.infoHash) && cr && cr < fim)
        js_texto(cr, fim, "infoHash", s.infoHash, sizeof s.infoHash);
      s.fileIdx = (int)js_num(p, fim, "fileIdx", -1);
    }
    // Nao tocar URL cortada; sem url e sem hash nao ha o que tocar.
    if ((s.infoHash[0] || !strncmp(s.url, "http", 4)) &&
        strlen(s.url) < sizeof s.url - 1) {
      js_texto(p, fim, "name", s.rotulo, sizeof s.rotulo);
      js_texto(p, fim, "description", s.descricao, sizeof s.descricao);
      js_texto(p, fim, "title", titulo, sizeof titulo);
      js_texto(p, fim, "filename", s.arquivo, sizeof s.arquivo);
      // behaviorHints.bingeGroup — ANCORADO NO OBJETO, e nao procurado solto.
      //
      // Duas coisas separadas, e as duas custam uma linha:
      //
      // 1. `bh < fim` E O QUE IMPORTA DE VERDADE. strstr varre ate o fim do
      //    documento, nao ate o fim DESTE stream: sem a guarda, uma fonte que
      //    nao manda behaviorHints herdaria o bingeGroup da fonte SEGUINTE do
      //    array — e herdaria calada, com o sintoma sendo o app lembrar da
      //    fonte errada no episodio seguinte. Mesma guarda que o
      //    "clientResolve" logo acima ja usa, e pelo mesmo motivo.
      // 2. js_texto_raiz_em le so as chaves de PROFUNDIDADE 1 de
      //    behaviorHints. behaviorHints tem objeto dentro dele
      //    (proxyHeaders.request/response, com nomes de cabecalho que o addon
      //    escolhe), e a regra da casa para chave aninhada de Stremio esta em
      //    js.h: procurar solto pega a primeira ocorrencia, que nem sempre e a
      //    que se quer. Nas respostas que deu para inspecionar aqui o js_texto
      //    solto daria o mesmo resultado; ancorar e seguro de graca.
      { const char *bh = strstr(p, "\"behaviorHints\"");
        if (bh && bh < fim) {
          js_texto_raiz_em(bh, fim, "bingeGroup", s.bingeGroup, sizeof s.bingeGroup);
          lerProxyHeaders(bh, fim, s.cabecalhos, sizeof s.cabecalhos);
        } }
      if (!s.descricao[0]) snprintf(s.descricao, sizeof s.descricao, "%s", titulo);
      if (!s.rotulo[0]) snprintf(s.rotulo, sizeof s.rotulo, "%s", provedor);
      snprintf(s.provedor, sizeof s.provedor, "%s", provedor);
      snprintf(texto, sizeof texto, "%s %s %s %s", s.rotulo, s.descricao, titulo, s.arquivo);
      s.altura = contem(texto, "4320") || token(texto, "8k") ? 4320 :
                 contem(texto, "2160") || token(texto, "4k") || token(texto, "uhd") ? 2160 :
                 contem(texto, "1440") ? 1440 : contem(texto, "1080") ? 1080 :
                 contem(texto, "720") ? 720 : contem(texto, "480") ? 480 : 0;
      s.dolbyVision = token(texto, "dv") || token(texto, "dovi") ||
                      contem(texto, "dolby vision") || contem(texto, "dolbyvision");
      s.dolbyAtmos = token(texto, "atmos");
      s.badges = badges_detectar(texto);
      s.mp4 = token(texto, "mp4") || contem(s.url, ".mp4");
      // "av1" tambem casa "av1.0.0" (nivel/perfil, comum em nome de release);
      // token() basta porque nao ha AV1 escrito colado a outra palavra nos
      // nomes que os addons mandam (ao contrario de "dv", que precisa do
      // cuidado de token() com DVDRip — o mesmo cuidado ja se aplica aqui).
      s.av1 = token(texto, "av1") || contem(texto, "av01");
      s.foraCache = stream_texto_fora_de_cache(texto);
      double bytes = js_num(p, fim, "videoSize", 0);
      if (bytes > 0) s.tamanhoMB = (long)(bytes / (1024.0 * 1024.0));
      else {
        const char *u = strstr(texto, " GB");
        double escala = 1024;
        if (!u) { u = strstr(texto, " MB"); escala = 1; }
        if (u) {
          const char *ini = u;
          while (ini > texto && (isdigit((unsigned char)ini[-1]) || ini[-1] == '.')) ini--;
          if (ini < u) s.tamanhoMB = (long)(atof(ini) * escala);
        }
      }
      if (n == cap) {
        int nova = cap ? cap * 2 : 32;
        Stream *tmp = realloc(v, (size_t)nova * sizeof *tmp);
        if (!tmp) { free(v); return -1; }
        v = tmp; cap = nova;
      }
      v[n++] = s;
    }
    p = js_prox(fim);
  }
  *saida = v;
  return n;
}
