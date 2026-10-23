// PROXY DE TS DA LIVE TV. Ver proxyts.h para o porque.
#include "proxyts.h"
#include <string.h>

int proxyts_candidata(const char *url, int mp4) {
  const char *q;
  size_t n;
  if (!url || mp4) return 0;
  if (strncmp(url, "http://", 7) && strncmp(url, "https://", 8)) return 0;
  // A extensao do caminho, sem a query: ".../video.mp4?token=..." e arquivo.
  q = strchr(url, '?');
  n = q ? (size_t)(q - url) : strlen(url);
  if (n >= 4 && (!strncmp(url + n - 4, ".mp4", 4) || !strncmp(url + n - 4, ".mkv", 4))) return 0;
  return 1;
}

#if defined(__EMSCRIPTEN__) || defined(NV_ANDROID)
void proxyts_definir_cabecalhos(const char *c) { (void)c; }
int  proxyts_redirecionou(void) { return 0; }
int  proxyts_disponivel(void) { return 0; }
int  proxyts_url(const char *f, char *s, size_t n) { (void)f; (void)s; (void)n; return 0; }
void proxyts_parar(void) {}
int  proxyts_e_url(const char *u) { (void)u; return 0; }
const char *proxyts_resolver(const char *url, char *buf, size_t n) {
  (void)buf; (void)n;
  return url && !strncmp(url, PROXYTS_PREFIXO, 8) ? url + 8 : url;
}
#else

#include "rede.h"
#include "livetv_regras.h"
// Keep the network proxy independent of SDL headers.
extern int ajustes_livetv_resolucao(void);
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0   // Mac: SO_NOSIGPIPE no socket
#endif

#define PX_SEG_MAX      64        // segmentos lidos de uma playlist
#define PX_SEG_TETO     (48L << 20)  // um segmento nunca passa disto
#define PX_PONTA        3         // comeca 3 segmentos antes do fim (como os players)
#define PX_SEM_NOVO_S   40        // sem segmento novo por isto: a fonte morreu
#define PX_FALHAS_MAX   6         // pedidos de playlist seguidos que falharam

static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
static int sockOuvir = -1, porta;
static char fonteAtual[4096];
// Cabecalhos do addon: os pendentes (proxima sessao) e os da sessao em curso.
static char cabsPendentes[512], cabsAtual[512];
static atomic_uint sessaoAtual, sessaoRedirecionada;
// O download do segmento em curso, para a troca de canal cortar na hora (a
// conta de 1 tela recusaria o canal novo com o velho ainda baixando).
static volatile int *cancelarEmCurso;

// --- playlist -----------------------------------------------------------------
typedef struct {
  int  alvo;            // EXT-X-TARGETDURATION (s)
  long seq0;            // EXT-X-MEDIA-SEQUENCE
  int  n;
  char uri[PX_SEG_MAX][1024];
  int  mestre;          // e master playlist: uri[0] e a variante escolhida
  int  fim;             // EXT-X-ENDLIST
  // O que o proxy NAO converte (#283): segmento cifrado (EXT-X-KEY com METHOD
  // diferente de NONE) e segmento fMP4 (EXT-X-MAP). Os dois chegariam ao uMS
  // como lixo; o certo e encerrar e deixar o app tentar a fonte direto.
  int  cifrado, fmp4;
} PxLista;

// "Nome: valor" por linha -> vetor terminado em NULL para rede.h (ate 7).
static const char *const *pxVetor(char *copia, const char **v) {
  char *l, *ctx = NULL;
  int k = 0;
  for (l = strtok_r(copia, "\n", &ctx); l && k < 7; l = strtok_r(NULL, "\n", &ctx))
    if (*l) v[k++] = l;
  v[k] = NULL;
  return k ? v : NULL;
}

static void pxJuntar(const char *base, const char *rel, char *dst, size_t n) {
  size_t semQuery = strcspn(base, "?"), barra = 0, i;
  if (!strncmp(rel, "http://", 7) || !strncmp(rel, "https://", 8)) { snprintf(dst, n, "%s", rel); return; }
  if (rel[0] == '/') {
    const char *h = strstr(base, "://"), *b = h ? strchr(h + 3, '/') : NULL;
    snprintf(dst, n, "%.*s%s", b ? (int)(b - base) : (int)strlen(base), base, rel);
    return;
  }
  for (i = 0; i < semQuery; i++) if (base[i] == '/') barra = i;
  snprintf(dst, n, "%.*s/%s", (int)barra, base, rel);
}

