#define SDL_MAIN_HANDLED
#include "../src/app.c"
#include <assert.h>

static Stream fonte;
static int redirecionou, abriu, modoPlayer, modoVideo;
static double bufferFim;
static char aberta[NV_MEDIA_URL_MAX + 128], cabVideo[512], cabProxy[512];
const Stream *stream_item(int i) { return i == 0 ? &fonte : NULL; }
int proxyts_redirecionou(void) { return redirecionou; }
double video_buffer_fim(void) { return bufferFim; }
void video_definir_cabecalhos(const char *c) { snprintf(cabVideo, sizeof cabVideo, "%s", c); }
void proxyts_definir_cabecalhos(const char *c) { snprintf(cabProxy, sizeof cabProxy, "%s", c); }
void player_modo_canal_tentativa(int modo) { modoPlayer = modo; }
void video_definir_modo_live(int modo) { modoVideo = modo; }
Uint32 SDL_GetTicks(void) { return 1234; }
void player_definir_fonte(const char *url) { abriu++; snprintf(aberta, sizeof aberta, "%s", url); }

static void preparar(void) {
  snprintf(fonte.url, sizeof fonte.url, "nvproxy:https://host/live");
  snprintf(fonte.cabecalhos, sizeof fonte.cabecalhos, "User-Agent: teste\n");
  canalFonteIdx = canalPlanoIdx = canalMemorizado = redirecionou = abriu = 0;
  canalFonteDesde = 0;
  canalTentativas = 1;
  bufferFim = 4;
  canalViaProxy = 1;
  canalPlanoN = 3;
  canalPlano[0] = (RotaTentativa){0, 1};
  canalPlano[1] = (RotaTentativa){0, 0};
  canalPlano[2] = (RotaTentativa){2, 1};
}
int main(void) {
  preparar();
  assert(retentarCanal() && abriu == 1 && canalTentativas == 2);
  assert(!strcmp(aberta, "https://host/live") && !canalViaProxy);
  assert(!cabProxy[0] && !strcmp(cabVideo, fonte.cabecalhos));
  assert(canalFonteDesde == 1234 && modoPlayer == 0 && modoVideo == 0);
  assert(retentarCanal() && abriu == 2 && canalTentativas == 3);
  assert(!strcmp(aberta, fonte.url) && canalViaProxy);
  assert(!strcmp(cabProxy, fonte.cabecalhos) && modoPlayer == 2 && modoVideo == 2);
  assert(!retentarCanal() && abriu == 2);
  preparar(); redirecionou = 1;
  assert(retentarCanal() && canalPlanoIdx == 2 && modoPlayer == 2 && abriu == 1);
  preparar(); canalTentativas = ROTAHOST_TETO_CANAL;
  assert(!retentarCanal() && !abriu);
  preparar(); canalTentativas = ROTAHOST_TETO_CANAL - 1;
  assert(retentarCanal() && canalTentativas == ROTAHOST_TETO_CANAL);
  assert(!retentarCanal() && abriu == 1);
  preparar(); canalFonteIdx = -1;
  assert(!retentarCanal() && !abriu);
  preparar(); canalPlanoN = 1;
  assert(!retentarCanal() && !abriu);
  preparar(); bufferFim = 0;
  assert(retentarCanal() && !canalViaProxy && abriu == 1);
  assert(!retentarCanal() && abriu == 1 && canalTentativas == 2);
  preparar(); bufferFim = 0; redirecionou = 1;
  assert(!retentarCanal() && !abriu && canalTentativas == 1);
  RotaPlanoEntrada e = {"https://host/live", 1, 0, 1, -1, 0, 1};
  preparar(); canalViaProxy = 0;
  canalPlanoN = rotahost_plano(&e, canalPlano, ROTAHOST_TENTATIVAS);
  assert(canalPlanoN == 3 && canalPlano[1].rota == 1);
  assert(retentarCanal() && canalViaProxy && abriu == 1);
  preparar(); canalViaProxy = 0; bufferFim = 0.5;
  canalPlanoN = rotahost_plano(&e, canalPlano, ROTAHOST_TENTATIVAS);
  assert(!retentarCanal() && !abriu && canalTentativas == 1);
  preparar(); e.proxyForcado = 1; bufferFim = 0;
  canalPlanoN = rotahost_plano(&e, canalPlano, ROTAHOST_TENTATIVAS);
  assert(canalPlanoN == 3 && canalPlano[1].rota == 0);
  assert(retentarCanal() && !canalViaProxy && abriu == 1);
  assert(!retentarCanal() && abriu == 1);
  preparar(); bufferFim = 0;
  canalPlano[1] = (RotaTentativa){2, 0};
  assert(!retentarCanal() && !abriu);
  puts("rotahost_canal: ordem, cabecalhos, modo, redirecionamento e teto ok");
  return 0;
}
