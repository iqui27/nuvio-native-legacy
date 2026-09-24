#include "rede.h"
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <dlfcn.h>
#include <time.h>
#ifdef NV_VIDAA
#include <ctype.h>   // isalnum, para vidaaUrlEncode (percent-encode da url do proxy)
#endif

/* Controle local da requisicao corrente. O estado nunca e compartilhado
 * entre sondagens: cada fio recebe seu teto e seu cancel token. */
static _Thread_local long redeLimiteLocal;
static _Thread_local volatile int *redeCancelLocal;
static _Thread_local int redeLimitouLocal;
static _Thread_local int redeCancelouLocal;
static _Thread_local long redeBytesLocal;

static long redeLimiteAtual(void) {
  if (redeLimiteLocal > 0) return redeLimiteLocal;
  return rede_teto > 0 ? rede_teto : 0;
}

#ifdef __EMSCRIPTEN__
// ---------------------------------------------------------------- EMSCRIPTEN
// Caminho de rede do alvo Tizen (WASM).
//
// POR QUE NAO DA PARA REAPROVEITAR O DE BAIXO: o de baixo faz dlopen da libcurl
// do aparelho. Nao existe dlopen em WebAssembly, e nao existe libcurl para
// abrir. O emcc compila aquele codigo sem reclamar — dlfcn.h tem stubs — e o
// unico sintoma e a tela de login dizendo "sem fio para falar com o servidor".
//
// POR QUE XHR SINCRONO E NAO emscripten_fetch: rede_url_final precisa do
// ENDERECO FINAL depois dos redirecionamentos, para distinguir um link de
// debrid que leva ao arquivo de um que leva a "downloading.mp4". emscripten_fetch
// nao expoe isso; XHR expoe, em responseURL. Um mecanismo so, com cabecalhos,
// status, Range e endereco final, e mais simples que dois.
//
// XHR sincrono BLOQUEIA o fio que chama, que e exatamente o contrato de
// rede_baixar ("BLOQUEIA — chamar de um fio proprio"). Nos fios de trabalho ele
// e plenamente suportado; no fio principal o navegador ainda o atende, com um
// aviso no console.
//
// CORS: NO NAVEGADOR COMUM, ISTO NAO FUNCIONA E NAO E DEFEITO. Os hosts de
// addon, o TMDB e o Supabase nao mandam Access-Control-Allow-Origin para uma
// origem file://. Dentro do .wgt o Tizen dispensa a checagem para os endereços
// declarados em <access origin="*"> no config.xml — e por isso que o fork em
// JavaScript funciona na TV. Para testar no Chrome do desktop e preciso subir o
// navegador com --disable-web-security ou por um proxy.
//
// timeout NAO E HONRADO: XHR sincrono proibe xhr.timeout (lanca
// InvalidAccessError). O parametro `segundos` e aceito e ignorado; quem corta a
// espera e o navegador.

#include <emscripten.h>
#include "fio1.h"   // so tem corpo com NV_UM_FIO; nv_http troca de implementacao abaixo

// Faz a requisicao e devolve um buffer de malloc com o corpo (com um NUL extra
// no fim, para quem trata como texto). Escreve o tamanho em *tam e o status
// HTTP em *status. Devolve 0 se a requisicao nem saiu.
//
// `cabs` vem como uma unica string com uma linha "Nome: valor" por cabecalho,
// separadas por \n, porque atravessar um vetor de ponteiros por EM_JS custaria
// mais codigo do que juntar e separar.
//
// DUAS IMPLEMENTACOES, MESMA ASSINATURA C, nada abaixo desta regiao muda:
// pedir2() chama nv_http() sem saber qual das duas esta ativa.
//
//   mt/ (pthreads de verdade): XHR SINCRONO. O fio que chama fica bloqueado
//   de verdade, que e o contrato de rede_baixar ("chamar de um fio proprio")
//   — o worker pthread trava, os outros workers e o fio principal seguem.
//
//   st/ (--um-fio, NV_UM_FIO): nao ha OUTRO fio de verdade para travar sem
//   travar TUDO — so existe este fio de JS. Por isso aqui e fetch()
//   ASSINCRONO, mas com um cuidado que nao e obvio: um EM_ASYNC_JS comum
//   (await fetch(...)) suspenderia, via ASYNCIFY, a PILHA INTEIRA desta
//   fibra e devolveria o controle ao LACO DE EVENTOS DO NAVEGADOR ate a
//   promessa resolver — exatamente o mesmo mecanismo de nv_ceder_quadro em
//   main.c. Isso tiraria o controle do ESCALONADOR DE FIBRAS (fio1.c) pelo
//   tempo inteiro da requisicao: nenhuma OUTRA fibra (nem o proprio desenho,
//   que so roda quando o fio principal volta a chamar fio1_rodar a cada
//   quadro) rodaria nesse meio tempo — o app inteiro pareceria travado numa
//   unica requisicao de rede, o oposto do que pthreads de verdade dão.
//
//   A saida e nao usar EM_ASYNC_JS aqui: o fetch() e disparado por uma
//   EM_JS comum (sincrona, so entrega o pedido e devolve um id na hora), e a
//   fibra chamadora fica num LACO POLLING fio1_ceder()+nv_http_pronto_assinc,
//   que devolve o controle ao ESCALONADOR (nao ao navegador) a cada volta —
//   as demais fibras (e o desenho, via fio1_rodar no laco de quadro) correm
//   normalmente enquanto o fetch() ainda esta em voo no proprio navegador.
#ifdef NV_UM_FIO

EM_JS(int, nv_http_iniciar_assinc, (const char *metodo, const char *url,
                                    const char *cabs, const char *corpo), {
  if (!Module.nvFioReqs) { Module.nvFioReqs = {}; Module.nvFioProxId = 1; }
  var id = Module.nvFioProxId++;
  var m = UTF8ToString(metodo), u = UTF8ToString(url);
  var cab = {};
  if (cabs) {
    UTF8ToString(cabs).split("\n").forEach(function (linha) {
      var i = linha.indexOf(":");
      if (i <= 0) return;
      cab[linha.slice(0, i).trim()] = linha.slice(i + 1).trim();
    });
  }
  // redirect:'follow' (padrao do fetch) para manter o mesmo comportamento do
  // XHR sincrono: rede_url_final quer o ENDERECO FINAL depois de
  // redirecionamentos (res.url), nao o pedido.
  var init = { method: m, headers: cab, redirect: "follow" };
  if (corpo) init.body = UTF8ToString(corpo);
  var reg = { pronto: false, erro: false, status: 0, url: "", etag: "", bytes: null };
  Module.nvFioReqs[id] = reg;
  fetch(u, init).then(function (res) {
    reg.status = res.status;
    // Pelo /v1/proxy do worker (so no VIDAA), res.url e o endereco do PROXY;
    // o destino real depois dos redirecionamentos vem neste cabecalho, que o
    // worker expoe por CORS. Sem proxy ele nao existe e vale res.url.
    reg.url = res.headers.get("x-nuvio-url-final") || res.url || "";
    // getResponseHeader (XHR) e headers.get (fetch) tem a mesma regra: null
    // quando o servidor nao mandou (ou o CORS escondeu) — string vazia e a
    // resposta certa para quem chama nos dois casos.
    reg.etag = res.headers.get("etag") || "";
    return res.arrayBuffer();
  }).then(function (buf) {
    // arrayBuffer(), nao responseText+charset=x-user-defined como no XHR
    // sincrono: aqui os bytes vem binarios de verdade, sem o truque de
    // tunelar byte a byte por um charset de 1 byte.
    reg.bytes = new Uint8Array(buf);
    reg.pronto = true;
  }).catch(function () {
    // Mesma regra do XHR sincrono: falha de rede (CORS, DNS, offline) vira
    // "requisicao nem saiu" — nv_http_colher_assinc devolve 0.
    reg.erro = true;
    reg.pronto = true;
  });
  return id;
});

// Nao-bloqueante DE VERDADE (nao e EM_ASYNC_JS): so olha uma flag. E o que
// permite ao chamador ceder para outras fibras entre uma chamada e outra.
EM_JS(int, nv_http_pronto_assinc, (int id), {
  var reg = Module.nvFioReqs && Module.nvFioReqs[id];
  return (reg && reg.pronto) ? 1 : 0;
});

