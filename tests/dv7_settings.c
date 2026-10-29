/* New DV7 preference cannot shift the upstream append-only settings arrays. */
#include "../src/ajustes.c"
#include <assert.h>
int main(void) {
  assert(!ajustes_dv7());
  assert(valorPadrao[AJ_DV7]==1 && !strcmp(CHAVE[AJ_DV7],"dv7ConversionLocal"));
  assert(OPCOES[AJ_DV7].tipo==OP_ESCOLHA && OPCOES[AJ_LIMPAR_CACHE].tipo==OP_ACAO);
  assert(!strcmp(CHAVE[AJ_LIMPAR_CACHE],"-limparCache"));
  assert(somenteDesteAparelho(AJ_DV7) && !dePerfil(AJ_DV7));
  assert(!ajustePortavel(AJ_DV7) && !ajusteAparelho(AJ_DV7));
  valor[AJ_DV_MKV]=1; valor[AJ_DV7]=0;
  assert(ajustes_dv7() && !ajustes_dv_mkv());
  assert(!ajustes_aplicar_blob("{\"c_settings_v1\":{\"values\":{\"c_dv7ConversionLocal\":1}}}"));
  assert(ajustes_dv7());
  int n=0;
  for(int i=0;i<AJ_N_TELA;i++) if(TELA[i].tipo==IT_OPC && TELA[i].op==AJ_DV7)n++;
#ifdef NV_DV7_CONVERSION
  assert(n==1);
#else
  assert(n==0);
#endif
  puts("PASS: DV7 defaults off, stays local, independent and build-gated; upstream settings retain their indexes");
}
