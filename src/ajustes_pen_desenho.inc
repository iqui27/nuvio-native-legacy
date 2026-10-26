/* Pencil Settings, 1920 x 1080. Values and actions remain owned by ajustes.c. */
#include "menu.h"
static void ajPenPanel(float x,float y,float w,float h,int focus) {
  GfxRect r={x,y,w,h};
  if(focus) ajNeutro(r,28,.14f,.169f,.176f,.204f,1);
  else if(ajVid()) gfx_vidro_miolo(r,28/h,.055f,.059f,.071f,.80f,1);
  else gfx_cor(r,28/h,.055f,.059f,.071f,1);
}
static void ajPenChip(const char *s,const char *ic,float x,float y,float w,int focus,int zone,int row) {
  GfxRect r={x,y,w,52};
  if(focus)ajPilulaAcento(r,26,1);
  else ajNeutro(r,26,.07f,.14f,.15f,.17f,1);
  if(ic) { if(focus)ajIconeT(ic,x+20,y+17,18,1);else ajIcone(ic,x+20,y+17,18,.8f); }
  TxtLinha t=focus?txt_linha_corta(TXT_G18M,s,ajTinta(),ajTinta(),ajTinta(),255,w-(ic?58:32)):ajTxtC(TXT_G18M,s,w-(ic?58:32));
  txt_desenhar(t,x+(ic?48:(w-t.w)/2),y+(52-t.h)/2);
  aj4Alvo(r,zone,row);
}
static void ajPenRailFoco(int row,int unused) { (void)unused;menu_abrir();(void)row; }
static void ajPenBase(void) {
  GfxRect screen={0,0,1920,1080};
  gfx_cor(screen,0,.035f,.035f,.043f,1);
  gfx_luz_canto(screen,0,1250,330,1550,.23f,.22f,.37f,.82f);
  gfx_luz_canto(screen,0,1920,640,1150,.18f,.19f,.34f,.65f);
  gfx_veu_css(screen,2,.65f,1,.82f);
  ajPenPanel(48,112,88,522,0);
  static const char *const icons[]={"aj_house","aj_compass","aj_tv-minimal-play","aj_search","aj_book-open","aj_calendar","aj_user-round","aj_sliders-horizontal"};
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
  aj4Texto("abrir",TXT_AJ_MINI14,246,1029,70,.45f);
  ajNeutro((GfxRect){289,1024,29,26},7,.07f,.14f,.15f,.17f,1);
  ajIcone("aj_arrow-left",297,1031,14,.8f);
  aj4Texto("voltar",TXT_AJ_MINI14,328,1029,160,.45f);
  char text[200];snprintf(text,sizeof text,i18n("%s · %d de %d"),i18n(page),part,total);
  TxtLinha t=ajTxt(TXT_AJ_MINI14,text);txt_desenhar_alpha(t,1824-t.w,1029,.45f);
  aj4Texto("navegar",TXT_AJ_MINI14,1643-t.w,1029,90,.45f);
  ajIcone("aj_arrow-up",1557-t.w,1030,16,.65f);
  ajIcone("aj_arrow-down",1601-t.w,1030,16,.65f);
}
static void ajPenRows(void) {
  const AjPenPage *page=ajPenCurrent();
  if(!page)return;
  ajPenBase();
  ajPenChip("Ajustes","pl_chevron-left",200,112,124,0,5,0);
  ajKicker(ajPenPage<27?"AJUSTES AVANÇADOS":page->section,200,200,.45f);
  aj4Texto(page->title,TXT_TITULO2,200,218,1500,1);
  if(ajPenPage<27 || ajPenPage>=46) {
    char sub[240];snprintf(sub,sizeof sub,i18n("Opções de %s que o modo Simples esconde. Valor atual à direita."),i18n(page->section));
    aj4Texto(sub,TXT_G20M,200,291,1520,.58f);
  } else {
    ajPenModo(118);
    aj4Texto(ajPenPage>=29?aj4Ajuda(page->cols[0].rows[0].op):ajPenPage==27?"Vale para quase todo título. Se um filme não tiver a faixa escolhida, tocamos a original e avisamos.":"Tamanho, fonte, cor e borda da legenda principal.",TXT_G20M,200,291,1520,.58f);
  }
  int index=0;
  for(int col=0;col<2;col++) {
    const AjPenColumn *column=&page->cols[col];if(!column->n)continue;
    ajPenPanel(column->x,344,column->w,640,0);
    ajKicker(column->title,column->x+26,370,.45f);
    for(int i=0;i<column->n;i++,index++) {
      const AjPenRow *row=&column->rows[i];float x=column->x+26,y=400+i*(ajPenPage<27 || ajPenPage>=46?82:84),w=column->w-52;
      int focus=index==ajPenRow && aj4Topo<0;
      if(focus)ajNeutro((GfxRect){x,y,w,74},20,.14f,.169f,.176f,.204f,1);
      ajIcone(row->icon,x+20,y+24,26,focus?1:.75f);
      int available=ajOpcaoDisponivel(row->op);
      aj4Texto(row->title,TXT_G21B,x+62,y+13,w-300,available?1:.45f);
      aj4Texto(row->help,TXT_AJ_MINI14,x+62,y+42,w-210,.4f);
      if(row->toggle) {
        int on=uxValorAtual(row->op)==0;float r=.20f,g=.21f,b=.24f;
        if(on)ajustes_acento(&r,&g,&b);
        gfx_cor((GfxRect){x+w-90,y+19,64,36},.5f,r,g,b,1);
        gfx_cor((GfxRect){x+w-86+(on?28:0),y+23,28,28},.5f,1,1,1,1);
      } else {
        char val[160];uxValorTexto(row->op,uxValorAtual(row->op),val,sizeof val);
        TxtLinha t=ajTxtC(TXT_G18R,val,180);txt_desenhar_alpha(t,x+w-52-t.w,y+24,.58f);
        ajIcone("aj_chevron-right",x+w-44,y+28,18,.45f);
      }
      aj4Alvo((GfxRect){x,y,w,74},8,index);
    }
  }
  ajPenRodape(ajPenPage<27?page->title:page->section,page->part,page->total);
}
static void ajPenHome(void) {
  static const char *const names[]={"Reproduzir","Áudio e legendas","Minha tela","Perfil e serviços"};
  static const char *const icons[]={"aj_play","aj_languages","aj_layout-grid","aj_users"};
  static const char *const desc[]={"Qualidade, vídeo escolhido e o que toca a seguir.","Voz, idioma das legendas e sincronia.","Como a tela inicial se comporta no seu sofá.","Contas, add-ons e servidores de mídia."};
  static const char *const advanced[]={"Qualidade, fonte, HDR e o player.","Faixas, sincronia e a segunda legenda.","Layout, texto, vidro e movimento.","Perfis, Trakt, add-ons e servidores."};
  static const int valueOps[]={AJ_QUALIDADE,AJ_AUD_LINGUA,AJ_HOME_LAYOUT,AJ_PERFIL_ATIVO};
  int av=lig(AJ_AVANCADAS);
  ajPenBase();
  ajKicker("NUVIO · TV DA SALA",200,122,.45f);
  aj4Texto("Ajustes",TXT_TITULO2,200,146,1000,1);
  aj4Texto(av?"420 ajustes em 8 seções. O modo avançado mostra as opções técnicas de cada bloco.":"8 seções. Cada escolha mostra o valor atual antes de você abrir.",TXT_G20M,200,234,1360,.58f);
  ajPenChip("Buscar ajuste","aj_search",1642,152,187,aj4Topo==0,0,0);
  ajPenModo(216);
  aj4Texto(av?"Avançado mostra as opções técnicas de cada seção":"Avançado revela os ajustes técnicos",TXT_AJ_MINI14,av?1452:1574,289,av?372:250,.4f);
  ajKicker("DO DIA A DIA",200,292,.45f);
  for(int i=0;i<4;i++) {
    float x=200+i*412;int focus=aj4Linha==i && aj4Topo<0;
    ajPenPanel(x,322,388,250,focus);
    ajNeutro((GfxRect){x+26,348,56,56},20,focus?.16f:.09f,.18f,.19f,.21f,1);
    ajIcone(icons[i],x+40,362,28,1);
    aj4Texto(names[i],TXT_G28B,x+26,425,336,1);
    ajBloco(TXT_G18R,av?advanced[i]:desc[i],x+26,464,336,22,.58f,2);
    ajCaps(TXT_AJ_MINI12,"AGORA",1.5f,x+26,530,.40f);
    char value[160];uxValorTexto(valueOps[i],uxValorAtual(valueOps[i]),value,sizeof value);
    if(i==3)snprintf(value,sizeof value,"%s",pfNome(perfis_ativo()));
    aj4Texto(value,TXT_G18M,x+96,527,260,1);
    aj4Alvo((GfxRect){x,322,388,250},1,i);
  }
  ajKicker("TAMBÉM AQUI",200,604,.45f);
  ajPenPanel(200,634,1624,214,0);
  static const char *const other[]={"TV ao vivo e guia","Controle parental","Desempenho","Sobre e ajuda"};
  static const char *const help[]={"Canais, EPG e gravações","PIN e limites por perfil","Cache, decodificação e memória","Versão, suporte e código de log"};
  static const char *const ic[]={"aj_tv-minimal-play","aj_lock","aj_cpu","aj_info"};
  for(int i=0;i<4;i++) {
    float x=i%2?1254:226,y=i<2?660:752;int row=9+i;
    if(aj4Linha==row && aj4Topo<0)ajNeutro((GfxRect){x,y,544,78},20,.14f,.169f,.176f,.204f,1);
    ajIcone(ic[i],x+16,y+22,26,.8f);
    aj4Texto(other[i],TXT_G22M,x+58,y+12,460,1);
    aj4Texto(help[i],TXT_AJ_MINI14,x+58,y+42,460,.4f);
    aj4Alvo((GfxRect){x,y,544,78},1,row);
  }
  char val[160];uxValorTexto(AJ_IPTV_CONTA,uxValorAtual(AJ_IPTV_CONTA),val,sizeof val);
  aj4Texto(val,TXT_G18R,828,689,230,.55f);
  uxValorTexto(AJ_GPU_EFEITOS,uxValorAtual(AJ_GPU_EFEITOS),val,sizeof val);
  aj4Texto(val,TXT_G18R,828,781,230,.55f);
  ajPenPanel(200,876,1624,124,0);
  ajNeutro((GfxRect){226,910,56,56},20,.09f,.18f,.19f,.21f,1);
  ajIcone("aj_sparkles",240,924,28,1);
  aj4Texto("O que o Nuvio faz?",TXT_G22M,298,916,340,1);
  aj4Texto("A visita guiada: recursos prontos para ligar.",TXT_AJ_MINI14,298,946,340,.4f);
  ajPenChip("Abrir",NULL,641,912,84,aj4Linha==7 && aj4Topo<0,1,7);
  gfx_cor((GfxRect){960,906,1,64},0,1,1,1,.12f);
  ajPenChip("Presets","aj_sparkles",av?966:1192,912,134,aj4Linha==4 && aj4Topo<0,1,4);
  char count[128];snprintf(count,sizeof count,i18n("Diferente do padrão · %d"),aj4Diferencas(aj4Opcoes));
  ajPenChip(count,"aj_rotate-ccw-clock",av?1112:1338,912,263,aj4Linha==5 && aj4Topo<0,1,5);
  ajPenChip("Regras da TV","aj_shield",av?1387:1613,912,185,aj4Linha==6 && aj4Topo<0,1,6);
  if(av)ajPenChip("Opções técnicas","aj_sliders-horizontal",1584,912,214,aj4Linha==13 && aj4Topo<0,1,13);
  ajPenRodape("Ajustes",1,3);
}
