/* Existing live source list; the app owns verification, private links and playback. */
static void ajPenSources(void) {
  ajPenBase();ajPenChip("Reprodução","pl_chevron-left",200,112,164,0,5,0);
  ajKicker("REPRODUÇÃO",200,198,1);aj4Texto("Escolha do vídeo",TXT_TITULO2,200,212,1500,1);
  aj4Texto("As fontes deste título. O Nuvio pergunta antes de trocar de uma para a outra.",TXT_G20M,200,291,1520,.58f);
  ajPenScope();
  aj4Texto("O que está em Privado só você vê",TXT_AJ_MINI14,1450,178,374,.4f);
  ajPenPanel(200,344,520,640,0);ajPenPanel(744,344,1080,640,0);
  ajKicker("COMO ESCOLHE",226,366,1);ajKicker("FONTES DISPONÍVEIS",770,366,1);
  static const char *const heads[]={"Escolher na mão","Ordem das fontes","Antes de abrir"};
  static const char *const body[]={"A TV mostra as fontes e você escolhe com o D-pad.","Por qualidade ou na ordem em que o addon montou.","O Nuvio confere a fonte e avisa antes de trocar."};
  for(int i=0;i<3;i++) {
    float y=i==0?400:i==1?584:738;
    ajNeutro((GfxRect){226,y,468,i?140:170},22,ajPenRow==-1-i?.14f:.05f,.10f,.11f,.13f,1);
    aj4Texto(heads[i],TXT_G21B,248,y+20,422,1);ajBloco(TXT_G18R,body[i],248,y+54,422,22,.58f,3);
    if(i==0) { ajPenToggle(248,512,lig(AJ_FONTE_MANUAL));aj4Texto(lig(AJ_FONTE_MANUAL)?"Ligado":"Desligado",TXT_G18R,326,519,300,.58f);aj4Alvo((GfxRect){248,512,400,36},15,0); }
    if(i==1)aj4Alvo((GfxRect){226,y,468,140},15,1);
  }
  int n=stream_n(),start=ajPenRow/6*6;
  for(int i=start;i<n && i<start+6;i++) {
    const Stream *source=stream_item(i);if(!source)continue;
    float y=400+(i-start)*76;
    if(i==ajPenRow)ajPenPanel(770,y,1028,70,1);
    int privateSource=source->debridServico>0 && source->debridServico<=DEBRID_N;
    float titleWidth=privateSource?760:source->altura>=2160?820:920;
    aj4Texto(source->rotulo,TXT_G21B,770,y+5,titleWidth,1);
    aj4Texto(privateSource?"Debrid":"Addon",TXT_AJ_MINI14,770,y+40,840,.4f);
    if(i==stream_atual()) { float r,g,b;ajustes_acento(&r,&g,&b);gfx_cor((GfxRect){1766,y+30,12,12},.5f,r,g,b,1); }
    float badgeX=770+ajTxtC(TXT_G21B,source->rotulo,titleWidth).w+16;
    if(privateSource) { ajNeutro((GfxRect){badgeX,y+5,95,24},6,.07f,.14f,.15f,.17f,1);aj4Texto("PRIVADO",TXT_AJ_MINI12,badgeX+10,y+11,80,.8f);badgeX+=107; }
    if(source->altura>=2160) { ajNeutro((GfxRect){badgeX,y+5,40,24},6,.07f,.24f,.09f,.16f,1);aj4Texto("4K",TXT_AJ_MINI12,badgeX+9,y+11,25,.9f); }
    char quality[40];if(source->altura>0)snprintf(quality,sizeof quality,"%dp",source->altura);else quality[0]=0;
    TxtLinha value=ajTxt(TXT_G18R,quality);txt_desenhar_alpha(value,(i==stream_atual()?1746:1798)-value.w,y+26,.58f);
    aj4Alvo((GfxRect){770,y,1028,70},8,i);
  }
  if(!n)aj4Texto("Abra um título para ver as fontes disponíveis.",TXT_G21B,770,430,1028,.58f);
  ajPenChip("Voltar ao padrão","aj_rotate-ccw-clock",226,902,206,ajPenRow==-3,15,2);
  ajPenChip("Salvar como preset","aj_check",444,902,230,ajPenRow==-4,9,2);
  aj4Texto("Cada linha é uma fonte. O Nuvio nunca troca sozinho quando você escolheu na mão.",TXT_AJ_MINI14,770,944,1028,.4f);
  ajPenRodape("Reprodução",2,5);
}
