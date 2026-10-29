#include <string.h>
static unsigned long comparisons;
static int counted_strcmp(const char *a,const char *b){comparisons++;return strcmp(a,b);}
#define strcmp counted_strcmp
#define CHANNEL_VARIANTS_TEST
#define main account_groups_main
#include "guia_grupos.c"
#undef main
static int grouping=1;
int ajustes_livetv_agrupar(void){return grouping;}
int g5ui_channel_allowed(const char *id,const char *name,const char *group,const char *source){return 1;}
int g5ui_channel_hidden(const char *id,const char *name,const char *group,const char *source){return 0;}
int g5ui_check_channel(const CatItem *it){return 1;}
int tex_falhou(const char *s){return 0;}
GLuint tex_obter_larg(const char *s,float w){return 0;}
const char *epg_canal_icone(int epg){return "";}
const char *epg_icone_id(const char *source,const char *id){return "";}
static G5Item items[32];
static const char *playing;
const char *player_id_canal(void){return playing?playing:"";}
static Stream resolved[32];static int resolvedCount;
void stream_definir_lista(const Stream *s,int n){assert(n<=32);resolvedCount=n;memcpy(resolved,s,(size_t)n*sizeof *s);}
#define XT_VARIANTES_MAX 32
#include "m3u_resolver.inc"
typedef struct { GuiaVariante variantes[32];int nVariantes,nFontesVariantes,resultado;
  char varianteBase[600],varianteTipo[16];uint64_t sessaoPerfil;Stream *fontesVariantes;atomic_int estado; } FonteJob;
