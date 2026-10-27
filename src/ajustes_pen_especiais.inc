static void ajPenParentChoice(void) {
  int rule=ajPenParentEditor;ajPenBase();
  ajPenChip("Controle parental","pl_chevron-left",200,112,230,0,5,0);
  ajKicker("CONTROLE PARENTAL",200,198,1);aj4Texto(AJ_PEN_PARENT_LABEL[rule],TXT_TITULO2,200,212,1500,1);
  ajPenPanel(200,344,1624,640,0);
  const int *choices=rule==PR_TEMPO?AJ_PEN_TIME:AJ_PEN_AGE;
  int n=rule==PR_TEMPO?7:rule==PR_IDADE || rule==PR_FILMES?6:2;
  for(int i=0;i<n;i++) {
    int value=rule==PR_BUSCA_LIVRE || rule==PR_COMPRAS?i:choices[i];float y=380+i*76;char label[96];
    ajPenParentText(rule,value,label,sizeof label);
    if(value==ajPenParentDraft)ajPenPanel(226,y,1572,70,1);
    aj4Texto(label,TXT_G21B,248,y+20,1500,1);
  }
  ajPenRodape("Controle parental",3,3);
}
/* Specialized Pencil pages use live subsystem state. */
static void ajPenFeatures(void) {
  ajPenBase();ajPenChip("Ajustes","pl_chevron-left",200,112,124,0,5,0);
  ajKicker("AJUSTES",200,198,1);aj4Texto("O que o Nuvio faz?",TXT_TITULO2,200,212,1500,1);
  aj4Texto("12 recursos prontos para ligar. A visita guiada liga um por um e explica cada um.",TXT_G20M,200,291,1520,.58f);
  for(int i=0;i<12;i++) {
    const AjPenRow *r=&AJ_PEN_FEATURES.cols[i/6].rows[i%6];
    float x=200+i%4*408,y=344+i/4*206;
    ajPenPanel(x,y,388,186,i==ajPenRow);
    ajNeutro((GfxRect){x+22,y+22,44,44},16,.09f,.18f,.19f,.21f,1);
    ajIcone(r->icon,x+32,y+32,24,.9f);
    aj4Texto(r->title,TXT_G21B,x+22,y+80,344,1);
    ajBloco(TXT_AJ_MINI14,r->help,x+22,y+112,344,22,.58f,2);
    int show=i==0 || i==1 || i==2 || i==4;
    int on=lig(i==0?AJ_CW_LIGADO:i==1?AJ_DEBRID_LIGADO:i==2?AJ_ADDONS_LIGADO:AJ_LEG_ESTILO_LIGADO);
    if(show) {
      float red,green,blue;ajustes_acento(&red,&green,&blue);
      gfx_cor((GfxRect){x+300,y+22,64,36},.5f,on?red:.20f,on?green:.21f,on?blue:.24f,1);
      gfx_cor((GfxRect){x+304+(on?28:0),y+26,28,28},.5f,1,1,1,1);
    }
    aj4Alvo((GfxRect){x,y,388,186},8,i);
  }
  ajPenChip("Abrir o Guia de uso",NULL,200,980,280,ajPenRow==12,8,12);
  ajPenRodape("O que o Nuvio faz",3,3);
}
static void ajPenRules(void) {
  ajPenBase();
  ajPenChip("Ajustes","pl_chevron-left",200,112,124,0,5,0);
  ajKicker("AJUSTES",200,198,1);
  aj4Texto("Regras da TV",TXT_TITULO2,200,212,1500,1);
  aj4Texto("O que a sua TV aguenta. O Nuvio segue estas regras para não travar nem cortar a imagem.",TXT_G20M,200,291,1520,.58f);
  static const char *const heads[]={"O QUE A TV FALA","O QUE O NUVIO NÃO FAZ"};
  static const char *const names[]={"Resolução da interface","HDR","Dolby Vision","DTS","Decodificação","Memória",
    "Recortar a imagem","Trocar de fonte sozinho","Esconder a faixa original","Enviar seu histórico","Forçar efeito pesado","Ligar a voz sem avisar"};
  static const char *const help[]={"A TV desenha até 4K","HDR10 e HDR10+ são entregues como HDR10","Perfil 8, com DV se a TV pedir","Convertido em Dolby Digital ou estéreo","HEVC e AV1 por hardware","O teto de RAM do aparelho",
    "A TV decide o formato; o Nuvio avisa","Só quando você deixa o automático","O original é sempre a primeira opção","Só com o seu comando","Se a TV não aguenta, o efeito não vem","A busca por voz só liga com o botão"};
  static const char *const icons[]={"aj_monitor","aj_sun","aj_sparkles","aj_volume-2","aj_cpu","aj_hard-drive","aj_crop","aj_repeat","aj_eye-off","aj_history","aj_bug","aj_mic"};
  long ram=0;tex_orcamento_info(NULL,&ram,NULL,NULL);
  char values[6][160];
  snprintf(values[0],sizeof values[0],"%dx%d",(int)NV_LAYOUT_REAL_W,(int)NV_LAYOUT_REAL_H);
  int hdr,dv,hevc,av1;stream_capacidades(&hdr,&dv,&hevc,&av1);
  snprintf(values[1],sizeof values[1],"%s",i18n(hdr<0?"Desconhecida":hdr?"Suportado":"Não suportado"));
  snprintf(values[2],sizeof values[2],"%s",i18n(dv<0?"Desconhecida":dv?"Suportado":"Não suportado"));
  snprintf(values[3],sizeof values[3],"%s",textoValor(AJ_DTS_AC3));
  snprintf(values[4],sizeof values[4],"%s",i18n(hevc<0 && av1<0?"Desconhecida":hevc>0 || av1>0?"Hardware":"Não suportado"));
  if(ram>0)snprintf(values[5],sizeof values[5],"%ld MB",ram);
  else snprintf(values[5],sizeof values[5],"%s",i18n("Desconhecida"));
  for(int c=0;c<2;c++) {
    float x=c?1024:200;ajPenPanel(x,344,800,640,0);ajKicker(heads[c],x+26,366,1);
    for(int r=0;r<6;r++) {
      int i=c*6+r;float y=400+r*84;
      ajIcone(icons[i],x+46,y+24,26,.75f);
      aj4Texto(names[i],TXT_G21B,x+88,y+13,470,1);
      char capability[240];const char *description=help[i];
      if(i==0) { snprintf(capability,sizeof capability,"%s: %s",i18n("Resolução da interface"),values[0]);description=capability; }
      if(i==1 || i==2) { snprintf(capability,sizeof capability,"%s · %s",i==1?"HDR":"Dolby Vision",values[i]);description=capability; }
      if(i==4) { snprintf(capability,sizeof capability,"HEVC: %s · AV1: %s",i18n(hevc<0?"Desconhecida":hevc?"Suportado":"Não suportado"),i18n(av1<0?"Desconhecida":av1?"Suportado":"Não suportado"));description=capability; }
      aj4Texto(description,TXT_AJ_MINI14,x+88,y+42,650,.4f);
      if(!c) { TxtLinha t=ajTxtC(TXT_G18R,values[i],220);txt_desenhar_alpha(t,x+710-t.w,y+24,.58f); }
    }
  }
  ajPenRodape("Regras da TV",3,3);
}
