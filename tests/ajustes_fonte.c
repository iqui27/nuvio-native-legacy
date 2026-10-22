// A escolha visual, o renderer e o valor persistido precisam concordar.
#include "../src/ajustes.c"
#include <assert.h>
#include <unistd.h>

int main(void) {
  char dir[] = "/tmp/nuvio-aj-fonte-XXXXXX";
  char path[700];
  assert(mkdtemp(dir));
  snprintf(path, sizeof path, "%s/envio-151.txt", dir);
  FILE *f = fopen(path, "w"); assert(f); fputs("1\n", f); fclose(f);
  ajustes_dir(dir);
  assert(valor[AJ_FONTE_UI] == 0);
  assert(txt_fonte_interface() == TXT_FAMILIA_INTER);
  for (int n = 0; n < OPCOES[AJ_FONTE_UI].n; n++) {
    mudarValor(AJ_FONTE_UI, 1);
    int escolhido = valor[AJ_FONTE_UI];
    const char *nome = OPCOES[AJ_FONTE_UI].valores[escolhido];
    assert(!strcmp(nome, TXT_FAMILIAS_PT[txt_fonte_interface()]));
    TxtFamilia familia = txt_fonte_interface();
    valor[AJ_FONTE_UI] = 0;
    txt_definir_fonte_interface(TXT_FAMILIA_INTER);
    ajustes_dir(dir);
    assert(valor[AJ_FONTE_UI] == escolhido);
    assert(txt_fonte_interface() == familia);
    ajustes_aplicar_blob("{\"fonteInterface\":0,\"fonte_interface\":0}");
    assert(valor[AJ_FONTE_UI] == escolhido);
    assert(txt_fonte_interface() == familia);
  }
  // #400: escolha do aparelho; persistencia sem importar/exportar pela conta.
  assert(!ajustes_fonte_ordem_addon());
  assert(somenteDesteAparelho(AJ_FONTE_ORDEM_ADDON));
  assert(!dePerfil(AJ_FONTE_ORDEM_ADDON));
  assert(!strcmp(CHAVE[AJ_FONTE_ORDEM_ADDON], "fonteOrdemLocal"));
  mudarValor(AJ_FONTE_ORDEM_ADDON, 1);
  assert(ajustes_fonte_ordem_addon());
  valor[AJ_FONTE_ORDEM_ADDON] = 0;
  ajustes_dir(dir);
  assert(ajustes_fonte_ordem_addon());
  ajustes_aplicar_blob("{\"fonteOrdemLocal\":0,\"fonte_ordem_local\":0}");
  assert(ajustes_fonte_ordem_addon());
  for (int i = 0; i + 1 < (int)(sizeof TELA / sizeof *TELA); i++)
    if (TELA[i].op == AJ_FONTE_TEXTO)
      assert(TELA[i + 1].op == AJ_FONTE_ORDEM_ADDON);
  ajustes_abrir_na_fonte();
  ajustes_iniciar();
  assert(!ajustes_foco_no_indice());
  assert(TELA[focoItem].op == AJ_FONTE_UI);
  snprintf(path, sizeof path, "%s/ajustes.txt", dir); unlink(path);
  snprintf(path, sizeof path, "%s/envio-151.txt", dir); unlink(path);
  rmdir(dir);
  puts("ajustes_fonte: nomes, troca, persistencia e sync ok");
  return 0;
}