// Parse a media/master playlist. Automatic uses the highest variant; an
// explicit resolution uses the nearest height. Re-read settings on each open.
static int pxLer(const char *texto, const char *base, PxLista *l) {
  const char *p = texto;
  int proximaEhVariante = 0, altura = 0, melhorAltura = -1;
  int alvo = nv_res_opcao_altura(ajustes_livetv_resolucao()), melhorDif = 1 << 30;
  long banda = 0, melhorBanda = -1;
  memset(l, 0, sizeof *l);
  l->alvo = 6;
  if (!texto || strncmp(texto, "#EXTM3U", 7)) return 0;
  while (p && *p) {
    size_t k = strcspn(p, "\r\n");
    char linha[1024];
    snprintf(linha, sizeof linha, "%.*s", (int)(k < sizeof linha ? k : sizeof linha - 1), p);
    if (!strncmp(linha, "#EXT-X-TARGETDURATION:", 22)) l->alvo = atoi(linha + 22);
    else if (!strncmp(linha, "#EXT-X-MEDIA-SEQUENCE:", 22)) l->seq0 = atol(linha + 22);
    else if (!strncmp(linha, "#EXT-X-ENDLIST", 14)) l->fim = 1;
    else if (!strncmp(linha, "#EXT-X-KEY:", 11) && !strstr(linha, "METHOD=NONE")) l->cifrado = 1;
    else if (!strncmp(linha, "#EXT-X-MAP:", 11)) l->fmp4 = 1;
    else if (!strncmp(linha, "#EXT-X-STREAM-INF", 17)) {
      const char *r = strstr(linha, "RESOLUTION="), *b = strstr(linha, "BANDWIDTH=");
      int largura;
      altura = 0; banda = b ? atol(b + 10) : 0;
      if (r) sscanf(r + 11, "%dx%d", &largura, &altura);
      proximaEhVariante = 1;
    }
    else if (linha[0] && linha[0] != '#') {
      if (proximaEhVariante) {
        int dif = abs(altura - alvo);
        if (!l->mestre || (alvo ? dif < melhorDif :
            altura > melhorAltura || (altura == melhorAltura && banda > melhorBanda))) {
          melhorAltura = altura; melhorBanda = banda; melhorDif = dif;
          l->mestre = 1; pxJuntar(base, linha, l->uri[0], sizeof l->uri[0]); l->n = 1;
        }
        proximaEhVariante = 0;
      } else if (!l->mestre && l->n < PX_SEG_MAX) {
        pxJuntar(base, linha, l->uri[l->n], sizeof l->uri[0]);
        l->n++;
      }
    }
    p += k;
    while (*p == '\r' || *p == '\n') p++;
  }
  if (l->alvo < 1) l->alvo = 1;
  if (l->alvo > 15) l->alvo = 15;
  return l->n > 0;
}

// --- MPEG-TS ------------------------------------------------------------------
// Guarda o PAT e o PMT do segmento (o ultimo visto) para repor no comeco de um
// segmento que nao comece por eles.
typedef struct { unsigned char pat[188], pmt[188]; int temPat, temPmt, pmtPid; } PxTabelas;

static int pxPid(const unsigned char *k) { return ((k[1] & 0x1F) << 8) | k[2]; }

static void pxColher(const unsigned char *b, long n, PxTabelas *t) {
  long i;
  for (i = 0; i + 188 <= n; i += 188) {
    const unsigned char *k = b + i;
    int pid;
    if (k[0] != 0x47) break;
    pid = pxPid(k);
    if (pid == 0 && (k[1] & 0x40)) {
      int af = (k[3] >> 4) & 3, off = 4 + (af == 3 ? 1 + k[4] : 0), p, sl, j;
      if (off >= 187) continue;
      p = off + 1 + k[off];
      if (p + 12 > 188 || k[p] != 0) continue;
      sl = ((k[p + 1] & 0x0F) << 8) | k[p + 2];
      for (j = p + 8; j + 4 <= p + 3 + sl - 4 && j + 4 <= 188; j += 4)
        if ((k[j] << 8 | k[j + 1]) != 0) { t->pmtPid = ((k[j + 2] & 0x1F) << 8) | k[j + 3]; break; }
      memcpy(t->pat, k, 188); t->temPat = 1;
    } else if (t->pmtPid && pid == t->pmtPid && (k[1] & 0x40)) {
      memcpy(t->pmt, k, 188); t->temPmt = 1;
    }
    if (t->temPat && t->temPmt && i > 188 * 64) break;
  }
}

// Onde comecam os pacotes alinhados (pode haver lixo antes); -1 = nao e TS.
static long pxInicio(const unsigned char *b, long n) {
  long i;
  for (i = 0; i < 188 && i + 376 < n; i++)
    if (b[i] == 0x47 && b[i + 188] == 0x47 && b[i + 376] == 0x47) return i;
  return -1;
}

// --- envio ----------------------------------------------------------------------
static int pxEnviar(int s, const void *b, size_t n) {
  const char *p = b;
  while (n) {
    ssize_t r = send(s, p, n, MSG_NOSIGNAL);
    if (r < 0 && errno == EINTR) continue;
    if (r <= 0) return 0;
    p += r; n -= (size_t)r;
  }
  return 1;
}

