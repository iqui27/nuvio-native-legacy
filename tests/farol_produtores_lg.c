// Farol — produtores player/LG (issue 7; PLANO.md 2.2). Harness: os trechos
// de producao — tratamento de erro do uMS (numeroDe + farolUmsErro), leitor e
// publicador do modelo da TV (farolTvModeloLer, edidResposta,
// video_tv_modelo), abertura com rearm (farolAbrir/farolRearmar), a cadeia da
// recarga manual do VOD (recarregarFonteVOD -> player_recarregar_vod ->
// video_recarregar -> recarregarMesmaFonte), o video_buscar que grava o alvo
// antes do pipeline, bloco do primeiro quadro e produtores de erro do player
// — sao extraidos de src/app.c, src/video.c e src/player.c pelo .sh e recebem
// respostas JSON reais. Nada de grep de texto: isto roda o codigo de producao.
#include "farolanel.h"
#include "farolerro.h"
#include "farolcod.h"
#include "fontesonda.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <limits.h>
#include <math.h>
#include <assert.h>
#include <pthread.h>

typedef uint32_t Uint32;
typedef uint64_t Uint64;

// Simbolos que o codigo extraido referencia. O quadro do backend e
// controlado pelos contadores abaixo: "pronto" sem "tocando/amostra" e o
// loadCompleted sem nenhum quadro entregue. posSeg e a posicao MOSTRADA —
// video_buscar escreve o alvo nela na hora; posAmostra e a amostra nativa,
// a unica que o FE_PRONTO aceita.
static unsigned inicioImagem;
static int memQuadroPendente, prontoAnel;
static int comVideo = 1;
static int tentativaN = 1, tentativaM = 3;
static int retomarPct = 12, medirAbertura;
static unsigned clock_ms = 300, abertoEm = 100, loadEm = 200;

// Sessao VOD do backend simulado (mesmos nomes dos estaticos de video.c).
static int ligado = 1, pronto, tocando, falhou, terminou;
static int dtsModoDv, dtsNativePending, dtsEstado;
static int dvSilencio, dvSegurar, recuperando, reconErroPend, recarregarPausa = -1;
static int semDVForcado, pausaPedida;
static int audioAtual, legAtual, audioAoCarregar, legAoCarregar, reconAudio, reconLeg;
static void *dtsSessao;
static char urlAtual[] = "https://example.invalid/media", midia[] = "m";
static char legUrlAtual[32], legUrlAoCarregar[32], reconLegUrl[32];
static double posAoCarregar, posNativa, posSeg, posAmostra, dvEntradaSeg, seekAlvo;
static unsigned seekEm;
enum { VIDEO_DTS_PREPARANDO = 1, VIDEO_DTS_CONVERTIDO, SEEK_REPOUSO_MS = 350 };
static int cargas;                     // quantas vezes o backend abriu a fonte
static int video_reconectando(void) { return 0; }
// Para no limite do firmware e modela o reset da carga.
static int tocarInterno(const char *url, int dv) {
  (void)dv;
  assert(url == urlAtual);
  ++cargas;
  pronto = tocando = 0;
  posNativa = posSeg = posAmostra = 0;
  return 1;
}

// O botao do app (recarregarFonteVOD): sessao de perfil e estado do player.
static int aguardandoFonte, fonteVODAutomatica;
static Uint32 fonteVODDesde, fonteManualDesde;
static uint64_t nv_perfil_sessao_ler(void) { return 1; }
static int nv_perfil_sessao_atual(uint64_t id) { return id == 1; }
static int player_aberto(void) { return 1; }
static int player_quer_sair(void) { return 0; }
static const char *player_id_canal(void) { return ""; }
static void player_toast(const char *text, int ms) { (void)text; (void)ms; }
#define i18n(x) (x)

