// farol_produtores_tv — issue 9 do lote sense (PLANO.md secao 5, aceite do
// BRIEF). O anel do Farol recebe o erro nativo de cada backend ja normalizado
// pelo farolerro, sem URL, titulo nem texto livre. Cada modo compila o
// BACKEND REAL e dirige o ponto que a TV atinge, com a cadeia de erro real:
//
//   TEST_ANDROID (cc -DTEST_ANDROID -DNV_ANDROID):
//     Java_..._nativeEvento — o JNI que o Kotlin chama com EV_ERRO
//     ("2004 player error:2") e EV_AUDIO_SEM_DECODER ("audio no decoder").
//   TEST_TPK (cc -DTEST_TPK -DNV_TPK):
//     video_tpk_log_host com as linhas do host .NET ("erro ConnectionFailed",
//     "erro AudioCodecNotSupported", "interrompido: ResourceConflict"),
//     nv_tpk_video_evento com o EV_ERRO nativo e recuperarPlayer (o ponto
//     real do "relogio congelado", chamado por vigiarPlayer).
//   TEST_TIZEN (cc -DTEST_TIZEN -D__EMSCRIPTEN__ -Itests/farol_wgt_stub):
//     video_tocar + video_bombear com nv_av implementado em C no lugar do
//     EM_JS — o mesmo contrato do webapis.avplay: "disp", "abrir", "estado"
//     (doubles) e "erro" (S.erro). O onerror da TV chega como
//     "PLAYER_ERROR_CONNECTION_FAILED".
//   (sem define):
//     as mesmas chamadas farolprod_* que os tres arquivos fazem, para as
//     cadeias que so nascem dentro do video_bombear ("2002 player error:
//     recovery timeout", "2002 player error: stalled download").
//
// As conferencias sao sobre o ANEL (farolanel_ler): tipo, a, b e o token
// fechado do farolerro — o que prova que nenhum texto cru passou.

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "farolanel.h"

static int falhos;

// Confere um evento do anel: tipo, modulo FM_VIDEO, sev e os dois numeros;
// o token e o cod fechado que o farolerro escolheu — nunca o texto da TV.
static void chk(const FarolEvento *e, int ev, int sev,
                int32_t a, int32_t b, const char *tok, const char *onde) {
  if (e->ev == (uint16_t)ev && e->mod == FM_VIDEO && e->sev == (uint8_t)sev &&
      e->a == a && e->b == b && !strcmp(e->tok, tok)) return;
  fprintf(stderr, "farol_produtores_tv [%s]: ev=%u mod=%u sev=%u a=%d b=%d "
          "tok=\"%s\" (esperava ev=%d sev=%d a=%d b=%d tok=%s)\n",
          onde, e->ev, e->mod, e->sev, (int)e->a, (int)e->b, e->tok,
          ev, sev, (int)a, (int)b, tok);
  falhos++;
}

// ======================================================================
// Android (Media3): o JNI real, como em tests/video_android_retomada.c.
// ======================================================================
#if defined(TEST_ANDROID)

#include <SDL2/SDL.h>
// SDL_system.h declara estas so com __ANDROID__ (NDK); no VPS elas ficam sem
// prototipo — os pontos dirigidos nao as usam, mas a declaracao e exigida.
void *SDL_AndroidGetJNIEnv(void);
void *SDL_AndroidGetActivity(void);
#include "../src/video_android.c"

static unsigned agora;
Uint32 SDL_GetTicks(void) { return agora; }
void marco(const char *nome) { (void)nome; }
void *SDL_AndroidGetJNIEnv(void) { return NULL; }
void *SDL_AndroidGetActivity(void) { return NULL; }

