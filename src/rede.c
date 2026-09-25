#include "rede.h"
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <dlfcn.h>
#include <time.h>

/* Controle local da requisicao corrente. O estado nunca e compartilhado
 * entre sondagens: cada fio recebe seu teto e seu cancel token. */
static _Thread_local long redeLimiteLocal;
static _Thread_local volatile int *redeCancelLocal;
static _Thread_local int redeLimitouLocal;
static _Thread_local int redeCancelouLocal;
static _Thread_local long redeBytesLocal;
static unsigned long redeAgoraMs(void);

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

// Faz a requisicao e devolve um buffer de malloc com o corpo (com um NUL extra
// no fim, para quem trata como texto). Escreve o tamanho em *tam e o status
// HTTP em *status. Devolve 0 se a requisicao nem saiu.
//
// `cabs` vem como uma unica string com uma linha "Nome: valor" por cabecalho,
// separadas por \n, porque atravessar um vetor de ponteiros por EM_JS custaria
// mais codigo do que juntar e separar.
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
    stringToUTF8(xhr.responseURL || "", urlFinal, urlFinalTam);
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

_Thread_local long rede_teto = 0;
// Destino do endereco final do proximo pedido (so rede_baixar_trecho_st liga).
static _Thread_local char *redeFinalDst;
static _Thread_local unsigned redeFinalTam;

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

