// Run the production draw/input decisions with a known marker and backend states.
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
static int comVideo = 1, esperandoFonte, erroFonte, saindo, retomadaAplicada;
static int pronto, playing, buffering, iniciando, switching;
static int visivel, skipFoco, duracaoReal = 1, epE, renderVisivel;
static float posSeg, duracaoSeg = 5694;
static double markerStart, markerEnd = 90;
static int cachedVisible, firstFrame, dv, skipLiberado;
static struct { float veu; } cine;
static int dvtela_ativa(void) { return dv; }
static int video_quadro_visto(void) { return firstFrame; }
static int player_carregando(void) { return esperandoFonte || (comVideo && !pronto); }
static int video_tocando(void) { return playing; }
static unsigned video_bufferando_ms(void) { return buffering; }
static int video_iniciando(void) { return iniciando; }
static int fontePrincipal(void) { return !switching; }
static double monoSeg(void) { return 1; }
typedef int CatEp;
static const CatEp *player_proximo_episodio(void) { return NULL; }
static void intro_definir_duracao(double d, int movie) { (void)d; (void)movie; }
static int intro_botao(double pos, double now, int osd, int focus, double *end, int *type) {
  (void)now; (void)osd; (void)focus;
  cachedVisible = pos >= markerStart && pos < markerEnd;
  if (cachedVisible) { if (end) *end = markerEnd; if (type) *type = 1; }
  return cachedVisible;
}
static int intro_botao_visivel(double *end, int *type) {
  if (cachedVisible) { if (end) *end = markerEnd; if (type) *type = 1; }
  return cachedVisible;
}
#include "decision.c"
static void check(int expected) {
  // Stale visibility from the previous playing frame must not accept OK/focus.
  cachedVisible = 1;
  assert(trechoPulavel(NULL) == expected);
  skipFoco = 1;
  renderVisivel = 0;
  desenharAcoesEpisodioCorpo();
  assert(renderVisivel == expected);
  if (!expected) assert(skipFoco == 0);
}
int main(void) {
  for (int resume = 0; resume <= 1; resume++) {
    posSeg = resume ? 56.94f : 0;
    pronto = playing = retomadaAplicada = skipLiberado = 0;
    check(0); // Loading, markers already known, no first frame.
    pronto = 1; check(0); // loadCompleted does not prove playback.
    playing = 1; check(0); // Resume seek not yet applied.
    retomadaAplicada = 1; buffering = 100; check(0);
    buffering = 0; iniciando = 1; check(0); // Native preroll.
    iniciando = 0; switching = 1; check(0); // Old pipeline still playing.
    switching = 0; esperandoFonte = 1; check(0);
    esperandoFonte = 0;
    cine.veu = 1; check(0); // Opening overlay still covers playback.
    cine.veu = 0; dv = 1; check(0); dv = 0;
#ifdef NV_ANDROID
    firstFrame = 0; check(0); // Audio-only ready timeout is not a video frame.
#endif
    firstFrame = 1; check(1);
    playing = 0; check(1); // Paused or scrubbing after playback started.
    buffering = 100; check(1); buffering = 0; playing = 1;
    erroFonte = 1; check(0); erroFonte = 0;
    saindo = 1; check(0); saindo = 0;
  }
  // Direct source switch: stale retomadaAplicada, new pipeline not playing yet.
  skipLiberado = 0; playing = 0; check(0);
  playing = 1; check(1);
  // Host/screenshot mode has no pipeline: the marker alone decides.
  comVideo = 0; playing = retomadaAplicada = skipLiberado = 0; switching = 1;
  check(1);
  esperandoFonte = 1; check(0); esperandoFonte = 0;
  comVideo = 1; switching = 0;
  posSeg = 100;
  cachedVisible = 0;
  assert(!trechoPulavel(NULL));
  renderVisivel = 0;
  desenharAcoesEpisodioCorpo();
  assert(!renderVisivel);
  puts("player_skip_loading: loading/resume/switch blocked; playing marker visible");
}
