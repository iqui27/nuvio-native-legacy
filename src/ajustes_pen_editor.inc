/* Pencil choice page; edits use the existing draft and persistence path. */
static void ajPenEditor(void) {
  if(uxEditor!=1 || ajIptvMenu(uxOp) || uxOp==AJ_GUIA_CATEGORIAS || (ajPaleta(uxOp) && uxOp!=AJ_TEMA) || uxRestaurar || uxAvisoRisco) {
    aj4Editor();return;
  }
  int op=uxOp;
  int audio=ajPenPage==27 || ajPenPage==28 || ajPenPage==AJ_PEN_SECONDARY;
  int screen=op==AJ_ESCALA_TEXTO || op==AJ_TEMA;
  int quality=op==AJ_QUALIDADE;
  const char *section=audio?"Áudio e legendas":screen?"Minha tela":"Reprodução";
  const char *caption=op==AJ_LEG_LINGUA2?"Uma tradução por cima da principal, para quando a original não basta.":op==AJ_FONTE_MANUAL?"As fontes deste título. O Nuvio pergunta antes de trocar de uma para a outra.":op==AJ_ESCALA_TEXTO?"Escala do texto: as letras do app inteiro, sem mexer no layout.":op==AJ_TEMA?"A cor do foco, dos botões e das marcas do app.":quality?"Automática deixa a TV pedir o melhor que ela aguenta.":op==AJ_AUD_LINGUA?"Escolha a faixa que você prefere. A original continua na lista — nada é escondido.":aj4Ajuda(op);
  ajPenBase();
  ajPenChip(section,"pl_chevron-left",200,112,audio?208:screen?150:164,0,5,0);
  ajKicker(section,200,198,1);
  const char *title=op==AJ_AUD_LINGUA?"Idioma do áudio":op==AJ_LEG_LINGUA2?"Segunda legenda":op==AJ_ESCALA_TEXTO?"Tamanho do texto":quality?"Qualidade do vídeo":op==AJ_FONTE_MANUAL?"Escolha do vídeo":aj4Rotulo(op);
  aj4Texto(title,TXT_TITULO2,200,212,1500,1);
  aj4Texto(caption,TXT_G20M,200,291,1520,.58f);
  ajPenScope();
  const char *scopeHelp=op==AJ_AUD_LINGUA?"Vale para a sala, não para o celular":op==AJ_LEG_LINGUA2?"Não empurra a legenda principal":op==AJ_ESCALA_TEXTO?"A escala vale nesta TV; o celular segue a conta":op==AJ_TEMA?"A cor vale nesta TV; o celular segue a conta":quality?"A qualidade é pedida à fonte a cada reprodução":"O que está em Privado só você vê";
  TxtLinha scope=ajTxt(TXT_AJ_MINI14,scopeHelp);txt_desenhar_alpha(scope,1824-scope.w,178,.4f);
  ajPenPanel(200,344,520,640,0);ajPenPanel(744,344,1080,640,0);
  ajKicker(quality?"COMO FUNCIONA":op==AJ_ESCALA_TEXTO?"ESCALA":op==AJ_TEMA?"COR DE DESTAQUE":op==AJ_LEG_LINGUA2?"SEGUNDA LEGENDA":op==AJ_FONTE_MANUAL?"COMO ESCOLHE":"PREFERÊNCIA",226,366,1);
  char original[160];uxValorTexto(op,uxOriginal,original,sizeof original);
  const char *heads[]={quality?"Automática":op==AJ_ESCALA_TEXTO?"Escala do texto":op==AJ_TEMA?"Cor de destaque":op==AJ_AUD_LINGUA?"Preferir faixa original":op==AJ_LEG_LINGUA2?"Idioma da segunda legenda":op==AJ_FONTE_MANUAL?"Escolher na mão":"Valor atual",quality?"Limite desta TV":screen?op==AJ_TEMA?"Cor da logo":"Tamanho da interface":op==AJ_LEG_LINGUA2?"Posição":op==AJ_FONTE_MANUAL?"Ordem das fontes":"Faixa de reserva",quality?"O que o addon oferece":op==AJ_ESCALA_TEXTO?"Mínimo garantido":op==AJ_LEG_LINGUA2?"Estilo separado":op==AJ_FONTE_MANUAL?"Antes de abrir":"Onde isso vale"};
  int hdr,dv,hevc,av1;stream_capacidades(&hdr,&dv,&hevc,&av1);
  char tvLimit[160];snprintf(tvLimit,sizeof tvLimit,"HEVC: %s · AV1: %s",i18n(hevc<0?"Desconhecida":hevc?"Suportado":"Não suportado"),i18n(av1<0?"Desconhecida":av1?"Suportado":"Não suportado"));
  const char *body[]={quality?"A TV pede a melhor qualidade que ela aguenta. Quem limita é a rede.":op==AJ_AUD_LINGUA?"Quando existe original em boa qualidade, ele vem primeiro — sem dublado por cima.":op==AJ_LEG_LINGUA2?"O idioma da tradução extra. Desligada, ela não aparece.":op==AJ_FONTE_MANUAL?"A TV mostra as fontes e você escolhe com o D-pad.":op==AJ_ESCALA_TEXTO?"Muda o tamanho das letras do app inteiro, sem mexer no layout.":screen?caption:original,quality?tvLimit:op==AJ_AUD_LINGUA?"Se a escolhida não existir no título, tocamos a original.":op==AJ_LEG_LINGUA2?"No topo da tela ou junto da principal.":op==AJ_FONTE_MANUAL?"Por qualidade ou na ordem em que o addon montou.":screen?(op==AJ_TEMA?"A logo do app pode acompanhar a cor.":"Escala dos cartões e das listas. Não mexe nas letras."):aj4Ajuda(op),quality?"Cada addon diz a qualidade do arquivo que ele tem.":op==AJ_LEG_LINGUA2?"A segunda legenda não muda quando você muda a principal.":op==AJ_FONTE_MANUAL?"O Nuvio confere a fonte e avisa antes de trocar.":op==AJ_ESCALA_TEXTO?"Com 85% as legendas continuam legíveis a 3 metros.":audio || escopoDe(op)==ESC_TV?"Nesta TV e no app da sala. O celular segue a conta, não a TV.":"A escolha acompanha este perfil."};
  for(int i=0;i<(op==AJ_TEMA?2:3);i++) {
    float y=i==0?400:i==1?(op==AJ_TEMA?614:quality?564:584):quality?718:738;
    float h=i==0?(op==AJ_TEMA?200:quality?150:170):140;
    ajNeutro((GfxRect){226,y,468,h},22,i<2 && uxRodape==i+2?.14f:.05f,.10f,.11f,.13f,1);
    aj4Texto(heads[i],TXT_G21B,248,y+20,422,1);
    ajBloco(TXT_G18R,body[i],248,y+54,422,22,.58f,3);
    if(i==0 && (op==AJ_AUD_LINGUA || op==AJ_FONTE_MANUAL || quality)) {
      int setting=op==AJ_AUD_LINGUA?AJ_AUD_ORIGINAL:op;
      int on=op==AJ_AUD_LINGUA?ajustes_audio_original():quality?uxOriginal==0:uxPendente==0;
      float toggleY=quality?492:506;ajPenToggle(248,toggleY,on);
      aj4Texto(on?"Ligado":"Desligado",TXT_G18R,326,toggleY+7,300,.58f);
      aj4Alvo((GfxRect){248,toggleY,400,36},14,setting);
    }
    if(op==AJ_ESCALA_TEXTO && i==1)aj4Alvo((GfxRect){226,y,468,h},14,AJ_TAMANHO_UI);
    if(op==AJ_TEMA && i==1) {
      ajPenToggle(248,696,lig(AJ_COR_LOGO));aj4Texto(lig(AJ_COR_LOGO)?"Ligado":"Desligado",TXT_G18R,326,703,300,.58f);
      aj4Alvo((GfxRect){248,696,400,36},14,AJ_COR_LOGO);
    }
    if(screen && i==0) {
      if(op==AJ_ESCALA_TEXTO) {
        ajPenChip("−",NULL,248,512,44,0,11,-1);
        char draft[100];uxValorTexto(op,uxPendente,draft,sizeof draft);aj4Texto(draft,TXT_G21B,306,524,80,1);
        ajPenChip("+",NULL,375,512,44,0,11,1);
      } else {
        static const int colors[]={0,6,1,3,2,4,5,11};
        for(int j=0;j<8;j++) { float r,g,b;corDeHex(TEMA_ACENTO[colors[j]].fill,&r,&g,&b);gfx_cor((GfxRect){248+j*54,510,40,40},.5f,r,g,b,1); }
      }
    }
  }
  ajPenChip("Voltar ao padrão","aj_rotate-ccw",226,902,206,uxRodape==1 || uxRodape==4,9,1);
  if(!screen)ajPenChip("Salvar como preset","aj_check",444,902,230,uxRodape==5,9,2);
  ajKicker(quality?"QUALIDADE":op==AJ_TEMA?"CORES":op==AJ_ESCALA_TEXTO?"ESCALA DO TEXTO":op==AJ_AUD_LINGUA?"FAIXAS DISPONÍVEIS":"ESCOLHA UM VALOR",770,366,1);
  if(OPCOES[op].tipo==OP_NUMERO) {
    char value[160];uxValorTexto(op,uxPendente,value,sizeof value);
    aj4Texto(value,TXT_TITULO2,800,480,900,1);
    aj4Texto("← →  Alterar     OK  Aplicar",TXT_G20M,800,580,900,.65f);
  } else {
    static const int colors[]={6,1,2,3,4,5,0,7,8,9,10,11};
    int n=nValores(op),selected=uxPendente,languageOrder[LING_MAX_OPC];
    if(op==AJ_AUD_LINGUA) { n=ajPenLanguageOrder(languageOrder);for(int i=0;i<n;i++)if(languageOrder[i]==uxPendente)selected=i; }
    if(op==AJ_TEMA)for(int i=0;i<12;i++)if(colors[i]==uxPendente)selected=i;
    int start=selected/6*6;
    for(int pos=start;pos<n && pos<start+6;pos++) {
      int i=op==AJ_TEMA && pos<12?colors[pos]:op==AJ_AUD_LINGUA?languageOrder[pos]:pos;
      float y=400+(pos-start)*76;char value[160];uxValorTexto(op,i,value,sizeof value);
      if(i==uxPendente && !uxRodape)ajNeutro((GfxRect){770,y,1028,70},20,.14f,.169f,.176f,.204f,1);
      const char *languageNames[]={"Original","Português (Brasil)","English","Español","Français","日本語"};
      aj4Texto(op==AJ_AUD_LINGUA && pos<6?languageNames[pos]:value,TXT_G21B,770,y+5,800,1);
      if((op==AJ_TEMA || op==AJ_ESCALA_TEXTO) && pos>0) { TxtLinha label=ajTxt(TXT_G18R,op==AJ_ESCALA_TEXTO && i==2?"Padrão":value);txt_desenhar_alpha(label,1798-label.w,y+26,.58f); }
      if(op==AJ_AUD_LINGUA && pos>=2 && pos<6) { TxtLinha label=ajTxt(TXT_G18R,value);txt_desenhar_alpha(label,1798-label.w,y+26,.58f); }
      if(op==AJ_AUD_LINGUA && pos==0) {
        TxtLinha label=ajTxt(TXT_G18R,"Padrão");txt_desenhar_alpha(label,1798-label.w,y+26,.58f);
        float badgeX=770+ajTxt(TXT_G21B,languageNames[0]).w+16;
        ajNeutro((GfxRect){badgeX,y+4,158,24},6,.07f,.14f,.15f,.17f,1);aj4Texto("MELHOR PARA ESTA TV",TXT_AJ_MINI12,badgeX+8,y+10,146,.8f);
      }
      if(op==AJ_AUD_LINGUA && i==uxOriginal && uxOriginal!=valorPadrao[op]) {
        float badgeX=770+ajTxt(TXT_G21B,pos<6?languageNames[pos]:value).w+16;
        ajNeutro((GfxRect){badgeX,y+4,100,24},6,.07f,.24f,.09f,.16f,1);aj4Texto("ALTERADO",TXT_AJ_MINI12,badgeX+10,y+10,84,.9f);
      }
      if(quality && i) { TxtLinha t=ajTxt(TXT_G18R,value);txt_desenhar_alpha(t,1798-t.w,y+26,.58f); }
      static const char *const qhelp[]={"A melhor que a TV aguenta","2160p, quando o addon tem","Full HD","Leve, funciona em qualquer rede"};
      static const char *const shelp[]={"Compacto: cabe mais por linha","Um pouco menor","O padrão do app","Maior","Grande","O maior"};
      static const char *const colorsHelp[]={"O padrão do Nuvio","Vermelho escuro","Azul do fundo do app","Roxo","Verde","Quente"};
      static const char *const languageHelp[]={"A faixa que veio no arquivo","Dublado","Original em inglês","Dublado em espanhol","Original em francês","Original em japonês"};
      aj4Texto(quality && i<4?qhelp[i]:op==AJ_ESCALA_TEXTO && i<6?shelp[i]:op==AJ_TEMA && pos<6?colorsHelp[pos]:op==AJ_AUD_LINGUA && pos<6?languageHelp[pos]:i==uxOriginal?"Valor atual":"OK para aplicar",TXT_AJ_MINI14,770,y+40,800,.4f);
      if(i==uxOriginal) { float r,g,b;ajustes_acento(&r,&g,&b);gfx_cor((GfxRect){1766,y+30,12,12},.5f,r,g,b,1); }
      aj4Alvo((GfxRect){770,y,1028,70},4,i);
    }
  }
  aj4Texto(op==AJ_AUD_LINGUA?"O Nuvio não corta faixas: a original é sempre a primeira opção da lista.":op==AJ_ESCALA_TEXTO?"A escala vale nesta TV. O celular segue a conta, não a TV.":op==AJ_TEMA?"A cor de destaque é a cor do foco. Da arte usa a cor da capa que está na tela.":quality?"A qualidade é pedida à fonte a cada reprodução. Se ela não tem, o Nuvio avisa antes de baixar.":"OK aplica. Voltar mantém o valor atual.",TXT_AJ_MINI14,770,944,1028,.4f);
  ajPenRodape(section,quality?2:3,quality?5:3);
}
