#include "webp.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sdlcompat.h"

// Dois decodificadores atras da mesma porta. Ambos entregam um bloco RGBA
// recem-alocado (bytes R,G,B,A na memoria) e dizem o tamanho; quem monta a
// SDL_Surface e webp_carregar la embaixo, uma vez so.
static uint8_t *decodificar(const unsigned char *dados, size_t n, int largMax,
                            int *lw, int *lh, int *ow, int *oh);
static void     soltar(uint8_t *px);

#ifdef __EMSCRIPTEN__
// ---------------------------------------------------------------- Tizen ----
//
// O NAVEGADOR JA SABE LER WEBP; o problema e que ele so sabe de forma
// assincrona, e este codigo roda no fio de decode do tex_cache, que e
// sincrono. O Emscripten nao tem port de libwebp (ver tools/tizen.sh) e o
// dlopen do outro ramo nao existe em WASM — ele compila, devolve NULL sempre,
// e era exatamente esse silencio que a TV do dono mostrava:
//
//   [tex] decode falhou (Unsupported image format) tam=14312 magica=52494646
//
// 52494646 e "RIFF": WebP puro, vindo do addon de posters.
//
// COMO A PONTE FUNCIONA (protocolo novo, 22/09/2026). O fio de decode le o
// tamanho no CABECALHO do arquivo (PNG IHDR, WebP VP8/VP8L/VP8X), calcula o
// tamanho de saida com a mesma conta do Worker e ALOCA TUDO ANTES: um bloco
// de job, uma COPIA dos bytes comprimidos e o bloco de pixels. Manda um
// pedaco de JS ao FIO PRINCIPAL, que so REPASSA o pedido a um Worker proprio
// (tools/decodificador.js), e dorme num futex. O Worker decodifica
// (createImageBitmap), reduz (OffscreenCanvas), escreve os pixels no bloco
// que ja existe e fecha o job com um compareExchange. Ninguem espera ninguem
// do lado do Worker.
//
// POR QUE MUDOU (logs da Samsung 1.4.1): icone de 128x128 levando 8 a 17 s,
// `decode lento: 17702 ms ... menu_home.png`, `8047 ms ... play.png`. O
// protocolo antigo pedia o malloc DEPOIS do decode (estado 2 -> o C aloca ->
// estado 3) e o Worker, que e um fio so, ficava em Atomics.wait ate 8 s por
// esse malloc. Quando o C ja tinha desistido do pedido (o prazo dele conta
// desde o envio, fila incluida), o Worker esperava os 8 s inteiros por
// ninguem, e a fila INTEIRA atras dele estourava o prazo tambem — cada
// estouro gerava outro abandono e outra espera de 8 s. tests/decodefila-tizen.sh
// reproduz: com prazo de 300 ms e 1 pedido lento em 7, o protocolo antigo
// perdeu 44 de 60 pedidos RAPIDOS (5 ms); o novo perde 0.
//
// POSSE DO JOB. job[J_EST]:
//   0 ABERTO      o C espera; Worker (ou fio principal) trabalha
//   1 PRONTO      quem decodificou escreveu J_W/J_H e os pixels; o C consome
//   4 ABANDONADO  o C passou do prazo e desistiu (CAS 0 -> 4); nada e liberado
//   5 LARGADO     o Worker terminou um job abandonado (CAS 4 -> 5): so agora o
//                 C pode liberar job, copia e pixels (varrer(), no proximo pedido)
// O C NUNCA libera nada que o Worker ainda pode tocar, e um job so volta ao
// allocator depois de 1 (consumido) ou 5 (largado) — entao nenhuma resposta
// atrasada pode cair na memoria de outro pedido. J_SEQ e um numero unico por
// pedido: o Worker confere antes de tocar no job, o que descarta tambem um
// pedido reencaminhado pelo onerror depois de o job ter sido liberado.
//
// POR QUE SAIU DO FIO PRINCIPAL (#72, AU7000, 1.3.2): a versao anterior fazia
// drawImage + getImageData no `then`, no fio principal, e no AU7000 cada arte
// custava ate 1 s de laco de quadro parado (`swap=1004` no [quadro], FPS=2
// enquanto a fileira carregava, e o app caindo no meio). No Mac isso media
// sub-milissegundo e por isso a nota antiga chamava de barato.
//
// O CAMINHO PELO FIO PRINCIPAL CONTINUA COMO RESERVA: e o que roda se o Worker
// nao subir (Chromium sem OffscreenCanvas, arquivo nao servido). O pedido em
// voo e devolvido a ele pelo onerror, em vez de vencer o prazo aqui. Segue o
// mesmo protocolo de posse.
//
// POR QUE NAO ASYNCIFY AQUI, que seria mais curto: o laco de quadro em main.c
// ja desenrola por asyncify a cada rAF, e por-lo tambem nos fios de decode
// significaria confiar no unwinding dentro de Worker de pthread para uma
// operacao que acontece centenas de vezes por sessao. O futex nao depende de
// nada disso.
//
// POR QUE UM WORKER PROPRIO E NAO O PTHREAD DE DECODE: o pthread esta
// bloqueado no futex, e a promessa do createImageBitmap so resolveria quando
// ele voltasse ao laco de eventos — nunca. O worker proprio nao roda C; so
// espera pedidos.
//
// CANAL DIRETO, SEM O FIO PRINCIPAL (23/09/2026). Ate aqui todo pedido ia ao
// Worker PASSANDO pelo fio principal (MAIN_THREAD_ASYNC_EM_ASM), e o Worker so
// nascia no primeiro pedido, tambem pelo fio principal. Os logs da Samsung
// 1.4.1 tem tarefas longas de 1 a 31 s nesse fio (manifestos, montagem da
// home): o pedido esperava a tarefa inteira na fila, estourava os 8 s e o
// icone local de 128 px levou 57 s (registro 1647). Agora:
//   - navegador_iniciar(), chamado no arranque (main.c), cria o Worker de
//     decode e uma SENTINELA (o mesmo decodificador.js, outro papel), ligados
//     por um MessageChannel;
//   - o fio de decode empilha o job numa pilha na memoria compartilhada
//     (filaCabeca, CAS; so a sentinela desempilha, e sempre a pilha inteira,
//     entao nao ha ABA) e acorda a sentinela com um futex;
//   - a sentinela dorme em Atomics.wait, desempilha, e repassa ao Worker pela
//     porta. Ela nunca volta ao laco de eventos — nao precisa: postMessage de
//     uma porta nao depende dele.
// O fio principal nao participa de nada disso. O caminho antigo (via fio
// principal) continua para quando o canal nao esta de pe: antes de o Worker e
// a sentinela confirmarem (canalEstado 1|2), sem MessageChannel, ou depois de
// um deles morrer (bit 4).
//
// E OS printf DESTE ARQUIVO SAO ASSINCRONOS (nvLog): no Emscripten o printf de
// um pthread e um syscall PROXIADO de forma SINCRONA ao fio principal. Com ele
// ocupado, o proprio aviso de "abandonado" segurava o fio de decode — medido
// em tests/decodefila-tizen.sh: 2000 ms num pedido de prazo 300 ms.
#include <emscripten.h>
#include <emscripten/threading.h>
#include <math.h>
#include <pthread.h>
#include <stdarg.h>

