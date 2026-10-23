/* Approved Guide v5, native renderer. No network work in drawing. */
#include "guiav5ui.h"
#include "xtream.h"
#include "perfis.h"
#include "perfilinfantil.h"
#include "sessao.h"
#include "dados.h"
#include "rede.h"
#include "js.h"
#include "idioma.h"
#include "ajustes.h"
#include "teclado.h"
#include "celular.h"
#include "celbotao.h"
#include "guia.h"
#include "player.h"
#include "video.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include "plrui.h"
#include "ponteiro.h"
#include "buscanorm.h"
#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#define G5_CAP 768
#define G5_GROUPS 96
static struct {
  char server[256], user[96], password[96], playlist[2048], xmltv[1100], file[3100], name[96];
  int type, live, movies, series, epg;
} draft, work_draft;
static G5Item *inventory, *staging;
static int count, staging_count, staging_total, total, profile=-1, dirty;
static char account[96], screen[48], previous[48], query[140], message[160], catalog_group[64];
static int focus, nav=-1, selected, edit=-1, playing, selected_season=1, credential_field;
static int mode=2, mv_count, mv_main, mv_focus;
static G5Item mv_items[4], locked_item, picker_items[48];
static int picker_count;
static G5Rules rules={.hide_adult=1,.max_age=18};
static G5Lease lease;
static int lease_seconds=7200, pin_settings, pin_result;
static int episodes_mode;
static char groups[G5_GROUPS][64];
static unsigned char group_on[G5_GROUPS];
static int group_count;
static atomic_int busy, ready;
static int operation, worker_profile, worker_ok;
static atomic_int worker_cancel;
static char worker_account[96], action[160];
static XtreamConta tested;
static pthread_t worker;
static int source_loaded, test_ok, remove_confirm, use_saved, committed;
static int save_failed;
static int catalog_fetch_pending;
static G5Item *series_catalog;
static int series_count, parent_series;
static char pin_value[96];
static int next_focus[1024], num_focus;
static const char *steps[]={"Tipo de fonte","Dados do provedor","Teste de conexão","Importar","Categorias","Guia de programação","Pronto"};
static const char *titles[]={"Adicione sua fonte ao Nuvio","Dados do provedor","Teste de conexão","Escolha o que importar","Categorias","Guia de programação","Tudo pronto"};
/* Internal route identifiers, not UI labels. */
static const char *const top_destinations[]={"","","vod-filmes","vod-series","fontes-gerenciar","parental-ajustes","multiview-2"};
static const char *alphabet="abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789:/?&=%._-+@!#$,;~()[]{}* '";