int main(void) {
  FarolEvento ev[8];
  int n;
  farolanel_zerar();

  // "2004 player error:2" — EV_ERRO do Media3 com o renderer de video.
  Java_space_nuvio_nativelegacy_NvPlayer_nativeEvento(NULL, NULL, EV_ERRO, 2004, 2);
  // "audio no decoder" — a flag EV_AUDIO_SEM_DECODER vira texto no produtor.
  Java_space_nuvio_nativelegacy_NvPlayer_nativeEvento(NULL, NULL, EV_AUDIO_SEM_DECODER, 0, 0);
  // Erro de rede nativo, desta vez no renderer de audio.
  Java_space_nuvio_nativelegacy_NvPlayer_nativeEvento(NULL, NULL, EV_ERRO, 2000, 1);
  // Erro de formato (3xxx): o cod sai do proprio numero, sem lista nova.
  Java_space_nuvio_nativelegacy_NvPlayer_nativeEvento(NULL, NULL, EV_ERRO, 3001, 2);

  n = farolanel_ler(ev, (int)(sizeof ev / sizeof ev[0]), 0);
  if (n != 4) { fprintf(stderr, "farol_produtores_tv [android]: %d eventos, esperava 4\n", n); return 1; }
  chk(&ev[0], FE_ERRO_NATIVO,    2, 2004, 2, "m3_2004", "android erro 2004");
  chk(&ev[1], FE_AUDIO_RECUSADO, 2, 0,    0, "m3_audio", "android audio sem decoder");
  chk(&ev[2], FE_ERRO_NATIVO,    2, 2000, 1, "m3_2000", "android erro 2000");
  chk(&ev[3], FE_ERRO_NATIVO,    2, 3001, 2, "m3_3001", "android erro 3001");
  if (falhos) return 1;
  puts("farol_produtores_tv: android ok");
  return 0;
}

// ======================================================================
// Samsung .tpk (Tizen.Multimedia): as linhas do log do host e o evento nativo,
// como em tests/tpk-preso.c.
// ======================================================================
#elif defined(TEST_TPK)

#include "../src/video_tpk.c"

static unsigned agora;
Uint32 SDL_GetTicks(void) { return agora; }
void marco(const char *nome) { (void)nome; }
void vel_log(int cent, const char *estado) { (void)cent; (void)estado; }
int proxyts_e_url(const char *url) { (void)url; return 0; }
int dts_tpk_e_url(const char *url) { (void)url; return 0; }
int dts_tpk_disponivel(void) { return 0; }
int ajustes_dts_tpk_modo(void) { return DTSTPK_DESLIGADO; }
double dts_tpk_duracao(void) { return 0.0; }
double dts_tpk_inicio_stream(void) { return 0.0; }

int main(void) {
  FarolEvento ev[8];
  int n;
  farolanel_zerar();
  ativo = 1;   // o host .NET so entrega eventos com a sessao aberta

  // A linha de texto que o host loga ANTES do evento nativo (Video.cs).
  video_tpk_log_host("erro ConnectionFailed");
  // O EV_ERRO com o inteiro do Tizen.Multimedia.Player chega logo depois —
  // sao duas observacoes reais do mesmo problema, e o anel guarda as duas.
  nv_tpk_video_evento(EV_ERRO, (int)0xFE6C0026, 0);
  reconErroPend = 0;   // o quadro consumiria o pendente; aqui mede-se so o anel
  // A recusa do codec de audio: "erro AudioCodecNotSupported" do enum real.
  video_tpk_log_host("erro AudioCodecNotSupported");
  // Outro app toma o video da TV (so na janela visivel, e so uma vez).
  video_tpk_log_host("interrompido: ResourceConflict");
  // O relogio congelado 15 s em Playing: vigiarPlayer chama recuperarPlayer.
  recuperarPlayer(-1);

  n = farolanel_ler(ev, (int)(sizeof ev / sizeof ev[0]), 0);
  if (n != 5) { fprintf(stderr, "farol_produtores_tv [tpk]: %d eventos, esperava 5\n", n); return 1; }
  chk(&ev[0], FE_ERRO_NATIVO,    2, 0,               0, "tpk_conexao", "tpk linha rede");
  chk(&ev[1], FE_ERRO_NATIVO,    2, (int32_t)0xFE6C0026, 0, "tpk_conexao", "tpk EV_ERRO");
  chk(&ev[2], FE_AUDIO_RECUSADO, 2, 0,               0, "tpk_audio",   "tpk audio recusado");
  chk(&ev[3], FE_ERRO_NATIVO,    2, 0,               0, "tpk_conflito","tpk resource conflict");
  chk(&ev[4], FE_ERRO_NATIVO,    2, -2,              0, "tpk_preso",   "tpk relogio congelado");
  if (falhos) return 1;
  puts("farol_produtores_tv: tpk ok");
  return 0;
}

