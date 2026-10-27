static void ajPenPresets(void) {
  ajPenSavedLoad();
  ajPenBase();ajPenChip("Ajustes","pl_chevron-left",200,112,124,0,5,0);
  ajKicker("AJUSTES",200,198,1);aj4Texto("Presets",TXT_TITULO2,200,212,1500,1);
  aj4Texto("Combos de ajustes prontos. Salve o seu e use quando quiser.",TXT_G20M,200,291,1520,.58f);
  ajPenPanel(200,344,1040,640,0);ajPenPanel(1264,344,560,640,0);
  ajKicker("PRESETS PRONTOS",226,366,1);ajKicker("SEUS PRESETS",1290,366,1);
  static const char *const desc[]={"Qualidade automática, áudio estéreo e legendas grandes","Sincronia ligada e volume normalizado","Sem spoiler, com o elenco e o trailer no destaque","Rádio e música: sem cartaz e sem trailer"};
  static const char *const icons[]={"aj_sparkles","aj_volume-2","aj_clapperboard","aj_radio"};
  for(int i=0;i<4;i++) {
    float y=400+i*84;
    if(aj4Linha==i && aj4Topo<0)ajNeutro((GfxRect){226,y,988,74},20,.14f,.169f,.176f,.204f,1);
    ajIcone(icons[i],246,y+24,26,.8f);aj4Texto(AJ_PEN_PRESET_NAMES[i],TXT_G21B,288,y+13,750,1);
    aj4Texto(desc[i],TXT_AJ_MINI14,288,y+42,800,.4f);
    aj4Texto("Usar",TXT_G18R,1112,y+24,70,.58f);ajIcone("aj_chevron-right",1180,y+28,18,.45f);
    aj4Alvo((GfxRect){226,y,988,74},1,i);
  }
  int start=aj4Linha>=4 && aj4Linha<4+ajPenSavedCount?(aj4Linha-4)/2*2:0;
  for(int i=start;i<ajPenSavedCount && i<start+2;i++) {
    float y=400+(i-start)*84;
    if(aj4Linha==4+i && aj4Topo<0)ajPenPanel(1290,y,508,74,1);
    ajIcone("aj_bookmark",1310,y+24,26,.8f);aj4Texto(ajPenSavedNames[i],TXT_G21B,1352,y+13,330,1);
    char language[96],size[96],summary[240];
    int audio=ajPenSavedValues[i][AJ_AUD_LINGUA],scale=ajPenSavedValues[i][AJ_LEG1_TAMANHO];
    uxValorTexto(AJ_AUD_LINGUA,audio>=0?audio:uxValorAtual(AJ_AUD_LINGUA),language,sizeof language);
    uxValorTexto(AJ_LEG1_TAMANHO,scale>=0?scale:uxValorAtual(AJ_LEG1_TAMANHO),size,sizeof size);
    snprintf(summary,sizeof summary,i18n("Áudio: %s · Legenda: %s"),language,size);
    aj4Texto(summary,TXT_AJ_MINI14,1352,y+42,330,.4f);
    aj4Texto("Usar",TXT_G18R,1696,y+24,70,.58f);ajIcone("aj_chevron-right",1768,y+28,18,.45f);
    aj4Alvo((GfxRect){1290,y,508,74},1,4+i);
  }
  ajNeutro((GfxRect){1290,594,508,180},22,.05f,.10f,.11f,.13f,1);
  aj4Texto("Salvar o estado atual",TXT_G21B,1312,618,460,1);
  ajBloco(TXT_G18R,"A qualidade, o idioma e o estilo de hoje viram um preset novo.",1312,650,460,24,.58f,2);
  ajPenChip("Salvar preset","aj_plus",1312,704,177,aj4Linha==4+ajPenSavedCount,1,4+ajPenSavedCount);
  aj4Texto("O preset vale nesta TV e vai para a conta.",TXT_AJ_MINI14,1290,930,508,.4f);
  ajPenRodape("Presets",3,3);
}
