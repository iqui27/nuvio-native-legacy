/* TV scope only; entry, keyboard, focus, motion and rendering belong to Spotlight. */
#include "esportes.h"
#include "guiav5ui.h"
#include "perfis.h"
#include "esportesui.h"

typedef struct {
  int tipo, indice, numero;
  time_t inicio;
  char id[128], nome[160], meta[200], arte[480], norm[400];
} SpotTVItem;
static SpotTVItem *tvItens;
static int tvN, tvCap, tvCanal, tvFim, tvVod, tvEsporte, tvLiga, tvJogo;
static char tvCategorias[256][64];
static int tvNCategorias;
static unsigned tvGuiaRev, tvEpgRev, tvEspRev;
static time_t tvMinuto;
static int tvPerfil;
static const G5Item *tvCatalogoAntes;
static int tvCatalogoN;
static const G5Item *(*tvCatalogo)(int indice,int *n);
static void (*tvAbrir)(const SpotTVResultado *);

static void tvLimpar(void){
  free(tvItens);tvItens=NULL;tvN=tvCap=tvCanal=tvFim=tvVod=0;
  tvEsporte=tvLiga=tvJogo=tvNCategorias=0;
  tvGuiaRev=guia_busca_revisao();tvEpgRev=epg_revisao();tvEspRev=esportes_revisao();
  tvMinuto=time(NULL)/60;tvPerfil=perfis_ativo();
  tvCatalogoAntes=tvCatalogo?tvCatalogo(-1,&tvCatalogoN):NULL;
}
static SpotTVItem *tvNovo(int tipo,int indice,const char *nome,const char *id){
  if(tvN==tvCap){
    int cap=tvCap?tvCap*2:256;SpotTVItem *p=realloc(tvItens,(size_t)cap*sizeof *p);
    if(!p){tvFim=1;return NULL;}tvItens=p;tvCap=cap;
  }
  SpotTVItem *p=&tvItens[tvN++];memset(p,0,sizeof *p);p->tipo=tipo;p->indice=indice;
  snprintf(p->nome,sizeof p->nome,"%s",nome);snprintf(p->id,sizeof p->id,"%s",id?id:"");
  busca_normalizar(nome,p->norm,sizeof p->norm);return p;
}
static void tvCanalIndexar(int i){
  const char *id,*nome,*logo,*cat,*base;
  if(!guia_canal_campos(i,&id,&nome,&logo,&cat,&base)||g5ui_channel_hidden(id,nome,cat,base))return;
  int numero=guia_canal_numero(i);if(numero<=0)numero=i+1;
  EpgProg prog[2];int n=guia_busca_programas(i,time(NULL),time(NULL)+86400,0,prog,2);
  SpotTVItem *p=tvNovo(SPOT_TV_CANAL,i,nome,id);if(!p)return;p->numero=numero;
  snprintf(p->arte,sizeof p->arte,"%s",logo);
  snprintf(p->meta,sizeof p->meta,"%d · %s",numero,n?prog[0].titulo:i18n(cat));
  char q[240];snprintf(q,sizeof q,"%d %s",numero,nome);busca_normalizar(q,p->norm,sizeof p->norm);
  int found=0;for(int j=0;j<tvNCategorias;j++)if(!strcmp(tvCategorias[j],cat)){found=1;break;}
  if(cat[0]&&!found&&tvNCategorias<256){
    snprintf(tvCategorias[tvNCategorias++],64,"%s",cat);
    p=tvNovo(SPOT_TV_CATEGORIA,i,i18n(cat),cat);
    if(p){
      snprintf(p->meta,sizeof p->meta,"%s",i18n("Categoria"));
      char labels[240];snprintf(labels,sizeof labels,"%s %s",cat,i18n(cat));busca_normalizar(labels,p->norm,sizeof p->norm);
    }
  }
  for(int j=0;j<n;j++){
    p=tvNovo(SPOT_TV_PROGRAMA,i,prog[j].titulo,id);if(!p)return;p->inicio=prog[j].ini;
    struct tm tm;char hora[16]="";if(localtime_r(&p->inicio,&tm))strftime(hora,sizeof hora,"%H:%M",&tm);
    snprintf(p->meta,sizeof p->meta,"%d · %s · %s",numero,nome,hora);snprintf(p->arte,sizeof p->arte,"%s",logo);
  }
}
static void tvEventoIndexar(const EspEvento *e){
  if(!e)return;
  for(int j=0;j<tvN;j++)if(tvItens[j].tipo==SPOT_TV_ESPORTE&&!strcmp(tvItens[j].id,e->id))return;
  SpotTVItem *p=tvNovo(SPOT_TV_ESPORTE,0,e->nome,e->id);if(!p)return;p->inicio=e->inicio;
  snprintf(p->meta,sizeof p->meta,"%s",e->liga);
  char names[1000];size_t used=(size_t)snprintf(names,sizeof names,"%s",e->nome);
  for(int j=0;j<e->nParticipantes&&used<sizeof names;j++)used+=(size_t)snprintf(names+used,sizeof names-used," %s %s",e->participantes[j].nome,e->participantes[j].sigla);
  busca_normalizar(names,p->norm,sizeof p->norm);
}
/* At most 32 channels or 32 VOD titles per frame, with a 2 ms soft budget.
 * Normalize once per publication/minute; query changes only scan the index. */