typedef struct PxFluxo PxFluxo;
typedef struct PxConexao {
  int s; unsigned sessao; char fonte[4096]; volatile int cancelar;
  PxFluxo *fluxo;
  struct PxConexao *proxima;
  uint64_t pos;
  int proteger;                    // cursor entregue, protegido por fluxo->trava
} PxConexao;

// O uMS faz mais de um GET durante o mesmo load (sonda e leitura). Uma
// segunda conexao nao pode fechar a primeira: isso virava endOfStream depois
// de 10..20s na C4. Uma ingestao HLS alimenta todos os leitores locais.
#define PX_BUFFER ((16U << 20) / 188 * 188)
#define PX_BLOCO (188U * 256)
struct PxFluxo {
  pthread_mutex_t trava;
  pthread_cond_t chegou;
  unsigned char *buffer;
  uint64_t inicio, fim;
  uint64_t segmentos[64];
  unsigned nSegmentos;
  PxTabelas tabelas;
  unsigned refs;                    // protegido pela trava global
  int encerrado, leitores;
  PxConexao *clientes;
  time_t semLeitorDesde;
  char lista[4096];
  char cabs[512];                   // os do addon, para playlist e segmentos
  PxConexao produtor;
};
static PxFluxo *fluxoAtual;
static uint64_t pxCursorInicio(const PxFluxo *f);

static void pxFluxoDestruir(PxFluxo *f) {
  pthread_cond_destroy(&f->chegou);
  pthread_mutex_destroy(&f->trava);
  free(f->buffer); free(f);
}

static void pxFluxoSoltar(PxFluxo *f) {
  int destruir;
  pthread_mutex_lock(&trava);
  destruir = --f->refs == 0;
  pthread_mutex_unlock(&trava);
  if (destruir) pxFluxoDestruir(f);
}

// Chamado com trava global; produtor/leitores conservam suas referencias.
static void pxFluxoParar(void) {
  PxFluxo *f = fluxoAtual;
  if (!f) return;
  fluxoAtual = NULL;
  __atomic_store_n(&f->produtor.cancelar, 1, __ATOMIC_RELEASE);
  pthread_mutex_lock(&f->trava);
  f->encerrado = 1; pthread_cond_broadcast(&f->chegou);
  pthread_mutex_unlock(&f->trava);
  if (--f->refs == 0) pxFluxoDestruir(f);
}

static int pxViva(const PxConexao *c) {
  if (__atomic_load_n(&c->cancelar, __ATOMIC_ACQUIRE) || atomic_load(&sessaoAtual) != c->sessao) return 0;
  if (c->s < 0 && c->fluxo) {
    int ocioso;
    pthread_mutex_lock(&c->fluxo->trava);
    ocioso = c->fluxo->semLeitorDesde && time(NULL) - c->fluxo->semLeitorDesde >= 3;
    pthread_mutex_unlock(&c->fluxo->trava);
    if (ocioso) return 0;
  }
  return 1;
}

static void pxDormir(const PxConexao *c, int ms) {
  while (ms > 0 && pxViva(c)) { usleep(100 * 1000); ms -= 100; }
}

// A playlist (de midia) ao vivo, com a variante resolvida. `final` recebe o
// endereco depois dos redirecionamentos — e a base dos segmentos.
//
// COM CABECALHOS DO ADDON (#283) o pedido sai por rede_baixar_bin_medido_
// controle, que os aceita mas nao diz o endereco final: a base dos segmentos
// e o endereco pedido. O redirecionamento da FONTE ja foi resolvido uma vez em
// pxAtender (rede_url_final_cab), e e esse endereco que chega aqui.
static int pxPlaylist(const char *url, const char *cabs, PxLista *l, char *final, size_t nf) {
  char atual[4096];
  int volta;
  snprintf(atual, sizeof atual, "%s", url);
  for (volta = 0; volta < 3; volta++) {
    long n = 0;
    int st = 0, er = 0;
    char fin[4096];
    char *b;
    fin[0] = 0;
    if (cabs && *cabs) {
      char copia[512];
      const char *v[8];
      RedeMedida md;
      RedeControle ctl;
      memset(&md, 0, sizeof md);
      ctl.max_bytes = 1048576; ctl.cancelado = NULL;
      snprintf(copia, sizeof copia, "%s", cabs);
      b = rede_baixar_bin_medido_controle(atual, 8, pxVetor(copia, v), &ctl, &n, &md);
      if (b && (md.status < 200 || md.status >= 300)) { free(b); b = NULL; }
    } else
      b = rede_baixar_trecho_st(atual, 8, 0, 1048575, &n, &st, &er, fin, sizeof fin);
    if (!b || n <= 0) { free(b); return 0; }
    if (fin[0]) snprintf(atual, sizeof atual, "%s", fin);
    if (!pxLer(b, atual, l)) { free(b); return 0; }
    free(b);
    if (!l->mestre) { snprintf(final, nf, "%s", atual); return 1; }
    snprintf(atual, sizeof atual, "%s", l->uri[0]);
  }
  return 0;
}

