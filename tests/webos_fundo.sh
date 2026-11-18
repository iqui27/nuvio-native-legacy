#!/bin/bash
# LG: saida e volta do app (Home) com um filme aberto. Compila a maquina REAL
# de src/video.c (do "SAIDA E VOLTA" ate video_definir_reconexao) contra um
# pipeline falso, e confere: ponto e faixas guardados, pipeline solto na
# saida, reabertura pausada e muda no ponto, Play durante a reabertura, erro
# com uma nova tentativa, desistencia em 20 s (45 s no caminho do DV),
# trailer/canal/fonte que falhou intocados.
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-webos-fundo.XXXXXX")
trap 'rm -rf "$work"' EXIT
python3 - "$work/t.c" <<'PY'
from pathlib import Path
import sys
src = Path('src/video.c').read_text()
ini = src.index('// ---------------------------------------------------------------- SAIDA E VOLTA\nstatic void fundoZerar')
fim = src.index('void video_definir_reconexao(int sim)', ini)
decl_ini = src.index('#ifndef FUNDO_PRAZO_MS')
decl_fim = src.index('static Uint32 fundoEm, fundoRetEm, fundoPousouEm;', decl_ini)
decl_fim = src.index('\n', decl_fim) + 1
pc = src.index('int video_pausa_confirmada(void) {')
pausa = src[pc:src.index('\n}\n', pc) + 3]
Path(sys.argv[1]).write_text(r'''
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
typedef uint32_t Uint32;
#include "video_reconexao.h"
#include "video_dvretry.h"
static Uint32 relogio = 1000;
static Uint32 SDL_GetTicks(void) { return relogio; }
static int ligado = 1, pronto, modoLoad, reconPermitida = 1, reconIniciou, reconErroPend, reconErroRede;
static int falhou, terminou, tocando, pausaPedida, recarregarPausa = -1, audioNaoSup;
static int nAudio, audioAtual, legAtual = -1, dvSilencio, dvSilenciado, dtsModoDv;
static int dvRecuado, dtsTentou, dtsRevisao, dtsNativePending, dtsEstado, dvSondado, dvMkvPerfil, dvAudios;
static int dvAudioMkvOrd, dvSegurar, dvSessaoN, dvFonteAberta;
static void *dtsSessao;
static char dtsSaida[64], dvAudioDe[16], urlAtual[256] = "https://cdn/x.mkv", legUrlAtual[1024], midia[64], erroTexto[96];
static double posSeg, durSeg, posAoCarregar, seekAlvo, dvEntradaSeg;
static Uint32 seekEm, seekEnvEm, seekRetryEm, bufferandoDesde, mkvRetryEm;
static NvReconexao recon;
static NvDvSonda mkvRetry;
static void soLog(void) {}
static int volume = 100, paradas, cargas, cargaAud = -9, cargaLeg = -9, pausas, cargaFalha;
static char cargaLegUrl[1024];
static void chamar(const char *m, const char *b, void (*cb)(void)) {
  (void)cb;
  if (!strcmp(m, "setVolume")) volume = strstr(b, "\"volume\":0}") ? 0 : 100;
}
static void pararSessao(void) { paradas++; midia[0] = 0; dtsSessao = NULL; pronto = tocando = 0; pausaPedida = 0; recarregarPausa = -1; seekEnvEm = 0; }
static int recarregarMesmaFonte(double alvo, int aud, int leg, const char *legUrl) {
  pararSessao(); cargas++;
  if (cargaFalha) { cargaFalha--; return 0; }
  posSeg = durSeg = 0; cargaAud = aud; cargaLeg = leg;
  snprintf(cargaLegUrl, sizeof cargaLegUrl, "%s", legUrl);
  if (alvo > 1.0) posAoCarregar = alvo;
  return 1;
}
/* O caminho do DTS/DV (iniciarDts): abre com a lista dele, no posSeg, com a
 * pausa pedida; sem abrir, volta ao player da TV no ponto (dtsVoltarNativo). */
static int bindVivoT, dtsAbre = 1, dtsInicios, dtsPausado;
static double dtsPonto;
static int bindAtivo(void) { return bindVivoT; }
static int iniciarDts(int s) {
  (void)s; dtsInicios++; dtsTentou = 1;
  if (!dtsAbre) { recarregarMesmaFonte(posSeg, -1, -1, legUrlAtual); return 0; }
  dtsPonto = posSeg; dtsPausado = pausaPedida; dtsSessao = (void *)1; pronto = 0;
  return 1;
}
static int fundoQuerTocarDe(void);
static void video_pausar(int p);
''' + src[decl_ini:decl_fim] + src[ini:fim] + 'static int pausaConfirmada;\nstatic int video_reconectando(void) { return 0; }\n' + pausa + r'''
static void video_pausar(int p) {
  if (fundoSusp) { fundoQuerTocar = !p; return; }
  if (fundoRet) { fundoQuerTocar = !p; if (recarregarPausa >= 0) recarregarPausa = !!p; pausaPedida = !!p; }
  if (midia[0]) { pausas++; tocando = !p; }
}
static int fundoQuerTocarDe(void) { return fundoQuerTocar; }
/* O pipeline falso: load -> mediaId (mudo, como o aoCarregar) -> loadCompleted
 * (seek ao ponto e pausa, como o loadCompleted) -> seekDone. */
static void midiaChegou(void) {
  snprintf(midia, sizeof midia, "m%d", cargas);
  if (__atomic_exchange_n(&fundoMudoPend, 0, __ATOMIC_ACQ_REL)) { volume = 0; __atomic_store_n(&fundoMudo, 1, __ATOMIC_RELEASE); }
  tocando = 1;
}
static void carregou(double durReal) {
  durSeg = durReal; pronto = 1;
  if (recarregarPausa >= 0) {
    int pausado = recarregarPausa;
    seekAlvo = posAoCarregar; posAoCarregar = 0; seekEnvEm = relogio;
    recarregarPausa = -1;
    if (pausado) { pausaPedida = 1; tocando = 0; }
  }
}
static void seekDone(void) { posSeg = seekAlvo; seekEnvEm = 0; }
static void quadros(int n) { while (n--) { relogio += 100; fundoVigiar(relogio); if (reconErroPend && !fundoRet) reconErroPend = 0; } }
static void tocandoEm(double p) {
  fundoZerar(); memset(&recon, 0, sizeof recon);
  ligado = 1; pronto = 1; tocando = 1; pausaPedida = 0; falhou = terminou = 0; modoLoad = 0; reconPermitida = 1;
  posSeg = p; durSeg = 7200; nAudio = 3; audioAtual = 2; legAtual = 1; legUrlAtual[0] = 0;
  snprintf(midia, sizeof midia, "m0"); dtsSessao = NULL; dtsModoDv = 0;
  seekEm = seekEnvEm = seekRetryEm = 0; recarregarPausa = -1; volume = 100;
  paradas = cargas = pausas = cargaFalha = 0;
  bindVivoT = 0; dtsAbre = 1; dtsInicios = 0; dtsPausado = -1; dtsPonto = -1;
}
#define OK(c, msg) do { if (!(c)) { printf("FAIL: %s (line %d)\n", msg, __LINE__); return 1; } } while (0)
int main(void) {
  /* 1. Home tocando: ponto, faixas, pipeline solto, relogio e barra no ponto. */
  tocandoEm(1234.5);
  OK(video_webos_fundo(-1.0, "window hidden") == 1, "suspends a playing VOD");
  OK(paradas == 1 && !midia[0], "pipeline released on leaving");
  OK(fundoAlvo == 1234.5 && fundoAudio == 2 && fundoLeg == 1, "point and tracks kept");
  OK(video_webos_fundo(-1.0, "app to background") == 1 && paradas == 1, "second hide signal is a no-op");
  quadros(5); OK(cargas == 0, "nothing reopens while away");
  /* 2. Volta: mesma fonte, mesmas faixas, pausada, muda; pousa e devolve o som. */
  video_webos_voltou("window shown"); video_webos_voltou("focus gained");
  OK(fundoVoltouPend == 1, "return noted once");
  fundoVoltouPend = 0; fundoComecar();
  OK(video_webos_retomada_evento() == 1 && video_webos_retomada_evento() == 0, "event 1 once");
  OK(cargas == 1 && cargaAud == 2 && cargaLeg == 1 && posAoCarregar == 1234.5 && recarregarPausa == 1, "reload at the point, same tracks, paused");
  midiaChegou(); OK(volume == 0, "muted before play");
  quadros(3); OK(fundoRet, "still reopening before loadCompleted");
  carregou(7200); quadros(2); OK(fundoRet, "waits for seekDone");
  seekDone(); quadros(1);
  OK(!fundoRet && volume == 100 && !tocando && posSeg == 1234.5, "landed paused at the point with sound back");
  /* 3. Play durante a reabertura: termina tocando. */
  tocandoEm(600); video_webos_fundo(-1.0, "window hidden"); fundoComecar();
  video_pausar(0);
  OK(recarregarPausa == 0 && fundoQuerTocar, "Play during the reopen is kept");
  midiaChegou(); carregou(7200); seekDone(); quadros(1);
  OK(!fundoRet && tocando && volume == 100, "ends playing");
  /* Play na tecla que trouxe o app de volta (main.c marca a volta, o player
   * recebe a mesma tecla antes do video_bombear): termina tocando. */
  tocandoEm(300); video_webos_fundo(-1.0, "window hidden");
  video_webos_voltou("key pressed"); video_pausar(0);
  fundoVoltouPend = 0; fundoComecar();
  OK(fundoQuerTocar == 1 && recarregarPausa == 0, "Play pressed with the returning key is kept");
  midiaChegou(); carregou(7200); seekDone(); quadros(1); OK(!fundoRet && tocando, "and it ends playing");
  /* "Manter o video pronto ao sair": nada a reter fora da tela nem reabrindo. */
  tocandoEm(80); pausaPedida = 1; pausaConfirmada = 1; tocando = 0;
  OK(video_pausa_confirmada(), "a normal paused session can be retained");
  video_webos_fundo(-1.0, "window hidden"); fundoComecar(); video_webos_retomada_evento();
  midiaChegou(); carregou(7200); pausaConfirmada = 1;
  OK(!video_pausa_confirmada(), "no retained session while reopening (muted, seek pending)");
  seekDone(); quadros(1); OK(!fundoRet && video_pausa_confirmada(), "retainable again once landed");
  /* 4. Erro: uma nova tentativa; o segundo erro desiste (evento 2). */
  tocandoEm(50); video_webos_fundo(-1.0, "window hidden"); fundoComecar(); video_webos_retomada_evento();
  reconErroPend = 1; quadros(1); OK(cargas == 2 && fundoRet, "one more try after an error");
  reconErroPend = 1; quadros(1); OK(!fundoRet && fundoDesistiu && video_webos_retomada_evento() == 2, "second error gives up");
  /* Desistir com o pipeline carregado (mudo): ele e solto, e a posicao fica
   * no ponto para o progresso gravado na saida. */
  tocandoEm(60); video_webos_fundo(-1.0, "window hidden"); fundoComecar(); video_webos_retomada_evento();
  midiaChegou(); carregou(7200); reconErroPend = 1; quadros(1);
  midiaChegou(); carregou(7200); posSeg = 3; reconErroPend = 1; quadros(1);
  OK(fundoDesistiu && !pronto && !midia[0] && posSeg == 60, "give-up releases the loaded pipeline, position stays on the point");
  /* 5. Sem imagem em 20 s desiste; no caminho do DV, 45 s. */
  tocandoEm(50); video_webos_fundo(-1.0, "window hidden"); fundoComecar(); video_webos_retomada_evento();
  quadros(199); OK(fundoRet, "still waiting at 19.9 s"); quadros(2); OK(fundoDesistiu, "gives up at 20 s");
  tocandoEm(50); dtsModoDv = 1; dtsSessao = (void *)1; video_webos_fundo(-1.0, "window hidden"); fundoComecar(); video_webos_retomada_evento();
  quadros(1); OK(fundoEraDv && cargas == 0 && dtsInicios == 1 && dtsModoDv, "DV session reopens through its own path");
  quadros(300); OK(fundoRet, "DV path still waiting at 30 s"); quadros(160); OK(fundoDesistiu, "DV path gives up at 45 s");
  /* 6. Load que nem sai: conta como erro (nova tentativa). */
  tocandoEm(70); video_webos_fundo(-1.0, "window hidden"); cargaFalha = 1; fundoComecar();
  OK(reconErroPend == 1, "failed load flags an error"); quadros(1); OK(cargas == 2 && fundoRet, "and retries");
  /* 7. Alvos: scrub da tela, seek em voo, reconexao, saida durante a reabertura. */
  tocandoEm(100); OK(video_webos_fundo(900.0, "window hidden") && fundoAlvo == 900.0, "scrub start wins");
  tocandoEm(100); seekEm = relogio + 350; seekAlvo = 444; video_webos_fundo(-1.0, "window hidden"); OK(fundoAlvo == 444, "seek in flight wins");
  tocandoEm(100); recon.pendente = 1; recon.tentativa = 1; recon.alvo = 333; pronto = 0;
  video_webos_fundo(-1.0, "window hidden"); OK(fundoAlvo == 333, "reconnect target wins");
  tocandoEm(100); video_webos_fundo(-1.0, "window hidden"); fundoComecar(); midiaChegou();
  OK(video_webos_fundo(-1.0, "window hidden") == 1 && fundoAlvo == 100 && fundoSusp && cargaAud == 2, "leaving again while reopening keeps the point and tracks");
  /* 8. DTS: de volta direto pela conversao, com a lista dela (o indice da
   * faixa nao vale na lista da TV), no ponto, pausado, sem load nativo nem mudo. */
  tocandoEm(2500); dtsSessao = (void *)1; video_webos_fundo(-1.0, "window hidden");
  OK(fundoEraDts && !dtsSessao, "DTS worker closed on leaving");
  bindVivoT = 1; fundoComecar(); video_webos_retomada_evento(); quadros(3);
  OK(dtsInicios == 0 && cargas == 0, "waits for the old video plane bind");
  bindVivoT = 0; quadros(1);
  OK(dtsInicios == 1 && cargas == 0 && dtsPonto == 2500 && dtsPausado == 1 && volume == 100 && audioAtual == 2,
     "DTS reopens through the converter at the point, paused, same track list, no native load");
  pronto = 1; quadros(1); OK(!fundoRet, "lands when the converter is prepared");
  /* DTS path that does not open: the TV player at the point, muted and paused. */
  tocandoEm(1800); dtsSessao = (void *)1; video_webos_fundo(-1.0, "window hidden");
  dtsAbre = 0; fundoComecar(); quadros(1);
  OK(dtsInicios == 1 && cargas == 1 && recarregarPausa == 1 && posAoCarregar == 1800, "converter fallback reopens paused at the point");
  midiaChegou(); OK(volume == 0, "fallback load muted");
  carregou(7200); seekDone(); quadros(1); OK(!fundoRet && volume == 100 && posSeg == 1800, "fallback lands with sound back");
  /* 9. Intocados: trailer (sem reconexao), canal ao vivo, fonte que falhou, fim. */
  tocandoEm(10); reconPermitida = 0; OK(video_webos_fundo(-1.0, "window hidden") == 0 && paradas == 0, "trailer untouched");
  tocandoEm(10); modoLoad = 1; OK(video_webos_fundo(-1.0, "window hidden") == 0, "live untouched");
  tocandoEm(10); falhou = 1; OK(video_webos_fundo(-1.0, "window hidden") == 0, "failed source untouched");
  tocandoEm(10); terminou = 1; OK(video_webos_fundo(-1.0, "window hidden") == 0, "ended title untouched");
  /* Sair com o titulo seguinte ainda abrindo (sem duracao): a barra nao
   * mostra a duracao do anterior. */
  tocandoEm(80); video_webos_fundo(-1.0, "window hidden");   /* titulo 1: 7200 s */
  fundoZerar();                                              /* video_tocar do titulo 2 */
  durSeg = 0; pronto = 0; posSeg = 0;
  video_webos_fundo(-1.0, "window hidden");
  OK(fundoDur == 0, "no stale duration from the previous title");
  /* 10. Legenda externa volta pela URL. */
  tocandoEm(10); snprintf(legUrlAtual, sizeof legUrlAtual, "http://127.0.0.1/sub.srt");
  video_webos_fundo(-1.0, "window hidden"); fundoComecar();
  OK(cargaLeg == -1 && !strcmp(cargaLegUrl, "http://127.0.0.1/sub.srt"), "external subtitle comes back by URL");
  puts("PASS: LG leave/return: suspend, paused muted reopen at the point, Play, retry, give-up, targets, untouched cases");
  return 0;
}
''')
PY
cc -std=gnu11 -Wall -Wextra -Wno-unused-function -Wno-unused-variable -Wno-unused-but-set-variable -Isrc "$work/t.c" -o "$work/t" -lm
"$work/t"
