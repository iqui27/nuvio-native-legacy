// Approved Mac Sources core. Data, filtering, focus, details and playback stay shared.
static void folhaDesktopAcao(int action) {
  if(action==200) { aberta=0; menu_ir(MENU_FONTES); return; }
  if(action>=100) { ponteiroFolhaFiltro(action-101,0); return; }
  ponteiroFolhaBotao(action,0);
  SDL_Event event={0}; event.type=SDL_KEYDOWN; event.key.keysym.sym=SDLK_RETURN;
  stream_folha_evento(&event);
}
static void folhaDesktopGerenciar(int a,int b) { (void)a;(void)b;folhaDesktopAcao(200); }
// pary < .5 draws a continuous decorative border, outside the focus-ring heuristic.
static void folhaDesktopBorda(GfxRect r,float radius,float width,float alpha) {
  gfx_rect(r,0,GFX_ANEL,0,width/r.h,.1f,radius,1,1,1,alpha);
}
static void folhaDesktopTag(const char *name,float *x,float y,float right,float alpha) {
  TxtLinha t=txt_linha(TXT_MINI,name,225,226,229,255);
  float w=t.w+12,h=20;
  if(*x+w>right) return;
  gfx_cor((GfxRect){*x,y,w,h},.16f,1,1,1,.07f*alpha);
  folhaDesktopBorda((GfxRect){*x,y,w,h},.16f,1,.14f*alpha);
  txt_desenhar_alpha(t,*x+6,y+(h-t.h)*.5f,alpha);
  *x+=w+22;
}
static void folhaDesktopLinha(const Stream *s,int i,GfxRect r,int sel,float a,Uint32 now) {
  const float x=r.x+24,right=r.x+r.w-24;
  char provider[220],size[40]="",title[240],ep[64];
  legendaProvedor(s,provider,sizeof provider);
  if(i==melhorFolha && nFiltrados()>1) {
    size_t len=strlen(provider);snprintf(provider+len,sizeof provider-len," · %s",i18n("Escolha automática"));
  }
  unsigned long long mb=tamanhoMBFonte(s);
  if(mb) { snprintf(size,sizeof size,"%.1f GB",mb/1024.0);plrui_decimal(size); }
  gfx_cor(r,28/r.h,sel?.15f:.085f,sel?.16f:.09f,sel?.18f:.10f,(sel?.90f:.68f)*a);
  folhaDesktopBorda(r,28/r.h,1.2f,(sel?.38f:.25f)*a);
  TxtLinha sz=txt_linha(TXT_PG_FIM,size,164,165,168,255);
  txt_desenhar_alpha(sz,right-sz.w,r.y+16,a);
  txt_desenhar_alpha(txt_linha_corta(TXT_PG_FIM,provider,164,165,168,255,r.w-80-sz.w),x,r.y+16,a);
  float cy=r.y+54;
  if(temMarca(i,automaticaDaFolha()) && (i==atual || i==preferida || fitPesada(i) || s->foraCache || stream_vod_falhou_ha(i)>=0)) {
    const char *mark=i==atual?"Reproduzindo agora":i==preferida?"Sua escolha anterior":fitPesada(i)?"Acima da conexão":s->foraCache?"Fora do cache":"Indisponível";
    plrui_kicker(mark,x,cy,198,195,188,a);cy+=FOLHA_MARCA_H;
  }
  tituloConteudo(s,title,sizeof title,ep,sizeof ep);
  const char *c1=strchr(alvoLista,':'),*c2=c1?strchr(c1+1,':'):NULL;
  if(c1 && c2) {
    const CatItem *item=itemFolha>=0?cat_item(itemFolha):NULL;
    char shortEp[80];snprintf(shortEp,sizeof shortEp," · T%d:E%d",atoi(c1+1),atoi(c2+1));
    size_t len=strlen(title);snprintf(title+len,sizeof title-len,"%s%s%s",shortEp,item&&item->nomeEpisodio[0]?" · ":"",item?item->nomeEpisodio:"");
  }
  if(ajustes_fonte_texto_addon()) {
    blocoAddon(s,x,cy,right-x,sel?235:215,a);
    return;
  }
  const CatItem *item=ajustes_fonte_texto_logo() && itemFolha>=0?cat_item(itemFolha):NULL;
  const char *logoURL=item?logotitulo_url(item,FOLHA_LOGO_W):NULL;
  GLuint logo=logoURL?tex_obter_larg_qualquer(logoURL,FOLHA_LOGO_W):0;
  float aspect=logoURL?tex_aspecto(logoURL):0;
  if(logo && aspect>.01f) {
    float h=40,w=fminf(right-x,h*aspect);h=w/aspect;
    gfx_rect((GfxRect){x,cy,w,h},logo,tex_marca_escura(logoURL)?GFX_MARCA:GFX_TEXTO,0,0,0,0,1,1,1,a);
  } else txt_desenhar_alpha(txt_linha_corta(TXT_CALLOUT,title,243,242,239,255,right-x),x,cy,a);
  cy+=48;
  float tx=x;
  char res[16],range[16];int hdr;
  rotuloQualidade(s,res,sizeof res,range,sizeof range,&hdr);
  folhaDesktopTag(res,&tx,cy,right,a);
  if(selospacote_ativo()>=0 || ajustes_selos_coloridos()) {
    if(selosPacoteVisiveis((Stream *)s)) desenharSelosPacote(s,tx,cy,right-tx,20,a,.85f,1);
    else badges_desenhar_selos(logosDa(s,0),tx,cy,right-tx,20,a);
  } else {
  if(temNoTexto(s,"remux")) folhaDesktopTag("REMUX",&tx,cy,right,a);
  else if(temNoTexto(s,"web-dl") || temNoTexto(s,"webdl")) folhaDesktopTag("WEB-DL",&tx,cy,right,a);
  if(hdr) folhaDesktopTag(s->dolbyVision?"DOLBY VISION":range,&tx,cy,right,a);
  if(temNoTexto(s,"hevc") || temNoTexto(s,"x265")) folhaDesktopTag("HEVC",&tx,cy,right,a);
  if(temNoTexto(s,"atmos")) folhaDesktopTag("ATMOS",&tx,cy,right,a);
  if(!strcmp(containerDa(s),"MP4")) folhaDesktopTag("MP4",&tx,cy,right,a);
  uint64_t extra=logosDa(s,0);
  const char *shown[]={"q-remux","q-webdl","v-dv","v-hdr10plus","v-hdr10","v-hdr","v-hlg","co-x265","a-atmos"};
  for(size_t k=0;k<sizeof shown/sizeof *shown;k++) extra&=~badges_bit(shown[k]);
  if(extra) badges_desenhar_tom(extra,tx,cy,right-tx,20,.85f,.85f,.85f,a);
  }
  cy+=FOLHA_SELO_H+12;
  cy+=desenharFaixas(s,x,cy,right-x,sel,a);
  float expanded=sel?abreFoco:0;
  if(expanded>.02f) {
    float aw=FOLHA_W-FOLHA_PAD_E-FOLHA_PAD_D-2*FOLHA_TXT;
    const char *file=arquivoFolha(s,aw);
    if(file[0]) { arquivoDesenhar(file,x,cy,aw,a*expanded);cy+=arquivoAltura(file,aw); }
    char line[160],line2[128];fitTexto2(i,line,sizeof line,line2,sizeof line2);
    if(line[0]) txt_desenhar_alpha(txt_linha_corta(TXT_PG_FIM,line,160,160,158,255,right-x),x,cy,a*expanded);
    if(line2[0]) txt_desenhar_alpha(txt_linha_corta(TXT_PG_FIM,line2,160,160,158,255,right-x),x,cy+FOLHA_FIT_H,a*expanded);
  }
}
static void folhaDesktopDesenhar(Uint32 now) {
  if(anim<.005f) { desktop_native_sources(NULL,0,NULL,0,0,0,0,0,0);return; }
  folhaGeometria();
  GfxRect panel=folhaDesktopRect();
  panel.y+=(1-anim)*20;
  float x=panel.x+42,right=panel.x+panel.w-42,w=right-x;
  int ptr=aberta && anim>.5f && !detAberto && !ptAberto;
  gfx_cor((GfxRect){0,0,NV_TELA_W,NV_TELA_H},0,0,0,0,.28f*anim);
  gfx_vidro_folha(panel,32/panel.h,anim);
  gfx_cor(panel,32/panel.h,.095f,.094f,.097f,.92f*anim);
  folhaDesktopBorda(panel,32/panel.h,1.3f,.24f*anim);
  if(ptr) {
    ponteiro_alvo(0,0,NV_TELA_W,NV_TELA_H,NULL,ponteiroFolhaFora,0,0);
    ponteiro_alvo(panel.x,panel.y,panel.w,panel.h,NULL,NULL,0,0);
    ponteiro_rolagem(toqueFontesRolar);
  }
  toquerol_vincular(&toqueFontes,(GfxRect){x,FOLHA_TOPO,w,areaLista()},gfx_escala(),0,
                    alturaTotal-areaLista()+40,1,&rolagem);
  plrui_kicker(contexto[0]?contexto:nomeFolha,x,panel.y+38,243,242,239,anim*.55f);
  txt_desenhar_alpha(txt_linha(TXT_ILHA_TITULO,"Fontes",243,242,239,255),x,panel.y+74,anim);
  DesktopSourceControl controls[32]={0};int count=0;
  float filterX=42,extraX=42;
  for(int i=0;i<nBotoes() && count<16;i++) {
    int b=botaoDe(i),filter=b==BT_SO_MP4 || b==BT_CACHE || b==BT_DUB;
    DesktopSourceControl *c=&controls[count++];
    c->action=i;c->enabled=ptr && (b!=BT_RECARREGAR_FONTE || recarregarFontePode());c->selected=botaoLigado(b);
    c->x=filter?filterX:b==BT_FECHAR?panel.w-68:b==BT_RECARREGAR?panel.w-248:extraX;
    c->y=filter?148:b==BT_FECHAR?36:b==BT_RECARREGAR?78:panel.h-68;
    c->w=b==BT_FECHAR?32:filter?150:b==BT_RECARREGAR?120:160;c->h=filter?42:32;
    c->kind=filter?1:b==BT_FECHAR?2:0;
    snprintf(c->title,sizeof c->title,"%s",i18n(b==BT_RECARREGAR?"Atualizar":b==BT_FECHAR?"Fechar":rotuloBotao(b)?rotuloBotao(b):"Recarregar"));
    if(filter) filterX+=c->w+18;else if(b!=BT_FECHAR && b!=BT_RECARREGAR) extraX+=c->w+12;
    if(ptr) ponteiro_alvo(panel.x+c->x,panel.y+c->y,c->w,c->h,ponteiroFolhaBotao,NULL,i,0);
  }
  folhaDados();
  float tabX=42,available=panel.w-84;
  int tabs=nProvedores+1;
  float tabW=fminf(250,(available-18*(tabs-1))/tabs);
  // Overflow providers retain the shared keyboard selection; show the selected range.
  int start=-1,shown=(int)((available+18)/148);if(shown<1)shown=1;
  if(tabs>shown) { start=fmaxf(-1,tabPosition(filtro)-shown+1);tabW=(available-18*(shown-1))/shown; }
  for(int p=start;p<nProvedores && count<30 && p<start+shown;p++) {
    int tab=tabAt(p);
    DesktopSourceControl *c=&controls[count++];
    c->x=tabX;c->y=212;c->w=tabW;c->h=42;c->action=101+tab;c->kind=1;c->selected=filtro==tab;c->enabled=ptr;
    if(tab==0) snprintf(c->title,sizeof c->title,"%s · %d",i18n("Todos"),folhaContagens[0]);
    else snprintf(c->title,sizeof c->title,"%s",tab<0?i18n("Onde ver"):provedores[tab]);
    if(ptr) ponteiro_alvo(panel.x+c->x,panel.y+c->y,c->w,c->h,NULL,ponteiroFolhaFiltro,tab,0);
    tabX+=tabW+18;
  }
  DesktopSourceControl *manage=&controls[count++];
  manage->x=extraX;manage->y=panel.h-86;manage->w=192;manage->h=46;
  manage->action=200;manage->enabled=ptr;manage->kind=3;snprintf(manage->title,sizeof manage->title,"%s",i18n("Gerenciar add-ons"));
  if(ptr) ponteiro_alvo(panel.x+manage->x,panel.y+manage->y,manage->w,manage->h,NULL,folhaDesktopGerenciar,0,0);
  unsigned native=desktop_native_sources(controls,count,folhaDesktopAcao,panel.x,panel.y,panel.w,panel.h,
                                        gfx_escala(),anim);
  if(native) gfx_rect(panel,native,GFX_TEXTO,0,0,0,0,1,1,1,anim);
  const char *hint=fraseDicaChips();
  if(hint) {
    GfxRect r={x,FOLHA_TOPO-folhaDica+4,w,folhaDica-6};
    txt_bloco(TXT_HERO_META,hint,190,190,188,r.x,r.y,r.w,28,anim,0);
    if(ptr) ponteiro_alvo(r.x,r.y,r.w,r.h,ponteiroFolhaDica,NULL,0,0);
  }
  if(folhaSala>0) stream_folha_sala_desenhar(x,FOLHA_TOPO-folhaSala,w,anim);
  int automatic=automaticaDaFolha();melhorFolha=automaticoDaFolhaModo();montar(automatic);
  GfxRecorteAnt clip=gfx_recorte_dentro(x-2,FOLHA_TOPO-2,w+4,areaLista()+4);
  for(int g=0;g<FOLHA_GRUPOS;g++) if(secN[g]) {
    float y=FOLHA_TOPO+secTopo(g)-rolagem;
    const char *name=GRUPO_SUB[g/2][0]?GRUPO_SUB[g/2]:GRUPO_NOME[g/2];
    plrui_kicker(name,x,y+4,243,242,239,anim*.8f);
    char text[48];snprintf(text,sizeof text,i18n(secN[g]==1?"%d fonte":"%d fontes"),secN[g]);
    TxtLinha t=txt_linha(TXT_PG_FIM,text,152,152,150,255);txt_desenhar_alpha(t,right-t.w,y+4,anim);
  }
  for(int g=0;g<nServerGroups;g++) {
    float y=FOLHA_TOPO+serverGroupY[g]-rolagem;
    plrui_kicker(lista[serverGroupIndex[g]].provedor,x,y+4,243,242,239,anim*.8f);
  }
  for(int row=primeiraVisivel(rolagem-8);row<nOrdem;row++) {
    GfxRect r={x,FOLHA_TOPO+linhaTopo(row)-rolagem,w,linhaAltura(row)};
    if(r.y>folhaBase) break;if(r.y+r.h<FOLHA_TOPO) continue;
    int i=ordem[row],sel=grupo==1 && foco==row;
    if(ptr) { float top=fmaxf(r.y,FOLHA_TOPO),bottom=fminf(r.y+r.h,folhaBase);
      if(bottom>top) ponteiro_alvo(r.x,top,r.w,bottom-top,ponteiroFolhaLinha,NULL,row,0); }
    if(i>=0) folhaDesktopLinha(&lista[i],i,r,sel,anim,now);
    else if(i<=-2) { OndeVer o;if(ondever_item(alvoPedido,-i-2,&o))desenharOnde(r,x+24,right-24,&o,sel,anim); }
  }
  if(!nOrdem) {
    const char *message=fraseVazioAba(filtro);
    if(filtro==-1) {
      int state=ondever_status(alvoPedido);
      message=state==ONDE_BUSCANDO?"Buscando onde assistir…":state==ONDE_FALHOU?
          "Não foi possível consultar a disponibilidade.":"Nenhum serviço informado para esta região.";
    }
    if(filtro!=-1 && fraseVazioChips()) message=fraseVazioChips();
    if(!message) message=addons_ocupado()?"Buscando fontes nos addons…":"Nenhuma fonte direta disponível. Use Recarregar para tentar novamente.";
    txt_bloco(TXT_PG_FIM,message,196,199,204,x,FOLHA_TOPO+30,w,28,anim,0);
  }
  if(nOrdem && filtro!=-1 && dicaTorrentsAltura>0) {
    char text[1536];
    if(dicaTorrents(text,sizeof text)) txt_bloco(TXT_PG_FIM,text,150,150,148,x,
        FOLHA_TOPO+alturaTotal-dicaTorrentsAltura-rolagem+12,w,28,anim,0);
  }
  gfx_recorte_voltar(clip);
  if(alturaTotal>areaLista()) {
    float h=areaLista()*fminf(1,areaLista()/alturaTotal),travel=areaLista()-h;
    gfx_cor((GfxRect){right+18,FOLHA_TOPO+travel*fmaxf(0,fminf(1,rolagem/fmaxf(1,alturaTotal-areaLista()))),6,h},.5f,.75f,.75f,.74f,anim*.65f);
  }
  if(temServidor()) txt_desenhar_alpha(txt_linha_corta(TXT_PG_FIM,"A cópia do seu servidor aparece primeiro quando disponível.",150,153,162,255,w),x,folhaBase+12,anim);
  if(detAnim>.01f) desenhaDetalhe(panel.x,panel.w,aberta);
  if(ptAnim>.01f) desenhaPartes(panel.x,panel.w,aberta&&ptAberto,now);
}
