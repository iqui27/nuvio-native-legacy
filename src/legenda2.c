// Second subtitle session. See legenda2.h for the contract.
#include "legenda2.h"
#include "rede.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t sinal = PTHREAD_COND_INITIALIZER;
static pthread_t fio;
static int fioCriado, encerrar;

// Owned by the UI thread, read by the worker under `trava`.
static uint64_t sessao = 1;
static unsigned selecao;
static Leg2Estado estado;
static char ident[128];
static LegendaDocumento *doc;
static int offManual, offAuto;
static Leg2SalvarAtraso salvarAtraso;
static unsigned atrasoVinculo;

typedef struct {
  int pendente;
  char url[1400], ident[128], idioma[24], origem[96];
  uint64_t sessao;
  unsigned selecao;
} Pedido;
static Pedido pedido;

static int baixarPadrao(const char *url, long maxBytes, unsigned prazoMs,
                        int (*parar)(void *), void *u, char **corpo, long *n) {
  RedePedido p;
  RedeResposta r;
  int ok;
  memset(&p, 0, sizeof p);
  p.url = url; p.seguir = 1; p.prazo_ms = prazoMs;
  p.max_bytes = (size_t)maxBytes; p.parar = parar; p.parar_usuario = u;
  ok = rede_pedir(&p, &r);
  if (!ok || r.erro != REDE_OK || r.status < 200 || r.status > 299 || !r.corpo) {
    printf("[legenda2] download failed: http=%d err=%d curl=%d\n", r.status, (int)r.erro, r.curl_erro);
    fflush(stdout);
    rede_resposta_limpar(&r);
    return -1;
  }
  *corpo = r.corpo; *n = (long)r.n_corpo; r.corpo = NULL;
  rede_resposta_limpar(&r);
  return 0;
}
static Leg2Baixador baixador = baixarPadrao;
void legenda2_definir_baixador(Leg2Baixador b) {
  pthread_mutex_lock(&trava);
  baixador = b ? b : baixarPadrao;
  pthread_mutex_unlock(&trava);
}

typedef struct { uint64_t sessao; unsigned selecao; } Dono;
static int donoAtual(const Dono *d) {
  return !encerrar && d->sessao == sessao && d->selecao == selecao;
}
static int pararDownload(void *u) {
  int r;
  pthread_mutex_lock(&trava);
  r = !donoAtual((const Dono *)u);
  pthread_mutex_unlock(&trava);
  return r;
}

static unsigned agoraMs(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (unsigned)(ts.tv_sec * 1000u + ts.tv_nsec / 1000000u);
}

static const char *nomeEstado(Leg2Estado e) {
  return e == LEG2_ATIVA ? "active" : e == LEG2_FALHOU ? "failed"
       : e == LEG2_NAO_SUPORTADA ? "unsupported" : e == LEG2_CARREGANDO ? "loading" : "none";
}

