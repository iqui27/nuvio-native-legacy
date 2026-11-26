// TorBox burn-in on LG and Android (tbqueima.h): Samsung's controller from
// video_tpk.c, with TorBox's own audio and lean logs.
#include "tbqueima.h"
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>

int tbq_codec_ass(const char *c) {
  if (!c) return 0;
  return !strncmp(c, "S_TEXT/ASS", 10) || !strncmp(c, "S_TEXT/SSA", 10) || !strcmp(c, "S_ASS") || !strcmp(c, "S_SSA");
}
int tbq_hdr_texto(const char *h) {
  return h && h[0] && strcasecmp(h, "none") && strcasecmp(h, "sdr");
}

#ifdef NV_TBQ
#include <pthread.h>
#include <time.h>
#include "tbburn.h"
#include "tbrelogio.h"
#include "dts/tbaudio_tpk.h"
#include "ajustes.h"
#include "linguas.h"
#include "mkvass.h"
#include "legref.h"
#include "rede.h"
#include "streams.h"

#ifdef NV_TBQ_TEST
unsigned tbq_teste_ms;
static unsigned msAgora(void) { return tbq_teste_ms; }
#else
static unsigned msAgora(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return (unsigned)((unsigned long long)t.tv_sec * 1000u + (unsigned long long)t.tv_nsec / 1000000u);
}
#endif
static int venceu(unsigned desde, unsigned ms) { return msAgora() - desde >= ms; }

#ifndef TBQ_LENTO_MS
#define TBQ_LENTO_MS 10000u    // the "slow to start" notice (as the .tpk, owner 24/11/2026)
#endif
#ifndef TBQ_SEM_INICIO_MS
#define TBQ_SEM_INICIO_MS 30000u   // no picture at all: a player error
#endif
#ifndef TBQ_PEDIDO_MS
#define TBQ_PEDIDO_MS 30000u   // no answer from TorBox
#endif
#ifdef NV_ANDROID
#define TBQ_ALVO "android"
#else
#define TBQ_ALVO "lg"
#endif

static struct {
  int ativo, pend, desistiu, voltar, decidido;
  int leg, sub, audio;          // the burned menu entry, TorBox's subtitle/audio indices
  unsigned pediuEm, abriuEm, esperaDesde;
  double alvo, durOrig;
  int tocouLog, registrando, aviso, lento, tirarMain, origMarcado, hdrArq;
  char url[4096];               // TorBox's playlist (a secret: never logged)
} tb;
static TbqVista vis;
// The file's tracks while burned (the player lists TorBox's).
static VideoFaixa listaA[NV_FAIXA_MAX], listaL[NV_FAIXA_MAX];
static int nListaA, nListaL, audioLista;

int tbq_ativo(void) { return tb.ativo; }
const char *tbq_url_player(void) { return tb.ativo && tb.url[0] ? tb.url : NULL; }
int tbq_n_audio(void) { return nListaA; }
int tbq_n_legenda(void) { return nListaL; }
const VideoFaixa *tbq_audio(int i) { return i >= 0 && i < nListaA ? &listaA[i] : NULL; }
const VideoFaixa *tbq_legenda(int i) { return i >= 0 && i < nListaL ? &listaL[i] : NULL; }
int tbq_audio_atual(void) { return audioLista; }