static void pxInicioSegmento(PxFluxo *f) {
  pthread_mutex_lock(&f->trava);
  f->segmentos[f->nSegmentos++ % 64] = f->fim;
  pthread_mutex_unlock(&f->trava);
}

static int pxEntregar(PxConexao *c, const void *b, size_t n) {
  PxFluxo *f = c->fluxo;
  const unsigned char *p = b;
  while (n) {
    size_t guardar = n < PX_BLOCO ? n : PX_BLOCO, off, primeira;
    uint64_t limite;
    struct timespec prazo;
    pthread_mutex_lock(&f->trava);
    limite = f->fim + guardar > PX_BUFFER ? f->fim + guardar - PX_BUFFER : 0;
    clock_gettime(CLOCK_REALTIME, &prazo); prazo.tv_nsec += 250000000;
    if (prazo.tv_nsec >= 1000000000) { prazo.tv_nsec -= 1000000000; prazo.tv_sec++; }
    for (;;) {
      uint64_t maisAdiantado = 0;
      int ativos = 0, r;
      PxConexao *leitor;
      for (leitor = f->clientes; leitor; leitor = leitor->proxima)
        if (leitor->proteger) {
          ativos++;
          if (leitor->pos > maisAdiantado) maisAdiantado = leitor->pos;
        }
      if (!ativos || maisAdiantado >= limite || f->encerrado) break;
      // Destravar permite ao leitor ativo consumir o comeco de um segmento
      // maior que o ring. Uma sonda parada nao prende o fluxo indefinidamente.
      r = pthread_cond_timedwait(&f->chegou, &f->trava, &prazo);
      if (r == ETIMEDOUT) {
        for (leitor = f->clientes; leitor; leitor = leitor->proxima)
          if (leitor->pos < limite) leitor->proteger = 0;
        break;
      }
    }
    if (f->encerrado) { pthread_mutex_unlock(&f->trava); return 0; }
    off = (size_t)(f->fim % PX_BUFFER);
    primeira = guardar < PX_BUFFER - off ? guardar : PX_BUFFER - off;
    memcpy(f->buffer + off, p, primeira);
    memcpy(f->buffer, p + primeira, guardar - primeira);
    f->fim += guardar;
    if (f->fim - f->inicio > PX_BUFFER) f->inicio = f->fim - PX_BUFFER;
    pthread_cond_broadcast(&f->chegou);
    pthread_mutex_unlock(&f->trava);
    p += guardar; n -= guardar;
  }
  return 1;
}

