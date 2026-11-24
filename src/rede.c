#include "rede.h"
#ifdef NV_ANDROID
#include "android.h"
#endif
#ifdef NV_IOS
#include "ios.h"
#endif
// Targets whose system HTTP stack stands in for libcurl (android_http /
// nv_host_http): Android always has it; iOS has no libcurl at all.
#if defined(NV_ANDROID) || defined(NV_IOS)
#define NV_HTTP_HOST 1
#endif
#include "negcache.h"
#include "addonurl.h"
#include "mediaurl.h"
#include "negcache.inc"
#include "rangediag.h"
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

#if defined(NV_TPK40) || defined(NV_UM_FIO)
// ST fibers also use pthread keys: compiler TLS belongs to the browser thread.
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
  int (*parar)(void *);
  void *pararUsuario;
  int limitouLocal;
  int cancelouLocal;
  long bytesLocal;
  int curlLocal;
  char *finalDst;
  unsigned finalTam;
  int parcialOk;
  unsigned long prazoAtualMs;
  int semRecuoV4;
  UsoHost usoHost[REDE_HOSTS_POR_FIO];
  long teto;
  int restoRecusado;
  int lateral;
  char erroTxt[200];
  char erroBuf[256];
} RedeFio;
static pthread_key_t fioChave;
static pthread_once_t fioUma = PTHREAD_ONCE_INIT;
static int fioChaveValida;
// Se o calloc falhar (sem memoria), o fio usa esta reserva PARTILHADA em vez
// de desreferenciar NULL: degrada a medicao daquele pedido, nao derruba o app.
static RedeFio fioReserva;
static void fioSoltar(void *p) { if (p != &fioReserva) free(p); }
static void fioCriarChave(void) { fioChaveValida = pthread_key_create(&fioChave, fioSoltar) == 0; }
static RedeFio *redeFio(void) {
  RedeFio *f;
  pthread_once(&fioUma, fioCriarChave);
  if (!fioChaveValida) return &fioReserva;
  f = (RedeFio *)pthread_getspecific(fioChave);
  if (!f) {
    f = (RedeFio *)calloc(1, sizeof *f);
    if (!f) return &fioReserva;
    if (pthread_setspecific(fioChave, f) != 0) { free(f); return &fioReserva; }
  }
  return f;
}
long *rede_teto_ptr(void) { return &redeFio()->teto; }
#define redeLimiteLocal   (redeFio()->limiteLocal)
#define redeCancelLocal   (redeFio()->cancelLocal)
#define redeParar (redeFio()->parar)
#define redePararUsuario (redeFio()->pararUsuario)
#define redeLimitouLocal  (redeFio()->limitouLocal)
#define redeCancelouLocal (redeFio()->cancelouLocal)
#define redeBytesLocal    (redeFio()->bytesLocal)
#define redeCurlLocal     (redeFio()->curlLocal)
#define redeFinalDst      (redeFio()->finalDst)
#define redeFinalTam      (redeFio()->finalTam)
#define redeParcialOk     (redeFio()->parcialOk)
#define redePrazoAtualMs  (redeFio()->prazoAtualMs)
#define redeSemRecuoV4    (redeFio()->semRecuoV4)
#define usoHost           (redeFio()->usoHost)
#define redeRestoRecusado (redeFio()->restoRecusado)
#define redeLateral       (redeFio()->lateral)
#else
/* Controle local da requisicao corrente. O estado nunca e compartilhado
 * entre sondagens: cada fio recebe seu teto e seu cancel token. */
static _Thread_local long redeLimiteLocal;
static _Thread_local volatile int *redeCancelLocal;
static _Thread_local int (*redeParar)(void *);
static _Thread_local void *redePararUsuario;
static _Thread_local int redeLimitouLocal;
static _Thread_local int redeCancelouLocal;
static _Thread_local long redeBytesLocal;
// 1 = este fio e leitura lateral ao video (rede_lateral, #385).
static _Thread_local int redeLateral;
#endif
RedePararEstado rede_parar_trocar(int (*parar)(void *), void *usuario) {
#if defined(NV_TPK40) || defined(NV_UM_FIO)
  RedeFio *f = redeFio();
  if (f == &fioReserva) return (RedePararEstado){0};
  RedePararEstado anterior = { f->parar, f->pararUsuario, 1 };
  f->parar = parar; f->pararUsuario = usuario;
#else
  RedePararEstado anterior = { redeParar, redePararUsuario, 1 };
  redeParar = parar; redePararUsuario = usuario;
#endif
  return anterior;
}
void rede_parar_quando(int (*parar)(void *),void *usuario){
  redeParar=parar;redePararUsuario=usuario;
}
static int redeParou(void){return (redeCancelLocal&&*redeCancelLocal)||(redeParar&&redeParar(redePararUsuario));}
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
// Fetch exposes the final response URL and allows deadline/size aborts.
// The C API still waits: pthreads yield through Asyncify, fibers to fio1.
//
// CORS: NO NAVEGADOR COMUM, ISTO NAO FUNCIONA E NAO E DEFEITO. Os hosts de
// addon, o TMDB e o Supabase nao mandam Access-Control-Allow-Origin para uma
// origem file://. Dentro do .wgt o Tizen dispensa a checagem para os endereços
// declarados em <access origin="*"> no config.xml — e por isso que o fork em
// JavaScript funciona na TV. Para testar no Chrome do desktop e preciso subir o
// navegador com --disable-web-security ou por um proxy.
//
// Requests stop at their deadline, including while reading the response.

#include <emscripten.h>
#include <emscripten/threading.h>
#include "fio1.h"   // so tem corpo com NV_UM_FIO; nv_http troca de implementacao abaixo
#include "redepedido_wgt.h"

// Faz a requisicao e devolve um buffer de malloc com o corpo (com um NUL extra
// no fim, para quem trata como texto). Escreve o tamanho em *tam e o status
// HTTP em *status. Devolve 0 se a requisicao nem saiu.
//
// `cabs` vem como uma unica string com uma linha "Nome: valor" por cabecalho,
// separadas por \n, porque atravessar um vetor de ponteiros por EM_JS custaria
// mais codigo do que juntar e separar.
//
// One bounded reader serves worker threads and cooperative fibers.
EM_JS(int, nv_http_iniciar_bruto, (const char *metodo, const char *url,
                                    const char *cabs, const char *corpo, int prazo,
                                    int teto, int modo), {
  if (!Module.nvFioReqs) { Module.nvFioReqs = {}; Module.nvFioProxId = 1; }
  var id = Module.nvFioProxId++;
  var cab = {};
  if (cabs) UTF8ToString(cabs).split("\n").forEach(function (linha) {
    var i = linha.indexOf(":");
    if (i > 0) cab[linha.slice(0, i).trim()] = linha.slice(i + 1).trim();
  });
  // Promise.finally shipped in Chrome 63; AbortController followed in Chrome 66.
  var controle = typeof AbortController === "function" ? new AbortController() : null;
  var xhr;
  var abortar = function () {
    if (controle) controle.abort();
    else if (xhr) xhr.abort();
  };
  var reg = { pronto: false, erro: false, status: 0, url: "", etag: "", bytes: null,
              mime: "", tamanho: -1, retry: 0, limitado: false, ttfb: -1,
              abortar: function () { reg.erro = true; abortar(); terminar(); } };
  Module.nvFioReqs[id] = reg;
  var terminar = function () {
    if (reg.pronto) return;
    reg.pronto = true;
    clearTimeout(timer);
  };
  var timer = setTimeout(function () {
    reg.erro = true; abortar(); terminar();
  }, prazo);
  var init = { method: UTF8ToString(metodo), headers: cab, redirect: "follow" };
  if (controle) init.signal = controle.signal;
  if (corpo) init.body = UTF8ToString(corpo);
  // Sonda (modo 2): o primeiro pedido vai com os cabecalhos da fonte e
  // redirect manual — se houver salto, os cabecalhos NAO podem seguir para
  // outra origem. O Location nao e exposto (opaqueredirect), entao a cadeia
  // e refeita com follow levando so Range, como o nativo ao trocar de origem.
  var enviar = function (cabs2, redir) {
    if (controle) controle = new AbortController();
    var op = { method: init.method, headers: cabs2, redirect: redir };
    if (controle) op.signal = controle.signal;
    if (init.body !== undefined) op.body = init.body;
    return fetch(UTF8ToString(url), op);
  };
  var soRange = function () {
    var so = {};
    Object.keys(cab).forEach(function (nome) {
      if (nome.toLowerCase() === "range") so[nome] = cab[nome];
    });
    return so;
  };
  teto = teto > 0 ? teto : 33554432;
  var inicio = Date.now();
  var cabecalhos = function (res) {
    if (reg.ttfb < 0) reg.ttfb = Date.now() - inicio;
    reg.status = res.status;
    reg.url = res.headers.get("x-nuvio-url-final") || res.url || "";
    reg.etag = res.headers.get("etag") || "";
    reg.mime = (res.headers.get("content-type") || "").split(";")[0].trim().toLowerCase();
    var cl = res.headers.get("content-length");
    reg.tamanho = res.status === 200 && cl !== null && /^\\d+$/.test(cl.trim()) ? (Number(cl) <= 64 ? Number(cl) : -1) : -1;
    var raw = (res.headers.get("retry-after") || "").trim();
    if (/^[0-9]+$/.test(raw)) reg.retry = Math.min(2147483647, Number(raw));
    else if (/^(Mon|Tue|Wed|Thu|Fri|Sat|Sun)/i.test(raw)) {
      var until = Date.parse(raw);
      if (isFinite(until)) reg.retry = Math.min(2147483647, Math.max(0, Math.ceil((until - Date.now()) / 1000)));
    }
  };
  if (!controle) {
    // XHR follows redirects and can abort even before response headers arrive.
    // x-user-defined preserves raw bytes while allowing a bounded partial read.
    try {
      xhr = new XMLHttpRequest();
      xhr.open(init.method, UTF8ToString(url), true);
      xhr.timeout = prazo;
      xhr.overrideMimeType("text/plain; charset=x-user-defined");
      // XHR nao deixa segurar o redirect: para a sonda so o Range e seguro.
      var cabX = modo === 2 ? soRange() : cab;
      Object.keys(cabX).forEach(function (nome) { xhr.setRequestHeader(nome, cabX[nome]); });
      var colher = function () {
        var texto = xhr.responseText || "";
        reg.n = Math.min(texto.length, teto);
        reg.bytes = new Uint8Array(modo === 2 ? 0 : reg.n);
        if (modo !== 2) for (var i = 0; i < reg.n; i++) reg.bytes[i] = texto.charCodeAt(i) & 255;
      };
      xhr.onreadystatechange = function () {
        if (reg.pronto || xhr.readyState !== 2) return;
        cabecalhos({status: xhr.status, url: xhr.responseURL,
          headers: {get: function (nome) { return xhr.getResponseHeader(nome); }}});
        if (modo === 1) {
          reg.bytes = new Uint8Array(0); terminar(); abortar();
        }
      };
      xhr.onprogress = function (event) {
        if (reg.pronto || (event.loaded < teto && (xhr.responseText || "").length < teto)) return;
        colher(); reg.limitado = true; terminar(); abortar();
      };
      xhr.onload = function () {
        if (reg.pronto) return;
        colher(); reg.limitado = reg.n === teto; terminar();
      };
      xhr.onerror = xhr.ontimeout = function () {
        if (reg.pronto) return;
        reg.erro = true; terminar(); abortar();
      };
      xhr.send(init.body === undefined ? null : init.body);
    } catch (e) { reg.erro = true; terminar(); abortar(); }
    return id;
  }
  var consumir = async function (res) {
    if (reg.pronto) {
      if (res.body) res.body.cancel().catch(function () {});
      return;
    }
    cabecalhos(res);
    if (modo === 1) {
      abortar(); reg.bytes = new Uint8Array(0); return;
    }
    if (!res.body) { reg.bytes = new Uint8Array(0); return; }
    // No arrayBuffer fallback: it would allocate an unbounded body.
    var reader = res.body.getReader();
    var parts = [], n = 0;
    while (!reg.pronto) {
      var part = await reader.read();
      if (part.done) break;
      var take = Math.min(part.value.length, teto - n);
        if (modo !== 2 && take) parts.push(part.value.slice(0, take));
      n += take;
      if (part.value.length > take || n === teto) {
        reg.limitado = true; abortar(); break;
      }
    }
    if (reg.pronto) return;
    reg.n = n;
    reg.retry = reg.retry || 0;
    reg.bytes = new Uint8Array(modo === 2 ? 0 : n);
    var offset = 0;
    parts.forEach(function (p) { reg.bytes.set(p, offset); offset += p.length; });
  };
  var falhar = function () { if (!reg.pronto) reg.erro = true; };
  try {
    enviar(cab, modo === 2 ? "manual" : "follow").then(async function (res) {
      if (modo === 2 && !reg.pronto &&
          (res.type === "opaqueredirect" || (res.status >= 300 && res.status < 400))) {
        // Salto encontrado: a cadeia refaz so com Range; os cabecalhos da
        // fonte ja ficaram na origem do primeiro pedido e nao atravessam.
        // O return espera o refazer terminar antes do terminar() da corrente.
        reg.ttfb = -1;
        return enviar(soRange(), "follow").then(consumir);
      }
      await consumir(res);
    }).catch(falhar).then(terminar, terminar);
  } catch (e) { reg.erro = true; terminar(); }
  return id;
});

static int nv_http_iniciar_assinc(const char *metodo, const char *url,
                                  const char *cabs, const char *corpo, int prazo,
                                  int teto, int modo) {
  return nv_http_iniciar_bruto(metodo, nv_media_url(url), cabs, corpo, prazo, teto, modo);
}

EM_JS(int, nv_http_pronto_assinc, (int id), {
  var reg = Module.nvFioReqs && Module.nvFioReqs[id];
  return reg && reg.pronto ? 1 : 0;
});

EM_JS(char *, nv_http_colher_bruto, (int id, int *tam, int *status,
                                      char *urlFinal, int urlFinalTam,
                                      char *etag, int etagTam, int *retryAfter,
                                      int *limitado, char *mime, int mimeTam,
                                      long *tamanho, int modo), {
  var reg = Module.nvFioReqs[id];
  delete Module.nvFioReqs[id];
  if (status) HEAP32[status >> 2] = 0;
  if (tam) HEAP32[tam >> 2] = 0;
  if (tamanho) HEAP32[tamanho >> 2] = reg && !reg.erro ? reg.tamanho : -1;
  if (retryAfter) HEAP32[retryAfter >> 2] = reg ? reg.retry : 0;
  if (!reg) return 0;
  /* Sonda (modo 2): os cabecalhos ja sao a resposta. Um corpo que estoura o
   * prazo ou e abortado depois deles nao pode apagar o status recebido —
   * um 403 com cabecalhos imediatos vale resposta, nao "nao testado". */
  if (modo === 2 && reg.status) {
    if (status) HEAP32[status >> 2] = reg.status;
    if (tam) HEAP32[tam >> 2] = reg.n || 0;
    if (limitado) HEAP32[limitado >> 2] = reg.limitado ? 1 : 0;
    return 1;
  }
  if (reg.erro || !reg.bytes) return 0;
  if (status) HEAP32[status >> 2] = reg.status;
  // Probe URLs must fit in full: a truncated signed URL is unusable.
  if (modo === 1 && (reg.status < 200 || reg.status >= 300 ||
      !reg.url || lengthBytesUTF8(reg.url) >= urlFinalTam)) {
    if (urlFinal && urlFinalTam > 0) stringToUTF8("", urlFinal, urlFinalTam);
    return 0;
  }
  if (urlFinal && urlFinalTam > 0) stringToUTF8(reg.url, urlFinal, urlFinalTam);
  if (etag && etagTam > 0) stringToUTF8(reg.etag, etag, etagTam);
  if (mime && mimeTam > 0) stringToUTF8(reg.mime, mime, mimeTam);
  if (limitado) HEAP32[limitado >> 2] = reg.limitado ? 1 : 0;
  if (modo) { if (tam) HEAP32[tam >> 2] = reg.n || 0; return 1; }
  var n = reg.bytes.length, p = _malloc(n + 1);
  if (!p) return 0;
  HEAPU8.set(reg.bytes, p); HEAPU8[p + n] = 0;
  if (tam) HEAP32[tam >> 2] = n;
  return p;
});

EM_JS(int, nv_http_url_tamanho, (int id), {
  var reg = Module.nvFioReqs[id];
  return reg && reg.url ? lengthBytesUTF8(reg.url) + 1 : 1;
});
static char *nv_http_colher_assinc(int id, int *tam, int *status,
                                  char *urlFinal, int urlFinalTam,
                                  char *etag, int etagTam, int *retryAfter,
                                  int *limitado, char *mime, int mimeTam,
                                  long *tamanho, int modo) {
  int cap = urlFinal && urlFinalTam > 0 ? nv_http_url_tamanho(id) : 0;
  char *url = cap > urlFinalTam && cap <= (8 << 20) ? malloc((size_t)cap) : NULL;
  char *r = nv_http_colher_bruto(id, tam, status, url ? url : urlFinal,
                                url ? cap : urlFinalTam, etag, etagTam,
                                retryAfter, limitado, mime, mimeTam, tamanho, modo);
  if (r && ((cap > urlFinalTam && !url) ||
            (url && !nv_media_url_guardar(url, urlFinal, (size_t)urlFinalTam)))) {
    if (!modo) free(r);
    r = NULL; urlFinal[0] = 0;
  }
  free(url);
  return r;
}

// Call-site audit: app sondarRecusaFio, streams probes, diagnostico
// vazaoWorker and livetvdiag fioRede run in workers; descoberta also has
// synchronous fallbacks on thread-creation failure. Both contexts must yield
// to fetch callbacks. NV_UM_FIO uses its scheduler instead of nested Asyncify.
EM_JS(void, nv_http_abortar, (int id), {
  var reg = Module.nvFioReqs && Module.nvFioReqs[id];
  if (reg && !reg.pronto) reg.abortar();
});
EM_JS(int, nv_http_ttfb, (int id), {
  var reg = Module.nvFioReqs && Module.nvFioReqs[id];
  return reg && reg.ttfb >= 0 ? reg.ttfb : 0;
});
static void nv_http_esperar(int id) {
  while (!nv_http_pronto_assinc(id)) {
    if(redeParou()){redeCancelouLocal=1;nv_http_abortar(id);break;}
#ifdef NV_UM_FIO
    fio1_ceder();
#else
    emscripten_sleep(1);
#endif
  }
}
// A mesma espera, conferindo tambem o `parar` por pedido (rede_pedir).
static void nv_http_esperar_pedido(const RedePedido *p, int id) {
  while (!nv_http_pronto_assinc(id)) {
    if (redeParou() || (p->parar && p->parar(p->parar_usuario))) {
      redeCancelouLocal = 1; nv_http_abortar(id); break;
    }
#ifdef NV_UM_FIO
    fio1_ceder();
#else
    emscripten_sleep(1);
#endif
  }
}

static char *nv_http(const char *metodo, const char *url, const char *cabs,
                     const char *corpo, int *tam, int *status,
                     char *urlFinal, int urlFinalTam,
                     char *etag, int etagTam, int *retryAfter, int prazo) {
  int limitado = 0, id;
  long limite = redeLimiteAtual();
  if (limite <= 0 || limite > REDE_CORPO_MAXIMO) limite = REDE_CORPO_MAXIMO;
  id = nv_http_iniciar_assinc(metodo, url, cabs, corpo, prazo, (int)limite, 0);
  // Worker event loops must run fetch callbacks while C waits.
  nv_http_esperar(id);
  char *r = nv_http_colher_assinc(id, tam, status, urlFinal, urlFinalTam,
                                 etag, etagTam, retryAfter, &limitado, NULL, 0, NULL, 0);
  if (limitado) redeLimitouLocal = 1;
  return r;
}

#ifndef NV_UM_FIO
_Thread_local long rede_teto = 0;
// Destino do endereco final do proximo pedido (so rede_baixar_trecho_st liga).
static _Thread_local char *redeFinalDst;
static _Thread_local unsigned redeFinalTam;
#endif

