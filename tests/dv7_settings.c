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
  assert(!ajustes_dv7_mapping());
  assert(AJ_DIAGNOSTICS_QR==AJ_SHOW_SKIP_CREDITS+1);
  /* Integrated E prefix stays exact; B group follows it, adjacent and in bounds. */
  assert(AJ_SOURCE_BADGE_POSITION==AJ_DIAGNOSTICS_QR+1);
  assert(AJ_DV7_MAPPING>AJ_SOURCE_BADGE_POSITION && AJ_DV7_MAPPING==AJ_DV_AUDIO-1 && valorPadrao[AJ_DV7_MAPPING]==1 &&
         !strcmp(CHAVE[AJ_DV7_MAPPING],"dv7PreserveMappingLocal"));
  assert(OPCOES[AJ_DV7_MAPPING].tipo==OP_ESCOLHA);
  assert(somenteDesteAparelho(AJ_DV7_MAPPING) && !dePerfil(AJ_DV7_MAPPING));
  assert(!ajustePortavel(AJ_DV7_MAPPING) && !ajusteAparelho(AJ_DV7_MAPPING));
  valor[AJ_DV7_MAPPING]=0; assert(ajustes_dv7_mapping());
  assert(!ajustes_aplicar_blob("{\"c_settings_v1\":{\"values\":{\"c_dv7PreserveMappingLocal\":1}}}"));
  valor[AJ_DV7_MAPPING]=1;
  int m=0;
  for(int i=0;i<AJ_N_TELA;i++) if(TELA[i].tipo==IT_OPC && TELA[i].op==AJ_DV7_MAPPING)m++;
#ifdef NV_DV7_CONVERSION
  assert(m==1);
#else
  assert(m==0);
#endif
  assert(AJ_DV_AUDIO==AJ_MP4_BD_TEST-1 && !ajustes_dv_audio() && valorPadrao[AJ_DV_AUDIO]==0);
  assert(!strcmp(CHAVE[AJ_DV_AUDIO],"dvAudioFixoLocal"));
  assert(somenteDesteAparelho(AJ_DV_AUDIO) && !dePerfil(AJ_DV_AUDIO));
  assert(!ajustePortavel(AJ_DV_AUDIO) && !ajusteAparelho(AJ_DV_AUDIO));
  for(int i=0;i<3;i++) { valor[AJ_DV_AUDIO]=i; assert(ajustes_dv_audio()==i); }
#ifndef NV_TPK
  for(int i=0;i<4;i++) {
    valor[AJ_DTS_AC3]=i; assert(ajustes_dts_desligado()==(i==3));
    assert(ajustes_dts_original()==(i==0) && ajustes_dts_ac3()==(i==2));
  }
#endif
  assert(AJ_DV7_MAPPING+2==AJ_MP4_BD_TEST && AJ_MP4_BD_TEST<AJ_N && valorPadrao[AJ_MP4_BD_TEST]==1);
  assert(!ajustes_mp4_bd_test() && !strcmp(CHAVE[AJ_MP4_BD_TEST],"dvMp4BdTestLocal"));
  assert(somenteDesteAparelho(AJ_MP4_BD_TEST) && !dePerfil(AJ_MP4_BD_TEST));
  assert(!ajustePortavel(AJ_MP4_BD_TEST) && !ajusteAparelho(AJ_MP4_BD_TEST));
  valor[AJ_MP4_BD_TEST]=0;
#if defined(NV_MP4_BD_TEST) && defined(NV_WEBOS) && defined(NV_DV7_CONVERSION)
  assert(ajustes_mp4_bd_test() && !foraDestaPlataforma(AJ_MP4_BD_TEST));
#else
  assert(!ajustes_mp4_bd_test() && foraDestaPlataforma(AJ_MP4_BD_TEST));
#endif
  assert(!ajustes_aplicar_blob("{\"c_settings_v1\":{\"values\":{\"c_dvMp4BdTestLocal\":1}}}"));
  assert(valor[AJ_MP4_BD_TEST]==0);
  puts("PASS: DV preferences local, opt-in and append-only; DTS Off preserves stored modes");
}
