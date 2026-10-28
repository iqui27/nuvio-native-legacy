/* Pencil Settings, 1920 x 1080. Values and actions remain owned by ajustes.c. */
#include "menu.h"
#include "ajustes_pen_cabecalhos.inc"
static void ajPenPanel(float x,float y,float w,float h,int focus) {
  GfxRect r={x,y,w,h};float radius=w==88?44:28;
  if(focus)gfx_rect((GfxRect){x-10,y+2+ajPenDy,w+20,h+26},0,GFX_SOMBRA,1,0,0,.5f,0,0,0,.30f);
  if(ajVid()) gfx_vidro_miolo(r,radius/h,.055f,.059f,.071f,.80f,1);
  else gfx_cor(r,radius/h,.045f,.047f,.058f,.96f);
  if(focus)ajNeutro(r,radius,.14f,.125f,.133f,.153f,1);
}
static void ajPenChip(const char *s,const char *ic,float x,float y,float w,int focus,int zone,int row) {
  GfxRect r={x,y,w,52};
  if(focus)ajPilulaAcento(r,26,1);
  else ajNeutro(r,26,.07f,.14f,.15f,.17f,1);
  if(ic) { if(focus)ajIconeT(ic,x+20,y+17,18,1);else ajIcone(ic,x+20,y+17,18,.8f); }
  TxtEstilo style=txt_largura(TXT_G18M,i18n(s))>w-(ic?58:32)?TXT_AJ_MINI14:TXT_G18M;
  TxtLinha t=focus?txt_linha_corta(style,s,ajTinta(),ajTinta(),ajTinta(),255,w-(ic?58:32)):ajTxtC(style,s,w-(ic?58:32));
  txt_desenhar(t,x+(ic?48:(w-t.w)/2),y+(52-t.h)/2);
  aj4Alvo(r,zone,row);
}
static void ajPenToggle(float x,float y,int on) {
  float r,g,b;ajustes_acento(&r,&g,&b);
  gfx_cor((GfxRect){x,y,64,36},.5f,on?r:.20f,on?g:.21f,on?b:.24f,1);
  gfx_cor((GfxRect){x+4+(on?28:0),y+4,28,28},.5f,1,1,1,1);
}
static void ajPenScope(void) {
  ajPenChip("Só esta TV",NULL,1600,118,146,0,9,0);
  float r,g,b;ajustes_acento(&r,&g,&b);gfx_cor((GfxRect){1618,139,6,6},.5f,r,g,b,1);
}
static void ajPenRailFoco(int row,int unused) { (void)unused;menu_abrir();(void)row; }
static void ajPenBase(void) {
  if(ajPenBody)return;
  GfxRect screen={0,0,1920,1080};
  gfx_cor(screen,0,.0424f,.0531f,.0638f,1);
  gfx_luz_canto(screen,0,1081,252,693,.5326f,.5058f,.5587f,.9163f);
  gfx_luz_canto(screen,0,1470,502,1767,.1883f,.1818f,.3155f,1);
  gfx_luz_canto(screen,0,754,491,1185,.0294f,.0046f,0,.4652f);
  gfx_veu_css(screen,2,1.0f,.4659f,.65f);
  ajPenPanel(48,112,88,522,0);
  static const char *const icons[]={"aj_house","aj_compass","aj_tv","aj_search","aj_book-open","aj_calendar","aj_user-round","aj_sliders-horizontal"};
  for(int i=0;i<8;i++) {
    ajIcone(icons[i],78,142+i*64,28,i==7?1:.6f);
    GfxRect r=aj4Rect((GfxRect){62,126+i*64,60,60});
    ponteiro_alvo(r.x,r.y,r.w,r.h,ajPenRailFoco,ajPenRailFoco,i,0);
  }
  float r,g,b;ajustes_acento(&r,&g,&b);
  gfx_cor((GfxRect){89,622,6,6},.5f,r,g,b,1);
}
static void ajPenModo(float y) {
  ajNeutro((GfxRect){1560,y,264,56},28,.06f,.14f,.15f,.17f,1);
  int av=lig(AJ_AVANCADAS);
  ajNeutro((GfxRect){av?1686:1565,y+5,av?133:117,46},23,.14f,.29f,.30f,.34f,1);
  if(aj4Topo==1)ajPilulaAcento((GfxRect){av?1686:1565,y+5,av?133:117,46},23,1);
  aj4Texto("Simples",TXT_G20B,1585,y+17,110,av?.55f:1);
  aj4Texto("Avançado",TXT_G20B,1706,y+17,116,av?1:.55f);
  aj4Alvo((GfxRect){1560,y,264,56},0,1);
}
static void ajPenRodape(const char *page,int part,int total) {
  ajNeutro((GfxRect){200,1024,36,26},7,.07f,.14f,.15f,.17f,1);
  aj4Texto("OK",TXT_G14B,208,1029,24,.8f);
  aj4Texto(uxEditor?"escolher":aj4Util==2?"usar":ajPenPage==AJ_PEN_RULES?"voltar":ajPenPage==AJ_PEN_TOUR?"ligar":ajPenPage==AJ_PEN_COMPONENTS?"experimentar":ajPenPage==AJ_PEN_SECONDARY || ajPenPage==AJ_PEN_SOURCES?"escolher":"abrir",TXT_AJ_MINI14,246,1029,70,.45f);
  int choosing=uxEditor || ajPenPage==AJ_PEN_SECONDARY || ajPenPage==AJ_PEN_SOURCES;
  float backX=ajPenPage==AJ_PEN_COMPONENTS?350:choosing?317:289;
  ajNeutro((GfxRect){backX,1024,29,26},7,.07f,.14f,.15f,.17f,1);
  ajIcone("aj_arrow-left",backX+8,1031,14,.8f);
  const char *back=ajPenPage==AJ_PEN_IPTV_PROVIDER?"TV ao vivo e guia":ajPenPage>=AJ_PEN_PROVIDER && ajPenPage<=AJ_PEN_ADDON_FAILURES?"Perfil e serviços":ajPenPage==AJ_PEN_SOURCES?"Reprodução":uxEditor?(ajPenPage==27 || ajPenPage==28 || ajPenPage==AJ_PEN_SECONDARY?"Áudio e legendas":uxOp==AJ_TEMA || uxOp==AJ_ESCALA_TEXTO?"Minha tela":ajPenContext()):ajPenPage==28 || ajPenPage==AJ_PEN_SECONDARY?"Áudio e legendas":ajPenPage>=29 && ajPenPage<46?AJ_PEN_BACK[ajPenPage-29]:"Ajustes";
  char backText[160];snprintf(backText,sizeof backText,i18n("voltar para %s"),i18n(back));
  aj4Texto(focoIndice || ajPenPage==AJ_PEN_COMPONENTS?"voltar":backText,TXT_AJ_MINI14,backX+39,1029,400,.45f);
  char text[200];
  if(uxEditor || aj4Util || focoIndice || ajPenPage<27 || ajPenPage>=AJ_PEN_PARENTAL)snprintf(text,sizeof text,"%s",i18n(page));
  else snprintf(text,sizeof text,i18n("%s · %d de %d"),i18n(page),part,total);
  TxtLinha t=ajTxt(TXT_AJ_MINI14,text);txt_desenhar_alpha(t,1824-t.w,1029,.45f);
  TxtLinha nav=ajTxt(TXT_AJ_MINI14,"navegar");float navX=1824-t.w-22-nav.w;
  txt_desenhar_alpha(nav,navX,1029,.45f);
  for(int i=0;i<2;i++) {
    float x=navX-110+i*44;
    ajNeutro((GfxRect){x,1024,28,26},7,.07f,.14f,.15f,.17f,1);
    ajIcone(i?"aj_arrow-down":"aj_arrow-up",x+6,1030,16,.65f);
  }
}
static void ajPenRows(void) {
  const AjPenPage *page=ajPenCurrent();
  if(!page)return;
  ajPenBase();
  const char *back=ajPenPage==28 || ajPenPage==AJ_PEN_SECONDARY?"Áudio e legendas":ajPenPage>=29 && ajPenPage<46?AJ_PEN_BACK[ajPenPage-29]:"Ajustes";
  ajPenChip(back,"pl_chevron-left",200,112,!strcmp(back,"Ajustes")?124:!strcmp(back,"Minha tela")?150:!strcmp(back,"Reprodução")?164:208,0,5,0);
  ajKicker(ajPenPage<27?"AJUSTES AVANÇADOS":!strcmp(back,"Ajustes")?"AJUSTES":back,200,174,1);
  aj4Texto(page->title,TXT_TITULO2,200,212,1500,1);
  if(ajPenPage<27 || (ajPenPage>=46 && ajPenPage<AJ_PEN_PARENTAL)) {
    char sub[240];snprintf(sub,sizeof sub,i18n("Opções de %s que o modo Simples esconde. Valor atual à direita."),i18n(page->section));
    aj4Texto(sub,TXT_G20M,200,291,1520,.58f);
  } else {
    ajPenModo(118);
    const char *help=ajPenPage==27?"Avançado mostra saída de som e sincronia fina":ajPenPage==28?"Avançado mostra o deslize da sincronia":ajPenPage==AJ_PEN_PARENTAL?"Avançado mostra os limites por perfil":ajPenPage==AJ_PEN_SECONDARY?"Segunda legenda":AJ_PEN_MODE_HELP[ajPenPage-29];
    TxtLinha modeHelp=ajTxt(TXT_AJ_MINI14,help);txt_desenhar_alpha(modeHelp,1824-modeHelp.w,190,.4f);
    aj4Texto(ajPenPage==AJ_PEN_PARENTAL?"Perfil infantil, PIN e os limites de cada perfil.":ajPenPage>=29 && ajPenPage<46?AJ_PEN_SUB[ajPenPage-29]:ajPenPage==27?"Vale para quase todo título. Se um filme não tiver a faixa escolhida, tocamos a original e avisamos.":ajPenPage==AJ_PEN_SECONDARY?"A segunda legenda não muda quando você muda a principal.":"Tamanho, fonte, cor e borda da legenda principal.",TXT_G20M,200,291,1520,.58f);
  }
  int index=0;
  for(int col=0;col<2;col++) {
    const AjPenColumn *column=&page->cols[col];if(!column->n)continue;
    ajPenPanel(column->x,344,column->w,640,0);
    ajKicker(column->title,column->x+26,366,1);
    for(int i=0;i<column->n;i++,index++) {
      const AjPenRow *row=&column->rows[i];int compact=column->n>7;
      float x=column->x+26,y=400+i*(compact?62:ajPenPage<27 || ajPenPage>=46?82:84),w=column->w-52;
      int focus=index==ajPenRow && aj4Topo<0;
      int transform=focus && !gfx_transformado();
      if(transform)gfx_transformar(x+w*.5f,y+37+ajPenDy,1.015f,0,0);
      if(focus) {
        if(ajPenFocusPage!=ajPenPage || ajustes_animacoes_reduzidas()) { ajPenFocusY=y;ajPenFocusV=0;ajPenFocusPage=ajPenPage; }
        ajPenFocusY=anim_mola2(&ajPenFocusV,ajPenFocusY,y,ajPenDt,24);
        gfx_rect((GfxRect){x-10,ajPenFocusY+2+ajPenDy,w+20,100},0,GFX_SOMBRA,1,0,0,.5f,0,0,0,.30f);
        ajNeutro((GfxRect){x,ajPenFocusY,w,compact?60:74},20,.14f,.169f,.176f,.204f,1);
      }
      ajIcone(row->icon,x+20,y+24,26,focus?1:.75f);
      int available=row->op>=0?ajOpcaoDisponivel(row->op):row->op!=-1;
      int hasHelp=row->help && row->help[0];
      aj4Texto(row->title,compact?TXT_G18M:TXT_G21B,x+62,y+(hasHelp?13:24),compact?w-170:w-365,available?1:.45f);
      aj4Texto(row->help,TXT_AJ_MINI14,x+62,y+(compact?35:42),w-210,.4f);
      if(row->toggle && row->op!=-1) {
        int on=row->op<=-10 && row->op>=-13?perfil_regra(ajPenParentProfile(),-10-row->op):row->op==-2?perfil_ajustes_travados(ajPenParentProfile()):uxValorAtual(row->op)==0;
        if(ajPenPage==AJ_PEN_PARENTAL && row->op==AJ_ENQUETES)on=!on;float r=.20f,g=.21f,b=.24f;
        if(on)ajustes_acento(&r,&g,&b);
        gfx_cor((GfxRect){x+w-90,y+19,64,36},.5f,r,g,b,1);
        gfx_cor((GfxRect){x+w-86+(on?28:0),y+23,28,28},.5f,1,1,1,1);
      } else {
        char val[160];snprintf(val,sizeof val,"%s",row->op>=0?textoValor(row->op):row->op==-3?i18n(admin_perfil_pin_definido(perfis_ativo())?"PIN definido":"Definir um PIN"):row->op==-6?pfNome(ajPenParentProfile()):row->op==-4?i18n("Livre e até 10 anos"):i18n("Abrir"));
        if((row->op==AJ_AUD_LINGUA || row->op==AJ_LEG_LINGUA) && !strcmp(ling_opcao_codigo(uxValorAtual(row->op)),"pt"))snprintf(val,sizeof val,"%s",i18n("Português (Brasil)"));
        if(row->op>=0 && OPCOES[row->op].tipo==OP_ACAO && !val[0])snprintf(val,sizeof val,"%s",i18n("Abrir"));
        if(ajPenPage==39 && i==5 && col==1)val[0]=0;
        if(ajPenPage==40 && col==1 && i==4)val[0]=0;
        if(ajPenPage==41 && row->op==AJ_IPTV_CONTA)snprintf(val,sizeof val,"%d",iptv_contas(NULL,0));
        if(ajPenPage==38 && (row->op==AJ_FALHA_PROVEDORES || row->op==AJ_FALHA_ADDONS))snprintf(val,sizeof val,i18n(row->op==AJ_FALHA_PROVEDORES?"%d provedores":"%d addons"),ajPenFailures(row->op==AJ_FALHA_PROVEDORES));
        if(ajPenPage==36 && row->op==AJ_DET_META_EXT) {
          int count=0;for(int op=AJ_TMDB_ARTE;op<=AJ_TMDB_CW;op++)if(lig(op))count++;
          snprintf(val,sizeof val,i18n("%d ajustes"),count);
        }
        if(row->op<=-10 && row->op>=-13)ajPenParentText(-10-row->op,perfil_regra(ajPenParentProfile(),-10-row->op),val,sizeof val);
        if(row->op==-4)ajPenParentText(PR_FILMES,perfil_regra(ajPenParentProfile(),PR_FILMES),val,sizeof val);
        if(ajPenPage==28 && row->op==AJ_LEG1_RESTAURAR)val[0]=0;
        if(ajPenPage==28 && row->op==AJ_LEG1_ATRASO && i==5)snprintf(val,sizeof val,i18n("%+.3f s"),uxValorAtual(row->op)/1000.0);
        float valueWidth=w>1000?430:300;
        if(txt_largura(TXT_PEN_VALUE,val)>valueWidth)ajBloco(TXT_PEN_VALUE,val,x+w-52-valueWidth,y+13,valueWidth,23,.58f,2);
        else { TxtLinha t=ajTxt(TXT_PEN_VALUE,val);txt_desenhar_alpha(t,x+w-52-t.w,y+24,.58f); }
        ajIcone("aj_chevron-right",x+w-44,y+28,18,.45f);
      }
      if(transform)gfx_sem_transformar();
      aj4Alvo((GfxRect){x,y,w,compact?60:74},8,index);
    }
  }
  if(ajPenPage==44)ajPenChip("Componentes",NULL,1300,112,180,ajPenRow==99,12,4);
  if(ajPenPage==29)aj4Texto("A TV é quem reproduz. O Nuvio converte o que a TV não entende.",TXT_AJ_MINI14,226,908,748,.4f);
  ajPenRodape(ajPenPage==27 || ajPenPage==28?"Áudio e legendas":ajPenPage<27?page->title:page->section,page->part,page->total);
}
static int ajPenChannels(void) {
  int total=0;GuiaFonteSaude source;
  for(int i=0;guia_fonte_saude(i,&source);i++)total+=source.canais;
  return total;
}
static void ajPenHome(void) {
  static const char *const names[]={"Reproduzir","Áudio e legendas","Minha tela","Perfil e serviços"};
  static const char *const icons[]={"aj_play","aj_languages","aj_layout-grid","aj_users"};
  static const char *const desc[]={"Qualidade, vídeo escolhido e o que toca a seguir.","Voz, idioma das legendas e sincronia.","Como a tela inicial se comporta no seu sofá.","Contas, add-ons e servidores de mídia."};
  static const char *const advanced[]={"Qualidade, fonte, HDR e o player.","Faixas, sincronia e a segunda legenda.","Layout, texto, vidro e movimento.","Perfis, Trakt, add-ons e servidores."};
  static const int valueOps[]={AJ_QUALIDADE,AJ_AUD_LINGUA,AJ_HOME_LAYOUT,AJ_PERFIL_ATIVO};
  int av=lig(AJ_AVANCADAS);
  ajPenBase();
  ajKicker("NUVIO · TV DA SALA",200,120,1);
  aj4Texto("Ajustes",TXT_TITULO2,200,140,1000,1);
  aj4Texto(av?"420 ajustes em 8 seções. O modo avançado mostra as opções técnicas de cada bloco.":"8 seções. Cada escolha mostra o valor atual antes de você abrir.",TXT_G20M,200,234,1360,.58f);
  if(!av || aj4Topo==0)ajPenChip("Buscar ajuste","aj_search",1642,152,187,aj4Topo==0,0,0);
  ajPenModo(216);
  const char *modeText=av?"Avançado mostra as opções técnicas de cada seção":"Avançado revela 4 ajustes técnicos";
  TxtLinha modeHelp=ajTxt(TXT_AJ_MINI14,modeText);txt_desenhar_alpha(modeHelp,1824-modeHelp.w,289,.4f);
  ajKicker("DO DIA A DIA",200,290,1);
  for(int i=0;i<4;i++) {
    float x=200+i*412;int focus=aj4Linha==i && aj4Topo<0;
    ajPenPanel(x,322,388,250,focus);
    ajNeutro((GfxRect){x+26,348,56,56},20,focus?.16f:.09f,.18f,.19f,.21f,1);
    ajIcone(icons[i],x+40,362,28,1);
    aj4Texto(names[i],TXT_G28B,x+26,425,336,1);
    ajBloco(TXT_G18R,av?advanced[i]:desc[i],x+26,464,336,22,.58f,2);
    ajCaps(TXT_AJ_MINI12,"AGORA",1.5f,x+26,530+ajPenDy,.40f);
    char value[160];snprintf(value,sizeof value,"%s",textoValor(valueOps[i]));
    if(i==3)snprintf(value,sizeof value,"%s",pfNome(perfis_ativo()));
    if(i==1 && !strcmp(ling_opcao_codigo(uxValorAtual(AJ_AUD_LINGUA)),"pt"))snprintf(value,sizeof value,"%s",i18n("Português (Brasil)"));
    if(av) {
      char second[100];
      if(i==0)snprintf(value,sizeof value,i18n("%s · DV %s"),textoValor(AJ_QUALIDADE),i18n(lig(AJ_DV)?"ligado":"desligado"));
      else if(i==1) { const char *code=ling_opcao_codigo(uxValorAtual(AJ_AUD_LINGUA));snprintf(value,sizeof value,i18n("%s · %s"),!strcmp(code,"pt")?"pt-BR":code,lig(AJ_ATMOS)?"Dolby Atmos":i18n("Estéreo")); }
      else if(i==2) { snprintf(second,sizeof second,"%s",textoValor(AJ_ESCALA_TEXTO));snprintf(value,sizeof value,i18n("%s · %s"),textoValor(AJ_HOME_LAYOUT),second); }
      else snprintf(value,sizeof value,i18n("%s · %d canais"),pfNome(perfis_ativo()),ajPenChannels());
    }
    int changed=0;for(int op=0;op<AJ_N;op++)if(aj4Simples(op)==i+1 && uxDiferente(op))changed++;
    if(av)ajBloco(TXT_G18M,value,x+96,519,changed?194:260,21,1,2);
    else aj4Texto(value,TXT_G18M,x+96,527,changed?194:260,1);
    if(changed) { char count[64];snprintf(count,sizeof count,i18n(changed==1?"%d alterado":"%d alterados"),changed);float r,g,b;ajustes_acento(&r,&g,&b);
      TxtLinha t=txt_linha_corta(TXT_G14B,count,r*255,g*255,b*255,255,100);
      txt_desenhar(t,x+96+fminf(txt_largura(TXT_G18M,value),194)+10,530);
    }
    aj4Alvo((GfxRect){x,322,388,250},1,i);
  }
  ajKicker("TAMBÉM AQUI",200,602,1);
  ajPenPanel(200,634,1624,214,0);
  static const char *const other[]={"TV ao vivo e guia","Controle parental","Desempenho","Sobre e ajuda"};
  static const char *const help[]={"Canais, EPG e gravações","PIN e limites por perfil","Cache, decodificação e memória","Versão, suporte e código de log"};
  static const char *const ic[]={"aj_tv","aj_lock","aj_cpu","aj_info"};
  for(int i=0;i<4;i++) {
    float x=i%2?1254:226,y=i<2?660:752;int row=9+i;
    if(aj4Linha==row && aj4Topo<0)ajNeutro((GfxRect){x,y,544,78},20,.14f,.169f,.176f,.204f,1);
    ajIcone(ic[i],x+16,y+22,26,.8f);
    aj4Texto(other[i],TXT_G21B,x+58,y+12,460,1);
    aj4Texto(help[i],TXT_AJ_MINI14,x+58,y+42,460,.4f);
    aj4Alvo((GfxRect){x,y,544,78},1,row);
  }
  char val[160];snprintf(val,sizeof val,i18n("%s · %d canais"),textoValor(AJ_IPTV_CONTA),ajPenChannels());
  aj4Texto(val,TXT_G18R,828,689,230,.55f);ajIcone("aj_chevron-right",1070,689,18,.45f);
  snprintf(val,sizeof val,"%s",textoValor(AJ_GPU_EFEITOS));
  aj4Texto(val,TXT_G18R,828,781,230,.55f);ajIcone("aj_chevron-right",1070,781,18,.45f);
  ajPenPanel(200,876,1624,124,0);
  ajNeutro((GfxRect){226,910,56,56},20,.09f,.18f,.19f,.21f,1);
  ajIcone("aj_sparkles",240,924,28,1);
  aj4Texto("O que o Nuvio faz?",TXT_G21B,298,916,340,1);
  aj4Texto("A visita guiada: 12 recursos prontos para ligar.",TXT_AJ_MINI14,298,946,340,.4f);
  ajPenChip("Abrir",NULL,641,912,84,aj4Linha==7 && aj4Topo<0,1,7);
  gfx_cor((GfxRect){av?940:960,906,1,64},0,1,1,1,.12f);
  ajPenChip("Presets","aj_sparkles",av?966:1192,912,134,aj4Linha==4 && aj4Topo<0,1,4);
  char count[128];snprintf(count,sizeof count,i18n("Diferente do padrão · %d"),aj4Diferencas(aj4Opcoes));
  ajPenChip(count,"aj_rotate-ccw",av?1112:1338,912,263,aj4Linha==5 && aj4Topo<0,1,5);
  ajPenChip("Regras da TV","aj_shield",av?1387:1613,912,185,aj4Linha==6 && aj4Topo<0,1,6);
  if(av)ajPenChip("Opções técnicas","aj_terminal",1584,912,214,aj4Linha==13 && aj4Topo<0,1,13);
  ajPenRodape("Ajustes",1,3);
}