static int video_pronto(void) { return pronto; }
static int video_tocando(void) { return tocando; }
static double video_pos_amostra(Uint64 *t) { if (t) *t = 0; return posAmostra; }
static unsigned SDL_GetTicks(void) { return clock_ms; }
static void acordar(void) {}
static int ehCanal(void) { return 0; }
static void memlog_evento(const char *e) { (void)e; }
static const char *erro_player = "";
static const char *video_erro_texto(void) { return erro_player; }
static int video_registro_negado(void) { return 0; }
// FAROL6B: o erro de fonte tambem grava o FarolErro normalizado e a
// proveniencia debrid da fonte corrente — os mesmos nomes dos estaticos de
// player.c, com um Stream minimo (so o campo que o trecho extraido le).
typedef struct { int debridServico; } Stream;
static const Stream fonteFarol;
static const char ultimaFonteUrl[] = "https://cdn.real-debrid.com/video";
static int stream_atual(void) { return 0; }
static const Stream *stream_item(int i) { (void)i; return &fonteFarol; }
static FarolErro ultimoFarol;
static int temUltimoFarol, ultimoFarolDebrid;
typedef void LSHandle;
typedef void LSMessage;
static const char *lsPayload(LSMessage *m) { return m; }
static int edidDts;
static int nv_dts_edid(const char *j) { return j && strstr(j, "TrueHD+dts") != NULL; }
// O callback do EDID imprime a resposta inteira; no harness e so ruido.
static int nuvio_teste_printf(const char *f, ...) { (void)f; return 0; }
#define printf(...) nuvio_teste_printf(__VA_ARGS__)

#include "farol_ums.inc"   // codigo de producao extraido pelo .sh

#undef printf

static const char resp_c9[] =
  "{\"configs\":{\"tv.model.edidType\":\"TrueHD+dts\","
  "\"tv.model.modelname\":\"OLED55C9PSA\"},\"returnValue\":true}";
static const char resp_ur[] =
  "{\"configs\":{\"tv.model.edidType\":\"TrueHD\","
  "\"tv.model.modelname\":\"43UR781C0SA\"},\"returnValue\":true}";
static const char resp_sem[] =
  "{\"configs\":{\"tv.model.edidType\":\"TrueHD+dts\"},\"returnValue\":true}";
static const char *const respostas[] = { resp_c9, resp_ur, resp_sem };

static pthread_barrier_t largada;
static void *fio_edid(void *a) {
  size_t i;
  (void)a;
  pthread_barrier_wait(&largada);
  for (i = 0; i < 3000; i++)
    edidResposta(NULL, (LSMessage *)respostas[i % 3], NULL);
  return NULL;
}

// (d) O callback do LS2 publica o modelo num fio enquanto o coletor le
// video_tv_modelo() em outro. Sob TSan uma escrita/leitura sem sincronia
// reprova o binario; cada leitura tem de sair um modelo INTEIRO publicado.
static int corrida(void) {
  pthread_t t;
  volatile size_t n = 0;
  int i;
  assert(!pthread_barrier_init(&largada, NULL, 2));
  assert(!pthread_create(&t, NULL, fio_edid, NULL));
  pthread_barrier_wait(&largada);
  for (i = 0; i < 200000; i++) {
    const char *m = video_tv_modelo();
    assert(!strcmp(m, "?") || !strcmp(m, "OLED55C9PSA") ||
           !strcmp(m, "43UR781C0SA"));
    n += strlen(m);
  }
  assert(!pthread_join(t, NULL));
  assert(!pthread_barrier_destroy(&largada));
  return n == 0;
}

static int unico(FarolEvento *ev) {
  int n = farolanel_ler(ev, FAROL_ANEL_N, 0);
  assert(n == 1);          // cada erro real do uMS grava exatamente 1 evento
  return n;
}

