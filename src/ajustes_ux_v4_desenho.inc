// One island, four visible rows, shared geometry across the v4 screens.
// Mirror placement only; glyphs and artwork retain their original orientation.
static GfxRect aj4Rect(GfxRect r) { r.x = aj4X(r.x, r.w); return r; }
static void aj4Draw(TxtLinha t,float x,float y) { txt_desenhar(t,aj4X(x,t.w),y); }
static void aj4DrawAlpha(TxtLinha t,float x,float y,float a) { txt_desenhar_alpha(t,aj4X(x,t.w),y,a); }
static void aj4Cor(GfxRect r,float raio,float cr,float cg,float cb,float ca) { gfx_cor(aj4Rect(r),raio,cr,cg,cb,ca); }
static void aj4Vidro(GfxRect r,float raio,float cr,float cg,float cb,float ca,float a) { gfx_vidro_miolo(aj4Rect(r),raio,cr,cg,cb,ca,a); }
static void aj4Neutro(GfxRect r,float raio,float va,float cr,float cg,float cb,float a) { ajNeutro(aj4Rect(r),raio,va,cr,cg,cb,a); }
static void aj4Pilula(GfxRect r,float raio,float a) { ajPilulaAcento(aj4Rect(r),raio,a); }
static const char *aj4IconeNome(const char *s) {
  if (aj4RTL()) {
    if (!strcmp(s,"aj_chevron-right")) return "aj_arrow-left";
    if (!strcmp(s,"pl_chevron-left")) return "aj_arrow-right";
  }
  return s;
}
static void aj4Icone(const char *s,float x,float y,float lado,float a) { ajIcone(aj4IconeNome(s),aj4X(x,lado),y,lado,a); }
static void aj4IconeT(const char *s,float x,float y,float lado,float a) { ajIconeT(aj4IconeNome(s),aj4X(x,lado),y,lado,a); }
static float aj4Bloco(TxtEstilo e,const char *s,float x,float y,float w,float lh,float a,int maxL) {
  return aj4RTL() ? txt_bloco_dir(e,s,243,242,239,NV_VTELA_W-x,y,w,lh,a,maxL) : ajBloco(e,s,x,y,w,lh,a,maxL);
}
#define txt_desenhar aj4Draw
#define txt_desenhar_alpha aj4DrawAlpha
#define gfx_cor aj4Cor
#define gfx_vidro_miolo aj4Vidro
#define ajNeutro aj4Neutro
#define ajPilulaAcento aj4Pilula
#define ajIcone aj4Icone
#define ajIconeT aj4IconeT
#define ajBloco aj4Bloco
static void aj4PontAtivar(int zona,int row) {
  if(zona==5) { uxEvento(SDLK_ESCAPE); return; }
  if(zona==8) { ajPenRow=row;int handled=ajPenEvent(SDLK_RETURN);if(!handled) { SDL_Event event={0};event.type=SDL_KEYDOWN;event.key.keysym.sym=SDLK_RETURN;eventoTela(&event); } return; }
  if(zona==10) { aj4Buscar();return; }
  if(zona==9) { if(row==1) { uxRodape=1;uxEvento(SDLK_RETURN); }else if(row==2)uxNotificar("Salvar como preset ainda não está disponível.");return; }
  aj4PontFoco(zona,row);
  if(zona==4)uxAplicar();
  else if(zona==7) { snprintf(aj4Consulta,sizeof aj4Consulta,"%s",i18n(row==2?"Áudio":"Fontes")); aj4Acao=aj4Linha=0; }
  else aj4Evento(SDLK_RETURN);
}
static void aj4Alvo(GfxRect r,int zona,int row) {
  r = aj4Rect(r);
  if(!pfTela && !teclado_aberto())ponteiro_alvo(r.x,r.y,r.w,r.h,aj4PontFoco,aj4PontAtivar,zona,row);
}
static void aj4Texto(const char *s, TxtEstilo e, float x, float y, float w, float a) {
  txt_desenhar_alpha(ajTxtC(e, s, w), x, y, a);
}
static void aj4Botao(const char *s, const char *ic, float x, float y, float w, int foco, int ativo) {
  int segmento=y==179;
  GfxRect r = {x,y,w,segmento?48:58};
  if (foco) ajPilulaAcento(r,29,1);
  else if(!segmento || ativo)ajNeutro(r,29,ativo ? 0.16f : 0.07f, ativo ? .20f : .14f, ativo ? .21f : .15f, ativo ? .24f : .17f,1);
  if (ic) { if (foco) ajIconeT(ic,x+22,y+15,28,1); else ajIcone(ic,x+22,y+15,28,.8f); }
  float textoW = w - (ic ? 72 : segmento ? 24 : 28);
  TxtLinha t = foco ? txt_linha_corta(TXT_V2_26,s,ajTinta(),ajTinta(),ajTinta(),255,textoW) : ajTxtC(TXT_V2_26,s,textoW);
  txt_desenhar(t,x+(ic?64:segmento?(w-t.w)*.5f:20),y+(r.h-t.h)*.5f);
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
  float tx = ic && ic[0] ? 714 : 646;
  if (foco) ajNeutro(r,21,.13f,.169f,.176f,.204f,1);
  if (ic) ajIcone(ic,656,y+40,30,.72f);
  float vw = val && val[0] ? 260 : 0;
  aj4Texto(rot, foco ? TXT_G30B : TXT_G30M,tx,y+20,1750-tx-vw-(vw?24:0),1);
  if (sub && sub[0]) {
    if(strchr(sub,'\n'))ajBloco(TXT_V2_24,sub,tx,y+54,1750-tx-vw-(vw?24:0),27,.72f,2);
    else aj4Texto(sub,TXT_V2_24,tx,y+65,1750-tx-vw-(vw?24:0),.72f);
  }
  if (vw) {
    TxtLinha t = ajTxtC(TXT_V2_24,val,260);
    txt_desenhar_alpha(t,1750-t.w,y+(marcado?42:24),.72f);
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
  if(marcado) {
    TxtLinha t=ajTxtC(foco?TXT_G30B:TXT_G30M,rot,850);
    float ar,ag,ab; ajustes_acento_marca(&ar,&ag,&ab);
    txt_desenhar(txt_linha(TXT_V2_24," · Atual",ar*255,ag*255,ab*255,255),tx+t.w+14,y+28);
  }
  ajIcone("aj_chevron-right",1778,y+44,26,foco?1:.5f);
  int row=(int)((y-(aj4Util==3 && !uxEditor?432:387))/(aj4Util==3 && !uxEditor?113:123));
  if(uxEditor && uxOp>=0)aj4Alvo(r,4,uxPendente/4*4+row);
  else aj4Alvo(r,1,aj4Linha/4*4+row);
}
static void aj4Pagina(const char *titulo, const char *sub) {
  ajFundo();
  gfx_cor((GfxRect){0,0,NV_VTELA_W,NV_VTELA_H},0,.035f,.04f,.045f,.76f);
  aj4Texto("NUVIO",TXT_G30B,72,62,160,1);
  aj4Texto("Sinta-se em casa.",TXT_V2_24,224,71,600,.65f);
  char hora[48];ajCenaHora(hora,sizeof hora);
  TxtLinha relogio=ajTxt(TXT_V2_28,hora), sala=ajTxt(TXT_V2_24,"Sala");
  txt_desenhar(relogio,1848-relogio.w,64);
  txt_desenhar_alpha(sala,1824-relogio.w-sala.w,71,.65f);
  // Same island material, with no decorative rim on the settings surface.
  GfxRect island={56,132,1808,900};
  if(ajVid())gfx_vidro_miolo(island,30.0f/900,.055f,.059f,.071f,.94f,1);
  else gfx_cor(island,30.0f/900,.071f,.075f,.086f,.99f);
  if (!focoIndice || aj4Util || uxEditor || pfTela) {
    aj4Botao("", NULL,100,174,60,0,0);
    ajIcone("pl_chevron-left",117,190,26,1);
    aj4Texto(titulo,TXT_V2_A3TIT,182,173,915,1);
  } else aj4Texto(titulo,TXT_V2_A3TIT,100,173,1000,1);
  aj4Botao("Buscar", "aj_search",1060,174,164,aj4Topo==0,0);
  ajNeutro((GfxRect){1236,174,310,58},29,.05f,.05f,.055f,.06f,1);
  aj4Botao("Simples",NULL,1241,179,132,aj4Topo==1 && !lig(AJ_AVANCADAS),!lig(AJ_AVANCADAS));
  aj4Botao("Avançado",NULL,1377,179,164,aj4Topo==1 && lig(AJ_AVANCADAS),lig(AJ_AVANCADAS));
  char perfil[160];
  if(edPerfil)pfRotuloChip(perfil,sizeof perfil);
  else snprintf(perfil,sizeof perfil,"%s",pfNome(perfis_ativo()));
  aj4Botao(perfil,"aj_user-round",1558,174,262,aj4Topo==2,0);
  aj4Texto(sub,TXT_V2_26,100,270,1170,.65f);
  const char *modo = lig(AJ_AVANCADAS) ? "Todos os ajustes, organizados" : "Escolhas do dia a dia, mais simples";
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
  char escopo[200];
  if(focoIndice && !aj4Util && !uxEditor)
    snprintf(escopo,sizeof escopo,i18n("Perfil ativo: %s"),pfNome(edPerfil?edPerfil:perfis_ativo()));
  else snprintf(escopo,sizeof escopo,"%s",i18n("Cada opção mostra onde se aplica."));
  aj4Texto(escopo,TXT_V2_24,142,902,428,.65f);
}
static void aj4Acento(const char *s,float x,float y,float w) {
  float r,g,b; ajustes_acento_marca(&r,&g,&b);
  txt_desenhar(txt_linha_corta(TXT_V2_24,s,r*255,g*255,b*255,255,w),x,y);
}
static void aj4Amostras(float bottom) {
  static const float xs[]={104,224,374}, ws[]={102,132,178}, hs[]={70,96,132};
  static const char *const nomes[]={"720p","1080p","4K"};
  float r,g,b; ajustes_acento_marca(&r,&g,&b);
  for(int i=0;i<3;i++) {
    gfx_cor((GfxRect){xs[i],bottom-hs[i],ws[i],hs[i]},12/hs[i],r*.22f,g*.22f,b*.22f,1);
    if(i==2)gfx_cor((GfxRect){xs[i],bottom-hs[i],ws[i],hs[i]},12/hs[i],r*.30f,g*.30f,b*.30f,1);
    TxtLinha t=ajTxtC(TXT_V2_24,nomes[i],ws[i]); txt_desenhar_alpha(t,xs[i]+(ws[i]-t.w)/2,bottom-(hs[i]+t.h)/2,i==2?1:.7f);
    gfx_cor((GfxRect){xs[i]+(ws[i]-32)/2,bottom+7,32,5},.5f,.56f,.57f,.59f,1);
  }
}
static void aj4Home(void) {
  int av = lig(AJ_AVANCADAS);
  aj4Pagina("Ajustes",av ? "O modo avançado mantém todas as opções disponíveis." : "Uma configuração confortável para todos da casa.");
  aj4Lateral(av?"CATÁLOGO COMPLETO":"COMECE AQUI",av?"Cada escolha.\nEm seu lugar.":"Menos ajustes.\nMais diversão.",
              av?"Os mesmos ajustes e valores, organizados pelo que você quer fazer.":"Escolha o que quer mudar. Os detalhes técnicos ficam no Avançado.");
  gfx_cor((GfxRect){100,582,470,1},0,.22f,.23f,.26f,1);
  aj4Texto("SUA CONFIGURAÇÃO",TXT_V2_KICK,100,620,470,.55f);
  char aj4Predef[160]; aj4PredefRotulo(aj4Predef,sizeof aj4Predef);
  aj4Acento(aj4Predef,100,670,470);
  ajBloco(TXT_V2_26,av?"Trocar de modo mantém suas escolhas salvas.":"Experimente uma configuração pronta para esta TV.",100,724,470,32,.65f,2);
  aj4Botao("Predefinições","aj_sparkles",100,806,320,aj4Linha==4 && aj4Topo<0,0);
  char contexto[200];
  if (av) {
    aj4Botao("Assistir",NULL,622,323,180,0,!aj4Set);
    aj4Botao("Personalizar",NULL,806,323,220,0,aj4Set);
    aj4Texto("← →  Trocar grupo",TXT_V2_24,1480,343,340,.6f);
  } else {
    aj4Texto("O QUE VOCÊ QUER MUDAR?",TXT_V2_24,622,343,850,.55f);
    const char *tarefas="4 tarefas do dia a dia";
    float largura=txt_largura(TXT_V2_24,tarefas);
    aj4Acento(tarefas,1820-largura,343,largura+1);
  }
  for (int i=0;i<4;i++) {
    int s=av?aj4Set*4+i:i, ico=av?s:(i==2?5:i==3?6:i);
    const char *rot=av?AJ4_SECOES[s]:AJ4_TAREFAS[i];
    const char *sub=av?AJ4_SUB[s]:i==2?"Estilo da tela inicial, texto e movimento":i==3?"Perfis, addons e contas conectadas":AJ4_SUB[i];
    aj4LinhaDesenhar(rot,sub,"","",AJ4_IC[ico],387+i*123,aj4Linha==i && aj4Topo<0,0);
  }
  snprintf(contexto,sizeof contexto,i18n("Diferente do padrão · %d"),aj4Diferencas(aj4Opcoes));
  aj4Botao(contexto,"aj_rotate-ccw-clock",622,895,410,aj4Linha==5 && aj4Topo<0,0);
  aj4Botao("Ajuda desta TV","aj_info",1048,895,260,aj4Linha==6 && aj4Topo<0,0);
  aj4Botao("O que o Nuvio faz?","aj_sparkles",1324,895,300,aj4Linha==7 && aj4Topo<0,0);
  if(aj4UndoN)aj4Botao("Desfazer","aj_rotate-ccw-clock",1640,895,180,aj4Linha==8 && aj4Topo<0,0);
  aj4Alvo((GfxRect){100,806,320,58},1,4);
  aj4Alvo((GfxRect){622,895,410,58},1,5);
  aj4Alvo((GfxRect){1048,895,260,58},1,6);
  aj4Alvo((GfxRect){1324,895,300,58},1,7);
  if(aj4UndoN)aj4Alvo((GfxRect){1640,895,180,58},1,8);
}
static void aj4SecaoDesenhar(void) {
  int av=lig(AJ_AVANCADAS) || aj4Excecao>=0, sec=aj4Atual;
  const char *titulo=av?AJ4_SECOES[sec]:AJ4_TAREFAS[sec];
  int fontes=av && !aj4Grupos && aj4Grupo==aj4GrupoDe(AJ_FONTE_MANUAL);
  char caminho[256];
  snprintf(caminho,sizeof caminho,i18n("Ajustes / %s · %s"),i18n(titulo),i18n(AJ4_GRUPOS[aj4Grupo]));
  aj4Pagina(titulo,fontes?"Ajustes / Reprodução · Seleção de fontes":caminho);
  if(fontes) {
    aj4Texto("SELEÇÃO DE FONTES",TXT_V2_KICK,100,332,470,.55f);
    ajBloco(TXT_ILHA_PERGUNTA,"Aperte Assistir.\nO Nuvio faz o resto.",100,372,470,44,1,2);
    ajBloco(TXT_V2_26,"Decida quem escolhe o vídeo e o que priorizar na seleção automática.",100,480,470,37,.65f,3);
    aj4Amostras(748);
    ajBloco(TXT_V2_24,"A qualidade depende do arquivo, da conexão e desta TV.",100,790,470,32,.65f,2);
    aj4Texto("SELEÇÃO AUTOMÁTICA",TXT_V2_24,622,343,900,.6f);
  } else if(av) {
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
    aj4Texto("NESTA SEÇÃO",TXT_V2_KICK,100,332,470,.55f);
    aj4Texto("Um grupo por vez.",TXT_ILHA_PERGUNTA,100,372,470,1);
    ajBloco(TXT_V2_26,"Escolha um grupo e avance para seus ajustes.",100,436,470,37,.65f,3);
    gfx_cor((GfxRect){100,538,470,1},0,.22f,.23f,.26f,1);
    ajBloco(TXT_V2_24,"Quatro escolhas do dia a dia.",100,604,470,32,.65f,2);
    ajBloco(TXT_V2_24,"Mude para Avançado para ver o catálogo completo.",100,650,470,32,.65f,3);
    ajIcone("aj_info",100,898,28,.6f);
    aj4Texto("Cada opção mostra onde se aplica.",TXT_V2_24,142,902,428,.65f);
    aj4Texto(AJ4_GRUPOS[aj4Grupo],TXT_V2_24,622,343,1000,.6f);
  }
  int n=aj4Lista(sec,av?aj4Grupo:-1,aj4Opcoes), start=aj4Linha/4*4;
  if(aj4Linha>=n)aj4Linha=n?n-1:0;
  for(int i=start;i<n && i<start+4;i++) {
    int op=aj4Opcoes[i]; char val[160];uxValorTexto(op,uxValorAtual(op),val,sizeof val);
    if(op==AJ_FONTE_MANUAL)snprintf(val,sizeof val,"%s",i18n(valor[op]==0?"Eu escolho":"Nuvio"));
    const char *selo=escopoDe(op)==ESC_TV?"Esta TV":escopoDe(op)>ESC_TV?"Este perfil":"";
    char dependencia[400];
    const char *ajuda=aj4Ajuda(op);
    if(inativa(op)) {
      ajAvisoTexto(op,dependencia,sizeof dependencia);
      char *acao=strstr(dependencia," — ");
      if(acao) { size_t separador=strlen(" — "); memmove(acao+1,acao+separador,strlen(acao+separador)+1); *acao='\n'; }
      ajuda=dependencia;
    }
    aj4LinhaDesenhar(aj4Rotulo(op),ajuda,val,selo,NULL,387+(i-start)*123,
                     !aj4Grupos && !aj4Acao && aj4Topo<0 && i==aj4Linha,0);
  }
  char b[80];snprintf(b,sizeof b,i18n("%d–%d de %d"),n?start+1:0,n<start+4?n:start+4,n);
  aj4Texto(b,TXT_V2_24,1620,343,200,.65f);
  float bx=622;
  const char *rotulos[]={"Itens anteriores","Próximos itens","Revisar restauração",aj4Escopo?"Esta TV":"Este perfil"};
  const char *icones[]={"aj_arrow-up","aj_arrow-down","aj_rotate-ccw-clock",NULL};
  const int acoes[]={2,3,1,4};
  for(int i=0;i<4;i++) {
    if((i==0 && !start) || (i==1 && start+4>=n))continue;
    float w=txt_largura(TXT_V2_24,i18n(rotulos[i]))+(icones[i]?64:40);
    GfxRect r={bx,895,w,58};int foco=aj4Acao==acoes[i] && aj4Topo<0;
    if(foco)ajPilulaAcento(r,29,1);
    else if(i==3)ajNeutro(r,29,.16f,.20f,.21f,.24f,1);
    if(icones[i]) { if(foco)ajIconeT(icones[i],bx+16,910,26,1);else ajIcone(icones[i],bx+16,910,26,.65f); }
    TxtLinha t=foco?txt_linha(TXT_V2_24,rotulos[i],ajTinta(),ajTinta(),ajTinta(),255):ajTxt(TXT_V2_24,rotulos[i]);
    txt_desenhar_alpha(t,bx+(icones[i]?54:20),895+(58-t.h)*.5f,foco?1:.7f);
    aj4Alvo(r,3,acoes[i]);bx+=w+12;
  }
  if(av)aj4Texto("Cada opção mostra onde se aplica.",TXT_V2_24,100,902,470,.6f);
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
  } else if(preset) {
    aj4Texto("ENCONTRE SEU EQUILÍBRIO",TXT_V2_KICK,100,332,470,.55f);
    ajBloco(TXT_ILHA_PERGUNTA,"O que importa\npara esta TV?",100,372,470,44,1,2);
    ajBloco(TXT_V2_26,"Escolha um ponto de partida. Ajuste os detalhes depois.",100,480,470,37,.65f,3);
    gfx_cor((GfxRect){100,610,470,1},0,.22f,.23f,.26f,1);
    aj4Texto("CONFIGURAÇÃO ATUAL",TXT_V2_KICK,100,646,470,.55f);
    char b[160]; aj4PredefRotulo(b,sizeof b); aj4Acento(b,100,688,470);
    ajBloco(TXT_V2_24,"Predefinições não alteram complementos, chaves nem idioma do áudio.",100,738,470,32,.65f,3);
    aj4Texto("Cada prévia mostra o perfil ou a TV afetada.",TXT_V2_24,100,902,470,.6f);
  }
  else {
    // Mock: the scope selector lives in the left column as group buttons,
    // not a toggle row in the main area.
    char nome[160];snprintf(nome,sizeof nome,"%s",pfNome(edPerfil?edPerfil:perfis_ativo()));
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
    ajBloco(TXT_V2_24,"Cada ajuste alterado mostra onde se aplica.",142,882,428,29,.65f,2);
  }
  if(!review && !preset) {
    int sec=start<n?aj4Secao(aj4Opcoes[start]):-1;
    const char *grupo=sec<0?"":sec<2?"Reprodução e idioma":AJ4_SECOES[sec];
    for(int i=start;i<n && i<start+4;i++) {
      int s=aj4Secao(aj4Opcoes[i]);
      if((sec<2 && s>=2) || (sec>=2 && sec!=s))grupo="Ajustes";
    }
    char cab[240]; snprintf(cab,sizeof cab,i18n("%s · %s"),i18n(aj4Escopo?"Esta TV":"Este perfil"),i18n(grupo));
    ajCapsRGB(TXT_V2_24,cab,1,622,343,.6f,243,242,239);
    char b[64]; snprintf(b,sizeof b,i18n("%d alterações"),n);
    aj4Texto(b,TXT_V2_24,1620,343,200,.65f);
  } else {
    aj4Texto(review?"PRÉVIA DA MUDANÇA":"TRÊS PONTOS DE PARTIDA",TXT_V2_24,622,343,950,.6f);
    if(preset && !review)aj4Acento("Nenhuma alteração",1550,343,270);
  }
  static const char *const desc[]={"Priorizar até 4K e melhor qualidade de imagem.","Priorizar 720p, limitar tamanho e desativar trailers automáticos.","Imagens leves, menos movimento e painéis sólidos."};
  for(int i=start;i<n && i<start+4;i++) {
    int op=review?aj4Ops[i]:preset?-1:aj4Opcoes[i];
    char val[350]="", sub[350]="";
    if(op>=0) {
      char old[160], novo[160];
      uxValorTexto(op,review?aj4Antes[i]:uxValorAtual(op),old,sizeof old);
      uxValorTexto(op,review?aj4Depois[i]:uxValorPadrao(op),novo,sizeof novo);
      if(review)snprintf(sub,sizeof sub,"%s → %s",old,novo);
      else { snprintf(val,sizeof val,"%s",old);snprintf(sub,sizeof sub,i18n("%s · %s"),i18n("Padrão"),novo); }
    }
    const char *selo=op>=0?(escopoDe(op)==ESC_TV?"Esta TV":"Este perfil"):"";
    if(preset && !review) {
      int tv,pr,count=aj4PresetContar(i,&tv,&pr);
      snprintf(val,sizeof val,i18n("%d alterações"),count);
      selo=tv&&pr?"Perfil + TV":tv?"Só nesta TV":pr?"Este perfil":"Nenhuma alteração";
    }
    aj4LinhaDesenhar(preset && !review?AJ4_PRESET_NOMES[i]:aj4Rotulo(op),preset && !review?desc[i]:sub,val,
      selo,preset && !review?"aj_sparkles":NULL,
      387+(i-start)*123,aj4Linha==i && !aj4Acao && aj4Topo<0,0);
  }
  if(!n)ajBloco(TXT_V2_28,"Nenhuma mudança em relação ao padrão neste escopo.",646,430,1100,40,.75f,3);
  if(preset && !review) {
    ajIcone("aj_info",622,810,28,.7f);
    aj4Acento("Economizar dados é uma preferência, sem cota mensal.",664,810,1156);
    aj4Texto("TV leve mantém a qualidade do vídeo e a memória Automática.",TXT_V2_24,622,858,1198,.65f);
    return;
  }
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
  int qualidade=op==AJ_QUALIDADE;
  aj4Pagina(aj4Rotulo(op),qualidade?"Ajustes / Reprodução · Imagem e som":"OK aplica. Voltar mantém o valor atual.");
  char atual[160];uxValorTexto(op,uxOriginal,atual,sizeof atual);
  if(qualidade) {
    aj4Texto("ESCOLHA UM VALOR",TXT_V2_KICK,100,332,470,.55f);
    aj4Texto("Quanto detalhe?",TXT_ILHA_PERGUNTA,100,372,470,1);
    ajBloco(TXT_V2_26,"Preferência das fontes de vídeo. Não altera a resolução dos menus.",100,437,470,37,.65f,3);
    aj4Amostras(718);
    char b[240];snprintf(b,sizeof b,i18n("Atual: %s"),atual); ajIcone("aj_check",100,774,26,1);aj4Acento(b,142,774,428);
    snprintf(b,sizeof b,i18n("Este perfil: %s"),pfNome(edPerfil?edPerfil:perfis_ativo()));
    ajIcone("aj_info",100,898,28,.6f);aj4Texto(b,TXT_V2_24,142,902,428,.65f);
    aj4Texto("OK APLICA · VOLTAR MANTÉM O VALOR ATUAL",TXT_V2_24,622,343,1050,.6f);
  } else {
    aj4Lateral("ESCOLHA UM VALOR",aj4Rotulo(op),aj4Ajuda(op));
    aj4Texto("Valor atual",TXT_V2_24,100,680,470,.6f);
    aj4Texto(atual,TXT_V2_28,100,722,470,1);
    aj4Texto(escopoDe(op)==ESC_TV?"Esta TV":"Este perfil",TXT_V2_24,100,780,470,.6f);
  }
  if(OPCOES[op].tipo==OP_NUMERO) {
    char val[160];uxValorTexto(op,uxPendente,val,sizeof val);
    aj4LinhaDesenhar(val,"← →  Alterar     OK  Aplicar","","",NULL,387,!uxRodape,0);
  } else {
    int n=nValores(op), start=uxPendente/4*4;
    for(int i=start;i<n && i<start+4;i++) {
      char val[160];uxValorTexto(op,i,val,sizeof val);
      static const char *const ajuda[]={"Deixe o Nuvio escolher sem limitar a resolução.","Priorizar até 4K quando TV e conexão permitirem.","Boa imagem com arquivos menores que a maioria das fontes 4K.","Use fontes menores para economizar dados."};
      aj4LinhaDesenhar(val,qualidade?ajuda[i]:i==uxOriginal?"Valor atual":"OK para aplicar",i==uxOriginal?"Atual":"","",i==uxOriginal?"aj_check":NULL,
                      387+(i-start)*123,i==uxPendente && !uxRodape,i==uxOriginal);
    }
  }
  if(qualidade && !uxRodape)aj4Texto("Escolhas manuais podem exceder esta preferência. A disponibilidade das fontes varia.",TXT_V2_24,622,902,1198,.65f);
  else aj4Botao("Restaurar padrão","aj_rotate-ccw-clock",622,895,365,uxRodape,0);
}
static void aj4BuscaDesenhar(void) {
  aj4Pagina("Buscar ajuste","Encontre qualquer opção, inclusive do Avançado.");
  aj4Lateral("VÁ DIRETO AO PONTO","Encontre sem procurar nos menus.","Busque pelo que quer mudar ou pelo nome técnico do ajuste.");
  gfx_cor((GfxRect){100,582,470,1},0,.22f,.23f,.26f,1);
  aj4Texto("EXPERIMENTE UMA TAREFA",TXT_V2_KICK,100,620,470,.55f);
  aj4Botao("Áudio",NULL,100,666,150,aj4Acao==2,0); aj4Alvo((GfxRect){100,666,150,58},7,2);
  aj4Botao("Fontes",NULL,266,666,166,aj4Acao==3,0); aj4Alvo((GfxRect){266,666,166,58},7,3);
  ajBloco(TXT_V2_24,"Resultados avançados abrem direto. Voltar retorna à busca.",100,760,470,32,.65f,3);
  aj4Botao(aj4Consulta[0]?aj4Consulta:"Qualidade, legendas, regras de fontes…","aj_search",622,323,1198,0,0);
  int n=aj4BuscarResultados(),start=aj4Linha/4*4;
  char meta[80];snprintf(meta,sizeof meta,i18n("%d resultados"),n);
  aj4Texto(aj4Consulta[0]?meta:"Sugestões · todos os modos",TXT_V2_24,622,391,1198,.55f);
  for(int i=start;i<n && i<start+4;i++) {
    AjusteBuscaResultado *r=&aj4Resultados[i];
    aj4LinhaDesenhar(aj4Rotulo(r->op),r->caminho,r->avancado?"Avançado":"",
      escopoDe(r->op)==ESC_TV?"Esta TV":"Este perfil",r->avancado?"aj_sliders-horizontal":r->icone,
      432+(i-start)*113,i==aj4Linha && !aj4Acao,0);
  }
  if(!n)ajBloco(TXT_V2_28,"Nenhum ajuste encontrado. Tente áudio ou qualidade.",646,430,1100,40,.75f,3);
  aj4Botao("Buscar novamente","aj_search",622,895,365,aj4Acao || !n,0);
  aj4Alvo((GfxRect){622,895,365,58},3,1);
}
static void aj4PerfilDesenhar(void) {
  aj4Pagina("Perfil e escopo","Saiba quem será afetado antes de aplicar um ajuste.");
  const char *nome=pfNome(edPerfil?edPerfil:perfis_ativo());
  aj4Texto("PERFIL ATIVO",TXT_V2_KICK,100,332,470,.55f);
  aj4Texto(nome,TXT_ILHA_PERGUNTA,100,372,470,1);
  ajBloco(TXT_V2_26,"Ajustes de perfil acompanham a pessoa. Ajustes da TV afetam todos aqui.",100,437,470,37,.65f,3);
  ajNeutro((GfxRect){100,580,104,104},52,.12f,.17f,.20f,.23f,1); ajIcone("aj_user-round",128,608,48,.8f);
  aj4Texto("ESTADO DO SALVAMENTO",TXT_V2_KICK,100,716,470,.55f);
  aj4Acento(uxAviso[0]?uxAviso:"Salvo nesta TV",100,758,470);
  char estado[240];snprintf(estado,sizeof estado,i18n("Sincronização da conta: %s"),textoValor(AJ_SYNC));
  ajBloco(TXT_V2_24,estado,100,806,470,32,.65f,2);
  ajBloco(TXT_V2_24,"Ajustes da TV são compartilhados entre perfis.",100,882,470,29,.65f,2);
  aj4Texto("ESCOLHAS DO PERFIL",TXT_V2_24,622,343,1000,.6f);
  static const char *const nomes[]={"Trocar perfil","Idiomas preferidos","Contas conectadas","Ajustes para todos nesta TV"};
  static const char *const desc[]={"Escolha outra pessoa sem alterar os ajustes da TV.","Escolhas de áudio e legenda do perfil ativo.","Gerenciar sincronização com Trakt, Simkl e Nuvio.","Compatibilidade de imagem, memória e efeitos dos menus."};
  static const char *const ic[]={"aj_user-round","aj_languages","aj_cloud","aj_monitor-cog"};
  char audio[160];uxValorTexto(AJ_AUD_LINGUA,valor[AJ_AUD_LINGUA],audio,sizeof audio);
  for(int i=0;i<4;i++)aj4LinhaDesenhar(nomes[i],desc[i],i==0?nome:i==1?audio:i==2?"Ver estado":"Ajustes do aparelho",i==0?"":i==3?"Esta TV":"Este perfil",ic[i],387+i*123,i==aj4Linha && !aj4Acao && aj4Topo<0,0);
  aj4Botao("Gerenciar perfis","aj_user-round",622,895,365,aj4Acao,0);aj4Alvo((GfxRect){622,895,365,58},3,1);
}
#include "ajustes_pen_desenho.inc"
#include "ajustes_pen_editor.inc"
#include "ajustes_pen_util.inc"
static void aj4Desenhar(void) {
  ponteiro_camada();
  aj4SeloAviso=0;
  if(uxEditor)ajPenEditor();
  else if(aj4Util==4)aj4PerfilDesenhar();
  else if(aj4Util==3 || (aj4Util==1 && !aj4Review))ajPenUtility();
  else if(aj4Review || aj4Util)aj4UtilDesenhar();
  else if(!focoIndice && ajPenCurrent())ajPenRows();
  else if(focoIndice)ajPenHome();
  else aj4SecaoDesenhar();
}
#undef txt_desenhar
#undef txt_desenhar_alpha
#undef gfx_cor
#undef gfx_vidro_miolo
#undef ajNeutro
#undef ajPilulaAcento
#undef ajIcone
#undef ajIconeT
#undef ajBloco
