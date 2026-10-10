#include "rede.h"
#ifdef NV_ANDROID
#include "android.h"
#endif
#include "negcache.h"
#include "addonurl.h"
#include "negcache.inc"
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <dlfcn.h>
#include <time.h>
#include <limits.h>
#include <ctype.h>

/* Jobs nao dependem de TLS nem dos controles antigos por fio. O grupo e
 * retido pelos jobs, e a requisicao retem seu job ate terminar. */
struct RedeGrupo {
  pthread_mutex_t trava;
  unsigned refs;
  uint64_t geracao;
  int cancelado;
};
struct RedeJob {
  RedeGrupo *grupo;
  unsigned refs;
  uint64_t geracao;
  int cancelado; /* protegido pela trava do grupo */
};
RedeGrupo *rede_grupo_criar(void) {
  RedeGrupo *g = calloc(1, sizeof *g);
  if (!g) return NULL;
  if (pthread_mutex_init(&g->trava, NULL)) { free(g); return NULL; }
  g->refs = 1; g->geracao = 1;
  return g;
}
void rede_grupo_soltar(RedeGrupo *g) {
  if (g && __sync_sub_and_fetch(&g->refs, 1) == 0) {
    pthread_mutex_destroy(&g->trava); free(g);
  }
}
uint64_t rede_grupo_avancar(RedeGrupo *g) {
  uint64_t v;
  if (!g) return 0;
  pthread_mutex_lock(&g->trava);
  if (g->geracao == UINT64_MAX) g->cancelado = 1;
  else g->geracao++;
  v = g->geracao;
  pthread_mutex_unlock(&g->trava);
  return v;
}
void rede_grupo_cancelar(RedeGrupo *g) {
  if (!g) return;
  pthread_mutex_lock(&g->trava); g->cancelado = 1;
  pthread_mutex_unlock(&g->trava);
}
RedeJob *rede_job_criar(RedeGrupo *g) {
  RedeJob *j;
  if (!g) return NULL;
  j = calloc(1, sizeof *j);
  if (!j) return NULL;
  __sync_add_and_fetch(&g->refs, 1);
  pthread_mutex_lock(&g->trava);
  j->geracao = g->geracao; j->cancelado = g->cancelado;
  pthread_mutex_unlock(&g->trava);
  j->grupo = g; j->refs = 1;
  return j;
}
void rede_job_reter(RedeJob *j) { if (j) __sync_add_and_fetch(&j->refs, 1); }
void rede_job_soltar(RedeJob *j) {
  if (j && __sync_sub_and_fetch(&j->refs, 1) == 0) {
    rede_grupo_soltar(j->grupo); free(j);
  }
}
void rede_job_cancelar(RedeJob *j) {
  if (!j) return;
  pthread_mutex_lock(&j->grupo->trava); j->cancelado = 1;
  pthread_mutex_unlock(&j->grupo->trava);
}
RedeErro rede_job_estado(RedeJob *j) {
  RedeErro e = REDE_OK;
  if (!j) return e;
  pthread_mutex_lock(&j->grupo->trava);
  if (j->cancelado || j->grupo->cancelado) e = REDE_CANCELADO;
  else if (j->geracao != j->grupo->geracao) e = REDE_GERACAO;
  pthread_mutex_unlock(&j->grupo->trava);
  return e;
}
#ifndef __EMSCRIPTEN__
static uint64_t pedidoAgoraMs(void) {
  struct timespec t;
  if (clock_gettime(CLOCK_MONOTONIC, &t)) return 0;
  return (uint64_t)t.tv_sec * 1000u + (unsigned long)t.tv_nsec / 1000000u;
}
#endif
void rede_resposta_limpar(RedeResposta *r) {
  if (!r) return;
  free(r->corpo); free(r->cabecalhos); memset(r, 0, sizeof *r);
}

#if defined(NV_TPK40) && defined(__EMSCRIPTEN__)
#error "NV_TPK40 e so da libnuvio.so do Tizen 4/5; nunca junto com Emscripten"
#endif

#ifdef NV_TPK40
// ESTADO POR FIO SEM _Thread_local (Tizen 4/5, #137/#180).
//
// A libnuvio.so dessas TVs pode entrar pelo carregador de ELF proprio do host
// (tizen-tpk/NuvioTpk40/Program40.cs), quando a UEP recusa o dlopen por memfd.
// Esse carregador nao monta TLS de compilador: um `_Thread_local` vira um
// PT_TLS e relocacoes R_ARM_TLS_DTPMOD32 que ele nao sabe aplicar, e o
// primeiro acesso — que e a primeira requisicao HTTPS, num fio novo — derruba
// o processo. Era a unica TLS de compilador de toda a lib (este modulo).
//
// Aqui o mesmo estado vive numa struct por fio, criada no primeiro uso via
// pthread_key (o proprio pegarHandle ja usa a mesma tecnica) e liberada pelo
// destrutor da chave quando o fio morre. Cada nome antigo vira uma macro que
// e um lvalue, entao o resto do arquivo nao muda: `rede_teto = x`,
// `sizeof usoHost` e `&redeCancelLocal` continuam validos. Fora do NV_TPK40
// (LG, Mac, testes, Tizen 6+) o codigo e o de sempre, com _Thread_local.
#define REDE_HOSTS_POR_FIO 16
typedef struct { char h[96]; unsigned long ms; } UsoHost;
typedef struct {
  long limiteLocal;
  volatile int *cancelLocal;
  int limitouLocal;
  int cancelouLocal;
  long bytesLocal;
  int curlLocal;
  char *finalDst;
  unsigned finalTam;
  int parcialOk;
  UsoHost usoHost[REDE_HOSTS_POR_FIO];
  long teto;
  int restoRecusado;
  int lateral;
  char erroTxt[200];
  char erroBuf[256];
} RedeFio;
static pthread_key_t fioChave;
static pthread_once_t fioUma = PTHREAD_ONCE_INIT;
// Se o calloc falhar (sem memoria), o fio usa esta reserva PARTILHADA em vez
// de desreferenciar NULL: degrada a medicao daquele pedido, nao derruba o app.
static RedeFio fioReserva;
static void fioSoltar(void *p) { if (p != &fioReserva) free(p); }
static void fioCriarChave(void) { pthread_key_create(&fioChave, fioSoltar); }
static RedeFio *redeFio(void) {
  RedeFio *f;
  pthread_once(&fioUma, fioCriarChave);
  f = (RedeFio *)pthread_getspecific(fioChave);
  if (!f) {
    f = (RedeFio *)calloc(1, sizeof *f);
    if (!f) return &fioReserva;
    pthread_setspecific(fioChave, f);
  }
  return f;
}
long *rede_teto_ptr(void) { return &redeFio()->teto; }
#define redeLimiteLocal   (redeFio()->limiteLocal)
#define redeCancelLocal   (redeFio()->cancelLocal)
#define redeLimitouLocal  (redeFio()->limitouLocal)
#define redeCancelouLocal (redeFio()->cancelouLocal)
#define redeBytesLocal    (redeFio()->bytesLocal)
#define redeCurlLocal     (redeFio()->curlLocal)
#define redeFinalDst      (redeFio()->finalDst)
#define redeFinalTam      (redeFio()->finalTam)
#define redeParcialOk     (redeFio()->parcialOk)
#define usoHost           (redeFio()->usoHost)
#define redeRestoRecusado (redeFio()->restoRecusado)
#define redeLateral       (redeFio()->lateral)
#else
/* Controle local da requisicao corrente. O estado nunca e compartilhado
 * entre sondagens: cada fio recebe seu teto e seu cancel token. */