static void pxHls(PxConexao *c, const char *urlLista, const char *cabs) {
  PxLista *lp = malloc(sizeof *lp);
  PxTabelas tab;
  char base[4096];
  long ultimo = -1, segs = 0, bytes = 0;
  int falhas = 0;
  time_t desdeNovo = time(NULL), t0 = time(NULL);
  memset(&tab, 0, sizeof tab);
  if (!lp) return;
#define l (*lp)
  while (pxViva(c)) {
    int i, novos = 0;
    if (!pxPlaylist(urlLista, cabs, &l, base, sizeof base)) {
      if (++falhas >= PX_FALHAS_MAX) { printf("[proxy-ts] playlist falhou %d vezes: fim\n", falhas); break; }
      pxDormir(c, 1000);
      continue;
    }
    falhas = 0;
    if (l.cifrado || l.fmp4) {
      printf("[proxy-ts] HLS %s: o proxy nao converte, fim (o app tenta a fonte direto)\n",
             l.cifrado ? "cifrado (EXT-X-KEY)" : "fMP4 (EXT-X-MAP)");
      fflush(stdout);
      break;
    }
    // A PRIMEIRA LEITURA comeca perto da ponta; playlist que voltou atras
    // (painel reiniciou o canal) recomeca da ponta tambem.
    if (ultimo < 0 || l.seq0 + l.n - 1 < ultimo) {
      long ini = l.seq0 + (l.n > PX_PONTA ? l.n - PX_PONTA : 0);
      if (ultimo >= 0) printf("[proxy-ts] a playlist voltou (seq %ld < %ld): recomecando da ponta\n",
                              l.seq0 + l.n - 1, ultimo);
      else printf("[proxy-ts] HLS: %d segmento(s), alvo %d s, comecando no %ld\n", l.n, l.alvo,
                  ini - l.seq0);
      fflush(stdout);
      ultimo = ini - 1;
    }
    for (i = 0; i < l.n && pxViva(c); i++) {
      long seq = l.seq0 + i, n = 0, ini;
      RedeMedida md;
      RedeControle ctl;
      unsigned char *b;
      if (seq <= ultimo) continue;
      memset(&md, 0, sizeof md);
      ctl.max_bytes = PX_SEG_TETO;
      ctl.cancelado = &c->cancelar;
      pthread_mutex_lock(&trava); cancelarEmCurso = &c->cancelar; pthread_mutex_unlock(&trava);
      { char copia[512];
        const char *v[8];
        snprintf(copia, sizeof copia, "%s", cabs ? cabs : "");
        b = (unsigned char *)rede_baixar_bin_medido_controle(l.uri[i], 15, pxVetor(copia, v), &ctl,
                                                             &n, &md); }
      pthread_mutex_lock(&trava); if (cancelarEmCurso == &c->cancelar) cancelarEmCurso = NULL; pthread_mutex_unlock(&trava);
      if (!pxViva(c)) { free(b); break; }
      ultimo = seq;
      if (!b || n < 188 || (ini = pxInicio(b, n)) < 0) {
        printf("[proxy-ts] segmento %ld sem TS (HTTP %d, %ld B): pulado\n", seq, md.status, n);
        fflush(stdout);
        free(b);
        continue;
      }
      // PAT/PMT ANTES DO PRIMEIRO PACOTE do segmento (o "203 AV Type Not
      // Founded" do segmento unico na C9): se o segmento nao abre com o PAT,
      // repoe o ultimo PAT/PMT visto antes dele.
      pxColher(b + ini, n - ini, &tab);
      pthread_mutex_lock(&c->fluxo->trava);
      c->fluxo->tabelas = tab;
      pthread_mutex_unlock(&c->fluxo->trava);
      pxInicioSegmento(c->fluxo);
      if (pxPid(b + ini) != 0 && tab.temPat && tab.temPmt) {
        if (!pxEntregar(c, tab.pat, 188) || !pxEntregar(c, tab.pmt, 188)) { free(b); goto fim; }
      }
      if (!pxEntregar(c, b + ini, (size_t)((n - ini) / 188 * 188))) { free(b); goto fim; }
      segs++; bytes += n - ini; novos++;
      if (segs <= 3 || segs % 30 == 0) {
        printf("[proxy-ts] segmento %ld: %ld KB em %lu ms (%s)\n", segs, (n - ini) / 1024, md.ms,
               pxPid(b + ini) == 0 ? "abre com PAT" : tab.temPat ? "PAT/PMT reposto" : "sem PAT");
        fflush(stdout);
      }
      free(b);
      desdeNovo = time(NULL);
    }
    if (l.fim) { printf("[proxy-ts] playlist com ENDLIST: fim\n"); break; }
    if (!novos && time(NULL) - desdeNovo > PX_SEM_NOVO_S) {
      printf("[proxy-ts] %d s sem segmento novo: fim\n", PX_SEM_NOVO_S);
      break;
    }
    // REFRESH: metade do alvo quando nada veio (o segmento seguinte esta para
    // sair), o alvo inteiro depois de entregar.
    pxDormir(c, (novos ? l.alvo * 1000 : l.alvo * 500));
  }
fim:
#undef l
  free(lp);
  printf("[proxy-ts] sessao %u encerrada: %ld segmento(s), %ld KB em %ld s\n", c->sessao, segs,
         bytes / 1024, (long)(time(NULL) - t0));
  fflush(stdout);
}

static void *pxProduzir(void *u) {
  PxFluxo *f = u;
  pxHls(&f->produtor, f->lista, f->cabs);
  pthread_mutex_lock(&f->trava);
  f->encerrado = 1; pthread_cond_broadcast(&f->chegou);
  pthread_mutex_unlock(&f->trava);
  pxFluxoSoltar(f);
  return NULL;
}