EM_JS(char *, nv_http_colher_assinc, (int id, int *tam, int *status,
                                      char *urlFinal, int urlFinalTam,
                                      char *etag, int etagTam), {
  var reg = Module.nvFioReqs && Module.nvFioReqs[id];
  if (Module.nvFioReqs) delete Module.nvFioReqs[id];
  if (status) HEAP32[status >> 2] = 0;
  if (tam) HEAP32[tam >> 2] = 0;
  if (!reg || reg.erro || !reg.bytes) return 0;
  if (status) HEAP32[status >> 2] = reg.status;
  if (urlFinal && urlFinalTam > 0) stringToUTF8(reg.url, urlFinal, urlFinalTam);
  if (etag && etagTam > 0) stringToUTF8(reg.etag, etag, etagTam);
  var n = reg.bytes.length;
  var p = _malloc(n + 1);
  if (!p) return 0;
  HEAPU8.set(reg.bytes, p);
  HEAPU8[p + n] = 0;
  if (tam) HEAP32[tam >> 2] = n;
  return p;
});

static char *nv_http(const char *metodo, const char *url, const char *cabs,
                     const char *corpo, int *tam, int *status,
                     char *urlFinal, int urlFinalTam,
                     char *etag, int etagTam) {
  int id = nv_http_iniciar_assinc(metodo, url, cabs, corpo);
  // fio1_ceder() dentro de uma fibra troca para outra fibra pronta (ou para
  // o desenho, via o escalonador); chamado da RAIZ (fio principal fora de
  // fibra — nao deveria acontecer aqui, rede_baixar exige "fio proprio", mas
  // sem essa garantia em tempo de compilacao) ele gira o escalonador em vez
  // de travar o navegador parado num while(1) puro.
  while (!nv_http_pronto_assinc(id)) fio1_ceder();
  return nv_http_colher_assinc(id, tam, status, urlFinal, urlFinalTam, etag, etagTam);
}

#else /* !NV_UM_FIO: mt/, pthreads de verdade, XHR sincrono de sempre */

EM_JS(char *, nv_http, (const char *metodo, const char *url, const char *cabs,
                        const char *corpo, int *tam, int *status,
                        char *urlFinal, int urlFinalTam,
                        char *etag, int etagTam), {
  var m = UTF8ToString(metodo), u = UTF8ToString(url);
  var xhr = new XMLHttpRequest();
  try {
    xhr.open(m, u, false);   // false = sincrono
  } catch (e) { return 0; }
  // Le a resposta como texto BRUTO, byte a byte. responseType='arraybuffer' é
  // proibido em XHR sincrono no fio principal, e a mesma funcao serve os dois
  // lados; este truque de charset entrega os bytes intactos em qualquer um.
  try { xhr.overrideMimeType("text/plain; charset=x-user-defined"); } catch (e) {}
  if (cabs) {
    UTF8ToString(cabs).split("\n").forEach(function (linha) {
      var i = linha.indexOf(":");
      if (i <= 0) return;
      try {
        xhr.setRequestHeader(linha.slice(0, i).trim(), linha.slice(i + 1).trim());
      } catch (e) {}
    });
  }
  try {
    xhr.send(corpo ? UTF8ToString(corpo) : null);
  } catch (e) { return 0; }

  if (status) HEAP32[status >> 2] = xhr.status;
  if (urlFinal && urlFinalTam > 0) {
    // x-nuvio-url-final: so o /v1/proxy do worker manda (VIDAA). Pelo proxy,
    // responseURL e o endereco do proprio proxy e nao diz para onde a fonte
    // redirecionou; nos outros alvos o cabecalho nunca vem e nada muda.
    var fim = null;
    try { fim = xhr.getResponseHeader("x-nuvio-url-final"); } catch (e) {}
    stringToUTF8(fim || xhr.responseURL || "", urlFinal, urlFinalTam);
  }
  // Cabecalho de RESPOSTA, para rede_baixar_etag. getResponseHeader devolve
  // null quando o servidor nao mandou o cabecalho (ou quando o CORS o esconde);
  // nos dois casos a string vazia e a resposta certa para quem chama.
  if (etag && etagTam > 0) {
    stringToUTF8(xhr.getResponseHeader("etag") || "", etag, etagTam);
  }

  var s = xhr.responseText || "";
  var n = s.length;
  var p = _malloc(n + 1);
  if (!p) return 0;
  for (var i = 0; i < n; i++) HEAPU8[p + i] = s.charCodeAt(i) & 0xff;
  HEAPU8[p + n] = 0;
  if (tam) HEAP32[tam >> 2] = n;
  return p;
});

#endif /* NV_UM_FIO */

_Thread_local long rede_teto = 0;

void rede_preparar(void) { }   // nao ha biblioteca para carregar

// Junta o vetor de cabecalhos numa string com uma linha por cabecalho.
static char *juntarCabs(const char *const *cab, const char *extra) {
  size_t total = 1;
  int k;
  char *s;
  if (!cab && !extra) return NULL;
  for (k = 0; cab && cab[k]; k++) total += strlen(cab[k]) + 1;
  if (extra) total += strlen(extra) + 1;
  s = (char *)malloc(total);
  if (!s) return NULL;
  s[0] = 0;
  if (extra) { strcat(s, extra); strcat(s, "\n"); }
  for (k = 0; cab && cab[k]; k++) { strcat(s, cab[k]); strcat(s, "\n"); }
  return s;
}

// Ouvinte unico dos 401 — ver rede_avisar_401 no cabecalho.
static void (*aviso401)(const char *url);
void rede_avisar_401(void (*f)(const char *url)) { aviso401 = f; }

#ifdef NV_VIDAA
// VAZIO E O PADRAO, como em todo outro arquivo que usa NV_REC_URL (ver
// recomenda.c, xtream.c, avisos.c, noticias.c, tex_cache.c): tools/env.sh so
// emite o -D quando o servico de recomendacoes esta configurado, e uma build
// sem ele nao pode falhar a compilar por causa disto.
#ifndef NV_REC_URL
#define NV_REC_URL ""
#endif

// ---------------------------------------------------------------- VIDAA/PROXY
//
// A pagina da TV VIDAA e servida em https pelo proprio worker de
// recomendacoes (rotaTv em servidor/recomendacoes/src/index.js). O Chromium
// da Hisense bloqueia toda requisicao http:// feita de dentro dela (conteudo
// misto), e alguns https tambem falham por o destino nao mandar CORS. Os dois
// casos chegam aqui como o MESMO sintoma: nv_http devolve NULL com http==0
// (nem open() nem send() lancam; o navegador so recusa em silencio). O worker
// tem uma rota generica para isto, /v1/proxy (servidor/recomendacoes/src/proxy.js):
// builda a mesma url como query e devolve com CORS.
//
// NUNCA PROXY:
//   - NV_REC_URL: seria pedir ao proxy que buscasse a si mesmo.
//   - SUPABASE_URL: rotaProxy so aceita GET sem cabecalho de autenticacao
//     (ver proxy.js) — mandar login ali so gastaria uma volta a mais para
//     falhar do mesmo jeito, e o pior caso seria logar a url errada.
//   - VIDEO: o worker rejeita content-type video/audio de proposito (ver
//     tipoAceito em proxy.js). Um HEAD/probe de stream por ali sempre
//     voltaria 415, nunca 200 — so custaria a rodada. streams.c troca
//     rede_url_final por rede_url_final_vidaa exatamente para nao precisar
//     desta distincao aqui: aquela funcao NUNCA tenta o proxy (ver embaixo)
//     e devolve "desconhecido" em vez de "morta" quando o bloqueio acontece.
static int vidaaNuncaProxiar(const char *url) {
  if (NV_REC_URL[0] && !strncmp(url, NV_REC_URL, strlen(NV_REC_URL))) return 1;
#ifdef NV_SUPABASE_URL
  if (NV_SUPABASE_URL[0] && !strncmp(url, NV_SUPABASE_URL, strlen(NV_SUPABASE_URL))) return 1;
#endif
  return 0;
}

// URL QUE "PARECE VIDEO", pelos mesmos sinais que o classificador do painel
// Xtream usa do lado do worker (xtream.js: classificar) — caminho de
// streaming ou extensao de midia. Nao precisa ser exaustivo: o objetivo aqui
// e so evitar mandar ao proxy algo que ELE MESMO vai rejeitar por tipo,
// nunca decidir o que e ou nao video para reproducao (isso e do addon).
static int vidaaPareceVideo(const char *url) {
  static const char *caminhos[] = { "/live/", "/movie/", "/series/", "/timeshift/", "/hls/", "/streaming/" };
  static const char *extensoes[] = { ".m3u8", ".m3u", ".ts", ".mp4", ".mkv", ".avi" };
  size_t i;
  for (i = 0; i < sizeof caminhos / sizeof caminhos[0]; i++)
    if (strstr(url, caminhos[i])) return 1;
  for (i = 0; i < sizeof extensoes / sizeof extensoes[0]; i++) {
    const char *p = strstr(url, extensoes[i]);
    // So conta perto do fim (antes de `?`/`#` ou do fim da string): um
    // ".mp4" no MEIO de um caminho de CDN nao e a extensao do recurso.
    if (p) { const char *f = p + strlen(extensoes[i]);
      if (*f == 0 || *f == '?' || *f == '#') return 1; }
  }
  return 0;
}

