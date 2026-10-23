// One island, four visible rows, shared geometry across the v4 screens.
static void aj4PontAtivar(int zona,int row) {
  if(zona==5) { aj4PontFoco(zona,row); return; }
  aj4PontFoco(zona,row);
  if(zona==4)uxAplicar();else aj4Evento(SDLK_RETURN);
}
static void aj4Alvo(GfxRect r,int zona,int row) {
  if(!pfTela && !teclado_aberto())ponteiro_alvo(r.x,r.y,r.w,r.h,aj4PontFoco,aj4PontAtivar,zona,row);
}
static void aj4Texto(const char *s, TxtEstilo e, float x, float y, float w, float a) {
  txt_desenhar_alpha(ajTxtC(e, s, w), x, y, a);
}
static void aj4Botao(const char *s, const char *ic, float x, float y, float w, int foco, int ativo) {
  GfxRect r = {x,y,w,58};
  if (foco) ajPilulaAcento(r,29,1);
  else ajNeutro(r,29,ativo ? 0.16f : 0.07f, ativo ? .20f : .14f, ativo ? .21f : .15f, ativo ? .24f : .17f,1);
  if (ic) { if (foco) ajIconeT(ic,x+22,y+15,28,1); else ajIcone(ic,x+22,y+15,28,.8f); }
  TxtLinha t = foco ? txt_linha_corta(TXT_V2_26,s,ajTinta(),ajTinta(),ajTinta(),255,w-(ic?72:28)) : ajTxtC(TXT_V2_26,s,w-(ic?72:28));
  txt_desenhar(t,x+(ic?64:20),y+(58-t.h)*.5f);
  if(y<240) {
    if(x<182 && w<=64)aj4Alvo(r,5,0);
    else if(x>=1060 && x<1230)aj4Alvo(r,0,0);
    else if(x>=1230 && x<1550)aj4Alvo(r,0,1);
    else if(x>=1550)aj4Alvo(r,0,2);
  }
}
static int aj4SeloAviso; // mixed-scope review: scope chips draw in the warning amber (mock .warning).
static void aj4LinhaDesenhar(const char *rot, const char *sub, const char *val, const char *selo,
                            const char *ic, float y, int foco, int marcado) {
  GfxRect r = {622,y,1198,113};
  float tx = ic ? 714 : 646;
  if (foco) ajNeutro(r,21,.13f,.169f,.176f,.204f,1);
  if (ic) ajIcone(ic,656,y+40,30,.72f);
  float vw = val && val[0] ? 260 : 0;
  aj4Texto(rot, foco ? TXT_G30B : TXT_G30M,tx,y+20,1750-tx-vw-(vw?24:0),1);
  if (sub && sub[0]) aj4Texto(sub,TXT_V2_26,tx,y+65,1750-tx-vw-(vw?24:0),.65f);
  if (vw) {
    TxtLinha t = ajTxtC(TXT_V2_26,val,260);
    txt_desenhar_alpha(t,1750-t.w,y+24,.72f);
  }
  if (selo && selo[0]) {
    if (aj4SeloAviso) {
      TxtLinha t=txt_linha(TXT_V2_24,selo,232,176,92,255);
      txt_desenhar_alpha(t,1750-t.w,y+68,.85f);
    } else {
      TxtLinha t=ajTxtC(TXT_V2_24,selo,260);
      txt_desenhar_alpha(t,1750-t.w,y+68,.55f);
    }
  }
  ajIcone(marcado ? "aj_check" : "aj_chevron-right",1778,y+44,26,foco?1:.5f);
  int row=(int)((y-(aj4Util==3?400:387))/(aj4Util==3?120:123));
  if(uxEditor && uxOp>=0)aj4Alvo(r,4,uxPendente/4*4+row);
  else aj4Alvo(r,1,aj4Linha/4*4+row);
}
static void aj4Pagina(const char *titulo, const char *sub) {
  ajFundo();
  gfx_cor((GfxRect){0,0,NV_VTELA_W,NV_VTELA_H},0,.035f,.04f,.045f,.76f);
  aj4Texto("NUVIO",TXT_G30B,72,62,160,1);
  aj4Texto("Sinta-se em casa.",TXT_V2_24,224,71,600,.65f);
  // Same island material, with no decorative rim on the settings surface.
  GfxRect island={56,132,1808,900};
  if(ajVid())gfx_vidro_miolo(island,30.0f/900,.055f,.059f,.071f,.94f,1);
  else gfx_cor(island,30.0f/900,.071f,.075f,.086f,.99f);
  if (!focoIndice || aj4Util || uxEditor || pfTela) {
    aj4Botao("", NULL,100,174,60,0,0);
    ajIcone("aj_chevron-left",117,190,26,1);
    aj4Texto(titulo,TXT_V2_A3TIT,182,173,915,1);
  } else aj4Texto(titulo,TXT_V2_A3TIT,100,173,1000,1);
  aj4Botao("Buscar", "aj_search",1060,174,164,aj4Topo==0,0);
  ajNeutro((GfxRect){1236,174,310,58},29,.05f,.05f,.055f,.06f,1);
  aj4Botao("Simples",NULL,1241,179,132,aj4Topo==1 && !lig(AJ_AVANCADAS),!lig(AJ_AVANCADAS));
  aj4Botao("Avançado",NULL,1377,179,164,aj4Topo==1 && lig(AJ_AVANCADAS),lig(AJ_AVANCADAS));
  char perfil[160]; pfRotuloChip(perfil,sizeof perfil);
  { char *u = strstr(perfil, "Perfil: "); if (u) memmove(perfil, u + 8, strlen(u + 8) + 1); }
  aj4Botao(perfil,"aj_user-round",1558,174,262,aj4Topo==2,0);
  aj4Texto(sub,TXT_V2_26,100,270,1170,.65f);
  const char *modo = lig(AJ_AVANCADAS) ? "Todos os ajustes, organizados" : "Escolhas do dia a dia";
  TxtLinha t = ajTxt(TXT_V2_26,modo); float ar,ag,ab; ajustes_acento_marca(&ar,&ag,&ab);
  t = txt_linha(TXT_V2_26,modo,(int)(ar*255),(int)(ag*255),(int)(ab*255),255);
  txt_desenhar(t,1820-t.w,270);
  gfx_cor((GfxRect){100,962,1720,1},0,.20f,.21f,.24f,1);
  aj4Texto(uxEditor ? "↑ ↓  Escolher     OK  Aplicar     Voltar  Cancelar" : "↑ ↓  Navegar     OK  Abrir     Voltar  Retornar",TXT_V2_24,100,986,1040,.72f);
  const char *status = uxAviso[0] ? uxAviso : uxEditor ? "A prévia não muda o valor salvo" : "As mudanças só são aplicadas após OK";
  t = ajTxtC(TXT_V2_24,status,700); txt_desenhar_alpha(t,1820-t.w,986,.55f);
}
static void aj4Lateral(const char *kick, const char *titulo, const char *corpo) {
  aj4Texto(kick,TXT_V2_KICK,100,332,470,.55f);
  ajBloco(TXT_ILHA_PERGUNTA,titulo,100,372,470,44,1,2);
  ajBloco(TXT_V2_28,corpo,100,480,470,37,.65f,4);
  ajIcone("aj_info",100,898,28,.6f);
  aj4Texto("Cada opção mostra onde se aplica.",TXT_V2_24,142,902,430,.65f);
}
static void aj4Home(void) {
  int av = lig(AJ_AVANCADAS);
  aj4Pagina("Ajustes",av ? "O modo avançado mantém todas as opções disponíveis." : "Uma configuração confortável para todos da casa.");
  aj4Lateral(av?"CATÁLOGO COMPLETO":"COMECE AQUI",av?"Cada escolha.\nEm seu lugar.":"Menos ajustes.\nMais diversão.",
              av?"Os mesmos ajustes e valores, organizados pelo que você quer fazer.":"Escolha o que quer mudar. Os detalhes técnicos ficam no Avançado.");
  gfx_cor((GfxRect){100,582,470,1},0,.22f,.23f,.26f,1);
  aj4Texto("SUA CONFIGURAÇÃO",TXT_V2_KICK,100,620,470,.55f);
  char aj4Predef[160]; aj4PredefRotulo(aj4Predef,sizeof aj4Predef);
  aj4Texto(aj4Predef,TXT_V2_26,100,670,470,.85f);
  ajBloco(TXT_V2_26,av?"Trocar de modo mantém suas escolhas salvas.":"Experimente uma configuração pronta para esta TV.",100,724,470,32,.65f,2);
  aj4Botao("Predefinições","aj_sparkles",100,806,320,aj4Linha==4 && aj4Topo<0,0);
  if (av) {
    aj4Botao("Assistir",NULL,622,323,180,0,!aj4Set);
    aj4Botao("Personalizar",NULL,806,323,220,0,aj4Set);
    aj4Texto("← →  Trocar grupo",TXT_V2_24,1480,343,340,.6f);
  } else aj4Texto("O QUE VOCÊ QUER MUDAR?",TXT_V2_24,622,343,1000,.55f);
  for (int i=0;i<4;i++) {
    int s=av?aj4Set*4+i:i, ico=av?s:(i==2?5:i==3?6:i);
    const char *rot=av?AJ4_SECOES[s]:AJ4_TAREFAS[i];
    const char *sub=av?AJ4_SUB[s]:i==2?"Estilo da tela inicial, texto e movimento":i==3?"Perfis, addons e contas conectadas":AJ4_SUB[i];
    aj4LinhaDesenhar(rot,sub,"","",AJ4_IC[ico],387+i*123,aj4Linha==i && aj4Topo<0,0);
  }
  aj4Botao("Diferente do padrão","aj_rotate-ccw-clock",622,895,365,aj4Linha==5 && aj4Topo<0,0);
  aj4Botao("Ajuda desta TV","aj_info",1003,895,290,aj4Linha==6 && aj4Topo<0,0);
  if(aj4UndoN)aj4Botao("Desfazer","aj_rotate-ccw-clock",1310,895,220,aj4Linha==7 && aj4Topo<0,0);
  aj4Alvo((GfxRect){100,806,320,58},1,4);
  aj4Alvo((GfxRect){622,895,365,58},1,5);
  aj4Alvo((GfxRect){1003,895,290,58},1,6);
  if(aj4UndoN)aj4Alvo((GfxRect){1310,895,220,58},1,7);
}
static void aj4SecaoDesenhar(void) {
  int av=lig(AJ_AVANCADAS) || aj4Excecao>=0, sec=aj4Atual;
  const char *titulo=av?AJ4_SECOES[sec]:AJ4_TAREFAS[sec];
  aj4Pagina(titulo,"Escolha um grupo e avance para seus ajustes.");
  if(av) {
    int gs[AJ4_N_GRUPOS], n=aj4GruposLista(sec,gs), start=0;
    for(int i=0;i<n;i++) if(gs[i]==aj4Grupo) start=i/4*4;
    if(n<=4) aj4Mais=0;
    if(!aj4Mais && (aj4GrupoFoco<start || aj4GrupoFoco>=start+4)) aj4GrupoFoco=start;
    aj4Texto("NESTA SEÇÃO",TXT_V2_KICK,100,332,470,.55f);
    aj4Texto("Um grupo por vez.",TXT_G30B,100,372,470,1);
    ajBloco(TXT_V2_26,"Escolha um grupo e avance para seus ajustes.",100,420,470,32,.65f,2);
    for(int i=start;i<n && i<start+4;i++) {
      float y=510+(i-start)*76;
      if(aj4Grupos && i==aj4GrupoFoco && !aj4Mais && aj4Topo<0)ajNeutro((GfxRect){100,y,470,68},18,.13f,.169f,.176f,.204f,1);
      if(gs[i]==aj4Grupo) { float ar,ag,ab;ajustes_acento_marca(&ar,&ag,&ab);gfx_cor((GfxRect){120,y+29,7,7},.5f,ar,ag,ab,1); }
      aj4Texto(AJ4_GRUPOS[gs[i]],TXT_V2_26,142,y+20,385,.8f);
      ajIcone("aj_chevron-right",522,y+22,26,.7f);
      aj4Alvo((GfxRect){100,y,470,68},2,i);
    }
    char b[96];snprintf(b,sizeof b,i18n("%s · %d grupos"),titulo,n);
    aj4Texto(b,TXT_V2_24,100,832,470,.55f);
    if(n>4) {
      // Mock "More groups" textbutton: pages the group list forward.
      GfxRect r={100,850,320,50};
      if(aj4Mais && aj4Topo<0)ajNeutro(r,14,.13f,.169f,.176f,.204f,1);
      TxtLinha t=ajTxtC(TXT_V2_24,"Mais grupos",250);
      txt_desenhar_alpha(t,116,850+(50-t.h)*.5f,aj4Mais?.9f:.6f);
      ajIcone("aj_chevrons-down",128+t.w,850+(50-26)*.5f,26,aj4Mais?.95f:.5f);
      aj4Alvo(r,2,999);
    }
    aj4Texto(AJ4_GRUPOS[aj4Grupo],TXT_V2_24,622,343,1000,.6f);
  } else {
    aj4Lateral("ESCOLHAS DO DIA A DIA","Do seu jeito.","Escolha uma opção. OK abre a prévia; Voltar mantém o valor atual.");
    aj4Texto("SEUS AJUSTES",TXT_V2_24,622,343,1000,.6f);
  }
  int n=aj4Lista(sec,av?aj4Grupo:-1,aj4Opcoes), start=aj4Linha/4*4;
  if(aj4Linha>=n)aj4Linha=n?n-1:0;
  for(int i=start;i<n && i<start+4;i++) {
    int op=aj4Opcoes[i]; char val[160];uxValorTexto(op,uxValorAtual(op),val,sizeof val);
    if(op==AJ_FONTE_MANUAL)snprintf(val,sizeof val,"%s",i18n(valor[op]==0?"Eu escolho":"Nuvio"));
    const char *selo=escopoDe(op)==ESC_TV?"Esta TV":escopoDe(op)>ESC_TV?"Este perfil":"";
    aj4LinhaDesenhar(aj4Rotulo(op),aj4Ajuda(op),val,selo,NULL,387+(i-start)*123,
                     !aj4Grupos && !aj4Acao && aj4Topo<0 && i==aj4Linha,0);
  }
  char b[80];snprintf(b,sizeof b,i18n("%d–%d de %d"),n?start+1:0,n<start+4?n:start+4,n);
  aj4Texto(b,TXT_V2_24,1620,343,200,.65f);
  aj4Botao("Revisar restauração","aj_rotate-ccw-clock",622,895,365,aj4Acao && aj4Topo<0,0);
  aj4Botao(aj4Escopo?"Esta TV":"Este perfil",NULL,1003,895,230,0,1);
  aj4Texto("Cada opção mostra onde se aplica.",TXT_V2_24,100,902,470,.6f);
}
static void aj4UtilDesenhar(void) {
  int review=aj4Review, preset=aj4Util==2;
  const char *title=review?"Revisar mudanças":preset?"Predefinições":"Diferente do padrão";
  aj4Pagina(title,review?"Confira valores e escopos antes de aplicar.":preset?"Configurações prontas, com prévia antes de aplicar.":"Compare suas escolhas com o padrão de fábrica.");
  int n=review?aj4N:preset?3:aj4Diferencas(aj4Opcoes), start=aj4Linha/4*4;
  int misto=0;
  if(review) { int tv=0,pr=0; for(int i=0;i<aj4N;i++) escopoDe(aj4Ops[i])==ESC_TV?(tv=1):(pr=1); misto=tv&&pr; }
  aj4SeloAviso=misto;
  char esc[160];
  if(review) {
    aj4Lateral("ANTES DE APLICAR","Você decide.","Somente as opções listadas serão alteradas. Contas e credenciais ficam preservadas.");
    if(misto) {
      // Mock .warning: mixed-scope review tells who else sees the changes.
      gfx_cor((GfxRect){100,620,470,1},0,.22f,.23f,.26f,1);
      txt_bloco_corta(TXT_V2_26,i18n("Esta predefinição afeta seu perfil e esta TV. Quem usar esta TV pode ver as mudanças."),232,176,92,100,642,470,32,.85f,3);
    }
  } else if(preset) aj4Lateral("UM BOM COMEÇO","Pronta para assistir.","Escolha uma configuração e revise as mudanças antes de confirmar.");
  else {
    // Mock: the scope selector lives in the left column as group buttons,
    // not a toggle row in the main area.
    char nome[160]; pfRotuloChip(nome,sizeof nome);
    { char *u=strstr(nome,"Perfil: "); if(u) memmove(u,u+8,strlen(u+8)+1); }
    if(aj4Escopo) snprintf(esc,sizeof esc,"%s",i18n("Esta TV"));
    else if(!nome[0] || !strcmp(nome,i18n("Perfil"))) snprintf(esc,sizeof esc,"%s",i18n("Este perfil"));
    else snprintf(esc,sizeof esc,i18n("Este perfil · %s"),nome);
    char tit[80]; snprintf(tit,sizeof tit,i18n("%d alterações.\nSem adivinhação."),n);
    aj4Texto("UM CAMINHO DE VOLTA",TXT_V2_KICK,100,332,470,.55f);
    ajBloco(TXT_ILHA_PERGUNTA,tit,100,372,470,44,1,2);
    ajBloco(TXT_V2_26,"Compare valores salvos com os padrões deste escopo.",100,470,470,32,.65f,2);
    for(int i=0;i<2;i++) {
      float y=510+i*76;
      if(i==aj4Escopo) { float ar,ag,ab;ajustes_acento_marca(&ar,&ag,&ab);gfx_cor((GfxRect){120,y+29,7,7},.5f,ar,ag,ab,1); }
      aj4Texto(i?"Esta TV":esc,TXT_V2_26,142,y+20,385,i==aj4Escopo?.85f:.6f);
      ajIcone("aj_chevron-right",522,y+22,26,.7f);
      aj4Alvo((GfxRect){100,y,470,68},6,i);
    }
    gfx_cor((GfxRect){100,676,470,1},0,.22f,.23f,.26f,1);
    ajBloco(TXT_V2_26,"Restaurar ajustes não remove complementos, acesso nem histórico.",100,698,470,32,.65f,2);
    ajIcone("aj_info",100,898,28,.6f);
    aj4Texto("Cada ajuste alterado mostra onde se aplica.",TXT_V2_24,142,902,430,.65f);
  }
  if(!review && !preset) {
    aj4Texto(esc,TXT_V2_24,622,343,900,.6f);
    char b[64]; snprintf(b,sizeof b,i18n("%d alterações"),n);
    aj4Texto(b,TXT_V2_24,1620,343,200,.65f);
  } else aj4Texto(review?"ATUAL / NOVO VALOR":"ESCOLHA UMA CONFIGURAÇÃO",TXT_V2_24,622,343,1100,.6f);
  static const char *const desc[]={"Prioriza a qualidade das fontes disponíveis.","Reduz qualidade, tamanho de arquivo e trailers automáticos.","Reduz efeitos e mantém a memória de imagens automática."};
  for(int i=start;i<n && i<start+4;i++) {
    int op=review?aj4Ops[i]:preset?-1:aj4Opcoes[i];
    char val[350]="", sub[350]="";
    if(op>=0) {
      char old[160], novo[160];
      uxValorTexto(op,review?aj4Antes[i]:uxValorAtual(op),old,sizeof old);
      uxValorTexto(op,review?aj4Depois[i]:uxValorPadrao(op),novo,sizeof novo);
      snprintf(val,sizeof val,"%s",old);snprintf(sub,sizeof sub,i18n("%s · %s"),i18n(review?"Novo valor":"Padrão"),novo);
    }
    aj4LinhaDesenhar(preset && !review?AJ4_PRESET_NOMES[i]:aj4Rotulo(op),preset && !review?desc[i]:sub,val,
      op>=0?(escopoDe(op)==ESC_TV?"Esta TV":"Este perfil"):"",preset && !review?"aj_sparkles":NULL,
      387+(i-start)*123,aj4Linha==i && !aj4Acao && aj4Topo<0,0);
  }
  if(!n)ajBloco(TXT_V2_28,"Nenhuma mudança em relação ao padrão neste escopo.",646,430,1100,40,.75f,3);
  aj4Botao(review?"Aplicar mudanças":aj4UndoN?"Desfazer":"Revisar restauração","aj_rotate-ccw-clock",622,895,365,aj4Acao==1,0);
  if(review)aj4Botao("Manter ajustes",NULL,1003,895,280,aj4Acao==2,0);
  aj4Alvo((GfxRect){622,895,365,58},3,1);
  if(review)aj4Alvo((GfxRect){1003,895,280,58},3,2);
}
static void aj4Editor(void) {
  // Specialized stateful editors retain their existing native implementation.
  if(uxEditor!=1 || ajIptvMenu(uxOp) || uxOp==AJ_GUIA_CATEGORIAS || ajPaleta(uxOp) || uxRestaurar || uxAvisoRisco) {
    ajDesenharTela(); return;
  }
  int op=uxOp;
  aj4Pagina(aj4Rotulo(op),"OK aplica. Voltar mantém o valor atual.");
  aj4Lateral("ESCOLHA UM VALOR",op==AJ_QUALIDADE?"Quanto detalhe?":aj4Rotulo(op),aj4Ajuda(op));
  char atual[160];uxValorTexto(op,uxOriginal,atual,sizeof atual);
  aj4Texto("Valor atual",TXT_V2_24,100,680,470,.6f);
  aj4Texto(atual,TXT_V2_28,100,722,470,1);
  aj4Texto(escopoDe(op)==ESC_TV?"Esta TV":"Este perfil",TXT_V2_24,100,780,470,.6f);
  if(OPCOES[op].tipo==OP_NUMERO) {
    char val[160];uxValorTexto(op,uxPendente,val,sizeof val);
    aj4LinhaDesenhar(val,"← →  Alterar     OK  Aplicar","","",NULL,387,!uxRodape,0);
  } else {
    int n=nValores(op), start=uxPendente/4*4;
    for(int i=start;i<n && i<start+4;i++) {
      char val[160];uxValorTexto(op,i,val,sizeof val);
      aj4LinhaDesenhar(val,i==uxOriginal?"Valor atual":"OK para aplicar","","",i==uxOriginal?"aj_check":NULL,
                      387+(i-start)*123,i==uxPendente && !uxRodape,i==uxOriginal);
    }
  }
  aj4Botao("Restaurar padrão","aj_rotate-ccw-clock",622,895,365,uxRodape,0);
}
static void aj4BuscaDesenhar(void) {
  aj4Pagina("Buscar ajuste","Encontre qualquer opção, inclusive do Avançado.");
  aj4Lateral("VÁ DIRETO AO PONTO","Sem procurar menus.","Busque pelo que quer mudar ou pelo nome técnico do ajuste.");
  aj4Botao(aj4Consulta[0]?aj4Consulta:"Buscar ajuste","aj_search",622,323,1198,0,0);
  int n=ajustes_buscar(aj4Consulta,aj4Resultados,AJ_N),start=aj4Linha/4*4;
  for(int i=start;i<n && i<start+4;i++) {
    AjusteBuscaResultado *r=&aj4Resultados[i];
    aj4LinhaDesenhar(aj4Rotulo(r->op),r->caminho,r->valor,
      escopoDe(r->op)==ESC_TV?"Esta TV":"Este perfil",r->avancado?"aj_sliders-horizontal":r->icone,
      400+(i-start)*120,i==aj4Linha && !aj4Acao,0);
  }
  if(!n)ajBloco(TXT_V2_28,"Nenhum ajuste encontrado. Tente áudio ou qualidade.",646,430,1100,40,.75f,3);
  aj4Botao("Buscar novamente","aj_search",622,895,365,aj4Acao || !n,0);
  aj4Alvo((GfxRect){622,895,365,58},3,1);
}
static void aj4PerfilDesenhar(void) {
  PfLinha lines[PF_MAX];int n=pfLinhas(lines),start=pfFoco/4*4;
  aj4Pagina("Perfil e conta","Os ajustes desta TV afetam todos que a utilizam.");
  char nome[160];pfRotuloChip(nome,sizeof nome);
  aj4Lateral("PERFIL ATIVO",nome,"Edite, copie ou restaure ajustes do perfil com os controles existentes.");
  aj4Texto("SEUS AJUSTES",TXT_V2_24,622,343,1000,.6f);
  for(int i=start;i<n && i<start+4;i++)
    aj4LinhaDesenhar(lines[i].rot,lines[i].apoio,"","Este perfil","aj_user-round",387+(i-start)*123,i==pfFoco,0);
}
static void aj4Desenhar(void) {
  ponteiro_camada();
  aj4SeloAviso=0;
  if(pfTela==PF_MENU)aj4PerfilDesenhar();
  else if(uxEditor)aj4Editor();
  else if(aj4Util==3)aj4BuscaDesenhar();
  else if(aj4Review || aj4Util)aj4UtilDesenhar();
  else if(focoIndice)aj4Home();
  else aj4SecaoDesenhar();
}
