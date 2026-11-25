// Seek-bar thumbnails made on the TV: the decoding engine. See miniatura_motor.h.
#include "miniatura_motor.h"
#include "miniplat.h"

// FFmpeg: the .tpk 6+ and LG link it for the DTS converter (NV_DTS_FFMPEG),
// Android links a thumbnail-only build (NV_MINI_FFMPEG).
#if defined(NV_MINIATURAS) && (defined(NV_DTS_FFMPEG) || defined(NV_MINI_FFMPEG))
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/sha.h>
#include <libavutil/pixdesc.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define BLOCO (256 * 1024)
#define BLOCOS 8                     // read cache: 2 MiB
#define LER_ADIANTE 4                // default: up to 1 MiB per range request (MiniConfig.lerBlocos)
#define MAX_PACOTE (8 * 1024 * 1024) // a keyframe bigger than this is skipped
#define MAX_PACOTES 400              // packets read per frame before giving up
#define TS_CAUDA (4 << 20)           // MPEG-TS: bytes read at the end for the duration
#define TS_ANTES_MS 300L             // MPEG-TS: aim this far before the target (read forward to a keyframe)
#define TS_MAX_LER (24 << 20)        // MPEG-TS: bytes read looking for one keyframe before giving up
#define TS_MAPA 1024                 // MPEG-TS: keyframe positions remembered (16 KB)

typedef struct { int64_t bloco; int n; unsigned uso; unsigned char *dados; } Bloco;

struct MiniMotor {
  MiniLer ler; void *usuario; int64_t tamanho;
  int64_t pos; long long lidos;
  Bloco cache[BLOCOS]; unsigned relogio;
  AVIOContext *io; AVFormatContext *fc;
  int vs; AVStream *st;
  AVCodecContext *cc; int ccThreads;
  const AVCodec *codec;
  AVCodecContext *jc;               // mjpeg encoder
  AVFrame *quadro, *saida;
  AVPacket *pkt, *jpkt;
  MiniInfo info;
  int64_t inicio;                   // stream start in stream time base
  float lutPq[1024], lutGama[4096];
  int lutPronta;
  int64_t jpts;
  int buffersNoPacote;              // get_buffer2 calls since the packet was sent
  AVFrame *cinza;                   // the ONE buffer every placeholder reference shares
  int semIndice;                    // MPEG-TS / M2TS: no keyframe index, seek by byte position
  // MPEG-TS: (byte position, time) of every keyframe found so far, sorted by
  // position. Byte guesses interpolate between the two nearest, so the map gets
  // sharper with every tile (the bitrate of a remux varies a lot).
  struct { int64_t pos; long ms; } mapa[TS_MAPA];
  int nMapa;
  unsigned char cabeca[16];         // first bytes of the file (logged when it can't be opened)
  int ncabeca;
  MiniConfig cfg;
  int pedidos;                      // range requests made by the demuxer path
  // Prefetch: bytes of the NEXT frame's keyframe fetched by a helper thread
  // while this frame decodes (nothing reads the source then).
  int64_t proxPos;                  // -1 = no target
  pthread_t preT; int preVivo;
  int64_t preIni; long preQuer, preGot; unsigned char *preBuf;
};

void mini_motor_config_padrao(MiniConfig *c) {
  memset(c, 0, sizeof *c);
  c->lerBlocos = LER_ADIANTE; c->prefetch = 1; c->tsBlocos = BLOCOS - 1;
}
void mini_motor_config(MiniMotor *m, const MiniConfig *c) {
  if (!m || !c) return;
  m->cfg = *c;
  if (m->cfg.lerBlocos < 1) m->cfg.lerBlocos = 1;
  if (m->cfg.lerBlocos > BLOCOS - 1) m->cfg.lerBlocos = BLOCOS - 1;
  if (m->cfg.tsBlocos < 1) m->cfg.tsBlocos = 1;
  if (m->cfg.tsBlocos > BLOCOS - 1) m->cfg.tsBlocos = BLOCOS - 1;
}

static unsigned agoraMs(void) {
  struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
  return (unsigned)(t.tv_sec * 1000u + t.tv_nsec / 1000000u);
}