// Prazo do fio de decode, em tempo de relogio desde o envio. O teste da fila
// compila com um prazo curto para forcar abandono.
#ifndef NV_NAV_PRAZO_MS
#define NV_NAV_PRAZO_MS 8000
#endif
// Layout do job (int32 cada). Espelhado em tools/decodificador.js e no JS
// logo abaixo — mudar aqui e mudar la.
// J_ORIGEM: 0 fio principal, 1 Worker via fio principal, 2 Worker pelo canal
// direto. J_FILA liga os jobs na pilha da sentinela; J_MIME (1 webp, 2 png, 3 gif) e
// J_LARG sao o que a sentinela repassa ao Worker, que nao le string do C.
enum { J_EST, J_W, J_H, J_PTR, J_OW, J_OH, J_ORIGEM, J_SEQ, J_CAP, J_DADOS, J_N, J_PROX,
       J_FILA, J_MIME, J_LARG, J_INTS };
enum { EST_ABERTO = 0, EST_PRONTO = 1, EST_ABANDONADO = 4, EST_LARGADO = 5 };

static int jaContou;
static int seqGlobal;   // __atomic_*: o C89 do projeto nao usa stdatomic
// Jobs abandonados que o Worker ainda nao largou. Lista encadeada por J_PROX.
static pthread_mutex_t lixoMtx = PTHREAD_MUTEX_INITIALIZER;
static int32_t *lixo;
static int nLixo;

// CANAL DIRETO. filaCabeca: topo da pilha de jobs para a sentinela (0 =
// vazia). canalEstado: bit 1 o Worker recebeu a porta, bit 2 a sentinela esta
// no laco, bit 4 um dos dois morreu. So vale o canal com exatamente 1|2.
static int32_t filaCabeca;
static int32_t canalEstado;
static int iniciarPedido;
enum { CANAL_DEC = 1, CANAL_SENT = 2, CANAL_MORTO = 4 };