// ======================================================================
// Samsung .wgt (AVPlay): o backend inteiro com nv_av em C no lugar do EM_JS.
// ======================================================================
#elif defined(TEST_TIZEN)

#include "../src/video_tizen.c"

// --- o mundo que o firmware empresta ao EM_JS ------------------------------
static unsigned agora;
static double estadoAv[EST_N];      // o "estado" que a getState relataria
static const char *erroAvplay;      // o S.erro do JS ("" = sem erro)
static int dispAvplay = 1;          // webapis.avplay existe

Uint32 SDL_GetTicks(void) { return agora; }
void marco(const char *nome) { (void)nome; }
const char *i18n(const char *s) { return s; }
const char *ling_nome(const char *codigo) { return codigo; }
const char *ling_audio(void) { return ""; }
const char *audioesc_origem_nome(int origem) { (void)origem; return ""; }
int audioesc_dts_truehd(const char *codec, const char *rotulo) { (void)codec; (void)rotulo; return 0; }
int audioesc_escolher(const AudioCand *c, int n, int *origem) {
  (void)c; (void)n; if (origem) *origem = 0; return -1;
}
int mkv_faixas_e_caps(const char *url, MkvFaixa *saida, int max,
                      MkvCap *caps, int maxCaps, int *nCaps) {
  (void)url; (void)saida; (void)max; (void)caps; (void)maxCaps;
  if (nCaps) *nCaps = 0;
  return 0;
}
double mkv_creditos_nomeados(const MkvCap *caps, int n) { (void)caps; (void)n; return 0.0; }
double mkv_creditos_ultimo(const MkvCap *caps, int n) { (void)caps; (void)n; return 0.0; }
void capmkv_aplicar(const MkvCap *caps, int n) { (void)caps; (void)n; }

// O fio de teste E o fio principal: avChamar executa o pedido na hora.
int emscripten_is_main_browser_thread(void) { return 1; }
int emscripten_main_runtime_thread_id(void) { return 0; }
em_proxying_queue *em_proxying_queue_create(void) { return NULL; }
void emscripten_proxy_sync(em_proxying_queue *q, int thread,
                           void (*func)(void *), void *arg) {
  (void)q; (void)thread; (void)func; (void)arg;
}

// nv_av em C, com o contrato do corpo JS: "disp" diz se ha webapis.avplay,
// "abrir" retorna 1 e limpa S.erro, "estado" enche doubles, "erro" copia
// S.erro para dst, os demais comandos de controle retornam sucesso.
double nv_av(const char *cmd, const char *txt, double a, double b,
             double c, double d, char *dst, int dstTam) {
  (void)txt; (void)a; (void)b; (void)c; (void)d;
  if (!cmd) return 0;
  if (!strcmp(cmd, "disp")) return dispAvplay;
  if (!strcmp(cmd, "abrir")) { erroAvplay = NULL; return 1; }
  if (!strcmp(cmd, "estado")) {
    if (!dst || dstTam < (int)sizeof estadoAv) return 0;
    memcpy(dst, estadoAv, sizeof estadoAv);
    return 1;
  }
  if (!strcmp(cmd, "erro")) {
    if (!erroAvplay || !dst || dstTam < 2) return 0;
    snprintf(dst, (size_t)dstTam, "%s", erroAvplay);
    return 1;
  }
  if (!strcmp(cmd, "faixas") || !strcmp(cmd, "infofluxo") ||
      !strcmp(cmd, "playinfo")) return 0;
  return 1;   // rect, roi, parar, pausar, buscar, encerrar, leg_mudo...
}