static PxFluxo *pxFluxoPegar(PxConexao *c, const char *lista, const char *cabs) {
  PxFluxo *f;
  pthread_t t;
  pthread_mutex_lock(&trava);
  if (!pxViva(c)) { pthread_mutex_unlock(&trava); return NULL; }
  f = fluxoAtual;
  if (f) {
    int encerrado;
    pthread_mutex_lock(&f->trava); encerrado = f->encerrado; pthread_mutex_unlock(&f->trava);
    if (encerrado) { pxFluxoParar(); f = NULL; }
  }
  if (!f && lista) {
    f = calloc(1, sizeof *f);
    if (f) f->buffer = malloc(PX_BUFFER);
    if (!f || !f->buffer) { if (f) free(f); pthread_mutex_unlock(&trava); return NULL; }
    if (pthread_mutex_init(&f->trava, NULL) != 0) {
      free(f->buffer); free(f); pthread_mutex_unlock(&trava); return NULL;
    }
    if (pthread_cond_init(&f->chegou, NULL) != 0) {
      pthread_mutex_destroy(&f->trava); free(f->buffer); free(f);
      pthread_mutex_unlock(&trava); return NULL;
    }
    f->refs = 2; // sessao e produtor
    f->produtor.s = -1; f->produtor.sessao = c->sessao; f->produtor.fluxo = f;
    snprintf(f->lista, sizeof f->lista, "%s", lista);
    snprintf(f->cabs, sizeof f->cabs, "%s", cabs ? cabs : "");
    fluxoAtual = f;
    if (pthread_create(&t, NULL, pxProduzir, f) != 0) {
      fluxoAtual = NULL; pxFluxoDestruir(f); pthread_mutex_unlock(&trava); return NULL;
    }
    pthread_detach(t);
  }
  if (f) {
    f->refs++;
    pthread_mutex_lock(&f->trava);
    f->leitores++; f->semLeitorDesde = 0;
    c->pos = pxCursorInicio(f); c->proteger = 1;
    c->proxima = f->clientes; f->clientes = c;
    pthread_mutex_unlock(&f->trava);
  }
  pthread_mutex_unlock(&trava);
  return f;
}

static uint64_t pxCursorInicio(const PxFluxo *f) {
  uint64_t ini = f->fim;
  unsigned n = f->nSegmentos < 64 ? f->nSegmentos : 64;
  for (unsigned i = 0; i < n; i++)
    if (f->segmentos[i] >= f->inicio && f->segmentos[i] < ini) ini = f->segmentos[i];
  // Um segmento pode exceder o buffer. Ainda ha TS alinhado disponivel;
  // o leitor repoe PAT/PMT e o decoder reencontra o proximo quadro chave.
  return ini == f->fim ? f->inicio : ini;
}

static void pxServirFluxo(PxConexao *c) {
  const char cab[] = "HTTP/1.1 200 OK\r\nContent-Type: video/mp2t\r\n"
    "Cache-Control: no-cache\r\nConnection: close\r\n\r\n";
  PxFluxo *f = c->fluxo;
  unsigned char *b = malloc(PX_BLOCO);
  uint64_t pos;
  int reporTabelas = 1;
  if (c->s < 0) { free(b); return; }
  if (!b || !pxEnviar(c->s, cab, sizeof cab - 1)) { free(b); return; }
  pthread_mutex_lock(&f->trava); pos = c->pos; pthread_mutex_unlock(&f->trava);
  while (pxViva(c)) {
    size_t n, off, primeira;
    PxTabelas tab = {0};
    pthread_mutex_lock(&f->trava);
    if (pos >= f->fim && !f->encerrado) {
      struct timespec ts;
      clock_gettime(CLOCK_REALTIME, &ts); ts.tv_sec++;
      pthread_cond_timedwait(&f->chegou, &f->trava, &ts);
    }
    if (pos < f->inicio) { pos = pxCursorInicio(f); reporTabelas = 1; }
    if (pos >= f->fim) {
      int fim = f->encerrado;
      unsigned char byte;
      ssize_t r;
      pthread_mutex_unlock(&f->trava);
      if (fim) break;
      // Detecta a sonda fechada mesmo sem segmento novo, sem conservar um
      // leitor fantasma e a ingestao apos o uMS sair.
      r = recv(c->s, &byte, 1, MSG_PEEK | MSG_DONTWAIT);
      if (r == 0 || (r < 0 && errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK)) break;
      continue;
    }
    n = f->fim - pos > PX_BLOCO ? PX_BLOCO : (size_t)(f->fim - pos);
    off = (size_t)(pos % PX_BUFFER);
    primeira = n < PX_BUFFER - off ? n : PX_BUFFER - off;
    memcpy(b, f->buffer + off, primeira);
    memcpy(b + primeira, f->buffer, n - primeira);
    if (reporTabelas) { tab = f->tabelas; reporTabelas = 0; }
    pos += n;
    pthread_mutex_unlock(&f->trava);
    if (tab.temPat && tab.temPmt &&
        (!pxEnviar(c->s, tab.pat, 188) || !pxEnviar(c->s, tab.pmt, 188))) break;
    if (!pxEnviar(c->s, b, n)) break;
    pthread_mutex_lock(&f->trava);
    c->pos = pos; c->proteger = 1;
    pthread_cond_broadcast(&f->chegou);
    pthread_mutex_unlock(&f->trava);
  }
  free(b);
}