// Host[:porta] de uma URL http(s), para a tabela abaixo e para o log. So
// string — o ramo Emscripten nunca tem libcurl para fazer isto por API.
static void vidaaHostDe(const char *url, char *dst, size_t tam) {
  const char *p = strstr(url, "://"), *fim;
  size_t n;
  dst[0] = 0;
  if (!p) return;
  p += 3;
  for (fim = p; *fim && *fim != '/' && *fim != '?' && *fim != '#'; fim++) {}
  n = (size_t)(fim - p);
  if (n >= tam) n = tam - 1;
  memcpy(dst, p, n);
  dst[n] = 0;
}

// HOSTS QUE JA SE PROVARAM SO ACESSIVEIS VIA PROXY, para as chamadas
// SEGUINTES ao mesmo host nao pagarem de novo a rodada perdida (https direto
// ate o status 0, so entao o proxy). 64 e teto generoso: um titulo nao fala
// com mais que uma duzia de addons/CDNs distintos. Descarte por indice mais
// antigo (round-robin), nao por LRU de verdade — simples e suficiente para
// uma tabela deste tamanho, que so existe para ECONOMIZAR uma rodada, nunca
// para decidir corretude (o pior caso de um descarte errado e pagar de novo a
// rodada perdida, nao um proxy indevido).
#define VIDAA_HOSTS_MAX 64
static char vidaaHosts[VIDAA_HOSTS_MAX][128];
static int vidaaHostsN;
static int vidaaHostsProx;
static pthread_mutex_t vidaaHostsTrava = PTHREAD_MUTEX_INITIALIZER;

static int vidaaHostViaProxy(const char *host) {
  int i, achou = 0;
  if (!host[0]) return 0;
  pthread_mutex_lock(&vidaaHostsTrava);
  for (i = 0; i < vidaaHostsN; i++)
    if (!strcmp(vidaaHosts[i], host)) { achou = 1; break; }
  pthread_mutex_unlock(&vidaaHostsTrava);
  return achou;
}

// Registra o host e loga UMA VEZ: a propria ausencia na tabela e a guarda —
// quem ja estava nao entra de novo, nem loga de novo.
static void vidaaHostLembrar(const char *host) {
  int i, ja = 0;
  if (!host[0]) return;
  pthread_mutex_lock(&vidaaHostsTrava);
  for (i = 0; i < vidaaHostsN; i++)
    if (!strcmp(vidaaHosts[i], host)) { ja = 1; break; }
  if (!ja) {
    int slot;
    if (vidaaHostsN < VIDAA_HOSTS_MAX) slot = vidaaHostsN++;
    else { slot = vidaaHostsProx; vidaaHostsProx = (vidaaHostsProx + 1) % VIDAA_HOSTS_MAX; }
    snprintf(vidaaHosts[slot], sizeof vidaaHosts[slot], "%s", host);
  }
  pthread_mutex_unlock(&vidaaHostsTrava);
  if (!ja) printf("[rede] via proxy: %s\n", host);
}

// Percent-encode minimo (RFC 3986, so os "unreserved" ficam de fora) — o
// bastante para uma url inteira caber dentro de ?u=. malloc: o chamador libera.
static char *vidaaUrlEncode(const char *s) {
  static const char hex[] = "0123456789ABCDEF";
  size_t n = strlen(s), i, o = 0;
  char *out = (char *)malloc(n * 3 + 1);
  if (!out) return NULL;
  for (i = 0; i < n; i++) {
    unsigned char c = (unsigned char)s[i];
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') out[o++] = (char)c;
    else { out[o++] = '%'; out[o++] = hex[c >> 4]; out[o++] = hex[c & 0xF]; }
  }
  out[o] = 0;
  return out;
}

// "NV_REC_URL/v1/proxy?u=<url-encoded>", num buffer malloc'ado (o chamador
// libera). NULL so por falta de memoria.
static char *vidaaUrlProxy(const char *url) {
  char *enc = vidaaUrlEncode(url), *out;
  size_t tam;
  if (!enc) return NULL;
  tam = strlen(NV_REC_URL) + strlen("/v1/proxy?u=") + strlen(enc) + 1;
  out = (char *)malloc(tam);
  if (out) snprintf(out, tam, "%s/v1/proxy?u=%s", NV_REC_URL, enc);
  free(enc);
  return out;
}
#endif /* NV_VIDAA */

#ifdef NV_VIDAA
// Cabecalho AUTHORIZATION presente (em `cab` ou no `extraCab` fixo de quem
// chamou)? Se sim, esta chamada NUNCA vai para o /v1/proxy generico, nem
// forcada (http://) nem por retry (https:// com status 0): rotaProxy so
// repassa `accept` e `if-none-match` (ver proxy.js) e DESCARTA qualquer
// outro cabecalho, entao proxiar um pedido autenticado devolveria um 401 por
// FALTA DE CREDENCIAL, nao pela credencial ser invalida — e
// rede_avisar_401 nao sabe diferenciar os dois. NV_REC_URL/Supabase ja saem
// pela lista de destinos proibidos; isto cobre o Trakt e qualquer outro host
// https que precise de Authorization.
static int vidaaTemAutorizacao(const char *const *cab, const char *extraCab) {
  int k;
  if (extraCab && !strncasecmp(extraCab, "Authorization:", 14)) return 1;
  for (k = 0; cab && cab[k]; k++)
    if (!strncasecmp(cab[k], "Authorization:", 14)) return 1;
  return 0;
}

static int vidaaPodeProxiar(const char *url, const char *const *cab, const char *extraCab) {
  return !vidaaNuncaProxiar(url) && !vidaaPareceVideo(url) &&
         !vidaaTemAutorizacao(cab, extraCab);
}
#endif