static int legAss(int i) {
  const VideoFaixa *f = video_legenda(i);
  return f && tbq_codec_ass(f->codec) && f->ordinalMkv >= 0;
}
static int temAss(void) {
  int i, n = video_n_legenda();
  for (i = 0; i < n; i++) if (legAss(i)) return 1;
  return 0;
}
// The file's audio track TorBox is asked for: its place in the list (the
// players list the file's audio tracks in container order).
static int idxAudio(void) {
  int a = video_audio_atual();
  return a >= 0 && a < video_n_audio() ? a : 0;
}
static void faixaDe(int i, TbFaixa *t) {
  const VideoFaixa *f = video_legenda(i);
  memset(t, 0, sizeof *t);
  if (!f) return;
  snprintf(t->idioma, sizeof t->idioma, "%s", f->idioma);
  { const char *nm = f->nome[0] ? f->nome : f->rotulo;
    size_t k = strlen(nm) < sizeof t->nome - 1 ? strlen(nm) : sizeof t->nome - 1;
    memcpy(t->nome, nm, k); t->nome[k] = 0; }
  t->tipo = f->tipoLeg ? f->tipoLeg : f->letreiro ? LING_LEG_LETREIROS : LING_LEG_COMUM;
  t->letreiro = f->letreiro;
}
static int hdr(void) { return tbburn_hdr_da_fonte() || tb.hdrArq; }
// The automatic choice: remembered for the show (or Off), else Signs & Songs,
// else the dialogue ASS track. Menu index or -1.
static int autoFaixa(int usarMemoria, const char **porque) {
  static TbFaixa f[NV_FAIXA_MAX];
  static int idx[NV_FAIXA_MAX];
  TbFaixa m;
  int n = 0, i, mem = 0, k, nl = video_n_legenda();
  memset(&m, 0, sizeof m);
  for (i = 0; i < nl && n < NV_FAIXA_MAX; i++)
    if (legAss(i)) { faixaDe(i, &f[n]); idx[n++] = i; }
  if (usarMemoria && hdr() && !tbburn_hdr_permitido()) {
    if (porque) *porque = "HDR: Off unless turned On for this show";
    return -1;
  }
  if (usarMemoria) mem = tbburn_memoria(&m);
  k = tbburn_escolher_auto(f, n, mem, &m, ling_legenda());
  if (porque) *porque = !n ? "no ASS track" : mem == 1 ? "Off remembered for this show" : mem == 2 && k >= 0 ? "remembered for this show" :
                        k >= 0 ? "Signs & Songs, else the dialogue track" : "none";
  return k >= 0 ? idx[k] : -1;
}

