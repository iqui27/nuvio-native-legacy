/* Dedicated service routes reuse credential, retry and IPTV account handlers. */
static void ajPenProviders(void) {
  int page=ajPenPage,provider=page==AJ_PEN_PROVIDER,iptv=page==AJ_PEN_IPTV_PROVIDER;
  const char *title=provider?"Provedor novo":iptv?"Adicionar conta":page==AJ_PEN_PROVIDER_FAILURES?"Falha nos provedores":"Falha nos add-ons";
  const char *back=iptv?"TV ao vivo e guia":"Perfil e serviços";
  ajPenBase();ajPenChip(back,"pl_chevron-left",200,112,230,0,5,0);
  ajKicker(back,200,198,1);aj4Texto(title,TXT_TITULO2,200,212,1500,1);
  aj4Texto(provider?"As chaves que dão link premium de verdade.":iptv?"Xtream, M3U ou Stalker":"O que está instalado e o que acontece quando a fonte falha.",TXT_G20M,200,291,1520,.58f);
  ajPenPanel(200,344,1624,640,0);
  int n=provider?DEBRID_N:iptv?3:2;
  static const char *const tv[]={"Xtream","M3U","Conta Stalker (portal)"};
  for(int i=0;i<n;i++) {
    float y=400+i*84;
    if(i==ajPenRow)ajPenPanel(226,y,1572,74,1);
    const char *name=provider?debrid_servico_nome(i):iptv?tv[i]:i?"Ordem":"Outra fonte se falhar";
    ajIcone(provider?"aj_key":iptv?"aj_tv":i?"aj_list-ordered":"aj_repeat",246,y+24,26,.8f);
    aj4Texto(name,TXT_G21B,288,y+24,1100,1);
    if(!provider && !iptv && !i)ajPenToggle(1682,y+19,uxValorAtual(page==AJ_PEN_PROVIDER_FAILURES?AJ_FALHA_PROVEDORES:AJ_FALHA_ADDONS)==0);
    else ajIcone("aj_chevron-right",1760,y+28,18,.45f);
    aj4Alvo((GfxRect){226,y,1572,74},8,i);
  }
  if(!provider && !iptv) {
    int count=ajPenFailures(page==AJ_PEN_PROVIDER_FAILURES);char text[96];
    snprintf(text,sizeof text,i18n(page==AJ_PEN_PROVIDER_FAILURES?"%d provedores":"%d addons"),count);
    aj4Texto(text,TXT_G18R,248,650,1450,.58f);
    int row=0;
    if(page==AJ_PEN_PROVIDER_FAILURES) {
      for(int i=0;i<DEBRID_N;i++)if(debrid_origem(debrid_servico_id(i))>0 && !debrid_servico_disponivel(i))aj4Texto(debrid_servico_nome(i),TXT_G21B,248,700+row++*48,1450,1);
    } else {
      AddonAba entries[ADDONS_MAX];int n=addons_parcial_abas(entries,ADDONS_MAX);
      for(int i=0;i<n && row<5;i++)if(entries[i].estado==ADDON_ABA_FALHOU)aj4Texto(entries[i].nome,TXT_G21B,248,700+row++*48,1450,1);
    }
  }
  ajPenRodape(back,4,4);
}
