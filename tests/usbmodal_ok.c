// #205: OK on the focused button of the USB flash drive cache modal closes it
// (it used to close only with Back). Dependency modal = uxEditor 2.
#include "../src/p2pmotor.h"
static int tDisp = 1, tUsb;
static int tst_disp(void) { return tDisp; }
int ajustes_p2p_ligado(void);
static int tst_usb(void) { return ajustes_p2p_ligado() && tUsb; }
#define p2pmotor_disponivel tst_disp
#define p2pmotor_usb_presente tst_usb
#include "../src/ajustes.c"
#include <assert.h>

int main(void) {
  memcpy(valor, valorPadrao, sizeof valor);
  valor[AJ_P2P_LIGADO] = 0;   // engine on
  p2pEndereco[0] = 0;
  tUsb = 0;                   // no drive: the option is greyed, OK opens the modal
  assert(inativa(AJ_P2P_DESTINO));
  uxAbrirEditor(AJ_P2P_DESTINO);
  assert(uxEditor == 2);
  uxEvento(SDLK_RETURN);
  assert(uxEditor == 0);      // OK closes it
  uxAbrirEditor(AJ_P2P_DESTINO);
  assert(uxEditor == 2);
  uxEvento(SDLK_ESCAPE);
  assert(uxEditor == 0);      // Back still closes it
  puts("usbmodal_ok: ok");
  return 0;
}