static char *pedir2(const char *metodo, const char *url, const char *const *cab,
                    const char *extraCab, const char *corpo,
                    long *tam, int *status, char *etag, unsigned tamEtag) {
  char *cabs, *corpoResp;
  int n = 0, http = 0;
#ifdef NV_VIDAA
  char *urlProxiada = NULL;
  const char *urlEfetiva = url;
#endif
  if (etag && tamEtag) etag[0] = 0;
  if (status) *status = 0;
  if (!url || !*url) return NULL;
#ifdef NV_VIDAA
  // http:// NUNCA SAI DIRETO: a pagina da TV e https, e o navegador bloqueia
  // conteudo misto antes mesmo de tentar a conexao. HOST JA CONHECIDO como
  // "so acessivel via proxy" (vidaaHostLembrar, mais abaixo) tambem entra
  // direto pela mesma rota, para nao pagar de novo a rodada perdida.
  if (vidaaPodeProxiar(url, cab, extraCab)) {
    if (!strncmp(url, "http://", 7)) {
      urlProxiada = vidaaUrlProxy(url);
      if (urlProxiada) urlEfetiva = urlProxiada;
    } else {
      char host[128];
      vidaaHostDe(url, host, sizeof host);
      if (vidaaHostViaProxy(host)) {
        urlProxiada = vidaaUrlProxy(url);
        if (urlProxiada) urlEfetiva = urlProxiada;
      }
    }
  }
  cabs = juntarCabs(cab, extraCab);
  corpoResp = nv_http(metodo, urlEfetiva, cabs, corpo, &n, &http, NULL, 0,
                      etag, (int)tamEtag);
  free(cabs);
  // HTTPS QUE FALHOU DIRETO (http==0: nem chegou a ter status, o mesmo
  // sintoma do bloqueio de conteudo misto) -> tenta 1x via proxy. So quando
  // AINDA NAO tinha ido por ele: o http:// e o host ja conhecido acima ja
  // usaram a rota certa de cara, e tentar de novo so repetiria a mesma falha.
  if (!corpoResp && http == 0 && !urlProxiada && vidaaPodeProxiar(url, cab, extraCab)) {
    char *urlP = vidaaUrlProxy(url);
    if (urlP) {
      char *cabs2 = juntarCabs(cab, extraCab);
      int n2 = 0, http2 = 0;
      char fim2[512];
      char *r2;
      fim2[0] = 0;
      r2 = nv_http(metodo, urlP, cabs2, corpo, &n2, &http2, fim2, (int)sizeof fim2, etag, (int)tamEtag);
      free(cabs2);
      // SO VALE SE FOI O PROXY QUE BUSCOU. rotaProxy manda x-nuvio-url-final
      // (o destino real, "http...") em TODA resposta que veio do destino, 404
      // inclusive; sem o cabecalho nv_http devolve o endereco do proprio
      // worker, e a resposta e dele: erro do proxy (403/415/429/502/504) ou
      // rota inexistente. MEDIDO em 24/09: o worker publicado ainda nao tem
      // /v1/proxy e responde 401 {"erro":"nao autenticado"} a qualquer url.
      // Este ramo aceitava esse 401 como se fosse do destino: logava "via
      // proxy: images.metahub.space" (o proxy nao buscou nada), anotava o
      // host, e dai em diante todo pedido a ele ia so ao proxy, sem tentar o
      // direto — 22 a 99 "HTTP 401 em images.metahub.space" por abertura da
      // home. Um host que falhou UMA vez por acaso (rede) ficava preso ao
      // proxy quebrado do mesmo jeito. Agora so anota quando o proxy entregou,
      // e o erro do worker sai com o nome certo no log.
      if (r2 && !strncmp(fim2, "http", 4) &&
          !(NV_REC_URL[0] && !strncmp(fim2, NV_REC_URL, strlen(NV_REC_URL)))) {
        char host[128];
        corpoResp = r2; n = n2; http = http2;
        vidaaHostDe(url, host, sizeof host);
        vidaaHostLembrar(host);
      } else if (r2) {
        char host[128];
        vidaaHostDe(url, host, sizeof host);
        printf("[rede] proxy nao buscou %s (HTTP %d do worker)\n", host, http2);
        free(r2);
      }
      free(urlP);
    }
  }
  free(urlProxiada);
#else
  cabs = juntarCabs(cab, extraCab);
  corpoResp = nv_http(metodo, url, cabs, corpo, &n, &http, NULL, 0,
                      etag, (int)tamEtag);
  free(cabs);
#endif
  if (status) *status = http;
  if (http == 401 && aviso401) aviso401(url);
  if (!corpoResp) { char seg[120];
    printf("[rede] falhou em %s\n", rede_url_publica(url, seg, sizeof seg));
    return NULL; }

  if (redeCancelLocal && *redeCancelLocal) {
    redeCancelouLocal = 1;
    free(corpoResp);
    return NULL;
  }

  // TETO: aqui ele so CORTA, nao interrompe.
  //
  // Na libcurl o teto abortava a conexao de dentro do recebedor, entao um
  // servidor que ignora o Range parava de mandar. XHR sincrono nao tem esse
  // ponto de corte: quando a chamada volta, o corpo INTEIRO ja veio. Um
  // servidor que ignore o Range num arquivo de 20 GB baixaria os 20 GB antes
  // desta linha rodar. Fica registrado como limitacao real deste alvo, nao como
  // detalhe: se rede_baixar_trecho comecar a travar o app, e isto.
  if (rede_teto > 0 && (long)n > rede_teto) {
    n = (int)rede_teto;
    corpoResp[n] = 0;
  }
  if (redeLimiteAtual() > 0 && (long)n > redeLimiteAtual()) {
    n = (int)redeLimiteAtual();
    corpoResp[n] = 0;
    redeLimitouLocal = 1;
  }
  redeBytesLocal = n;

  // Mesma regra do caminho da libcurl: 4xx vira NULL para quem NAO pediu
  // status, e corpo devolvido para quem pediu — o corpo do erro do PostgREST e
  // a unica pista de qual funcao ou tabela faltou.
  if (http >= 400 && !status) {
    free(corpoResp);
    { char seg[120];
      printf("[rede] HTTP %d em %s\n", http, rede_url_publica(url, seg, sizeof seg)); }
    fflush(stdout);
    return NULL;
  }
  if (tam) *tam = n;
  return corpoResp;
}

static char *pedir(const char *metodo, const char *url, const char *const *cab,
                   const char *extraCab, const char *corpo,
                   long *tam, int *status) {
  return pedir2(metodo, url, cab, extraCab, corpo, tam, status, NULL, 0);
}

char *rede_baixar_etag(const char *url, int segundos, const char *const *cab,
                       int *status, char *etag, unsigned tamEtag) {
  (void)segundos;
  return pedir2("GET", url, cab, NULL, NULL, NULL, status, etag, tamEtag);
}

char *rede_baixar(const char *url, int segundos) {
  (void)segundos;
  return pedir("GET", url, NULL, NULL, NULL, NULL, NULL);
}

char *rede_baixar_bin(const char *url, int segundos, long *tam) {
  (void)segundos;
  return pedir("GET", url, NULL, NULL, NULL, tam, NULL);
}

char *rede_baixar_com(const char *url, int segundos, const char *const *cab) {
  (void)segundos;
  return pedir("GET", url, cab, NULL, NULL, NULL, NULL);
}

// Ver rede.h. Sem `status` para pedir2, que entao ja devolve NULL em >= 400
// e loga so o host (rede_url_publica) — a url do painel vai no corpo e nunca
// passa por aqui em log.
char *rede_postar_bin(const char *url, int segundos, const char *corpo, long *tam) {
  (void)segundos;
  return pedir("POST", url, NULL, "Content-Type: text/plain", corpo ? corpo : "", tam, NULL);
}

char *rede_baixar_st(const char *url, int segundos, const char *const *cab,
                     int *status) {
  (void)segundos;
  return pedir("GET", url, cab, NULL, NULL, NULL, status);
}

char *rede_baixar_trecho(const char *url, int segundos, long ini, long fim,
                         long *tam) {
  char faixa[80];
  const char *cab[2];
  char *r;
  (void)segundos;
  snprintf(faixa, sizeof faixa, "Range: bytes=%ld-%ld", ini, fim);
  cab[0] = faixa; cab[1] = NULL;
  rede_teto = fim - ini + 1;
  r = pedir("GET", url, cab, NULL, NULL, tam, NULL);
  rede_teto = 0;
  return r;
}

char *rede_postar(const char *url, int segundos, const char *const *cab,
                  const char *corpo) {
  return rede_postar_st(url, segundos, cab, corpo, NULL);
}

// APAGAR e um verbo de verdade aqui, e nao um POST com corpo vazio: o Trakt
// remove um item da barra de retomada por DELETE /sync/playback/:id, e responde
// 404 a um POST na mesma URL. Ver trakt_playback_remover.
char *rede_apagar(const char *url, int segundos, const char *const *cab,
                  int *status) {
  char *r;
  (void)segundos;
  r = pedir("DELETE", url, cab, NULL, NULL, NULL, status);
  // 204 sem corpo e a resposta NORMAL de um DELETE aceito. Devolver NULL ali
  // faria o chamador ler sucesso como falha de transporte.
  if (!r && status && *status > 0) return strdup("");
  return r;
}

char *rede_postar_st(const char *url, int segundos, const char *const *cab,
                     const char *corpo, int *status) {
  int temCt = 0, k;
  char *r;
  (void)segundos;
  // JSON e o padrao (Supabase, Trakt); quem manda o proprio Content-Type
  // (Real-Debrid quer form-urlencoded) nao recebe um segundo.
  for (k = 0; cab && cab[k]; k++)
    if (!strncasecmp(cab[k], "Content-Type:", 13)) temCt = 1;
  r = pedir("POST", url, cab, temCt ? NULL : "Content-Type: application/json",
            corpo ? corpo : "", NULL, status);
  // Falha de TRANSPORTE continua sendo NULL; corpo de 4xx e devolvido. Um POST
  // que respondeu com corpo vazio devolve "" e nao NULL, como no outro caminho.
  if (!r && status && *status > 0) return strdup("");
  return r;
}

int rede_url_final(const char *url, int segundos, char *dst, unsigned tam) {
  const char *cab[2];
  char *corpo;
  int n = 0, http = 0;
  char *cabs;
  (void)segundos;
  if (!url || !*url || !dst || tam == 0) return 0;
  dst[0] = 0;
  // Um pedaco minusculo em vez de HEAD, pelo mesmo motivo do outro caminho:
  // servidores de debrid respondem HEAD com 405 ou mentem no redirecionamento,
  // mas honram Range.
  cab[0] = "Range: bytes=0-64"; cab[1] = NULL;
  cabs = juntarCabs(cab, NULL);
  corpo = nv_http("GET", url, cabs, NULL, &n, &http, dst, (int)tam, NULL, 0);
  free(cabs);
  free(corpo);
  return dst[0] ? 1 : 0;
}

#ifdef NV_VIDAA
// Ver rede.h. NUNCA passa pelo proxy (o proprio pedir2 evita isto por
// vidaaPareceVideo — url de video sempre volta 415 do worker), entao esta
// chamada e SEMPRE direta: um bloqueio de conteudo misto ou de CORS aqui e
// ROTINEIRO, nao a excecao, e e exatamente o que -1 comunica ao chamador.
int rede_url_final_vidaa(const char *url, int segundos, char *dst, unsigned tam) {
  const char *cab[2];
  char *corpo, *cabs;
  int n = 0, http = 0;
  (void)segundos;
  if (!url || !*url || !dst || tam == 0) return -1;
  dst[0] = 0;
  cab[0] = "Range: bytes=0-64"; cab[1] = NULL;
  cabs = juntarCabs(cab, NULL);
  corpo = nv_http("GET", url, cabs, NULL, &n, &http, dst, (int)tam, NULL, 0);
  free(cabs);
  if (!corpo) return -1;     // xhr nem completou (open/send lancou): desconhecido
  free(corpo);
  if (dst[0]) return 1;      // achou endereco final: resolveu
  return http > 0 ? 0 : -1;  // completou sem endereco: so e "morto" se veio status de verdade
}
#endif

