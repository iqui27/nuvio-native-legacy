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
#include "tela21.h"
#include "lembrete.h"
#include "guialembrete.h"
#include "segurar.h"
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
static int lease_seconds=7200, pin_settings, pin_result, rules_mode;
static int episodes_mode;
static char groups[G5_GROUPS][64];
static unsigned char group_on[G5_GROUPS];
static int group_count, group_totals[G5_GROUPS];
/* Phone-guided setup: 1 = test/fetch posted, waiting worker; 2 = counts on the
 * phone, waiting for the final choices. Status never carries credentials. */
static int phone_stage, work_phone;
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
  if(screen[0])celular_fechar();phone_stage=0;screen[0]=0;
  profile=p;snprintf(account,sizeof account,"%s",owner);
  worker_cancel=1;clear_catalog();erase(&draft,sizeof draft);erase(&lease,sizeof lease);committed=0;catalog_fetch_pending=0;
  memset(&rules,0,sizeof rules);rules.hide_adult=1;rules.max_age=18;group_count=0;
  char file[64];filename(file,sizeof file,"rules");char *b=dados_ler(file);
  if(b){js_texto(b,NULL,"group",rules.blocked_group,sizeof rules.blocked_group);js_texto(b,NULL,"channel",rules.blocked_channel,sizeof rules.blocked_channel);
    const char *r=js_array(b,NULL,"groups");while(r&&rules.group_count<G5_RULES_MAX){js_cadeia(r,rules.blocked_groups[rules.group_count++],64);r=js_prox(js_fim(r));}
    r=js_array(b,NULL,"channels");while(r&&rules.channel_count<G5_RULES_MAX){js_cadeia(r,rules.blocked_channels[rules.channel_count++],80);r=js_prox(js_fim(r));}
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
    if(g==group_count&&g<G5_GROUPS){snprintf(groups[g],sizeof groups[g],"%s",inventory[i].group);group_on[g]=!inventory[i].adult;group_totals[g]=0;group_count++;}
    if(g<G5_GROUPS)group_totals[g]++;
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
          staging_count+=n;staging_total+=t;
          if(work_phone){char st[160];int l=0,m=0,s=0;for(int i=0;i<staging_count;i++){if(staging[i].kind==G5_LIVE)l++;else if(staging[i].kind==G5_MOVIE)m++;else if(staging[i].kind==G5_SERIES)s++;}
            snprintf(st,sizeof st,"{\"s\":\"fetch\",\"live\":%d,\"movies\":%d,\"series\":%d}",l,m,s);celular_status(st);}
          if(action[0]||staging_count>=G5_CAP)break;
        }
      }
    }
  }
  atomic_store_explicit(&ready,1,memory_order_release);return NULL;
}
static int start(int op){
  if(atomic_load(&busy))return 0;
  work_draft=draft;work_phone=phone_stage==1||phone_stage==4;
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
static int modal_tela(const char *s);
static int foco_pagina;   /* page focus kept while a modal is open */
static void go(const char *name){
  int de=modal_tela(screen),para=modal_tela(name);
  if(!de&&para)foco_pagina=focus;
  snprintf(screen,sizeof screen,"%s",name);focus=de&&!para?foco_pagina:0;nav=-1;message[0]=0;
}
/* The guide page under every v5 screen: leaving a sub-screen returns there
 * instead of exposing the legacy guide drawn when screen[] is empty. */
static char gv_pagina[24]="grade";
static void gv_voltar(void);
static void gv_tecla_solta(SDL_Keycode k);
static int gv_is(const char *s);
static int gv_overlay_nome(const char *s);
static void gv_abrir(const char *name);
void g5ui_open(const char *name){context();go(name);edit=-1;
  if(gv_is(name))gv_abrir(name);
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
  if((!strcmp(name,"setup-credenciais")||!strcmp(name,"fontes-gerenciar"))&&use_saved)load_saved_credentials();
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
void g5ui_close(void){celular_fechar();phone_stage=0;screen[0]=0;edit=-1;worker_cancel=1;erase(draft.password,sizeof draft.password);nav=-1;}
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
  jsw_cs(&w,"group",rules.blocked_group);jsw_cs(&w,"channel",rules.blocked_channel);
  jsw_chave(&w,"groups");jsw_arr_ini(&w);for(int i=0;i<rules.group_count;i++)jsw_str(&w,rules.blocked_groups[i]);jsw_arr_fim(&w);
  jsw_chave(&w,"channels");jsw_arr_ini(&w);for(int i=0;i<rules.channel_count;i++)jsw_str(&w,rules.blocked_channels[i]);jsw_arr_fim(&w);jsw_obj_fim(&w);
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
/* Phone setup: a short pairing SESSION (test -> choices -> done), same token,
 * Host/Origin and expiry as the single-use pairing. The page only builds JSON
 * form values and polls a credential-free status; the native worker validates
 * everything and talks to the provider. */
static void phone_status_counts(const char *state);
static void phone(void){
  static char html[24000];
  snprintf(html,sizeof html,"<!doctype html><html lang='pt-BR'><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'><title>%s</title>"
    "<style>*{box-sizing:border-box}body{margin:0;background:#0d0d0d;color:#f3f2ef;font:16px system-ui}main{max-width:480px;margin:auto;padding:20px 22px 16px}h1{font-size:27px;line-height:1.1;margin:14px 0 8px}p{color:#b2b4b8;line-height:1.45;margin:0 0 14px;font-size:14px}[hidden]{display:none!important}"
    ".k{font-size:12px;letter-spacing:.12em;color:#b2b4b8}.rail{display:flex;gap:6px;margin:14px 0 4px}.rail i{flex:1;height:4px;border-radius:2px;background:#343941}.rail i.on{background:#bbcfc3}"
    ".row{display:flex;justify-content:space-between;align-items:center;margin:10px 0 6px}.chip{background:#24262c;border-radius:10px;padding:6px 12px;font-size:13px}"
    "label{display:block;font-size:13px;color:#c9cbce;margin-top:8px}input,select{font:inherit;border:1px solid #42464e;border-radius:14px;padding:11px 12px;background:#22252b;color:#f3f2ef;width:100%%;margin-top:4px}"
    "button{font:inherit;border:0;border-radius:32px;background:#e5e9e6;color:#15171b;font-weight:600;width:100%%;padding:15px;margin-top:16px;display:flex;justify-content:center;align-items:center}button.sec{background:#24262c;color:#f3f2ef}"
    ".note{display:flex;gap:10px;align-items:center;background:#1a1c21;border-radius:14px;padding:12px 14px;font-size:13px;color:#c9cbce;margin-top:14px}"
    ".ft{text-align:center;color:#8b8e94;font-size:12px;margin-top:14px}input[type=checkbox]{width:22px;height:22px;margin:0 12px 0 0;vertical-align:middle;accent-color:#e5e9e6}"
    "#s2 label.tipo{display:flex;justify-content:space-between;align-items:center;border-bottom:1px solid #26292f;padding:10px 0;font-size:16px;color:#f3f2ef}#s2 label.tipo span:last-child{color:#8b8e94;font-size:14px}"
    "#cats{background:#17191e;border-radius:16px;padding:6px 14px;margin-top:14px}#cats h2{font-size:16px;margin:8px 0 2px}#cats .sel{float:right;font-size:13px;color:#b2b4b8;margin-top:10px}"
    "#cats label{display:flex;justify-content:space-between;align-items:center;border-bottom:1px solid #26292f;padding:9px 0;font-size:15px}#cats label span:last-child{color:#8b8e94;font-size:13px}"
    ".okc{width:64px;height:64px;border-radius:50%%;background:#bbcfc3;color:#15171b;font-size:30px;display:flex;align-items:center;justify-content:center;margin:26px 0 6px}"
    ".resumo{background:#17191e;border-radius:16px;padding:6px 16px;margin-top:16px}.resumo div{display:flex;justify-content:space-between;border-bottom:1px solid #26292f;padding:12px 0;font-size:15px}.resumo div span:last-child{color:#8b8e94;font-size:13px}</style>"
    "<main><div class='row' style='margin-top:0'><div class=k>NUVIO LEGACY</div><div class=chip>%s</div></div><div class=rail><i class=on id=r1></i><i id=r2></i><i id=r3></i></div>"
    "<form method=post action='' id=f><section id=s1><h1>%s</h1><p>%s</p>"
    "<div class=row><select id=kind style='width:auto'><option value=0>Xtream Codes</option><option value=1>M3U URL</option></select><span class=chip>1 / 3</span></div>"
    "<label>%s<input id=name autocomplete=off maxlength=95 placeholder='%s'></label><div id=xtream><label>%s<input id=server type=url autocomplete=off placeholder='https://provider.example'></label><label>%s<input id=user autocomplete=off placeholder='%s'></label><label>%s<input id=password type=password autocomplete=off placeholder='%s'></label></div>"
    "<div id=m3u hidden><label>%s<input id=playlist type=url autocomplete=off></label><label>%s<input id=file type=file accept='.m3u,.m3u8,text/plain'></label></div>"
    "<div class=note><svg width='18' height='18' viewBox='0 0 24 24' fill='none' stroke='#b2b4b8' stroke-width='2'><rect x='3' y='11' width='18' height='11' rx='2'/><path d='M7 11V7a5 5 0 0 1 10 0v4'/></svg> %s</div><button type=button id=next><span>%s</span><svg width='20' height='20' viewBox='0 0 24 24' fill='none' stroke='#15171b' stroke-width='2.4' style='margin-left:8px'><path d='M9 18l6-6-6-6'/></svg></button><div class=ft>%s</div></section>"
    "<section id=s2 hidden><div class=row><span class=chip id=prov></span><span class=chip>2 / 3</span></div><h1>%s</h1><p id=sub2></p>"
    "<label class=tipo><span><input type=checkbox id=live checked>%s</span><span id=nlive></span></label>"
    "<label class=tipo><span><input type=checkbox id=movies checked>%s</span><span id=nmovies></span></label>"
    "<label class=tipo><span><input type=checkbox id=series checked>%s</span><span id=nseries></span></label>"
    "<label class=tipo><span><input type=checkbox id=epg checked>%s</span><span></span></label>"
    "<div id=cats hidden><span class=sel id=selall>%s</span><h2>%s</h2><div id=catlist></div></div>"
    "<p style='margin-top:12px'>%s</p><label id=lx hidden>%s<input id=xmltv type=url></label>"
    "<button type=button id=send>%s</button><button type=button class=sec id=back>%s</button></section>"
    "<section id=s3 hidden><div class=rail style='display:none'></div><div class=okc>&#10003;</div><h1>%s</h1><p>%s</p>"
    "<div class=resumo><div><span>%s</span><span id=rlive></span></div><div><span>%s</span><span id=rvod></span></div><div><span>%s</span><span>%s</span></div></div>"
    "<button type=button id=finish>%s</button><div class=ft>%s</div></section>"
    "<input type=hidden name=t id=t></form></main>"
    "<script>const $=id=>document.getElementById(id);let fileText='',last=null;"
    "const post=o=>fetch(location.pathname,{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:new URLSearchParams({t:JSON.stringify(o)})});"
    "const sleep=ms=>new Promise(r=>setTimeout(r,ms));"
    "async function poll(want){for(let n=0;n<300;n++){try{const r=await fetch(location.pathname+'/s',{signal:AbortSignal.timeout(5000)});if(!r.ok)break;const s=JSON.parse(await r.text());last=s;if(s.s==='err'){fail();return null;}if(s.s===want)return s;}catch(e){break;}await sleep(1000);}fail();return null;}"
    "function creds(step){return {step,name:$('name').value,type:Number($('kind').value),server:$('server').value,user:$('user').value,password:$('password').value,playlist:$('playlist').value,file:fileText};}"
    "function fail(){$('next').disabled=false;$('next').textContent='%s';}"
    "function step2(s){$('s1').hidden=true;$('s2').hidden=false;$('r1').className='on';$('r2').className='on';$('prov').textContent=$('name').value||($('kind').value==='0'?'Xtream Codes':'M3U');$('sub2').textContent='%s';"
    "$('nlive').textContent=s.live||0;$('nmovies').textContent=s.movies||0;$('nseries').textContent=s.series||0;"
    "$('catlist').replaceChildren();window._g=s.groups||[];$('cats').hidden=!window._g.length;window._g.forEach((g,i)=>{const l=document.createElement('label'),a=document.createElement('span'),b=document.createElement('span'),c=document.createElement('input');c.type='checkbox';c.className='cat';c.id='cat'+i;c.checked=g.on!==0;a.append(c,document.createTextNode(g.n));b.textContent=g.c;l.append(a,b);$('catlist').append(l);});}"
    "function step3(s){$('s2').hidden=true;$('s3').hidden=false;$('r3').className='on';$('rlive').textContent=(s.live||0)+' %s';$('rvod').textContent=((s.movies||0)+(s.series||0))+' %s';}"
    "$('file').onchange=async()=>{const f=$('file').files[0];if(f&&f.size<=3000)fileText=await f.text();else alert('%s');};"
    "$('kind').onchange=()=>{$('xtream').hidden=$('kind').value!=='0';$('m3u').hidden=$('kind').value==='0';};"
    "$('selall').onclick=()=>{document.querySelectorAll('.cat').forEach(c=>c.checked=true);};"
    "$('epg').onchange=()=>{$('lx').hidden=!$('epg').checked;};"
    "$('next').onclick=async()=>{if($('kind').value==='0'&&(!$('server').value||!$('user').value||!$('password').value)){$('server').focus();return;}if($('kind').value!=='0'&&!$('playlist').value&&!fileText){$('playlist').focus();return;}"
    "$('next').disabled=true;$('next').textContent='%s';try{const r=await post(creds(1));if(!r.ok)throw Error();const s=await poll('ok');if(s)step2(s);}catch(e){fail();}};"
    "$('back').onclick=()=>{$('s1').hidden=false;$('s2').hidden=true;$('next').disabled=false;};"
    "$('send').onclick=async()=>{let o={step:2,live:+$('live').checked,movies:+$('movies').checked,series:+$('series').checked,epg:+$('epg').checked,xmltv:$('xmltv').value};"
    "o.off=(window._g||[]).map((g,i)=>i).filter(i=>!$('cat'+i).checked);$('send').disabled=true;try{const r=await post(o);if(!r.ok)throw Error();const s=await poll('done');if(s)step3(s);else fail();}catch(e){fail();}$('send').disabled=false;};"
    "$('finish').onclick=async()=>{try{const r=await post({step:3});if(r.ok){$('finish').disabled=true;$('finish').textContent=$('s3 h1').textContent;}}catch(e){fail();}};$('f').onsubmit=()=>false;</script></html>",
    i18n("Guia TV"),i18n("Guia TV"),
    i18n("Sua fonte. Pronta para a sua TV."),i18n("Use os dados enviados pelo seu provedor."),
    i18n("Nome da fonte"),i18n("Ex.: IPTV da sala"),i18n("Servidor"),i18n("Usuário"),i18n("Usuário do provedor"),i18n("Senha"),i18n("Senha do provedor"),i18n("URL da lista M3U"),i18n("Arquivo"),
    i18n("Os dados ficam só na sua TV."),i18n("Testar conexão"),i18n("Conectado à TV"),
    i18n("Escolha o que importar"),i18n("Canais ao vivo"),i18n("Filmes"),i18n("Séries"),i18n("Guia de programação"),
    i18n("Selecionar tudo"),i18n("Categorias"),i18n("Conteúdo adulto oculto por padrão."),i18n("URL XMLTV"),i18n("Revisar e adicionar à TV"),i18n("Voltar"),
    i18n("Tudo certo. Olhe para a TV."),i18n("Suas escolhas já estão na tela grande."),i18n("Canais ao vivo"),i18n("Filmes e séries"),i18n("Conteúdo adulto"),i18n("Oculto"),
    i18n("Concluir"),i18n("Guia TV"),
    i18n("Não foi possível conectar"),i18n("Conectado. Escolha o conteúdo e os grupos antes de importar."),
    i18n("canais"),i18n("títulos"),i18n("Arquivo muito grande. Use uma URL."),i18n("Testando conexão…"));
  if(celular_abrir_sessao("Guia TV",html))go("setup-celular-tv");else snprintf(message,sizeof message,"%s",i18n("Não foi possível conectar"));
}
/* Short public session status for the phone page: state and counts only. */
static void phone_status_counts(const char *state){
  int nl=0,nm=0,ns=0;for(int i=0;i<count;i++){if(!group_allowed(inventory[i].group))continue;if(inventory[i].kind==G5_LIVE)nl++;else if(inventory[i].kind==G5_MOVIE)nm++;else if(inventory[i].kind==G5_SERIES)ns++;}
  Jsw w;jsw_iniciar(&w);jsw_obj_ini(&w);jsw_cs(&w,"s",state);
  jsw_chave(&w,"live");jsw_int(&w,nl);jsw_chave(&w,"movies");jsw_int(&w,nm);jsw_chave(&w,"series");jsw_int(&w,ns);
  jsw_chave(&w,"groups");jsw_arr_ini(&w);for(int i=0;i<group_count;i++){jsw_obj_ini(&w);jsw_cs(&w,"n",groups[i]);jsw_ci(&w,"c",group_totals[i]);jsw_ci(&w,"on",group_on[i]);jsw_obj_fim(&w);}jsw_arr_fim(&w);jsw_obj_fim(&w);
  const char *j=jsw_texto_final(&w);celular_status(j?j:"{\"s\":\"err\"}");jsw_livre(&w);
}
static void drop_kind(int kind){int w=0;for(int i=0;i<count;i++)if(inventory[i].kind!=kind)inventory[w++]=inventory[i];count=w;}
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
  TxtLinha l=txt_linha_corta_bruta(style,text,dim?170:243,dim?173:242,dim?181:239,255,width);txt_desenhar(l,x,y);
}
static void panel(float x,float y,float w,float h){t21_ilha((GfxRect){x,y,w,h},28,1);}
/* Modal over a screen (PIN, unlock, multiview picker/limits): veil, sheet
 * material and the .96 -> 1 entrance of the guide sheets. Only the modal's
 * controls stay in the D-pad, never the page behind it. */
static float modal_t=1,page_t=1;
static void modal(float x,float y,float w,float h){
  float t=anim_suave(modal_t);
  gfx_cor((GfxRect){0,0,1920,1080},0,0,0,0,T21_VEIL*t);
  gfx_opacidade_grupo=t;
  if(t<1)gfx_transformar(960,540,anim_mistura(T21_SHEET_SCALE,1,t),0,0);
  t21_ilha_folha((GfxRect){x,y,w,h},T21_R_SHEET,1);
  num_focus=0;
}
static int modal_tela(const char *s){
  return !strcmp(s,"parental-pin")||!strcmp(s,"parental-desbloqueio")||!strcmp(s,"multiview-escolher")||!strcmp(s,"multiview-limite");
}
/* D-pad order: each control registers its rectangle while drawing; arrows go
 * to the nearest control in that direction (left/right stay on the row). */
static GfxRect focus_rect[1024];
static void reg_focus(int id,float x,float y,float w,float h){
  if(num_focus<1024){focus_rect[num_focus]=(GfxRect){x,y,w,h};next_focus[num_focus++]=id;}
}
static int nav_spatial(SDL_Keycode k){
  int at=-1;for(int i=0;i<num_focus;i++)if(next_focus[i]==focus){at=i;break;}
  if(at<0){if(!num_focus)return 0;focus=next_focus[0];return 1;}
  GfxRect a=focus_rect[at];float ax=a.x+a.w/2,ay=a.y+a.h/2,best=1e9f;int pick=-1;
  for(int i=0;i<num_focus;i++){
    if(i==at)continue;
    GfxRect b=focus_rect[i];float bx=b.x+b.w/2,by=b.y+b.h/2,main,side;
    float ov_y=fminf(a.y+a.h,b.y+b.h)-fmaxf(a.y,b.y),ov_x=fminf(a.x+a.w,b.x+b.w)-fmaxf(a.x,b.x);
    if(k==SDLK_RIGHT||k==SDLK_LEFT){
      main=k==SDLK_RIGHT?bx-ax:ax-bx;if(main<=1)continue;
      side=ov_y>0?0:fabsf(by-ay);
    }else{
      main=k==SDLK_DOWN?by-ay:ay-by;if(main<=1)continue;
      side=ov_x>0?0:fabsf(bx-ax);
    }
    float d=main+3*side;if(d<best){best=d;pick=i;}
  }
  if(pick<0)return 0;
  focus=next_focus[pick];return 1;
}
static int button_lit;   /* next button is the selected one of a segment */
static void button(int id,const char *text,const char *icon,float x,float y,float w,float h,int row){
  GfxRect r={x,y,w,h};int focused=nav<0&&focus==id,lit=button_lit&&!row;button_lit=0;
  float ar=1,ag=1,ab=1;if(lit)ajustes_acento(&ar,&ag,&ab);
  if(row){if(focused)plrui_linha_foco(r,20,1);}
  else if(focused)plrui_pilula_foco(r,1);
  else if(lit)gfx_cor(r,.5f,ar,ag,ab,.22f);
  else if(h<=54)gfx_cor(r,.5f,.141f,.149f,.173f,1);
  else plrui_botao_repouso(r,1);
  int c=focused&&!row?ajustes_tinta_foco():240;float tint=c/255.0f;
  float ir=lit&&!focused?ar:tint,ig=lit&&!focused?ag:tint,ib=lit&&!focused?ab:tint;
  if(icon&&*icon)gfx_icone((GfxRect){x+24,y+(h-28)/2,28,28},icon,ir,ig,ib,1);
  TxtLinha l=txt_linha_corta_bruta(TXT_ILHA_CORPO,text,(int)(ir*255),(int)(ig*255),(int)(ib*255),255,w-(icon?84:48));
  txt_desenhar(l,icon||row?x+(icon?68:24):x+(w-l.w)/2,y+(h-l.h)/2);
  ponteiro_alvo(x,y,w,h,point_focus,point_activate,id,0);
  if(id>=control_count)control_count=id+1;
  reg_focus(id,x,y,w,h);
}
static void toggle(int id,const char *name,int on,float x,float y,float w){button(id,name,on?"aj_check":"aj_plus",x,y,w,64,1);}
static void chip(float x,float y,float w,const char *text,int dim){
  GfxRect r={x,y,w,36};gfx_cor(r,18.0f/36,.14f,.15f,.17f,.9f);
  label(TXT_ILHA_CORPO,text,x+16,y+8,w-28,dim);
}
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
  {const char *k[]={"OK","Voltar"},*l[]={"Selecionar","Voltar ao guia"};plrui_dicas(k,l,2,48,1042,0,1);}
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
    else{TxtLinha number=txt_linha_bruta(TXT_ILHA_CORPO,n,step==i?18:170,step==i?19:173,step==i?22:181,255);txt_desenhar(number,94,y+21);}
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
      if(focus==i&&nav<0)plrui_linha_foco(r,28,1);else gfx_cor(r,.1,.135,.145,.165,1);gfx_icone((GfxRect){x+28,y+30,38,38},icons[i],.95,.95,.95,1);
      label(TXT_HEADLINE,i18n(names[i]),x+28,y+80,610,0);label(TXT_ILHA_CORPO,i18n(subs[i]),x+28,y+132,610,1);
      if(i<2){gfx_cor((GfxRect){x+28,y+177,440,40},.25,1,1,1,.035);label(TXT_ILHA_CORPO,i18n(i==0?"Canais ao vivo":"Seu provedor define o conteúdo disponível."),x+40,y+184,418,0);}
      ponteiro_alvo(x,y,667,r.h,point_focus,point_activate,i,0);control_count=4;reg_focus(i,x,y,667,r.h);
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
      button(id,i==3?(draft.password[0]?"••••••••":i18n("Senha")):value[0]?value:i18n(i==0?"Sua fonte":i==1?"Servidor":"Usuário"),NULL,474,y+26,702,60,1);
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
  else if(phone_stage==4)label(TXT_ILHA_CORPO,i18n("Testando conexão…"),102,450,660,0);
  else if(phone_stage==2)label(TXT_ILHA_CORPO,i18n("Confira as opções e continue na TV."),102,450,660,0);
  else label(TXT_ILHA_CORPO,i18n("Pareamento expirado"),102,450,660,0);
  const char *names[]={"Dados do provedor","Teste de conexão","Escolha o que importar","Confira as opções e continue na TV."};
  for(int i=0;i<4;i++)label(TXT_ILHA_CORPO,i18n(names[i]),876,410+i*108,830,1);
  button(4,i18n("Continuar na TV"),"pl_chevron-left",102,880,400,60,0);button(5,i18n("Novo QR"),"aj_rotate-cw",550,880,260,60,0);
}
static void sources_screen(void){
  label(TXT_ILHA_SUB,i18n("GUIA TV / FONTES"),48,150,1600,1);label(TXT_TITULO2,i18n("Seus provedores, num só lugar."),48,194,1450,0);
  button(0,i18n("Adicionar fonte"),"aj_plus",1450,176,420,64,0);
  panel(48,320,1824,180);gfx_icone((GfxRect){84,368,36,36},"aj_tv-minimal-play",.9f,.9f,.9f,1);label(TXT_HEADLINE,draft.name[0]?draft.name:xtream_configurado()?"Xtream Codes":draft.playlist[0]?"M3U":i18n("Nenhuma fonte"),140,350,900,0);
  label(TXT_ILHA_CORPO,i18n(atomic_load(&busy)?"Atualizando…":source_loaded?"Configurado":"Não configurado"),140,410,1000,1);
  button(1,i18n("Atualizar"),"aj_rotate-cw",1090,375,240,64,0);button(2,i18n("Editar"),"aj_settings-2",1344,375,225,64,0);button(3,i18n(remove_confirm?"Confirmar remoção":"Remover"),"aj_trash-2",1580,375,250,64,0);
  button(4,i18n("Controle parental"),"aj_shield-check",48,650,450,64,0);button(5,i18n("Guia de programação"),"aj_clock",530,650,450,64,0);
}
static void catalog(void){
  int series=!strcmp(screen,"vod-series"), search=!strcmp(screen,"vod-busca");filter(episodes_mode?G5_EPISODE:search?-1:series?G5_SERIES:G5_MOVIE);
  int nl=0,nm=0,ns=0;for(int i=0;i<count;i++){if(inventory[i].kind==G5_LIVE)nl++;else if(inventory[i].kind==G5_MOVIE)nm++;else if(inventory[i].kind==G5_SERIES)ns++;}
  /* Status pills: provider, catalog sizes, manage. Catch-up and identity chips
   * only when the source really announces them (kept honest). */
  char pill[160];const char *prov=draft.name[0]?draft.name:i18n("Do seu IPTV");
  snprintf(pill,sizeof pill,"%s · %s",prov,i18n("Conectado"));chip(48,112,430,pill,1);
  snprintf(pill,sizeof pill,i18n("%d filmes · %d séries"),nm,ns);chip(496,112,450,pill,1);
  button(803,i18n("Gerenciar fontes"),"aj_settings-2",1566,104,306,52,0);
  panel(48,190,320,800);label(TXT_ILHA_NOME,i18n("Catálogo do provedor"),80,217,270,0);
  char nbuf[24];
  button(800,i18n("Filmes"),"aj_film",68,258,280,54,1);snprintf(nbuf,sizeof nbuf,"%d",nm);label(TXT_ILHA_CORPO,nbuf,286,270,62,1);
  button(801,i18n("Séries"),"aj_tv-minimal-play",68,312,280,54,1);snprintf(nbuf,sizeof nbuf,"%d",ns);label(TXT_ILHA_CORPO,nbuf,286,324,62,1);
  button(802,i18n("Buscar"),"aj_search",68,366,280,54,1);
  gfx_cor((GfxRect){68,439,280,1},0,1,1,1,.10);label(TXT_ILHA_NOME,i18n("Categorias"),80,462,270,0);
  button(805,i18n("Todas as categorias"),"aj_folders",68,502,280,54,1);snprintf(nbuf,sizeof nbuf,"%d",nindices);label(TXT_ILHA_CORPO,nbuf,286,514,62,1);
  for(int g=0;g<group_count&&g<4;g++){button(820+g,groups[g],"aj_folders",68,563+g*62,280,54,1);
    snprintf(nbuf,sizeof nbuf,"%d",group_totals[g]);label(TXT_ILHA_CORPO,nbuf,286,575+g*62,62,1);}
  label(TXT_ILHA_SUB,i18n("Conteúdo adulto oculto por padrão."),80,884,270,1);
  button(806,i18n("Adicionar fonte"),"aj_plus",68,915,280,54,1);
  if(search)button(804,query[0]?query:i18n("Buscar canais, filmes e séries"),"aj_search",400,204,1472,64,1);
  else if(nindices&&!atomic_load(&busy)){
    G5Item *hero=&inventory[indices[focus>=0&&focus<nindices?focus:0]];
    GfxRect r={400,190,1472,330};panel(r.x,r.y,r.w,r.h);
    GLuint art=!restricted(hero)&&hero->backdrop[0]?tex_obter_larg(hero->backdrop,1472):0;
    if(art){gfx_tex_aspect_atual=tex_aspecto(hero->backdrop);gfx_rect(r,art,GFX_CARD,0,0,0,.08,1,1,1,1);gfx_tex_aspect_atual=0;}
    gfx_rect(r,0,GFX_VEU,0,0,0,.08,0,0,0,.75);
    label(TXT_ILHA_CORPO,i18n("Do seu IPTV"),432,218,1200,1);
    label(TXT_V2_TIT,restricted(hero)?i18n("Conteúdo bloqueado"):hero->name,432,258,1350,0);
    chip(432,348,150,i18n("Seu IPTV"),1);
    if(hero->age>=0&&!restricted(hero)){snprintf(pill,sizeof pill,i18n("%d+"),hero->age);chip(594,348,86,pill,1);}
    if(!restricted(hero)&&hero->group[0])chip(692,348,330,hero->group,1);
    label(TXT_ILHA_CORPO,i18n("Seu provedor define o conteúdo disponível."),432,402,1350,0);
    button(807,i18n("Detalhes"),"aj_play",432,446,260,60,0);button(series?800:801,i18n(series?"Filmes":"Séries"),"aj_chevron-right",710,446,260,60,0);
    label(TXT_HEADLINE,i18n(series?"Séries do seu provedor":"Filmes do seu provedor"),400,548,900,0);
    snprintf(pill,sizeof pill,i18n("%d títulos"),nindices);label(TXT_ILHA_SUB,pill,1280,560,590,1);
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
    if(tex){gfx_tex_aspect_atual=tex_aspecto(it->poster);gfx_rect(r,tex,GFX_CARD,0,0,0,.07,1,1,1,1);gfx_tex_aspect_atual=0;}else gfx_cor(r,.07,.12,.13,.15,1);
    gfx_cor((GfxRect){x+10,y+10,134,36},.25f,.05f,.05f,.06f,.85f);label(TXT_ILHA_SUB,i18n("Seu IPTV"),x+20,y+16,114,0);
    if(locked)gfx_icone((GfxRect){x+90,y+108,48,48},"aj_lock",.8,.8,.8,1);
    if(focus==n&&nav<0)plrui_linha_foco((GfxRect){x-4,y+274,238,76},18,1);
    label(TXT_W20_24B,locked?i18n("Conteúdo bloqueado"):it->name,x,y+282,230,0);
    char meta[96];if(!locked&&it->age>=0)snprintf(meta,sizeof meta,i18n("%s · %d+"),g5_item_category(it,it->kind),it->age);
    else snprintf(meta,sizeof meta,"%s",g5_item_category(it,it->kind));
    label(TXT_ILHA_CORPO,meta,x,y+318,230,1);
    ponteiro_alvo(x,y,230,350,point_focus,point_activate,n,0);
    reg_focus(n,x,y,230,350);
  }
  control_count=nindices;
}
static void title_screen(void){
  if(selected<0||selected>=count){go("vod-vazio");return;}
  G5Item *it=episodes_mode&&series_catalog?&series_catalog[parent_series]:&inventory[selected];if(restricted(it)){locked_item=*it;go("parental-canal-bloqueado");return;}
  if(it->kind==G5_SERIES){
    panel(48,190,320,800);label(TXT_ILHA_NOME,i18n("Catálogo"),80,217,270,0);
    button(800,i18n("Filmes"),"aj_film",68,258,280,54,1);button(801,i18n("Séries"),"aj_tv-minimal-play",68,312,280,54,1);
    button(2,i18n("Voltar"),"pl_chevron-left",68,915,280,54,1);
    GfxRect hero={400,190,1472,290};panel(hero.x,hero.y,hero.w,hero.h);
    GLuint art=it->backdrop[0]?tex_obter_larg(it->backdrop,1472):0;
    if(art){gfx_tex_aspect_atual=tex_aspecto(it->backdrop);gfx_rect(hero,art,GFX_CARD,0,0,0,.09,1,1,1,1);gfx_tex_aspect_atual=0;}
    gfx_rect(hero,0,GFX_VEU,0,0,0,.09,0,0,0,.8);
    label(TXT_ILHA_CORPO,i18n("Séries"),432,224,1200,1);label(TXT_V2_TIT,it->name,432,270,1350,0);
    label(TXT_ILHA_CORPO,i18n("Do seu IPTV"),432,343,1300,0);
    button(1,i18n("Carregar episódios"),"aj_play",432,400,400,60,0);
    panel(400,510,360,472);label(TXT_HEADLINE,i18n("Temporada"),432,548,290,0);
    char season[64];snprintf(season,sizeof season,"%s %d",i18n("Temporada"),selected_season);
    button(0,season,"aj_tv-minimal-play",432,600,296,72,1);
    label(TXT_HEADLINE,i18n("Episódios"),790,514,1000,0);
    if(episodes_mode){int row=0;for(int i=0;i<count&&row<4;i++)if(inventory[i].kind==G5_EPISODE){
      G5Item *episode=&inventory[i];float y=562+row*109;GLuint t=episode->poster[0]?tex_obter_larg(episode->poster,146):0;
      if(t){gfx_tex_aspect_atual=tex_aspecto(episode->poster);gfx_rect((GfxRect){808,y,146,78},t,GFX_CARD,0,0,0,.14,1,1,1,1);gfx_tex_aspect_atual=0;}
      button(100+i,episode->name,"aj_play",972,y,880,78,1);gfx_cor((GfxRect){790,y+92,1082,1},0,1,1,1,.12);row++;
    }}else label(TXT_ILHA_CORPO,i18n("Carregar episódios"),808,592,1000,1);
    return;
  }
  panel(48,128,1824,848);GLuint t=it->poster[0]?tex_obter_larg(it->poster,350):0;
  if(t)gfx_rect((GfxRect){100,205,350,525},t,GFX_CARD,0,0,0,.04,1,1,1,1);
  label(TXT_ILHA_SUB,i18n("Do seu IPTV"),510,220,1250,1);label(TXT_TITULO2,it->name,510,278,1250,0);
  txt_bloco_bruto(TXT_ILHA_CORPO,it->plot[0]?it->plot:i18n("Sem descrição"),180,182,190,510,380,1200,36,1,4);
  if(it->kind==G5_SERIES){
    button(0,i18n("Temporada"),"aj_tv-minimal-play",510,590,330,64,0);
    char season[32];snprintf(season,sizeof season,"%d",selected_season);label(TXT_HEADLINE,season,870,600,200,0);
    button(1,i18n("Carregar episódios"),"aj_chevron-right",510,700,480,64,0);
  }else button(0,i18n("Assistir"),"aj_play",510,590,350,64,0);
  button(2,i18n("Voltar"),"pl_chevron-left",510,860,260,60,0);
}
static void parental_screen(void){
  if(!strcmp(screen,"parental-lista")){
    panel(100,128,1720,848);label(TXT_TITULO2,i18n(rules_mode?"Canais ao vivo":"Categorias"),142,170,1450,0);
    int begin=focus>=0&&focus<900?(focus/8)*8:0;
    if(rules_mode){for(int i=begin;i<begin+8;i++){const char *id,*name,*logo,*group,*base;if(!guia_canal_regra_campos(i,&id,&name,&logo,&group,&base))continue;
      int on=!strcmp(rules.blocked_channel,id);for(int j=0;j<rules.channel_count;j++)on|=!strcmp(rules.blocked_channels[j],id);
      toggle(i,name,on,142,270+(i-begin)*70,1620);
    }}else for(int i=begin;i<group_count&&i<begin+8;i++){int on=!strcmp(rules.blocked_group,groups[i]);for(int j=0;j<rules.group_count;j++)on|=!strcmp(rules.blocked_groups[j],groups[i]);toggle(i,groups[i],on,142,270+(i-begin)*70,1620);}
    button(900,i18n("Voltar"),"pl_chevron-left",142,884,280,60,0);return;
  }
  if(!strcmp(screen,"parental-pin")){
    modal(545,119,830,842);label(TXT_ILHA_CORPO,i18n("PIN do perfil"),586,164,740,1);
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
    txt_bloco(TXT_V2_A3TIT,i18n("Este conteúdo precisa do PIN do perfil."),243,242,239,900,292,880,60,1,2);
    label(TXT_ILHA_CORPO,i18n("Imagem, áudio e detalhes ficam ocultos."),900,440,850,1);
    button(0,i18n("Digitar PIN do perfil"),"aj_lock",900,550,780,72,0);button(1,i18n("Escolher outro canal"),"pl_chevron-left",900,650,780,72,0);
    button(2,i18n("Controle parental"),"aj_shield-check",900,750,780,72,0);return;
  }
  label(TXT_ILHA_CORPO,i18n("GUIA TV / REGRAS DO PERFIL"),100,150,1700,1);label(TXT_TITULO2,i18n("Assista com tranquilidade."),100,194,1700,0);
  panel(100,276,390,740);panel(522,276,1298,740);
  label(TXT_HEADLINE,i18n("Proteger este perfil"),132,310,330,0);
  button(90,i18n("Controle parental"),"aj_shield-check",132,358,326,62,1);
  button(91,i18n("Canais ao vivo"),"aj_tv-minimal-play",132,428,326,62,1);
  button(92,i18n("Categorias"),"aj_film",132,498,326,62,1);
  button(93,i18n("Perfil infantil"),"aj_shield-check",132,568,326,62,1);
  gfx_cor((GfxRect){132,652,326,1},0,1,1,1,.13);
  txt_bloco(TXT_ILHA_CORPO,i18n("Use o mesmo PIN do perfil."),180,182,190,132,678,326,34,1,3);
  const char *rows[]={"Ocultar conteúdo adulto","Bloquear canal em foco","Bloquear categoria em foco","Classificação indicativa","Pedir PIN sem classificação","Ocultar completamente","Perfil infantil","Desbloqueio temporário"};
  for(int i=0;i<8;i++){
    int on=i==0?rules.hide_adult:i==1?(rules.blocked_channel[0]!=0||rules.channel_count):i==2?(rules.blocked_group[0]!=0||rules.group_count):i==4?rules.ask_unrated:i==5?rules.hide_locked:i==6?rules.kids:0;
    float y=308+i*82;button(i,i18n(rows[i]),i==0?"aj_eye-off":i==1?"aj_tv-minimal-play":i==2?"aj_folders":i==7?"aj_clock":"aj_shield-check",554,y,1234,78,1);
    const char *state=i==1||i==2?"Editar":i==3?NULL:i==7?NULL:on?"Ativado":"Desativado";char value[80];
    if(i==3){snprintf(value,sizeof value,"%d+",rules.max_age);state=value;}
    if(i==7){snprintf(value,sizeof value,i18n("%d horas"),lease_seconds/3600);state=value;}
    label(TXT_ILHA_CORPO,i18n(state),1530,y+26,225,1);gfx_cor((GfxRect){554,y+81,1234,1},0,1,1,1,.12);
  }
  if(!strcmp(screen,"parental-desbloqueio")){
    modal(530,260,860,580);
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
  *x=48;*y=238;*w=mode==3?1160:904;*h=mode==4?324:650;
}
static void multiview_screen(void){
  label(TXT_HEADLINE,i18n("Todos os jogos. Do seu jeito."),48,145,900,0);
  button_lit=mode==2;button(800,"2","aj_layout-dashboard",1180,132,180,64,0);
  button_lit=mode==3;button(801,"1 + 2","aj_layout-dashboard",1380,132,220,64,0);
  button_lit=mode==4;button(802,"2 × 2","aj_layout-dashboard",1620,132,250,64,0);
  int n=mode;
  for(int i=0;i<n;i++){
    float x,y,w,h;if(mode==3){x=i==0?48:1232;y=i==2?575:238;w=i==0?1160:640;h=i==0?650:313;}
    else{x=48+(i%2)*928;y=238+(i/2)*340;w=904;h=mode==4?324:650;}
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
    modal(1130,128,742,848);label(TXT_HEADLINE,i18n("Adicionar canal"),1174,174,630,0);
    int start=focus>=900?(focus-900)/7*7:0;
    for(int i=start;i<picker_count&&i<start+7;i++)button(900+i,picker_items[i].name,"aj_plus",1174,250+(i-start)*84,654,68,1);
    if(!picker_count)label(TXT_ILHA_CORPO,i18n("Nenhum canal na lista"),1174,262,630,1);
    label(TXT_ILHA_SUB,i18n("Um vídeo ao vivo. Os demais são prévias."),1174,906,630,1);
  }
  if(!strcmp(screen,"multiview-limite")){
    modal(490,235,940,650);
    label(TXT_ILHA_SUB,"MULTIVIEW",546,288,800,1);label(TXT_TITULO2,i18n("Mantenha esta TV fluida."),546,345,820,0);
    label(TXT_ILHA_CORPO,i18n("Esta TV reproduz um vídeo ao vivo."),546,460,820,0);
    label(TXT_ILHA_CORPO,i18n("Outros quadros mostram arte estática da fonte."),546,530,820,1);
    label(TXT_ILHA_CORPO,i18n("Reduzir a qualidade não cria outro decodificador."),546,610,820,1);
    button(810,i18n("Usar um vídeo e prévias"),"aj_check",546,735,820,70,0);
  }
}
/* Guia TV v5: tela principal (definicoes adiante neste arquivo). */
static int gv_is(const char *s);
static void gv_screen(void);
static int gv_event(SDL_Keycode k,int repeat);
static void gv_tick(void);
int g5ui_draw(void){
  if(!screen[0])return 0;
  gfx_cor((GfxRect){0,0,1920,1080},0,.045,.047,.051,1);control_count=0;num_focus=0;
  if(gv_is(screen)){gv_screen();return 1;}
  /* New screen rises in; a modal grows over its page; closing a modal keeps
   * the page still. Same T21 ramps as the guide. */
  static char ult[48];static Uint32 ult_ms;Uint32 ms=SDL_GetTicks();
  float dt=ult_ms&&ms>ult_ms?fminf((float)(ms-ult_ms)/1000.0f,.05f):0;ult_ms=ms;
  if(strcmp(ult,screen)){
    if(modal_tela(screen))modal_t=0;else if(!modal_tela(ult))page_t=0;
    snprintf(ult,sizeof ult,"%s",screen);
  }
  modal_t=t21_entrada(modal_t,dt,1);page_t=t21_entrada(page_t,dt,1);
  float og=gfx_opacidade_grupo,pa=anim_suave(page_t);
  top();
  gfx_opacidade_grupo=og*pa;
  if(pa<1)gfx_transformar(960,540,1,0,18*(1-pa));
  if(!strcmp(screen,"setup-celular-tv"))phone_screen();
  else if(!strncmp(screen,"setup-",6))wizard();
  else if(!strcmp(screen,"fontes-gerenciar"))sources_screen();
  else if(!strcmp(screen,"vod-titulo")||!strcmp(screen,"vod-serie-temporadas"))title_screen();
  else if(!strncmp(screen,"vod-",4))catalog();
  else if(!strcmp(screen,"parental-perfil-infantil"))kids_screen();
  else if(!strncmp(screen,"parental-",9))parental_screen();
  else if(!strncmp(screen,"multiview-",10))multiview_screen();
  gfx_sem_transformar();gfx_opacidade_grupo=og;
  /* One visible focus: a focus id not drawn on this screen goes to the first control. */
  if(nav<0&&num_focus){int ok=0;for(int i=0;i<num_focus;i++)ok|=next_focus[i]==focus;if(!ok)focus=next_focus[0];}
  return 1;
}
static void activate(int id){
  if(id>=1000){if(episodes_mode&&series_catalog){free(inventory);inventory=series_catalog;series_catalog=NULL;count=series_count;selected=parent_series;}episodes_mode=0;int n=id-1000;
    if(n<0||n>6)return;if(n<2){snprintf(gv_pagina,sizeof gv_pagina,"%s",n?"cartoes":"grade");gv_voltar();return;}if(n==5){want_pin(1);return;}g5ui_open(top_destinations[n]);return;}
  if(atomic_load(&busy)){snprintf(message,sizeof message,"%s",i18n("Aguarde…"));return;}
  if(!strcmp(screen,"setup-tipo")){
    if(id==4){gv_voltar();return;}if(id==2){go("setup-arquivo");return;}if(id==3){phone();return;}
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
  if(!strcmp(screen,"setup-pronto")){if(id==0)gv_voltar();else go(id==1?"vod-filmes":"fontes-gerenciar");return;}
  if(!strcmp(screen,"fontes-gerenciar")){
    if(id==0)go("setup-tipo");else if(id==1){use_saved=1;action[0]=0;start(2);}else if(id==2)go("setup-credenciais");
    else if(id==3){if(!remove_confirm){remove_confirm=1;return;}xtream_esquecer();draft.playlist[0]=draft.xmltv[0]=0;char file[64];filename(file,sizeof file,"source");dados_apagar(file);clear_catalog();dirty=1;committed=0;remove_confirm=0;}
    else if(id==4)want_pin(1);else if(id==5)go("setup-epg");return;
  }
  if(!strcmp(screen,"parental-pin")){
    size_t n=strlen(pin_value);
    if(id>=20&&id<=31){if(id==29)erase(pin_value,sizeof pin_value);else if(id==31){if(n)pin_value[n-1]=0;}else if(n<4){pin_value[n]=(char)('0'+(id==30?0:id-19));pin_value[n+1]=0;}return;}
    if(id==0){if(n==4)start(3);return;}
    if(id==1){erase(pin_value,sizeof pin_value);   /* up one level: the screen that asked */
      if(previous[0]&&!gv_is(previous))go(previous);else gv_voltar();return;}
    if(id==3)lease_seconds=lease_seconds==3600?7200:lease_seconds==7200?14400:3600;return;
  }
  if(!strcmp(screen,"parental-canal-bloqueado")){
    if(id==0)want_pin(0);else if(id==1)gv_voltar();else if(id==3)lease_seconds=lease_seconds==3600?7200:lease_seconds==7200?14400:3600;else want_pin(1);return;
  }
  if(!strcmp(screen,"parental-desbloqueio")){
    if(id==10)lease_seconds=lease_seconds==3600?7200:lease_seconds==7200?14400:3600;else if(id==11)want_pin(0);return;
  }
  if(!strcmp(screen,"parental-lista")){
    if(id==900){go("parental-ajustes");return;}
    if(rules_mode){const char *channel_id,*name,*logo,*group,*base;
      if(!guia_canal_regra_campos(id,&channel_id,&name,&logo,&group,&base))return;
      int i;for(i=0;i<rules.channel_count;i++)if(!strcmp(rules.blocked_channels[i],channel_id))break;
      if(i<rules.channel_count){memmove(rules.blocked_channels[i],rules.blocked_channels[i+1],(rules.channel_count-i-1)*sizeof rules.blocked_channels[0]);rules.channel_count--;}
      else if(i<G5_RULES_MAX)snprintf(rules.blocked_channels[rules.channel_count++],80,"%s",channel_id);
      if(!strcmp(rules.blocked_channel,channel_id))rules.blocked_channel[0]=0;
    }else if(id>=0&&id<group_count){int i;for(i=0;i<rules.group_count;i++)if(!strcmp(rules.blocked_groups[i],groups[id]))break;
      if(i<rules.group_count){memmove(rules.blocked_groups[i],rules.blocked_groups[i+1],(rules.group_count-i-1)*sizeof rules.blocked_groups[0]);rules.group_count--;}
      else if(i<G5_RULES_MAX)snprintf(rules.blocked_groups[rules.group_count++],64,"%s",groups[id]);
      if(!strcmp(rules.blocked_group,groups[id]))rules.blocked_group[0]=0;
    }save_rules();return;
  }
  if(!strcmp(screen,"parental-ajustes")){
    if(id==90)return;
    if(id==91||id==92){rules_mode=id==91;go("parental-lista");return;}
    if(id==93)id=6;
    if(id==0)rules.hide_adult=!rules.hide_adult;
    else if(id==1||id==2){rules_mode=id==1;go("parental-lista");return;}
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
    if(id>=100&&id<100+count&&episodes_mode){selected=id-100;if(!restricted(&inventory[selected]))playing=1;return;}
    if(id==800||id==801||id==2){if(episodes_mode&&series_catalog){free(inventory);inventory=series_catalog;series_catalog=NULL;count=series_count;episodes_mode=0;}go(id==800?"vod-filmes":"vod-series");return;}
    if(id==0){selected_season=selected_season>=30?1:selected_season+1;return;}
    if(id==2){go("vod-series");return;}
    if(episodes_mode&&series_catalog){free(inventory);inventory=series_catalog;series_catalog=NULL;count=series_count;selected=parent_series;episodes_mode=0;}
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
  if(e->type==SDL_KEYUP&&gv_is(screen)){gv_tecla_solta(e->key.keysym.sym);return 1;}
  if(e->type!=SDL_KEYDOWN)return 1;
  SDL_Keycode k=e->key.keysym.sym;
  if(gv_is(screen))return gv_event(k,e->key.repeat);
  if(k==SDLK_ESCAPE||k==SDLK_AC_BACK||k==SDLK_BACKSPACE){
    if(!strcmp(screen,"setup-celular-tv")){celular_fechar();go("setup-tipo");}
    else if(!strncmp(screen,"setup-",6)&&strcmp(screen,"setup-tipo"))go("setup-tipo");
    else if(!strcmp(screen,"parental-lista"))go("parental-ajustes");
    else if(episodes_mode&&series_catalog){free(inventory);inventory=series_catalog;series_catalog=NULL;count=series_count;selected=parent_series;episodes_mode=0;go("vod-serie-temporadas");}
    /* modals and detail pages close to the page under them, like their buttons */
    else if(!strcmp(screen,"multiview-escolher")||!strcmp(screen,"multiview-limite"))go(mode==4?"multiview-4":mode==3?"multiview-3":"multiview-2");
    else if(!strcmp(screen,"parental-desbloqueio"))go("parental-ajustes");
    else if(!strcmp(screen,"parental-pin"))activate(1);
    else if(!strcmp(screen,"vod-titulo")||!strcmp(screen,"vod-serie-temporadas"))activate(2);
    else gv_voltar();return 1;
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
    if(!nav_spatial(k)&&k==SDLK_UP)nav=0;
    return 1;
  }
  if((k==SDLK_RETURN||k==SDLK_KP_ENTER)&&!e->key.repeat)activate(focus);
  return 1;
}
void g5ui_update(void){
  context();
  if(gv_is(screen))gv_tick();
  if(lease.expires&&lease.expires<=time(NULL)){erase(&lease,sizeof lease);if(player_id_canal()[0])player_encerrar();dirty=1;}
  if(catalog_fetch_pending&&!atomic_load(&busy)){catalog_fetch_pending=0;action[0]=0;start(2);}
  if(atomic_load_explicit(&ready,memory_order_acquire)){
    int phone_fetch=0;
    pthread_join(worker,NULL);atomic_store(&busy,0);atomic_store(&ready,0);erase(&work_draft,sizeof work_draft);
    if(!worker_cancel&&worker_profile==profile&&!strcmp(worker_account,account)){
      if(operation==1)test_ok=worker_ok;
      if(operation==3){
        if(worker_ok){if(pin_settings==1)go("parental-ajustes");else if(pin_settings==2){rules.hide_adult=0;save_rules();go("setup-categorias");}
          else{g5_grant(&lease,profile,locked_item.source,locked_item.id,time(NULL),lease_seconds,1);if(locked_item.kind==G5_LIVE)g5ui_close();else go(previous[0]?previous:"vod-filmes");}}
        else{go("parental-pin");snprintf(message,sizeof message,"%s",i18n(pin_result==0?"PIN incorreto":"Não foi possível verificar o PIN"));}
      }else if(staging&&worker_ok){if(operation==4){free(series_catalog);series_catalog=inventory;series_count=count;parent_series=selected;}else free(inventory);inventory=staging;staging=NULL;count=staging_count;total=staging_total;build_groups();source_loaded=1;
        if(operation==4){episodes_mode=1;selected=0;go("vod-serie-temporadas");}
        if(committed)dirty=1;
        if(operation==2&&strstr(screen,"vod-"))focus=0;
      }
      if(phone_stage==1&&operation==1){phone_stage=worker_ok?4:0;phone_fetch=worker_ok;if(!worker_ok)celular_status("{\"s\":\"err\"}");}
      else if(phone_stage==4&&operation==2){phone_stage=worker_ok?2:0;if(worker_ok)phone_status_counts("ok");else celular_status("{\"s\":\"err\"}");}
      if(!worker_ok&&operation!=3)snprintf(message,sizeof message,"%s",i18n("Falha na conexão"));
    }
    free(staging);staging=NULL;
    if(phone_fetch&&!start(2)){phone_stage=0;celular_status("{\"s\":\"err\"}");}
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
  if(!strcmp(screen,"setup-celular-tv")||(!strcmp(screen,"setup-pronto")&&phone_stage==3)){
    char data[CEL_CORPO_MAX+1];
    if(celular_pegar(data,sizeof data)){
      int pstep=(int)js_num(data,NULL,"step",0);
      if(atomic_load(&busy))erase(data,sizeof data);
      else if(pstep==3&&phone_stage==3){erase(data,sizeof data);gv_voltar();}
      if(pstep==0||pstep==1){
        draft.type=(int)js_num(data,NULL,"type",0);js_texto(data,NULL,"name",draft.name,sizeof draft.name);js_texto(data,NULL,"server",draft.server,sizeof draft.server);js_texto(data,NULL,"user",draft.user,sizeof draft.user);js_texto(data,NULL,"password",draft.password,sizeof draft.password);
        js_texto(data,NULL,"playlist",draft.playlist,sizeof draft.playlist);
      }
      char file[3100]="";js_texto_linhas(data,NULL,"file",file,sizeof file);
      if(pstep==1){
        /* Authenticate first, then fetch real catalog counts and categories. */
        use_saved=0;action[0]=0;if(file[0]){draft.type=2;snprintf(draft.file,sizeof draft.file,"%s",file);}
        draft.live=draft.movies=draft.series=1;draft.epg=0;phone_stage=1;
        celular_status("{\"s\":\"fetch\"}");
        if(!start(1)){phone_stage=0;celular_status("{\"s\":\"err\"}");}
      }else if(pstep==2&&phone_stage==2){
        draft.live=js_num(data,NULL,"live",1)!=0;draft.movies=js_num(data,NULL,"movies",1)!=0;draft.series=js_num(data,NULL,"series",1)!=0;draft.epg=js_num(data,NULL,"epg",1)!=0;
        js_texto(data,NULL,"xmltv",draft.xmltv,sizeof draft.xmltv);
        if(draft.xmltv[0]&&!g5_http_url(draft.xmltv))celular_status("{\"s\":\"err\"}");
        else if(!draft.live&&!draft.movies&&!draft.series)celular_status("{\"s\":\"err\"}");
        else{
          for(int i=0;i<group_count;i++)group_on[i]=!adult_group(groups[i]);
          const char *g=js_array(data,NULL,"off");while(g){const char *e=js_fim(g);if(!e)break;char value[24]="";snprintf(value,sizeof value,"%.*s",(int)(e-g),g);char *end;long i=strtol(value,&end,10);
            if(end!=value&&!*end&&i>=0&&i<group_count)group_on[i]=0;g=js_prox(e);}
          persist();
          if(save_failed)celular_status("{\"s\":\"err\"}");
          else{if(!draft.live)drop_kind(G5_LIVE);if(!draft.movies)drop_kind(G5_MOVIE);if(!draft.series)drop_kind(G5_SERIES);phone_status_counts("done");phone_stage=3;go("setup-pronto");}
        }
      }else if(pstep==0){
        draft.live=js_num(data,NULL,"live",1)!=0;draft.movies=js_num(data,NULL,"movies",1)!=0;draft.series=js_num(data,NULL,"series",1)!=0;draft.epg=js_num(data,NULL,"epg",1)!=0;
        js_texto(data,NULL,"xmltv",draft.xmltv,sizeof draft.xmltv);
        if(file[0]){
          G5Playlist p;G5Item *items=calloc(G5_CAP,sizeof *items);g5_playlist_init(&p,items,G5_CAP);
          if(items&&g5_playlist_feed(&p,file,strlen(file))&&g5_playlist_finish(&p)>=0){free(inventory);inventory=items;count=p.count;total=p.total;draft.type=2;snprintf(draft.file,sizeof draft.file,"%s",file);build_groups();test_ok=1;go("setup-importar");}
          else{free(items);snprintf(message,sizeof message,"%s",i18n("Formato não suportado"));}
        }else{use_saved=0;go("setup-teste");start(1);}celular_fechar();
      }
      erase(file,sizeof file);erase(data,sizeof data);
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

/* ==========================================================================
 * GUIA TV v5 — A TELA PRINCIPAL DO GUIA (mock v5: guia.html + v4.js "newGuide"
 * + v5.css, aprovado pelo dono). Substitui o desenho do guia de sempre: barra
 * de cima com abas, linha de chips, barra lateral Biblioteca/Categorias, heroi
 * "ASSISTINDO AGORA" com o cartao de preview, canais fixados, grade com regua
 * e "agora", cartoes, esportes e os estados de folha/erro/vazio. Os dados vem
 * do guia de sempre (guia.c) pelas funcoes publicas — nenhuma fixture aqui.
 * ========================================================================== */
#define GV_X 48.0f
#define GV_SB_W 360.0f
#define GV_MX 432.0f          /* inicio do conteudo principal */
#define GV_MW 1440.0f
#define GV_TOP_Y 24.0f
#define GV_TOP_H 56.0f
#define GV_CHIP_Y 96.0f
#define GV_CHIP_H 56.0f
#define GV_BASE 168.0f        /* topo da sidebar e do heroi (256 com faixa de erro) */
#define GV_HERO_H 352.0f
#define GV_CARD_W 560.0f      /* cartao 16:9 do preview (mock v5) */
#define GV_CARD_H 315.0f
#define GV_BIG_W 626.0f       /* previa grande: ocupa a direita do heroi */
#define GV_ROW_H 76.0f
#define GV_ROW_GAP 10.0f
#define GV_GRID_X (GV_MX + 324.0f + 8.0f)   /* celulas de programa */
#define GV_GRID_W 1116.0f

/* Canal do guia, copiado para a tela v5 desenhar sem depender dos estaticos. */
#define GV_PROGS 24
typedef struct {
  char id[80], nome[140], logo[480], grupo[64], base[256];
  int num, fav, np;
  EpgProg p[GV_PROGS];
  /* Own titles across the two-second cache: EPG publication frees the arena. */
  char titulo[GV_PROGS][240];
} GVCh;
/* gv_ch is a window of the filtered list starting at gv_off; gv_lin, gv_top
 * and gv_match count the whole filtered list, so every channel is reachable. */
#define GV_WIN 24
#define GV_FAV "\001fav"     /* gv_grupo: Biblioteca > Favoritos */
static GVCh gv_ch[GV_WIN]; static int gv_n, gv_off, gv_match, gv_top, gv_total_n, gv_favs, gv_cats, gv_cats_total;
static GVCh gv_pins[6]; static int gv_npins;
static char gv_cat[4][64]; static int gv_cat_n[4];
static char gv_grupo[64];   /* filtro de categoria ("Todos ao vivo" quando vazio) */
static int gv_zone, gv_lin, gv_pin, gv_sb, gv_chipi, gv_card, gv_hbt;
static int gv_tdesl;        /* grade adiantada, em minutos (passos de 30) */
static time_t gv_tfoc;      /* instante do programa em foco; 0 = agora */
static int gv_snap_force;
static char gv_numero[8]; static Uint32 gv_numero_desde;
static int gv_epg_extra, gv_verif_estado;   /* folhas: 0 verificando 1 ok 2 falha */

static int gv_hue(const char *s){unsigned h=0;while(s&&*s)h=h*31u+(unsigned char)*s++;return (int)(h%360);}
/* Aparelho de estado: quais "screens" sao da tela principal do guia. */
static int gv_is(const char *s){
  static const char *const ss[]={"grade","cartoes","preview","preview-grade","preview-foco",
    "preview-hero-cheio","preview-troca","dock","catchup","categorias","busca","numero","opcoes",
    "canal-opcoes","ajustes","pastas","epg-saude","epg-verificando","epg-adicionado","epg-falha",
    "erros","vazio","fontes-erro","esportes-ligas","esportes-liga","esportes-favoritos",
    "esportes-ajustes","esportes-onde-assistir",NULL};
  for(int i=0;ss[i];i++)if(!strcmp(s,ss[i]))return 1;
  return 0;
}
static int gv_sheet(void){
  const char *s=screen;
  return !strcmp(s,"pastas")||!strcmp(s,"canal-opcoes")||!strcmp(s,"epg-saude")||!strcmp(s,"erros")
    ||!strcmp(s,"ajustes")||!strcmp(s,"epg-verificando")||!strcmp(s,"epg-adicionado")||!strcmp(s,"epg-falha");
}
static int gv_sports(void){return !strncmp(screen,"esportes-",9);}
static void gv_hora(time_t t,char *dst,size_t n){struct tm m;localtime_r(&t,&m);strftime(dst,n,"%H:%M",&m);}
static int gv_past(void){return !strcmp(screen,"catchup");}
/* Inicio da janela de 2 h da grade: meia hora corrente (duas horas antes no
 * Rever), adiantada por gv_tdesl. */
static time_t gv_tbase(void){
  time_t t=time(NULL)-(gv_past()?7200:0);
  return t-t%(30*60);
}
static time_t gv_t0(void){return gv_tbase()+(time_t)gv_tdesl*60;}
static int gv_filtra(const char *id,const char *cat){
  if(!gv_grupo[0])return 1;
  if(!strcmp(gv_grupo,GV_FAV))return guia_e_favorito(id);
  return !strcmp(gv_grupo,cat?cat:"");
}
static int gv_num_de(int i){int n=guia_canal_numero(i);return n>0?n:i+1;}
static void gv_copia(GVCh *c,int i,const char *id,const char *nome,const char *logo,const char *cat,const char *base,int progs){
  memset(c,0,sizeof *c);
  snprintf(c->id,sizeof c->id,"%s",id);snprintf(c->nome,sizeof c->nome,"%s",nome);
  snprintf(c->logo,sizeof c->logo,"%s",logo?logo:"");snprintf(c->grupo,sizeof c->grupo,"%s",cat?cat:"");
  snprintf(c->base,sizeof c->base,"%s",base?base:"");
  c->num=gv_num_de(i);c->fav=guia_e_favorito(id);
  if(!progs)return;
  time_t t0=gv_t0();
  c->np=guia_programa_faixa(id,t0-3600,t0+6*3600,c->p,GV_PROGS);
  if(c->np>GV_PROGS)c->np=GV_PROGS;
  for(int k=0;k<c->np;k++){
    snprintf(c->titulo[k],sizeof c->titulo[k],"%s",c->p[k].titulo?c->p[k].titulo:"");
    c->p[k].titulo=c->titulo[k];
  }
}
/* Recolhe os canais, contagens e categorias do guia publicado. Barato: o
 * guia ja publica tudo em memoria; e refeito por tela aberta, quando a janela
 * muda e a cada 2 s (a grade EPG chega depois da lista). A lista inteira e
 * percorrida: um canal bloqueado no meio nao corta o resto. */
static void gv_snapshot(void){
  static Uint32 ult; static char ult_screen[48];
  Uint32 agora=SDL_GetTicks();
  if(gv_n && !gv_snap_force && agora-ult<2000 && !strcmp(ult_screen,screen))return;
  ult=agora;gv_snap_force=0;snprintf(ult_screen,sizeof ult_screen,"%s",screen);
  gv_n=0;gv_match=0;gv_npins=0;gv_favs=0;gv_cats=0;gv_cats_total=0;gv_total_n=0;
  char seen[16][64];int ns=0;
  const char *id,*nome,*logo,*cat,*base;
  for(int i=0;guia_canal_regra_campos(i,NULL,NULL,NULL,NULL,NULL);i++){
    if(!guia_canal_campos(i,&id,&nome,NULL,&cat,&base))continue;
    gv_total_n++;
    if(cat&&cat[0]){
      int j;for(j=0;j<ns;j++)if(!strcmp(seen[j],cat))break;
      if(j==ns&&ns<16)snprintf(seen[ns++],64,"%s",cat);
      int g;for(g=0;g<gv_cats&&g<4;g++)if(!strcmp(gv_cat[g],cat))break;
      if(g==gv_cats&&g<4){snprintf(gv_cat[g],sizeof gv_cat[g],"%s",cat);gv_cat_n[g]=1;gv_cats=g+1;}
      else if(g<4)gv_cat_n[g]++;
    }
    int fav=guia_e_favorito(id);
    if(fav){gv_favs++;
      if(gv_npins<6){guia_canal_campos(i,NULL,NULL,&logo,NULL,NULL);gv_copia(&gv_pins[gv_npins++],i,id,nome,logo,cat,base,0);}}
    if(!gv_filtra(id,cat))continue;
    int k=gv_match++;
    if(k<gv_off||k>=gv_off+GV_WIN)continue;
    guia_canal_campos(i,NULL,NULL,&logo,NULL,NULL);
    gv_copia(&gv_ch[gv_n++],i,id,nome,logo,cat,base,1);
  }
  gv_cats_total=ns;
  if(gv_lin>=gv_match)gv_lin=gv_match>0?gv_match-1:0;
}
/* Mantem o foco dentro da janela copiada (recentra e refaz a copia). */
static void gv_janela(void){
  if(gv_lin<0)gv_lin=0;
  if(gv_n&&gv_lin>=gv_off+2&&(gv_lin<gv_off+gv_n-2||gv_off+gv_n>=gv_match))return;
  if(!gv_n&&!gv_match)return;
  int off=gv_lin-GV_WIN/3;if(off<0)off=0;
  if(off!=gv_off||!gv_n){gv_off=off;gv_snap_force=1;gv_snapshot();}
}
static GVCh *gv_focus(void){return gv_lin>=gv_off&&gv_lin<gv_off+gv_n?&gv_ch[gv_lin-gv_off]:NULL;}
/* Instante que o foco aponta: o programa escolhido com DIREITA/ESQUERDA, ou
 * agora (no Rever, o ultimo que ja terminou). */
static time_t gv_tempo(void){return gv_tfoc?gv_tfoc:time(NULL);}
static EpgProg *gv_prog(GVCh *c){
  if(!c||!c->np)return NULL;
  time_t t=gv_tempo();
  if(gv_past()&&!gv_tfoc){EpgProg *u=NULL;for(int i=0;i<c->np;i++)if(c->p[i].fim<=t)u=&c->p[i];if(u)return u;}
  for(int i=0;i<c->np;i++)if(c->p[i].ini<=t&&t<c->p[i].fim)return &c->p[i];
  for(int i=0;i<c->np;i++)if(c->p[i].ini>t)return &c->p[i];
  return NULL;
}
/* Selo de qualidade so quando o nome do provedor diz; nada inventado. */
static const char *gv_quali(const char *nome){
  if(strstr(nome,"4K")||strstr(nome,"2160")||strstr(nome,"UHD"))return "4K";
  if(strstr(nome,"1080")||strstr(nome,"FHD"))return "FHD";
  if(strstr(nome,"720")||strstr(nome,"HD"))return "HD";
  if(strstr(nome," SD"))return "SD";
  return "";
}
static void gv_num_quali(const GVCh *c,char *dst,size_t n){
  const char *q=gv_quali(c->nome);
  if(q[0])snprintf(dst,n,"%d · %s",c->num,q);else snprintf(dst,n,"%d",c->num);
}
/* Primeira letra do nome (UTF-8 inteiro), como o azulejo do mock. */
static void gv_iniciais(const char *nome,char *dst,size_t n){
  while(*nome==' ')nome++;
  size_t l=1;
  if((unsigned char)*nome>=0xC0){while(nome[l]&&((unsigned char)nome[l]&0xC0)==0x80)l++;}
  if(!*nome)l=0;
  if(l>n-1)l=n-1;
  memcpy(dst,nome,l);dst[l]=0;
  if(l==1)dst[0]=(char)toupper((unsigned char)dst[0]);
}

/* --- pecas visuais do guia v5 -------------------------------------------- */
/* Folhas e modais por cima da pagina: a pagina de baixo perde o foco. */
static int gv_overlay_nome(const char *s){
  return !strcmp(s,"busca")||!strcmp(s,"numero")||!strcmp(s,"opcoes")||!strcmp(s,"canal-opcoes")
    ||!strcmp(s,"ajustes")||!strcmp(s,"pastas")||!strncmp(s,"epg-",4)||!strcmp(s,"erros")
    ||!strcmp(s,"fontes-erro")||!strcmp(s,"esportes-ajustes")||!strcmp(s,"esportes-onde-assistir");
}
static int gv_overlay(void){return gv_overlay_nome(screen);}
/* Telas cujo D-pad e a ordem espacial dos controles desenhados. */
static int gv_lista(void){return gv_overlay()||!strcmp(screen,"vazio")||gv_sports();}

/* --- movimento: o foco desliza, a grade rola, folhas e paginas entram ------- */
#define GV_MOLA_FOCO 22.0f
typedef struct{float x,y,w,h,vx,vy,vw,vh;int ok;}GVGlide;
static GVGlide gv_gfoco,gv_gcanal,gv_gside;
static float gv_dt,gv_folha_t=1,gv_pag_t=1;
static float gv_rol,gv_rol_v,gv_tmin,gv_tmin_v,gv_hh=GV_HERO_H,gv_hh_v;
static int gv_reg;          /* controles desenhados agora entram no D-pad */
static char gv_folha_ant[24];
static GfxRect gv_glide(GVGlide *g,GfxRect r){
  if(!g->ok||gv_dt<=0){*g=(GVGlide){r.x,r.y,r.w,r.h,0,0,0,0,1};return r;}
  g->x=anim_mola2(&g->vx,g->x,r.x,gv_dt,GV_MOLA_FOCO);g->y=anim_mola2(&g->vy,g->y,r.y,gv_dt,GV_MOLA_FOCO);
  g->w=anim_mola2(&g->vw,g->w,r.w,gv_dt,GV_MOLA_FOCO);g->h=anim_mola2(&g->vh,g->h,r.h,gv_dt,GV_MOLA_FOCO);
  return (GfxRect){g->x,g->y,g->w,g->h};
}
static void gv_registra(int id,float x,float y,float w,float h){if(gv_reg)reg_focus(id,x,y,w,h);}
static int gv_btn_foc(int id){
  if(focus!=id)return 0;
  if(gv_lista())return gv_reg;
  return gv_zone==2||gv_zone==5;
}

static void gv_label(TxtEstilo st,const char *t,float x,float y,float w,int dim){
  if(st==TXT_TITULO2)st=TXT_V2_A3TIT;
  if(st==TXT_HEADLINE)st=TXT_V2_GRUPO;
  TxtLinha l=txt_linha_corta_bruta(st,t,dim?154:243,dim?156:242,dim?158:239,255,w);txt_desenhar(l,x,y);
}
/* Chip (DESIGN 5): repouso branco 8% / #24262C, ligado = acento 22% com texto
 * no acento, foco = pilula cheia no acento com a tinta de contraste. */
static void gv_tinta(int foc,int sel,float *cr,float *cg,float *cb){
  if(foc){*cr=*cg=*cb=ajustes_tinta_foco()/255.0f;}
  else if(sel)ajustes_acento(cr,cg,cb);
  else{*cr=.953f;*cg=.949f;*cb=.937f;}
}
static void gv_fundo_ctrl(GfxRect r,int foc,int sel){
  if(foc){plrui_pilula_foco(r,1);return;}
  if(sel){float ar,ag,ab;ajustes_acento(&ar,&ag,&ab);gfx_cor(r,.5f,ar,ag,ab,.22f);return;}
  if(ajustes_vidro())gfx_cor(r,.5f,1,1,1,T21_CHIP_REST);else gfx_cor(r,.5f,.141f,.149f,.173f,1);
}
static void gv_chip(float x,float y,float w,const char *text,const char *icon,int sel,int foc,int dot,int id){
  GfxRect r={x,y,w,GV_CHIP_H};float cr,cg,cb,tx=x+20;
  gv_fundo_ctrl(r,foc,sel);gv_tinta(foc,sel,&cr,&cg,&cb);
  if(!foc&&!sel){cr*=.82f;cg*=.82f;cb*=.82f;}
  if(dot){const float c[3][3]={{.29f,.871f,.502f},{.961f,.647f,.141f},{.973f,.439f,.341f}};
    int di=dot==2?1:dot==3?2:0;gfx_cor((GfxRect){tx,y+22,12,12},.5f,c[di][0],c[di][1],c[di][2],1);tx+=22;}
  if(icon&&*icon){gfx_icone((GfxRect){tx,y+14,28,28},icon,cr,cg,cb,1);tx+=36;}
  TxtLinha l=txt_linha_corta_bruta(TXT_ILHA_CORPO,text,(int)(cr*255),(int)(cg*255),(int)(cb*255),255,x+w-tx-18);
  txt_desenhar(l,tx,y+(GV_CHIP_H-(float)l.h)/2);
  ponteiro_alvo(x,y,w,GV_CHIP_H,point_focus,point_activate,id,0);
}
static void gv_button(int id,const char *text,const char *icon,float x,float y,float w,int lit,int small){
  float h=small?50.0f:56.0f;GfxRect r={x,y,w,h};float cr,cg,cb;
  int foc=gv_btn_foc(id);
  if(!foc&&!lit)plrui_botao_repouso(r,1);else gv_fundo_ctrl(r,foc,lit);
  gv_tinta(foc,lit,&cr,&cg,&cb);
  if(icon&&*icon)gfx_icone((GfxRect){x+18,y+(h-25)/2,25,25},icon,cr,cg,cb,1);
  {TxtLinha l=txt_linha_corta_bruta(TXT_ILHA_CORPO,text,(int)(cr*255),(int)(cg*255),(int)(cb*255),255,w-(icon?70:44));
   txt_desenhar(l,icon?x+50:x+(w-(float)l.w)/2,y+(h-(float)l.h)/2);}
  ponteiro_alvo(x,y,w,h,point_focus,point_activate,id,0);
  gv_registra(id,x,y,w,h);
}
/* Dicas do rodape: o mesmo componente das dicas do player e da faixa. */
static void gv_hints(const char *const *keys,const char *const *labels,int n){
  plrui_dicas(keys,labels,n,GV_X,1042,0,1);
  const ContaPerfil *p=perfis_item_ativo();
  if(p){
    gfx_icone((GfxRect){1752,1030,24,24},"aj_shield-check",.65f,.66f,.68f,1);
    gv_label(TXT_ILHA_APOIO,p->nome,1786,1030,110,1);}
}
static void gv_dim(float top,float a){
  gfx_cor((GfxRect){0,top,1920,1080-top},0,0,0,0,fminf(a,T21_VEIL)*gv_folha_t);
}

/* --- barra de cima: Guia TV + abas Grade/Cartoes/Esportes/Filmes e series --- */
static void gv_topbar(void){
  gv_label(TXT_ILHA_TITULO,i18n("Guia TV"),GV_X,GV_TOP_Y+4,210,0);
  const char *names[]={"Grade","Cartões","Esportes","Filmes e séries"};
  const char *icons[]={"aj_layout-list","aj_layout-dashboard","aj_activity","aj_film"};
  int ativa=gv_sports()?2:!strcmp(gv_pagina,"cartoes")?1:0;
  float ws[4],tw=8,x=GV_X+240;
  for(int i=0;i<4;i++){ws[i]=(float)txt_linha(TXT_ILHA_CORPO,names[i],243,242,239,255).w+76;tw+=ws[i]+(i?4:0);}
  /* Seletor segmentado (DESIGN 5): trilho 5%, aba ativa 14%, foco no acento. */
  if(ajustes_vidro())gfx_cor((GfxRect){x,GV_TOP_Y,tw,GV_TOP_H},.5f,1,1,1,T21_SEG_TRACK);
  else gfx_cor((GfxRect){x,GV_TOP_Y,tw,GV_TOP_H},.5f,.114f,.118f,.137f,1);
  x+=4;
  for(int i=0;i<4;i++){
    GfxRect r={x,GV_TOP_Y+4,ws[i],GV_TOP_H-8};
    int on=i==ativa,f=gv_zone==5&&focus==1100+i&&!gv_overlay();
    if(f)plrui_pilula_foco(r,1);
    else if(on){if(ajustes_vidro())gfx_cor(r,.5f,1,1,1,T21_SEG_SELECTED);else gfx_cor(r,.5f,.204f,.212f,.243f,1);}
    float c=f?ajustes_tinta_foco()/255.0f:on?.953f:.62f;
    gfx_icone((GfxRect){x+18,GV_TOP_Y+14,28,28},icons[i],c,c,c,1);
    {TxtLinha l=txt_linha_corta(TXT_ILHA_CORPO,names[i],(int)(c*255),(int)(c*255),(int)(c*255),255,ws[i]-66);
     txt_desenhar(l,x+54,GV_TOP_Y+(GV_TOP_H-(float)l.h)/2);}
    ponteiro_alvo(r.x,r.y,r.w,r.h,point_focus,point_activate,1100+i,0);
    x+=ws[i]+4;
  }
  gv_button(1104,i18n("Buscar"),"aj_search",1420,GV_TOP_Y,180,0,0);
  gv_button(1105,i18n("Opções"),"aj_sliders-horizontal",1612,GV_TOP_Y,190,0,0);
  time_t now=time(NULL);struct tm tm;localtime_r(&now,&tm);char clock[16];strftime(clock,sizeof clock,"%H:%M",&tm);
  gv_label(TXT_ILHA_NOME,clock,1826,GV_TOP_Y+12,90,0);
}
/* --- linha de chips: Todos ao vivo / Esportes / fontes / Rever / EPG ------ */
static const char *gv_fonte_nome(void){
  return draft.name[0]?draft.name:committed?(draft.type==0?"Xtream":draft.type==1?"Lista por URL":"Arquivo M3U"):NULL;
}
static int gv_chip_tem(int i){return i>=0&&i<6&&(i!=2||gv_fonte_nome());}
static void gv_chips_row(void){
  char txt[96];float x=GV_X;
  int foc=gv_zone==4&&!gv_overlay();
  if(!gv_total_n){
    float w=(float)txt_linha(TXT_ILHA_CORPO,"Adicionar M3U / Xtream / XMLTV",243,242,239,255).w+76;
    gv_chip(x,GV_CHIP_Y,w,i18n("Adicionar M3U / Xtream / XMLTV"),"aj_plus",1,foc,0,1200);
    return;
  }
  snprintf(txt,sizeof txt,"%s  %d",i18n("Todos ao vivo"),gv_total_n);
  float w=(float)txt_linha_bruta(TXT_ILHA_CORPO,txt,243,242,239,255).w+40;
  gv_chip(x,GV_CHIP_Y,w,txt,NULL,!gv_grupo[0]&&!gv_past(),foc&&gv_chipi==0,0,1200);x+=w+10;
  w=(float)txt_linha(TXT_ILHA_CORPO,"Esportes",243,242,239,255).w+76;
  gv_chip(x,GV_CHIP_Y,w,i18n("Esportes"),"aj_activity",!strcmp(gv_grupo,"Esportes"),foc&&gv_chipi==1,0,1201);x+=w+10;
  const char *snome=gv_fonte_nome();
  if(snome){
    snprintf(txt,sizeof txt,"%s  %d",snome,gv_total_n);
    w=(float)txt_linha_bruta(TXT_ILHA_CORPO,txt,243,242,239,255).w+62;
    gv_chip(x,GV_CHIP_Y,w,txt,NULL,0,foc&&gv_chipi==2,source_loaded?1:3,1202);x+=w+10;
  }
  w=(float)txt_linha(TXT_ILHA_CORPO,"Rever",243,242,239,255).w+76;
  gv_chip(x,GV_CHIP_Y,w,i18n("Rever"),"aj_rotate-ccw-clock",gv_past(),foc&&gv_chipi==3,0,1203);x+=w+10;
  w=(float)txt_linha(TXT_ILHA_CORPO,"Adicionar fonte",243,242,239,255).w+76;
  gv_chip(x,GV_CHIP_Y,w,i18n("Adicionar fonte"),"aj_plus",0,foc&&gv_chipi==4,0,1204);x+=w+10;
  w=(float)txt_linha_bruta(TXT_ILHA_CORPO,"EPG",243,242,239,255).w+76;
  gv_chip(x,GV_CHIP_Y,w,"EPG","aj_calendar",0,foc&&gv_chipi==5,0,1205);
}

/* --- barra lateral: Biblioteca e Categorias ------------------------------ */
/* Ordem do D-pad na lateral: 0-4 Biblioteca, 6-9 categorias, 10-11 rodape. */
static int gv_sb_tem(int idx){return (idx>=0&&idx<5)||(idx>=6&&idx<6+gv_cats&&idx<10)||idx==10||idx==11;}
static int gv_sb_ativo(int idx){
  if(idx==2)return !strcmp(gv_grupo,GV_FAV);
  if(idx==3)return !gv_grupo[0];
  if(idx>=6&&idx<10)return idx-6<gv_cats&&!strcmp(gv_grupo,gv_cat[idx-6]);
  return 0;
}
static GfxRect gv_sb_rect(int idx,float base){
  float ycat=base+58+5*54+14;
  if(idx<5)return (GfxRect){GV_X+12,base+58+idx*54,GV_SB_W-24,48};
  if(idx<10)return (GfxRect){GV_X+12,ycat+62+(idx-6)*50,GV_SB_W-24,44};
  return (GfxRect){GV_X+20,1000-94+(idx-10)*46,GV_SB_W-40,42};
}
static const char *gv_cat_icone(const char *n){
  if(strstr(n,"sport")||strstr(n,"Sport"))return "aj_activity";
  if(strstr(n,"otíc")||strstr(n,"News")||strstr(n,"news"))return "aj_megaphone";
  if(strstr(n,"Film")||strstr(n,"film")||strstr(n,"Cine")||strstr(n,"Movie"))return "aj_film";
  if(strstr(n,"Adult")||strstr(n,"adult")||strstr(n,"XXX"))return "aj_eye-off";
  return "aj_tv-minimal-play";
}
static void gv_side_row(int idx,const char *icon,const char *name,const char *count,int id,float base){
  GfxRect r=gv_sb_rect(idx,base);float y=r.y,h=r.h;
  int f=idx==gv_sb&&gv_zone==3&&!gv_overlay(),on=gv_sb_ativo(idx);
  if(on&&!f){if(ajustes_vidro())gfx_cor(r,18.0f/h,1,1,1,.06f);else gfx_cor(r,18.0f/h,.141f,.149f,.173f,1);}
  float c=f||on?.953f:.72f;
  gfx_icone((GfxRect){GV_X+28,y+(h-24)/2,24,24},icon,c,c,c,1);
  {TxtLinha l=txt_linha_corta_bruta(TXT_ILHA_CORPO,name,(int)(c*255),(int)(c*254),(int)(c*251),255,count?GV_SB_W-64-84:GV_SB_W-64-28);
   txt_desenhar(l,GV_X+64,y+(h-(float)l.h)/2);}
  if(count){TxtLinha l=txt_linha_corta_bruta(TXT_ILHA_CORPO,count,154,156,158,255,64);
   txt_desenhar(l,GV_X+GV_SB_W-28-(float)l.w,y+(h-(float)l.h)/2);}
  ponteiro_alvo(r.x,r.y,r.w,r.h,point_focus,point_activate,id,0);
}
static void gv_sidebar(float base){
  panel(GV_X,base,GV_SB_W,1000-base);
  if(!gv_total_n){
    gv_label(TXT_ILHA_NOME,i18n("Biblioteca"),GV_X+26,base+26,300,0);
    txt_bloco(TXT_ILHA_CORPO,i18n("Nenhum canal ainda."),154,156,158,GV_X+26,base+92,GV_SB_W-52,32,1,2);
    txt_bloco(TXT_ILHA_CORPO,i18n("Categorias e pastas aparecem ao adicionar uma lista."),154,156,158,GV_X+26,base+136,GV_SB_W-52,32,1,3);
    return;
  }
  /* Foco de linha (DESIGN 3): superficie um degrau mais clara, deslizando. */
  if(gv_zone==3&&!gv_overlay()&&gv_sb<10)plrui_linha_foco(gv_glide(&gv_gside,gv_sb_rect(gv_sb,base)),18,1);
  else gv_gside.ok=0;
  char nb[24];
  gv_label(TXT_ILHA_NOME,i18n("Biblioteca"),GV_X+26,base+20,300,0);
  const char *nomes[]={"Vistos recentemente","Adicionados","Favoritos","Todos ao vivo","Suas pastas"};
  const char *icons[]={"aj_rotate-ccw-clock","aj_plus","aj_star","aj_tv-minimal-play","aj_folders"};
  for(int i=0;i<5;i++){
    const char *cnt=i==2?(snprintf(nb,sizeof nb,"%d",gv_favs),nb):i==3?(snprintf(nb,sizeof nb,"%d",gv_total_n),nb):NULL;
    gv_side_row(i,icons[i],i18n(nomes[i]),cnt,1300+i,base);
  }
  float y=base+58+5*54+14;
  gfx_cor((GfxRect){GV_X+24,y,GV_SB_W-48,1},0,1,1,1,.08f);
  snprintf(nb,sizeof nb,"%d",gv_cats_total);
  gv_label(TXT_ILHA_NOME,i18n("Categorias"),GV_X+26,y+16,220,0);
  {TxtLinha l=txt_linha_bruta(TXT_ILHA_CORPO,nb,154,156,158,255);txt_desenhar(l,GV_X+GV_SB_W-28-(float)l.w,y+18);}
  for(int i=0;i<4&&i<gv_cats;i++){
    snprintf(nb,sizeof nb,"%d",gv_cat_n[i]);
    gv_side_row(6+i,gv_cat_icone(gv_cat[i]),gv_cat[i],nb,1306+i,base);
  }
  /* Ordem / Mostrar-ocultar pregados no pe da barra lateral: botoes. */
  const char *bn[]={"Ordem: personalizada","Mostrar / ocultar"},*bi[]={"aj_sliders-horizontal","aj_eye"};
  for(int i=0;i<2;i++){
    GfxRect pb=gv_sb_rect(10+i,base);int f=gv_zone==3&&gv_sb==10+i&&!gv_overlay();float cr,cg,cb;
    gv_fundo_ctrl(pb,f,0);gv_tinta(f,0,&cr,&cg,&cb);
    gfx_icone((GfxRect){pb.x+16,pb.y+9,24,24},bi[i],cr,cg,cb,1);
    TxtLinha l=txt_linha_corta(TXT_ILHA_CORPO,bn[i],(int)(cr*255),(int)(cg*255),(int)(cb*255),255,pb.w-66);
    txt_desenhar(l,pb.x+50,pb.y+(pb.h-(float)l.h)/2);
    ponteiro_alvo(pb.x,pb.y,pb.w,pb.h,point_focus,point_activate,1310+i,0);
  }
}

/* --- heroi: ASSISTINDO AGORA + cartao de previa --------------------------- */
static void gv_hsl(int h,float s,float l,float *r,float *g,float *b){
  float c=(1-fabsf(2*l-1))*s,hp=(h%360)/60.0f,x=c*(1-fabsf(fmodf(hp,2)-1));
  float t[3]={0,0,0};
  if(hp<1){t[0]=c;t[1]=x;}else if(hp<2){t[0]=x;t[1]=c;}else if(hp<3){t[1]=c;t[2]=x;}
  else if(hp<4){t[1]=x;t[2]=c;}else if(hp<5){t[0]=x;t[2]=c;}else{t[0]=c;t[2]=x;}
  float m=l-c/2;*r=t[0]+m;*g=t[1]+m;*b=t[2]+m;
}
static float gv_badge(const char *t,float x,float y){
  if(!t||!*t)return 0;   /* unknown quality: no empty pill */
  float w=(float)txt_linha_bruta(TXT_ILHA_CORPO,t,243,242,239,255).w+24;
  gfx_cor((GfxRect){x,y,w,44},12.0f/44,.168f,.176f,.204f,1);
  gv_label(TXT_ILHA_CORPO,t,x+12,y+9,w-20,0);
  return w+6;
}
/* placar do titulo "Brasileirao: Flamengo x Palmeiras" -> "FLA 2 x 1 PAL" quando ha numeros */
static int gv_placar(const char *titulo,char *dst,size_t n){
  const char *x=strstr(titulo,"×");if(!x)return 0;
  char esq[80],dir[80];const char *dp=strchr(titulo,':');
  const char *ini=dp?dp+1:titulo;
  snprintf(esq,sizeof esq,"%.*s",(int)(x-ini),ini);snprintf(dir,sizeof dir,"%s",x+strlen("×"));
  char *e2=esq;while(*e2==' ')e2++;char *f=e2+strlen(e2);while(f>e2&&f[-1]==' ')f--;*f=0;
  char *d2=dir;while(*d2==' ')d2++;f=d2+strlen(d2);while(f>d2&&f[-1]==' ')f--;*f=0;
  snprintf(dst,n,"%s × %s",e2,d2);
  return 1;
}
/* Logo real do canal num azulejo claro; sem logo, a inicial no azulejo da cor
 * do canal (mock v5). Nunca o nome cortado. */
static void gv_avatar(const char *logo,const char *nome,GfxRect r){
  float rr=fminf(r.w,r.h)*.25f;
  if(logo&&logo[0]&&tex_obter_larg(logo,r.w)&&tex_aspecto(logo)>0){
    gfx_cor(r,rr/r.h,1,1,1,.08f);
    guia_logo_desenhar(logo,nome,(GfxRect){r.x+5,r.y+5,r.w-10,r.h-10},r.w-10,r.h-10,1,1);
    return;
  }
  float cr,cg,cb;gv_hsl(gv_hue(nome),.45f,.30f,&cr,&cg,&cb);gfx_cor(r,rr/r.h,cr,cg,cb,1);
  char ini[8];gv_iniciais(nome,ini,sizeof ini);
  TxtLinha t=txt_linha_bruta(r.h>=120?TXT_V2_TIT:r.h>=48?TXT_HEADLINE:TXT_W20_24B,ini,243,242,239,255);
  txt_desenhar(t,r.x+(r.w-(float)t.w)/2,r.y+(r.h-(float)t.h)/2);
}
static float gv_progresso(const EpgProg *p){
  if(!p||p->fim<=p->ini)return 0;
  return anim_clamp((float)(time(NULL)-p->ini)/(float)(p->fim-p->ini),0,1);
}
static GVCh *gv_canal_cartao(void){return gv_card>=0&&gv_card<gv_n?&gv_ch[gv_card]:gv_focus();}
static void gv_broadcast(GfxRect pv,GVCh *c,EpgProg *p,int full){
  float raio=full?0:(ajustes_guia_previa_grande()?0:20.0f/pv.h);
  int live=guia_preview_no_ar();
  int troca=!strcmp(screen,"preview-troca");
  if(live)gfx_furo_raio(pv,raio);
  else{
    float r,g,b;gv_hsl(gv_hue(c->nome),.45f,.30f,&r,&g,&b);
    gfx_cor(pv,raio,r,g,b,1);
    float lado=fminf(pv.h*.44f,170);
    gv_avatar(c->logo,c->nome,(GfxRect){pv.x+(pv.w-lado)/2,pv.y+(pv.h-lado)/2-(p&&pv.h>200?16:0),lado,lado});
  }
  /* veu preto do modo grande/imersivo: emenda o video no heroi (2.0.8) */
  if(full||ajustes_guia_previa_grande()){
    gfx_rect((GfxRect){pv.x,pv.y,pv.w,pv.h},0,GFX_VEU_CSS,0,2,1,0,.045f,.047f,.051f,.9f);
    gfx_rect((GfxRect){pv.x,pv.y+pv.h-120,pv.w,120},0,GFX_VEU_CSS,0,0,1,0,.045f,.047f,.051f,.8f);
  }
  /* selos sobre o video (GL por cima do furo) */
  if(live||!strcmp(screen,"preview")||!strcmp(screen,"preview-foco")||troca||full){
    TxtLinha cb=txt_linha_bruta(TXT_ILHA_NOME,c->nome,25,33,44,255);
    GfxRect brand={pv.x+pv.w-(float)cb.w-48,pv.y+16,(float)cb.w+24,40};
    gfx_cor(brand,7.0f/40,.953f,.949f,.937f,1);
    txt_desenhar(cb,brand.x+12,brand.y+(40-(float)cb.h)/2);
    TxtLinha lv=txt_linha(TXT_ILHA_SUB,"AO VIVO",243,242,239,255);
    GfxRect pill={full?brand.x-(float)lv.w-62:pv.x+18,pv.y+18,(float)lv.w+44,36};
    gfx_cor(pill,.5f,.91f,.208f,.353f,1);
    gfx_cor((GfxRect){pill.x+14,pill.y+13,10,10},.5f,1,1,1,1);
    txt_desenhar(lv,pill.x+30,pill.y+(36-(float)lv.h)/2);
  }
  if(p&&live){
    char placar[96];
    if(gv_placar(p->titulo,placar,sizeof placar)){
      TxtLinha s=txt_linha_bruta(TXT_ILHA_CORPO,placar,243,242,239,255);
      TxtLinha m=txt_linha(TXT_ILHA_SUB,"Ao vivo · sem som",25,33,44,255);
      GfxRect sb={pv.x+18,pv.y+pv.h-118,(float)s.w+(float)m.w+52,40};
      gfx_cor(sb,7.0f/40,.92f,.925f,.894f,1);
      gfx_cor((GfxRect){sb.x,sb.y,(float)s.w+26,40},7.0f/40,.118f,.298f,.263f,1);
      txt_desenhar(s,sb.x+13,sb.y+(40-(float)s.h)/2);
      txt_desenhar(m,sb.x+(float)s.w+38,sb.y+(40-(float)m.h)/2);
    }
  }
  /* Faixa do programa: hora de inicio, progresso e fim (com ou sem video). */
  if(p&&pv.h>150){
    float by=pv.y+pv.h-50;char hi[8],hf[8];
    gv_hora(p->ini,hi,8);gv_hora(p->fim,hf,8);
    if(!live)gfx_rect((GfxRect){pv.x,pv.y+pv.h-90,pv.w,90},0,GFX_VEU_CSS,0,0,1,0,0,0,0,.5f);
    TxtLinha a=txt_linha_bruta(TXT_ILHA_SUB,hi,243,242,239,255),z=txt_linha_bruta(TXT_ILHA_SUB,hf,243,242,239,255);
    float sw=live?(float)txt_linha(TXT_ILHA_SUB,"Sem som",0,0,0,255).w+40:0;
    txt_desenhar(a,pv.x+18,by);
    GfxRect bar={pv.x+30+(float)a.w,by+(float)a.h/2-2,pv.w-60-(float)a.w-(float)z.w-sw,4};
    gfx_cor(bar,.5f,1,1,1,.28f);
    gfx_cor((GfxRect){bar.x,bar.y,bar.w*gv_progresso(p),4},.5f,.953f,.949f,.937f,1);
    txt_desenhar(z,bar.x+bar.w+12,by);
    if(live){gfx_icone((GfxRect){pv.x+pv.w-sw-6,by,22,22},"aj_volume-2",.7f,.7f,.7f,1);
      gv_label(TXT_ILHA_SUB,i18n("Sem som"),pv.x+pv.w-sw+22,by,sw,0);}
  }
  if(troca){
    gfx_cor(pv,raio,.067f,.09f,.098f,.55f);
    TxtLinha t=txt_linha(TXT_ILHA_CORPO,"Trocando canal…",243,242,239,255);
    float tx=pv.x+(pv.w-(float)t.w-42)/2;
    gfx_icone((GfxRect){tx,pv.y+pv.h/2-16,32,32},"aj_loader",.85f,.86f,.88f,1);
    txt_desenhar(t,tx+42,pv.y+(pv.h-(float)t.h)/2);
  }
}
/* Heroi: GV_HERO_H com os botoes; 208 (o compacto do mock) com o foco na
 * lateral, para a grade mostrar mais canais. A altura anda na mola. */
static void gv_hero(float base){
  float h=gv_hh,k=anim_clamp((h-208)/(GV_HERO_H-208),0,1);
  GfxRect r={GV_MX,base,GV_MW,h};float rr=28.0f/h;
  int full=!strcmp(screen,"preview-hero-cheio"),grande=ajustes_guia_previa_grande();
  GVCh *c=gv_focus();EpgProg *p=gv_prog(c);
  if(!c){t21_ilha(r,28,1);return;}
  float r2,g2,b2;gv_hsl(gv_hue(c->nome),.38f,.19f,&r2,&g2,&b2);
  gfx_cor(r,rr,r2,g2,b2,1);
  gfx_rect((GfxRect){r.x,r.y,r.w*0.70f,r.h},0,GFX_VEU_CSS,0,3,1.2f,0,r2,g2,b2,.9f);
  GfxRect pv;
  if(full)pv=r;
  else if(grande)pv=(GfxRect){r.x+r.w-GV_BIG_W,r.y,GV_BIG_W,r.h};
  else{float ch=h-36,cw=fminf(GV_CARD_W,ch*16/9);pv=(GfxRect){r.x+r.w-cw-18,r.y+18,cw,ch};}
  /* Cartao da previa em foco (DIREITA depois dos botoes): cresce com sombra. */
  int cardf=gv_zone==2&&gv_hbt==2&&!gv_overlay()&&!full;
  static GVGlide gcard;
  if(!full&&!grande){
    GfxRect alvo=cardf?(GfxRect){pv.x-pv.w*.02f,pv.y-pv.h*.02f,pv.w*1.04f,pv.h*1.04f}:pv;
    pv=gv_glide(&gcard,alvo);
    if(cardf)gfx_rect((GfxRect){pv.x-22,pv.y-4,pv.w+44,pv.h+40},0,GFX_SOMBRA,1,0,0,.5f,0,0,0,.5f);
  }
  gv_broadcast(pv,c,p,full||grande);
  float x=r.x+32,iw=(full?760:pv.x-r.x-64);char buf[200];
  int passado=p&&(p->fim<=time(NULL)||gv_past());
  if(passado)snprintf(buf,sizeof buf,"%s · %s %d",i18n("REVER"),i18n("CANAL"),c->num);
  else snprintf(buf,sizeof buf,"%s · %s %d",i18n(full?"PRÉVIA IMERSIVA":p&&p->ini>time(NULL)?"A SEGUIR":"ASSISTINDO AGORA"),i18n("CANAL"),c->num);
  float og=gfx_opacidade_grupo;
  gfx_opacidade_grupo=og*k;gv_label(TXT_ILHA_SUB,buf,x,r.y+28,iw,1);gfx_opacidade_grupo=og;
  gv_label(TXT_ILHA_TITULO,p?p->titulo:c->nome,x,r.y+anim_mistura(26,62,k),iw,0);
  float y=r.y+anim_mistura(82,124,k);char hi[8],hf[8],meta[180];
  if(p){gv_hora(p->ini,hi,8);gv_hora(p->fim,hf,8);
    snprintf(meta,sizeof meta,"%s · %d · %s–%s",c->nome,c->num,hi,hf);
    TxtLinha m=txt_linha_corta_bruta(TXT_ILHA_CORPO,meta,154,156,158,255,iw*.6f);txt_desenhar(m,x,y);
    float mx=x+(float)m.w+20;
    meta[0]=0;
    if(p->fim<=time(NULL))snprintf(meta,sizeof meta,i18n("Exibido há %ld min"),(long)(time(NULL)-p->fim)/60);
    else if(p->ini<=time(NULL))snprintf(meta,sizeof meta,i18n("%ld min restantes"),(long)(p->fim-time(NULL))/60);
    if(meta[0]){TxtLinha t=txt_linha_corta_bruta(TXT_ILHA_NOME,meta,243,242,239,255,x+iw-mx);txt_desenhar(t,mx,y-2);mx+=(float)t.w+18;}
    const char *q=gv_quali(c->nome);if(q[0]&&mx+80<x+iw)gv_badge(q,mx,y-8);
  }else{
    gv_label(TXT_ILHA_CORPO,i18n("Sem programação"),x,y,iw,1);
  }
  txt_bloco(TXT_ILHA_CORPO,p?i18n("Sem descrição no guia."):i18n("Vincule um guia ao canal · segure OK"),154,156,158,x,r.y+anim_mistura(126,180,k),iw,32,1,2);
  if(k<.02f)return;
  /* Botoes: aparecem com a altura do heroi. */
  gfx_opacidade_grupo=og*k;
  y=r.y+252;
  if(gv_zone==2&&!gv_overlay())focus=1312+gv_hbt;
  if(passado)gv_button(1312,i18n("Assistir"),"aj_play",x,y,220,0,1);
  else{
    gv_button(1312,i18n("Tela cheia"),"aj_maximize-2",x,y,240,0,1);
    gv_button(1313,i18n(full?"Prévia na janela":"Ampliar prévia"),"aj_play",x+256,y,full?300:280,0,1);
    if(!full)gv_label(TXT_ILHA_APOIO,i18n("Selecione novamente para tela cheia. A prévia continua sem som durante a navegação."),x,y+62,iw,1);
  }
  gfx_opacidade_grupo=og;
}

/* --- heroi compacto do modo cartoes (mock v5, preview() do guia.html) ---- */
static void gv_hero_compact(float base){
  GfxRect r={GV_MX,base,GV_MW,208};float rr=28.0f/208;
  GVCh *c=gv_canal_cartao();EpgProg *p=gv_prog(c);
  if(!c){t21_ilha(r,28,1);return;}
  float r2,g2,b2;gv_hsl(gv_hue(c->nome),.38f,.19f,&r2,&g2,&b2);
  gfx_cor(r,rr,r2,g2,b2,1);
  gfx_rect((GfxRect){r.x,r.y,r.w*.72f,r.h},0,GFX_VEU_CSS,0,3,1.2f,0,r2,g2,b2,.9f);
  GfxRect art={r.x+r.w-358,r.y+16,330,176};
  gv_broadcast(art,c,p,0);
  float iw=art.x-r.x-64;char meta[180],hi[8],hf[8];
  gv_label(TXT_ILHA_TITULO,p?p->titulo:c->nome,r.x+32,r.y+28,iw,0);
  if(p){gv_hora(p->ini,hi,8);gv_hora(p->fim,hf,8);
    snprintf(meta,sizeof meta,"%s · %d · %s–%s",c->nome,c->num,hi,hf);
    TxtLinha m=txt_linha_corta_bruta(TXT_ILHA_CORPO,meta,154,156,158,255,iw*.6f);txt_desenhar(m,r.x+32,r.y+90);
    float mx=r.x+32+(float)m.w+20;
    long min=p->fim>time(NULL)?(p->fim-time(NULL))/60:0;
    snprintf(meta,sizeof meta,i18n("%ld min restantes"),min);
    TxtLinha t=txt_linha_corta_bruta(TXT_ILHA_NOME,meta,243,242,239,255,r.x+32+iw-mx);txt_desenhar(t,mx,r.y+88);mx+=(float)t.w+18;
    const char *q=gv_quali(c->nome);if(q[0]&&mx+80<r.x+32+iw)gv_badge(q,mx,r.y+82);
  }else gv_label(TXT_ILHA_CORPO,i18n("Sem programação"),r.x+32,r.y+90,iw,1);
  txt_bloco(TXT_ILHA_CORPO,i18n("Sem descrição no guia."),154,156,158,r.x+32,r.y+140,iw,30,1,2);
}
/* --- canais fixados (os favoritos, na ordem do guia) ---------------------- */
static GVCh *gv_pin_ch(int i){return i>=0&&i<gv_npins?&gv_pins[i]:NULL;}
static void gv_dock(float base,float heroh){
  float y=base+heroh+20;
  gv_label(TXT_ILHA_NOME,i18n("Canais fixados"),GV_MX,y,300,0);
  char hint[160];snprintf(hint,sizeof hint,i18n("↑ %s · %d / 6 · %s"),i18n("na grade"),gv_npins,i18n("Segure OK para desafixar"));
  {TxtLinha l=txt_linha_corta_bruta(TXT_ILHA_CORPO,hint,154,156,158,255,800);txt_desenhar(l,GV_MX+GV_MW-(float)l.w,y+4);}
  y+=44;
  for(int i=0;i<gv_npins;i++){
    GVCh *c=&gv_pins[i];
    GfxRect r={GV_MX+i*242,y,230,66};
    int f=gv_zone==1&&gv_pin==i&&!gv_overlay();float cr,cg,cb;
    if(f)plrui_pilula_foco(r,1);
    else if(ajustes_vidro())gfx_cor(r,.5f,1,1,1,.06f);else gfx_cor(r,.5f,.114f,.118f,.137f,1);
    gv_tinta(f,0,&cr,&cg,&cb);
    gv_avatar(c->logo,c->nome,(GfxRect){r.x+12,y+12,42,42});
    TxtLinha l=txt_linha_corta_bruta(TXT_ILHA_CORPO,c->nome,(int)(cr*255),(int)(cg*255),(int)(cb*255),255,154);
    txt_desenhar(l,r.x+66,y+(66-(float)l.h)/2);
    ponteiro_alvo(r.x,r.y,r.w,r.h,point_focus,point_activate,1400+i,0);
  }
}
static GVCh *gv_canal_focado(void){
  if(gv_zone==1)return gv_pin_ch(gv_pin);
  if(gv_zone==6)return gv_card<gv_n?&gv_ch[gv_card]:NULL;
  return gv_focus();
}
static GVCh *gv_canal_focado_ou_primeiro(void){GVCh *c=gv_canal_focado();return c?c:gv_n?&gv_ch[0]:NULL;}
/* --- grade com regua e "agora" --------------------------------------------- */
static float gv_rows_y(float base){return base+gv_hh+20+108+16+38;}
static int gv_vis(float base){
  int n=(int)((1004-gv_rows_y(base)+GV_ROW_GAP)/(GV_ROW_H+GV_ROW_GAP));return n<1?1:n;
}
static float gv_px(time_t t){
  float t0=(float)gv_tbase()+gv_tmin*60.0f;
  return GV_GRID_X+((float)t-t0)/60.0f*(GV_GRID_W/120.0f);
}
static GfxRect gv_cel(const EpgProg *p,float y){
  float l=fmaxf(gv_px(p->ini),GV_GRID_X)+8,r=gv_px(p->fim);
  return (GfxRect){l,y,r-l,GV_ROW_H};
}
/* Duas passadas: fundos (pass 0) e textos (pass 1), com o foco deslizante no
 * meio, para o texto focado ficar por cima da pilula. */
static void gv_row(float y,GVCh *c,int foc,int pass){
  GfxRect cr={GV_MX,y,320,GV_ROW_H};float rr=20.0f/GV_ROW_H;
  time_t now=time(NULL);EpgProg *pf=foc&&gv_zone==0?gv_prog(c):NULL;
  if(!pass){
    if(ajustes_vidro())gfx_cor(cr,rr,1,1,1,.04f);else gfx_cor(cr,rr,.071f,.075f,.086f,1);
    if(!c->np){GfxRect pr={GV_GRID_X+8,y,1920-GV_GRID_X,GV_ROW_H};gfx_cor(pr,rr,.102f,.106f,.121f,1);}
    for(int j=0;j<c->np;j++){
      EpgProg *p=&c->p[j];GfxRect pr=gv_cel(p,y);
      if(pr.w<=8||pr.x>=1920)continue;
      pr.w-=8;
      int live=p->ini<=now&&now<p->fim,past=p->fim<=now;
      if(past)gfx_cor(pr,rr,.082f,.09f,.106f,1);
      else if(live)gfx_cor(pr,rr,.133f,.145f,.169f,1);
      else gfx_cor(pr,rr,.102f,.106f,.121f,1);
      if(live)gfx_cor((GfxRect){pr.x+14,pr.y+pr.h-6,(pr.w-28)*gv_progresso(p),3},.5f,.953f,.949f,.937f,.55f);
    }
    return;
  }
  gv_avatar(c->logo,c->nome,(GfxRect){cr.x+14,y+12,52,52});
  char num[64];gv_num_quali(c,num,sizeof num);
  gv_label(TXT_ILHA_CORPO,c->nome,cr.x+80,y+12,c->fav?200:228,0);
  gv_label(TXT_ILHA_SUB,num,cr.x+80,y+42,228,1);
  if(c->fav)gfx_icone((GfxRect){cr.x+284,y+12,22,22},"aj_star",.949f,.765f,.282f,1);
  if(!c->np){
    gv_label(TXT_ILHA_CORPO,i18n("Sem programação"),GV_GRID_X+28,y+12,500,1);
    gv_label(TXT_ILHA_SUB,i18n("Vincule um guia ao canal · segure OK"),GV_GRID_X+28,y+42,600,1);
  }
  int ink=ajustes_tinta_foco();
  for(int j=0;j<c->np;j++){
    EpgProg *p=&c->p[j];GfxRect pr=gv_cel(p,y);
    if(pr.w<=8||pr.x>=1920)continue;
    pr.w-=8;
    int f=pf==p&&!gv_overlay(),past=p->fim<=now;
    int tc=f?ink:past?150:243;
    float bell=lembrete_achar(c->id,p->ini,p->titulo)>=0&&pr.w>90?30:0;
    {TxtLinha l=txt_linha_corta_bruta(TXT_ILHA_CORPO,p->titulo,tc,tc,tc,255,pr.w-40-bell);
     txt_desenhar(l,pr.x+20,y+12);}
    char hi[8],hf[8],q[32];gv_hora(p->ini,hi,8);gv_hora(p->fim,hf,8);snprintf(q,sizeof q,"%s–%s",hi,hf);
    {int t2=f?ink:past?120:170;TxtLinha l=txt_linha_corta_bruta(TXT_ILHA_SUB,q,t2,t2,t2,255,pr.w-40);
     txt_desenhar(l,pr.x+20,y+42);}
    if(bell){float b=f?ink/255.0f:.85f;gfx_icone((GfxRect){pr.x+pr.w-38,y+14,22,22},"aj_bell",b,b,b,1);}
  }
}
static void gv_grid(float base){
  float y=base+gv_hh+20+108+16;
  int vis=gv_vis(base);
  if(gv_lin<gv_top)gv_top=gv_lin;
  if(gv_lin>=gv_top+vis)gv_top=gv_lin-vis+1;
  if(gv_top>gv_match-vis)gv_top=gv_match-vis;
  if(gv_top<0)gv_top=0;
  gv_rol=anim_mola2(&gv_rol_v,gv_rol,(float)gv_top,gv_dt,T21_SCROLL);
  gv_tmin=anim_mola2(&gv_tmin_v,gv_tmin,(float)gv_tdesl,gv_dt,T21_SCROLL);
  /* Regua: meia em meia hora, a pilula "agora" e a linha do instante. */
  time_t now=time(NULL);char hh[8],agora[48];gv_hora(now,hh,8);snprintf(agora,sizeof agora,"%s · %s",hh,i18n("agora"));
  float nx=gv_px(now);
  TxtLinha al=txt_linha_bruta(TXT_ILHA_SUB,agora,18,19,22,255);
  GfxRect pill={fmaxf(GV_GRID_X,nx-(float)al.w/2-14),y-2,(float)al.w+28,36};
  int pillv=nx>=GV_GRID_X&&nx<1900;
  GfxRecorteAnt ant=gfx_recorte_dentro(GV_GRID_X,y-4,1920-GV_GRID_X,44);
  time_t t0=gv_tbase()+(time_t)floorf(gv_tmin)*60;t0-=t0%1800;
  for(time_t t=t0;gv_px(t)<1920;t+=1800){
    float x=gv_px(t);char hi[8];gv_hora(t,hi,8);
    TxtLinha l=txt_linha_bruta(TXT_ILHA_SUB,hi,154,156,158,255);
    if(pillv&&x+(float)l.w>pill.x-8&&x<pill.x+pill.w+8)continue;
    if(x>=GV_GRID_X-1)txt_desenhar(l,x,y+4);
  }
  if(pillv){gfx_cor(pill,.5f,.953f,.949f,.937f,1);txt_desenhar(al,pill.x+14,pill.y+(36-(float)al.h)/2);}
  gfx_recorte_voltar(ant);
  float y0=y+38;
  ant=gfx_recorte_dentro(GV_MX,y0-4,1920-GV_MX,1004-(y0-4));
  int i0=(int)floorf(gv_rol),i1=i0+vis+1;
  for(int pass=0;pass<2;pass++){
    for(int i=i0;i<=i1;i++){
      if(i<gv_off||i>=gv_off+gv_n)continue;
      float ry=y0+((float)i-gv_rol)*(GV_ROW_H+GV_ROW_GAP);
      /* Parado, so linhas inteiras; rolando, a que entra desliza pela borda. */
      int rolando=fabsf(gv_rol-(float)gv_top)>.02f;
      if(ry+GV_ROW_H<y0-4||ry>1004||(!rolando&&ry+GV_ROW_H>1004))continue;
      gv_row(ry,&gv_ch[i-gv_off],i==gv_lin,pass);
      if(!pass)ponteiro_alvo(GV_MX,ry,GV_MW,GV_ROW_H,point_focus,point_activate,1500+i,0);
    }
    if(pass)continue;
    /* Foco: a linha do canal ganha superficie; o programa, a pilula no acento. */
    GVCh *fc=gv_focus();
    if(gv_zone==0&&fc&&!gv_overlay()){
      float fy=y0+((float)gv_lin-gv_top)*(GV_ROW_H+GV_ROW_GAP);
      plrui_linha_foco(gv_glide(&gv_gcanal,(GfxRect){GV_MX,fy,320,GV_ROW_H}),20,1);
      EpgProg *p=gv_prog(fc);
      if(p){GfxRect pr=gv_cel(p,fy);pr.w-=8;if(pr.x<1920&&pr.w>8)plrui_pilula_foco(gv_glide(&gv_gfoco,pr),1);}
      else gv_gfoco.ok=0;
    }else{gv_gcanal.ok=0;gv_gfoco.ok=0;}
  }
  if(pillv){
    float last=y0+(fminf((float)vis,(float)(gv_match-gv_top)))*(GV_ROW_H+GV_ROW_GAP)-GV_ROW_GAP;
    gfx_cor((GfxRect){nx-1,y0-4,3,fminf(last,1004)-(y0-4)},0,.953f,.949f,.937f,.85f);
  }
  gfx_recorte_voltar(ant);
}

/* --- modo cartoes ---------------------------------------------------------- */
static void gv_cards(float base){
  float y=base+208+20+108+16;
  for(int i=0;i<gv_n&&i<10;i++){
    GVCh *c=&gv_ch[i];float x=GV_MX+(i%5)*288;
    GfxRect r={x,y+(i/5)*236,272,216};float rr=20.0f/216;
    int foc=gv_zone==6&&gv_card==i&&!gv_overlay();
    if(foc)plrui_linha_foco(r,20,1);
    else if(ajustes_vidro())gfx_cor(r,rr,1,1,1,.05f);else gfx_cor(r,rr,.086f,.094f,.11f,1);
    GfxRect tile={r.x+12,r.y+12,r.w-24,56};
    float r2,g2,b2;gv_hsl(gv_hue(c->nome),.45f,.30f,&r2,&g2,&b2);
    gfx_cor(tile,16.0f/56,r2,g2,b2,1);
    gv_avatar(c->logo,c->nome,(GfxRect){tile.x+(tile.w-44)/2,tile.y+6,44,44});
    const char *qs=gv_quali(c->nome);
    if(qs[0]){TxtLinha q=txt_linha_bruta(TXT_ILHA_SUB,qs,243,242,239,255);
      gfx_cor((GfxRect){r.x+20,r.y+22,(float)q.w+20,32},.5f,.051f,.051f,.051f,.7f);
      txt_desenhar(q,r.x+30,r.y+22+(32-(float)q.h)/2);}
    if(c->fav)gfx_icone((GfxRect){r.x+r.w-44,r.y+26,22,22},"aj_star",.949f,.765f,.282f,1);
    gv_label(TXT_ILHA_NOME,c->nome,r.x+18,r.y+80,r.w-36,0);
    EpgProg *p=gv_prog(c);
    if(p&&p->fim>time(NULL)){
      gv_label(TXT_ILHA_CORPO,p->titulo,r.x+18,r.y+112,r.w-36,0);
      char m2[140];
      if(p->ini>time(NULL)){char hi[8];gv_hora(p->ini,hi,8);snprintf(m2,sizeof m2,"%s",hi);}
      else snprintf(m2,sizeof m2,i18n("%ld min restantes"),(long)(p->fim-time(NULL))/60);
      gv_label(TXT_ILHA_SUB,m2,r.x+18,r.y+140,r.w-36,1);
      GfxRect bar={r.x+18,r.y+170,r.w-36,4};gfx_cor(bar,.5f,1,1,1,.16f);
      gfx_cor((GfxRect){bar.x,bar.y,bar.w*gv_progresso(p),4},.5f,.953f,.949f,.937f,1);
      int pi=(int)(p-c->p);EpgProg *nx=pi+1<c->np?&c->p[pi+1]:NULL;
      if(nx){snprintf(m2,sizeof m2,i18n("A seguir: %s"),nx->titulo);gv_label(TXT_ILHA_SUB,m2,r.x+18,r.y+182,r.w-36,1);}
    }else{
      gv_label(TXT_ILHA_CORPO,i18n("Sem programação"),r.x+18,r.y+112,r.w-36,1);
      gv_label(TXT_ILHA_SUB,i18n("Segure OK para vincular guia"),r.x+18,r.y+140,r.w-36,1);
    }
    ponteiro_alvo(r.x,r.y,r.w,r.h,point_focus,point_activate,1600+i,0);
  }
}
/* --- estado vazio: adicione sua primeira fonte ----------------------------- */
static void gv_vazio(void){
  gv_label(TXT_TITULO2,i18n("Adicione sua primeira fonte"),GV_MX+10,GV_BASE+28,1000,0);
  gv_label(TXT_ILHA_CORPO,i18n("O Nuvio não inclui canais. Use as listas que o seu provedor entregou."),GV_MX+10,GV_BASE+94,1300,1);
  const char *ts[]={"Lista M3U","Acesso Xtream","Guia XMLTV"};
  const char *ds[]={"Um endereço .m3u ou .m3u8 com a sua lista de canais.","Servidor, usuário e senha do seu provedor.","Programação dos canais que você já tem."};
  const char *bs[]={"Digitar URL","Informar acesso","Digitar URL"};
  const char *icons[]={"aj_link","aj_key","aj_file-text"};
  for(int i=0;i<3;i++){
    float x=GV_MX+i*488;GfxRect r={x,GV_BASE+150,464,384};
    if(ajustes_vidro())gfx_cor(r,28.0f/384,1,1,1,.04f);else gfx_cor(r,28.0f/384,.086f,.094f,.11f,1);
    gfx_cor((GfxRect){x+32,GV_BASE+182,72,72},20.0f/72,.141f,.149f,.173f,1);
    gfx_icone((GfxRect){x+50,GV_BASE+200,36,36},icons[i],.95f,.95f,.95f,1);
    gv_label(TXT_TITULO2,i18n(ts[i]),x+32,GV_BASE+280,400,0);
    txt_bloco(TXT_ILHA_CORPO,i18n(ds[i]),154,156,158,x+32,GV_BASE+344,400,32,1,3);
    gv_button(1700+i,i18n(bs[i]),NULL,x+32,GV_BASE+466,320,0,0);
  }
  gfx_icone((GfxRect){GV_MX,GV_BASE+600,28,28},"aj_smartphone",.7f,.71f,.72f,1);
  gv_label(TXT_ILHA_CORPO,i18n("Digitar um endereço longo é lento: escolha “Digitar no celular” na próxima tela."),GV_MX+40,GV_BASE+606,1200,1);
}
/* --- faixa de erro das fontes (fontes-erro) -------------------------------- */
static void gv_strip(void){
  GfxRect r={GV_X,168,GV_MW+GV_SB_W+24,72};gfx_cor(r,24.0f/72,.165f,.102f,.102f,1);
  gfx_icone((GfxRect){r.x+24,186,32,32},"aj_triangle-alert",.95f,.6f,.43f,1);
  /* title over status, buttons right-aligned inside the strip */
  float b2=r.x+r.w-16-250,b1=b2-12-290;
  gv_label(TXT_ILHA_NOME,i18n("Lista por URL: acesso recusado (401)"),r.x+72,176,b1-r.x-96,0);
  gv_label(TXT_ILHA_SUB,i18n("Última atualização há 2 dias · 86 canais preservados"),r.x+72,206,b1-r.x-96,1);
  gv_button(1710,i18n("Tentar novamente"),"aj_rotate-cw",b1,179,290,0,1);
  gv_button(1711,i18n("Diagnóstico"),"aj_activity",b2,179,250,0,1);
}

/* --- folhas (sheets) do guia principal ------------------------------------- */
/* Ilha da folha (DESIGN 2): material da ilha, raio 28, sem contorno. */
static float gv_sheet_base(float x,float y,float w,float h){
  t21_ilha_folha((GfxRect){x,y,w,h},T21_R_SHEET,1);return x+32;
}
static void gv_sheet_head(float x,float y,const char *title,const char *sub){
  gv_label(TXT_TITULO2,title,x,y,900,0);
  gv_label(TXT_ILHA_CORPO,sub,x,y+58,1200,1);
}
/* Linha de folha: so a focada ganha superficie (plrui_linha_foco). */
static void gv_opt(int id,const char *icon,const char *name,const char *sub,float x,float y,float w,int h){
  GfxRect r={x,y,w,h};float rr=20.0f/h;
  int f=gv_btn_foc(id);
  if(f)plrui_linha_foco(r,20,1);
  else if(ajustes_vidro())gfx_cor(r,rr,1,1,1,.05f);else gfx_cor(r,rr,.114f,.118f,.137f,1);
  gfx_icone((GfxRect){x+20,y+(h-25)/2,25,25},icon,.953f,.949f,.937f,1);
  if(sub){gv_label(TXT_ILHA_NOME,name,x+60,y+(h-54)/2,w-80,0);gv_label(TXT_ILHA_SUB,sub,x+60,y+(h-54)/2+30,w-80,1);}
  else{TxtLinha l=txt_linha_corta_bruta(TXT_ILHA_NOME,name,243,242,239,255,w-80);txt_desenhar(l,x+60,y+(h-(float)l.h)/2);}
  ponteiro_alvo(x,y,w,h,point_focus,point_activate,id,0);
  gv_registra(id,x,y,w,h);
}
/* Resultados da busca: ids guardados no desenho para o OK achar o canal. */
static char gv_bres[8][80];
static void gv_busca_sheet(void){
  gv_dim(0,.42f);
  float x=gv_sheet_base(340,100,1240,700);
  GfxRect f={x,132,1176,64};int ff=gv_btn_foc(1799);
  if(ff)plrui_linha_foco(f,20,1);else gfx_cor(f,20.0f/64,.141f,.149f,.173f,1);
  gfx_icone((GfxRect){x+20,150,28,28},"aj_search",.8f,.8f,.8f,1);
  gv_label(TXT_ILHA_NOME,query[0]?query:i18n("Buscar canais, filmes e séries"),x+60,150,760,!query[0]);
  {TxtLinha l=txt_linha(TXT_ILHA_SUB,"Canais e programas",154,156,158,255);txt_desenhar(l,x+1150-(float)l.w,154);}
  ponteiro_alvo(f.x,f.y,f.w,f.h,point_focus,point_activate,1799,0);
  gv_registra(1799,f.x,f.y,f.w,f.h);
  char c[96];
  snprintf(c,sizeof c,"%s  %d",i18n("Todas as fontes"),gv_total_n);
  {float w=(float)txt_linha_bruta(TXT_ILHA_CORPO,c,243,242,239,255).w+40;gv_chip(x,216,w,c,NULL,1,0,0,0);}
  int nm=0,np2=0;
  char needle[140];busca_normalizar(query,needle,sizeof needle);
  const char *id,*nome,*cat;
  if(needle[0])for(int i=0;nm<4&&guia_canal_regra_campos(i,NULL,NULL,NULL,NULL,NULL);i++){
    if(!guia_canal_campos(i,&id,&nome,NULL,&cat,NULL))continue;
    char hay[256];busca_normalizar(nome,hay,sizeof hay);
    if(!strstr(hay,needle))continue;
    GVCh m={0};snprintf(m.nome,sizeof m.nome,"%s",nome);m.num=gv_num_de(i);
    char meta[160],nq[48];gv_num_quali(&m,nq,sizeof nq);snprintf(meta,sizeof meta,"%s · %s",nq,cat&&cat[0]?cat:"IPTV");
    snprintf(gv_bres[nm],sizeof gv_bres[nm],"%s",id);
    gv_opt(1800+nm,"aj_tv-minimal-play",nome,meta,x,348+nm*70,1176,64);nm++;
  }
  snprintf(c,sizeof c,i18n("CANAIS · %d"),nm);
  gv_label(TXT_ILHA_NOME,c,x,304,400,1);
  float my=348+nm*70+18;
  for(int i=0;i<gv_n&&np2<3;i++){
    EpgProg *p=gv_prog(&gv_ch[i]);if(!p)continue;
    char hay[256];busca_normalizar(p->titulo,hay,sizeof hay);
    if(needle[0]&&!strstr(hay,needle))continue;
    char meta[160];snprintf(meta,sizeof meta,"%s · %d",gv_ch[i].nome,gv_ch[i].num);
    snprintf(gv_bres[4+np2],sizeof gv_bres[4+np2],"%s",gv_ch[i].id);
    gv_opt(1810+np2,"aj_play",p->titulo,meta,x,my+44+np2*70,1176,64);np2++;
  }
  snprintf(c,sizeof c,i18n("PROGRAMAS · %d"),np2);
  gv_label(TXT_ILHA_NOME,c,x,my,400,1);
  {char fr[96];snprintf(fr,sizeof fr,i18n("%d resultados · use o teclado do controle ou o celular"),nm+np2);gv_label(TXT_ILHA_SUB,fr,x,752,1100,1);}
}
/* Ir para o numero: igual primeiro, senao o primeiro que comeca com os
 * digitos; a lista inteira, nao so a janela copiada. */
static int gv_numero_alvo(const char **id_out,const char **nome_out){
  int pref=-1,n=(int)strlen(gv_numero);const char *id,*nome,*pid=NULL,*pnome=NULL;
  if(!n)return -1;
  for(int i=0;guia_canal_regra_campos(i,NULL,NULL,NULL,NULL,NULL);i++){
    if(!guia_canal_campos(i,&id,&nome,NULL,NULL,NULL))continue;
    char nn[24];snprintf(nn,sizeof nn,"%d",gv_num_de(i));
    if(!strcmp(nn,gv_numero)){if(id_out)*id_out=id;if(nome_out)*nome_out=nome;return i;}
    if(pref<0&&!strncmp(nn,gv_numero,(size_t)n)){pref=i;pid=id;pnome=nome;}
  }
  if(pref>=0){if(id_out)*id_out=pid;if(nome_out)*nome_out=pnome;}
  return pref;
}
static void gv_numero_sheet(void){
  gv_dim(0,.42f);
  float x=gv_sheet_base(520,230,880,560);
  gv_label(TXT_ILHA_NOME,i18n("IR PARA CANAL"),x,262,400,1);
  TxtLinha t=txt_linha_bruta(TXT_V2_TIT,gv_numero[0]?gv_numero:"_",243,242,239,255);
  txt_desenhar(t,x,296);
  float pisca=.35f+.35f*(1+sinf(SDL_GetTicks()/180.0f))*.5f;
  gfx_cor((GfxRect){x+(float)t.w+16,304,5,(float)t.h-16},0,.953f,.949f,.937f,pisca);
  const char *id=NULL,*nome=NULL;int alvo=gv_numero_alvo(&id,&nome);
  if(alvo>=0){
    GVCh m={0};snprintf(m.nome,sizeof m.nome,"%s",nome);m.num=gv_num_de(alvo);
    EpgProg pp;char meta[200],nq[48];gv_num_quali(&m,nq,sizeof nq);
    if(guia_programa_agora(id,time(NULL),&pp)&&pp.titulo)snprintf(meta,sizeof meta,"%s · %s",nq,pp.titulo);
    else snprintf(meta,sizeof meta,"%s",nq);
    gv_opt(1900,"aj_tv-minimal-play",nome,meta,x,452,816,84);
  }
  gv_label(TXT_ILHA_SUB,i18n("Também corresponde…"),x,552,400,1);
  {int k=0,n=(int)strlen(gv_numero);const char *cid,*cnome;
   for(int i=0;n&&k<2&&guia_canal_regra_campos(i,NULL,NULL,NULL,NULL,NULL);i++){
    if(i==alvo||!guia_canal_campos(i,&cid,&cnome,NULL,NULL,NULL))continue;
    char nn[24];snprintf(nn,sizeof nn,"%d",gv_num_de(i));
    if(strncmp(nn,gv_numero,(size_t)n))continue;
    GfxRect r2={x,596+k*64,816,58};gfx_cor(r2,18.0f/58,.114f,.118f,.137f,1);
    gv_label(TXT_ILHA_SUB,nn,x+20,596+k*64+17,80,1);
    gv_label(TXT_ILHA_CORPO,cnome,x+110,596+k*64+14,680,0);k++;}}
  gv_label(TXT_ILHA_CORPO,i18n("OK para ir agora · vai sozinho em 2 s"),x,740,780,1);
}
static void gv_opcoes_sheet(void){
  gv_dim(0,.42f);
  float x=gv_sheet_base(660,196,600,500);
  GVCh *c=gv_canal_focado_ou_primeiro();
  if(!c)return;
  gv_avatar(c->logo,c->nome,(GfxRect){x,228,60,60});
  {char tt[180];snprintf(tt,sizeof tt,"%d %s",c->num,c->nome);gv_label(TXT_TITULO2,tt,x+80,226,440,0);}
  char meta[96],nq[48];gv_num_quali(c,nq,sizeof nq);snprintf(meta,sizeof meta,"%s · %s",c->grupo[0]?c->grupo:"IPTV",nq);
  gv_label(TXT_ILHA_SUB,meta,x+80,274,440,1);
  gv_opt(1910,"aj_star",i18n(c->fav?"Remover dos favoritos":"Adicionar aos favoritos"),NULL,x,320,536,76);
  {EpgProg *p=gv_prog(c);
   gv_opt(1911,"aj_bell",i18n("Criar lembrete"),p?p->titulo:i18n("Sem programação"),x,404,536,76);}
  gv_opt(1912,"aj_calendar",i18n("Vincular guia manualmente"),i18n("Agora: vinculado pelo nome (95%)"),x,488,536,76);
  gv_opt(1913,"aj_eye-off",i18n("Ocultar este canal"),i18n("Desfazer em Categorias ▸ Mostrar / ocultar"),x,572,536,76);
}
static void gv_canal_opcoes_sheet(void){
  gv_dim(0,.42f);
  float x=gv_sheet_base(360,190,1440,680);
  GVCh *c=gv_canal_focado_ou_primeiro();
  if(!c)return;
  {char titulo[160];snprintf(titulo,sizeof titulo,"%d %s",c->num,c->nome);
   gv_sheet_head(x,222,titulo,i18n("Segure OK num canal · opções do canal"));}
  const char *ops[]={"Adicionar aos favoritos","Ocultar canal","Editar nome do canal","Vincular canal EPG","Buscar logo do canal","Adicionar à pasta","Criar lembrete"};
  const char *oicons[]={"aj_star","aj_eye-off","aj_pencil","aj_calendar","aj_search","aj_folders","aj_bell"};
  for(int i=0;i<7;i++)gv_opt(1920+i,oicons[i],i18n(i==0&&c->fav?"Remover dos favoritos":ops[i]),NULL,x,318+i*66,560,58);
  float rx=x+620;
  gv_label(TXT_TITULO2,i18n("Vincular guia manualmente"),rx,300,700,0);
  GfxRect f={rx,352,700,60};gfx_cor(f,20.0f/60,.114f,.118f,.137f,1);
  gfx_icone((GfxRect){rx+18,369,26,26},"aj_search",.6f,.6f,.6f,1);
  gv_label(TXT_ILHA_CORPO,i18n("Buscar canais EPG:"),rx+56,370,600,1);
  for(int i=0;i<2;i++){
    GfxRect mr={rx,432+i*88,700,80};gfx_cor(mr,18.0f/80,.082f,.09f,.106f,1);
    gv_label(TXT_ILHA_NOME,i==0?c->nome:i18n("Outro guia"),rx+20,mr.y+12,660,0);
    gv_label(TXT_ILHA_SUB,i?i18n("Guia mundial · nome parecido"):i18n("XMLTV do provedor · id exato"),rx+20,mr.y+44,660,1);
  }
  gv_button(1930,i18n("Usar correspondência"),"aj_check",rx,620,430,0,1);
  gfx_cor((GfxRect){rx,692,700,1},0,1,1,1,.10f);
  gv_label(TXT_ILHA_SUB,i18n("Limpeza do nome"),rx,708,400,1);
  {TxtLinha l=txt_linha_corta_bruta(TXT_ILHA_CORPO,c->nome,243,242,239,255,560);txt_desenhar(l,rx,742);
   const char *q=gv_quali(c->nome);if(q[0])gv_badge(q,rx+(float)l.w+16,736);}
  txt_bloco(TXT_ILHA_SUB,i18n("Etiquetas do provedor viram selos. O nome original fica para casar o guia."),154,156,158,rx,790,700,26,1,2);
}
static void gv_ajustes_sheet(void){
  gv_dim(0,.42f);
  float x=gv_sheet_base(360,190,1440,800);
  gv_sheet_head(x,222,i18n("Opções da grade"),i18n("A prévia é opcional. Imagens do programa continuam padrão em TVs com 1 GB."));
  struct {const char *nome,*sub;} rows[]={
    {"Prévia ao vivo no guia","Sem som por padrão. Uma prévia por vez."},
    {"Tamanho da prévia","Grande ocupa a lateral direita do destaque, com o véu preto. Como o guia de hoje."},
    {"Quando iniciar a prévia","Escolha quando um vídeo começa."},
    {"Prévia sem som","Nenhum som enquanto navega."},
    {"Alcance do guia","Menos dias usam menos memória."},
    {"Material Glass",""},
  };
  for(int i=0;i<6;i++){
    float y=336+i*88;
    gv_label(TXT_ILHA_NOME,i18n(rows[i].nome),x,y,800,0);
    if(rows[i].sub[0])gv_label(TXT_ILHA_SUB,i18n(rows[i].sub),x,y+34,800,1);
    float bx=x+1376;
    if(i==0)gv_button(1940,guia_preview_ligado()?i18n("Ativada"):i18n("Desativada"),NULL,bx-190,y,190,guia_preview_ligado(),1);
    if(i==1){int big=ajustes_guia_previa_grande();gv_button(1941,i18n("Cartão"),NULL,bx-330,y,160,!big,1);gv_button(1942,i18n("Grande"),NULL,bx-160,y,160,big,1);}
    if(i==2){gv_button(1943,i18n("Ao destacar"),NULL,bx-490,y,230,0,1);gv_button(1944,i18n("Ao selecionar"),NULL,bx-250,y,250,1,1);}
    if(i==3)gv_button(1945,i18n("Ativada"),NULL,bx-190,y,190,1,1);
    if(i==4){gv_button(1946,i18n("Um dia"),NULL,bx-410,y,190,1,1);gv_button(1947,i18n("Três dias"),NULL,bx-210,y,210,0,1);}
    if(i==5)gv_button(1948,ajustes_vidro()?i18n("Ativada"):i18n("Desativada"),NULL,bx-190,y,190,ajustes_vidro(),1);
    if(i<5)gfx_cor((GfxRect){x,y+72,1376,1},0,1,1,1,.08f);
  }
  gv_button(1949,i18n("Saúde do EPG"),"aj_calendar",x,898,360,0,0);
  gv_label(TXT_ILHA_SUB,i18n("Ao selecionar inicia a prévia · selecionar de novo abre tela cheia."),x+390,914,980,1);
}
static void gv_pastas_sheet(void){
  gv_dim(0,.42f);
  float x=gv_sheet_base(360,190,1440,420);
  gv_sheet_head(x,222,i18n("Suas pastas"),i18n("Pastas agrupam categorias de várias fontes. Os canais ficam nas categorias originais."));
  gv_button(1950,i18n("Nova pasta"),"aj_folders",x,336,300,1,1);
  gv_label(TXT_ILHA_SUB,i18n("Ordem"),x,412,100,1);
  gv_button(1951,i18n("Mais assistidos"),NULL,x+80,400,240,0,1);
  gv_button(1952,i18n("A–Z"),NULL,x+330,400,110,0,1);
  gv_button(1953,i18n("Personalizada"),NULL,x+450,400,230,1,1);
  txt_bloco(TXT_ILHA_CORPO,i18n("Sem pastas ainda. Crie uma para reunir as categorias que você mais abre."),154,156,158,x,480,680,32,1,2);
  float rx=x+760;
  gv_label(TXT_TITULO2,i18n("Categorias nesta pasta"),rx,336,616,0);
  {float bx=rx,by=392;
  for(int i=0;i<4&&i<gv_cats;i++){
    char b[96];snprintf(b,sizeof b,"%s · %d",gv_cat[i],gv_cat_n[i]);
    float w=(float)txt_linha_bruta(TXT_ILHA_CORPO,b,243,242,239,255).w+24;
    if(bx+w>rx+616){bx=rx;by+=56;}
    gfx_cor((GfxRect){bx,by,w,44},12.0f/44,.168f,.176f,.204f,1);
    gv_label(TXT_ILHA_CORPO,b,bx+12,by+8,w-20,0);bx+=w+12;}
  gv_label(TXT_ILHA_SUB,i18n("Excluir a pasta remove só a pasta. Categorias e canais ficam."),rx,by+70,616,1);}
}
static void gv_epg_sheet(void){
  gv_dim(0,.42f);
  int failed=!strcmp(screen,"epg-falha"),loaded=!strcmp(screen,"epg-adicionado");
  float x=gv_sheet_base(360,190,1440,500);
  gv_sheet_head(x,222,i18n("Adicionar fonte XMLTV"),i18n("O guia é verificado antes de salvar."));
  gv_label(TXT_ILHA_SUB,i18n("URL do XMLTV"),x,336,400,1);
  GfxRect f={x,368,700,64};gfx_cor(f,20.0f/64,.114f,.118f,.137f,1);
  gv_label(TXT_ILHA_CORPO,draft.xmltv[0]?draft.xmltv:"https://example.com/guia.xml",x+20,388,600,0);
  const char *ic=failed?"aj_triangle-alert":loaded?"aj_check":"aj_activity";
  gfx_icone((GfxRect){x,462,36,36},ic,failed?.95f:(loaded?.62f:.6f),failed?.6f:(loaded?.87f:.6f),failed?.43f:(loaded?.86f:.6f),1);
  gv_label(TXT_TITULO2,i18n(failed?"Falha na verificação":loaded?"Fonte adicionada e carregada":"Verificando fonte EPG…"),x+56,460,900,0);
  gv_label(TXT_ILHA_CORPO,i18n(failed?"Verifique a URL e tente novamente. O guia atual será mantido.":loaded?"Guia mundial · 384 canais vinculados · guia pronto":"Lendo XMLTV e conferindo os identificadores."),x+56,518,1000,1);
  gv_button(1960,i18n(failed?"Verificar URL":loaded?"Concluir":"Cancelar"),failed?"aj_pencil":loaded?"aj_check":"aj_x",x,590,340,0,0);
  if(failed)gv_button(1961,i18n("Tentar novamente"),"aj_rotate-cw",x+370,590,340,0,0);
}
static void gv_epg_saude_sheet(void){
  gv_dim(0,.42f);
  float x=gv_sheet_base(360,190,1440,700);
  gv_sheet_head(x,222,i18n("Saúde do EPG"),i18n("Dados por fonte · programas mais antigos carregados, não a última atualização."));
  const char *nomes[]={"XMLTV do provedor","Guia do Xtream"};
  const char *estados[]={"Atualizado","Desatualizado"};
  for(int i=0;i<2;i++){
    float y=340+i*128;
    gv_label(TXT_ILHA_NOME,i18n(nomes[i]),x,y,600,0);
    gv_label(TXT_ILHA_SUB,i18n(i?"Programa mais antigo: há 2 dias":"Programa mais antigo: hoje, 04:00"),x,y+34,700,1);
    gv_badge(i18n(estados[i]),x+880,y+6);
    gv_button(1970+i,i18n("Recarregar EPG"),"aj_rotate-cw",x+1060,y-8,260,0,1);
    gfx_cor((GfxRect){x,y+100,1380,1},0,1,1,1,.12f);
  }
  float y=604;
  gv_label(TXT_ILHA_NOME,i18n("Recarregar EPG desatualizado ao abrir"),x,y,800,0);
  gv_label(TXT_ILHA_SUB,i18n("Só fontes desatualizadas atualizam"),x,y+34,800,1);
  gv_button(1972,i18n("Ativada"),NULL,x+1140,y,180,0,1);
  gv_label(TXT_ILHA_NOME,i18n("Alcance do guia"),x,y+98,600,0);
  gv_button(1973,i18n("Um dia"),NULL,x+928,y+96,180,1,1);
  gv_button(1974,i18n("Três dias"),NULL,x+1120,y+96,200,0,1);
  gv_button(1975,i18n("Adicionar XMLTV"),"aj_plus",x,y+180,420,0,0);
  gv_label(TXT_ILHA_SUB,i18n("O guia atual continua se a verificação falhar"),x+460,y+186,800,1);
}
static void gv_erros_sheet(void){
  gv_dim(0,.42f);
  float x=gv_sheet_base(360,190,1440,700);
  gv_sheet_head(x,222,i18n("Recuperação do guia"),i18n("O retorno das fontes e dos vídeos aparece no guia, sem perder o canal escolhido."));
  const char *ts[]={"Lista inacessível","Nenhum canal na lista","Vídeo expirado ou acesso negado","Formato de vídeo incompatível","Transmissão atrasada"};
  const char *ds[]={"Não foi possível ler a lista. Os últimos canais ficam.","Confira a URL ou peça uma lista de canais ao provedor.","O provedor recusou o acesso. Confira suas credenciais.","Escolha um vídeo compatível do seu provedor.","Voltar para a transmissão atual."};
  const char *bs[]={"Tentar de novo","Editar URL","Atualizar acesso","Escolher vídeo","Ir ao vivo"};
  const char *icons[]={"aj_wifi-off","aj_list-video","aj_key","aj_triangle-alert","aj_rotate-ccw-clock"};
  for(int i=0;i<5;i++){
    float y=336+i*108;
    gfx_icone((GfxRect){x,y,32,32},icons[i],.7f,.71f,.72f,1);
    gv_label(TXT_ILHA_NOME,i18n(ts[i]),x+56,y,700,0);
    gv_label(TXT_ILHA_SUB,i18n(ds[i]),x+56,y+34,900,1);
    gv_button(1980+i,i18n(bs[i]),NULL,x+1060,y-8,280,i==4,1);
    gfx_cor((GfxRect){x,y+86,1380,1},0,1,1,1,.12f);
  }
}
static void gv_fontes_erro_modal(void){
  gv_dim(248,.5f);
  float x=gv_sheet_base(1072,272,800,452);
  gv_label(TXT_TITULO2,i18n("Adicionar fonte"),x,300,600,0);
  gv_button(1990,i18n("Lista M3U"),NULL,x,372,180,1,1);
  gv_button(1991,i18n("Guia XMLTV"),NULL,x+192,372,210,0,1);
  gv_button(1992,i18n("Xtream"),NULL,x+414,372,150,0,1);
  gv_label(TXT_ILHA_SUB,i18n("URL"),x,446,100,1);
  GfxRect f={x,480,736,64};gfx_cor(f,20.0f/64,.114f,.118f,.137f,1);
  gv_label(TXT_ILHA_CORPO,draft.playlist[0]?draft.playlist:"https://example.com/lista.m3u",x+20,500,600,0);
  gv_label(TXT_ILHA_SUB,i18n("Verificada antes de salvar. Seus canais continuam disponíveis se o link falhar."),x,560,700,1);
  gv_button(1993,i18n("Testar e adicionar"),"aj_check",x,640,340,1,1);
  gv_button(1994,i18n("Digitar no celular"),"aj_smartphone",x+360,640,320,0,1);
}

/* --- esportes: ligas, jogos e times ---------------------------------------- */
typedef struct{char liga[64],casa[48],fora[48],score[48],estado[48],canais[96];int min;} GVMatch;
static GVMatch *gv_matches(int *n){
  static GVMatch out[6];int k=0;
  for(int i=0;i<gv_n&&k<6;i++){
    if(strcmp(gv_ch[i].grupo,"Esportes"))continue;
    EpgProg *p=gv_prog(&gv_ch[i]);if(!p)continue;
    char placar[96];
    if(gv_placar(p->titulo,placar,sizeof placar)){
      GVMatch *m=&out[k++];
      snprintf(m->liga,sizeof m->liga,"%s",gv_ch[i].grupo);
      const char *x=strstr(placar,"×");   /* "A × B" from gv_placar */
      snprintf(m->casa,sizeof m->casa,"%.*s",x?(int)(x-placar)-1:0,placar);
      snprintf(m->fora,sizeof m->fora,"%s",x?x+strlen("×")+1:"");
      snprintf(m->score,sizeof m->score,"%s",placar);
      long fim=(p->fim-time(NULL))/60;
      snprintf(m->estado,sizeof m->estado,"%s",fim<0?i18n("Encerrado"):i18n("Ao vivo"));
      m->min=(int)((time(NULL)-p->ini)/60);
      int nc=0;   /* channels airing this same match, not the channel number */
      for(int j=0;j<gv_n;j++){EpgProg *q=gv_prog(&gv_ch[j]);if(q&&!strcmp(q->titulo,p->titulo))nc++;}
      if(nc>1)snprintf(m->canais,sizeof m->canais,"%d",nc);
      else snprintf(m->canais,sizeof m->canais,"%s",gv_ch[i].nome);
    }
  }
  *n=k;return out;
}
static void gv_sports_header(const char *tit,const char *sub,const char *on){
  gv_label(TXT_ILHA_SUB,i18n("GUIA TV / ESPORTES"),GV_MX,180,600,1);
  gv_label(TXT_TITULO2,i18n(tit),GV_MX,214,900,0);
  gv_label(TXT_ILHA_CORPO,i18n(sub),GV_MX,272,900,1);
  float x=GV_MX+GV_MW-180-12-210-12-150;   /* right-aligned to the content edge */
  gv_button(2000,i18n("Ligas"),NULL,x,190,150,on&&!strcmp(on,"ligas"),1);
  gv_button(2001,i18n("Favoritos"),"aj_star",x+162,190,210,on&&!strcmp(on,"favoritos"),1);
  gv_button(2002,i18n("Ajustes"),"aj_sliders-horizontal",x+384,190,180,0,1);
}
static void gv_league_card(int i,const char *code,const char *name,const char *sub,const char *icon,float x,float y,int venue){
  GfxRect r={x,y,348,210};float rr=24.0f/210;
  int f=gv_btn_foc(2100+i);
  if(venue){float r2,g2,b2;gv_hsl(gv_hue(name),.3f,.22f,&r2,&g2,&b2);gfx_cor(r,rr,r2,g2,b2,.85f);}
  else gfx_cor(r,rr,.098f,.114f,.133f,1);
  if(f)plrui_linha_foco(r,24,1);
  ponteiro_alvo(r.x,r.y,r.w,r.h,point_focus,point_activate,2100+i,0);
  gv_registra(2100+i,r.x,r.y,r.w,r.h);
  gfx_cor((GfxRect){x+24,y+24,64,64},18.0f/64,.168f,.196f,.216f,1);
  gfx_icone((GfxRect){x+38,y+38,36,36},icon,.9f,.91f,.92f,1);
  gv_label(TXT_TITULO2,code,x+104,y+34,180,0);
  gv_label(TXT_ILHA_NOME,name,x+24,y+108,300,0);
  gv_label(TXT_ILHA_CORPO,sub,x+24,y+148,300,1);
}
static void gv_sports_bottom(float y){
  GfxRect r={GV_MX,y,GV_MW,86};gfx_cor(r,24.0f/86,.071f,.075f,.086f,1);
  gfx_icone((GfxRect){GV_MX+24,y+28,28,28},"aj_image",.7f,.71f,.72f,1);
  gv_label(TXT_ILHA_NOME,i18n("Fundo por esporte"),GV_MX+66,y+20,400,0);
  gv_label(TXT_ILHA_SUB,i18n("Cenário ilustrativo · qualidade da imagem"),GV_MX+66,y+50,600,1);
  gv_button(2010,i18n("Auto"),NULL,GV_MX+820,y+16,140,1,1);
  gv_button(2011,i18n("Baixa"),NULL,GV_MX+970,y+16,150,0,1);
  gv_button(2012,i18n("Alta"),NULL,GV_MX+1130,y+16,140,0,1);
}
static void gv_match_card(int id,GVMatch *m,float x,float y){
  GfxRect r={x,y,466,244};float rr=24.0f/244;
  gfx_cor(r,rr,.094f,.11f,.129f,1);
  if(gv_btn_foc(id))plrui_linha_foco(r,24,1);
  ponteiro_alvo(r.x,r.y,r.w,r.h,point_focus,point_activate,id,0);
  gv_registra(id,r.x,r.y,r.w,r.h);
  gv_label(TXT_ILHA_SUB,m->liga,r.x+24,r.y+20,240,1);
  gv_label(TXT_ILHA_NOME,m->estado,r.x+r.w-160,r.y+20,140,0);
  char sig[2][8]={{0},{0}};
  for(int t=0;t<2;t++){
    int k=0;   /* ASCII letters only: never split a UTF-8 sequence */
    for(const char *s=t?m->fora:m->casa;*s&&k<3;s++)if(isalnum((unsigned char)*s)&&!((unsigned char)*s&0x80))sig[t][k++]=(char)toupper((unsigned char)*s);
    float r2,g2,b2,tx=t?r.x+r.w-114:r.x+40;gv_hsl(gv_hue(sig[t]),.5f,.32f,&r2,&g2,&b2);
    gfx_cor((GfxRect){tx,r.y+76,74,80},10.0f/80,r2,g2,b2,1);
    TxtLinha l=txt_linha_corta_bruta(TXT_W20_24B,sig[t],243,242,239,255,70);txt_desenhar(l,tx+(74-(float)l.w)/2,r.y+76+(80-(float)l.h)/2);
  }
  /* no real score in the EPG: both team names, then the match minute */
  const char *mid[3]={m->casa,m->fora,NULL};char min[32];snprintf(min,sizeof min,i18n("%d min"),m->min);mid[2]=min;
  for(int k=0;k<3;k++){
    TxtLinha l=txt_linha_corta_bruta(k<2?TXT_ILHA_NOME:TXT_ILHA_SUB,mid[k],k<2?243:154,k<2?242:156,k<2?239:158,255,210);
    txt_desenhar(l,r.x+(r.w-(float)l.w)/2,r.y+72+k*32);
  }
  char cc[128];
  if(isdigit((unsigned char)m->canais[0]))snprintf(cc,sizeof cc,i18n("%s canais disponíveis"),m->canais);
  else snprintf(cc,sizeof cc,"%s",m->canais);
  gv_label(TXT_ILHA_CORPO,cc,r.x+24,r.y+186,240,1);
  gv_label(TXT_ILHA_CORPO,i18n("Onde assistir"),r.x+r.w-190,r.y+186,190,0);
}
static void gv_sports_screen(void){
  if(!strcmp(screen,"esportes-ligas")){
    gv_sports_header("Seu esporte. Sua liga.","Jogos e placares por competição","ligas");
    gv_label(TXT_TITULO2,i18n("Futebol"),GV_MX,340,400,0);
    gv_label(TXT_ILHA_SUB,i18n("4 ligas · 6 jogos ao vivo"),GV_MX+980,348,440,1);
    const char *codes[]={"BR","LIB","PL","LIGA"},*names[]={"Brasileirão Série A","CONMEBOL Libertadores","Premier League","LaLiga"};
    const char *subs[]={NULL,NULL,NULL,NULL};(void)subs;
    for(int i=0;i<4;i++){
      static const char *const subs[]={"2 ao vivo · 10 jogos","Amanhã · 2 jogos","Sábado · 8 jogos","Sábado · 5 jogos"};
      gv_league_card(i,codes[i],names[i],i18n(subs[i]),i<2?"aj_activity":"aj_play",GV_MX+i*366,392,i==0);
    }
    gv_label(TXT_TITULO2,i18n("Basquete, tênis e automobilismo"),GV_MX,618,800,0);
    gv_label(TXT_ILHA_SUB,i18n("Outros esportes"),GV_MX+980,626,440,1);
    const char *c2[]={"NBA","NBB","ATP","F1"},*n2[]={"NBA","NBB","Circuito ATP","Fórmula 1"};
    for(int i=0;i<4;i++){
      static const char *const subs2[]={"Hoje · 8 jogos","Hoje · 3 jogos","3 partidas ao vivo","Domingo · Grande Prêmio"};
      gv_league_card(i+4,c2[i],n2[i],i18n(subs2[i]),"aj_play",GV_MX+i*366,670,0);
    }
    gv_sports_bottom(900);
    gv_label(TXT_ILHA_SUB,i18n("16 esportes · Ligas visíveis neste perfil"),GV_MX,1000,600,1);
  }else if(!strcmp(screen,"esportes-liga")){
    gv_sports_header("Brasileirão Série A","24ª rodada · Hoje, 22 de outubro","ligas");
    float x=GV_MX;
    gv_button(2020,i18n("Ligas"),"pl_chevron-left",x,314,180,0,1);
    gv_button(2021,i18n("Hoje 10"),NULL,x+200,314,180,1,1);
    gv_button(2022,i18n("Amanhã 4"),NULL,x+390,314,190,0,1);
    gv_button(2023,i18n("Sábado 8"),NULL,x+590,314,190,0,1);
    gv_label(TXT_TITULO2,i18n("Ao vivo"),GV_MX,400,300,0);
    gv_label(TXT_ILHA_SUB,i18n("Horários de Brasília"),GV_MX+980,408,440,1);
    int nm;GVMatch *m=gv_matches(&nm);
    for(int i=0;i<nm&&i<3;i++)gv_match_card(2200+i,&m[i],GV_MX+(i%3)*486,452);
    /* the rest of the round, never the live cards repeated */
    if(nm>3)gv_label(TXT_TITULO2,i18n("Mais jogos da rodada"),GV_MX,740,600,0);
    for(int i=3;i<nm&&i<6;i++)gv_match_card(2210+i-3,&m[i],GV_MX+(i%3)*486,792);
  }else if(!strcmp(screen,"esportes-favoritos")){
    gv_sports_header("Seus times, todas as competições","Uma só fileira para acompanhar quem você torce","favoritos");
    gv_label(TXT_TITULO2,i18n("Times favoritos"),GV_MX,340,400,0);
    for(int i=0;i<4&&i<gv_n;i++){
      GfxRect r={GV_MX+i*242,392,224,230};float rr=24.0f/230;
      gfx_cor(r,rr,.114f,.118f,.137f,1);
      if(gv_btn_foc(2300+i))plrui_linha_foco(r,24,1);
      ponteiro_alvo(r.x,r.y,r.w,r.h,point_focus,point_activate,2300+i,0);
      gv_registra(2300+i,r.x,r.y,r.w,r.h);
      gv_avatar(gv_ch[i].logo,gv_ch[i].nome,(GfxRect){r.x+72,r.y+24,80,80});
      gv_label(TXT_ILHA_NOME,gv_ch[i].nome,r.x+12,r.y+120,200,0);
      gv_label(TXT_ILHA_SUB,i18n("Futebol · 2 competições"),r.x+12,r.y+152,200,1);
    }
    gv_label(TXT_TITULO2,i18n("Agenda dos seus times"),GV_MX,660,600,0);
    int nm;GVMatch *m=gv_matches(&nm);
    for(int i=0;i<nm&&i<3;i++)gv_match_card(2220+i,&m[i],GV_MX+(i%3)*486,712);
  }else if(!strcmp(screen,"esportes-ajustes")){
    gv_dim(0,.42f);
    float x=gv_sheet_base(580,170,1050,760);
    gv_label(TXT_TITULO2,i18n("Ajustes de esportes"),x,210,700,0);
    gv_opt(2030,"aj_activity",i18n("Esportes neste perfil"),i18n("Ativado"),x,290,986,76);
    gv_opt(2031,"aj_eye",i18n("Ligas visíveis"),i18n("8 de 16"),x,374,986,76);
    gv_label(TXT_ILHA_SUB,i18n("Símbolos usados quando não há logo da equipe ou da liga"),x,480,800,1);
    const char *sis[]={"Futebol","Basquete","Tênis","Automobilismo","Vôlei","MMA"};
    for(int i=0;i<6;i++){
      float cx=x+(i%3)*328,cy=516+(i/3)*72;
      gfx_cor((GfxRect){cx,cy,312,60},16.0f/60,.114f,.118f,.137f,1);
      gfx_icone((GfxRect){cx+16,cy+18,24,24},"aj_play",.7f,.71f,.72f,1);
      gv_label(TXT_ILHA_CORPO,i18n(sis[i]),cx+52,cy+18,240,0);
    }
    gv_sports_bottom(660);
  }else if(!strcmp(screen,"esportes-onde-assistir")){
    gv_dim(0,.5f);
    float x=gv_sheet_base(432,184,1440,794);
    gv_label(TXT_ILHA_SUB,i18n("BRASILEIRÃO · AO VIVO · 67 MIN"),x,216,700,1);
    gv_label(TXT_TITULO2,i18n("Onde assistir"),x,248,700,0);
    {GVCh *fc=gv_focus();EpgProg *fp=gv_prog(fc);
    char sub2[180];char h1[8],h2[8];
    if(fp){gv_hora(fp->ini,h1,8);gv_hora(fp->fim,h2,8);snprintf(sub2,sizeof sub2,"%s · %s, %s–%s",fp->titulo,i18n("hoje"),h1,h2);}
    else snprintf(sub2,sizeof sub2,"%s",i18n("Ao vivo"));
    gv_label(TXT_ILHA_CORPO,sub2,x,316,900,1);}
    gv_button(2040,i18n("Fechar"),"aj_x",x+1240,220,160,0,1);
    for(int i=0;i<3&&i<gv_n;i++){
      GVCh *c=&gv_ch[i];char sub[160];
      snprintf(sub,sizeof sub,"%s · %d",c->grupo,c->num);
      gv_opt(2050+i,"aj_tv-minimal-play",c->nome,sub,x,380+i*136,780,122);
      gv_badge(gv_quali(c->nome),x+660,380+i*136+40);
    }
    GfxRect pv={x+830,380,578,324};
    GVCh *c=gv_focus();if(!c)c=&gv_ch[0];
    gv_broadcast(pv,c,gv_prog(c),0);
    gv_label(TXT_ILHA_NOME,i18n("Prévia do primeiro canal"),x+830,730,500,0);
    gv_label(TXT_ILHA_CORPO,i18n("O jogo continua sem som. Selecione um canal para assistir em tela cheia."),x+830,764,560,1);
    gv_label(TXT_ILHA_SUB,i18n("Disponibilidade conforme suas contas. Contas, categorias e canais ocultos não aparecem."),x,930,1300,1);
  }
}

/* --- dispatcher da tela principal ------------------------------------------ */
static int gv_cartoes(void){return !strcmp(gv_pagina,"cartoes");}
static void gv_footer(void){
  int ov=gv_overlay();
  if(!strcmp(screen,"busca")){const char *k[]={"OK","Voltar"},*l[]={"Assistir / abrir programa","Fechar busca"};gv_hints(k,l,2);}
  else if(!strcmp(screen,"numero")){const char *k[]={"0-9","OK","Voltar"},*l[]={"Digitar","Ir agora","Apagar / cancelar"};gv_hints(k,l,3);}
  else if(!strcmp(screen,"opcoes")||!strcmp(screen,"canal-opcoes")||!strcmp(screen,"esportes-onde-assistir")){const char *k[]={"OK","Voltar"},*l[]={"Escolher","Fechar"};gv_hints(k,l,2);}
  else if(!strcmp(screen,"fontes-erro")){const char *k[]={"OK","↓","Voltar"},*l[]={"Editar campo","Botões","Fechar"};gv_hints(k,l,3);}
  else if(ov){const char *k[]={"↑↓","←→","OK","Voltar"},*l[]={"Mover","Escolher","Selecionar","Guia"};gv_hints(k,l,4);}
  else if(!strcmp(screen,"vazio")||!gv_total_n){const char *k[]={"OK","←→","Voltar"},*l[]={"Escolher","Mover","Sair"};gv_hints(k,l,3);}
  else if(gv_sports()){const char *k[]={"↑↓←→","OK","Voltar"},*l[]={"Navegar","Abrir / onde assistir","Restaurar seleção"};gv_hints(k,l,3);}
  else if(gv_zone==1){const char *k[]={"←→","OK","↓","Segure OK"},*l[]={"Canal fixado","Assistir","Voltar à grade","Desafixar"};gv_hints(k,l,4);}
  else if(gv_zone==2){const char *k[]={"OK","↑","↓","Voltar"},*l[]={"Tela cheia","Canais fixados / prévia","Voltar à grade","Guia"};gv_hints(k,l,4);}
  else if(gv_zone==3){const char *k[]={"↑↓","OK","→"},*l[]={"Escolher categoria","Filtrar categoria","Voltar à grade"};gv_hints(k,l,3);}
  else if(gv_past()){const char *k[]={"OK","Segure OK","→","Voltar"},*l[]={"Assistir","Opções do canal","Próximos programas","Ao vivo"};gv_hints(k,l,4);}
  else if(gv_cartoes()){const char *k[]={"OK","Segure OK","0-9","←","CH ±","↑"},*l[]={"Assistir","Opções","Ir para número","Categorias","Pular seção","Canais fixados"};gv_hints(k,l,6);}
  else{const char *k[]={"OK","Segure OK","↑","CH ±","←"},*l[]={"Assistir","Opções do canal","Canais fixados / prévia","Trocar canal","Categorias"};gv_hints(k,l,5);}
}
static int gv_sem_veu;
static void gv_draw_sheet(int err){
  if(!strcmp(screen,"busca"))gv_busca_sheet();
  else if(!strcmp(screen,"numero"))gv_numero_sheet();
  else if(!strcmp(screen,"opcoes"))gv_opcoes_sheet();
  else if(!strcmp(screen,"canal-opcoes"))gv_canal_opcoes_sheet();
  else if(!strcmp(screen,"ajustes"))gv_ajustes_sheet();
  else if(!strcmp(screen,"pastas"))gv_pastas_sheet();
  else if(!strcmp(screen,"epg-saude"))gv_epg_saude_sheet();
  else if(!strncmp(screen,"epg-",4))gv_epg_sheet();
  else if(!strcmp(screen,"erros"))gv_erros_sheet();
  else if(err)gv_fontes_erro_modal();
}
static void gv_screen(void){
  static Uint32 ult;Uint32 agora=SDL_GetTicks();
  gv_dt=ult&&agora>ult?fminf((float)(agora-ult)/1000.0f,.05f):0;ult=agora;
  gv_snapshot();gv_janela();
  int err=!strcmp(screen,"fontes-erro");
  float base=err?256:GV_BASE,og=gfx_opacidade_grupo;
  gv_folha_t=t21_entrada(gv_folha_t,gv_dt,1);
  gv_pag_t=t21_entrada(gv_pag_t,gv_dt,1);
  /* Foco na lateral: heroi compacto e mais canais na grade (mock categorias). */
  gv_hh=anim_mola2(&gv_hh_v,gv_hh,gv_zone==3&&!gv_overlay()?208:GV_HERO_H,gv_dt,T21_SCROLL);
  gv_reg=0;
  gv_topbar();
  gv_chips_row();
  if(err)gv_strip();
  /* Troca de pagina: o conteudo entra subindo, nunca aparece de estalo. */
  float pa=anim_suave(gv_pag_t);
  gfx_opacidade_grupo=og*pa;
  if(pa<1)gfx_transformar(960,540,1,0,18*(1-pa));
  if(!gv_total_n){
    gv_reg=1;gv_vazio();gv_reg=0;
    gfx_sem_transformar();gfx_opacidade_grupo=og;
    gv_sidebar(base);
  }else{
    gv_sidebar(base);
    if(gv_sports()){
      gfx_sem_transformar();gfx_opacidade_grupo=og;
      gv_reg=1;gv_sports_screen();gv_reg=0;
    }else{
      if(gv_cartoes()){gv_hero_compact(base);gv_dock(base,208);gv_cards(base);}
      else{gv_hero(base);gv_dock(base,gv_hh);gv_grid(base);}
      gfx_sem_transformar();gfx_opacidade_grupo=og;
      if(gv_overlay()){
        /* Folha: veu da tela e a ilha crescendo de .96 com o fade (T21). */
        float t=anim_suave(gv_folha_t);
        gv_dim(err?248:0,err?.5f:.42f);
        gv_sem_veu=1;gfx_opacidade_grupo=og*t;
        if(t<1)gfx_transformar(960,540,anim_mistura(T21_SHEET_SCALE,1,t),0,0);
        gv_reg=1;gv_draw_sheet(err);gv_reg=0;
        gfx_sem_transformar();gfx_opacidade_grupo=og;gv_sem_veu=0;
      }
    }
  }
  if(gv_lista()&&num_focus){int ok=0;for(int i=0;i<num_focus;i++)if(next_focus[i]==focus)ok=1;if(!ok)focus=next_focus[0];}
  gv_footer();
}
/* --- eventos da tela principal -------------------------------------------- */
static void gv_go(const char *name){
  snprintf(previous,sizeof previous,"%s",screen);
  if(gv_overlay_nome(name)){
    if(gv_overlay())snprintf(gv_folha_ant,sizeof gv_folha_ant,"%s",screen);else gv_folha_ant[0]=0;
    gv_folha_t=0;
  }else{
    if(strcmp(gv_pagina,name))gv_pag_t=0;
    snprintf(gv_pagina,sizeof gv_pagina,"%s",name);gv_folha_ant[0]=0;
  }
  go(name);
  focus=-1;
  if(!strcmp(name,"dock"))gv_zone=1;
  if(!strcmp(name,"categorias"))gv_zone=3;
  if(!strcmp(name,"cartoes")&&gv_zone==0)gv_zone=6;
  if(!strcmp(name,"grade")&&gv_zone==6)gv_zone=0;
  if(!strncmp(name,"epg-",4)&&strcmp(name,"epg-saude"))gv_verif_estado=!strcmp(name,"epg-adicionado")?1:!strcmp(name,"epg-falha")?2:0;
}
/* Abertura vinda de fora (guia_abrir): a pagina entra e o foco volta a grade
 * no agora. Folhas abertas assim ficam sobre a pagina corrente. */
static void gv_abrir(const char *name){
  if(gv_overlay_nome(name)){gv_folha_t=0;gv_folha_ant[0]=0;return;}
  snprintf(gv_pagina,sizeof gv_pagina,"%s",name);
  gv_pag_t=0;gv_tdesl=0;gv_tfoc=0;gv_tmin=0;gv_folha_ant[0]=0;
  gv_zone=!strcmp(name,"cartoes")?6:0;
}
/* Volta um nivel: da folha para a folha de baixo ou para a pagina do guia. */
static void gv_voltar(void){
  if(gv_overlay()&&gv_folha_ant[0]){char a[24];snprintf(a,sizeof a,"%s",gv_folha_ant);gv_folha_ant[0]=0;go(a);gv_folha_t=0;return;}
  go(gv_pagina[0]?gv_pagina:"grade");gv_pag_t=gv_overlay_nome(previous)?1:0;
}
/* Novo filtro (chip ou lateral): a grade recomeca no primeiro canal. */
static void gv_filtrar(const char *grupo,const char *pagina){
  snprintf(gv_grupo,sizeof gv_grupo,"%s",grupo);
  gv_lin=gv_top=gv_off=0;gv_rol=0;gv_tdesl=0;gv_tfoc=0;gv_snap_force=1;gv_snapshot();
  gv_zone=gv_cartoes()&&!strcmp(pagina,"cartoes")?6:0;
  gv_go(pagina);
}
/* Programa futuro: OK marca/desmarca o lembrete (como no guia de sempre). */
static int gv_lembrete(GVCh *c,EpgProg *p){
  if(!c||!p||p->ini<=time(NULL))return 0;
  char msg[200];
  int r=lembrete_alternar(c->id,c->nome,p->titulo,c->base,p->ini,p->fim);
  if(r==1)snprintf(msg,sizeof msg,i18n("Lembrete marcado: %s"),p->titulo);
  else if(r==0)snprintf(msg,sizeof msg,"%s",i18n("Lembrete cancelado"));
  else snprintf(msg,sizeof msg,"%s",i18n("Não cabe mais lembrete"));
  glem_aviso_curto(msg);
  return 1;
}
static void gv_assistir(const char *id){if(id&&id[0]&&guia_pedir_canal_id(id))g5ui_close();}
static int gv_activate(void){
  GVCh *c;
  if(!strcmp(screen,"busca")){
    if(focus==1799){input(5,"Buscar",query,0);return 1;}
    if(focus>=1800&&focus<1804)gv_assistir(gv_bres[focus-1800]);
    else if(focus>=1810&&focus<1813)gv_assistir(gv_bres[4+focus-1810]);
    return 1;}
  if(!strcmp(screen,"numero")){const char *id=NULL;if(gv_numero_alvo(&id,NULL)>=0){gv_numero[0]=0;gv_assistir(id);}return 1;}
  if(!strcmp(screen,"opcoes")||!strcmp(screen,"canal-opcoes")){
    c=gv_canal_focado_ou_primeiro();
    if(!c){gv_voltar();return 1;}
    if(focus==1910||focus==1920){guia_alternar_favorito(c->id);gv_snap_force=1;gv_voltar();}
    else if(focus==1911||focus==1926){EpgProg *p=gv_prog(c);if(p&&p->ini>time(NULL))gv_lembrete(c,p);else{
        EpgProg nx;if(guia_programa_proximo(c->id,time(NULL),0,&nx)){GVCh k=*c;gv_lembrete(&k,&nx);}}gv_voltar();}
    else if(focus==1912)gv_go("canal-opcoes");
    return 1;}
  if(gv_sheet()||!strncmp(screen,"epg-",4)){
    if(!strcmp(screen,"ajustes")){
      if(focus==1940)guia_definir_preview(!guia_preview_ligado());
      else if(focus==1942)ajustes_definir_guia_previa_grande(1);
      else if(focus==1941)ajustes_definir_guia_previa_grande(0);
      else if(focus==1948)ajustes_definir_vidro(!ajustes_vidro());
      else if(focus==1949)gv_go("epg-saude");
    }else if(!strcmp(screen,"pastas")){
      /* sem pastas ainda: nada a abrir */
    }else if(!strncmp(screen,"epg-",4)&&strcmp(screen,"epg-saude")){
      if(focus==1960)gv_voltar();else if(focus==1961)gv_go("epg-verificando");
    }else if(!strcmp(screen,"epg-saude")){
      if(focus==1975)gv_go("epg-verificando");
    }
    return 1;}
  if(!strcmp(screen,"fontes-erro")){
    if(focus>=1990&&focus<1993)gv_go("epg-verificando");
    else gv_voltar();return 1;}
  if(!strcmp(screen,"vazio")||!gv_total_n){g5ui_open("setup-tipo");return 1;}
  if(gv_sports()){
    if(focus==2040){gv_voltar();return 1;}
    if(focus>=2050&&focus<2053){if(focus-2050<gv_n)gv_assistir(gv_ch[focus-2050].id);return 1;}
    if(focus==2002){gv_go("esportes-ajustes");return 1;}
    if(focus==2000||focus==2020){gv_go("esportes-ligas");return 1;}
    if(focus==2001){gv_go("esportes-favoritos");return 1;}
    if(focus>=2200&&focus<2230){gv_go("esportes-onde-assistir");return 1;}
    if(focus>=2100&&focus<2108){gv_go("esportes-liga");return 1;}
    if(focus>=2300&&focus<2304){gv_go("esportes-liga");return 1;}
    return 1;}
  if(gv_zone==5){
    int id=focus-1100;
    if(id==0)gv_go("grade");
    else if(id==1)gv_go("cartoes");
    else if(id==2)gv_go("esportes-ligas");
    else if(id==3)g5ui_open("vod-filmes");
    else if(id==4)gv_go("busca");
    else if(id==5)gv_go("ajustes");
    return 1;}
  if(gv_zone==4){
    switch(gv_chipi){
      case 0:gv_filtrar("","grade");break;
      case 1:gv_filtrar("Esportes","grade");break;
      case 2:g5ui_open("fontes-gerenciar");break;
      case 3:gv_tdesl=0;gv_tfoc=0;gv_snap_force=1;gv_go("catchup");gv_zone=0;break;
      case 4:g5ui_open("setup-tipo");break;
      case 5:gv_go("epg-saude");break;
    }return 1;}
  if(gv_zone==3){
    if(gv_sb>=6&&gv_sb<10&&gv_sb-6<gv_cats)gv_filtrar(gv_cat[gv_sb-6],gv_cartoes()?"cartoes":"grade");
    else if(gv_sb==2)gv_filtrar(GV_FAV,gv_cartoes()?"cartoes":"grade");
    else if(gv_sb==3)gv_filtrar("",gv_cartoes()?"cartoes":"grade");
    else if(gv_sb==4)gv_go("pastas");
    return 1;}
  if(gv_zone==2){c=gv_focus();
    if(gv_hbt==0){if(c)gv_assistir(c->id);}
    else gv_go(guia_preview_no_ar()?"preview-hero-cheio":"preview");
    return 1;}
  if(gv_zone==1){c=gv_canal_focado();if(c)gv_assistir(c->id);return 1;}
  if(gv_zone==6||gv_zone==0){c=gv_canal_focado();
    if(c){EpgProg *p=gv_prog(c);if(!gv_lembrete(c,p))gv_assistir(c->id);}
    return 1;}
  return 1;
}
/* OK segurado (NV_HOLD, segurar.h): opcoes do canal; no dock, desafixa. */
static Uint32 gv_ok_desde;static int gv_ok_longo;
static void gv_ok_segurado(void){
  if(gv_zone==1){GVCh *c=gv_canal_focado();if(c){guia_alternar_favorito(c->id);gv_snap_force=1;if(gv_pin>0&&gv_pin>=gv_npins-1)gv_pin--;}return;}
  if((gv_zone==0||gv_zone==6)&&gv_canal_focado())gv_go("opcoes");
}
static int gv_zona_ok(int z){
  if(z==1)return gv_npins>0;
  if(z==6)return gv_cartoes()&&gv_n>0;
  if(z==0)return !gv_cartoes()&&gv_match>0;
  return 1;
}
/* Proximo programa (dir 1) ou anterior (-1) do canal em foco; a grade anda em
 * passos de meia hora para o programa caber na janela. */
static int gv_tempo_passo(int dir){
  GVCh *c=gv_focus();EpgProg *p=gv_prog(c);
  if(!c||!c->np)return 0;
  time_t now=time(NULL),alvo=0;
  if(dir>0){
    time_t de=p?p->fim:gv_tempo();
    for(int i=0;i<c->np;i++)if(c->p[i].ini>=de){alvo=c->p[i].ini;break;}
    if(!alvo)return 0;
  }else{
    if(!p)return 0;
    for(int i=c->np-1;i>=0;i--)if(c->p[i].fim<=p->ini){alvo=c->p[i].ini;break;}
    if(!alvo)return 0;
    if(!gv_past()&&alvo<now&&p->ini<=now)return 0;   /* ao vivo: ESQUERDA vai a lateral */
  }
  gv_tfoc=alvo<=now&&now<alvo+60&&!gv_past()?0:alvo;
  if(!gv_past()&&alvo<=now){
    for(int i=0;i<c->np;i++)if(c->p[i].ini==alvo&&c->p[i].fim>now){gv_tfoc=0;break;}
  }
  time_t t0=gv_tbase();
  int min=(int)((gv_tempo()-t0)/60);
  while(min<gv_tdesl&&gv_tdesl>0)gv_tdesl-=30;
  while(min>=gv_tdesl+90)gv_tdesl+=30;
  if(gv_tdesl<0)gv_tdesl=0;
  if(gv_tdesl>24*60)gv_tdesl=24*60;
  gv_snap_force=1;
  return 1;
}
static void gv_mover_linha(int d){
  int n=gv_match;if(n<1)return;
  int l=gv_lin+d;if(l<0)l=0;if(l>n-1)l=n-1;
  gv_lin=l;gv_janela();
}
static int gv_event(SDL_Keycode k,int repeat){
  int ok=k==SDLK_RETURN||k==SDLK_KP_ENTER;
  if((k==SDLK_ESCAPE||k==SDLK_AC_BACK||k==SDLK_BACKSPACE)&&!repeat){
    gv_ok_desde=0;
    if(!strcmp(screen,"numero")&&gv_numero[0]){size_t n=strlen(gv_numero);gv_numero[n-1]=0;gv_numero_desde=SDL_GetTicks();if(n>1)return 1;}
    if(gv_overlay()){gv_numero[0]=0;gv_voltar();return 1;}
    if(!strcmp(screen,"esportes-liga")||!strcmp(screen,"esportes-favoritos")){gv_go("esportes-ligas");return 1;}
    if(gv_sports()||gv_past()){gv_go(gv_cartoes()?"cartoes":"grade");if(gv_zone!=6)gv_zone=0;return 1;}
    /* Foco fora da grade volta para ela antes de sair do guia. */
    int base=gv_cartoes()?6:0;
    if(gv_total_n&&gv_zone!=base&&gv_zona_ok(base)){gv_zone=base;return 1;}
    guia_fechar();g5ui_close();return 1;}
  if(gv_lista()){
    if(k==SDLK_UP||k==SDLK_DOWN||k==SDLK_LEFT||k==SDLK_RIGHT){nav_spatial(k);return 1;}
    if(ok&&!repeat)return gv_activate();
    if(!strcmp(screen,"vazio")||!gv_total_n||gv_sports())return 1;
    if(!(k>=SDLK_0&&k<=SDLK_9))return 1;
  }
  if(k>=SDLK_0&&k<=SDLK_9&&(!gv_overlay()||!strcmp(screen,"numero"))){
    size_t n=strlen(gv_numero);
    if(n<4){gv_numero[n]=(char)('0'+(k-SDLK_0));gv_numero[n+1]=0;gv_numero_desde=SDL_GetTicks();}
    if(strcmp(screen,"numero"))gv_go("numero");
    return 1;}
  if(!strcmp(screen,"numero")){if(ok&&!repeat)return gv_activate();return 1;}
  if(ok){
    if(!repeat){gv_ok_desde=SDL_GetTicks();if(!gv_ok_desde)gv_ok_desde=1;gv_ok_longo=0;}
    return 1;}
  int chp=k==SDLK_PAGEUP||k==SDLK_PAGEDOWN;
  if(chp&&(gv_zone==0||gv_zone==6)){
    int d=k==SDLK_PAGEUP?-1:1;
    if(gv_zone==6){gv_card=d<0?0:(gv_n<10?gv_n:10)-1;if(gv_card<0)gv_card=0;}
    else gv_mover_linha(d*gv_vis(GV_BASE));
    return 1;}
  /* navegacao por zonas: 5 abas, 4 chips, 2 heroi, 1 fixados, 0 grade / 6 cartoes, 3 lateral */
  int grid=gv_cartoes()?6:0;
  if(k==SDLK_UP){
    if(gv_zone==0&&gv_lin>0)gv_mover_linha(-1);
    else if(gv_zone==6&&gv_card>=5)gv_card-=5;
    else if(gv_zone==0||gv_zone==6)gv_zone=gv_zona_ok(1)?1:gv_cartoes()?4:2;
    else if(gv_zone==1)gv_zone=gv_cartoes()?4:2;
    else if(gv_zone==2)gv_zone=4;
    else if(gv_zone==4){gv_zone=5;focus=1100+(gv_cartoes()?1:gv_past()?0:0);}
    else if(gv_zone==3){int s2=gv_sb-1;while(s2>=0&&!gv_sb_tem(s2))s2--;if(s2>=0)gv_sb=s2;else{gv_zone=4;gv_chipi=0;}}
    return 1;}
  if(k==SDLK_DOWN){
    if(gv_zone==5)gv_zone=4;
    else if(gv_zone==4)gv_zone=gv_cartoes()?(gv_zona_ok(1)?1:grid):2;
    else if(gv_zone==2)gv_zone=gv_zona_ok(1)?1:grid;
    else if(gv_zone==1)gv_zone=grid;
    else if(gv_zone==0)gv_mover_linha(1);
    else if(gv_zone==6&&gv_card+5<gv_n&&gv_card+5<10)gv_card+=5;
    else if(gv_zone==3){int s2=gv_sb+1;while(s2<12&&!gv_sb_tem(s2))s2++;if(s2<12)gv_sb=s2;}
    if(gv_zone==1&&gv_pin>=gv_npins)gv_pin=gv_npins-1;
    return 1;}
  if(k==SDLK_LEFT){
    if(gv_zone==0){if(!gv_tempo_passo(-1)){gv_zone=3;if(!gv_sb_tem(gv_sb)||gv_sb>=10)gv_sb=3;}}
    else if(gv_zone==6){if(gv_card%5)gv_card--;else{gv_zone=3;if(!gv_sb_tem(gv_sb)||gv_sb>=10)gv_sb=3;}}
    else if(gv_zone==1&&gv_pin>0)gv_pin--;
    else if(gv_zone==2&&gv_hbt>0)gv_hbt=gv_hbt==2&&gv_past()?0:gv_hbt-1;
    else if(gv_zone==5&&focus>1100)focus--;
    else if(gv_zone==4){int c2=gv_chipi-1;while(c2>=0&&!gv_chip_tem(c2))c2--;if(c2>=0)gv_chipi=c2;}
    return 1;}
  if(k==SDLK_RIGHT){
    if(gv_zone==3)gv_zone=grid;
    else if(gv_zone==0)gv_tempo_passo(1);
    else if(gv_zone==6&&gv_card%5<4&&gv_card+1<gv_n&&gv_card+1<10)gv_card++;
    else if(gv_zone==1&&gv_pin<gv_npins-1)gv_pin++;
    else if(gv_zone==2&&gv_hbt<2)gv_hbt=gv_past()?2:gv_hbt+1;
    else if(gv_zone==5&&focus<1105)focus++;
    else if(gv_zone==4){int c2=gv_chipi+1;while(c2<6&&!gv_chip_tem(c2))c2++;if(c2<6)gv_chipi=c2;}
    return 1;}
  return 1;
}
static void gv_tecla_solta(SDL_Keycode k){
  if(k!=SDLK_RETURN&&k!=SDLK_KP_ENTER)return;
  if(gv_ok_desde&&!gv_ok_longo){
    if(gv_zone==3)focus=1300+gv_sb;
    if(gv_zone==4)focus=1200+gv_chipi;
    gv_ok_desde=0;gv_activate();return;}
  gv_ok_desde=0;gv_ok_longo=0;
}
static void gv_tick(void){
  if(gv_ok_desde&&!gv_ok_longo&&segurar_venceu(SDL_GetTicks(),gv_ok_desde,"guia")){gv_ok_longo=1;gv_ok_segurado();}
  if(gv_numero[0]&&gv_numero_desde&&SDL_GetTicks()-gv_numero_desde>2000){
    const char *id=NULL;
    gv_numero_desde=0;
    if(gv_numero_alvo(&id,NULL)>=0){gv_numero[0]=0;gv_assistir(id);}
    else{gv_numero[0]=0;if(!strcmp(screen,"numero"))gv_voltar();}
  }
}
