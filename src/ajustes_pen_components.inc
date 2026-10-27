/* Native component reference, reachable from About and through the capture harness. */
static void ajPenComponents(void) {
  ajPenBase();ajKicker("BIBLIOTECA",200,198,1);aj4Texto("Componentes",TXT_TITULO2,200,212,1500,1);
  aj4Texto("Cada peça do Ajustes, com o estado normal e o estado de foco (D-pad, sem hover).",TXT_G20M,200,291,1520,.58f);
  ajPenPanel(200,344,520,290,0);ajPenPanel(744,344,520,290,0);ajPenPanel(1288,344,536,290,0);
  ajKicker("LINHA DE AJUSTE",218,366,1);ajKicker("INTERRUPTOR",762,366,1);ajKicker("LISTA DE ESCOLHA",1300,366,1);
  for(int i=0;i<2;i++) {
    float y=400+i*84;if(i)ajPenPanel(218,y,484,74,1);
    ajIcone("aj_volume-2",238,y+24,26,.8f);aj4Texto("Idioma do áudio",TXT_G21B,280,y+5,250,1);
    aj4Texto("A faixa que você prefere",TXT_AJ_MINI14,280,y+35,260,.4f);aj4Texto("Português (Brasil)",TXT_G18R,540,y+20,150,.58f);
  }
  ajBloco(TXT_AJ_MINI14,"Sem foco: só a superfície. Com foco: mais clara, escala 1,015 e sombra.",218,579,484,20,.4f,2);
  static const char *const toggles[]={"Legendas","Sincronia","Painel"};
  for(int i=0;i<3;i++) {
    float y=400+i*82;ajIcone(i?"aj_audio-lines":"aj_captions",782,y+20,26,.8f);aj4Texto(toggles[i],TXT_G21B,824,y+16,250,1);
    float r,g,b;ajustes_acento(&r,&g,&b);int on=i!=2;
    gfx_cor((GfxRect){1150,y+12,64,36},.5f,on?r:.2f,on?g:.21f,on?b:.24f,1);
    gfx_cor((GfxRect){1154+(on?28:0),y+16,28,28},.5f,1,1,1,1);
  }
  static const char *const choice[]={"Original","Português (Brasil)","English"};
  static const char *const help[]={"A faixa do arquivo","Dublado","Original em inglês"};
  for(int i=0;i<3;i++) {
    float y=400+i*74;if(i==1)ajPenPanel(1300,y,500,68,1);
    aj4Texto(choice[i],TXT_G21B,1300,y+4,480,1);aj4Texto(help[i],TXT_AJ_MINI14,1300,y+30,480,.4f);
  }
  ajPenPanel(200,648,520,336,0);ajPenPanel(744,648,520,336,0);ajPenPanel(1288,648,536,336,0);
  ajKicker("CHIPS, BOTÕES E PASSO",218,666,1);ajKicker("CARTÃO COM AGORA",762,666,1);ajKicker("ESCALA DE TEXTO",1300,666,1);
  ajPenChip("Buscar ajuste","aj_search",218,694,190,ajPenRow==0,12,0);
  ajPenChip("Presets","aj_sparkles",420,694,134,ajPenRow==1,12,1);
  ajPenChip("Só esta TV",NULL,566,694,138,0,9,0);
  ajPenChip("Salvar como preset","aj_check",218,764,270,ajPenRow==2,12,2);
  ajPenChip("Abrir","aj_play",218,834,160,ajPenRow==3,12,3);
  ajPenChip("−",NULL,218,896,44,0,11,-1);aj4Texto("100%",TXT_G21B,280,908,90,1);ajPenChip("+",NULL,380,896,44,0,11,1);
  ajPenPanel(770,724,468,230,1);ajIcone("aj_play",792,748,28,1);aj4Texto("Reproduzir",TXT_G28B,792,795,420,1);
  ajBloco(TXT_G18R,"Qualidade, vídeo escolhido e o que toca a seguir.",792,832,420,22,.58f,2);ajCaps(TXT_AJ_MINI12,"AGORA",1.5f,792,893,.4f);aj4Texto("Automática",TXT_G18M,862,889,300,1);
  static const char *const labels[]={"Título da página","Título de cartão","Linha de ajuste","Texto de apoio","Kicker"};
  static const char *const examples[]={"Ajustes","Reproduzir","Idioma do áudio","A faixa que você prefere","DO DIA A DIA"};
  static const TxtEstilo styles[]={TXT_TITULO2,TXT_G28B,TXT_G21B,TXT_AJ_MINI14,TXT_AJ_MINI12};
  for(int i=0;i<5;i++) { float y=694+i*51;aj4Texto(labels[i],TXT_AJ_MINI14,1300,y,160,.4f);aj4Texto(examples[i],styles[i],1460,y,340,1); }
  aj4Texto("Inter Display. A escala de 85% mantém esta hierarquia.",TXT_AJ_MINI14,1300,944,500,.4f);
  ajPenRodape("Componentes",1,1);
}
