// Diagnostics "Download log" card, REAL render with no visible window: hidden
// GL window, FBO, glReadPixels (as tests/novidades148_shot.c). The actual
// celb_desenhar/qrDesenhar/gfx path draws the local QR, the same address after
// OK refresh, and the cloud QR once the upload is "ready". The cloud HTTP
// boundary is an in-process fixture (tests/celbotao_log_shot.sh renames
// rede_pedir in logcloud.o only, as tests/logcloud_flow.c replaces it): no
// sockets to any service, no account, no upload. The local page listens on
// the LAN like the app (tests/celbotao_log.c does the same).
//
// Proof without a decoder: every module (and the quiet zone) read back from
// the framebuffer matches qr_gerar's matrix for the expected URL. The
// 134-byte cloud link (longer than the old 96-byte cache key) is generated
// once and then reused frame after frame (no GL re-upload). URLs are never
// printed.
#define qr_gerar shot_qr_gerar   // count generations; the real encoder runs
#include "../src/celbotao.c"
#undef qr_gerar
#include "nvtmp.h"
#include "rede.h"
#include "tex_cache.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <sys/stat.h>

int qr_gerar(Qr *q, const char *texto);
static int geracoes, pedidos;
int shot_qr_gerar(Qr *q, const char *t) { geracoes++; return qr_gerar(q, t); }

static char nuvemLink[QR_MAX_BYTES + 1];
int shot_rede_pedir(const RedePedido *q, RedeResposta *r) {
  if (strcmp(q->url, "https://fixture.invalid/v1/loglink")) return 0;
  r->corpo = malloc(sizeof nuvemLink + 16);
  assert(r->corpo);
  snprintf(r->corpo, sizeof nuvemLink + 16, "{\"url\":\"%s\"}", nuvemLink);
  r->status = 200;
  pedidos++;
  return 1;
}
void shot_rede_limpar(RedeResposta *r) { free(r->corpo); r->corpo = NULL; }

static GLuint fbo, fboTex;
static unsigned char pix[1920 * 1080 * 4];

static void tecla(SDL_Keycode k) {
  SDL_Event e; memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN; e.key.keysym.sym = k;
  assert(celb_evento(&e));
}
static void quadro(void) {
  SDL_PumpEvents();
  txt_novo_quadro();
  tex_novo_quadro();
  tex_bombear(8);
  gfx_novo_quadro();
  celb_atualizar(1.0f / 60.0f);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glViewport(0, 0, 1920, 1080);
  glClearColor(0.051f, 0.051f, 0.051f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  celb_desenhar();
  glFinish();
}
static void quadros(int n) { while (n-- > 0) quadro(); }

static int escuro(float fx, float fy) {
  int x = (int)fx, y = (int)fy;
  const unsigned char *p;
  assert(x >= 0 && x < 1920 && y >= 0 && y < 1080);
  p = pix + ((size_t)(1079 - y) * 1920 + x) * 4;
  return p[0] + p[1] + p[2] < 3 * 128;
}
// Reads the frame back, compares the drawn QR with the encoder, saves a BMP.
static void capturar(const char *saida, const char *nome, const char *u) {
  Qr q;
  int lado, x, y, erros = 0;
  float k, x0, y0, m;
  char arq[700];
  SDL_Surface *s;
  glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pix);
  { float a = gfx_escala_entrar(); k = gfx_escala(); gfx_escala_sair(a); }
  assert(qr_gerar(&q, u));
  lado = q.lado + 8;   // qrTextura: four quiet modules on every edge
  // celb_desenharCorpo_ (CELB_LOG): QR under the title, centered in the card.
  x0 = (cartao.x + (CB_W - CB_QR) * 0.5f) * k;
  y0 = (cartao.y + CB_PAD + CB_TITULO_H + 18.0f + CB_MOLDURA) * k;
  m = CB_QR * k / (float)lado;
  for (y = 0; y < lado; y++)
    for (x = 0; x < lado; x++)
      if (escuro(x0 + (x + 0.5f) * m, y0 + (y + 0.5f) * m) != qr_modulo(&q, x - 4, y - 4)) erros++;
  if (erros) printf("FAIL: %s QR: %d of %d modules differ from the encoder\n", nome, erros, lado * lado);
  assert(!erros);
  printf("ok: %s QR rendered, %dx%d modules match the encoder\n", nome, lado, lado);
  s = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32);
  assert(s);
  for (y = 0; y < 1080; y++)
    memcpy((char *)s->pixels + y * s->pitch, pix + (size_t)(1079 - y) * 1920 * 4, 1920 * 4);
  snprintf(arq, sizeof arq, "%s-%s.bmp", saida, nome);
  assert(SDL_SaveBMP(s, arq) == 0);
  SDL_FreeSurface(s);
  printf("capture: %s\n", arq);
}