// ------------------------------------------------------------------ reader
static Bloco *achar(MiniMotor *m, int64_t b) {
  for (int i = 0; i < BLOCOS; i++) if (m->cache[i].dados && m->cache[i].bloco == b) return &m->cache[i];
  return NULL;
}
static Bloco *vago(MiniMotor *m) {
  Bloco *v = &m->cache[0];
  for (int i = 0; i < BLOCOS; i++) {
    if (!m->cache[i].dados) return &m->cache[i];
    if (m->cache[i].uso < v->uso) v = &m->cache[i];
  }
  return v;
}
// Fetch block b plus up to LER_ADIANTE-1 following blocks not cached yet, in
// ONE range request.
static int buscar(MiniMotor *m, int64_t b, int quer) {
  int n = 1, max = m->semIndice ? m->cfg.tsBlocos : m->cfg.lerBlocos;
  (void)quer;
  while (n < max && (b + n) * BLOCO < m->tamanho && !achar(m, b + n)) n++;
  int64_t ini = b * BLOCO, fim = (b + n) * (int64_t)BLOCO - 1;
  if (fim >= m->tamanho) fim = m->tamanho - 1;
  size_t tam = (size_t)(fim - ini + 1);
  unsigned char *buf = malloc(tam);
  if (!buf) return -1;
  long got = m->ler(m->usuario, ini, fim, buf);
  if (got <= 0) { free(buf); return -1; }
  m->lidos += got; m->pedidos++;
  for (int k = 0; k < n && (int64_t)k * BLOCO < got; k++) {
    Bloco *v = vago(m);
    int nb = (int)((got - (long)k * BLOCO) < BLOCO ? got - (long)k * BLOCO : BLOCO);
    unsigned char *d = malloc((size_t)nb);
    if (!d) break;
    memcpy(d, buf + (size_t)k * BLOCO, (size_t)nb);
    free(v->dados);
    *v = (Bloco){ b + k, nb, ++m->relogio, d };
  }
  free(buf);
  return achar(m, b) ? 0 : -1;
}
static int lerIo(void *u, uint8_t *dst, int quer) {
  MiniMotor *m = u;
  if (m->pos >= m->tamanho) return AVERROR_EOF;
  int64_t b = m->pos / BLOCO;
  Bloco *k = achar(m, b);
  if (!k) { if (buscar(m, b, (int)(m->pos - b * BLOCO) + quer) < 0) return AVERROR(EIO); k = achar(m, b); }
  k->uso = ++m->relogio;
  int off = (int)(m->pos - b * BLOCO);
  int n = k->n - off;
  if (n <= 0) return AVERROR_EOF;
  if (n > quer) n = quer;
  memcpy(dst, k->dados + off, (size_t)n);
  m->pos += n;
  return n;
}
// Prefetched bytes (block-aligned) into the cache, as if read now.
static void guardarBlocos(MiniMotor *m, int64_t ini, const unsigned char *d, long got) {
  int64_t b0 = ini / BLOCO;
  for (long k = 0; (long)k * BLOCO < got; k++) {
    if (achar(m, b0 + k)) continue;
    int nb = (int)((got - k * (long)BLOCO) < BLOCO ? got - k * (long)BLOCO : BLOCO);
    unsigned char *c = malloc((size_t)nb);
    if (!c) break;
    memcpy(c, d + (size_t)k * BLOCO, (size_t)nb);
    Bloco *v = vago(m);
    free(v->dados);
    *v = (Bloco){ b0 + k, nb, ++m->relogio, c };
  }
}
static void *preTrabalho(void *u) {
  MiniMotor *m = u;
  m->preGot = m->ler(m->usuario, m->preIni, m->preIni + m->preQuer - 1, m->preBuf);
  return NULL;
}
// Starts fetching the next keyframe's blocks (if not cached already).
static void preIniciar(MiniMotor *m) {
  int64_t b, ini;
  int n = 0;
  if (!m->cfg.prefetch || m->proxPos < 0 || m->preVivo) return;
  b = m->proxPos / BLOCO;
  while (n < BLOCOS / 2 && achar(m, b)) { b++; n++; }       // skip what is cached
  int quer = m->semIndice ? m->cfg.tsBlocos : m->cfg.lerBlocos;
  if (n >= quer) return;
  ini = b * BLOCO;
  if (ini >= m->tamanho) return;
  m->preQuer = (long)(quer - n) * BLOCO;
  if (ini + m->preQuer > m->tamanho) m->preQuer = (long)(m->tamanho - ini);
  m->preBuf = malloc((size_t)m->preQuer);
  if (!m->preBuf) return;
  m->preIni = ini; m->preGot = 0;
  if (pthread_create(&m->preT, NULL, preTrabalho, m) == 0) m->preVivo = 1;
  else { free(m->preBuf); m->preBuf = NULL; }
}
// Waits for the prefetch (it ran during the decode) and caches what came.
static long preTerminar(MiniMotor *m) {
  long got = 0;
  if (!m->preVivo) return 0;
  pthread_join(m->preT, NULL);
  m->preVivo = 0;
  got = m->preGot;
  if (got > 0) { guardarBlocos(m, m->preIni, m->preBuf, got); m->lidos += got; }
  free(m->preBuf); m->preBuf = NULL;
  m->proxPos = -1;
  return got > 0 ? got : 0;
}

static int64_t seekIo(void *u, int64_t off, int whence) {
  MiniMotor *m = u;
  if (whence & AVSEEK_SIZE) return m->tamanho;
  whence &= ~AVSEEK_FORCE;
  if (whence == SEEK_SET) m->pos = off;
  else if (whence == SEEK_CUR) m->pos += off;
  else if (whence == SEEK_END) m->pos = m->tamanho + off;
  else return -1;
  if (m->pos < 0) m->pos = 0;
  return m->pos;
}

// First bytes as hex, for the log when the container is not recognised.
static void anotarCabeca(MiniMotor *m, char *erro, size_t nerro, const char *motivo) {
  char hex[16 * 3 + 1] = "";
  unsigned char b[16];
  int n = 0;
  if (m->ler && m->tamanho > 0) {
    long got = m->ler(m->usuario, 0, m->tamanho < 16 ? m->tamanho - 1 : 15, b);
    n = got > 0 ? (int)got : 0;
  }
  for (int i = 0; i < n; i++) snprintf(hex + 3 * i, 4, "%02x%s", b[i], i + 1 < n ? " " : "");
  if (erro && nerro) snprintf(erro, nerro, "%s (first bytes: %s)", motivo, n ? hex : "unreadable");
}

// ------------------------------------------------------------------ open
static void erroDe(char *erro, size_t n, const char *s) { if (erro && n) snprintf(erro, n, "%s", s); }

static long msDe(const MiniMotor *m, int64_t ts) {
  if (ts == AV_NOPTS_VALUE) return -1;
  return (long)av_rescale_q(ts - m->inicio, m->st->time_base, (AVRational){1, 1000});
}
static int64_t tsDe(const MiniMotor *m, long ms) {
  return av_rescale_q(ms, (AVRational){1, 1000}, m->st->time_base) + m->inicio;
}

// MPEG-TS: first and last video timestamps give the start and the duration
// (no find_stream_info, which would decode). Reads the head and the last few MB.
static int64_t tsPacote(const AVPacket *p) { return p->pts != AV_NOPTS_VALUE ? p->pts : p->dts; }