// ---- the subtitle clock when the player reads TorBox itself (tbrelogio.h) ---
// As video_tpk.c with TorBox audio: TorBox's playlist once per burned stream,
// then the head of segment 0 and of the segment the player lands in (256 KB
// each, one at a time). Stops after 3 failures per stream.
static pthread_mutex_t tbcTrava = PTHREAD_MUTEX_INITIALIZER;
static int tbcRodando, tbcFalhas;
static unsigned tbcGer, tbcPlGer, tbcUltimo;
static TbaPl tbcPl;
typedef struct { unsigned char *b; size_t n, max; } TbcAcum;
static int tbcAcumular(const void *d, size_t n, void *u) {
  TbcAcum *a = u;
  size_t k = a->n + n > a->max ? a->max - a->n : n;
  if (k) { memcpy(a->b + a->n, d, k); a->n += k; }
  return a->n < a->max;
}
static unsigned char *tbcBaixar(const char *url, size_t max, int cabeca, size_t *n, char *final, size_t fn, int *st) {
  RedePedido p; RedeResposta r; TbcAcum a;
  char faixa[64];
  const char *cabs[2] = { NULL, NULL };
  memset(&p, 0, sizeof p); memset(&r, 0, sizeof r);
  a.b = malloc(max + 1); a.n = 0; a.max = max;
  if (!a.b) return NULL;
  p.url = url; p.prazo_ms = 15000; p.seguir = 1; p.rebaixar_midia = 1;
  p.max_bytes = REDE_CORPO_MAXIMO;
  if (cabeca) { snprintf(faixa, sizeof faixa, "Range: bytes=0-%zu", max - 1); cabs[0] = faixa; p.cabecalhos = cabs; }
  p.receber_corpo = tbcAcumular; p.corpo_usuario = &a;
  rede_pedir(&p, &r);
  *st = r.status;
  if (final && fn) snprintf(final, fn, "%s", r.final[0] ? r.final : url);
  rede_resposta_limpar(&r);
  if (*st < 200 || *st >= 300 || !a.n) { free(a.b); return NULL; }
  a.b[a.n] = 0; *n = a.n;
  return a.b;
}
static void tbcFalhou(const char *o, int st, int definitivo) {
  int k;
  pthread_mutex_lock(&tbcTrava);
  k = tbcFalhas = definitivo ? 3 : tbcFalhas + 1;
  pthread_mutex_unlock(&tbcTrava);
  printf("[tbburn] subtitle clock: %s (HTTP %d)%s\n", o, st, k >= 3 ? "; no correction for this stream" : "");
  fflush(stdout);
}
static void *tbcFio(void *u) {
  char *url = u, *final = malloc(4096);
  unsigned g, gg;
  int seg, st = 0;
  size_t n = 0;
  pthread_mutex_lock(&tbcTrava); g = tbcGer; pthread_mutex_unlock(&tbcTrava);
  if (!final) goto fim;
  if (tbcPlGer != g) {
    TbaPl pl;
    unsigned char *b = tbcBaixar(url, 4u * 1024u * 1024u, 0, &n, final, 4096, &st);
    memset(&pl, 0, sizeof pl);
    if (!b || !tba_pl_ler((char *)b, final, &pl)) { free(b); tbcFalhou("TorBox's playlist did not load", st, 0); goto fim; }
    free(b);
    if (pl.master) {
      char var[4096];
      snprintf(var, sizeof var, "%s", pl.variante);
      tba_pl_liberar(&pl); memset(&pl, 0, sizeof pl);
      b = tbcBaixar(var, 4u * 1024u * 1024u, 0, &n, final, 4096, &st);
      if (!b || !tba_pl_ler((char *)b, final, &pl) || pl.master || !pl.nseg) {
        free(b); tba_pl_liberar(&pl); tbcFalhou("TorBox's media playlist did not load", st, 0); goto fim;
      }
      free(b);
    }
    if (pl.mapa[0]) { tba_pl_liberar(&pl); tbcFalhou("TorBox's segments are fMP4, not read", 200, 1); goto fim; }
    { double *d = malloc(sizeof *d * (size_t)(pl.nseg > 0 ? pl.nseg : 1));
      int i;
      if (d) { for (i = 0; i < pl.nseg; i++) d[i] = pl.seg[i].dur; tbr_segmentos(g, d, pl.nseg); free(d); } }
    pthread_mutex_lock(&tbcTrava);
    tba_pl_liberar(&tbcPl); tbcPl = pl; tbcPlGer = g;
    pthread_mutex_unlock(&tbcTrava);
  }
  while (tbr_falta(&gg, &seg) && gg == g) {
    char *uri = NULL;
    unsigned char *h;
    double pts = 0;
    pthread_mutex_lock(&tbcTrava);
    if (tbcPlGer == g && seg >= 0 && seg < tbcPl.nseg) uri = strdup(tbcPl.seg[seg].uri);
    pthread_mutex_unlock(&tbcTrava);
    if (!uri) break;
    h = tbcBaixar(uri, 256u * 1024u, 1, &n, NULL, 0, &st);
    free(uri);
    if (!h || !tba_ts_primeiro_pts(h, n, &pts)) { free(h); tbcFalhou("a segment head gave no video PTS", st, 0); break; }
    free(h);
    tbr_pts(g, seg, pts);
  }
fim:
  free(final); free(url);
  rede_fio_fechar();
  pthread_mutex_lock(&tbcTrava); tbcRodando = 0; tbcUltimo = msAgora(); pthread_mutex_unlock(&tbcTrava);
  return NULL;
}
static void tbcBombear(void) {
  unsigned g = tbr_geracao(), gg;
  int seg, precisa;
  pthread_t f;
  char *u;
  if (!g || !tb.ativo || !tb.url[0]) return;
  pthread_mutex_lock(&tbcTrava);
  if (g != tbcGer) { tbcGer = g; tbcFalhas = 0; }
  precisa = tbcPlGer != g || tbr_falta(&gg, &seg);
  if (tbcRodando || tbcFalhas >= 3 || !precisa || msAgora() - tbcUltimo < 1000u) { pthread_mutex_unlock(&tbcTrava); return; }
  tbcRodando = 1;
  pthread_mutex_unlock(&tbcTrava);
  u = strdup(tb.url);
  if (u && pthread_create(&f, NULL, tbcFio, u) == 0) { pthread_detach(f); return; }
  free(u);
  pthread_mutex_lock(&tbcTrava); tbcRodando = 0; pthread_mutex_unlock(&tbcTrava);
}