static void *pxAtender(void *u) {
  PxConexao *c = u;
  char req[4096], cab[512], cabs[512];
  ssize_t r, tot = 0;
  unsigned s = 0;
  const char *q;
  // O PEDIDO: so a primeira linha interessa (GET /live.ts?s=N).
  while (tot < (ssize_t)sizeof req - 1) {
    r = recv(c->s, req + tot, sizeof req - 1 - (size_t)tot, 0);
    if (r <= 0) break;
    tot += r; req[tot] = 0;
    if (strstr(req, "\r\n\r\n") || strstr(req, "\n\n")) break;
  }
  req[tot > 0 ? tot : 0] = 0;
  q = strstr(req, "?s=");
  if (q) s = (unsigned)strtoul(q + 3, NULL, 10);
  pthread_mutex_lock(&trava);
  if (s && s == atomic_load(&sessaoAtual)) snprintf(c->fonte, sizeof c->fonte, "%s", fonteAtual);
  else c->fonte[0] = 0;
  snprintf(cabs, sizeof cabs, "%s", c->fonte[0] ? cabsAtual : "");
  pthread_mutex_unlock(&trava);
  c->sessao = s;
  if (!c->fonte[0] || strncmp(req, "GET ", 4)) {
    static const char nao[] = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
    pxEnviar(c->s, nao, sizeof nao - 1);
    goto sair;
  }
  { const char *p = req, *range = "absent";
    while ((p = strchr(p, '\n')) != NULL) {
      p++;
      if (!strncasecmp(p, "Range:", 6)) {
        const char *v = p + 6;
        while (*v == ' ' || *v == '\t') v++;
        range = !strncmp(v, "bytes=0-", 8) ? "inicio" : "outro";
        break;
      }
    }
    // A URI e os demais cabecalhos podem conter credenciais. Somente a
    // categoria do Range ajuda a confirmar as sondas do uMS no aparelho.
    printf("[proxy-ts] sessao %u: GET (Range %s)\n", c->sessao, range);
    fflush(stdout); }
  // Uma conexao da mesma sessao usa a ingestao que ja esta em andamento.
  c->fluxo = pxFluxoPegar(c, NULL, NULL);
  if (c->fluxo) {
    printf("[proxy-ts] sessao %u: GET compartilha a ingestao HLS\n", c->sessao);
    fflush(stdout); pxServirFluxo(c); goto sair;
  }
  // O QUE A FONTE E: um trecho do comeco decide entre TS continuo e playlist.
  { long n = 0;
    int st = 0, er = 0;
    char fin[4096];
    char *b;
    fin[0] = 0;
    if (cabs[0]) {
      // Com os cabecalhos do addon (#283): sem eles o CDN recusa, e a sonda
      // concluiria "nao e playlist" e mandaria o uMS direto — que e o caminho
      // que nao toca. O endereco final sai de um pedido a parte, de 64 bytes.
      char copia[512];
      const char *v[8];
      RedeMedida md;
      RedeControle ctl;
      memset(&md, 0, sizeof md);
      ctl.max_bytes = 65536; ctl.cancelado = NULL;
      snprintf(copia, sizeof copia, "%s", cabs);
      b = rede_baixar_bin_medido_controle(c->fonte, 8, pxVetor(copia, v), &ctl, &n, &md);
      st = md.status;
      if (b && n >= 7 && !strncmp(b, "#EXTM3U", 7) && st >= 200 && st < 300) {
        snprintf(copia, sizeof copia, "%s", cabs);
        if (!rede_url_final_cab(c->fonte, 8, pxVetor(copia, v), fin, sizeof fin, NULL)) fin[0] = 0;
      }
    } else
      b = rede_baixar_trecho_st(c->fonte, 8, 0, 65535, &n, &st, &er, fin, sizeof fin);
    if (b && n >= 7 && !strncmp(b, "#EXTM3U", 7) && (!st || (st >= 200 && st < 300))) {
      free(b);
      c->fluxo = pxFluxoPegar(c, fin[0] ? fin : c->fonte, cabs);
      if (c->fluxo) {
        printf("[proxy-ts] sessao %u: a fonte e playlist HLS; entregando TS continuo\n", c->sessao);
        fflush(stdout); pxServirFluxo(c);
      }
    } else {
      // TS CONTINUO (ou qualquer outra coisa): o player vai direto a fonte,
      // como antes do proxy. O 302 nao custa nada ao caminho que ja tocava.
      int ts = b && n > 376 && pxInicio((unsigned char *)b, n) >= 0;
      free(b);
      printf("[proxy-ts] sessao %u: a fonte %s (HTTP %d, %ld B): redirecionando o player\n",
             c->sessao, ts ? "ja e TS continuo" : "nao e playlist", st, n);
      atomic_store(&sessaoRedirecionada, c->sessao);
      fflush(stdout);
      snprintf(cab, sizeof cab, "HTTP/1.1 302 Found\r\nLocation: %s\r\nContent-Length: 0\r\n"
               "Connection: close\r\n\r\n", c->fonte);
      pxEnviar(c->s, cab, strlen(cab));
    }
  }
sair:
  if (c->fluxo) {
    PxConexao **p;
    pthread_mutex_lock(&c->fluxo->trava);
    for (p = &c->fluxo->clientes; *p && *p != c; p = &(*p)->proxima) {}
    if (*p) *p = c->proxima;
    if (--c->fluxo->leitores == 0) c->fluxo->semLeitorDesde = time(NULL);
    pthread_cond_broadcast(&c->fluxo->chegou);
    pthread_mutex_unlock(&c->fluxo->trava);
    pxFluxoSoltar(c->fluxo);
  }
  close(c->s);
  free(c);
  return NULL;
}

