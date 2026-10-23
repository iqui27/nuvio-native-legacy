// Recoverable per-profile server metadata. Never persist token-bearing art URLs.
#include "servidores.h"
#include "dados.h"
#include "jsw.h"
#include "js.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void filename(char *out, size_t n, int p, int provider){snprintf(out,n,"server-library-p%d-%d.json",p,provider);}
void servidores_cache_apagar(int profile,int provider){char name[80];filename(name,sizeof name,profile,provider);dados_apagar(name);}
int servidores_cache_gravar(int profile,int provider,const char *server,const CatItem *items,int n){
  if(!server||!*server||!items||n<0||provider<0||provider>2)return 0;
  Jsw w;jsw_iniciar(&w);jsw_obj_ini(&w);jsw_cs(&w,"server",server);jsw_chave(&w,"items");jsw_arr_ini(&w);
  if(n>CAT_MAX)n=CAT_MAX;
  for(int i=0;i<n;i++){
    const CatItem *it=&items[i];if(!it->imdb[0]||(!strcmp(it->tipo,"group")||!strcmp(it->tipo,"list")))continue;
    jsw_obj_ini(&w);jsw_cs(&w,"id",it->imdb);jsw_cs(&w,"type",it->tipo);jsw_cs(&w,"title",it->titulo);
    jsw_cs(&w,"meta",it->meta);jsw_cs(&w,"genre",it->genero);jsw_ci(&w,"progress",it->progresso);jsw_ci(&w,"resumed",it->retomadoMs);jsw_obj_fim(&w);
  }
  jsw_arr_fim(&w);jsw_obj_fim(&w);char name[80];filename(name,sizeof name,profile,provider);
  const char *text=jsw_texto_final(&w);int ok=text&&dados_gravar_leve(name,text);jsw_livre(&w);return ok;
}
int servidores_cache_ler(int profile,int provider,const char *server,CatItem *out,int max){
  char name[80],id[64]="";if(!server||!*server||!out||max<1)return 0;filename(name,sizeof name,profile,provider);
  char *body=dados_ler(name);if(!body)return 0;int n=0;
  if(!js_texto_raiz(body,"server",id,sizeof id)||strcmp(id,server)){free(body);return 0;}
  for(const char *p=js_array(body,NULL,"items");p&&n<max;p=js_prox(js_fim(p))){
    const char *end=js_fim(p);if(!end)break;CatItem it={0};
    if(!js_texto(p,end,"id",it.imdb,sizeof it.imdb)||!js_texto(p,end,"type",it.tipo,sizeof it.tipo))continue;
    js_texto(p,end,"title",it.titulo,sizeof it.titulo);js_texto(p,end,"meta",it.meta,sizeof it.meta);js_texto(p,end,"genre",it.genero,sizeof it.genero);
    it.progresso=(int)js_num(p,end,"progress",0);it.retomadoMs=(long long)js_num(p,end,"resumed",0);out[n++]=it;
  }
  free(body);return n;
}
