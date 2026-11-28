// AUDIO FROM ANOTHER SOURCE on LG: doador_tpk.h over the LG converter (see
// doador_webos.h). Only the webOS build with FFmpeg; the .tpk has its donor
// server (doador_tpk.c), the other platforms its stubs.
#if defined(NV_WEBOS) && defined(NV_DTS_FFMPEG) && !defined(NV_TPK) && !defined(NV_DOADOR_TEST)
#include "doador_tpk.h"
#include "doador_webos.h"
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <math.h>

#define DOAW_AUDIO_MAX 24
typedef struct {
  DoaLegInfo leg[DOA_LEG_MAX];
  double *legT[DOA_LEG_MAX];
  int nLeg;
} Fonte;
static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
static Fonte fo[2];
static DoaRendicao arqAudio[DOAW_AUDIO_MAX];
static int nArqAudio, difOk, difCorte, difVel, difPpm, baseHls;
static double iniArq, iniDoador;
static unsigned geracao;

static void fonteLimpar(Fonte *f) {
  for (int i = 0; i < f->nLeg; i++) free(f->legT[i]);
  memset(f, 0, sizeof *f);
}
void doaw_limpar(void) {
  pthread_mutex_lock(&trava);
  fonteLimpar(&fo[0]); fonteLimpar(&fo[1]);
  nArqAudio = difOk = 0;
  pthread_mutex_unlock(&trava);
}
static int ehTextoLeg(enum AVCodecID id) {
  return id == AV_CODEC_ID_SUBRIP || id == AV_CODEC_ID_TEXT || id == AV_CODEC_ID_ASS || id == AV_CODEC_ID_SSA ||
         id == AV_CODEC_ID_WEBVTT || id == AV_CODEC_ID_MOV_TEXT;
}
static int temSemCaixa(const char *h, const char *n) {
  size_t k = strlen(n);
  for (; *h; h++) if (!strncasecmp(h, n, k)) return 1;
  return 0;
}
static int cmpDouble(const void *a, const void *b) {
  double x = *(const double *)a, y = *(const double *)b;
  return x < y ? -1 : x > y;
}
// The same reading as the .tpk's (doador_tpk.c legendasIndice).
static void legendasIndice(Fonte *f, AVFormatContext *ic, const char *quem) {
  int sem = 0, poucos = 0;
  for (unsigned i = 0; ic && i < ic->nb_streams && f->nLeg < DOA_LEG_MAX; i++) {
    AVStream *st = ic->streams[i];
    int n, j;
    double *t;
    DoaLegInfo *li;
    if (st->codecpar->codec_type != AVMEDIA_TYPE_SUBTITLE || !ehTextoLeg(st->codecpar->codec_id)) continue;
    n = avformat_index_get_entries_count(st);
    if (n <= 0) { sem++; continue; }
    if (n < 20) { poucos++; continue; }
    if (!(t = malloc(sizeof *t * (size_t)n))) continue;
    for (j = 0; j < n; j++) {
      const AVIndexEntry *e = avformat_index_get_entry(st, j);
      t[j] = e ? e->timestamp * av_q2d(st->time_base) : 0;
    }
    qsort(t, (size_t)n, sizeof *t, cmpDouble);
    li = &f->leg[f->nLeg];
    memset(li, 0, sizeof *li);
    { AVDictionaryEntry *l = av_dict_get(st->metadata, "language", NULL, 0), *ti = av_dict_get(st->metadata, "title", NULL, 0);
      snprintf(li->lingua, sizeof li->lingua, "%s", l && strcmp(l->value, "und") ? l->value : "");
      snprintf(li->nome, sizeof li->nome, "%s", ti ? ti->value : "");
      snprintf(li->codec, sizeof li->codec, "%s", avcodec_get_name(st->codecpar->codec_id));
      li->forcada = (st->disposition & AV_DISPOSITION_FORCED) || (ti && (temSemCaixa(ti->value, "forced") || temSemCaixa(ti->value, "sign"))); }
    li->n = n;
    li->falas = doa_falas(t, n, !strcmp(li->codec, "ass") || !strcmp(li->codec, "ssa"));
    f->legT[f->nLeg++] = t;
  }
  printf("[doador] %s: %d text subtitle track(s) indexed for the subtitle match", quem, f->nLeg);
  for (int i = 0; i < f->nLeg; i++)
    printf("%s%s %s%s (%d points, %d lines)", i ? "; " : ": ", f->leg[i].lingua[0] ? f->leg[i].lingua : "und", f->leg[i].codec,
           f->leg[i].forcada ? " forced" : "", f->leg[i].n, f->leg[i].falas);
  if (sem || poucos) printf(" | left out: %d without index points, %d with under 20", sem, poucos);
  printf("\n");
}
static double fpsDe(AVFormatContext *ic) {
  for (unsigned i = 0; ic && i < ic->nb_streams; i++) {
    AVStream *s = ic->streams[i];
    AVRational r;
    if (s->codecpar->codec_type != AVMEDIA_TYPE_VIDEO || (s->disposition & AV_DISPOSITION_ATTACHED_PIC)) continue;
    r = s->avg_frame_rate.num > 0 ? s->avg_frame_rate : s->r_frame_rate;
    return r.num > 0 && r.den > 0 ? (double)r.num / r.den : 0;
  }
  return 0;
}
static int velConhecida(double k) {
  static const double v[] = { 24000.0 / 23976.0, 25.0 / 24.0, 25000.0 / 23976.0 };
  for (int i = 0; i < 3; i++)
    if (fabs(k / v[i] - 1.0) < 0.0008 || fabs(k * v[i] - 1.0) < 0.0008) return 1;
  return 0;
}
void doaw_fontes_abertas(void *mainFmt, void *donorFmt, int hls) {
  AVFormatContext *m = mainFmt, *d = donorFmt;
  if (!m || !d) return;
  pthread_mutex_lock(&trava);
  fonteLimpar(&fo[0]); fonteLimpar(&fo[1]);
  legendasIndice(&fo[0], m, "this file");
  legendasIndice(&fo[1], d, "the other version");
  nArqAudio = 0;
  for (unsigned i = 0; i < m->nb_streams && nArqAudio < DOAW_AUDIO_MAX; i++) {
    AVStream *st = m->streams[i];
    AVDictionaryEntry *l, *t;
    DoaRendicao *r;
    if (st->codecpar->codec_type != AVMEDIA_TYPE_AUDIO) continue;
    r = &arqAudio[nArqAudio++];
    memset(r, 0, sizeof *r);
    l = av_dict_get(st->metadata, "language", NULL, 0);
    t = av_dict_get(st->metadata, "title", NULL, 0);
    r->chave = r->ord = nArqAudio - 1; r->si = (int)i; r->canais = st->codecpar->ch_layout.nb_channels;
    snprintf(r->lingua, sizeof r->lingua, "%s", l && strcmp(l->value, "und") ? l->value : "");
    snprintf(r->nome, sizeof r->nome, "%s", t ? t->value : "");
    snprintf(r->codec, sizeof r->codec, "%s", avcodec_get_name(st->codecpar->codec_id));
    snprintf(r->codecFonte, sizeof r->codecFonte, "%s", r->codec);
  }
  iniArq = m->start_time != AV_NOPTS_VALUE ? m->start_time / (double)AV_TIME_BASE : 0;
  iniDoador = d->start_time != AV_NOPTS_VALUE ? d->start_time / (double)AV_TIME_BASE : 0;
  baseHls = hls;
  // Another cut or another speed, as doa_diferenca says it on the .tpk.
  { double fa = fpsDe(m), fb = fpsDe(d), k = fa > 0 && fb > 0 ? fb / fa : 0, kk = 1;
    difVel = fa > 0 && fb > 0 && fabs(fa - fb) > 0.05;
    difPpm = 0;
    if (difVel && velConhecida(k) && m->duration > 0 && d->duration > 0 &&
        fabs((double)d->duration * k / (double)m->duration - 1.0) < 0.01) { kk = k; difPpm = (int)lround((k - 1.0) * 1e6); }
    difOk = m->duration > 0 && d->duration > 0;
    difCorte = difOk ? (int)lround(((double)d->duration * kk - (double)m->duration) / 1e6) : 0; }
  geracao++;
  printf("[doador] LG: both files open (%s): starts %.3f / %.3f s, another cut %+d s, frame rates %s\n", hls ? "HLS" : "converter",
         iniArq, iniDoador, difCorte, difVel ? "differ" : "the same");
  fflush(stdout);
  pthread_mutex_unlock(&trava);
}
unsigned doaw_geracao(void) { pthread_mutex_lock(&trava); unsigned g = geracao; pthread_mutex_unlock(&trava); return g; }
int doaw_ms(double k, double desloc, int *ms) {
  double base;
  int ok;
  pthread_mutex_lock(&trava);
  ok = geracao != 0 && k > 0.9 && k < 1.1;
  // The converter: file = k * donor + (offset + file start - donor start);
  // the HLS remux scales the donor's start with the speed.
  base = baseHls ? iniArq - k * iniDoador : iniArq - iniDoador;
  pthread_mutex_unlock(&trava);
  if (ok && ms) *ms = (int)lround((desloc - base) * 1000.0);
  return ok;
}
int doaw_faixa_si(const char *lingua, const char *codec, int canais) {
  int si = -1;
  if (!lingua || !lingua[0]) return -1;
  pthread_mutex_lock(&trava);
  for (int i = 0; i < nArqAudio && si < 0; i++)
    if (!strcmp(arqAudio[i].lingua, lingua) && (!codec || !codec[0] || !strcmp(arqAudio[i].codec, codec)) &&
        (!canais || arqAudio[i].canais == canais)) si = arqAudio[i].si;
  for (int i = 0; i < nArqAudio && si < 0; i++) if (!strcmp(arqAudio[i].lingua, lingua)) si = arqAudio[i].si;
  pthread_mutex_unlock(&trava);
  return si;
}

