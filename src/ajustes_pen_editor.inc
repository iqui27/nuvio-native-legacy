/* Pencil choice page; edits use the existing draft and persistence path. */
static void ajPenEditor(void) {
  if(uxEditor!=1 || ajIptvMenu(uxOp) || uxOp==AJ_GUIA_CATEGORIAS || (ajPaleta(uxOp) && uxOp!=AJ_TEMA) || uxRestaurar || uxAvisoRisco) {
    aj4Editor();return;
  }
  int op=uxOp;
  int audio=ajPenPage==27 || ajPenPage==28;
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
  ajPenChip(escopoDe(op)==ESC_TV?"Só esta TV":"Este perfil",NULL,1650,118,174,0,9,0);
  const char *scopeHelp=op==AJ_AUD_LINGUA?"Vale para a sala, não para o celular":op==AJ_LEG_LINGUA2?"Não empurra a legenda principal":op==AJ_ESCALA_TEXTO?"A escala vale nesta TV; o celular segue a conta":op==AJ_TEMA?"A cor vale nesta TV; o celular segue a conta":quality?"A qualidade é pedida à fonte a cada reprodução":"O que está em Privado só você vê";
  TxtLinha scope=ajTxt(TXT_AJ_MINI14,scopeHelp);txt_desenhar_alpha(scope,1824-scope.w,178,.4f);
  ajPenPanel(200,344,520,640,0);ajPenPanel(744,344,1080,640,0);
  ajKicker(quality?"COMO FUNCIONA":op==AJ_ESCALA_TEXTO?"ESCALA":op==AJ_TEMA?"COR DE DESTAQUE":op==AJ_LEG_LINGUA2?"SEGUNDA LEGENDA":op==AJ_FONTE_MANUAL?"COMO ESCOLHE":"PREFERÊNCIA",226,366,1);
  char original[160];uxValorTexto(op,uxOriginal,original,sizeof original);
  const char *heads[]={quality?"Automática":op==AJ_ESCALA_TEXTO?"Escala do texto":op==AJ_TEMA?"Cor de destaque":op==AJ_AUD_LINGUA?"Preferir faixa original":op==AJ_LEG_LINGUA2?"Idioma da segunda legenda":op==AJ_FONTE_MANUAL?"Escolher na mão":"Valor atual",quality?"Limite desta TV":screen?op==AJ_TEMA?"Cor da logo":"Tamanho da interface":op==AJ_LEG_LINGUA2?"Posição":op==AJ_FONTE_MANUAL?"Ordem das fontes":"Faixa de reserva",quality?"O que o addon oferece":op==AJ_ESCALA_TEXTO?"Mínimo garantido":op==AJ_LEG_LINGUA2?"Estilo separado":op==AJ_FONTE_MANUAL?"Antes de abrir":"Onde isso vale"};
  const char *body[]={quality?"A TV pede a melhor qualidade que ela aguenta. Quem limita é a rede.":op==AJ_AUD_LINGUA?"Quando existe original em boa qualidade, ele vem primeiro — sem dublado por cima.":op==AJ_LEG_LINGUA2?"O idioma da tradução extra. Desligada, ela não aparece.":op==AJ_FONTE_MANUAL?"A TV mostra as fontes e você escolhe com o D-pad.":screen?caption:original,quality?ajudaOpcao(AJ_RESOLUCAO):op==AJ_AUD_LINGUA?"Se a escolhida não existir no título, tocamos a original.":op==AJ_LEG_LINGUA2?"No topo da tela ou junto da principal.":op==AJ_FONTE_MANUAL?"Por qualidade ou na ordem em que o addon montou.":screen?ajudaOpcao(op==AJ_TEMA?AJ_COR_LOGO:AJ_TAMANHO_UI):aj4Ajuda(op),quality?"Cada addon diz a qualidade do arquivo que ele tem.":op==AJ_LEG_LINGUA2?"A segunda legenda não muda quando você muda a principal.":op==AJ_FONTE_MANUAL?"O Nuvio confere a fonte e avisa antes de trocar.":op==AJ_ESCALA_TEXTO?"Com 85% as legendas continuam legíveis a 3 metros.":escopoDe(op)==ESC_TV?"Nesta TV e no app da sala. O celular segue a conta, não a TV.":"A escolha acompanha este perfil."};
  for(int i=0;i<3;i++) {
    float y=i==0?400:i==1?584:728,h=i==0?170:i==1?130:140;
    ajNeutro((GfxRect){226,y,468,h},22,.05f,.10f,.11f,.13f,1);
    aj4Texto(heads[i],TXT_G21B,248,y+20,422,1);
    ajBloco(TXT_G18R,body[i],248,y+54,422,22,.58f,3);
    if(op==AJ_TEMA && i==2)continue;
    if(screen && i==0) {
      if(op==AJ_ESCALA_TEXTO) {
        ajPenChip("−",NULL,248,512,44,0,11,-1);
        aj4Texto(original,TXT_G21B,306,524,80,1);
        ajPenChip("+",NULL,375,512,44,0,11,1);
      } else {
        static const int colors[]={6,0,2,3,4,5,1,11};
        for(int j=0;j<8;j++) { float r,g,b;corDeHex(TEMA_ACENTO[colors[j]].fill,&r,&g,&b);gfx_cor((GfxRect){248+j*54,510,40,40},.5f,r,g,b,1); }
      }
    }
  }
  ajPenChip("Voltar ao padrão","aj_rotate-ccw-clock",226,902,206,uxRodape,9,1);
  if(!screen)ajPenChip("Salvar como preset","aj_check",444,902,230,0,9,2);
  ajKicker(quality?"QUALIDADE":op==AJ_TEMA?"CORES":op==AJ_ESCALA_TEXTO?"ESCALA DO TEXTO":op==AJ_AUD_LINGUA?"FAIXAS DISPONÍVEIS":"ESCOLHA UM VALOR",770,366,1);
  if(OPCOES[op].tipo==OP_NUMERO) {
    char value[160];uxValorTexto(op,uxPendente,value,sizeof value);
    aj4Texto(value,TXT_TITULO2,800,480,900,1);
    aj4Texto("← →  Alterar     OK  Aplicar",TXT_G20M,800,580,900,.65f);
  } else {
    int n=nValores(op),start=uxPendente/6*6;
    for(int i=start;i<n && i<start+6;i++) {
      float y=400+(i-start)*76;char value[160];uxValorTexto(op,i,value,sizeof value);
      if(i==uxPendente && !uxRodape)ajNeutro((GfxRect){770,y,1028,70},20,.14f,.169f,.176f,.204f,1);
      aj4Texto(value,TXT_G21B,770,y+5,800,1);
      if(quality && i) { TxtLinha t=ajTxt(TXT_G18R,value);txt_desenhar_alpha(t,1798-t.w,y+26,.58f); }
      static const char *const qhelp[]={"A melhor que a TV aguenta","2160p, quando o addon tem","Full HD","Leve, funciona em qualquer rede"};
      static const char *const shelp[]={"Compacto: cabe mais por linha","Um pouco menor","O padrão do app","Maior","Grande","O maior"};
      aj4Texto(quality && i<4?qhelp[i]:op==AJ_ESCALA_TEXTO && i<6?shelp[i]:i==uxOriginal?"Valor atual":"OK para aplicar",TXT_AJ_MINI14,770,y+40,800,.4f);
      if(i==uxOriginal) { float r,g,b;ajustes_acento(&r,&g,&b);gfx_cor((GfxRect){1766,y+30,12,12},.5f,r,g,b,1); }
      aj4Alvo((GfxRect){770,y,1028,70},4,i);
    }
  }
  aj4Texto(op==AJ_AUD_LINGUA?"O Nuvio não corta faixas: a original é sempre a primeira opção da lista.":"OK aplica. Voltar mantém o valor atual.",TXT_AJ_MINI14,770,944,1028,.4f);
  ajPenRodape(section,quality?2:3,quality?5:3);
}
