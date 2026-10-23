#ifndef NV_DRIVETV_H
#define NV_DRIVETV_H
#include "rede.h"
#include <stdint.h>
#define DRIVE_APPLICACAO_SCOPE "https://www.googleapis.com/auth/drive.file"
typedef struct {
  const char *cliente,*segredo;
  RedeJob *job;
  int (*pedir)(void *,const RedePedido *,RedeResposta *);
  void *ctx;
} DriveTv;
typedef struct {
  char dispositivo[2049],codigo[129],url[257];
  uint64_t expiraMs,proximaMs;
  unsigned intervalo;
} DriveTvCodigo;
typedef struct {
  char acesso[4097],renovacao[4097];
  uint64_t expiraMs;
} DriveTvToken;
enum {
  DRIVE_TV_AGUARDAR=0,DRIVE_TV_OK=1,DRIVE_TV_INVALIDO=-1,
  DRIVE_TV_REDE=-2,DRIVE_TV_NEGADO=-3,DRIVE_TV_EXPIRADO=-4,
  DRIVE_TV_CONFIGURACAO=-5,DRIVE_TV_RESPOSTA=-6,DRIVE_TV_CANCELADO=-7,
  DRIVE_TV_REPETIR=-8
};
// Worker-only, exactly one HTTP request at most per call, no sleeps/threads.
// agoraMs is the caller's monotonic clock. Polling enforces Google's interval,
// slow_down, transport backoff, expiry and RedeJob profile cancellation.
// This limited-input flow must not be used as a phone OAuth fallback.
int drivetv_iniciar(const DriveTv *,uint64_t agoraMs,DriveTvCodigo *);
int drivetv_consultar(const DriveTv *,DriveTvCodigo *,uint64_t agoraMs,DriveTvToken *);
int drivetv_renovar(const DriveTv *,const char *renovacao,uint64_t agoraMs,DriveTvToken *);
#endif