static void mapaAnotar(MiniMotor *m, int64_t pos, long ms) {
  if (pos < 0 || ms < 0) return;
  int i = 0;
  while (i < m->nMapa && m->mapa[i].pos < pos) i++;
  if (i < m->nMapa && m->mapa[i].pos == pos) return;
  // Keep it monotonic: a point whose time contradicts its neighbours is noise.
  if (i > 0 && m->mapa[i - 1].ms > ms) return;
  if (i < m->nMapa && m->mapa[i].ms < ms) return;
  if (m->nMapa >= TS_MAPA) return;
  memmove(&m->mapa[i + 1], &m->mapa[i], (size_t)(m->nMapa - i) * sizeof m->mapa[0]);
  m->mapa[i].pos = pos; m->mapa[i].ms = ms;
  m->nMapa++;
}
// Byte position for a time: straight line between the two known points around it.
static int64_t mapaPosicao(const MiniMotor *m, long ms) {
  int i = 0;
  if (m->nMapa < 2) return (int64_t)((double)ms * (double)m->tamanho / (double)m->info.duracaoMs);
  while (i + 2 < m->nMapa && m->mapa[i + 1].ms <= ms) i++;
  int64_t p0 = m->mapa[i].pos, p1 = m->mapa[i + 1].pos;
  long t0 = m->mapa[i].ms, t1 = m->mapa[i + 1].ms;
  if (t1 <= t0) return p0;
  return p0 + (int64_t)((double)(ms - t0) * (double)(p1 - p0) / (double)(t1 - t0));
}
static int pontasTs(MiniMotor *m, char *erro, size_t nerro) {
  int64_t primeiro = AV_NOPTS_VALUE, ultimo = AV_NOPTS_VALUE;
  AVPacket *p = av_packet_alloc();
  if (!p) { erroDe(erro, nerro, "memory"); return -1; }
  for (int i = 0; i < 3000 && primeiro == AV_NOPTS_VALUE; i++) {
    if (av_read_frame(m->fc, p) < 0) break;
    if (p->stream_index == m->vs) primeiro = tsPacote(p);
    av_packet_unref(p);
  }
  int64_t posUltimo = -1;
  int64_t de = m->tamanho - TS_CAUDA;
  if (primeiro != AV_NOPTS_VALUE && av_seek_frame(m->fc, -1, de > 0 ? de : 0, AVSEEK_FLAG_BYTE) >= 0) {
    for (int i = 0; i < 6000; i++) {
      if (av_read_frame(m->fc, p) < 0) break;
      int64_t t = p->stream_index == m->vs ? tsPacote(p) : AV_NOPTS_VALUE;
      if (t != AV_NOPTS_VALUE && (ultimo == AV_NOPTS_VALUE || t > ultimo)) { ultimo = t; posUltimo = p->pos; }
      av_packet_unref(p);
    }
  }
  av_packet_free(&p);
  if (primeiro == AV_NOPTS_VALUE || ultimo == AV_NOPTS_VALUE || ultimo <= primeiro) {
    erroDe(erro, nerro, "MPEG-TS: no video timestamps"); return -1;
  }
  m->inicio = primeiro;
  m->info.duracaoMs = msDe(m, ultimo);
  mapaAnotar(m, 0, 0);
  mapaAnotar(m, posUltimo > 0 ? posUltimo : m->tamanho, m->info.duracaoMs);
  return m->info.duracaoMs > 0 ? 0 : -1;
}

// MPEG-TS: jump to a byte position proportional to the time, read forward to
// the next keyframe packet, correct the position from its timestamp (up to 3
// tries; a binary search over an 80 GB file would read ~30 MB per frame).
// Leaves the keyframe packet in m->pkt.
// MPEG-TS: jump to the byte position the map gives for the time, find the next
// keyframe packet (probe windows, or read forward), correct from its timestamp
// (up to 3 tries; a binary search over an 80 GB file would read ~30 MB per
// frame). Leaves the keyframe packet in m->pkt.
static int chaveTs(MiniMotor *m, long alvoMs, long tolMs, volatile int *cancelar, int *pacotes, char *erro, size_t nerro) {
  long mira = alvoMs - TS_ANTES_MS;
  for (int tentativa = 0; tentativa < 3; tentativa++) {
    int64_t pos = mapaPosicao(m, mira < 0 ? 0 : mira);
    if (pos > m->tamanho - TS_CAUDA / 4) pos = m->tamanho - TS_CAUDA / 4;
    if (pos < 0) pos = 0;
    if (av_seek_frame(m->fc, -1, pos, AVSEEK_FLAG_BYTE) < 0) { erroDe(erro, nerro, "seek failed"); return -1; }
    long long lidos0 = m->lidos;
    for (;;) {
      if (cancelar && *cancelar) { erroDe(erro, nerro, "cancelled"); return -1; }
      if (*pacotes >= MAX_PACOTES || m->lidos - lidos0 > TS_MAX_LER) { erroDe(erro, nerro, "no keyframe found near the target"); return -1; }
      int rr = av_read_frame(m->fc, m->pkt);
      if (rr < 0) { erroDe(erro, nerro, rr == AVERROR(EIO) ? "source read failed" : "end of file before the keyframe"); return -1; }
      (*pacotes)++;
      if (m->pkt->stream_index == m->vs && (m->pkt->flags & AV_PKT_FLAG_KEY) && tsPacote(m->pkt) != AV_NOPTS_VALUE) break;
      av_packet_unref(m->pkt);
    }
    long achado = msDe(m, tsPacote(m->pkt));
    mapaAnotar(m, m->pkt->pos, achado);
    if (labs(achado - alvoMs) <= tolMs || tentativa == 2) return 0;
    av_packet_unref(m->pkt);
    // Missed: the map now has this keyframe, the next guess interpolates closer.
    // Overshot by less than a few seconds: aim a bit earlier than the map says.
    if (achado > alvoMs) mira -= (achado - alvoMs) / 2;
  }
  return -1;
}