static char *pedir2(const char *metodo, const char *url, const char *const *cab,
                    const char *extraCab, const char *corpo,
                    long *tam, int *status, char *etag, unsigned tamEtag) {
  char *cabs, *corpoResp;
  int n = 0, http = 0;
  if (etag && tamEtag) etag[0] = 0;
  if (status) *status = 0;
  if (!url || !*url) return NULL;
  cabs = juntarCabs(cab, extraCab);
  corpoResp = nv_http(metodo, url, cabs, corpo, &n, &http,
                      redeFinalDst, (int)redeFinalTam, etag, (int)tamEtag);
  free(cabs);
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

// UM pedido de Range (o laco em pedacos e rede_baixar_trecho_st, no fim do
// arquivo). XHR nao entrega corpo cortado: uma conexao que fecha antes do
// Content-Length e erro de rede e o corpo some — aqui nunca ha "parcial".
static char *trechoUmaVez(const char *url, int segundos, long ini, long fim,
                          long *tam, int *status, int *erro,
                          char *final, unsigned tamFinal) {
  char faixa[80];
  const char *cab[2];
  char *r;
  int st = 0;
  (void)segundos;
  if (erro) *erro = 0;
  if (final && tamFinal) final[0] = 0;
  snprintf(faixa, sizeof faixa, "Range: bytes=%ld-%ld", ini, fim);
  cab[0] = faixa; cab[1] = NULL;
  rede_teto = fim - ini + 1;
  redeFinalDst = final; redeFinalTam = final ? tamFinal : 0;
  r = pedir("GET", url, cab, NULL, NULL, tam, &st);
  redeFinalDst = NULL; redeFinalTam = 0;
  rede_teto = 0;
  if (status) *status = st;
  // Com `status` o pedir devolve o corpo do erro; aqui o corpo so vale em 2xx.
  if (r && (st < 200 || st >= 300)) {
    char seg[120];
    printf("[rede] HTTP %d em %s\n", st, rede_url_publica(url, seg, sizeof seg));
    fflush(stdout);
    free(r); r = NULL;
    if (tam) *tam = 0;
  }
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

// VAZAO NO TIZEN (ver rede_medir_vazao em rede.h). XHR sincrono so devolve
// quando o corpo inteiro chegou, entao nao ha "bytes por segundo" de dentro de
// uma resposta: sao pedidos de Range em PEDACOS, cronometrados um a um, com o
// corpo descartado no proprio JavaScript (so o tamanho atravessa para o C —
// nada vai para o heap do WASM).
//
// O pedaco comeca em 1 MB e dobra enquanto volta em menos de 0,7 s, ate 8 MB:
// pedaco pequeno numa rede rapida mediria mais o vai-e-volta de cada pedido do
// que a vazao; pedaco grande numa rede lenta passaria da janela sem como
// cortar (XHR sincrono nao tem prazo nem aborto). Cada pedaco vira tantas
// amostras quantos segundos inteiros levou (no minimo uma), todas com a taxa
// dele — e a equivalencia mais proxima de "uma amostra por segundo".
//
// Mesma limitacao ja escrita em pedir2: servidor que IGNORA o Range manda o
// arquivo inteiro, e a chamada so volta depois. Os CDNs de debrid honram
// Range; se um nao honrar, o teste trava o fio dele ate o navegador desistir.
EM_JS(int, nv_http_contar, (const char *url, const char *cabs, double ini,
                            double fim, int *status, char *urlFinal,
                            int urlFinalTam), {
  var xhr = new XMLHttpRequest();
  HEAP32[status >> 2] = 0;
  try { xhr.open("GET", UTF8ToString(url), false); } catch (e) { return -1; }
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
  try { xhr.setRequestHeader("Range", "bytes=" + ini + "-" + fim); } catch (e) {}
  try { xhr.send(null); } catch (e) { return -1; }
  HEAP32[status >> 2] = xhr.status;
  if (urlFinal && urlFinalTam > 0) stringToUTF8(xhr.responseURL || "", urlFinal, urlFinalTam);
  var s = xhr.responseText || "";
  return s.length;
});

int rede_medir_vazao(const char *url, const char *const *cab, int segundos,
                     long inicio, long long maxBytes, volatile int *cancelado,
                     int *kbps, int nMax, RedeVazao *res,
                     char *final, unsigned tamFinal) {
  char *cabs;
  char atual[4096];
  unsigned long t0, janela, gasto = 0;
  long long pos = inicio > 0 ? inicio : 0, total = 0, pedaco = 1024LL * 1024LL;
  int nSeg = 0, primeiro = 1;
  if (res) memset(res, 0, sizeof *res);
  if (final && tamFinal) final[0] = 0;
  if (!url || !*url) { if (res) res->erro = 2; return 0; }
  if (segundos < 1) segundos = 1;
  janela = (unsigned long)segundos * 1000UL;
  snprintf(atual, sizeof atual, "%s", url);
  cabs = juntarCabs(cab, NULL);
  t0 = redeAgoraMs();
  while (gasto < janela && (maxBytes <= 0 || total < maxBytes)) {
    unsigned long a = redeAgoraMs(), dt;
    int st = 0, n, k, reps;
    char fin[4096];
    if (cancelado && *cancelado) { if (res) res->cancelado = 1; break; }
    fin[0] = 0;
    n = nv_http_contar(atual, cabs, (double)pos, (double)(pos + pedaco - 1), &st,
                       fin, (int)sizeof fin);
    dt = redeAgoraMs() - a;
    if (res) res->status = st;
    if (primeiro && final && tamFinal) snprintf(final, tamFinal, "%s", fin);
    if (n < 0 || st == 0) { if (res) res->erro = -1; break; }
    if (st < 200 || st >= 300 || n == 0) break;
    if (primeiro && res) res->esperaMs = dt;
    primeiro = 0;
    // Os pedacos seguintes vao direto ao endereco final: sem pagar o
    // redirecionamento do addon de novo a cada pedaco.
    if (fin[0]) snprintf(atual, sizeof atual, "%s", fin);
    total += n;
    pos += n;
    gasto = redeAgoraMs() - t0;
    reps = (int)(dt / 1000UL);
    if (reps < 1) reps = 1;
    for (k = 0; k < reps && kbps && nSeg < nMax; k++)
      kbps[nSeg++] = (int)((long long)n * 8 / (long long)(dt > 0 ? dt : 1));
    if (st == 200) break;              // Range ignorado: veio o arquivo todo
    if ((long long)n < pedaco) break;  // o arquivo acabou
    if (dt < 700UL && pedaco < 8LL * 1024LL * 1024LL) pedaco *= 2;
  }
  free(cabs);
  if (res) {
    res->bytes = total;
    res->ms = gasto;
    if (cancelado && *cancelado) res->cancelado = 1;
  }
  if (res && res->cancelado) return 0;
  return nSeg;
}

#else

// Codigo da libcurl do ultimo pedido DESTE fio (0 = transporte ok). So o
// rede_baixar_trecho_st le: o resto do modulo segue devolvendo NULL e logando.
static _Thread_local int redeCurlLocal;
// Destino do endereco final do proximo pedido (so rede_baixar_trecho_st liga).
static _Thread_local char *redeFinalDst;
static _Thread_local unsigned redeFinalTam;
// 1 = o pedido corrente aceita corpo CORTADO (206 que fechou antes do fim):
// so trechoUmaVez liga. Ver a nota la.
static _Thread_local int redeParcialOk;

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
// Prazos em milissegundos (7.16.2+), para a segunda tentativa caber no que
// sobrou do prazo do pedido. Ver rede_baixar_interno2.
#define OPT_TIMEOUT_MS        155
#define OPT_CONNECTTIMEOUT_MS 156
// Vigia de progresso (7.32+): NOPROGRESS=0 liga o XFERINFOFUNCTION.
#define OPT_NOPROGRESS         43
#define OPT_XFERINFOFUNCTION 20219
#define OPT_XFERINFODATA     10057
// Keepalive de TCP (7.25+) nas conexoes guardadas no cache do handle.
#define OPT_TCP_KEEPALIVE     213
#define OPT_TCP_KEEPIDLE      214
#define OPT_TCP_KEEPINTVL     215
// CURLINFO_NUM_CONNECTS = CURLINFO_LONG + 26: 0 = o pedido foi por conexao
// REUSADA. So para o log de falha.
#define INFO_NUM_CONNECTS  2097178

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

// CONEXAO OCIOSA DEMAIS NAO E REUSADA (24/09/2026).
//
// Uma conexao guardada no cache pode morrer SEM AVISO: o NAT do roteador
// esquece a entrada, o servidor some, o Wi-Fi da TV troca de canal. Sem FIN
// nem RST a libcurl nao tem como saber — a checagem dela e "o socket ficou
// legivel?" — e manda o pedido por ela. Ninguem responde, e o pedido espera o
// prazo INTEIRO (curl 28). tests/rede_parada.sh reproduz: 4 s de prazo, 4 s
// de espera, NULL.
//
// MEDIDO na C9 com o curl da propria TV (mesma libcurl 7.53.1), mzstatic
// reusado depois de 1, 20, 45, 90, 150 e 300 s parado: conexao reusada
// (num_connects=0) e 30-40 ms em todos. Ou seja, NESTA rede isto nao explica
// o curl 28 da arte da Apple; e defesa barata para as outras (e para a C9 com
// outro roteador). O limite e por HOST, e nao pelo handle: um fio de arte
// fala com o TMDB o tempo todo e com o mzstatic de vez em quando, e a
// conexao do mzstatic envelhece enquanto o handle nao para de ser usado.
// Host velho descarta o HANDLE inteiro (a libcurl 7.53 nao tem
// CURLOPT_MAXAGE_CONN, e FRESH_CONNECT deixaria a velha no cache para o
// pedido seguinte escolher). Custa um handshake por host depois de uma pausa;
// rajada de arte, que e onde o reuso rende, nunca para tanto.
// NUVIO_REDE_OCIOSO_MS muda o limite (0 = sem limite, como antes).
#define REDE_OCIOSO_PADRAO_MS 20000UL
static unsigned long ociosoMaxMs = REDE_OCIOSO_PADRAO_MS;
#define REDE_HOSTS_POR_FIO 16
typedef struct { char h[96]; unsigned long ms; } UsoHost;
static _Thread_local UsoHost usoHost[REDE_HOSTS_POR_FIO];

// "https://a.b:443" de "https://a.b:443/x?y". Sem esquema = vazio.
static void hostDaUrl(const char *u, char *h, size_t n) {
  const char *p = u ? strstr(u, "://") : NULL;
  size_t k;
  h[0] = 0;
  if (!p || n == 0) return;
  p += 3;
  k = (size_t)(p - u) + strcspn(p, "/?#");
  if (k >= n) k = n - 1;
  memcpy(h, u, k);
  h[k] = 0;
}
static void hostsEsquecer(void) { memset(usoHost, 0, sizeof usoHost); }
static void hostUsado(const char *url, unsigned long agora) {
  char h[96];
  int i, velho = 0;
  hostDaUrl(url, h, sizeof h);
  if (!h[0]) return;
  for (i = 0; i < REDE_HOSTS_POR_FIO; i++) {
    if (!strcmp(usoHost[i].h, h)) { usoHost[i].ms = agora; return; }
    if (usoHost[i].ms < usoHost[velho].ms) velho = i;
  }
  snprintf(usoHost[velho].h, sizeof usoHost[velho].h, "%s", h);
  usoHost[velho].ms = agora;
}
// 1 = o fio ja falou com este host e a conexao guardada esta parada ha mais
// que o limite.
static int hostOcioso(const char *url, unsigned long agora) {
  char h[96];
  int i;
  if (!ociosoMaxMs) return 0;
  hostDaUrl(url, h, sizeof h);
  if (!h[0]) return 0;
  for (i = 0; i < REDE_HOSTS_POR_FIO; i++)
    if (usoHost[i].ms && !strcmp(usoHost[i].h, h))
      return agora - usoHost[i].ms > ociosoMaxMs;
  return 0;
}

static void *pegarHandle(const char *url) {
  void *c;
  if (!curl_reset) return curl_init();     // libcurl sem reset: como antes
  pthread_once(&handleUma, handleCriarChave);
  c = pthread_getspecific(handleChave);
  if (c && hostOcioso(url, redeAgoraMs())) {
    curl_cleanup(c);
    pthread_setspecific(handleChave, NULL);
    hostsEsquecer();
    c = NULL;
  }
  if (c) curl_reset(c);
  else {
    c = curl_init();
    if (c) pthread_setspecific(handleChave, c);
  }
  // Depois do reset: curl_easy_reset volta MAXCONNECTS ao padrao.
  if (c) curl_setopt(c, OPT_MAXCONNECTS, REDE_CONEXOES_POR_FIO);
  return c;
}
// DEVOLVE SO CONEXAO LIMPA (23/09/2026). Transferencia que nao terminou bem —
// erro, prazo estourado, ou o CORTE DE PROPOSITO do teto/Range (curl 23) —
// descarta o handle inteiro, e com ele a conexao: na C9 apareceram cartoes com
// a arte de OUTRO titulo com o reuso ligado, e sumiram com ele desligado. A
// suspeita e a libcurl 7.53.1 da TV devolvendo ao cache uma conexao com resto
// da resposta abortada, que sai no pedido seguinte. Download completo segue
// reaproveitado (e o ganho de 0,5-0,8 s por imagem). Conexao que fica anota o
// host (e o do fim dos redirecionamentos) para o limite de ociosidade.
static void soltarHandleR(void *c, int r, const char *url) {
  if (!curl_reset) { curl_cleanup(c); return; }
  if (r != 0) {
    curl_cleanup(c);
    pthread_setspecific(handleChave, NULL);
    hostsEsquecer();
    return;
  }
  { unsigned long agora = redeAgoraMs();
    char *fim = NULL;
    hostUsado(url, agora);
    if (curl_getinfo && !curl_getinfo(c, INFO_URL_FINAL, &fim) && fim) hostUsado(fim, agora); }
}

// OPCOES DE TODO PEDIDO, com o prazo em ms.
//
// CONNECTTIMEOUT: DNS + TCP + TLS numa conexao nova levam 0,14-0,7 s na C9
// (curl da TV, mzstatic). O padrao da libcurl e esperar ate o prazo inteiro, e
// a resolucao e c-ares (curl -V da TV: AsynchDNS, c-ares 1.12; resolv.conf
// aponta para o 127.0.0.1 do connman), que pelo padrao dele so reenvia uma
// consulta sem resposta depois de 5 s — com prazo de 6 s, uma consulta perdida
// seria curl 28 (padrao do c-ares, NAO medido na TV). Metade do prazo, entre
// 2 e 5 s, deixa o resto para a segunda tentativa (rede_baixar_interno2).
// KEEPALIVE: sonda a conexao parada no cache a cada 15 s, o que mantem viva a
// entrada do NAT e faz a morta dar erro no socket (a libcurl a descarta sem
// tentar mandar nada por ela).
static unsigned long conexaoMs(unsigned long prazoMs) {
  unsigned long m = prazoMs / 2;
  if (m < 2000) m = 2000;
  if (m > 5000) m = 5000;
  return m < prazoMs ? m : prazoMs;
}
static void opcoesComuns(void *c, unsigned long prazoMs) {
  curl_setopt(c, OPT_TIMEOUT_MS, (long)prazoMs);
  curl_setopt(c, OPT_CONNECTTIMEOUT_MS, (long)conexaoMs(prazoMs));
  // O app roda com fios; sem NOSIGNAL a libcurl usa alarmes para o timeout de
  // DNS e pode derrubar o processo inteiro a partir de um fio secundario.
  curl_setopt(c, OPT_NOSIGNAL, (long)1);
  curl_setopt(c, OPT_TCP_KEEPALIVE, (long)1);
  curl_setopt(c, OPT_TCP_KEEPIDLE, (long)15);
  curl_setopt(c, OPT_TCP_KEEPINTVL, (long)5);
  // Os addons sao servidos por hosts com cadeias que este aparelho de 2019 nao
  // conhece; o pacote de CAs dele e de fabrica e nao se atualiza. Verificar
  // recusaria fontes legitimas do dono. O conteudo e midia publica e a escolha
  // esta escrita aqui de proposito.
  curl_setopt(c, OPT_SSL_VERIFYPEER, (long)0);
  curl_setopt(c, OPT_SSL_VERIFYHOST, (long)0);
  curl_setopt(c, OPT_USERAGENT, "Nuvio/1.0 (webOS)");
}

// VIGIA DE PROGRESSO. A libcurl chama isto ao menos uma vez por segundo
// durante a transferencia, chegue byte ou nao.
//  - CORPO PARADO: a resposta comecou e nenhum byte novo chegou em `paradoMs`
//    — conexao morta no meio do corpo. Sem isto o pedido esperava o prazo
//    inteiro; com isto aborta (curl 42) e rede_baixar_interno2 repete em
//    conexao nova com o que sobrou do prazo. So conta DEPOIS do primeiro byte
//    do corpo: antes dele o servidor pode estar so pensando (catalogo de addon
//    leva segundos para montar).
//  - CANCELAMENTO: o recebedor so via o cancelamento quando chegava byte; uma
//    conexao parada nao cancelava nunca.
typedef struct {
  unsigned long ultimoMs, paradoMs;
  long long visto;
  int parou;
} Vigia;
static int vigiar(void *u, long long dlTotal, long long dlAgora,
                  long long ulTotal, long long ulAgora) {
  Vigia *v = (Vigia *)u;
  unsigned long agora = redeAgoraMs();
  (void)dlTotal; (void)ulTotal; (void)ulAgora;
  if (redeCancelLocal && *redeCancelLocal) { redeCancelouLocal = 1; return 1; }
  if (dlAgora > v->visto) { v->visto = dlAgora; v->ultimoMs = agora; return 0; }
  if (v->visto > 0 && v->paradoMs && agora - v->ultimoMs > v->paradoMs) {
    v->parou = 1;
    return 1;
  }
  return 0;
}
static unsigned long paradoMsDe(unsigned long prazoMs) {
  unsigned long m = prazoMs / 3;
  if (m < 2000) m = 2000;
  if (m > 8000) m = 8000;
  return m;
}
static void ligarVigia(void *c, Vigia *v, unsigned long prazoMs) {
  memset(v, 0, sizeof *v);
  v->paradoMs = paradoMsDe(prazoMs);
  v->ultimoMs = redeAgoraMs();
  curl_setopt(c, OPT_XFERINFOFUNCTION, vigiar);
  curl_setopt(c, OPT_XFERINFODATA, v);
  curl_setopt(c, OPT_NOPROGRESS, (long)0);
}

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
  // REUSO LIGADO, mas so para transferencia limpa (soltarHandleR).
  // NUVIO_REDE_REUSO=0 desliga de vez, para comparar numa TV com problema.
  { const char *r = getenv("NUVIO_REDE_REUSO");
    if (!(r && r[0] == '0')) *(void **)(&curl_reset) = dlsym(h, "curl_easy_reset"); }
  { const char *o = getenv("NUVIO_REDE_OCIOSO_MS");
    if (o && *o) ociosoMaxMs = strtoul(o, NULL, 10); }
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

// UM pedido de Range (o laco em pedacos e rede_baixar_trecho_st, no fim do
// arquivo). Range e um cabecalho comum, entao o caminho com cabecalhos ja
// existente serve.
//
// TETO DE VERDADE, e nao so o cabecalho. MEDIDO: um servidor que ignora o
// Range responde 200 com o arquivo INTEIRO — no teste vieram 31 MB para um
// pedido de 2 MB. Sem o teto, ler o cabecalho de um filme de 20 GB baixaria o
// filme. O corte e no recebedor, entao a conexao morre no limite em vez de
// esperar o fim.
//
// CORPO CORTADO FICA (#92): um 206 que fecha antes do Content-Length (curl 18,
// ou 56 no meio do corpo) devolve o que veio, com o codigo em *erro. So quem
// sabe pedir o resto (o laco) liga isto; o resto do modulo segue igual.
static char *trechoUmaVez(const char *url, int segundos, long ini, long fim,
                          long *tam, int *status, int *erro,
                          char *final, unsigned tamFinal) {
  char faixa[80];
  const char *cab[2];
  char *r;
  int st = 0;
  if (final && tamFinal) final[0] = 0;
  snprintf(faixa, sizeof faixa, "Range: bytes=%ld-%ld", ini, fim);
  cab[0] = faixa; cab[1] = NULL;
  redeCurlLocal = 0;
  rede_teto = fim - ini + 1;
  redeFinalDst = final; redeFinalTam = final ? tamFinal : 0;
  redeParcialOk = 1;
  // Com `status` o interno2 devolve o corpo de um 4xx (e o contrato do
  // Supabase); aqui ele e descartado, mas o codigo fica com quem chamou.
  r = rede_baixar_interno2(url, segundos, tam, cab, &st, NULL, 0);
  redeParcialOk = 0;
  redeFinalDst = NULL; redeFinalTam = 0;
  rede_teto = 0;
  if (status) *status = st;
  if (erro) *erro = redeCurlLocal;
  if (r && (st < 200 || st >= 300)) {
    char seg[120];
    printf("[rede] HTTP %d em %s\n", st, rede_url_publica(url, seg, sizeof seg));
    fflush(stdout);
    free(r); r = NULL;
    if (tam) *tam = 0;
  }
  return r;
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

// SEGUNDA TENTATIVA EM CONEXAO NOVA (24/09/2026), so para falha de TRANSPORTE
// que nao trouxe nada — e so se sobrou prazo. Conexao que nao abriu a tempo
// (CONNECTTIMEOUT, ver opcoesComuns), recusada, TLS que caiu no meio, conexao
// reusada que morreu sem resposta, e o corpo que parou (vigia). O que JA custou
// o prazo inteiro nao repete: o prazo e do pedido, nao de cada tentativa, e
// quem chama continua sabendo quanto vai esperar no maximo. Corte de proposito
// (teto, curl 23), cancelamento e resposta HTTP de erro nunca repetem.
static int valeRepetir(int r, const Vigia *v, size_t bytes) {
  if (redeCancelouLocal) return 0;
  if (r == 42) return v->parou;            // vigia: corpo parado
  if (bytes > 0) return 0;
  return r == 7 || r == 28 || r == 35 || r == 52 || r == 55 || r == 56;
}

static char *rede_baixar_interno2(const char *url, int segundos, long *tam,
                                  const char *const *cab, int *status,
                                  char *etag, unsigned tamEtag) {
  Balde b = { NULL, 0 };
  CacaCab caca;
  Vigia vigia;
  void *c, *lista = NULL;
  int r = 0, tentativa, reusada = 0;
  unsigned long inicio, prazoMs, gasto = 0;
  caca.dst = (etag && tamEtag > 1) ? etag : NULL;
  caca.tam = tamEtag;
  if (etag && tamEtag) etag[0] = 0;
  if (status) *status = 0;
  if (!url || !*url || !abrir()) return NULL;
  inicio = redeAgoraMs();
  prazoMs = (unsigned long)(segundos > 0 ? segundos : 30) * 1000UL;
  for (tentativa = 0; ; tentativa++) {
    unsigned long resta = prazoMs - gasto;
    c = pegarHandle(url);
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
    opcoesComuns(c, resta);
    ligarVigia(c, &vigia, prazoMs);
    curl_setopt(c, OPT_ACCEPT_ENCODING, "");   // "" = todas as que a lib suporta
    if (cab && slist_append) {
      int k;
      for (k = 0; cab[k]; k++) lista = slist_append(lista, cab[k]);
      if (lista) curl_setopt(c, OPT_HTTPHEADER, lista);
    }
    r = curl_perform(c);
    { long novas = -1;
      if (curl_getinfo) curl_getinfo(c, INFO_NUM_CONNECTS, &novas);
      reusada = novas == 0; }
    gasto = redeAgoraMs() - inicio;
    if (tentativa == 0 && r != 0 && valeRepetir(r, &vigia, b.n) &&
        gasto + 1000 < prazoMs) {
      char seg[120];
      printf("[rede] curl %d em %s (conexao %s, %ld bytes, %lu ms): de novo em conexao nova\n",
             r, rede_url_publica(url, seg, sizeof seg), reusada ? "reusada" : "nova",
             (long)b.n, gasto);
      fflush(stdout);
      if (lista) { curl_setopt(c, OPT_HTTPHEADER, (void *)0); if (slist_free) slist_free(lista); lista = NULL; }
      soltarHandleR(c, r, url);            // r != 0: descarta o handle
      free(b.p);
      b.p = NULL; b.n = 0;
      if (etag && tamEtag) etag[0] = 0;
      continue;
    }
    break;
  }
  if (r == 42 && vigia.parou) r = 28;      // para quem chama, e prazo
  redeCurlLocal = r;
  if (redeFinalDst && redeFinalTam && curl_getinfo) {
    char *fim = NULL;
    curl_getinfo(c, INFO_URL_FINAL, &fim);
    snprintf(redeFinalDst, redeFinalTam, "%s", fim ? fim : "");
  }
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
      soltarHandleR(c, r, url);
      free(b.p);
      { char seg[120];
        printf("[rede] HTTP %ld em %s\n", http, rede_url_publica(url, seg, sizeof seg)); }
      fflush(stdout);
      return NULL;
    } }
  if (lista) { curl_setopt(c, OPT_HTTPHEADER, (void *)0); if (slist_free) slist_free(lista); }
  soltarHandleR(c, r, url);
  // 23 = CURLE_WRITE_ERROR. Quando ha teto, ele e o resultado ESPERADO: o
  // recebedor devolve menos bytes de proposito para cortar a conexao assim que
  // enche. Nesse caso o que ja veio e exatamente o que se queria — tratar como
  // falha jogaria fora o cabecalho inteiro que acabamos de baixar.
  if (redeCancelouLocal) {
    free(b.p);
    return NULL;
  }
  if (r == 23 && redeLimiteAtual() > 0 && b.n > 0) r = 0;
  // CORTE NO MEIO DO 206 (#92): o CDN do Real-Debrid responde 206 ao Range de
  // 256 KB e fecha a conexao depois de 77465 bytes, sempre. O que veio e o
  // comeco verdadeiro do trecho pedido: fica, com o codigo em redeCurlLocal, e
  // quem ligou redeParcialOk (trechoUmaVez) pede o resto. O handle ja foi
  // descartado acima (r != 0), junto com a conexao morta.
  if ((r == 18 || r == 56) && redeParcialOk && b.n > 0 && status && *status == 206) {
    char seg[120];
    printf("[rede] corte %d em %s (conexao %s, %ld bytes, %lu ms): fica o que veio\n", r,
           rede_url_publica(url, seg, sizeof seg), reusada ? "reusada" : "nova",
           (long)b.n, gasto);
    fflush(stdout);
    redeBytesLocal = (long)b.n;
    if (tam) *tam = (long)b.n;
    return b.p;
  }
  // O RESTO DA HISTORIA no log: por qual conexao foi, quanto veio e quanto
  // esperou. "falha 28" sozinho nao separa conexao morta (reusada, 0 bytes),
  // rede lenta (bytes > 0, prazo inteiro) e DNS/conexao que nao abriu (nova,
  // 0 bytes). O comeco da linha fica igual para quem ja procura por ele.
  if (r != 0) { char seg[120]; free(b.p);
    printf("[rede] falha %d em %s (conexao %s, %ld bytes, %lu ms%s)\n", r,
           rede_url_publica(url, seg, sizeof seg), reusada ? "reusada" : "nova",
           (long)b.n, gasto, tentativa ? ", 2a tentativa" : "");
    fflush(stdout);
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
  c = pegarHandle(url);
  if (!c) return 0;
  curl_setopt(c, OPT_URL, url);
  curl_setopt(c, OPT_WRITEFUNCTION, receber);
  curl_setopt(c, OPT_WRITEDATA, &b);
  curl_setopt(c, OPT_FOLLOWLOCATION, (long)1);
  opcoesComuns(c, (unsigned long)(segundos > 0 ? segundos : 20) * 1000UL);
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
  soltarHandleR(c, r, url);
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
  c = pegarHandle(url);
  if (!c) return NULL;
  curl_setopt(c, OPT_URL, url);
  curl_setopt(c, OPT_WRITEFUNCTION, receber);
  curl_setopt(c, OPT_WRITEDATA, &b);
  opcoesComuns(c, (unsigned long)(segundos > 0 ? segundos : 20) * 1000UL);
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
  soltarHandleR(c, r, url);
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
  c = pegarHandle(url);
  if (!c) return NULL;
  curl_setopt(c, OPT_URL, url);
  curl_setopt(c, OPT_WRITEFUNCTION, receber);
  curl_setopt(c, OPT_WRITEDATA, &b);
  opcoesComuns(c, (unsigned long)(segundos > 0 ? segundos : 20) * 1000UL);
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
  soltarHandleR(c, r, url);
  // Falha de TRANSPORTE (r != 0) continua sendo NULL — ai nao houve resposta
  // nenhuma. O corpo de um 4xx, ao contrario, e devolvido: e nele que o
  // PostgREST explica o que faltou.
  if (r != 0) { free(b.p); return NULL; }
  return b.p ? b.p : strdup("");
}

// ------------------------------------------------------------ VAZAO (curl)
//
// Ver rede_medir_vazao em rede.h. Um recebedor que CONTA e descarta, com um
// balde de bytes por segundo desde o primeiro byte do corpo. O corte (janela
// cheia, teto de bytes, cancelamento) e de proposito: o recebedor devolve 0 e a
// libcurl aborta com 23 (ou 42 pelo vigia), e o handle sai do cache como em
// todo corte (soltarHandleR com r != 0) — a conexao abortada no meio de um
// corpo de gigabytes nunca e reaproveitada.
#define VAZ_SEG_BALDES 64
typedef struct {
  unsigned long pedido, t0, janelaMs, ultimo;
  long long bytes, maxBytes;
  long long balde[VAZ_SEG_BALDES];
  int status, porJanela, porTeto, cancelou;
  volatile int *cancel;
} Contador;

// Linha de status de CADA resposta ("HTTP/1.1 302", depois "HTTP/1.1 206"):
// a ultima vale. Corpo de 4xx/5xx nao e medida de nada.
static size_t contadorCab(void *dados, size_t tam, size_t qtd, void *u) {
  Contador *c = (Contador *)u;
  const char *s = (const char *)dados;
  size_t b = tam * qtd;
  if (b > 9 && !strncmp(s, "HTTP/", 5)) {
    const char *p = memchr(s, ' ', b);
    if (p) c->status = atoi(p + 1);
  }
  return b;
}

static size_t contadorCorpo(void *dados, size_t tam, size_t qtd, void *u) {
  Contador *c = (Contador *)u;
  size_t b = tam * qtd;
  unsigned long agora = redeAgoraMs(), el;
  (void)dados;
  if (c->status >= 400) return 0;
  if (c->cancel && *c->cancel) { c->cancelou = 1; return 0; }
  if (!c->t0) c->t0 = agora;
  el = agora - c->t0;
  if (el >= c->janelaMs) { c->porJanela = 1; return 0; }
  if (el / 1000UL < VAZ_SEG_BALDES) c->balde[el / 1000UL] += (long long)b;
  c->bytes += (long long)b;
  c->ultimo = agora;
  if (c->maxBytes > 0 && c->bytes >= c->maxBytes) { c->porTeto = 1; return 0; }
  return b;
}

// Chamado ~1x/s mesmo sem byte: e o que fecha a janela de um corpo PARADO.
static int contadorVigia(void *u, long long dt, long long dn, long long ut, long long un) {
  Contador *c = (Contador *)u;
  (void)dt; (void)dn; (void)ut; (void)un;
  if (c->cancel && *c->cancel) { c->cancelou = 1; return 1; }
  if (c->t0 && redeAgoraMs() - c->t0 >= c->janelaMs) { c->porJanela = 1; return 1; }
  return 0;
}

int rede_medir_vazao(const char *url, const char *const *cab, int segundos,
                     long inicio, long long maxBytes, volatile int *cancelado,
                     int *kbps, int nMax, RedeVazao *res,
                     char *final, unsigned tamFinal) {
  static const Contador vazio;
  Contador ct = vazio;
  char faixa[40];
  void *c, *lista = NULL;
  int r, k, nSeg = 0;
  unsigned long el = 0;
  if (res) memset(res, 0, sizeof *res);
  if (final && tamFinal) final[0] = 0;
  if (!url || !*url || !abrir()) { if (res) res->erro = 2; return 0; }
  if (segundos < 1) segundos = 1;
  ct.janelaMs = (unsigned long)segundos * 1000UL;
  ct.maxBytes = maxBytes;
  ct.cancel = cancelado;
  ct.pedido = redeAgoraMs();
  c = pegarHandle(url);
  if (!c) { if (res) res->erro = 2; return 0; }
  curl_setopt(c, OPT_URL, url);
  curl_setopt(c, OPT_WRITEFUNCTION, contadorCorpo);
  curl_setopt(c, OPT_WRITEDATA, &ct);
  curl_setopt(c, OPT_HEADERFUNCTION, contadorCab);
  curl_setopt(c, OPT_HEADERDATA, &ct);
  curl_setopt(c, OPT_FOLLOWLOCATION, (long)1);
  // Prazo: a janela mais 8 s para DNS, TLS, redirecionamentos e o primeiro
  // byte. O conexaoMs de opcoesComuns fica no teto de 5 s.
  opcoesComuns(c, ct.janelaMs + 8000UL);
  curl_setopt(c, OPT_XFERINFOFUNCTION, contadorVigia);
  curl_setopt(c, OPT_XFERINFODATA, &ct);
  curl_setopt(c, OPT_NOPROGRESS, (long)0);
  // Sem ACCEPT_ENCODING: a vazao que interessa e a do arquivo como o player o
  // recebe, sem gzip de CDN nenhum no meio.
  if (inicio > 0) {
    snprintf(faixa, sizeof faixa, "%ld-", inicio);
    curl_setopt(c, OPT_RANGE, faixa);
  }
  if (cab && slist_append) {
    for (k = 0; cab[k]; k++) lista = slist_append(lista, cab[k]);
    if (lista) curl_setopt(c, OPT_HTTPHEADER, lista);
  }
  r = curl_perform(c);
  { long http = 0;
    if (curl_getinfo) curl_getinfo(c, INFO_RESPONSE_CODE, &http);
    if (http > 0) ct.status = (int)http; }
  if (final && tamFinal && curl_getinfo) {
    char *fim = NULL;
    curl_getinfo(c, INFO_URL_FINAL, &fim);
    snprintf(final, tamFinal, "%s", fim ? fim : "");
  }
  if (lista) { curl_setopt(c, OPT_HTTPHEADER, (void *)0); if (slist_free) slist_free(lista); }
  soltarHandleR(c, r, url);
  // Corte de proposito nao e erro; prazo estourado ou arquivo que acabou com
  // bytes na mao tambem nao (a medida e o que veio ate ali).
  if (ct.porJanela || ct.porTeto || ((r == 28 || r == 18) && ct.bytes > 0)) r = 0;
  if (ct.t0) el = ct.porJanela ? ct.janelaMs : (ct.ultimo ? ct.ultimo : ct.t0) - ct.t0;
  if (kbps && nMax > 0 && ct.bytes > 0 && !ct.cancelou && ct.status < 400 && r == 0) {
    nSeg = (int)(el / 1000UL);
    if (nSeg > nMax) nSeg = nMax;
    if (nSeg > VAZ_SEG_BALDES) nSeg = VAZ_SEG_BALDES;
    for (k = 0; k < nSeg; k++) kbps[k] = (int)(ct.balde[k] * 8 / 1000);
    if (nSeg == 0) {
      // Menos de 1 s de corpo: uma amostra so, pela taxa media do que veio.
      kbps[0] = (int)(ct.bytes * 8 / (long long)(el > 0 ? el : 1));
      nSeg = 1;
    }
  }
  if (res) {
    res->status = ct.status;
    res->erro = ct.cancelou ? 0 : r;
    res->bytes = ct.bytes;
    res->ms = el;
    res->esperaMs = ct.t0 ? ct.t0 - ct.pedido : 0;
    res->cancelado = ct.cancelou;
  }
  return nSeg;
}

#endif  /* __EMSCRIPTEN__ */

static unsigned long redeAgoraMs(void) {
  struct timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return 0;
  return (unsigned long)ts.tv_sec * 1000UL + (unsigned long)ts.tv_nsec / 1000000UL;
}

// ------------------------------------------------------------ RANGE EM PEDACOS
//
// #92, 1.4.6 (Real-Debrid pelo Torrentio, LG): o CDN responde 206 ao Range de
// 256 KB do mkvass e fecha a conexao depois de 77465 bytes, TODA vez (curl 18,
// ~200 ms). O modulo jogava o corpo fora e pedia o MESMO Range de novo — para
// sempre, e a legenda nunca vinha. Nao se sabe se o corte e um teto por pedido
// ou o CDN derrubando conexao a mais no mesmo link (o mkvass abria tres extras
// com o video tocando); os dois lados sao tratados:
//   - o que veio FICA, e o laco pede so o resto, do byte ini+recebidos;
//   - o host ganha um TETO de pedido (o que veio arredondado para baixo em
//     16 KB, no minimo REDE_CORTE_MIN), lembrado a sessao inteira: os pedidos
//     seguintes ja saem em pedacos que cabem, sem corte e com a conexao
//     reaproveitada;
//   - rede_corte_host conta ao mkvass, que passa a UMA conexao extra.
#define REDE_CORTE_MIN    (32L * 1024)
#define REDE_CORTE_HOSTS  8
// CORTES de um trecho antes de desistir. Conta so o pedaco que o servidor
// fechou no meio, nao o que ja saiu do tamanho do teto de proposito: um anexo
// de fontes de 5,6 MB (#92, fansub do relato de 25/09) sao 176 pedacos de
// 32 KB, e o limite antigo de 32 PEDACOS fazia o trecho falhar sempre, sem
// corte nenhum.
#define REDE_PEDACOS_MAX  32

// RESTO RECUSADO (#92, v1.4.7, webOS 25 + Real-Debrid): depois de um corte, o
// pedido do resto volta com ZERO bytes em ~140 ms, toda vez, e as tentativas
// de 2 s e 5 s so repetem a recusa. Quem chama le isto logo depois de
// rede_baixar_trecho_st, NO MESMO FIO, para recuar de verdade.
static _Thread_local int redeRestoRecusado;
int rede_resto_recusado(void) { return redeRestoRecusado; }

typedef struct { char h[96]; long teto; } CorteHost;
static CorteHost corteHost[REDE_CORTE_HOSTS];
static int corteProx;
static pthread_mutex_t corteTrava = PTHREAD_MUTEX_INITIALIZER;

// "https://a.b:443" de "https://a.b:443/x?y": o esquema e a porta contam.
static void corteHostDe(const char *u, char *h, size_t n) {
  const char *p = u ? strstr(u, "://") : NULL;
  size_t k;
  h[0] = 0;
  if (!p || n == 0) return;
  k = (size_t)(p + 3 - u) + strcspn(p + 3, "/?#");
  if (k >= n) k = n - 1;
  memcpy(h, u, k);
  h[k] = 0;
}

long rede_corte_host(const char *url) {
  char h[96];
  long t = 0;
  int i;
  corteHostDe(url, h, sizeof h);
  if (!h[0]) return 0;
  pthread_mutex_lock(&corteTrava);
  for (i = 0; i < REDE_CORTE_HOSTS; i++)
    if (corteHost[i].teto && !strcmp(corteHost[i].h, h)) { t = corteHost[i].teto; break; }
  pthread_mutex_unlock(&corteTrava);
  return t;
}

// O host de `url` cortou um 206 depois de `veio` bytes. So BAIXA o teto.
static void corteAprender(const char *url, long veio) {
  char h[96], seg[120];
  long teto = (veio / (16L * 1024)) * (16L * 1024);
  int i, achou = -1, mudou = 0;
  if (teto < REDE_CORTE_MIN) teto = REDE_CORTE_MIN;
  corteHostDe(url, h, sizeof h);
  if (!h[0]) return;
  pthread_mutex_lock(&corteTrava);
  for (i = 0; i < REDE_CORTE_HOSTS; i++)
    if (corteHost[i].teto && !strcmp(corteHost[i].h, h)) { achou = i; break; }
  if (achou < 0) {
    achou = corteProx; corteProx = (corteProx + 1) % REDE_CORTE_HOSTS;
    snprintf(corteHost[achou].h, sizeof corteHost[achou].h, "%s", h);
    corteHost[achou].teto = teto; mudou = 1;
  } else if (teto < corteHost[achou].teto) { corteHost[achou].teto = teto; mudou = 1; }
  pthread_mutex_unlock(&corteTrava);
  if (mudou) {
    printf("[rede] %s corta Range em %ld bytes: pedidos de ate %ld KB neste host daqui em diante\n",
           rede_url_publica(url, seg, sizeof seg), veio, teto / 1024);
    fflush(stdout);
  }
}

char *rede_baixar_trecho_st(const char *url, int segundos, long ini, long fim,
                            long *tam, int *status, int *erro,
                            char *final, unsigned tamFinal) {
  unsigned long t0 = redeAgoraMs(),
                prazo = (unsigned long)(segundos > 0 ? segundos : 30) * 1000UL;
  long pedido = fim - ini + 1, veio = 0;
  char *buf = NULL;
  int st = 0, e = 0, pedacos = 0, cortes = 0;
  // `atual`: o endereco dos pedacos seguintes (o final, depois do primeiro:
  // sem pagar o redirecionamento de novo). `fin`: o final de cada resposta.
  char atual[4096], fin[4096];
  if (tam) *tam = 0;
  if (status) *status = 0;
  if (erro) *erro = 0;
  if (final && tamFinal) final[0] = 0;
  redeRestoRecusado = 0;
  if (!url || pedido <= 0) return trechoUmaVez(url, segundos, ini, fim, tam, status, erro, final, tamFinal);
  snprintf(atual, sizeof atual, "%s", url);
  for (;;) {
    long a = ini + veio, b = fim, n = 0, teto = rede_corte_host(atual);
    int seg = segundos, cortado;
    char *r;
    if (teto <= 0) teto = rede_corte_host(url);
    if (teto > 0 && b - a + 1 > teto) b = a + teto - 1;
    if (pedacos > 0) {
      unsigned long gasto = redeAgoraMs() - t0;
      if (gasto + 1000UL > prazo) { e = e ? e : 28; goto falhou; }
      seg = (int)((prazo - gasto + 999UL) / 1000UL);
    }
    r = trechoUmaVez(atual, seg, a, b, &n, &st, &e, fin, sizeof fin);
    pedacos++;
    if (pedacos == 1 && final && tamFinal) snprintf(final, tamFinal, "%s", fin);
    // Pedaco seguinte que falhou sem trazer NADA, depressa (nao e o prazo): o
    // servidor recusou o resto. A diferenca para "rede lenta" e o que decide o
    // recuo longo no mkvass.
    if (!r && veio > 0 && n == 0 && e != 28) redeRestoRecusado = 1;
    if (!r) goto falhou;
    // Sem 206 o servidor ignorou o Range (200 com o comeco do arquivo): no
    // primeiro pedido e o contrato de sempre (quem chama recebe o que veio);
    // no meio de um trecho ja em pedacos, o que viria nao e o resto.
    if (st != 206) {
      if (pedacos == 1) { if (tam) *tam = n; if (status) *status = st; return r; }
      free(r); goto falhou;
    }
    cortado = e != 0;
    { char *nv = realloc(buf, (size_t)(veio + n + 1));
      if (!nv) { free(r); goto falhou; }
      buf = nv; memcpy(buf + veio, r, (size_t)n); veio += n; buf[veio] = 0; free(r); }
    if (cortado) {
      cortes++;
      corteAprender(atual, n);
      if (strcmp(atual, url)) corteAprender(url, n);
      printf("[rede] Range %ld+%ld: %ld de %ld bytes ate aqui, pedindo o resto (pedaco %d)\n",
             ini, pedido, veio, pedido, pedacos);
      fflush(stdout);
    }
    if (fin[0]) snprintf(atual, sizeof atual, "%s", fin);
    if (veio >= pedido) break;
    // Resposta completa e mais curta que o pedido: o arquivo acabou.
    if (!cortado && n < b - a + 1) break;
    if (cortes >= REDE_PEDACOS_MAX) goto falhou;
  }
  if (tam) *tam = veio;
  if (status) *status = 206;
  return buf;
falhou:
  // Sem progresso: o que ja veio se perde (quem chama pede o trecho de novo,
  // e o teto aprendido faz o pedido seguinte caber).
  if (veio > 0) {
    printf("[rede] Range %ld+%ld sem progresso depois de %ld bytes em %d pedaco(s) (HTTP %d, curl %d%s)\n",
           ini, pedido, veio, pedacos, st, e,
           redeRestoRecusado ? ", o servidor recusou o resto" : "");
    fflush(stdout);
  }
  free(buf);
  if (tam) *tam = 0;
  if (status) *status = st;
  if (erro) *erro = e ? e : (veio > 0 ? 18 : 0);
  return NULL;
}

char *rede_baixar_trecho(const char *url, int segundos, long ini, long fim,
                         long *tam) {
  return rede_baixar_trecho_st(url, segundos, ini, fim, tam, NULL, NULL, NULL, 0);
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