static void erase(void *p,size_t n){volatile unsigned char *s=p;while(n--)*s++=0;}
static void filename(char *out,size_t n,const char *what){snprintf(out,n,"guiav5-%s-p%d.json",what,perfis_ativo());}
static void save_rules(void);
static void clear_catalog(void){free(inventory);free(series_catalog);inventory=series_catalog=NULL;count=total=series_count=0;source_loaded=0;}
static void context(void){
  int p=perfis_ativo();const char *owner=sessao_usuario();if(!owner)owner="";
  if(profile==p&&!strcmp(account,owner))return;
  catalog_group[0]=0;
  if(atomic_load(&busy))worker_cancel=1;
  if(screen[0])celular_fechar();screen[0]=0;
  profile=p;snprintf(account,sizeof account,"%s",owner);
  worker_cancel=1;clear_catalog();erase(&draft,sizeof draft);erase(&lease,sizeof lease);committed=0;catalog_fetch_pending=0;
  memset(&rules,0,sizeof rules);rules.hide_adult=1;rules.max_age=18;group_count=0;
  char file[64];filename(file,sizeof file,"rules");char *b=dados_ler(file);
  if(b){js_texto(b,NULL,"group",rules.blocked_group,sizeof rules.blocked_group);js_texto(b,NULL,"channel",rules.blocked_channel,sizeof rules.blocked_channel);
    rules.hide_adult=js_num(b,NULL,"adult",1)!=0;rules.max_age=(int)js_num(b,NULL,"age",18);
    rules.ask_unrated=js_num(b,NULL,"unrated",0)!=0;rules.hide_locked=js_num(b,NULL,"hide",0)!=0;rules.kids=js_num(b,NULL,"kids",0)!=0;free(b);}
  filename(file,sizeof file,"source");b=dados_ler(file);
  if(b){committed=1;js_texto_linhas(b,NULL,"file",draft.file,sizeof draft.file);js_texto(b,NULL,"playlist",draft.playlist,sizeof draft.playlist);js_texto(b,NULL,"xmltv",draft.xmltv,sizeof draft.xmltv);
    js_texto(b,NULL,"name",draft.name,sizeof draft.name);
    draft.type=js_num(b,NULL,"type",0);draft.live=js_num(b,NULL,"live",1);draft.movies=js_num(b,NULL,"movies",1);draft.series=js_num(b,NULL,"series",1);draft.epg=js_num(b,NULL,"epg",1);free(b);}
  if(perfil_infantil_ativo()){rules.kids=rules.hide_adult=rules.hide_locked=rules.ask_unrated=1;rules.max_age=10;}
  mv_count=0;playing=0;dirty=1;episodes_mode=0;
  if(!atomic_load(&busy))use_saved=xtream_configurado();
  if(committed&&draft.type!=0)catalog_fetch_pending=1;
}
static int adult_group(const char *group){
  char n[128];busca_normalizar(group,n,sizeof n);
  return strstr(n,"adult")||strstr(n,"xxx")||strstr(n,"18+")||strstr(n,"porn");
}
static int restricted(const G5Item *it){
  if(rules.kids&&it->kind==G5_LIVE){char n[128];busca_normalizar(it->group,n,sizeof n);if(!strstr(n,"infantil")&&!strstr(n,"kids")&&!strstr(n,"family")&&!strstr(n,"familia"))return 1;}
  return g5_restricted(&rules,&lease,it,profile,time(NULL));
}
static int group_allowed(const char *group){
  for(int i=0;i<group_count;i++)if(!strcmp(groups[i],group))return group_on[i];
  return 1;
}
static void build_groups(void){
  group_count=0;
  for(int i=0;i<count;i++){
    if(adult_group(inventory[i].group))inventory[i].adult=1;
    if(!inventory[i].group[0])snprintf(inventory[i].group,sizeof inventory[i].group,"%s","Outros");
    int g;for(g=0;g<group_count;g++)if(!strcmp(groups[g],inventory[i].group))break;
    if(g==group_count&&g<G5_GROUPS){snprintf(groups[g],sizeof groups[g],"%s",inventory[i].group);group_on[g]=!inventory[i].adult;group_count++;}
  }
  if(committed){char file[64];filename(file,sizeof file,"source");char *b=dados_ler(file);
    if(b){const char *p=js_array(b,NULL,"groups");while(p){const char *e=js_fim(p);if(!e)break;char name[64]="";js_texto(p,e,"name",name,sizeof name);
      for(int i=0;i<group_count;i++)if(!strcmp(groups[i],name))group_on[i]=js_num(p,e,"on",0)!=0;p=js_prox(e);}free(b);}
  }
}
static int feed(const void *bytes,size_t n,void *user){return g5_playlist_feed(user,bytes,n);}
static int cancelled(void *unused){(void)unused;return worker_cancel;}
static int load_playlist(G5Item *out,int cap){
  G5Playlist parser;g5_playlist_init(&parser,out,cap);
  if(work_draft.type==2){
    if(!g5_playlist_feed(&parser,work_draft.file,strlen(work_draft.file)))return -1;
    int n=g5_playlist_finish(&parser);staging_total=parser.total;return n;
  }
  if(!g5_http_url(work_draft.playlist))return -1;
  RedePedido req={.url=work_draft.playlist,.prazo_ms=30000,.max_bytes=16*1024*1024,.seguir=1,.mesmo_host=1,
    .receber_corpo=feed,.corpo_usuario=&parser,.parar=cancelled};
  RedeResposta res={0};int ok=rede_pedir(&req,&res)==REDE_OK&&res.status>=200&&res.status<300;
  rede_resposta_limpar(&res);staging_total=parser.total;
  return ok?g5_playlist_finish(&parser):-1;
}
static void *load(void *unused){
  (void)unused;worker_ok=0;staging_count=staging_total=0;
  if(operation==3){pin_result=perfis_verificar_pin(worker_profile,pin_value);erase(pin_value,sizeof pin_value);worker_ok=pin_result==1;}
  else if(operation==1){
    if(work_draft.type==0){int status=0;char *b=xtream_consultar(work_draft.server,work_draft.user,work_draft.password,"",15,&status);
      memset(&tested,0,sizeof tested);worker_ok=b&&status>=200&&status<300&&xtream_conta_parse(b,&tested)&&tested.auth==1&&xtream_conta_aviso(&tested,time(NULL))!=XA_EXPIRADA&&xtream_conta_aviso(&tested,time(NULL))!=XA_DESATIVADA;
      if(b){erase(b,strlen(b));free(b);}}
    else{staging=calloc(G5_CAP,sizeof *staging);if(staging){staging_count=load_playlist(staging,G5_CAP);worker_ok=staging_count>=0;}}
  }else{
    staging=calloc(G5_CAP,sizeof *staging);
    if(staging){
      if(work_draft.type!=0){staging_count=load_playlist(staging,G5_CAP);worker_ok=staging_count>=0;}
      else{
        int kinds[3]={G5_MOVIE,G5_SERIES,G5_LIVE};worker_ok=1;
        for(int k=0;k<3&&!worker_cancel;k++){
          if((k==0&&!work_draft.movies)||(k==1&&!work_draft.series)||(k==2&&!work_draft.live))continue;
          int status=0,n=0,t=0;char *b=xtream_consultar(work_draft.server,work_draft.user,work_draft.password,action[0]?action:k==0?"get_vod_streams":k==1?"get_series":"get_live_streams",30,&status);
          if(b&&status>=200&&status<300){if(operation==4){n=g5_parse_episodes(b,selected_season,staging,G5_CAP);t=n;}else n=g5_parse_xtream(b,kinds[k],staging+staging_count,G5_CAP/3,&t);}else n=-1;
          free(b);if(n<0){worker_ok=0;break;}
          int category_status=0;char *categories=operation==4?NULL:xtream_consultar(work_draft.server,work_draft.user,work_draft.password,k==0?"get_vod_categories":k==1?"get_series_categories":"get_live_categories",15,&category_status);
          const char *cat=categories&&category_status==200?js_raiz_array(categories):NULL;
          while(cat){const char *end=js_fim(cat);if(!end)break;char id[64]="",name[64]="";js_texto(cat,end,"category_id",id,sizeof id);js_texto(cat,end,"category_name",name,sizeof name);
            if(name[0])for(int i=staging_count;i<staging_count+n;i++)if(!strcmp(staging[i].group,id)){snprintf(staging[i].group,sizeof staging[i].group,"%s",name);if(adult_group(name))staging[i].adult=1;}cat=js_prox(end);
          }free(categories);
          staging_count+=n;staging_total+=t;if(action[0]||staging_count>=G5_CAP)break;
        }
      }
    }
  }
  atomic_store_explicit(&ready,1,memory_order_release);return NULL;
}
static int start(int op){
  if(atomic_load(&busy))return 0;
  work_draft=draft;
  if(use_saved&&work_draft.type==0)xtream_dados_copiar(work_draft.server,sizeof work_draft.server,work_draft.user,sizeof work_draft.user,work_draft.password,sizeof work_draft.password);
  operation=op;worker_profile=profile;snprintf(worker_account,sizeof worker_account,"%s",account);worker_cancel=0;
  atomic_store(&ready,0);atomic_store(&busy,1);
  if(pthread_create(&worker,NULL,load,NULL)){atomic_store(&busy,0);snprintf(message,sizeof message,"%s",i18n("Não foi possível conectar"));return 0;}
  return 1;
}
static void load_saved_credentials(void){
  if(draft.type==0)xtream_dados_copiar(draft.server,sizeof draft.server,draft.user,sizeof draft.user,draft.password,sizeof draft.password);
}
static void want_pin(int settings);
static void go(const char *name){snprintf(screen,sizeof screen,"%s",name);focus=0;nav=-1;message[0]=0;}
void g5ui_open(const char *name){context();go(name);edit=-1;
  if(!strncmp(name,"setup-",6)&&strcmp(name,"setup-tipo")&&use_saved)load_saved_credentials();
  if(!strcmp(name,"parental-ajustes")){want_pin(1);return;}
  if(!strncmp(name,"multiview-",10)&&!mv_count){
    for(int i=0;i<4096&&mv_count<4;i++){
      const char *id,*title,*logo,*group,*base;if(!guia_canal_campos(i,&id,&title,&logo,&group,&base))continue;
      if(!g5ui_channel_allowed(id,title,group,base))continue;G5Item *it=&mv_items[mv_count++];memset(it,0,sizeof *it);it->kind=G5_LIVE;it->age=-1;
      snprintf(it->id,sizeof it->id,"%s",id);snprintf(it->name,sizeof it->name,"%s",title);snprintf(it->poster,sizeof it->poster,"%s",logo);snprintf(it->group,sizeof it->group,"%s",group);snprintf(it->source,sizeof it->source,"%s",base);
    }
  }
  if(!strncmp(name,"multiview-",10)){
    picker_count=0;
    for(int i=0;i<4096&&picker_count<48;i++){
      const char *id,*title,*logo,*group,*base;if(!guia_canal_campos(i,&id,&title,&logo,&group,&base))continue;
      if(!g5ui_channel_allowed(id,title,group,base))continue;
      G5Item *it=&picker_items[picker_count++];memset(it,0,sizeof *it);it->kind=G5_LIVE;it->age=-1;
      snprintf(it->id,sizeof it->id,"%s",id);snprintf(it->name,sizeof it->name,"%s",title);snprintf(it->poster,sizeof it->poster,"%s",logo);snprintf(it->group,sizeof it->group,"%s",group);snprintf(it->source,sizeof it->source,"%s",base);
    }
    for(int i=0;i<count&&picker_count<48;i++)if(inventory[i].kind==G5_LIVE&&!restricted(&inventory[i])&&group_allowed(inventory[i].group))picker_items[picker_count++]=inventory[i];
  }
  if(!strncmp(name,"setup-",6)&&!draft.live&&!draft.movies&&!draft.series){draft.live=draft.movies=draft.series=draft.epg=1;}
  if(!strcmp(name,"setup-credenciais")||!strcmp(name,"fontes-gerenciar"))load_saved_credentials();
  if(!strncmp(name,"vod-",4)&&!source_loaded&&!atomic_load(&busy)){
    if(draft.type==0 && xtream_configurado()){
      use_saved=1;action[0]=0;draft.movies=draft.series=1;start(2);
    }else if(draft.playlist[0]){action[0]=0;start(2);}
  }
}
void g5ui_forget(void){
  g5ui_close();clear_catalog();erase(&lease,sizeof lease);erase(&draft,sizeof draft);committed=0;
  for(int p=1;p<=32;p++){char file[80];const char *parts[]={"source","rules"};for(int k=0;k<2;k++){snprintf(file,sizeof file,"guiav5-%s-p%d.json",parts[k],p);dados_apagar(file);}}
  profile=-1;
}
void g5ui_close(void){celular_fechar();screen[0]=0;edit=-1;worker_cancel=1;erase(draft.password,sizeof draft.password);nav=-1;}
int g5ui_active(void){return screen[0]!=0;}
static void persist(void){
  extern void g5ui_save_source(const char *,const char *,int,int,int,int,int);
  g5ui_save_source(draft.playlist,draft.xmltv,draft.type,draft.live,draft.movies,draft.series,draft.epg);
  if(save_failed)return;
  if(draft.type==0){xtream_definir_servidor(draft.server);xtream_definir_usuario(draft.user);if(draft.password[0])xtream_definir_senha(draft.password);}
  erase(draft.password,sizeof draft.password);use_saved=1;dirty=1;source_loaded=1;committed=1;
}
#include "jsw.h"
static void save_rules(void){
  perfil_infantil_definir(profile,rules.kids);
  char file[64];Jsw w;jsw_iniciar(&w);jsw_obj_ini(&w);
  jsw_ci(&w,"adult",rules.hide_adult);jsw_ci(&w,"age",rules.max_age);jsw_ci(&w,"unrated",rules.ask_unrated);jsw_ci(&w,"hide",rules.hide_locked);jsw_ci(&w,"kids",rules.kids);
  jsw_cs(&w,"group",rules.blocked_group);jsw_cs(&w,"channel",rules.blocked_channel);jsw_obj_fim(&w);
  filename(file,sizeof file,"rules");if(jsw_texto_final(&w))dados_gravar(file,jsw_texto_final(&w));jsw_livre(&w);dirty=1;
}
void g5ui_save_source(const char *playlist,const char *xml,int type,int live,int movies,int series,int epg){
  char file[64];Jsw w;jsw_iniciar(&w);jsw_obj_ini(&w);
  jsw_cs(&w,"playlist",playlist);jsw_cs(&w,"xmltv",xml);jsw_cs(&w,"file",draft.file);jsw_cs(&w,"name",draft.name);
  jsw_chave(&w,"type");jsw_int(&w,type);jsw_chave(&w,"live");jsw_int(&w,live);jsw_chave(&w,"movies");jsw_int(&w,movies);jsw_chave(&w,"series");jsw_int(&w,series);jsw_chave(&w,"epg");jsw_int(&w,epg);
  jsw_chave(&w,"groups");jsw_arr_ini(&w);for(int i=0;i<group_count;i++){jsw_obj_ini(&w);jsw_cs(&w,"name",groups[i]);jsw_ci(&w,"on",group_on[i]);jsw_obj_fim(&w);}jsw_arr_fim(&w);
  jsw_obj_fim(&w);filename(file,sizeof file,"source");save_failed=!jsw_texto_final(&w)||!dados_gravar(file,jsw_texto_final(&w));jsw_livre(&w);
}
static void input(int field,const char *title,const char *initial,int secret){
  edit=field;teclado_contexto("Guia TV");teclado_abrir_com(title,"",field==1||field==2?95:TECLADO_LONGO,alphabet,initial);
  if(secret)teclado_mascarar(1);
}
static void phone(void){
  static char html[24000];
  /* Same expiring, single-use local pairing. Browser only builds one JSON form
   * value; the native worker validates it and performs provider requests. */
  snprintf(html,sizeof html,"<!doctype html><html lang='pt-BR'><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'><title>%s</title>"
    "<style>*{box-sizing:border-box}body{margin:0;background:#0d0d0d;color:#f3f2ef;font:17px system-ui}main{max-width:480px;min-height:100vh;margin:auto;padding:48px 22px 24px}h1{font-size:34px;line-height:1.08;margin:24px 0 16px}p{color:#b2b4b8;line-height:1.5}input,select,button{font:inherit;border:0;border-radius:16px;padding:16px;background:#24262c;color:inherit;width:100%%;margin:7px 0 16px}button{border-radius:32px;background:#e5e9e6;color:#15171b;font-weight:600}label{display:block}input[type=checkbox]{width:auto}[hidden]{display:none!important}.rail{height:4px;background:linear-gradient(to right,#bbcfc3 32%%,#343941 32%%,#343941 65%%,#0d0d0d 65%%,#0d0d0d 67%%,#343941 67%%);margin:24px 0}.k{font-size:13px;letter-spacing:.12em;color:#b2b4b8}input,select{padding:12px;margin:3px 0 14px;border:1px solid #42464e;border-radius:14px;background:#22252b}label{font-size:14px}button{margin-top:22px}input[type=checkbox]{width:24px;height:24px;margin:10px 14px 10px 0;vertical-align:middle}#s2 label{border-bottom:1px solid #30343b;padding:4px 0;font-size:17px}#s2 h1{margin-bottom:24px}</style>"
    "<main><div class=k>NUVIO LEGACY · %s</div><div class=rail></div><form method=post action='' id=f><section id=s1><h1>%s</h1><p>%s</p>"
    "<label>%s<select id=kind><option value=0>Xtream Codes</option><option value=1>M3U URL</option></select></label>"
    "<label>%s<input id=name autocomplete=off maxlength=95></label><div id=xtream><label>%s<input id=server type=url autocomplete=off></label><label>%s<input id=user autocomplete=off></label><label>%s<input id=password type=password autocomplete=off></label></div>"
    "<div id=m3u hidden><label>%s<input id=playlist type=url autocomplete=off></label><label>%s<input id=file type=file accept='.m3u,.m3u8,text/plain'></label></div><button type=button id=next>%s</button></section>"
    "<section id=s2 hidden><h1>%s</h1><p>%s</p><label><input type=checkbox id=live checked>%s</label><label><input type=checkbox id=movies checked>%s</label><label><input type=checkbox id=series checked>%s</label>"
    "<label><input type=checkbox id=epg checked>%s</label><label>%s<input id=xmltv type=url></label><p>%s</p><input type=hidden name=t id=t><button>%s</button><button type=button id=back>%s</button></section></form></main>"
    "<script>const $=id=>document.getElementById(id);let fileText='';$('file').onchange=async()=>{const f=$('file').files[0];if(f&&f.size<=3000)fileText=await f.text();else alert('%s');};"
    "$('kind').onchange=()=>{$('xtream').hidden=$('kind').value!=='0';$('m3u').hidden=$('kind').value==='0';};"
    "$('next').onclick=()=>{if($('kind').value==='0'&&(!$('server').value||!$('user').value||!$('password').value)){$('server').focus();return;}if($('kind').value!=='0'&&!$('playlist').value&&!fileText){$('playlist').focus();return;}$('s1').hidden=true;$('s2').hidden=false;};$('back').onclick=()=>{$('s1').hidden=false;$('s2').hidden=true;};$('f').onsubmit=()=>{let o={name:$('name').value,type:Number($('kind').value),server:$('server').value,user:$('user').value,password:$('password').value,playlist:$('playlist').value,file:fileText,xmltv:$('xmltv').value,live:+$('live').checked,movies:+$('movies').checked,series:+$('series').checked,epg:+$('epg').checked};$('t').value=JSON.stringify(o);if($('t').value.length>4000){alert('%s');return false;}};</script></html>",
    i18n("Guia TV"),i18n("Configurar pelo celular"),i18n("Dados do provedor"),i18n("Use os dados enviados pelo seu provedor."),
    i18n("Tipo de fonte"),i18n("Nome"),i18n("Servidor"),i18n("Usuário"),i18n("Senha"),i18n("URL da lista M3U"),i18n("Arquivo"),i18n("Continuar"),
    i18n("Escolha o que importar"),i18n("Confira as opções e continue na TV."),i18n("Canais ao vivo"),i18n("Filmes"),i18n("Séries"),i18n("Guia de programação"),i18n("URL XMLTV"),
    i18n("Conteúdo adulto oculto por padrão."),i18n("Enviar para a TV"),i18n("Voltar"),i18n("Arquivo muito grande. Use uma URL."),i18n("Arquivo muito grande. Use uma URL."));
  char sent[4096];snprintf(sent,sizeof sent,"<!doctype html><html lang=pt-BR><meta charset=utf-8><meta name=viewport content='width=device-width,initial-scale=1'><style>body{background:#0d0d0d;color:#f3f2ef;font:20px system-ui;padding:36px;max-width:480px;margin:auto}h1{font-size:38px}p{color:#b2b4b8}</style><h1>%s</h1><p>%s</p><p>%s</p></html>",i18n("Dados enviados. Olhe para a TV."),i18n("Confira as opções e continue na TV."),i18n("A fonte ainda precisa ser testada e confirmada."));
  if(celular_abrir_pagina("Guia TV",html,sent))go("setup-celular-tv");else snprintf(message,sizeof message,"%s",i18n("Não foi possível conectar"));
}
static int match_query(const char *name){char hay[256],needle[256];busca_normalizar(name,hay,sizeof hay);busca_normalizar(query,needle,sizeof needle);return !needle[0]||strstr(hay,needle);}
static int visible_item(int i,int kind){
  G5Item *it=&inventory[i];return (kind<0||it->kind==kind)&&(!catalog_group[0]||!strcmp(catalog_group,it->group))&&group_allowed(it->group)&&match_query(it->name)&&(!(rules.hide_locked||rules.kids)||!restricted(it));
}
static int indices[G5_CAP], nindices;
static void filter(int kind){nindices=0;for(int i=0;i<count;i++)if(visible_item(i,kind))indices[nindices++]=i;}
static void select_item(int i){if(i<0||i>=count)return;selected=i;locked_item=inventory[i];
  if(restricted(&inventory[i])){snprintf(previous,sizeof previous,"%s",screen);go("parental-canal-bloqueado");return;}
  go(inventory[i].kind==G5_SERIES?"vod-serie-temporadas":"vod-titulo");
}
static void want_pin(int settings){pin_settings=settings;erase(pin_value,sizeof pin_value);if(strcmp(screen,"parental-canal-bloqueado")&&strcmp(screen,"parental-pin"))snprintf(previous,sizeof previous,"%s",screen);go("parental-pin");focus=20;}
static int control_count;
static void activate(int id);
static void point_focus(int id,int unused){(void)unused;focus=id;nav=-1;}
static void point_activate(int id,int unused){(void)unused;focus=id;activate(id);}
static void label(TxtEstilo style,const char *text,float x,float y,float width,int dim){
  if(style==TXT_TITULO2)style=TXT_V2_A3TIT;
  if(style==TXT_HEADLINE)style=TXT_V2_GRUPO;
  TxtLinha l=txt_linha_corta(style,text,dim?170:243,dim?173:242,dim?181:239,255,width);txt_desenhar(l,x,y);
}
static void panel(float x,float y,float w,float h){
  GfxRect r={x,y,w,h};if(ajustes_vidro())gfx_vidro_painel(r,28.0f/h,.82f,1);
  else gfx_cor(r,28.0f/h,.071f,.075f,.086f,1);
}
static void button(int id,const char *text,const char *icon,float x,float y,float w,float h,int row){
  GfxRect r={x,y,w,h};int focused=nav<0&&focus==id;
  if(row){if(focused)plrui_linha_foco(r,20,1);}
  else if(focused)plrui_pilula_foco(r,1);else plrui_botao_repouso(r,1);
  int c=focused&&!row?ajustes_tinta_foco():240;float tint=c/255.0f;
  if(icon&&*icon)gfx_icone((GfxRect){x+24,y+(h-28)/2,28,28},icon,tint,tint,tint,1);
  TxtLinha l=txt_linha_corta(TXT_ILHA_CORPO,text,c,c,c,255,w-(icon?84:48));txt_desenhar(l,x+(icon?68:24),y+(h-l.h)/2);
  ponteiro_alvo(x,y,w,h,point_focus,point_activate,id,0);
  if(id>=control_count)control_count=id+1;
  if(num_focus<1024)next_focus[num_focus++]=id;
}
static void toggle(int id,const char *name,int on,float x,float y,float w){button(id,name,on?"aj_check":"aj_plus",x,y,w,64,1);}
static void top(void){
  label(TXT_ILHA_TITULO,i18n("Guia TV"),48,32,210,0);
  gfx_cor((GfxRect){251,27,700,58},.5,.12,.125,.14,1);
  const char *names[]={"Guia","Cartões","Filmes","Séries","Fontes","Controle parental","Multiview"};
  const char *icons[]={"aj_tv-minimal-play","aj_layout-dashboard","aj_film","aj_tv-minimal-play","aj_settings-2","aj_shield-check","aj_layout-dashboard"};
  const float xs[]={263,411,564,746,1100,1305,1582},ws[]={140,145,174,190,197,269,210};
  for(int i=0;i<7;i++){
    int active=i==2&&!strncmp(screen,"vod-",4);GfxRect r={xs[i],29,ws[i],54};
    if(i>=4||nav==i||active)gfx_cor(r,.5,1,1,1,nav==i||active?.12:.07);
    gfx_icone((GfxRect){xs[i]+14,43,26,26},icons[i],.68,.69,.71,1);
    label(TXT_ILHA_CORPO,i18n(names[i]),xs[i]+50,42,ws[i]-62,nav!=i&&!active);
    ponteiro_alvo(xs[i],29,ws[i],54,point_focus,point_activate,1000+i,0);
  }
  time_t now=time(NULL);struct tm tm;localtime_r(&now,&tm);char clock[16];strftime(clock,sizeof clock,"%H:%M",&tm);label(TXT_ILHA_NOME,clock,1814,43,90,0);
  gfx_cor((GfxRect){48,1018,56,38},.28,.14,.15,.17,1);label(TXT_W20_24B,"OK",60,1023,44,0);label(TXT_ILHA_CORPO,i18n("Selecionar"),112,1022,200,1);
  label(TXT_ILHA_CORPO,i18n("Voltar ao guia"),330,1022,440,1);
  const ContaPerfil *p=perfis_item_ativo();if(p){gfx_icone((GfxRect){1634,1026,26,26},"aj_shield-check",.65,.66,.68,1);label(TXT_ILHA_CORPO,p->nome,1675,1022,200,1);}
  if(message[0])label(TXT_ILHA_SUB,message,500,982,1300,0);
}
static int setup_index(void){for(int i=0;i<7;i++){const char *keys[]={"setup-tipo","setup-credenciais","setup-teste","setup-importar","setup-categorias","setup-epg","setup-pronto"};if(!strcmp(screen,keys[i]))return i;}return 1;}
static void wizard(void){
  int step=setup_index();panel(48,128,350,848);panel(430,128,1442,848);
  label(TXT_ILHA_NOME,i18n("Adicionar fonte IPTV"),84,166,290,0);
  for(int i=0;i<7;i++){
    float y=222+i*76;if(step==i)gfx_cor((GfxRect){70,y,306,72},.28,1,1,1,.09);
    char n[8];snprintf(n,sizeof n,"%d",i+1);gfx_cor((GfxRect){84,y+18,36,36},.5,1,1,1,step==i?.9:.08);
    if(i<step)gfx_icone((GfxRect){91,y+25,22,22},"aj_check",.72,.79,.76,1);
    else{TxtLinha number=txt_linha(TXT_ILHA_CORPO,n,step==i?18:170,step==i?19:173,step==i?22:181,255);txt_desenhar(number,94,y+21);}
    label(TXT_ILHA_CORPO,i18n(steps[i]),136,y+21,228,step!=i);
  }
  label(TXT_ILHA_SUB,i18n("Sua fonte. Suas escolhas."),84,888,270,1);
  label(TXT_ILHA_SUB,i18n("Uma etapa de cada vez."),84,920,270,1);
  char step_label[96];snprintf(step_label,sizeof step_label,"%s · %d / 7",i18n("CONFIGURAR FONTE"),step+1);
  label(TXT_ILHA_SUB,step_label,474,170,800,1);
  label(TXT_TITULO2,i18n(!strcmp(screen,"setup-arquivo")?"Arquivo ou QR":titles[step]),474,210,1310,0);
  label(TXT_ILHA_CORPO,i18n("Use os dados enviados pelo seu provedor."),474,284,1300,1);
  if(step==0){
    const char *names[]={"Xtream Codes","M3U por URL","Arquivo ou QR","Configurar pelo celular"};
    const char *subs[]={"Servidor, usuário e senha.","Cole o link da lista do provedor.","Envie uma lista pelo celular.","Leia o QR e continue no celular."};
    const char *icons[]={"aj_tv-minimal-play","aj_link","aj_file-text","aj_smartphone"};
    for(int i=0;i<4;i++){float x=474+(i%2)*687,y=340+(i/2)*265;GfxRect r={x,y,667,i<2?245:195};
      gfx_cor(r,.1,focus==i&&nav<0?.17:.135,focus==i&&nav<0?.18:.145,focus==i&&nav<0?.20:.165,1);gfx_icone((GfxRect){x+28,y+30,38,38},icons[i],.95,.95,.95,1);
      label(TXT_HEADLINE,i18n(names[i]),x+28,y+80,610,0);label(TXT_ILHA_CORPO,i18n(subs[i]),x+28,y+132,610,1);
      if(i<2){gfx_cor((GfxRect){x+28,y+177,440,40},.25,1,1,1,.035);label(TXT_ILHA_CORPO,i18n(i==0?"Canais ao vivo":"Seu provedor define o conteúdo disponível."),x+40,y+184,418,0);}
      ponteiro_alvo(x,y,667,r.h,point_focus,point_activate,i,0);control_count=4;if(num_focus<1024)next_focus[num_focus++]=i;
    }
    button(4,i18n("Voltar ao guia"),"pl_chevron-left",474,880,280,60,0);
  }else if(!strcmp(screen,"setup-arquivo")){
    button(0,i18n("Enviar arquivo pelo celular"),"aj_smartphone",474,360,900,100,1);
    label(TXT_ILHA_CORPO,i18n("Arquivo muito grande. Use uma URL."),474,500,1240,1);
    button(1,i18n("M3U por URL"),"aj_link",474,590,900,80,1);
  }else if(step==1){
    label(TXT_ILHA_CORPO,draft.type==0?"Xtream Codes":"M3U",486,348,680,0);
    const char *fields[]={"Nome","Servidor","Usuário","Senha"};
    for(int i=0;i<(draft.type==0?4:2);i++){
      float y=394+i*108;int id=i==0?6:i-1;
      const char *value=i==0?draft.name:i==1?draft.server:i==2?draft.user:draft.password;
      if(draft.type!=0&&i==1)value=draft.playlist;
      label(TXT_ILHA_CORPO,i18n(draft.type!=0&&i==1?"URL da lista M3U":fields[i]),474,y,700,1);
      gfx_cor((GfxRect){474,y+26,702,60},.25,.135,.145,.165,1);
      button(id,i==3?(draft.password[0]?"••••••••":i18n("Senha")):value[0]?value:i18n(i==0?"Sua fonte":i==1?"Endereço do servidor":"Usuário"),NULL,474,y+26,702,60,1);
    }
    panel(1218,340,610,112);gfx_icone((GfxRect){1246,365,28,28},"aj_smartphone",.9,.9,.9,1);
    txt_bloco(TXT_ILHA_CORPO,i18n("Leia o QR e continue no celular."),180,182,190,1288,362,510,32,1,2);
    button(4,i18n("Configurar pelo celular"),"aj_smartphone",1218,471,610,60,0);
    const char *keys="ABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890.:/-@";
    for(int i=0;i<42;i++){char key[2]={i<41?keys[i]:'<',0};button(100+i,key,NULL,1218+(i%7)*88,549+(i/7)*56,80,48,0);}
    button(5,i18n("Voltar"),"pl_chevron-left",474,880,150,60,0);
    button(3,i18n("Testar conexão"),"aj_chevron-right",646,880,330,60,0);
  }else if(step==2){
    label(TXT_HEADLINE,i18n(atomic_load(&busy)?"Testando conexão…":test_ok?"Conexão estabelecida":"Falha na conexão"),474,360,1200,0);
    if(test_ok&&draft.type==0){char v[160];
      snprintf(v,sizeof v,i18n("Conexões: %d de %d"),tested.conexoes,tested.maxConexoes);label(TXT_ILHA_CORPO,v,474,438,1200,1);
      if(tested.expira){time_t expires=(time_t)tested.expira;struct tm tm;localtime_r(&expires,&tm);strftime(v,sizeof v,"%d/%m/%Y",&tm);label(TXT_ILHA_CORPO,v,474,500,1200,1);}
      label(TXT_ILHA_CORPO,tested.temM3u8?"HLS · MPEG-TS":"MPEG-TS",474,558,1200,1);
    }
    button(0,i18n(test_ok?"Continuar":"Tentar novamente"),"aj_chevron-right",474,880,330,60,0);button(1,i18n("Editar"),"aj_pencil",830,880,220,60,0);
  }else if(step==3){
    toggle(0,i18n("Canais ao vivo"),draft.live,474,350,1280);toggle(1,i18n("Filmes"),draft.movies,474,434,1280);toggle(2,i18n("Séries"),draft.series,474,518,1280);toggle(3,i18n("Guia de programação"),draft.epg,474,602,1280);
    button(4,i18n("Continuar"),"aj_chevron-right",474,880,300,60,0);
  }else if(step==4){
    button(0,i18n("Buscar categorias"),"aj_search",474,340,740,60,0);button(1,i18n("Selecionar tudo"),"aj_check",1260,340,490,60,0);
    int start=focus>=2?(focus-2)/10*10:0;
    for(int i=start;i<group_count&&i<start+10;i++)if(match_query(groups[i]))toggle(2+i,groups[i],group_on[i],474+((i-start)%2)*660,430+((i-start)/2)*76,630);
    if(!group_count)label(TXT_ILHA_CORPO,i18n(atomic_load(&busy)?"Carregando…":"Nenhuma categoria"),474,450,1200,1);
    toggle(2+group_count,i18n("Ocultar conteúdo adulto"),rules.hide_adult,474,818,1250);
    button(3+group_count,i18n("Continuar"),"aj_chevron-right",474,900,300,60,0);
  }else if(step==5){
    toggle(0,i18n("Usar guia do provedor"),draft.epg,474,370,1280);
    button(1,i18n("URL XMLTV"),"aj_link",474,480,1280,80,1);
    label(TXT_ILHA_CORPO,draft.xmltv[0]?i18n("Configurado"):i18n("Opcional"),500,576,1200,1);
    button(2,i18n("Importar"),"aj_check",474,880,300,60,0);
  }else{
    label(TXT_HEADLINE,i18n("Fonte adicionada ao perfil"),474,370,1200,0);
    label(TXT_ILHA_CORPO,i18n("Conteúdo adulto oculto por padrão."),474,450,1200,1);
    button(0,i18n("Abrir o Guia de TV"),"aj_tv-minimal-play",474,600,580,80,1);
    button(1,i18n("Filmes e séries"),"aj_film",474,704,580,80,1);
    button(2,i18n("Gerenciar fontes"),"aj_settings-2",474,880,360,60,0);
  }
}
static void phone_screen(void){
  panel(48,128,1824,848);label(TXT_TITULO2,i18n("Mais fácil pelo celular."),102,200,1500,0);
  label(TXT_ILHA_CORPO,i18n("Leia o QR e continue no celular."),102,286,1500,1);
  if(celular_estado()==CEL_ESPERANDO){celb_qr_desenhar(celular_url(),128,392,1);label(TXT_ILHA_SUB,celular_url(),102,740,740,0);}
  else label(TXT_ILHA_CORPO,i18n("Pareamento expirado"),102,450,660,0);
  const char *names[]={"Dados do provedor","Teste de conexão","Escolha o que importar","Confira as opções e continue na TV."};
  for(int i=0;i<4;i++)label(TXT_ILHA_CORPO,i18n(names[i]),876,410+i*108,830,1);
  button(4,i18n("Continuar na TV"),"pl_chevron-left",102,880,400,60,0);button(5,i18n("Novo QR"),"aj_rotate-cw",550,880,260,60,0);
}
static void sources_screen(void){
  label(TXT_ILHA_SUB,i18n("GUIA TV / FONTES"),48,150,1600,1);label(TXT_TITULO2,i18n("Seus provedores, num só lugar."),48,194,1450,0);
  button(0,i18n("Adicionar fonte"),"aj_plus",1450,176,420,64,0);
  panel(48,320,1824,180);label(TXT_HEADLINE,draft.name[0]?draft.name:xtream_configurado()?"Xtream Codes":draft.playlist[0]?"M3U":i18n("Nenhuma fonte"),140,350,900,0);
  label(TXT_ILHA_CORPO,i18n(atomic_load(&busy)?"Atualizando…":source_loaded?"Configurado":"Não configurado"),140,410,1000,1);
  button(1,i18n("Atualizar"),"aj_rotate-cw",1090,375,240,64,0);button(2,i18n("Editar"),"aj_settings-2",1344,375,225,64,0);button(3,i18n(remove_confirm?"Confirmar remoção":"Remover"),"aj_trash-2",1580,375,250,64,0);
  button(4,i18n("Controle parental"),"aj_shield-check",48,650,450,64,0);button(5,i18n("Guia de programação"),"aj_clock",530,650,450,64,0);
}
static void catalog(void){
  int series=!strcmp(screen,"vod-series"), search=!strcmp(screen,"vod-busca");filter(episodes_mode?G5_EPISODE:search?-1:series?G5_SERIES:G5_MOVIE);
  button(803,draft.name[0]?draft.name:i18n("Do seu IPTV"),"aj_tv-minimal-play",48,112,310,52,0);
  char numbers[120];snprintf(numbers,sizeof numbers,i18n("Mostrando %d de %d itens"),count,total?total:count);label(TXT_ILHA_CORPO,numbers,390,125,690,1);
  panel(48,190,320,800);label(TXT_ILHA_NOME,i18n("Catálogo"),80,217,270,0);
  button(800,i18n("Filmes"),"aj_film",68,258,280,54,1);button(801,i18n("Séries"),"aj_tv-minimal-play",68,312,280,54,1);button(802,i18n("Buscar"),"aj_search",68,366,280,54,1);
  gfx_cor((GfxRect){68,439,280,1},0,1,1,1,.10);label(TXT_ILHA_NOME,i18n("Categorias"),80,462,270,0);
  button(805,i18n("Todas"),"aj_folder",68,502,280,54,1);
  for(int g=0;g<group_count&&g<4;g++)button(820+g,groups[g],"aj_folder",68,563+g*62,280,54,1);
  label(TXT_ILHA_SUB,i18n("Conteúdo adulto oculto por padrão."),80,884,270,1);
  button(806,i18n("Adicionar fonte"),"aj_plus",68,915,280,54,1);
  if(search)button(804,query[0]?query:i18n("Buscar canais, filmes e séries"),"aj_search",400,204,1472,64,1);
  else if(nindices&&!atomic_load(&busy)){
    G5Item *hero=&inventory[indices[focus>=0&&focus<nindices?focus:0]];
    GfxRect r={400,190,1472,330};panel(r.x,r.y,r.w,r.h);
    GLuint art=!restricted(hero)&&hero->backdrop[0]?tex_obter_larg(hero->backdrop,1472):0;
    if(art)gfx_rect(r,art,GFX_CARD,0,0,0,.08,1,1,1,1);
    gfx_rect(r,0,GFX_VEU,0,0,0,.08,0,0,0,.75);
    label(TXT_ILHA_CORPO,i18n("Do seu IPTV"),432,224,1200,1);
    label(TXT_V2_TIT,restricted(hero)?i18n("Conteúdo bloqueado"):hero->name,432,266,1350,0);
    label(TXT_ILHA_CORPO,i18n("Seu provedor define o conteúdo disponível."),432,394,1350,0);
    button(807,i18n("Detalhes"),"aj_play",432,446,260,60,0);button(series?800:801,i18n(series?"Filmes":"Séries"),"aj_chevron-right",710,446,260,60,0);
    label(TXT_HEADLINE,i18n(series?"Séries do seu provedor":"Filmes do seu provedor"),400,548,1300,0);
  }
  if(total>count){char limit[120];snprintf(limit,sizeof limit,i18n("Mostrando %d de %d itens"),count,total);label(TXT_ILHA_SUB,limit,442,936,1300,1);}
  int loading=atomic_load(&busy), empty=!nindices;
  if(loading||empty||!strcmp(screen,"vod-sem-catalogo")||!strcmp(screen,"vod-vazio")||!strcmp(screen,"vod-carregando")){
    const char *s=loading||!strcmp(screen,"vod-carregando")?"Carregando catálogo…":query[0]||!strcmp(screen,"vod-vazio")?"Nenhum resultado":"Esta fonte não tem catálogo VOD";
    label(TXT_HEADLINE,i18n(s),650,420,1100,0);label(TXT_ILHA_CORPO,i18n("Seu provedor define o conteúdo disponível."),650,490,1100,1);
    button(0,i18n(query[0]?"Limpar busca":"Adicionar fonte"),"aj_plus",650,620,500,64,0);return;
  }
  int page_size=search?12:6;int start=focus>=0&&focus<768?(focus/page_size)*page_size:0;
  for(int n=start;n<nindices&&n<start+page_size;n++){
    G5Item *it=&inventory[indices[n]];float x=400+((n-start)%6)*249,y=(search?304:596)+((n-start)/6)*324;
    GfxRect r={x,y,230,270};int locked=restricted(it);GLuint tex=!locked&&it->poster[0]?tex_obter_larg(it->poster,230):0;
    if(tex)gfx_rect(r,tex,GFX_CARD,0,0,0,.07,1,1,1,1);else gfx_cor(r,.07,.12,.13,.15,1);
    if(locked)gfx_icone((GfxRect){x+90,y+108,48,48},"aj_lock",.8,.8,.8,1);
    if(focus==n&&nav<0)plrui_linha_foco((GfxRect){x-4,y+274,238,76},18,1);
    label(TXT_W20_24B,locked?i18n("Conteúdo bloqueado"):it->name,x,y+282,230,0);
    label(TXT_ILHA_CORPO,it->group,x,y+318,230,1);
    ponteiro_alvo(x,y,230,350,point_focus,point_activate,n,0);
    if(num_focus<1024)next_focus[num_focus++]=n;
  }
  control_count=nindices;
}
static void title_screen(void){
  if(selected<0||selected>=count){go("vod-vazio");return;}
  G5Item *it=&inventory[selected];if(restricted(it)){locked_item=*it;go("parental-canal-bloqueado");return;}
  panel(48,128,1824,848);GLuint t=it->poster[0]?tex_obter_larg(it->poster,350):0;
  if(t)gfx_rect((GfxRect){100,205,350,525},t,GFX_CARD,0,0,0,.04,1,1,1,1);
  label(TXT_ILHA_SUB,i18n("Do seu IPTV"),510,220,1250,1);label(TXT_TITULO2,it->name,510,278,1250,0);
  txt_bloco(TXT_ILHA_CORPO,it->plot[0]?it->plot:i18n("Sem descrição"),180,182,190,510,380,1200,36,1,4);
  if(it->kind==G5_SERIES){
    button(0,i18n("Temporada"),"aj_tv-minimal-play",510,590,330,64,0);
    char season[32];snprintf(season,sizeof season,"%d",selected_season);label(TXT_HEADLINE,season,870,600,200,0);
    button(1,i18n("Carregar episódios"),"aj_chevron-right",510,700,480,64,0);
  }else button(0,i18n("Assistir"),"aj_play",510,590,350,64,0);
  button(2,i18n("Voltar"),"pl_chevron-left",510,860,260,60,0);
}
static void parental_screen(void){
  if(!strcmp(screen,"parental-pin")){
    gfx_cor((GfxRect){0,0,1920,1080},0,0,0,0,.35);
    panel(545,119,830,842);label(TXT_ILHA_CORPO,i18n("PIN do perfil"),586,164,740,1);
    gfx_icone((GfxRect){1306,171,26,26},"aj_lock",.94,.94,.94,1);
    label(TXT_V2_A3TIT,i18n("Digitar PIN do perfil"),586,218,740,0);
    label(TXT_ILHA_CORPO,i18n("Use o mesmo PIN do perfil."),586,290,740,1);
    for(int i=0;i<4;i++){gfx_cor((GfxRect){586+i*92,349,76,76},.2,.19,.20,.23,1);label(TXT_HEADLINE,i<(int)strlen(pin_value)?"•":"_",615+i*92,372,50,0);}
    label(TXT_ILHA_NOME,i18n("Desbloqueio temporário"),586,473,580,0);
    char duration[80];snprintf(duration,sizeof duration,i18n("%d horas"),lease_seconds/3600);button(3,duration,NULL,1170,453,163,60,0);
    const char *keys[]={"1","2","3","4","5","6","7","8","9","Limpar","0","Excluir"};
    for(int i=0;i<12;i++)button(20+i,i18n(keys[i]),NULL,586+(i%3)*253,537+(i/3)*64,243,54,0);
    button(0,i18n("Confirmar"),"aj_shield-check",586,807,230,60,0);
    button(1,i18n("Cancelar"),"pl_chevron-left",832,807,230,60,0);return;
  }
  if(!strcmp(screen,"parental-canal-bloqueado")){
    panel(48,128,1824,848);panel(100,270,720,440);gfx_icone((GfxRect){410,390,90,90},"aj_lock",.7,.7,.7,1);
    label(TXT_HEADLINE,i18n("Prévia bloqueada"),250,535,600,0);
    label(TXT_TITULO2,i18n("Este conteúdo precisa do PIN do perfil."),900,292,850,0);
    label(TXT_ILHA_CORPO,i18n("Imagem, áudio e detalhes ficam ocultos."),900,420,850,1);
    button(0,i18n("Digitar PIN do perfil"),"aj_lock",900,550,780,72,0);button(1,i18n("Escolher outro canal"),"pl_chevron-left",900,650,780,72,0);
    button(2,i18n("Controle parental"),"aj_shield-check",900,750,780,72,0);return;
  }
  label(TXT_ILHA_SUB,i18n("GUIA TV / REGRAS DO PERFIL"),48,148,1700,1);label(TXT_TITULO2,i18n("Assista com tranquilidade."),48,190,1700,0);
  panel(48,300,470,676);panel(548,300,1324,676);
  label(TXT_HEADLINE,i18n("Proteger este perfil"),90,345,390,0);label(TXT_ILHA_CORPO,i18n("Use o mesmo PIN do perfil."),90,420,390,1);
  const char *rows[]={"Ocultar conteúdo adulto","Bloquear canal em foco","Bloquear categoria em foco","Classificação indicativa","Pedir PIN sem classificação","Ocultar completamente","Perfil infantil","Desbloqueio temporário"};
  for(int i=0;i<8;i++){
    int on=i==0?rules.hide_adult:i==1?rules.blocked_channel[0]!=0:i==2?rules.blocked_group[0]!=0:i==4?rules.ask_unrated:i==5?rules.hide_locked:i==6?rules.kids:0;
    toggle(i,i18n(rows[i]),on,584,334+i*72,1248);
  }
  if(!strcmp(screen,"parental-desbloqueio")){
    gfx_cor((GfxRect){0,0,1920,1080},0,0,0,0,.3);panel(530,260,860,580);
    label(TXT_HEADLINE,i18n("Desbloqueio temporário"),586,320,750,0);label(TXT_ILHA_CORPO,i18n("Regras voltam ao trocar de perfil."),586,400,750,1);
    char time[64];snprintf(time,sizeof time,i18n("%d horas"),lease_seconds/3600);button(10,time,"aj_clock",586,500,740,64,0);button(11,i18n("Continuar com PIN"),"aj_lock",586,650,740,64,0);
  }
}
static void kids_screen(void){
  panel(48,128,1824,310);label(TXT_ILHA_SUB,i18n("Perfil infantil"),98,174,1600,1);label(TXT_TITULO2,i18n("Um mundo de favoritos."),98,232,1600,0);
  label(TXT_ILHA_CORPO,i18n("Somente canais e títulos permitidos."),98,324,1600,1);filter(-1);
  int n=0;for(int i=0;i<nindices&&n<6;i++){G5Item *it=&inventory[indices[i]];if(restricted(it))continue;button(n,it->name,"aj_tv-minimal-play",48+(n%3)*614,500+(n/3)*150,580,120,1);indices[n++]=indices[i];}
  button(800,i18n("Controle parental"),"aj_lock",48,880,500,64,0);
}
void g5ui_multiview_rect(float *x,float *y,float *w,float *h){
  *x=48;*y=238;*w=mode==3?1160:904;*h=mode==4?344:650;
}
static void multiview_screen(void){
  label(TXT_HEADLINE,i18n("Todos os jogos. Do seu jeito."),48,145,900,0);
  button(800,"2","aj_layout-dashboard",1180,132,180,64,0);button(801,"1 + 2","aj_layout-dashboard",1380,132,220,64,0);button(802,"2 × 2","aj_layout-dashboard",1620,132,250,64,0);
  int n=mode;
  for(int i=0;i<n;i++){
    float x,y,w,h;if(mode==3){x=i==0?48:1232;y=i==2?575:238;w=i==0?1160:640;h=i==0?650:313;}
    else{x=48+(i%2)*928;y=238+(i/2)*368;w=904;h=mode==4?344:650;}
    GfxRect r={x,y,w,h};gfx_cor(r,22/h,.08,.09,.105,1);
    G5Item *it=i<mv_count?&mv_items[i]:NULL;
    int live=it && i==mv_main && player_mini_no_guia_ativo()&&video_pronto();
    if(live)gfx_furo_raio(r,22/h);
    else if(it&&it->poster[0]&&!restricted(it)){GLuint t=tex_obter_larg(it->poster,w);if(t)gfx_rect(r,t,GFX_CARD,0,0,0,22/h,1,1,1,1);}
    label(TXT_ILHA_SUB,i18n(live?"Ao vivo · áudio ligado":"Prévia estática · sem áudio"),x+26,y+26,w-52,0);
    if(it)label(TXT_HEADLINE,restricted(it)?i18n("Conteúdo bloqueado"):it->name,x+26,y+h-80,w-52,0);
    button(i,i18n(it?"Tornar principal":"Adicionar canal"),it?"aj_tv-minimal-play":"aj_plus",x+24,y+h-140,w-48,52,0);
  }
  button(803,i18n("Adicionar canal"),"aj_plus",48,920,360,64,0);button(804,i18n("Trocar principal"),"aj_arrow-left-right",440,920,360,64,0);button(805,i18n("Limites desta TV"),"aj_info",840,920,360,64,0);
  if(!strcmp(screen,"multiview-escolher")){
    panel(1130,128,742,848);label(TXT_HEADLINE,i18n("Adicionar canal"),1174,174,630,0);
    int start=focus>=900?(focus-900)/7*7:0;
    for(int i=start;i<picker_count&&i<start+7;i++)button(900+i,picker_items[i].name,"aj_plus",1174,276+(i-start)*84,630,68,1);
    label(TXT_ILHA_SUB,i18n("Um vídeo ao vivo. Os demais são prévias."),1174,880,630,1);
  }
  if(!strcmp(screen,"multiview-limite")){
    gfx_cor((GfxRect){0,0,1920,1080},0,0,0,0,.3);panel(490,235,940,650);
    label(TXT_ILHA_SUB,"MULTIVIEW",546,288,800,1);label(TXT_TITULO2,i18n("Mantenha esta TV fluida."),546,345,820,0);
    label(TXT_ILHA_CORPO,i18n("Esta TV reproduz um vídeo ao vivo."),546,460,820,0);
    label(TXT_ILHA_CORPO,i18n("Outros quadros mostram arte estática da fonte."),546,530,820,1);
    label(TXT_ILHA_CORPO,i18n("Reduzir a qualidade não cria outro decodificador."),546,610,820,1);
    button(810,i18n("Usar um vídeo e prévias"),"aj_check",546,735,820,70,0);
  }
}
int g5ui_draw(void){
  if(!screen[0])return 0;
  gfx_cor((GfxRect){0,0,1920,1080},0,.045,.047,.051,1);control_count=0;num_focus=0;top();
  if(!strcmp(screen,"setup-celular-tv"))phone_screen();
  else if(!strncmp(screen,"setup-",6))wizard();
  else if(!strcmp(screen,"fontes-gerenciar"))sources_screen();
  else if(!strcmp(screen,"vod-titulo")||!strcmp(screen,"vod-serie-temporadas"))title_screen();
  else if(!strncmp(screen,"vod-",4))catalog();
  else if(!strcmp(screen,"parental-perfil-infantil"))kids_screen();
  else if(!strncmp(screen,"parental-",9))parental_screen();
  else if(!strncmp(screen,"multiview-",10))multiview_screen();
  return 1;
}
static void activate(int id){
  if(id>=1000){episodes_mode=0;int n=id-1000;
    if(n<0||n>6)return;if(n<2){g5ui_close();return;}if(n==5){want_pin(1);return;}g5ui_open(top_destinations[n]);return;}
  if(atomic_load(&busy)){snprintf(message,sizeof message,"%s",i18n("Aguarde…"));return;}
  if(!strcmp(screen,"setup-tipo")){
    if(id==4){g5ui_close();return;}if(id==2){go("setup-arquivo");return;}if(id==3){phone();return;}
    use_saved=0;draft.type=id;draft.live=draft.movies=draft.series=draft.epg=1;go("setup-credenciais");return;
  }
  if(!strcmp(screen,"setup-arquivo")){if(id==0)phone();else{draft.type=1;go("setup-credenciais");}return;}
  if(!strcmp(screen,"setup-celular-tv")){if(id==4){celular_fechar();go("setup-credenciais");}else if(id==5)phone();return;}
  if(!strcmp(screen,"setup-credenciais")){
    if(id>=100&&id<142){
      char *value=credential_field==6?draft.name:credential_field==1?draft.user:credential_field==2?draft.password:draft.type?draft.playlist:draft.server;
      size_t cap=credential_field==6?sizeof draft.name:credential_field==1?sizeof draft.user:credential_field==2?sizeof draft.password:draft.type?sizeof draft.playlist:sizeof draft.server;
      size_t n=strlen(value);const char *keys="ABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890.:/-@";
      if(id==141){if(n)value[n-1]=0;}else if(n+1<cap){value[n]=keys[id-100];value[n+1]=0;}use_saved=0;return;
    }
    if(id==5){go("setup-tipo");return;}
    if(id==6){credential_field=6;input(6,"Nome",draft.name,0);return;}
    if(id==4){phone();return;}if(id==3){
      if(draft.type==0&&!g5_http_url(draft.server)){snprintf(message,sizeof message,"%s",i18n("Endereço inválido"));return;}
      if(draft.type==0&&(!draft.user[0]||!draft.password[0])){snprintf(message,sizeof message,"%s",i18n("Preencha usuário e senha"));return;}
      go("setup-teste");test_ok=0;start(1);return;}
    credential_field=id;
    if(draft.type==0)input(id,id==0?"Servidor":id==1?"Usuário":"Senha",id==0?draft.server:id==1?draft.user:draft.password,id==2);
    else input(3,"URL da lista M3U",draft.playlist,1);return;
  }
  if(!strcmp(screen,"setup-teste")){if(id==1)go("setup-credenciais");else if(test_ok)go("setup-importar");else start(1);return;}
  if(!strcmp(screen,"setup-importar")){
    if(id==0)draft.live=!draft.live;else if(id==1)draft.movies=!draft.movies;else if(id==2)draft.series=!draft.series;else if(id==3)draft.epg=!draft.epg;
    else{if(!draft.live&&!draft.movies&&!draft.series){snprintf(message,sizeof message,"%s",i18n("Selecione pelo menos uma opção"));return;}
      go("setup-categorias");if(draft.type==0){action[0]=0;start(2);}else build_groups();}return;
  }
  if(!strcmp(screen,"setup-categorias")){
    if(id==0)input(5,"Buscar categorias",query,0);
    else if(id==1)for(int i=0;i<group_count;i++)group_on[i]=!adult_group(groups[i]);
    else if(id==2+group_count){want_pin(2);}
    else if(id==3+group_count)go("setup-epg");
    else if(id>=2&&id<2+group_count){if(adult_group(groups[id-2]))want_pin(2);else group_on[id-2]=!group_on[id-2];}
    return;
  }
  if(!strcmp(screen,"setup-epg")){if(id==0)draft.epg=!draft.epg;else if(id==1)input(4,"URL XMLTV",draft.xmltv,1);else{persist();if(save_failed)snprintf(message,sizeof message,"%s",i18n("Não foi possível salvar a lista"));else go("setup-pronto");}return;}
  if(!strcmp(screen,"setup-pronto")){if(id==0)g5ui_close();else go(id==1?"vod-filmes":"fontes-gerenciar");return;}
  if(!strcmp(screen,"fontes-gerenciar")){
    if(id==0)go("setup-tipo");else if(id==1){use_saved=1;action[0]=0;start(2);}else if(id==2)go("setup-credenciais");
    else if(id==3){if(!remove_confirm){remove_confirm=1;return;}xtream_esquecer();draft.playlist[0]=draft.xmltv[0]=0;char file[64];filename(file,sizeof file,"source");dados_apagar(file);clear_catalog();dirty=1;committed=0;remove_confirm=0;}
    else if(id==4)want_pin(1);else if(id==5)go("setup-epg");return;
  }
  if(!strcmp(screen,"parental-pin")){
    size_t n=strlen(pin_value);
    if(id>=20&&id<=31){if(id==29)erase(pin_value,sizeof pin_value);else if(id==31){if(n)pin_value[n-1]=0;}else if(n<4){pin_value[n]=(char)('0'+(id==30?0:id-19));pin_value[n+1]=0;}return;}
    if(id==0){if(n==4)start(3);return;}
    if(id==1){erase(pin_value,sizeof pin_value);g5ui_close();return;}
    if(id==3)lease_seconds=lease_seconds==3600?7200:lease_seconds==7200?14400:3600;return;
  }
  if(!strcmp(screen,"parental-canal-bloqueado")){
    if(id==0)want_pin(0);else if(id==1)g5ui_close();else if(id==3)lease_seconds=lease_seconds==3600?7200:lease_seconds==7200?14400:3600;else want_pin(1);return;
  }
  if(!strcmp(screen,"parental-desbloqueio")){
    if(id==10)lease_seconds=lease_seconds==3600?7200:lease_seconds==7200?14400:3600;else if(id==11)want_pin(0);return;
  }
  if(!strcmp(screen,"parental-ajustes")){
    if(id==0)rules.hide_adult=!rules.hide_adult;
    else if(id==1){if(rules.blocked_channel[0])rules.blocked_channel[0]=0;else snprintf(rules.blocked_channel,sizeof rules.blocked_channel,"%s",locked_item.id);}
    else if(id==2){if(rules.blocked_group[0])rules.blocked_group[0]=0;else snprintf(rules.blocked_group,sizeof rules.blocked_group,"%s",locked_item.group);}
    else if(id==3)rules.max_age=rules.max_age>=18?10:rules.max_age+2;
    else if(id==4)rules.ask_unrated=!rules.ask_unrated;else if(id==5)rules.hide_locked=!rules.hide_locked;
    else if(id==6){rules.kids=!rules.kids;if(rules.kids){rules.hide_adult=rules.hide_locked=rules.ask_unrated=1;rules.max_age=10;erase(&lease,sizeof lease);go("parental-perfil-infantil");}}
    else if(id==7)go("parental-desbloqueio");save_rules();return;
  }
  if(!strcmp(screen,"parental-perfil-infantil")){if(id==800)want_pin(1);else if(id>=0&&id<nindices)select_item(indices[id]);return;}
  if(!strcmp(screen,"vod-titulo")){
    if(id==0&&selected<count&&!restricted(&inventory[selected]))playing=1;else if(id==2)go("vod-filmes");return;
  }
  if(!strcmp(screen,"vod-serie-temporadas")){
    if(id==0){selected_season=selected_season>=30?1:selected_season+1;return;}
    if(id==2){go("vod-series");return;}
    snprintf(action,sizeof action,"get_series_info&series_id=%s",inventory[selected].id);start(4);return;
  }
  if(!strncmp(screen,"vod-",4)){
    if(id==800){episodes_mode=0;catalog_group[0]=0;go("vod-filmes");}else if(id==801){episodes_mode=0;catalog_group[0]=0;go("vod-series");}else if(id==802||id==804){go("vod-busca");input(5,"Buscar",query,0);}
    else if(id==803)go("fontes-gerenciar");else if(id==805){catalog_group[0]=0;focus=0;}else if(id==806)go("setup-tipo");else if(id==807&&nindices)select_item(indices[0]);else if(id>=820&&id<820+group_count){snprintf(catalog_group,sizeof catalog_group,"%s",groups[id-820]);focus=0;}else if(nindices&&id<nindices)select_item(indices[id]);else if(query[0])query[0]=0;else go("setup-tipo");return;
  }
  if(!strncmp(screen,"multiview-",10)){
    if(id>=900&&id<900+picker_count){
      G5Item *it=&picker_items[id-900];
      if(!restricted(it)&&mv_count<4)mv_items[mv_count++]=*it;
      else if(mv_count>=4){go("multiview-limite");return;}
      if(mv_count==1)mv_focus=1;
      go(mode==4?"multiview-4":mode==3?"multiview-3":"multiview-2");return;}
    if(id==800||id==801||id==802){mode=id==800?2:id==801?3:4;go(mode==4?"multiview-4":mode==3?"multiview-3":"multiview-2");
      if(player_mini_no_guia_ativo()){float x,y,w,h;g5ui_multiview_rect(&x,&y,&w,&h);player_mini_no_guia(x,y,w,h);}}
    else if(id==803||(id>=mv_count&&id<4)){g5ui_open("multiview-escolher");}
    else if(id==805)go("multiview-limite");else if(id==810)go("multiview-2");
    else if((id==804||id<mv_count)&&mv_count){int target=id==804?(mv_main+1)%mv_count:id;G5Item swap=mv_items[0];mv_items[0]=mv_items[target];mv_items[target]=swap;mv_main=0;mv_focus=1;}
    return;
  }
}
int g5ui_event(const SDL_Event *e){
  if(!screen[0])return 0;
  if(e->type!=SDL_KEYDOWN)return 1;
  SDL_Keycode k=e->key.keysym.sym;
  if(k==SDLK_ESCAPE||k==SDLK_AC_BACK||k==SDLK_BACKSPACE){
    if(!strcmp(screen,"setup-celular-tv")){celular_fechar();go("setup-tipo");}
    else if(!strncmp(screen,"setup-",6)&&strcmp(screen,"setup-tipo"))go("setup-tipo");
    else if(episodes_mode&&series_catalog){free(inventory);inventory=series_catalog;series_catalog=NULL;count=series_count;selected=parent_series;episodes_mode=0;go("vod-serie-temporadas");}
    else g5ui_close();return 1;
  }
  if(!strcmp(screen,"parental-pin")&&k>=SDLK_0&&k<=SDLK_9){activate(k==SDLK_0?30:19+(int)(k-SDLK_0));return 1;}
  if(k==SDLK_TAB){nav=nav<0?0:-1;return 1;}
  if(nav>=0){if(k==SDLK_LEFT&&nav>0)nav--;if(k==SDLK_RIGHT&&nav<6)nav++;
    if(k==SDLK_DOWN)nav=-1;if((k==SDLK_RETURN||k==SDLK_KP_ENTER)&&!e->key.repeat)activate(1000+nav);return 1;}
  if(k==SDLK_UP||k==SDLK_DOWN||k==SDLK_LEFT||k==SDLK_RIGHT){
    if(!strcmp(screen,"parental-pin")&&focus>=20&&focus<=31){
      int next=focus+(k==SDLK_UP?-3:k==SDLK_DOWN?3:k==SDLK_LEFT?-1:1);
      if(next>=20&&next<=31)focus=next;else focus=k==SDLK_DOWN?0:3;return 1;
    }
    if(!strncmp(screen,"vod-",4)&&strcmp(screen,"vod-titulo")&&strcmp(screen,"vod-serie-temporadas")&&focus>=0&&focus<768&&nindices){
      int step=k==SDLK_UP?-6:k==SDLK_DOWN?6:k==SDLK_LEFT?-1:1;
      if(focus+step<0)nav=0;else if(focus+step<nindices)focus+=step;else if(k==SDLK_RIGHT)focus=800;
      return 1;
    }
    int at=0;for(int i=0;i<num_focus;i++)if(next_focus[i]==focus){at=i;break;}
    int dir=k==SDLK_UP||k==SDLK_LEFT?-1:1;
    if(at+dir<0)nav=0;else if(at+dir<num_focus)focus=next_focus[at+dir];
  }
  if((k==SDLK_RETURN||k==SDLK_KP_ENTER)&&!e->key.repeat)activate(focus);
  return 1;
}
void g5ui_update(void){
  context();
  if(lease.expires&&lease.expires<=time(NULL)){erase(&lease,sizeof lease);if(player_id_canal()[0])player_encerrar();dirty=1;}
  if(catalog_fetch_pending&&!atomic_load(&busy)){catalog_fetch_pending=0;action[0]=0;start(2);}
  if(atomic_load_explicit(&ready,memory_order_acquire)){
    pthread_join(worker,NULL);atomic_store(&busy,0);atomic_store(&ready,0);erase(&work_draft,sizeof work_draft);
    if(!worker_cancel&&worker_profile==profile&&!strcmp(worker_account,account)){
      if(operation==1)test_ok=worker_ok;
      if(operation==3){
        if(worker_ok){if(pin_settings==1)go("parental-ajustes");else if(pin_settings==2){rules.hide_adult=0;save_rules();go("setup-categorias");}
          else{g5_grant(&lease,profile,locked_item.source,locked_item.id,time(NULL),lease_seconds,1);if(locked_item.kind==G5_LIVE)g5ui_close();else go(previous[0]?previous:"vod-filmes");}}
        else{go("parental-pin");snprintf(message,sizeof message,"%s",i18n(pin_result==0?"PIN incorreto":"Não foi possível verificar o PIN"));}
      }else if(staging&&worker_ok){if(operation==4){free(series_catalog);series_catalog=inventory;series_count=count;parent_series=selected;}else free(inventory);inventory=staging;staging=NULL;count=staging_count;total=staging_total;build_groups();source_loaded=1;
        if(operation==4){episodes_mode=1;go("vod-filmes");}
        if(committed)dirty=1;
        if(operation==2&&strstr(screen,"vod-"))focus=0;
      }
      if(!worker_ok&&operation!=3)snprintf(message,sizeof message,"%s",i18n("Falha na conexão"));
    }
    free(staging);staging=NULL;
  }
  if(edit>=0&&!teclado_aberto()){
    int r=teclado_resultado();int field=edit;edit=-1;
    if(r==TECLADO_PRONTO){const char *t=teclado_texto();
      if(field==0){use_saved=0;snprintf(draft.server,sizeof draft.server,"%s",t);}else if(field==1){use_saved=0;snprintf(draft.user,sizeof draft.user,"%s",t);}
      else if(field==2){use_saved=0;snprintf(draft.password,sizeof draft.password,"%s",t);}else if(field==3)snprintf(draft.playlist,sizeof draft.playlist,"%s",t);
      else if(field==4){if(!*t||g5_http_url(t))snprintf(draft.xmltv,sizeof draft.xmltv,"%s",t);}
      else if(field==5)snprintf(query,sizeof query,"%s",t);
      else if(field==6)snprintf(draft.name,sizeof draft.name,"%s",t);
      else if(field==7){snprintf(pin_value,sizeof pin_value,"%s",t);start(3);}
    }teclado_esquecer();
  }
  if(!strcmp(screen,"setup-celular-tv")){
    char data[CEL_CORPO_MAX+1];
    if(celular_pegar(data,sizeof data)){
      draft.type=(int)js_num(data,NULL,"type",0);js_texto(data,NULL,"name",draft.name,sizeof draft.name);js_texto(data,NULL,"server",draft.server,sizeof draft.server);js_texto(data,NULL,"user",draft.user,sizeof draft.user);js_texto(data,NULL,"password",draft.password,sizeof draft.password);
      js_texto(data,NULL,"playlist",draft.playlist,sizeof draft.playlist);js_texto(data,NULL,"xmltv",draft.xmltv,sizeof draft.xmltv);
      draft.live=js_num(data,NULL,"live",1)!=0;draft.movies=js_num(data,NULL,"movies",1)!=0;draft.series=js_num(data,NULL,"series",1)!=0;draft.epg=js_num(data,NULL,"epg",1)!=0;
      char file[3100]="";js_texto_linhas(data,NULL,"file",file,sizeof file);
      if(file[0]){
        G5Playlist p;G5Item *items=calloc(G5_CAP,sizeof *items);g5_playlist_init(&p,items,G5_CAP);
        if(items&&g5_playlist_feed(&p,file,strlen(file))&&g5_playlist_finish(&p)>=0){free(inventory);inventory=items;count=p.count;total=p.total;draft.type=2;snprintf(draft.file,sizeof draft.file,"%s",file);build_groups();test_ok=1;go("setup-importar");}
        else{free(items);snprintf(message,sizeof message,"%s",i18n("Formato não suportado"));}
      }else{use_saved=0;go("setup-teste");start(1);}erase(file,sizeof file);erase(data,sizeof data);celular_fechar();
    }
  }
}
int g5ui_changed(void){int r=dirty;dirty=0;return r;}
int g5ui_play(CatItem *item,char *url,size_t cap){
  if(!playing||selected<0||selected>=count)return 0;playing=0;G5Item *it=&inventory[selected];if(restricted(it))return 0;
  if(it->url[0])snprintf(url,cap,"%s",it->url);else if(!xtream_vod_url(it->id,it->ext[0]?it->ext:"mp4",it->kind==G5_EPISODE,url,(unsigned)cap))return 0;
  memset(item,0,sizeof *item);snprintf(item->imdb,sizeof item->imdb,"g5:%s:%s",it->source,it->id);
  snprintf(item->titulo,sizeof item->titulo,"%s",it->name);snprintf(item->tipo,sizeof item->tipo,"movie");snprintf(item->poster,sizeof item->poster,"%s",it->poster);return 1;
}
int g5ui_channel_allowed(const char *id,const char *name,const char *group,const char *source){
  if(committed&&!strncmp(id?id:"","xtream:",7)&&(!draft.live||!group_allowed(group?group:"")))return 0;
  G5Item it={.kind=G5_LIVE,.age=-1};snprintf(it.id,sizeof it.id,"%s",id?id:"");snprintf(it.name,sizeof it.name,"%s",name?name:"");snprintf(it.group,sizeof it.group,"%s",group?group:"");snprintf(it.source,sizeof it.source,"%s",source?source:"");it.adult=adult_group(it.group)||adult_group(it.name);return !restricted(&it);
}
int g5ui_channel_hidden(const char *id,const char *name,const char *group,const char *source){
  if(committed&&!strncmp(id?id:"","xtream:",7)&&(!draft.live||!group_allowed(group?group:"")))return 1;
  return (rules.hide_locked||rules.kids)&&!g5ui_channel_allowed(id,name,group,source);
}
int g5ui_check_channel(const CatItem *item){
  context();
  char group[96]="";guia_info_canal(item->imdb,NULL,NULL,group,sizeof group);
  if(!group[0])snprintf(group,sizeof group,"%s",item->genero);
  memset(&locked_item,0,sizeof locked_item);
  snprintf(locked_item.id,sizeof locked_item.id,"%s",item->imdb);snprintf(locked_item.group,sizeof locked_item.group,"%s",group);
  if(g5ui_channel_allowed(item->imdb,item->titulo,group,guia_canal_origem()))return 1;
  memset(&locked_item,0,sizeof locked_item);snprintf(locked_item.id,sizeof locked_item.id,"%s",item->imdb);snprintf(locked_item.source,sizeof locked_item.source,"%s",guia_canal_origem());snprintf(locked_item.group,sizeof locked_item.group,"%s",group);locked_item.kind=G5_LIVE;go("parental-canal-bloqueado");return 0;
}
int g5ui_multiview_channel(CatItem *item){
  if(!mv_focus||mv_main>=mv_count)return 0;mv_focus=0;G5Item *it=&mv_items[mv_main];if(restricted(it))return 0;
  XtreamConta c;int max=-1,used=0;if(xtream_conta(&c)){max=c.maxConexoes;used=c.conexoes;}
  if(!g5_live_slots(1,1,max,used)){go("multiview-limite");return 0;}
  memset(item,0,sizeof *item);snprintf(item->imdb,sizeof item->imdb,it->url[0]?"g5live:%s":!strcmp(it->source,"xtream")&&!strchr(it->id,':')?"xtream:%s":"%s",it->id);
  snprintf(item->titulo,sizeof item->titulo,"%s",it->name);snprintf(item->tipo,sizeof item->tipo,"channel");return 1;
}
int g5ui_live(const char *id,char *url,size_t cap){
  if(strncmp(id,"g5live:",7))return 0;
  for(int i=0;i<mv_count;i++)if(!strcmp(id+7,mv_items[i].id)&&mv_items[i].url[0]&&!restricted(&mv_items[i])){snprintf(url,cap,"%s",mv_items[i].url);return 1;}
  for(int i=0;i<count;i++)if(inventory[i].kind==G5_LIVE&&!strcmp(id+7,inventory[i].id)&&group_allowed(inventory[i].group)&&!restricted(&inventory[i])){snprintf(url,cap,"%s",inventory[i].url);return 1;}return 0;
}
int g5ui_m3u_channels(G5Item *out,int cap){
  context();int n=0;if(!committed||!draft.live||draft.type==0)return 0;for(int i=0;i<count&&n<cap;i++)if(inventory[i].kind==G5_LIVE&&group_allowed(inventory[i].group))out[n++]=inventory[i];return n;
}
const char *g5ui_xmltv(void){return draft.epg?draft.xmltv:"";}
int g5ui_provider_epg(void){return !committed||draft.epg;}