static _Thread_local long redeLimiteLocal;
static _Thread_local volatile int *redeCancelLocal;
static _Thread_local int redeLimitouLocal;
static _Thread_local int redeCancelouLocal;
static _Thread_local long redeBytesLocal;
// 1 = este fio e leitura lateral ao video (rede_lateral, #385).
static _Thread_local int redeLateral;
#endif
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
                        char *etag, int etagTam, int *retryAfter), {
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
  if (retryAfter) {
    // null significa ausente OU oculto pelo CORS, nao um prazo inventado.
    var raw = "", wait = 0;
    try { raw = (xhr.getResponseHeader("retry-after") || "").trim(); } catch (e) {}
    if (/^[0-9]+$/.test(raw)) {
      wait = Math.min(2147483647, Number(raw));
    } else if (/^(Mon|Tue|Wed|Thu|Fri|Sat|Sun)/i.test(raw)) {
      var until = Date.parse(raw);
      if (isFinite(until)) wait = Math.max(0, Math.ceil((until - Date.now()) / 1000));
    }
    HEAP32[retryAfter >> 2] = Math.min(2147483647, wait);
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
unsigned rede_pedido_capacidades(void) { return 0; }
int rede_pedir(const RedePedido *p, RedeResposta *r) {
  (void)p;
  if (!r) return 0;
  memset(r, 0, sizeof *r);
  r->erro = REDE_INDISPONIVEL;
  return 0; /* nenhum XHR: nao prometer teto, timeout ou cancel inexistentes */
}

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
// Ouvinte da saude da rede — ver rede_avisar_saude no cabecalho.
static void (*avisoSaude)(int codigo, const char *url);
void rede_avisar_saude(void (*f)(int codigo, const char *url)) { avisoSaude = f; }
// Ouvinte do pedido por host (o painel de registro, aba Rede) — ver rede.h.
static void (*avisoHost)(const char *url, int codigo, int http, unsigned ms);
void rede_avisar_host(void (*f)(const char *url, int codigo, int http, unsigned ms)) { avisoHost = f; }

static char *pedir2(const char *metodo, const char *url, const char *const *cab,
                    const char *extraCab, const char *corpo,
                    long *tam, int *status, char *etag, unsigned tamEtag,
                    int *retryAfter) {
  char *cabs, *corpoResp;
  int n = 0, http = 0;
  if (etag && tamEtag) etag[0] = 0;
  if (status) *status = 0;
  if (retryAfter) *retryAfter = 0;
  if (!url || !*url) return NULL;
  if (!status && (!metodo || !strcmp(metodo, "GET")) && negcache_barra(url, (long)time(NULL))) return NULL;
  cabs = juntarCabs(cab, extraCab);
  corpoResp = nv_http(metodo, url, cabs, corpo, &n, &http,
                      redeFinalDst, (int)redeFinalTam, etag, (int)tamEtag, retryAfter);
  free(cabs);
  if (status) *status = http;
  // XHR sem status e sem corpo = nao houve resposta (rede, DNS, CORS): para a
  // saude da rede e o "6" do curl. Qualquer status e transporte que funcionou.
  if (avisoSaude) avisoSaude(corpoResp || http ? 0 : 6, url);
  if (avisoHost) avisoHost(url, corpoResp || http ? 0 : 6, http, 0);
  if (http == 401 && aviso401) aviso401(url);
  negcache_nota(url, corpoResp || http ? 0 : 6, http, (long)time(NULL));
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
  return pedir2(metodo, url, cab, extraCab, corpo, tam, status, NULL, 0, NULL);
}

char *rede_baixar_etag(const char *url, int segundos, const char *const *cab,
                       int *status, char *etag, unsigned tamEtag) {
  (void)segundos;
  return pedir2("GET", url, cab, NULL, NULL, NULL, status, etag, tamEtag, NULL);
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
char *rede_baixar_st_retry(const char *url, int segundos, const char *const *cab,
                           int *status, int *retryAfter) {
  (void)segundos;
  return pedir2("GET", url, cab, NULL, NULL, NULL, status, NULL, 0, retryAfter);
}

// UM pedido de Range (o laco em pedacos e rede_baixar_trecho_st, no fim do
// arquivo). XHR nao entrega corpo cortado: uma conexao que fecha antes do
// Content-Length e erro de rede e o corpo some — aqui nunca ha "parcial".
static char *trechoUmaVez(const char *url, int segundos, long long ini, long long fim,
                          long *tam, int *status, int *erro,
                          char *final, unsigned tamFinal) {
  char faixa[80];
  const char *cab[2];
  char *r;
  int st = 0;
  (void)segundos;
  if (erro) *erro = 0;
  if (final && tamFinal) final[0] = 0;
  snprintf(faixa, sizeof faixa, "Range: bytes=%lld-%lld", ini, fim);
  cab[0] = faixa; cab[1] = NULL;
  rede_teto = (long)(fim - ini + 1);
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

void rede_discord_ca(const char *caminho) { (void)caminho; }
const char *rede_ultimo_erro(void) { return ""; }

char *rede_postar_seguro_st(const char *url, int segundos, const char *const *cab,
                            const char *corpo, int *status) {
  if (status) *status = 0;
  if (!url || strncmp(url, "https://", 8)) return NULL;
  return rede_postar_st(url, segundos, cab, corpo, status);
}

// Sonda sem corpo atravessando a ponte. O XHR ainda recebe o corpo inteiro se
// o servidor ignorar Range, mas nao aloca essa copia no heap do WASM.
EM_JS(int, nv_url_sonda, (const char *url, const char *cabs, char *dst,
                         int tam, int *status, char *mime, int mimeTam, long *corpo), {
  var xhr = new XMLHttpRequest();
  if (status) HEAP32[status >> 2] = 0;
  try {
    xhr.open("GET", UTF8ToString(url), false);
    if (cabs) UTF8ToString(cabs).split("\n").forEach(function (linha) {
      var i = linha.indexOf(":");
      if (i <= 0) return;
      try { xhr.setRequestHeader(linha.slice(0, i).trim(), linha.slice(i + 1).trim()); } catch (e) {}
    });
    xhr.setRequestHeader("Range", "bytes=0-63");
    xhr.send(null);
  } catch (e) { return 0; }
  if (status) HEAP32[status >> 2] = xhr.status;
  if (mime && mimeTam > 0) {
    var ct = (xhr.getResponseHeader("Content-Type") || "").split(";")[0].trim().toLowerCase();
    stringToUTF8(ct, mime, mimeTam);
  }
  if (corpo) {
    // Pelo Content-Length, sem ler o corpo (nao copia midia para o JS).
    var cl = xhr.getResponseHeader("Content-Length");
    var len = (xhr.status === 200 && cl !== null && /^\d+$/.test(cl.trim())) ? parseInt(cl, 10) : -1;
    HEAP32[corpo >> 2] = len > 64 ? -1 : len;
  }
  var finalUrl = xhr.responseURL || "";
  var bytes = 0;
  for (var k = 0; k < finalUrl.length; k++) {
    var code = finalUrl.charCodeAt(k);
    if (code >= 0xD800 && code <= 0xDBFF && k + 1 < finalUrl.length) { bytes += 4; k++; }
    else bytes += code < 0x80 ? 1 : code < 0x800 ? 2 : 3;
  }
  if (xhr.status < 200 || xhr.status >= 300 || !finalUrl || bytes >= tam) return 0;
  stringToUTF8(finalUrl, dst, tam);
  return 1;
});

int rede_url_final_cab(const char *url, int segundos, const char *const *cab,
                       char *dst, unsigned tam, int *status) {
  return rede_url_final_tipo(url, segundos, cab, dst, tam, status, NULL, 0, NULL);
}

int rede_url_final_tipo(const char *url, int segundos, const char *const *cab,
                        char *dst, unsigned tam, int *status,
                        char *mime, unsigned mimeTam, long *corpo) {
  char *cabs;
  int ok, http = 0;
  long c32 = -1;
  (void)segundos;
  if (status) *status = 0;
  if (mime && mimeTam) mime[0] = 0;
  if (corpo) *corpo = -1;
  if (dst && tam) dst[0] = 0;
  if (!url || !*url || !dst || !tam || tam > INT_MAX) return 0;
  // Um pedaco minusculo em vez de HEAD, pelo mesmo motivo do outro caminho:
  // servidores de debrid respondem HEAD com 405 ou mentem no redirecionamento,
  // mas honram Range.
  cabs = juntarCabs(cab, NULL);
  if (cab && cab[0] && !cabs) return 0;
  ok = nv_url_sonda(url, cabs, dst, (int)tam, &http, mime,
                    mime && mimeTam < INT_MAX ? (int)mimeTam : 0, &c32);
  free(cabs);
  if (corpo) *corpo = c32;
  if (status) *status = http;
  return ok;
}

int rede_url_final(const char *url, int segundos, char *dst, unsigned tam) {
  return rede_url_final_cab(url, segundos, NULL, dst, tam, NULL);
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

void rede_vazao_espera(unsigned long ms) { (void)ms; }

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

// Aquecer conexao: o navegador gerencia as dele, nao ha o que abrir daqui.
int rede_aquecer_lote(const char *const *origens, int n, unsigned *ms) {
  (void)origens; (void)n; (void)ms;
  return 0;
}

// Navegador nao abre socket. O websocket do Tizen web usa o WebSocket do
// proprio navegador (discordws.c); estes existem para o modulo linkar.
RedeTls *rede_tls_abrir(const char *url, int segundos) { (void)url; (void)segundos; return NULL; }
int rede_tls_enviar(RedeTls *t, const void *buf, size_t n) { (void)t; (void)buf; (void)n; return -1; }
int rede_tls_tentar_enviar(RedeTls *t, const void *buf, size_t n, size_t *foi) { (void)t; (void)buf; (void)n; if (foi) *foi = 0; return -1; }
int rede_tls_receber(RedeTls *t, void *buf, size_t n, int esperaMs) { (void)t; (void)buf; (void)n; (void)esperaMs; return -1; }
void rede_tls_fechar(RedeTls *t) { (void)t; }

#else

#ifndef NV_TPK40   // no NV_TPK40 estes nomes sao macros da struct por fio (topo)
// Codigo da libcurl do ultimo pedido DESTE fio (0 = transporte ok). So o
// rede_baixar_trecho_st le: o resto do modulo segue devolvendo NULL e logando.
static _Thread_local int redeCurlLocal;
#endif
// Texto do ultimo erro de TRANSPORTE deste fio ("curl 35: ..."), para a tela de
// login e o log do APK de release (#223): o stderr da libcurl nao chega a lugar
// nenhum la. Sem segredo: a libcurl so descreve a falha, nunca o corpo.
#ifdef NV_TPK40
// 4/5: sem TLS de compilador (ver o RedeFio la em cima).
#define redeErroTxt (redeFio()->erroTxt)
#define redeErroBuf (redeFio()->erroBuf)
#else
static _Thread_local char redeErroTxt[200];
static _Thread_local char redeErroBuf[256];   // CURLOPT_ERRORBUFFER (>= 256 bytes)
#endif
// CA bundle (Mozilla, art/discord-ca.pem) configured once at startup. Android's
// libcurl+mbedTLS is built with no CA path at all.
static char discordCa[600];
#ifndef NV_TPK40
// Destino do endereco final do proximo pedido (so rede_baixar_trecho_st liga).
static _Thread_local char *redeFinalDst;
static _Thread_local unsigned redeFinalTam;
// 1 = o pedido corrente aceita corpo CORTADO (206 que fechou antes do fim):
// so trechoUmaVez liga. Ver a nota la.
static _Thread_local int redeParcialOk;
#endif

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
// CURLINFO_TOTAL_TIME = CURLINFO_DOUBLE (0x300000) + 3: segundos do pedido.
#define INFO_TEMPO_TOTAL     3145731
// CURLINFO_NAMELOOKUP/CONNECT/APPCONNECT_TIME = CURLINFO_DOUBLE + 4/5/33.
#define INFO_TEMPO_DNS       3145732
#define INFO_TEMPO_TCP       3145733
#define INFO_TEMPO_TLS       3145761
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
#define INFO_REDIRECT_URL 1048607
#define INFO_SIZE_DOWNLOAD 3145736
#define OPT_CAINFO 10065
#define OPT_ERRORBUFFER 10010
#define OPT_POSTFIELDSIZE 60
#define OPT_PROTOCOLS 181

// Ouvinte unico dos 401 — ver rede_avisar_401 no cabecalho. (O ramo
// Emscripten tem a sua propria definicao, porque os dois lados do #ifdef
// compilam separado.)
static void (*aviso401)(const char *url);
void rede_avisar_401(void (*f)(const char *url)) { aviso401 = f; }
// Ouvinte da saude da rede — ver rede_avisar_saude no cabecalho.
static void (*avisoSaude)(int codigo, const char *url);
void rede_avisar_saude(void (*f)(int codigo, const char *url)) { avisoSaude = f; }
// Ouvinte do pedido por host (o painel de registro, aba Rede) — ver rede.h.
static void (*avisoHost)(const char *url, int codigo, int http, unsigned ms);
void rede_avisar_host(void (*f)(const char *url, int codigo, int http, unsigned ms)) { avisoHost = f; }

static void *(*curl_init)(void);
static int   (*curl_setopt_f)(void *, int, ...);
// #203: o OPT_URL passa por aqui para um APELIDO de addon com URL grande
// (addonurl.h) voltar a ser a URL inteira. O curl copia a string no setopt,
// entao a expandida e liberada logo depois. Macro, e nao funcao variadica:
// cada ramo recebe o valor com o tipo que ja tinha.
// #416/#419: um alias oficial, ligado para todos os fios so apos responder.
static int tmdbAlternativo;
static char *tmdbUrl(const char *url) {
  const char *h, *fim;
  char *nova;
  size_t prefixo;
  if (!url || strncasecmp(url, "https://", 8)) return NULL;
  h = url + 8;
  fim = h + strcspn(h, "/?#");
  // So o host exato; preserva esquema, porta, caminho e query byte a byte.
  if (strncasecmp(h, "api.themoviedb.org", 18) ||
      (h + 18 != fim && (h[18] != ':' || h + 19 == fim ||
       h + 19 + strspn(h + 19, "0123456789") != fim))) return NULL;
  prefixo = (size_t)(h - url);
  nova = malloc(strlen(url) + 1);
  if (!nova) return NULL;
  memcpy(nova, url, prefixo);
  strcpy(nova + prefixo, "api.tmdb.org");
  strcat(nova, h + 18);
  return nova;
}
static void tmdbLigou(void) {
  if (!__atomic_exchange_n(&tmdbAlternativo, 1, __ATOMIC_ACQ_REL)) {
    printf("[rede] api.themoviedb.org bloqueado aqui; usando api.tmdb.org\n");
    fflush(stdout);
  }
}
static int urlSetopt(void *c, const char *url) {
  char *grande = nv_longa_expandir(url);
  char *alias = __atomic_load_n(&tmdbAlternativo, __ATOMIC_ACQUIRE) ?
                tmdbUrl(grande ? grande : url) : NULL;
  int r = curl_setopt_f(c, 10002 /* OPT_URL */, alias ? alias : grande ? grande : url);
  free(alias); free(grande);
  return r;
}
#define curl_setopt(c, op, v) ((op) == 10002 ? urlSetopt((c), (const char *)(uintptr_t)(v)) \
                                              : curl_setopt_f((c), (op), (v)))
static int   (*curl_perform)(void *);
static void  (*curl_cleanup)(void *);
static int   (*curl_global)(long);
static void *(*slist_append)(void *, const char *);
static void  (*slist_free)(void *);
static int   (*curl_getinfo)(void *, int, ...);
static void  (*curl_reset)(void *);
static time_t (*curl_getdate_fn)(const char *, const time_t *);

// Chamado depois da repeticao existente do GET, diretamente nos demais caminhos.
static int tmdbFallback(void *c, int r) {
  char *url = NULL, *alias;
  long http = 0;
  if (!(r == 35 || r == 7 || r == 28 || r == 56 || r == 52) ||
      redeCancelouLocal || !curl_getinfo) return r;
  if (curl_getinfo(c, INFO_RESPONSE_CODE, &http) || http ||
      curl_getinfo(c, INFO_URL_FINAL, &url) || !(alias = tmdbUrl(url))) return r;
  if (!curl_setopt_f(c, OPT_URL, alias)) {
    r = curl_perform(c); /* uma unica tentativa, sem recursao */
    if (!r) tmdbLigou();
  }
  free(alias);
  return r;
}
static int performTmdb(void *c) { return tmdbFallback(c, curl_perform(c)); }

static int retrySegundos(const char *s, size_t n) {
  char b[128], *fim;
  unsigned long long v;
  time_t ate, agora;
  while (n && (*s == ' ' || *s == '\t')) { s++; n--; }
  while (n && isspace((unsigned char)s[n - 1])) n--;
  if (!n || n >= sizeof b) return 0;
  memcpy(b, s, n); b[n] = 0;
  if (isdigit((unsigned char)b[0])) {
    size_t i;
    for (i = 0; i < n; i++) if (!isdigit((unsigned char)b[i])) return 0;
    v = strtoull(b, &fim, 10);
    return *fim ? 0 : v > INT_MAX ? INT_MAX : (int)v;
  }
  /* curl_getdate conhece as tres formas de HTTP-date, tambem no curl antigo. */
  if (!curl_getdate_fn) return 0;
  ate = curl_getdate_fn(b, NULL); agora = time(NULL);
  if (ate == (time_t)-1 || ate <= agora) return 0;
  return difftime(ate, agora) > INT_MAX ? INT_MAX : (int)difftime(ate, agora);
}
// Conexao crua (CONNECT_ONLY): so rede_tls_* usam. Existem desde a 7.18.2, entao
// a libcurl 7.53.1 das TVs tem; o que ela NAO tem e websocket (7.86+), e por
// isso o quadro do websocket e feito a mao em discordws.c.
static int   (*curl_send)(void *, const void *, size_t, size_t *);
static int   (*curl_recv)(void *, void *, size_t, size_t *);
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
#ifndef NV_TPK40   // no NV_TPK40 o vetor mora na struct por fio (topo)
#define REDE_HOSTS_POR_FIO 16
typedef struct { char h[96]; unsigned long ms; } UsoHost;
static _Thread_local UsoHost usoHost[REDE_HOSTS_POR_FIO];
#endif

// "https://a.b:443" de "https://a.b:443/x?y". Sem esquema = vazio.
static void hostDaUrl(const char *u, char *h, size_t n) {
  const char *p = u ? strstr(u, "://") : NULL;
  size_t k;
  h[0] = 0;
  if (!p || n == 0) return;
  p += 3;
  // Userinfo (`usuario:senha@`) NUNCA vai para o log nem para a chave de host:
  // addons IPTV carregam credencial ali. Guarda esquema + host(+porta).
  { const char *fim = p + strcspn(p, "/?#"), *q, *at = NULL;
    size_t e = (size_t)(p - u);
    for (q = p; q < fim; q++) if (*q == '@') at = q;
    if (at) p = at + 1;
    k = (size_t)(fim - p);
    if (e + k >= n) { if (e >= n) e = n - 1; k = n - 1 - e; }
    memcpy(h, u, e);
    memcpy(h + e, p, k);
    h[e + k] = 0;
  }
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

// HANDLES AQUECIDOS (2.0.2). O handle e do FIO, e o fio que confere a fonte
// nasce a cada escolha: uma conexao aberta antes por outro fio morreria com ele.
// rede_aquecer_lote abre DNS + TCP + TLS dos hosts que vem a seguir e ESTACIONA o
// handle aqui; o primeiro fio que pedir um destes hosts sem ter handle proprio o
// adota (curl_easy_reset preserva as conexoes). So adota dentro de
// REDE_ESTAC_VALE_MS: conexao parada demais pode ter morrido sem aviso (ver
// REDE_OCIOSO_PADRAO_MS acima), e ai o pedido esperaria o prazo inteiro.
#define REDE_ESTAC_MAX 3
#define REDE_ESTAC_HOSTS 6
#define REDE_ESTAC_VALE_MS 15000UL
typedef struct { void *c; unsigned long ms; int nh; char h[REDE_ESTAC_HOSTS][96]; } Estac;
static Estac estac[REDE_ESTAC_MAX];
static pthread_mutex_t estacTrava = PTHREAD_MUTEX_INITIALIZER;
// Chamar com estacTrava: joga fora os que passaram da validade.
static void estacVelhos(unsigned long agora) {
  int i;
  for (i = 0; i < REDE_ESTAC_MAX; i++)
    if (estac[i].c && agora - estac[i].ms > REDE_ESTAC_VALE_MS) {
      if (curl_cleanup) curl_cleanup(estac[i].c);
      estac[i].c = NULL;
    }
}
static void estacionar(void *c, char h[][96], int nh) {
  unsigned long agora = redeAgoraMs();
  int i, livre = -1, velho = 0;
  pthread_mutex_lock(&estacTrava);
  estacVelhos(agora);
  for (i = 0; i < REDE_ESTAC_MAX; i++) {
    if (!estac[i].c) { livre = i; break; }
    if (estac[i].ms < estac[velho].ms) velho = i;
  }
  if (livre < 0) { livre = velho; if (curl_cleanup) curl_cleanup(estac[livre].c); }
  estac[livre].c = c; estac[livre].ms = agora; estac[livre].nh = nh;
  for (i = 0; i < nh && i < REDE_ESTAC_HOSTS; i++) snprintf(estac[livre].h[i], 96, "%s", h[i]);
  pthread_mutex_unlock(&estacTrava);
}
static void *estacTomar(const char *url) {
  char h[96];
  unsigned long agora = redeAgoraMs();
  void *c = NULL;
  int i, k;
  hostDaUrl(url, h, sizeof h);
  if (!h[0]) return NULL;
  pthread_mutex_lock(&estacTrava);
  estacVelhos(agora);
  for (i = 0; i < REDE_ESTAC_MAX && !c; i++)
    for (k = 0; estac[i].c && k < estac[i].nh; k++)
      if (!strcmp(estac[i].h[k], h)) { c = estac[i].c; estac[i].c = NULL; break; }
  pthread_mutex_unlock(&estacTrava);
  return c;
}

static void *pegarHandle(const char *url) {
  void *c;
  if (!curl_reset) return curl_init();     // libcurl sem reset: como antes
  pthread_once(&handleUma, handleCriarChave);
  c = pthread_getspecific(handleChave);
  if (!c) {
    c = estacTomar(url);
    if (c) pthread_setspecific(handleChave, c);
  }
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
  // TODO pedido da libcurl passa por aqui com o CURLcode: e o ponto unico da
  // saude da rede (redesaude.h decide o que e transporte).
  if (avisoSaude) avisoSaude(r, url);
  if (avisoHost) {
    long http = 0;
    double seg = 0;
    if (curl_getinfo) { curl_getinfo(c, INFO_RESPONSE_CODE, &http); curl_getinfo(c, INFO_TEMPO_TOTAL, &seg); }
    avisoHost(url, r, (int)http, seg > 0 ? (unsigned)(seg * 1000.0 + 0.5) : 0u);
  }
#ifdef NV_ANDROID
  // #266/#332: tempos dos primeiros pedidos e de toda falha — DNS, TCP e TLS
  // separados (CURLINFO_*_TIME, acumulados desde o inicio). So o host vai ao log.
  { static int vistos;
    int n = __atomic_add_fetch(&vistos, 1, __ATOMIC_RELAXED);
    if ((n <= 6 || r != 0) && curl_getinfo) {
      double dns = 0, tcp = 0, tls = 0, tot = 0;
      long http = 0, novas = -1;
      char h[96];
      curl_getinfo(c, INFO_TEMPO_DNS, &dns); curl_getinfo(c, INFO_TEMPO_TCP, &tcp);
      curl_getinfo(c, INFO_TEMPO_TLS, &tls); curl_getinfo(c, INFO_TEMPO_TOTAL, &tot);
      curl_getinfo(c, INFO_RESPONSE_CODE, &http); curl_getinfo(c, INFO_NUM_CONNECTS, &novas);
      hostDaUrl(url, h, sizeof h);
      printf("[rede] tempo #%d %s curl=%d http=%ld dns=%.0f tcp=%.0f tls=%.0f total=%.0f ms conexoes_novas=%ld\n",
             n, h[0] ? h : "?", r, http, dns * 1000, tcp * 1000, tls * 1000, tot * 1000, novas);
      fflush(stdout);
    }
  }
#endif
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
  // Bundle embarcado como fallback: a verificacao esta desligada acima, mas se
  // um dia ligar (ou a lib ignorar o VERIFYPEER) nao ha "sem CA" no Android.
  if (discordCa[0]) curl_setopt(c, OPT_CAINFO, discordCa);
  redeErroBuf[0] = 0;
  redeErroTxt[0] = 0;
  curl_setopt(c, OPT_ERRORBUFFER, redeErroBuf);
}

// Guarda o texto da falha (so o codigo e a frase da libcurl, sem URL nem corpo).
static void anotarErro(int r) {
  if (!r) { redeErroTxt[0] = 0; return; }
  snprintf(redeErroTxt, sizeof redeErroTxt, "curl %d%s%.150s", r,
           redeErroBuf[0] ? ": " : "", redeErroBuf);
}
const char *rede_ultimo_erro(void) { return redeErroTxt; }

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

typedef struct { char *p; size_t n, cap; } Balde;

/* Durante o download a capacidade cresce geometricamente. Antes de devolver
 * o corpo, solta a folga: caches contam o tamanho recebido, nao a capacidade. */
static char *baldeFinal(Balde *b) {
  if (b->p && b->cap > b->n + 1) {
    char *p = realloc(b->p, b->n + 1);
    if (p) { b->p = p; b->cap = b->n + 1; }
  }
  return b->p;
}

static char *rede_baixar_interno(const char *url, int segundos, long *tam,
                                 const char *const *cab);
static char *rede_baixar_interno2(const char *url, int segundos, long *tam,
                                  const char *const *cab, int *status,
                                  char *etag, unsigned tamEtag);
static char *rede_baixar_interno3(const char *url, int segundos, long *tam,
                                  const char *const *cab, int *status,
                                  char *etag, unsigned tamEtag, int *retry);

// Onde o ETag da resposta e anotado, quando alguem o pediu.
typedef struct { char *dst; unsigned tam; int *retry; } CacaCab;

// A libcurl entrega UMA linha de cabecalho por chamada, com o CRLF no fim. So
// o ETag interessa; devolver menos bytes do que recebeu abortaria a
// transferencia, entao o retorno e sempre o tamanho inteiro.
static size_t receberCab(void *dados, size_t tam, size_t qtd, void *u) {
  CacaCab *c = (CacaCab *)u;
  const char *s = (const char *)dados;
  size_t bytes = tam * qtd, n;
  if (bytes >= 5 && !strncasecmp(s, "HTTP/", 5)) {
    if (c && c->retry) *c->retry = 0;
    if (c && c->dst && c->tam) c->dst[0] = 0;
  }
  if (c && c->retry && bytes > 12 && !strncasecmp(s, "retry-after:", 12)) {
    *c->retry = retrySegundos(s + 12, bytes - 12);
  }
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
#ifndef NV_TPK40   // no NV_TPK40 e rede_teto_ptr() (rede.h)
_Thread_local long rede_teto = 0;
#endif

static size_t receber(void *dados, size_t tam, size_t qtd, void *u) {
  Balde *b = (Balde *)u;
  size_t bytes, necessario;
  long limite = redeLimiteAtual();
  if (tam && qtd > SIZE_MAX / tam) return 0;
  bytes = tam * qtd;
  if (!bytes) return 0;
  if (redeCancelLocal && *redeCancelLocal) {
    redeCancelouLocal = 1;
    return 0;
  }
  if (limite > 0 && b->n >= (size_t)limite) {
    redeLimitouLocal = 1;
    return 0;
  }
  if (limite > 0 && bytes > (size_t)limite - b->n) {
    bytes = (size_t)limite - b->n;
    redeLimitouLocal = 1;
  }
  // O contrato publico expoe `long` para o tamanho, inclusive no ARM 32-bit.
  if (bytes > (size_t)LONG_MAX || b->n > (size_t)LONG_MAX - bytes ||
      b->n + bytes == SIZE_MAX) return 0;
  necessario = b->n + bytes + 1;
  if (necessario > b->cap) {
    size_t cap = b->cap ? b->cap : 4096;
    char *novo;
    while (cap < necessario) {
      if (cap > SIZE_MAX / 2) { cap = necessario; break; }
      cap *= 2;
    }
    if (limite > 0 && cap > (size_t)limite + 1) cap = (size_t)limite + 1;
    novo = realloc(b->p, cap);
    if (!novo) return 0;              // devolver 0 aborta a transferencia
    b->p = novo; b->cap = cap;
  }
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

#ifdef NV_ANDROID
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <sys/utsname.h>
// #266/#332: o curl_global_init da libcurl do APK semeia o PSA e o CTR-DRBG do
// mbedTLS. Ate a 266.1 a semente vinha de /dev/random (o mbedTLS so usa
// getrandom com glibc), que BLOQUEIA em kernel < 5.6 com pouca entropia
// estimada. A deps.sh agora aponta para /dev/urandom; esta linha diz, no log, o
// kernel e se /dev/random estava travado naquela hora — prova ou derruba a tese.
static void entropiaLog(void) {
  struct utsname u;
  char ea[32] = "?";
  const char *rnd = "?";
  FILE *f = fopen("/proc/sys/kernel/random/entropy_avail", "r");
  int fd;
  if (f) { if (!fgets(ea, sizeof ea, f)) snprintf(ea, sizeof ea, "?"); fclose(f); }
  ea[strcspn(ea, "\r\n")] = 0;
  fd = open("/dev/random", O_RDONLY | O_NONBLOCK | O_CLOEXEC);
  if (fd >= 0) {
    struct pollfd p = { fd, POLLIN, 0 };
    int r = poll(&p, 1, 0);
    rnd = r > 0 && (p.revents & POLLIN) ? "legivel" : r == 0 ? "BLOQUEARIA" : "erro";
    close(fd);
  } else rnd = "sem-acesso";
  if (uname(&u) != 0) snprintf(u.release, sizeof u.release, "?");
  printf("[rede] kernel=%s entropy_avail=%s /dev/random=%s\n",
         u.release, ea, rnd);
  fflush(stdout);
}
#endif

static int abrirReal(void) {
  void *h;
  int r = __atomic_load_n(&pronto, __ATOMIC_ACQUIRE);
  // Acquire publica tambem os ponteiros dlsym e o global_init para os outros
  // fios. Um int alinhado sem atomica ainda seria uma data race em C.
  // So o RESULTADO (1 carregou, -1 falhou de vez) passa sem trava. Enquanto
  // um fio ainda esta no dlopen+global_init a bandeira vale -2, e quem chega
  // nesse instante ESPERA na trava em vez de voltar "sem rede": era o que
  // acontecia — sete fios de noticias partindo juntos, o primeiro carregava a
  // libcurl e os outros cinco falhavam na hora (medido em tests/agenda_shot).
  // No arranque do aparelho os quatro fios de arte partem do mesmo jeito.
  if (r == 1 || r == -1) return r > 0;
  pthread_mutex_lock(&abrirTrava);
  r = __atomic_load_n(&pronto, __ATOMIC_ACQUIRE);
  if (r == 1 || r == -1) { pthread_mutex_unlock(&abrirTrava); return r > 0; }
  __atomic_store_n(&pronto, -2, __ATOMIC_RELEASE);
#ifdef NV_TPK40
  // Rastro do Tizen 4/5 (#180): qual soname abriu, ou o erro. Ver tpk.c.
  nv_tpk40_etapa("begin libcurl-dlopen");
  h = dlopen("libcurl.so.5", RTLD_NOW);
  if (h) nv_tpk40_etapa("ok libcurl-dlopen libcurl.so.5");
  if (!h) { h = dlopen("libcurl.so.4", RTLD_NOW); if (h) nv_tpk40_etapa("ok libcurl-dlopen libcurl.so.4"); }
  if (!h) { char m[300]; const char *e = dlerror();
            snprintf(m, sizeof m, "fail libcurl-dlopen %s", e ? e : "sem mensagem");
            nv_tpk40_etapa(m); }
#else
#ifdef NV_ANDROID
  // Android: o sistema nao oferece libcurl a apps; ela vai no APK (jniLibs) com
  // o nome "libcurl.so", e o dlopen por nome acha na pasta nativa do app.
  printf("[rede] dlopen libcurl.so\n"); fflush(stdout);
  { unsigned long t0 = redeAgoraMs();
    const char *e;
    h = dlopen("libcurl.so", RTLD_NOW);
    e = h ? "" : dlerror();
    printf("[rede] dlopen libcurl.so %s em %lu ms%s%s\n", h ? "ok" : "FALHOU", redeAgoraMs() - t0,
           h ? "" : ": ", e ? e : "?");
    fflush(stdout); }
  if (!h)
#endif
  h = dlopen("libcurl.so.5", RTLD_NOW);
  if (!h) h = dlopen("libcurl.so.4", RTLD_NOW);
#endif
  if (!h) h = dlopen("libcurl.4.dylib", RTLD_NOW);   // Mac
  if (!h) h = dlopen("libcurl.dylib", RTLD_NOW);
  if (!h) { printf("[rede] sem libcurl: %s\n", dlerror());
            __atomic_store_n(&pronto, -1, __ATOMIC_RELEASE); pthread_mutex_unlock(&abrirTrava); return 0; }
  *(void **)(&curl_init)    = dlsym(h, "curl_easy_init");
  *(void **)(&curl_setopt_f)  = dlsym(h, "curl_easy_setopt");
  *(void **)(&curl_perform) = dlsym(h, "curl_easy_perform");
  *(void **)(&curl_cleanup) = dlsym(h, "curl_easy_cleanup");
  *(void **)(&curl_global)  = dlsym(h, "curl_global_init");
  *(void **)(&slist_append) = dlsym(h, "curl_slist_append");
  *(void **)(&slist_free)   = dlsym(h, "curl_slist_free_all");
  *(void **)(&curl_getinfo) = dlsym(h, "curl_easy_getinfo");
  *(void **)(&curl_getdate_fn) = dlsym(h, "curl_getdate");
  *(void **)(&curl_send)    = dlsym(h, "curl_easy_send");
  *(void **)(&curl_recv)    = dlsym(h, "curl_easy_recv");
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
  if (!curl_init || !curl_setopt_f || !curl_perform) {
    printf("[rede] libcurl sem os simbolos esperados\n");
    __atomic_store_n(&pronto, -1, __ATOMIC_RELEASE);
    pthread_mutex_unlock(&abrirTrava);
    return 0;
  }
#ifdef NV_ANDROID
  // #266: a Shield (Tegra) para em "rede_preparar" sem imprimir mais nada; a
  // libcurl 8 + mbedTLS 3.6 inicia o PSA/entropia aqui dentro. Marca cada lado.
  entropiaLog();
  printf("[rede] curl_global_init\n"); fflush(stdout);
  { unsigned long t0 = redeAgoraMs();
    int gi = curl_global ? curl_global(3 /* CURL_GLOBAL_DEFAULT */) : -1;
    printf("[rede] curl_global_init ok (r=%d) em %lu ms\n", gi, redeAgoraMs() - t0);
    fflush(stdout); }
#else
  if (curl_global) curl_global(3 /* CURL_GLOBAL_DEFAULT */);
#endif
  // DEPOIS do global_init e ANTES de soltar a trava: a partir daqui qualquer fio
  // pode entrar em curl_easy_perform, e e la que o OpenSSL comeca a ser usado.
#ifndef NV_ANDROID   // libcurl do APK nao usa libcrypto 1.0 (e dlopen de libcrypto.so seria do sistema)
  prepararOpenSSL();
#endif
  __atomic_store_n(&pronto, 1, __ATOMIC_RELEASE);
  pthread_mutex_unlock(&abrirTrava);
  return 1;
}

#ifdef NV_ANDROID
// #266: na Shield (Tegra, Android 11) o app nunca terminava de abrir, parado em
// rede_preparar. Suspeita nao provada: dlopen da libcurl / curl_global_init
// (mbedTLS 3.6 PSA/entropia) bloqueando. No Android isso roda num fio proprio;
// a interface segue abrindo e quem precisa de rede espera pela bandeira, com
// limite. Se a init nunca voltar, o log diz.
static int fioIniciado;
static void *fioCurl(void *a) {
  struct timespec t0, t1;
  (void)a;
  clock_gettime(CLOCK_MONOTONIC, &t0);
  abrirReal();
  clock_gettime(CLOCK_MONOTONIC, &t1);
  printf("[rede] libcurl pronta em %ld ms (pronto=%d)\n",
         (long)((t1.tv_sec - t0.tv_sec) * 1000 + (t1.tv_nsec - t0.tv_nsec) / 1000000),
         __atomic_load_n(&pronto, __ATOMIC_ACQUIRE));
  fflush(stdout);
  return NULL;
}
static void iniciarFioCurl(void) {
  pthread_t t;
  if (__atomic_exchange_n(&fioIniciado, 1, __ATOMIC_ACQ_REL)) return;
  if (pthread_create(&t, NULL, fioCurl, NULL) != 0) { fioCurl(NULL); return; }
  pthread_detach(t);
}
// Espera a libcurl ate `ms`. Depois que UMA espera estourou, ninguem mais
// espera: sem isso cada imagem e cada chamada pagaria os 20 s de novo enquanto
// a init segue presa (a 266.1 fazia isso, e a tela de login ficava parada).
static int esgotou;
static int abrirAte(unsigned long ms) {
  int r = __atomic_load_n(&pronto, __ATOMIC_ACQUIRE);
  unsigned long t0;
  if (r == 1 || r == -1) return r > 0;
  iniciarFioCurl();
  if (__atomic_load_n(&esgotou, __ATOMIC_ACQUIRE)) ms = 0;
  t0 = redeAgoraMs();
  for (;;) {
    struct timespec d = { 0, 10 * 1000 * 1000 };
    r = __atomic_load_n(&pronto, __ATOMIC_ACQUIRE);
    if (r == 1 || r == -1) return r > 0;
    if (redeAgoraMs() - t0 >= ms) {
      if (ms && !__atomic_exchange_n(&esgotou, 1, __ATOMIC_ACQ_REL)) {
        printf("[rede] libcurl nao ficou pronta em %lu ms (pronto=%d); pedidos simples vao pelo Android\n", ms, r);
        fflush(stdout);
      }
      return 0;
    }
    nanosleep(&d, NULL);
  }
}
static int abrir(void) { return abrirAte(20000); }

// RESERVA PELO ANDROID (#266/#332). Pedido simples (GET/POST de texto, com
// cabecalhos) que a libcurl nao pode atender — nao ficou pronta, ou caiu no
// transporte — vai por NuvioActivity.httpPedir (HttpURLConnection). A conta, o
// login e o Supabase passam por aqui (nuvem.c: rede_*_st), e assim o login
// nunca depende so da nossa libcurl. `curlRuim` gruda na sessao: se a libcurl
// falhou onde o Android respondeu, os proximos pedidos simples vao direto.
static int curlRuim;
static char *viaAndroid(const char *verbo, const char *url, int segundos,
                        const char *const *cab, const char *corpo, int *status,
                        long *tam, const char *porque) {
  char *cabs, *r, erro[200], h[96];
  size_t n = 1;
  int k, st = 0, temCt = 0;
  long bytes = 0;
  unsigned long t0 = redeAgoraMs();
  static int logados;
  if (status) *status = 0;
  for (k = 0; cab && cab[k]; k++) {
    n += strlen(cab[k]) + 1;
    if (!strncasecmp(cab[k], "Content-Type:", 13)) temCt = 1;
  }
  if (corpo && !temCt) n += 32;
  cabs = (char *)malloc(n);
  if (!cabs) return NULL;
  cabs[0] = 0;
  for (k = 0; cab && cab[k]; k++) { strcat(cabs, cab[k]); strcat(cabs, "\n"); }
  // Mesmo padrao do postarNativo: JSON, salvo quem mandou o proprio.
  if (corpo && !temCt) strcat(cabs, "Content-Type: application/json\n");
  char *alias = __atomic_load_n(&tmdbAlternativo, __ATOMIC_ACQUIRE) ? tmdbUrl(url) : NULL;
  r = android_http(verbo, alias ? alias : url, cabs, corpo, (segundos > 0 ? segundos : 20) * 1000,
                   &st, &bytes, erro, sizeof erro);
  if (!r && !st && !alias && !redeCancelouLocal && (alias = tmdbUrl(url))) {
    r = android_http(verbo, alias, cabs, corpo, (segundos > 0 ? segundos : 20) * 1000,
                     &st, &bytes, erro, sizeof erro);
    if (r) tmdbLigou();
  }
  free(alias); free(cabs);
  hostDaUrl(url, h, sizeof h);
  if (__atomic_add_fetch(&logados, 1, __ATOMIC_RELAXED) <= 12 || !r) {
    printf("[rede] via Android (%s) %s %s: http=%d em %lu ms%s%s\n", porque, verbo,
           h[0] ? h : "?", st, redeAgoraMs() - t0, r ? "" : " FALHOU: ", r ? "" : erro);
    fflush(stdout);
  }
  if (!r) { snprintf(redeErroTxt, sizeof redeErroTxt, "android: %.180s", erro); return NULL; }
  redeErroTxt[0] = 0;
  if (st == 401 && aviso401) aviso401(url);
  if (status) *status = st;
  else if (st < 200 || st > 299) { free(r); return NULL; }  // como rede_baixar sem status
  if (tam) *tam = bytes;
  return r;
}
// 1 = este pedido simples vai direto pelo Android (sem tocar na libcurl).
static int androidPrimeiro(void) {
  if (__atomic_load_n(&curlRuim, __ATOMIC_ACQUIRE)) return 1;
  // 8 s, e nao 20: a tela de login nao fica parada esperando a libcurl.
  return !abrirAte(8000);
}
// Falha de TRANSPORTE da libcurl num pedido simples: tenta o Android uma vez.
static char *androidDepois(const char *verbo, const char *url, int segundos,
                           const char *const *cab, const char *corpo, int *status) {
  char *r;
  if (redeCancelouLocal || (redeCancelLocal && *redeCancelLocal)) return NULL;
  r = viaAndroid(verbo, url, segundos, cab, corpo, status, NULL, "curl falhou");
  if (r && !__atomic_exchange_n(&curlRuim, 1, __ATOMIC_ACQ_REL)) {
    printf("[rede] libcurl falhou onde o Android respondeu: pedidos simples ficam pelo Android nesta sessao\n");
    fflush(stdout);
  }
  return r;
}
void rede_preparar(void) { iniciarFioCurl(); }
#else
static int abrir(void) { return abrirReal(); }
void rede_preparar(void) { abrir(); }
#endif

/* Request novo isolado do handle/controles por fio dos wrappers. */
// Trusted Mozilla bundle configured once at startup. Android's libcurl has
// no system CA fallback; strict per-request TLS also needs this bundle.
#include "rede_pedido.inc"

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
static char *trechoUmaVez(const char *url, int segundos, long long ini, long long fim,
                          long *tam, int *status, int *erro,
                          char *final, unsigned tamFinal) {
  char faixa[80];
  const char *cab[2];
  char *r;
  int st = 0;
  if (final && tamFinal) final[0] = 0;
  snprintf(faixa, sizeof faixa, "Range: bytes=%lld-%lld", ini, fim);
  cab[0] = faixa; cab[1] = NULL;
  redeCurlLocal = 0;
  rede_teto = (long)(fim - ini + 1);
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
#ifdef NV_ANDROID
  char *r;
  int st = 0;
  if (!url || !*url) { if (status) *status = 0; return NULL; }
  if (androidPrimeiro()) {   // st local: com status NULL o 4xx tambem volta com corpo, como no curl
    r = viaAndroid("GET", url, segundos, cab, NULL, &st, NULL, "libcurl fora");
    if (status) *status = st;
    return r;
  }
  r = rede_baixar_interno2(url, segundos, NULL, cab, &st, NULL, 0);
  if (status) *status = st;
  if (!r && st == 0) {
    r = androidDepois("GET", url, segundos, cab, NULL, &st);
    if (status) *status = st;
  }
  return r;
#else
  return rede_baixar_interno2(url, segundos, NULL, cab, status, NULL, 0);
#endif
}
char *rede_baixar_st_retry(const char *url, int segundos, const char *const *cab,
                           int *status, int *retryAfter) {
  return rede_baixar_interno3(url, segundos, NULL, cab, status, NULL, 0, retryAfter);
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
//
// EXCECAO (#385): Range de leitura LATERAL ao video (rede_lateral) cuja
// conexao foi recusada ou nao fechou o aperto de mao. No registro do .tpk o no
// do CDN do TorBox recusava toda conexao nossa com o video aberto, e cada
// Range da pre-busca virava duas. A segunda conexao ao host que acabou de
// recusar nao ajuda a legenda e e mais uma batida no CDN que o player usa.
static int valeRepetir(int r, const Vigia *v, size_t bytes) {
  if (redeCancelouLocal) return 0;
  if (redeLateral && redeParcialOk && (r == 6 || r == 7 || r == 35)) return 0;
  if (r == 42) return v->parou;            // vigia: corpo parado
  if (bytes > 0) return 0;
  return r == 7 || r == 28 || r == 35 || r == 52 || r == 55 || r == 56;
}

static char *rede_baixar_interno2(const char *url, int segundos, long *tam,
                                  const char *const *cab, int *status,
                                  char *etag, unsigned tamEtag) {
  return rede_baixar_interno3(url, segundos, tam, cab, status, etag, tamEtag, NULL);
}
static char *rede_baixar_interno3(const char *url, int segundos, long *tam,
                                  const char *const *cab, int *status,
                                  char *etag, unsigned tamEtag, int *retry) {
  Balde b = {0};
  CacaCab caca;
  Vigia vigia;
  void *c, *lista = NULL;
  int r = 0, tentativa, reusada = 0;
  unsigned long inicio, prazoMs, gasto = 0;
  caca.dst = (etag && tamEtag > 1) ? etag : NULL;
  caca.tam = tamEtag;
  caca.retry = retry;
  if (retry) *retry = 0;
  if (etag && tamEtag) etag[0] = 0;
  if (status) *status = 0;
  if (!url || !*url) return NULL;
  // Endereco de API de metadados que ja respondeu "nao existe" (ou host em
  // recuo por timeout): nao pede de novo — ver negcache.h.
  if (!status && negcache_barra(url, (long)time(NULL))) return NULL;
#ifdef NV_ANDROID
  // libcurl nunca ficou pronta: GET simples (catalogo, imagem) pelo Android.
  if (!abrir()) return viaAndroid("GET", url, segundos, cab, NULL, status, tam, "libcurl fora");
#else
  if (!abrir()) return NULL;
#endif
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
    if (caca.dst || caca.retry) {
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
#ifdef NV_TPK40
    // Rastro do Tizen 4/5 (#180): a PRIMEIRA requisicao do processo, antes e
    // depois do curl_easy_perform — e onde a TV 2018-2020 morria. So o host
    // vai para o arquivo (rede_url_publica), nunca o caminho.
    { static int primeira = 1;
      if (primeira) { char seg[120], m[200];
        primeira = 0;
        snprintf(m, sizeof m, "begin https-1 %s", rede_url_publica(url, seg, sizeof seg));
        nv_tpk40_etapa(m);
        r = curl_perform(c);
        { long http = 0; if (curl_getinfo) curl_getinfo(c, INFO_RESPONSE_CODE, &http);
          snprintf(m, sizeof m, "ok https-1 curl=%d http=%ld", r, http); }
        nv_tpk40_etapa(m);
      } else r = curl_perform(c); }
#else
    r = curl_perform(c);
#endif
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
      b.p = NULL; b.n = b.cap = 0;
      if (etag && tamEtag) etag[0] = 0;
      continue;
    }
    r = tmdbFallback(c, r);
    break;
  }
  if (r == 42 && vigia.parou) r = 28;      // para quem chama, e prazo
  redeCurlLocal = r;
  anotarErro(r);
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
    negcache_nota(url, r, (int)http, (long)time(NULL));
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
    return baldeFinal(&b);
  }
  // O RESTO DA HISTORIA no log: por qual conexao foi, quanto veio e quanto
  // esperou. "falha 28" sozinho nao separa conexao morta (reusada, 0 bytes),
  // rede lenta (bytes > 0, prazo inteiro) e DNS/conexao que nao abriu (nova,
  // 0 bytes). O comeco da linha fica igual para quem ja procura por ele.
  if (r != 0) { char seg[120]; free(b.p);
    printf("[rede] falha %d em %s (conexao %s, %ld bytes, %lu ms%s)%s%s\n", r,
           rede_url_publica(url, seg, sizeof seg), reusada ? "reusada" : "nova",
           (long)b.n, gasto, tentativa ? ", 2a tentativa" : "",
           redeErroBuf[0] ? ": " : "", redeErroBuf);
    fflush(stdout);
    return NULL; }
  redeBytesLocal = (long)b.n;
  if (tam) *tam = (long)b.n;
  return baldeFinal(&b);
}

typedef struct { size_t n; int limitado; } UrlSonda;
// CURLINFO_CONTENT_TYPE = CURLINFO_STRING (0x100000) + 18.
#define INFO_CONTENT_TYPE 1048594
static size_t receberSonda(void *dados, size_t tam, size_t qtd, void *u) {
  UrlSonda *s = u;
  size_t bytes;
  (void)dados;
  if (tam && qtd > SIZE_MAX / tam) return 0;
  if (redeCancelLocal && *redeCancelLocal) { redeCancelouLocal = 1; return 0; }
  bytes = tam * qtd;
  if (bytes > 64 - s->n) { s->n = 64; s->limitado = 1; return 0; }
  s->n += bytes;
  return bytes;
}

int rede_url_final_cab(const char *url, int segundos, const char *const *cab,
                       char *dst, unsigned tam, int *status) {
  return rede_url_final_tipo(url, segundos, cab, dst, tam, status, NULL, 0, NULL);
}

int rede_url_final_tipo(const char *url, int segundos, const char *const *cab,
                        char *dst, unsigned tam, int *status,
                        char *mime, unsigned mimeTam, long *corpo) {
  UrlSonda s = {0};
  Vigia vigia;
  void *c, *lista = NULL;
  char *fim = NULL;
  long http = 0;
  unsigned long prazoMs;
  int r, ok, k;
  if (status) *status = 0;
  if (mime && mimeTam) mime[0] = 0;
  if (corpo) *corpo = -1;
  if (dst && tam) dst[0] = 0;
  if (!url || !*url || !dst || !tam || tam > INT_MAX || !abrir() || !curl_getinfo) return 0;
  if (cab && cab[0] && (!slist_append || !slist_free)) return 0;
  c = pegarHandle(url);
  if (!c) return 0;
  curl_setopt(c, OPT_URL, url);
  curl_setopt(c, OPT_WRITEFUNCTION, receberSonda);
  curl_setopt(c, OPT_WRITEDATA, &s);
  curl_setopt(c, OPT_FOLLOWLOCATION, (long)1);
  prazoMs = (unsigned long)(segundos > 0 ? segundos : 20) * 1000UL;
  opcoesComuns(c, prazoMs);
  ligarVigia(c, &vigia, prazoMs);
  // Um pedaco minusculo em vez de HEAD: varios servidores de debrid respondem
  // HEAD com 405 ou mentem no redirecionamento, mas honram Range.
  curl_setopt(c, OPT_RANGE, "0-63");
  for (k = 0; cab && cab[k]; k++) {
    void *nova = slist_append(lista, cab[k]);
    if (!nova) { if (lista) slist_free(lista); soltarHandleR(c, 1, url); return 0; }
    lista = nova;
  }
  if (lista) curl_setopt(c, OPT_HTTPHEADER, lista);
  r = performTmdb(c);
  curl_getinfo(c, INFO_RESPONSE_CODE, &http);
  curl_getinfo(c, INFO_URL_FINAL, &fim);
  if (status) *status = (int)http;
  if (mime && mimeTam) {
    char *ct = NULL, *pv;
    unsigned k2 = 0;
    curl_getinfo(c, INFO_CONTENT_TYPE, &ct);
    for (pv = ct; pv && *pv && *pv != ';' && k2 + 1 < mimeTam; pv++)
      if (*pv != ' ' && *pv != '\t') mime[k2++] = (char)tolower((unsigned char)*pv);
    mime[k2] = 0;
  }
  // Corpo inteiro conhecido so num 200 que acabou antes do nosso teto de 64 B.
  if (corpo && http == 200 && !s.limitado && !r) *corpo = (long)s.n;
  // Curl23 so vale quando NOS cortamos o corpo apos o teto. Um corpo cortado
  // pelo servidor (curl18/56) ou outro erro continua falha de transporte.
  ok = (!r || (r == 23 && s.limitado)) && !redeCancelouLocal &&
       http >= 200 && http < 300 && fim && *fim && strlen(fim) < tam;
  if (ok) memcpy(dst, fim, strlen(fim) + 1);
  // DIZER POR QUE FALHOU. Quem chama (streams.c) so imprimia "N nao resolveu",
  // e "nao resolveu" cobre coisas muito diferentes: host que nao existe (6),
  // recusa de conexao (7), estouro de tempo (28), TLS (35, 60) e HTTP 4xx/5xx
  // do proprio servidor da fonte. Sem separar, todo relato de "nao toca" vira
  // adivinhacao — e foi exatamente onde este ficou parado.
  if (!ok) {
    char seg[120];
    printf("[rede] url final falhou: curl %d, HTTP %ld em %s\n", r, http,
           rede_url_publica(url, seg, sizeof seg));
    fflush(stdout);
  }
  if (lista) { curl_setopt(c, OPT_HTTPHEADER, (void *)0); slist_free(lista); }
  soltarHandleR(c, r, url);
  return ok;
}

int rede_url_final(const char *url, int segundos, char *dst, unsigned tam) {
  return rede_url_final_cab(url, segundos, NULL, dst, tam, NULL);
}

// AQUECER CONEXOES (2.0.2): abre DNS + TCP + TLS de cada origem ("https://host")
// com um HEAD em "/", SEM seguir redirecionamento e sem ler corpo, e deixa o
// handle estacionado para o fio que for pedir esses hosts (estacTomar). Nunca
// toca um caminho de stream: um HEAD na raiz nao resolve link nem cria arquivo
// no debrid. Chamar de um fio proprio e descartavel (aquecer.c): o handle do fio
// sai da chave para ficar no estacionamento. ms[i] = tempo da origem i (0 =
// falhou). Devolve quantas abriram.
static size_t descartarCorpo(void *d, size_t t, size_t q, void *u) { (void)d; (void)u; return t * q; }
int rede_aquecer_lote(const char *const *origens, int n, unsigned *ms) {
  char ok[REDE_ESTAC_HOSTS][96];
  int i, abriu = 0, nok = 0;
  void *c;
  if (!origens || n < 1 || !abrir() || !curl_reset || !curl_getinfo) return 0;
  for (i = 0; i < n; i++) {
    char u[200], h[96];
    unsigned long t0;
    int r;
    if (ms) ms[i] = 0;
    hostDaUrl(origens[i], h, sizeof h);
    if (!h[0] || strlen(h) + 2 > sizeof u) continue;
    snprintf(u, sizeof u, "%s/", h);
    c = pegarHandle(u);
    if (!c) continue;
    curl_setopt(c, OPT_URL, u);
    curl_setopt(c, OPT_NOBODY, (long)1);
    curl_setopt(c, OPT_WRITEFUNCTION, descartarCorpo);
    curl_setopt(c, OPT_FOLLOWLOCATION, (long)0);
    opcoesComuns(c, 4000UL);
    t0 = redeAgoraMs();
    r = performTmdb(c);
    if (ms) { unsigned long dt = redeAgoraMs() - t0; ms[i] = r ? 0 : (unsigned)(dt ? dt : 1); }
    soltarHandleR(c, r, u);
    if (!r) {
      abriu++;
      if (nok < REDE_ESTAC_HOSTS) snprintf(ok[nok++], 96, "%s", h);
    }
  }
  // O handle, com as conexoes que abriu, sai deste fio e vai para o estacionamento.
  c = pthread_getspecific(handleChave);
  if (c && nok > 0) {
    pthread_setspecific(handleChave, NULL);
    hostsEsquecer();
    estacionar(c, ok, nok);
  }
  return abriu;
}

char *rede_postar(const char *url, int segundos, const char *const *cab,
                  const char *corpo) {
  return rede_postar_st(url, segundos, cab, corpo, NULL);
}

// APAGAR pelo CUSTOMREQUEST, e nao por OPT_POST: o Trakt so remove um item da
// barra de retomada por DELETE /sync/playback/:id. Ver trakt_playback_remover.
char *rede_apagar(const char *url, int segundos, const char *const *cab,
                  int *status) {
  Balde b = {0};
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
  r = performTmdb(c);
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
  return baldeFinal(&b);
}

// Set once at startup before OAuth/WebSocket workers begin.
void rede_discord_ca(const char *caminho) {
  snprintf(discordCa, sizeof discordCa, "%s", caminho ? caminho : "");
}
#define OPT_CAINFO 10065

static char *postarNativo(const char *url, int segundos, const char *const *cab,
                          const char *corpo, int *status, int seguro) {
  Balde b = {0};
  void *c, *lista = NULL;
  int r;
  if (status) *status = 0;
  if (!url || !*url || !abrir()) return NULL;
  c = seguro ? curl_init() : pegarHandle(url);
  if (!c) return NULL;
  curl_setopt(c, OPT_URL, url);
  curl_setopt(c, OPT_WRITEFUNCTION, receber);
  curl_setopt(c, OPT_WRITEDATA, &b);
  opcoesComuns(c, (unsigned long)(segundos > 0 ? segundos : 20) * 1000UL);
  if (seguro) {
    // OAuth credentials require authenticated TLS and never follow redirects.
    curl_setopt(c, OPT_SSL_VERIFYPEER, (long)1);
    curl_setopt(c, OPT_SSL_VERIFYHOST, (long)2);
    if (discordCa[0]) curl_setopt(c, OPT_CAINFO, discordCa);
    curl_setopt(c, OPT_FOLLOWLOCATION, (long)0);
  }
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
  r = performTmdb(c);
  anotarErro(r);
  if (r != 0) { char seg[120];
    printf("[rede] POST falhou em %s: %s\n", rede_url_publica(url, seg, sizeof seg), redeErroTxt);
    fflush(stdout); }
  // O codigo sai ANTES do cleanup: depois dele a alca nao existe mais.
  { long codigo = 0;
    if (!r && curl_getinfo) curl_getinfo(c, INFO_RESPONSE_CODE, &codigo);
    if (status) *status = (int)codigo;
    if (codigo == 401 && aviso401) aviso401(url); }
  if (lista) { curl_setopt(c, OPT_HTTPHEADER, (void *)0); if (slist_free) slist_free(lista); }
  if (seguro) curl_cleanup(c); else soltarHandleR(c, r, url);
  // Falha de TRANSPORTE (r != 0) continua sendo NULL — ai nao houve resposta
  // nenhuma. O corpo de um 4xx, ao contrario, e devolvido: e nele que o
  // PostgREST explica o que faltou.
  if (r != 0) { free(b.p); return NULL; }
  return b.p ? baldeFinal(&b) : strdup("");
}

char *rede_postar_st(const char *url, int segundos, const char *const *cab,
                     const char *corpo, int *status) {
#ifdef NV_ANDROID
  char *r;
  int st = 0;
  if (!url || !*url) { if (status) *status = 0; return NULL; }
  if (androidPrimeiro()) {   // st local: com status NULL o 4xx tambem volta com corpo, como no curl
    r = viaAndroid("POST", url, segundos, cab, corpo ? corpo : "", &st, NULL, "libcurl fora");
    if (status) *status = st;
    return r;
  }
  r = postarNativo(url, segundos, cab, corpo, &st, 0);
  if (status) *status = st;
  if (!r && st == 0) {
    r = androidDepois("POST", url, segundos, cab, corpo ? corpo : "", &st);
    if (status) *status = st;
  }
  return r;
#else
  return postarNativo(url, segundos, cab, corpo, status, 0);
#endif
}

char *rede_postar_seguro_st(const char *url, int segundos, const char *const *cab,
                            const char *corpo, int *status) {
  if (status) *status = 0;
  if (!url || strncmp(url, "https://", 8)) return NULL;
  return postarNativo(url, segundos, cab, corpo, status, 1);
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
#define VAZ_ESPERA_PADRAO_MS 8000UL
static unsigned long vazEsperaMs = VAZ_ESPERA_PADRAO_MS;
void rede_vazao_espera(unsigned long ms) { vazEsperaMs = ms ? ms : VAZ_ESPERA_PADRAO_MS; }
typedef struct {
  unsigned long pedido, t0, janelaMs, espera, ultimo;
  long long bytes, maxBytes;
  long long balde[VAZ_SEG_BALDES];
  int status, porJanela, porTeto, cancelou, porEspera;
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
  // Nenhum byte de corpo dentro do prazo: fonte parada, desiste.
  if (!c->t0 && c->espera && redeAgoraMs() - c->pedido >= c->espera) { c->porEspera = 1; return 1; }
  return 0;
}

#ifdef NV_ANDROID
static int medirUma(const char *url, const char *const *cab, int segundos,
                    long inicio, long long maxBytes, volatile int *cancelado,
                    int *kbps, int nMax, RedeVazao *res,
                    char *final, unsigned tamFinal);
// ANDROID: o player baixa arquivo progressivo em VAZ_ANDROID_CONEXOES conexoes
// ao mesmo tempo (ParaleloDataSource.kt), porque o Android limita a janela TCP
// de cada uma (~20 Mbps a 250 ms do debrid na TCL Smart TV Pro, 30/09/2026).
// A medida tem de ser do MESMO jeito, senao diz "ate 9 GB" para quem toca 40 GB
// sem parar: as conexoes leem trechos distantes do mesmo arquivo e os baldes
// por segundo somam. Resultado, status e URL final sao os da primeira.
#define VAZ_ANDROID_CONEXOES 4
#define VAZ_ANDROID_SALTO (256L * 1024L * 1024L)
typedef struct {
  const char *url; const char *const *cab; int segundos; long inicio;
  long long maxBytes; volatile int *cancelado;
  int kbps[VAZ_SEG_BALDES]; int n; RedeVazao res;
} VazExtra;
static void *vazExtraFio(void *a) {
  VazExtra *x = (VazExtra *)a;
  x->n = medirUma(x->url, x->cab, x->segundos, x->inicio, x->maxBytes, x->cancelado,
                  x->kbps, VAZ_SEG_BALDES, &x->res, NULL, 0);
  return NULL;
}
int rede_medir_vazao(const char *url, const char *const *cab, int segundos,
                     long inicio, long long maxBytes, volatile int *cancelado,
                     int *kbps, int nMax, RedeVazao *res,
                     char *final, unsigned tamFinal) {
  VazExtra ex[VAZ_ANDROID_CONEXOES - 1];
  pthread_t fio[VAZ_ANDROID_CONEXOES - 1];
  int vivo[VAZ_ANDROID_CONEXOES - 1];
  int k, i, n;
  for (k = 0; k < VAZ_ANDROID_CONEXOES - 1; k++) {
    memset(&ex[k], 0, sizeof ex[k]);
    ex[k].url = url; ex[k].cab = cab; ex[k].segundos = segundos;
    ex[k].inicio = inicio + (long)(k + 1) * VAZ_ANDROID_SALTO;
    ex[k].maxBytes = maxBytes; ex[k].cancelado = cancelado;
    vivo[k] = pthread_create(&fio[k], NULL, vazExtraFio, &ex[k]) == 0;
  }
  n = medirUma(url, cab, segundos, inicio, maxBytes, cancelado, kbps, nMax, res, final, tamFinal);
  for (k = 0; k < VAZ_ANDROID_CONEXOES - 1; k++) {
    if (!vivo[k]) continue;
    pthread_join(fio[k], NULL);
    // Arquivo menor que o salto (416) ou trecho que falhou: so nao soma.
    if (ex[k].n <= 0 || !kbps) continue;
    for (i = 0; i < ex[k].n && i < nMax; i++) {
      kbps[i] += ex[k].kbps[i];
      if (i >= n) n = i + 1;
    }
    if (res) res->bytes += ex[k].res.bytes;
  }
  return n;
}
static int medirUma(const char *url, const char *const *cab, int segundos,
                    long inicio, long long maxBytes, volatile int *cancelado,
                    int *kbps, int nMax, RedeVazao *res,
                    char *final, unsigned tamFinal) {
#else
int rede_medir_vazao(const char *url, const char *const *cab, int segundos,
                     long inicio, long long maxBytes, volatile int *cancelado,
                     int *kbps, int nMax, RedeVazao *res,
                     char *final, unsigned tamFinal) {
#endif
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
  ct.espera = vazEsperaMs;
  c = pegarHandle(url);
  if (!c) { if (res) res->erro = 2; return 0; }
  curl_setopt(c, OPT_URL, url);
  curl_setopt(c, OPT_WRITEFUNCTION, contadorCorpo);
  curl_setopt(c, OPT_WRITEDATA, &ct);
  curl_setopt(c, OPT_HEADERFUNCTION, contadorCab);
  curl_setopt(c, OPT_HEADERDATA, &ct);
  curl_setopt(c, OPT_FOLLOWLOCATION, (long)1);
  // Prazo: a janela mais o prazo do 1o byte (8 s, ou o de rede_vazao_espera) para DNS, TLS, redirecionamentos e o primeiro
  // byte. O conexaoMs de opcoesComuns fica no teto de 5 s.
  opcoesComuns(c, ct.janelaMs + vazEsperaMs);
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
  r = performTmdb(c);
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


// ------------------------------------------------------------ TLS CRU (curl)
//
// Ver rede_tls_abrir em rede.h. CONNECT_ONLY faz a libcurl resolver, conectar
// e negociar o TLS e PARAR: nenhum byte de HTTP sai. Dai em diante o fio fala
// pelo curl_easy_send/recv, que cifram e decifram. O handle e proprio e nunca
// vai para o cache de pegarHandle: uma conexao de websocket nao e reaproveitavel.
#define OPT_CONNECT_ONLY      141
#define OPT_HTTP_VERSION       84
// CURLINFO_LASTSOCKET = CURLINFO_LONG + 29. ACTIVESOCKET (7.45+) seria o certo,
// mas LASTSOCKET existe em toda versao que estas TVs trazem e devolve o mesmo
// descritor quando ha uma conexao so — que e o caso de um handle CONNECT_ONLY.
#define INFO_LASTSOCKET  2097181
#define CURLE_AGAIN_ 81

#include <sys/select.h>

struct RedeTls { void *c; int fd; };

static int tlsEsperar(int fd, int escrever, int ms) {
  fd_set f;
  struct timeval tv;
  if (fd < 0 || fd >= FD_SETSIZE) return -1;
  FD_ZERO(&f);
  FD_SET(fd, &f);
  tv.tv_sec = ms / 1000;
  tv.tv_usec = (ms % 1000) * 1000;
  return select(fd + 1, escrever ? NULL : &f, escrever ? &f : NULL, NULL, &tv);
}

RedeTls *rede_tls_abrir(const char *url, int segundos) {
  RedeTls *t;
  void *c;
  long fd = -1;
  int r;
  if (!url || !*url || !abrir() || !curl_send || !curl_recv) return NULL;
  c = curl_init();
  if (!c) return NULL;
  curl_setopt(c, OPT_URL, url);
  curl_setopt(c, OPT_CONNECT_ONLY, (long)1);
  // HTTP/1.1 no ALPN. Sem isto uma libcurl com nghttp2 (a do Mac, a do Android)
  // negocia h2 com o servidor, e o aperto de mao do websocket — texto HTTP/1.1
  // — chega num canal HTTP/2: o servidor fecha sem resposta. MEDIDO no Mac
  // contra gateway.discord.gg. A 7.53.1 das TVs nao tem h2, mas a opcao e
  // inofensiva la.
  curl_setopt(c, OPT_HTTP_VERSION, (long)2 /* CURL_HTTP_VERSION_1_1 */);
  curl_setopt(c, OPT_CONNECTTIMEOUT_MS, (long)(segundos > 0 ? segundos : 15) * 1000L);
  curl_setopt(c, OPT_NOSIGNAL, (long)1);
  curl_setopt(c, OPT_TCP_KEEPALIVE, (long)1);
  curl_setopt(c, OPT_TCP_KEEPIDLE, (long)15);
  curl_setopt(c, OPT_TCP_KEEPINTVL, (long)5);
  // Presence sends OAuth credentials: fail closed if the CA store is stale.
  curl_setopt(c, OPT_SSL_VERIFYPEER, (long)1);
  curl_setopt(c, OPT_SSL_VERIFYHOST, (long)2);
  if (discordCa[0]) curl_setopt(c, OPT_CAINFO, discordCa);
  r = performTmdb(c);
  if (r == 0 && curl_getinfo) curl_getinfo(c, INFO_LASTSOCKET, &fd);
  if (r != 0 || fd < 0) {
    printf("[rede] tls: falhou (curl %d)\n", r);
    curl_cleanup(c);
    return NULL;
  }
  t = (RedeTls *)calloc(1, sizeof *t);
  if (!t) { curl_cleanup(c); return NULL; }
  t->c = c;
  t->fd = (int)fd;
  return t;
}

int rede_tls_tentar_enviar(RedeTls *t, const void *buf, size_t n, size_t *foi) {
  int r;
  if (foi) *foi = 0;
  if (!t || !foi) return -1;
  r = curl_send(t->c, buf, n, foi);
  if (r == CURLE_AGAIN_) { *foi = 0; return 0; }
  return r == 0 ? 0 : -1;
}

int rede_tls_enviar(RedeTls *t, const void *buf, size_t n) {
  const unsigned char *p = (const unsigned char *)buf;
  int voltas = 0;
  if (!t) return -1;
  while (n > 0) {
    size_t foi = 0;
    int r = curl_send(t->c, p, n, &foi);
    if (r == CURLE_AGAIN_) {
      // Socket cheio: espera ate 10 s no total. Websocket de presenca manda
      // quadros de centenas de bytes; isto so acontece com a rede travada.
      if (++voltas > 100 || tlsEsperar(t->fd, 1, 100) < 0) return -1;
      continue;
    }
    if (r != 0) return -1;
    p += foi;
    n -= foi;
  }
  return 0;
}

int rede_tls_receber(RedeTls *t, void *buf, size_t n, int esperaMs) {
  size_t veio = 0;
  int r;
  if (!t) return -1;
  // recv ANTES do select: o TLS pode ter bytes ja decifrados no buffer dele, e
  // o socket nao acusaria nada.
  r = curl_recv(t->c, buf, n, &veio);
  if (r == CURLE_AGAIN_ && esperaMs > 0) {
    if (tlsEsperar(t->fd, 0, esperaMs) <= 0) return 0;
    r = curl_recv(t->c, buf, n, &veio);
  }
  if (r == CURLE_AGAIN_) return 0;
  if (r != 0) return -1;
  return veio > 0 ? (int)veio : -1;   // 0 bytes com r == 0 = o outro lado fechou
}

void rede_tls_fechar(RedeTls *t) {
  if (!t) return;
  curl_cleanup(t->c);
  free(t);
}


/* One response, bounded independently of the generic download TLS state. */
typedef struct {
  Balde body;
  int status, valid, overflow, range_headers;
  int64_t start, end, total, requested, limit;
  volatile int *cancel;
} DtsRange;
/* Reject values outside int64_t instead of relying on overflowing scanf. */
static int dtsRangeNumber(const char **text, int64_t *value) {
  const char *p = *text;
  int64_t n = 0;
  if (*p < '0' || *p > '9') return 0;
  while (*p >= '0' && *p <= '9') {
    int digit = *p++ - '0';
    if (n > (INT64_MAX - digit) / 10) return 0;
    n = n * 10 + digit;
  }
  *text = p; *value = n;
  return 1;
}
/* Credential stripping must compare complete origins, never truncations. */
static int dtsRangeOrigin(const char *url, char *origin, size_t capacity) {
  const char *authority = strstr(url, "://");
  size_t n;
  if (!authority) return 0;
  authority += 3;
  n = (size_t)(authority - url) + strcspn(authority, "/?#");
  if (n >= capacity) return 0;
  memcpy(origin, url, n); origin[n] = 0;
  return 1;
}
static size_t dtsRangeHeader(void *data, size_t size, size_t count, void *user) {
  DtsRange *r = user;
  size_t n;
  const char *p = data;
  char line[192];
  if (size && count > SIZE_MAX / size) return 0;
  n = size * count;
  if (n > 5 && !strncasecmp(p, "HTTP/", 5)) {
    const char *space = memchr(p, ' ', n);
    r->status = space ? atoi(space + 1) : 0;
    r->valid = 0; r->total = -1; r->range_headers = 0;
  } else if (n > 14 && !strncasecmp(p, "Content-Range:", 14)) {
    int64_t lo, hi, total = -1;
    const char *value;
    if (++r->range_headers > 1 || n >= sizeof line) return 0;
    memcpy(line, p + 14, n - 14); line[n - 14] = 0;
    value = line;
    while (*value == ' ' || *value == '\t') value++;
    if (strncasecmp(value, "bytes ", 6)) return n;
    value += 6;
    if (!dtsRangeNumber(&value, &lo) || *value != '-') return n;
    value++;
    if (!dtsRangeNumber(&value, &hi) || *value != '/' || hi < lo) return n;
    value++;
    if (*value == '*') value++;
    else if (!dtsRangeNumber(&value, &total) || total <= hi) return n;
    while (*value == ' ' || *value == '\t' || *value == '\r' || *value == '\n') value++;
    if (*value) return n;
    r->start = lo; r->end = hi; r->total = total; r->valid = 1;
  }
  return n;
}
static size_t dtsRangeBody(void *data, size_t size, size_t count, void *user) {
  DtsRange *r = user;
  size_t n;
  char *next;
  if (size && count > SIZE_MAX / size) return 0;
  n = size * count;
  if (r->cancel && __atomic_load_n(r->cancel, __ATOMIC_RELAXED)) return 0;
  if (r->status >= 300 && r->status < 400) return n;
  if (r->status != 206 || !r->valid || r->start != r->requested ||
      r->end - r->start >= r->limit || n > (size_t)r->limit - r->body.n) {
    r->overflow = 1; return 0;
  }
  if (r->body.n + n + 1 > r->body.cap) {
    size_t cap = r->body.cap ? r->body.cap : 4096;
    while (cap < r->body.n + n + 1) cap *= 2;
    if (cap > (size_t)r->limit + 1) cap = (size_t)r->limit + 1;
    next = realloc(r->body.p, cap);
    if (!next) return 0;
    r->body.p = next; r->body.cap = cap;
  }
  if (n) memcpy(r->body.p + r->body.n, data, n);
  r->body.n += n;
  if (r->body.p) r->body.p[r->body.n] = 0;
  return n;
}
static int dtsRangeProgress(void *user, long long dt, long long dn,
                            long long ut, long long un) {
  DtsRange *r = user;
  (void)dt; (void)dn; (void)ut; (void)un;
  return r->cancel && __atomic_load_n(r->cancel, __ATOMIC_RELAXED);
}
static void *dtsRangeHeaders(const char *raw, int public_only) {
  char *copy, *line, *ctx = NULL;
  void *list = NULL;
  if (!raw || !*raw || !slist_append) return NULL;
  copy = strdup(raw);
  if (!copy) return NULL;
  for (line = strtok_r(copy, "\n", &ctx); line; line = strtok_r(NULL, "\n", &ctx)) {
    char *colon = strchr(line, ':');
    size_t n = strlen(line), key;
    if (n && line[n - 1] == '\r') line[--n] = 0;
    if (!colon || strchr(line, '\r')) continue;
    key = (size_t)(colon - line);
    if ((key == 5 && !strncasecmp(line, "range", key)) ||
        (key == 15 && !strncasecmp(line, "accept-encoding", key))) continue;
    if (public_only && !((key == 7 && !strncasecmp(line, "referer", key)) ||
                        (key == 10 && !strncasecmp(line, "user-agent", key)) ||
                        (key == 6 && !strncasecmp(line, "origin", key)))) continue;
    list = slist_append(list, line);
  }
  free(copy);
  return list;
}
/* `final` (8192 bytes, optional) receives the address that answered 206 when
 * at least one redirect was followed, "" otherwise; `cross` says whether that
 * address is on another origin (credential headers were NOT sent to it). */
char *rede_baixar_trecho64_final(const char *url, const char *headers,
                              int64_t start, int64_t end, long *size,
                              int64_t *total, int *status,
                              volatile int *cancelled, char *final, int *cross) {
  char current[8192], range[64], origin[128];
  int hop, public_only = 0, downgraded = 0;
  if (size) *size = 0;
  if (total) *total = -1;
  if (status) *status = 0;
  if (final) final[0] = 0;
  if (cross) *cross = 0;
  if (!url || strlen(url) >= sizeof current || start < 0 || end < start ||
      end - start >= 4 * 1024 * 1024 || !abrir()) return NULL;
  if (strncmp(url, "http://", 7) && strncmp(url, "https://", 8)) return NULL;
  snprintf(current, sizeof current, "%s", url);
  if (!dtsRangeOrigin(url, origin, sizeof origin)) return NULL;
  snprintf(range, sizeof range, "%lld-%lld", (long long)start, (long long)end);
  for (hop = 0; hop < 6; hop++) {
    DtsRange r;
    void *c, *list;
    int result;
    long http = 0;
    char redirected[8192] = "", *next = NULL;
    if (cancelled && __atomic_load_n(cancelled, __ATOMIC_RELAXED)) return NULL;
    memset(&r, 0, sizeof r);
    r.requested = start; r.limit = end - start + 1; r.total = -1; r.cancel = cancelled;
    c = pegarHandle(current);
    if (!c) return NULL;
    opcoesComuns(c, 15000);
    /* DTS requests may carry provider credentials: always authenticate TLS,
     * independent of the legacy generic download defaults. */
    curl_setopt(c, OPT_SSL_VERIFYPEER, (long)1);
    curl_setopt(c, OPT_SSL_VERIFYHOST, (long)2);
    if (discordCa[0]) curl_setopt(c, OPT_CAINFO, discordCa);
    curl_setopt(c, OPT_URL, current);
    curl_setopt(c, OPT_RANGE, range);
    curl_setopt(c, OPT_FOLLOWLOCATION, (long)0);
    curl_setopt(c, OPT_WRITEFUNCTION, dtsRangeBody);
    curl_setopt(c, OPT_WRITEDATA, &r);
    curl_setopt(c, OPT_HEADERFUNCTION, dtsRangeHeader);
    curl_setopt(c, OPT_HEADERDATA, &r);
    curl_setopt(c, OPT_XFERINFOFUNCTION, dtsRangeProgress);
    curl_setopt(c, OPT_XFERINFODATA, &r);
    curl_setopt(c, OPT_NOPROGRESS, (long)0);
    list = downgraded ? NULL : dtsRangeHeaders(headers, public_only);
    if (list) curl_setopt(c, OPT_HTTPHEADER, list);
    result = performTmdb(c);
    if (curl_getinfo) {
      curl_getinfo(c, INFO_RESPONSE_CODE, &http);
      /* CURLINFO_REDIRECT_URL resolves relative Location without following it. */
      curl_getinfo(c, 1048607, &next);
      if (next && strlen(next) < sizeof redirected)
        snprintf(redirected, sizeof redirected, "%s", next);
    }
    if (status) *status = (int)http;
    if (list && slist_free) slist_free(list);
    soltarHandleR(c, result, current);
    if (!result && http >= 300 && http < 400 && redirected[0]) {
      char host[128];
      free(r.body.p);
      if (strncmp(redirected, "http://", 7) && strncmp(redirected, "https://", 8)) return NULL;
      if (!dtsRangeOrigin(redirected, host, sizeof host)) return NULL;
      if (strcmp(host, origin)) public_only = 1;
      /* HTTPS -> HTTP: followed (debrid CDNs answer on plain HTTP: Real-Debrid
       * via Debridio, where the TV's own player plays), but NO caller header
       * goes to the downgraded hop or anything after it: not credentials, not
       * even Referer/User-Agent/Origin. Only the URL travels. */
      if (!strncmp(current, "https://", 8) && !strncmp(redirected, "http://", 7)) {
        static int logged;
        downgraded = public_only = 1;
        if (!logged) {
          logged = 1;
          printf("[rede] range: HTTPS redirect to HTTP followed without request headers\n");
          fflush(stdout);
        }
      }
      snprintf(current, sizeof current, "%s", redirected);
      continue;
    }
    if (!result && http == 206 && r.valid && !r.overflow &&
        r.start == start && r.end <= end && r.body.n &&
        (uint64_t)(r.end - r.start + 1) == r.body.n) {
      if (size) *size = (long)r.body.n;
      if (total) *total = r.total;
      if (final && hop > 0) snprintf(final, 8192, "%s", current);
      if (cross) *cross = public_only;
      return baldeFinal(&r.body);
    }
    free(r.body.p);
    return NULL;
  }
  return NULL;
}
char *rede_baixar_trecho64_cab(const char *url, const char *headers,
                              int64_t start, int64_t end, long *size,
                              int64_t *total, int *status,
                              volatile int *cancelled) {
  return rede_baixar_trecho64_final(url, headers, start, end, size, total, status, cancelled, NULL, NULL);
}

#endif  /* __EMSCRIPTEN__ */

#ifdef __EMSCRIPTEN__
char *rede_baixar_trecho64_cab(const char *u, const char *h, int64_t s, int64_t e, long *n, int64_t *t, int *st, volatile int *c) {
  (void)u; (void)h; (void)s; (void)e; (void)c;
  if (n) *n = 0;
  if (t) *t = -1;
  if (st) *st = 0;
  return NULL;
}
#endif

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
#ifndef NV_TPK40   // no NV_TPK40 e campo da struct por fio (topo)
static _Thread_local int redeRestoRecusado;
#endif
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

// PAUSA DO HOST QUE RECUSOU CONEXAO (#385), so para a leitura LATERAL ao
// video (rede_lateral). No registro do .tpk o no do CDN do TorBox recusou 11
// conexoes nossas em 13-15 s logo depois de o video abrir: a pre-busca da
// legenda (5 Ranges, cada um em duas conexoes) e a sonda do MKV pela rede, ao
// mesmo tempo, e o player caiu com ConnectionFailed. Recusa de CONEXAO e o CDN
// pedindo calma tanto quanto o curl 28 / 429 / 5xx do #308, entao o mesmo
// ritmo: 10 s, dobrando ate 60 s, solta no primeiro Range lateral que der
// certo. Por host (esquema, host e porta) do endereco pedido E do endereco em
// que a conexao falhou (o redirecionador do addon e o no do CDN): o proximo
// Range lateral nao paga nem o 302. O player, o proxy do player, a conferencia
// da fonte e o resto do app nao passam por aqui.
#ifndef REDE_CALMA_INI_MS
#define REDE_CALMA_INI_MS 10000UL
#endif
#ifndef REDE_CALMA_MAX_MS
#define REDE_CALMA_MAX_MS 60000UL
#endif
#define REDE_CALMA_HOSTS 8
typedef struct { char h[96]; unsigned long ate, pausa; int erro, avisou; } CalmaHost;
static CalmaHost calmaHost[REDE_CALMA_HOSTS];
static int calmaProx;
static pthread_mutex_t calmaTrava = PTHREAD_MUTEX_INITIALIZER;

void rede_lateral(int sim) { redeLateral = sim ? 1 : 0; }

// curl 6 (o nome do no nao resolve), 7 (conexao recusada / sem rota) e 35 (o
// aperto de mao TLS caiu): a conexao nem abriu. Prazo (28) e HTTP ficam com
// quem ja tratava deles (rede.c repete, o mkvass pausa pelo #308).
static int conexaoRecusada(int e) { return e == 6 || e == 7 || e == 35; }

// O host de `url` esta em pausa? Devolve os ms que faltam (0 = livre) e o
// codigo que a abriu. `*avisar` = 1 so na primeira consulta desta pausa.
static unsigned long calmaFalta(const char *url, int *erro, int *avisar) {
  char h[96];
  unsigned long agora = redeAgoraMs(), falta = 0;
  int i;
  *avisar = 0;
  corteHostDe(url, h, sizeof h);
  if (!h[0]) return 0;
  pthread_mutex_lock(&calmaTrava);
  for (i = 0; i < REDE_CALMA_HOSTS; i++)
    if (calmaHost[i].h[0] && !strcmp(calmaHost[i].h, h) && calmaHost[i].ate > agora) {
      falta = calmaHost[i].ate - agora;
      *erro = calmaHost[i].erro;
      if (!calmaHost[i].avisou) { calmaHost[i].avisou = 1; *avisar = 1; }
      break;
    }
  pthread_mutex_unlock(&calmaTrava);
  return falta;
}

// A conexao a `url` falhou com `erro`: abre (ou dobra) a pausa do host.
// Devolve a pausa aplicada em ms (0 = sem host).
static unsigned long calmaAbrir(const char *url, int erro) {
  char h[96];
  unsigned long agora = redeAgoraMs(), pausa;
  int i, achou = -1;
  corteHostDe(url, h, sizeof h);
  if (!h[0]) return 0;
  pthread_mutex_lock(&calmaTrava);
  for (i = 0; i < REDE_CALMA_HOSTS; i++)
    if (calmaHost[i].h[0] && !strcmp(calmaHost[i].h, h)) { achou = i; break; }
  if (achou < 0) {
    achou = calmaProx; calmaProx = (calmaProx + 1) % REDE_CALMA_HOSTS;
    snprintf(calmaHost[achou].h, sizeof calmaHost[achou].h, "%s", h);
    calmaHost[achou].pausa = 0;
  }
  // Ainda dentro da pausa (outro fio lateral ja abriu): nao dobra de novo.
  if (calmaHost[achou].pausa && calmaHost[achou].ate > agora) pausa = calmaHost[achou].pausa;
  else {
    pausa = calmaHost[achou].pausa ? calmaHost[achou].pausa * 2 : REDE_CALMA_INI_MS;
    if (pausa > REDE_CALMA_MAX_MS) pausa = REDE_CALMA_MAX_MS;
    calmaHost[achou].pausa = pausa;
    calmaHost[achou].ate = agora + pausa;
    calmaHost[achou].avisou = 0;
  }
  calmaHost[achou].erro = erro;
  pthread_mutex_unlock(&calmaTrava);
  return pausa;
}

// Range lateral que deu certo: o host atendeu, a pausa (e o dobro) acabam.
static void calmaSoltar(const char *url) {
  char h[96];
  int i;
  corteHostDe(url, h, sizeof h);
  if (!h[0]) return;
  pthread_mutex_lock(&calmaTrava);
  for (i = 0; i < REDE_CALMA_HOSTS; i++)
    if (calmaHost[i].h[0] && !strcmp(calmaHost[i].h, h)) calmaHost[i].h[0] = 0;
  pthread_mutex_unlock(&calmaTrava);
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

char *rede_baixar_trecho_st(const char *url, int segundos, long long ini, long long fim,
                            long *tam, int *status, int *erro,
                            char *final, unsigned tamFinal) {
  unsigned long t0 = redeAgoraMs(),
                prazo = (unsigned long)(segundos > 0 ? segundos : 30) * 1000UL;
  long pedido = (long)(fim - ini + 1), veio = 0;
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
  // #385: host em pausa por conexao recusada. Nenhuma conexao: o mesmo codigo,
  // na hora, para quem chama tratar como a falha que ja foi.
  if (redeLateral && url) {
    int e0 = 0, avisar = 0;
    unsigned long falta = calmaFalta(url, &e0, &avisar);
    if (falta) {
      if (avisar) {
        char h[96];
        corteHostDe(url, h, sizeof h);
        printf("[rede] %s em pausa (recusou conexao, curl %d): Range lateral sem conexao por mais %lu ms\n",
               h, e0, falta);
        fflush(stdout);
      }
      if (erro) *erro = e0;
      return NULL;
    }
  }
  if (!url || pedido <= 0) return trechoUmaVez(url, segundos, ini, fim, tam, status, erro, final, tamFinal);
  snprintf(atual, sizeof atual, "%s", url);
  for (;;) {
    long long a = ini + veio, b = fim; long n = 0, teto = rede_corte_host(atual);
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
    if (!r && redeLateral && conexaoRecusada(e)) {
      // #385: a conexao nem abriu. Pausa no host pedido e no que recusou (o
      // no do CDN, quando houve redirecionamento); uma linha so por pausa.
      char h[96];
      unsigned long p = calmaAbrir(atual, e);
      if (strcmp(atual, url)) calmaAbrir(url, e);
      if (fin[0]) { char hf[96], ha[96];
        corteHostDe(fin, hf, sizeof hf); corteHostDe(atual, ha, sizeof ha);
        if (hf[0] && strcmp(hf, ha)) calmaAbrir(fin, e); }
      corteHostDe(atual, h, sizeof h);
      printf("[rede] %s recusou conexao (curl %d, Range lateral ao video): sem conexao nova a este host por %lu ms\n",
             h, e, p);
      fflush(stdout);
    }
    if (!r) goto falhou;
    // Sem 206 o servidor ignorou o Range (200 com o comeco do arquivo): no
    // primeiro pedido e o contrato de sempre (quem chama recebe o que veio);
    // no meio de um trecho ja em pedacos, o que viria nao e o resto.
    if (redeLateral) calmaSoltar(atual);
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
      printf("[rede] Range %lld+%ld: %ld de %ld bytes ate aqui, pedindo o resto (pedaco %d)\n",
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
    printf("[rede] Range %lld+%ld sem progresso depois de %ld bytes em %d pedaco(s) (HTTP %d, curl %d%s)\n",
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
