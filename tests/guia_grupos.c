#include "perfis.h"
#define NV_PERFIL_SESSAO_FORTE
#include "../src/guia.c"
#include <assert.h>
#include <unistd.h>
int ajustes_livetv_sugerir(void){return 0;}
const char *livecat_secao(const char *n,int *r){(void)n;*r=0;return NULL;}
int xtream_e_id(const char *id){return id && !strncmp(id,"xtream:",7);}
static int perfilTeste=1,falharGravacao,ordem,fonteEscolhida;
static uint64_t sessaoTeste=1;
static const char *pasta;
static IptvConta contasTeste[2]={{.tipo=IPTV_XTREAM,.id=11,.ativa=1,.nome="Account A"},{.tipo=IPTV_XTREAM,.id=22,.ativa=1,.nome="Account B"}};
uint64_t perfis_sessao_geracao(void){return sessaoTeste;}
int perfis_sessao_atual(uint64_t s){return s==sessaoTeste;}
int perfis_sessao_travar(uint64_t s){return s==sessaoTeste;}
void perfis_sessao_destravar(void){}
const char *i18n(const char *s){return s;}
int ajustes_guia_ordem(void){return ordem;}
int ajustes_guia_fonte(void){return fonteEscolhida;}
int iptv_contas(IptvConta *out,int max){if(out && max>=2)memcpy(out,contasTeste,sizeof contasTeste);return 2;}
int addons_n(void){return 1;}
const char *addons_base(int i){(void)i;return "https://addon.invalid";}
const char *addons_nome(int i){(void)i;return "Addon C";}
int addons_ativo(int i){(void)i;return 1;}
int perfis_ativo(void){return perfilTeste;}
char *dados_ler(const char *nome){
  char cam[1024];snprintf(cam,sizeof cam,"%s/%s",pasta,nome);FILE *f=fopen(cam,"rb");if(!f)return NULL;
  assert(!fseek(f,0,SEEK_END));long n=ftell(f);assert(n>=0);rewind(f);char *b=calloc((size_t)n+1,1);assert(b);
  assert(fread(b,1,(size_t)n,f)==(size_t)n);fclose(f);return b;
}
int dados_gravar(const char *nome,const char *texto){
  if(falharGravacao)return 0;char cam[1024];snprintf(cam,sizeof cam,"%s/%s",pasta,nome);
  FILE *f=fopen(cam,"wb");assert(f);assert(fwrite(texto,1,strlen(texto),f)==strlen(texto));return !fclose(f);
}
char *dados_caminho(char *out,unsigned n,const char *nome){snprintf(out,n,"%s/%s",pasta,nome);return out;}
void marco(const char *s){(void)s;}
static void semear(void) {
  stagingZerar();sTeto=30000;nFav=nFavOrd=0;
  for(int a=0;a<2;a++) {
    char chave[24];contaChave(&contasTeste[a],chave,sizeof chave);
    int f=sFonteChave(chave,LTV_XTREAM,contasTeste[a].nome,"");assert(f>=0);
    for(int c=0;c<2;c++) {
      char id[80],epg[40];snprintf(id,sizeof id,"xtream:@%08x:%d",contasTeste[a].id,c+1);snprintf(epg,sizeof epg,"epg-%u-%d",contasTeste[a].id,c);
      GCru cru={.id=id,.nome=c?"Sports channel":"News channel",.logo="",.desc="",.epgId=epg,.tvgNome="",.url="",.ua="",.ref="",.grupo=c?"Sports":"News",.tipo="channel",.numero=c+1};
      assert(sCanalAdd(f,&cru));sCanais[sNCanais-1].epg=(int)contasTeste[a].id*10+c;
    }
  }
  adotarStaging();
}
static void rotulo(int i,const char *s){assert(!strcmp(linhaNome(i),s));}
int main(int argc,char **argv) {
  assert(argc==2);pasta=argv[1];semear();
  assert(nCanais==4 && nCats==4 && nLinhas()==4);
  rotulo(0,"News  ·  Account A");rotulo(1,"News  ·  Account B");rotulo(2,"Sports  ·  Account A");rotulo(3,"Sports  ·  Account B");
  char id[80];snprintf(id,sizeof id,"%s",linhaItem(3,0)->id);guia_focar_id(id);
  ordem=1;guia_reordenar();assert(!strcmp(guia_id_focado(),id));
  rotulo(0,"News  ·  Account A");rotulo(1,"Sports  ·  Account A");rotulo(2,"News  ·  Account B");rotulo(3,"Sports  ·  Account B");
  IptvConta tmp=contasTeste[0];contasTeste[0]=contasTeste[1];contasTeste[1]=tmp;guia_reordenar();
  rotulo(0,"News  ·  Account B");rotulo(2,"News  ·  Account A");assert(!strcmp(guia_id_focado(),id));
  strcpy(contasTeste[0].nome,"Renamed B");guia_reordenar();rotulo(0,"News  ·  Renamed B");
  GuiaCategoria cs[4];assert(guia_categorias(cs,4)==4);GuiaCategoria old=cs[0];
  // A Cancelled editor is only a private snapshot; nothing changes until save.
  cs[0].oculta=1;assert(nLinhas()==4);falharGravacao=1;assert(!guia_categorias_definir(cs,4));assert(nLinhas()==4);falharGravacao=0;
  assert(guia_categorias_definir(cs,4));assert(nLinhas()==3 && nCanais==4);
  for(int l=0;l<nLinhas();l++)assert(categoriaChave(linhaCatDe(l))!=old.chave);
  categoriasPerfil=-1;empacotar();assert(nLinhas()==3);
  // Durable preferences and source keys do not depend on account order/name.
  tmp=contasTeste[0];contasTeste[0]=contasTeste[1];contasTeste[1]=tmp;guia_reordenar();assert(nLinhas()==3);
  sessaoTeste++;assert(!guia_categoria_definir(&old,0));
  perfilTeste=2;categoriasPerfil=-1;empacotar();assert(nLinhas()==4);
  perfilTeste=1;categoriasPerfil=-1;empacotar();assert(nLinhas()==3);
  assert(guia_categorias(cs,4)==4);for(int i=0;i<4;i++)cs[i].oculta=1;
  assert(guia_categorias_definir(cs,4));assert(nLinhas()==0 && nCanais==4);
  for(int i=0;i<4;i++)cs[i].oculta=0;assert(guia_categorias_definir(cs,4));assert(nLinhas()==4);
  fonteEscolhida=2;vistaMontar();assert(nLinhas()==0);fonteEscolhida=1;vistaMontar();assert(nLinhas()==4);fonteEscolhida=0;
  contasTeste[0].ativa=0;empacotar();assert(nCanais==2 && nLinhas()==2);contasTeste[0].ativa=1;
  // Old per-profile favorites and guide presentation migrate without account leakage.
  assert(dados_gravar("guia-fav-p1.txt", "xtream:@0000000b:1\n"));
  assert(dados_gravar("guia-modo-p1.txt", "lista\n"));favLido=nFav=0;favLer();modoLer();assert(nFav==1 && modoLista);
  semear();favLido=0;favLer();favMigrar();favAplicar();vistaMontar();assert(nFavOrd==1 && nLinhas()==5);
  assert(!strcmp(fav[0].chave,"xta0000000b"));char *saved=dados_ler("guia-ltv-fav-p1.txt");assert(saved && strstr(saved,"xta0000000b\txtream:@0000000b:1"));free(saved);
  // A source cache preserves per-account IDs and hidden keys across restarts.
  cacheGravar();int f=0;for(;f<nLtvF;f++)if(!strcmp(ltvF[f].chave,"xta0000000b"))break;assert(f<nLtvF);
  stagingZerar();sTeto=30000;assert(cacheFonteLer("xta0000000b")==2);assert(strstr(sCanais[0].id,"@0000000b"));
  // Account-zero source keys and exact per-channel choices remain the first account.
  IptvConta legacy={.tipo=IPTV_XTREAM,.ativa=1};char legacyKey[24];
  contaChave(&legacy,legacyKey,sizeof legacyKey);assert(!strcmp(legacyKey,"xt"));
  guiafiltro_ler(&fltLive,"Sxt\txtream:1\t1\tFirst\tNews\n");
  assert(guiafiltro_canal_oculto_fonte(&fltLive,legacyKey,"xtream:1"));
  assert(!guiafiltro_canal_oculto_fonte(&fltLive,"xta0000000b","xtream:@0000000b:1"));
  memset(&fltLive,0,sizeof fltLive);
  // Hidden prefix does not consume a visible slot; inventory still restores it.
  nCategoriasOcultas=nCategoriasVisiveis=0;
  stagingZerar();sTeto=1;int source=sFonteChave("xta0000000b",LTV_XTREAM,"A","");
  categoriasOcultas[0]=categoriaChaveFonte(&sLtvF[source],"Hidden");nCategoriasOcultas=1;categoriasCopiar();
  for(int i=0;i<1000;i++) {
    char key[80];snprintf(key,sizeof key,"xtream:@0000000b:%d",i);
    GCru c={.id=key,.nome="Channel",.grupo=i<999?"Hidden":"Wanted"};
    sGrupoInfo(source,c.grupo,1);sCanalAdd(source,&c);
  }
  selecionarSecoes();assert(sNCanais==1 && !strcmp(sCats[sCanais[0].cat].nome,"Wanted"));
  adotarStaging();GuiaCategoria restore[2];assert(guia_categorias(restore,2)==2 && restore[0].oculta);
  restore[0].oculta=0;assert(guia_categorias_definir(restore,1));categoriasCopiar();
  source=sFonteChave("xta0000000b",LTV_XTREAM,"A","");
  assert(!sCategoriaOculta(source,"Hidden"));
  puts("PASS: packed source groups, both order modes, labels/rename/focus, staged visibility, failed-save rollback, hidden drawer, profile guard, legacy favorites/mode and source cache");
}
