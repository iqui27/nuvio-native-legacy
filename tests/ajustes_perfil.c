// AJUSTES POR PERFIL NESTA TV (ajustes_perfil_guardar/_restaurar/_esquecer).
//
// A troca de perfil guarda os ajustes do perfil que sai e devolve os do que
// entra (sync_trocar_perfil). O que este teste prova, sem SDL na tela:
//   1. a copia leva o que e DO PERFIL e volta igual;
//   2. o que e DESTE APARELHO (superficie 4K) nao entra na copia: trocar de
//      perfil nunca muda o que descreve a TV;
//   3. perfil sem copia devolve 0 e nao mexe em nada (quem chama decide partir
//      do principal);
//   4. o logout apaga as copias.
#include "../src/ajustes.c"
#include <assert.h>
#include <unistd.h>

int main(void) {
  char dir[] = "/tmp/nuvio-aj-perfil-XXXXXX";
  assert(mkdtemp(dir));
  setenv("NUVIO_DADOS", dir, 1);
  dados_iniciar(dir);
  ajustes_dir(dir);
  // Sem ajustes.txt, o ajuste novo nasce LIGADO (V_LIGA: 0 = Ligado).
  assert(ajustes_addons_do_principal() == 1);
  assert(!ajustes_auto_creditos());

  // Perfil 1: destaque desligado, 4K pedido. 2.0.2: ele manda o historico so
  // para o Simkl e nao quer o social; o perfil 2 e o contrario.
  valor[AJ_CW_FONTE] = AJ_CWF_SIMKL;
  valor[AJ_SALVOS_DEST] = AJ_SALVOS_SIMKL;
  valor[AJ_HIST_CONTA] = 1;     // Desligado
  valor[AJ_SOCIAL] = 1;         // Desligado
  valor[AJ_AUTO_CREDITOS] = 0;  // Ligado only for profile 1
  valor[AJ_HERO] = 1;
  valor[AJ_RESOLUCAO] = RES_4K;
  valor[AJ_ADDONS_PRINCIPAL] = 0;
  ajustes_perfil_guardar(1);

  // O perfil 2 mexe nas duas coisas.
  valor[AJ_CW_FONTE] = AJ_CWF_CONTA;
  valor[AJ_SALVOS_DEST] = AJ_SALVOS_LOCAL;
  valor[AJ_HIST_CONTA] = 0;
  valor[AJ_SOCIAL] = 0;
  valor[AJ_AUTO_CREDITOS] = 1;
  valor[AJ_HERO] = 0;
  valor[AJ_RESOLUCAO] = 0;
  valor[AJ_ADDONS_PRINCIPAL] = 1;
  ajustes_perfil_guardar(2);

  // Volta ao 1: o destaque dele volta, a TV continua como esta.
  assert(ajustes_perfil_restaurar(1) == 1);
  assert(valor[AJ_HERO] == 1);
  assert(valor[AJ_RESOLUCAO] == 0);
  assert(valor[AJ_ADDONS_PRINCIPAL] == 1);   // ajuste desta TV, nao do perfil
  assert(valor[AJ_CW_FONTE] == AJ_CWF_SIMKL && valor[AJ_SALVOS_DEST] == AJ_SALVOS_SIMKL);
  assert(!ajustes_hist_conta() && !ajustes_social());
  assert(ajustes_auto_creditos());

  // E o 2 de novo.
  assert(ajustes_perfil_restaurar(2) == 1);
  assert(valor[AJ_HERO] == 0);
  assert(valor[AJ_CW_FONTE] == AJ_CWF_CONTA && valor[AJ_SALVOS_DEST] == AJ_SALVOS_LOCAL);
  assert(ajustes_hist_conta() && ajustes_social());
  assert(!ajustes_auto_creditos());

  // Perfil nunca usado aqui: nada muda.
  valor[AJ_HERO] = 1;
  assert(ajustes_perfil_restaurar(3) == 0);
  assert(valor[AJ_HERO] == 1);

  // A copia nao carrega linha de aparelho nem chave local com "-".
  { char *t = dados_ler("ajustes-p1.txt");
    assert(t);
    assert(!strstr(t, CHAVE[AJ_RESOLUCAO]));
    assert(!strstr(t, CHAVE[AJ_ADDONS_PRINCIPAL]));
    assert(!strstr(t, "\n-") && t[0] != '-');
    assert(strstr(t, CHAVE[AJ_HERO]));
    assert(strstr(t, "autoCreditosLocal 0\n"));
    free(t); }

  // An older or foreign client may upload device-only keys. Pull must
  // protect the same settings that export and profile copies protect.
  valor[AJ_TEX_MB] = 1;
  valor[AJ_QUALIDADE_IMG] = 1;
  valor[AJ_GPU_EFEITOS] = 1;
  valor[AJ_HERO] = 1;
  char blob[1024];
  snprintf(blob, sizeof blob,
           "{\"%s\":2,\"%s\":0,\"%s\":0,\"%s\":0}",
           CHAVE[AJ_TEX_MB], CHAVE[AJ_QUALIDADE_IMG],
           CHAVE[AJ_GPU_EFEITOS], CHAVE[AJ_HERO]);
  ajustes_aplicar_blob(blob);
  assert(valor[AJ_TEX_MB] == 1);
  assert(valor[AJ_QUALIDADE_IMG] == 1);
  assert(valor[AJ_GPU_EFEITOS] == 1);
  assert(valor[AJ_HERO] == 0);  // Person preference still follows the account.

  // Neither spelling in an account blob controls this local profile choice.
  valor[AJ_AUTO_CREDITOS] = 1;
  ajustes_aplicar_blob("{\"autoCreditosLocal\":true,\"auto_creditos_local\":true}");
  assert(!ajustes_auto_creditos());
  char *merged = NULL;
  valor[AJ_PAUSA_OVERLAY] = 0;
  assert(ajustes_mesclar_blob("{\"pauseOverlayEnabled\":false}", &merged) > 0);
  assert(merged && !strstr(merged, "autoCreditos") && !strstr(merged, "auto_creditos"));
  free(merged);

  // An old snapshot has no credit key and must not inherit the previous ON.
  dados_gravar("ajustes-p4.txt", "heroSectionEnabled 1\n");
  valor[AJ_AUTO_CREDITOS] = 0;
  assert(ajustes_perfil_restaurar(4));
  assert(!ajustes_auto_creditos());
  // First visit: primary-profile inheritance keeps its other settings, but
  // the new profile's credits remain off. The helper changes no other option.
  assert(ajustes_perfil_restaurar(1) && ajustes_auto_creditos());
  int hero = valor[AJ_HERO];
  ajustes_auto_creditos_restaurar(5);
  assert(!ajustes_auto_creditos() && valor[AJ_HERO] == hero);
  ajustes_auto_creditos_restaurar(1);
  assert(ajustes_auto_creditos() && valor[AJ_HERO] == hero);
  // Invalid persisted choices also fall back to off.
  dados_gravar("ajustes-p6.txt", "autoCreditosLocal 99\n");
  ajustes_auto_creditos_restaurar(6);
  assert(!ajustes_auto_creditos());

  valor[AJ_AUTO_CREDITOS] = 0;
  assert(gravar());
  valor[AJ_AUTO_CREDITOS] = 1;
  ajustes_dir(dir);
  assert(ajustes_auto_creditos());

  // Logout: nenhuma copia sobra.
  ajustes_perfil_esquecer();
  assert(!ajustes_auto_creditos());
  ajustes_dir(dir);
  assert(!ajustes_auto_creditos());
  assert(ajustes_perfil_restaurar(1) == 0);
  assert(ajustes_perfil_restaurar(2) == 0);

  puts("ajustes_perfil: tudo ok");
  return 0;
}
