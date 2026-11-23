/* Sonda do contrato aparelho.modelo (farol-8, volta 3): passa UM modelo pelo
 * caminho de producao — receptor farolperfil_definir_modelo (o mesmo da LG
 * webOS e do host TPK), coleta, farolperfil_escrever e o redator fechado —
 * e imprime o FarolJson resultante em uma linha. O .mjs companheiro valida o
 * aparelho.modelo emitido com o modeloOk REAL do servidor publicado e o
 * registro inteiro com a rotaFarolRegistro local.
 *
 *   sonda <modelo>     imprime o JSON (modelo presente ou "x")
 *
 * Modelo que a privacidade do coletor recusa (URL, @, IPv4) faz o receptor
 * devolver 0 e o cache fica "?" — o JSON sai com modelo "x" do mesmo jeito,
 * que e o que interessa ao contrato. Compilada sem FAROL_PERFIL_TESTE: so API
 * de producao. */
#include "farolperfil.h"
#include <assert.h>
#include <stdio.h>

int main(int argc, char **argv) {
  char json[FAROL_JSON_N];
  FarolJson redator;
  FarolPerfil perfil;

  assert(argc == 2);
  (void)farolperfil_definir_modelo(argv[1]);
  farolperfil_iniciar();
  farolperfil_encerrar();
  assert(farolperfil_obter(&perfil));
  assert(farolredator_abrir(&redator, json, sizeof json));
  assert(farolperfil_escrever(&redator, &perfil));
  assert(farolredator_fechar(&redator));
  printf("%s\n", json);
  return 0;
}