int main(void) {
  FarolEvento ev[8];
  int n;
  farolanel_zerar();
  memset(estadoAv, 0, sizeof estadoAv);

  assert(video_iniciar() == 1);
  video_definir_reconexao(1);
  video_definir_mp4(1);                       // sem a sonda MKV (evita o fio)
  assert(video_tocar("http://tv.local/f.mp4") == 1);

  // Quadro pronto e andando: ainda nenhum evento no anel.
  agora = 2000;
  estadoAv[EST_ABERTO] = 1; estadoAv[EST_PRONTO] = 1; estadoAv[EST_TOCANDO] = 1;
  estadoAv[EST_POS] = 12.0; estadoAv[EST_DUR] = 3600.0;
  estadoAv[EST_BUFPCT] = 100; estadoAv[EST_LARG] = 1920; estadoAv[EST_ALT] = 1080;
  video_bombear();
  n = farolanel_ler(ev, (int)(sizeof ev / sizeof ev[0]), 0);
  if (n != 0) { fprintf(stderr, "farol_produtores_tv [tizen]: %d eventos antes do erro\n", n); return 1; }

  // A TV acusa a queda: onerror com o nome do enum PlayerError do AVPlay.
  erroAvplay = "PLAYER_ERROR_CONNECTION_FAILED";
  estadoAv[EST_ERRO] = 1; estadoAv[EST_TOCANDO] = 0; estadoAv[EST_BUFPCT] = 0;
  agora = 3000;
  video_bombear();

  n = farolanel_ler(ev, (int)(sizeof ev / sizeof ev[0]), 0);
  if (n != 2) { fprintf(stderr, "farol_produtores_tv [tizen]: %d eventos no erro, esperava 2\n", n); return 1; }
  chk(&ev[0], FE_ERRO_NATIVO, 2, 0, 0, "av_conexao", "tizen onerror");
  chk(&ev[1], FE_RECONEXAO,   1, 1, (int32_t)nv_recon_espera_ms(1), "av_conexao", "tizen retentativa 1");

  // A espera vence, a mesma fonte reabre (abrir limpa S.erro), a reproducao
  // volta pronta e a rede cai de novo: segunda retentativa, mesmo problema.
  agora += nv_recon_espera_ms(1) + 1;
  estadoAv[EST_ERRO] = 0; estadoAv[EST_PRONTO] = 0;
  video_bombear();                            // vencida -> reabrirMesmaFonte
  estadoAv[EST_PRONTO] = 1; estadoAv[EST_TOCANDO] = 1; estadoAv[EST_BUFPCT] = 100;
  video_bombear();                            // pronto de novo, seek pos-queda
  erroAvplay = "PLAYER_ERROR_CONNECTION_FAILED";
  estadoAv[EST_ERRO] = 1; estadoAv[EST_TOCANDO] = 0;
  video_bombear();

  n = farolanel_ler(ev, (int)(sizeof ev / sizeof ev[0]), 0);
  if (n != 4) { fprintf(stderr, "farol_produtores_tv [tizen]: %d eventos na 2a queda, esperava 4\n", n); return 1; }
  chk(&ev[2], FE_ERRO_NATIVO, 2, 0, 0, "av_conexao", "tizen onerror 2");
  chk(&ev[3], FE_RECONEXAO,   1, 2, (int32_t)nv_recon_espera_ms(2), "av_conexao", "tizen retentativa 2");

  if (falhos) return 1;
  puts("farol_produtores_tv: tizen ok");
  return 0;
}

// ======================================================================
// Nivel do helper: as cadeias que so nascem dentro dos video_bombear
// (os printf do video_android.c em recovery timeout / stalled download) e a
// retentativa, com os mesmos argumentos dos pontos reais.
// ======================================================================
#else

#include "farolprodutor.h"
#include <pthread.h>

