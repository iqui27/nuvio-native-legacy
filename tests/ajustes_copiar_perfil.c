// #679: copiar personalizacao do perfil origem para o destino.
static void testarHeranca(void);
#include "../src/ajustes.c"
#include "nvtmp.h"
#include <assert.h>
#include <string.h>
#include <unistd.h>

int main(void) {
  char dir[NVT_MAX];
  nvt_set(dir, sizeof dir, "/nuvio-copiar-perfil-XXXXXX");
  assert(mkdtemp(dir));
  setenv("NUVIO_DADOS", dir, 1);
  dados_iniciar(dir);
  ajustes_dir(dir);

  valor[AJ_HERO] = 1;
  valor[AJ_MENU_TOPO] = 1;
  valor[AJ_RAIL] = 1;
  ajustes_perfil_guardar(1);

  valor[AJ_HERO] = 0;
  valor[AJ_MENU_TOPO] = 0;
  valor[AJ_RAIL] = 0;
  ajustes_perfil_guardar(2);

  // Recarrega o perfil 1 (ativo): a copia le o que esta na memoria dele.
  ajustes_perfil_restaurar(1);
  assert(ajustes_copiar_perfil(1, 2) >= 3);
  assert(ajustes_perfil_restaurar(2) == 1);
  assert(valor[AJ_HERO] == 1 && valor[AJ_MENU_TOPO] == 1 && valor[AJ_RAIL] == 1);

  puts("ok ajustes_copiar_perfil");
  testarHeranca();
  return 0;
}

// #679: com "Personalização igual ao perfil principal" ligado no principal,
// o secundario herda as chaves locais que nunca salvou; as que ja tem ficam.
void testarHeranca(void) {
  char dir[NVT_MAX];
  nvt_set(dir, sizeof dir, "/nuvio-herda-perfil-XXXXXX");
  assert(mkdtemp(dir));
  setenv("NUVIO_DADOS", dir, 1);
  dados_iniciar(dir);
  ajustes_dir(dir);

  // Principal (ativo=1, o padrao do teste): vidro ligado e a flag ligada.
  memset(valor, 0, sizeof valor);
  for (int i = 0; i < AJ_N; i++) valor[i] = valorPadrao[i];
  valor[AJ_VIDRO] = 0;                      // ligado
  valor[AJ_VIDRO_OPAC] = 4;                 // 0.92
  valor[AJ_AJUSTES_DO_PRINCIPAL] = 0;       // heranca ligada
  ajustes_perfil_guardar(1);

  // Secundario sem arquivo proprio: restaurar deve herdar vidro+opacidade.
  ajustes_perfil_restaurar(1);              // valor[] volta a ser o principal salvo
  ajustes_perfil_restaurar(2);
  assert(valor[AJ_VIDRO] == 0 && valor[AJ_VIDRO_OPAC] == 4);

  // Com a flag desligada no principal, nada herda.
  ajustes_perfil_restaurar(1);
  valor[AJ_AJUSTES_DO_PRINCIPAL] = 1;       // desligado
  ajustes_perfil_guardar(1);
  memset(valor, 0, sizeof valor);
  for (int i = 0; i < AJ_N; i++) valor[i] = valorPadrao[i];
  valor[AJ_AJUSTES_DO_PRINCIPAL] = 1;
  ajustes_perfil_restaurar(2);
  assert(valor[AJ_VIDRO_OPAC] == valorPadrao[AJ_VIDRO_OPAC]);

  // Opt-in: o padrao da opcao e Desligado — um principal novo, que nunca
  // mexeu no ajuste, nao grava nem aplica a personalizacao em secundarios.
  assert(valorPadrao[AJ_AJUSTES_DO_PRINCIPAL] == 1);
  nvt_set(dir, sizeof dir, "/nuvio-herda-optin-XXXXXX");
  assert(mkdtemp(dir));
  setenv("NUVIO_DADOS", dir, 1);
  dados_iniciar(dir);
  ajustes_dir(dir);
  memset(valor, 0, sizeof valor);
  for (int i = 0; i < AJ_N; i++) valor[i] = valorPadrao[i];
  valor[AJ_VIDRO_OPAC] = 4;                 // principal com personalizacao
  ajustes_perfil_guardar(1);                // grava a flag no padrao (Desligado)
  ajustes_perfil_restaurar(1);
  ajustes_perfil_restaurar(2);              // secundario parcial/sem arquivo
  assert(valor[AJ_VIDRO_OPAC] == valorPadrao[AJ_VIDRO_OPAC]);

  puts("ok ajustes_heranca_principal");
}