/* iPhone Duo closed (portrait phone): the Settings hub and its pages as one
   island card list, sized in physical pt. Drawn on the same virtual canvas;
   values and actions stay owned by the TV renderer above. */
static int ajFone(void) {
#ifdef NV_TOUCH_UI
  return layout_modo_mobile() && NV_LAYOUT_REAL_H > NV_LAYOUT_REAL_W;
#else
  return 0;
#endif
}
static float ajPt(float pt) { return pt*NV_LAYOUT_REAL_W/(466*gfx_escala()); }
/* Text at pt px height: the layer factor makes the rasteriser draw at that size. */
static float ajFT(const char *s,TxtEstilo st,float nn,float pt,float x,float y,float maxW,float a,int right) {
  if(!s || !s[0])return 0;
  float eh=gfx_escala(),f=pt/nn*NV_LAYOUT_REAL_W/466,k=eh/f;
  gfx_escala_sair(f);
  TxtLinha t=ajTxtC(st,s,maxW*k);
  float w=t.w/k;
  txt_desenhar_alpha(t,right?x*k-t.w:x*k,(y+ajPenDy)*k-ajPenDy,a);
  gfx_escala_sair(eh);
  return w;
}
static float ajFoneRol,ajFoneRolMax,ajFoneEsc=1;
static int ajFonePag=-2,ajFoneUlt=-1;
static int ajFoneToque(const PonteiroRolagem *e) {
  if(e->fase==PONT_ROL_INICIO)return 1;
  if(e->fase==PONT_ROL_MOVER) {
    ajFoneRol-=e->delta/ajFoneEsc;
    if(ajFoneRol<0)ajFoneRol=0;
    if(ajFoneRol>ajFoneRolMax)ajFoneRol=ajFoneRolMax;
  }
  return 0;
}
static void ajFoneFundo(void) {
  GfxRect s={0,0,NV_VTELA_W,NV_VTELA_H};
  gfx_cor(s,0,.0424f,.0531f,.0638f,1);
  gfx_luz_canto(s,0,NV_VTELA_H*.35f,NV_VTELA_W*.8f,NV_VTELA_W*.9f,.1883f,.1818f,.3155f,.5f);
}
static void ajFoneTopo(float L,float R,float T,int pagina,const char *titulo) {
  float y=T+ajPt(10);
  if(pagina) {
    GfxRect b={L,y-ajPt(4),ajPt(36),ajPt(36)};
    ajNeutro(b,ajPt(18),.09f,.18f,.19f,.21f,1);
    ajIcone("aj_arrow-left",b.x+ajPt(9),b.y+ajPt(9),ajPt(18),1);
    aj4Alvo((GfxRect){b.x-ajPt(4),b.y-ajPt(4),ajPt(44),ajPt(44)},5,0);
    ajFT(titulo,TXT_G28B,28,22,L+ajPt(48),y,R-L-ajPt(48+44),1,0);
  } else ajFT(titulo,TXT_G28B,28,22,L,y+ajPt(2),R-L-ajPt(44),1,0);
  GfxRect q={R-ajPt(36),y-ajPt(4),ajPt(36),ajPt(36)};
  int foco=aj4Topo==0;
  if(foco)ajPilulaAcento(q,ajPt(18),1);else ajNeutro(q,ajPt(18),.07f,.14f,.15f,.17f,1);
  if(foco)ajIconeT("aj_search",q.x+ajPt(9),q.y+ajPt(9),ajPt(18),1);else ajIcone("aj_search",q.x+ajPt(9),q.y+ajPt(9),ajPt(18),.85f);
  aj4Alvo((GfxRect){q.x-ajPt(4),q.y-ajPt(4),ajPt(44),ajPt(44)},0,0);
#ifndef NV_IOS
  char hora[32];ajCenaHora(hora,sizeof hora);
  ajFT(hora,TXT_G18M,18,15,q.x-ajPt(10),y+ajPt(2),ajPt(70),.7f,1);
#endif
}
static void ajFoneLinha(float L,float R,float y,float h,const char *icone,const char *nome,const char *valor,int foco,int zona,int row,int ultima) {
  GfxRect r={L,y,R-L,h};
  if(foco)ajNeutro((GfxRect){L+ajPt(6),y+ajPt(2),R-L-ajPt(12),h-ajPt(4)},ajPt(14),.14f,.169f,.176f,.204f,1);
  ajIcone(icone,L+ajPt(16),y+(h-ajPt(20))/2,ajPt(20),foco?1:.78f);
  float vx=R-ajPt(16)-ajPt(12);
  float vw=0;
  if(valor && valor[0])vw=ajFT(valor,TXT_G18R,18,13,vx-ajPt(6),y+h/2-ajPt(8),(R-L)*.4f,.55f,1)+ajPt(6);
  ajFT(nome,TXT_G18M,18,16,L+ajPt(52),y+h/2-ajPt(10),vx-vw-(L+ajPt(52))-ajPt(8),1,0);
  ajIcone("aj_chevron-right",vx,y+(h-ajPt(14))/2,ajPt(14),.45f);
  if(!ultima)gfx_cor((GfxRect){L+ajPt(52),y+h-1,R-L-ajPt(52)-ajPt(16),1},0,1,1,1,.08f);
  aj4Alvo(r,zona,row);
}
static void ajFoneLista(void) {
  ajFoneFundo();
  float esc=gfx_escala(),L=ajPt(16),R=NV_VTELA_W-nv_inset_d/esc-ajPt(16),RB=NV_VTELA_W,T=nv_inset_t/esc;
  ajFoneTopo(L,R,T,0,"Ajustes");
  static const char *const nome[14]={"Reproduzir","Áudio e legendas","Minha tela","Perfil e serviços","Presets","Diferente do padrão","Regras da TV","O que o Nuvio faz?",NULL,"TV ao vivo e guia","Controle parental","Desempenho","Sobre e ajuda","Opções técnicas"};
  static const char *const icone[14]={"aj_play","aj_languages","aj_layout-grid","aj_users","aj_sparkles","aj_rotate-ccw","aj_shield","aj_sparkles",NULL,"aj_tv","aj_lock","aj_cpu","aj_info","aj_terminal"};
  static const int valueOps[]={AJ_QUALIDADE,AJ_AUD_LINGUA,AJ_HOME_LAYOUT,AJ_PERFIL_ATIVO};
  int ordem[13],n=0;
  static const int base[]={0,1,2,3,9,10,11,12,7,4,5,6};
  for(int i=0;i<12;i++)ordem[n++]=base[i];
  if(lig(AJ_AVANCADAS))ordem[n++]=13;
  float h=ajPt(48),y0=T+ajPt(62);
  ajNeutro((GfxRect){L,y0,RB-L+ajPt(18),h*n},ajPt(18),.07f,.11f,.12f,.14f,1);
  for(int k=0;k<n;k++) {
    int id=ordem[k];char val[160]="";
    if(id<4) {
      snprintf(val,sizeof val,"%s",textoValor(valueOps[id]));
      if(id==3)snprintf(val,sizeof val,"%s",pfNome(perfis_ativo()));
      if(id==1 && !strcmp(ling_opcao_codigo(uxValorAtual(AJ_AUD_LINGUA)),"pt"))snprintf(val,sizeof val,"%s",i18n("Português (Brasil)"));
    } else if(id==9)snprintf(val,sizeof val,i18n("%s · %d canais"),textoValor(AJ_IPTV_CONTA),ajPenChannels());
    else if(id==11)snprintf(val,sizeof val,"%s",textoValor(AJ_GPU_EFEITOS));
    else if(id==5)snprintf(val,sizeof val,"%d",aj4Diferencas(aj4Opcoes));
    ajFoneLinha(L,RB,y0+k*h,h,icone[id],nome[id],val,aj4Linha==id && aj4Topo<0,1,id,k==n-1);
  }
}
static void ajFoneValor(const AjPenRow *row,int col,int i,char *val,size_t n) {
  snprintf(val,n,"%s",row->op>=0?textoValor(row->op):row->op==-3?i18n(admin_perfil_pin_definido(perfis_ativo())?"PIN definido":"Definir um PIN"):row->op==-6?pfNome(ajPenParentProfile()):row->op==-4?i18n("Livre e até 10 anos"):i18n("Abrir"));
  if((row->op==AJ_AUD_LINGUA || row->op==AJ_LEG_LINGUA) && !strcmp(ling_opcao_codigo(uxValorAtual(row->op)),"pt"))snprintf(val,n,"%s",i18n("Português (Brasil)"));
  if(row->op>=0 && OPCOES[row->op].tipo==OP_ACAO && !val[0])snprintf(val,n,"%s",i18n("Abrir"));
  if(ajPenPage==39 && i==5 && col==1)val[0]=0;
  if(ajPenPage==40 && col==1 && i==4)val[0]=0;
  if(ajPenPage==41 && row->op==AJ_IPTV_CONTA)snprintf(val,n,"%d",iptv_contas(NULL,0));
  if(ajPenPage==38 && (row->op==AJ_FALHA_PROVEDORES || row->op==AJ_FALHA_ADDONS))snprintf(val,n,i18n(row->op==AJ_FALHA_PROVEDORES?"%d provedores":"%d addons"),ajPenFailures(row->op==AJ_FALHA_PROVEDORES));
  if(ajPenPage==36 && row->op==AJ_DET_META_EXT) {
    int count=0;for(int op=AJ_TMDB_ARTE;op<=AJ_TMDB_CW;op++)if(lig(op))count++;
    snprintf(val,n,i18n("%d ajustes"),count);
  }
  if(row->op<=-10 && row->op>=-13)ajPenParentText(-10-row->op,perfil_regra(ajPenParentProfile(),-10-row->op),val,n);
  if(row->op==-4)ajPenParentText(PR_FILMES,perfil_regra(ajPenParentProfile(),PR_FILMES),val,n);
  if(ajPenPage==28 && row->op==AJ_LEG1_RESTAURAR)val[0]=0;
  if(ajPenPage==28 && row->op==AJ_LEG1_ATRASO && i==5)snprintf(val,n,i18n("%+.3f s"),uxValorAtual(row->op)/1000.0);
}
static void ajFonePagina(void) {
  const AjPenPage *page=ajPenCurrent();
  ajFoneFundo();
  float esc=gfx_escala(),L=ajPt(16),R=NV_VTELA_W-nv_inset_d/esc-ajPt(16),RB=NV_VTELA_W,T=nv_inset_t/esc;
  ajFoneEsc=esc;
  float top=T+ajPt(56),bottom=NV_VTELA_H,hr=ajPt(52),hk=ajPt(30);
  /* total content height */
  float total=0;int rows=0;
  for(int c=0;c<2;c++)if(page->cols[c].n){total+=hk+page->cols[c].n*hr;rows+=page->cols[c].n;}
  ajFoneRolMax=fmaxf(0,total+ajPt(24)-(bottom-top));
  if(ajFonePag!=ajPenPage) { ajFonePag=ajPenPage;ajFoneRol=0;ajFoneUlt=-1; }
  /* keyboard focus keeps the row in view */
  if(ajPenRow!=ajFoneUlt && aj4Topo<0) {
    float yy=0;int idx=0;
    for(int c=0;c<2;c++) {
      if(!page->cols[c].n)continue;
      yy+=hk;
      if(ajPenRow>=idx && ajPenRow<idx+page->cols[c].n) {
        float ry=yy+(ajPenRow-idx)*hr;
        if(ry<ajFoneRol)ajFoneRol=ry-hk;
        else if(ry+hr>ajFoneRol+(bottom-top))ajFoneRol=ry+hr-(bottom-top)+ajPt(12);
      }
      yy+=page->cols[c].n*hr;idx+=page->cols[c].n;
    }
    if(ajFoneRol<0)ajFoneRol=0;if(ajFoneRol>ajFoneRolMax)ajFoneRol=ajFoneRolMax;
    ajFoneUlt=ajPenRow;
  }
  ponteiro_rolagem(ajFoneToque);
  GfxRecorteAnt ant=gfx_recorte_dentro(0,top,NV_VTELA_W,bottom-top);
  float y=top-ajFoneRol;int index=0;
  for(int c=0;c<2;c++) {
    const AjPenColumn *col=&page->cols[c];if(!col->n)continue;
    if(col->title && col->title[0])ajFT(col->title,TXT_AJ_MINI14,14,11,L+ajPt(4),y+ajPt(10),RB-L,.45f,0);
    y+=hk;
    float py=y;
    ajPenPanel(L,py,RB-L+ajPt(18),col->n*hr,0);
    for(int i=0;i<col->n;i++,index++) {
      const AjPenRow *row=&col->rows[i];float ry=y+i*hr;
      int visible=ry+hr>top && ry<bottom;
      if(!visible)continue;
      int foco=index==ajPenRow && aj4Topo<0;
      int ok=row->op>=0?ajOpcaoDisponivel(row->op):row->op!=-1;
      float a=ok?1:.45f;
      if(foco)ajNeutro((GfxRect){L+ajPt(6),ry+ajPt(2),RB-L-ajPt(12),hr-ajPt(4)},ajPt(14),.14f,.169f,.176f,.204f,1);
      ajIcone(row->icon,L+ajPt(16),ry+(hr-ajPt(20))/2,ajPt(20),foco?1:.75f);
      float rx=RB-ajPt(16),tx=L+ajPt(52);
      int help=row->help && row->help[0];
      float ty=ry+(help?ajPt(9):(hr-ajPt(16))/2-ajPt(2));
      if(row->toggle && row->op!=-1) {
        int on=row->op<=-10 && row->op>=-13?perfil_regra(ajPenParentProfile(),-10-row->op):row->op==-2?perfil_ajustes_travados(ajPenParentProfile()):uxValorAtual(row->op)==0;
        if(ajPenPage==AJ_PEN_PARENTAL && row->op==AJ_ENQUETES)on=!on;
        float r=.20f,g=.21f,b=.24f;if(on)ajustes_acento(&r,&g,&b);
        float sw=ajPt(40),sh=ajPt(24),sx=rx-sw,sy=ry+(hr-sh)/2;
        gfx_cor((GfxRect){sx,sy,sw,sh},.5f,r,g,b,1);
        gfx_cor((GfxRect){sx+ajPt(2)+(on?ajPt(16):0),sy+ajPt(2),ajPt(20),ajPt(20),},.5f,1,1,1,1);
        ajFT(row->title,TXT_G18M,18,16,tx,ty,sx-tx-ajPt(10),a,0);
      } else {
        char val[160];ajFoneValor(row,c,i,val,sizeof val);
        float vx=rx-ajPt(14)-ajPt(4),vw=0;
        if(val[0])vw=ajFT(val,TXT_G18R,18,13,vx,ry+(hr-ajPt(14))/2-ajPt(1),(RB-L)*.38f,.55f,1)+ajPt(8);
        ajIcone("aj_chevron-right",rx-ajPt(14),ry+(hr-ajPt(14))/2,ajPt(14),.45f);
        ajFT(row->title,TXT_G18M,18,16,tx,ty,vx-vw-tx-ajPt(6),a,0);
      }
      if(help)ajFT(row->help,TXT_AJ_MINI14,14,11,tx,ty+ajPt(21),rx-tx-ajPt(54),.4f,0);
      if(i<col->n-1)gfx_cor((GfxRect){tx,ry+hr-1,rx-tx,1},0,1,1,1,.08f);
      GfxRect t={L,fmaxf(ry,top),RB-L,fminf(ry+hr,bottom)-fmaxf(ry,top)};
      if(t.h>ajPt(8))aj4Alvo(t,8,index);
    }
    y+=col->n*hr;
  }
  gfx_recorte_voltar(ant);
  ajFoneTopo(L,R,T,1,page->title);
}
