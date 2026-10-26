static void ajPenUtility(void) {
  int search=aj4Util==3;
  ajPenBase();
  ajPenChip("Ajustes","pl_chevron-left",200,112,124,0,5,0);
  ajKicker("AJUSTES",200,200,.45f);
  aj4Texto(search?"Buscar ajuste":"Diferente do padrão",TXT_TITULO2,200,218,1500,1);
  aj4Texto(search?"Encontre o ajuste pelo nome ou pelo que quer mudar.":"Só o que você mudou. O resto está como o Nuvio entregou.",TXT_G20M,200,291,1520,.58f);
  ajPenPanel(200,344,1624,640,0);
  int n=search?aj4BuscarResultados():aj4Diferencas(aj4Opcoes),start=aj4Linha/5*5;
  if(search) {
    ajPenChip(aj4Consulta[0]?aj4Consulta:"Buscar ajuste","aj_search",226,360,1300,aj4Acao!=0,10,0);
    char count[100];snprintf(count,sizeof count,i18n("%d resultados"),n);
    aj4Texto(count,TXT_G18R,1570,377,230,.4f);
    ajKicker("RESULTADOS",226,474,.45f);
  } else {
    char count[100];snprintf(count,sizeof count,i18n("%d alterações"),n);
    ajKicker(count,226,370,.45f);
    ajPenChip(aj4Escopo?"Esta TV":"Este perfil",NULL,1600,112,224,0,6,!aj4Escopo);
  }
  for(int i=start;i<n && i<start+5;i++) {
    int op=search?aj4Resultados[i].op:aj4Opcoes[i];float y=(search?504:400)+(i-start)*84;
    if(i==aj4Linha && !aj4Acao)ajNeutro((GfxRect){226,y,1572,74},20,.14f,.169f,.176f,.204f,1);
    ajIcone(search?aj4Resultados[i].icone:"aj_rotate-ccw-clock",246,y+24,26,.75f);
    aj4Texto(aj4Rotulo(op),TXT_G21B,288,y+13,1010,1);
    const char *path=search?aj4Resultados[i].caminho:uxCaminho(op);
    aj4Texto(path,TXT_AJ_MINI14,288,y+42,1010,.4f);
    char value[350],current[160],standard[160];uxValorTexto(op,uxValorAtual(op),current,sizeof current);
    if(search)snprintf(value,sizeof value,"%s",current);
    else { uxValorTexto(op,uxValorPadrao(op),standard,sizeof standard);snprintf(value,sizeof value,"%s → %s",standard,current); }
    TxtLinha t=ajTxtC(TXT_G18R,value,450);txt_desenhar_alpha(t,1750-t.w,y+24,.58f);
    ajIcone("aj_chevron-right",1760,y+28,18,.45f);
    aj4Alvo((GfxRect){226,y,1572,74},1,i);
  }
  if(!n)aj4Texto("Nenhuma mudança em relação ao padrão neste escopo.",TXT_G21B,248,520,1450,.65f);
  if(search)aj4Texto("A busca olha o nome e a ajuda de cada ajuste.",TXT_AJ_MINI14,226,944,1500,.4f);
  else {
    ajPenChip(aj4UndoN?"Desfazer":"Voltar tudo ao padrão","aj_rotate-ccw-clock",226,878,330,aj4Acao==1,3,1);
    aj4Texto("O resto está como o Nuvio entregou.",TXT_G18R,900,900,800,.4f);
  }
  ajPenRodape(search?"Buscar ajuste":"Diferente do padrão",3,3);
}
