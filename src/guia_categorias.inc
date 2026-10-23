// Hidden categories are presentation state, independent from loaded source data.
#define G_MAX_OCULTAS 30000
static uint64_t categoriasOcultas[G_MAX_OCULTAS];
static int nCategoriasOcultas,nCategoriasVisiveis,categoriasPerfil=-1;
static uint64_t categoriasVisiveis[G_MAX_OCULTAS];
#ifdef NV_GUIA_BACKUP_CATEGORIAS
static BackupCategorias *categoriasOrigem;
#endif
static uint64_t categoriasSessao;
static int categoriaComparar(const void *aa,const void *bb) {
  uint64_t a=*(const uint64_t*)aa,b=*(const uint64_t*)bb;return (a>b)-(a<b);
}
static void categoriasOrdenar(void) {
  qsort(categoriasOcultas,(size_t)nCategoriasOcultas,sizeof *categoriasOcultas,categoriaComparar);
  qsort(categoriasVisiveis,(size_t)nCategoriasVisiveis,sizeof *categoriasVisiveis,categoriaComparar);
}
static uint64_t categoriaChaveFonte(const GFonteLtv *f,const char *nome) {
  unsigned id=0;
  int tipo=f->tipo==LTV_XTREAM?IPTV_XTREAM:f->tipo==LTV_STALKER?IPTV_STALKER:f->tipo==LTV_M3U?IPTV_M3U:0;
  char origem[64];
  // Keep saved category choices from the approved multi-account builds.
  if(tipo && fonteConta(f,&id))snprintf(origem,sizeof origem,"%d:%u:",tipo,id);
  else if(!tipo)snprintf(origem,sizeof origem,"0:0:");
  else snprintf(origem,sizeof origem,"url:%s:",f->chave);
  const char *partes[]={origem,tipo?"":f->base,nome};
  uint64_t h=1469598103934665603ULL;
  for(int i=0;i<3;i++) { for(const unsigned char *p=(const unsigned char*)partes[i];*p;p++)h=(h^*p)*1099511628211ULL;h=(h^0)*1099511628211ULL; }
  return h?h:1;
}
static uint64_t categoriaChave(int cat) {
  return cat>=0 && cat<nCats?categoriaChaveFonte(&ltvF[cats[cat].fonte],cats[cat].nome):0;
}
static void categoriasGarantir(void) {
  int perfil=perfis_ativo();uint64_t sessao=nv_perfil_sessao_ler();
  if(categoriasPerfil==perfil && categoriasSessao==sessao)return;
  categoriasPerfil=perfil;categoriasSessao=sessao;nCategoriasOcultas=nCategoriasVisiveis=0;
#ifdef NV_GUIA_BACKUP_CATEGORIAS
  backupcategorias_liberar(categoriasOrigem);categoriasOrigem=NULL;
  char origemNome[64];arquivoPerfil("categorias-origem",origemNome,sizeof origemNome);
  char *origem=dados_ler(origemNome);if(origem){backupcategorias_ler(origem,&categoriasOrigem);free(origem);}
#endif
  char nome[64];arquivoPerfil("categorias",nome,sizeof nome);char *texto=dados_ler(nome);
  if(texto) {
    for(char *p=texto;*p;) {
      char *fim=strchr(p,'\n');if(fim)*fim++=0;else fim=p+strlen(p);
      unsigned long long chave;char sobra;
      if(strlen(p)==21 && !strncmp(p,"show ",5) && sscanf(p+5,"%16llx%c",&chave,&sobra)==1 && chave && nCategoriasVisiveis<G_MAX_OCULTAS)
        categoriasVisiveis[nCategoriasVisiveis++]=(uint64_t)chave;
      if(nCategoriasOcultas<G_MAX_OCULTAS && strlen(p)==16 && sscanf(p,"%16llx%c",&chave,&sobra)==1 && chave) {
        categoriasOcultas[nCategoriasOcultas++]=(uint64_t)chave;
      }
      p=fim;
    }
    free(texto);
  }
  categoriasOrdenar();
  int n=0;for(int i=0;i<nCategoriasOcultas;i++)if(!n || categoriasOcultas[i]!=categoriasOcultas[n-1])categoriasOcultas[n++]=categoriasOcultas[i];
  nCategoriasOcultas=n;
}
static int categoriaOculta(int cat) {
  if(cat<0 || cat>=nCats)return 0;
  uint64_t chave=categoriaChave(cat);
  if(bsearch(&chave,categoriasVisiveis,(size_t)nCategoriasVisiveis,sizeof *categoriasVisiveis,categoriaComparar))return 0;
  if(bsearch(&chave,categoriasOcultas,(size_t)nCategoriasOcultas,sizeof *categoriasOcultas,categoriaComparar))return 1;
#ifdef NV_GUIA_BACKUP_CATEGORIAS
  const GFonteLtv *f=&ltvF[cats[cat].fonte];unsigned id=0;
  int tipo=f->tipo==LTV_XTREAM?IPTV_XTREAM:f->tipo==LTV_STALKER?IPTV_STALKER:f->tipo==LTV_M3U?IPTV_M3U:0;
  if(!tipo || fonteConta(f,&id))return categoriasOrigem && backupcategorias_oculta(categoriasOrigem,tipo,id,f->base,cats[cat].nome)>0;
#endif
  return 0;
}
static uint64_t sCategoriasOcultas[G_MAX_OCULTAS],sCategoriasVisiveis[G_MAX_OCULTAS];
static int sNCategoriasOcultas,sNCategoriasVisiveis;
static int sCategoriasTemOcultas(void) { return sNCategoriasOcultas>0; }
static void categoriasCopiar(void) {
  categoriasGarantir();
  sNCategoriasOcultas=nCategoriasOcultas;sNCategoriasVisiveis=nCategoriasVisiveis;
  memcpy(sCategoriasOcultas,categoriasOcultas,(size_t)nCategoriasOcultas*sizeof *categoriasOcultas);
  memcpy(sCategoriasVisiveis,categoriasVisiveis,(size_t)nCategoriasVisiveis*sizeof *categoriasVisiveis);
}
static int sCategoriaOculta(int f,const char *nome) {
  uint64_t chave=categoriaChaveFonte(&sLtvF[f],nome);
  if(bsearch(&chave,sCategoriasVisiveis,(size_t)sNCategoriasVisiveis,sizeof chave,categoriaComparar))return 0;
  return bsearch(&chave,sCategoriasOcultas,(size_t)sNCategoriasOcultas,sizeof chave,categoriaComparar)!=NULL;
}
int guia_categorias(GuiaCategoria *out,int max) {
  categoriasGarantir();int n=0;
  // Source inventory includes hidden and capped groups, so Settings can restore them.
  for(int f=0;f<nLtvF;f++)for(int g=0;g<ltvF[f].ngi;g++) {
    const char *nome=ltvF[f].gi[g].nome;
    if(out && n<max) {
      GuiaCategoria *c=&out[n];memset(c,0,sizeof *c);
      c->chave=categoriaChaveFonte(&ltvF[f],nome);c->sessao=categoriasSessao;
      snprintf(c->nome,sizeof c->nome,"%s",i18n(nome));
      snprintf(c->conta,sizeof c->conta,"%s",ltvF[f].nome);
      c->oculta=!bsearch(&c->chave,categoriasVisiveis,(size_t)nCategoriasVisiveis,sizeof c->chave,categoriaComparar) &&
        bsearch(&c->chave,categoriasOcultas,(size_t)nCategoriasOcultas,sizeof c->chave,categoriaComparar)!=NULL;
    }
    n++;
  }
  return out?(n<max?n:max):n;
}