#else

// Constantes da libcurl escritas a mao: nao ha curl.h no SDK do aparelho, e
// puxar o header inteiro so por meia duzia de numeros nao se paga. Os valores
// sao estaveis desde sempre (CURLOPTTYPE_OBJECTPOINT = 10000 etc).
#define OPT_URL             10002
#define OPT_WRITEFUNCTION   20011
#define OPT_WRITEDATA       10001
#define OPT_TIMEOUT            13
#define OPT_FOLLOWLOCATION     52
#define OPT_SSL_VERIFYPEER     64
#define OPT_SSL_VERIFYHOST     81
#define OPT_USERAGENT       10018
#define OPT_ACCEPT_ENCODING 10102
#define OPT_NOSIGNAL          99
#define OPT_HTTPHEADER      10023
#define OPT_NOBODY             44
#define OPT_RANGE           10007
#define INFO_URL_FINAL    1048577
#define OPT_POSTFIELDS      10015
#define OPT_POST               47
// CURLOPT_CUSTOMREQUEST. Numerico como os outros: esta camada abre a libcurl
// por dlopen e nunca inclui curl.h, entao as constantes sao transcritas.
#define OPT_CUSTOMREQUEST   10036
// CURLINFO_RESPONSE_CODE = CURLINFO_LONG (0x200000) + 2.
#define INFO_RESPONSE_CODE   2097154
// Cabecalhos de RESPOSTA. So rede_baixar_etag os pede; ver a nota la.
#define OPT_HEADERFUNCTION  20079
#define OPT_HEADERDATA      10029
// CURLOPT_MAXCONNECTS: tamanho do cache de conexoes do handle. Ver pegarHandle.
#define OPT_MAXCONNECTS        71

// Ouvinte unico dos 401 — ver rede_avisar_401 no cabecalho. (O ramo
// Emscripten tem a sua propria definicao, porque os dois lados do #ifdef
// compilam separado.)
static void (*aviso401)(const char *url);
void rede_avisar_401(void (*f)(const char *url)) { aviso401 = f; }

static void *(*curl_init)(void);
static int   (*curl_setopt)(void *, int, ...);
static int   (*curl_perform)(void *);
static void  (*curl_cleanup)(void *);
static int   (*curl_global)(long);
static void *(*slist_append)(void *, const char *);
static void  (*slist_free)(void *);
static int   (*curl_getinfo)(void *, int, ...);
static void  (*curl_reset)(void *);
static int    pronto;

// UM HANDLE POR FIO, REUSADO, e nao um novo por pedido.
//
// Cada `curl_easy_init` + `perform` + `cleanup` abria conexao NOVA: DNS + TCP +
// handshake TLS numa CPU de 2019, para cada imagem e cada chamada de API.
// MEDIDO na LG com o curl da propria TV, mesma URL, conexao nova x reusada:
// metahub (poster) 190 -> 27 ms, cinemeta 212 -> 40 ms, image.tmdb.org
// 875 -> 176 ms. So o handshake custa 150-200 ms (trakt, cinemeta, metahub) e
// ~500 ms no tmdb — numa imagem de 20 KB, 80% do tempo era abrir a conexao.
//
// O cache de conexoes e de DNS da libcurl vive DENTRO do handle facil; jogar o
// handle fora a cada pedido jogava os dois fora. Um handle por fio (chave
// pthread, destruido quando o fio morre) mantem as conexoes abertas entre
// pedidos do mesmo fio sem partilhar nada entre fios — `curl_easy_*` nao pode
// ser usado por dois fios ao mesmo tempo, e assim nunca e. `curl_easy_reset`
// limpa as opcoes do pedido anterior e PRESERVA conexoes, DNS e sessoes TLS.
//
// Bonus: `curl_easy_cleanup` era onde o OpenSSL 1.0 morria (ver
// prepararOpenSSL); agora ele so roda no fim do fio.
static pthread_key_t handleChave;
static pthread_once_t handleUma = PTHREAD_ONCE_INIT;
static void handleSoltar(void *c) { if (c && curl_cleanup) curl_cleanup(c); }
static void handleCriarChave(void) { pthread_key_create(&handleChave, handleSoltar); }
// QUANTAS CONEXOES CADA FIO GUARDA. O padrao da libcurl e 5, e um fio de
// arte fala com mais hosts que isso numa volta so: metahub, api e image do
// TMDB, api e media do Trakt, tv.apple.com e mzstatic, Kitsu, fanart.tv. Com 5
// o LRU fecha justamente a conexao que vai ser usada no proximo titulo, e o
// reuso vira conexao nova de novo. 10 cobre os hosts de arte de uma volta;
// sao 4 fios de arte na LG (40 sockets ociosos no pior caso, alguns KB de
// estado TLS cada), e o servidor fecha o que ficar parado.
#define REDE_CONEXOES_POR_FIO 10L
static void *pegarHandle(void) {
  void *c;
  if (!curl_reset) return curl_init();     // libcurl sem reset: como antes
  pthread_once(&handleUma, handleCriarChave);
  c = pthread_getspecific(handleChave);
  if (c) curl_reset(c);
  else {
    c = curl_init();
    if (c) pthread_setspecific(handleChave, c);
  }
  // Depois do reset: curl_easy_reset volta MAXCONNECTS ao padrao.
  if (c) curl_setopt(c, OPT_MAXCONNECTS, REDE_CONEXOES_POR_FIO);
  return c;
}
// Devolve o handle ao fio. So destroi de verdade quando nao ha reuso.
static void soltarHandle(void *c) { if (!curl_reset) curl_cleanup(c); }

typedef struct { char *p; size_t n; } Balde;

static char *rede_baixar_interno(const char *url, int segundos, long *tam,
                                 const char *const *cab);
static char *rede_baixar_interno2(const char *url, int segundos, long *tam,
                                  const char *const *cab, int *status,
                                  char *etag, unsigned tamEtag);

// Onde o ETag da resposta e anotado, quando alguem o pediu.
typedef struct { char *dst; unsigned tam; } CacaCab;

// A libcurl entrega UMA linha de cabecalho por chamada, com o CRLF no fim. So
// o ETag interessa; devolver menos bytes do que recebeu abortaria a
// transferencia, entao o retorno e sempre o tamanho inteiro.
static size_t receberCab(void *dados, size_t tam, size_t qtd, void *u) {
  CacaCab *c = (CacaCab *)u;
  const char *s = (const char *)dados;
  size_t bytes = tam * qtd, n;
  if (c && c->dst && c->tam > 1 && bytes > 5 && !strncasecmp(s, "etag:", 5)) {
    s += 5; bytes -= 5;
    while (bytes && (*s == ' ' || *s == '\t')) { s++; bytes--; }
    while (bytes && (s[bytes - 1] == '\r' || s[bytes - 1] == '\n' ||
                     s[bytes - 1] == ' ')) bytes--;
    n = bytes < (size_t)c->tam - 1 ? bytes : (size_t)c->tam - 1;
    memcpy(c->dst, s, n);
    c->dst[n] = 0;
  }
  return tam * qtd;
}

// Teto opcional de bytes para a proxima transferencia; 0 = sem teto. Existe
// porque servidor que IGNORA o cabecalho Range responde 200 com o arquivo
// inteiro, e nesse caso o cabecalho pedido nao limita nada.
_Thread_local long rede_teto = 0;

static size_t receber(void *dados, size_t tam, size_t qtd, void *u) {
  Balde *b = (Balde *)u;
  size_t bytes = tam * qtd;
  long limite = redeLimiteAtual();
  char *novo;
  if (redeCancelLocal && *redeCancelLocal) {
    redeCancelouLocal = 1;
    return 0;
  }
  if (limite > 0 && b->n >= (size_t)limite) {
    redeLimitouLocal = 1;
    return 0;
  }
  if (limite > 0 && b->n + bytes > (size_t)limite) {
    bytes = (size_t)limite - b->n;
    redeLimitouLocal = 1;
  }
  novo = realloc(b->p, b->n + bytes + 1);
  if (!novo) return 0;              // devolver 0 aborta a transferencia
  b->p = novo;
  memcpy(b->p + b->n, dados, bytes);
  b->n += bytes;
  b->p[b->n] = 0;
  return bytes;
}

