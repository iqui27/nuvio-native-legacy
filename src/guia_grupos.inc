// Account identity augments the source-keyed guide; channel records stay packed.
static IptvConta grupoContas[IPTV_MAX_CONTAS];
static int grupoNContas;
static int grupoOrdFonte[G_MAX_LTV];
static void contaChave(const IptvConta *c,char *out,size_t n) {
  if(!c->id && c->tipo!=IPTV_M3U) snprintf(out,n,"%s",c->tipo==IPTV_XTREAM?"xt":"st");
  else snprintf(out,n,"%s%08x",c->tipo==IPTV_XTREAM?"xta":c->tipo==IPTV_STALKER?"sta":"ma",c->id);
}
static int fonteConta(const GFonteLtv *f,unsigned *id) {
  const char *p=NULL; unsigned v=0; char sobra;
  if(f->tipo==LTV_XTREAM && !strcmp(f->chave,"xt")) { *id=0; return 1; }
  if(f->tipo==LTV_STALKER && !strcmp(f->chave,"st")) { *id=0; return 1; }
  if(f->tipo==LTV_XTREAM && !strncmp(f->chave,"xta",3))p=f->chave+3;
  if(f->tipo==LTV_STALKER && !strncmp(f->chave,"sta",3))p=f->chave+3;
  if(f->tipo==LTV_M3U && !strncmp(f->chave,"ma",2))p=f->chave+2;
  if(!p || strlen(p)!=8 || sscanf(p,"%8x%c",&v,&sobra)!=1)return 0;
  *id=v;return 1;
}
static int fonteOrdem(int f) {
  if(f<0 || f>=nLtvF)return IPTV_MAX_CONTAS+G_MAX_LTV;
  for(int i=0;i<grupoNContas;i++) {
    char chave[LTV_CHAVE];contaChave(&grupoContas[i],chave,sizeof chave);
    if(!strcmp(chave,ltvF[f].chave))return i;
  }
  return IPTV_MAX_CONTAS+f;
}

static int fonteAtiva(int f) {
  if(f<0 || f>=nLtvF)return 0;
  unsigned id;
  if(!fonteConta(&ltvF[f],&id))return 1;
  int tipo=ltvF[f].tipo==LTV_XTREAM?IPTV_XTREAM:ltvF[f].tipo==LTV_STALKER?IPTV_STALKER:IPTV_M3U;
  for(int i=0;i<grupoNContas;i++)if(grupoContas[i].tipo==tipo && grupoContas[i].id==id)return grupoContas[i].ativa;
  return 0;
}
static int fonteVisivel(int f) {
  if(f<0 || f>=nLtvF)return 0;
  int escolha=ajustes_guia_fonte(), tipo=ltvF[f].tipo;
  return !escolha || (escolha==1 && tipo==LTV_XTREAM) || (escolha==2 && tipo==LTV_M3U) ||
      (escolha==3 && tipo==LTV_STALKER) || (escolha==4 && tipo==LTV_ADDON);
}