// ---- the switches ------------------------------------------------------------
static void lentoFim(const char *porque) {
  if (!tb.lento) return;
  tb.lento = 0; tb.aviso = 4;
  printf("[tbburn] slow start notice down: %s\n", porque);
  fflush(stdout);
}
// TorBox's burned stream `m` opens at the player's point. The menus keep the
// file's tracks from here on.
static void abrirQueimado(const char *m) {
  if (!tb.ativo) {
    int i;
    nListaA = video_n_audio(); if (nListaA > NV_FAIXA_MAX) nListaA = NV_FAIXA_MAX;
    nListaL = video_n_legenda(); if (nListaL > NV_FAIXA_MAX) nListaL = NV_FAIXA_MAX;
    for (i = 0; i < nListaA; i++) listaA[i] = *video_audio(i);
    for (i = 0; i < nListaL; i++) listaL[i] = *video_legenda(i);
  }
  audioLista = tb.audio >= 0 && tb.audio < nListaA ? tb.audio : 0;
  // Another burned stream (an audio pick) starts at the picture's time.
  tb.alvo = vis.pos + (tb.ativo ? tbr_correcao() : 0.0);
  if (tb.alvo < 0) tb.alvo = 0;
  snprintf(tb.url, sizeof tb.url, "%s", m);
  tbr_comecar();
  tb.ativo = 1; tb.abriuEm = msAgora(); tb.esperaDesde = tb.abriuEm ? tb.abriuEm : 1u; tb.tocouLog = 0;
  tb.tirarMain = 1;   // owner 22/11/2026: Nuvio stops drawing the track TorBox burns
  lentoFim("a new burned stream");
  printf("[tbburn] " TBQ_ALVO ": opening TorBox's burned stream at %.1f s (%u ms after the request; TorBox audio); "
         "menus keep the file's %d audio and %d subtitle(s)\n",
         tb.alvo, msAgora() - tb.pediuEm, nListaA, nListaL);
  fflush(stdout);
  tbr_ancorar(tb.alvo);
  tbq_host_reabrir(tb.alvo, -1);
}
static void desligar(const char *porque, int desistir) {
  double alvo = vis.pos, corr;
  if (!tb.ativo) return;
  lentoFim("back to the original");
  corr = tbr_correcao();
  alvo += corr;
  if (vis.dur > 0 && alvo > vis.dur) alvo = vis.dur;
  if (alvo < 0) alvo = 0;
  tbr_encerrar();
  tb.ativo = 0; tb.esperaDesde = 0;
  if (desistir) { tb.desistiu = 1; tb.voltar = 1; }
  printf("[tbburn] " TBQ_ALVO ": back to the original file at %.1f s (player clock %+.0f ms to the burned picture): %s\n",
         alvo, corr * 1000.0, porque);
  fflush(stdout);
  tbq_host_reabrir(alvo, audioLista);
}
// The burned stream never gave a picture and its item is in the account: the
// item is stuck (.tpk logs 19-21). Back to the original, remove and re-add.
static void semVideo(const char *porque) {
  int item = tbburn_item_na_conta(video_url_atual()), sub = tb.sub, aud = tb.audio;
  printf("[tbburn] %s (%u ms after the burned stream opened): back to the original now (item %d%s)\n", porque,
         msAgora() - tb.abriuEm, item, item > 0 ? " in your TorBox: removing and re-adding it" : ", not known");
  fflush(stdout);
  desligar("TorBox sent no video for this file", 1);
  if (item > 0 && video_url_atual()[0] && tbburn_pode()) {
    tb.sub = sub; tb.audio = aud;
    tb.registrando = 0; tb.desistiu = 0;
    tb.voltar = 2;   // no "unavailable" notice: it's on its way
    tb.pend = 1; tb.pediuEm = msAgora();
    tbburn_readicionar(video_url_atual(), sub, aud);
  }
}
static void pedir(int i, const char *porque) {
  const VideoFaixa *f = video_legenda(i);
  int sub = f ? f->ordinalMkv : -1, aud = tb.ativo ? audioLista : idxAudio();
  if ((tb.ativo || tb.pend) && tb.sub == sub && tb.audio == aud) { tb.leg = i; return; }
  tb.leg = i; tb.sub = sub; tb.audio = aud; tb.pend = 1; tb.pediuEm = msAgora();
  tb.registrando = 0;
  if (!tb.ativo) tb.durOrig = vis.dur;
  printf("[tbburn] burn in subtitle %d (%s, %s) [%s]: asking TorBox (subtitle %d, audio %d; playing %s)\n", i,
         f ? f->rotulo : "?", f ? f->codec : "?", porque, sub, aud, tb.ativo ? "TorBox's stream" : "the original file");
  fflush(stdout);
  tbburn_pedir(video_url_atual(), sub, aud);
}