// CARREGAMENTO DA LIBCURL, UMA VEZ SO E COM TRAVA.
//
// `curl_global_init` NAO e seguro entre fios — e a propria libcurl documenta
// isso. Isto aqui era uma bandeira simples, e enquanto so a descoberta e dois
// fios de decode chamavam, a corrida quase nunca acontecia. Ao acrescentar
// QUATRO fios de rede para as artes, todos partindo no arranque, ela passou a
// acontecer: dois fios entram com `pronto == 0`, os dois fazem dlopen e os dois
// chamam curl_global_init ao mesmo tempo. O estado global fica corrompido e
// TODO download passa a falhar — catalogos, addons e artes de uma vez, que foi
// exatamente o que o dono viu depois do ultimo deploy.
//
// A trava e estatica e sem inicializacao dinamica de proposito: ela precisa
// existir ANTES do primeiro fio, e um PTHREAD_MUTEX_INITIALIZER garante isso
// sem depender de ninguem chamar nada primeiro.
static pthread_mutex_t abrirTrava = PTHREAD_MUTEX_INITIALIZER;

// ---------------------------------------------------------------- OPENSSL 1.0
//
// O OpenSSL 1.0 NAO E SEGURO ENTRE FIOS SOZINHO, e este app o usa de uma duzia
// deles. Quem usa a biblioteca precisa instalar dois calos — uma funcao de
// trava e uma de identidade de fio — e a documentacao da libcurl diz isso com
// todas as letras. Este app nunca instalou.
//
// MEDIDO NA C9, e o rastro nao deixa duvida:
//   sk_is_sorted <- lh_delete <- RAND_poll <- ERR_remove_thread_state
//     <- libcurl (curl_easy_cleanup) <- rede_baixar_interno2
//   SIGSEGV, SEGV_MAPERR
// ERR_remove_thread_state percorre uma tabela hash GLOBAL do OpenSSL para
// limpar o estado de erro do fio, e a libcurl a chama em todo curl_easy_cleanup.
// Com varios fios fechando handles ao mesmo tempo, a tabela e corrompida.
//
// APARECE QUANDO HA MUITO HTTPS AO MESMO TEMPO, e por isso passou tanto tempo
// escondido: com o cache de catalogo quente o arranque faz poucas conexoes. Foi
// preciso arrancar SEM cache — todos os catalogos, o Trakt, as colecoes e as
// artes de uma vez — para o app morrer em segundos.
//
// A partir do OpenSSL 1.1 a biblioteca se tranca sozinha e CRYPTO_num_locks nem
// existe: por isso a ausencia do simbolo nao e erro, e so significa "nao
// precisa". Mesma coisa no Mac, onde a libcurl usa outra pilha de TLS.
static pthread_mutex_t *sslTravas;
static int nSslTravas;

static void sslTravar(int modo, int n, const char *arq, int linha) {
  (void)arq; (void)linha;
  if (!sslTravas || n < 0 || n >= nSslTravas) return;
  if (modo & 1)  pthread_mutex_lock(&sslTravas[n]);   /* 1 = CRYPTO_LOCK */
  else           pthread_mutex_unlock(&sslTravas[n]);
}

static unsigned long sslIdDoFio(void) {
  // O id precisa ser unico por fio e cabe num unsigned long. pthread_self e um
  // ponteiro nas plataformas deste app; o cast e a receita que a propria
  // documentacao do OpenSSL usa.
  return (unsigned long)(uintptr_t)pthread_self();
}

// Chamada com abrirTrava tomada, uma vez so.
static void prepararOpenSSL(void) {
  void *hc;
  int (*numLocks)(void);
  void (*setLocking)(void (*)(int, int, const char *, int));
  void (*setId)(unsigned long (*)(void));
  int i;
  static const char *nomes[] = { "libcrypto.so.1.0.0", "libcrypto.so.1.0.2",
                                 "libcrypto.so.10", "libcrypto.so.1",
                                 "libcrypto.so", NULL };
  if (sslTravas) return;
  hc = NULL;
  for (i = 0; nomes[i] && !hc; i++) hc = dlopen(nomes[i], RTLD_NOW);
  if (!hc) return;   // sem libcrypto separada: nada a fazer
  *(void **)(&numLocks)   = dlsym(hc, "CRYPTO_num_locks");
  *(void **)(&setLocking) = dlsym(hc, "CRYPTO_set_locking_callback");
  *(void **)(&setId)      = dlsym(hc, "CRYPTO_set_id_callback");
  if (!numLocks || !setLocking) return;   // 1.1+: ela se tranca sozinha
  nSslTravas = numLocks();
  if (nSslTravas < 1) return;
  sslTravas = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t) * (size_t)nSslTravas);
  if (!sslTravas) { nSslTravas = 0; return; }
  for (i = 0; i < nSslTravas; i++) pthread_mutex_init(&sslTravas[i], NULL);
  setLocking(sslTravar);
  if (setId) setId(sslIdDoFio);
  printf("[rede] OpenSSL 1.0 travado para %d regioes\n", nSslTravas);
  fflush(stdout);
}

static int abrir(void) {
  void *h;
  int r;
  // Leitura rapida sem trava para o caso comum (ja carregado). Escrita de int
  // e atomica nas arquiteturas em que este app roda; o que precisa de trava e a
  // SEQUENCIA dlopen+global_init, nao a bandeira.
  // So o RESULTADO (1 carregou, -1 falhou de vez) passa sem trava. Enquanto
  // um fio ainda esta no dlopen+global_init a bandeira vale -2, e quem chega
  // nesse instante ESPERA na trava em vez de voltar "sem rede": era o que
  // acontecia — sete fios de noticias partindo juntos, o primeiro carregava a
  // libcurl e os outros cinco falhavam na hora (medido em tests/agenda_shot).
  // No arranque do aparelho os quatro fios de arte partem do mesmo jeito.
  if (pronto == 1 || pronto == -1) return pronto > 0;
  pthread_mutex_lock(&abrirTrava);
  if (pronto == 1 || pronto == -1) { r = pronto > 0; pthread_mutex_unlock(&abrirTrava); return r; }
  pronto = -2;
  h = dlopen("libcurl.so.5", RTLD_NOW);
  if (!h) h = dlopen("libcurl.so.4", RTLD_NOW);
  if (!h) h = dlopen("libcurl.4.dylib", RTLD_NOW);   // Mac
  if (!h) h = dlopen("libcurl.dylib", RTLD_NOW);
  if (!h) { printf("[rede] sem libcurl: %s\n", dlerror());
            pronto = -1; pthread_mutex_unlock(&abrirTrava); return 0; }
  *(void **)(&curl_init)    = dlsym(h, "curl_easy_init");
  *(void **)(&curl_setopt)  = dlsym(h, "curl_easy_setopt");
  *(void **)(&curl_perform) = dlsym(h, "curl_easy_perform");
  *(void **)(&curl_cleanup) = dlsym(h, "curl_easy_cleanup");
  *(void **)(&curl_global)  = dlsym(h, "curl_global_init");
  *(void **)(&slist_append) = dlsym(h, "curl_slist_append");
  *(void **)(&slist_free)   = dlsym(h, "curl_slist_free_all");
  *(void **)(&curl_getinfo) = dlsym(h, "curl_easy_getinfo");
  // O REUSO NUNCA LIGOU ATE AQUI (23/09/2026). pegarHandle e soltarHandle
  // existem desde 5eb8bd2, mas este dlsym nao: `curl_reset` ficava NULL, e o
  // ramo "libcurl sem reset: como antes" criava e destruia um handle por
  // pedido — conexao nova, DNS e handshake TLS completo em TODA imagem e TODA
  // chamada de API. MEDIDO na C9 com o curl da TV (mesma libcurl 7.53.1):
  // api.themoviedb.org 650-740 ms com conexao nova x 190-230 ms reusada;
  // image.tmdb.org w1280 1,14-1,19 s x 0,32 s. O relatorio 1669 batia com o
  // numero de conexao nova: tmdb resolve 646 ms e download 1116 ms por arte.
  *(void **)(&curl_reset)   = dlsym(h, "curl_easy_reset");
  if (!curl_init || !curl_setopt || !curl_perform) {
    printf("[rede] libcurl sem os simbolos esperados\n");
    pronto = -1;
    pthread_mutex_unlock(&abrirTrava);
    return 0;
  }
  if (curl_global) curl_global(3 /* CURL_GLOBAL_DEFAULT */);
  // DEPOIS do global_init e ANTES de soltar a trava: a partir daqui qualquer fio
  // pode entrar em curl_easy_perform, e e la que o OpenSSL comeca a ser usado.
  prepararOpenSSL();
  pronto = 1;
  pthread_mutex_unlock(&abrirTrava);
  return 1;
}

