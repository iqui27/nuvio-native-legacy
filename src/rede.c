#include "rede.h"
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <dlfcn.h>

// Ver rede.h. Fica ANTES da guarda de alvo porque os dois caminhos de rede
// imprimem URL, e debrid.c tambem usa.
const char *rede_url_publica(const char *url, char *dst, unsigned tam) {
  const char *e, *h;
  unsigned n;
  if (!dst || tam == 0) return "";
  dst[0] = 0;
  if (!url || !*url) return dst;
  e = strstr(url, "://");
  if (!e) { snprintf(dst, tam, "%.*s", (int)tam - 1, url); return dst; }
  h = e + 3;
  while (*h && *h != '/' && *h != '?' && *h != '#') h++;
  n = (unsigned)(h - url);
  if (n >= tam) n = tam - 1;
  memcpy(dst, url, n);
  dst[n] = 0;
  // O "/..." avisa que havia caminho: sem ele, um log com host nu parece um
  // pedido a raiz do servidor, que e uma leitura errada.
  if (*h && n + 4 < tam) { memcpy(dst + n, "/...", 4); dst[n + 4] = 0; }
  return dst;
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
                        char *urlFinal, int urlFinalTam), {
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

  var s = xhr.responseText || "";
  var n = s.length;
  var p = _malloc(n + 1);
  if (!p) return 0;
  for (var i = 0; i < n; i++) HEAPU8[p + i] = s.charCodeAt(i) & 0xff;
  HEAPU8[p + n] = 0;
  if (tam) HEAP32[tam >> 2] = n;
  return p;
});

long rede_teto = 0;

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

static char *pedir(const char *metodo, const char *url, const char *const *cab,
                   const char *extraCab, const char *corpo,
                   long *tam, int *status) {
  char *cabs, *corpoResp;
  int n = 0, http = 0;
  if (status) *status = 0;
  if (!url || !*url) return NULL;
  cabs = juntarCabs(cab, extraCab);
  corpoResp = nv_http(metodo, url, cabs, corpo, &n, &http, NULL, 0);
  free(cabs);
  if (status) *status = http;
  if (!corpoResp) { char seg[120];
    printf("[rede] falhou em %s\n", rede_url_publica(url, seg, sizeof seg));
    return NULL; }

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
  corpo = nv_http("GET", url, cabs, NULL, &n, &http, dst, (int)tam);
  free(cabs);
  free(corpo);
  return dst[0] ? 1 : 0;
}

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

static void *(*curl_init)(void);
static int   (*curl_setopt)(void *, int, ...);
static int   (*curl_perform)(void *);
static void  (*curl_cleanup)(void *);
static int   (*curl_global)(long);
static void *(*slist_append)(void *, const char *);
static void  (*slist_free)(void *);
static int   (*curl_getinfo)(void *, int, ...);
static int    pronto;

typedef struct { char *p; size_t n; } Balde;

static char *rede_baixar_interno(const char *url, int segundos, long *tam,
                                 const char *const *cab);
static char *rede_baixar_interno2(const char *url, int segundos, long *tam,
                                  const char *const *cab, int *status);

// Teto opcional de bytes para a proxima transferencia; 0 = sem teto. Existe
// porque servidor que IGNORA o cabecalho Range responde 200 com o arquivo
// inteiro, e nesse caso o cabecalho pedido nao limita nada.
long rede_teto = 0;