static void *pxOuvir(void *u) {
  (void)u;
  for (;;) {
    int s = accept(sockOuvir, NULL, NULL);
    PxConexao *c;
    pthread_t t;
    if (s < 0) { if (errno == EINTR) continue; usleep(200 * 1000); continue; }
#ifdef SO_NOSIGPIPE
    { int um = 1; setsockopt(s, SOL_SOCKET, SO_NOSIGPIPE, &um, sizeof um); }
#endif
    { struct timeval tv = { 10, 0 };
      setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
      setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof tv); }
    c = calloc(1, sizeof *c);
    if (!c) { close(s); continue; }
    c->s = s;
    if (pthread_create(&t, NULL, pxAtender, c) == 0) pthread_detach(t);
    else { close(s); free(c); }
  }
  return NULL;
}

static int pxSubir(void) {
  struct sockaddr_in a;
  socklen_t al = sizeof a;
  pthread_t t;
  int um = 1;
  if (sockOuvir >= 0) return 1;
  signal(SIGPIPE, SIG_IGN);
  sockOuvir = socket(AF_INET, SOCK_STREAM, 0);
  if (sockOuvir < 0) return 0;
  setsockopt(sockOuvir, SOL_SOCKET, SO_REUSEADDR, &um, sizeof um);
  memset(&a, 0, sizeof a);
  a.sin_family = AF_INET;
  a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  a.sin_port = 0;   // porta livre
  if (bind(sockOuvir, (struct sockaddr *)&a, sizeof a) < 0 || listen(sockOuvir, 8) < 0 ||
      getsockname(sockOuvir, (struct sockaddr *)&a, &al) < 0) {
    close(sockOuvir); sockOuvir = -1; return 0;
  }
  porta = ntohs(a.sin_port);
  if (pthread_create(&t, NULL, pxOuvir, NULL) != 0) { close(sockOuvir); sockOuvir = -1; return 0; }
  pthread_detach(t);
  printf("[proxy-ts] ouvindo em 127.0.0.1:%d\n", porta);
  fflush(stdout);
  return 1;
}

int proxyts_disponivel(void) { return 1; }

void proxyts_definir_cabecalhos(const char *cabs) {
  pthread_mutex_lock(&trava);
  snprintf(cabsPendentes, sizeof cabsPendentes, "%s", cabs ? cabs : "");
  pthread_mutex_unlock(&trava);
}

int proxyts_redirecionou(void) {
  unsigned s = atomic_load(&sessaoAtual);
  return s && atomic_load(&sessaoRedirecionada) == s;
}

int proxyts_url(const char *fonte, char *saida, size_t n) {
  unsigned s;
  if (!fonte || !fonte[0] || !saida || n < 40) return 0;
  pthread_mutex_lock(&trava);
  if (!pxSubir()) { pthread_mutex_unlock(&trava); return 0; }
  if (cancelarEmCurso) __atomic_store_n(cancelarEmCurso, 1, __ATOMIC_RELEASE);
  pxFluxoParar();
  snprintf(fonteAtual, sizeof fonteAtual, "%s", fonte);
  snprintf(cabsAtual, sizeof cabsAtual, "%s", cabsPendentes);
  s = atomic_fetch_add(&sessaoAtual, 1) + 1;
  pthread_mutex_unlock(&trava);
  snprintf(saida, n, "http://127.0.0.1:%d/live.ts?s=%u", porta, s);
  return 1;
}

void proxyts_parar(void) {
  pthread_mutex_lock(&trava);
  if (cancelarEmCurso) __atomic_store_n(cancelarEmCurso, 1, __ATOMIC_RELEASE);
  pxFluxoParar();
  atomic_fetch_add(&sessaoAtual, 1);
  fonteAtual[0] = 0;
  cabsAtual[0] = 0;
  pthread_mutex_unlock(&trava);
}

int proxyts_e_url(const char *url) {
  return url && !strncmp(url, "http://127.0.0.1:", 17) && strstr(url, "/live.ts?s=");
}

const char *proxyts_resolver(const char *url, char *buf, size_t n) {
  if (url && !strncmp(url, PROXYTS_PREFIXO, 8)) {
    if (proxyts_url(url + 8, buf, n)) return buf;
    return url + 8;
  }
  if (atomic_load(&sessaoAtual) && fonteAtual[0]) proxyts_parar();
  return url;
}

#endif