MiniMotor *mini_motor_abrir(MiniLer ler, void *usuario, int64_t tamanho, char *erro, size_t nerro) {
  MiniMotor *m;
  unsigned char *buf;
  AVDictionary *op = NULL;
  if (!ler || tamanho <= 0) { erroDe(erro, nerro, "no source"); return NULL; }
  m = calloc(1, sizeof *m);
  if (!m) { erroDe(erro, nerro, "memory"); return NULL; }
  m->ler = ler; m->usuario = usuario; m->tamanho = tamanho; m->vs = -1;
  mini_motor_config_padrao(&m->cfg);
  m->proxPos = -1;
  av_log_set_level(AV_LOG_QUIET);
  buf = av_malloc(65536);
  m->io = buf ? avio_alloc_context(buf, 65536, 0, m, lerIo, NULL, seekIo) : NULL;
  m->fc = avformat_alloc_context();
  if (!m->io || !m->fc) { if (!m->io) av_free(buf); erroDe(erro, nerro, "memory"); mini_motor_fechar(m); return NULL; }
  m->fc->pb = m->io;
  m->fc->flags |= AVFMT_FLAG_CUSTOM_IO;
  av_dict_set(&op, "probesize", "262144", 0);
  av_dict_set(&op, "analyzeduration", "500000", 0);
  // The demuxer never opens anything itself (external references in a hostile file).
  av_dict_set(&op, "protocol_whitelist", "none", 0);
  if (avformat_open_input(&m->fc, NULL, NULL, &op) < 0) {
    av_dict_free(&op); m->fc = NULL; anotarCabeca(m, erro, nerro, "not a video container"); mini_motor_fechar(m); return NULL;
  }
  av_dict_free(&op);
  const char *nome = m->fc->iformat ? m->fc->iformat->name : "";
  m->semIndice = strcmp(nome, "mpegts") == 0;     // .ts and Blu-ray .m2ts (192-byte packets) alike
  if (!m->semIndice && !strstr(nome, "matroska") && !strstr(nome, "mp4") && !strstr(nome, "mov")) {
    char motivo[64];
    snprintf(motivo, sizeof motivo, "container not supported: %s", nome);
    anotarCabeca(m, erro, nerro, motivo); mini_motor_fechar(m); return NULL;
  }
  snprintf(m->info.formato, sizeof m->info.formato, "%s",
           m->semIndice ? "mpegts" : strstr(nome, "matroska") ? "matroska" : "mp4");
  // No avformat_find_stream_info: it decodes frames to probe (with frame threads
  // on a 4K stream that is ~150 MB). The MKV/MP4 header already has what we use.
  m->vs = av_find_best_stream(m->fc, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
  if (m->vs < 0) { erroDe(erro, nerro, "no video track"); mini_motor_fechar(m); return NULL; }
  m->st = m->fc->streams[m->vs];
  AVCodecParameters *par = m->st->codecpar;
  if (par->codec_id != AV_CODEC_ID_H264 && par->codec_id != AV_CODEC_ID_HEVC) {
    erroDe(erro, nerro, "codec not supported (only H.264 / HEVC)"); mini_motor_fechar(m); return NULL;
  }
  // MPEG-TS carries no picture size in its header (only the stream type): it is
  // known after the first decode. Until then plan memory for the worst case.
  if (m->semIndice && (par->width <= 0 || par->height <= 0)) {
    m->info.largura = par->codec_id == AV_CODEC_ID_HEVC ? 3840 : 1920;
    m->info.altura = par->codec_id == AV_CODEC_ID_HEVC ? 2160 : 1080;
  } else if (par->width <= 0 || par->height <= 0 || par->width > 4096 || par->height > 2304) {
    erroDe(erro, nerro, "picture size not supported (above 4K)"); mini_motor_fechar(m); return NULL;
  }
  m->codec = avcodec_find_decoder(par->codec_id);
  if (!m->codec) { erroDe(erro, nerro, "decoder missing in this build"); mini_motor_fechar(m); return NULL; }
  for (unsigned i = 0; i < m->fc->nb_streams; i++)
    if ((int)i != m->vs) m->fc->streams[i]->discard = AVDISCARD_ALL;   // never parse audio/subs packets we skip
  m->inicio = m->st->start_time != AV_NOPTS_VALUE ? m->st->start_time : 0;
  if (m->semIndice) {
    if (pontasTs(m, erro, nerro) < 0) { mini_motor_fechar(m); return NULL; }
    goto pronto;
  }
  int64_t dur = m->fc->duration > 0 ? av_rescale_q(m->fc->duration, AV_TIME_BASE_Q, (AVRational){1, 1000}) :
                m->st->duration > 0 ? av_rescale_q(m->st->duration, m->st->time_base, (AVRational){1, 1000}) : 0;
  if (dur <= 0) { erroDe(erro, nerro, "unknown duration"); mini_motor_fechar(m); return NULL; }
  m->info.duracaoMs = (long)dur;
  m->info.largura = par->width; m->info.altura = par->height;
pronto:
  m->info.hdr = par->color_trc == AVCOL_TRC_SMPTE2084 || par->color_trc == AVCOL_TRC_ARIB_STD_B67;
  const AVPixFmtDescriptor *d = av_pix_fmt_desc_get((enum AVPixelFormat)par->format);
  m->info.bits = d && d->comp[0].depth > 8 ? d->comp[0].depth : 8;
  // Without probing, an HEVC stream's pixel format can be unknown: plan for 10-bit.
  if (!d && par->codec_id == AV_CODEC_ID_HEVC) m->info.bits = 10;
  snprintf(m->info.codec, sizeof m->info.codec, "%s", par->codec_id == AV_CODEC_ID_HEVC ? "hevc" : "h264");
  if (m->semIndice) goto alocar;
  // Matroska loads its Cues on the first seek: one seek to the middle fills the index.
  if (avformat_index_get_entries_count(m->st) <= 1)
    av_seek_frame(m->fc, m->vs, tsDe(m, m->info.duracaoMs / 2), AVSEEK_FLAG_BACKWARD);
  int n = avformat_index_get_entries_count(m->st), k = 0;
  for (int i = 0; i < n; i++) {
    const AVIndexEntry *e = avformat_index_get_entry(m->st, i);
    if (e && (e->flags & AVINDEX_KEYFRAME)) k++;
  }
  m->info.chaves = k;
  if (k < 2) { erroDe(erro, nerro, "no keyframe index (file can't be skipped through)"); mini_motor_fechar(m); return NULL; }
alocar:
  m->quadro = av_frame_alloc(); m->saida = av_frame_alloc();
  m->pkt = av_packet_alloc(); m->jpkt = av_packet_alloc();
  if (!m->quadro || !m->saida || !m->pkt || !m->jpkt) { erroDe(erro, nerro, "memory"); mini_motor_fechar(m); return NULL; }
  return m;
}

const MiniInfo *mini_motor_info(const MiniMotor *m) { return m ? &m->info : NULL; }
long long mini_motor_lidos(const MiniMotor *m) { return m ? m->lidos : 0; }

// ------------------------------------------------------------------ keyframes
static int entradaChave(MiniMotor *m, long alvoMs, long tolMs, int64_t *ts, int64_t *pos) {
  int n = avformat_index_get_entries_count(m->st);
  int64_t alvo = tsDe(m, alvoMs);
  int i = av_index_search_timestamp(m->st, alvo, AVSEEK_FLAG_BACKWARD);
  const AVIndexEntry *antes = NULL, *depois = NULL;
  for (int k = i < 0 ? 0 : i; k >= 0 && k < n; k--) {
    const AVIndexEntry *e = avformat_index_get_entry(m->st, k);
    if (e && (e->flags & AVINDEX_KEYFRAME) && e->timestamp <= alvo) { antes = e; break; }
  }
  for (int k = i < 0 ? 0 : i; k < n; k++) {
    const AVIndexEntry *e = avformat_index_get_entry(m->st, k);
    if (e && (e->flags & AVINDEX_KEYFRAME) && e->timestamp > alvo) { depois = e; break; }
  }
  const AVIndexEntry *e = antes;
  if (depois && (!antes || labs(msDe(m, depois->timestamp) - alvoMs) < labs(msDe(m, antes->timestamp) - alvoMs)) &&
      labs(msDe(m, depois->timestamp) - alvoMs) <= tolMs)
    e = depois;
  if (!e) e = depois;
  if (!e) return -1;
  *ts = e->timestamp;
  if (pos) *pos = e->pos;
  return 0;
}

void mini_motor_proximo(MiniMotor *m, long alvoMs, long tolMs) {
  int64_t ts, pos = -1;
  if (!m) return;
  m->proxPos = -1;
  if (alvoMs < 0) return;
  // MPEG-TS: no prefetch. The map's guess is rarely where the forward read
  // starts after its corrections (host: 1108 ms with, 970 ms without).
  // (Probing small windows for the keyframe start was tried on the TV, 22/10:
  // on a UHD Blu-ray M2TS it never found one, so TS reads forward.)
  if (m->semIndice) pos = -1;
  else if (entradaChave(m, alvoMs, tolMs, &ts, &pos) < 0) pos = -1;
  m->proxPos = pos >= 0 && pos < m->tamanho ? pos : -1;
}

long mini_motor_chave(MiniMotor *m, long alvoMs, long tolMs) {
  int64_t ts;
  if (!m || entradaChave(m, alvoMs, tolMs, &ts, NULL) < 0) return -1;
  return msDe(m, ts);
}

size_t mini_motor_memoria(const MiniMotor *m, int threads) {
  if (!m) return 0;
  size_t px = (size_t)m->info.largura * (size_t)m->info.altura;
  size_t quadro = px * 3 / 2 * (m->info.bits > 8 ? 2 : 1);
  // One frame being decoded, the frame handed out, decoder tables and per-thread
  // buffers, the packet, the read cache.
  // Current picture + the shared placeholder + the picture handed out (pool) +
  // per-picture tables + per-thread buffers + packet + read cache.
  return quadro * 3 + px / 2 + (size_t)threads * (2u << 20) + MAX_PACOTE / 2 + BLOCOS * BLOCO;
}

// ------------------------------------------------------------------ decoder
// MEASURED (host, FFmpeg 7.1 source checked): an HEVC CRA keyframe (open GOP,
// x265's default) lists earlier pictures in its RPS. Decoding it on its own,
// the decoder makes a full-size gray placeholder for each one
// (refs.c generate_missing_ref): ~6 x 25 MB for 4K 10-bit, ~160 MB in all.
// They are never read (the keyframe is intra; its leading pictures are never
// sent), so every placeholder gets the SAME buffer. The first buffer asked for
// after a packet is the real picture (hevc_frame_start: set_new_ref before
// frame_rps), and gets its own.
static int obterBuffer(AVCodecContext *c, AVFrame *f, int flags) {
  MiniMotor *m = c->opaque;
  if (!m || m->buffersNoPacote++ == 0) return avcodec_default_get_buffer2(c, f, flags);
  if (m->cinza && (m->cinza->width != f->width || m->cinza->height != f->height ||
                   m->cinza->format != f->format))
    av_frame_free(&m->cinza);
  if (!m->cinza) {
    AVFrame *t = av_frame_alloc();
    if (!t) return AVERROR(ENOMEM);
    t->width = f->width; t->height = f->height; t->format = f->format;
    int r = avcodec_default_get_buffer2(c, t, flags);
    if (r < 0) { av_frame_free(&t); return r; }
    m->cinza = t;
  }
  for (int i = 0; i < AV_NUM_DATA_POINTERS; i++) {
    f->data[i] = m->cinza->data[i];
    f->linesize[i] = m->cinza->linesize[i];
    if (m->cinza->buf[i]) {
      f->buf[i] = av_buffer_ref(m->cinza->buf[i]);
      if (!f->buf[i]) { av_frame_unref(f); return AVERROR(ENOMEM); }
    }
  }
  f->extended_data = f->data;
  return 0;
}

static int abrirDecoder(MiniMotor *m, int threads) {
  if (m->cc && m->ccThreads == threads) return 0;
  avcodec_free_context(&m->cc);
  m->cc = avcodec_alloc_context3(m->codec);
  if (!m->cc) return -1;
  if (avcodec_parameters_to_context(m->cc, m->st->codecpar) < 0) return -1;
  m->cc->thread_count = threads;
  m->cc->thread_type = FF_THREAD_SLICE;     // never FF_THREAD_FRAME: it keeps a frame per thread
  m->cc->skip_loop_filter = AVDISCARD_ALL;  // no deblock / SAO: invisible at 320x180
  m->cc->skip_frame = AVDISCARD_NONINTRA;
  m->cc->flags2 |= AV_CODEC_FLAG2_SHOW_ALL; // an I-frame that is not an IDR still comes out
  m->cc->pkt_timebase = m->st->time_base;
  m->cc->opaque = m;
  m->cc->get_buffer2 = obterBuffer;
  if (avcodec_open2(m->cc, m->codec, NULL) < 0) { avcodec_free_context(&m->cc); return -1; }
  m->ccThreads = threads;
  return 0;
}

void mini_motor_soltar_decoder(MiniMotor *m) {
  if (!m) return;
  avcodec_free_context(&m->cc);
  m->ccThreads = 0;
  av_frame_free(&m->cinza);
  avcodec_free_context(&m->jc);
  av_frame_unref(m->quadro);
  av_frame_unref(m->saida);
}

static int abrirJpeg(MiniMotor *m) {
  if (m->jc) return 0;
  const AVCodec *c = avcodec_find_encoder(AV_CODEC_ID_MJPEG);
  if (!c) return -1;
  m->jc = avcodec_alloc_context3(c);
  if (!m->jc) return -1;
  m->jc->width = MINI_W; m->jc->height = MINI_H;
  m->jc->pix_fmt = AV_PIX_FMT_YUVJ420P;
  m->jc->color_range = AVCOL_RANGE_JPEG;
  m->jc->time_base = (AVRational){1, 25};
  m->jc->flags |= AV_CODEC_FLAG_QSCALE;
  m->jc->global_quality = FF_QP2LAMBDA * 4;
  m->jc->strict_std_compliance = FF_COMPLIANCE_UNOFFICIAL;
  if (avcodec_open2(m->jc, c, NULL) < 0) { avcodec_free_context(&m->jc); return -1; }
  return 0;
}

// ------------------------------------------------------------------ colour
static float hable(float x) {
  const float A = 0.15f, B = 0.50f, C = 0.10f, D = 0.20f, E = 0.02f, F = 0.30f;
  return (x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F) - E / F;
}
static void prepararLuts(MiniMotor *m, int pq) {
  if (m->lutPronta == (pq ? 2 : 1)) return;
  float branco = hable(10.0f);
  for (int i = 0; i < 1024; i++) {
    float e = i / 1023.0f, lin;
    if (pq) {
      const float m1 = 2610.f / 16384, m2 = 2523.f / 4096 * 128;
      const float c1 = 3424.f / 4096, c2 = 2413.f / 4096 * 32, c3 = 2392.f / 4096 * 32;
      float p = powf(e, 1 / m2);
      float num = p - c1; if (num < 0) num = 0;
      lin = powf(num / (c2 - c3 * p), 1 / m1) * 100.0f;      // 100 nit = 1.0
    } else {
      const float a = 0.17883277f, b = 0.28466892f, c = 0.55991073f;
      lin = (e <= 0.5f ? e * e / 3 : (expf((e - c) / a) + b) / 12) * 10.0f;
    }
    m->lutPq[i] = hable(lin) / branco;
  }
  for (int i = 0; i < 4096; i++) m->lutGama[i] = powf(i / 4095.0f, 1 / 2.4f);
  m->lutPronta = pq ? 2 : 1;
}

static inline float clamp01(float v) { return v < 0 ? 0 : v > 1 ? 1 : v; }
static inline int clamp255(float v) { return v < 0 ? 0 : v > 255 ? 255 : (int)(v + 0.5f); }

// Average of plane p over [x0,x1) x [y0,y1), at most 4x4 samples.
static float media(const AVFrame *f, int p, int prof, int x0, int x1, int y0, int y1) {
  int sx = (x1 - x0 + 3) / 4, sy = (y1 - y0 + 3) / 4, n = 0;
  long acc = 0;
  if (sx < 1) sx = 1;
  if (sy < 1) sy = 1;
  for (int y = y0; y < y1; y += sy) {
    const uint8_t *l = f->data[p] + (size_t)y * (size_t)f->linesize[p];
    if (prof > 8) for (int x = x0; x < x1; x += sx) { acc += ((const uint16_t *)l)[x]; n++; }
    else for (int x = x0; x < x1; x += sx) { acc += l[x]; n++; }
  }
  return n ? (float)acc / (float)n : 0.0f;
}

// Shrinks the decoded frame into m->saida (yuvj420p 320x180, letter/pillarboxed).
static int reduzir(MiniMotor *m) {
  AVFrame *f = m->quadro, *o = m->saida;
  const AVPixFmtDescriptor *d = av_pix_fmt_desc_get((enum AVPixelFormat)f->format);
  if (!d || (d->flags & AV_PIX_FMT_FLAG_RGB) || d->nb_components < 3 || d->log2_chroma_w != 1 || d->log2_chroma_h != 1)
    return -1;                                   // 4:2:0 only (what TV sources are)
  int prof = d->comp[0].depth, max = (1 << prof) - 1;
  int cheio = f->color_range == AVCOL_RANGE_JPEG || f->format == AV_PIX_FMT_YUVJ420P;
  float yMin = cheio ? 0 : 16 << (prof - 8), yEsc = cheio ? max : 219 << (prof - 8);
  float cMeio = 128 << (prof - 8), cEsc = cheio ? max : 224 << (prof - 8);
  float kr = 0.2126f, kb = 0.0722f;              // BT.709
  if (f->colorspace == AVCOL_SPC_BT2020_NCL || f->colorspace == AVCOL_SPC_BT2020_CL) { kr = 0.2627f; kb = 0.0593f; }
  else if (f->colorspace == AVCOL_SPC_BT470BG || f->colorspace == AVCOL_SPC_SMPTE170M) { kr = 0.299f; kb = 0.114f; }
  else if (f->colorspace == AVCOL_SPC_UNSPECIFIED && f->height < 720) { kr = 0.299f; kb = 0.114f; }
  float kg = 1 - kr - kb;
  int pq = f->color_trc == AVCOL_TRC_SMPTE2084, hlg = f->color_trc == AVCOL_TRC_ARIB_STD_B67;
  if (pq || hlg) prepararLuts(m, pq);
  // Display aspect: sample aspect ratio from the frame, else the stream.
  AVRational sar = f->sample_aspect_ratio.num ? f->sample_aspect_ratio : m->st->sample_aspect_ratio;
  double dw = f->width * (sar.num > 0 && sar.den > 0 ? (double)sar.num / sar.den : 1.0);
  double s = fmin(MINI_W / dw, (double)MINI_H / f->height);
  int sw = (int)lround(dw * s), sh = (int)lround(f->height * s);
  sw = sw < 2 ? 2 : sw > MINI_W ? MINI_W : sw & ~1;
  sh = sh < 2 ? 2 : sh > MINI_H ? MINI_H : sh & ~1;
  int ox = ((MINI_W - sw) / 2) & ~1, oy = ((MINI_H - sh) / 2) & ~1;

  av_frame_unref(o);
  o->format = AV_PIX_FMT_YUVJ420P; o->width = MINI_W; o->height = MINI_H;
  o->color_range = AVCOL_RANGE_JPEG;
  if (av_frame_get_buffer(o, 32) < 0) return -1;
  for (int y = 0; y < MINI_H; y++) memset(o->data[0] + (size_t)y * (size_t)o->linesize[0], 0, MINI_W);
  for (int y = 0; y < MINI_H / 2; y++) {
    memset(o->data[1] + (size_t)y * (size_t)o->linesize[1], 128, MINI_W / 2);
    memset(o->data[2] + (size_t)y * (size_t)o->linesize[2], 128, MINI_W / 2);
  }
  // Per 2x2 output block: four luma samples, one chroma pair.
  for (int by = 0; by < sh; by += 2) {
    for (int bx = 0; bx < sw; bx += 2) {
      float cbAcc = 0, crAcc = 0;
      for (int dy = 0; dy < 2; dy++) for (int dx = 0; dx < 2; dx++) {
        int x = bx + dx, y = by + dy;
        int x0 = (int)((long)x * f->width / sw), x1 = (int)((long)(x + 1) * f->width / sw);
        int y0 = (int)((long)y * f->height / sh), y1 = (int)((long)(y + 1) * f->height / sh);
        if (x1 <= x0) x1 = x0 + 1;
        if (y1 <= y0) y1 = y0 + 1;
        float Y = media(f, 0, prof, x0, x1, y0, y1);
        int cx0 = x0 / 2, cx1 = (x1 + 1) / 2, cy0 = y0 / 2, cy1 = (y1 + 1) / 2;
        if (cx1 > (f->width + 1) / 2) cx1 = (f->width + 1) / 2;
        if (cy1 > (f->height + 1) / 2) cy1 = (f->height + 1) / 2;
        float U = media(f, 1, prof, cx0, cx1, cy0, cy1), V = media(f, 2, prof, cx0, cx1, cy0, cy1);
        float yn = (Y - yMin) / yEsc, cb = (U - cMeio) / cEsc, cr = (V - cMeio) / cEsc;
        float r = yn + 2 * (1 - kr) * cr, b = yn + 2 * (1 - kb) * cb;
        float g = (yn - kr * r - kb * b) / kg;
        if (pq || hlg) {
          // PQ/HLG signal -> linear (LUT, already Hable-tonemapped) -> BT.2020 to BT.709 -> gamma.
          float lr = m->lutPq[(int)(clamp01(r) * 1023 + 0.5f)];
          float lg = m->lutPq[(int)(clamp01(g) * 1023 + 0.5f)];
          float lb = m->lutPq[(int)(clamp01(b) * 1023 + 0.5f)];
          float r7 = 1.6605f * lr - 0.5876f * lg - 0.0728f * lb;
          float g7 = -0.1246f * lr + 1.1329f * lg - 0.0083f * lb;
          float b7 = -0.0182f * lr - 0.1006f * lg + 1.1187f * lb;
          r = m->lutGama[(int)(clamp01(r7) * 4095 + 0.5f)];
          g = m->lutGama[(int)(clamp01(g7) * 4095 + 0.5f)];
          b = m->lutGama[(int)(clamp01(b7) * 4095 + 0.5f)];
        }
        float R = clamp01(r) * 255, G = clamp01(g) * 255, B = clamp01(b) * 255;
        o->data[0][(size_t)(oy + y) * (size_t)o->linesize[0] + (size_t)(ox + x)] =
          (uint8_t)clamp255(0.299f * R + 0.587f * G + 0.114f * B);
        cbAcc += -0.168736f * R - 0.331264f * G + 0.5f * B;
        crAcc += 0.5f * R - 0.418688f * G - 0.081312f * B;
      }
      size_t ci = (size_t)((oy + by) / 2), cj = (size_t)((ox + bx) / 2);
      o->data[1][ci * (size_t)o->linesize[1] + cj] = (uint8_t)clamp255(cbAcc / 4 + 128);
      o->data[2][ci * (size_t)o->linesize[2] + cj] = (uint8_t)clamp255(crAcc / 4 + 128);
    }
  }
  return 0;
}

static int codificar(MiniMotor *m, unsigned char **jpg, int *njpg) {
  if (abrirJpeg(m) < 0) return -1;
  m->saida->pts = m->jpts++;           // the encoder wants rising timestamps
  if (avcodec_send_frame(m->jc, m->saida) < 0) return -1;
  int r = avcodec_receive_packet(m->jc, m->jpkt);
  if (r < 0) return -1;
  *jpg = malloc((size_t)m->jpkt->size);
  if (!*jpg) { av_packet_unref(m->jpkt); return -1; }
  memcpy(*jpg, m->jpkt->data, (size_t)m->jpkt->size);
  *njpg = m->jpkt->size;
  av_packet_unref(m->jpkt);
  return 0;
}

// ------------------------------------------------------------------ one frame
int mini_motor_quadro(MiniMotor *m, long alvoMs, long tolMs, int threads, volatile int *cancelar,
                      unsigned char **jpg, int *njpg, MiniQuadro *q, char *erro, size_t nerro) {
  int64_t ts;
  unsigned t0 = agoraMs(), td = 0;
  long long lidos0;
  int pacotes = 0, r = -1;
  MiniQuadro z = {0};
  if (!q) q = &z;
  memset(q, 0, sizeof *q);
  *jpg = NULL; *njpg = 0;
  if (!m) { erroDe(erro, nerro, "not open"); return -1; }
  if (threads < 1) threads = 1;
  if (threads > 3) threads = 3;
  lidos0 = m->lidos;
  int pedidos0 = m->pedidos;
  long preKB = 0;
  if (abrirDecoder(m, threads) < 0) { erroDe(erro, nerro, "decoder failed to open"); return -1; }
  if (m->semIndice) {
    if (chaveTs(m, alvoMs, tolMs, cancelar, &pacotes, erro, nerro) < 0) goto fim;
    ts = tsPacote(m->pkt);
  } else {
    if (entradaChave(m, alvoMs, tolMs, &ts, NULL) < 0) { erroDe(erro, nerro, "no keyframe for this time"); return -1; }
    if (av_seek_frame(m->fc, m->vs, ts, AVSEEK_FLAG_BACKWARD) < 0) { erroDe(erro, nerro, "seek failed"); return -1; }
  }
  avcodec_flush_buffers(m->cc);
  while (pacotes < MAX_PACOTES) {
    if (cancelar && *cancelar) { erroDe(erro, nerro, "cancelled"); goto fim; }
    if (!m->semIndice) {
      int rr = av_read_frame(m->fc, m->pkt);
      if (rr < 0) { erroDe(erro, nerro, rr == AVERROR(EIO) ? "source read failed" : "end of file before the keyframe"); goto fim; }
      pacotes++;
      if (m->pkt->stream_index != m->vs) { av_packet_unref(m->pkt); continue; }
      if (!(m->pkt->flags & AV_PKT_FLAG_KEY)) { av_packet_unref(m->pkt); continue; }
    }
    if (m->pkt->size > MAX_PACOTE) { av_packet_unref(m->pkt); erroDe(erro, nerro, "keyframe too big"); goto fim; }
    q->bytesPacote = m->pkt->size;
    unsigned d0 = agoraMs();
    m->buffersNoPacote = 0;
    preIniciar(m);                              // the next keyframe downloads while this one decodes
    int s1 = avcodec_send_packet(m->cc, m->pkt);
    av_packet_unref(m->pkt);
    if (s1 < 0 && s1 != AVERROR(EAGAIN)) { erroDe(erro, nerro, "decode failed"); goto fim; }
    avcodec_send_packet(m->cc, NULL);           // drain: the keyframe comes out now
    int rf = avcodec_receive_frame(m->cc, m->quadro);
    td = agoraMs() - d0;
    if (rf < 0) { erroDe(erro, nerro, "keyframe did not decode"); goto fim; }
    if (m->quadro->width > 4096 || m->quadro->height > 2304) { erroDe(erro, nerro, "picture size not supported (above 4K)"); goto fim; }
    if (m->semIndice) { m->info.largura = m->quadro->width; m->info.altura = m->quadro->height; }  // TS: now known
    break;
  }
  if (pacotes >= MAX_PACOTES) { erroDe(erro, nerro, "no keyframe found near the target"); goto fim; }
  q->ptsMs = msDe(m, m->quadro->best_effort_timestamp != AV_NOPTS_VALUE ? m->quadro->best_effort_timestamp : m->quadro->pts);
  if (q->ptsMs < 0) q->ptsMs = msDe(m, ts);
  if (reduzir(m) < 0) { erroDe(erro, nerro, "pixel format not supported"); goto fim; }
  av_frame_unref(m->quadro);                     // the big frame goes away before the JPEG work
  if (codificar(m, jpg, njpg) < 0) { erroDe(erro, nerro, "jpeg encode failed"); goto fim; }
  r = 0;
fim:
  av_frame_unref(m->quadro);
  av_frame_unref(m->saida);
  if (m->cc) avcodec_flush_buffers(m->cc);       // leaves draining mode and drops every reference
  q->decodMs = td;
  q->totalMs = agoraMs() - t0;
  preKB = preTerminar(m) / 1024;
  q->bytesLidos = (long)(m->lidos - lidos0) - preKB * 1024;
  q->threads = threads;
  q->pedidos = m->pedidos - pedidos0;
  q->adiantadoKB = preKB;
  return r;
}

void mini_motor_fechar(MiniMotor *m) {
  if (!m) return;
  preTerminar(m);
  avcodec_free_context(&m->cc);
  av_frame_free(&m->cinza);
  avcodec_free_context(&m->jc);
  av_frame_free(&m->quadro); av_frame_free(&m->saida);
  av_packet_free(&m->pkt); av_packet_free(&m->jpkt);
  if (m->fc) avformat_close_input(&m->fc);
  if (m->io) { av_freep(&m->io->buffer); avio_context_free(&m->io); }
  for (int i = 0; i < BLOCOS; i++) free(m->cache[i].dados);
  free(m);
}

// ------------------------------------------------------------------ fingerprint
static void sha256(const unsigned char *d, size_t n, uint8_t out[32]) {
  struct AVSHA *s = av_sha_alloc();
  if (!s) { memset(out, 0, 32); return; }
  av_sha_init(s, 256);
  av_sha_update(s, d, n);
  av_sha_final(s, out);
  av_free(s);
}
void mini_impressao(uint64_t tamanho, const unsigned char *cabeca, size_t ncabeca,
                    const unsigned char *cauda, size_t ncauda, char saida[65]) {
  uint8_t h1[32], h2[32], fim[32], buf[2 + 8 + 64];
  sha256(cabeca, ncabeca, h1);
  sha256(cauda, ncauda, h2);
  buf[0] = 'v'; buf[1] = '1';
  for (int i = 0; i < 8; i++) buf[2 + i] = (uint8_t)(tamanho >> (56 - 8 * i));
  memcpy(buf + 10, h1, 32); memcpy(buf + 42, h2, 32);
  sha256(buf, sizeof buf, fim);
  for (int i = 0; i < 32; i++) snprintf(saida + 2 * i, 3, "%02x", fim[i]);
}

#else
typedef int mini_motor_vazio;   // ISO C: a translation unit must not be empty
#endif