void rede_preparar(void) { abrir(); }

char *rede_baixar_bin(const char *url, int segundos, long *tam) {
  return rede_baixar_interno(url, segundos, tam, NULL);
}

char *rede_baixar(const char *url, int segundos) {
  return rede_baixar_interno(url, segundos, NULL, NULL);
}

char *rede_baixar_trecho(const char *url, int segundos, long ini, long fim,
                         long *tam) {
  char faixa[80];
  const char *cab[2];
  // Range e um cabecalho comum, entao o caminho com cabecalhos ja existente
  // serve. Nao ha modo "binario com cabecalhos" separado porque
  // rede_baixar_interno ja devolve o tamanho quando `tam` e passado — quem
  // pediu texto e que ignora esse campo.
  snprintf(faixa, sizeof faixa, "Range: bytes=%ld-%ld", ini, fim);
  cab[0] = faixa; cab[1] = NULL;
  // TETO DE VERDADE, e nao so o cabecalho. MEDIDO: um servidor que ignora o
  // Range responde 200 com o arquivo INTEIRO — no teste vieram 31 MB para um
  // pedido de 2 MB. Sem o teto, ler o cabecalho de um filme de 20 GB baixaria
  // o filme. O corte e no recebedor, entao a conexao morre no limite em vez de
  // esperar o fim.
  rede_teto = fim - ini + 1;
  { char *r = rede_baixar_interno(url, segundos, tam, cab);
    rede_teto = 0;
    return r; }
}

char *rede_baixar_com(const char *url, int segundos, const char *const *cab) {
  return rede_baixar_interno(url, segundos, NULL, cab);
}

char *rede_baixar_st(const char *url, int segundos, const char *const *cab,
                     int *status) {
  return rede_baixar_interno2(url, segundos, NULL, cab, status, NULL, 0);
}

char *rede_baixar_etag(const char *url, int segundos, const char *const *cab,
                       int *status, char *etag, unsigned tamEtag) {
  return rede_baixar_interno2(url, segundos, NULL, cab, status, etag, tamEtag);
}

static char *rede_baixar_interno(const char *url, int segundos, long *tam,
                                 const char *const *cab) {
  return rede_baixar_interno2(url, segundos, tam, cab, NULL, NULL, 0);
}

static char *rede_baixar_interno2(const char *url, int segundos, long *tam,
                                  const char *const *cab, int *status,
                                  char *etag, unsigned tamEtag) {
  Balde b = { NULL, 0 };
  CacaCab caca;
  void *c, *lista = NULL;
  int r;
  caca.dst = (etag && tamEtag > 1) ? etag : NULL;
  caca.tam = tamEtag;
  if (etag && tamEtag) etag[0] = 0;
  if (status) *status = 0;
  if (!url || !*url || !abrir()) return NULL;
  c = pegarHandle();
  if (!c) return NULL;
  curl_setopt(c, OPT_URL, url);
  // SO QUANDO ALGUEM PEDIU. O handle e reusado por fio (pegarHandle) e
  // curl_easy_reset limpa as opcoes entre pedidos, entao deixar o recebedor
  // instalado aqui nao respinga no pedido seguinte do mesmo fio — mas tambem
  // nao ha por que pagar uma chamada por cabecalho em todo download de imagem.
  if (caca.dst) {
    curl_setopt(c, OPT_HEADERFUNCTION, receberCab);
    curl_setopt(c, OPT_HEADERDATA, &caca);
  }
  curl_setopt(c, OPT_WRITEFUNCTION, receber);
  curl_setopt(c, OPT_WRITEDATA, &b);
  curl_setopt(c, OPT_FOLLOWLOCATION, (long)1);
  curl_setopt(c, OPT_TIMEOUT, (long)(segundos > 0 ? segundos : 30));
  // O app roda com fios; sem NOSIGNAL a libcurl usa alarmes para o timeout de
  // DNS e pode derrubar o processo inteiro a partir de um fio secundario.
  curl_setopt(c, OPT_NOSIGNAL, (long)1);
  // Os addons sao servidos por hosts com cadeias que este aparelho de 2019 nao
  // conhece; o pacote de CAs dele e de fabrica e nao se atualiza. Verificar
  // recusaria fontes legitimas do dono. O conteudo e midia publica e a escolha
  // esta escrita aqui de proposito.
  curl_setopt(c, OPT_SSL_VERIFYPEER, (long)0);
  curl_setopt(c, OPT_SSL_VERIFYHOST, (long)0);
  curl_setopt(c, OPT_USERAGENT, "Nuvio/1.0 (webOS)");
  curl_setopt(c, OPT_ACCEPT_ENCODING, "");   // "" = todas as que a lib suporta
  if (cab && slist_append) {
    int k;
    for (k = 0; cab[k]; k++) lista = slist_append(lista, cab[k]);
    if (lista) curl_setopt(c, OPT_HTTPHEADER, lista);
  }
  r = curl_perform(c);
  // STATUS HTTP, e nao so o codigo de erro da libcurl. MEDIDO: numa navegacao
  // da home o log tinha 93 "decode falhou" e ZERO "[rede] falha" — ou seja, o
  // curl_easy_perform devolvia 0 (sucesso de TRANSPORTE) para respostas que nao
  // eram a imagem. Um 404, um 403 ou um 429 e uma transferencia bem-sucedida
  // para a libcurl; quem tem de olhar o status e quem chama.
  //
  // Sem isto o erro chegava sem nome ao tex_cache, que so via "corpo curto" e
  // devolvia 0 em silencio — e o unico sintoma era card sem arte. A assinatura
  // de imagem que ja existe la pega o 404 com pagina de erro GRANDE; esta
  // conferencia pega o resto, e diz QUAL foi o codigo.
  //
  // A EXCECAO e quem pediu `status`: para o Supabase, um 4xx nao e falha, e a
  // resposta. O corpo do 404 diz QUAL funcao ou tabela nao existe (PGRST202 /
  // PGRST205), e e essa string que distingue "servidor antigo" de "parametro
  // errado". Jogar o corpo fora aqui apagaria a unica pista.
  { long http = 0;
    if (curl_getinfo) curl_getinfo(c, INFO_RESPONSE_CODE, &http);
    if (status) *status = (int)http;
    if (http == 401 && aviso401) aviso401(url);
    if (!r && http >= 400 && !status) {
      if (lista) { curl_setopt(c, OPT_HTTPHEADER, (void *)0); if (slist_free) slist_free(lista); }
      soltarHandle(c);
      free(b.p);
      { char seg[120];
        printf("[rede] HTTP %ld em %s\n", http, rede_url_publica(url, seg, sizeof seg)); }
      fflush(stdout);
      return NULL;
    } }
  if (lista) { curl_setopt(c, OPT_HTTPHEADER, (void *)0); if (slist_free) slist_free(lista); }
  soltarHandle(c);
  // 23 = CURLE_WRITE_ERROR. Quando ha teto, ele e o resultado ESPERADO: o
  // recebedor devolve menos bytes de proposito para cortar a conexao assim que
  // enche. Nesse caso o que ja veio e exatamente o que se queria — tratar como
  // falha jogaria fora o cabecalho inteiro que acabamos de baixar.
  if (redeCancelouLocal) {
    free(b.p);
    return NULL;
  }
  if (r == 23 && redeLimiteAtual() > 0 && b.n > 0) r = 0;
  if (r != 0) { char seg[120]; free(b.p);
    printf("[rede] falha %d em %s\n", r, rede_url_publica(url, seg, sizeof seg));
    return NULL; }
  redeBytesLocal = (long)b.n;
  if (tam) *tam = (long)b.n;
  return b.p;
}

int rede_url_final(const char *url, int segundos, char *dst, unsigned tam) {
  Balde b = { NULL, 0 };
  void *c;
  char *fim = NULL;
  int r;
  if (!url || !*url || !abrir() || !curl_getinfo) return 0;
  c = pegarHandle();
  if (!c) return 0;
  curl_setopt(c, OPT_URL, url);
  curl_setopt(c, OPT_WRITEFUNCTION, receber);
  curl_setopt(c, OPT_WRITEDATA, &b);
  curl_setopt(c, OPT_FOLLOWLOCATION, (long)1);
  curl_setopt(c, OPT_TIMEOUT, (long)(segundos > 0 ? segundos : 20));
  curl_setopt(c, OPT_NOSIGNAL, (long)1);
  curl_setopt(c, OPT_SSL_VERIFYPEER, (long)0);
  curl_setopt(c, OPT_SSL_VERIFYHOST, (long)0);
  curl_setopt(c, OPT_USERAGENT, "Nuvio/1.0 (webOS)");
  // Um pedaco minusculo em vez de HEAD: varios servidores de debrid respondem
  // HEAD com 405 ou mentem no redirecionamento, mas honram Range.
  curl_setopt(c, OPT_RANGE, "0-64");
  r = curl_perform(c);
  if (!r) curl_getinfo(c, INFO_URL_FINAL, &fim);
  if (!r && fim) snprintf(dst, tam, "%s", fim);
  // DIZER POR QUE FALHOU. Quem chama (streams.c) so imprimia "N nao resolveu",
  // e "nao resolveu" cobre coisas muito diferentes: host que nao existe (6),
  // recusa de conexao (7), estouro de tempo (28), TLS (35, 60) e HTTP 4xx/5xx
  // do proprio servidor da fonte. Sem separar, todo relato de "nao toca" vira
  // adivinhacao — e foi exatamente onde este ficou parado.
  if (r || !fim) {
    long http = 0;
    char seg[120];
    curl_getinfo(c, INFO_RESPONSE_CODE, &http);
    printf("[rede] url final falhou: curl %d, HTTP %ld em %s\n", r, http,
           rede_url_publica(url, seg, sizeof seg));
    fflush(stdout);
  }
  soltarHandle(c);
  free(b.p);
  return (!r && fim) ? 1 : 0;
}

