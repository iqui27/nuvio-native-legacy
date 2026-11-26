// SEEK-BAR THUMBNAILS MADE ON THE DEVICE (miniplat.h): the decoding engine.
//
// Pure: no SDL, no GL, no network of its own. The caller hands in a range
// reader (miniatura.c reads the source link through rede; the host test reads
// a file). Opens the container with libavformat (Matroska / MP4), keeps the
// container index, and makes ONE 320x180 JPEG from ONE keyframe at a time:
//
//   seek to the keyframe nearest the target -> read that packet only ->
//   decode it (intra frames only, no loop filters) -> shrink it in our own C
//   (with PQ/HLG -> SDR) -> encode JPEG -> free the frame and flush.
//
// So at most one decoded source frame is ever in memory (a 4K 10-bit frame is
// ~25 MB). mini_motor_memoria() estimates what the next frame needs, for the
// caller's free-memory check before every frame.
//
// Built where miniplat.h says thumbnails are made on the device and FFmpeg is
// linked (the .c is empty elsewhere).
#ifndef NV_MINIATURA_MOTOR_H
#define NV_MINIATURA_MOTOR_H
#include <stddef.h>
#include <stdint.h>

#define MINI_W 320
#define MINI_H 180

// Reads bytes [ini, fim] into dst (fim - ini + 1 bytes). Returns the number of
// bytes read (short only at the end of the file) or -1. Called from the
// engine's thread only.
typedef long (*MiniLer)(void *usuario, int64_t ini, int64_t fim, unsigned char *dst);

typedef struct MiniMotor MiniMotor;

typedef struct {
  long duracaoMs;
  int largura, altura;
  int hdr;              // 1 = PQ or HLG (tonemapped to SDR)
  int bits;             // 8 / 10
  char codec[16];       // "h264" / "hevc"
  char formato[16];     // "matroska" / "mp4" / "mpegts"
  int chaves;           // keyframes in the container index (0 = none known; mpegts seeks by byte)
} MiniInfo;

typedef struct {
  long ptsMs;           // time of the frame actually shown
  unsigned decodMs;     // decode only
  unsigned totalMs;     // seek + read + decode + shrink + JPEG
  long bytesLidos;      // bytes read from the source for this frame
  long bytesPacote;     // keyframe packet size
  int threads;
  int pedidos;          // range requests on the demuxer path
  long adiantadoKB;     // prefetched for the NEXT frame during this decode (not in bytesLidos)
} MiniQuadro;

// Read tuning. Defaults (mini_motor_config_padrao) are the TV-tested values.
typedef struct {
  int lerBlocos;        // 256 KiB blocks per range request, 1..7 (default 4 = 1 MiB)
  int prefetch;         // 1 = fetch the next keyframe (mini_motor_proximo) during a decode
  int tsBlocos;         // MPEG-TS: blocks per request while reading forward (default 7)
} MiniConfig;

// Opens the container. `tamanho` = total file size. NULL on failure, with a
// short English reason in `erro` (never a URL).
MiniMotor *mini_motor_abrir(MiniLer ler, void *usuario, int64_t tamanho,
                            char *erro, size_t nerro);
const MiniInfo *mini_motor_info(const MiniMotor *m);
// Time (ms) of the keyframe that a frame for `alvoMs` would come from: the
// nearest keyframe within `tolMs`, else the one before. -1 when the index does
// not know. Two targets with the same answer share one decode.
long mini_motor_chave(MiniMotor *m, long alvoMs, long tolMs);
// Bytes one more decoded frame needs (frame + decoder working set), estimate.
size_t mini_motor_memoria(const MiniMotor *m, int threads);
// Makes the JPEG for `alvoMs`. `threads` = 1..3 decoding threads (slice/WPP
// threading only; frame threading is never used). Returns 0 and a malloc'd
// JPEG in *jpg (caller frees), or -1 with `erro` set. `cancelar` (optional)
// is polled between packets.
int mini_motor_quadro(MiniMotor *m, long alvoMs, long tolMs, int threads,
                      volatile int *cancelar, unsigned char **jpg, int *njpg,
                      MiniQuadro *q, char *erro, size_t nerro);
// Frees the decoder and its buffer pools (the container stays open).
void mini_motor_soltar_decoder(MiniMotor *m);
void mini_motor_config_padrao(MiniConfig *c);
void mini_motor_config(MiniMotor *m, const MiniConfig *c);
// The frame the caller will ask for next (-1 = unknown): its keyframe's bytes
// are prefetched during the next mini_motor_quadro decode (MiniConfig.prefetch).
void mini_motor_proximo(MiniMotor *m, long alvoMs, long tolMs);
// Bytes read from the source so far.
long long mini_motor_lidos(const MiniMotor *m);
void mini_motor_fechar(MiniMotor *m);

// Content fingerprint (same as OpenSeek): SHA-256("v1" || u64be(size) ||
// SHA-256(first 64 KiB) || SHA-256(last 64 KiB)), lowercase hex (65 bytes).
void mini_impressao(uint64_t tamanho, const unsigned char *cabeca, size_t ncabeca,
                    const unsigned char *cauda, size_t ncauda, char saida[65]);
#endif