void rede_preparar(void) { }   // nao ha biblioteca para carregar
void rede_zerar_cache(void) { }   // o navegador gere DNS e conexoes (#34)
unsigned rede_cache_geracao(void) { return 0; }
unsigned rede_pedido_capacidades(void) { return 0; }
// WGT: somente GET publico sem redirects, em fluxo; nunca usa responseText.
EM_JS_DEPS(nv_fluxo_deps, "$wasmTable");
EM_ASYNC_JS(int, nv_http_fluxo, (const char *url, unsigned prazo, unsigned teto,
                                int callback, void *usuario, int *status, unsigned *total), {
  if (typeof ReadableStream === 'undefined' || typeof AbortController === 'undefined') return 2;
  var controle = new AbortController(), expirou = false, bloco = _malloc(16384);
  if (!bloco) return 3;
  var timer = setTimeout(function() { expirou = true; controle.abort(); }, prazo);
  try {
    var resposta = await fetch(UTF8ToString(url), {
      redirect: 'error', credentials: 'omit', signal: controle.signal
    });
    HEAP32[status >> 2] = resposta.status;
    if (!resposta.body || !resposta.body.getReader) return 2;
    var leitor = resposta.body.getReader(), n = 0;
    while (true) {
      var parte = await leitor.read();
      if (parte.done) break;
      if (parte.value.length > teto - n) return 8;
      n += parte.value.length; HEAPU32[total >> 2] = n;
      if (resposta.status >= 200 && resposta.status < 300) {
        for (var i = 0; i < parte.value.length; i += 16384) {
          var pedaco = parte.value.subarray(i, i + 16384);
          HEAPU8.set(pedaco, bloco);
          if (!wasmTable.get(callback)(bloco, pedaco.length, usuario)) return 4;
        }
      }
    }
    return 0;
  } catch (e) { return expirou ? 5 : 4; }
  finally { clearTimeout(timer); controle.abort(); _free(bloco); }
});
#ifdef NV_UM_FIO
// Mesma resposta do nv_http_fluxo, mas SEM Asyncify: no build cooperativo o
// EM_ASYNC_JS suspende a pilha inteira pela duracao do fetch — chamado de uma
// fibra fio1 (o "pthread" do fonteurl), congelava o laco de quadros junto.
// Aqui o fetch roda no event loop do navegador e a espera cede ao escalonador
// (nv_http_esperar). Os codigos sao os mesmos do fluxo: 2 sem reader, 3 sem
// memoria no colher, 4 erro/aborto, 5 prazo, 8 acima do teto, 0 ok.
EM_JS(int, nv_http_fluxo_iniciar, (const char *url, unsigned prazo, unsigned teto), {
  if (typeof ReadableStream === 'undefined' || typeof AbortController === 'undefined') return 0;
  if (!Module.nvFioReqs) { Module.nvFioReqs = {}; Module.nvFioProxId = 1; }
  var id = Module.nvFioProxId++;
  var controle = new AbortController(), expirou = false;
  var reg = { pronto: false, codigo: 4, status: 0, n: 0, parts: null, pi: 0, pos: 0,
              abortar: function () { controle.abort(); } };
  Module.nvFioReqs[id] = reg;
  var timer = setTimeout(function () { expirou = true; controle.abort(); }, prazo);
  fetch(UTF8ToString(url), { redirect: 'error', credentials: 'omit', signal: controle.signal })
    .then(async function (res) {
      reg.status = res.status;
      if (!res.body || !res.body.getReader) { reg.codigo = 2; return; }
      var ok = res.status >= 200 && res.status < 300;
      var leitor = res.body.getReader(), n = 0;
      if (ok) reg.parts = [];
      while (true) {
        var parte = await leitor.read();
        if (parte.done) break;
        if (parte.value.length > teto - n) { reg.codigo = 8; controle.abort(); return; }
        n += parte.value.length; reg.n = n;
        if (ok) reg.parts.push(parte.value);
      }
      reg.codigo = 0;
    })
    .catch(function () { reg.codigo = expirou ? 5 : 4; })
    .then(function () { clearTimeout(timer); reg.pronto = true; });
  return id;
});
EM_JS(int, nv_http_fluxo_colher, (int id, int *status, unsigned *total), {
  var reg = Module.nvFioReqs && Module.nvFioReqs[id];
  if (!reg) return 4;
  if (status) HEAP32[status >> 2] = reg.status;
  if (total) HEAPU32[total >> 2] = reg.n;
  return reg.codigo;
});
// Entrega as fatias ja lidas pelo fetch, na ordem em que chegaram (a fila
// alta fica no lado JS; no C nao entra copia extra do corpo inteiro). O
// rede_pedir drena isto DURANTE a espera cooperativa — nao so depois do EOF.
// 0 = esgotado.
EM_JS(int, nv_http_fluxo_pedaco, (int id, void *dst, unsigned cap), {
  var reg = Module.nvFioReqs && Module.nvFioReqs[id];
  if (!reg || !reg.parts) return 0;
  while (reg.pi < reg.parts.length) {
    var p = reg.parts[reg.pi], k = p.length - reg.pos;
    if (k > cap) k = cap;
    if (k > 0) { HEAPU8.set(p.subarray(reg.pos, reg.pos + k), dst); reg.pos += k; }
    if (reg.pos >= p.length) { reg.parts[reg.pi] = null; reg.pi++; reg.pos = 0; }
    if (k > 0) return k;
  }
  return 0;
});
EM_JS(void, nv_http_fluxo_soltar, (int id), {
  if (Module.nvFioReqs) delete Module.nvFioReqs[id];
});
#endif
static char *juntarCabs(const char *const *cab, const char *extra);
int rede_pedir(const RedePedido *p, RedeResposta *r) {
  if (!r) return 0;
  memset(r, 0, sizeof *r);
  // Os demais pedidos continuam indisponiveis: o XHR nao cumpre o contrato.
  if (p && p->receber_corpo && p->url && !strncmp(p->url,"https://",8) &&
      (!p->metodo || !strcmp(p->metodo,"GET")) && !p->seguir && !p->cabecalhos &&
      !p->corpo && !p->n_corpo && !p->job && !p->parar && !p->intervalo &&
      !p->janela_corpo_ms && !p->ca_arquivo && !p->tls_pessoal && !p->pino_publico &&
      !p->max_cabecalhos && p->prazo_ms<=300000 && p->max_bytes<=REDE_CORPO_MAXIMO) {
    unsigned total=0;
#ifdef NV_UM_FIO
    int id = nv_http_fluxo_iniciar(p->url, p->prazo_ms?p->prazo_ms:15000,
        p->max_bytes?p->max_bytes:REDE_CORPO_PADRAO);
    if (id <= 0) { r->erro = REDE_INDISPONIVEL; return 0; }
    // A espera cooperativa tambem DRENA o fluxo: cada fatia vai ao
    // receber_corpo assim que o fetch a entrega (contrato de rede.h), e o
    // retorno 0 aborta o fetch NA HORA — igual ao nv_http_fluxo do build com
    // pthread. fio1_ceder entre fatias mantem o passo do escalonador curto
    // mesmo drenando um corpo de 32 MiB.
    unsigned char bloco[16384];
    int k, recusou = 0;
    for (;;) {
      while ((k = nv_http_fluxo_pedaco(id, bloco, sizeof bloco)) > 0) {
        if (!p->receber_corpo(bloco, (size_t)k, p->corpo_usuario)) { recusou = 1; break; }
        fio1_ceder();
      }
      if (recusou) { nv_http_abortar(id); break; }
      if (nv_http_pronto_assinc(id)) break;
      if (redeParou()) { redeCancelouLocal = 1; nv_http_abortar(id); break; }
      fio1_ceder();
    }
    r->erro = nv_http_fluxo_colher(id, &r->status, &total);
    r->n_corpo = total;
    if (recusou) r->erro = REDE_TRANSPORTE;
    nv_http_fluxo_soltar(id);
    return r->erro==REDE_OK;
#else
    r->erro=nv_http_fluxo(p->url,p->prazo_ms?p->prazo_ms:15000,
        p->max_bytes?p->max_bytes:REDE_CORPO_PADRAO,
        (int)p->receber_corpo,p->corpo_usuario,&r->status,&total);
    r->n_corpo=total;
    return r->erro==REDE_OK;
#endif
  }
  /* Sonda/diagnostico: GET com cabecalhos cujos redirects o proprio fetch
   * segue; o corpo e contado e cortado no teto (modo 2), nunca guardado. O
   * `parar` do pedido vale durante a espera, como no caminho nativo. */
  if (rede_pedido_wgt_sonda(p)) {
    char *cabs = juntarCabs(p->cabecalhos, NULL);
    unsigned long ini;
    int id, n = 0;
    if (!cabs && p->cabecalhos) { r->erro = REDE_MEMORIA; return 0; }
    redeCancelouLocal = 0;
    ini = redeAgoraMs();
    id = nv_http_iniciar_assinc("GET", p->url, cabs, NULL,
        (int)(p->prazo_ms ? p->prazo_ms : 15000),
        (int)(p->max_bytes ? p->max_bytes : REDE_CORPO_PADRAO), 2);
    free(cabs);
    nv_http_esperar_pedido(p, id);
    r->primeiro_byte_ms = (unsigned)nv_http_ttfb(id);
    { int cancelado = redeCancelouLocal;
      if (!nv_http_colher_assinc(id, &n, &r->status, NULL, 0,
                                 NULL, 0, NULL, NULL, NULL, 0, NULL, 2) || cancelado) {
        /* Cabecalhos colhidos nao viram resposta num cancelamento. */
        r->status = 0;
        r->erro = cancelado ? REDE_CANCELADO : REDE_TRANSPORTE;
        return 0;
      } }
    r->n_corpo = (size_t)n;
    r->bytes_fio = (uint64_t)n;
    r->ms = (unsigned)(redeAgoraMs() - ini);
    r->erro = REDE_OK;
    return 1;
  }
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
static void (*avisoSaude)(int codigo, const char *url, unsigned primeiraMs);
void rede_avisar_saude(void (*f)(int codigo, const char *url, unsigned primeiraMs)) { avisoSaude = f; }
// Ouvinte do pedido por host (o painel de registro, aba Rede) — ver rede.h.
static void (*avisoHost)(const char *url, int codigo, int http, unsigned ms);
void rede_avisar_host(void (*f)(const char *url, int codigo, int http, unsigned ms)) { avisoHost = f; }

#ifdef NV_VIDAA
// VAZIO E O PADRAO, como em todo outro arquivo que usa NV_REC_URL (ver
// recomenda.c, xtream.c, avisos.c, noticias.c, tex_cache.c): tools/env.sh so
// emite o -D quando o servico de recomendacoes esta configurado, e uma build
// sem ele nao pode falhar a compilar por causa disto.
#include "nvbackend.h"
#ifndef NV_REC_URL
#define NV_REC_URL ""
#endif
// O /v1/proxy mora no worker da TV (servidor/tv), nao mais na API: la ele nao
// some quando alguem publica a API de outra arvore (issue #135).
#ifndef NV_VIDAA_PROXY_URL
#define NV_VIDAA_PROXY_URL "https://nuvio-tv.henriquef29.workers.dev"
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
  char rec[400];
  if (nv_rec_url_copiar(rec, sizeof rec) && !strncmp(url, rec, strlen(rec))) return 1;
  if (!strncmp(url, NV_VIDAA_PROXY_URL, strlen(NV_VIDAA_PROXY_URL))) return 1;
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

// "NV_VIDAA_PROXY_URL/v1/proxy?u=<url-encoded>", num buffer malloc'ado (o chamador
// libera). NULL so por falta de memoria.
static char *vidaaUrlProxy(const char *url) {
  char *enc = vidaaUrlEncode(url), *out;
  size_t tam;
  if (!enc) return NULL;
  tam = strlen(NV_VIDAA_PROXY_URL) + strlen("/v1/proxy?u=") + strlen(enc) + 1;
  out = (char *)malloc(tam);
  if (out) snprintf(out, tam, "%s/v1/proxy?u=%s", NV_VIDAA_PROXY_URL, enc);
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

static int nvPrazo(int segundos) {
  return segundos > 0 ? (segundos > INT_MAX / 1000 ? INT_MAX : segundos * 1000) : 15000;
}

static char *pedir2(const char *metodo, const char *url, int segundos, const char *const *cab,
                    const char *extraCab, const char *corpo,
                    long *tam, int *status, char *etag, unsigned tamEtag,
                    int *retryAfter) {
  char *cabs, *corpoResp;
  int n = 0, http = 0;
#ifdef NV_VIDAA
  unsigned long inicioPedido = redeAgoraMs();
  char *urlProxiada = NULL;
  const char *urlEfetiva = url;
#endif
  if (etag && tamEtag) etag[0] = 0;
  if (status) *status = 0;
  if (retryAfter) *retryAfter = 0;
  if (!url || !*url) return NULL;
  if (!status && (!metodo || !strcmp(metodo, "GET")) && negcache_barra(url, (long)time(NULL))) return NULL;
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
  corpoResp = nv_http(metodo, urlEfetiva, cabs, corpo, &n, &http,
                      redeFinalDst, (int)redeFinalTam, etag, (int)tamEtag, retryAfter, nvPrazo(segundos));
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
      char rec[400];
      char *r2;
      fim2[0] = 0;
      nv_rec_url_copiar(rec, sizeof rec);
      unsigned long gastoPedido = redeAgoraMs() - inicioPedido;
      int resta = nvPrazo(segundos) - (int)gastoPedido;
      r2 = resta > 0 ? nv_http(metodo, urlP, cabs2, corpo, &n2, &http2, fim2, (int)sizeof fim2, etag, (int)tamEtag, NULL, resta) : NULL;
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
          !(rec[0] && !strncmp(fim2, rec, strlen(rec))) &&
          strncmp(fim2, NV_VIDAA_PROXY_URL, strlen(NV_VIDAA_PROXY_URL))) {
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
  corpoResp = nv_http(metodo, url, cabs, corpo, &n, &http,
                      redeFinalDst, (int)redeFinalTam, etag, (int)tamEtag, retryAfter, nvPrazo(segundos));
  free(cabs);
#endif
  if (status) *status = http;
  // XHR sem status e sem corpo = nao houve resposta (rede, DNS, CORS): para a
  // saude da rede e o "6" do curl. Qualquer status e transporte que funcionou.
  if (avisoSaude) avisoSaude(corpoResp || http ? 0 : 6, url, 0);
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

static char *pedir(const char *metodo, const char *url, int segundos, const char *const *cab,
                   const char *extraCab, const char *corpo,
                   long *tam, int *status) {
  return pedir2(metodo, url, segundos, cab, extraCab, corpo, tam, status, NULL, 0, NULL);
}

char *rede_baixar_etag(const char *url, int segundos, const char *const *cab,
                       int *status, char *etag, unsigned tamEtag) {
  return pedir2("GET", url, segundos, cab, NULL, NULL, NULL, status, etag, tamEtag, NULL);
}

char *rede_baixar(const char *url, int segundos) {
  return pedir("GET", url, segundos, NULL, NULL, NULL, NULL, NULL);
}

char *rede_baixar_bin(const char *url, int segundos, long *tam) {
  return pedir("GET", url, segundos, NULL, NULL, NULL, tam, NULL);
}
char *rede_baixar_bin_seguro(const char *url, int segundos, long *tam) {
  // No navegador (.wgt) o TLS e sempre verificado pela plataforma — mesma
  // funcao; so o nativo tem o caminho publico sem verificacao.
  return rede_baixar_bin(url, segundos, tam);
}

char *rede_baixar_com(const char *url, int segundos, const char *const *cab) {
  return pedir("GET", url, segundos, cab, NULL, NULL, NULL, NULL);
}

// GET em fluxo no .wgt: o XHR sincrono so entrega o corpo inteiro, entao o
// "fluxo" aqui e o corpo baixado de uma vez, entregue em pedacos de 64 KB. O
// teto de bytes vale do mesmo jeito (o heap WASM e o limite).
int rede_baixar_fluxo(const char *url, int segundos, const char *const *cab,
                      int (*cb)(const char *dados, size_t n, void *u), void *u,
                      long long maxBytes, int *status, long long *bytes) {
  long tam = 0, i;
  char *b;
  if (bytes) *bytes = 0;
  if (status) *status = 0;
  if (!cb) return 0;
  b = pedir("GET", url, segundos, cab, NULL, NULL, &tam, status);
  if (!b) return 0;
  if (maxBytes > 0 && tam > maxBytes) { free(b); return 0; }
  if (bytes) *bytes = tam;
  for (i = 0; i < tam; i += 65536)
    if (!cb(b + i, (size_t)(tam - i < 65536 ? tam - i : 65536), u)) break;
  free(b);
  return status ? (*status >= 200 && *status < 300) : 1;
}

// Ver rede.h. Sem `status` para pedir2, que entao ja devolve NULL em >= 400
// e loga so o host (rede_url_publica) — a url do painel vai no corpo e nunca
// passa por aqui em log.
char *rede_postar_bin(const char *url, int segundos, const char *corpo, long *tam) {
  return pedir("POST", url, segundos, NULL, "Content-Type: text/plain", corpo ? corpo : "", tam, NULL);
}

char *rede_baixar_st(const char *url, int segundos, const char *const *cab,
                     int *status) {
  return pedir("GET", url, segundos, cab, NULL, NULL, NULL, status);
}
char *rede_baixar_st_retry(const char *url, int segundos, const char *const *cab,
                           int *status, int *retryAfter) {
  return pedir2("GET", url, segundos, cab, NULL, NULL, NULL, status, NULL, 0, retryAfter);
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
  if (erro) *erro = 0;
  if (final && tamFinal) final[0] = 0;
  snprintf(faixa, sizeof faixa, "Range: bytes=%lld-%lld", ini, fim);
  cab[0] = faixa; cab[1] = NULL;
  rede_teto = (long)(fim - ini + 1);
  redeFinalDst = final; redeFinalTam = final ? tamFinal : 0;
  r = pedir("GET", url, segundos, cab, NULL, NULL, tam, &st);
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
  r = pedir("DELETE", url, segundos, cab, NULL, NULL, NULL, status);
  // 204 sem corpo e a resposta NORMAL de um DELETE aceito. Devolver NULL ali
  // faria o chamador ler sucesso como falha de transporte.
  if (!r && status && *status > 0) return strdup("");
  return r;
}

static char *escrever_st(const char *metodo, const char *url, int segundos,
                         const char *const *cab, const char *corpo, int *status, int *retryAfter) {
  int temCt = 0, k;
  char *r;
  // JSON e o padrao (Supabase, Trakt); quem manda o proprio Content-Type
  // (Real-Debrid quer form-urlencoded) nao recebe um segundo.
  for (k = 0; cab && cab[k]; k++)
    if (!strncasecmp(cab[k], "Content-Type:", 13)) temCt = 1;
  r = pedir2(metodo, url, segundos, cab, temCt ? NULL : "Content-Type: application/json",
             corpo ? corpo : "", NULL, status, NULL, 0, retryAfter);
  // Falha de TRANSPORTE continua sendo NULL; corpo de 4xx e devolvido. Um POST
  // que respondeu com corpo vazio devolve "" e nao NULL, como no outro caminho.
  if (!r && status && *status > 0) return strdup("");
  return r;
}

char *rede_postar_st(const char *url, int segundos, const char *const *cab,
                     const char *corpo, int *status) {
  return escrever_st("POST", url, segundos, cab, corpo, status, NULL);
}
char *rede_putar_st(const char *url, int segundos, const char *const *cab,
                    const char *corpo, int *status) {
  return escrever_st("PUT", url, segundos, cab, corpo, status, NULL);
}
char *rede_postar_st_retry(const char *url, int segundos, const char *const *cab, const char *corpo, int *status, int *retryAfter) {
  return escrever_st("POST", url, segundos, cab, corpo, status, retryAfter);
}
// Account row updates use the same WGT bridge as authenticated POST/DELETE.
char *rede_patch_st(const char *url, int segundos, const char *const *cab,
                    const char *corpo, int *status) {
  char *r = pedir2("PATCH", url, segundos, cab, "Content-Type: application/json", corpo, NULL, status, NULL, 0, NULL);
  if (!r && status && *status > 0) return strdup("");
  return r;
}

// PUT de JSON pela mesma ponte (o TorBox edita um item so por PUT).
char *rede_colocar_st(const char *url, int segundos, const char *const *cab,
                      const char *corpo, int *status) {
  char *r = pedir2("PUT", url, segundos, cab, "Content-Type: application/json",
                   corpo ? corpo : "", NULL, status, NULL, 0, NULL);
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

#ifdef NV_VIDAA
// HEAD never downloads a media body, including when Range would be ignored;
// in --um-fio nv_http is fetch() and yields to the other fibers meanwhile.
// 1 resolved, 0 dead (real HTTP error), -1 unknown (blocked, or HEAD refused:
// signed GET URLs answer HEAD with 403).
static int sondaVidaa(const char *url, int segundos, const char *cabs, char *dst, unsigned tam, int *http) {
  int n = 0;
  char *corpo;
  *http = 0;
  dst[0] = 0;
  corpo = nv_http("HEAD", url, cabs, NULL, &n, http, dst, (int)tam, NULL, 0, NULL, nvPrazo(segundos));
  if (!corpo) return -1;
  free(corpo);
  if (*http == 403 || *http == 405 || *http == 501) return -1;
  if (*http >= 200 && *http < 300 && dst[0]) return 1;
  return *http > 0 ? 0 : -1;
}
#else
// Probe headers only: abort before downloading media, even if Range is ignored.
static int nv_url_sonda(const char *url, const char *cabs, char *dst,
                         int tam, int *status, char *mime, int mimeTam, long *corpo, int prazo) {
  int id;
  char *headers = cabs && *cabs ? NULL : strdup("Range: bytes=0-63");
  if (cabs && *cabs) {
    headers = malloc(strlen(cabs) + 22);
    if (headers) sprintf(headers, "%s\nRange: bytes=0-63", cabs);
  }
  if (!headers) return 0;
  id = nv_http_iniciar_assinc("GET", url, headers, NULL, prazo, 64, 1);
  free(headers);
  nv_http_esperar(id);
  char *ok = nv_http_colher_assinc(id, NULL, status, dst, tam, NULL, 0, NULL,
                                  NULL, mime, mimeTam, corpo, 1);
  return ok && *status >= 200 && *status < 300 && dst[0];
}
#endif

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
#ifdef NV_VIDAA
  // No type/size from HEAD (mime empty, corpo -1). Unknown is not dead: the
  // player tries the URL itself (same policy as verificarUma).
  ok = sondaVidaa(url, segundos, cabs, dst, tam, &http);
  if (ok < 0) ok = snprintf(dst, tam, "%s", url) < (int)tam;
#else
  ok = nv_url_sonda(url, cabs, dst, (int)tam, &http, mime,
                    mime && mimeTam < INT_MAX ? (int)mimeTam : 0, &c32, nvPrazo(segundos));
#endif
  free(cabs);
  if (corpo) *corpo = c32;
  if (status) *status = http;
  return ok;
}

int rede_url_final(const char *url, int segundos, char *dst, unsigned tam) {
  return rede_url_final_cab(url, segundos, NULL, dst, tam, NULL);
}

#ifdef NV_VIDAA
// Ver rede.h. NUNCA passa pelo proxy (o proprio pedir2 evita isto por
// vidaaPareceVideo — url de video sempre volta 415 do worker), entao esta
// chamada e SEMPRE direta: um bloqueio de conteudo misto ou de CORS aqui e
// ROTINEIRO, nao a excecao, e e exatamente o que -1 comunica ao chamador.
int rede_url_final_vidaa(const char *url, int segundos, char *dst, unsigned tam) {
  int http;
  if (!url || !*url || !dst || tam == 0) return -1;
  return sondaVidaa(url, segundos, NULL, dst, tam, &http);
}
#endif

// Tizen throughput uses timed Range chunks, starting at 1 MB and doubling
// below 0.7 s up to 8 MB. Fetch discards the body in JavaScript and aborts
// at the chunk limit or remaining deadline. Even when Range is ignored,
// bytes/kbps describe only bytes consumed before abort, never the file size.
// Each chunk contributes at least one sample, repeated per elapsed second.
static int nv_http_contar(const char *url, const char *cabs, double ini,
                            double fim, int *status, char *urlFinal,
                            int urlFinalTam, int prazo) {
  int id, n = 0;
  char *headers = malloc((cabs ? strlen(cabs) : 0) + 96);
  if (!headers) return -1;
  sprintf(headers, "%s%sRange: bytes=%.0f-%.0f", cabs ? cabs : "", cabs && *cabs ? "\n" : "", ini, fim);
  id = nv_http_iniciar_assinc("GET", url, headers, NULL, prazo, (int)(fim - ini + 1), 2);
  free(headers);
  nv_http_esperar(id);
  return nv_http_colher_assinc(id, &n, status, urlFinal, urlFinalTam, NULL, 0, NULL,
                              NULL, NULL, 0, NULL, 2) ? n : -1;
}

void rede_vazao_espera(unsigned long ms) { (void)ms; }

int rede_medir_vazao(const char *url, const char *const *cab, int segundos,
                     long inicio, long long maxBytes, volatile int *cancelado,
                     int *kbps, int nMax, RedeVazao *res,
                     char *final, unsigned tamFinal) {
  char *cabs;
  char atual[NV_MEDIA_URL_MAX];
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
    char fin[NV_MEDIA_URL_MAX];
    if (cancelado && *cancelado) { if (res) res->cancelado = 1; break; }
    fin[0] = 0;
    n = nv_http_contar(atual, cabs, (double)pos, (double)(pos + pedaco - 1), &st,
                       fin, (int)sizeof fin, (int)(janela - gasto));
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
    if (st == 200) break;              // Range ignored: bounded sample only
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

int rede_aquecer_job(const char *const *origens, int n, unsigned *ms, RedeJob *job) {
  (void)job; return rede_aquecer_lote(origens, n, ms);
}

// Navegador nao abre socket. O websocket do Tizen web usa o WebSocket do
// proprio navegador (discordws.c); estes existem para o modulo linkar.
RedeTls *rede_tls_abrir(const char *url, int segundos) { (void)url; (void)segundos; return NULL; }
int rede_tls_enviar(RedeTls *t, const void *buf, size_t n) { (void)t; (void)buf; (void)n; return -1; }
int rede_tls_tentar_enviar(RedeTls *t, const void *buf, size_t n, size_t *foi) { (void)t; (void)buf; (void)n; if (foi) *foi = 0; return -1; }
int rede_tls_receber(RedeTls *t, void *buf, size_t n, int esperaMs) { (void)t; (void)buf; (void)n; (void)esperaMs; return -1; }
void rede_tls_fechar(RedeTls *t) { (void)t; }

#else

// FAROL (#34): geracao do cache de DNS/conexoes do cliente. So existe no
// ramo nativo: no Emscripten rede_zerar_cache e no-op e a geracao e sempre 0.
static unsigned redeGerCache;
unsigned rede_cache_geracao(void) { return __atomic_load_n(&redeGerCache, __ATOMIC_ACQUIRE); }

#ifndef NV_TPK40   // no NV_TPK40 estes nomes sao macros da struct por fio (topo)
// Codigo da libcurl do ultimo pedido DESTE fio (0 = transporte ok). So o
// rede_baixar_trecho_st le: o resto do modulo segue devolvendo NULL e logando.
static _Thread_local int redeCurlLocal;
#endif
// Numeric transport error for this thread. Raw backend errors may contain
// request URLs/credentials, so never publish them in the UI or log (#418).
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
#if !defined(NV_TPK40) && !defined(NV_UM_FIO)
static _Thread_local unsigned long redePrazoAtualMs;
static _Thread_local int redeSemRecuoV4;
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
// CURLINFO_PRIMARY_IP = CURLINFO_STRING (0x100000) + 32: IP do socket conectado.
#define INFO_IP_PRIMARIO     1048608
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
#define OPT_SOCKOPTFUNCTION  20148
#define OPT_SOCKOPTDATA      10149
// Keepalive de TCP (7.25+) nas conexoes guardadas no cache do handle.
#define OPT_TCP_KEEPALIVE     213
#define OPT_TCP_KEEPIDLE      214
#define OPT_TCP_KEEPINTVL     215
// CURLINFO_NUM_CONNECTS = CURLINFO_LONG + 26: 0 = o pedido foi por conexao
// REUSADA. So para o log de falha.
#define INFO_NUM_CONNECTS  2097178
#define INFO_REDIRECT_URL 1048607
#define INFO_SIZE_DOWNLOAD 3145736
// CURLINFO_LASTSOCKET = CURLINFO_LONG + 29. ACTIVESOCKET (7.45+) seria o certo,
// mas LASTSOCKET existe em toda versao que estas TVs trazem e devolve o mesmo
// descritor quando ha uma conexao so (ver rede_tls_abrir e a CancelVigia).
#define INFO_LASTSOCKET  2097181
#define OPT_CAINFO 10065
#define OPT_ERRORBUFFER 10010
#define OPT_POSTFIELDSIZE 60
// CURLOPT_REDIR_PROTOCOLS em bitmask (7.19.4+): o *_STR da libcurl 7.85 nao
// existe nas libcurl antigas das TVs. CURLPROTO_HTTPS = 2.
#define OPT_REDIR_PROTOCOLS 182
// CURLOPT_MAXREDIRS: teto de saltos de uma cadeia de redirects.
#define OPT_MAXREDIRS        68
#define OPT_PROTOCOLS 181
// CURLOPT_IPRESOLVE (7.10.8+): 0 = qualquer familia, 1 = so IPv4, 2 = so IPv6.
// Ver o "v6 ruim" por host junto de performV4.
#define OPT_IPRESOLVE        113

// Ouvinte unico dos 401 — ver rede_avisar_401 no cabecalho. (O ramo
// Emscripten tem a sua propria definicao, porque os dois lados do #ifdef
// compilam separado.)
static void (*aviso401)(const char *url);
void rede_avisar_401(void (*f)(const char *url)) { aviso401 = f; }
// Ouvinte da saude da rede — ver rede_avisar_saude no cabecalho.
static void (*avisoSaude)(int codigo, const char *url, unsigned primeiraMs);
void rede_avisar_saude(void (*f)(int codigo, const char *url, unsigned primeiraMs)) { avisoSaude = f; }
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
  url = nv_media_url(url);
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
// ROTA IPv6 QUEBRADA POR HOST (#707): a declaracao completa (com a vigia)
// fica junto das do cancelVigia, logo depois das funcoes multi.
static unsigned long conexaoMs(unsigned long prazoMs);
static void  (*curl_cleanup)(void *);
static int   (*curl_global)(long);
static void *(*curl_multi_init)(void);
static int   (*curl_multi_add)(void *, void *);
static int   (*curl_multi_remove)(void *, void *);
static int   (*curl_multi_perform)(void *, int *);
static int   (*curl_multi_poll)(void *, void *, unsigned, int, int *);
static int   (*curl_multi_wait)(void *, void *, unsigned, int, int *);
static int   (*curl_multi_cleanup)(void *);
static int   (*curl_multi_setopt)(void *, int, ...);
static void *(*curl_multi_info_read)(void *, int *);
static void *(*slist_append)(void *, const char *);
static void  (*slist_free)(void *);
static int   (*curl_getinfo)(void *, int, ...);
static void  (*curl_reset)(void *);
static time_t (*curl_getdate_fn)(const char *, const time_t *);

// Definida com a vigia de cancelamento mais abaixo (#775); o tmdbFallback a
// usa quando o pedido ja esta num multi proprio.
typedef struct { void *multi; int ligado; } CancelVigia;
static int cancelVigiaPerform(CancelVigia *v, void *c);
static int cancelVigiaLigar(void *c, CancelVigia *v);
static void cancelVigiaSoltar(CancelVigia *v, int ligado, void *c);
// O recuo IPv4 do #707 roda cada tentativa por cancelVigiaPerform — ver
// performV4 logo depois de hostDaUrl. v->multi NULL cai no easy perform.
static int performV4(void *c, const char *url, CancelVigia *v);

#ifdef NV_TPK
static void *dnsShare;
static pthread_mutex_t dnsTrava = PTHREAD_MUTEX_INITIALIZER;
static void dnsLock(void *c, int data, int access, void *arg) {
  (void)c; (void)data; (void)access; (void)arg;
  pthread_mutex_lock(&dnsTrava);
}
static void dnsUnlock(void *c, int data, void *arg) {
  (void)c; (void)data; (void)arg;
  pthread_mutex_unlock(&dnsTrava);
}
static void prepararDns(void *h, const char *ares) {
  void *(*init)(void);
  int (*option)(void *, int, ...);
  int (*cleanup)(void *);
  void *s;
  /* #630: c-ares 1.17 uses retrans: in milliseconds, retry: (not
     timeout:/attempts:). Two rounds at 250/500 ms per nameserver fit
     the usual 1-2 server setup inside the 2 s connect deadline. Keep
     nameserver order, dual-stack and explicit operator RES_OPTIONS. */
  if (ares && !strncmp(ares, "1.17.", 5))
    setenv("RES_OPTIONS", "retrans:250 retry:2", 0);
  *(void **)(&init) = dlsym(h, "curl_share_init");
  *(void **)(&option) = dlsym(h, "curl_share_setopt");
  *(void **)(&cleanup) = dlsym(h, "curl_share_cleanup");
  if (!init || !option || !cleanup || !(s = init())) return;
  /* CURLSHOPT_LOCKFUNC/UNLOCKFUNC/SHARE; CURL_LOCK_DATA_DNS only.
     Process-lifetime cache survives failed handles and worker exits.
     Connections and TLS sessions remain private to each worker. */
  if (option(s, 3, dnsLock) || option(s, 4, dnsUnlock) || option(s, 1, 3)) {
    cleanup(s); return;
  }
  dnsShare = s;
}
#endif

// Chamado depois da repeticao existente do GET, diretamente nos demais caminhos.
// So falha de TRANSPORTE que falhou RAPIDO: conexao recusada (7), aperto de mao
// TLS que nao fechou (35), resposta vazia (52) e recepcao que caiu (56). O curl
// 28 (timeout) NAO entra: ele so acontece quando o prazo do pedido ja foi
// gasto inteiro, e a tentativa no alias dobraria a espera maxima de quem
// chamou — o mesmo principio de valeRepetir logo abaixo ("o que ja custou o
// prazo inteiro nao repete").
static int tmdbFallback(void *c, int r, const char *pedido, void *cv) {
  char *url = NULL, *alias, *origem;
  long http = 0;
  if (!(r == 35 || r == 7 || r == 56 || r == 52) ||
      redeCancelouLocal || !curl_getinfo) return r;
  // ESCRITA que pode ja ter sido aplicada nao repete no alias: resposta
  // perdida (52: o servidor recebeu e calou) ou corpo cortado no retorno (56)
  // deixam o POST no servidor — reenviar duplicaria a acao (#707 r2). O flag
  // e o mesmo que ja barra o recuo IPv4 no caminho do POST.
  if (redeSemRecuoV4 && (r == 52 || r == 56)) return r;
  // Um redirect de outro host ao TMDB nao autoriza reenviar seus cabecalhos.
  origem = tmdbUrl(pedido);
  if (!origem) return r;
  free(origem);
  if (curl_getinfo(c, INFO_RESPONSE_CODE, &http) || http ||
      curl_getinfo(c, INFO_URL_FINAL, &url) || !(alias = tmdbUrl(url))) return r;
  if (!curl_setopt_f(c, OPT_URL, alias)) {
    /* Uma unica tentativa no alias, sem recursao: com o recuo IPv4 ela tambem
       pode ter duas performs, cada uma cancelavel pela vigia ja armada. */
    r = performV4(c, alias, (CancelVigia *)cv);
    if (!r) tmdbLigou();
  }
  free(alias);
  return r;
}
static int performTmdbCv(void *c, const char *url, void *cv) {
  return tmdbFallback(c,
           cv ? performV4(c, url, (CancelVigia *)cv) : performV4(c, url, NULL),
           url, cv);
}
// TODA perform sem vigia propria vai pelo multi do fio quando a interface
// existe (pre-aquecimento, sondas, rede_pedido.inc): o cache de conexoes fica
// no multi, que o fio guarda e que viaja no estacionamento JUNTO com a easy.
// Com curl_easy_perform a conexao ficaria na easy de um fio e o fio consumidor
// a associaria ao proprio multi — o HEAD aquecido nunca seria reusado. Sem a
// interface multi (libcurl antiga), performV4 cai no easy perform.
static int performTmdb(void *c, const char *url) {
  CancelVigia cv;
  int on, r;
  on = cancelVigiaLigar(c, &cv);
  r = tmdbFallback(c, performV4(c, url, &cv), url, on ? &cv : NULL);
  cancelVigiaSoltar(&cv, on, c);
  return r;
}

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
// On TPK, opcoesComuns attaches a shared DNS cache across these handles (#630).
//
// Bonus: `curl_easy_cleanup` era onde o OpenSSL 1.0 morria (ver
// prepararOpenSSL); agora ele so roda no fim do fio.
// HANDLE + MULTI POR FIO (#775). O handle pode sair do handleChave quando a
// conexao esta presa ao multi de cancelamento persistente do fio: quem
// desassociar a easy no meio do pedido nao pode deixar o multi apontando para
// um handle limpo/destruido, entao a dupla vive junta nesta caixinha e morre
// junta (multi primeiro: libcurl devolve as conexoes ociosas dele; easy
// depois). O multi tem o PROPRIO cache de conexoes — o que preserva o reuso
// entre pedidos cancelaveis e nao cancelaveis sem CURLSHARE CONNECT.
// `ger`: a geracao do cache de rede (rede_zerar_cache) que este pool ja viu.
typedef struct { void *c; void *multi; unsigned ger; } HandlePool;
// FAROL (#34): rede_zerar_cache sobe a geracao (redeGerCache acima); cada fio
// descarta o PROPRIO par easy+multi na proxima vez que pega handle (nunca no
// meio de uma transferencia, de nenhum fio) e o handle novo pede a poda do
// cache de DNS.
// Pool novo nasce na geracao de agora: nada a descartar no primeiro pedido.
static HandlePool *poolAlocar(void) {
  HandlePool *p = (HandlePool *)calloc(1, sizeof *p);
  if (p) p->ger = __atomic_load_n(&redeGerCache, __ATOMIC_ACQUIRE);
  return p;
}
static pthread_key_t handleChave;
static pthread_once_t handleUma = PTHREAD_ONCE_INIT;
static void handleCriarChave(void);
// Guarda o multi de cancelamento no pool do handle do fio corrente. O cache
// de conexoes do pedido cancelavel vai junto: e ele (e nao o da easy) que
// mantinha as conexoes abertas, e um multi temporario por pedido as fechava
// ao fim — GET cancelavel virava sempre conexao nova. Devolve 1 se guardou;
// com 0 quem chama faz o curl_multi_cleanup de sempre.
static int redeMultiGuardar(void *m) {
  HandlePool *p;
  if (!curl_reset) return 0;      // sem pool de handle, nao ha onde guardar
  pthread_once(&handleUma, handleCriarChave);
  p = (HandlePool *)pthread_getspecific(handleChave);
  if (!p) {
    p = poolAlocar();
    if (!p || pthread_setspecific(handleChave, p) != 0) { free(p); return 0; }
  }
  if (p->multi) curl_multi_cleanup(p->multi);   // nunca deveria sobrar
  p->multi = m;
  return 1;
}
static void *redeMultiPegar(void) {
  HandlePool *p = (HandlePool *)pthread_getspecific(handleChave);
  void *m = p ? p->multi : NULL;
  if (p) p->multi = NULL;
  return m;
}
// Destrutor da chave: a easy sai do multi ANTES de qualquer cleanup — uma easy
// associada a um multi destruido quebra a proxima perform, e destruir a easy
// associada tambem e proibido.
static void handleSoltar(void *pv) {
  HandlePool *p = (HandlePool *)pv;
  if (!p) return;
  if (p->multi && p->c && curl_multi_remove) curl_multi_remove(p->multi, p->c);
  if (p->multi && curl_multi_cleanup) curl_multi_cleanup(p->multi);
  if (p->c && curl_cleanup) curl_cleanup(p->c);
  free(p);
}
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

// "V6 RUIM" POR HOST (#707). O relato: o roteador do aparelho anunciava uma
// rota IPv6 MORTA (Tizen 10, MediaFusion + TorBox). O connect de um host com
// AAAA+A ia pelo v6 e nunca chegava — e a libcurl 7.53.1 da TV nao refaz em
// IPv4 sozinha em todo caminho. Na interface virava N falhas de transporte
// seguidas em hosts diferentes (a sonda de cada fonte), que e exatamente o
// criterio do "sem internet" da redesaude: a pessoa lia "sem internet" com a
// casa online — e o Stremio, que recua para v4, tocava o mesmo titulo.
//
// Mecanismo, irmao do calmaHost (#385): a tentativa em familia QUALQUER que
// falhou ANTES de abrir a conexao (o nome nao resolveu, o connect caiu ou o
// prazo morreu sem resposta) refaz o pedido UMA vez com IPRESOLVE=V4 no MESMO
// handle, e o host (esquema+host+porta) fica marcado: os pedidos seguintes ja
// saem direto em v4, sem pagar a tentativa v6. A marca caduca — a rota pode
// voltar — e cai fora do vetor por rodizio, como os demais caches por host.
//
// O QUE NAO DISPARA: cancelamento (42), corpo parado (o pedido ja tinha
// conexao aberta), resposta HTTP de erro (o servidor respondeu) e TLS (35):
// o aperto de mao so comeca DEPOIS do connect, entao a familia ja deu certo.
#define REDE_V4_HOSTS 8
#define REDE_V4_MARCA_MS (10UL * 60UL * 1000UL)
typedef struct { char h[96]; unsigned long ate; } V4Host;
static V4Host v4Host[REDE_V4_HOSTS];
static int v4Prox;
static pthread_mutex_t v4Trava = PTHREAD_MUTEX_INITIALIZER;

static int v4Marcado(const char *url, unsigned long agora) {
  char h[96];
  int i, achou = 0;
  hostDaUrl(url, h, sizeof h);
  if (!h[0]) return 0;
  pthread_mutex_lock(&v4Trava);
  for (i = 0; i < REDE_V4_HOSTS; i++)
    if (v4Host[i].h[0] && !strcmp(v4Host[i].h, h)) {
      if (v4Host[i].ate > agora) achou = 1;
      else v4Host[i].h[0] = 0;   // caducou: a rota v6 pode ter voltado
      break;
    }
  pthread_mutex_unlock(&v4Trava);
  return achou;
}
static void v4Marcar(const char *url, unsigned long agora) {
  char h[96];
  int i, achou = -1;
  hostDaUrl(url, h, sizeof h);
  if (!h[0]) return;
  pthread_mutex_lock(&v4Trava);
  for (i = 0; i < REDE_V4_HOSTS; i++)
    if (v4Host[i].h[0] && !strcmp(v4Host[i].h, h)) { achou = i; break; }
  if (achou < 0) {
    achou = v4Prox; v4Prox = (v4Prox + 1) % REDE_V4_HOSTS;
    snprintf(v4Host[achou].h, sizeof v4Host[achou].h, "%s", h);
  }
  v4Host[achou].ate = agora + REDE_V4_MARCA_MS;
  pthread_mutex_unlock(&v4Trava);
}

// A conexao TCP abriu? Sinal da libcurl, nunca limiar de tempo: loopback e
// LAN conectam em dezenas de us, e um 28 com o TLS parado nao e rota v6.
// CONNECT_TIME > 0 = connect fechado nesta tentativa; IP primario preenchido =
// havia socket conectado (a 8.x zera o CONNECT_TIME de conexao REUSADA que
// estoura, mas mantem o IP). Sem conexao (6, 7, connect engolido) as duas
// vem zeradas. Getinfo que falha conta como "abriu": na duvida nao recua.
static int v4TcpAbriu(void *c) {
  double tcp = 0;
  char *ip = NULL;
  if (curl_getinfo(c, INFO_TEMPO_TCP, &tcp) || tcp != 0.0) return 1;
  if (curl_getinfo(c, INFO_IP_PRIMARIO, &ip)) return 1;
  return ip && ip[0];
}
// Conexao que nem abriu e a rota v6 pode ser a culpada: o nome nao resolveu
// (6, o AAAA que morre no caminho v6 quebrado), o connect caiu (7: sem rota,
// recusa, blackhole que responde RST tarde) ou o prazo morreu sem nenhuma
// resposta HTTP (28 com http=0: o connect v6 engolido em silencio). Cancelado
// e codigo sem libcurl/getinfo ficam de fora.
static int v4Vale(int r, void *c) {
  long http = 0;
  if (redeCancelouLocal || redeParou()) return 0;
  if (r == 6 || r == 7) return 1;
  if (r != 28) return 0;
  if (!curl_getinfo) return 0;
  curl_getinfo(c, INFO_RESPONSE_CODE, &http);
  if (http != 0) return 0;
  return !v4TcpAbriu(c);
}
// A easy do pedido cancelavel esta associada ao multi do FIO (pool); uma
// vigia temporaria em cima dele deixa cada tentativa cancelavel sem mexer no
// fluxo de cancelVigiaLigar/Soltar de quem chamou — o multi volta para o pool
// por la, com o cache de conexoes junto.
static void v4VigiaPegar(void *c, CancelVigia *v) {
  HandlePool *p;
  v->multi = NULL;
  v->ligado = 0;
  if (!curl_reset || !curl_multi_init || !curl_multi_add || !curl_multi_remove ||
      !curl_multi_perform || !curl_multi_cleanup || !curl_multi_info_read ||
      !(curl_multi_poll || curl_multi_wait)) return;
  pthread_once(&handleUma, handleCriarChave);
  p = (HandlePool *)pthread_getspecific(handleChave);
  if (p && p->c == c && p->multi) v->multi = p->multi;
}

// curl_easy_perform com o recuo IPv6->IPv4 do #707 e a vigia de cancelamento
// do #775 por tentativa: cada tentativa (dual-stack e, se valer, so IPv4) e um
// cancelVigiaPerform no mesmo multi — a 1a usa a easy ja associada por
// cancelVigiaLigar, a 2a a reassocia; redeParou() vale nas duas. Sem multi
// (v NULL sem pool, libcurl antiga) cai no curl_easy_perform de sempre. Vale para TODO caminho que passa por aqui: sonda de
// fonte, GET/POST, aquecimento, Range lateral — todos o chamam direto ou pelo
// performTmdb/performTmdbCv. A marca e do host DO URL DO PEDIDO: um
// redirecionamento que a libcurl segue internamente herda a familia desta
// tentativa, como sempre.
static int performV4(void *c, const char *url, CancelVigia *v) {
  CancelVigia local;
  unsigned long t0 = redeAgoraMs();
  unsigned long agora, prazoMs = redePrazoAtualMs;
  int r, marcado = v4Marcado(url, t0);
  long restMs;
  if (!v) { v4VigiaPegar(c, &local); v = &local; }
  curl_setopt(c, OPT_IPRESOLVE, (long)marcado);
  r = cancelVigiaPerform(v, c);
  if (marcado || redeSemRecuoV4 || !v4Vale(r, c)) return r;
  agora = redeAgoraMs();
  if (prazoMs > 0) {
    restMs = (long)(t0 + prazoMs > agora ? t0 + prazoMs - agora : 0UL);
    if (restMs < 500) return r;
    curl_setopt(c, OPT_TIMEOUT_MS, restMs);
    curl_setopt(c, OPT_CONNECTTIMEOUT_MS, (long)conexaoMs((unsigned long)restMs));
  }
  { char seg[120];
    printf("[rede] conexao falhou (curl %d) em %s: de novo so em IPv4\n",
           r, rede_url_publica(url, seg, sizeof seg));
    fflush(stdout); }
  curl_setopt(c, OPT_IPRESOLVE, (long)1);
  r = cancelVigiaPerform(v, c);
  { char seg[120];
    if (r) printf("[rede] IPv4 tambem falhou (curl %d) em %s\n",
                  r, rede_url_publica(url, seg, sizeof seg));
    else v4Marcar(url, redeAgoraMs());
    fflush(stdout); }
  return r;
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
// O par easy+multi viaja junto: o cache de conexoes do pre-aquecimento mora no
// multi (performTmdb passa por ele), e o fio consumidor precisa DOS DOIS para
// reusar o HEAD aquecido — a easy sozinha chegaria sem conexao nenhuma.
// `ger`: a geracao do cache em que o par foi estacionado (FAROL #34). Um HEAD
// que comecou ANTES de rede_zerar_cache e estacionou DEPOIS nao pode entregar
// conexoes/DNS do mundo velho ao fio que o adota — sem este campo ele entrava
// no pool ja com a geracao nova (poolAlocar) e ninguem mais o descartava.
typedef struct { void *c; void *multi; unsigned long ms; unsigned ger; int nh; char h[REDE_ESTAC_HOSTS][96]; } Estac;
static Estac estac[REDE_ESTAC_MAX];
static pthread_mutex_t estacTrava = PTHREAD_MUTEX_INITIALIZER;
// Chamar com estacTrava: joga fora os que passaram da validade.
static void estacVelhos(unsigned long agora) {
  int i;
  for (i = 0; i < REDE_ESTAC_MAX; i++)
    if (estac[i].c && agora - estac[i].ms > REDE_ESTAC_VALE_MS) {
      if (estac[i].multi && curl_multi_cleanup) curl_multi_cleanup(estac[i].multi);
      if (curl_cleanup) curl_cleanup(estac[i].c);
      estac[i].c = NULL;
      estac[i].multi = NULL;
    }
}
static void estacionar(void *c, void *multi, char h[][96], int nh) {
  unsigned long agora = redeAgoraMs();
  int i, livre = -1, velho = 0;
  pthread_mutex_lock(&estacTrava);
  estacVelhos(agora);
  for (i = 0; i < REDE_ESTAC_MAX; i++) {
    if (!estac[i].c) { livre = i; break; }
    if (estac[i].ms < estac[velho].ms) velho = i;
  }
  if (livre < 0) { livre = velho;
    if (estac[livre].multi && curl_multi_cleanup) curl_multi_cleanup(estac[livre].multi);
    if (curl_cleanup) curl_cleanup(estac[livre].c); }

  estac[livre].c = c; estac[livre].multi = multi;
  estac[livre].ms = agora; estac[livre].nh = nh;
  // Carimbo e a geracao de NASCIMENTO do par (a do pool do fio), nao a do
  // instante do estacionamento: e assim que um HEAD iniciado antes de
  // rede_zerar_cache e estacionado depois ainda morre no estacTomar.
  { HandlePool *pp = (HandlePool *)pthread_getspecific(handleChave);
    estac[livre].ger = pp ? pp->ger : __atomic_load_n(&redeGerCache, __ATOMIC_ACQUIRE); }
  for (i = 0; i < nh && i < REDE_ESTAC_HOSTS; i++) snprintf(estac[livre].h[i], 96, "%s", h[i]);
  pthread_mutex_unlock(&estacTrava);
}
static void *estacTomar(const char *url, void **multi) {
  char h[96];
  unsigned long agora = redeAgoraMs();
  void *c = NULL;
  int i, k;
  if (multi) *multi = NULL;
  hostDaUrl(url, h, sizeof h);
  if (!h[0]) return NULL;
  pthread_mutex_lock(&estacTrava);
  estacVelhos(agora);
  // FAROL (#34): par estacionado por um job que comecou antes do reset — a
  // conexao/DNS dele e do mundo velho mesmo tendo chegado depois. Morre na
  // hora, como os que rede_zerar_cache apaga sob a mesma trava.
  { unsigned gAgora = __atomic_load_n(&redeGerCache, __ATOMIC_ACQUIRE);
    for (i = 0; i < REDE_ESTAC_MAX; i++) {
      if (estac[i].c && estac[i].ger != gAgora) {
        if (estac[i].multi && curl_multi_cleanup) curl_multi_cleanup(estac[i].multi);
        if (curl_cleanup) curl_cleanup(estac[i].c);
        estac[i].c = NULL;
        estac[i].multi = NULL;
      } } }
  for (i = 0; i < REDE_ESTAC_MAX && !c; i++)
    for (k = 0; estac[i].c && k < estac[i].nh; k++)
      if (!strcmp(estac[i].h[k], h)) { c = estac[i].c;
        if (multi) *multi = estac[i].multi;
        estac[i].c = NULL; estac[i].multi = NULL; break; }
  pthread_mutex_unlock(&estacTrava);
  return c;
}

// FAROL (#34, "Reconectar a rede"): joga fora o cache de DNS e as conexoes
// ociosas do cliente SEM mexer em transferencia alguma. Os pares estacionados
// (ninguem os usa) morrem aqui; os de cada fio morrem na proxima pegarHandle
// daquele fio. Seguro de qualquer fio; barato (nao toca em rede).
void rede_zerar_cache(void) {
  int i;
  __atomic_add_fetch(&redeGerCache, 1u, __ATOMIC_ACQ_REL);
  pthread_mutex_lock(&estacTrava);
  for (i = 0; i < REDE_ESTAC_MAX; i++) {
    if (!estac[i].c) continue;
    if (estac[i].multi && curl_multi_cleanup) curl_multi_cleanup(estac[i].multi);
    if (curl_cleanup) curl_cleanup(estac[i].c);
    estac[i].c = NULL;
    estac[i].multi = NULL;
  }
  pthread_mutex_unlock(&estacTrava);
}

static void *pegarHandle(const char *url) {
  void *c, *multi = NULL;
  HandlePool *p;
  int zerou = 0;
  if (!curl_reset) return curl_init();     // libcurl sem reset: como antes
  pthread_once(&handleUma, handleCriarChave);
  p = (HandlePool *)pthread_getspecific(handleChave);
  c = p ? p->c : NULL;
  if (!c) {
    c = estacTomar(url, &multi);
    if (c) {
      if (!p) {
        p = poolAlocar();
        if (p && pthread_setspecific(handleChave, p) != 0) { free(p); p = NULL; }
      }
      if (p) {
        p->c = c;
        if (p->multi && p->multi != multi && curl_multi_cleanup)
          curl_multi_cleanup(p->multi);      // nunca deveria sobrar
        p->multi = multi;
      } else {
        if (multi && curl_multi_cleanup) curl_multi_cleanup(multi);
        curl_cleanup(c); c = NULL;
      }
    }
  }
  // Zerado pelo Farol: este fio ainda tem o par da geracao anterior. Nao ha
  // transferencia em curso neste fio (quem chama pegarHandle acabou a anterior),
  // entao o descarte e o mesmo do host ocioso, e DNS + conexoes + sessoes TLS
  // do fio morrem juntos.
  { unsigned g = __atomic_load_n(&redeGerCache, __ATOMIC_ACQUIRE);
    if (p && p->ger != g) { p->ger = g; zerou = 1; } }
  if (c && (zerou || hostOcioso(url, redeAgoraMs()))) {
    // O par inteiro vai embora: o cache de conexoes mora no multi e o socket
    // morto deste host esta la — manter o multi reusiaria essa conexao no
    // proximo pedido e o prazo inteiro seria gasto esperando resposta que
    // nao vem (rede_parada.sh, curl 28). Descartar o multi fecha TODAS as
    // conexoes do fio, como a base que derrubava o handle unico.
    if (p && p->multi) curl_multi_remove(p->multi, c);
    curl_cleanup(c);
    if (p && p->multi) { curl_multi_cleanup(p->multi); p->multi = NULL; }
    if (p) p->c = NULL;
    hostsEsquecer();
    c = NULL;
  }
  if (c) curl_reset(c);
  else {
    c = curl_init();
    if (c) {
      if (!p) {
        p = poolAlocar();
        if (p && pthread_setspecific(handleChave, p) != 0) { free(p); p = NULL; }
      }
      if (p) p->c = c; else { curl_cleanup(c); c = NULL; }
    }
  }
  // Depois do reset: curl_easy_reset volta MAXCONNECTS ao padrao.
  if (c) curl_setopt(c, OPT_MAXCONNECTS, REDE_CONEXOES_POR_FIO);
  // Cache de DNS zerado pelo Farol: DNS_CACHE_TIMEOUT 0 faz a libcurl podar na
  // hora as entradas do cache (inclusive o compartilhado do .tpk); o reset do
  // proximo pedido volta ao padrao de 60 s.
  if (c && zerou) curl_setopt(c, 92 /* CURLOPT_DNS_CACHE_TIMEOUT */, 0L);
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
static void descartarHandle(void *c) {
  HandlePool *p;
  // Se a easy esta associada ao multi do fio (cancelamento), tira antes de
  // limpar: a libcurl proibe cleanup de easy associada. O multi morre JUNTO:
  // e nele que ficou o cache de conexoes, e a conexao que falhou esta la —
  // manter o multi entregaria o socket morto ao proximo pedido.
  if (curl_reset) {
    p = (HandlePool *)pthread_getspecific(handleChave);
    if (p) {
      if (p->multi && curl_multi_remove) curl_multi_remove(p->multi, c);
      if (p->multi && curl_multi_cleanup) curl_multi_cleanup(p->multi);
      p->multi = NULL;
      p->c = NULL;
    }
  }
  curl_cleanup(c);
  if (curl_reset) {
    hostsEsquecer();
  }
}
static void soltarHandleDesde(void *c, int r, const char *url, unsigned primeiraMs) {
  // #382: HTTP recebido prova conectividade mesmo se o corpo falhou/parou.
  // O CURLcode original continua com o chamador e no painel de pedidos.
  if (avisoSaude) {
    long http = 0;
    if (curl_getinfo) curl_getinfo(c, INFO_RESPONSE_CODE, &http);
    // #382: "Resolving timed out" (28 sem resolver) vai como codigo proprio:
    // o resolvedor da TV demora minutos depois do boot sem a casa estar fora.
    avisoSaude(http > 0 && r != 42 && r != 23 ? 0
               : r == 28 && strstr(redeErroBuf, "Resolving timed out") ? 1028 : r, url, primeiraMs);
  }
  if (avisoHost) {
    long http = 0;
    double seg = 0;
    if (curl_getinfo) { curl_getinfo(c, INFO_RESPONSE_CODE, &http); curl_getinfo(c, INFO_TEMPO_TOTAL, &seg); }
    avisoHost(url, r, (int)http, seg > 0 ? (unsigned)(seg * 1000.0 + 0.5) : 0u);
  }
  // #266/#332/#382: tempos dos primeiros pedidos e de toda falha — DNS, TCP e TLS
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
  if (!curl_reset || r != 0) { descartarHandle(c); return; }
  { unsigned long agora = redeAgoraMs();
    char *fim = NULL;
    hostUsado(url, agora);
    if (curl_getinfo && !curl_getinfo(c, INFO_URL_FINAL, &fim) && fim) hostUsado(fim, agora); }
  /* Keep connections, DNS and TLS sessions, but detach request-owned buffers
   * and callbacks before their stack frames expire. Cleanup at thread exit
   * or parked-handle eviction can invoke progress/header callbacks. */
  curl_reset(c);
}

static void soltarHandleR(void *c, int r, const char *url) {
  soltarHandleDesde(c, r, url, 0);
}

// OPCOES DE TODO PEDIDO, com o prazo em ms.
//
// CONNECTTIMEOUT: DNS + TCP + TLS numa conexao nova levam 0,14-0,7 s na C9
// (curl da TV, mzstatic). O padrao da libcurl e esperar ate o prazo inteiro, e
// o c-ares antigo pode reenviar uma consulta perdida so depois de ~5 s.
// Metade do prazo, entre 2 e 10 s: pedidos de 30 s permitem essa retransmissao
// e ainda deixam tempo para o retry, sem aumentar o prazo total. O backend da
// biblioteca carregada vai ao log no arranque; curl -V nao prova qual ela usa.
// KEEPALIVE: sonda a conexao parada no cache a cada 15 s, o que mantem viva a
// entrada do NAT e faz a morta dar erro no socket (a libcurl a descarta sem
// tentar mandar nada por ela).
static unsigned long conexaoMs(unsigned long prazoMs) {
  unsigned long m = prazoMs / 2;
  if (m < 2000) m = 2000;
  if (m > 10000) m = 10000;
  return m < prazoMs ? m : prazoMs;
}
// 1 quando algum cabecalho do pedido carrega credencial do dono. Usado para
// verificar o TLS e nao seguir redirect (ver rede_baixar_interno3).
static int carregaCredencial(const char *const *cab) {
  static const char *const nomes[] = { "authorization:", "apikey:", "x-api-key:", "trakt-api-key:",
    "x-emby-token:", "x-plex-token:", "x-mediabrowser-token:", "cookie:", NULL };
  int k, i;
  for (k = 0; cab && cab[k]; k++)
    for (i = 0; nomes[i]; i++)
      if (!strncasecmp(cab[k], nomes[i], strlen(nomes[i]))) return 1;
  return 0;
}
// 1 quando a QUERY da URL leva credencial do dono: ?token= no requestdl do
// TorBox, ?api_key= do TMDB/fanart, ?apikey= do mdblist, ?password= dos
// paineis Xtream, ?key= e nomes de token/segredo de outras APIs. O nome do
// parametro casa INTEIRO (depois de ? ou &) e a comparacao ignora maiuscula
// — "monkey=1" ou "tokenize=1" nao contam. Ficam de fora de proposito os
// parametros de assinatura de conteudo (sig, signature, expires): protegem o
// recurso, nao a conta, e alargar o freio ao GET publico quebra midia de
// addon que redireciona.
static int credencialNaQuery(const char *url) {
  static const char *const nomes[] = { "token", "apikey", "api_key", "api-key",
    "key", "access_token", "auth_token", "refresh_token", "id_token",
    "client_secret", "secret", "password", "passwd", "auth", NULL };
  const char *q = url ? strchr(url, '?') : NULL;
  const char *frag = url ? strchr(url, '#') : NULL;
  // O `?` tem de ser da QUERY DE VERDADE: um `#` antes dele o deixa dentro do
  // fragmento, e o fragmento nao sai no pedido — ?token= ali nao e credencial
  // (media de addon com #help?token= perdia o redirect e o TLS aberto).
  if (!q || (frag && frag < q)) return 0;
  for (q++; *q && *q != '#';) {
    const char *e, *f = q;
    size_t n;
    int i;
    while (*f && *f != '&' && *f != '#') f++;
    for (e = q; e < f && *e != '='; e++) {}
    n = (size_t)(e - q);
    for (i = 0; nomes[i]; i++)
      if (strlen(nomes[i]) == n && !strncasecmp(q, nomes[i], n)) return 1;
    if (*f != '&') break;
    q = f + 1;
  }
  return 0;
}
// Pedido sensivel: credencial no cabecalho OU na query — nos dois casos o TLS
// e verificado e o redirect fica desligado (ver rede_baixar_interno3).
static int pedidoComCredencial(const char *url, const char *const *cab) {
  return carregaCredencial(cab) || credencialNaQuery(url);
}
// Prefixo oficial de anexos de release — o MESMO de AT_DL_PREFIXO em
// atualizacao.c. O GitHub responde 302 do releases/download ao CDN de assets,
// entao no caminho binario verificado (rede_baixar_bin_seguro) o redirect so
// vale para URL daqui.
static int urlAnexoOficial(const char *url) {
  static const char prefixo[] =
    "https://github.com/iqui27/nuvio-native-legacy/releases/download/";
  return !strncmp(url, prefixo, sizeof prefixo - 1);
}
// DESTINO permitido de um redirect do download verificado. O prefixo so
// controla a URL INICIAL; sem esta lista um 302 levava o artefato para onde o
// transporte quisesse. So https:// e so os hosts que o GitHub usa para servir
// os anexos: o proprio github.com (o releases/download que responde o 302) e
// os CDNs de assets. Host EXATO com porta opcional — comparacao de prefixo
// deixaria passar github.com.evil.test; URL relativa ou com credencial
// (user@host) nao casa e o salto e recusado.
static int destinoAnexoOk(const char *url) {
  static const char *const hosts[] = {
    "github.com",
    "release-assets.githubusercontent.com",
    "objects.githubusercontent.com", NULL };
  const char *h, *f, *r, *p;
  int i;
  if (!url || strncasecmp(url, "https://", 8)) return 0;
  // Espaco, CR/LF, tab e controles nao cabem num salto: a linha do cabecalho
  // os traz crus (caminho com espaco) ou a reserva os resolve de um valor
  // dobrado — e o transporte seguiria mesmo assim, com o caminho saindo
  // codificado no segundo GET ou o destino costurado fora do lido. A recusa
  // e ANTES de seguir e vale nos dois caminhos verificados (receberCab e
  // viaAndroidSeguro), mesma politica dos dois lados.
  for (p = url; *p; p++)
    if (*(const unsigned char *)p <= 0x20 || *(const unsigned char *)p == 0x7f)
      return 0;
  h = url + 8;
  f = h + strcspn(h, "/?#");
  // userinfo nao entra: https://oficial@evil tem autoridade valida para a
  // libcurl (host = evil) mesmo que o prefixo case num host oficial.
  if (memchr(h, '@', (size_t)(f - h))) return 0;
  for (i = 0; hosts[i]; i++) {
    size_t n = strlen(hosts[i]);
    if (strncasecmp(h, hosts[i], n)) continue;
    r = h + n;
    if (r == f) return 1;
    if (*r == ':' && r + 1 < f && r + 1 + strspn(r + 1, "0123456789") == f) return 1;
  }
  return 0;
}
static void opcoesComuns(void *c, unsigned long prazoMs) {
  redePrazoAtualMs = prazoMs;
#ifdef NV_TPK
  if (dnsShare) curl_setopt(c, 10100 /* CURLOPT_SHARE */, dnsShare);
#endif
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

// Keep only the curl code: raw error buffers can repeat secret request data.
static void anotarErro(int r) {
  if (!r) { redeErroTxt[0] = 0; return; }
  snprintf(redeErroTxt, sizeof redeErroTxt, "curl %d", r);
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
  if (redeParou()) { redeCancelouLocal = 1; return 1; }
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

// CANCELAMENTO SEM FIO EXTRA (#775). O XFERINFO so acorda ~1x/s num pedido
// parado, entao um cancelamento pelo callback de geracao (p2pPararCb,
// rede_parar_trocar) esperava o tique inteiro. Quando ha parar instalado e a
// libcurl tem a interface multi, o pedido vai por um multi proprio no MESMO
// fio: um laço curl_multi_perform + poll de 20 ms recheca o criterio a cada
// volta e sai com curl_multi_remove_handle — sem tocar em socket alheio, sem
// shutdown() e sem prazo extra: um /settings que pensa 1,5 s responde normal
// e o cancelamento volta em dezenas de ms. O fim checa redeParou() de novo no
// fio do pedido para marcar redeCancelouLocal. Sem multi (libcurl antiga) o
// pedido fica com o XFERINFO de sempre.
static int cancelVigiaLigar(void *c, CancelVigia *v) {
  v->multi = NULL;
  v->ligado = 0;
  if (!curl_multi_init || !curl_multi_add || !curl_multi_remove ||
      !curl_multi_perform || !curl_multi_cleanup || !curl_multi_info_read ||
      !(curl_multi_poll || curl_multi_wait)) return 0;
  // O pedido SEMPRE vai pelo multi do fio quando a interface existe: o cache
  // de conexoes mora la, e e o que faz um GET sem cancelamento (/warm) abrir
  // a conexao que o proximo pedido cancelavel (/settings) reusa — mesma porta,
  // conexoes_novas=0. Sem parar instalado o laco so nao tem o que disparar.
  v->multi = redeMultiPegar();      // multi do fio, com o cache de conexoes
  if (!v->multi) v->multi = curl_multi_init();
  if (!v->multi) return 0;
  /* O cache de conexoes fica no MULTI enquanto a easy esta associada. Sem
     CURLMOPT_MAXCONNECTS ele guardaria so 4 — metade do orcamento do fio. */
  if (curl_multi_setopt)
    curl_multi_setopt(v->multi, 6 /* CURLMOPT_MAXCONNECTS */, REDE_CONEXOES_POR_FIO);
  if (curl_multi_add(v->multi, c)) { curl_multi_cleanup(v->multi); v->multi = NULL; return 0; }
  v->ligado = 1;
  return 1;
}
// ADOTA o multi do pedido no multi persistente do fio e solta a easy: o cache
// de conexoes (DNS + TCP + TLS ociosos) vive no multi, entao guardar o multi e
// o que preserva o reuso entre pedidos cancelaveis — com um multi temporario
// por pedido, como era antes, cada GET cancelavel abria conexao nova.
// Conexao que continua viva passa a valer no proximo pedido do fio; se a easy
// for descartada (falha de transporte), as conexoes dela sao fechadas com o
// multi, como o descarte do handle ja fazia. O multi adotado morre quando o
// handle do fio morre (handleSoltar), sem CURLSHARE CONNECT (libcurl das TVs
// < 7.57).
static void cancelVigiaSoltar(CancelVigia *v, int ligado, void *c) {
  if (!ligado || !v->multi) return;
  if (v->ligado) { curl_multi_remove(v->multi, c); v->ligado = 0; }
  // Sem handle no fio (curl_easy_reset ausente ou calloc/setspecific falho) o
  // multi nao tem onde morar: cleanup aqui, como no multi temporario de antes,
  // em vez de perder o ponteiro e vazar multi + conexoes.
  if (!redeMultiGuardar(v->multi)) curl_multi_cleanup(v->multi);
  v->multi = NULL;
}
// Substitui curl_easy_perform quando a vigia esta ligada: a easy ja foi
// adicionada ao multi em cancelVigiaLigar. Devolve o CURLE da transferencia.
static int cancelVigiaPerform(CancelVigia *v, void *c) {
  int ainda = 1, r = 23 /* CURLE_WRITE_ERROR */, achou = 0;
  /* Chamadores sem vigia (sonda e POST sem parar instalado, retry do proprio
     tmdbFallback com cv == NULL) caem no curl_easy_perform de sempre — v ou
     v->multi NULL nunca e desreferenciado. */
  if (!v || !v->multi) return curl_perform(c);
  /* O retry do alias TMDB vem depois do remove do fim da 1a perform: reassocia
     a easy ao MESMO multi para ela continuar cancelavel (e no cache dele). */
  if (!v->ligado) {
    if (curl_multi_add(v->multi, c)) return curl_perform(c);
    v->ligado = 1;
  }
  while (curl_multi_perform(v->multi, &ainda) == 0 /* CURLM_OK */) {
    if (redeParou()) {
      redeCancelouLocal = 1;
      curl_multi_remove(v->multi, c);
      v->ligado = 0;
      return 42 /* CURLE_ABORTED_BY_CALLBACK */;
    }
    if (!ainda) break;
    /* poll existe na 7.66+; wait cobre as libcurl das TVs (7.45+/7.53). */
    if (curl_multi_poll) curl_multi_poll(v->multi, NULL, 0, 20, NULL);
    else curl_multi_wait(v->multi, NULL, 0, 20, NULL);
  }
  { /* CURLMsg: { CURLMSG msg; CURL *easy_handle; union { ... CURLcode result } data } */
    typedef struct { int msg; void *easy; union { void *p; int result; } data; } NvCurlMsg;
    NvCurlMsg *info;
    int naFila;
    while ((info = (NvCurlMsg *)curl_multi_info_read(v->multi, &naFila)) != NULL)
      if (info->msg == 1 /* CURLMSG_DONE */ && info->easy == c)
        { r = info->data.result; achou = 1; } }
  curl_multi_remove(v->multi, c);
  v->ligado = 0;
  /* Sem mensagem DONE (a multi falhou ou a easy sumiu sem resultado) a
     transferencia nao completou: reporta erro de recepcao como o easy faria,
     nunca um CURLE_WRITE_ERROR espurio. */
  return achou ? r : 56 /* CURLE_RECV_ERROR */;
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
                                  char *etag, unsigned tamEtag, int *retry, int bin);

// Onde o ETag da resposta e anotado, quando alguem o pediu. `confereDestino`
// liga o filtro de Location do download verificado (rede_baixar_interno3).
typedef struct { char *dst; unsigned tam; int *retry; int confereDestino; } CacaCab;

// A libcurl entrega UMA linha de cabecalho por chamada, com o CRLF no fim. So
// o ETag interessa; devolver menos bytes do que recebeu abortaria a
// transferencia, entao o retorno e sempre o tamanho inteiro — EXCETO o
// Location recusado do download verificado, que e justamente o aborto.
static size_t receberCab(void *dados, size_t tam, size_t qtd, void *u) {
  CacaCab *c = (CacaCab *)u;
  const char *s = (const char *)dados;
  size_t bytes = tam * qtd, n;
  if (bytes >= 5 && !strncasecmp(s, "HTTP/", 5)) {
    if (c && c->retry) *c->retry = 0;
    if (c && c->dst && c->tam) c->dst[0] = 0;
  }
  if (c && c->confereDestino) {
    // CONTINUACAO DOBRADA (obs-fold): uma linha que COMECA com espaco ou tab
    // estende o cabecalho anterior — e nao ha como provar qual era. Dobrado,
    // um Location mau cabe em duas linhas: a 1a com a parte que passa no
    // filtro e a continuacao com o resto, que a libcurl costura e segue.
    // No caminho verificado QUALQUER dobra recusa a resposta (GitHub e os
    // CDNs de anexo nao dobram; a RFC 7230 proibe obs-fold novo).
    if (bytes && (*s == ' ' || *s == '\t')) return 0;
    if (bytes > 9 && !strncasecmp(s, "location:", 9)) {
      // Cada salto do download verificado e conferido ANTES de ser seguido:
      // so https:// e so host oficial (destinoAnexoOk). Fora da lista a linha
      // volta curta, a libcurl aborta com write-error e o 302 nao e seguido —
      // e o que impede a cadeia de levar o artefato a http:// ou a outro CDN.
      // A conferencia vale so para a linha INTEIRA (mesmo limite do alvo da
      // reserva segura): um Location maior que a copia nao pode ser validado
      // por pedaco — o corte pode ler "host oficial + porta decimal" de uma
      // autoridade que continua (userinfo, outro host) e a libcurl segue a
      // linha completa. Nao coube, o salto e recusado.
      char loc[2048];
      s += 9; bytes -= 9;
      while (bytes && (*s == ' ' || *s == '\t')) { s++; bytes--; }
      while (bytes && (s[bytes - 1] == '\r' || s[bytes - 1] == '\n' ||
                       s[bytes - 1] == ' ')) bytes--;
      n = bytes < sizeof loc ? bytes : sizeof loc - 1;
      memcpy(loc, s, n); loc[n] = 0;
      if (n < bytes || !destinoAnexoOk(loc)) {
        char seg[120];
        printf("[rede] redirect recusado: %s\n", rede_url_publica(loc, seg, sizeof seg));
        fflush(stdout);
        return 0;
      }
    }
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
  if (redeParou()) {
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

// ------------------------------------------------- saida do processo (#F110)
//
// O processo pode terminar com fios SOLTOS dentro da libcurl/OpenSSL: o fio
// de episodios e detachado por desenho (fechar a tela nao espera a busca), e
// os fios de arte e noticia do arranque valem o mesmo. O que acontecia na
// saida — destrinchado pelo TSan e pelos crashs de tests/detail_assistido,
// ~3/4 das corridas (139/134):
//
//   1. o primeiro OPENSSL_init_ssl da libcurl registra OPENSSL_cleanup via
//      atexit; no exit() do fio principal ele da free() no estado global da
//      libcrypto;
//   2. o fio solto seguia no CRYPTO_zalloc do MESMO heap — free() e zalloc()
//      concorrentes corrompiam os freelists ("corrupted size vs. prev_size",
//      "double free or corruption") ou o malloc do fio pisava num no ja
//      liberado (SIGSEGV dentro da libcrypto). O relatorio do TSan e literal:
//      OPENSSL_cleanup (fio principal) x CRYPTO_zalloc (fio de episodios)
//      no mesmo endereco.
//
// O remedio e tirar o desmanche do caminho do fio — nao ha ponto onde todos
// os fios de rede possam ser juntados antes do return do main (o app e os
// testes saem direto) e a lista deles muda a cada versao. O conserto e
// OPENSSL_init_crypto(OPENSSL_INIT_NO_ATEXIT) ANTES do curl_global_init:
// a chamada consome o RUN_ONCE "register_atexit" com a variante que NAO
// registra — a init seguinte da libcurl acha a vez gasta e o
// OPENSSL_cleanup nunca entra na fila do exit(). E o mecanismo oficial
// para quem nao pode garantir fio fora do OpenSSL na saida. No OpenSSL
// 1.0 o simbolo nem existe — e ele tambem nao se desmonta sozinho.
//
// (A versao anterior tambem abria a libcurl com RTLD_NODELETE para "tirar
// as libs do _dl_fini". A revisao provou o ramo inocuo — o destrutor de
// uma .so NODELETE roda no exit() do mesmo jeito, e a glibc nao desmapeia
// nada fora do dlclose, que nunca chamamos — e o mutante sem ele seguia
// 0/25. Saiu: o conserto e so o NO_ATEXIT acima.)
//
// Nada e liberado, de proposito: o kernel recolhe tudo no _exit. O que nao
// pode acontecer e teardown disputando heap e codigo com fio vivo.

// Chamada com abrirTrava tomada, depois do dlopen da libcurl e ANTES do
// curl_global_init — e esse init quem registra o OPENSSL_cleanup.
//
// O OPENSSL_init_crypto e procurado na libcrypto que a libcurl usa de fato,
// nesta ordem: 1) no handle da propria libcurl — dlsym num handle de dlopen
// busca nela e nas dependencias dela, o que acerta a libcrypto certa quando
// DUAS estao residentes, cobre OpenSSL estatico exportado por ela e soname
// fora da lista (ex.: libcrypto.3.dylib do Homebrew); 2) em RTLD_DEFAULT —
// uma libcrypto do escopo global fora da arvore dela; 3) na lista de sonames
// JA residentes (RTLD_NOLOAD, para nao carregar uma .so do nada so para
// marcar; sem a flag o dlopen tenta pelo nome mesmo, como antes). Sem
// simbolo — libcurl sem OpenSSL dinamico (mbedTLS, GnuTLS, SecureTransport)
// ou OpenSSL 1.0 — retorna sem tocar em nada.
static void segurarSaidaRede(void *curl) {
  static const char *const crypto[] = {
    "libcrypto.so.3", "libcrypto.so.1.1", "libcrypto.so.1.0.2",
    "libcrypto.so.1.0.0", "libcrypto.so.10", "libcrypto.so.1",
    "libcrypto.so", "libcrypto.dylib", NULL };
  int (*initCrypto)(unsigned long long, const void *) = NULL;
  int i;
  *(void **)(&initCrypto) = dlsym(curl, "OPENSSL_init_crypto");
#ifdef RTLD_DEFAULT
  if (!initCrypto)
    *(void **)(&initCrypto) = dlsym(RTLD_DEFAULT, "OPENSSL_init_crypto");
#endif
  for (i = 0; crypto[i] && !initCrypto; i++) {
    void *hc;
#ifdef RTLD_NOLOAD
    hc = dlopen(crypto[i], RTLD_NOLOAD | RTLD_NOW);
#else
    hc = dlopen(crypto[i], RTLD_NOW);
#endif
    if (hc) *(void **)(&initCrypto) = dlsym(hc, "OPENSSL_init_crypto");
  }
  // 0x80000 = OPENSSL_INIT_NO_ATEXIT (openssl/crypto.h). Sem headers como o
  // resto do arquivo: a constante e ABI e nao muda.
  if (initCrypto) initCrypto(0x80000ULL, NULL);
}

#ifdef NV_HTTP_HOST
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
#ifdef _WIN32
  if (!h) h = dlopen("libcurl-x64.dll", RTLD_NOW);
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
  *(void **)(&curl_multi_init)     = dlsym(h, "curl_multi_init");
  *(void **)(&curl_multi_add)      = dlsym(h, "curl_multi_add_handle");
  *(void **)(&curl_multi_remove)   = dlsym(h, "curl_multi_remove_handle");
  *(void **)(&curl_multi_perform)  = dlsym(h, "curl_multi_perform");
  *(void **)(&curl_multi_poll)     = dlsym(h, "curl_multi_poll");
  *(void **)(&curl_multi_wait)     = dlsym(h, "curl_multi_wait");
  *(void **)(&curl_multi_cleanup)  = dlsym(h, "curl_multi_cleanup");
  *(void **)(&curl_multi_setopt)   = dlsym(h, "curl_multi_setopt");
  *(void **)(&curl_multi_info_read)= dlsym(h, "curl_multi_info_read");
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
  segurarSaidaRede(h);   // antes do curl_global_init — ele registra o OPENSSL_cleanup
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
  // Prefixo ABI de curl_version_info_data ate CURLVERSION_SECOND (7.11.1).
  // Sem curl.h, como as opcoes acima: a biblioteca carregada vem do aparelho.
  {
    typedef struct {
      int age;
      const char *version;
      unsigned version_num;
      const char *host;
      int features;
      const char *ssl_version;
      long ssl_version_num;
      const char *libz_version;
      const char *const *protocols;
      const char *ares;
      int ares_num;
    } CurlVersao;
    const CurlVersao *(*versao)(int);
    const CurlVersao *v;
    *(void **)(&versao) = dlsym(h, "curl_version_info");
    v = versao ? versao(1 /* CURLVERSION_SECOND */) : NULL;
    if (v) printf("[rede] libcurl=%s tls_backend=%s async_dns=%s%s\n", v->version ? v->version : "?",
                  v->ssl_version ? v->ssl_version : "none",
                  v->age >= 1 && v->ares ? "c-ares/" :
                  (v->features & (1 << 7)) ? "threaded" : "no (synchronous)",
                  v->age >= 1 && v->ares ? v->ares : "");
    else printf("[rede] curl_version_info indisponivel\n");
#ifdef NV_TPK
    prepararDns(h, v && v->age >= 1 ? v->ares : NULL);
#endif
    fflush(stdout);
  }
  // DEPOIS do global_init e ANTES de soltar a trava: a partir daqui qualquer fio
  // pode entrar em curl_easy_perform, e e la que o OpenSSL comeca a ser usado.
#ifndef NV_ANDROID   // libcurl do APK nao usa libcrypto 1.0 (e dlopen de libcrypto.so seria do sistema)
  prepararOpenSSL();
#endif
  __atomic_store_n(&pronto, 1, __ATOMIC_RELEASE);
  pthread_mutex_unlock(&abrirTrava);
  return 1;
}

#ifdef NV_HTTP_HOST
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
// nunca depende so da nossa libcurl. The session latch requires transport
// failures recovered by Android on three distinct hostnames.
static int curlRuim;
static char *viaAndroid(const char *verbo, const char *url, int segundos,
                        const char *const *cab, const char *corpo, int *status,
                        long *tam, int *retryAfter, const char *porque) {
  char *cabs, *r, erro[200], h[96];
  size_t n = 1;
  int k, st = 0, temCt = 0;
  long bytes = 0;
  unsigned long t0 = redeAgoraMs();
  static int logados;
  redeBytesLocal = 0;
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
                   &st, retryAfter, &bytes, erro, sizeof erro);
  if (!r && !st && !alias && !redeCancelouLocal && (alias = tmdbUrl(url))) {
    r = android_http(verbo, alias, cabs, corpo, (segundos > 0 ? segundos : 20) * 1000,
                     &st, retryAfter, &bytes, erro, sizeof erro);
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
  // Os wrappers medidos leem este contador mesmo sem pedir `tam`.
  redeBytesLocal = bytes;
  if (st == 401 && aviso401) aviso401(url);
  if (status) *status = st;
  else if (st < 200 || st > 299) { free(r); return NULL; }  // como rede_baixar sem status
  if (tam) *tam = bytes;
  // #205: a contagem tambem vai no estado do fio. Quem baixa ARTE le
  // redeBytesLocal (rede_baixar_bin_medido_controle), nao o `tam`, e este e o
  // unico caminho de download que nao passa pelo recebedor da libcurl — sem
  // isto, todo corpo entregue aqui chegava ao tex_cache como "corpo curto
  // (0 B)" e a fileira inteira ficava sem arte, refetch a cada item (TCL).
  redeBytesLocal = bytes;
  return r;
}
// RESERVA SEGURA do download verificado (F94, rede_baixar_bin_seguro). A
// reserva comum nao serve para o artefato de release: o httpPedir liga
// instanceFollowRedirects e um 302 levaria o binario para fora da lista. O
// httpPedirSeguro faz UM salto por chamada — o redirect fica desligado no
// Java e, num 3xx, o "corpo" devolvido e o Location ja resolvido contra a URL
// do salto. Quem decide o proximo passo e aqui, com a MESMA regra do caminho
// libcurl: a URL inicial e https, salto so quando ela esta no prefixo oficial
// de anexos (e sem credencial no pedido), cada destino conferido por
// destinoAnexoOk (so https, so host oficial) e no maximo 5 saltos. O TLS e
// verificado pela pilha do proprio Android em cada salto.
static char *viaAndroidSeguro(const char *url, int segundos, const char *const *cab,
                              int *status, long *tam, int *retryAfter) {
  redeBytesLocal = 0;
  if (status) *status = 0;
  if (retryAfter) *retryAfter = 0;
#ifdef NV_ANDROID
  { char *cabs, *r = NULL, erro[200] = "", h[96], alvo[2048];
    size_t n = 1;
    int k, st = 0, saltos = 0, segue;
    long bytes = 0;
    unsigned long t0 = redeAgoraMs();
    static int logados;
    if (strncmp(url, "https://", 8) || strlen(url) >= sizeof alvo) return NULL;
    // A regra do segredo precede, como no caminho libcurl: com credencial no
    // pedido nenhum salto e seguido (e o prefixo oficial so libera a URL
    // inicial — cada Location continua conferida pela lista).
    segue = urlAnexoOficial(url) && !pedidoComCredencial(url, cab);
    for (k = 0; cab && cab[k]; k++) n += strlen(cab[k]) + 1;
    cabs = (char *)malloc(n);
    if (!cabs) return NULL;
    cabs[0] = 0;
    for (k = 0; cab && cab[k]; k++) { strcat(cabs, cab[k]); strcat(cabs, "\n"); }
    snprintf(alvo, sizeof alvo, "%s", url);
    for (;;) {
      long nb = 0;
      free(r); r = NULL;
      if (redeCancelouLocal || redeParou()) break;
      st = 0;
      r = android_http_seguro("GET", alvo, cabs, NULL,
                              (segundos > 0 ? segundos : 20) * 1000,
                              &st, retryAfter, &nb, erro, sizeof erro);
      if (!r) break;
      bytes = nb;
      if (st < 300 || st >= 400) break;
      // Num 3xx o "corpo" e o Location ABSOLUTO resolvido pelo host — o unico
      // dado de fora que ainda nao passou pela lista. Recusa: salto proibido
      // (fora do prefixo ou com credencial), 3xx sem Location, mais de 5
      // saltos, destino fora da lista ou comprido demais.
      if (!segue || !nb || ++saltos > 5 || !destinoAnexoOk(r) ||
          strlen(r) >= sizeof alvo) {
        char seg[120];
        printf("[rede] reserva segura: salto recusado%s%s\n",
               nb ? " para " : " (3xx sem Location)",
               nb ? rede_url_publica(r, seg, sizeof seg) : "");
        fflush(stdout);
        free(r); r = NULL; st = 0;
        break;
      }
      snprintf(alvo, sizeof alvo, "%s", r);
    }
    free(cabs);
    hostDaUrl(alvo, h, sizeof h);
    if (__atomic_add_fetch(&logados, 1, __ATOMIC_RELAXED) <= 12 || !r) {
      printf("[rede] via Android seguro GET %s: http=%d em %lu ms%s%s\n",
             h[0] ? h : "?", st, redeAgoraMs() - t0, r ? "" : " FALHOU: ", r ? "" : erro);
      fflush(stdout);
    }
    if (!r) { snprintf(redeErroTxt, sizeof redeErroTxt, "android: %.180s", erro); return NULL; }
    redeErroTxt[0] = 0;
    redeBytesLocal = bytes;
    if (st == 401 && aviso401) aviso401(alvo);
    if (status) *status = st;
    else if (st < 200 || st > 299) { free(r); return NULL; }
    if (tam) *tam = bytes;
    return r;
  }
#else
  // Host sem o modo salto-a-salto (iOS usa a mesma trilha): o caminho
  // verificado falha de proposito em vez de cair na reserva comum, que
  // seguiria redirect sem a lista. Nenhum chamador do download verificado
  // existe nessas plataformas hoje.
  { char seg[120];
    printf("[rede] reserva segura indisponivel neste host: %s\n",
           rede_url_publica(url, seg, sizeof seg));
    fflush(stdout); }
  snprintf(redeErroTxt, sizeof redeErroTxt, "curl %d", 5);
  (void)segundos; (void)cab; (void)tam;
  return NULL;
#endif
}
// 1 = este pedido simples vai direto pelo Android (sem tocar na libcurl).
static int androidPrimeiro(void) {
  if (__atomic_load_n(&curlRuim, __ATOMIC_ACQUIRE)) return 1;
  // 8 s, e nao 20: a tela de login nao fica parada esperando a libcurl.
  return !abrirAte(8000);
}
// Falha de TRANSPORTE da libcurl num pedido simples: tenta o Android uma vez.
static char *androidDepois(const char *verbo, const char *url, int segundos,
                           const char *const *cab, const char *corpo, int *status, int *retryAfter) {
  static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
  static char hosts[3][256];
  static unsigned nHosts;
  char *r, origem[320], *host, *fim;
  int codigo = redeCurlLocal;
  unsigned i;
  if (redeCancelouLocal || redeParou()) return NULL;
  r = viaAndroid(verbo, url, segundos, cab, corpo, status, NULL, retryAfter, "curl falhou");
  // Only transport errors count; malformed URLs, unsupported protocols,
  // allocation failures and deliberate aborts say nothing about curl health.
  if (!r || !(codigo == 5 || codigo == 6 || codigo == 7 || codigo == 28 ||
              codigo == 35 || codigo == 52 || codigo == 55 || codigo == 56)) return r;
  hostDaUrl(url, origem, sizeof origem);
  host = strstr(origem, "://");
  if (!host) return r;
  host += 3;
  fim = *host == '[' ? strchr(host, ']') : strchr(host, ':');
  if (fim) { if (*fim == ']') fim++; *fim = 0; }
  if (!*host || strlen(host) >= sizeof hosts[0]) return r;
  pthread_mutex_lock(&trava);
  for (i = 0; i < nHosts; i++) if (!strcasecmp(hosts[i], host)) break;
  if (i == nHosts && nHosts < 3) {
    snprintf(hosts[nHosts++], sizeof hosts[0], "%s", host);
    if (nHosts == 3 && !__atomic_exchange_n(&curlRuim, 1, __ATOMIC_ACQ_REL)) {
      printf("[rede] libcurl failed on three distinct hosts recovered by Android; simple requests use Android this session\n");
      fflush(stdout);
    }
  }
  pthread_mutex_unlock(&trava);
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
char *rede_baixar_bin_seguro(const char *url, int segundos, long *tam) {
  // bin=1: download verificado so para artefato de release (.so do .tpk,
  // .apk) — ver o bloco `bin` em rede_baixar_interno3. Legenda, cartaz e
  // fonte seguem no caminho publico de rede_baixar_bin.
  return rede_baixar_interno3(url, segundos, tam, NULL, NULL, NULL, 0, NULL, 1);
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
#ifdef NV_HTTP_HOST
  char *r;
  int st = 0;
  if (!url || !*url) { if (status) *status = 0; return NULL; }
  if (androidPrimeiro()) {   // st local: com status NULL o 4xx tambem volta com corpo, como no curl
    r = viaAndroid("GET", url, segundos, cab, NULL, &st, NULL, NULL, "libcurl fora");
    if (status) *status = st;
    return r;
  }
  redeCurlLocal = 0;
  r = rede_baixar_interno2(url, segundos, NULL, cab, &st, NULL, 0);
  if (status) *status = st;
  if (!r && st == 0) {
    r = androidDepois("GET", url, segundos, cab, NULL, &st, NULL);
    if (status) *status = st;
  }
  return r;
#else
  return rede_baixar_interno2(url, segundos, NULL, cab, status, NULL, 0);
#endif
}
char *rede_baixar_st_retry(const char *url, int segundos, const char *const *cab,
                           int *status, int *retryAfter) {
  return rede_baixar_interno3(url, segundos, NULL, cab, status, NULL, 0, retryAfter, 0);
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
  if (redeCancelouLocal || redeParou()) return 0;
  if (redeLateral && redeParcialOk && (r == 6 || r == 7 || r == 35)) return 0;
  if (r == 42) return v->parou;            // vigia: corpo parado
  if (bytes > 0) return 0;
  return r == 7 || r == 28 || r == 35 || r == 52 || r == 55 || r == 56;
}

static char *rede_baixar_interno2(const char *url, int segundos, long *tam,
                                  const char *const *cab, int *status,
                                  char *etag, unsigned tamEtag) {
  return rede_baixar_interno3(url, segundos, tam, cab, status, etag, tamEtag, NULL, 0);
}
static char *rede_baixar_interno3(const char *url, int segundos, long *tam,
                                  const char *const *cab, int *status,
                                  char *etag, unsigned tamEtag, int *retry, int bin) {
  Balde b = {0};
  CacaCab caca;
  Vigia vigia;
  CancelVigia cv;
  void *c, *lista = NULL;
  int r = 0, tentativa, reusada = 0, cvOn = 0;
  unsigned long inicio, prazoMs, gasto = 0;
  unsigned primeiraMs = 0;
  caca.dst = (etag && tamEtag > 1) ? etag : NULL;
  caca.tam = tamEtag;
  caca.retry = retry;
  if (retry) *retry = 0;
  if (etag && tamEtag) etag[0] = 0;
  if (status) *status = 0;
  if (!url || !*url) return NULL;
  // Marca do pedido CANCELADO anterior deste fio: e do pedido que parou, nao
  // deste — sem limpar aqui o corpo deste e descartado na saida (#775).
  redeCancelouLocal = 0;
  // Endereco de API de metadados que ja respondeu "nao existe" (ou host em
  // recuo por timeout): nao pede de novo — ver negcache.h.
  if (!status && negcache_barra(url, (long)time(NULL))) return NULL;
#ifdef NV_HTTP_HOST
  // libcurl nunca ficou pronta: GET simples (catalogo, imagem) pelo Android.
  if (!abrir()) {
    // O DOWNLOAD VERIFICADO nao pode cair na reserva comum: o httpPedir segue o
    // 302 sozinho e sairia da lista oficial. Salto a salto com a mesma regra
    // do receberCab — ver viaAndroidSeguro.
    if (bin) return viaAndroidSeguro(url, segundos, cab, status, tam, retry);
    return viaAndroid("GET", url, segundos, cab, NULL, status, tam, retry, "libcurl fora");
  }
#else
  if (!abrir()) return NULL;
#endif
  inicio = redeAgoraMs();
  prazoMs = (unsigned long)(segundos > 0 ? segundos : 30) * 1000UL;
  /* Provider lists can stream for minutes. Keep the existing body-stall
   * watchdog and connect deadline, with a larger bounded list budget. */
  if (strstr(url, "/player_api.php?") &&
      (strstr(url, "action=get_vod_") || strstr(url, "action=get_series") ||
       strstr(url, "action=get_live_")) && prazoMs < 120000UL)
    prazoMs = 120000UL;
  for (tentativa = 0; ; tentativa++) {
    unsigned long resta = prazoMs - gasto;
    c = pegarHandle(url);
    if (!c) return NULL;
    curl_setopt(c, OPT_URL, url);
    // SO QUANDO ALGUEM PEDIU. O handle e reusado por fio (pegarHandle) e
    // curl_easy_reset limpa as opcoes entre pedidos, entao deixar o recebedor
    // instalado aqui nao respinga no pedido seguinte do mesmo fio — mas tambem
    // nao ha por que pagar uma chamada por cabecalho em todo download de imagem.
    // A EXCECAO e o download verificado do anexo oficial: a resposta pode
    // trazer um Location para o CDN e cada salto precisa ser conferido
    // (ver receberCab). Pedido com credencial fica de fora: o redirect dele
    // esta desligado no ramo abaixo, entao nao ha salto a conferir.
    caca.confereDestino = bin && !strncmp(url, "https://", 8) &&
                          !pedidoComCredencial(url, cab) && urlAnexoOficial(url);
    if (caca.dst || caca.retry || caca.confereDestino) {
      curl_setopt(c, OPT_HEADERFUNCTION, receberCab);
      curl_setopt(c, OPT_HEADERDATA, &caca);
    }
    curl_setopt(c, OPT_WRITEFUNCTION, receber);
    curl_setopt(c, OPT_WRITEDATA, &b);
    curl_setopt(c, OPT_FOLLOWLOCATION, (long)1);
    opcoesComuns(c, resta);
    // Pedido que leva credencial (Authorization, apikey, token) NO CABECALHO
    // OU NA QUERY: TLS verificado e nenhum redirect — um 3xx reenviaria os
    // cabecalhos do dono (apikey, trakt-api-key...) a outro host, e sem
    // verificar qualquer um no caminho le a senha/token, em cabecalho ou na
    // URL (?token= do requestdl do TorBox, ?api_key= do TMDB, ?password= dos
    // paineis Xtream). As APIs (Supabase, Trakt, debrid) nao redirecionam.
    if (pedidoComCredencial(url, cab) && !strncmp(url, "https://", 8)) {
      curl_setopt(c, OPT_SSL_VERIFYPEER, (long)1);
      curl_setopt(c, OPT_SSL_VERIFYHOST, (long)2);
      if (discordCa[0]) curl_setopt(c, OPT_CAINFO, discordCa);
      curl_setopt(c, OPT_FOLLOWLOCATION, (long)0);
    } else if (bin && !strncmp(url, "https://", 8)) {
      // DOWNLOAD BINARIO VERIFICADO (rede_baixar_bin_seguro): e o caminho dos
      // artefatos da release (.so do .tpk, .apk), que viram codigo nativo —
      // certificado e hostname conferidos pela CA do pacote, como no pedido
      // com credencial. O redirect fica ligado SO no prefixo oficial de
      // anexos: o GitHub responde 302 do releases/download ao CDN de assets,
      // e sem credencial no pedido nada vaza ao host seguinte. Fora do
      // prefixo um 3xx volta como esta, sem seguir.
      curl_setopt(c, OPT_SSL_VERIFYPEER, (long)1);
      curl_setopt(c, OPT_SSL_VERIFYHOST, (long)2);
      if (discordCa[0]) curl_setopt(c, OPT_CAINFO, discordCa);
      if (!urlAnexoOficial(url)) curl_setopt(c, OPT_FOLLOWLOCATION, (long)0);
      else {
        // O prefixo so libera a URL INICIAL; a cadeia tem regra propria:
        // saltos so em https (bitmask — a *_STR da 7.85 nao existe nas
        // libcurl das TVs), teto de saltos, e cada Location conferida pelo
        // receberCab contra a lista oficial de hosts (destinoAnexoOk).
        curl_setopt(c, OPT_REDIR_PROTOCOLS, (long)2 /* CURLPROTO_HTTPS */);
        curl_setopt(c, OPT_MAXREDIRS, (long)5);
      }
    }
    ligarVigia(c, &vigia, prazoMs);
    cvOn = cancelVigiaLigar(c, &cv);
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
        r = performV4(c, url, &cv);
        { long http = 0; if (curl_getinfo) curl_getinfo(c, INFO_RESPONSE_CODE, &http);
          snprintf(m, sizeof m, "ok https-1 curl=%d http=%ld", r, http); }
        nv_tpk40_etapa(m);
      } else r = performV4(c, url, &cv); }
#else
    r = performV4(c, url, &cv);
#endif
    { long novas = -1;
      if (curl_getinfo) curl_getinfo(c, INFO_NUM_CONNECTS, &novas);
      reusada = novas == 0; }
    gasto = redeAgoraMs() - inicio;
    if (tentativa == 0 && r != 0 && valeRepetir(r, &vigia, b.n) &&
        (gasto + 1000 < prazoMs || (reusada && b.n == 0))) {
      char seg[120];
      printf("[rede] curl %d em %s (conexao %s, %ld bytes, %lu ms): de novo em conexao nova\n",
             r, rede_url_publica(url, seg, sizeof seg), reusada ? "reusada" : "nova",
             (long)b.n, gasto);
      fflush(stdout);
      cancelVigiaSoltar(&cv, cvOn, c);
      if (lista) { curl_setopt(c, OPT_HTTPHEADER, (void *)0); if (slist_free) slist_free(lista); lista = NULL; }
      // #382: conta so o resultado final, mas preserva a janela da primeira falha.
      primeiraMs = (unsigned)(inicio + gasto);
      descartarHandle(c);
      /* A dead reused socket can consume the entire first budget. Give the
       * single fresh attempt its own deadline; cancellation still applies. */
      if (gasto + 1000 >= prazoMs) { inicio = redeAgoraMs(); gasto = 0; }
      free(b.p);
      b.p = NULL; b.n = b.cap = 0;
      if (etag && tamEtag) etag[0] = 0;
      continue;
    }
    /* O retry no alias TMDB tem de seguir cancelavel: roda enquanto a vigia
       ainda esta armada, com o MESMO multi (cancelVigiaPerform reassocia a
       easy que a 1a perform removeu). cv == NULL cai no easy perform. */
    r = tmdbFallback(c, r == 42 && vigia.parou ? 28 : r, url, cvOn ? &cv : NULL);
    cancelVigiaSoltar(&cv, cvOn, c);
    break;
  }
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
      soltarHandleDesde(c, r, url, primeiraMs);
      free(b.p);
      { char seg[120];
        printf("[rede] HTTP %ld em %s\n", http, rede_url_publica(url, seg, sizeof seg)); }
      fflush(stdout);
      return NULL;
    } }
  if (lista) { curl_setopt(c, OPT_HTTPHEADER, (void *)0); if (slist_free) slist_free(lista); }
  soltarHandleDesde(c, r, url, primeiraMs);
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
    printf("[rede] falha %d em %s (conexao %s, %ld bytes, %lu ms%s)\n", r,
           rede_url_publica(url, seg, sizeof seg), reusada ? "reusada" : "nova",
           (long)b.n, gasto, tentativa ? ", 2a tentativa" : "");
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
  CancelVigia cv;
  void *c, *lista = NULL;
  char *fim = NULL;
  long http = 0;
  unsigned long prazoMs;
  int r, ok, k, cvOn;
  if (status) *status = 0;
  if (mime && mimeTam) mime[0] = 0;
  if (corpo) *corpo = -1;
  if (dst && tam) dst[0] = 0;
  if (!url || !*url || !dst || !tam || tam > INT_MAX || !abrir() || !curl_getinfo) return 0;
  redeCancelouLocal = 0;
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
  cvOn = cancelVigiaLigar(c, &cv);
  // Um pedaco minusculo em vez de HEAD: varios servidores de debrid respondem
  // HEAD com 405 ou mentem no redirecionamento, mas honram Range.
  curl_setopt(c, OPT_RANGE, "0-63");
  for (k = 0; cab && cab[k]; k++) {
    void *nova = slist_append(lista, cab[k]);
    if (!nova) { if (lista) slist_free(lista);
                 cancelVigiaSoltar(&cv, cvOn, c); soltarHandleR(c, 1, url); return 0; }
    lista = nova;
  }
  if (lista) curl_setopt(c, OPT_HTTPHEADER, lista);
  r = performTmdbCv(c, url, cvOn ? &cv : NULL);
  cancelVigiaSoltar(&cv, cvOn, c);
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
       http >= 200 && http < 300 && fim && *fim;
  if (ok) ok = nv_media_url_guardar(fim, dst, tam);
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
static int vigiarMorno(void *u, long long a, long long b, long long c, long long d) {
  (void)a; (void)b; (void)c; (void)d;
  return rede_job_estado((RedeJob *)u) != REDE_OK;
}
int rede_aquecer_job(const char *const *origens, int n, unsigned *ms, RedeJob *job) {
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
    if (rede_job_estado(job) != REDE_OK) { soltarHandleR(c, 42, u); break; }
    if (job) {
      curl_setopt(c, OPT_XFERINFOFUNCTION, vigiarMorno);
      curl_setopt(c, OPT_XFERINFODATA, job);
      curl_setopt(c, OPT_NOPROGRESS, 0L);
    }
    t0 = redeAgoraMs();
    r = performTmdb(c, u);
    curl_setopt(c, OPT_XFERINFOFUNCTION, (void *)0);
    curl_setopt(c, OPT_XFERINFODATA, (void *)0);
    curl_setopt(c, OPT_NOPROGRESS, 1L);
    if (rede_job_estado(job) != REDE_OK) r = 42;
    if (ms) { unsigned long dt = redeAgoraMs() - t0; ms[i] = r ? 0 : (unsigned)(dt ? dt : 1); }
    soltarHandleR(c, r, u);
    if (!r) {
      abriu++;
      if (nok < REDE_ESTAC_HOSTS) snprintf(ok[nok++], 96, "%s", h);
    }
  }
  // O PAR easy+multi, com as conexoes que abriu, sai deste fio e vai para o
  // estacionamento: o cache das conexoes aquecidas mora no multi (o HEAD de
  // cada origem passou por performTmdb, que o usa), entao estacionar so a
  // easy entregaria um handle SEM conexao ao fio consumidor.
  { HandlePool *p = (HandlePool *)pthread_getspecific(handleChave);
    void *m = NULL;
    c = p ? p->c : NULL;
    if (c && nok > 0) {
      m = p->multi;
      p->c = NULL;
      p->multi = NULL;
    } else c = NULL;
    if (c) {
      hostsEsquecer();
      estacionar(c, m, ok, nok);
    } }
  return abriu;
}
int rede_aquecer_lote(const char *const *origens, int n, unsigned *ms) {
  return rede_aquecer_job(origens, n, ms, NULL);
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
  // DELETE autenticado (Trakt /sync/playback leva Authorization): mesmo
  // tratamento do GET com credencial — TLS verificado e sem redirect, que
  // numa alca reusada do pool pode vir ligado de um GET anterior. A
  // credencial na query vale igual (ex.: ?token=).
  if (pedidoComCredencial(url, cab) && !strncmp(url, "https://", 8)) {
    curl_setopt(c, OPT_SSL_VERIFYPEER, (long)1);
    curl_setopt(c, OPT_SSL_VERIFYHOST, (long)2);
    if (discordCa[0]) curl_setopt(c, OPT_CAINFO, discordCa);
    curl_setopt(c, OPT_FOLLOWLOCATION, (long)0);
  }
  curl_setopt(c, OPT_CUSTOMREQUEST, "DELETE");
  if (slist_append) {
    int k;
    for (k = 0; cab && cab[k]; k++) lista = slist_append(lista, cab[k]);
    if (lista) curl_setopt(c, OPT_HTTPHEADER, lista);
  }
  r = performTmdb(c, url);
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


// GET EM FLUXO (guia de TV, M4/M9): o corpo vai em pedacos para `cb` e nada
// fica na memoria — e o que deixa um XMLTV de centenas de MB ou uma M3U de 20
// mil canais passar numa TV de 1,5 GB. Ver rede.h.
typedef struct {
  int (*cb)(const char *, size_t, void *);
  void *u;
  long long n, max;
  int cortou, limitou;
} Fluxo;
static size_t receberFluxo(void *dados, size_t tam, size_t qtd, void *u) {
  Fluxo *f = u;
  size_t bytes;
  if (tam && qtd > SIZE_MAX / tam) return 0;
  bytes = tam * qtd;
  if (redeCancelLocal && *redeCancelLocal) { redeCancelouLocal = 1; return 0; }
  if (f->max > 0 && f->n + (long long)bytes > f->max) { f->limitou = 1; return 0; }
  f->n += (long long)bytes;
  if (bytes && !f->cb((const char *)dados, bytes, f->u)) { f->cortou = 1; return 0; }
  return bytes;
}
int rede_baixar_fluxo(const char *url, int segundos, const char *const *cab,
                      int (*cb)(const char *dados, size_t n, void *u), void *u,
                      long long maxBytes, int *status, long long *bytes) {
  Fluxo f;
  Vigia vigia;
  CancelVigia cv;
  void *c, *lista = NULL;
  unsigned long prazoMs;
  long http = 0;
  int r, cvOn;
  if (status) *status = 0;
  if (bytes) *bytes = 0;
  if (!url || !*url || !cb || !abrir()) return 0;
  redeCancelouLocal = 0;
  memset(&f, 0, sizeof f);
  f.cb = cb; f.u = u; f.max = maxBytes;
  c = pegarHandle(url);
  if (!c) return 0;
  curl_setopt(c, OPT_URL, url);
  curl_setopt(c, OPT_WRITEFUNCTION, receberFluxo);
  curl_setopt(c, OPT_WRITEDATA, &f);
  curl_setopt(c, OPT_FOLLOWLOCATION, (long)1);
  prazoMs = (unsigned long)(segundos > 0 ? segundos : 60) * 1000UL;
  opcoesComuns(c, prazoMs);
  ligarVigia(c, &vigia, prazoMs);
  cvOn = cancelVigiaLigar(c, &cv);
  curl_setopt(c, OPT_ACCEPT_ENCODING, "");
  if (cab && slist_append) {
    int k;
    for (k = 0; cab[k]; k++) lista = slist_append(lista, cab[k]);
    if (lista) curl_setopt(c, OPT_HTTPHEADER, lista);
  }
  r = performV4(c, url, &cv);
  cancelVigiaSoltar(&cv, cvOn, c);
  if (r == 42 && vigia.parou) r = 28;
  if (curl_getinfo) curl_getinfo(c, INFO_RESPONSE_CODE, &http);
  if (status) *status = (int)http;
  if (bytes) *bytes = f.n;
  redeCurlLocal = r;
  anotarErro(r);
  if (lista) { curl_setopt(c, OPT_HTTPHEADER, (void *)0); if (slist_free) slist_free(lista); }
  soltarHandleR(c, r, url);
  // O cb devolver 0 NAO e sucesso: e corte. Antes isto virava r=0 e, se o HTTP
  // era 200, a funcao devolvia 1 — um ENOSPC/EFBIG no fwrite do consumidor
  // (#764) era publicado como arquivo completo. O consumidor que corta de
  // proposito (amostra, limite) sempre recebeu 0 aqui e continua recebendo.
  if (r != 0 || http < 200 || http >= 300) {
    char seg[120];
    printf("[rede] fluxo: curl %d, HTTP %ld, %lld bytes em %s%s\n", r, http, f.n,
           rede_url_publica(url, seg, sizeof seg), f.limitou ? " (teto)" : "");
    fflush(stdout);
    return 0;
  }
  return 1;
}

// Set once at startup before OAuth/WebSocket workers begin.
void rede_discord_ca(const char *caminho) {
  snprintf(discordCa, sizeof discordCa, "%s", caminho ? caminho : "");
}
#define OPT_CAINFO 10065

static char *postarNativo(const char *url, int segundos, const char *const *cab,
                          const char *corpo, int *status, int seguro, int *retryAfter,
                          const char *verbo) {
  Balde b = {0};
  CacaCab respostaCab = { .retry = retryAfter };
  Vigia vigia;
  CancelVigia cv;
  if (retryAfter) *retryAfter = 0;
  void *c, *lista = NULL;
  int r, cvOn;
  if (status) *status = 0;
  redeCurlLocal = 0;
  redeCancelouLocal = 0;
  anotarErro(0);
  if (!url || !*url) { anotarErro(3); return NULL; }
  if (!abrir()) { anotarErro(2); return NULL; }
  c = seguro ? curl_init() : pegarHandle(url);
  if (!c) { anotarErro(27); return NULL; }
  curl_setopt(c, OPT_URL, url);
  curl_setopt(c, OPT_WRITEFUNCTION, receber);
  curl_setopt(c, OPT_WRITEDATA, &b);
  opcoesComuns(c, (unsigned long)(segundos > 0 ? segundos : 20) * 1000UL);
  if(redeParar)ligarVigia(c,&vigia,(unsigned long)(segundos > 0 ? segundos : 20)*1000UL);
  cvOn = cancelVigiaLigar(c, &cv);
  // POST de API (login, token, sync): o corpo e/ou os cabecalhos levam senha,
  // refresh token ou chave. opcoesComuns desliga a verificacao para a MIDIA
  // publica dos addons; aqui o destino e sempre uma API com certificado valido.
  if (!strncmp(url, "https://", 8)) {
    curl_setopt(c, OPT_SSL_VERIFYPEER, (long)1);
    curl_setopt(c, OPT_SSL_VERIFYHOST, (long)2);
    if (discordCa[0]) curl_setopt(c, OPT_CAINFO, discordCa);
    if (pedidoComCredencial(url, cab))
      curl_setopt(c, OPT_FOLLOWLOCATION, (long)0);
  }
  if (seguro) {
    // OAuth credentials require authenticated TLS and never follow redirects.
    curl_setopt(c, OPT_SSL_VERIFYPEER, (long)1);
    curl_setopt(c, OPT_SSL_VERIFYHOST, (long)2);
    if (discordCa[0]) curl_setopt(c, OPT_CAINFO, discordCa);
    curl_setopt(c, OPT_FOLLOWLOCATION, (long)0);
  }
  if (retryAfter) {
    curl_setopt(c, OPT_HEADERFUNCTION, receberCab);
    curl_setopt(c, OPT_HEADERDATA, &respostaCab);
  }
  curl_setopt(c, OPT_POST, (long)1);
  curl_setopt(c, OPT_POSTFIELDS, corpo ? corpo : "");
  if (verbo) curl_setopt(c, OPT_CUSTOMREQUEST, verbo);
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
  redeSemRecuoV4 = 1;
  r = performTmdbCv(c, url, cvOn ? &cv : NULL);
  redeSemRecuoV4 = 0;
  cancelVigiaSoltar(&cv, cvOn, c);
  redeCurlLocal = r;
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
  return rede_postar_st_retry(url, segundos, cab, corpo, status, NULL);
}

// POST e PUT de JSON: a mesma rota (Android: libcurl primeiro, HttpURLConnection
// se ela nao respondeu). `verbo` NULL = POST.
static char *enviarNativo(const char *verbo, const char *url, int segundos, const char *const *cab,
                          const char *corpo, int *status, int *retryAfter) {
#ifdef NV_HTTP_HOST
  const char *v = verbo ? verbo : "POST";
  char *r;
  int st = 0;
  if (retryAfter) *retryAfter = 0;
  if (!url || !*url) { if (status) *status = 0; return NULL; }
  if (androidPrimeiro()) {   // st local: com status NULL o 4xx tambem volta com corpo, como no curl
    r = viaAndroid(v, url, segundos, cab, corpo ? corpo : "", &st, NULL, retryAfter, "libcurl fora");
    if (status) *status = st;
    return r;
  }
  r = postarNativo(url, segundos, cab, corpo, &st, 0, retryAfter, verbo);
  if (status) *status = st;
  if (!r && st == 0) {
    r = androidDepois(v, url, segundos, cab, corpo ? corpo : "", &st, retryAfter);
    if (status) *status = st;
  }
  return r;
#else
  return postarNativo(url, segundos, cab, corpo, status, 0, retryAfter, verbo);
#endif
}

char *rede_postar_st_retry(const char *url, int segundos, const char *const *cab,
                           const char *corpo, int *status, int *retryAfter) {
  return enviarNativo(NULL, url, segundos, cab, corpo, status, retryAfter);
}

// PUT de JSON. Existe para o TorBox, que edita um item (airlocked) so por PUT.
char *rede_colocar_st(const char *url, int segundos, const char *const *cab,
                      const char *corpo, int *status) {
  return enviarNativo("PUT", url, segundos, cab, corpo, status, NULL);
}

char *rede_postar_seguro_st(const char *url, int segundos, const char *const *cab,
                            const char *corpo, int *status) {
  if (status) *status = 0;
  if (!url || strncmp(url, "https://", 8)) return NULL;
  return postarNativo(url, segundos, cab, corpo, status, 1, NULL, NULL);
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
  redeCancelouLocal = 0;
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
  // byte. O conexaoMs de opcoesComuns fica no teto de 10 s.
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
  r = performTmdb(c, url);
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
  /* CONNECT_ONLY nao passa pelo multi/vigia: performTmdb removeria a easy
     antes do CURLINFO_LASTSOCKET, e o curl_easy_send/recv do websocket
     precisam dela associada a conexao que o CONNECT_ONLY abriu. */
  r = tmdbFallback(c, curl_perform(c), url, NULL);
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


// SONDA TLS (#737). O curl 35 do login nao diz O QUE o aperto de mao perdeu:
// cadeia de CA velha (a TV 2018 tem o store de fabrica, sem a ISRG Root X1),
// cifra/versao que a libcurl 7.53.1 nao negocia mais com o servidor, ou SNI.
// A sonda separa os tres com DUAS tentativas no MESMO host, ambas CONNECT_ONLY
// (nenhum byte de HTTP sai): primeiro com a verificacao LIGADA e o CAINFO do
// pacote (a prova "o bundle Mozilla do .ipk resolve a cadeia?"), depois sem
// verificar (a prova "o aperto de mao em si fecha?"). Os resultados se leem:
//   bundle=0  e  semca=<TLS>     -> servidor/lib nao se entendem (cifra/SNI);
//                                   mexer no CA nao ajuda.
//   bundle=<TLS>  e  semca=0     -> cifra/SNI ok; e a CADEIA. Falta a raiz no
//                                   store do aparelho E no bundle embarcado.
//   bundle=60/51/77  e  semca=0  -> mesma leitura: aperto ok, rejeicao do cert.
//   bundle=0                    -> o login devia passar: o bundle embarcado
//                                   cobre o host e o problema e transitorio.
// Sem CAINFO o "bundle" e o store de fabrica do webOS — o que distingue "a
// .ipk nao trouxe o PEM" de "o PEM nao cobre a cadeia do servidor".
// Nao vai a tela nem muda comportamento: e so o log, para o relato que chega
// sem in-app. Uma so chamada por tentativa de login com TLS; quem chama regula.
int rede_sonda_tls(const char *url) {
  static const int N = 3;
  const char *rotulo[3] = { "bundle", "sistema", "semca" };
  int r[3], i, base;
  void *c;
  if (!url || strncmp(url, "https://", 8) || !abrir()) return -1;
  for (i = 0; i < N; i++) {
    c = curl_init();
    if (!c) { r[i] = -2; continue; }
    curl_setopt(c, OPT_URL, url);
    curl_setopt(c, OPT_CONNECT_ONLY, (long)1);
    curl_setopt(c, OPT_HTTP_VERSION, (long)2 /* CURL_HTTP_VERSION_1_1 */);
    curl_setopt(c, OPT_CONNECTTIMEOUT_MS, (long)900);
    curl_setopt(c, OPT_TIMEOUT_MS, (long)900);
    curl_setopt(c, OPT_NOSIGNAL, (long)1);
    // semca replica exatamente o que opcoesComuns faz no caminho do login; as
    // duas outras sao a hipotese CA ligada, com o PEM do pacote e sem ele.
    if (i == 2) {
      curl_setopt(c, OPT_SSL_VERIFYPEER, (long)0);
      curl_setopt(c, OPT_SSL_VERIFYHOST, (long)0);
    } else {
      curl_setopt(c, OPT_SSL_VERIFYPEER, (long)1);
      curl_setopt(c, OPT_SSL_VERIFYHOST, (long)2);
      if (i == 0 && discordCa[0]) curl_setopt(c, OPT_CAINFO, discordCa);
    }
    r[i] = curl_perform(c);
    curl_cleanup(c);
  }
  { char seg[120];
    printf("[rede] sonda tls %s: %s=%d %s=%d %s=%d\n",
           rede_url_publica(url, seg, sizeof seg),
           rotulo[0], r[0], rotulo[1], r[1], rotulo[2], r[2]);
    fflush(stdout); }
  // Resumo: 0 = o aperto de mao em si falhou nas duas (cifra/SNI/rede);
  // 1 = fecha sem verificar mas nao com CA (cadeia/certificado);
  // 2 = fecha verificando (o problema do login e outro ou transitorio).
  base = r[2] == 0;
  if (!base) return 0;
  if (r[0] == 0 || r[1] == 0) return 2;
  return 1;
}


/* One response, bounded independently of the generic download TLS state. */
typedef struct {
  Balde body;
  int status, valid, overflow, range_headers;
  int64_t start, end, total, requested, limit;
  volatile int *cancel, *urgent;
  uint64_t progress_ms;
  long long progress_bytes;
  unsigned idle_ms;
  int *stalled;
  void (*retry)(void *,int);
  void *retry_arg;
  int retry_seen, retry_seconds;
#ifdef NV_HTTP_BUFFER_DIAG
  const char *server;
#endif
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
    r->retry_seen = r->retry_seconds = 0;
#ifdef NV_HTTP_BUFFER_DIAG
    r->server = "none";
#endif
  } else if (n > 12 && !strncasecmp(p, "Retry-After:", 12)) {
    r->retry_seen = 1; r->retry_seconds = retrySegundos(p + 12, n - 12);
#ifdef NV_HTTP_BUFFER_DIAG
  } else if (n > 7 && !strncasecmp(p, "Server:", 7)) {
    const char *v = p + 7;
    size_t left = n - 7;
    while (left && (*v == ' ' || *v == '\t')) { v++; left--; }
    /* Classify known products; arbitrary response text never reaches the log. */
    static const char *names[] = {"cloudflare", "nginx", "envoy", "openresty", "cloudfront", "apache"};
    r->server = "other";
    for (size_t i = 0; i < sizeof names / sizeof *names; i++) {
      size_t k = strlen(names[i]);
      if (left >= k && !strncasecmp(v, names[i], k) &&
          (left == k || v[k] == '/' || isspace((unsigned char)v[k]))) {
        r->server = names[i]; break;
      }
    }
#endif
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
  (void)dt; (void)ut; (void)un;
  if(r->cancel && __atomic_load_n(r->cancel,__ATOMIC_RELAXED)) return 1;
  uint64_t now=pedidoAgoraMs();
  if(dn!=r->progress_bytes) { r->progress_bytes=dn; r->progress_ms=now; }
  if(r->urgent && r->idle_ms && now>=r->progress_ms &&
     now-r->progress_ms>=r->idle_ms && __atomic_load_n(r->urgent,__ATOMIC_RELAXED)) {
    if(r->stalled) *r->stalled=1;
    printf("[range-stall] start=%lld idleMs=%llu got=%llu\n",(long long)r->requested,
           (unsigned long long)(now-r->progress_ms),(unsigned long long)r->body.n);
    fflush(stdout); return 1;
  }
  return 0;
}
char *rede_range_headers(const char *raw, int public_only) {
  char *copy, *line, *ctx = NULL;
  size_t used = 0;
  if (!raw) raw = "";
  char *out = malloc(strlen(raw) + 2);
  if (!out) return NULL;
  copy = strdup(raw);
  if (!copy) { free(out); return NULL; }
  for (line = strtok_r(copy, "\n", &ctx); line; line = strtok_r(NULL, "\n", &ctx)) {
    char *colon = strchr(line, ':');
    size_t n = strlen(line), key;
    if (n && line[n - 1] == '\r') line[--n] = 0;
    if (!colon || strchr(line, '\r')) continue;
    key = (size_t)(colon - line);
    if ((key == 5 && !strncasecmp(line, "range", key)) ||
        (key == 15 && !strncasecmp(line, "accept-encoding", key))) continue;
    if (public_only == 2) continue;
    if (public_only && !((key == 10 && !strncasecmp(line, "user-agent", key)) ||
                         (key == 7 && !strncasecmp(line, "referer", key)) ||
                         (key == 6 && !strncasecmp(line, "origin", key)))) continue;
    memcpy(out + used, line, n); used += n; out[used++] = '\n';
  }
  out[used] = 0;
  free(copy);
  return out;
}
static void *dtsRangeHeaders(const char *raw, int public_only) {
  void *list = NULL;
  char *copy = rede_range_headers(raw, public_only), *ctx = NULL;
  if (!copy) return NULL;
  if (slist_append) for (char *line = strtok_r(copy, "\n", &ctx); line; line = strtok_r(NULL, "\n", &ctx))
    list = slist_append(list, line);
  free(copy);
  return list;
}
#ifdef NV_HTTP_BUFFER_DIAG
typedef struct { unsigned id; uint64_t begun; int sample; } RangeDiag;
static unsigned rangeDiagSequence, rangeDiagActive;
static RangeDiag rangeDiagBegin(void) {
  static _Thread_local uint64_t next_sample;
  RangeDiag d = {0};
  d.id = __atomic_add_fetch(&rangeDiagSequence, 1, __ATOMIC_RELAXED);
  d.begun = pedidoAgoraMs();
  d.sample = d.id <= 8 || d.begun >= next_sample;
  if (d.sample) next_sample = d.begun + 10000;
  return d;
}
static void rangeDiagLog(const RangeDiag *d, const char *kind, void *c,
                         const char *url, const char *current, const char *next,
                         int hop, int public_only, int result, long http,
                         const DtsRange *r, int64_t end, int64_t bytes,
                         uint64_t hop_ms, unsigned active) {
  uint64_t now = pedidoAgoraMs();
  /* Errors stay visible; healthy redirects/payloads are sampled per worker.
   * No body copies, per-request heap allocations or extra HTTP requests. */
  if (!d->sample && !result && http < 400 && now - hop_ms < 3000) return;
  if (r->cancel && __atomic_load_n(r->cancel, __ATOMIC_RELAXED)) return;
  char original[NV_RANGE_DIAG_HOST_MAX], host[NV_RANGE_DIAG_HOST_MAX], target[NV_RANGE_DIAG_HOST_MAX];
  double dns = 0, tcp = 0, tls = 0, first = 0;
  long connections = -1;
  nv_range_diag_host(url, original); nv_range_diag_host(current, host); nv_range_diag_host(next, target);
  if (curl_getinfo) {
    curl_getinfo(c, INFO_TEMPO_DNS, &dns); curl_getinfo(c, INFO_TEMPO_TCP, &tcp);
    curl_getinfo(c, INFO_TEMPO_TLS, &tls);
    curl_getinfo(c, 3145745 /* CURLINFO_STARTTRANSFER_TIME */, &first);
    curl_getinfo(c, INFO_NUM_CONNECTS, &connections);
  }
  printf("[range-diag] v=1 id=%u kind=%s monoMs=%llu start=%lld end=%lld hop=%d "
         "origHost=%s host=%s nextHost=%s stripped=%d http=%ld curl=%d "
         "hopMs=%llu elapsedMs=%llu dnsMs=%.0f tcpMs=%.0f tlsMs=%.0f firstMs=%.0f "
         "newConn=%ld activeAtStart=%u retrySeen=%d retrySec=%d server=%s bytes=%lld\n",
         d->id, kind, (unsigned long long)now, (long long)r->requested, (long long)end,
         hop, original, host, target, public_only, http, result,
         (unsigned long long)(now - hop_ms), (unsigned long long)(now - d->begun),
         dns * 1000, tcp * 1000, tls * 1000, first * 1000, connections, active,
         r->retry_seen, r->retry_seen ? r->retry_seconds : -1,
         r->server ? r->server : "none", (long long)bytes);
}
#endif
/* `final` (8192 bytes, optional) receives the address that answered 206 when
 * at least one redirect was followed, "" otherwise; `cross` says whether that
 * address is on another origin (credential headers were NOT sent to it). */
static char *range64(const char *url, const char *headers,
                              int64_t start, int64_t end, long *size,
                              int64_t *total, int *status,
                              volatile int *cancelled, char *final, int *cross,
                              volatile int *urgent,unsigned idle_ms,int *stalled,
                              void (*retry)(void *,int),void *retry_arg, int strict_tls) {
  char current[8192], range[64], origin[128];
  int hop, public_only = 0, downgraded = 0;
  if(stalled) *stalled=0;
  if (size) *size = 0;
  if (total) *total = -1;
  if (status) *status = 0;
  if (final) final[0] = 0;
  if (cross) *cross = 0;
  if (!url || strlen(url) >= sizeof current || start < 0 || end < start ||
      end - start >= 4 * 1024 * 1024 || !abrir()) return NULL;
  if (strict_tls ? strncmp(url, "https://", 8) :
      (strncmp(url, "http://", 7) && strncmp(url, "https://", 8))) return NULL;
  snprintf(current, sizeof current, "%s", url);
  if (!dtsRangeOrigin(url, origin, sizeof origin)) return NULL;
  snprintf(range, sizeof range, "%lld-%lld", (long long)start, (long long)end);
#ifdef NV_HTTP_BUFFER_DIAG
  RangeDiag diag = rangeDiagBegin();
#endif
  for (hop = 0; hop < 6; hop++) {
    DtsRange r;
    void *c, *list;
    int result;
    long http = 0;
    char redirected[8192] = "", *next = NULL;
    if (cancelled && __atomic_load_n(cancelled, __ATOMIC_RELAXED)) return NULL;
    memset(&r, 0, sizeof r);
    r.requested = start; r.limit = end - start + 1; r.total = -1; r.cancel = cancelled;
    r.urgent=urgent; r.idle_ms=idle_ms; r.stalled=stalled; r.progress_ms=pedidoAgoraMs();
    r.retry=retry; r.retry_arg=retry_arg;
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
#ifdef NV_HTTP_BUFFER_DIAG
    uint64_t hop_ms = pedidoAgoraMs();
    unsigned active = __atomic_add_fetch(&rangeDiagActive, 1, __ATOMIC_RELAXED);
#endif
    // O hop manual segue `current` (o redirect ja aplicado): marcar a familia
    // pelo host realmente alcancado, nao pela URL original (#707 r2).
    result = performTmdb(c, current);
#ifdef NV_HTTP_BUFFER_DIAG
    __atomic_sub_fetch(&rangeDiagActive, 1, __ATOMIC_RELAXED);
#endif
    if (curl_getinfo) {
      curl_getinfo(c, INFO_RESPONSE_CODE, &http);
      /* CURLINFO_REDIRECT_URL resolves relative Location without following it. */
      curl_getinfo(c, 1048607, &next);
      if (next && strlen(next) < sizeof redirected)
        snprintf(redirected, sizeof redirected, "%s", next);
    }
    if (status) *status = (int)http;
    /* Every 429 hop reports its Retry-After: >=0 when seen (0 malformed),
     * -1 when absent — the same contract as the old int* out-param. */
    if (http == 429 && r.retry) r.retry(r.retry_arg, r.retry_seen ? r.retry_seconds : -1);
#ifdef NV_HTTP_BUFFER_DIAG
    rangeDiagLog(&diag, "chunk", c, url, current, redirected, hop, public_only,
                 result, http, &r, end, (int64_t)r.body.n, hop_ms, active);
#endif
    if (list && slist_free) slist_free(list);
    soltarHandleR(c, result, current);
    if (!result && http >= 300 && http < 400 && redirected[0]) {
      char host[128];
      free(r.body.p);
      if (strict_tls ? strncmp(redirected, "https://", 8) :
          (strncmp(redirected, "http://", 7) && strncmp(redirected, "https://", 8))) return NULL;
      if (!dtsRangeOrigin(redirected, host, sizeof host)) return NULL;
      if (strcmp(host, origin)) public_only = strict_tls ? 2 : 1;
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
      if (cross) *cross = downgraded ? 2 : public_only;
      return baldeFinal(&r.body);
    }
    /* Numeric only: identify failed interval, response validation and discarded
     * partial bytes without exposing URL, request headers or response payload. */
    if(!cancelled || !__atomic_load_n(cancelled,__ATOMIC_RELAXED)) {
      printf("[range-http] start=%lld end=%lld curl=%d http=%ld got=%llu "
             "valid=%d overflow=%d responseStart=%lld responseEnd=%lld total=%lld\n",
             (long long)start,(long long)end,result,http,(unsigned long long)r.body.n,
             r.valid,r.overflow,(long long)r.start,(long long)r.end,(long long)r.total);
      fflush(stdout);
    }
    free(r.body.p);
    return NULL;
  }
  return NULL;
}
char *rede_baixar_trecho64_vigiado_retry(const char *url,const char *headers,
    int64_t start,int64_t end,long *size,int64_t *total,int *status,
    volatile int *cancelled,char *final,int *cross,volatile int *urgent,
    unsigned idle_ms,int *stalled,void (*retry)(void *,int),void *retry_arg) {
  return range64(url,headers,start,end,size,total,status,cancelled,final,cross,
                 urgent,idle_ms,stalled,retry,retry_arg,0);
}
char *rede_baixar_trecho64_tls(const char *url,const char *headers,
    int64_t start,int64_t end,long *size,int64_t *total,int *status,
    volatile int *cancelled,char *final,int *cross) {
  return range64(url,headers,start,end,size,total,status,cancelled,final,cross,
                 NULL,0,NULL,NULL,NULL,1);
}
char *rede_baixar_trecho64_vigiado(const char *url,const char *headers,
    int64_t start,int64_t end,long *size,int64_t *total,int *status,
    volatile int *cancelled,char *final,int *cross,volatile int *urgent,
    unsigned idle_ms,int *stalled) {
  return rede_baixar_trecho64_vigiado_retry(url,headers,start,end,size,total,status,
      cancelled,final,cross,urgent,idle_ms,stalled,NULL,NULL);
}
char *rede_baixar_trecho64_final(const char *url,const char *headers,
                              int64_t start,int64_t end,long *size,
                              int64_t *total,int *status,
                              volatile int *cancelled,char *final,int *cross) {
  return rede_baixar_trecho64_vigiado(url,headers,start,end,size,total,status,cancelled,
                                    final,cross,NULL,0,NULL);
}
char *rede_baixar_trecho64_cab(const char *url, const char *headers,
                              int64_t start, int64_t end, long *size,
                              int64_t *total, int *status,
                              volatile int *cancelled) {
  return rede_baixar_trecho64_final(url, headers, start, end, size, total, status, cancelled, NULL, NULL);
}

/* ONE LONG READ, LIKE THE TV'S OWN PLAYER (Samsung DTS/TrueHD converter).
 *
 * The ranged reader above asks 4 MiB at a time from up to 4 lanes. A 4K remux
 * at 60-67 Mbit/s is then ~2 requests a second on 4 connections, and TorBox's
 * CDN throttled it after about a minute and refused every connection after
 * (S95C, 13/10/2026), while the TV's player streaming the same file over one
 * connection never dropped. This asks "bytes=start-" once and hands the body
 * to `sink` as it arrives. The sink may block (backpressure: the converter is
 * paced by the TV); time spent inside it does not count as a stall. */
typedef struct {
  DtsRange r;
  int (*sink)(void *, const char *, size_t);
  void *opaque;
  int stopped, accepted;
  int64_t got;
  uint64_t lastByteMs;
  long stallMs;
} DtsFluxo;
static uint64_t fluxoAgoraMs(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return (uint64_t)t.tv_sec * 1000u + (uint64_t)t.tv_nsec / 1000000u;
}
static size_t dtsFluxoBody(void *data, size_t size, size_t count, void *user) {
  DtsFluxo *f = user;
  size_t n;
  if (size && count > SIZE_MAX / size) return 0;
  n = size * count;
  if (f->r.cancel && __atomic_load_n(f->r.cancel, __ATOMIC_RELAXED)) return 0;
  if (f->r.status >= 300 && f->r.status < 400) return n;
  if (!f->accepted) {
    /* 206 starting exactly where asked; a 200 is the whole file, usable only
     * when the request was for byte 0 (some origins ignore Range). */
    if (f->r.status == 206 && f->r.valid && f->r.start == f->r.requested) f->accepted = 1;
    else if (f->r.status == 200 && f->r.requested == 0) f->accepted = 1;
    else { f->r.overflow = 1; return 0; }
  }
  if (!f->sink(f->opaque, data, n)) { f->stopped = 1; return 0; }
  f->got += (int64_t)n;
  f->lastByteMs = fluxoAgoraMs();
  return n;
}
static int dtsFluxoProgress(void *user, long long dt, long long dn, long long ut, long long un) {
  DtsFluxo *f = user;
  (void)dt; (void)dn; (void)ut; (void)un;
  if (f->r.cancel && __atomic_load_n(f->r.cancel, __ATOMIC_RELAXED)) return 1;
  return f->stallMs > 0 && fluxoAgoraMs() - f->lastByteMs > (uint64_t)f->stallMs;
}
static int fluxo64(const char *url, const char *headers, int64_t start,
                 int (*sink)(void *opaque, const char *data, size_t n), void *opaque,
                 int64_t *total, int *status, volatile int *cancelled,
                 char *final, int *cross, long stall_ms, int64_t *bytes, int strict_tls) {
  char current[8192], range[48], origin[128];
  int hop, public_only = 0, downgraded = 0;
  if (total) *total = -1;
  if (status) *status = 0;
  if (final) final[0] = 0;
  if (cross) *cross = 0;
  if (bytes) *bytes = 0;
  if (!url || !sink || strlen(url) >= sizeof current || start < 0 || !abrir()) return -1;
  if (strict_tls ? strncmp(url, "https://", 8) :
      (strncmp(url, "http://", 7) && strncmp(url, "https://", 8))) return -1;
  snprintf(current, sizeof current, "%s", url);
  if (!dtsRangeOrigin(url, origin, sizeof origin)) return -1;
  snprintf(range, sizeof range, "%lld-", (long long)start);
#ifdef NV_HTTP_BUFFER_DIAG
  RangeDiag diag = rangeDiagBegin();
#endif
  for (hop = 0; hop < 6; hop++) {
    DtsFluxo f;
    void *c, *list;
    int result;
    long http = 0;
    char redirected[8192] = "", *next = NULL;
    if (cancelled && __atomic_load_n(cancelled, __ATOMIC_RELAXED)) return -1;
    memset(&f, 0, sizeof f);
    f.r.requested = start; f.r.total = -1; f.r.cancel = cancelled;
    f.sink = sink; f.opaque = opaque; f.stallMs = stall_ms; f.lastByteMs = fluxoAgoraMs();
    c = pegarHandle(current);
    if (!c) return -1;
    opcoesComuns(c, 15000);                 /* connect deadline from here...      */
    curl_setopt(c, OPT_TIMEOUT_MS, (long)0); /* ...but no limit on the whole body */
    /* O recuo IPv4 do #707 le redePrazoAtualMs como prazo total do pedido: um
     * stream longo com dados chegando nao pode ganhar os 15 s de opcoesComuns
     * (regressao da r2). 0 = sem teto, igual ao OPT_TIMEOUT_MS acima. */
    redePrazoAtualMs = 0;
    curl_setopt(c, OPT_SSL_VERIFYPEER, (long)1);
    curl_setopt(c, OPT_SSL_VERIFYHOST, (long)2);
    if (discordCa[0]) curl_setopt(c, OPT_CAINFO, discordCa);
    curl_setopt(c, OPT_URL, current);
    curl_setopt(c, OPT_RANGE, range);
    curl_setopt(c, OPT_FOLLOWLOCATION, (long)0);
    curl_setopt(c, OPT_WRITEFUNCTION, dtsFluxoBody);
    curl_setopt(c, OPT_WRITEDATA, &f);
    curl_setopt(c, OPT_HEADERFUNCTION, dtsRangeHeader);
    curl_setopt(c, OPT_HEADERDATA, &f.r);
    curl_setopt(c, OPT_XFERINFOFUNCTION, dtsFluxoProgress);
    curl_setopt(c, OPT_XFERINFODATA, &f);
    curl_setopt(c, OPT_NOPROGRESS, (long)0);
    list = downgraded ? NULL : dtsRangeHeaders(headers, public_only);
    if (list) curl_setopt(c, OPT_HTTPHEADER, list);
#ifdef NV_HTTP_BUFFER_DIAG
    uint64_t hop_ms = pedidoAgoraMs();
    unsigned active = __atomic_add_fetch(&rangeDiagActive, 1, __ATOMIC_RELAXED);
#endif
    // O hop manual segue `current` (o redirect ja aplicado): marcar a familia
    // pelo host realmente alcancado, nao pela URL original (#707 r2).
    result = performTmdb(c, current);
#ifdef NV_HTTP_BUFFER_DIAG
    __atomic_sub_fetch(&rangeDiagActive, 1, __ATOMIC_RELAXED);
#endif
    if (curl_getinfo) {
      curl_getinfo(c, INFO_RESPONSE_CODE, &http);
      curl_getinfo(c, 1048607, &next);
      if (next && strlen(next) < sizeof redirected)
        snprintf(redirected, sizeof redirected, "%s", next);
    }
    if (status) *status = (int)http;
#ifdef NV_HTTP_BUFFER_DIAG
    rangeDiagLog(&diag, "stream", c, url, current, redirected, hop, public_only,
                 result, http, &f.r, -1, f.got, hop_ms, active);
#endif
    if (list && slist_free) slist_free(list);
    soltarHandleR(c, result, current);
    if (bytes) *bytes = f.got;
    if (f.r.total >= 0 && total) *total = f.r.total;
    if (!result && http >= 300 && http < 400 && redirected[0]) {
      char host[128];
      if (strict_tls ? strncmp(redirected, "https://", 8) :
          (strncmp(redirected, "http://", 7) && strncmp(redirected, "https://", 8))) return -1;
      if (!dtsRangeOrigin(redirected, host, sizeof host)) return -1;
      if (strcmp(host, origin)) public_only = strict_tls ? 2 : 1;
      if (!strncmp(current, "https://", 8) && !strncmp(redirected, "http://", 7)) downgraded = public_only = 1;
      snprintf(current, sizeof current, "%s", redirected);
      continue;
    }
    if (f.accepted) {
      if (final && hop > 0) snprintf(final, 8192, "%s", current);
      if (cross) *cross = downgraded ? 2 : public_only;
    }
    if (f.stopped) return 1;                 /* the sink asked to stop */
    if (!result && f.accepted) return 0;     /* body ended (EOF or a capped range) */
    return -1;                               /* error: status/curl, bytes so far in *bytes */
  }
  return -1;
}
int rede_fluxo64(const char *url, const char *headers, int64_t start,
                 int (*sink)(void *, const char *, size_t), void *opaque,
                 int64_t *total, int *status, volatile int *cancelled,
                 char *final, int *cross, long stall_ms, int64_t *bytes) {
  return fluxo64(url, headers, start, sink, opaque, total, status, cancelled, final, cross, stall_ms, bytes, 0);
}
int rede_fluxo64_tls(const char *url, const char *headers, int64_t start,
                 int (*sink)(void *, const char *, size_t), void *opaque,
                 int64_t *total, int *status, volatile int *cancelled,
                 char *final, int *cross, long stall_ms, int64_t *bytes) {
  return fluxo64(url, headers, start, sink, opaque, total, status, cancelled, final, cross, stall_ms, bytes, 1);
}

#endif  /* __EMSCRIPTEN__ */

// #382: o resolvedor da TV falha por minutos depois do boot ("Resolving timed out").
// Falha de transporte (status 0) tenta de novo com espera crescente; resposta
// HTTP, ou tentativa que gastou o prazo inteiro, encerra.
#ifndef REDE_POST_ESPERA_MS
#define REDE_POST_ESPERA_MS 1000
#endif
char *rede_postar_repetindo(const char *url, int segundos, const char *const *cab,
                            const char *corpo, int *status, int tentativas) {
  char *r = NULL;
  int k;
  for (k = 0; k < tentativas || k == 0; k++) {
    unsigned long t0 = redeAgoraMs();
    r = rede_postar_st(url, segundos, cab, corpo, status);
    if (r || !status || *status != 0 || k + 1 >= tentativas) break;
    if (redeAgoraMs() - t0 >= (unsigned long)segundos * 1000UL * 3 / 4) break;
    { struct timespec d;
      unsigned long ms = (unsigned long)REDE_POST_ESPERA_MS << k;
      d.tv_sec = (time_t)(ms / 1000); d.tv_nsec = (long)(ms % 1000) * 1000000L;
      nanosleep(&d, NULL); }
  }
  return r;
}

#ifdef __EMSCRIPTEN__
char *rede_baixar_trecho64_tls(const char *u, const char *h, int64_t s, int64_t e, long *n, int64_t *t, int *st,
                               volatile int *c, char *f, int *x) {
  return rede_baixar_trecho64_final(u, h, s, e, n, t, st, c, f, x);
}
int rede_fluxo64(const char *u, const char *h, int64_t s, int (*sk)(void *, const char *, size_t), void *o,
                 int64_t *t, int *st, volatile int *c, char *f, int *x, long ms, int64_t *b) {
  (void)u; (void)h; (void)s; (void)sk; (void)o; (void)c; (void)ms;
  if (t) *t = -1;
  if (st) *st = 0;
  if (f) f[0] = 0;
  if (x) *x = 0;
  if (b) *b = 0;
  return -1;
}
int rede_fluxo64_tls(const char *u, const char *h, int64_t s, int (*sk)(void *, const char *, size_t), void *o,
                     int64_t *t, int *st, volatile int *c, char *f, int *x, long ms, int64_t *b) {
  return rede_fluxo64(u,h,s,sk,o,t,st,c,f,x,ms,b);
}
int rede_sonda_tls(const char *url) { (void)url; return -1; }
char *rede_baixar_trecho64_vigiado(const char *u,const char *h,int64_t s,int64_t e,
 long *n,int64_t *t,int *st,volatile int *c,char *f,int *x,volatile int *urgent,unsigned idle,int *stalled) {
 (void)urgent;(void)idle;if(stalled)*stalled=0;
 return rede_baixar_trecho64_final(u,h,s,e,n,t,st,c,f,x);
}
char *rede_baixar_trecho64_vigiado_retry(const char *u,const char *h,int64_t s,int64_t e,
 long *n,int64_t *t,int *st,volatile int *c,char *f,int *x,volatile int *urgent,unsigned idle,int *stalled,
 void (*retry)(void *,int),void *arg) {
 (void)retry;(void)arg;return rede_baixar_trecho64_vigiado(u,h,s,e,n,t,st,c,f,x,urgent,idle,stalled);
}
char *rede_baixar_trecho64_cab(const char *u, const char *h, int64_t s, int64_t e, long *n, int64_t *t, int *st, volatile int *c) {
  (void)u; (void)h; (void)s; (void)e; (void)c;
  if (n) *n = 0;
  if (t) *t = -1;
  if (st) *st = 0;
  return NULL;
}
char *rede_baixar_trecho64_final(const char *u, const char *h, int64_t s, int64_t e, long *n, int64_t *t, int *st,
                                 volatile int *c, char *f, int *x) {
  if (f) f[0] = 0;
  if (x) *x = 0;
  return rede_baixar_trecho64_cab(u, h, s, e, n, t, st, c);
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
#if !defined(NV_TPK40) && !defined(NV_UM_FIO) // otherwise stored in RedeFio
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
  char atual[NV_MEDIA_URL_MAX], fin[NV_MEDIA_URL_MAX];
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