int main(int argc, char **argv) {
  const char *saida = argc > 1 ? argv[1] : nvt("/nuvio-celb-log");
  const char *dir = getenv("NUVIO_DADOS"), *esperado = getenv("NV_SHOT_ESPERADO");
  char cam[700], local[256];
  SDL_Window *w;
  FILE *f;
  int g, i;
  if (!dir || !dir[0]) return 2;
  // Synthetic log and settings: the switch is on (0 = on, off by default).
  snprintf(cam, sizeof cam, "%s/nuvio.log", dir);
  f = fopen(cam, "w"); assert(f);
  fputs("[app] start=1\n[net] password=SHOT_SECRET\n", f); fclose(f);
  setenv("NUVIO_LOG", cam, 1);
  snprintf(cam, sizeof cam, "%s/ajustes.txt", dir);
  f = fopen(cam, "w"); assert(f);
  fputs("diagnosticsQrLocal 0\n", f); fclose(f);
  dados_iniciar(dir);
  ajustes_dir(dir);
  assert(ajustes_diagnostics_qr());
  // Cloud link exactly as long as the QR carries (old cache key: 96 bytes).
  snprintf(nuvemLink, sizeof nuvemLink, "https://fixture.invalid/v1/loglink/");
  for (i = (int)strlen(nuvemLink); i < QR_MAX_BYTES; i++) nuvemLink[i] = "0123456789abcdef"[i % 16];
  nuvemLink[QR_MAX_BYTES] = 0;
  assert(strlen(nuvemLink) > 96);

  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");   // never takes focus
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  w = SDL_CreateWindow("celbotao-log-shot", 0, 0, 64, 64, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(w);
  assert(SDL_GL_CreateContext(w));
  glGenTextures(1, &fboTex);
  glBindTexture(GL_TEXTURE_2D, fboTex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1920, 1080, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  glGenFramebuffers(1, &fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fboTex, 0);
  assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
  glViewport(0, 0, 1920, 1080);
  gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1));
  tex_iniciar(64);
  gfx_icones_dir("deploy/app/art");

  // LOCAL: the computer address as QR, generated once for all frames.
  assert(celb_abrir(CELB_LOG, "Registro"));
  if (celular_estado() != CEL_ESPERANDO) printf("FAIL: no local address: %s\n", motivoLog);
  assert(celular_estado() == CEL_ESPERANDO && celular_url()[0] && !nuvem);
  snprintf(local, sizeof local, "%s", celular_url());
  quadros(90);
  assert(geracoes == 1 && !strcmp(texQrDe, local));
  capturar(saida, "local", local);

  // OK: fresh snapshot at the SAME address, so the same cached QR.
  tecla(SDLK_RETURN);
  quadros(30);
  assert(!strcmp(celular_url(), local) && logSnapshot[0] && logcloud_estado() == LOGCLOUD_PARADO);
  assert(geracoes == 1 && pedidos == 0);
  capturar(saida, "local-ok", local);

  // RIGHT: explicit cloud action; the fixture answers with the long link.
  g = geracoes;
  tecla(SDLK_RIGHT);
  for (i = 0; i < 600 && logcloud_estado() != LOGCLOUD_PRONTO; i++) { quadro(); SDL_Delay(5); }
  assert(logcloud_estado() == LOGCLOUD_PRONTO && pedidos == 1);
  quadros(90);
  assert(geracoes == g + 1 && !strcmp(texQrDe, nuvemLink));   // once, then cached
  // Real GL uploads (gl_compat.h counter): none while the link stays on screen.
  perftv_enabled = 1; i = perftv_uploads;
  quadros(60);
  printf("cloud QR: texture uploads in 60 steady frames: %d\n", perftv_uploads - i);
  assert(perftv_uploads == i && geracoes == g + 1);
  perftv_enabled = 0;
  capturar(saida, "cloud", nuvemLink);

  if (esperado && esperado[0]) {   // for the optional decoder in the .sh
    f = fopen(esperado, "w"); assert(f);
    fprintf(f, "%s\n%s\n", local, nuvemLink); fclose(f);
  }
  tecla(SDLK_ESCAPE);
  assert(!celb_aberto() && !texQr && !logSnapshot[0] && logcloud_estado() == LOGCLOUD_PARADO);
  puts("PASS: diagnostics card renders local QR, same QR after OK, cloud QR once (hidden window, no network)");
  return 0;
}
