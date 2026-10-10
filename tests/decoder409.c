// #409: selecao real, sem rede nem TV. Stubs fracos permitem provar o FAIL anterior.
#include "streams.h"
#include "badges.h"
#include "ajustes.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// R2: stream_automatico_erro_decoder agora recebe o renderer do erro (0 video,
// 1 audio, 2 desconhecido). Erros de audio nao bloqueiam codecs de video, e
// codec desconhecido nao pode ser atribuido a HEVC por falta de anuncio.
__attribute__((weak)) void stream_definir_decoder4k(int h, int a, int v, int av) { (void)h; (void)a; (void)v; (void)av; }
__attribute__((weak)) void stream_automatico_erro_decoder(int i, int c, int r) { (void)i; (void)c; (void)r; }
static int falhas;
static void espera(int esperado, const char *nome) {
  int i = stream_automatico();
  printf("%s %s: fonte %d (esperada %d)\n", i == esperado ? "PASS" : "FAIL", nome, i, esperado);
  falhas += i != esperado;
}
int main(void) {
  Stream s[3] = {0};
  ajustes_dir(getenv("NUVIO_DADOS"));
  for (int i = 0; i < 3; i++) {
    snprintf(s[i].url, sizeof s[i].url, "https://example.invalid/%d", i);
    snprintf(s[i].rotulo, sizeof s[i].rotulo, "%s", i == 2 ? "1080p" : "4K UHD HEVC VidFast");
    s[i].altura = i == 2 ? 1080 : 2160;
  }
  stream_definir_lista(s, 3);
  espera(0, "capacidade desconhecida preserva 4K");
  stream_definir_decoder4k(0, 0, 0, 0);
  espera(2, "sem decoder 4K pula 2160p");
  assert(stream_n() == 3 && stream_item(0)->altura == 2160);
  stream_preferir(0);
  // Inclusive preferida e caminhos de antecipacao devem obedecer ao decoder.
  if (stream_automatico_disponivel(0)) { puts("FAIL preferida sem decoder disponivel"); falhas++; }
  stream_definir_decoder4k(1, 0, 0, 0); espera(0, "HEVC 4K mantem codec conhecido");
  const char *codecs[] = {"H.264", "VP9", "AV1", "HEVC"};
  strcpy(s[0].rotulo, "4K UHD VidFast");
  for (int c = 0; c < 4; c++) {
    snprintf(s[0].arquivo, sizeof s[0].arquivo, "filme.2160p.%s.mkv", codecs[c]);
    s[1].altura = 1080;
    stream_definir_lista(s, 3);
    stream_definir_decoder4k(c != 3, c != 0, c != 1, c != 2);
    espera(1, codecs[c]);
    stream_definir_decoder4k(1, 1, 1, 1); espera(0, "codec 4K suportado");
  }
  s[0].arquivo[0] = 0; s[0].badges = badges_bit("co-x264");
  stream_definir_lista(s, 3); stream_definir_decoder4k(1, 0, 1, 1);
  espera(1, "codec por selo AVC");
  s[0].badges = 0; s[1].altura = 2160;
  strcpy(s[0].rotulo, "4K UHD HEVC VidFast");
  for (int c = 4001; c <= 4005; c++) {
    if (c == 4002) continue;
    stream_definir_decoder4k(-1, -1, -1, -1);
    stream_definir_lista(s, 3);
    stream_automatico_erro_decoder(0, c, 0);
    stream_automatico_excluir(0);
    espera(2, "erro decoder nao repete tamanho/codec");
  }
  stream_definir_lista(s, 3); espera(0, "lista nova nao herda falha");
  stream_automatico_erro_decoder(0, 2004, 0); stream_automatico_excluir(0);
  espera(1, "erro de rede nao muda decoder");

  // P2-2: codec desconhecido nao pode virar HEVC. Probe de 4K AVC (mas sem
  // HEVC) nao deve excluir uma fonte "4K" sem codec anunciado — so a falha
  // aprendida conta. E uma falha HEVC declarada (codecDaFonte = 0) nao
  // bloqueia uma HEVC posterior de codec diferente.
  strcpy(s[0].rotulo, "4K"); s[0].arquivo[0] = 0;
  strcpy(s[1].rotulo, "4K HEVC");
  stream_definir_lista(s, 3); stream_definir_decoder4k(0, 1, 1, 1);
  espera(0, "P2 codec desconhecido permanece elegivel");
  stream_definir_decoder4k(-1, -1, -1, -1);
  stream_automatico_erro_decoder(0, 4003, 0); stream_automatico_excluir(0);
  espera(1, "P2 falha desconhecida nao bloqueia HEVC");

  // P2-3: capacidade UHD e falha aprendida tem limites independentes. Uma
  // AV1 1080p que falhou precisa bloquear AV1 >=1080p nesta lista — nao so
  // o 2160p. (O probe 4K ainda nao filtra o 1080p, isso e o que o teste
  // abaixo confere.)
  strcpy(s[0].rotulo, "1080p AV1"); s[0].altura = 1080;
  strcpy(s[1].rotulo, "2160p AV1"); s[1].altura = 2160;
  for (int c = 4001; c <= 4005; c++) {
    if (c == 4002) continue;
    stream_definir_lista(s, 3);
    stream_automatico_erro_decoder(0, c, 0);
    espera(2, "P2 falha AV1 1080p bloqueia igual e maior");
    if (stream_automatico_disponivel(0) || stream_automatico_disponivel(1)) falhas++;
  }
  s[1].altura = 720;
  // i=1 (720p AV1) NAO e excedida: codecDaFonte=3, altura=720, falha=1080.
  // i=2 (1080p sem rotulo) NAO e excedida: codecDaFonte=-1, excedeDecoder pula.
  // O automatico escolhe a de MAIOR pontuacao entre as nao-excedidas — e
  // pontos() premia 1080p sobre 720p, entao retorna 2. O que importa aqui e
  // que a 720 AV1 NAO tenha sido bloqueada pelo limite aprendido de 1080p.
  stream_definir_lista(s, 3); stream_automatico_erro_decoder(0, 4003, 0);
  if (!stream_automatico_disponivel(1)) { puts("FAIL AV1 720p bloqueada pela falha de 1080p"); falhas++; }
  else puts("PASS AV1 menor que a falha continua elegivel");
  stream_definir_lista(s, 3); stream_definir_decoder4k(0, 0, 0, 0);
  espera(0, "probe UHD nao proibe AV1 1080p");

  // P2-1: erro 4003/4001/4004/4005 no renderer de audio NAO bloqueia codecs
  // de video. Sem o renderer, a falha de EAC3 do audio marcaria a HEVC 4K
  // inteira como ruim, e o automatico nunca mais a abriria. Codec no rotulo
  // explicitamente HEVC: sem o tratamento, o teste falha com codec=0
  // bloqueado em 2160p.
  strcpy(s[0].rotulo, "4K HEVC 2160p"); s[0].altura = 2160;
  strcpy(s[1].rotulo, "4K HEVC 1080p"); s[1].altura = 1080;
  stream_definir_lista(s, 3); stream_definir_decoder4k(1, 1, 1, 1);
  for (int c = 4001; c <= 4005; c++) {
    if (c == 4002) continue;
    stream_definir_lista(s, 3);
    stream_automatico_erro_decoder(0, c, 1);    // 1 = renderer audio
    espera(0, "P2 erro audio ignorado para codec de video");
  }
  // E o inverso: renderer=0 (video) com codec declarado HEVC bloqueia o 2160p.
  stream_definir_lista(s, 3); stream_automatico_erro_decoder(0, 4003, 0);
  espera(1, "P2 erro de video bloqueia codec 2160p mas nao 1080p");

  return falhas ? 1 : 0;
}