char *rede_postar(const char *url, int segundos, const char *const *cab,
                  const char *corpo) {
  return rede_postar_st(url, segundos, cab, corpo, NULL);
}

// APAGAR pelo CUSTOMREQUEST, e nao por OPT_POST: o Trakt so remove um item da
// barra de retomada por DELETE /sync/playback/:id. Ver trakt_playback_remover.
char *rede_apagar(const char *url, int segundos, const char *const *cab,
                  int *status) {
  Balde b = { NULL, 0 };
  void *c, *lista = NULL;
  int r;
  if (status) *status = 0;
  if (!url || !*url || !abrir()) return NULL;
  c = pegarHandle();
  if (!c) return NULL;
  curl_setopt(c, OPT_URL, url);
  curl_setopt(c, OPT_WRITEFUNCTION, receber);
  curl_setopt(c, OPT_WRITEDATA, &b);
  curl_setopt(c, OPT_TIMEOUT, (long)(segundos > 0 ? segundos : 20));
  curl_setopt(c, OPT_NOSIGNAL, (long)1);
  curl_setopt(c, OPT_SSL_VERIFYPEER, (long)0);
  curl_setopt(c, OPT_SSL_VERIFYHOST, (long)0);
  curl_setopt(c, OPT_USERAGENT, "Nuvio/1.0 (webOS)");
  curl_setopt(c, OPT_CUSTOMREQUEST, "DELETE");
  if (slist_append) {
    int k;
    for (k = 0; cab && cab[k]; k++) lista = slist_append(lista, cab[k]);
    if (lista) curl_setopt(c, OPT_HTTPHEADER, lista);
  }
  r = curl_perform(c);
  { long h = 0;
    if (!r && curl_getinfo) curl_getinfo(c, INFO_RESPONSE_CODE, &h);
    if (status) *status = (int)h;
    if (h == 401 && aviso401) aviso401(url); }
  if (lista) { curl_setopt(c, OPT_HTTPHEADER, (void *)0); if (slist_free) slist_free(lista); }
  soltarHandle(c);
  if (r != 0) { free(b.p); return NULL; }
  // 204 sem corpo e a resposta NORMAL de um DELETE aceito: devolver NULL ali
  // faria o chamador ler sucesso como falha de transporte.
  if (!b.p) return strdup("");
  return b.p;
}

char *rede_postar_st(const char *url, int segundos, const char *const *cab,
                     const char *corpo, int *status) {
  Balde b = { NULL, 0 };
  void *c, *lista = NULL;
  int r;
  if (status) *status = 0;
  if (!url || !*url || !abrir()) return NULL;
  c = pegarHandle();
  if (!c) return NULL;
  curl_setopt(c, OPT_URL, url);
  curl_setopt(c, OPT_WRITEFUNCTION, receber);
  curl_setopt(c, OPT_WRITEDATA, &b);
  curl_setopt(c, OPT_TIMEOUT, (long)(segundos > 0 ? segundos : 20));
  curl_setopt(c, OPT_NOSIGNAL, (long)1);
  curl_setopt(c, OPT_SSL_VERIFYPEER, (long)0);
  curl_setopt(c, OPT_SSL_VERIFYHOST, (long)0);
  curl_setopt(c, OPT_USERAGENT, "Nuvio/1.0 (webOS)");
  curl_setopt(c, OPT_POST, (long)1);
  curl_setopt(c, OPT_POSTFIELDS, corpo ? corpo : "");
  if (slist_append) {
    int k;
    // JSON e o padrao (Supabase, Trakt); quem manda o proprio Content-Type
    // (Real-Debrid quer form-urlencoded) nao recebe um segundo.
    { int temCt = 0;
      for (k = 0; cab && cab[k]; k++) if (!strncasecmp(cab[k], "Content-Type:", 13)) temCt = 1;
      if (!temCt) lista = slist_append(lista, "Content-Type: application/json"); }
    for (k = 0; cab && cab[k]; k++) lista = slist_append(lista, cab[k]);
    if (lista) curl_setopt(c, OPT_HTTPHEADER, lista);
  }
  r = curl_perform(c);
  // O codigo sai ANTES do cleanup: depois dele a alca nao existe mais.
  { long codigo = 0;
    if (!r && curl_getinfo) curl_getinfo(c, INFO_RESPONSE_CODE, &codigo);
    if (status) *status = (int)codigo;
    if (codigo == 401 && aviso401) aviso401(url); }
  if (lista) { curl_setopt(c, OPT_HTTPHEADER, (void *)0); if (slist_free) slist_free(lista); }
  soltarHandle(c);
  // Falha de TRANSPORTE (r != 0) continua sendo NULL — ai nao houve resposta
  // nenhuma. O corpo de um 4xx, ao contrario, e devolvido: e nele que o
  // PostgREST explica o que faltou.
  if (r != 0) { free(b.p); return NULL; }
  return b.p ? b.p : strdup("");
}

#endif  /* __EMSCRIPTEN__ */

static unsigned long redeAgoraMs(void) {
  struct timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return 0;
  return (unsigned long)ts.tv_sec * 1000UL + (unsigned long)ts.tv_nsec / 1000000UL;
}

char *rede_baixar_medido(const char *url, int segundos,
                         const char *const *cabecalhos, RedeMedida *medida) {
  return rede_baixar_medido_controle(url, segundos, cabecalhos, NULL, medida);
}

char *rede_baixar_medido_controle(const char *url, int segundos,
                                  const char *const *cabecalhos,
                                  const RedeControle *controle,
                                  RedeMedida *medida) {
  int status = 0;
  unsigned long inicio = redeAgoraMs();
  char *corpo;
  long limite = controle ? controle->max_bytes : 0;
  volatile int *cancel = controle ? controle->cancelado : NULL;
  redeLimiteLocal = limite > 0 ? limite : 0;
  redeCancelLocal = cancel;
  redeLimitouLocal = 0;
  redeCancelouLocal = 0;
  redeBytesLocal = 0;
  corpo = rede_baixar_st(url, segundos, cabecalhos, &status);
  if (medida) {
    medida->status = status;
    medida->bytes = redeBytesLocal;
    medida->ms = redeAgoraMs() - inicio;
    medida->limitado = redeLimitouLocal;
    medida->cancelado = redeCancelouLocal;
  }
  redeLimiteLocal = 0;
  redeCancelLocal = NULL;
  redeLimitouLocal = 0;
  redeCancelouLocal = 0;
  redeBytesLocal = 0;
  return corpo;
}

char *rede_baixar_bin_medido_controle(const char *url, int segundos,
                                      const char *const *cabecalhos,
                                      const RedeControle *controle,
                                      long *tam, RedeMedida *medida) {
  int status = 0;
  unsigned long inicio = redeAgoraMs();
  char *corpo;
  long limite = controle ? controle->max_bytes : 0;
  volatile int *cancel = controle ? controle->cancelado : NULL;
  redeLimiteLocal = limite > 0 ? limite : 0;
  redeCancelLocal = cancel;
  redeLimitouLocal = 0;
  redeCancelouLocal = 0;
  redeBytesLocal = 0;
  corpo = rede_baixar_st(url, segundos, cabecalhos, &status);
  if (tam) *tam = redeBytesLocal;
  if (medida) {
    medida->status = status;
    medida->bytes = redeBytesLocal;
    medida->ms = redeAgoraMs() - inicio;
    medida->limitado = redeLimitouLocal;
    medida->cancelado = redeCancelouLocal;
  }
  redeLimiteLocal = 0;
  redeCancelLocal = NULL;
  redeLimitouLocal = 0;
  redeCancelouLocal = 0;
  redeBytesLocal = 0;
  return corpo;
}