// O host .NET e o JNI emitem dos fios de callback; o anel e a unica estrutura
// compartilhada. Sob TSan isto cobre a trava de gravacao sem arrastar o resto
// do estado do backend (que tem fio proprio por desenho).
#define FIOS_N   4
#define EVS_FIO 25   /* 4 x 50 eventos: tudo fica no anel de 256 */
static void *fioProdutor(void *arg) {
  int i, k = (int)(intptr_t)arg;
  for (i = 0; i < EVS_FIO; i++) {
    farolprod_erro(FP_TIZEN_TPK, "erro ConnectionFailed", 0, 0);
    farolprod_recon(FP_TIZEN_TPK, "erro ConnectionFailed", 0, k * EVS_FIO + i, 1000);
  }
  return NULL;
}

int main(void) {
  FarolEvento ev[8];
  int n, i;
  farolanel_zerar();

  // As duas cadeias que video_android.c escreve em erroTxt nos caminhos
  // "recovery timeout" e "stalled download" (video_bombear, fora do JNI).
  farolprod_erro(FP_ANDROID, "2002 player error: recovery timeout", 2002, 0);
  farolprod_erro(FP_ANDROID, "2002 player error: stalled download", 2002, 0);
  // A retentativa seguinte: a=tentativa, b=espera, tok=cod do erro original.
  farolprod_recon(FP_ANDROID, "2004 player error:2", 2004, 1, 4000);
  // O prazo estourado do .wgt, texto fixo do proprio app.
  farolprod_erro(FP_TIZEN_WGT, "network recovery timeout", 0, 0);
  // .tpk: o relogio congelado visto pela retentativa (recuperarPlayer -> -2).
  farolprod_recon(FP_TIZEN_TPK, NULL, -2, 2, 8000);

  n = farolanel_ler(ev, (int)(sizeof ev / sizeof ev[0]), 0);
  if (n != 5) { fprintf(stderr, "farol_produtores_tv [helper]: %d eventos, esperava 5\n", n); return 1; }
  chk(&ev[0], FE_ERRO_NATIVO, 2, 2002, 0, "m3_parado",    "android recovery timeout");
  chk(&ev[1], FE_ERRO_NATIVO, 2, 2002, 0, "m3_parado",    "android stalled download");
  chk(&ev[2], FE_RECONEXAO,   1, 1, 4000, "m3_2004",      "android retentativa");
  chk(&ev[3], FE_ERRO_NATIVO, 2, 0,    0, "av_conexao",   "wgt recovery timeout");
  chk(&ev[4], FE_RECONEXAO,   1, 2, 8000, "tpk_preso",    "tpk retentativa preso");

  // Barreira de privacidade do anel: nenhum token carrega URL ou texto livre.
  for (i = 0; i < n; i++) {
    const char *p;
    for (p = ev[i].tok; *p; p++)
      if (!(*p >= 'a' && *p <= 'z') && !(*p >= '0' && *p <= '9') &&
          *p != '_' && *p != '.' && *p != ':' && *p != '+' && *p != '-') {
        fprintf(stderr, "farol_produtores_tv [helper]: tok inseguro \"%s\"\n", ev[i].tok);
        falhos++;
        break;
      }
  }
  // Quatro fios de produtor gravando ao mesmo tempo, como no callback do host.
  { pthread_t t[FIOS_N];
    farolanel_zerar();
    for (i = 0; i < FIOS_N; i++)
      assert(pthread_create(&t[i], NULL, fioProdutor, (void *)(intptr_t)i) == 0);
    for (i = 0; i < FIOS_N; i++) pthread_join(t[i], NULL);
    n = farolanel_contar(FE_ERRO_NATIVO, 0, 0);
    if (n != FIOS_N * EVS_FIO ||
        farolanel_contar(FE_RECONEXAO, 0, 0) != FIOS_N * EVS_FIO) {
      fprintf(stderr, "farol_produtores_tv [helper]: %d erros nativos de %d\n",
              n, FIOS_N * EVS_FIO);
      return 1;
    }
  }

  if (falhos) return 1;
  puts("farol_produtores_tv: helper ok");
  return 0;
}

#endif
