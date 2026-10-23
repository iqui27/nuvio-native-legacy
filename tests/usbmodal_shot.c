// #205: PNGs of the USB flash drive cache modal at 1920x1080 for three states
// (drive in use with cache, drive rejected with a reason, no drive). The drive
// info is injected (the UI only reads p2pmotor_usb_info), so no disk is needed.
// -DUSBMODAL_ANTES builds the same states against a tree without that API.
#include "../src/p2pmotor.h"
#include <SDL2/SDL_image.h>
static int tDisp = 1, tUsb;
static int tst_disp(void) { return tDisp; }
int ajustes_p2p_ligado(void);
static int tst_usb(void) { return ajustes_p2p_ligado() && tUsb; }
#define p2pmotor_disponivel tst_disp
#define p2pmotor_usb_presente tst_usb
#ifndef USBMODAL_ANTES
static P2pmUsbInfo tInfo;
static void tst_info(P2pmUsbInfo *o) { *o = tInfo; }
#define p2pmotor_usb_info tst_info
#endif
#include "../src/ajustes.c"
#include "badges.h"
#include <assert.h>

static void captura(const char *nome, SDL_Window *win) {
  for (int i = 0; i < 60; i++) {
    SDL_PumpEvents();
    txt_novo_quadro();
    tex_novo_quadro();
    tex_bombear(6);
    ajustes_atualizar(1.0f / 60.0f, SDL_GetTicks());
    glClearColor(0.025f, 0.025f, 0.03f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ajustes_desenhar(SDL_GetTicks());
    SDL_GL_SwapWindow(win);
  }
  unsigned char *pix = malloc(1920 * 1080 * 4);
  glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pix);
  SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32);
  for (int y = 0; y < 1080; y++) memcpy((char *)s->pixels + y * s->pitch, pix + (1079 - y) * 1920 * 4, 1920 * 4);
  assert(IMG_SavePNG(s, nome) == 0);
  SDL_FreeSurface(s);
  free(pix);
  printf("captura: %s\n", nome);
}

int main(int argc, char **argv) {
  const char *dir = argc > 1 ? argv[1] : "/tmp", *pre = argc > 2 ? argv[2] : "usbmodal";
  char nome[700];
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_Window *w = SDL_CreateWindow("usbmodal", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1920, 1080,
                                   SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
  assert(w && SDL_GL_CreateContext(w));
  SDL_GL_SetSwapInterval(0);
  glViewport(0, 0, 1920, 1080);
  gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1));
  tex_iniciar(64);
  gfx_icones_dir("deploy/app/art");
  ajustes_recursos("deploy/app/art");
  badges_carregar("deploy/app/art");
  ajustes_iniciar();
  for (int est = 0; est < 3; est++) {
    static const char *const NOME[] = { "em-uso", "recusado", "sem-pendrive" };
    ajustes_teste_ux_captura(0);
    valor[AJ_P2P_LIGADO] = 0;   // 0 = engine on (see ajustes_p2p_motivo)
    tUsb = est == 0;
#ifndef USBMODAL_ANTES
    memset(&tInfo, 0, sizeof tInfo);
    if (est == 0) {
      tInfo = (P2pmUsbInfo){ .estado = 1, .emUso = 1, .gravavel = 1, .motivo = P2PM_USB_NONE,
                             .total = 14ull << 30, .livre = 9ull << 30, .cache = 1536ull << 20, .limite = 4ull << 30 };
      snprintf(tInfo.nome, sizeof tInfo.nome, "SANDISK32");
      snprintf(tInfo.caminho, sizeof tInfo.caminho, "/media/usb/SANDISK32");
      snprintf(tInfo.fs, sizeof tInfo.fs, "vfat");
    } else if (est == 1) {
      tInfo = (P2pmUsbInfo){ .estado = 2, .motivo = P2PM_USB_READONLY, .total = 29ull << 30, .livre = 20ull << 30 };
      snprintf(tInfo.nome, sizeof tInfo.nome, "BACKUP");
      snprintf(tInfo.caminho, sizeof tInfo.caminho, "/media/usb/BACKUP");
      snprintf(tInfo.fs, sizeof tInfo.fs, "ntfs");
    }
    if (getenv("USBMODAL_WEBOS")) ajUsbWebos = 1;
#endif
    focarOpcao(AJ_P2P_DESTINO);
    uxAbrirEditor(AJ_P2P_DESTINO);
    snprintf(nome, sizeof nome, "%s/%s-%s.png", dir, pre, NOME[est]);
    captura(nome, w);
  }
  return 0;
}
