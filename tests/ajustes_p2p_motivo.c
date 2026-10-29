// #205: motivo especifico por causa em P2P > Cache do P2P e Limite de espaco.
#include "../src/p2pmotor.h"
static int tDisp, tUsb;
static int tst_disp(void) { return tDisp; }
int ajustes_p2p_ligado(void);
static int tst_usb(void) { return ajustes_p2p_ligado() && tUsb; }
#define p2pmotor_disponivel tst_disp
#define p2pmotor_usb_presente tst_usb
#ifdef P2PM_ANDROID_VISIVEL
#define NV_ANDROID 1   // only for ajustes.c: the other sources stay desktop
void android_limpar_cache(void) {}
void extplayer_atualizar_apps(void) {}
int extplayer_n_apps(void) { return 0; }
const char *extplayer_app_pkg(int i) { (void)i; return ""; }
const char *extplayer_app_nome(int i) { (void)i; return ""; }
void android_baixar_registro(const char *a, const char *b, const char *c) { (void)a; (void)b; (void)c; }
#endif
#include "../src/ajustes.c"
#include <assert.h>

static void ok(int op, const char *pt) {
  assert(inativa(op));
  assert(!strcmp(ajudaOpcaoBase(op), pt));
}

int main(void) {
  memcpy(valor, valorPadrao, sizeof valor);
  const char *motor = "O motor P2P embutido não está disponível nesta TV.";
  const char *ext = "O cache fica no servidor P2P configurado em Endereço do servidor P2P. Escolha Nesta TV para usar o motor desta TV.";
  const char *usb = "Conecte um pendrive USB com espaço livre na TV para guardar o cache nele. Sem pendrive o cache fica no armazenamento interno.";
  const char *lim = "O limite vale só para o motor desta TV. Com um servidor P2P configurado, quem guarda o cache é o servidor.";
  const char *ligar = "Ative P2P para escolher onde guardar o cache.";
  valor[AJ_P2P_LIGADO] = 1;
  assert(!ajustes_p2p_ligado());
  tDisp = 0; tUsb = 1; p2pEndereco[0] = 0;
  ok(AJ_P2P_DESTINO, motor); ok(AJ_P2P_LIMITE, motor);
  tDisp = 1; snprintf(p2pEndereco, sizeof p2pEndereco, "http://x:1");
  ok(AJ_P2P_DESTINO, ext); ok(AJ_P2P_LIMITE, lim);
  p2pEndereco[0] = 0;
  ok(AJ_P2P_DESTINO, ligar);
  assert(!inativa(AJ_P2P_LIMITE));
  tUsb = 0;
  ok(AJ_P2P_DESTINO, ligar);
  valor[AJ_P2P_LIGADO] = 0;
  assert(ajustes_p2p_ligado());
  ok(AJ_P2P_DESTINO, usb);
  tUsb = 1;
  assert(!inativa(AJ_P2P_DESTINO));
  assert(!inativa(AJ_P2P_LIMITE));
#if defined(NV_ANDROID) && !defined(NV_P2P_MOTOR)
  assert(foraDestaPlataforma(AJ_P2P_DESTINO));
  assert(foraDestaPlataforma(AJ_P2P_LIMITE));
#else
  assert(!foraDestaPlataforma(AJ_P2P_DESTINO));
#endif
  puts("ajustes_p2p_motivo: ok");
  return 0;
}