static void *trabalhar(void *u) {
  (void)u;
  for (;;) {
    Pedido p;
    Dono d;
    Leg2Baixador b;
    char *bruto = NULL, *texto = NULL;
    long n = 0;
    int rc, nCues = 0;
    unsigned t0;
    Leg2Estado fim = LEG2_FALHOU;
    LegendaDocumento *novo = NULL, *velho = NULL;
    pthread_mutex_lock(&trava);
    while (!encerrar && !pedido.pendente) pthread_cond_wait(&sinal, &trava);
    if (encerrar) { pthread_mutex_unlock(&trava); return NULL; }
    p = pedido; pedido.pendente = 0;
    b = baixador;
    pthread_mutex_unlock(&trava);
    d.sessao = p.sessao; d.selecao = p.selecao;
    t0 = agoraMs();
    rc = b(p.url, LEG2_MAX_BYTES, LEG2_PRAZO_MS, pararDownload, &d, &bruto, &n);
    if (!rc && bruto && n > LEG2_MAX_BYTES) rc = -1;   // a downloader that ignored the cap
    if (!rc && bruto && !pararDownload(&d)) {
      texto = legenda_utf8_idioma(bruto, n, p.idioma, p.url, NULL);
      if (texto && legenda_eh_ass(texto)) fim = LEG2_NAO_SUPORTADA;
      else if (texto) {
        LegendaDocumentoInfo info;
        memset(&info, 0, sizeof info);
        snprintf(info.idioma, sizeof info.idioma, "%s", p.idioma);
        snprintf(info.origem, sizeof info.origem, "%s", p.origem);
        snprintf(info.identidade, sizeof info.identidade, "%s", p.ident);
        info.sessao = p.sessao;
        info.flags = LEGENDA_DOC_COMPLETO;
        novo = legenda_documento_criar(texto, &info);
        if (novo) legenda_documento_dados(novo, &nCues);
        if (novo && nCues > 0) fim = LEG2_ATIVA;
        else { legenda_documento_liberar(novo); novo = NULL; }
      }
    }
    free(bruto); free(texto);
    pthread_mutex_lock(&trava);
    if (donoAtual(&d)) {
      if (novo) { velho = doc; doc = novo; novo = NULL; }
      estado = fim;
      // English, no URL, no title, no file name.
      printf("[legenda2] second subtitle %s: bytes=%ld cues=%d ms=%u\n",
             nomeEstado(fim), n, nCues, agoraMs() - t0);
    } else {
      printf("[legenda2] stale result dropped (session/selection changed)\n");
    }
    fflush(stdout);
    pthread_mutex_unlock(&trava);
    legenda_documento_liberar(novo);
    legenda_documento_liberar(velho);
  }
}

static void garantirFio(void) {
  // Caller holds `trava`.
  if (fioCriado || encerrar) return;
  if (pthread_create(&fio, NULL, trabalhar, NULL) == 0) fioCriado = 1;
}

// Drops the selection/document. Caller holds `trava`; returns the document to
// release outside the lock.
static LegendaDocumento *limpar(void) {
  LegendaDocumento *d = doc;
  doc = NULL;
  selecao++;
  pedido.pendente = 0;
  estado = LEG2_NENHUMA;
  ident[0] = 0;
  offAuto = 0;
  return d;
}

void legenda2_reiniciar(void) {
  LegendaDocumento *d;
  pthread_mutex_lock(&trava);
  d = limpar();
  sessao++;
  if (!salvarAtraso) offManual = 0;   // the unbound loader keeps its original session contract
  pthread_mutex_unlock(&trava);
  legenda_documento_liberar(d);
}

uint64_t legenda2_sessao(void) {
  uint64_t s;
  pthread_mutex_lock(&trava); s = sessao; pthread_mutex_unlock(&trava);
  return s;
}

unsigned legenda2_escolher(const char *identidade, const char *url,
                           const char *idioma, const char *origem) {
  LegendaDocumento *d;
  unsigned s;
  pthread_mutex_lock(&trava);
  d = limpar();
  if (url && *url && identidade && *identidade) {
    snprintf(ident, sizeof ident, "%s", identidade);
    estado = LEG2_CARREGANDO;
    snprintf(pedido.url, sizeof pedido.url, "%s", url);
    snprintf(pedido.ident, sizeof pedido.ident, "%s", identidade);
    snprintf(pedido.idioma, sizeof pedido.idioma, "%s", idioma ? idioma : "");
    snprintf(pedido.origem, sizeof pedido.origem, "%s", origem ? origem : "");
    pedido.sessao = sessao;
    pedido.selecao = selecao;
    pedido.pendente = 1;
    garantirFio();
    if (!fioCriado) estado = LEG2_FALHOU;
    pthread_cond_signal(&sinal);
  }
  s = selecao;
  pthread_mutex_unlock(&trava);
  legenda_documento_liberar(d);
  return s;
}

void legenda2_desligar(void) {
  LegendaDocumento *d;
  pthread_mutex_lock(&trava);
  d = limpar();
  pthread_mutex_unlock(&trava);
  legenda_documento_liberar(d);
}

Leg2Estado legenda2_estado(void) {
  Leg2Estado e;
  pthread_mutex_lock(&trava); e = estado; pthread_mutex_unlock(&trava);
  return e;
}