void tbq_tocar(void) {
  if (tb.pend || tb.registrando) tbburn_cancelar();
  tbr_encerrar();
  if (tb.origMarcado) tbburn_original_parou();
  memset(&tb, 0, sizeof tb);
  tb.leg = tb.sub = -1;
  nListaA = nListaL = 0; audioLista = 0;
}

void tbq_bombear(const TbqVista *v) {
  static char m[4096];
  char motivo[160];
  int e;
  vis = *v;
  if (vis.ativo && vis.pronto && !tb.ativo) tb.hdrArq = tbq_hdr_texto(video_hdr());
  tbburn_arquivo_tem_ass(temAss());   // the menu's list: the file's tracks while burned
  tbburn_cedo_bombear();
  { int orig = vis.ativo && vis.pronto && vis.tocando && !tb.ativo && !vis.trailer && video_url_atual()[0];
    if (orig && !tb.origMarcado) { tb.origMarcado = 1; tbburn_original_tocando(video_url_atual()); }
    else if (tb.origMarcado && (tb.ativo || !vis.ativo)) { tb.origMarcado = 0; tbburn_original_parou(); } }
  tbcBombear();
  // Subtitle reader: 2 connections while burned, 1 while sync's reference is
  // on the network: never more than 4 on TorBox (player 1 + these).
  mkvass_teto_conexoes(tb.ativo || tb.pend ? (legref_em_rede() > 0 ? 1 : 2) : 0);
  // The header's codecs on the player's tracks (video_mkv_sondado 1): decide once.
  if (!tb.decidido && !tb.ativo && !tb.pend && !tb.desistiu && vis.ativo && vis.pronto && video_mkv_sondado() == 1 &&
      video_n_legenda() > 0 && temAss() && ajustes_leg_queimar() && tbburn_pode() && vis.permitido && !vis.trailer) {
    const char *porque = "";
    int k = autoFaixa(1, &porque);
    tb.decidido = 1;
    if (k >= 0) pedir(k, porque);
    else { printf("[tbburn] nothing to burn (%s)\n", porque); fflush(stdout); }
  }
  if (tb.pend || tb.registrando) {
    e = tbburn_resultado(m, sizeof m, motivo, sizeof motivo);
    if (e == TBB_ESPERANDO && tb.pend) {
      printf("[tbburn] TorBox is registering the re-added file; Nuvio draws the subtitles meanwhile and switches "
             "to TorBox's stream by itself when it is ready\n");
      fflush(stdout);
      tb.pend = 0; tb.registrando = 1; tb.voltar = 2; tb.aviso = 2;
      return;
    }
    if (tb.registrando) {
      if (e == TBB_ESPERANDO) return;
      tb.registrando = 0;
      if (e == TBB_PRONTO && vis.ativo && !vis.trailer && m[0]) {
        printf("[tbburn] burn-in: TorBox's stream is ready %u ms after the request: switching now\n", msAgora() - tb.pediuEm);
        fflush(stdout);
        abrirQueimado(m);
      } else {
        printf("[tbburn] re-added file: %s; Nuvio keeps drawing the subtitles\n",
               e == TBB_PRONTO ? "playback moved on" : motivo[0] ? motivo : "TorBox gave no stream");
        fflush(stdout);
        tbburn_cancelar();
      }
      return;
    }
    if (e == TBB_PRONTO) {
      tb.pend = 0;
      if (!vis.ativo || vis.trailer || !m[0]) { tbburn_cancelar(); return; }
      abrirQueimado(m);
      return;   // this frame's view is the player before the reopen
    } else if (e == TBB_FALHOU || venceu(tb.pediuEm, TBQ_PEDIDO_MS)) {
      tb.pend = 0; tb.desistiu = 1; tb.voltar = !tb.ativo;
      tbburn_cancelar();
      printf("[tbburn] not burning in on this source: %s%s\n", e == TBB_FALHOU ? motivo : "no answer in 30 s",
             tb.ativo ? " (TorBox's stream keeps the previous choice)" : "; Nuvio draws the subtitles");
      fflush(stdout);
    }
  }
  if (!tb.ativo) return;
  if (tb.esperaDesde && vis.pronto) { tb.esperaDesde = 0; lentoFim("it started"); }
  if (tb.esperaDesde && !tb.lento && venceu(tb.esperaDesde, TBQ_LENTO_MS)) {
    tb.lento = 1; tb.aviso = 3;
    printf("[tbburn] TorBox's burned stream has no picture %u ms after the switch: notice up (OK goes back to the original)\n",
           msAgora() - tb.esperaDesde);
    fflush(stdout);
  }
  if (tb.esperaDesde && venceu(tb.esperaDesde, TBQ_SEM_INICIO_MS)) {
    printf("[tbburn] TorBox's burned stream did not start in 30 s: treated as a player error\n");
    fflush(stdout);
    tb.esperaDesde = 0;
    (void)tbq_erro_player(-1);
    return;
  }
  // The one clock check per open/seek (owner 26/11: lean logs).
  if (!tb.tocouLog && vis.pronto && vis.tocando) {
    tb.tocouLog = 1;
    printf("[tbburn] " TBQ_ALVO ": burned stream playing %u ms after opening | clock check: asked %.1f s, player at %.1f s, "
           "subtitle correction %+.0f ms; duration TorBox %.1f s vs file %.1f s\n", msAgora() - tb.abriuEm, tb.alvo, vis.pos,
           tbr_correcao() * 1000.0, vis.dur, tb.durOrig);
    fflush(stdout);
  }
}

