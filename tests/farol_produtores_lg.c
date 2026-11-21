// Farol — produtores player/LG (issue 7; PLANO.md 2.2). Harness: o trecho de
// tratamento de erro do uMS (numeroDe + farolUmsErro) e o leitor do modelo da
// TV (farolTvModeloLer), extraidos de src/video.c pelo .sh, recebem respostas
// JSON reais e gravam os eventos esperados no anel. Nada de grep de texto:
// isto roda o codigo de producao.
#include "farolanel.h"
#include "farolerro.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "farol_ums.inc"   // numeroDe, farolUmsErro, farolTvModeloLer (src/video.c)

static int unico(FarolEvento *ev) {
  int n = farolanel_ler(ev, FAROL_ANEL_N, 0);
  assert(n == 1);          // cada erro real do uMS grava exatamente 1 evento
  return n;
}

int main(void) {
  FarolEvento ev[FAROL_ANEL_N];
  char erro[96], m[64];
  size_t i;

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

  puts("farol_produtores_lg: harness ok");
  return 0;
}