// Read on the UI thread only; written on the UI thread only. The worker never
// touches `ident`, so no copy is needed for the caller.
const char *legenda2_identidade(void) { return ident; }

static int limitarManual(int ms) {
  return ms > LEG_ATRASO_MANUAL_MAX ? LEG_ATRASO_MANUAL_MAX : ms < -LEG_ATRASO_MANUAL_MAX ? -LEG_ATRASO_MANUAL_MAX : ms;
}
static int limitar(int ms) { return ms > 30000 ? 30000 : ms < -30000 ? -30000 : ms; }
void legenda2_vincular_atraso(int ms, Leg2SalvarAtraso salvar) {
  pthread_mutex_lock(&trava);
  offManual = limitarManual(ms);
  salvarAtraso = salvar;
  atrasoVinculo++;
  pthread_mutex_unlock(&trava);
}
void legenda2_atualizar_offset_manual(int ms) {
  pthread_mutex_lock(&trava); offManual = limitarManual(ms); pthread_mutex_unlock(&trava);
}
int legenda2_offset_manual(void) {
  int v;
  pthread_mutex_lock(&trava); v = offManual; pthread_mutex_unlock(&trava);
  return v;
}
void legenda2_definir_offset_manual(int ms) {
  Leg2SalvarAtraso salvar;
  unsigned vinculo;
  ms = limitarManual(ms);
  pthread_mutex_lock(&trava); salvar = salvarAtraso; vinculo = atrasoVinculo; pthread_mutex_unlock(&trava);
  // Settings may write to disk and publish the committed value back to us.
  // Neither filesystem work nor callbacks run while the subtitle lock is held.
  if (salvar && !salvar(ms)) return;
  pthread_mutex_lock(&trava);
  if (vinculo == atrasoVinculo) offManual = ms;
  pthread_mutex_unlock(&trava);
}
void legenda2_definir_offset_auto(int ms) {
  pthread_mutex_lock(&trava); offAuto = limitar(ms); pthread_mutex_unlock(&trava);
}
int legenda2_offset_total(void) {
  int v;
  pthread_mutex_lock(&trava); v = limitarManual(offManual + offAuto); pthread_mutex_unlock(&trava);
  return v;
}

LegendaDocumento *legenda2_documento(void) {
  LegendaDocumento *d = NULL;
  pthread_mutex_lock(&trava);
  if (doc && estado == LEG2_ATIVA) d = legenda_documento_reter(doc);
  pthread_mutex_unlock(&trava);
  return d;
}

static double (*volatile tempoMapa)(double);
void legenda2_definir_tempo(double (*tempo)(double)) { tempoMapa = tempo; }
int legenda2_cues(double posSeg, LegendaCue *dst, int max) {
  LegendaDocumento *d = NULL;
  int off = 0, n;
  double (*f)(double) = tempoMapa;
  if (f) posSeg = f(posSeg);   // fora da trava: legsync tem a sua
  pthread_mutex_lock(&trava);
  if (doc && estado == LEG2_ATIVA) { d = legenda_documento_reter(doc); off = limitarManual(offAuto - offManual); }
  pthread_mutex_unlock(&trava);
  if (!d) return 0;
  // The offset is applied HERE and only here (legenda_documento_cues adds it
  // to the lookup time: positive = earlier). offManual is the user's delay
  // (positive = later), so it enters subtracted; offAuto is already lookup.
  n = legenda_documento_cues(d, posSeg, off, dst, max);
  legenda_documento_liberar(d);
  return n;
}

void legenda2_encerrar(void) {
  LegendaDocumento *d;
  int juntar;
  pthread_mutex_lock(&trava);
  encerrar = 1;
  d = limpar();
  juntar = fioCriado; fioCriado = 0;
  pthread_cond_broadcast(&sinal);
  pthread_mutex_unlock(&trava);
  if (juntar) pthread_join(fio, NULL);
  legenda_documento_liberar(d);
  pthread_mutex_lock(&trava);
  encerrar = 0;   // a later session may start again (tests)
  pthread_mutex_unlock(&trava);
}
