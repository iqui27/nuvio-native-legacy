// Farol — produtores player/LG (issue 7; PLANO.md 2.2). Harness: os trechos
// de producao — tratamento de erro do uMS (numeroDe + farolUmsErro), leitor e
// publicador do modelo da TV (farolTvModeloLer, edidResposta,
// video_tv_modelo), abertura com rearm (farolAbrir), bloco do primeiro quadro
// e produtores de erro do player — sao extraidos de src/video.c e
// src/player.c pelo .sh e recebem respostas JSON reais. Nada de grep de
// texto: isto roda o codigo de producao.
#include "farolanel.h"
#include "farolerro.h"
#include "farolcod.h"
#include "fontesonda.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <pthread.h>

// Simbolos que o codigo extraido referencia. O quadro do backend e
// controlado pelos contadores abaixo: "pronto" sem "tocando/posicao" e o
// loadCompleted sem nenhum quadro entregue.
static unsigned inicioImagem;
static int memQuadroPendente, prontoAnel;
static int comVideo = 1;
static int tentativaN = 1, tentativaM = 3;
static int retomarPct = 12, medirAbertura;
static unsigned abertoEm = 100, loadEm = 200;
static int backend_pronto = 1, backend_tocando;
static double backend_pos;
static int video_pronto(void) { return backend_pronto; }
static int video_tocando(void) { return backend_tocando; }
static double video_pos(void) { return backend_pos; }
static unsigned SDL_GetTicks(void) { return 300; }
static void acordar(void) {}
static int ehCanal(void) { return 0; }
static void memlog_evento(const char *e) { (void)e; }
static const char *erro_player = "";
static const char *video_erro_texto(void) { return erro_player; }
static int video_registro_negado(void) { return 0; }
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

  // 7B (a): backend pronto com ZERO quadros nao emite FE_PRONTO; o primeiro
  // quadro (tocando + posicao andando) emite UM, uma vez por abertura.
  farolanel_zerar();
  farolAbrir();                       // abertura: grava so FE_ABRIR e rearma
  backend_pronto = 1; backend_tocando = 0; backend_pos = 0.0;   // loadCompleted
  memQuadroPendente = 1;              // o load armou a marca do primeiro quadro
  quadro_atualizar(200);
  quadro_atualizar(201);
  n = farolanel_ler(ev, FAROL_ANEL_N, 0);
  assert(n == 1 && ev[0].ev == FE_ABRIR);      // pronto sem quadro: nada
  backend_tocando = 1; backend_pos = 0.4;      // a imagem chegou
  quadro_atualizar(202);
  n = farolanel_ler(ev, FAROL_ANEL_N, 0);
  assert(n == 2 && ev[1].ev == FE_PRONTO && ev[1].mod == FM_PLAYER);
  assert(ev[1].a == 200 && ev[1].b == 12);     // ms ate abrir, retomada %
  quadro_atualizar(203);
  quadro_atualizar(204);
  assert(farolanel_ler(ev, FAROL_ANEL_N, 0) == 2);  // nao repete no mesmo load

  // 7B (b): segunda abertura direta — player_definir_fonte -> tocarFonte ->
  // tocarFonteInterno -> farolAbrir (a renovacao do link Stalker). Emite
  // FE_ABRIR, rearma e o primeiro quadro da fonte nova emite FE_PRONTO de
  // novo, mesmo com inicioImagem da abertura anterior ainda definido.
  assert(inicioImagem != 0);
  farolAbrir();
  memQuadroPendente = 1;                        // o load novo arma de novo
  backend_pronto = 0; backend_tocando = 0; backend_pos = 0.0;
  quadro_atualizar(300);
  n = farolanel_ler(ev, FAROL_ANEL_N, 0);
  assert(n == 3 && ev[2].ev == FE_ABRIR);       // sem FE_PRONTO ainda
  backend_pronto = 1; backend_tocando = 1; backend_pos = 0.1;
  quadro_atualizar(301);
  n = farolanel_ler(ev, FAROL_ANEL_N, 0);
  assert(n == 4 && ev[3].ev == FE_PRONTO);

  // Produtores de erro do player (blocos extraidos): a classificacao segue a
  // plataforma compilada e o cod passa pela lista fechada.
  erro_player = NV_FAROL_PLAT == FP_TIZEN_WGT ? "PLAYER_ERROR_INVALID_OPERATION"
              : "40403 server error:40403";
  farolanel_zerar();
  emitir_recusa();
  emitir_erro_fonte();
  n = farolanel_ler(ev, FAROL_ANEL_N, 0);
  assert(n == 2 && ev[0].ev == FE_RECUSA_SINC && ev[1].ev == FE_ERRO_FONTE);
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
