// Watchdog real do app: trilhas chegam aos 14 s, antes do primeiro quadro.
// Como stalker_scheduler.c, inclui app.c e descarta as telas no linker.
#define SDL_MAIN_HANDLED
#include "../src/app.c"

static int audios, legendas, prontoTeste, tocouTeste, dvTeste;
static int proximaBoa = 1, falhas;
static double bufferTeste;
#define CHECK(c) do { if (!(c)) { printf("FALHOU %d: %s\n", __LINE__, #c); falhas++; } } while (0)

int player_carregando(void) { return 1; }
int video_fonte_tocou(void) { return tocouTeste; }
int video_dv_ativo(void) { return dvTeste; }
int video_pronto(void) { return prontoTeste; }
double video_buffer_fim(void) { return bufferTeste; }
int video_n_audio(void) { return audios; }
int video_n_legenda(void) { return legendas; }
int stream_atual(void) { return 0; }
int stream_n(void) { return 2; }
int stream_proxima_sem_perda(int i) { (void)i; return proximaBoa; }
int ajustes_fonte_repor(void) { return 1; }

int main(void) {
  CHECK(!aberturaVencida(14000));
  CHECK(!aberturaVencida(15000));
  CHECK(aberturaVencida(15004)); // fonte realmente muda: continua no curto

  audios = 4; legendas = 4; // trilhas publicadas aos 14 s, nenhum quadro/buffer
  CHECK(!aberturaVencida(14000));
#if defined(NV_ANDROID) || defined(NV_TPK)
  CHECK(!aberturaVencida(15004)); // primeira fonte medida na TCL
  audios = 1; legendas = 2;
  CHECK(!aberturaVencida(15013)); // segunda fonte medida
  CHECK(!aberturaVencida(30000));
  CHECK(aberturaVencida(30001)); // progresso nao vira espera infinita
  audios = 0;
  CHECK(!aberturaVencida(15004)); // legenda conhecida tambem prova resposta
  legendas = 0; audios = 1;
  CHECK(!aberturaVencida(15004)); // audio conhecido, sem legenda
#else
  CHECK(aberturaVencida(15004)); // LG/webOS conserva a regra anterior
#endif
  audios = legendas = 0; fonteVODTentativas++;
  CHECK(aberturaVencida(15004)); // nova fonte sem trilhas: volta ao curto
  prontoTeste = 1;
  CHECK(!aberturaVencida(15004));
  prontoTeste = 0; bufferTeste = 0.6;
  CHECK(!aberturaVencida(15004));
  bufferTeste = 0; proximaBoa = 0; fonteVODTentativas++;
  CHECK(!aberturaVencida(15004)); // nunca baixar qualidade por pressa
  CHECK(aberturaVencida(30001));
  tocouTeste = 1;
  CHECK(!aberturaVencida(40000));
  tocouTeste = 0; dvTeste = 1;
  CHECK(!aberturaVencida(40000)); // caminho video_dv_ativo preservado
  printf("inicio_trilhas: %s (%d falhas)\n", falhas ? "FALHOU" : "ok", falhas);
  return falhas ? 1 : 0;
}
