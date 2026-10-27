static void ajPenUtility(void) {
  int search=aj4Util==3;
  ajPenBase();
  ajPenChip("Ajustes","pl_chevron-left",200,112,124,0,5,0);
  ajKicker("AJUSTES",200,198,1);
  aj4Texto(search?"Buscar ajuste":"Diferente do padrão",TXT_TITULO2,200,212,1500,1);
  int n=search?aj4BuscarResultados():aj4Diferencas(aj4Opcoes),start=aj4Linha/5*5;
  char subtitle[240];snprintf(subtitle,sizeof subtitle,i18n("As %d escolhas que você mudou em relação ao padrão do Nuvio."),n);
  aj4Texto(search?"Digite o que você quer mudar. A lista mostra onde cada coisa mora.":subtitle,TXT_G20M,200,291,1520,.58f);
  if(search) { ajPenPanel(200,344,1624,84,0);ajPenPanel(200,452,1624,532,0); }
  else ajPenPanel(200,344,1624,640,0);
  if(search) {
    ajIcone("aj_search",226,370,24,.65f);aj4Texto(aj4Consulta[0]?aj4Consulta:"Buscar ajuste",TXT_G28B,266,370,1260,1);
    aj4Alvo((GfxRect){200,344,1624,84},10,0);
    char count[100];snprintf(count,sizeof count,i18n("%d ajustes encontrados"),n);
    aj4Texto(count,TXT_G18R,1570,377,230,.4f);
    char heading[240];snprintf(heading,sizeof heading,i18n("RESULTADOS PARA \"%s\""),aj4Consulta);
    ajKicker(heading,226,474,1);
  } else {
    char count[100];snprintf(count,sizeof count,i18n("%d alterações"),n);
    ajKicker(count,226,366,1);
    /* Left/right still changes the reset scope; the mock has no scope chip. */
  }
  for(int i=start;i<n && i<start+5;i++) {
    int op=search?aj4Resultados[i].op:aj4Opcoes[i];float y=(search?504:400)+(i-start)*84;
    if(i==aj4Linha && !aj4Acao)ajNeutro((GfxRect){226,y,1572,74},20,.14f,.169f,.176f,.204f,1);
    const AjPenRow *designed=NULL;
    for(int p=27;p<46 && !designed;p++) {
      const AjPenPage *page=p<29?&AJ_PEN_AUDIO[p-27]:&AJ_PEN_MAIN[p-29];
      for(int c=0;c<2;c++)for(int r=0;r<page->cols[c].n;r++)if(page->cols[c].rows[r].op==op) { designed=&page->cols[c].rows[r];break; }
    }
    int group=search?ajPenSearchGroup(op):-1;
    ajIcone(group>=0?aj4Resultados[i].icone:designed?designed->icon:search?aj4Resultados[i].icone:"aj_rotate-ccw-clock",246,y+24,26,.75f);
    aj4Texto(group>=0?AJ_PEN_SEARCH_TITLES[group]:designed?designed->title:aj4Rotulo(op),TXT_G21B,288,y+13,1010,1);
    const char *path=search?aj4Resultados[i].caminho:uxCaminho(op);
    aj4Texto(path,TXT_AJ_MINI14,288,y+42,1010,.4f);
    char value[350],current[160],standard[160];uxValorTexto(op,uxValorAtual(op),current,sizeof current);
    if(search)snprintf(value,sizeof value,"%s",group==1?textoValor(AJ_LEG1_TAMANHO):current);
    else { uxValorTexto(op,uxValorPadrao(op),standard,sizeof standard);snprintf(value,sizeof value,"%s → %s",standard,current); }
    TxtLinha t;
    if(!search) { float r,g,b;ajustes_acento(&r,&g,&b);t=txt_linha_corta(TXT_G18R,value,r*255,g*255,b*255,255,450); }
    else t=ajTxtC(TXT_G18R,value,450);
    txt_desenhar_alpha(t,1750-t.w,y+24,search?.58f:1);
    ajIcone("aj_chevron-right",1760,y+28,18,.45f);
    aj4Alvo((GfxRect){226,y,1572,74},1,i);
  }
  if(!n)aj4Texto("Nenhuma mudança em relação ao padrão neste escopo.",TXT_G21B,248,520,1450,.65f);
  if(search)aj4Texto("A busca olha o nome e a ajuda de cada ajuste.",TXT_AJ_MINI14,226,944,1500,.4f);
  else {
    ajPenChip(aj4UndoN?"Desfazer":"Voltar tudo ao padrão","aj_rotate-ccw",226,886,275,aj4Acao==1,3,1);
    aj4Texto("O resto está como o Nuvio entregou.",TXT_AJ_MINI14,900,907,800,.4f);
  }
  ajPenRodape(search?"Buscar ajuste":"Diferente do padrão",3,3);
}
