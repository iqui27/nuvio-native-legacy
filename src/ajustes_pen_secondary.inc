static void ajPenSecondary(void) {
  ajPenBase();ajPenChip("Áudio e legendas","pl_chevron-left",200,112,208,0,5,0);
  ajKicker("ÁUDIO E LEGENDAS",200,198,1);aj4Texto("Segunda legenda",TXT_TITULO2,200,212,1500,1);
  aj4Texto("Uma tradução por cima da principal, para quando a original não basta.",TXT_G20M,200,291,1520,.58f);
  ajPenChip("Só esta TV",NULL,1650,118,174,0,9,0);
  aj4Texto("Não empurra a legenda principal",TXT_AJ_MINI14,1560,178,264,.4f);
  ajPenPanel(200,344,520,640,0);ajPenPanel(744,344,1080,640,0);
  ajKicker("SEGUNDA LEGENDA",226,366,1);ajKicker("ESTILO DA SEGUNDA",770,366,1);
  static const char *const heads[]={"Idioma da segunda legenda","Posição","Estilo separado"};
  static const char *const body[]={"O idioma da tradução extra. Desligada, ela não aparece.","No topo da tela ou junto da principal.","A segunda legenda não muda quando você muda a principal."};
  for(int i=0;i<3;i++) {
    float y=i==0?400:i==1?584:738;
    ajNeutro((GfxRect){226,y,468,i?140:170},22,.05f,.10f,.11f,.13f,1);
    aj4Texto(heads[i],TXT_G21B,248,y+20,422,1);ajBloco(TXT_G18R,body[i],248,y+54,422,22,.58f,3);
    if(i<2) {
      int op=i?AJ_LEG2_POS:AJ_LEG_LINGUA2;
      char text[160];snprintf(text,sizeof text,"%s",textoValor(op));aj4Texto(text,TXT_G18M,306,y+104,364,1);
    }
  }
  static const int ops[]={AJ_LEG2_FONTE,AJ_LEG2_TAMANHO,AJ_LEG2_COR,AJ_LEG2_FUNDO,AJ_LEG2_BORDA,AJ_LEG_LINGUA2};
  static const char *const labels[]={"Igual à principal","Tamanho","Cor","Fundo","Borda","Idioma da segunda legenda"};
  static const char *const help[]={"Usa o mesmo estilo da principal","De 50% a 300% do tamanho principal","Branco, amarelo, verde, azul, vermelho","Escuro de 25% a 100%","Nenhuma, contorno ou sombra","O idioma da tradução extra"};
  for(int i=0;i<6;i++) {
    float y=400+i*76;
    if(ajPenRow==i)ajPenPanel(770,y,1028,70,1);
    aj4Texto(labels[i],TXT_G21B,770,y+5,800,1);aj4Texto(help[i],TXT_AJ_MINI14,770,y+40,800,.4f);
    char text[160];snprintf(text,sizeof text,"%s",textoValor(ops[i]));TxtLinha value=ajTxtC(TXT_G18R,text,240);txt_desenhar_alpha(value,1798-value.w,y+26,.58f);
    aj4Alvo((GfxRect){770,y,1028,70},8,i);
  }
  aj4Texto("A segunda legenda aparece por cima, sem empurrar a principal.",TXT_AJ_MINI14,770,944,1028,.4f);
  ajPenRodape("Áudio e legendas",3,3);
}