// printf SEM ESPERAR O FIO PRINCIPAL. Ver a nota do canal direto: o printf de
// pthread e sincrono com o fio principal. A linha vai por um pedido assincrono
// e o fio principal a imprime quando puder, na ordem em que chegou.
static void nvLog(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void nvLog(const char *fmt, ...) {
  char *b = (char *)malloc(256);
  va_list ap;
  if (!b) return;
  va_start(ap, fmt);
  vsnprintf(b, 256, fmt, ap);
  va_end(ap);
  if (emscripten_is_main_browser_thread()) { printf("%s\n", b); free(b); return; }
  MAIN_THREAD_ASYNC_EM_ASM({ out(UTF8ToString($0)); _free($0); }, b);
}

static void soltarJob(int32_t *job) {
  free((void *)(intptr_t)job[J_DADOS]);
  free((void *)(intptr_t)job[J_PTR]);
  free(job);
}

// Libera os abandonados que o Worker ja largou (estado 5). Barato: a lista so
// tem o que passou do prazo, e o Worker novo termina cada um em milissegundos.
static int varrer(void) {
  int32_t *j, *ant = NULL, *prox;
  int vivos;
  pthread_mutex_lock(&lixoMtx);
  for (j = lixo; j; j = prox) {
    prox = (int32_t *)(intptr_t)j[J_PROX];
    if (__atomic_load_n(&j[J_EST], __ATOMIC_ACQUIRE) == EST_LARGADO) {
      if (ant) ant[J_PROX] = (int32_t)(intptr_t)prox; else lixo = prox;
      soltarJob(j);
      nLixo--;
    } else ant = j;
  }
  vivos = nLixo;
  pthread_mutex_unlock(&lixoMtx);
  return vivos;
}

int navegador_abandonados_vivos(void) { return varrer(); }

static unsigned le32be(const unsigned char *p) { return ((unsigned)p[0] << 24) | ((unsigned)p[1] << 16) | ((unsigned)p[2] << 8) | p[3]; }
static unsigned le24le(const unsigned char *p) { return p[0] | ((unsigned)p[1] << 8) | ((unsigned)p[2] << 16); }

// Tamanho da imagem pelo CABECALHO, sem decodificar. PNG, WebP e GIF passam
// por esta ponte (JPEG e software, jpegrapido.c). 0 quando nao reconhece. Do
// GIF vale a TELA LOGICA, que e o tamanho do primeiro quadro que o
// createImageBitmap devolve.
static int dimensoes(const unsigned char *d, size_t n, int *w, int *h) {
  *w = *h = 0;
  if (n >= 24 && d[0] == 0x89 && !memcmp(d + 1, "PNG", 3) && !memcmp(d + 12, "IHDR", 4)) {
    *w = (int)le32be(d + 16); *h = (int)le32be(d + 20);
  } else if (n >= 30 && !memcmp(d, "RIFF", 4) && !memcmp(d + 8, "WEBP", 4)) {
    if (!memcmp(d + 12, "VP8X", 4)) {
      *w = (int)le24le(d + 24) + 1; *h = (int)le24le(d + 27) + 1;
    } else if (!memcmp(d + 12, "VP8L", 4) && d[20] == 0x2f) {
      unsigned b = d[21] | ((unsigned)d[22] << 8) | ((unsigned)d[23] << 16) | ((unsigned)d[24] << 24);
      *w = (int)(b & 0x3fff) + 1; *h = (int)((b >> 14) & 0x3fff) + 1;
    } else if (!memcmp(d + 12, "VP8 ", 4) && d[23] == 0x9d && d[24] == 0x01 && d[25] == 0x2a) {
      *w = (d[26] | (d[27] << 8)) & 0x3fff; *h = (d[28] | (d[29] << 8)) & 0x3fff;
    }
  } else if (n >= 13 && !memcmp(d, "GIF8", 4)) {
    *w = d[6] | (d[7] << 8); *h = d[8] | (d[9] << 8);
  }
  return *w > 0 && *h > 0 && *w <= 32768 && *h <= 32768;
}

// NO FIO PRINCIPAL, uma vez, no arranque. Cria o Worker de decode e a
// sentinela e liga os dois por um MessageChannel. Idempotente. Chamado de um
// pthread, pede a si mesmo ao fio principal e volta na hora (o pedido corrente
// segue pelo caminho antigo).
void navegador_iniciar(void) {
#ifdef NV_COOP
  // TIZEN 4 (NV_COOP): nao ha Worker de decode. Ele precisa de
  // SharedArrayBuffer, Atomics e OffscreenCanvas, todos M60+, e o M56 nao tem
  // nenhum. Todo pedido vai por nv_dec_coop, no fio principal.
  __atomic_store_n(&iniciarPedido, 1, __ATOMIC_RELEASE);
  __atomic_store_n(&canalEstado, CANAL_MORTO, __ATOMIC_RELEASE);
  return;
#endif
  if (!emscripten_is_main_browser_thread()) {
    if (!__atomic_exchange_n(&iniciarPedido, 1, __ATOMIC_ACQ_REL))
      emscripten_async_run_in_main_runtime_thread(EM_FUNC_SIG_V, navegador_iniciar);
    return;
  }
  __atomic_store_n(&iniciarPedido, 1, __ATOMIC_RELEASE);
  EM_ASM({
    // `Module.nvDec` guarda o worker e os pedidos em voo que foram PELO FIO
    // PRINCIPAL, por numero de pedido; o Worker avisa `feito` e o pedido sai
    // da lista. Se o worker morre, o que ainda esta na lista volta ao caminho
    // antigo. Os pedidos do canal direto nao entram na lista (ver onerror).
    // Sem virgula em nivel de chave (ver a nota do EM_ASM mais abaixo).
    if (Module.nvDec !== undefined) return;
    var pCanal = $1 >> 2;
    var D = {};
    D.w = null;
    D.s = null;
    D.morto = false;
    D.voo = {};
    Module.nvDec = D;
    var matarCanal = function (quem, e) {
      Atomics.or(HEAP32, pCanal, 4);
      console.log('[webp] ' + quem + ' caiu (' + (e && e.message ? e.message : '?') + '); canal direto desligado');
    };
    if (typeof OffscreenCanvas === 'undefined' || typeof Worker === 'undefined') {
      D.morto = true;
      Atomics.or(HEAP32, pCanal, 4);
      return;
    }
    try {
      D.w = new Worker('decodificador.js');
      D.w.onmessage = function (ev) {
        if (ev.data && ev.data.feito !== undefined) delete D.voo[ev.data.feito];
      };
      // Worker morto: o canal direto sai (os pedidos novos voltam ao fio
      // principal), e os pedidos que tinham ido pelo fio principal voltam ao
      // decode no proprio fio principal. Os que estavam no canal direto
      // vencem o prazo no C e ficam no aguardo para sempre — no maximo um
      // por fio de decode, e so se o Worker morrer com eles em voo.
      D.w.onerror = function (e) {
        D.morto = true;
        matarCanal('decodificador.js', e);
        try { if (D.s) D.s.terminate(); } catch (x) {}
        console.log('[webp] decode volta ao fio principal');
        var k;
        for (k in D.voo) { if (D.voo.hasOwnProperty(k)) { var f = D.voo[k]; delete D.voo[k]; f(); } }
      };
      if (typeof MessageChannel === 'undefined') {
        Atomics.or(HEAP32, pCanal, 4);
        D.w.postMessage({ memoria: wasmMemory.buffer });
      } else {
        var mc = new MessageChannel();
        D.w.postMessage({ memoria: wasmMemory.buffer, porta: mc.port2, canal: $1 }, [mc.port2]);
        D.s = new Worker('decodificador.js');
        D.s.onerror = function (e) { matarCanal('sentinela', e); };
        D.s.postMessage({ sentinela: 1, memoria: wasmMemory.buffer, porta: mc.port1, cab: $0, canal: $1 }, [mc.port1]);
      }
    } catch (e) { D.morto = true; matarCanal('criacao', e); }
  }, (int)(intptr_t)&filaCabeca, (int)(intptr_t)&canalEstado);
}

// Codigo do mime para a sentinela (0 = so pelo caminho antigo).
static int mimeCodigo(const char *mime) {
  if (!mime) return 0;
  if (!strcmp(mime, "image/webp")) return 1;
  if (!strcmp(mime, "image/png")) return 2;
  if (!strcmp(mime, "image/gif")) return 3;
  return 0;
}

#ifdef NV_COOP
// O CAMINHO DE RESERVA (noFioPrincipal, mais abaixo) SEM Atomics, para o
// Tizen 4. Aqui nao ha outro fio: o JS roda entre as voltas das fibras, e a
// fibra que pediu cede ate J_EST sair de 0. O protocolo de posse e o MESMO
// (0 aberto, 1 pronto, 4 abandonado, 5 largado); so as operacoes deixam de
// ser atomicas, porque nada roda ao mesmo tempo que o JS. O custo que o #72
// mediu (drawImage + getImageData no fio principal) volta aqui, e sem
// alternativa: o M56 nao tem OffscreenCanvas para levar isto a um Worker.
EM_JS(void, nv_dec_coop, (int32_t *job, int seq, const char *mimeC, int largMax), {
  var pJob = job >> 2;
  var mime = UTF8ToString(mimeC);
  var vivo = function () {
    var e = HEAP32[pJob];
    return HEAP32[pJob + 7] === seq && (e === 0 || e === 4);
  };
  var fim = function (w, h, ow, oh) {
    HEAP32[pJob + 1] = w;
    HEAP32[pJob + 2] = h;
    HEAP32[pJob + 4] = ow;
    HEAP32[pJob + 5] = oh;
    if (HEAP32[pJob] === 0) HEAP32[pJob] = 1;
    else if (HEAP32[pJob] === 4) HEAP32[pJob] = 5;
  };
  HEAP32[pJob + 6] = 0;
  try {
    var bytes = HEAPU8.slice(HEAP32[pJob + 9], HEAP32[pJob + 9] + HEAP32[pJob + 10]);
    createImageBitmap(new Blob([bytes], { type: mime })).then(function (bmp) {
      var ow = bmp.width;
      var oh = bmp.height;
      var w = ow;
      var h = oh;
      if (!vivo()) { if (bmp.close) bmp.close(); return; }
      if (HEAP32[pJob] === 4) { if (bmp.close) bmp.close(); fim(0, 0, 0, 0); return; }
      if (largMax > 0 && ow > largMax) {
        w = largMax;
        h = Math.max(1, Math.round(oh * largMax / ow));
      }
      if (w > 0 && h > 0 && w * h * 4 <= HEAP32[pJob + 8]) {
        var cv = Module.nvDecCanvas || (Module.nvDecCanvas = document.createElement('canvas'));
        cv.width = w;
        cv.height = h;
        var cx = cv.getContext('2d');
        cx.imageSmoothingEnabled = true;
        if ('imageSmoothingQuality' in cx) cx.imageSmoothingQuality = 'high';
        cx.clearRect(0, 0, w, h);
        cx.drawImage(bmp, 0, 0, w, h);
        HEAPU8.set(cx.getImageData(0, 0, w, h).data, HEAP32[pJob + 3]);
      } else { w = 0; h = 0; }
      if (bmp.close) bmp.close();
      fim(w, h, ow, oh);
    }).catch(function () { if (vivo()) fim(0, 0, 0, 0); });
  } catch (e) { fim(0, 0, 0, 0); }
});
#endif

uint8_t *navegador_decodificar(const unsigned char *dados, size_t n, const char *mime,
                               int largMax, int *lw, int *lh, int *ow, int *oh) {
  int32_t *job;
  int codigo;
  int fw, fh, sw, sh, w, h, noWorker, seq;
  size_t cap;
  double limite;
  unsigned char *copia;
  uint8_t *px;
  if (ow) *ow = 0;
  if (oh) *oh = 0;

  // Chamado do fio principal nao da: ele nao pode bloquear em Atomics.wait, e
  // se pudesse seria ele mesmo quem deixaria de rodar o `then`. Hoje o unico
  // chamador e o fio de decode do tex_cache; esta guarda existe para que
  // amanha isto vire NULL em vez de travar o app.
#ifdef NV_COOP
  // No Tizen 4 todo mundo e o "fio principal" do navegador; quem nao pode
  // esperar e o principal DE VERDADE, fora de fibra.
  if (!coop_em_fibra()) return NULL;
#else
  if (emscripten_is_main_browser_thread()) return NULL;
#endif
  varrer();
  if (!dimensoes(dados, n, &fw, &fh)) return NULL;
  // A MESMA CONTA do Worker (Math.round = floor(x + 0.5)); a linha a mais no
  // teto cobre um arredondamento diferente sem abrir espaco para estouro: o
  // Worker so escreve se w*h*4 <= J_CAP.
  sw = fw; sh = fh;
  if (largMax > 0 && fw > largMax) {
    sw = largMax;
    sh = (int)floor((double)fh * largMax / fw + 0.5);
    if (sh < 1) sh = 1;
  }
  cap = (size_t)sw * (size_t)(sh + 1) * 4;
  if (cap > 64u * 1024 * 1024) return NULL;   // 4K cheio e 33 MB; mais que isso nao e arte
  job = (int32_t *)calloc(J_INTS, sizeof(int32_t));
  copia = (unsigned char *)malloc(n);
  px = (uint8_t *)malloc(cap);
  if (!job || !copia || !px) { free(job); free(copia); free(px); return NULL; }
  // COPIA dos bytes: `dados` e do chamador e morre quando esta funcao volta;
  // num abandono o Worker ainda vai le-los. A copia morre com o job.
  memcpy(copia, dados, n);
  seq = __atomic_add_fetch(&seqGlobal, 1, __ATOMIC_RELAXED) & 0x7fffffff;
  if (!seq) seq = __atomic_add_fetch(&seqGlobal, 1, __ATOMIC_RELAXED) & 0x7fffffff;
  job[J_SEQ] = seq;
  job[J_CAP] = (int32_t)cap;
  job[J_DADOS] = (int32_t)(intptr_t)copia;
  job[J_N] = (int32_t)n;
  job[J_PTR] = (int32_t)(intptr_t)px;
  codigo = mimeCodigo(mime);
  job[J_MIME] = codigo;
  job[J_LARG] = largMax;
  if (!__atomic_load_n(&iniciarPedido, __ATOMIC_ACQUIRE)) navegador_iniciar();

#ifdef NV_COOP
  (void)codigo;
  nv_dec_coop(job, seq, mime, largMax);
  if (0) {
#else
  if (codigo && __atomic_load_n(&canalEstado, __ATOMIC_ACQUIRE) == (CANAL_DEC | CANAL_SENT)) {
#endif
    // CANAL DIRETO: empilha e acorda a sentinela. Sem fio principal.
    int32_t topo = __atomic_load_n(&filaCabeca, __ATOMIC_ACQUIRE);
    job[J_ORIGEM] = 2;
    do { job[J_FILA] = topo; }
    while (!__atomic_compare_exchange_n(&filaCabeca, &topo, (int32_t)(intptr_t)job, 1,
                                        __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE));
    emscripten_futex_wake(&filaCabeca, 1);
  }
#ifndef NV_COOP
  else MAIN_THREAD_ASYNC_EM_ASM({
    // UMA DECLARACAO POR LINHA, sem `var a = 1, b = 2`: o bloco do EM_ASM
    // passa pelo pre-processador de C, e la chave nao protege virgula — so
    // parentese protege. Uma virgula solta aqui parte o bloco em dois
    // argumentos de macro e o build morre em "undeclared identifier '$1'".
    var pJob = $0 >> 2;
    var seq = $1;
    var mime = UTF8ToString($2);
    var largMax = $3;
    // So toca no job se ele ainda e ESTE pedido e ainda esta em aberto ou
    // abandonado; um reencaminhamento tardio (onerror) de job ja liberado
    // cai aqui e nao escreve nada.
    var vivo = function () {
      var e = Atomics.load(HEAP32, pJob);
      return HEAP32[pJob + 7] === seq && (e === 0 || e === 4);
    };
    var fim = function (w, h, ow, oh) {
      HEAP32[pJob + 1] = w;
      HEAP32[pJob + 2] = h;
      HEAP32[pJob + 4] = ow;
      HEAP32[pJob + 5] = oh;
      // 0 -> 1 entrega ao C; se o C ja desistiu (4), 4 -> 5 larga o job.
      if (Atomics.compareExchange(HEAP32, pJob, 0, 1) === 4) Atomics.compareExchange(HEAP32, pJob, 4, 5);
      Atomics.notify(HEAP32, pJob);
    };
    // CAMINHO ANTIGO, no fio principal. Fica como reserva: e o que roda
    // quando o Worker nao sobe (sem OffscreenCanvas, arquivo ausente, CSP).
    var noFioPrincipal = function () {
      if (!vivo()) return;
      HEAP32[pJob + 6] = 0;
      // `slice` (e nao `subarray`) de proposito: copia para um ArrayBuffer
      // comum. O Blob nao aceita vista sobre SharedArrayBuffer.
      var bytes = HEAPU8.slice(HEAP32[pJob + 9], HEAP32[pJob + 9] + HEAP32[pJob + 10]);
      try {
        createImageBitmap(new Blob([bytes], { type: mime })).then(function (bmp) {
          var ow = bmp.width;
          var oh = bmp.height;
          var w = ow;
          var h = oh;
          if (!vivo()) { if (bmp.close) bmp.close(); return; }
          if (Atomics.load(HEAP32, pJob) === 4) { if (bmp.close) bmp.close(); fim(0, 0, 0, 0); return; }
          // REDUZ NO CANVAS, nao no heap: o bitmap inteiro vive na memoria do
          // navegador; so o tamanho pedido atravessa para o WASM.
          if (largMax > 0 && ow > largMax) {
            w = largMax;
            h = Math.max(1, Math.round(oh * largMax / ow));
          }
          if (w > 0 && h > 0 && w * h * 4 <= HEAP32[pJob + 8]) {
            var cv = document.createElement('canvas');
            cv.width = w; cv.height = h;
            var cx = cv.getContext('2d');
            cx.imageSmoothingEnabled = true;
            if ('imageSmoothingQuality' in cx) cx.imageSmoothingQuality = 'high';
            cx.drawImage(bmp, 0, 0, w, h);
            HEAPU8.set(cx.getImageData(0, 0, w, h).data, HEAP32[pJob + 3]);
          } else { w = 0; h = 0; }
          if (bmp.close) bmp.close();
          fim(w, h, ow, oh);
        }).catch(function () { if (vivo()) fim(0, 0, 0, 0); });
      } catch (e) { fim(0, 0, 0, 0); }
    };
    // CAMINHO VIA FIO PRINCIPAL PARA O WORKER (#72): so repassa. O Worker e
    // criado em navegador_iniciar; se ainda nao existe (iniciar pedido agora
    // por este mesmo fio), este pedido decodifica aqui mesmo.
    var D = Module.nvDec;
    if (D === undefined) { noFioPrincipal(); return; }
    if (D.morto || !D.w) { noFioPrincipal(); }
    else {
      D.voo[seq] = noFioPrincipal;
      HEAP32[pJob + 6] = 1;
      D.w.postMessage({ job: $0, seq: seq, mime: mime, largMax: largMax });
    }
  }, (int)(intptr_t)job, seq, (int)(intptr_t)mime, (int)largMax);
#endif

  // Prazo em TEMPO DE RELOGIO desde o envio. A versao antiga somava so as
  // fatias que venciam por timeout, e um futex acordado cedo nao contava:
  // o log mostrou pedido de 17 s com "prazo" de 8.
  limite = emscripten_get_now() + NV_NAV_PRAZO_MS;
  for (;;) {
    double resta;
    if (__atomic_load_n(&job[J_EST], __ATOMIC_ACQUIRE) == EST_PRONTO) break;
    resta = limite - emscripten_get_now();
    if (resta <= 0) {
      int32_t esperado = EST_ABERTO;
      if (__atomic_compare_exchange_n(&job[J_EST], &esperado, EST_ABANDONADO, 0,
                                      __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
        // DESISTE SEM LIBERAR: job, copia e pixels ficam com o Worker ate
        // ele largar (estado 5); varrer() devolve tudo depois.
        int origem = job[J_ORIGEM], aguardo;
        pthread_mutex_lock(&lixoMtx);
        job[J_PROX] = (int32_t)(intptr_t)lixo;
        lixo = job;
        aguardo = ++nLixo;
        pthread_mutex_unlock(&lixoMtx);
        // Daqui em diante o job nao e mais deste fio: varrer() de outro fio
        // pode libera-lo assim que o Worker largar. Nada de job[] abaixo.
        nvLog("[webp] navegador nao respondeu em %d ms; pedido %d abandonado (%d no aguardo, %s)",
              NV_NAV_PRAZO_MS, seq, aguardo, origem == 2 ? "canal direto" : "via fio principal");
        return NULL;
      }
      continue;   // perdeu a corrida para o 0 -> 1: o resultado chegou agora
    }
#ifdef NV_COOP
    coop_esperar(&job[J_EST], EST_ABERTO, limite);
#else
    emscripten_futex_wait(&job[J_EST], EST_ABERTO, resta < 250.0 ? resta : 250.0);
#endif
  }

  w  = job[J_W];
  h  = job[J_H];
  if (ow) *ow = job[J_OW];
  if (oh) *oh = job[J_OH];
  noWorker = job[J_ORIGEM];
  free(copia);
  free(job);
  if (w < 1 || h < 1 || (size_t)w * (size_t)h * 4 > cap) { free(px); return NULL; }
  if (!jaContou) {
    jaContou = 1;
    nvLog("[webp] navegador decodificou o primeiro: %dx%d (%s), %s", w, h, mime,
          noWorker == 2 ? "no worker, canal direto" : noWorker ? "no worker" : "no fio principal");
  }
  *lw = w; *lh = h;
  return px;
}

static uint8_t *decodificar(const unsigned char *dados, size_t n, int largMax,
                            int *lw, int *lh, int *ow, int *oh) {
  return navegador_decodificar(dados, n, "image/webp", largMax, lw, lh, ow, oh);
}

static void soltar(uint8_t *px) { free(px); }   // o _malloc do JS e este malloc

#else
// -------------------------------------------------------- webOS e Mac ------
#include <dlfcn.h>

typedef int      (*FnInfo)(const uint8_t *, size_t, int *, int *);
typedef uint8_t *(*FnRgba)(const uint8_t *, size_t, int *, int *);
typedef void     (*FnFree)(void *);

// A API AVANCADA da libwebp (decode.h), so o que a escala precisa. As structs
// sao copiadas do cabecalho porque o alvo LG nao tem os headers no toolchain —
// e a lib entra por dlopen justamente por isso. O layout e estavel desde o ABI
// 0x0200 (os `pad[]` existem para isso), e WebPInitDecoderConfigInternal so
// recusa quando o byte MAIOR da versao difere: 0x0209 casa com a libwebp 0.5
// da TV e com a 1.x do Mac. Se recusar, o caminho antigo (tamanho cheio +
// tex_reduzir) continua valendo — nada quebra, so gasta mais.
//
// POR QUE ESCALAR DENTRO DO DECODER (19/09/2026): os fundos do Xperience
// passaram a vir em 3840x2160 (~180 KB cada, 17 estilos por marca). Em
// WebPDecodeRGBA um deles e 33 MB de RGBA em tamanho cheio ANTES da reducao
// para os 1920 do heroi — na C9 de 2 GB e um pico por pasta focada, e na
// Samsung e um oitavo do heap fixo. Com use_scaling a libwebp reduz linha a
// linha e o bloco que nasce ja tem o tamanho pedido: 8 MB para 1920.
typedef struct { int width, height, has_alpha, has_animation, format; uint32_t pad[5]; } WpFeatures;
typedef struct {
  int colorspace;                // 1 = MODE_RGBA
  int width, height;
  int is_external_memory;
  union {
    struct { uint8_t *rgba; int stride; size_t size; } RGBA;
    struct { uint8_t *y, *u, *v, *a; int y_stride, u_stride, v_stride, a_stride;
             size_t y_size, u_size, v_size, a_size; } YUVA;
  } u;
  uint32_t pad[4];
  uint8_t *private_memory;
} WpBuffer;
typedef struct {
  int bypass_filtering, no_fancy_upsampling, use_cropping;
  int crop_left, crop_top, crop_width, crop_height;
  int use_scaling, scaled_width, scaled_height;
  int use_threads, dithering_strength, flip, alpha_dithering_strength;
  uint32_t pad[5];
} WpOptions;
typedef struct { WpFeatures input; WpBuffer output; WpOptions options; } WpConfig;
#define WP_ABI 0x0209
#define WP_MODE_RGBA 1
typedef int  (*FnCfgInit)(WpConfig *, int);
typedef int  (*FnDecode)(const uint8_t *, size_t, WpConfig *);
typedef void (*FnFreeBuf)(WpBuffer *);

static FnInfo pInfo; static FnRgba pRgba; static FnFree pFree;
static FnCfgInit pCfgInit; static FnDecode pDecode; static FnFreeBuf pFreeBuf;
static int tentado, escalaAvisada;

static void abrir(void) {
  static const char *nomes[] = { "libwebp.so.7", "libwebp.so", "libwebp.7.dylib", "/opt/homebrew/lib/libwebp.7.dylib", NULL };
  void *h = NULL; int i;
  tentado = 1;
  for (i = 0; nomes[i] && !h; i++) h = dlopen(nomes[i], RTLD_NOW);
  if (!h) { printf("[webp] libwebp ausente; .webp nao vai decodificar\n"); return; }
  pInfo = (FnInfo)dlsym(h, "WebPGetInfo");
  pRgba = (FnRgba)dlsym(h, "WebPDecodeRGBA");
  pFree = (FnFree)dlsym(h, "WebPFree");   // ausente em libwebp antiga: free() serve
  if (!pInfo || !pRgba) { printf("[webp] libwebp sem WebPDecodeRGBA\n"); pInfo = NULL; pRgba = NULL; }
  pCfgInit = (FnCfgInit)dlsym(h, "WebPInitDecoderConfigInternal");
  pDecode  = (FnDecode)dlsym(h, "WebPDecode");
  pFreeBuf = (FnFreeBuf)dlsym(h, "WebPFreeDecBuffer");
  if (!pCfgInit || !pDecode || !pFreeBuf) {
    printf("[webp] libwebp sem API avancada: WebP grande decodifica em tamanho cheio\n");
    pCfgInit = NULL;
  }
}

// Decodifica ja reduzido a `lw` de largura. NULL quando a API avancada nao
// existe ou recusou (versao, memoria): o chamador cai no tamanho cheio.
static uint8_t *decodificarEscalado(const unsigned char *dados, size_t n, int w, int h, int lw, int *lh) {
  WpConfig cfg;
  uint8_t *px;
  size_t linha = (size_t)lw * 4, y;
  int alto = (int)(((long)h * lw + w / 2) / w);
  if (alto < 1) alto = 1;
  if (!pCfgInit) return NULL;
  memset(&cfg, 0, sizeof cfg);
  if (!pCfgInit(&cfg, WP_ABI)) {
    if (!escalaAvisada) { escalaAvisada = 1; printf("[webp] libwebp recusou o ABI %#x: sem escala no decoder\n", WP_ABI); }
    pCfgInit = NULL;
    return NULL;
  }
  cfg.options.use_scaling   = 1;
  cfg.options.scaled_width  = lw;
  cfg.options.scaled_height = alto;
  cfg.output.colorspace     = WP_MODE_RGBA;
  if (pDecode(dados, n, &cfg) != 0 || !cfg.output.u.RGBA.rgba) { pFreeBuf(&cfg.output); return NULL; }
  // Copia para um bloco proprio, contiguo, que `soltar` libera com free():
  // o da libwebp tem stride proprio e so WebPFreeDecBuffer sabe solta-lo.
  px = malloc(linha * (size_t)cfg.output.height);
  if (px)
    for (y = 0; y < (size_t)cfg.output.height; y++)
      memcpy(px + y * linha, cfg.output.u.RGBA.rgba + y * (size_t)cfg.output.u.RGBA.stride, linha);
  *lh = cfg.output.height;
  pFreeBuf(&cfg.output);
  return px;
}

// WEBP ANIMADO: SO O PRIMEIRO QUADRO. Avatar de perfil pode vir animado
// (VP8X com ANIM/ANMF), e a libwebp simples — a unica que a LG tem, sem a
// libwebpdemux — responde WebPGetInfo com o tamanho do canvas e depois
// RECUSA o WebPDecodeRGBA (VP8_STATUS_UNSUPPORTED_FEATURE). No log de campo
// (LG 1.4.1-1.4.3) era o "[tex] decode falhou ... magica=52494646" de avatar
// pequeno: RIFF certo, tamanho certo, e cartao vazio no lugar da foto.
//
// O quadro 1 e recortado do ANMF e embrulhado num WebP parado (VP8/VP8L
// direto, ou VP8X+ALPH+VP8 quando o quadro tem alfa separado), que a libwebp
// le. Como o quadro pode ser menor que o canvas (o img2webp corta a borda
// transparente ja no primeiro), ele e colado na posicao dele num canvas
// transparente do tamanho declarado. O navegador do Tizen ja faz isto sozinho
// (createImageBitmap pega o primeiro quadro).
static unsigned le24(const unsigned char *p) { return p[0] | ((unsigned)p[1] << 8) | ((unsigned)p[2] << 16); }
static unsigned le32(const unsigned char *p) { return le24(p) | ((unsigned)p[3] << 24); }
static void pe32(unsigned char *p, unsigned v) { p[0] = v; p[1] = v >> 8; p[2] = v >> 16; p[3] = v >> 24; }
static void pe24(unsigned char *p, unsigned v) { p[0] = v; p[1] = v >> 8; p[2] = v >> 16; }

static uint8_t *primeiroQuadro(const unsigned char *d, size_t n, int *lw, int *lh, int *ow, int *oh) {
  size_t pos = 30, fim;
  unsigned cw, ch;
  if (n < 30 || memcmp(d + 12, "VP8X", 4) || !(d[20] & 0x02)) return NULL;
  cw = le24(d + 24) + 1; ch = le24(d + 27) + 1;
  if ((size_t)cw * ch > 4096u * 4096u) return NULL;
  fim = 8 + (size_t)le32(d + 4);
  if (fim > n) fim = n;   // RIFF que declara mais do que tem: le o que veio
  while (pos + 8 <= fim) {
    size_t tam = le32(d + pos + 4), dado = pos + 8;
    if (tam > fim - dado) return NULL;
    if (!memcmp(d + pos, "ANMF", 4) && tam >= 16 + 8) {
      const unsigned char *q = d + dado;
      unsigned fx = le24(q) * 2, fy = le24(q + 3) * 2;
      unsigned fw = le24(q + 6) + 1, fh = le24(q + 9) + 1;
      const unsigned char *sub = q + 16;
      size_t nsub = tam - 16, i = 0, a = 0, nAlph = 0, img = 0, nImg = 0;
      int ehVp8l = 0, w = 0, h = 0, y;
      unsigned char *parado; size_t np;
      uint8_t *fr, *tela;
      // Subchunks do quadro: ALPH opcional, depois VP8 ou VP8L. Os tamanhos
      // guardados (nAlph, nImg) sao os do campo, sem o byte de preenchimento;
      // o embrulho repoe o preenchimento com zero.
      while (i + 8 <= nsub) {
        size_t t = le32(sub + i + 4);
        if (t > nsub - i - 8) return NULL;
        if (!memcmp(sub + i, "ALPH", 4)) { a = i; nAlph = t; }
        else if (!memcmp(sub + i, "VP8 ", 4) || !memcmp(sub + i, "VP8L", 4)) {
          img = i; nImg = t; ehVp8l = sub[i + 3] == 'L';
          break;
        }
        i += 8 + t + (t & 1);
      }
      if (!nImg) return NULL;
      if (ehVp8l) nAlph = 0;   // VP8L traz o alfa no proprio fluxo
      { size_t pA = nAlph ? 8 + nAlph + (nAlph & 1) : 0, pI = 8 + nImg + (nImg & 1);
        np = 12 + (nAlph ? 18 + pA : 0) + pI;
        parado = calloc(1, np);
        if (!parado) return NULL;
        memcpy(parado, "RIFF", 4); pe32(parado + 4, (unsigned)(np - 8)); memcpy(parado + 8, "WEBP", 4);
        if (nAlph) {
          memcpy(parado + 12, "VP8X", 4); pe32(parado + 16, 10);
          parado[20] = 0x10;   // so o bit de alfa
          pe24(parado + 24, fw - 1); pe24(parado + 27, fh - 1);
          memcpy(parado + 30, sub + a, 8 + nAlph);
          memcpy(parado + 30 + pA, sub + img, 8 + nImg);
        } else memcpy(parado + 12, sub + img, 8 + nImg); }
      fr = pRgba(parado, np, &w, &h);
      free(parado);
      if (!fr) return NULL;
      if (w == (int)cw && h == (int)ch) tela = fr;
      else {
        tela = calloc((size_t)cw * ch, 4);
        if (tela)
          for (y = 0; y < h && fy + (unsigned)y < ch; y++) {
            int cabe = fx >= cw ? 0 : ((unsigned)w > cw - fx ? (int)(cw - fx) : w);
            if (cabe > 0) memcpy(tela + ((size_t)(fy + y) * cw + fx) * 4, fr + (size_t)y * w * 4, (size_t)cabe * 4);
          }
        soltar(fr);
        if (!tela) return NULL;
      }
      *lw = (int)cw; *lh = (int)ch;
      if (ow) *ow = (int)cw;
      if (oh) *oh = (int)ch;
      return tela;
    }
    pos = dado + tam + (tam & 1);
  }
  return NULL;
}

static uint8_t *decodificar(const unsigned char *dados, size_t n, int largMax,
                            int *lw, int *lh, int *ow, int *oh) {
  int w = 0, h = 0; uint8_t *px;
  if (!tentado) abrir();
  if (pRgba && n >= 30 && !memcmp(dados + 12, "VP8X", 4) && (dados[20] & 0x02))
    return primeiroQuadro(dados, n, lw, lh, ow, oh);
  if (!pRgba || !pInfo(dados, n, &w, &h) || w < 1 || h < 1) return NULL;
  if (ow) *ow = w;
  if (oh) *oh = h;
  if (largMax > 0 && w > largMax) {
    px = decodificarEscalado(dados, n, w, h, largMax, lh);
    if (px) { *lw = largMax; return px; }
  }
  px = pRgba(dados, n, &w, &h);
  if (!px) return NULL;
  *lw = w; *lh = h;
  return px;
}

// O bloco da API avancada ja foi copiado para malloc; o do WebPDecodeRGBA e
// da lib. Os dois passam por aqui: WebPFree e free() sao o mesmo alocador na
// libwebp, e e por isso que a copia acima pode usar malloc.
static void soltar(uint8_t *px) { if (pFree) pFree(px); else free(px); }
#endif

SDL_Surface *webp_carregar_larg(const char *caminho, int largMax, int *ow, int *oh) {
  FILE *f; long n; unsigned char *dados; SDL_Surface *s;
  if (ow) *ow = 0;
  if (oh) *oh = 0;
  if (!caminho) return NULL;
  f = fopen(caminho, "rb");
  if (!f) return NULL;
  fseek(f, 0, SEEK_END); n = ftell(f); rewind(f);
  if (n < 16 || n > 32L * 1024 * 1024) { fclose(f); return NULL; }
  dados = malloc((size_t)n);
  if (!dados || fread(dados, 1, (size_t)n, f) != (size_t)n) { free(dados); fclose(f); return NULL; }
  fclose(f);
  s = webp_carregar_larg_mem(dados, (size_t)n, largMax, ow, oh);
  free(dados);
  return s;
}

/* Os bytes do download direto ao decode no LG (ver jpeg_rapido_carregar_mem). */
SDL_Surface *webp_carregar_larg_mem(const unsigned char *dados, size_t n, int largMax,
                                    int *ow, int *oh) {
  int w = 0, h = 0; uint8_t *px; SDL_Surface *s;
  if (ow) *ow = 0;
  if (oh) *oh = 0;
  if (!dados || n < 16 || n > 32L * 1024 * 1024) return NULL;
  if (memcmp(dados, "RIFF", 4) || memcmp(dados + 8, "WEBP", 4)) return NULL;
  px = decodificar(dados, n, largMax, &w, &h, ow, oh);
  if (!px) return NULL;
  // ABGR8888 no SDL = bytes R,G,B,A na memoria em little-endian, que e o que
  // os dois decodificadores entregam. Copia para uma superficie propria: a do
  // SDL_..From apontaria para memoria de fora.
  s = nv_superficie(0, w, h, 32, SDL_PIXELFORMAT_ABGR8888);
  if (s) {
    int y;
    for (y = 0; y < h; y++) memcpy((char *)s->pixels + y * s->pitch, px + (size_t)y * w * 4, (size_t)w * 4);
  }
  soltar(px);
  return s;
}

SDL_Surface *webp_carregar(const char *caminho) {
  return webp_carregar_larg(caminho, 0, NULL, NULL);
}