int tbq_erro_player(int cod) {
  if (!tb.ativo) return 0;
  printf("[tbburn] " TBQ_ALVO ": player error %d on TorBox's burned stream%s\n", cod, tb.tocouLog ? "" : " before it played");
  fflush(stdout);
  lentoFim("back to the original");
  if (!tb.tocouLog && tbburn_item_na_conta(video_url_atual()) > 0) semVideo("TorBox's burned stream never started");
  else desligar("the player failed on TorBox's stream", 1);
  return 1;
}

void tbq_fundo(double *alvo) {
  if (tb.pend || tb.registrando) { tbburn_cancelar(); tb.pend = tb.registrando = 0; tb.decidido = 0; }
  if (!tb.ativo) return;
  if (alvo) {
    double corr = tbr_correcao();
    *alvo += corr;
    if (*alvo < 0) *alvo = 0;
    if (vis.dur > 0 && *alvo > vis.dur) *alvo = vis.dur;
  }
  tbr_encerrar();
  lentoFim("the app left");
  tb.ativo = 0; tb.esperaDesde = 0;
  tb.decidido = 0;   // asks TorBox again once the original plays
  printf("[tbburn] " TBQ_ALVO ": the app left while burned: the original reopens at %.1f s, then TorBox is asked again\n",
         alvo ? *alvo : vis.pos);
  fflush(stdout);
}

void tbq_buscar(double s) { if (tb.ativo) tbr_ancorar(s); }

int tbq_escolher_audio(int i) {
  int nA = tb.ativo ? nListaA : video_n_audio();
  if (!(tb.ativo || tb.pend) || i < 0 || i >= nA) return 0;
  if (tb.ativo) {
    if (i == audioLista) return 1;
    audioLista = i;
  }
  if (i == tb.audio) return tb.ativo;
  tb.audio = i; tb.pend = 1; tb.pediuEm = msAgora();
  printf("[tbburn] audio %d chosen: asking TorBox for subtitle %d with audio %d\n", i, tb.sub, i);
  fflush(stdout);
  tbburn_pedir(video_url_atual(), tb.sub, i);
  return tb.ativo;
}

