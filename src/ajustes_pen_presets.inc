static void ajPenPresets(void) {
  ajPenBase();ajPenChip("Ajustes","pl_chevron-left",200,112,124,0,5,0);
  ajKicker("AJUSTES",200,198,1);aj4Texto("Presets",TXT_TITULO2,200,212,1500,1);
  aj4Texto("Combos de ajustes prontos. Salve o seu e use quando quiser.",TXT_G20M,200,291,1520,.58f);
  ajPenPanel(200,344,1040,640,0);ajPenPanel(1264,344,560,640,0);
  ajKicker("PRESETS PRONTOS",226,366,1);ajKicker("SEUS PRESETS",1290,366,1);
  static const char *const desc[]={"Priorizar até 4K e melhor qualidade de imagem.","Priorizar 720p, limitar tamanho e desativar trailers automáticos.","Imagens leves, menos movimento e painéis sólidos."};
  for(int i=0;i<3;i++) {
    float y=400+i*84;
    if(aj4Linha==i && aj4Topo<0)ajNeutro((GfxRect){226,y,988,74},20,.14f,.169f,.176f,.204f,1);
    ajIcone("aj_sparkles",246,y+24,26,.8f);aj4Texto(AJ4_PRESET_NOMES[i],TXT_G21B,288,y+13,750,1);
    aj4Texto(desc[i],TXT_AJ_MINI14,288,y+42,800,.4f);
    aj4Texto("Usar",TXT_G18R,1112,y+24,70,.58f);ajIcone("aj_chevron-right",1180,y+28,18,.45f);
    aj4Alvo((GfxRect){226,y,988,74},1,i);
  }
  ajNeutro((GfxRect){1290,594,508,180},22,.05f,.10f,.11f,.13f,1);
  aj4Texto("Salvar o estado atual",TXT_G21B,1312,618,460,1);
  ajBloco(TXT_G18R,"Salvar como preset ainda não está disponível.",1312,650,460,24,.58f,3);
  aj4Texto("Cada prévia mostra o perfil ou a TV afetada.",TXT_AJ_MINI14,1290,930,508,.4f);
  ajPenRodape("Presets",3,3);
}
