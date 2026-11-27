// Ver casa_app.h.
#include "casa_app.h"
#include "ajustes.h"
#include "casa_rede.h"
#include "p2p.h"
#include "p2pmotor.h"

static int resolver(const char *hash, int idx, const char *fontes, int temporada, int episodio,
                    char *url, unsigned n, uint64_t *tam) {
  uint64_t teto = 0, livre = 0;
  int e = p2pmotor_resolver(hash, idx, fontes, temporada, episodio, url, n);
  if (e == P2P_OK && tam) p2pmotor_arquivo_atual(tam, &teto, &livre);
  return e;
}
static const CasaMotor MOTOR = { resolver, p2pmotor_segurar, p2pmotor_ativo };

void casa_app_quadro(void) {
  static int ligado;
  int p2p = ajustes_p2p_ligado(), temMotor = p2pmotor_disponivel();
  // Cliente: so quem ligou o P2P, nao tem motor e nao digitou um servidor.
  int usar = p2p && ajustes_casa_usar() && !temMotor && !ajustes_p2p_url()[0];
  // Ajudante: so com o motor neste pacote e o P2P ligado neste aparelho.
  int ajudar = p2p && ajustes_casa_ajudar() && temMotor;
  if (!ligado) { p2p_definir_casa(casa_ajudante_base); ligado = 1; }
  casa_configurar(usar, ajudar, temMotor ? &MOTOR : 0);
}