// ---- the calls of tbburn.h (faixas.c, legendasui.c, app.c, player.c) ----------
void video_tpk_queimar_faixa(int i) {
  TbFaixa t;
  printf("[tbburn] Burn in pick: menu entry %d of %d (%s, ordinal %d) | player %s, side connections %s, trailer %s, "
         "TorBox allowed %d\n", i, video_n_legenda(), video_legenda(i) ? video_legenda(i)->codec : "-",
         video_legenda(i) ? video_legenda(i)->ordinalMkv : -1, vis.ativo ? "open" : "CLOSED",
         vis.permitido ? "allowed" : "NOT ALLOWED", vis.trailer ? "YES" : "no", tbburn_pode());
  fflush(stdout);
  if (!vis.ativo || !vis.permitido || vis.trailer || !video_url_atual()[0]) return;
  tb.decidido = 1;
  if (i >= 0 && i < video_n_legenda() && legAss(i)) {
    faixaDe(i, &t);
    tbburn_definir_escolha(1);
    tbburn_lembrar(&t);
    if (hdr()) tbburn_hdr_lembrar(1);
    tb.desistiu = 0;
    if (tbburn_pode()) pedir(i, "picked in Burn in");
    else { printf("[tbburn] Burn in pick not sent: TorBox burn-in is not allowed now (plan, key or refused)\n"); fflush(stdout); }
    return;
  }
  tbburn_definir_escolha(0);
  tbburn_lembrar(NULL);
  if (hdr()) tbburn_hdr_lembrar(0);
  if (tb.pend || tb.registrando) {
    tb.pend = 0; tb.registrando = 0; tbburn_cancelar();
    printf("[tbburn] request dropped: Burn in Off\n"); fflush(stdout);
  }
  if (tb.ativo) desligar("Burn in Off", 0);
  tb.voltar = 2;   // the ASS tracks are back in Main/Second: let the pick run again (no toast)
}
void video_tpk_queimar_ligar(int ligar) {
  const char *porque = "";
  int k;
  if (!ligar) { video_tpk_queimar_faixa(-1); return; }
  k = autoFaixa(0, &porque);
  printf("[tbburn] Burn in On: %s\n", k >= 0 ? "burning the automatic pick" : porque);
  fflush(stdout);
  if (k >= 0) video_tpk_queimar_faixa(k);
}
int video_tpk_leg_queimavel(int i) { return legAss(i); }
int video_tpk_leg_oculta(int i) { return (tb.ativo || tb.pend) && legAss(i); }
int video_tpk_queimada(void) { return tb.ativo || tb.pend ? tb.leg : -1; }
int video_tpk_queimar_desejada(void) {
  const char *porque = "";
  if (!vis.ativo || vis.trailer || tb.ativo || tb.pend) return -1;
  if (!tbburn_pode()) return -1;
  return autoFaixa(1, &porque);
}
int video_tpk_queimar_voltou(void) { int v = tb.voltar; tb.voltar = 0; return v; }
int video_tpk_queimar_aviso(void) { int v = tb.aviso; tb.aviso = 0; return v; }
int video_tpk_queimar_tirar_main(void) { int v = tb.tirarMain; tb.tirarMain = 0; return v; }
int video_tpk_queimar_abrindo(void) { return tb.ativo && tb.esperaDesde; }
// "Slow to start" notice up: OK goes back to the original (owner 24/11/2026).
int video_tpk_queimar_tardio_ok(void) {
  if (!tb.lento) return 0;
  tb.lento = 0;
  if (!tb.ativo || !tb.esperaDesde || vis.pronto) return 0;
  printf("[tbburn] slow start: OK pressed %u ms after the switch: back to the original\n", msAgora() - tb.esperaDesde);
  fflush(stdout);
  tb.esperaDesde = 0;
  desligar("the viewer chose the original (burned stream slow to start)", 1);
  return 1;
}
// No Passthrough here (owner 26/11/2026): the Original audio row never shows.
int  video_tpk_tba_linhas(void) { return 0; }
void video_tpk_tba_rotulo(int k, char *nome, unsigned long nn, char *sub, unsigned long ns) {
  (void)k; if (nome && nn) nome[0] = 0; if (sub && ns) sub[0] = 0;
}
void video_tpk_tba_ok(int k) { (void)k; }
void video_tpk_tba_passo(int k, int dir) { (void)k; (void)dir; }
void video_tpk_tba_soltar(void) {}

// The early lookup (owner 18/11): a source focused 1 s in the list is looked
// up in the TorBox account, as tpk.c does. Installed at load time.
static void fonteEmFoco(const Stream *s) {
  if (!s || s->mp4) return;
  tbburn_procurar_cedo(s->url, s->hashTorrent[0] ? s->hashTorrent : s->infoHash, s->arquivo, s->tamanhoBytes,
                       "source focused 1 s");
}
__attribute__((constructor)) static void instalar(void) { if (!stream_foco_demorado) stream_foco_demorado = fonteEmFoco; }
#endif