static int tvPasso(void){
  esportes_passo();
  esportes_catalogo_visiveis();
  for(int i=0;i<esportes_catalogo_n()&&i<8;i++)esportes_catalogo_pedir(i);
  int n=0;const G5Item *vod=tvCatalogo?tvCatalogo(-1,&n):NULL;
  int reset=tvPerfil!=perfis_ativo()||tvGuiaRev!=guia_busca_revisao()||tvEpgRev!=epg_revisao()||tvMinuto!=time(NULL)/60||vod!=tvCatalogoAntes||n!=tvCatalogoN;
  if(reset)tvLimpar();
  if(tvFim&&tvEspRev!=esportes_revisao()){
    while(tvN&&tvItens[tvN-1].tipo==SPOT_TV_ESPORTE)tvN--;
    tvFim=tvEsporte=tvLiga=tvJogo=0;tvEspRev=esportes_revisao();
  }
  if(tvFim)return 0;
  Uint32 start=SDL_GetTicks();int before=tvN;
  for(int batch=0;batch<32&&SDL_GetTicks()-start<2;batch++){
    if(tvCanal>=0){
      if(guia_canal_regra_campos(tvCanal,NULL,NULL,NULL,NULL,NULL))tvCanalIndexar(tvCanal++);
      else tvCanal=-1;
    }else if(tvVod<n){
      int i=tvVod++;const G5Item *it=tvCatalogo(i,&n);if(!it||(it->kind!=G5_MOVIE&&it->kind!=G5_SERIES))continue;
      SpotTVItem *p=tvNovo(SPOT_TV_VOD,i,it->name,it->id);if(!p)break;
      snprintf(p->arte,sizeof p->arte,"%s",it->poster);snprintf(p->meta,sizeof p->meta,"%s",i18n(it->kind==G5_SERIES?"Série":"Filme"));
    }else{
      int main=esportes_eventos_n(),fav=esportes_favoritos_n();
      if(tvEsporte<main)tvEventoIndexar(esportes_evento(tvEsporte++));
      else if(tvEsporte<main+fav)tvEventoIndexar(esportes_favoritos_evento(tvEsporte++-main));
      else if(tvLiga<esportes_catalogo_n()){
        if(tvJogo<esportes_catalogo_eventos_n(tvLiga))tvEventoIndexar(esportes_catalogo_evento(tvLiga,tvJogo++));
        else{tvLiga++;tvJogo=0;}
      }else{tvFim=1;break;}
    }
  }
  return reset||tvN!=before||tvFim;
}
static int tvAbrirEsporte(const char *id){
  for(int i=0;i<esportes_eventos_n();i++){const EspEvento *e=esportes_evento(i);if(e&&!strcmp(e->id,id)){esportesui_fontes(e);return 1;}}
  for(int i=0;i<esportes_favoritos_n();i++){const EspEvento *e=esportes_favoritos_evento(i);if(e&&!strcmp(e->id,id)){esportesui_fontes(e);return 1;}}
  for(int i=0;i<esportes_catalogo_n();i++)for(int j=0;j<esportes_catalogo_eventos_n(i);j++){
    const EspEvento *e=esportes_catalogo_evento(i,j);if(e&&!strcmp(e->id,id)){esportesui_fontes(e);return 1;}
  }
  return 0;
}
static void montarTV(const char *q){
  if(!q[0])return;
  static const char *const groups[]={"Canais ao vivo","Guia de programação","Categorias","Filmes e séries","Esportes"};
  for(int tipo=0;tipo<5;tipo++){
    int total=0;
    for(int i=0;i<tvN&&total<6;i++){
      const SpotTVItem *p=&tvItens[i];if(p->tipo!=tipo||!strstr(p->norm,q))continue;
      if(!total)cabecalho(i18n(groups[tipo]));total++;
      Linha *l=nova(tipo==SPOT_TV_CANAL||tipo==SPOT_TV_PROGRAMA?L_CANAL:tipo==SPOT_TV_VOD?L_TITULO:L_CATALOGO);if(!l)return;
      l->ref=i;snprintf(l->t1,sizeof l->t1,"%s",p->nome);snprintf(l->t2,sizeof l->t2,"%s",p->meta);
      snprintf(l->arte,sizeof l->arte,"%s",p->arte);snprintf(l->icone,sizeof l->icone,"%s",tipo==SPOT_TV_ESPORTE?"aj_trophy":"aj_rows-3");
      snprintf(l->chave,sizeof l->chave,"tv|%d|%.64s|%lld",tipo,p->id,(long long)p->inicio);
    }
  }
  if(!nLin){Linha *l=nova(L_AVISO);if(l)snprintf(l->t1,sizeof l->t1,"%s",i18n(tvFim?"Nada encontrado":"Carregando…"));}
}