static size_t receber(void *dados, size_t tam, size_t qtd, void *u) {
  Balde *b = (Balde *)u;
  size_t bytes = tam * qtd;
  char *novo;
  if (rede_teto > 0 && b->n >= (size_t)rede_teto) return 0;   // corta a conexao
  if (rede_teto > 0 && b->n + bytes > (size_t)rede_teto)
    bytes = (size_t)rede_teto - b->n;
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
  if (pronto) return pronto > 0;
  pthread_mutex_lock(&abrirTrava);
  if (pronto) { r = pronto > 0; pthread_mutex_unlock(&abrirTrava); return r; }
  pronto = -1;
  h = dlopen("libcurl.so.5", RTLD_NOW);
  if (!h) h = dlopen("libcurl.so.4", RTLD_NOW);
  if (!h) h = dlopen("libcurl.4.dylib", RTLD_NOW);   // Mac
  if (!h) h = dlopen("libcurl.dylib", RTLD_NOW);
  if (!h) { printf("[rede] sem libcurl: %s\n", dlerror());
            pthread_mutex_unlock(&abrirTrava); return 0; }
  *(void **)(&curl_init)    = dlsym(h, "curl_easy_init");
  *(void **)(&curl_setopt)  = dlsym(h, "curl_easy_setopt");
  *(void **)(&curl_perform) = dlsym(h, "curl_easy_perform");
  *(void **)(&curl_cleanup) = dlsym(h, "curl_easy_cleanup");
  *(void **)(&curl_global)  = dlsym(h, "curl_global_init");
  *(void **)(&slist_append) = dlsym(h, "curl_slist_append");
  *(void **)(&slist_free)   = dlsym(h, "curl_slist_free_all");
  *(void **)(&curl_getinfo) = dlsym(h, "curl_easy_getinfo");
  if (!curl_init || !curl_setopt || !curl_perform) {
    printf("[rede] libcurl sem os simbolos esperados\n");
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
  return rede_baixar_interno2(url, segundos, NULL, cab, status);
}

static char *rede_baixar_interno(const char *url, int segundos, long *tam,
                                 const char *const *cab) {
  return rede_baixar_interno2(url, segundos, tam, cab, NULL);
}

static char *rede_baixar_interno2(const char *url, int segundos, long *tam,
                                  const char *const *cab, int *status) {
  Balde b = { NULL, 0 };
  void *c, *lista = NULL;
  int r;
  if (status) *status = 0;
  if (!url || !*url || !abrir()) return NULL;
  c = curl_init();
  if (!c) return NULL;
  curl_setopt(c, OPT_URL, url);
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
    if (!r && http >= 400 && !status) {
      curl_cleanup(c);
      if (lista && slist_free) slist_free(lista);
      free(b.p);
      { char seg[120];
        printf("[rede] HTTP %ld em %s\n", http, rede_url_publica(url, seg, sizeof seg)); }
      fflush(stdout);
      return NULL;
    } }
  curl_cleanup(c);
  if (lista && slist_free) slist_free(lista);
  // 23 = CURLE_WRITE_ERROR. Quando ha teto, ele e o resultado ESPERADO: o
  // recebedor devolve menos bytes de proposito para cortar a conexao assim que
  // enche. Nesse caso o que ja veio e exatamente o que se queria — tratar como
  // falha jogaria fora o cabecalho inteiro que acabamos de baixar.
  if (r == 23 && rede_teto > 0 && b.n > 0) r = 0;
  if (r != 0) { char seg[120]; free(b.p);
    printf("[rede] falha %d em %s\n", r, rede_url_publica(url, seg, sizeof seg));
    return NULL; }
  if (tam) *tam = (long)b.n;
  return b.p;
}

int rede_url_final(const char *url, int segundos, char *dst, unsigned tam) {
  Balde b = { NULL, 0 };
  void *c;
  char *fim = NULL;
  int r;
  if (!url || !*url || !abrir() || !curl_getinfo) return 0;
  c = curl_init();
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
  curl_cleanup(c);
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
  c = curl_init();
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
  if (status && curl_getinfo) { long h = 0; curl_getinfo(c, INFO_RESPONSE_CODE, &h); *status = (int)h; }
  if (lista && slist_free) slist_free(lista);
  curl_cleanup(c);
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
  c = curl_init();
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
  if (status && !r && curl_getinfo) {
    long codigo = 0;
    curl_getinfo(c, INFO_RESPONSE_CODE, &codigo);
    *status = (int)codigo;
  }
  curl_cleanup(c);
  if (lista && slist_free) slist_free(lista);
  // Falha de TRANSPORTE (r != 0) continua sendo NULL — ai nao houve resposta
  // nenhuma. O corpo de um 4xx, ao contrario, e devolvido: e nele que o
  // PostgREST explica o que faltou.
  if (r != 0) { free(b.p); return NULL; }
  return b.p ? b.p : strdup("");
}

#endif  /* __EMSCRIPTEN__ */
