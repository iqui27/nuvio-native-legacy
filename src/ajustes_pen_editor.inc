/* Pencil choice page; edits use the existing draft and persistence path. */
static void ajPenEditor(void) {
  if(uxEditor!=1 || ajIptvMenu(uxOp) || uxOp==AJ_GUIA_CATEGORIAS || ajPaleta(uxOp) || uxRestaurar || uxAvisoRisco) {
    aj4Editor();return;
  }
  int op=uxOp;
  ajPenBase();
  ajPenChip(ajPenPage==27 || ajPenPage==28?"Áudio e legendas":"Ajustes","pl_chevron-left",200,112,208,0,5,0);
  ajKicker(ajPenPage==27 || ajPenPage==28?"ÁUDIO E LEGENDAS":"AJUSTES",200,198,1);
  const char *title=op==AJ_AUD_LINGUA?"Idioma do áudio":op==AJ_LEG_LINGUA2?"Segunda legenda":aj4Rotulo(op);
  aj4Texto(title,TXT_TITULO2,200,212,1500,1);
  aj4Texto(op==AJ_AUD_LINGUA?"Escolha a faixa que você prefere. A original continua na lista — nada é escondido.":aj4Ajuda(op),TXT_G20M,200,291,1520,.58f);
  ajPenChip(escopoDe(op)==ESC_TV?"Só esta TV":"Este perfil",NULL,1600,118,174,0,9,0);
  ajPenPanel(200,344,520,640,0);ajPenPanel(744,344,1080,640,0);
  ajKicker("PREFERÊNCIA",226,366,1);
  char original[160];uxValorTexto(op,uxOriginal,original,sizeof original);
  const char *heads[]={"Valor atual","Faixa de reserva","Onde isso vale"};
  const char *body[]={original,op==AJ_AUD_LINGUA?"Se a escolhida não existir no título, tocamos a original.":aj4Ajuda(op),escopoDe(op)==ESC_TV?"Nesta TV e no app da sala. O celular segue a conta, não a TV.":"A escolha acompanha este perfil."};
  for(int i=0;i<3;i++) {
    float y=i==0?400:i==1?584:728,h=i==0?170:i==1?130:140;
    ajNeutro((GfxRect){226,y,468,h},22,.05f,.10f,.11f,.13f,1);
    aj4Texto(heads[i],TXT_G21B,248,y+20,422,1);
    ajBloco(TXT_G18R,body[i],248,y+54,422,22,.58f,3);
  }
  ajPenChip("Voltar ao padrão","aj_rotate-ccw-clock",226,902,206,uxRodape,9,1);
  ajPenChip("Salvar como preset","aj_check",444,902,230,0,9,2);
  ajKicker(op==AJ_AUD_LINGUA?"FAIXAS DISPONÍVEIS":"ESCOLHA UM VALOR",770,366,1);
  if(OPCOES[op].tipo==OP_NUMERO) {
    char value[160];uxValorTexto(op,uxPendente,value,sizeof value);
    aj4Texto(value,TXT_TITULO2,800,480,900,1);
    aj4Texto("← →  Alterar     OK  Aplicar",TXT_G20M,800,580,900,.65f);
  } else {
    int n=nValores(op),start=uxPendente/6*6;
    for(int i=start;i<n && i<start+6;i++) {
      float y=400+(i-start)*76;char value[160];uxValorTexto(op,i,value,sizeof value);
      if(i==uxPendente && !uxRodape)ajNeutro((GfxRect){770,y,1028,70},20,.14f,.169f,.176f,.204f,1);
      aj4Texto(value,TXT_G21B,770,y+5,800,1);
      aj4Texto(i==uxOriginal?"Valor atual":"OK para aplicar",TXT_AJ_MINI14,770,y+40,800,.4f);
      if(i==uxOriginal) { float r,g,b;ajustes_acento(&r,&g,&b);gfx_cor((GfxRect){1766,y+30,12,12},.5f,r,g,b,1); }
      aj4Alvo((GfxRect){770,y,1028,70},4,i);
    }
  }
  aj4Texto(op==AJ_AUD_LINGUA?"O Nuvio não corta faixas: a original é sempre a primeira opção da lista.":"OK aplica. Voltar mantém o valor atual.",TXT_AJ_MINI14,770,944,1028,.4f);
  ajPenRodape(ajPenPage==27 || ajPenPage==28?"Áudio e legendas":"Ajustes",3,3);
}