#define FJOB_DONE 2
static int fonteJobCancelado(void *u){return 0;}
int stalker_e_id(const char *id){return !strncmp(id,"stalker:",8);}
int stalker_resolver_sessao(const char *id,char *url,unsigned n,uint64_t session){snprintf(url,n,"https://example.invalid/stalker.ts");return 1;}
int addons_consultar_sessao(const char *id,const char *type,const char *base,int workers,int (*cancel)(void *),void *ctx,Stream **out,uint64_t session){
  *out=calloc(1,sizeof **out);assert(*out);snprintf((*out)->rotulo,sizeof (*out)->rotulo,"Addon");snprintf((*out)->url,sizeof (*out)->url,"https://example.invalid/addon.ts");return 1;
}
#include "variant_worker.inc"
static char ids[32][80];
static void fixture(void){
  FILE *f=fopen("tests/fixtures/channel-variants.m3u","rb");assert(f);
  G5Playlist p;g5_playlist_init(&p,items,32);char bytes[31];size_t n;
  while((n=fread(bytes,1,sizeof bytes,f)))assert(g5_playlist_feed(&p,bytes,n));
  fclose(f);assert(g5_playlist_finish(&p)==13);
  stagingZerar();sTeto=30000;
  int source=sFonteChave("fixture",LTV_M3U,"Fixture","");
  for(int i=0;i<p.count;i++){
    snprintf(ids[i],sizeof ids[i],"m3u:fixture:%s",items[i].id);
    GCru c={.id=ids[i],.nome=items[i].name,.logo=items[i].poster,.epgId=items[i].epg,.url=items[i].url,.grupo=items[i].group,.tipo="channel"};
    assert(sCanalAdd(source,&c));
  }
  adotarStaging();estado=G_PRONTO;
}
static int visible(void){int n=0;for(int i=0;i<nCanais;i++)if(guia_canal_campos(i,NULL,NULL,NULL,NULL,NULL))n++;return n;}
int main(int argc,char **argv){
  assert(argc==2);pasta=argv[1];
  const char *names[]={"HBO HD+","HBO FHD¹","HBO FHD²","HBO H265¹","HBO UHD HEVC","hbo 4K H264 ALT 2","HBO SD BACKUP 3","HBO HD 2"};
  char normalized[160];
  for(unsigned i=0;i<sizeof names/sizeof *names;i++){nv_nome_base(names[i],normalized,sizeof normalized);assert(!strcmp(normalized,"hbo"));}
  nv_nome_base("HBO 2 HD",normalized,sizeof normalized);assert(!strcmp(normalized,"hbo 2"));
  nv_nome_base("SporTV 2",normalized,sizeof normalized);assert(!strcmp(normalized,"sportv 2"));
  G5Item parsed;const char *json="{\"stream_id\":1,\"name\":\"HBO HD\",\"epg_channel_id\":\"hbo.br\"}";
  assert(g5_parse_xtream_item(json,json+strlen(json),G5_LIVE,&parsed)&&!strcmp(parsed.epg,"hbo.br"));
  fixture();
  assert(nCanais==13);
  assert(visible()==9);
  GuiaVariante v[32];assert(guia_variantes(ids[0],v,32)==4);
  assert(!strcmp(v[0].id,ids[1]));
  playing=ids[0];assert(!resolverCanalM3u()&&resolvedCount==4);
  assert(!strcmp(resolved[0].canalVariante,ids[1]));
  assert(strstr(resolved[0].rotulo,"HBO FHD¹")&&strstr(resolved[0].url,"hbo-fhd1"));
  GCanal *header=canalPorId(ids[1]);header->ua="Fixture agent";header->ref="https://example.invalid/";
  assert(!resolverCanalM3u()&&strstr(resolved[0].cabecalhos,"User-Agent: Fixture agent\n")&&strstr(resolved[0].cabecalhos,"Referer: https://example.invalid/\n"));
  CatItem item;assert(guia_item_do_canal(ids[3],"","",&item));assert(!strcmp(item.titulo,"HBO"));
  guia_focar_id(ids[3]);assert(!strcmp(guia_id_focado(),ids[0]));
  guia_alternar_favorito(ids[2]);assert(guia_e_favorito(ids[0])&&guia_e_favorito(ids[3]));assert(nFavOrd==1);
  guia_variante_tocou(ids[3]);assert(guia_variantes(ids[0],v,32)==4&&!strcmp(v[0].id,ids[3]));
  assert(guia_item_do_canal(ids[0],"","",&item)&&!strcmp(item.imdb,ids[3]));
  assert(!resolverCanalM3u()&&!strcmp(resolved[0].canalVariante,ids[3]));
  assert(!strcmp(resolved[1].canalVariante,ids[1]));
  FonteJob job={0};job.nVariantes=guia_variantes(ids[0],job.variantes,32);escolherVariantes(&job);
  assert(job.nFontesVariantes==4&&!strcmp(job.fontesVariantes[0].canalVariante,ids[3]));
  assert(strstr(job.fontesVariantes[0].rotulo,"HBO H265¹"));free(job.fontesVariantes);
  job.nVariantes=1;strcpy(job.variantes[0].id,"stalker:fixture");escolherVariantes(&job);
  assert(job.nFontesVariantes==1&&strstr(job.fontesVariantes[0].url,"stalker.ts"));free(job.fontesVariantes);
  assert(lembrete_alternar(ids[2],"HBO","Movie","",100,200)==1);
  assert(guia_lembrete_achar(ids[0],100,"Movie")==0);
  assert(guia_lembrete_alternar(ids[0],"HBO","Movie","",100,200)==0);
  grouping=0;guia_reordenar();assert(visible()==13);assert(guia_e_favorito(ids[2])&&!guia_e_favorito(ids[0]));
  grouping=1;guia_reordenar();assert(visible()==9&&nFavOrd==1);
  char *saved=dados_ler("guia-variantes-p1.txt");assert(saved&&strstr(saved,ids[3]));free(saved);
  fixture();assert(guia_variantes(ids[0],v,32)==4&&!strcmp(v[0].id,ids[3]));
  falharGravacao=1;guia_variante_tocou(ids[1]);falharGravacao=0;
  assert(guia_variantes(ids[0],v,32)==4&&!strcmp(v[0].id,ids[3]));
  /* A full snapshot must use cached lookups, with grouping on and off. */
  stagingZerar();sTeto=30000;
  int source=sFonteChave("large",LTV_M3U,"Large","");
  for(int i=0;i<30000;i++){
    char id[80],name[80];snprintf(id,sizeof id,"m3u:large:%d",i);snprintf(name,sizeof name,"Channel %d HD",i);
    GCru c={.id=id,.nome=name,.grupo="Large",.tipo="channel"};assert(sCanalAdd(source,&c));
  }
  adotarStaging();
  for(int enabled=0;enabled<2;enabled++){
    grouping=enabled;guia_reordenar();comparisons=0;
    variantesMontar();favAplicar();assert(comparisons<30UL*(unsigned long)nCanais);
    comparisons=0;
    for(int i=0;i<nCanais;i++)assert(!guia_e_favorito(canais[i].id));
    assert(comparisons<30UL*(unsigned long)nCanais);
  }
  puts("PASS: variants, favorites, headers, pins and linear 30,000-channel snapshots");
}