// ---- doador_tpk.h ----------------------------------------------------------
int doa_disponivel(void) { return 1; }
int doa_legendas(int fonte, DoaLegInfo *out, int max) {
  int n = 0;
  pthread_mutex_lock(&trava);
  if (fonte >= 0 && fonte < 2) for (int i = 0; i < fo[fonte].nLeg && n < max; i++) out[n++] = fo[fonte].leg[i];
  pthread_mutex_unlock(&trava);
  return n;
}
int doa_legenda_tempos(int fonte, int k, double *t, int max) {
  int n = 0;
  pthread_mutex_lock(&trava);
  if (fonte >= 0 && fonte < 2 && k >= 0 && k < fo[fonte].nLeg) {
    n = fo[fonte].leg[k].n < max ? fo[fonte].leg[k].n : max;
    memcpy(t, fo[fonte].legT[k], sizeof *t * (size_t)n);
  }
  pthread_mutex_unlock(&trava);
  return n;
}
int doa_diferenca(int *corte, int *velocidade, int *ppm) {
  int v;
  pthread_mutex_lock(&trava);
  v = difOk;
  if (v) { if (corte) *corte = difCorte; if (velocidade) *velocidade = difVel; if (ppm) *ppm = difPpm; }
  pthread_mutex_unlock(&trava);
  return v;
}
int doa_faixas_arquivo(DoaRendicao *out, int max) {
  int n = 0;
  pthread_mutex_lock(&trava);
  for (; n < nArqAudio && n < max; n++) out[n] = arqAudio[n];
  pthread_mutex_unlock(&trava);
  return n;
}
// The rest belongs to the .tpk's server: on LG the converter does it (video.c).
int  doa_abrir(const DoaAbrir *o, char *out, size_t n) { (void)o; if (out && n) out[0] = 0; return 0; }
void doa_fechar(void) {}
int  doa_ativo(void) { return 0; }
int  doa_pronto(void) { return 0; }
int  doa_falhou(char *m, size_t n) { if (m && n) m[0] = 0; return 0; }
int  doa_rendicoes(DoaRendicao *out, int max) { (void)out; (void)max; return 0; }
int  doa_rendicao_de(int chave) { (void)chave; return -1; }
void doa_definir_audio(int chave) { (void)chave; }
int  doa_audio(void) { return -1; }
void doa_desloc(int ms) { (void)ms; }
int  doa_desloc_ms(void) { return 0; }
int  doa_estado(char *m, size_t n) { if (m && n) m[0] = 0; return 0; }
void doa_log(void) {}
void doa_definir_casa(int (*casa)(const char *, const char *)) { (void)casa; }
int  doa_sondar(const char *u, const char *c, DoaSonda *o, int m) { (void)u; (void)c; (void)o; (void)m; return -1; }
int  doa_preabrir(const char *u, const char *c, int p, char *m, size_t n) { (void)u; (void)c; (void)p; if (m && n) m[0] = 0; return 0; }
void doa_dopasuj(int a) { (void)a; }
int  doa_dopasuj_resultado(DoaDopasuj *o, int *a) { (void)o; (void)a; return 0; }
int  doa_dopasuj_ativo(void) { return 0; }
int  doa_doador_saude(int *l) { if (l) *l = 0; return 0; }
int  doa_seguir_evento(int *ms) { (void)ms; return 0; }
int  doa_pedidos_audio(void) { return 0; }
int  doa_pedidos(void) { return 0; }
void doa_aplicar_legendas(double k, double d, double c) { (void)k; (void)d; (void)c; }
void doa_dopasuj_fino(void) {}
void doa_mapa_definir(const DoaMapa *m, double p) { (void)m; (void)p; }
void doa_posicao(double p) { (void)p; }
int  doa_mapa_trecho(double p, double *i, double *f, int *r) { (void)p; (void)i; (void)f; (void)r; return -1; }
void doa_dopasuj_fino_trecho(int t) { (void)t; }
void doa_match_definir(const char *o, const char *l, const char *c, int n, int v) { (void)o; (void)l; (void)c; (void)n; (void)v; }
#else
#include "doador_webos.h"
// Elsewhere (host tests of video.c, other platforms): nothing opened, no answer.
void doaw_fontes_abertas(void *m, void *d, int h) { (void)m; (void)d; (void)h; }
unsigned doaw_geracao(void) { return 0; }
void doaw_limpar(void) {}
int  doaw_ms(double k, double d, int *ms) { (void)k; (void)d; (void)ms; return 0; }
int  doaw_faixa_si(const char *l, const char *c, int n) { (void)l; (void)c; (void)n; return -1; }
#endif