int main(int argc, char **argv) {
  FarolEvento ev[FAROL_ANEL_N];
  char erro[96], m[64];
  size_t i;
  int n;

  if (argc > 1 && !strcmp(argv[1], "race")) return corrida();

  // errorCode 200 "Audio Codec Not Supported" (registro 1545, webOS 5):
  // o video segue mudo — e recusa de AUDIO, nao erro nativo generico.
  farolanel_zerar();
  farolUmsErro("{\"errorCode\":200,\"errorText\":\"Audio Codec Not Supported\"}",
               12.0, erro, sizeof erro);
  unico(ev);
  assert(ev[0].ev == FE_AUDIO_RECUSADO && ev[0].mod == FM_AUDIO);
  assert(ev[0].a == 200);
  assert(!strcmp(ev[0].tok, "ums_200"));
  assert(!strcmp(erro, "200 Audio Codec Not Supported"));

  // errorCode 204 "Fail To Demultiplex": formato — erro nativo com o numero.
  farolanel_zerar();
  farolUmsErro("{\"errorCode\":204,\"errorText\":\"Fail To Demultiplex\"}",
               0.0, erro, sizeof erro);
  unico(ev);
  assert(ev[0].ev == FE_ERRO_NATIVO && ev[0].mod == FM_VIDEO);
  assert(ev[0].a == 204);
  assert(!strcmp(ev[0].tok, "ums_204"));

  // errorCode 40403 = 40000+HTTP (registro 4958, #158): recusa do servidor.
  farolanel_zerar();
  farolUmsErro("{\"errorCode\":40403,\"errorText\":\"server error:40403\"}",
               0.0, erro, sizeof erro);
  unico(ev);
  assert(ev[0].ev == FE_ERRO_NATIVO && ev[0].a == 40403);
  assert(!strcmp(ev[0].tok, "ums_40403"));

  // "com.webos.pipeline.<id> is not running": pipeline morto, posicao em s.
  farolanel_zerar();
  farolUmsErro("{\"errorCode\":-1000,\"errorText\":\"com.webos.pipeline.7 is not running\"}",
               482.4, erro, sizeof erro);
  unico(ev);
  assert(ev[0].ev == FE_PIPELINE_MORREU && ev[0].mod == FM_VIDEO);
  assert(ev[0].a == 482);
  assert(!strcmp(ev[0].tok, "ums_pipeline"));

  // A mesma morte sem errorCode no payload.
  farolanel_zerar();
  farolUmsErro("{\"errorText\":\"com.webos.pipeline.3 is not running\"}",
               7.9, erro, sizeof erro);
  unico(ev);
  assert(ev[0].ev == FE_PIPELINE_MORREU && ev[0].a == 7);

  // Seek recusado continua sendo erro nativo, nao audio nem pipeline.
  farolanel_zerar();
  farolUmsErro("{\"errorCode\":-22,\"errorText\":\"seek Failure\"}",
               60.0, erro, sizeof erro);
  unico(ev);
  assert(ev[0].ev == FE_ERRO_NATIVO);

  // Modelo da TV: a resposta real do getConfigs, com a chave nova ao lado
  // da do EDID que ja existia.
  farolTvModeloLer(
    "{\"configs\":{\"tv.model.edidType\":\"edid1.4\","
    "\"tv.model.modelname\":\"OLED55C9PSA\"},\"returnValue\":true}",
    m, sizeof m);
  assert(!strcmp(m, "OLED55C9PSA"));
  // Sem a chave (configd mais velho): sai vazio — video_tv_modelo responde "?".
  farolTvModeloLer(
    "{\"configs\":{\"tv.model.edidType\":\"edid1.4\"},\"returnValue\":true}",
    m, sizeof m);
  assert(m[0] == 0);
  // Saneamento: so [A-Za-z0-9._ -], teto de 32, espaco nas pontas cortado.
  farolTvModeloLer(
    "{\"tv.model.modelname\":\" 43UR781C0SA \xC3\x87 \x41\x42 }\"}",
    m, sizeof m);
  for (i = 0; m[i]; i++) {
    unsigned char c = (unsigned char)m[i];
    assert((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '.' || c == '-' || c == '_' || c == ' ');
  }
  farolTvModeloLer(
    "{\"tv.model.modelname\":\"ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ABCDEFGH\"}",
    m, sizeof m);
  assert(strlen(m) == 32);
  // Nem URL nem byte de controle sobrevivem no modelo.
  farolTvModeloLer("{\"tv.model.modelname\":\"x://y/z?k=v&n=1\"}", m, sizeof m);
  assert(strstr(m, "://") == NULL && strchr(m, '/') == NULL &&
         strchr(m, '?') == NULL && strchr(m, '=') == NULL && strchr(m, '&') == NULL);

  // 7B (publicacao): o callback do getConfigs publica e o getter le o modelo
  // inteiro — antes de qualquer resposta vale "?".
  assert(!strcmp(video_tv_modelo(), "?"));
  edidResposta(NULL, (LSMessage *)resp_c9, NULL);
  assert(!strcmp(video_tv_modelo(), "OLED55C9PSA"));
  edidResposta(NULL, (LSMessage *)resp_sem, NULL);
  assert(!strcmp(video_tv_modelo(), "?"));

  // 7B (a): backend pronto com ZERO amostras nao emite FE_PRONTO; a primeira
  // amostra nativa (tocando + posicao do pipeline) emite UM, por abertura.
  farolanel_zerar();
  farolAbrir();                       // abertura: grava so FE_ABRIR e rearma
  pronto = 1; tocando = 0; posSeg = posAmostra = 0.0;   // loadCompleted
  memQuadroPendente = 1;              // o load armou a marca do primeiro quadro
  quadro_atualizar(200);
  quadro_atualizar(201);
  n = farolanel_ler(ev, FAROL_ANEL_N, 0);
  assert(n == 1 && ev[0].ev == FE_ABRIR);      // pronto sem quadro: nada
  tocando = 1; posSeg = posAmostra = 0.4;      // a imagem chegou
  quadro_atualizar(202);
  n = farolanel_ler(ev, FAROL_ANEL_N, 0);
  assert(n == 2 && ev[1].ev == FE_PRONTO && ev[1].mod == FM_PLAYER);
  assert(ev[1].a == 100 && ev[1].b == 12);     // ms da carga (300-200), retomada %
  quadro_atualizar(203);
  quadro_atualizar(204);
  assert(farolanel_ler(ev, FAROL_ANEL_N, 0) == 2);  // nao repete no mesmo load

  // 7B (b): segunda abertura direta — player_definir_fonte -> tocarFonte ->
  // tocarFonteInterno -> farolRearmar -> farolAbrir (a renovacao do link
  // Stalker). Emite FE_ABRIR, rearma e a primeira amostra da fonte nova emite
  // FE_PRONTO de novo, mesmo com inicioImagem da abertura anterior definido.
  assert(inicioImagem != 0);
  farolAbrir();
  memQuadroPendente = 1;                        // o load novo arma de novo
  pronto = 0; tocando = 0; posSeg = posAmostra = 0.0;
  quadro_atualizar(300);
  n = farolanel_ler(ev, FAROL_ANEL_N, 0);
  assert(n == 3 && ev[2].ev == FE_ABRIR);       // sem FE_PRONTO ainda
  pronto = 1; tocando = 1; posSeg = posAmostra = 0.1;
  quadro_atualizar(301);
  n = farolanel_ler(ev, FAROL_ANEL_N, 0);
  assert(n == 4 && ev[3].ev == FE_PRONTO);

  // 7C (a): a RECARGA MANUAL do VOD e uma abertura para o ciclo do anel —
  // recarregarFonteVOD (o botao, app.c) -> player_recarregar_vod ->
  // video_recarregar -> recarregarMesmaFonte -> tocarInterno. A carga aceita
  // grava +1 FE_ABRIR e entrega +1 FE_PRONTO na primeira amostra dela.
  farolanel_zerar();
  cargas = 0;
  clock_ms = 1000; abertoEm = clock_ms;
  farolRearmar();                            // fonte inicial aberta (load)
  pronto = tocando = 1;
  posNativa = posSeg = posAmostra = 0.25;
  quadro_atualizar(clock_ms += 2);
  n = farolanel_ler(ev, FAROL_ANEL_N, 0);
  assert(n == 2 && ev[0].ev == FE_ABRIR && ev[1].ev == FE_PRONTO);

  recarregarFonteVOD();                      // o botao reabre a MESMA fonte
  assert(cargas == 1 && !pronto && !tocando && posAmostra == 0.0);
  quadro_atualizar(++clock_ms);              // carga nova, nenhuma amostra ainda
  n = farolanel_ler(ev, FAROL_ANEL_N, 0);
  assert(n == 3 && ev[2].ev == FE_ABRIR);    // o rearm gravou a abertura nova
  pronto = tocando = 1;
  posNativa = posSeg = posAmostra = 0.25;    // primeira amostra da recarga
  quadro_atualizar(++clock_ms);
  n = farolanel_ler(ev, FAROL_ANEL_N, 0);
  assert(n == 4 && ev[3].ev == FE_PRONTO);

  // Recarga recusada pelo backend (pipeline ocupado): nada no anel.
  recuperando = 1;
  recarregarFonteVOD();
  recuperando = 0;
  assert(cargas == 1 && farolanel_ler(ev, FAROL_ANEL_N, 0) == 4);

  // 7C (b): o `a` do FE_PRONTO mede da ABERTURA DA FONTE CORRENTE (loadEm,
  // rearmado por farolRearmar) — uma segunda fonte 10 min depois na mesma
  // sessao grava os 50 ms da carga dela, nao os 600 s da sessao do player.
  clock_ms = 600100;
  farolRearmar();                            // loadEm = 600100
  pronto = tocando = 1;
  posNativa = posSeg = posAmostra = 0.25;
  clock_ms += 50;
  quadro_atualizar(clock_ms);
  n = farolanel_ler(ev, FAROL_ANEL_N, 0);
  assert(n == 6 && ev[4].ev == FE_ABRIR && ev[5].ev == FE_PRONTO);
  assert(ev[5].a == 50);

  // 7C (c): retomada — video_buscar grava o alvo na posicao MOSTRADA antes
  // do seek acontecer; sem amostra nativa da carga, NENHUM FE_PRONTO sai.
  farolRearmar();
  pronto = tocando = 1;                      // o backend ja anuncia playing
  posNativa = posSeg = posAmostra = 0.0;     // pipeline ainda nao reportou
  video_buscar(120.0);                       // posSeg=120 na hora, seekEm armado
  assert(posSeg == 120.0 && posNativa == 0.0 && posAmostra == 0.0);
  assert(clock_ms < seekEm);
  quadro_atualizar(++clock_ms);
  n = farolanel_ler(ev, FAROL_ANEL_N, 0);
  assert(n == 7);                            // so o FE_ABRIR da abertura
  // A amostra nativa chega quando o seek pousa: ai o FE_PRONTO sai.
  posNativa = posSeg = posAmostra = 119.8;
  quadro_atualizar(++clock_ms);
  n = farolanel_ler(ev, FAROL_ANEL_N, 0);
  assert(n == 8 && ev[7].ev == FE_PRONTO);

  // Produtores de erro do player (blocos extraidos): a classificacao segue a
  // plataforma compilada e o cod passa pela lista fechada.
  erro_player = NV_FAROL_PLAT == FP_TIZEN_WGT ? "PLAYER_ERROR_INVALID_OPERATION"
              : "40403 server error:40403";
  farolanel_zerar();
  emitir_recusa();
  emitir_erro_fonte();
  n = farolanel_ler(ev, FAROL_ANEL_N, 0);
  assert(n == 2 && ev[0].ev == FE_RECUSA_SINC && ev[1].ev == FE_ERRO_FONTE);
  assert(temUltimoFarol && ultimoFarolDebrid);
#if defined(__EMSCRIPTEN__)
  assert(!strcmp(ev[0].tok, "av_invalid_operation") &&
         !strcmp(ev[1].tok, "av_invalid_operation"));

  // 7B (c): no .wgt o prepareAsync entrega a mensagem INTEIRA — o recorte de
  // 20 bytes depois de "player_error" pode cair dentro de uma URL. Fragmento
  // privado nao chega ao anel: vira o desconhecido convencional av_outro.
  erro_player = "prepare: demo://test.invalid/player_error_CANARIOPRIVADO/filme?chave=canario";
  farolanel_zerar();
  emitir_recusa();
  emitir_erro_fonte();
  n = farolanel_ler(ev, FAROL_ANEL_N, 0);
  assert(n == 2 && ev[0].ev == FE_RECUSA_SINC && ev[1].ev == FE_ERRO_FONTE);
  for (i = 0; i < (size_t)n; i++)
    assert(strstr(ev[i].tok, "canarioprivado") == NULL &&
           !strcmp(ev[i].tok, "av_outro"));
#else
  assert(!strcmp(ev[0].tok, "ums_40403") && !strcmp(ev[1].tok, "ums_40403"));
#endif

  puts("farol_produtores_lg: harness ok");
  return 0;
}
