/* Approved Guide v5, native renderer. No network work in drawing. */
#include "guiav5ui.h"
#include "xtream.h"
#include "perfis.h"
#include "perfilinfantil.h"
#include "sessao.h"
#include "dados.h"
#include "catsnapdisk.h"
#include "rede.h"
#include "js.h"
#include "jsw.h"
#include "admin.h"
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
#include "badges.h"
#include "ponteiro.h"
#include "rolagemtoque.h"
#include "buscanorm.h"
#include "spotlight.h"
#include "tela21.h"
#include "focoprof.h"
#include "lembrete.h"
#include "guialembrete.h"
#include "segurar.h"
#include "ltv.h"
#include "esportesui.h"
#include "esportesnoticias.h"
#include "streams.h"
#include "fontescanais.h"
#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

/* Text scale (Ajustes > Escala de texto): the approved layout stays fixed and
 * every line of these screens is measured and drawn inside TXT_INI, like
 * t21_texto_linha. At 100% this is the plain path, bit for bit. */
static TxtLinha g5_esc(TxtLinha l,float k){l.w*=k;l.h*=k;return l;}
static TxtLinha g5_linha(TxtEstilo e,const char *s,int r,int g,int b,int a){float k=gfx_escala_txt();TXT_INI();TxtLinha l=txt_linha(e,s,r,g,b,a);TXT_FIM();return g5_esc(l,k);}
static TxtLinha g5_linha_bruta(TxtEstilo e,const char *s,int r,int g,int b,int a){float k=gfx_escala_txt();TXT_INI();TxtLinha l=txt_linha_bruta(e,s,r,g,b,a);TXT_FIM();return g5_esc(l,k);}
static TxtLinha g5_linha_corta(TxtEstilo e,const char *s,int r,int g,int b,int a,float m){float k=gfx_escala_txt();TXT_INI();TxtLinha l=txt_linha_corta(e,s,r,g,b,a,m/k);TXT_FIM();return g5_esc(l,k);}
static TxtLinha g5_linha_corta_bruta(TxtEstilo e,const char *s,int r,int g,int b,int a,float m){float k=gfx_escala_txt();TXT_INI();TxtLinha l=txt_linha_corta_bruta(e,s,r,g,b,a,m/k);TXT_FIM();return g5_esc(l,k);}
static void g5_desenhar(TxtLinha l,float x,float y){float k=gfx_escala_txt();l.w/=k;l.h/=k;TXT_INI();txt_desenhar(l,x/k,y/k);TXT_FIM();}
static float g5_bloco(TxtEstilo e,const char *s,int r,int g,int b,float x,float y,float w,float ld,float a,int n){float k=gfx_escala_txt();TXT_INI();float v=txt_bloco(e,s,r,g,b,x/k,y/k,w/k,ld,a,n);TXT_FIM();return v*k;}
static float g5_bloco_bruto(TxtEstilo e,const char *s,int r,int g,int b,float x,float y,float w,float ld,float a,int n){float k=gfx_escala_txt();TXT_INI();float v=txt_bloco_bruto(e,s,r,g,b,x/k,y/k,w/k,ld,a,n);TXT_FIM();return v*k;}
#define txt_linha g5_linha
#define txt_linha_bruta g5_linha_bruta
#define txt_linha_corta g5_linha_corta
#define txt_linha_corta_bruta g5_linha_corta_bruta
#define txt_desenhar g5_desenhar
#define txt_bloco g5_bloco
#define txt_bloco_bruto g5_bloco_bruto

#define G5_CAP 4096
#define VOD_ITEM 10000
/* Every retained item may belong to a different provider category. */
#define G5_GROUPS G5_CAP
static struct {
  char server[256], user[96], password[96], playlist[2048], xmltv[1100], file[3100], name[96];
  int type, live, movies, series, epg;
} draft, work_draft, sources_saved_draft;
static G5Item *inventory, *staging;
static int count, staging_count, staging_total, total, profile=-1, dirty;
static char account[96], screen[48], previous[48], query[140], message[160], catalog_group[64];
static int focus, nav=-1, selected, edit=-1, playing, selected_season=1, credential_field;
typedef struct{float x,y,w,h,vx,vy,vw,vh;int ok;}GVGlide;
static int mode=2, mv_count, mv_main, mv_target;
static GfxRect mv_rect(int slot);
/* UI slot indices include the main player; extra decoders are dense 0..2. */
static int mv_decoder(int slot){return slot-(slot>mv_main);}
static void mv_tick(void);
static void mv_picker_build(void);
static void mv_alts_build(void);
/* Per-slot source key, playback controller and display state (see guiav5.h). */
static char mv_fonte[4][24];
static G5MvCtl mvctl;
static int mv_disp[4], mv_extra_on[4], mv_main_req, mv_aberto;
static Uint32 mv_born[4];
static GVGlide mv_glide[4];   /* slot rects morph on the focus spring */
static float gv_side,gv_side_v;
static int gv_return_zone,gv_return_focus,gv_side_open;
static int vod_return_focus,vod_sidebar,vod_selected_slot=-1;
/* Keep compact references for the full provider catalog; decode only visible cards. */
typedef struct {const char *json,*end;int item,row;}VodRef;
typedef struct {char id[64],name[64];int kind,start,count;float scroll,velocity;}VodRow;
typedef struct {char *json[2],*categories[2];VodRef *refs;VodRow *rows;int count,nrows;}VodCatalog;
static VodCatalog vod_data,vod_pending;
static int *vod_order,vod_size,vod_rows,vod_hero;
static VodRow *vod_layout;
static float vod_scroll,vod_scroll_v,vod_focus[64];
static int vod_focus_ids[64],vod_focus_next;
static void vod_free(VodCatalog *v){for(int i=0;i<2;i++){free(v->json[i]);free(v->categories[i]);}free(v->refs);free(v->rows);memset(v,0,sizeof *v);}
static void vod_rebuild(void);
static unsigned vod_revision;
static int vod_lazy, vod_failed, vod_cache_checked, vod_begin_pending;
static char vod_cache_key[65], vod_work_key[65], vod_request_id[64];
static int vod_request_kind;
static long long vod_refresh_stamp, vod_work_stamp;
static unsigned vod_refresh_revision;
static int vod_snapshot_served, worker_snapshot_served;
static char (*vod_done[2])[64];
static int vod_done_count[2];
static void vod_begin(void);
static void vod_tick(void);
static void vod_load_worker(void);
static void vod_index(VodCatalog *v,int mixed);
static G5Item vod_info,vod_info_pending;
static struct {
  pthread_t thread;
  atomic_int busy,ready;
  char server[256],user[96],password[96],action[160],account[96];
  int profile,ref,ok;
  unsigned revision;
} metadata;
static int vod_info_ref=-1,vod_info_request=-1;
static Uint32 vod_info_since;
static unsigned vod_info_revision;
static float gv_edge_x,gv_edge_y;
static int gv_exit_armed;
static int gv_arrow(SDL_Keycode k){return k==SDLK_LEFT||k==SDLK_RIGHT||k==SDLK_UP||k==SDLK_DOWN;}
static void gv_sidebar(float base);
static void gv_snapshot(void);
static int gv_snap_force;
static void gv_side_enter(void);
static void gv_side_leave(void);
static G5Item mv_items[4], locked_item;
/* Picker rows: a header per configured Live TV source, then its channels
 * (favourites first), disabled when the source is at its connection limit. */
typedef struct{G5Item it;char fonte_nome[96],fonte_key[24];int cap,uso,fav,header,disabled;}MvPick;
static MvPick picks[192];
static int npicks;
static MvPick mv_alts[32];
static int mv_nalt;
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
// Published after each completed catalog request; drawing never reads staging.
static atomic_int phone_counts[3], phone_parts;
static atomic_int busy, ready;
static int operation, worker_profile, worker_ok, worker_categories_missing;
static int categories_missing;
static unsigned worker_generation, catalog_generation;
static atomic_int worker_cancel;
static char worker_account[96], action[160];
static XtreamConta tested;
static pthread_t worker;
static int source_loaded, test_ok, use_saved, committed;
static int save_failed;
static int sources_offset,sources_category_offset,sources_remove=-1;
static char sources_key[LTV_CHAVE],sources_edit[LTV_CHAVE];
static int sources_adding,sources_saved_use,sources_saved_count,sources_saved_total,sources_saved_loaded,sources_saved_groups;
static G5Item *sources_saved_inventory;
static char sources_saved_names[G5_GROUPS][64];
static unsigned char sources_saved_on[G5_GROUPS];
static void erase(void *p,size_t n);
static void sources_cancel(void){
  if(!sources_adding)return;
  worker_cancel=1;catalog_generation++;
  free(inventory);inventory=sources_saved_inventory;sources_saved_inventory=NULL;
  count=sources_saved_count;total=sources_saved_total;source_loaded=sources_saved_loaded;
  group_count=sources_saved_groups;memcpy(groups,sources_saved_names,sizeof groups);memcpy(group_on,sources_saved_on,sizeof group_on);
  draft=sources_saved_draft;use_saved=sources_saved_use;sources_adding=0;
  erase(&sources_saved_draft,sizeof sources_saved_draft);
}
static int sources_options;
static int catalog_fetch_pending;
static G5Item *series_catalog;
static int series_count, parent_series;
static char pin_value[96];
static int next_focus[1024], num_focus;
static const char *steps[]={"Tipo de fonte","Dados do provedor","Teste de conexão","Importar","Categorias","Guia de programação","Pronto"};
static const char *titles[]={"Adicione sua fonte ao Nuvio","Dados do provedor","Teste de conexão","Escolha o que importar","Categorias","Guia de programação","Tudo pronto"};
static const char *alphabet="abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789:/?&=%._-+@!#$,;~()[]{}* '";

static void erase(void *p,size_t n){volatile unsigned char *s=p;while(n--)*s++=0;}
static void filename(char *out,size_t n,const char *what){snprintf(out,n,"guiav5-%s-p%d.json",what,perfis_ativo());}
static void save_rules(void);
static void clear_catalog(void){
  worker_cancel=1;catalog_generation++;vod_begin_pending=0;
  /* Detached worker retains staging until ready; UI never waits for transport. */
  for(int k=0;k<2;k++){free(vod_done[k]);vod_done[k]=NULL;}
  free(inventory);free(series_catalog);inventory=series_catalog=NULL;count=total=series_count=0;source_loaded=categories_missing=0;vod_lazy=vod_failed=vod_cache_checked=0;memset(vod_done_count,0,sizeof vod_done_count);vod_selected_slot=-1;vod_revision++;vod_free(&vod_data);free(vod_order);free(vod_layout);vod_order=NULL;vod_layout=NULL;vod_size=vod_rows=0;}
static void gs_reset(void);
static void context(void){
  int p=perfis_ativo();const char *owner=sessao_usuario();if(!owner)owner="";
  if(profile==p&&!strcmp(account,owner))return;
  sources_cancel();sources_edit[0]=0;
  gs_reset();catalog_group[0]=0;
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
typedef struct {G5Playlist parser;CatsnapStream snapshot;int snapshot_ok;} PlaylistTransfer;
static int feed(const void *bytes,size_t n,void *user){
  PlaylistTransfer *t=user;
  if(t->snapshot_ok)t->snapshot_ok=catsnap_stream_write(&t->snapshot,bytes,n);
  return g5_playlist_feed(&t->parser,bytes,n);
}
static int cancelled(void *unused){(void)unused;return worker_cancel;}
static int load_playlist(G5Item *out,int cap){
  G5Playlist parser;g5_playlist_init(&parser,out,cap);
  if(work_draft.type==2){
    if(!g5_playlist_feed(&parser,work_draft.file,strlen(work_draft.file)))return -1;
    int n=g5_playlist_finish(&parser);staging_total=parser.total;return n;
  }
  if(!g5_http_url(work_draft.playlist))return -1;
  char identity[2500],key[65],file[160];
  snprintf(identity,sizeof identity,"%d\n%s\n%s",worker_profile,worker_account,work_draft.playlist);
  admin_sha256_hex((const unsigned char*)identity,strlen(identity),key);erase(identity,sizeof identity);
  snprintf(file,sizeof file,"guiav5-playlist-%s.txt",key);
  int fresh=0;char *body=catsnap_read(file,vod_work_stamp,&fresh);
  if(body&&fresh){
    int ok=g5_playlist_feed(&parser,body,strlen(body));free(body);
    int n=ok?g5_playlist_finish(&parser):-1;staging_total=parser.total;
    if(n>=0)return n;
    g5_playlist_init(&parser,out,cap);
  }else free(body);
  PlaylistTransfer transfer={0};g5_playlist_init(&transfer.parser,out,cap);
  transfer.snapshot_ok=catsnap_stream_begin(&transfer.snapshot,file,vod_work_stamp);
  RedePedido req={.url=work_draft.playlist,.prazo_ms=30000,.max_bytes=16*1024*1024,.seguir=1,.mesmo_host=1,
    .receber_corpo=feed,.corpo_usuario=&transfer,.parar=cancelled};
  RedeResposta res={0};int ok=rede_pedir(&req,&res)==REDE_OK&&res.status>=200&&res.status<300;
  rede_resposta_limpar(&res);staging_total=transfer.parser.total;
  int n=ok?g5_playlist_finish(&transfer.parser):-1;
  catsnap_stream_finish(&transfer.snapshot,n>=0&&!worker_cancel&&transfer.snapshot_ok);
  return n;
}
static void *load(void *unused){
  (void)unused;rede_parar_quando(cancelled,NULL);
  worker_ok=worker_categories_missing=0;staging_count=staging_total=0;
  if(operation!=6&&operation!=7&&operation!=8)vod_free(&vod_pending);
  if(operation==6||operation==7||operation==8){vod_load_worker();}
  else if(operation==3){pin_result=perfis_verificar_pin(worker_profile,pin_value);erase(pin_value,sizeof pin_value);worker_ok=pin_result==1;}
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
          if(operation==2&&k<2&&n>=0){vod_pending.json[k]=b;b=NULL;}
          free(b);if(n<0){worker_ok=0;break;}
          int category_status=0;char *categories=operation==4?NULL:xtream_consultar(work_draft.server,work_draft.user,work_draft.password,k==0?"get_vod_categories":k==1?"get_series_categories":"get_live_categories",15,&category_status);
          const char *root=categories?categories+strspn(categories," \t\r\n"):NULL;
          const char *end=root?js_fim(root):NULL;
          int categories_ok=category_status==200&&root&&*root=='['&&end&&end[-1]==']';
          if(operation!=4&&!categories_ok){
            worker_categories_missing=1;
            for(int i=staging_count;i<staging_count+n;i++)snprintf(staging[i].group,sizeof staging[i].group,"Uncategorized");
          }
          const char *cat=categories_ok?js_raiz_array(categories):NULL;
          while(cat){const char *end=js_fim(cat);if(!end)break;char id[64]="",name[64]="";js_texto(cat,end,"category_id",id,sizeof id);js_texto(cat,end,"category_name",name,sizeof name);
            if(name[0])for(int i=staging_count;i<staging_count+n;i++)if(!strcmp(staging[i].group,id)){snprintf(staging[i].group,sizeof staging[i].group,"%s",name);if(adult_group(name))staging[i].adult=1;}cat=js_prox(end);
          }
          if(operation==2&&k<2){vod_pending.categories[k]=categories;categories=NULL;}
          free(categories);
          staging_count+=n;staging_total+=t;
          if(work_phone){char st[160];int l=0,m=0,s=0;for(int i=0;i<staging_count;i++){if(staging[i].kind==G5_LIVE)l++;else if(staging[i].kind==G5_MOVIE)m++;else if(staging[i].kind==G5_SERIES)s++;}
            atomic_store(&phone_counts[G5_LIVE],l);atomic_store(&phone_counts[G5_MOVIE],m);atomic_store(&phone_counts[G5_SERIES],s);atomic_fetch_add(&phone_parts,1);
            snprintf(st,sizeof st,"{\"s\":\"fetch\",\"live\":%d,\"movies\":%d,\"series\":%d}",l,m,s);celular_status(st);}
          if(action[0]||staging_count>=G5_CAP)break;
        }
      }
    }
  }
  if(work_phone&&operation==2&&work_draft.type!=0&&worker_ok){
    int sizes[3]={0};for(int i=0;i<staging_count;i++)if(staging[i].kind<=G5_SERIES)sizes[staging[i].kind]++;
    for(int i=0;i<3;i++)atomic_store(&phone_counts[i],sizes[i]);atomic_store(&phone_parts,1);
  }
  rede_parar_quando(NULL,NULL);
  atomic_store_explicit(&ready,1,memory_order_release);return NULL;
}
static void *load_metadata(void *unused){
  (void)unused;int status=0;
  char *body=xtream_consultar_metadata(metadata.server,metadata.user,metadata.password,metadata.action,15,&status);
  erase(metadata.password,sizeof metadata.password);
  memset(&vod_info_pending,0,sizeof vod_info_pending);metadata.ok=body&&status==200;
  if(metadata.ok){const char *end=body+strlen(body);while(end>body&&isspace((unsigned char)end[-1]))end--;g5_parse_xtream_item(body,end,G5_MOVIE,&vod_info_pending);}free(body);
  atomic_store_explicit(&metadata.ready,1,memory_order_release);return NULL;
}
static int start(int op){
  if(op==5){
    if(atomic_load(&metadata.busy)||atomic_load(&busy)||!ltv_fonte_ligada("xt"))return 0;
    snprintf(metadata.server,sizeof metadata.server,"%s",draft.server);
    snprintf(metadata.user,sizeof metadata.user,"%s",draft.user);
    snprintf(metadata.password,sizeof metadata.password,"%s",draft.password);
    if(use_saved)xtream_dados_copiar(metadata.server,sizeof metadata.server,metadata.user,sizeof metadata.user,metadata.password,sizeof metadata.password);
    snprintf(metadata.action,sizeof metadata.action,"%s",action);
    snprintf(metadata.account,sizeof metadata.account,"%s",account);
    metadata.profile=profile;metadata.ref=vod_info_request;metadata.revision=vod_info_revision;
    atomic_store(&metadata.busy,1);
    if(pthread_create(&metadata.thread,NULL,load_metadata,NULL)){atomic_store(&metadata.busy,0);erase(metadata.password,sizeof metadata.password);return 0;}
    pthread_detach(metadata.thread);
    return 1;
  }
  if(atomic_load(&busy)||(!ltv_fonte_ligada(draft.type?"g5m3u":"xt")&&op!=3))return 0;
  work_draft=draft;work_phone=phone_stage==1||phone_stage==4;
  vod_work_stamp=catsnap_refresh_stamp(profile);
  if(op==2){vod_refresh_revision=guia_catalog_refresh_revision();vod_refresh_stamp=vod_work_stamp;}
  if(use_saved&&work_draft.type==0)xtream_dados_copiar(work_draft.server,sizeof work_draft.server,work_draft.user,sizeof work_draft.user,work_draft.password,sizeof work_draft.password);
  if(op==6||op==7||op==8){
    vod_work_stamp=vod_refresh_stamp;
    vod_free(&vod_pending);
    snprintf(vod_work_key,sizeof vod_work_key,"%s",vod_cache_key);
    for(int k=0;k<2;k++){
      vod_pending.json[k]=vod_data.json[k]?strdup(vod_data.json[k]):NULL;
      vod_pending.categories[k]=vod_data.categories[k]?strdup(vod_data.categories[k]):NULL;
      if((vod_data.json[k]&&!vod_pending.json[k])||(vod_data.categories[k]&&!vod_pending.categories[k])){vod_free(&vod_pending);vod_failed=1;return 0;}
    }
  }
  worker_generation=catalog_generation;
  operation=op;worker_profile=profile;snprintf(worker_account,sizeof worker_account,"%s",account);worker_cancel=0;
  atomic_store(&ready,0);atomic_store(&busy,1);
  if(pthread_create(&worker,NULL,load,NULL)){atomic_store(&busy,0);snprintf(message,sizeof message,"%s",i18n("Não foi possível conectar"));return 0;}
  pthread_detach(worker);
  return 1;
}
static void load_saved_credentials(void){
  if(draft.type==0)xtream_dados_copiar(draft.server,sizeof draft.server,draft.user,sizeof draft.user,draft.password,sizeof draft.password);
}
static void want_pin(int settings);
static int modal_tela(const char *s);
static int sem_foco;      /* page under a modal or a closing modal: no focus, no D-pad */
static int gv_zone;       /* guide focus zone (defined with the guide below) */
static int gv_is(const char *s);
/* One Back history for every v5 page and modal outside the guide: each open
 * remembers the screen under it and its focus; going to a screen already in
 * the history returns there instead of stacking it again. A modal closed
 * forward leaves no level behind. */
typedef struct{char nome[48];int foco;}PgNivel;
static PgNivel pg_pilha[16];static int pg_n;
static int pg_nivel(const char *s){return s[0]&&!gv_is(s);}
/* multiview-2/3/4 are one page: the layout is state, not a level */
static int pg_igual(const char *a,const char *b){
  return !strcmp(a,b)||(!strncmp(a,"multiview-",10)&&!strncmp(b,"multiview-",10)&&!modal_tela(a)&&!modal_tela(b));
}
static void go(const char *name){
  if(sources_adding&&strncmp(name,"setup-",6)&&strncmp(name,"parental-",9))sources_cancel();
  int volta=0,vfoco=0;
  if(!pg_nivel(screen)||!pg_nivel(name))pg_n=0;
  else if(!pg_igual(screen,name)){
    int fecha=modal_tela(screen)&&!modal_tela(name);
    if(fecha)while(pg_n&&modal_tela(pg_pilha[pg_n-1].nome))pg_n--;
    int i=pg_n;while(i>0&&!pg_igual(pg_pilha[i-1].nome,name))i--;
    if(i){pg_n=i-1;volta=1;vfoco=pg_pilha[pg_n].foco;}
    else if(!fecha&&pg_n<16){snprintf(pg_pilha[pg_n].nome,sizeof pg_pilha[0].nome,"%s",screen);pg_pilha[pg_n++].foco=focus;}
  }
  snprintf(screen,sizeof screen,"%s",name);focus=volta?vfoco:!strcmp(name,"multiview-escolher")?899:!strcmp(name,"multiview-limite")?810:modal_tela(name)?-1:!strncmp(name,"vod-",4)&&strcmp(name,"vod-titulo")&&strcmp(name,"vod-serie-temporadas")?VOD_ITEM:0;nav=-1;message[0]=0;
}
static void gv_voltar(void);
static void pg_voltar(void){if(pg_n)go(pg_pilha[pg_n-1].nome);else gv_voltar();}
/* Remote Back and every on-screen Back button: one level up, focus on the
 * opener; without history, the page's fixed parent. */
static void pg_tras(void){
  if(pg_n&&pg_nivel(screen))pg_voltar();
  else if(!strcmp(screen,"multiview-fonte"))go("multiview-slot");
  else if(!strncmp(screen,"multiview-",10)&&modal_tela(screen))go(mode==4?"multiview-4":mode==3?"multiview-3":"multiview-2");
  else if(!strcmp(screen,"parental-desbloqueio")||!strcmp(screen,"parental-lista"))go("parental-ajustes");
  else if(!strncmp(screen,"setup-",6)&&strcmp(screen,"setup-tipo"))go("setup-tipo");
  else gv_voltar();
}
/* The guide page under every v5 screen: leaving a sub-screen returns there
 * instead of exposing the legacy guide drawn when screen[] is empty. */
static char gv_pagina[48]="grade";
static void gv_tecla_solta(SDL_Keycode k);
static int gv_is(const char *s);
static int gv_overlay_nome(const char *s);
static void gv_abrir(const char *name);
static void gv_go(const char *name);
static void gv_busca_abrir(void);
static void gv_filtrar(const char *grupo,const char *pagina);
static void gv_assistir(const char *id);
static void gv_deixa(void);
static char gv_saida[48];   /* guide screen left for a v5 page; Back returns to it */
static void merge_mixed_vod(void){
  G5Item item;if(!guia_vod_item(0,&item))return;
  if(!inventory)inventory=calloc(G5_CAP,sizeof *inventory);
  for(int i=0;inventory&&count<G5_CAP&&guia_vod_item(i,&item);i++){
    if(!ltv_fonte_ligada(item.source))continue;
    int exists=0;for(int j=0;j<count;j++)if(!strcmp(inventory[j].source,item.source)&&!strcmp(inventory[j].id,item.id))exists=1;
    if(!exists){inventory[count++]=item;total++;}
  }
  build_groups();
}
void g5ui_open(const char *name){context();if(gv_is(screen)&&!gv_is(name))gv_deixa();go(name);edit=-1;
  if(!strcmp(name,"busca")){go(gv_pagina);gv_busca_abrir();return;}
  if(gv_is(name))gv_abrir(name);
  else{gv_side_open=0;vod_sidebar=0;}
  if(!strncmp(name,"setup-",6)&&strcmp(name,"setup-tipo")&&use_saved)load_saved_credentials();
  if(!strcmp(name,"parental-ajustes")){want_pin(1);return;}
  if(!strncmp(name,"multiview-",10)&&!modal_tela(name)){
    /* Fresh entry (not a layout switch, that is go()): one slot preloaded
     * with the channel on the air (the guide preview) or, failing that, the
     * first allowed channel. Never reopens a stream already playing — the
     * app-side request is id-guarded. */
    if(!mv_aberto){
      mv_main=0;mv_count=0;memset(mv_items,0,sizeof mv_items);memset(mv_fonte,0,sizeof mv_fonte);
      memset(&mvctl,0,sizeof mvctl);memset(mv_disp,0,sizeof mv_disp);memset(mv_extra_on,0,sizeof mv_extra_on);
      memset(mv_born,0,sizeof mv_born);mv_main_req=0;for(int i=0;i<4;i++)mv_glide[i].ok=0;
      video_mv_parar_todos();
      const char *noAr=player_id_canal();
      int picked=-1;
      for(int pass=0;pass<2&&picked<0;pass++)
        for(int i=0;i<4096;i++){
          const char *id,*title,*logo,*group,*base;
          if(!guia_canal_campos(i,&id,&title,&logo,&group,&base))continue;
          if(!g5ui_channel_allowed(id,title,group,base))continue;
          if(!pass&&(!noAr[0]||strcmp(id,noAr)))continue;   /* pass 0: the channel on the air */
          picked=i;break;
        }
      if(picked>=0){
        const char *id,*title,*logo,*group,*base;
        guia_canal_campos(picked,&id,&title,&logo,&group,&base);
        G5Item *it=&mv_items[0];memset(it,0,sizeof *it);it->kind=G5_LIVE;it->age=-1;
        snprintf(it->id,sizeof it->id,"%s",id);snprintf(it->name,sizeof it->name,"%s",title);snprintf(it->poster,sizeof it->poster,"%s",logo);snprintf(it->group,sizeof it->group,"%s",group);snprintf(it->source,sizeof it->source,"%s",base);
        const char *k=guia_canal_fonte(picked);snprintf(mv_fonte[0],sizeof mv_fonte[0],"%s",k?k:"");
        mv_count=1;mv_born[0]=SDL_GetTicks();
      }
    }
    mode=!strcmp(name,"multiview-4")?4:!strcmp(name,"multiview-3")?3:2;
  }
  if(!strcmp(name,"multiview-escolher")){mv_picker_build();query[0]=0;}
  if(!strncmp(name,"setup-",6)&&!draft.live&&!draft.movies&&!draft.series){draft.live=draft.movies=draft.series=draft.epg=1;}
  if((!strcmp(name,"setup-credenciais")||!strcmp(name,"fontes-gerenciar"))&&use_saved)load_saved_credentials();
  if(!strcmp(name,"fontes-gerenciar")){sources_key[0]=0;sources_remove=-1;if(!committed)draft.epg=draft.live=draft.movies=draft.series=1;}
  if(!strncmp(name,"vod-",4)&&!episodes_mode&&!atomic_load(&busy))merge_mixed_vod();
  if(!strncmp(name,"vod-",4)&&draft.type==0&&xtream_configurado()&&!phone_stage){vod_begin();return;}
  if(!strncmp(name,"vod-",4)&&(!source_loaded||categories_missing)&&!atomic_load(&busy)){
    if(draft.type==0 && xtream_configurado()){
      use_saved=1;action[0]=0;if(!committed)draft.movies=draft.series=1;start(2);
    }else if(draft.playlist[0]){action[0]=0;start(2);}
  }
}
void g5ui_remove_source(void){
  char file[64];filename(file,sizeof file,"source");dados_apagar(file);clear_catalog();erase(&draft,sizeof draft);committed=0;catalog_fetch_pending=0;
}
void g5ui_forget(void){
  g5ui_close();clear_catalog();erase(&lease,sizeof lease);erase(&draft,sizeof draft);committed=0;
  for(int p=1;p<=32;p++){char file[80];const char *parts[]={"source","rules"};for(int k=0;k<2;k++){snprintf(file,sizeof file,"guiav5-%s-p%d.json",parts[k],p);dados_apagar(file);}}
  profile=-1;
}
void g5ui_close(void){sources_cancel();gs_reset();celular_fechar();phone_stage=0;screen[0]=0;edit=-1;worker_cancel=1;erase(draft.password,sizeof draft.password);nav=-1;}
int g5ui_active(void){return screen[0]!=0;}
static void persist(void){
  extern void g5ui_save_source(const char *,const char *,int,int,int,int,int);
  g5ui_save_source(draft.playlist,draft.xmltv,draft.type,draft.live,draft.movies,draft.series,draft.epg);
  if(save_failed)return;
  if(draft.type==0){xtream_definir_servidor(draft.server);xtream_definir_usuario(draft.user);if(draft.password[0])xtream_definir_senha(draft.password);}
  if(sources_adding){free(sources_saved_inventory);sources_saved_inventory=NULL;sources_adding=0;erase(&sources_saved_draft,sizeof sources_saved_draft);}
  erase(draft.password,sizeof draft.password);use_saved=1;dirty=1;committed=1;
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
  phone_stage=0;message[0]=0;
  atomic_store(&phone_parts,0);for(int i=0;i<3;i++)atomic_store(&phone_counts[i],0);
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
static void drop_kind(int kind){
  int w=0;for(int i=0;i<count;i++)if(inventory[i].kind!=kind)inventory[w++]=inventory[i];count=w;
  if(kind==G5_MOVIE||kind==G5_SERIES){int k=kind==G5_SERIES;free(vod_data.json[k]);free(vod_data.categories[k]);vod_data.json[k]=vod_data.categories[k]=NULL;}
  vod_selected_slot=-1;vod_rebuild();
}
static int match_query(const char *name){char hay[256],needle[256];busca_normalizar(name,hay,sizeof hay);busca_normalizar(query,needle,sizeof needle);return !needle[0]||strstr(hay,needle);}
static int item_source_on(const G5Item *it){
  return ltv_fonte_ligada(!strcmp(it->source,"xtream")?"xt":!strcmp(it->source,"m3u")?"g5m3u":it->source);
}
static int visible_item(int i,int kind){
  G5Item *it=&inventory[i];return item_source_on(it)&&(kind<0||it->kind==kind||(kind==G5_MOVIE&&it->kind==G5_EPISODE&&!episodes_mode))&&(!catalog_group[0]||!strcmp(catalog_group,it->group))&&group_allowed(it->group)&&match_query(it->name)&&(!(rules.hide_locked||rules.kids)||!restricted(it));
}
static int indices[G5_CAP+1], nindices;
static int vod_item_focus(int id){return id>=VOD_ITEM&&id<VOD_ITEM+vod_size?id:VOD_ITEM;}
static void filter(int kind){nindices=0;for(int i=0;i<count;i++)if(visible_item(i,kind))indices[nindices++]=i;}
static void select_item(int i){if(i<0||i>=count)return;selected=i;locked_item=inventory[i];
  if(restricted(&inventory[i])){snprintf(previous,sizeof previous,"%s",screen);go("parental-canal-bloqueado");return;}
  go(inventory[i].kind==G5_SERIES?"vod-serie-temporadas":"vod-titulo");
}
static void want_pin(int settings){if(gv_is(screen))gv_deixa();pin_settings=settings;erase(pin_value,sizeof pin_value);if(strcmp(screen,"parental-canal-bloqueado")&&strcmp(screen,"parental-pin"))snprintf(previous,sizeof previous,"%s",screen);go("parental-pin");focus=20;}
/* Back from the episode list: the series page, focus on Load episodes. */
static void leave_episodes(void){free(inventory);inventory=series_catalog;series_catalog=NULL;count=series_count;selected=parent_series;episodes_mode=0;go("vod-serie-temporadas");focus=1;}
static int control_count;
static void activate(int id);
static void gv_point_focus(int id);
static void point_focus(int id,int unused){(void)unused;if(!strcmp(screen,"fontes-gerenciar")&&id!=focus)sources_key[0]=0;focus=id;nav=-1;if(gv_is(screen))gv_point_focus(id);}
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
static float modal_t=1,page_t=1,modal_sai_t;
static char modal_sai[48];   /* closing modal, drawn shrinking and fading out */
static void modal(float x,float y,float w,float h){
  float t=anim_suave(sem_foco?modal_sai_t:modal_t);
  gfx_cor((GfxRect){0,0,1920,1080},0,0,0,0,T21_VEIL*t);
  gfx_opacidade_grupo=t;
  if(t<1)gfx_transformar(960,540,anim_mistura(T21_SHEET_SCALE,1,t),0,0);
  t21_ilha_folha((GfxRect){x,y,w,h},T21_R_SHEET,1);
}
static int modal_tela(const char *s){
  return !strcmp(s,"parental-pin")||!strcmp(s,"parental-desbloqueio")||!strcmp(s,"multiview-escolher")||!strcmp(s,"multiview-limite")||!strcmp(s,"multiview-slot")||!strcmp(s,"multiview-fonte");
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
/* Focus glides between controls on the guide's spring. */
#define GV_MOLA_FOCO 22.0f
static float gv_dt;
static GfxRect gv_glide(GVGlide *g,GfxRect r){
  if(!g->ok||gv_dt<=0){*g=(GVGlide){r.x,r.y,r.w,r.h,0,0,0,0,1};return r;}
  g->x=anim_mola2(&g->vx,g->x,r.x,gv_dt,GV_MOLA_FOCO);g->y=anim_mola2(&g->vy,g->y,r.y,gv_dt,GV_MOLA_FOCO);
  g->w=anim_mola2(&g->vw,g->w,r.w,gv_dt,GV_MOLA_FOCO);g->h=anim_mola2(&g->vh,g->h,r.h,gv_dt,GV_MOLA_FOCO);
  return (GfxRect){g->x,g->y,g->w,g->h};
}
static GVGlide foco_glide;

static int foco_visto;   /* a focused control was drawn this frame */
static GfxRect foco_rect(GfxRect r){foco_visto=1;r.x+=gv_edge_x;r.y+=gv_edge_y;return gv_glide(&foco_glide,r);}
static int button_lit;   /* next button is the selected one of a segment */
static void button(int id,const char *text,const char *icon,float x,float y,float w,float h,int row){
  GfxRect r={x,y,w,h};int focused=nav<0&&focus==id&&!sem_foco,lit=button_lit&&!row;button_lit=0;
  float ar=1,ag=1,ab=1;if(lit)ajustes_acento(&ar,&ag,&ab);
  if(row){if(focused)plrui_linha_foco(foco_rect(r),20,1);}
  else{
    if(lit)gfx_cor(r,.5f,ar,ag,ab,.22f);
    else if(h<=54)gfx_cor(r,.5f,.141f,.149f,.173f,1);
    else plrui_botao_repouso(r,1);
    if(focused)plrui_pilula_foco(foco_rect(r),1);
  }
  int c=focused&&!row?ajustes_tinta_foco():240;float tint=c/255.0f;
  float ir=lit&&!focused?ar:tint,ig=lit&&!focused?ag:tint,ib=lit&&!focused?ab:tint;
  if(icon&&*icon)gfx_icone((GfxRect){x+24,y+(h-28)/2,28,28},icon,ir,ig,ib,1);
  TxtLinha l=txt_linha_corta_bruta(TXT_ILHA_CORPO,text,(int)(ir*255),(int)(ig*255),(int)(ib*255),255,w-(icon?84:48));
  txt_desenhar(l,icon||row?x+(icon?68:24):x+(w-l.w)/2,y+(h-l.h)/2);
  if(id>=control_count)control_count=id+1;
  if(sem_foco)return;
  ponteiro_alvo(x,y,w,h,point_focus,point_activate,id,0);
  reg_focus(id,x,y,w,h);
}
static void toggle(int id,const char *name,int on,float x,float y,float w){button(id,name,on?"aj_check":"aj_plus",x,y,w,64,1);}
static void chip(float x,float y,float w,const char *text,int dim){
  GfxRect r={x,y,w,36};gfx_cor(r,18.0f/36,.14f,.15f,.17f,.9f);
  label(TXT_ILHA_CORPO,text,x+16,y+8,w-28,dim);
}
static void gv_topbar(void);
/* The guide's own top bar on every v5 page; nav>=0 is its focus (tab index). */
static void top(void){
  gv_topbar();
  if(message[0])label(TXT_ILHA_SUB,message,500,982,1300,0);
}
static int cat_vis(int *vis){int n=0;for(int i=0;i<group_count;i++)if(match_query(groups[i]))vis[n++]=i;return n;}
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
      gfx_cor(r,.1,.135,.145,.165,1);if(focus==i&&nav<0)plrui_linha_foco(foco_rect(r),28,1);gfx_icone((GfxRect){x+28,y+30,38,38},icons[i],.95,.95,.95,1);
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
#ifndef NV_DESKTOP_UI /* desktop types in the field itself */
    const char *keys="ABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890.:/-@";
    for(int i=0;i<42;i++){char key[2]={i<41?keys[i]:'<',0};button(100+i,key,NULL,1218+(i%7)*88,549+(i/7)*56,80,48,0);}
#endif
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
    /* Pages of ten over the filtered list; the page follows the focus. */
    int vis[G5_GROUPS],nv=cat_vis(vis);static int start;
    for(int j=0;j<nv;j++)if(focus==2+vis[j])start=j/10*10;
    if(start>=nv)start=0;
    for(int j=start;j<nv&&j<start+10;j++){int i=vis[j];toggle(2+i,groups[i],group_on[i],474+((j-start)%2)*660,430+((j-start)/2)*76,630);}
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
// The guide can still hold the previous source while this setup is saved.
static float phone_epg_days(void){
  if(!committed||!draft.epg)return 0;
  time_t end=epg_fim_url(draft.xmltv),now=time(NULL);
  char provider[1100];
  if(draft.type==0&&xtream_url_xmltv(provider,sizeof provider)){
    time_t b=epg_fim_url(provider);if(b>end)end=b;
  }
  return end>now?(float)(end-now)/86400:0;
}
static int phone_step(void){
  return phone_stage==3?4:phone_stage==2?3:phone_stage==4?2:phone_stage==1?1:celular_conectado()?1:0;
}
static void phone_screen(void){
  int step=phone_step(),done=step==4,fetch=phone_stage==4;
  int expired=celular_estado()==CEL_EXPIROU||celular_estado()==CEL_FALHOU||celular_estado()==CEL_PARADO;
  float ar,ag,ab;ajustes_acento(&ar,&ag,&ab);
  panel(48,128,1824,848);
  label(TXT_V2_KICK,i18n("Configurar pelo celular"),102,166,1000,1);
  label(TXT_ILHA_TITULO,i18n(done?"Tudo pronto":expired?"Pareamento expirado":fetch?"Importando…":phone_stage==1?"Testando conexão…":step?"Conectado à TV":"Mais fácil pelo celular."),102,207,1540,0);
  label(TXT_ILHA_CORPO,i18n(done?"Fonte adicionada ao perfil":step>=3?"Conectado. Escolha o conteúdo e os grupos antes de importar.":step?"Use os dados enviados pelo seu provedor.":"Leia o QR e continue no celular."),102,272,1620,1);
  // Native vector scene: scan on the phone, transfer, then a populated TV.
  gfx_cor((GfxRect){110,324,82,120},.14f,.32f,.34f,.38f,1);
  gfx_cor((GfxRect){114,328,74,112},.12f,.09f,.10f,.12f,1);
  gfx_cor((GfxRect){137,331,28,5},.5f,.32f,.34f,.38f,1);
  for(int i=0;i<3;i++){
    float x=126+(i==1?30:0),y=346+(i==2?30:0);
    gfx_cor((GfxRect){x,y,15,15},0,.86f,.87f,.89f,1);
    gfx_cor((GfxRect){x+3,y+3,9,9},0,.09f,.10f,.12f,1);
    gfx_cor((GfxRect){x+6,y+6,3,3},0,.86f,.87f,.89f,1);
  }
  for(int i=0;i<7;i++)gfx_cor((GfxRect){147+(i%3)*6,365+(i/3)*7,4,4},0,.86f,.87f,.89f,1);
  if(!done&&!expired){
    float scan=ajustes_animacoes_reduzidas()?20:20+18*sinf(SDL_GetTicks()/350.0f);
    gfx_cor((GfxRect){124,347+scan,54,2},.5f,ar,ag,ab,.8f);
  }
  for(int i=0;i<2;i++){
    gfx_cor((GfxRect){124,399+i*12,8,7},.3f,ar,ag,ab,step?.7f:.2f);
    gfx_cor((GfxRect){138,400+i*12,38-i*10,4},.5f,.7f,.72f,.76f,.5f);
  }
  gfx_cor((GfxRect){137,432,28,3},.5f,.6f,.62f,.66f,.7f);
  gfx_cor((GfxRect){200,382,184,2},.5f,1,1,1,.12f);
  for(int i=0;i<3;i++){
    float a=.25f,p=(float)i/3;
    if(step&&!done&&!expired){
      a=ajustes_animacoes_reduzidas()?.65f:.35f+.5f*(.5f+.5f*sinf(SDL_GetTicks()/350.0f-i));
      if(!ajustes_animacoes_reduzidas())p=fmodf(SDL_GetTicks()/1400.0f+p,1);
    }
    float x=204+p*156;
    gfx_cor((GfxRect){x,373,22,18},.25f,ar,ag,ab,a);
    gfx_cor((GfxRect){x+5,379,12,2},.5f,.95f,.95f,.95f,a);
    gfx_cor((GfxRect){x+5,384,8,2},.5f,.95f,.95f,.95f,a);
  }
  gfx_cor((GfxRect){392,332,252,102},.1f,.32f,.34f,.38f,1);
  gfx_cor((GfxRect){396,336,244,94},.08f,.09f,.10f,.12f,1);
  gfx_cor((GfxRect){507,434,22,10},0,.32f,.34f,.38f,1);
  gfx_cor((GfxRect){476,444,84,4},.5f,.32f,.34f,.38f,1);
  if(done){
    gfx_cor((GfxRect){498,354,40,40},.5f,ar,ag,ab,.22f);
    gfx_icone((GfxRect){507,363,22,22},"aj_check",ar,ag,ab,1);
    gfx_cor((GfxRect){471,406,94,3},.5f,.72f,.74f,.77f,.7f);
  }else{
    gfx_cor((GfxRect){407,348,71,10},.4f,.72f,.74f,.77f,.5f);
    for(int i=0;i<3;i++){
      gfx_cor((GfxRect){407+i*74,366,64,38},.12f,.18f,.20f,.24f,1);
      gfx_icone((GfxRect){428+i*74,374,22,22},i==0?"aj_tv":i==1?"aj_film":"aj_clapperboard",.7f,.72f,.76f,step?.9f:.4f);
      gfx_cor((GfxRect){407+i*74,411,42,3},.5f,.72f,.74f,.77f,.4f);
    }
  }
  label(TXT_ILHA_SUB,i18n("Os dados ficam só na sua TV."),102,459,600,1);
  if(!done&&!expired&&celular_url()[0]){
    celb_qr_desenhar_lado(celular_url(),102,500,224,1);
    const char *token=strrchr(celular_url(),'/');char code[32];
    snprintf(code,sizeof code,"%.4s  %.4s",token?token+1:"",token?token+5:"");
    label(TXT_ILHA_SUB,i18n("Conexão"),382,486,270,1);
    label(TXT_ILHA_TITULO,code,382,530,350,0);
    label(TXT_ILHA_APOIO,i18n("Não compartilhe este código."),382,602,330,1);
    label(TXT_ILHA_SUB,celular_url(),102,744,630,0);
    label(TXT_ILHA_APOIO,i18n("Celular e TV na mesma rede Wi-Fi."),102,790,610,1);
  }else{
    gfx_icone((GfxRect){162,490,128,128},done?"aj_check":"aj_wifi-off",done?ar:.7f,done?ag:.7f,done?ab:.7f,1);
    label(TXT_ILHA_NOME,i18n(done?"Pronto para assistir":"Novo QR"),102,660,570,0);
    label(TXT_ILHA_CORPO,i18n(done?"Suas escolhas já estão na tela grande.":"Leia o QR e continue no celular."),102,710,570,1);
  }
  const char *names[]={"Conectado à TV","Dados do provedor","Importar","Categorias"};
  for(int i=0;i<4;i++){
    float y=342+i*68;int complete=done||i<step,active=!expired&&!done&&i==step;
    gfx_cor((GfxRect){788,y,38,38},.5f,complete||active?ar:1,complete||active?ag:1,complete||active?ab:1,complete?.22f:.07f);
    if(complete)gfx_icone((GfxRect){797,y+9,20,20},"aj_check",ar,ag,ab,1);
    else if(active)gfx_arco((GfxRect){797,y+9,20,20},2,ajustes_animacoes_reduzidas()?0:(SDL_GetTicks()%1600)/1600.0f,.7f,ar,ag,ab,1);
    label(TXT_ILHA_NOME,i18n(names[i]),848,y+1,620,!complete&&!active);
    label(TXT_ILHA_APOIO,i18n(complete?"Pronto":active?"Em andamento":"A seguir"),1530,y+8,270,1);
  }
  char progress[64];snprintf(progress,sizeof progress,i18n("%d de %d"),done?4:step+1,4);
  label(TXT_ILHA_SUB,progress,788,640,1000,1);
  if(fetch){
    int parts=atomic_load(&phone_parts),parts_total=draft.type==0?3:1;
    gfx_cor((GfxRect){788,622,1000,5},.5f,1,1,1,.08f);
    if(parts)gfx_cor((GfxRect){788,622,1000*fminf((float)parts/parts_total,1),5},.5f,ar,ag,ab,1);
  }
  int sizes[3]={0},ng=0;
  if(step>=3){
    unsigned char retained[G5_GROUPS]={0};
    for(int i=0;i<count;i++)if(inventory[i].kind<=G5_SERIES&&(!done||group_allowed(inventory[i].group))){
      sizes[inventory[i].kind]++;
      for(int g=0;g<group_count;g++)if(!strcmp(groups[g],inventory[i].group)){retained[g]=1;break;}
    }
    for(int i=0;i<group_count;i++)ng+=retained[i];
  }
  else if(fetch)for(int i=0;i<3;i++)sizes[i]=atomic_load(&phone_counts[i]);
  const char *stats[]={"Canais ao vivo","Filmes","Séries","Categorias"};
  for(int i=0;i<4;i++){
    float x=788+i*251;char n[24];snprintf(n,sizeof n,"%d",i<3?sizes[i]:ng);
    label(TXT_ILHA_TITULO,step>=3||fetch?n:"—",x,689,230,0);
    label(TXT_ILHA_SUB,i18n(stats[i]),x,744,230,1);
  }
  label(TXT_ILHA_SUB,i18n("Guia de programação"),788,800,340,1);
  float days=phone_epg_days();char coverage[96];
  if(days>0)snprintf(coverage,sizeof coverage,i18n("%.1f dias de programação"),days);
  else snprintf(coverage,sizeof coverage,"%s · %s",i18n("Sem programação"),i18n(done&&draft.epg?"Abrir o Guia de TV":"Não configurado"));
  label(TXT_ILHA_SUB,coverage,1150,800,630,1);
  if(done){button(0,i18n("Abrir o Guia de TV"),"aj_tv",102,880,420,60,0);button(1,i18n("Filmes e séries"),"aj_film",550,880,330,60,0);button(2,i18n("Gerenciar fontes"),"aj_settings-2",908,880,360,60,0);}
  else{button(4,i18n("Continuar na TV"),"pl_chevron-left",102,880,400,60,0);button(5,i18n("Novo QR"),"aj_rotate-cw",550,880,260,60,0);}
}
#include "guiasetup.inc"
/* Provider labels remain intact for rules; only presentation strips type prefixes. */
static const char *vod_category(const char *name){
  while(isspace((unsigned char)*name))name++;
  const char *colon=strchr(name,':');
  if(colon){char prefix[64];size_t n=(size_t)(colon-name);if(n<sizeof prefix){memcpy(prefix,name,n);prefix[n]=0;char normalized[64];busca_normalizar(prefix,normalized,sizeof normalized);
    if(strstr(normalized,"filme")||strstr(normalized,"serie")||strstr(normalized,"movie")){name=colon+1;while(isspace((unsigned char)*name))name++;}}}
  return *name?name:i18n("Outros");
}
static int vod_add_row(VodCatalog *v,int kind,const char *id,const char *name){
  for(int r=0;r<v->nrows;r++)if(v->rows[r].kind==kind&&!strcmp(v->rows[r].id,id))return r;
  VodRow *rows=realloc(v->rows,(v->nrows+1)*sizeof *rows);if(!rows)return -1;v->rows=rows;
  VodRow *row=&rows[v->nrows];memset(row,0,sizeof *row);row->kind=kind;
  snprintf(row->id,sizeof row->id,"%s",id);snprintf(row->name,sizeof row->name,"%s",name);
  return v->nrows++;
}
/* UI thread only: inventory references are merged after the worker is joined. */
static void vod_index_inventory(VodCatalog *v){
  VodRef *refs=realloc(v->refs,(v->count+count+1)*sizeof *refs);
  if(!refs)return;v->refs=refs;
  for(int i=0;i<count;i++){
    G5Item *it=&inventory[i];if(it->kind==G5_LIVE)continue;
    if(!strcmp(it->source,"xtream")&&((it->kind==G5_MOVIE&&v->json[0])||(it->kind==G5_SERIES&&v->json[1])))continue;
    int row=vod_add_row(v,it->kind,it->group,it->group[0]?it->group:i18n("Outros"));
    if(row>=0){v->refs[v->count++]=(VodRef){NULL,NULL,i,row};v->rows[row].count++;}
  }
}
static void vod_index(VodCatalog *v,int mixed){
  free(v->refs);free(v->rows);v->refs=NULL;v->rows=NULL;v->count=v->nrows=0;
  int capacity=0;for(int k=0;k<2;k++)if(v->json[k])for(const char *p=js_raiz_array(v->json[k]);p;){const char *end=js_fim(p);if(!end)break;capacity++;p=js_prox(end);}
  v->refs=calloc(capacity?capacity:1,sizeof *v->refs);if(!v->refs)return;
  for(int k=0;k<2;k++){
    for(const char *p=v->categories[k]?js_raiz_array(v->categories[k]):NULL;p;){const char *end=js_fim(p);if(!end)break;
      char id[64]="",name[64]="";js_texto(p,end,"category_id",id,sizeof id);if(!id[0])snprintf(id,sizeof id,"%.0f",js_num(p,end,"category_id",0));js_texto(p,end,"category_name",name,sizeof name);
      if(id[0]&&name[0])vod_add_row(v,k?G5_SERIES:G5_MOVIE,id,name);p=js_prox(end);
    }
    for(const char *p=v->json[k]?js_raiz_array(v->json[k]):NULL;p;){const char *end=js_fim(p);if(!end)break;
      char id[80]="",name[140]="",group[64]="";js_texto(p,end,"name",name,sizeof name);
      js_texto(p,end,k?"series_id":"stream_id",id,sizeof id);if(!id[0]&&js_num(p,end,k?"series_id":"stream_id",-1)>=0)snprintf(id,sizeof id,"%.0f",js_num(p,end,k?"series_id":"stream_id",-1));
      js_texto(p,end,"category_id",group,sizeof group);if(!group[0])snprintf(group,sizeof group,"%.0f",js_num(p,end,"category_id",0));
      int row=vod_add_row(v,k?G5_SERIES:G5_MOVIE,group,i18n("Outros"));
      if(row>=0&&id[0]&&name[0])v->refs[v->count++]=(VodRef){p,end,-1,row};p=js_prox(end);
    }
  }
  for(int i=0;i<v->count;i++)v->rows[v->refs[i].row].count++;
  if(mixed)vod_index_inventory(v);
}
static void vod_rebuild(void){
  vod_revision++;
  VodCatalog *v=&vod_data;if(!vod_lazy||!v->refs)vod_index(v,1);
  free(vod_order);free(vod_layout);vod_order=calloc(v->count+v->nrows+1,sizeof *vod_order);vod_layout=calloc(v->nrows?v->nrows:1,sizeof *vod_layout);
  vod_size=vod_rows=0;vod_hero=0;vod_scroll=vod_scroll_v=0;
}
static G5Item *vod_item(int ref){
  static G5Item decoded[24];static int keys[24],cursor;static unsigned generation;
  if(ref==-1){static G5Item placeholder;memset(&placeholder,0,sizeof placeholder);placeholder.kind=G5_MOVIE;placeholder.age=-1;snprintf(placeholder.name,sizeof placeholder.name,"%s",i18n("Carregando…"));return &placeholder;}
  if(ref<0||ref>=vod_data.count)return NULL;
  VodRef *r=&vod_data.refs[ref];if(r->item>=0)return r->item<count?&inventory[r->item]:NULL;
  /* Parse a few visible records, never expand the full catalog into G5Items. */
  if(generation!=vod_revision){for(int i=0;i<24;i++)keys[i]=-1;generation=vod_revision;}
  for(int i=0;i<24;i++)if(keys[i]==ref)return &decoded[i];
  int slot=cursor++%24;keys[slot]=ref;
  VodRow *row=&vod_data.rows[r->row];g5_parse_xtream_item(r->json,r->end,row->kind,&decoded[slot]);
  snprintf(decoded[slot].group,sizeof decoded[slot].group,"%s",row->name);decoded[slot].adult|=adult_group(row->name);
  return &decoded[slot];
}
static void vod_filter(void){
  static char last_query[140],last_group[64],last_screen[48];static int last_count=-1;
  static unsigned revision;static G5Rules last_rules;static G5Lease last_lease;static unsigned long long policy;
  if(!vod_data.refs||(!vod_data.json[0]&&!vod_data.json[1]&&last_count!=count)){vod_rebuild();last_count=count;}
  if(!vod_order||!vod_layout){vod_size=vod_rows=0;return;}
  unsigned long long current=0;for(int i=0;i<group_count;i++)current=current*33+group_on[i];
  current=current*33+ltv_fonte_ligada("xt");current=current*33+ltv_fonte_ligada("g5m3u");
  for(int i=0;i<count;i++)current=current*33+item_source_on(&inventory[i]);
  if(revision==vod_revision&&current==policy&&!strcmp(last_query,query)&&!strcmp(last_group,catalog_group)&&!strcmp(last_screen,screen)&&!memcmp(&last_rules,&rules,sizeof rules)&&!memcmp(&last_lease,&lease,sizeof lease))return;
  revision=vod_revision;policy=current;last_rules=rules;last_lease=lease;
  snprintf(last_query,sizeof last_query,"%s",query);snprintf(last_group,sizeof last_group,"%s",catalog_group);snprintf(last_screen,sizeof last_screen,"%s",screen);
  vod_size=vod_rows=0;
  for(int kind=G5_MOVIE;kind<=G5_EPISODE;kind++)for(int r=0;r<vod_data.nrows;r++){
    VodRow *row=&vod_data.rows[r];if(row->kind!=kind||!group_allowed(row->name)||(rules.hide_adult&&adult_group(row->name))||(catalog_group[0]&&strcmp(catalog_group,row->name)))continue;
    if(!strcmp(screen,"vod-series")&&kind!=G5_SERIES)continue;
    int start=vod_size;
    for(int i=0;i<vod_data.count;i++)if(vod_data.refs[i].row==r){
      if(vod_data.refs[i].json?!ltv_fonte_ligada("xt"):!item_source_on(&inventory[vod_data.refs[i].item]))continue;
      if(query[0]||rules.hide_locked||rules.kids){G5Item *it=vod_item(i);if(!it||!match_query(it->name)||((rules.hide_locked||rules.kids)&&restricted(it)))continue;}
      vod_order[vod_size++]=i;
    }
    int loaded=0,k=row->kind==G5_SERIES;
    for(int i=0;i<vod_done_count[k];i++)loaded|=!strcmp(vod_done[k][i],row->id);
    if(vod_lazy&&!loaded&&vod_size==start&&!query[0]&&ltv_fonte_ligada("xt"))vod_order[vod_size++]=-1;
    if(vod_size>start){VodRow *out=&vod_layout[vod_rows++];float scroll=out->scroll,velocity=out->velocity;*out=*row;out->start=start;out->count=vod_size-start;out->scroll=scroll;out->velocity=velocity;}
  }
}
static int vod_position(void){int id=focus>=VOD_ITEM?focus:vod_return_focus;int p=id-VOD_ITEM;return p>=0&&p<vod_size?p:0;}
static int vod_row_at(int position){for(int r=0;r<vod_rows;r++)if(position>=vod_layout[r].start&&position<vod_layout[r].start+vod_layout[r].count)return r;return 0;}
#include "guiav5_vodload.inc"
static void vod_select(int position){
  if(position<0||position>=vod_size||vod_order[position]<0)return;G5Item *it=vod_item(vod_order[position]);if(!it)return;
  if(vod_data.refs[vod_order[position]].item>=0){int i=vod_data.refs[vod_order[position]].item;select_item(i);}
  else{
    G5Item copy=*it;
    if(vod_selected_slot<0||vod_selected_slot>=count){G5Item *items=realloc(inventory,(count<G5_CAP?G5_CAP:count+1)*sizeof *items);if(!items)return;inventory=items;vod_selected_slot=count++;}
    inventory[vod_selected_slot]=copy;select_item(vod_selected_slot);
  }
}
static void vod_navigate(SDL_Keycode k){
  int p=vod_position(),r=vod_row_at(p),col=p-vod_layout[r].start;
  if(focus==810){if(k==SDLK_DOWN)focus=vod_item_focus(vod_return_focus);else if(k==SDLK_UP)nav=3;return;}
  if(focus<VOD_ITEM){if(k==SDLK_DOWN)focus=VOD_ITEM;else if(k==SDLK_UP)nav=3;return;}
  if(k==SDLK_UP&&r==0){vod_return_focus=focus;if(!strcmp(screen,"vod-busca"))focus=804;else nav=3;return;}
  if(k==SDLK_UP||k==SDLK_DOWN){int next=r+(k==SDLK_DOWN?1:-1);if(next>=0&&next<vod_rows)focus=VOD_ITEM+vod_layout[next].start+(col<vod_layout[next].count?col:vod_layout[next].count-1);}
  else if(k==SDLK_RIGHT&&col+1<vod_layout[r].count)focus++;
  else if(k==SDLK_LEFT&&col>0)focus--;
  vod_return_focus=focus;vod_hero=vod_position();
}
static float vod_chip(float x,float y,const char *icon,const char *text,int connected){
  TxtLinha line=txt_linha_corta_bruta(TXT_ILHA_SUB,text,216,218,222,255,620);float width=line.w+(icon?66:connected?56:32);
  gfx_cor((GfxRect){x,y,width,40},.5f,.141f,.149f,.173f,1);
  if(icon)gfx_icone((GfxRect){x+16,y+9,22,22},icon,.84,.85,.87,1);
  if(connected)gfx_cor((GfxRect){x+17,y+16,8,8},.5f,.36,.80,.52,1);
  txt_desenhar(line,x+(icon?48:connected?36:16),y+(40-line.h)/2);return width;
}
static const char *gv_quali(const char *nome);
/* Existing bottom margin only; metadata is confirmed, never inferred codecs. */
static float stream_footer_at(const char *name,const char *source,const char *format,int draw,float right){
  VideoInfo info={0};if(strncmp(screen,"vod-",4)&&guia_preview_no_ar())video_informacoes(&info);
  char text[280];snprintf(text,sizeof text,"%s%s%s%s%s%s%s",source&&*source?source:"",gv_quali(name)[0]?" · ":"",gv_quali(name),info.codec[0]?" · ":"",info.codec,format&&*format?" · ":"",format?format:"");
  TxtLinha line=txt_linha_corta_bruta(TXT_ILHA_APOIO,text,154,156,158,255,600);
  if(draw)txt_desenhar(line,right-line.w,1049);
  return line.w+24;
}
static float stream_footer(const char *name,const char *source,const char *format,int draw){return stream_footer_at(name,source,format,draw,1872);}
static void catalog(void){
  int search=!strcmp(screen,"vod-busca");
  vod_filter();nindices=vod_size;
  if(focus>=VOD_ITEM+vod_size)focus=VOD_ITEM;
  int position=vod_position(),active=vod_row_at(position);
  if(focus>=VOD_ITEM&&focus<VOD_ITEM+vod_size){vod_hero=position;vod_return_focus=focus;}
  if(vod_hero>=vod_size)vod_hero=0;
  int nm=0,ns=0;for(int r=0;r<vod_data.nrows;r++){
    VodRow *row=&vod_data.rows[r];if(!group_allowed(row->name)||(rules.hide_adult&&adult_group(row->name)))continue;
    if(row->kind==G5_SERIES)ns+=row->count;else nm+=row->count;
  }
  char pill[192];snprintf(pill,sizeof pill,"%s  ·  %s",draft.name[0]?draft.name:i18n("Do seu IPTV"),i18n(atomic_load(&busy)?"Atualizando…":source_loaded?"Conectado":"Não configurado"));
  if(vod_size){G5Item *item=vod_item(vod_order[vod_hero]);stream_footer(item->name,draft.name[0]?draft.name:i18n("Do seu IPTV"),item->ext,1);}
  else stream_footer("",pill,"",1);
  button(803,i18n("Gerenciar fontes"),"aj_settings-2",1566,106,306,52,0);
  float sidebar_alpha=gfx_opacidade_grupo;int saved_sem=sem_foco;
  if(!vod_sidebar)sem_foco=1;gfx_opacidade_grupo*=gv_side;
  gfx_transformar(0,0,1,-360*(1-gv_side),0);
  panel(48,190,320,800);label(TXT_ILHA_NOME,i18n("Catálogo do provedor"),80,217,270,0);
  button(800,i18n("Filmes"),"aj_film",68,258,280,54,1);
  button(801,i18n("Séries"),"aj_tv-minimal-play",68,312,280,54,1);
  button(802,i18n("Buscar"),"aj_search",68,366,280,54,1);
  gfx_cor((GfxRect){68,439,280,1},0,1,1,1,.10);label(TXT_ILHA_NOME,i18n("Categorias"),80,462,270,0);
  button(805,i18n("Todas as categorias"),"aj_folders",68,502,280,54,1);
  for(int g=0;g<group_count&&g<4;g++)button(820+g,vod_category(groups[g]),"aj_folders",68,563+g*62,280,54,1);
  label(TXT_ILHA_SUB,i18n("Conteúdo adulto oculto por padrão."),80,884,270,1);
  button(806,i18n("Adicionar fonte"),"aj_plus",68,915,280,54,1);
  gfx_sem_transformar();gfx_opacidade_grupo=sidebar_alpha;sem_foco=saved_sem;
  float left=64+352*gv_side,area=1856-left,cw=186,gap=34,stride=cw+gap;
  int loading=(atomic_load(&busy)&&operation!=5)||!strcmp(screen,"vod-carregando"),empty=!vod_size||!strcmp(screen,"vod-sem-catalogo")||!strcmp(screen,"vod-vazio");
  if(empty){
    if(loading){
      float pulse=.075f+.035f*(1+sinf(SDL_GetTicks()*.004f));
      panel(left-16,190,area+16,300);
      gfx_cor((GfxRect){left+20,232,area*.48f,34},.2f,1,1,1,pulse);
      gfx_cor((GfxRect){left+20,300,area*.70f,22},.2f,1,1,1,pulse);
      label(TXT_ILHA_SUB,i18n("Carregando catálogo…"),left+20,365,area-40,1);
      for(int c=0;c<(int)(area/stride);c++){
        gfx_cor((GfxRect){left+10+c*stride,560,cw,270},.07f,1,1,1,pulse);
        gfx_cor((GfxRect){left+10+c*stride,848,cw*.8f,20},.2f,1,1,1,pulse);
      }
      return;
    }
    const char *s=vod_failed?"Falha na conexão":loading?"Carregando catálogo…":query[0]||!strcmp(screen,"vod-vazio")?"Nenhum resultado":"Esta fonte não tem catálogo VOD";
    label(TXT_HEADLINE,i18n(s),left,420,area,0);label(TXT_ILHA_CORPO,i18n("Seu provedor define o conteúdo disponível."),left,490,area,1);
    button(vod_failed?810:0,i18n(vod_failed?"Tentar novamente":query[0]?"Limpar busca":"Adicionar fonte"),vod_failed?"aj_refresh-cw":"aj_plus",left,620,500,64,0);return;
  }
  if(vod_failed)button(810,i18n("Tentar novamente"),"aj_refresh-cw",left,158,280,40,0);
  if(search)button(804,query[0]?query:i18n("Buscar canais, filmes e séries"),"aj_search",left,190,area,64,1);
  else{
    G5Item hero_copy=*vod_item(vod_order[vod_hero]);
    if(vod_info_revision==vod_revision&&vod_info_ref==vod_order[vod_hero]){
      if(vod_info.backdrop[0])snprintf(hero_copy.backdrop,sizeof hero_copy.backdrop,"%s",vod_info.backdrop);
      if(vod_info.year[0])snprintf(hero_copy.year,sizeof hero_copy.year,"%s",vod_info.year);
      if(vod_info.rating[0])snprintf(hero_copy.rating,sizeof hero_copy.rating,"%s",vod_info.rating);
      if(vod_info.genre[0])snprintf(hero_copy.genre,sizeof hero_copy.genre,"%s",vod_info.genre);
      if(vod_info.plot[0])snprintf(hero_copy.plot,sizeof hero_copy.plot,"%s",vod_info.plot);
    }
    G5Item *hero=&hero_copy;int locked=restricted(hero);GfxRect r={left-16,178,area+16,310};
    panel(r.x,r.y,r.w,r.h);
    GLuint art=!locked&&hero->backdrop[0]?tex_obter_larg(hero->backdrop,1280):0;const char *art_path=hero->backdrop;
    if(!art&&!locked&&hero->poster[0]){art=tex_obter_larg(hero->poster,320);art_path=hero->poster;if(art){GLuint blur=gfx_desfocado(art,hero->poster);if(blur)art=blur;}}
    if(art){gfx_tex_aspect_atual=tex_aspecto(art_path);gfx_card_forcar_cover_atual=1;
      gfx_card_heroi_veu_atual[0]=.045f;gfx_card_heroi_veu_atual[1]=.047f;gfx_card_heroi_veu_atual[2]=.051f;gfx_card_heroi_veu_atual[3]=.92f;
      gfx_rect(r,art,GFX_CARD,0,0,0,.08,1,1,1,1);gfx_card_heroi_veu_atual[3]=0;gfx_card_forcar_cover_atual=0;gfx_tex_aspect_atual=0;}
    gfx_veu_css_base(r,0,1.6f,.85f,.88f,.10f);
    char category[160];snprintf(category,sizeof category,"%s  /  %s",i18n(hero->kind==G5_SERIES?"Séries":"Filmes"),locked?i18n("Conteúdo bloqueado"):vod_category(hero->group));
    label(TXT_ILHA_SUB,category,left+16,201,area-48,1);
    label(TXT_V2_TIT,locked?i18n("Conteúdo bloqueado"):hero->name,left+16,241,area-64,0);
    float mx=left+16;
    if(!locked){
      if(hero->year[0])mx+=vod_chip(mx,310,NULL,hero->year,0)+10;
      if(hero->rating[0]&&strcmp(hero->rating,"0")&&strcmp(hero->rating,"0.0"))mx+=vod_chip(mx,310,"aj_star",hero->rating,0)+10;
      if(hero->genre[0])mx+=vod_chip(mx,310,NULL,hero->genre,0)+10;
      if(hero->age>=0){snprintf(pill,sizeof pill,i18n("%d+"),hero->age);vod_chip(mx,310,NULL,pill,0);}
      if(hero->plot[0])label(TXT_ILHA_CORPO,hero->plot,left+16,363,area*.73f,1);
    }
  }
  float top=search?280:514,row_h=396;
  vod_scroll=anim_mola2(&vod_scroll_v,vod_scroll,active*row_h,gv_dt,T21_SCROLL);
  GfxRecorteAnt clip=gfx_recorte_dentro(left-16,top-8,area+32,1000-top);
  for(int r=0;r<vod_rows;r++){
    VodRow *row=&vod_layout[r];float y=top+r*row_h-vod_scroll;
    if(y+row_h<top-8||y>1000)continue;
    int row_focus=r==active&&nav<0&&focus>=VOD_ITEM&&!vod_sidebar;
    /* Category headings stay quiet; focused posters carry the 2.1 surface. */
    char heading[160];snprintf(heading,sizeof heading,"%s  /  %s",i18n(row->kind==G5_SERIES?"Séries":"Filmes"),vod_category(row->name));
    TxtLinha title=txt_linha_corta_bruta(TXT_HEADLINE,heading,row_focus?243:170,row_focus?242:173,row_focus?239:181,255,area-220);txt_desenhar(title,left,y);
    snprintf(pill,sizeof pill,i18n("%d títulos"),row->count);TxtLinha size=txt_linha_bruta(TXT_ILHA_SUB,pill,170,173,181,255);txt_desenhar(size,left+area-size.w,y+5);
    gfx_cor((GfxRect){left,y+43,area,1},0,1,1,1,.08f);
    int col=r==active?position-row->start:0,visible=(int)((area-20)/stride);
    float target=r==active?fmaxf(0,(col-visible+1)*stride):row->scroll;
    if(r==active)row->scroll=anim_mola2(&row->velocity,row->scroll,target,gv_dt,T21_SCROLL);
    int first=(int)(row->scroll/stride);if(first<0)first=0;
    for(int c=first;c<row->count;c++){
      float x=left+10+c*stride-row->scroll;if(x>left+area)break;
      int p=row->start+c,id=VOD_ITEM+p;
      if(vod_order[p]<0){
        float pulse=.075f+.035f*(1+sinf(SDL_GetTicks()*.004f));
        gfx_cor((GfxRect){x,y+66,cw,270},.07f,1,1,1,pulse);
        if(!vod_sidebar)reg_focus(id,x,y+58,cw,312);
        continue;
      }
      G5Item *it=vod_item(vod_order[p]);if(!it)continue;int locked=restricted(it),focused=focus==id&&nav<0&&!vod_sidebar&&!sem_foco;
      int slot=0;while(slot<64&&vod_focus_ids[slot]!=id)slot++;
      if(slot==64){slot=vod_focus_next++%64;vod_focus_ids[slot]=id;vod_focus[slot]=0;}
      float f=vod_focus[slot]=anim_mola(vod_focus[slot],focused?1:0,gv_dt,T21_FOCO);
      GfxRect base={x,y+66,cw,270},poster=base;
      float growth=(T21_POSTER_SCALE-1)*f;poster=(GfxRect){base.x-base.w*growth/2,base.y-base.h*growth/2,base.w*(1+growth),base.h*(1+growth)};
      poster.y-=T21_POSTER_LIFT*f;
      if(f>.001f)gfx_rect((GfxRect){poster.x-22,poster.y-10,poster.w+44,poster.h+42},0,GFX_SOMBRA,f,0,0,.12f,0,0,0,.55f);
      GLuint tex=!locked&&it->poster[0]?tex_obter_larg(it->poster,cw):0;
      if(tex){gfx_tex_aspect_atual=tex_aspecto(it->poster);gfx_rect(poster,tex,GFX_CARD,0,0,0,.07f,1,1,1,1);gfx_tex_aspect_atual=0;}
      else{gfx_cor(poster,.07f,.12f,.13f,.15f,1);gfx_icone((GfxRect){poster.x+poster.w/2-24,poster.y+poster.h/2-24,48,48},locked?"aj_lock":it->kind==G5_SERIES?"aj_tv-minimal-play":"aj_film",.6f,.62f,.66f,1);}
      if(focused){plrui_linha_foco((GfxRect){x-8,y+338,cw+16,34},14,1);foco_visto=1;}
      label(TXT_W20_24B,locked?i18n("Conteúdo bloqueado"):it->name,x,y+343,cw,!focused);
      ponteiro_alvo(x,y+58,cw,312,point_focus,point_activate,id,0);
      if(!vod_sidebar)reg_focus(id,x,y+58,cw,312);
    }
  }
  gfx_recorte_voltar(clip);
  /* Animated scrolling may leave the destination offscreen for a few frames;
   * retain its focus while the row catches up. */
  if(!vod_sidebar&&focus>=VOD_ITEM&&focus<VOD_ITEM+vod_size){int seen=0;for(int i=0;i<num_focus;i++)seen|=next_focus[i]==focus;if(!seen)reg_focus(focus,left,top, cw,270);}
  control_count=vod_size;
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
    static int begin;if(focus>=0&&focus<900)begin=focus/8*8;
    if(!rules_mode&&begin>=group_count)begin=0;
    if(rules_mode){for(int i=begin;i<begin+8;i++){const char *id,*name,*logo,*group,*base;if(!guia_canal_regra_campos(i,&id,&name,&logo,&group,&base))continue;
      int on=!strcmp(rules.blocked_channel,id);for(int j=0;j<rules.channel_count;j++)on|=!strcmp(rules.blocked_channels[j],id);
      toggle(i,name,on,142,270+(i-begin)*70,1620);
    }}else for(int i=begin;i<group_count&&i<begin+8;i++){int on=!strcmp(rules.blocked_group,groups[i]);for(int j=0;j<rules.group_count;j++)on|=!strcmp(rules.blocked_groups[j],groups[i]);toggle(i,groups[i],on,142,270+(i-begin)*70,1620);}
    button(900,i18n("Voltar"),"pl_chevron-left",142,884,280,60,0);return;
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
}
static void kids_screen(void){
  panel(48,128,1824,310);label(TXT_ILHA_SUB,i18n("Perfil infantil"),98,174,1600,1);label(TXT_TITULO2,i18n("Um mundo de favoritos."),98,232,1600,0);
  label(TXT_ILHA_CORPO,i18n("Somente canais e títulos permitidos."),98,324,1600,1);filter(-1);
  int n=0;for(int i=0;i<nindices&&n<6;i++){G5Item *it=&inventory[indices[i]];if(restricted(it))continue;button(n,it->name,"aj_tv-minimal-play",48+(n%3)*614,500+(n/3)*150,580,120,1);indices[n++]=indices[i];}
  button(800,i18n("Controle parental"),"aj_lock",48,880,500,64,0);
}
static GfxRect gv_preview_rect(void);
void g5ui_multiview_rect(float *x,float *y,float *w,float *h){
  if(gv_is(screen)){
    GfxRect r=gv_preview_rect();*x=r.x;*y=r.y;*w=r.w;*h=r.h;return;
  }
  GfxRect r=mv_rect(mv_main);*x=r.x;*y=r.y;*w=r.w;*h=r.h;
}
/* Slot area below the multiview control row; the slots are always 16:9 and
 * the leftover is letterbox (g5_mv_layout), never a squashed rectangle. */
#define MV_CTRL_Y (layout_desktop_ativo()?204.0f:156.0f)
#define MV_AREA_Y (MV_CTRL_Y+80.0f)
#define MV_AREA_H (1056.0f-MV_AREA_Y)
static GfxRect mv_rect(int i){
  G5MvRect rs[4];
  g5_mv_layout(mode,48,MV_AREA_Y,1824,MV_AREA_H,24,rs);
  return i>=0&&i<4?(GfxRect){rs[i].x,rs[i].y,rs[i].w,rs[i].h}:(GfxRect){0,0,0,0};
}
/* The slot's drawn rect: springs to the target on layout switch (morph). */
static GfxRect mv_rect_anim(int i){return gv_glide(&mv_glide[i],mv_rect(i));}
/* Account snapshot of one source key: "xt" the main Xtream account,
 * "xta<id>" an added one (its own max_connections, never another's). */
static int mv_conta(const char *key,XtreamConta *c){
  unsigned id=0;char sobra;
  memset(c,0,sizeof *c);
  if(key&&!strcmp(key,"xt"))return xtream_fonte_conta(0,c);
  if(key&&!strncmp(key,"xta",3)&&strlen(key)==11&&sscanf(key+3,"%8x%c",&id,&sobra)==1)return xtream_fonte_conta(id,c);
  return 0;
}
static int mv_cap(const char *key){
  XtreamConta c;int max=mv_conta(key,&c)?c.maxConexoes:-1;
  return g5_mv_source_cap(key,max);
}
static int mv_xtream_max(void){return mv_cap("xt");}
/* Slots of one source already filled (skip = the slot being replaced). */
static int mv_uso(const char *key,int skip){
  int u=0;if(!key||!key[0])return 0;
  for(int i=0;i<4;i++)if(i!=skip&&mv_items[i].id[0]&&!strcmp(mv_fonte[i],key))u++;
  return u;
}
static const char *mv_fonte_nome(const char *key,char *buf,size_t n){
  for(int i=0;;i++){GuiaFonteSaude s;if(!guia_fonte_saude(i,&s))break;
    if(s.chave&&key&&!strcmp(s.chave,key)){snprintf(buf,n,"%s",s.nome);return buf;}}
  if(key&&!strcmp(key,"xt"))snprintf(buf,n,"%s",draft.name[0]?draft.name:"Xtream");
  else if(key&&!strcmp(key,"g5m3u"))snprintf(buf,n,"%s",draft.name[0]?draft.name:"M3U");
  else snprintf(buf,n,"%s",key&&key[0]?key:"Live TV");
  return buf;
}
/* Direct URL for the extra decoders: Xtream live, a guide M3U channel (with
 * its User-Agent/Referer) or the list's own URL. Never logged.
 * Addon/Stalker channels need the resolver/session of the main player. */
static int mv_url(const G5Item *it,char *url,size_t n,char *cab,size_t nc){
  cab[0]=0;
  if(it->url[0]){snprintf(url,n,"%s",it->url);return 1;}
  {char ua[256],ref[256];
   if(guia_m3u_canal(it->id,url,n,ua,sizeof ua,ref,sizeof ref)){
     size_t u=0;
     if(ua[0])u+=(size_t)snprintf(cab,nc,"User-Agent: %s\n",ua);
     if(ref[0]&&u<nc)snprintf(cab+u,nc-u,"Referer: %s\n",ref);
     return 1;
   }}
  if(!strcmp(it->source,"xtream")||!strncmp(it->id,"xtream:",7)){
    char id[96];
    if(!strncmp(it->id,"xtream:",7))snprintf(id,sizeof id,"%s",it->id);
    else snprintf(id,sizeof id,"xtream:%s",it->id);
    return xtream_url(id,url,(unsigned)n);
  }
  return 0;
}
static void mv_label_c(TxtEstilo st,const char *s,float cx,float y,float maxw){
  TxtLinha l=txt_linha_corta_bruta(st,s,243,242,239,255,maxw);txt_desenhar(l,cx-(float)l.w/2,y);
}
/* Mini diagram of a layout for the switch chips: tiny 16:9 blocks. */
static void mv_diagrama(int m,float x,float y,float w,float h,float alfa){
  G5MvRect rs[4];g5_mv_layout(m,x,y,w,h,2.0f,rs);
  for(int i=0;i<m;i++)gfx_cor((GfxRect){rs[i].x,rs[i].y,rs[i].w,rs[i].h},.3f,1,1,1,alfa);
}
/* Control row: kicker, segmented layout switch with mini diagrams, then the
 * global actions. No footer row — the slots own the vertical space. */
static void mv_topo(void){
  label(TXT_ILHA_SUB,"MULTIVIEW",48,MV_CTRL_Y+18,160,1);
  float y=MV_CTRL_Y,h=56,x=228,w=3*108+8;
  if(ajustes_vidro())gfx_cor((GfxRect){x,y,w,h},.5f,1,1,1,T21_SEG_TRACK);
  else gfx_cor((GfxRect){x,y,w,h},.5f,.114f,.118f,.137f,1);
  for(int i=0;i<3;i++){
    int m=i==0?2:i==1?3:4,id=800+i;
    GfxRect r={x+4+i*108,y+4,100,h-8};
    int on=mode==m,f=nav<0&&focus==id&&!sem_foco;
    if(f)plrui_pilula_foco(foco_rect(r),1);
    else if(on){if(ajustes_vidro())gfx_cor(r,.5f,1,1,1,T21_SEG_SELECTED);else gfx_cor(r,.5f,.204f,.212f,.243f,1);}
    float c=f?ajustes_tinta_foco()/255.0f:on?.95f:.55f;
    mv_diagrama(m,r.x+10,r.y+(r.h-24)/2,34,24,c*.9f);
    TxtLinha l=txt_linha_bruta(TXT_ILHA_CORPO,i==0?"2":i==1?"1 + 2":"2 × 2",(int)(c*255),(int)(c*255),(int)(c*255),255);
    txt_desenhar(l,r.x+52,r.y+(r.h-(float)l.h)/2);
    if(!sem_foco){ponteiro_alvo(r.x,r.y,r.w,r.h,point_focus,point_activate,id,0);reg_focus(id,r.x,r.y,r.w,r.h);}
  }
  {const char *l1=i18n("Limites desta TV"),*l2=i18n("Voltar ao guia");
   float w2=(float)txt_linha(TXT_ILHA_CORPO,l2,243,242,239,255).w+92;
   float w1=(float)txt_linha(TXT_ILHA_CORPO,l1,243,242,239,255).w+92;
   button(806,l2,"aj_log-out",1872-w2,MV_CTRL_Y,w2,56,0);
   button(805,l1,"aj_info",1872-w2-w1-12,MV_CTRL_Y,w1,56,0);}
}
/* The overlay of a filled slot: glass chip with channel + source, and the
 * audio badge (accent pill) on the main slot. Never pauses the video. */
static void mv_slot_overlay(int i,G5Item *it,GfxRect r,int live){
  char fonte[96];mv_fonte_nome(mv_fonte[i],fonte,sizeof fonte);
  TxtLinha ln=txt_linha_corta_bruta(TXT_ILHA_NOME,restricted(it)?i18n("Conteúdo bloqueado"):it->name,243,242,239,255,r.w-160);
  float cw=(float)ln.w+48,chh=74;
  GfxRect cr={r.x+20,r.y+r.h-chh-16,cw,chh};
  plrui_material_video(cr,22,1);
  txt_desenhar(ln,cr.x+24,cr.y+12);
  label(TXT_ILHA_SUB,fonte,cr.x+24,cr.y+46,cw-40,1);
  if(i==mv_main){
    float ar,ag,ab;ajustes_acento(&ar,&ag,&ab);
    TxtLinha la=txt_linha_bruta(TXT_ILHA_CORPO,i18n("Áudio"),(int)(ar*255),(int)(ag*255),(int)(ab*255),255);
    GfxRect b={r.x+r.w-(float)la.w-96,r.y+18,(float)la.w+78,44};
    gfx_cor(b,.5f,ar,ag,ab,.22f);
    gfx_icone((GfxRect){b.x+16,b.y+10,24,24},"aj_volume-2",ar,ag,ab,1);
    txt_desenhar(la,b.x+50,b.y+(44-(float)la.h)/2);
  }
  (void)live;
}
static void mv_slot(int i){
  GfxRect r=mv_rect_anim(i),alvo=mv_rect(i);
  G5Item *it=mv_items[i].id[0]?&mv_items[i]:NULL;
  int focused=nav<0&&focus==i&&!sem_foco;
  float t=1;
  if(it&&mv_born[i]){float e=(float)(SDL_GetTicks()-mv_born[i])/260.0f;t=anim_suave(e>1?1:e);}
  float og=gfx_opacidade_grupo;
  if(t<1){gfx_transformar(r.x+r.w/2,r.y+r.h/2,anim_mistura(.94f,1,t),0,0);gfx_opacidade_grupo=og*t;}
  int principal=i==mv_main;
  int live=it&&principal&&player_mini_no_guia_ativo()&&video_pronto();
  int extra_live=it&&!principal&&mv_extra_on[i]&&video_mv_estado(mv_decoder(i))==2;
  panel(r.x,r.y,r.w,r.h);
  if(live||extra_live)gfx_furo_raio(r,22/r.h);
  else if(it&&it->poster[0]&&!restricted(it)){GLuint tx=tex_obter_larg(it->poster,r.w);if(tx)gfx_rect(r,tx,GFX_CARD,0,0,0,22/r.h,1,1,1,1);}
  if(!it){
    /* Empty slot: an inviting Add-channel tile, not a grey box. */
    float cx=r.x+r.w/2,cy=r.y+r.h/2;
    gfx_cor((GfxRect){cx-40,cy-78,80,80},.5f,1,1,1,.08f);
    gfx_icone((GfxRect){cx-19,cy-57,38,38},"aj_plus",.95f,.95f,.95f,.9f);
    mv_label_c(TXT_ILHA_NOME,i18n("Adicionar canal"),cx,cy+18,400);
  }else{
    int disp=principal?(video_falhou()?G5_MV_ERRO:live||mvctl.slot[i].estado==G5_MV_TOCANDO?G5_MV_TOCANDO:G5_MV_ABRINDO):mv_disp[i];
    if(disp==G5_MV_SEM_FONTE||disp==G5_MV_SEM_DECODER||disp==G5_MV_ERRO){
      float cx=r.x+r.w/2,cy=r.y+r.h/2;
      const char *icon=disp==G5_MV_ERRO?"aj_refresh-cw":disp==G5_MV_SEM_FONTE?"aj_lock":"aj_tv-minimal-play";
      char reason[160];
      if(disp==G5_MV_SEM_FONTE){int cap=mv_cap(mv_fonte[i]);
        if(cap>0)snprintf(reason,sizeof reason,i18n("Esta fonte permite %d tela(s)"),cap);
        else snprintf(reason,sizeof reason,"%s",i18n("Só toca no quadro principal"));}
      else if(disp==G5_MV_SEM_DECODER)snprintf(reason,sizeof reason,i18n("Esta TV decodifica %d canal(is) por vez"),video_mv_max()+1);
      else snprintf(reason,sizeof reason,"%s",i18n(mvctl.slot[i].tentativas<4?"Erro no canal — tentando de novo":"Erro no canal"));
      gfx_icone((GfxRect){cx-19,cy-58,38,38},icon,.75f,.75f,.78f,1);
      mv_label_c(TXT_ILHA_CORPO,reason,cx,cy+6,r.w-160);
    }else if(disp==G5_MV_ABRINDO){
      mv_label_c(TXT_ILHA_SUB,i18n("Abrindo…"),r.x+r.w/2,r.y+r.h-60,300);
    }
    mv_slot_overlay(i,it,r,live||extra_live);
  }
  if(live&&!player_janela_animando(NULL,NULL,NULL,NULL))player_mini_no_guia(alvo.x,alvo.y,alvo.w,alvo.h);
  if(focused){
    float ar,ag,ab;ajustes_acento(&ar,&ag,&ab);
    GfxRect fr=foco_rect(alvo);
    gfx_anel_fora(fr,22/fr.h,4,2.5f,ar,ag,ab,.9f);
  }
  if(!sem_foco){ponteiro_alvo(alvo.x,alvo.y,alvo.w,alvo.h,point_focus,point_activate,i,0);
    reg_focus(i,alvo.x,alvo.y,alvo.w,alvo.h);}
  if(t<1){gfx_sem_transformar();gfx_opacidade_grupo=og;}
}
static void multiview_screen(void){
  mv_topo();
  for(int i=0;i<mode;i++)mv_slot(i);
}
/* Channel name without case and quality tags, so "Globo HD" and "globo"
 * from two sources are the same channel for Change-source. */
static void mv_norm(const char *s,char *d,size_t n){
  size_t i=0;for(;s[i]&&i+1<n;i++)d[i]=(char)tolower((unsigned char)s[i]);d[i]=0;
  static const char *tags[]={" full hd"," fhd"," uhd"," 4k"," hd"," sd"," h265"," hevc",0};
  for(;;){
    size_t L=strlen(d);while(L&&d[L-1]==' ')d[--L]=0;
    if(!L)break;int cut=0;
    for(int k=0;tags[k];k++){size_t m=strlen(tags[k]);if(L>m&&!memcmp(d+L-m,tags[k],m)){d[L-m]=0;cut=1;break;}}
    if(!cut)break;
  }
}
static void mv_pick_fill(MvPick *p,int guia_idx,const G5Item *inv){
  memset(p,0,sizeof *p);
  if(guia_idx>=0){
    const char *id,*title,*logo,*group,*base;
    guia_canal_campos(guia_idx,&id,&title,&logo,&group,&base);
    p->it.kind=G5_LIVE;p->it.age=-1;
    snprintf(p->it.id,sizeof p->it.id,"%s",id);snprintf(p->it.name,sizeof p->it.name,"%s",title);
    snprintf(p->it.poster,sizeof p->it.poster,"%s",logo);snprintf(p->it.group,sizeof p->it.group,"%s",group);
    snprintf(p->it.source,sizeof p->it.source,"%s",base);
    const char *k=guia_canal_fonte(guia_idx);snprintf(p->fonte_key,sizeof p->fonte_key,"%s",k?k:"");
    p->fav=guia_e_favorito(id);
  }else if(inv){
    p->it=*inv;
    snprintf(p->fonte_key,sizeof p->fonte_key,"%s",!strcmp(inv->source,"xtream")?"xt":"g5m3u");
  }
  mv_fonte_nome(p->fonte_key,p->fonte_nome,sizeof p->fonte_nome);
  p->cap=mv_cap(p->fonte_key);
  p->uso=mv_uso(p->fonte_key,mv_target);
  p->disabled=p->cap>0&&p->uso>=p->cap;
}
/* Picker rows: one section per configured Live TV source (order of the
 * guide's sources), favourites first inside each, query-filtered. */
static void mv_picker_build(void){
  npicks=0;
  static MvPick raw[192];int nr=0;
  for(int i=0;i<4096&&nr<160;i++){
    const char *id,*title,*logo,*group,*base;
    if(!guia_canal_campos(i,&id,&title,&logo,&group,&base))continue;
    if(!g5ui_channel_allowed(id,title,group,base)||!match_query(title))continue;
    mv_pick_fill(&raw[nr++],i,NULL);
  }
  for(int i=0;i<count&&nr<192;i++)
    if(inventory[i].kind==G5_LIVE&&!restricted(&inventory[i])&&group_allowed(inventory[i].group)&&match_query(inventory[i].name))
      mv_pick_fill(&raw[nr++],-1,&inventory[i]);
  unsigned char done[192]={0};
  for(;;){
    int first=-1;for(int i=0;i<nr;i++)if(!done[i]){first=i;break;}
    if(first<0||npicks>=190)break;
    MvPick *h=&picks[npicks++];memset(h,0,sizeof *h);h->header=1;
    snprintf(h->fonte_key,sizeof h->fonte_key,"%s",raw[first].fonte_key);
    snprintf(h->fonte_nome,sizeof h->fonte_nome,"%s",raw[first].fonte_nome);
    h->cap=raw[first].cap;
    for(int pass=0;pass<2;pass++)
      for(int i=0;i<nr&&npicks<192;i++)
        if(!done[i]&&!strcmp(raw[i].fonte_key,h->fonte_key)&&(pass||raw[i].fav)){
          done[i]=1;picks[npicks]=raw[i];npicks++;
        }
  }
}
/* Sources carrying the same channel as mv_target, for Change source. */
static void mv_alts_build(void){
  mv_nalt=0;
  if(mv_target<0||mv_target>3||!mv_items[mv_target].id[0])return;
  char want[160];mv_norm(mv_items[mv_target].name,want,sizeof want);
  for(int i=0;i<4096&&mv_nalt<32;i++){
    const char *id,*title,*logo,*group,*base;
    if(!guia_canal_campos(i,&id,&title,&logo,&group,&base))continue;
    if(!g5ui_channel_allowed(id,title,group,base))continue;
    char n[160];mv_norm(title,n,sizeof n);
    if(strcmp(n,want))continue;
    const char *k=guia_canal_fonte(i);
    if(!strcmp(k?k:"",mv_fonte[mv_target]))continue;
    mv_pick_fill(&mv_alts[mv_nalt],i,NULL);
    int dup=0;for(int j=0;j<mv_nalt;j++)if(!strcmp(mv_alts[j].fonte_key,mv_alts[mv_nalt].fonte_key))dup=1;
    if(!dup)mv_nalt++;
  }
  for(int i=0;i<count&&mv_nalt<32;i++)
    if(inventory[i].kind==G5_LIVE&&!restricted(&inventory[i])){
      char n[160];mv_norm(inventory[i].name,n,sizeof n);if(strcmp(n,want))continue;
      const char *k=!strcmp(inventory[i].source,"xtream")?"xt":"g5m3u";
      if(!strcmp(k,mv_fonte[mv_target]))continue;
      int dup=0;for(int j=0;j<mv_nalt;j++)if(!strcmp(mv_alts[j].fonte_key,k))dup=1;
      if(dup)continue;
      mv_pick_fill(&mv_alts[mv_nalt],-1,&inventory[i]);mv_nalt++;
    }
}
/* Playback per slot: the main slot runs on the app player (its watchdog owns
 * the retries); the others ask the platform for an extra decoder. Opens are
 * edge-triggered by the controller; periodic refreshes never re-open. */
static void mv_tick(void){
  unsigned agora=SDL_GetTicks();
  int extras=video_mv_max();
  static int main_ant=-1;
  if(main_ant!=mv_main){   /* moving main changes the extra-decoder mapping */
    video_mv_parar_todos();memset(mv_extra_on,0,sizeof mv_extra_on);
    for(int i=0;i<4;i++)g5_mv_ctl_set(&mvctl,i,"",NULL,agora);
    main_ant=mv_main;
  }
  for(int i=0;i<4;i++){
    int ativo=i<mode&&mv_items[i].id[0];
    g5_mv_ctl_set(&mvctl,i,ativo?mv_items[i].id:"",ativo?mv_fonte[i]:"",agora);
    if(!ativo){if(mv_extra_on[i]){mv_extra_on[i]=0;video_mv_parar(mv_decoder(i));}mv_disp[i]=G5_MV_VAZIO;continue;}
    if(i==mv_main){
      if(g5_mv_ctl_abrir(&mvctl,i,agora))mv_main_req=1;
      if(player_mini_no_guia_ativo()&&video_pronto())g5_mv_ctl_pronto(&mvctl,i);
      continue;
    }
    int ord=mv_decoder(i);
    char url[2100],cab[600];
    int cap=mv_cap(mv_fonte[i]);
    int bloq=ord>=extras?G5_MV_SEM_DECODER:!mv_url(&mv_items[i],url,sizeof url,cab,sizeof cab)?G5_MV_SEM_FONTE:
             cap>0&&mv_uso(mv_fonte[i],i)>=cap?G5_MV_SEM_FONTE:0;
    if(bloq){
      if(mv_extra_on[i]){mv_extra_on[i]=0;video_mv_parar(ord);}
      /* stopped: reopen once the block lifts (cap freed, slot swapped) */
      if(mvctl.slot[i].pediu){mvctl.slot[i].pediu=0;mvctl.slot[i].estado=G5_MV_ABRINDO;}
      if(mv_disp[i]!=bloq)printf("[mv] slot %d parado: %s\n",i,bloq==G5_MV_SEM_DECODER?"limite de decodificadores da TV":"limite de conexoes da fonte"),fflush(stdout);
      mv_disp[i]=bloq;continue;
    }
    mv_disp[i]=mvctl.slot[i].estado;
    if(g5_mv_ctl_abrir(&mvctl,i,agora)){
      if(!video_mv_abrir(ord,url,cab)){mv_extra_on[i]=1;printf("[mv] slot %d: decodificador extra ativo (%d de %d)\n",i,ord+1,extras);fflush(stdout);}
      else{mv_disp[i]=G5_MV_SEM_DECODER;printf("[mv] slot %d: decodificador extra indisponivel\n",i);fflush(stdout);}
    }
    if(mv_extra_on[i]){
      int st=video_mv_estado(ord);
      if(st==2)g5_mv_ctl_pronto(&mvctl,i);
      else if(st<0&&mvctl.slot[i].estado!=G5_MV_ERRO){
        g5_mv_ctl_erro(&mvctl,i,agora);mv_extra_on[i]=0;video_mv_parar(ord);
        printf("[mv] slot %d: erro na transmissao (tentativa %d de 4)\n",i,mvctl.slot[i].tentativas);fflush(stdout);
      }
      GfxRect r=mv_rect(i);video_mv_janela(ord,(int)r.x,(int)r.y,(int)r.w,(int)r.h);
    }
  }
}
/* Modals over their page (PIN, temporary unlock, multiview picker/limits). */
static void modal_draw(const char *s){
  if(!strcmp(s,"parental-pin")){
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
    button(1,i18n("Cancelar"),"pl_chevron-left",832,807,230,60,0);
  }
  if(!strcmp(s,"parental-desbloqueio")){
    modal(530,260,860,580);
    label(TXT_HEADLINE,i18n("Desbloqueio temporário"),586,320,750,0);label(TXT_ILHA_CORPO,i18n("Regras voltam ao trocar de perfil."),586,400,750,1);
    char time[64];snprintf(time,sizeof time,i18n("%d horas"),lease_seconds/3600);button(10,time,"aj_clock",586,500,740,64,0);button(11,i18n("Continuar com PIN"),"aj_lock",586,650,740,64,0);
  }
  if(!strcmp(s,"multiview-escolher")){
    /* Channel picker: sheet over Multiview, search on top, results grouped by
     * source. A source at its connection limit keeps its rows visible but
     * disabled, with the reason; selecting one replaces that source's slot. */
    static char ultima[140];if(strcmp(ultima,query)){snprintf(ultima,sizeof ultima,"%s",query);mv_picker_build();}
    modal(1050,128,822,848);
    label(TXT_ILHA_SUB,"MULTIVIEW",1094,174,600,1);
    label(TXT_TITULO2,i18n(mv_target>=0&&mv_target<mode&&mv_items[mv_target].id[0]?"Trocar canal":"Adicionar canal"),1094,214,700,0);
    button(899,query[0]?query:i18n("Buscar canal"),"aj_search",1094,284,734,64,0);
    static int start;
    if(focus==899||start>=npicks)start=0;
    if(focus>=900&&focus<900+npicks){
      int target=focus-900,height=0;
      if(start>target)start=target;
      for(int i=start;i<=target;i++)height+=picks[i].header?84:72;
      while(height>900-364&&start<target)height-=picks[start++].header?84:72;
    }
    float y=364;
    for(int i=start;i<npicks&&y+(picks[i].header?84:64)<=900;i++){
      MvPick *p=&picks[i];
      if(p->header){
        GfxRect hr={1094,y+22,734,1};gfx_cor(hr,0,1,1,1,.08f);
        label(TXT_ILHA_SUB,p->fonte_nome,1094,y+32,520,1);
        if(p->cap>0){char c8[48];snprintf(c8,sizeof c8,i18n("%d tela(s)"),p->cap);label(TXT_ILHA_SUB,c8,1640,y+32,188,1);}
        y+=84;continue;
      }
      GfxRect r={1094,y,734,64};
      int focused=nav<0&&focus==900+i&&!sem_foco;
      if(focused)plrui_linha_foco(foco_rect(r),20,1);
      int c=p->disabled?120:240;   /* focused rows keep the label readable */
      if(p->fav)gfx_icone((GfxRect){r.x+22,y+20,24,24},"aj_star",c/255.0f,c/255.0f,c/255.0f,1);
      TxtLinha l=txt_linha_corta_bruta(TXT_ILHA_CORPO,p->it.name,c,c,c,255,p->disabled?300:600);
      txt_desenhar(l,r.x+58,y+(64-(float)l.h)/2);
      if(p->disabled)label(TXT_ILHA_SUB,i18n("Fonte no limite"),r.x+r.w-230,y+22,214,1);
      ponteiro_alvo(r.x,r.y,r.w,r.h,point_focus,point_activate,900+i,0);reg_focus(900+i,r.x,r.y,r.w,r.h);
      y+=72;
    }
    if(!npicks)label(TXT_ILHA_CORPO,i18n("Nenhum canal na lista"),1094,380,700,1);
  }
  if(!strcmp(s,"multiview-slot")){
    /* Per-slot actions, where the footer row used to be. */
    G5Item *it=mv_target>=0&&mv_target<4&&mv_items[mv_target].id[0]?&mv_items[mv_target]:NULL;
    modal(630,240,660,620);
    label(TXT_ILHA_SUB,"MULTIVIEW",678,292,560,1);
    label(TXT_TITULO2,it?it->name:"",678,332,560,0);
    if(it){char fonte[96];label(TXT_ILHA_CORPO,mv_fonte_nome(mv_fonte[mv_target],fonte,sizeof fonte),678,414,560,1);}
    float y=484;
    button(850,i18n("Trocar canal"),"aj_arrow-left-right",678,y,564,64,1);y+=82;
    if(mv_nalt){button(851,i18n("Trocar fonte"),"aj_refresh-cw",678,y,564,64,1);y+=82;}
    if(it&&mv_target!=mv_main){button(852,i18n("Áudio principal"),"aj_volume-2",678,y,564,64,1);y+=82;}
    button(853,i18n("Remover canal"),"aj_x",678,y,564,64,1);
  }
  if(!strcmp(s,"multiview-fonte")){
    /* Same channel from another configured source, with the capacity left. */
    modal(630,240,660,320+82*(mv_nalt>0?mv_nalt:1));
    label(TXT_ILHA_SUB,"MULTIVIEW",678,292,560,1);
    label(TXT_TITULO2,i18n("Trocar fonte"),678,332,560,0);
    label(TXT_ILHA_CORPO,i18n("O mesmo canal por outra fonte configurada."),678,414,560,1);
    float y=486;
    for(int i=0;i<mv_nalt;i++){
      MvPick *p=&mv_alts[i];
      GfxRect r={678,y,564,64};
      int focused=nav<0&&focus==860+i&&!sem_foco;
      if(focused)plrui_linha_foco(foco_rect(r),20,1);
      int c=p->disabled?120:240;
      TxtLinha l=txt_linha_corta_bruta(TXT_ILHA_CORPO,p->fonte_nome,c,c,c,255,300);
      txt_desenhar(l,r.x+24,y+(64-(float)l.h)/2);
      char cap[64];
      if(p->cap>0)snprintf(cap,sizeof cap,i18n("%d de %d tela(s) livre(s)"),p->cap-p->uso,p->cap);
      else snprintf(cap,sizeof cap,"%s",i18n("Sem limite"));
      label(TXT_ILHA_SUB,cap,r.x+r.w-260,y+22,244,1);
      ponteiro_alvo(r.x,r.y,r.w,r.h,point_focus,point_activate,860+i,0);reg_focus(860+i,r.x,r.y,r.w,r.h);
      y+=82;
    }
    if(!mv_nalt)label(TXT_ILHA_CORPO,i18n("Nenhuma outra fonte tem este canal."),678,500,560,1);
  }
  if(!strcmp(s,"multiview-limite")){
    modal(490,235,940,650);
    label(TXT_ILHA_SUB,"MULTIVIEW",546,288,800,1);label(TXT_TITULO2,i18n("Limites desta TV"),546,345,820,0);
    char linha[192];
    snprintf(linha,sizeof linha,i18n("Esta TV decodifica até %d canal(is) ao mesmo tempo."),video_mv_max()+1);
    label(TXT_ILHA_CORPO,linha,546,460,820,0);
    if(xtream_configurado()){snprintf(linha,sizeof linha,i18n("Sua conta Xtream permite %d tela(s) por vez."),mv_xtream_max());label(TXT_ILHA_CORPO,linha,546,530,820,0);}
    label(TXT_ILHA_CORPO,i18n("Listas M3U e addons não têm limite de conta."),546,600,820,1);
    label(TXT_ILHA_CORPO,i18n("Um quadro por fonte evita bloqueio da conta."),546,660,820,1);
    button(810,i18n("Entendi"),"aj_check",546,735,820,70,0);
  }
}
/* The page a modal sits on, drawn still under the veil. PIN never shows a
 * protected page behind it. */
static const char *modal_pagina(void){
  if(!strncmp(screen,"multiview-",10))return mode==4?"multiview-4":mode==3?"multiview-3":"multiview-2";
  if(!strcmp(screen,"parental-desbloqueio"))return "parental-ajustes";
  if(!strcmp(screen,"parental-pin")&&(!strcmp(previous,"parental-canal-bloqueado")||!strcmp(previous,"setup-categorias")||!strcmp(previous,"fontes-gerenciar")))return previous;
  return NULL;
}
/* Guia TV v5: tela principal (definicoes adiante neste arquivo). */
static int gv_is(const char *s);
static void gv_screen(void);
static int gv_event(SDL_Keycode k,int repeat);
static int gv_activate(void);
static void gv_tick(void);
static void gs_tick(void);
static void page_draw(void){
  if(!strcmp(screen,"setup-celular-tv")||(!strcmp(screen,"setup-pronto")&&phone_stage==3))phone_screen();
  else if(!strncmp(screen,"setup-",6))wizard();
  else if(!strcmp(screen,"fontes-gerenciar"))sources_screen();
  else if(!strcmp(screen,"vod-titulo")||!strcmp(screen,"vod-serie-temporadas"))title_screen();
  else if(!strncmp(screen,"vod-",4))catalog();
  else if(!strcmp(screen,"parental-perfil-infantil"))kids_screen();
  else if(!strncmp(screen,"parental-",9))parental_screen();
  else if(!strncmp(screen,"multiview-",10))multiview_screen();
}
int g5ui_draw(void){
  if(!screen[0])return 0;
  gfx_cor((GfxRect){0,0,1920,1080},0,.045,.047,.051,1);control_count=0;num_focus=0;foco_visto=0;
  /* New screen rises in; a modal grows over its page and, closed, shrinks and
   * fades out over the page it returns to. Same T21 ramps as the guide. */
  static char ult[48];static Uint32 ult_ms;Uint32 ms=SDL_GetTicks();
  float dt=ult_ms&&ms>ult_ms?fminf((float)(ms-ult_ms)/1000.0f,.05f):0;ult_ms=ms;
  if(strcmp(ult,screen)){
    if(modal_tela(screen))modal_t=0;else if(!modal_tela(ult))page_t=0;
    if(modal_tela(ult)&&!modal_tela(screen)){snprintf(modal_sai,sizeof modal_sai,"%s",ult);modal_sai_t=1;}
    if(!modal_tela(ult)&&!modal_tela(screen))foco_glide.ok=0;
    snprintf(ult,sizeof ult,"%s",screen);
  }
  gv_edge_x=anim_mistura(gv_edge_x,0,1-expf(-T21_SCROLL*dt));
  gv_edge_y=anim_mistura(gv_edge_y,0,1-expf(-T21_SCROLL*dt));
  modal_t=t21_entrada(modal_t,dt,1);page_t=t21_entrada(page_t,dt,1);modal_sai_t=t21_entrada(modal_sai_t,dt,0);
  if(gv_is(screen))gv_screen();
  else{
    gv_dt=dt;
    gv_side=anim_mola2(&gv_side_v,gv_side,(vod_sidebar||gv_side_open)?1:0,dt,T21_SCROLL);
    float og=gfx_opacidade_grupo,pa=anim_suave(page_t);
    top();
    gfx_opacidade_grupo=og*pa;
    if(pa<1)gfx_transformar(960,540,1,0,18*(1-pa));
    char cur[48];snprintf(cur,sizeof cur,"%s",screen);
    int md=modal_tela(cur);const char *pg=md?modal_pagina():cur;
    if(pg){
      if(md){snprintf(screen,sizeof screen,"%s",pg);sem_foco=1;}
      page_draw();
      if(md){snprintf(screen,sizeof screen,"%s",cur);sem_foco=0;}
    }
    gfx_sem_transformar();gfx_opacidade_grupo=og;
    if(!strncmp(screen,"fontes-",7)&&gv_side>.001f){gv_snapshot();gv_sidebar(168);}
    if(md){modal_draw(cur);gfx_sem_transformar();gfx_opacidade_grupo=og;}
    /* One visible focus: a focus id not drawn on this screen goes to the first control. */
    if(nav<0&&num_focus){int ok=0;for(int i=0;i<num_focus;i++)ok|=next_focus[i]==focus;if(!ok)focus=next_focus[0];}
  }
  if(modal_sai_t>0&&modal_sai[0]&&!modal_tela(screen)){
    float og=gfx_opacidade_grupo;sem_foco=1;modal_draw(modal_sai);sem_foco=0;
    gfx_sem_transformar();gfx_opacidade_grupo=og;
  }
  if(!foco_visto)foco_glide.ok=0;
#ifdef NV_DESKTOP_UI
  if(edit>=0)teclado_desenhar(SDL_GetTicks());   /* input(): the desktop text field */
#endif
  return 1;
}
static void activate(int id){
  if(id==1106){g5ui_open("multiview-2");return;}
  if(id>=1100&&id<=1105){   /* guide top bar: Grade, Cartoes, Esportes, Filmes e series, Buscar, Opcoes */
    int n=id-1100;nav=-1;
    if(gv_is(screen)){if(n==3)g5ui_open("vod-filmes");else if(n==5)g5ui_open("fontes-gerenciar");else if(n==4)gv_go("busca");else{gv_zone=5;focus=id;gv_go(n==0?"grade":n==1?"cartoes":"esportes-ligas");}return;}
    if(episodes_mode&&series_catalog){free(inventory);inventory=series_catalog;series_catalog=NULL;count=series_count;selected=parent_series;}episodes_mode=0;
    if(n==3){g5ui_open("vod-filmes");return;}
    if(n==5){if(strcmp(screen,"fontes-gerenciar"))g5ui_open("fontes-gerenciar");return;}
    gv_saida[0]=0;if(n<3)snprintf(gv_pagina,sizeof gv_pagina,"%s",n==0?"grade":n==1?"cartoes":"esportes-ligas");
    gv_voltar();if(n>=4)gv_go(n==4?"busca":"ajustes");else{gv_zone=5;focus=id;}return;}
  if(gv_is(screen)){focus=id;gv_activate();return;}
  if(!strcmp(screen,"fontes-gerenciar")&&sources_activate(id))return;
  if(atomic_load(&busy)){snprintf(message,sizeof message,"%s",i18n("Aguarde…"));return;}
  if(!strcmp(screen,"setup-tipo")){
    if(id==4){pg_tras();return;}if(id==2){go("setup-arquivo");return;}if(id==3){phone();return;}
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
    if(id==5){pg_tras();return;}
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
  if(!strcmp(screen,"setup-pronto")){if(id==0)gv_voltar();else if(id==1)go("vod-filmes");else{pg_n=0;go("fontes-gerenciar");}return;}
  if(!strcmp(screen,"parental-pin")){
    size_t n=strlen(pin_value);
    if(id>=20&&id<=31){if(id==29)erase(pin_value,sizeof pin_value);else if(id==31){if(n)pin_value[n-1]=0;}else if(n<4){pin_value[n]=(char)('0'+(id==30?0:id-19));pin_value[n+1]=0;}return;}
    if(id==0){if(n==4)start(3);return;}
    if(id==1){erase(pin_value,sizeof pin_value);   /* up one level: the screen that asked */
      if(pg_n)pg_voltar();else if(previous[0]&&!gv_is(previous))go(previous);else gv_voltar();return;}
    if(id==3)lease_seconds=lease_seconds==3600?7200:lease_seconds==7200?14400:3600;return;
  }
  if(!strcmp(screen,"parental-canal-bloqueado")){
    if(id==0)want_pin(0);else if(id==1)pg_tras();else if(id==3)lease_seconds=lease_seconds==3600?7200:lease_seconds==7200?14400:3600;else want_pin(1);return;
  }
  if(!strcmp(screen,"parental-desbloqueio")){
    if(id==10)lease_seconds=lease_seconds==3600?7200:lease_seconds==7200?14400:3600;else if(id==11)want_pin(0);return;
  }
  if(!strcmp(screen,"parental-lista")){
    if(id==900){pg_tras();return;}
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
    if(id==0&&selected<count&&!restricted(&inventory[selected]))playing=1;else if(id==2){if(pg_n)pg_voltar();else go("vod-filmes");}return;
  }
  if(!strcmp(screen,"vod-serie-temporadas")){
    if(id>=100&&id<100+count&&episodes_mode){selected=id-100;if(!restricted(&inventory[selected]))playing=1;return;}
    if(id==2&&episodes_mode&&series_catalog){leave_episodes();return;}   /* episodes are one level */
    if(id==800||id==801||id==2){if(episodes_mode&&series_catalog){free(inventory);inventory=series_catalog;series_catalog=NULL;count=series_count;episodes_mode=0;}if(id==2&&pg_n)pg_voltar();else go(id==800?"vod-filmes":"vod-series");return;}
    if(id==0){selected_season=selected_season>=30?1:selected_season+1;return;}
    if(id==2){go("vod-series");return;}
    if(episodes_mode&&series_catalog){free(inventory);inventory=series_catalog;series_catalog=NULL;count=series_count;selected=parent_series;episodes_mode=0;}
    snprintf(action,sizeof action,"get_series_info&series_id=%s",inventory[selected].id);start(4);return;
  }
  if(!strncmp(screen,"vod-",4)){
    if(id==810){guia_recarregar(0);vod_lazy=1;vod_failed=0;memset(vod_done_count,0,sizeof vod_done_count);source_loaded=0;
      if(draft.type==0)vod_begin();else{vod_refresh_stamp=catsnap_refresh_stamp(profile);action[0]=0;start(2);}return;}
    if(id==800){episodes_mode=0;catalog_group[0]=0;go("vod-filmes");}else if(id==801){episodes_mode=0;catalog_group[0]=0;go("vod-series");}else if(id==802||id==804){go("vod-busca");input(5,"Buscar",query,0);}
    else if(id==803)go("fontes-gerenciar");else if(id==805){catalog_group[0]=0;focus=VOD_ITEM;}else if(id==806)go("setup-tipo");else if(id>=820&&id<820+group_count){snprintf(catalog_group,sizeof catalog_group,"%s",groups[id-820]);focus=VOD_ITEM;}else if(id>=VOD_ITEM&&id<VOD_ITEM+vod_size)vod_select(id-VOD_ITEM);else if(query[0])query[0]=0;else go("setup-tipo");
    if(id==800||id==801||id==805||(id>=820&&id<820+group_count)){vod_sidebar=0;focus=vod_return_focus=VOD_ITEM;}
    return;
  }
  if(!strncmp(screen,"multiview-",10)){
    unsigned agora=SDL_GetTicks();
    if(!strcmp(screen,"multiview-slot")){
      if(id==850){int t=mv_target;g5ui_open("multiview-escolher");mv_target=t;return;}
      if(id==851&&mv_nalt){go("multiview-fonte");return;}
      if(id==852&&mv_target>=0&&mv_target<4&&mv_items[mv_target].id[0]&&mv_target!=mv_main){
        mv_main=mv_target;   /* mv_tick reopens the two slots on the right decoders */
        pg_tras();focus=mv_target;return;
      }
      if(id==853&&mv_target>=0&&mv_target<4){
        memset(&mv_items[mv_target],0,sizeof mv_items[0]);mv_fonte[mv_target][0]=0;
        g5_mv_ctl_set(&mvctl,mv_target,"",NULL,agora);
        if(mv_target==mv_main){mv_main=0;for(int s=0;s<4;s++)if(mv_items[s].id[0]){mv_main=s;break;}}
        pg_tras();focus=mv_target;return;
      }
      return;
    }
    if(!strcmp(screen,"multiview-fonte")){
      if(id>=860&&id<860+mv_nalt){
        MvPick *p=&mv_alts[id-860];
        if(p->disabled){snprintf(message,sizeof message,"%s",i18n("Fonte no limite"));return;}
        mv_items[mv_target]=p->it;snprintf(mv_fonte[mv_target],sizeof mv_fonte[0],"%s",p->fonte_key);
        g5_mv_ctl_set(&mvctl,mv_target,p->it.id,p->fonte_key,agora);mv_born[mv_target]=agora;
        printf("[mv] slot %d: mesma programacao por outra fonte\n",mv_target);fflush(stdout);
        go(mode==4?"multiview-4":mode==3?"multiview-3":"multiview-2");focus=mv_target;return;
      }
      return;
    }
    if(!strcmp(screen,"multiview-escolher")){
      if(id==899){input(5,i18n("Buscar canal"),query,0);return;}
      if(id>=900&&id<900+npicks){
        MvPick *p=&picks[id-900];
        if(p->header||mv_target<0||mv_target>=mode)return;
        if(restricted(&p->it))return;
        if(p->disabled){
          /* Source at its connection limit: replace that source's slot. */
          int j=-1;for(int s=0;s<4;s++)if(mv_items[s].id[0]&&!strcmp(mv_fonte[s],p->fonte_key)){j=s;break;}
          if(j<0){snprintf(message,sizeof message,"%s",i18n("Fonte no limite"));return;}
          mv_items[j]=p->it;g5_mv_ctl_set(&mvctl,j,p->it.id,p->fonte_key,agora);mv_born[j]=agora;
          snprintf(message,sizeof message,"%s",i18n("Canal trocado no quadro dessa fonte"));
          pg_tras();focus=j;return;
        }
        mv_items[mv_target]=p->it;
        snprintf(mv_fonte[mv_target],sizeof mv_fonte[0],"%s",p->fonte_key);
        if(mv_target>=mv_count)mv_count=mv_target+1;
        g5_mv_ctl_set(&mvctl,mv_target,p->it.id,p->fonte_key,agora);mv_born[mv_target]=agora;
        if(!mv_items[mv_main].id[0])mv_main=mv_target;
        pg_tras();focus=mv_target;return;
      }
      return;
    }
    if(!strcmp(screen,"multiview-limite")){if(id==810)pg_tras();return;}
    if(id==800||id==801||id==802){
      int ant=mode;
      mode=id==800?2:id==801?3:4;
      for(int i=ant;i<mode;i++)if(!mv_glide[i].ok)mv_born[i]=agora;   /* new slots fade in */
      if(mv_main>=mode){mv_main=0;for(int s=0;s<mode;s++)if(mv_items[s].id[0]){mv_main=s;break;}}
      go(mode==4?"multiview-4":mode==3?"multiview-3":"multiview-2");focus=id;
      if(player_mini_no_guia_ativo()){float x,y,w,h;g5ui_multiview_rect(&x,&y,&w,&h);player_mini_no_guia(x,y,w,h);}
      return;
    }
    if(id==805){go("multiview-limite");return;}
    if(id==806){gv_voltar();return;}
    if(id>=0&&id<mode){
      mv_target=id;
      if(mv_items[id].id[0]){mv_alts_build();go("multiview-slot");}
      else g5ui_open("multiview-escolher");
      return;
    }
    return;
  }
}
static int g5ui_event_inner(const SDL_Event *e){
  if(!screen[0])return 0;
  if(e->type==SDL_KEYUP&&(gv_is(screen)||gv_side_open)){gv_tecla_solta(e->key.keysym.sym);return 1;}
  if(e->type!=SDL_KEYDOWN)return 1;
  SDL_Keycode k=e->key.keysym.sym;
  if(gv_is(screen)||gv_side_open)return gv_event(k,e->key.repeat);
  if(!strcmp(screen,"fontes-gerenciar")&&gv_arrow(k)&&nav<0){
    if(focus>=100&&focus<116&&(k==SDLK_UP||k==SDLK_DOWN)){
      int row=(focus-100)/4,col=(focus-100)%4,d=k==SDLK_DOWN?1:-1;GuiaFonteSaude source;
      if(row+d<0&&sources_offset){sources_offset-=4;sources_key[0]=0;focus=112+col;return 1;}
      if(row+d==4&&guia_fonte_saude(sources_offset+4,&source)){sources_offset+=4;sources_key[0]=0;focus=100+col;return 1;}
    }
    sources_key[0]=0;if(!nav_spatial(k)&&k==SDLK_UP)nav=0;return 1;
  }
  int vod=!strncmp(screen,"vod-",4)&&strcmp(screen,"vod-titulo")&&strcmp(screen,"vod-serie-temporadas");
  if(vod&&vod_sidebar&&(k==SDLK_RIGHT||k==SDLK_ESCAPE||k==SDLK_AC_BACK||k==SDLK_BACKSPACE)){
    vod_sidebar=0;focus=vod_item_focus(vod_return_focus);return 1;
  }
  if(vod&&vod_sidebar&&gv_arrow(k)){
    if(k==SDLK_UP||k==SDLK_DOWN){
      int ids[9],n=0;ids[n++]=800;ids[n++]=801;ids[n++]=802;ids[n++]=805;
      for(int g=0;g<group_count&&g<4;g++)ids[n++]=820+g;ids[n++]=806;
      for(int i=0;i<n;i++)if(ids[i]==focus){
        int next=i+(k==SDLK_UP?-1:1);
        if(next>=0&&next<n)focus=ids[next];
        else if(next<0){vod_sidebar=0;nav=3;}
        break;
      }
    }return 1;
  }
  if(vod)vod_filter();
  if(vod&&k==SDLK_LEFT&&!vod_sidebar&&nav<0&&((focus>=VOD_ITEM&&focus<VOD_ITEM+vod_size&&vod_position()==vod_layout[vod_row_at(vod_position())].start)||!vod_size)){
    vod_return_focus=vod_item_focus(focus);vod_sidebar=1;focus=800;nav=-1;return 1;
  }
  if(k==SDLK_ESCAPE||k==SDLK_AC_BACK||k==SDLK_BACKSPACE){
    if(vod&&nav<0&&focus>=VOD_ITEM){vod_return_focus=focus;nav=3;return 1;}
    if(!strcmp(screen,"setup-celular-tv"))celular_fechar();
    /* modals and detail pages close to the page under them, like their buttons */
    if(!strcmp(screen,"parental-pin"))activate(1);
    else if(episodes_mode&&series_catalog&&!strncmp(screen,"vod-",4))leave_episodes();
    else if(!(pg_n&&pg_nivel(screen))&&(!strcmp(screen,"vod-titulo")||!strcmp(screen,"vod-serie-temporadas")))activate(2);
    else pg_tras();return 1;
  }
  if(!strcmp(screen,"parental-pin")&&k>=SDLK_0&&k<=SDLK_9){activate(k==SDLK_0?30:19+(int)(k-SDLK_0));return 1;}
  int md=modal_tela(screen);   /* a modal keeps the D-pad inside it */
  if(k==SDLK_TAB){if(!md)nav=nav<0?0:-1;return 1;}
  if(nav>=0){if(vod&&k==SDLK_LEFT&&nav==0){vod_return_focus=vod_item_focus(focus);vod_sidebar=1;nav=-1;focus=800;return 1;}if(k==SDLK_LEFT&&nav>0)nav--;if(k==SDLK_RIGHT&&nav<5)nav++;
    if(k==SDLK_DOWN){nav=-1;if(vod)focus=vod_item_focus(vod_return_focus);}if((k==SDLK_RETURN||k==SDLK_KP_ENTER)&&!e->key.repeat)activate(1100+nav);return 1;}
  /* Multiview: every control (slots, layout chips, global chips, picker rows)
   * registers its rect while drawing; the arrows follow the spatial map. */
  if(gv_arrow(k)&&!strncmp(screen,"multiview-",10)){
    if(!strcmp(screen,"multiview-escolher")&&(k==SDLK_UP||k==SDLK_DOWN)&&focus>=900&&focus<900+npicks){
      int t=focus-900+(k==SDLK_DOWN?1:-1);
      while(t>=0&&t<npicks&&picks[t].header)t+=(k==SDLK_DOWN?1:-1);
      if(t>=0&&t<npicks){focus=900+t;return 1;}
      if(k==SDLK_UP){focus=899;return 1;}
    }
    if(!nav_spatial(k)&&k==SDLK_UP&&!modal_tela(screen))nav=0;
    return 1;
  }
  if(k==SDLK_UP||k==SDLK_DOWN||k==SDLK_LEFT||k==SDLK_RIGHT){
    if(!strcmp(screen,"parental-pin")&&focus>=20&&focus<=31){
      int next=focus+(k==SDLK_UP?-3:k==SDLK_DOWN?3:k==SDLK_LEFT?-1:1);
      if(next>=20&&next<=31)focus=next;else focus=k==SDLK_DOWN?0:3;return 1;
    }
    if(vod&&!vod_sidebar&&vod_size){vod_navigate(k);return 1;}
    /* Paged lists: Up/Down walk the whole list and the page follows. */
    if(k==SDLK_UP||k==SDLK_DOWN){
      int d=k==SDLK_DOWN?1:-1;
      if(!strcmp(screen,"parental-lista")&&focus>=0&&focus<900){
        int t=focus+d,ok=t>=0&&(rules_mode?guia_canal_regra_campos(t,NULL,NULL,NULL,NULL,NULL):t<group_count);
        if(ok){focus=t;return 1;}
      }

      if(!strcmp(screen,"setup-categorias")&&focus>=2&&focus<2+group_count){
        int vis[G5_GROUPS],nv=cat_vis(vis),p=0;for(int j=0;j<nv;j++)if(vis[j]==focus-2)p=j;
        int t=p+2*d;if(d>0&&t>=nv&&(p/2+1)*2<nv)t=nv-1;
        if(t>=0&&t<nv){focus=2+vis[t];return 1;}
      }
    }
    if(!nav_spatial(k)&&k==SDLK_UP&&!md)nav=0;
    return 1;
  }
  if((k==SDLK_RETURN||k==SDLK_KP_ENTER)&&!e->key.repeat&&focus>=0)activate(focus);
  return 1;
}
int g5ui_event(const SDL_Event *e){
  int old_focus=focus,old_nav=nav,old_zone=gv_zone;
#ifdef NV_DESKTOP_UI
  if(screen[0]&&edit>=0&&teclado_aberto()){teclado_evento(e);return 1;}
#endif
  if(e->type==SDL_KEYDOWN&&gv_arrow(e->key.keysym.sym))gv_exit_armed=0;
  int handled=g5ui_event_inner(e);
  if(e->type==SDL_KEYDOWN&&gv_arrow(e->key.keysym.sym)&&old_focus==focus&&old_nav==nav&&old_zone==gv_zone){
    SDL_Keycode k=e->key.keysym.sym;
    gv_edge_x=k==SDLK_LEFT?-6:k==SDLK_RIGHT?6:0;gv_edge_y=k==SDLK_UP?-6:k==SDLK_DOWN?6:0;
  }return handled;
}
void g5ui_update(void){
  context();
  if(!atomic_load(&busy)&&!strncmp(screen,"vod-",4)&&guia_catalog_refresh_revision()!=vod_refresh_revision){
    if(draft.type==0)vod_begin();else{action[0]=0;start(2);}
  }
#ifdef NV_DESKTOP_UI
  if(edit>=0)teclado_atualizar(1.0f/60,SDL_GetTicks());
#endif
  if(gv_is(screen))gv_tick();
  gs_tick();
  {int agora=!strncmp(screen,"multiview-",10);
   if(mv_aberto&&!agora){   /* left Multiview: extra decoders stop at once */
     video_mv_parar_todos();memset(&mvctl,0,sizeof mvctl);memset(mv_extra_on,0,sizeof mv_extra_on);
     mv_main_req=0;for(int i=0;i<4;i++){mv_glide[i].ok=0;mv_born[i]=0;}
   }
   mv_aberto=agora;
   if(agora)mv_tick();}
  if(atomic_load_explicit(&metadata.ready,memory_order_acquire)){
    atomic_store(&metadata.ready,0);atomic_store(&metadata.busy,0);
    if(metadata.ok&&metadata.profile==profile&&!strcmp(metadata.account,account)&&metadata.revision==vod_revision){vod_info=vod_info_pending;vod_info_ref=metadata.ref;}
  }
  if(lease.expires&&lease.expires<=time(NULL)){erase(&lease,sizeof lease);if(player_id_canal()[0])player_encerrar();dirty=1;}
  if(catalog_fetch_pending&&!atomic_load(&busy)){catalog_fetch_pending=0;action[0]=0;start(2);}
  if(atomic_load_explicit(&ready,memory_order_acquire)){
    int phone_fetch=0,fallback_fetch=0;
    atomic_store(&busy,0);atomic_store(&ready,0);erase(&work_draft,sizeof work_draft);
    if(!worker_cancel&&worker_generation==catalog_generation&&worker_profile==profile&&!strcmp(worker_account,account)){
      if(operation==6||operation==7||operation==8){
        vod_snapshot_served|=worker_snapshot_served;
        if(vod_fallback_required()){vod_lazy=0;fallback_fetch=1;}
        else if(!worker_ok&&(operation==6||operation==7))vod_failed=1;
        if(worker_ok||operation==6){
          int old_focus=focus;float old_scroll=vod_scroll;
          vod_index_inventory(&vod_pending);
          vod_free(&vod_data);vod_data=vod_pending;memset(&vod_pending,0,sizeof vod_pending);
          vod_rebuild();focus=old_focus;vod_scroll=old_scroll;
          source_loaded=operation!=8||!worker_categories_missing;vod_failed=!worker_ok;
          if(operation==7){
            int k=vod_request_kind;
            char (*done)[64]=realloc(vod_done[k],(vod_done_count[k]+1)*sizeof *done);
            if(!done)vod_failed=1;
            else{vod_done[k]=done;snprintf(done[vod_done_count[k]++],64,"%s",vod_request_id);}
          }
          for(int r=0;r<vod_data.nrows&&group_count<G5_GROUPS;r++){
            const char *name=vod_data.rows[r].name;int found=0;
            for(int g=0;g<group_count;g++)found|=!strcmp(groups[g],name);
            if(!found){snprintf(groups[group_count],64,"%s",name);group_on[group_count++]=!adult_group(name);}
          }
        }else vod_failed=1;
      }
      if(operation==1)test_ok=worker_ok;
      if(operation==3){
        if(worker_ok){if(pin_settings==1)go("parental-ajustes");else if(pin_settings==2){rules.hide_adult=0;save_rules();go("setup-categorias");}
          else{g5_grant(&lease,profile,locked_item.source,locked_item.id,time(NULL),lease_seconds,1);if(locked_item.kind==G5_LIVE)g5ui_close();else go(previous[0]?previous:"vod-filmes");}}
        else{go("parental-pin");snprintf(message,sizeof message,"%s",i18n(pin_result==0?"PIN incorreto":"Não foi possível verificar o PIN"));}
      }else if(staging&&worker_ok&&operation!=6&&operation!=7&&operation!=8){if(operation==4){free(series_catalog);series_catalog=inventory;series_count=count;parent_series=selected;}else free(inventory);inventory=staging;staging=NULL;count=staging_count;total=staging_total;build_groups();source_loaded=1;
        if(operation==2){vod_failed=0;categories_missing=worker_categories_missing;vod_selected_slot=-1;vod_free(&vod_data);vod_data=vod_pending;memset(&vod_pending,0,sizeof vod_pending);merge_mixed_vod();vod_rebuild();}
        if(operation==4){episodes_mode=1;selected=0;go("vod-serie-temporadas");}
        if(committed)dirty=1;
        if(operation==2&&strstr(screen,"vod-"))focus=VOD_ITEM;
      }
      if(phone_stage==1&&operation==1){phone_stage=worker_ok?4:0;phone_fetch=worker_ok;if(!worker_ok)celular_status("{\"s\":\"err\"}");}
      else if(phone_stage==4&&operation==2){phone_stage=worker_ok?2:0;if(worker_ok)phone_status_counts("ok");else celular_status("{\"s\":\"err\"}");}
      if(!worker_ok&&operation!=3&&operation!=5){
        if(operation==2)source_loaded=0; /* Retry a failed catalog on the next open. */
        snprintf(message,sizeof message,"%s",i18n("Falha na conexão"));
      }
    }
    free(staging);staging=NULL;vod_free(&vod_pending);
    if(fallback_fetch){action[0]=0;start(2);}
    if(phone_fetch&&!start(2)){phone_stage=0;celular_status("{\"s\":\"err\"}");}
  }
  if(vod_begin_pending&&!atomic_load(&busy)){vod_begin_pending=0;vod_begin();}
  vod_tick();
  if(!strncmp(screen,"vod-",4)&&strcmp(screen,"vod-titulo")&&strcmp(screen,"vod-serie-temporadas")&&vod_size&&!atomic_load(&busy)&&!atomic_load(&metadata.busy)){
    int ref=vod_order[vod_hero<vod_size?vod_hero:0];
    if(ref>=0){
    if(vod_info_request!=ref||vod_info_revision!=vod_revision){vod_info_request=ref;vod_info_since=SDL_GetTicks();vod_info_revision=vod_revision;}
    else if(vod_info_since&&SDL_GetTicks()-vod_info_since>350){
      G5Item *it=vod_item(ref);vod_info_since=0;
      if(it&&!restricted(it)&&!strcmp(it->source,"xtream")&&xtream_configurado()){
        snprintf(action,sizeof action,it->kind==G5_SERIES?"get_series_info&series_id=%s":"get_vod_info&vod_id=%s",it->id);start(5);action[0]=0;
      }
    }
  }
  }
  if(edit>=0&&!teclado_aberto()){
    int r=teclado_resultado();int field=edit;edit=-1;
    if(r==TECLADO_PRONTO){const char *t=teclado_texto();
      if(field==0){use_saved=0;snprintf(draft.server,sizeof draft.server,"%s",t);}else if(field==1){use_saved=0;snprintf(draft.user,sizeof draft.user,"%s",t);}
      else if(field==2){use_saved=0;snprintf(draft.password,sizeof draft.password,"%s",t);}else if(field==3)snprintf(draft.playlist,sizeof draft.playlist,"%s",t);
      else if(field==4){if(!*t||g5_http_url(t))snprintf(draft.xmltv,sizeof draft.xmltv,"%s",t);}
      else if(field==5){snprintf(query,sizeof query,"%s",t);focus=-1;}
      else if(field==6)snprintf(draft.name,sizeof draft.name,"%s",t);
      else if(field==7){snprintf(pin_value,sizeof pin_value,"%s",t);start(3);}
      else if(field==8&&sources_edit[0]){
        if(*t&&!strpbrk(t,"\t\r\n")){
          LtvPref *pref=ltv_pref(sources_edit,1);if(pref){snprintf(pref->nome,sizeof pref->nome,"%s",t);ltv_pref_gravar();}
        }
      }
      if(field==4&&!strcmp(screen,"fontes-gerenciar")){g5ui_save_source(draft.playlist,draft.xmltv,draft.type,draft.live,draft.movies,draft.series,draft.epg);guia_recarregar(0);}
    }if(field==8)sources_edit[0]=0;teclado_esquecer();
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
        message[0]=0;use_saved=0;action[0]=0;if(file[0]){draft.type=2;snprintf(draft.file,sizeof draft.file,"%s",file);}
        draft.live=draft.movies=draft.series=1;draft.epg=0;phone_stage=1;
        atomic_store(&phone_parts,0);for(int i=0;i<3;i++)atomic_store(&phone_counts[i],0);
        celular_status("{\"s\":\"fetch\"}");
        if(!start(1)){phone_stage=0;celular_status("{\"s\":\"err\"}");}
      }else if(pstep==2&&phone_stage==2){
        draft.live=js_num(data,NULL,"live",1)!=0;draft.movies=js_num(data,NULL,"movies",1)!=0;draft.series=js_num(data,NULL,"series",1)!=0;draft.epg=js_num(data,NULL,"epg",1)!=0;
        js_texto(data,NULL,"xmltv",draft.xmltv,sizeof draft.xmltv);
        if(draft.xmltv[0]&&!g5_http_url(draft.xmltv))celular_status("{\"s\":\"err\"}");
        else if(!draft.live&&!draft.movies&&!draft.series)celular_status("{\"s\":\"err\"}");
        else{
          for(int i=0;i<group_count;i++)group_on[i]=!adult_group(groups[i]);
          // js_array/js_prox only iterate objects and strings, not numeric IDs.
          char off[CEL_CORPO_MAX+1];if(js_bruto(data,NULL,"off",off,sizeof off)&&off[0]=='['){const char *g=off+1;
            for(int n=0;n<G5_GROUPS;n++){while(isspace((unsigned char)*g))g++;char *end;long i=strtol(g,&end,10);if(end==g)break;
              g=end;while(isspace((unsigned char)*g))g++;if(*g!=','&&*g!=']')break;if(i>=0&&i<group_count)group_on[i]=0;if(*g==']')break;g++;
            }
          }
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
  if(!playing||selected<0||selected>=count)return 0;playing=0;G5Item *it=&inventory[selected];if(!item_source_on(it)||restricted(it))return 0;
  int addon=it->source[0]=='a'&&!it->url[0];
  if(addon){const char *base=guia_vod_origem(it->source);if(!base||!base[0])return 0;url[0]=0;}
  else if(it->url[0])snprintf(url,cap,"%s",it->url);else if(!xtream_vod_url(it->id,it->ext[0]?it->ext:"mp4",it->kind==G5_EPISODE,url,(unsigned)cap))return 0;
  memset(item,0,sizeof *item);
  if(addon){snprintf(item->imdb,sizeof item->imdb,"%s",it->id);snprintf(item->origem,sizeof item->origem,"%s",it->source);}
  else snprintf(item->imdb,sizeof item->imdb,"g5:%s:%s",it->source,it->id);
  snprintf(item->titulo,sizeof item->titulo,"%s",it->name);snprintf(item->tipo,sizeof item->tipo,"%s",it->kind==G5_SERIES||it->kind==G5_EPISODE?"series":"movie");snprintf(item->poster,sizeof item->poster,"%s",it->poster);return 1;
}
int g5ui_channel_allowed(const char *id,const char *name,const char *group,const char *source){
  if(source&&source[0]){char key[LTV_CHAVE];ltv_chave_fonte(LTV_ADDON,source,key,sizeof key);if(!ltv_fonte_ligada(key))return 0;}
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
int g5ui_preview_sem_som(void){return gv_is(screen)&&ajustes_guia_previa_sem_som();}
int g5ui_multiview_channel(CatItem *item){
  /* The main slot's open request, edge-triggered by mv_tick's controller.
   * Never reopens the channel already on the air (layout switch, entry with
   * the preview playing): a slot is (re)opened only on a channel change or a
   * real stream error with backoff — periodic refreshes never reach here. */
  if(strncmp(screen,"multiview-",10)||!mv_main_req)return 0;
  mv_main_req=0;
  if(mv_main<0||mv_main>3)return 0;
  G5Item *it=&mv_items[mv_main];if(!it->id[0]||restricted(it))return 0;
  memset(item,0,sizeof *item);
  snprintf(item->imdb,sizeof item->imdb,it->url[0]?"g5live:%s":!strcmp(it->source,"xtream")&&!strchr(it->id,':')?"xtream:%s":"%s",it->id);
  if(!strcmp(player_id_canal(),item->imdb))return 0;
  /* Capacity of the requested channel's own account; the main session it
   * replaces frees its connection when it belongs to that same account. */
  XtreamConta c;int max=-1,used=0;
  if(mv_conta(mv_fonte[mv_main],&c)){max=c.maxConexoes;used=c.conexoes;
    char ch[LTV_CHAVE];
    if(used>0&&player_id_canal()[0]&&guia_fonte_do_canal(player_id_canal(),ch,sizeof ch)&&!strcmp(ch,mv_fonte[mv_main]))used--;}
  if(!g5_live_slots(1,1,max,used)){go("multiview-limite");return 0;}
  snprintf(item->titulo,sizeof item->titulo,"%s",it->name);snprintf(item->tipo,sizeof item->tipo,"channel");return 1;
}
int g5ui_live(const char *id,char *url,size_t cap){
  if(!ltv_fonte_ligada("g5m3u")||strncmp(id,"g5live:",7))return 0;
  for(int i=0;i<mv_count;i++)if(!strcmp(id+7,mv_items[i].id)&&mv_items[i].url[0]&&!restricted(&mv_items[i])){snprintf(url,cap,"%s",mv_items[i].url);return 1;}
  for(int i=0;i<count;i++)if(inventory[i].kind==G5_LIVE&&!strcmp(id+7,inventory[i].id)&&group_allowed(inventory[i].group)&&!restricted(&inventory[i])){snprintf(url,cap,"%s",inventory[i].url);return 1;}return 0;
}
int g5ui_m3u_channels(G5Item *out,int cap){
  context();int n=0;
  if(!ltv_fonte_ligada("g5m3u")||!committed)return 0;
  if(sources_adding){
    if(!sources_saved_draft.live||!sources_saved_draft.type)return 0;
    for(int i=0;i<sources_saved_count&&n<cap;i++)if(sources_saved_inventory[i].kind==G5_LIVE){
      int on=1;for(int g=0;g<sources_saved_groups;g++)if(!strcmp(sources_saved_names[g],sources_saved_inventory[i].group)){on=sources_saved_on[g];break;}
      if(on)out[n++]=sources_saved_inventory[i];
    }
  }else if(draft.live&&draft.type)for(int i=0;i<count&&n<cap;i++)if(inventory[i].kind==G5_LIVE&&group_allowed(inventory[i].group))out[n++]=inventory[i];
  return n;
}
const char *g5ui_xmltv(void){return ltv_fonte_ligada("g5m3u")?(sources_adding?(sources_saved_draft.epg?sources_saved_draft.xmltv:""):(draft.epg?draft.xmltv:"")):"";}
int g5ui_provider_epg(void){return !committed||(sources_adding?sources_saved_draft.epg:draft.epg);}

/* ==========================================================================
 * GUIA TV v5 — A TELA PRINCIPAL DO GUIA (mock v5: guia.html + v4.js "newGuide"
 * + v5.css, aprovado pelo dono). Substitui o desenho do guia de sempre: barra
 * de cima com abas, linha de chips, barra lateral Biblioteca/Categorias, heroi
 * "ASSISTINDO AGORA" com o cartao de preview, canais fixados, grade com regua
 * e "agora", cartoes, esportes e os estados de folha/erro/vazio. Os dados vem
 * do guia de sempre (guia.c) pelas funcoes publicas — nenhuma fixture aqui.
 * ========================================================================== */
/* Desktop: start right of the native sidebar, like Home/Explore; TV keeps 48..1872. */
#define GV_X (layout_desktop_ativo()?ajustes_conteudo_x():48.0f)
#define GV_W (layout_desktop_ativo()?NV_TELA_W-48.0f-GV_X:1824.0f)
#define GV_SB_W 360.0f
#define GV_MX (GV_X+384.0f*gv_side)
#define GV_MW (GV_W-384.0f*gv_side)
#define GV_TOP_Y (layout_desktop_ativo()?64.0f:24.0f)
#define GV_TOP_H 56.0f
#define GV_CHIP_Y (layout_desktop_ativo()?136.0f:96.0f)
#define GV_CHIP_H 56.0f
#define GV_BASE (layout_desktop_ativo()?208.0f:168.0f)        /* topo da sidebar e do heroi (256 com faixa de erro) */
#define GV_HERO_H 360.0f
#define GV_SOURCE_Y 1046.0f
#define GV_SOURCE_H 30.0f
#define GV_GRID_BOTTOM 1044.0f
#define GV_ROW_H 76.0f
#define GV_ROW_GAP 10.0f
#define GV_GRID_X (GV_MX + 324.0f + 8.0f)   /* celulas de programa */
#define GV_GRID_W (GV_END-GV_GRID_X)
#define GV_END (GV_MX + GV_MW)               /* borda direita do conteudo */

/* Canal do guia, copiado para a tela v5 desenhar sem depender dos estaticos. */
#define GV_PROGS 24
typedef struct {
  char id[80], nome[140], logo[480], grupo[64], base[256];
  int num, fav, np;
  EpgProg p[GV_PROGS];
  /* Own titles and synopsis across the two-second cache: EPG publication
   * frees the arenas. */
  char titulo[GV_PROGS][240];
  char descr[GV_PROGS][384];
} GVCh;
/* gv_ch is a window of the filtered list starting at gv_off; gv_lin, gv_top
 * and gv_match count the whole filtered list, so every channel is reachable. */
#define GV_WIN 24
#define GV_FAV "\001fav"     /* gv_grupo: Biblioteca > Favoritos */
static GVCh gv_ch[GV_WIN]; static int gv_n, gv_off, gv_match, gv_top, gv_total_n, gv_favs, gv_cats, gv_cats_total;
static GVCh gv_pins[6]; static int gv_npins;
#define GV_NCAT 30000                  /* same bound as the guide inventory */
#define GV_CAT_WIN 8                   /* rows that fit in the sidebar */
static char gv_cat[GV_NCAT][64]; static int gv_cat_n[GV_NCAT];
static int gv_cat_off;               /* first category row drawn (scroll) */
static char gv_fonte[LTV_CHAVE]; /* Stable source selection; empty means all live. */
static int gv_source_off;
static char gv_grupo[64];   /* filtro de categoria ("Todos ao vivo" quando vazio) */
static int gv_zone, gv_lin, gv_pin, gv_sb, gv_chipi, gv_card, gv_hbt;
static float gv_card_scroll,gv_card_scroll_v;
static int gv_tdesl;        /* grade adiantada, em minutos (passos de 30) */
static time_t gv_tfoc;      /* instante do programa em foco; 0 = agora */
static char gv_numero[8]; static Uint32 gv_numero_desde;
static int gv_epg_extra, gv_verif_estado;   /* folhas: 0 verificando 1 ok 2 falha */

/* Aparelho de estado: quais "screens" sao da tela principal do guia. */
static int gv_is(const char *s){
  static const char *const ss[]={"grade","cartoes","preview","preview-grade","preview-foco",
    "preview-hero-cheio","preview-troca","dock","catchup","categorias","numero","opcoes",
    "canal-opcoes","ajustes","pastas","epg-saude","epg-verificando","epg-adicionado","epg-falha",
    "erros","vazio","fontes-erro","esportes-ligas","esportes-liga","esportes-favoritos",
    "esportes-ajustes","esportes-onde-assistir","esportes-noticias","esportes-hoje",
    "esportes-proximos","esportes-resultados","esportes-modalidades","esportes-catalogo",
    "esportes-visibilidade","esportes-modalidades-config","esportes-artigo","esportes-canais","esportes-times","esportes-evento",NULL};
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
    snprintf(c->descr[k],sizeof c->descr[k],"%s",c->p[k].desc?c->p[k].desc:"");
    c->p[k].desc=c->descr[k];
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
  const char *id,*nome,*logo,*cat,*base;
  for(int i=0;guia_canal_regra_campos(i,NULL,NULL,NULL,NULL,NULL);i++){
    if(!guia_canal_campos(i,&id,&nome,NULL,&cat,&base))continue;
    gv_total_n++;
    const char *catv=(cat&&cat[0])?cat:i18n("Outros");
    {
      int g;for(g=0;g<gv_cats;g++)if(!strcmp(gv_cat[g],catv))break;
      if(g==gv_cats&&g<GV_NCAT){snprintf(gv_cat[g],sizeof gv_cat[g],"%s",catv);gv_cat_n[g]=1;gv_cats=g+1;}
      else if(g<gv_cats)gv_cat_n[g]++;
    }
    int fav=guia_e_favorito(id);
    if(fav){gv_favs++;
      if(gv_npins<6){guia_canal_campos(i,NULL,NULL,&logo,NULL,NULL);gv_copia(&gv_pins[gv_npins++],i,id,nome,logo,cat,base,0);}}
    if(gv_fonte[0]&&strcmp(gv_fonte,guia_canal_fonte(i)))continue;
    if(!gv_filtra(id,catv))continue;
    int k=gv_match++;
    if(k<gv_off||k>=gv_off+GV_WIN)continue;
    guia_canal_campos(i,NULL,NULL,&logo,NULL,NULL);
    gv_copia(&gv_ch[gv_n++],i,id,nome,logo,cat,base,1);
  }
  gv_cats_total=gv_cats;
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
  return !strcmp(s,"numero")||!strcmp(s,"opcoes")||!strcmp(s,"canal-opcoes")
    ||!strcmp(s,"ajustes")||!strcmp(s,"pastas")||!strncmp(s,"epg-",4)||!strcmp(s,"erros")
    ||!strcmp(s,"fontes-erro")||!strcmp(s,"esportes-ajustes")||!strcmp(s,"esportes-onde-assistir");
}
static int gv_overlay(void){return gv_overlay_nome(screen);}
/* Telas cujo D-pad e a ordem espacial dos controles desenhados. */
static int gv_lista(void){return gv_overlay()||!gv_total_n||!strcmp(screen,"vazio")||gv_sports();}

/* --- movimento: o foco desliza, a grade rola, folhas e paginas entram ------- */
static GVGlide gv_gfoco,gv_gcanal,gv_gside;
static float gv_folha_t=1,gv_pag_t=1;
static float gv_rol,gv_rol_v,gv_tmin,gv_tmin_v,gv_hh=GV_HERO_H,gv_hh_v;
static int gv_reg;          /* controles desenhados agora entram no D-pad */
/* Back history: each sheet over a sheet keeps the one below and its focus;
 * the page keeps its focus while a sheet or a v5 page is open. */
typedef struct{char nome[48];int foco;}GVNivel;
static GVNivel gv_pilha[6];static int gv_npilha,gv_pfoco=-1,gv_saida_foco;
static char gv_sai[48];static float gv_sai_t,gv_veu_t;   /* closing sheet, veil */
static void gv_registra(int id,float x,float y,float w,float h){if(gv_reg)reg_focus(id,x,y,w,h);}
/* Focus on sheet and Sports controls glides on one spring, like the pages. */
static GVGlide gv_gfolha;static int gv_gfolha_visto;
static GfxRect gv_foco_folha(GfxRect r){if(!gv_reg)return r;gv_gfolha_visto=1;r.x+=gv_edge_x;r.y+=gv_edge_y;return gv_glide(&gv_gfolha,r);}
static int gv_btn_foc(int id){
  if(!gv_is(screen))return nav>=0&&nav==id-1100;
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
  if(foc){plrui_pilula_foco(gv_foco_folha(r),1);return;}
  if(sel){float ar,ag,ab;ajustes_acento(&ar,&ag,&ab);gfx_cor(r,.5f,ar,ag,ab,.22f);return;}
  if(ajustes_vidro())gfx_cor(r,.5f,1,1,1,T21_CHIP_REST);else gfx_cor(r,.5f,.141f,.149f,.173f,1);
}
static void gv_chip(float x,float y,float w,const char *text,const char *icon,int sel,int foc,int dot,int id){
  GfxRect r={x,y,w,GV_SOURCE_H};float cr,cg,cb,tx=x+8;
  int was_reg=gv_reg;gv_reg=!gv_overlay()&&!sem_foco;
  if(foc)gv_fundo_ctrl(r,1,0);
  gv_tinta(foc,sel,&cr,&cg,&cb);
  gv_reg=was_reg;
  if(!foc&&!sel){cr*=.65f;cg*=.65f;cb*=.65f;}
  if(sel&&!foc)gfx_cor(r,.5f,1,1,1,T21_SEG_SELECTED);
  if(dot){const float c[3][3]={{.29f,.871f,.502f},{.961f,.647f,.141f},{.973f,.439f,.341f}};
    int di=dot==2?1:dot==3?2:0;gfx_cor((GfxRect){tx,y+12,6,6},.5f,c[di][0],c[di][1],c[di][2],1);tx+=14;}
  if(icon&&*icon){gfx_icone((GfxRect){tx,y+7,16,16},icon,cr,cg,cb,1);tx+=22;}
  TxtLinha l=txt_linha_corta_bruta(TXT_ILHA_APOIO,text,(int)(cr*255),(int)(cg*255),(int)(cb*255),255,x+w-tx-8);
  txt_desenhar(l,tx,y+(GV_SOURCE_H-(float)l.h)/2);
  ponteiro_alvo(x,y,w,GV_SOURCE_H,point_focus,point_activate,id,0);
  if(id)gv_registra(id,x,y,w,GV_SOURCE_H);
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
static int gv_sem_veu;      /* the screen draws the one veil under the sheets */
static void gv_dim(float top,float a){
  if(gv_sem_veu)return;
  int options=!strcmp(screen,"ajustes")||(!strcmp(gv_sai,"ajustes")&&gv_sai_t>0);
  gfx_cor((GfxRect){0,top,options?NV_LAYOUT_REAL_W:1920,(options?NV_LAYOUT_REAL_H:1080)-top},0,0,0,0,fminf(a,T21_VEIL)*gv_veu_t);
}

/* --- barra de cima: Guia TV + abas Grade/Cartoes/Esportes/Filmes e series --- */
static void gv_topbar(void){
  gv_label(TXT_ILHA_TITULO,i18n("Guia TV"),GV_X,GV_TOP_Y+4,210,0);
  const char *names[]={"Grade","Cartões","Esportes","Filmes e séries"};
  const char *icons[]={"aj_layout-list","aj_layout-dashboard","aj_activity","aj_film"};
  int g=gv_is(screen),ativa=!g?(!strncmp(screen,"vod-",4)?3:-1):gv_sports()?2:!strcmp(gv_pagina,"cartoes")?1:0;
  float ws[4],tw=8,x=GV_X+240;
  for(int i=0;i<4;i++){ws[i]=(float)txt_linha(TXT_ILHA_CORPO,names[i],243,242,239,255).w+76;tw+=ws[i]+(i?4:0);}
  /* Seletor segmentado (DESIGN 5): trilho 5%, aba ativa 14%, foco no acento. */
  if(ajustes_vidro())gfx_cor((GfxRect){x,GV_TOP_Y,tw,GV_TOP_H},.5f,1,1,1,T21_SEG_TRACK);
  else gfx_cor((GfxRect){x,GV_TOP_Y,tw,GV_TOP_H},.5f,.114f,.118f,.137f,1);
  x+=4;
  for(int i=0;i<4;i++){
    GfxRect r={x,GV_TOP_Y+4,ws[i],GV_TOP_H-8};
    int on=i==ativa,f=g?gv_btn_foc(1100+i):nav==i;
    if(f)plrui_pilula_foco(gv_foco_folha(r),1);
    else if(on){if(ajustes_vidro())gfx_cor(r,.5f,1,1,1,T21_SEG_SELECTED);else gfx_cor(r,.5f,.204f,.212f,.243f,1);}
    float c=f?ajustes_tinta_foco()/255.0f:on?.953f:.62f;
    int quiet_vod=!g&&ativa==3&&on;
    if(!quiet_vod)gfx_icone((GfxRect){x+18,GV_TOP_Y+14,28,28},icons[i],c,c,c,1);
    {TxtLinha l=txt_linha_corta(TXT_ILHA_CORPO,names[i],(int)(c*255),(int)(c*255),(int)(c*255),255,ws[i]-66);
     txt_desenhar(l,quiet_vod?x+(ws[i]-l.w)/2:x+54,GV_TOP_Y+(GV_TOP_H-(float)l.h)/2);}
    ponteiro_alvo(r.x,r.y,r.w,r.h,point_focus,point_activate,1100+i,0);
    gv_registra(1100+i,r.x,r.y,r.w,r.h);
    x+=ws[i]+4;
  }
  gv_button(1104,i18n("Buscar"),"aj_search",GV_X,GV_CHIP_Y,160,0,0);
  gv_button(1105,i18n("Opções"),"aj_sliders-horizontal",GV_X+172,GV_CHIP_Y,170,0,0);
  gv_button(1106,i18n("Multiview"),"aj_layout-dashboard",GV_X+354,GV_CHIP_Y,190,0,0);
  time_t now=time(NULL);struct tm tm;localtime_r(&now,&tm);char clock[16];strftime(clock,sizeof clock,"%H:%M",&tm);
  gv_label(TXT_ILHA_NOME,clock,1826,GV_TOP_Y+12,90,0);
}
/* Compact source footer. IDs 1200/1203..1205 retain their existing actions. */
static int gv_source_index(int slot){return slot==2?0:slot>=6?slot-5:-1;}
static int gv_chip_tem(int i){
  if(i==0||i==3||i==4||i==5)return 1;
  GuiaFonteSaude f;return gv_source_index(i)>=0&&guia_fonte_saude(gv_source_index(i),&f);
}
static int gv_chip_foc(int i){return gv_zone==4&&!gv_overlay()&&gv_chipi==i;}
static GVCh *gv_hero_channel(GVCh *selected);
static float gv_source_chip(int slot,float x,int draw,float budget){
  char text[256];const char *icon=NULL;int sel=0,dot=0;
  int index=gv_source_index(slot);GuiaFonteSaude f;
  if(index>=0&&guia_fonte_saude(index,&f)){
    snprintf(text,sizeof text,"%s  %d",f.nome,f.canais);
    sel=!strcmp(gv_fonte,f.chave);
    dot=!guia_fonte_ligada(index)||f.saude==LTV_SAUDE_FORA?3:
        f.deCache||f.saude!=LTV_SAUDE_OK?2:1;
  }else if(slot==0){snprintf(text,sizeof text,"%s  %d",i18n("Todos ao vivo"),gv_total_n);sel=!gv_fonte[0];}
  else if(slot==3){snprintf(text,sizeof text,"%s",i18n("Rever"));icon="aj_rotate-ccw-clock";sel=gv_past();}
  else if(slot==4){snprintf(text,sizeof text,"%s",i18n("Adicionar fonte"));icon="aj_plus";}
  else{snprintf(text,sizeof text,"EPG");icon="aj_calendar";}
  float w;TXT_INI();w=(float)txt_largura_familia(TXT_ILHA_APOIO,text,txt_fonte_interface());TXT_FIM();
  w=w*gfx_escala_txt()+18+(dot?14:icon?22:0);
  if(layout_desktop_ativo()&&w>budget)w=budget;
  if(draw)gv_chip(x,GV_SOURCE_Y,w,text,icon,sel,gv_chip_foc(slot),dot,1200+slot);
  return w+10;
}
#define GV_META_R (GV_X+GV_W)   /* metadata right edge; TV: 1872 */
/* Chip widths w[0..n) (each with its 10 gap) in a row of avail units after the
 * metadata reserve. Scrolls *off so slot at (-1 = no scroll) fits; returns the
 * end index of the visible run. Desktop scrolls with the same budget it draws;
 * TV keeps its old scroll budget (scroll = 1814, i.e. sum > 1824). */
static int gv_chips_fit(const float *w,int n,int at,int *off,float avail,float scroll){
  if(at>=0){
    if(at<*off)*off=at;
    float s=0;for(int i=*off;i<=at;i++)s+=w[i];
    while(s-10>scroll&&*off<at)s-=w[(*off)++];
  }
  if(*off>=n)*off=0;
  float s=0;int end=*off;
  while(end<n&&s+w[end]-10<=avail)s+=w[end++];
  return end;
}
static void gv_chips_row(void){
  /* Selected source stays visible; long source lists use the existing focus spring. */
  int slots[72],n=0;slots[n++]=0;
  GuiaFonteSaude f;
  for(int i=0;n<69&&guia_fonte_saude(i,&f);i++)slots[n++]=i?5+i:2;
  slots[n++]=3;slots[n++]=4;slots[n++]=5;
  if(gv_fonte[0]){
    int found=0;for(int i=0;guia_fonte_saude(i,&f);i++)if(!strcmp(gv_fonte,f.chave))found=1;
    if(!found){gv_fonte[0]=0;gv_snap_force=1;}
  }
  if(!gv_chip_tem(gv_chipi))gv_chipi=0;
  int at=0;for(int i=0;i<n;i++)if(slots[i]==gv_chipi)at=i;
  GVCh *channel=gv_hero_channel(gv_focus());
  char source[96]="";
  if(channel)for(int i=0;guia_fonte_saude(i,&f);i++)if(!strcmp(f.chave,gv_fonte[0]?gv_fonte:guia_canal_fonte(gv_lin))){snprintf(source,sizeof source,"%s",f.nome);break;}
  float right=GV_META_R;
  float metadata=channel?stream_footer_at(channel->nome,source,"",1,right):0;
  float avail=GV_W-metadata;
  float w[72];for(int i=0;i<n;i++)w[i]=gv_source_chip(slots[i],0,0,avail);
  int end=gv_chips_fit(w,n,gv_zone==4?at:-1,&gv_source_off,avail,layout_desktop_ativo()?avail:1814);
  if(gv_source_off>0)gfx_icone((GfxRect){GV_X-24,GV_SOURCE_Y+5,20,20},"aj_chevron-left",.6f,.6f,.6f,1);
  if(end<n)gfx_icone((GfxRect){right+4,GV_SOURCE_Y+5,20,20},"aj_chevron-right",.6f,.6f,.6f,1);
  float x=GV_X;for(int i=gv_source_off;i<end;i++){gv_source_chip(slots[i],x,1,avail);x+=w[i];}
}

/* --- barra lateral: Biblioteca e Categorias ------------------------------ */
/* Ordem do D-pad na lateral: 0-4 Biblioteca, 6-9 categorias, 10-11 rodape. */
/* Lateral: Favoritos, Todos ao vivo, Suas pastas (2..4) e as categorias
 * (6..13). Vistos recentemente, Adicionados, Ordem e Mostrar/ocultar ficam
 * fora ate o guia ter esses dados. */
#define GV_SB_N (6+GV_NCAT)
/* gv_sb carries the absolute category index (6+i); gv_cat_off picks the
 * eight rows that are drawn. The D-pad scrolls the window under the focus. */
static int gv_sb_tem(int idx){return (idx>=2&&idx<5)||(idx>=6&&idx<6+gv_cats);}
static int gv_sb_ativo(int idx){
  if(idx==2)return !strcmp(gv_grupo,GV_FAV);
  if(idx==3)return !gv_grupo[0];
  if(idx>=6&&idx<GV_SB_N)return idx-6<gv_cats&&!strcmp(gv_grupo,gv_cat[idx-6]);
  return 0;
}
static void gv_cat_scroll(void){
  int sel=gv_sb>=6?gv_sb-6:0;
  if(sel<gv_cat_off)gv_cat_off=sel;
  if(sel>=gv_cat_off+GV_CAT_WIN)gv_cat_off=sel-GV_CAT_WIN+1;
  if(gv_cat_off>gv_cats-GV_CAT_WIN)gv_cat_off=gv_cats>GV_CAT_WIN?gv_cats-GV_CAT_WIN:0;
}
static GfxRect gv_sb_rect(int idx,float base){
  float ycat=base+58+3*54+14;
  if(idx<5)return (GfxRect){GV_X+12,base+58+(idx-2)*54,GV_SB_W-24,48};
  return (GfxRect){GV_X+12,ycat+62+(idx-6-gv_cat_off)*50,GV_SB_W-24,44};
}
static const char *gv_cat_icone(const char *n){
  if(strstr(n,"sport")||strstr(n,"Sport"))return "aj_activity";
  if(strstr(n,"otíc")||strstr(n,"News")||strstr(n,"news"))return "aj_megaphone";
  if(strstr(n,"Film")||strstr(n,"film")||strstr(n,"Cine")||strstr(n,"Movie"))return "aj_film";
  if(strstr(n,"Adult")||strstr(n,"adult")||strstr(n,"XXX"))return "aj_eye-off";
  return "aj_tv-minimal-play";
}
static int gv_sb_foc(void){
  if(gv_side_open)return gv_sb;
  if(gv_sports()&&gv_reg)return focus>=1302&&focus<1305?focus-1300:focus>=1320&&focus<1320+gv_cats?focus-1314:-1;
  return gv_zone==3&&!gv_overlay()?gv_sb:-1;
}
static void gv_side_row(int idx,const char *icon,const char *name,const char *count,int id,float base){
  GfxRect r=gv_sb_rect(idx,base);float y=r.y,h=r.h;
  int f=idx==gv_sb_foc(),on=gv_sb_ativo(idx);
  if(on&&!f){if(ajustes_vidro())gfx_cor(r,18.0f/h,1,1,1,.06f);else gfx_cor(r,18.0f/h,.141f,.149f,.173f,1);}
  float c=f||on?.953f:.72f;
  gfx_icone((GfxRect){GV_X+28,y+(h-24)/2,24,24},icon,c,c,c,1);
  {TxtLinha l=txt_linha_corta_bruta(TXT_ILHA_CORPO,name,(int)(c*255),(int)(c*254),(int)(c*251),255,count?GV_SB_W-64-84:GV_SB_W-64-28);
   txt_desenhar(l,GV_X+64,y+(h-(float)l.h)/2);}
  if(count){TxtLinha l=txt_linha_corta_bruta(TXT_ILHA_CORPO,count,154,156,158,255,64);
   txt_desenhar(l,GV_X+GV_SB_W-28-(float)l.w,y+(h-(float)l.h)/2);}
  if(gv_side_open)ponteiro_alvo(r.x,r.y,r.w,r.h,point_focus,point_activate,id,0);
  gv_registra(id,r.x,r.y,r.w,r.h);
}
static void gv_sidebar(float base){
  if(gv_side<.001f&&!gv_side_open)return;
  float side_alpha=gfx_opacidade_grupo;
  gfx_opacidade_grupo*=gv_side;
  gfx_transformar(0,0,1,-384*(1-gv_side),0);
  panel(GV_X,base,GV_SB_W,1000-base);
  if(!gv_total_n){
    gv_label(TXT_ILHA_NOME,i18n("Biblioteca"),GV_X+26,base+26,300,0);
    txt_bloco(TXT_ILHA_CORPO,i18n("Nenhum canal ainda."),154,156,158,GV_X+26,base+92,GV_SB_W-52,32,1,2);
    txt_bloco(TXT_ILHA_CORPO,i18n("Categorias e pastas aparecem ao adicionar uma lista."),154,156,158,GV_X+26,base+136,GV_SB_W-52,32,1,3);
    gfx_sem_transformar();gfx_opacidade_grupo=side_alpha;return;
  }
  gv_cat_scroll();
  /* Foco de linha (DESIGN 3): superficie um degrau mais clara, deslizando. */
  int si=gv_sb_foc();
  if(si>=0){GfxRect r=gv_sb_rect(si,base);r.x+=gv_edge_x;r.y+=gv_edge_y;plrui_linha_foco(gv_glide(&gv_gside,r),18,1);}
  else gv_gside.ok=0;
  char nb[24];
  gv_label(TXT_ILHA_NOME,i18n("Biblioteca"),GV_X+26,base+20,300,0);
  const char *nomes[]={"Favoritos","Todos ao vivo","Suas pastas"};
  const char *icons[]={"aj_star","aj_tv-minimal-play","aj_folders"};
  for(int i=0;i<3;i++){
    const char *cnt=i==0?(snprintf(nb,sizeof nb,"%d",gv_favs),nb):i==1?(snprintf(nb,sizeof nb,"%d",gv_total_n),nb):NULL;
    gv_side_row(2+i,icons[i],i18n(nomes[i]),cnt,1302+i,base);
  }
  float y=base+58+3*54+14;
  gfx_cor((GfxRect){GV_X+24,y,GV_SB_W-48,1},0,1,1,1,.08f);
  snprintf(nb,sizeof nb,"%d",gv_cats_total);
  gv_label(TXT_ILHA_NOME,i18n("Categorias"),GV_X+26,y+16,220,0);
  {TxtLinha l=txt_linha_bruta(TXT_ILHA_CORPO,nb,154,156,158,255);txt_desenhar(l,GV_X+GV_SB_W-28-(float)l.w,y+18);}
  int shown=gv_cats-gv_cat_off;if(shown>GV_CAT_WIN)shown=GV_CAT_WIN;
  for(int i=0;i<shown;i++){
    int c=i+gv_cat_off;
    snprintf(nb,sizeof nb,"%d",gv_cat_n[c]);
    gv_side_row(6+c,gv_cat_icone(gv_cat[c]),gv_cat[c],nb,1320+c,base);
  }
  /* Scroll hints when the list does not fit in the visible rows. */
  float ycat=base+58+3*54+14;
  if(gv_cat_off>0)gfx_icone((GfxRect){GV_X+GV_SB_W-56,ycat+36,22,22},"aj_chevron-up",.6,.6,.6,1);
  if(gv_cat_off+GV_CAT_WIN<gv_cats)gfx_icone((GfxRect){GV_X+GV_SB_W-56,ycat+62+GV_CAT_WIN*50,22,22},"aj_chevron-down",.6,.6,.6,1);
  gfx_sem_transformar();gfx_opacidade_grupo=side_alpha;
}

/* --- heroi: ASSISTINDO AGORA + cartao de previa --------------------------- */
static float gv_badge(const char *t,float x,float y){
  return t&&*t?badge_desenhar(x,y,t,BADGE_NEUTRO,1)+BADGE_GAP:0;
}
/* Shared URL texture cache and logo treatment; no extra background plate. */
static void gv_avatar(const char *logo,const char *nome,GfxRect r,float tom){
  if(logo&&logo[0]){guia_logo_desenhar(logo,nome,r,r.w,r.h,tom,1);return;}
  gfx_icone(r,"aj_tv-minimal-play",tom,tom,tom,1);
}
static float gv_progresso(const EpgProg *p){
  if(!p||p->fim<=p->ini)return 0;
  return anim_clamp((float)(time(NULL)-p->ini)/(float)(p->fim-p->ini),0,1);
}
static GVCh *gv_canal_cartao(void){return gv_card>=gv_off&&gv_card<gv_off+gv_n?&gv_ch[gv_card-gv_off]:gv_focus();}
/* The now-watching card owns the playing identity, independently of grid focus
 * and filters. Cache the inventory index, validating it after publication. */
static GVCh *gv_hero_channel(GVCh *selected){
  static GVCh watching;static int index=-1;
  if(!guia_preview_no_ar())return selected;
  const char *id=guia_canal_grupo_id(player_id_canal()),*cid,*name,*logo,*group,*base;
  if(!guia_canal_campos(index,&cid,&name,&logo,&group,&base)||strcmp(cid,id)){
    for(index=0;guia_canal_regra_campos(index,NULL,NULL,NULL,NULL,NULL);index++)
      if(guia_canal_campos(index,&cid,&name,&logo,&group,&base)&&!strcmp(cid,id))break;
    if(!guia_canal_campos(index,&cid,&name,&logo,&group,&base))return NULL;
  }
  gv_copia(&watching,index,cid,name,logo,group,base,1);
  return &watching;
}
static void gv_broadcast(GfxRect pv,GVCh *c,EpgProg *p,int full){
  float raio=full?0:20.0f/pv.h;
  int live=guia_preview_no_ar();
  player_mini_guia_preencher(!full&&strcmp(gv_pagina,"cartoes")!=0);
  int troca=!strcmp(screen,"preview-troca");
  // Follow every frame of the native player transition, including its hole.
  if(player_janela_animando(&pv.x,&pv.y,&pv.w,&pv.h))raio=20.0f/pv.h;
  else if(player_mini_no_guia_ativo())player_mini_no_guia(pv.x,pv.y,pv.w,pv.h);
  if(live)gfx_furo_raio(pv,raio);
  else{
    gfx_cor(pv,raio,.02f,.02f,.02f,1);
    float lado=fminf(pv.h*.44f,170);
    GfxRect logo={pv.x+(pv.w-lado)/2,pv.y+(pv.h-lado)/2-(p&&pv.h>200?16:0),lado,lado};
    if(c->logo[0])guia_logo_desenhar(c->logo,c->nome,logo,lado,lado,1,1);
    else{
      char ini[8];gv_iniciais(c->nome,ini,sizeof ini);
      TxtLinha t=txt_linha_bruta(TXT_V2_TIT,ini,243,242,239,255);
      txt_desenhar(t,logo.x+(logo.w-(float)t.w)/2,logo.y+(logo.h-(float)t.h)/2);
    }
  }
  /* veu preto do modo grande/imersivo: emenda o video no heroi (2.0.8) */
  if(full){
    gfx_rect((GfxRect){pv.x,pv.y,pv.w,pv.h},0,GFX_VEU_CSS,0,2,1,0,.045f,.047f,.051f,.9f);
    gfx_rect((GfxRect){pv.x,pv.y+pv.h-120,pv.w,120},0,GFX_VEU_CSS,0,0,1,0,.045f,.047f,.051f,.8f);
  }
  /* Program details belong to the card; keep the video unobstructed. */
  if(live||!strcmp(screen,"preview")||!strcmp(screen,"preview-foco")||troca||full)
    {float bw=badge_desenhar(pv.x+16,pv.y+16,i18n("AO VIVO"),BADGE_REALCE_CHEIO,1);
     if(live&&strcmp(gv_pagina,"cartoes")&&c->logo[0])
       gv_avatar(c->logo,c->nome,(GfxRect){pv.x+28+bw,pv.y+16,40,32},1);}
  if(troca){
    gfx_cor(pv,raio,.067f,.09f,.098f,.55f);
    TxtLinha t=txt_linha(TXT_ILHA_CORPO,"Trocando canal…",243,242,239,255);
    float tx=pv.x+(pv.w-(float)t.w-42)/2;
    gfx_icone((GfxRect){tx,pv.y+pv.h/2-16,32,32},"aj_loader",.85f,.86f,.88f,1);
    txt_desenhar(t,tx+42,pv.y+(pv.h-(float)t.h)/2);
  }
}
/* Grade preview fills the marked right frame; the hero height uses its spring. */
static GfxRect gv_preview_rect(void){
  GfxRect r={GV_MX,!strcmp(screen,"fontes-erro")?256:GV_BASE,GV_MW,gv_hh};
  if(!strcmp(screen,"preview-hero-cheio"))return r;
  if(!strcmp(gv_pagina,"cartoes"))return (GfxRect){r.x+r.w-358,r.y+16,330,176};
  /* Owner's marked frame, scaled to the TV canvas; left card stays at GV_BASE. */
  return (GfxRect){r.x+r.w*.51f,GV_CHIP_Y,r.w*.49f,r.y+r.h+40-GV_CHIP_Y};
}
static void gv_hero(float base){
  float h=gv_hh,k=anim_clamp((h-208)/(GV_HERO_H-208),0,1);
  GfxRect r={GV_MX,base,GV_MW,h};float rr=28.0f/h;
  int full=!strcmp(screen,"preview-hero-cheio");
  GVCh *c=gv_hero_channel(gv_focus());EpgProg current,*p=gv_prog(c);
  if(!c)return;
  if(guia_preview_no_ar())p=guia_programa_agora(c->id,time(NULL),&current)?&current:NULL;
  GfxRect pv=gv_preview_rect();
  gv_broadcast(pv,c,p,full);
  float x=r.x+32,iw=(full?760:pv.x-r.x-64);char buf[200];
  int passado=p&&(p->fim<=time(NULL)||gv_past());
  if(passado)snprintf(buf,sizeof buf,"%s · %s %d",i18n("REVER"),i18n("CANAL"),c->num);
  else snprintf(buf,sizeof buf,"%s · %s %d",i18n(full?"PRÉVIA IMERSIVA":p&&p->ini>time(NULL)?"A SEGUIR":"ASSISTINDO AGORA"),i18n("CANAL"),c->num);
  float og=gfx_opacidade_grupo;
  gfx_opacidade_grupo=og*k;gv_label(TXT_ILHA_SUB,buf,x,r.y+28,iw,1);gfx_opacidade_grupo=og;
  gv_label(TXT_ILHA_TITULO,p?p->titulo:c->nome,x,r.y+anim_mistura(26,62,k),iw,0);
  float y=r.y+anim_mistura(82,124,k);char hi[8],hf[8],meta[180];
  gv_avatar(c->logo,c->nome,(GfxRect){x,y-6,40,40},1);
  float mx0=x+52;
  if(p){gv_hora(p->ini,hi,8);gv_hora(p->fim,hf,8);
    snprintf(meta,sizeof meta,"%s · %d · %s–%s",c->nome,c->num,hi,hf);
    TxtLinha m=txt_linha_corta_bruta(TXT_ILHA_CORPO,meta,154,156,158,255,iw*.6f-52);txt_desenhar(m,mx0,y);
    float mx=mx0+(float)m.w+20;
    meta[0]=0;
    if(p->fim<=time(NULL))snprintf(meta,sizeof meta,i18n("Exibido há %ld min"),(long)(time(NULL)-p->fim)/60);
    else if(p->ini<=time(NULL))snprintf(meta,sizeof meta,i18n("%ld min restantes"),(long)(p->fim-time(NULL))/60);
    if(meta[0]){TxtLinha t=txt_linha_corta_bruta(TXT_ILHA_NOME,meta,243,242,239,255,x+iw-mx);txt_desenhar(t,mx,y-2);mx+=(float)t.w+18;}
  }else{
    snprintf(meta,sizeof meta,"%s · %d",c->nome,c->num);
    gv_label(TXT_ILHA_CORPO,meta,mx0,y,iw-52,1);
  }
  EpgProg next;
  if(guia_programa_proximo(c->id,p?p->ini:time(NULL),0,&next)){
    snprintf(buf,sizeof buf,i18n("A seguir: %s"),next.titulo);gv_label(TXT_ILHA_CORPO,buf,x,r.y+anim_mistura(126,180,k),iw,1);}
  if(p&&p->desc&&p->desc[0])
    gv_label(TXT_ILHA_CORPO,p->desc,x,r.y+anim_mistura(156,216,k),iw,1);
  if(player_mini_no_guia_ativo())player_mini_no_guia(pv.x,pv.y,pv.w,pv.h);
  if(gv_zone==2&&!gv_overlay()){
    focus=1314;plrui_linha_foco(pv,20,1);
    ponteiro_alvo(pv.x,pv.y,pv.w,pv.h,point_focus,point_activate,1314,0);
  }
}

/* --- heroi compacto do modo cartoes (mock v5, preview() do guia.html) ---- */
static void gv_hero_compact(float base){
  GfxRect r={GV_MX,base,GV_MW,208};float rr=28.0f/208;
  GVCh *c=gv_hero_channel(gv_canal_cartao());EpgProg current,*p=gv_prog(c);
  if(!c)return;
  if(guia_preview_no_ar())p=guia_programa_agora(c->id,time(NULL),&current)?&current:NULL;
  GfxRect art=gv_preview_rect();
  gv_broadcast(art,c,p,0);
  if(player_mini_no_guia_ativo())player_mini_no_guia(art.x,art.y,art.w,art.h);
  float iw=art.x-r.x-64;char meta[180],hi[8],hf[8];
  gv_avatar(c->logo,c->nome,(GfxRect){r.x+32,r.y+84,40,40},1);
  gv_label(TXT_ILHA_TITULO,p?p->titulo:c->nome,r.x+32,r.y+28,iw,0);
  if(p){gv_hora(p->ini,hi,8);gv_hora(p->fim,hf,8);
    snprintf(meta,sizeof meta,"%s · %d · %s–%s",c->nome,c->num,hi,hf);
    TxtLinha m=txt_linha_corta_bruta(TXT_ILHA_CORPO,meta,154,156,158,255,iw*.6f-52);txt_desenhar(m,r.x+84,r.y+90);
    float mx=r.x+84+(float)m.w+20;
    long min=p->fim>time(NULL)?(p->fim-time(NULL))/60:0;
    snprintf(meta,sizeof meta,i18n("%ld min restantes"),min);
    TxtLinha t=txt_linha_corta_bruta(TXT_ILHA_NOME,meta,243,242,239,255,r.x+32+iw-mx);txt_desenhar(t,mx,r.y+88);mx+=(float)t.w+18;
  }else{snprintf(meta,sizeof meta,"%s · %d",c->nome,c->num);gv_label(TXT_ILHA_CORPO,meta,r.x+84,r.y+90,iw-52,1);}
  EpgProg next;
  if(guia_programa_proximo(c->id,time(NULL),0,&next)){
    gv_hora(next.ini,hi,sizeof hi);
    snprintf(meta,sizeof meta,"%s · %s · %s",i18n("A SEGUIR"),hi,next.titulo);
    gv_label(TXT_ILHA_CORPO,meta,r.x+32,r.y+140,iw,1);
  }else gv_label(TXT_ILHA_CORPO,i18n("Sem descrição no guia."),r.x+32,r.y+140,iw,1);
  if(p&&p->desc&&p->desc[0])
    gv_label(TXT_ILHA_CORPO,p->desc,r.x+32,r.y+172,iw,1);
}
/* --- canais fixados (os favoritos, na ordem do guia) ---------------------- */
static GVCh *gv_pin_ch(int i){return i>=0&&i<gv_npins?&gv_pins[i]:NULL;}
static void gv_dock(float base,float heroh){
  if(!gv_npins)return;
  float y=base+heroh+20;
  gv_label(TXT_ILHA_NOME,i18n("Canais fixados"),GV_MX,y,300,0);
  y+=44;
  for(int i=0;i<gv_npins;i++){
    GVCh *c=&gv_pins[i];
    float pinw=(GV_MW-5*12)/6;
    GfxRect r={GV_MX+i*(pinw+12),y,pinw,66};
    int f=gv_zone==1&&gv_pin==i&&!gv_overlay();float cr,cg,cb;
    if(f)plrui_pilula_foco(r,1);
    else if(ajustes_vidro())gfx_cor(r,.5f,1,1,1,.06f);else gfx_cor(r,.5f,.114f,.118f,.137f,1);
    gv_tinta(f,0,&cr,&cg,&cb);
    gv_avatar(c->logo,c->nome,(GfxRect){r.x+12,y+12,42,42},f?cr:1);
    TxtLinha l=txt_linha_corta_bruta(TXT_ILHA_CORPO,c->nome,(int)(cr*255),(int)(cg*255),(int)(cb*255),255,pinw-76);
    txt_desenhar(l,r.x+66,y+(66-(float)l.h)/2);
    ponteiro_alvo(r.x,r.y,r.w,r.h,point_focus,point_activate,1400+i,0);
  }
}
static GVCh *gv_canal_focado(void){
  if(gv_zone==1)return gv_pin_ch(gv_pin);
  if(gv_zone==6)return gv_card>=gv_off&&gv_card<gv_off+gv_n?&gv_ch[gv_card-gv_off]:NULL;
  return gv_focus();
}
static GVCh *gv_canal_focado_ou_primeiro(void){GVCh *c=gv_canal_focado();return c?c:gv_n?&gv_ch[0]:NULL;}
/* --- grade com regua e "agora" --------------------------------------------- */
static float gv_rows_y(float base){return base+gv_hh+20+(gv_npins?108+16:0)+38;}
static int gv_vis(float base){
  int n=(int)((GV_GRID_BOTTOM-gv_rows_y(base)+GV_ROW_GAP)/(GV_ROW_H+GV_ROW_GAP));return n<1?1:n;
}
static float gv_px(time_t t){
  float t0=(float)gv_tbase()+gv_tmin*60.0f;
  return GV_GRID_X+((float)t-t0)/60.0f*(GV_GRID_W/120.0f);
}
static GfxRect gv_cel(const EpgProg *p,float y){
  float l=fmaxf(gv_px(p->ini),GV_GRID_X)+8,r=fminf(gv_px(p->fim),GV_END+8);
  return (GfxRect){l,y,r-l,GV_ROW_H};
}
/* Duas passadas: fundos (pass 0) e textos (pass 1), com o foco deslizante no
 * meio, para o texto focado ficar por cima da pilula. */
static void gv_row(float y,GVCh *c,int foc,int pass){
  GfxRect cr={GV_MX,y,320,GV_ROW_H};float rr=20.0f/GV_ROW_H;
  time_t now=time(NULL);EpgProg *pf=foc&&gv_zone==0?gv_prog(c):NULL;
  if(!pass){
    if(ajustes_vidro())gfx_cor(cr,rr,1,1,1,.04f);else gfx_cor(cr,rr,.071f,.075f,.086f,1);
    if(!c->np){GfxRect pr={GV_GRID_X+8,y,GV_END-GV_GRID_X-8,GV_ROW_H};gfx_cor(pr,rr,.102f,.106f,.121f,1);}
    for(int j=0;j<c->np;j++){
      EpgProg *p=&c->p[j];GfxRect pr=gv_cel(p,y);
      if(pr.w<=8||pr.x>=GV_END)continue;
      pr.w-=8;
      int live=p->ini<=now&&now<p->fim,past=p->fim<=now;
      if(past)gfx_cor(pr,rr,.082f,.09f,.106f,1);
      else if(live)gfx_cor(pr,rr,.133f,.145f,.169f,1);
      else gfx_cor(pr,rr,.102f,.106f,.121f,1);
      if(live)gfx_cor((GfxRect){pr.x+14,pr.y+pr.h-6,(pr.w-28)*gv_progresso(p),3},.5f,.953f,.949f,.937f,.55f);
    }
    return;
  }
  gv_avatar(c->logo,c->nome,(GfxRect){cr.x+14,y+12,52,52},1);
  char num[64];gv_num_quali(c,num,sizeof num);
  gv_label(TXT_ILHA_CORPO,c->nome,cr.x+80,y+12,c->fav?200:228,0);
  gv_label(TXT_ILHA_SUB,num,cr.x+80,y+42,228,1);
  if(c->fav)gfx_icone((GfxRect){cr.x+284,y+12,22,22},"aj_star",.949f,.765f,.282f,1);
  if(!c->np){
    gv_label(TXT_ILHA_CORPO,i18n("Sem programação"),GV_GRID_X+28,y+12,500,1);
  }
  for(int j=0;j<c->np;j++){
    EpgProg *p=&c->p[j];GfxRect pr=gv_cel(p,y);
    if(pr.w<=8||pr.x>=GV_END)continue;
    pr.w-=8;
    if(pr.w<60)continue;   /* a sliver at the edge: no lone ellipsis */
    int f=pf==p&&!gv_overlay(),past=p->fim<=now;
    int tc=past&&!f?150:243;
    float bell=guia_lembrete_achar(c->id,p->ini,p->titulo)>=0&&pr.w>90?30:0;
    {TxtLinha l=txt_linha_corta_bruta(TXT_ILHA_CORPO,p->titulo,tc,tc,tc,255,pr.w-40-bell);
     txt_desenhar(l,pr.x+20,y+12);}
    char hi[8],hf[8],q[32];gv_hora(p->ini,hi,8);gv_hora(p->fim,hf,8);snprintf(q,sizeof q,"%s–%s",hi,hf);
    {int t2=f?190:past?120:170;TxtLinha l=txt_linha_corta_bruta(TXT_ILHA_SUB,q,t2,t2,t2,255,pr.w-40);
     txt_desenhar(l,pr.x+20,y+42);}
    if(bell){float b=.85f;gfx_icone((GfxRect){pr.x+pr.w-38,y+14,22,22},"aj_bell",b,b,b,1);}
  }
}
static void gv_grid(float base){
  float y=gv_rows_y(base)-38;
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
  int pillv=nx>=GV_GRID_X&&nx<GV_END-20;
  GfxRecorteAnt ant=gfx_recorte_dentro(GV_GRID_X,y-4,GV_END-GV_GRID_X,44);
  time_t t0=gv_tbase()+(time_t)floorf(gv_tmin)*60;t0-=t0%1800;
  for(time_t t=t0;gv_px(t)<GV_END;t+=1800){
    float x=gv_px(t);char hi[8];gv_hora(t,hi,8);
    TxtLinha l=txt_linha_bruta(TXT_ILHA_SUB,hi,154,156,158,255);
    if(pillv&&x+(float)l.w>pill.x-8&&x<pill.x+pill.w+8)continue;
    if(x>=GV_GRID_X-1)txt_desenhar(l,x,y+4);
  }
  if(pillv){gfx_cor(pill,.5f,.953f,.949f,.937f,1);txt_desenhar(al,pill.x+14,pill.y+(36-(float)al.h)/2);}
  gfx_recorte_voltar(ant);
  float y0=y+38;
  float bottom=GV_GRID_BOTTOM;
  ant=gfx_recorte_dentro(GV_MX,y0-4,GV_MW,bottom-(y0-4));
  int i0=(int)floorf(gv_rol),i1=i0+vis+1;
  for(int pass=0;pass<2;pass++){
    for(int i=i0;i<=i1;i++){
      if(i<gv_off||i>=gv_off+gv_n)continue;
      float ry=y0+((float)i-gv_rol)*(GV_ROW_H+GV_ROW_GAP);
      /* Parado, so linhas inteiras; rolando, a que entra desliza pela borda. */
      int rolando=fabsf(gv_rol-(float)gv_top)>.02f;
      if(ry+GV_ROW_H<y0-4||ry>bottom||(!rolando&&ry+GV_ROW_H>bottom))continue;
      gv_row(ry,&gv_ch[i-gv_off],i==gv_lin,pass);
      if(!pass)ponteiro_alvo(GV_MX,ry,GV_MW,GV_ROW_H,point_focus,point_activate,1500+i,0);
    }
    if(pass)continue;
    /* Foco (DESIGN 3): canal e programa ganham a superficie um degrau mais clara. */
    GVCh *fc=gv_focus();
    if(gv_zone==0&&fc&&!gv_overlay()){
      float fy=y0+((float)gv_lin-gv_top)*(GV_ROW_H+GV_ROW_GAP);
      plrui_linha_foco(gv_glide(&gv_gcanal,(GfxRect){GV_MX+gv_edge_x,fy+gv_edge_y,320,GV_ROW_H}),20,1);
      EpgProg *p=gv_prog(fc);
      if(p){GfxRect pr=gv_cel(p,fy);pr.w-=8;if(pr.x<GV_END&&pr.w>8)plrui_linha_foco(gv_glide(&gv_gfoco,pr),20,1);}
      else gv_gfoco.ok=0;
    }else{gv_gcanal.ok=0;gv_gfoco.ok=0;}
  }
  if(pillv){
    float last=y0+(fminf((float)vis,(float)(gv_match-gv_top)))*(GV_ROW_H+GV_ROW_GAP)-GV_ROW_GAP;
    gfx_cor((GfxRect){nx-1,y0-4,3,fminf(last,GV_GRID_BOTTOM)-(y0-4)},0,.953f,.949f,.937f,.85f);
  }
  gfx_recorte_voltar(ant);
}

/* --- modo cartoes ---------------------------------------------------------- */
static void gv_cards(float base){
  float y=base+208+20+(gv_npins?108+16:0);
  int target=(gv_card/5>0?gv_card/5-1:0)*5;
  gv_card_scroll=anim_mola2(&gv_card_scroll_v,gv_card_scroll,(float)target,gv_dt,T21_SCROLL);
  GfxRecorteAnt clip=gfx_recorte_dentro(GV_MX-8,y-8,GV_MW+16,GV_GRID_BOTTOM-y);
  for(int k=0;k<gv_n;k++){
    int i=gv_off+k;GVCh *c=&gv_ch[k];float cw=(GV_MW-4*16)/5,x=GV_MX+(i%5)*(cw+16);
    GfxRect r={x,y+((float)(i/5)-gv_card_scroll/5)*252,cw,236};float rr=20.0f/236;
    if(r.y+r.h<y||r.y>GV_GRID_BOTTOM)continue;
    int foc=gv_zone==6&&gv_card==i&&!gv_overlay();
    if(foc){r.x+=gv_edge_x;r.y+=gv_edge_y;plrui_linha_foco(r,20,1);}
    else if(ajustes_vidro())gfx_cor(r,rr,1,1,1,.05f);else gfx_cor(r,rr,.086f,.094f,.11f,1);
    GfxRect tile={r.x+12,r.y+12,r.w-24,112};
    gv_avatar(c->logo,c->nome,(GfxRect){tile.x+(tile.w-112)/2,tile.y,112,112},1);
    const char *qs=gv_quali(c->nome);
    if(qs[0]){TxtLinha q=txt_linha_bruta(TXT_ILHA_SUB,qs,243,242,239,255);
      gfx_cor((GfxRect){r.x+20,r.y+22,(float)q.w+20,32},.5f,.051f,.051f,.051f,.7f);
      txt_desenhar(q,r.x+30,r.y+22+(32-(float)q.h)/2);}
    if(c->fav)gfx_icone((GfxRect){r.x+r.w-44,r.y+26,22,22},"aj_star",.949f,.765f,.282f,1);
    gv_label(TXT_ILHA_NOME,c->nome,r.x+18,r.y+130,r.w-36,0);
    EpgProg *p=gv_prog(c);
    if(p&&p->fim>time(NULL)){
      gv_label(TXT_ILHA_CORPO,p->titulo,r.x+18,r.y+162,r.w-36,0);
      char m2[140];
      if(p->ini>time(NULL)){char hi[8];gv_hora(p->ini,hi,8);snprintf(m2,sizeof m2,"%s",hi);}
      else snprintf(m2,sizeof m2,i18n("%ld min restantes"),(long)(p->fim-time(NULL))/60);
      gv_label(TXT_ILHA_SUB,m2,r.x+18,r.y+194,r.w-36,1);
      GfxRect bar={r.x+18,r.y+224,r.w-36,4};gfx_cor(bar,.5f,1,1,1,.16f);
      gfx_cor((GfxRect){bar.x,bar.y,bar.w*gv_progresso(p),4},.5f,.953f,.949f,.937f,1);
      int pi=(int)(p-c->p);EpgProg *nx=pi+1<c->np?&c->p[pi+1]:NULL;
      (void)nx;
    }else{
      gv_label(TXT_ILHA_CORPO,i18n("Sem programação"),r.x+18,r.y+162,r.w-36,1);
    }
    if(r.y+r.h<=GV_GRID_BOTTOM)ponteiro_alvo(r.x,r.y,r.w,r.h,point_focus,point_activate,1600+i,0);
  }
  gfx_recorte_voltar(clip);
}
/* Empty guide: configured sources must remain reachable without setup. */
static int gv_empty_offset;
static void gv_vazio(void){
  GuiaFonteSaude source;
  if(guia_fonte_saude(0,&source)){
    int on=0,off=0;
    for(int i=0;guia_fonte_saude(i,&source);i++)if(guia_fonte_ligada(i))on++;else off++;
    gv_label(TXT_TITULO2,i18n(on?"Suas fontes estão indisponíveis":"Suas fontes estão desligadas"),GV_MX+10,GV_BASE+28,1400,0);
    gv_label(TXT_ILHA_CORPO,i18n(on?"Tente atualizar suas fontes ou gerencie sua configuração.":"Ligue uma fonte para voltar a assistir aos seus canais."),GV_MX+10,GV_BASE+94,1400,1);
    if(gv_empty_offset>=off)gv_empty_offset=0;
    int row=0;
    for(int i=0,n=0;guia_fonte_saude(i,&source);i++){
      if(guia_fonte_ligada(i))continue;
      if(n++<gv_empty_offset)continue;
      if(row==4)break;
      if(!row&&focus<1700&&gv_zone!=5&&gv_zone!=4)focus=4000+i;
      float y=GV_BASE+160+row++*100;
      gv_label(TXT_ILHA_NOME,source.nome,GV_MX+32,y+17,850,1);
      gv_button(4000+i,i18n("Ligar fonte"),"aj_power",GV_MX+1000,y,260,0,0);
    }
    if(!row&&focus<1700&&gv_zone!=5&&gv_zone!=4)focus=1799;
    float y=GV_BASE+600;
    gv_button(1799,i18n("Gerenciar fontes"),"aj_sliders-horizontal",GV_MX+10,y,340,0,0);
    if(on)gv_button(1710,i18n("Tentar novamente"),"aj_rotate-cw",GV_MX+370,y,300,0,0);
    if(gv_empty_offset)gv_button(1797,i18n("Anterior"),"aj_chevron-left",GV_MX+700,y,240,0,0);
    if(gv_empty_offset+4<off)gv_button(1798,i18n("Próximo"),"aj_chevron-right",GV_MX+960,y,240,0,0);
    return;
  }
  gv_label(TXT_TITULO2,i18n("Adicione sua primeira fonte"),GV_MX+10,GV_BASE+28,1000,0);
  gv_label(TXT_ILHA_CORPO,i18n("O Nuvio não inclui canais. Use as listas que o seu provedor entregou."),GV_MX+10,GV_BASE+94,1300,1);
  const char *ts[]={"Lista M3U","Acesso Xtream","Guia XMLTV"};
  const char *ds[]={"Um endereço .m3u ou .m3u8 com a sua lista de canais.","Servidor, usuário e senha do seu provedor.","Programação dos canais que você já tem."};
  const char *bs[]={"Digitar URL","Informar acesso","Digitar URL"};
  const char *icons[]={"aj_link","aj_key","aj_file-text"};
  float cw=fminf(464,(GV_MW-48)/3);   /* TV: always 464 */
  for(int i=0;i<3;i++){
    float x=GV_MX+i*(cw+24);GfxRect r={x,GV_BASE+150,cw,384};
    if(ajustes_vidro())gfx_cor(r,28.0f/384,1,1,1,.04f);else gfx_cor(r,28.0f/384,.086f,.094f,.11f,1);
    gfx_cor((GfxRect){x+32,GV_BASE+182,72,72},20.0f/72,.141f,.149f,.173f,1);
    gfx_icone((GfxRect){x+50,GV_BASE+200,36,36},icons[i],.95f,.95f,.95f,1);
    gv_label(TXT_TITULO2,i18n(ts[i]),x+32,GV_BASE+280,cw-64,0);
    txt_bloco(TXT_ILHA_CORPO,i18n(ds[i]),154,156,158,x+32,GV_BASE+344,cw-64,32,1,3);
    gv_button(1700+i,i18n(bs[i]),NULL,x+32,GV_BASE+466,fminf(320,cw-64),0,0);
  }
  if(!layout_desktop_ativo()) gfx_icone((GfxRect){GV_MX,GV_BASE+600,28,28},"aj_smartphone",.7f,.71f,.72f,1);
  if(!layout_desktop_ativo()) gv_label(TXT_ILHA_CORPO,i18n("Digitar um endereço longo é lento: escolha “Digitar no celular” na próxima tela."),GV_MX+40,GV_BASE+606,1200,1);
}
/* --- faixa de erro das fontes (fontes-erro) -------------------------------- */
/* The first source that failed or fell back to its cached list. */
static int gv_fonte_falha(GuiaFonteSaude *f){
  for(int i=0;guia_fonte_saude(i,f);i++)if(f->saude==LTV_SAUDE_FORA||f->deCache)return 1;
  return 0;
}
static void gv_strip(void){
  GuiaFonteSaude f;if(!gv_fonte_falha(&f))return;
  GfxRect r={GV_X,168,GV_MW+GV_SB_W+24,72};gfx_cor(r,24.0f/72,.165f,.102f,.102f,1);
  gfx_icone((GfxRect){r.x+24,186,32,32},"aj_triangle-alert",.95f,.6f,.43f,1);
  /* title over status, buttons right-aligned inside the strip */
  float b2=r.x+r.w-16-250,b1=b2-12-290;
  char t[160],n[16],c[96];snprintf(t,sizeof t,i18n("%s não respondeu da última vez"),f.nome);
  gv_label(TXT_ILHA_NOME,t,r.x+72,176,b1-r.x-96,0);
  snprintf(n,sizeof n,"%d",f.canais);snprintf(c,sizeof c,i18n("%s canais disponíveis"),n);
  snprintf(t,sizeof t,"%s · %s",i18n("lista guardada"),c);
  if(f.atualizada){time_t a=(time_t)f.atualizada;struct tm m;localtime_r(&a,&m);size_t k=strlen(t);strftime(t+k,sizeof t-k," · %d/%m %H:%M",&m);}
  gv_label(TXT_ILHA_SUB,t,r.x+72,206,b1-r.x-96,1);
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
  if(f)plrui_linha_foco(gv_foco_folha(r),20,1);
  else if(ajustes_vidro())gfx_cor(r,rr,1,1,1,.05f);else gfx_cor(r,rr,.114f,.118f,.137f,1);
  gfx_icone((GfxRect){x+20,y+(h-25)/2,25,25},icon,.953f,.949f,.937f,1);
  if(sub){gv_label(TXT_ILHA_NOME,name,x+60,y+(h-54)/2,w-80,0);gv_label(TXT_ILHA_SUB,sub,x+60,y+(h-54)/2+30,w-80,1);}
  else{TxtLinha l=txt_linha_corta_bruta(TXT_ILHA_NOME,name,243,242,239,255,w-80);txt_desenhar(l,x+60,y+(h-(float)l.h)/2);}
  ponteiro_alvo(x,y,w,h,point_focus,point_activate,id,0);
  gv_registra(id,x,y,w,h);
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
}
static void gv_opcoes_sheet(void){
  gv_dim(0,.42f);
  float x=gv_sheet_base(660,196,600,420);
  GVCh *c=gv_canal_focado_ou_primeiro();
  if(!c)return;
  gv_avatar(c->logo,c->nome,(GfxRect){x,228,60,60},1);
  {char tt[180];snprintf(tt,sizeof tt,"%d %s",c->num,c->nome);gv_label(TXT_TITULO2,tt,x+80,226,440,0);}
  char meta[96],nq[48];gv_num_quali(c,nq,sizeof nq);snprintf(meta,sizeof meta,"%s · %s",c->grupo[0]?c->grupo:"IPTV",nq);
  gv_label(TXT_ILHA_SUB,meta,x+80,274,440,1);
  gv_opt(1910,"aj_star",i18n(c->fav?"Remover dos favoritos":"Adicionar aos favoritos"),NULL,x,320,536,76);
  {EpgProg *p=gv_prog(c);
   gv_opt(1911,"aj_bell",i18n("Criar lembrete"),p?p->titulo:i18n("Sem programação"),x,404,536,76);}
  gv_opt(1912,"aj_calendar",i18n("Vincular guia manualmente"),NULL,x,488,536,76);
}
static void gv_canal_opcoes_sheet(void){
  gv_dim(0,.42f);
  float x=gv_sheet_base(360,190,1440,680);
  GVCh *c=gv_canal_focado_ou_primeiro();
  if(!c)return;
  {char titulo[160];snprintf(titulo,sizeof titulo,"%d %s",c->num,c->nome);
   gv_sheet_head(x,222,titulo,i18n("Opções"));}
  /* no channel-hide API yet: no "Ocultar canal" row */
  const char *ops[]={"Adicionar aos favoritos","Editar nome do canal","Vincular canal EPG","Buscar logo do canal","Adicionar à pasta","Criar lembrete"};
  const char *oicons[]={"aj_star","aj_pencil","aj_calendar","aj_search","aj_folders","aj_bell"};
  static const int oids[]={1920,1922,1923,1924,1925,1926};
  for(int i=0;i<6;i++)gv_opt(oids[i],oicons[i],i18n(i==0&&c->fav?"Remover dos favoritos":ops[i]),NULL,x,318+i*66,560,58);
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
/* Options owns its list navigation and nested pickers; the guide stays behind it. */
static int gv_aj_sub,gv_aj_source_off;
static float gv_aj_scroll;
static GfxRect gv_aj_body;
#ifdef NV_TOUCH_UI
static ToqueRolagem gv_aj_touch;
static int gv_aj_rolar(const PonteiroRolagem *e){return !strcmp(screen,"ajustes")&&toquerol_evento(&gv_aj_touch,e);}
#endif
static float gv_aj_scale(void){return layout_modo_mobile()?1.6f:1;}
static void gv_aj_target(int id,GfxRect r){
  if(!gv_reg)return;
  if(id!=1936){
    float bottom=fminf(r.y+r.h,gv_aj_body.y+gv_aj_body.h);
    r.y=fmaxf(r.y,gv_aj_body.y);r.h=bottom-r.y;if(r.h<=0)return;
  }
  float k=gv_aj_scale();r.x*=k;r.y*=k;r.w*=k;r.h*=k;
  ponteiro_alvo(r.x,r.y,r.w,r.h,point_focus,point_activate,id,0);gv_registra(id,r.x,r.y,r.w,r.h);
}
static const int gv_aj_ids[]={1936,1940,1942,1944,1945,1947,1948,1949,1937,1938,1939};
static const char *gv_aj_title(int sub){
  return i18n(sub==1942?"Tamanho da prévia":sub==1944?"Quando iniciar a prévia":sub==1947?"Alcance do guia":sub==1937?"Gerenciar fontes":sub==1949?"Saúde do EPG":"Opções da grade");
}
static int gv_aj_selected(int sub){
  return sub==1942?(ajustes_guia_previa_grande()?1942:1941):sub==1944?(ajustes_guia_previa_destacar()?1943:1944):sub==1947?(ajustes_guia_dias()==7?1958:ajustes_guia_dias()==3?1947:1946):sub==1937?1955:1975;
}
static void gv_aj_back(void){
  focus=gv_aj_sub;gv_aj_sub=0;gv_gfolha.ok=0;
#ifdef NV_TOUCH_UI
  toquerol_limpar(&gv_aj_touch);
#endif
}
static void gv_aj_open(int id){
  gv_aj_sub=id;gv_aj_source_off=0;gv_aj_scroll=0;focus=gv_aj_selected(id);
#ifdef NV_TOUCH_UI
  toquerol_limpar(&gv_aj_touch);
#endif
  GuiaFonteSaude source;if((id==1937||id==1949)&&guia_fonte_saude(0,&source))focus=1985;
  gv_gfolha.ok=0;
}
static void gv_aj_row(int id,const char *icon,const char *title,const char *caption,const char *value,int toggle,int selected,float x,float y,float w){
  GfxRect r={x,y,w,64};int f=gv_btn_foc(id);
  if(f)plrui_linha_foco(gv_foco_folha(r),T21_R_ROW,1);
  gfx_icone((GfxRect){x+18,y+19,26,26},icon,.953f,.949f,.937f,f?1:.65f);
  float tail=toggle>=0?78:value?fminf(w*.36f,280):48;
  float tw=w-92-tail;
  TxtLinha t=txt_linha_corta_bruta(TXT_ILHA_NOME,title,243,242,239,255,tw);
  if(layout_modo_mobile()&&!caption&&txt_linha_bruta(TXT_ILHA_NOME,title,243,242,239,255).w>tw)
    txt_bloco_bruto(TXT_ILHA_NOME,title,243,242,239,x+60,y+6,tw,26,1,2);
  else txt_desenhar(t,x+60,y+(caption?9:(64-t.h)/2));
  if(caption)gv_label(TXT_ILHA_APOIO,caption,x+60,y+38,tw,1);
  if(toggle>=0){
    static float on[9],velocity[9];static int ready;
    int i=id==1940?0:id==1945?1:id>=1985&&id<1991?3+id-1985:2;
    if(!ready){on[0]=guia_preview_ligado();on[1]=ajustes_guia_previa_sem_som();on[2]=ajustes_vidro();ready=1;}
    if(gv_reg)on[i]=anim_mola2(&velocity[i],on[i],toggle,gv_dt,T21_FOCO);
    ajustes_ui_interruptor(x+w-20,y+32,on[i],1);
  }else{
    if(value){TxtLinha v=txt_linha_corta_bruta(TXT_ILHA_SUB,value,154,156,158,255,tail-34);txt_desenhar(v,x+w-48-v.w,y+(64-v.h)/2);}
    if(selected||!gv_aj_sub||gv_aj_sub==1937||gv_aj_sub==1949)
      gfx_icone((GfxRect){x+w-36,y+22,20,20},selected?"aj_check":"aj_chevron-right",.75f,.76f,.78f,1);
  }
  gv_aj_target(id,r);
}
static void gv_ajustes_sheet(void){
  gv_dim(0,.42f);
  float k=gv_aj_scale(),sw=NV_LAYOUT_REAL_W/k,sh=NV_LAYOUT_REAL_H/k;
  float w=fminf(1040,sw-(nv_inset_e+nv_inset_d)/k-48);
  float h=fminf(1000,sh-(nv_inset_t+nv_inset_b)/k-48);
  float x=(sw+(nv_inset_e-nv_inset_d)/k-w)/2,y=(sh+(nv_inset_t-nv_inset_b)/k-h)/2;
  t21_ilha_folha((GfxRect){x,y,w,h},T21_R_MODAL,1);
  x+=32;w-=64;
  plrui_kicker(i18n("Guia TV"),x,y+30,154,156,158,1);
  gv_label(TXT_ILHA_TITULO,gv_aj_title(gv_aj_sub),x,y+64,w-80,0);
  /* Header disc is the only action above the list: Up reaches it, Back closes. */
  GfxRect back={x+w-56,y+30,56,56};
  if(gv_btn_foc(1936))plrui_pilula_foco(gv_foco_folha(back),1);else plrui_botao_repouso(back,1);
  float ink=gv_btn_foc(1936)?ajustes_tinta_foco()/255.0f:.953f;
  gfx_icone((GfxRect){back.x+16,back.y+16,24,24},gv_aj_sub?"aj_arrow-left":"aj_x",ink,ink,ink,1);
  if(gv_reg)ponteiro_camada();
  gv_aj_target(1936,back);
  gv_aj_body=(GfxRect){x,y+124,w,h-144};
  float end=gv_aj_sub?(gv_aj_sub==1937||gv_aj_sub==1949?852:410):980;
  float limit=fmaxf(0,end-124-gv_aj_body.h),fy=-1;
  if(!gv_aj_sub){
    const float rows[]={0,162,232,302,372,504,574,706,776,846,916};
    for(int i=1;i<11;i++)if(gv_aj_ids[i]==focus)fy=rows[i];
  }else if(gv_aj_sub==1942||gv_aj_sub==1944||gv_aj_sub==1947){
    int first=gv_aj_sub==1942?1941:gv_aj_sub==1944?1943:1946;
    if(focus==first||focus==first+1)fy=194+(focus-first)*76;
  }else if(focus>=1985&&focus<1991)fy=194+(focus-1985)*76;
  else if(focus==1955||focus==1975)fy=688;
  else if(focus==1956||focus==1957)fy=788;
  int reveal=1;
#ifdef NV_TOUCH_UI
  toquerol_vincular(&gv_aj_touch,gv_aj_body,k,0,limit,1,&gv_aj_scroll);
  reveal=!toquerol_livre(&gv_aj_touch);
  if(gv_reg)ponteiro_rolagem(gv_aj_rolar);
#endif
  if(reveal&&fy>=0){
    gv_aj_scroll=fmaxf(gv_aj_scroll,fy+64-124-gv_aj_body.h);
    gv_aj_scroll=fminf(gv_aj_scroll,fy-124);
  }
  gv_aj_scroll=fmaxf(0,fminf(gv_aj_scroll,limit));
  GfxRecorteAnt clip=gfx_recorte_dentro(x,gv_aj_body.y,w,gv_aj_body.h);
  y-=gv_aj_scroll;
  if(gv_aj_sub){
    gv_label(TXT_ILHA_SUB,i18n("Opções da grade"),x,y+124,w,1);
    gfx_cor((GfxRect){x,y+165,w,1},0,1,1,1,.08f);
    int sub=gv_aj_sub,sel=gv_aj_selected(sub);
    if(sub==1942||sub==1944||sub==1947){
      const char *a=sub==1942?"Cartão":sub==1944?"Ao destacar":"Um dia";
      const char *b=sub==1942?"Grande":sub==1944?"Ao selecionar":"Três dias";
      const char *c=sub==1947?"Sete dias":NULL;
      int first=sub==1942?1941:sub==1944?1943:1946;
      gv_aj_row(first,"aj_monitor",i18n(a),NULL,NULL,-1,sel==first,x,y+194,w);
      gv_aj_row(first+1,"aj_monitor",i18n(b),NULL,NULL,-1,sel==first+1,x,y+270,w);
      if(c) gv_aj_row(1958,"aj_monitor",i18n(c),NULL,NULL,-1,sel==1958,x,y+346,w);
      if(sub!=1942)gv_label(TXT_ILHA_SUB,i18n(sub==1947?"Menos dias usam menos memória.":"Escolha quando um vídeo começa."),x+18,y+374,w-36,1);
    }else{
      GuiaFonteSaude source;int n=0;
      for(;n<6&&guia_fonte_saude(gv_aj_source_off+n,&source);n++){
        char status[120];snprintf(status,sizeof status,i18n("%d de %d canais com guia"),source.comGuia,source.canais);
        gv_aj_row(1985+n,sub==1937?"aj_tv-minimal-play":"aj_calendar",source.nome,status,NULL,sub==1937?guia_fonte_ligada(gv_aj_source_off+n):-1,0,x,y+194+n*76,w);
      }
      if(!n)gv_label(TXT_ILHA_SUB,i18n("Nenhuma fonte"),x+18,y+212,w-36,1);
      if(sub==1937){
        gv_aj_row(1955,"aj_plus",i18n("Adicionar fonte"),NULL,NULL,-1,0,x,y+688,w);
      }else gv_aj_row(1975,"aj_plus",i18n("Adicionar XMLTV"),NULL,NULL,-1,0,x,y+688,w);
      if(gv_aj_source_off)gv_aj_row(1956,"aj_arrow-left",i18n("Anterior"),NULL,NULL,-1,0,x,y+788,200);
      if(guia_fonte_saude(gv_aj_source_off+6,&source))gv_aj_row(1957,"aj_chevron-right",i18n("Próximo"),NULL,NULL,-1,0,x+w-200,y+788,200);
    }
    gfx_recorte_voltar(clip);return;
  }
  plrui_kicker(i18n("Prévia"),x+18,y+132,154,156,158,1);
  gv_aj_row(1940,"aj_tv-minimal-play",i18n("Prévia ao vivo no guia"),NULL,NULL,guia_preview_ligado(),0,x,y+162,w);
  gv_aj_row(1942,"aj_monitor",i18n("Tamanho da prévia"),NULL,i18n(ajustes_guia_previa_grande()?"Grande":"Cartão"),-1,0,x,y+232,w);
  gv_aj_row(1944,"aj_play",i18n("Quando iniciar a prévia"),NULL,i18n(ajustes_guia_previa_destacar()?"Ao destacar":"Ao selecionar"),-1,0,x,y+302,w);
  gv_aj_row(1945,"aj_volume-2",i18n("Prévia sem som"),NULL,NULL,ajustes_guia_previa_sem_som(),0,x,y+372,w);
  gfx_cor((GfxRect){x+18,y+454,w-36,1},0,1,1,1,.08f);
  plrui_kicker(i18n("Aparência"),x+18,y+474,154,156,158,1);
  gv_aj_row(1947,"aj_calendar-days",i18n("Alcance do guia"),i18n(epg_memoria_baixa()?"Menos dias usam menos memória. Em Automático com pouca RAM, o guia mostra só as próximas 24 horas.":"Menos dias usam menos memória."),i18n(ajustes_guia_dias()==7?"Sete dias":ajustes_guia_dias()==3?"Três dias":"Um dia"),-1,0,x,y+504,w);
  gv_aj_row(1948,"aj_palette",i18n("Material Glass"),NULL,NULL,ajustes_vidro(),0,x,y+574,w);
  gfx_cor((GfxRect){x+18,y+656,w-36,1},0,1,1,1,.08f);
  plrui_kicker(i18n("Guia"),x+18,y+676,154,156,158,1);
  gv_aj_row(1949,"aj_calendar",i18n("Saúde do EPG"),NULL,NULL,-1,0,x,y+706,w);
  gv_aj_row(1937,"aj_settings-2",i18n("Gerenciar fontes"),NULL,NULL,-1,0,x,y+776,w);
  gv_aj_row(1938,"aj_shield-check",i18n("Controle parental"),NULL,NULL,-1,0,x,y+846,w);
  gv_aj_row(1939,"aj_layout-dashboard",i18n("Multiview"),NULL,NULL,-1,0,x,y+916,w);
  gfx_recorte_voltar(clip);
}
static void gv_pastas_sheet(void){
  gv_dim(0,.42f);
  float x=gv_sheet_base(360,190,1440,420);
  gv_sheet_head(x,222,i18n("Suas pastas"),i18n("Pastas agrupam categorias de várias fontes. Os canais ficam nas categorias originais."));
  gv_button(1950,i18n("Nova pasta"),"aj_folders",x,336,300,1,1);
  /* folder order comes back with real folders */
  txt_bloco(TXT_ILHA_CORPO,i18n("Sem pastas ainda. Crie uma para reunir as categorias que você mais abre."),154,156,158,x,412,680,32,1,2);
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
  gv_label(TXT_ILHA_CORPO,draft.xmltv[0]?draft.xmltv:"https://example.com/guia.xml",x+20,388,600,!draft.xmltv[0]);
  const char *ic=failed?"aj_triangle-alert":loaded?"aj_check":"aj_activity";
  gfx_icone((GfxRect){x,462,36,36},ic,failed?.95f:(loaded?.62f:.6f),failed?.6f:(loaded?.87f:.6f),failed?.43f:(loaded?.86f:.6f),1);
  gv_label(TXT_TITULO2,i18n(failed?"Falha na verificação":loaded?"Fonte adicionada e carregada":"Verificando fonte EPG…"),x+56,460,900,0);
  gv_label(TXT_ILHA_CORPO,i18n(failed?"Verifique a URL e tente novamente. O guia atual será mantido.":loaded?"":"Lendo XMLTV e conferindo os identificadores."),x+56,518,1000,1);
  if(loaded){GuiaFonteSaude f;int com=0,tot=0;for(int i=0;guia_fonte_saude(i,&f);i++){com+=f.comGuia;tot+=f.canais;}
    char t[120];snprintf(t,sizeof t,i18n("%d de %d canais com guia"),com,tot);gv_label(TXT_ILHA_CORPO,t,x+56,518,1000,1);}
  gv_button(1960,i18n(failed?"Verificar URL":loaded?"Concluir":"Cancelar"),failed?"aj_pencil":loaded?"aj_check":"aj_x",x,590,340,0,0);
  if(failed)gv_button(1961,i18n("Tentar novamente"),"aj_rotate-cw",x+370,590,340,0,0);
}
static void gv_epg_saude_sheet(void){
  gv_dim(0,.42f);
  GuiaFonteSaude f;int n=0,nf=0;while(nf<3&&guia_fonte_saude(nf,&f))nf++;
  float yb=340+(nf?nf:1)*128+20;   /* the button row follows the sources */
  float x=gv_sheet_base(360,190,1440,yb+56+48-190);
  gv_sheet_head(x,222,i18n("Saúde do EPG"),"");
  for(;n<3&&guia_fonte_saude(n,&f);n++){
    float y=340+n*128;char t[120];
    gv_label(TXT_ILHA_NOME,f.nome,x,y,820,0);
    snprintf(t,sizeof t,i18n("%d de %d canais com guia"),f.comGuia,f.canais);
    gv_label(TXT_ILHA_SUB,t,x,y+34,820,1);
    gv_badge(i18n(f.saude==LTV_SAUDE_CARREGANDO?"Carregando…":f.saude==LTV_SAUDE_FORA?"Falha na conexão":f.deCache?"lista guardada":f.comGuia>0?"Em dia":"Sem programação"),x+880,y+6);
    gv_button(1970+n,i18n("Recarregar EPG"),"aj_rotate-cw",x+1060,y-8,260,0,1);
    gfx_cor((GfxRect){x,y+100,1380,1},0,1,1,1,.12f);
  }
  if(!n){gv_label(TXT_ILHA_NOME,i18n("Nenhuma fonte"),x,340,820,0);
    gv_label(TXT_ILHA_SUB,i18n("Categorias e pastas aparecem ao adicionar uma lista."),x,374,1000,1);}
  gv_button(1975,i18n("Adicionar XMLTV"),"aj_plus",x,yb,420,0,0);
  gv_label(TXT_ILHA_SUB,i18n("O guia atual continua se a verificação falhar"),x+460,yb+16,800,1);
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
  gv_label(TXT_ILHA_CORPO,draft.playlist[0]?draft.playlist:"https://example.com/lista.m3u",x+20,500,600,!draft.playlist[0]);
  gv_label(TXT_ILHA_SUB,i18n("Verificada antes de salvar. Seus canais continuam disponíveis se o link falhar."),x,560,700,1);
  gv_button(1993,i18n("Testar e adicionar"),"aj_check",x,640,340,1,1);
  gv_button(1994,i18n("Digitar no celular"),"aj_smartphone",x+360,640,320,0,1);
}

#include "guiav5_esportes.inc"

/* TV search adapters: the scope itself lives in Spotlight. */
static const G5Item *gv_busca_catalogo(int i,int *n){
  *n=count;
  if(i<0)return inventory;
  return i<count&&group_allowed(inventory[i].group)&&!restricted(&inventory[i])?&inventory[i]:NULL;
}
static void gv_busca_resultado(const SpotTVResultado *r){
  if(r->tipo==SPOT_TV_VOD){
    if(r->indice>=0&&r->indice<count&&!strcmp(inventory[r->indice].id,r->id)&&group_allowed(inventory[r->indice].group)&&!restricted(&inventory[r->indice])){
      gv_deixa();select_item(r->indice);
    }return;
  }
  if(r->tipo==SPOT_TV_CATEGORIA){gv_filtrar(r->id,"grade");return;}
  if(r->tipo==SPOT_TV_ESPORTE){
    gv_go("esportes-hoje");gs_start();snprintf(query,sizeof query,"%s",r->nome);return;
  }
  gv_filtrar("","grade");
  int pos=0;
  for(int i=0;guia_canal_regra_campos(i,NULL,NULL,NULL,NULL,NULL);i++){
    const char *id;
    if(!guia_canal_campos(i,&id,NULL,NULL,NULL,NULL))continue;
    if(!strcmp(id,guia_canal_grupo_id(r->id))){gv_lin=pos;break;}pos++;
  }
  gv_tfoc=r->tipo==SPOT_TV_PROGRAMA?r->inicio:0;
  if(gv_tfoc>time(NULL))gv_tdesl=(int)((gv_tfoc-gv_tbase())/1800)*30;
  gv_janela();gv_snap_force=1;gv_snapshot();gv_zone=0;
  guia_focar_id(r->id);
  if(r->tipo==SPOT_TV_CANAL)gv_assistir(r->id);
}
static void gv_busca_abrir(void){
  if((!source_loaded||categories_missing)&&!atomic_load(&busy)){
    if(xtream_configurado()){use_saved=1;load_saved_credentials();draft.movies=draft.series=1;start(2);}
    else if(draft.playlist[0])start(2);
  }
  spot_abrir_tv(gv_busca_catalogo,gv_busca_resultado);
}

/* --- dispatcher da tela principal ------------------------------------------ */
static int gv_cartoes(void){return !strcmp(gv_pagina,"cartoes");}
static void gv_draw_sheet(int err){
  if(!strcmp(screen,"numero"))gv_numero_sheet();
  else if(!strcmp(screen,"opcoes"))gv_opcoes_sheet();
  else if(!strcmp(screen,"canal-opcoes"))gv_canal_opcoes_sheet();
  else if(!strcmp(screen,"ajustes"))gv_ajustes_sheet();
  else if(!strcmp(screen,"pastas"))gv_pastas_sheet();
  else if(!strcmp(screen,"epg-saude"))gv_epg_saude_sheet();
  else if(!strncmp(screen,"epg-",4))gv_epg_sheet();
  else if(!strcmp(screen,"erros"))gv_erros_sheet();
  else if(err)gv_fontes_erro_modal();
}
/* Draws the guide screen `nome` in place of the current one: the page under
 * a sheet, or a sheet that is closing. */
static void gv_como(const char *nome,void (*f)(void)){
  char cur[48];snprintf(cur,sizeof cur,"%s",screen);snprintf(screen,sizeof screen,"%s",nome);
  f();snprintf(screen,sizeof screen,"%s",cur);
}
static void gv_folha(void){if(gv_sports())gv_sports_screen();else gv_draw_sheet(!strcmp(screen,"fontes-erro"));}
static void gv_folha_em(float t){
  float og=gfx_opacidade_grupo;gfx_opacidade_grupo=og*t;
  if(!strcmp(screen,"ajustes")){
    float k=gv_aj_scale(),s=anim_mistura(T21_SHEET_SCALE,1,t);
    gfx_transformar(0,0,k*s,NV_LAYOUT_REAL_W*(1-s)/2,NV_LAYOUT_REAL_H*(1-s)/2);
  }else if(t<1)gfx_transformar(960,540,anim_mistura(T21_SHEET_SCALE,1,t),0,0);
  gv_folha();gfx_sem_transformar();gfx_opacidade_grupo=og;
}
static void gv_fantasma(void){gv_reg=0;gv_folha_em(anim_suave(gv_sai_t));}
static void gv_screen(void){
  static Uint32 ult;Uint32 agora=SDL_GetTicks();
  gv_dt=ult&&agora>ult?fminf((float)(agora-ult)/1000.0f,.05f):0;ult=agora;
  gv_snapshot();gv_janela();
  GuiaFonteSaude falha;
  int err=!strcmp(screen,"fontes-erro")&&gv_fonte_falha(&falha),ov=gv_overlay();
  float base=err?256:GV_BASE,og=gfx_opacidade_grupo;
  gv_folha_t=t21_entrada(gv_folha_t,gv_dt,1);
  gv_pag_t=t21_entrada(gv_pag_t,gv_dt,1);
  gv_veu_t=t21_entrada(gv_veu_t,gv_dt,ov);
  gv_sai_t=t21_entrada(gv_sai_t,gv_dt,0);
  int sai=gv_sai_t>0&&gv_sai[0]&&strcmp(gv_sai,screen);
  /* the closing sheet sits under a sheet opened over it, over one it returns to */
  int sai_sob=sai&&ov&&gv_npilha&&!strcmp(gv_pilha[gv_npilha-1].nome,gv_sai);
  /* Keep the hero spring while the sidebar opens and closes. */
  gv_hh=anim_mola2(&gv_hh_v,gv_hh,GV_HERO_H,gv_dt,T21_SCROLL);
  gv_side=anim_mola2(&gv_side_v,gv_side,gv_side_open?1:0,gv_dt,T21_SCROLL);
  int topo=gv_lista();   /* Tabs and content share the same focus registry. */
  gv_gfolha_visto=0;
  gv_reg=topo;
  if(gv_sports()&&ov)gv_reg=0;
  gv_topbar();
  if(!gv_sports())gv_chips_row();
  gv_reg=0;
  /* Troca de pagina: o conteudo entra subindo, nunca aparece de estalo. */
  float pa=anim_suave(gv_pag_t);
  gfx_opacidade_grupo=og*pa;
  if(pa<1)gfx_transformar(960,540,1,0,18*(1-pa));
  if(!gv_total_n&&!gv_sports()){
    gv_reg=1;gv_vazio();gv_reg=0;
    gfx_sem_transformar();gfx_opacidade_grupo=og;
    gv_sidebar(base);
  }else{
    gv_reg=topo&&gv_side_open;if(!gv_sports()||gv_side_open)gv_sidebar(base);gv_reg=0;
    int esp=!strncmp(ov?gv_pagina:screen,"esportes-",9);
    if(esp){
      if(ov){gfx_sem_transformar();gfx_opacidade_grupo=og;gv_como(gv_pagina,gv_sports_screen);}
      else{gv_reg=1;gv_sports_screen();gv_reg=0;}
      gfx_sem_transformar();gfx_opacidade_grupo=og;
    }else{
      if(gv_cartoes()){gv_hero_compact(base);gv_dock(base,208);gv_cards(base);}
      else{gv_hero(base);gv_dock(base,gv_hh);gv_grid(base);}
      gfx_sem_transformar();gfx_opacidade_grupo=og;
    }
    if(ov||sai){
      /* Folha: um so veu e a ilha crescendo de .96 com o fade (T21); ao fechar,
       * encolhe e some pela rampa de saida. */
      int e2=ov?err:!strcmp(gv_sai,"fontes-erro")&&gv_fonte_falha(&falha);
      gv_dim(e2?248:0,e2?.5f:.42f);
      gv_sem_veu=1;
      if(sai_sob)gv_como(gv_sai,gv_fantasma);
      if(ov){gv_reg=1;gv_folha_em(anim_suave(gv_folha_t));gv_reg=0;}
      if(sai&&!sai_sob)gv_como(gv_sai,gv_fantasma);
      gv_sem_veu=0;
    }
    /* error strip above the veil, after the sheet: the sheet takes first focus */
    if(err){gv_reg=1;gv_strip();gv_reg=0;}
  }
  if(strcmp(screen,"ajustes")&&gv_lista()&&!gv_side_open&&num_focus){int ok=0;for(int i=0;i<num_focus;i++)if(next_focus[i]==focus)ok=1;
    if(!ok){focus=next_focus[0];if(topo)for(int i=0;i<num_focus;i++)if(next_focus[i]>=2000){focus=next_focus[i];break;}}}
  if(gv_side_open&&ov){gv_reg=0;gv_sidebar(base);}
  if(!gv_gfolha_visto)gv_gfolha.ok=0;
}
/* --- eventos da tela principal -------------------------------------------- */
static void gv_go(const char *name){
  if(!strcmp(name,"busca")){gv_busca_abrir();return;}
  snprintf(previous,sizeof previous,"%s",screen);
  int f0=focus;
  if(gv_overlay_nome(name)){
    if(gv_overlay()){if(gv_npilha<6){snprintf(gv_pilha[gv_npilha].nome,sizeof gv_pilha[0].nome,"%s",screen);gv_pilha[gv_npilha++].foco=focus;}
      snprintf(gv_sai,sizeof gv_sai,"%s",screen);gv_sai_t=1;}
    else{gv_pfoco=focus;gv_npilha=0;}
    gv_folha_t=0;
  }else{
    if(strcmp(gv_pagina,name))gv_pag_t=0;
    snprintf(gv_pagina,sizeof gv_pagina,"%s",name);gv_npilha=0;
  }
  go(name);
  if(!strcmp(name,"ajustes")){gv_aj_sub=0;gv_aj_scroll=0;gv_gfolha.ok=0;
#ifdef NV_TOUCH_UI
    toquerol_limpar(&gv_aj_touch);
#endif
  }
  focus=!strcmp(name,"ajustes")?1940:-1;
  if((gv_zone==5||gv_zone==4)&&!gv_overlay_nome(name))focus=f0;   /* a tab keeps its focus */
  if(!strcmp(name,"dock"))gv_zone=1;
  if(!strcmp(name,"categorias"))gv_side_enter();
  if(!strcmp(name,"cartoes")&&gv_zone==0)gv_zone=6;
  if(!strcmp(name,"grade")&&gv_zone==6)gv_zone=0;
  if(!strncmp(name,"epg-",4)&&strcmp(name,"epg-saude"))gv_verif_estado=!strcmp(name,"epg-adicionado")?1:!strcmp(name,"epg-falha")?2:0;
}
/* Leaving the guide for a v5 page (setup, sources, parental, VOD): Back from
 * that page comes here again, focus included. */
static void gv_deixa(void){
  snprintf(gv_saida,sizeof gv_saida,"%s",screen);gv_saida_foco=focus;
  if(!gv_overlay())gv_pfoco=focus;
}
/* Abertura vinda de fora (guia_abrir): a pagina entra e o foco volta a grade
 * no agora. Folhas abertas assim ficam sobre a pagina corrente. */
static void gv_abrir(const char *name){
  gv_npilha=0;gv_saida[0]=0;
  if(gv_overlay_nome(name)){gv_folha_t=0;if(!strcmp(name,"ajustes")){gv_aj_sub=0;gv_gfolha.ok=0;focus=1940;}return;}
  snprintf(gv_pagina,sizeof gv_pagina,"%s",name);
  gv_pag_t=0;gv_tdesl=0;gv_tfoc=0;gv_tmin=0;gv_pfoco=-1;
  gv_zone=!strcmp(name,"cartoes")?6:0;gv_side_open=0;gv_exit_armed=0;gv_empty_offset=0;
}
/* Volta um nivel: da folha para a folha de baixo, da pagina v5 para a tela do
 * guia que a abriu, ou para a pagina do guia; o foco volta para onde estava.
 * A folha que fecha encolhe e some sobre o que fica (gv_sai). */
static void gv_voltar(void){
  if(!strcmp(screen,"ajustes")&&!gv_aj_sub&&sources_options){sources_options=0;go("fontes-gerenciar");focus=204;return;}
  if(!strcmp(screen,"ajustes")&&gv_aj_sub){gv_aj_back();return;}
  int ov=gv_overlay();
  if(ov){snprintf(gv_sai,sizeof gv_sai,"%s",screen);gv_sai_t=1;}
  if(ov&&gv_npilha){GVNivel n=gv_pilha[--gv_npilha];go(n.nome);focus=n.foco;gv_folha_t=1;return;}
  if(!gv_is(screen)&&gv_saida[0]){
    char a[48];snprintf(a,sizeof a,"%s",gv_saida);gv_saida[0]=0;go(a);focus=gv_saida_foco;
    if(gv_overlay())gv_folha_t=0;else gv_pag_t=0;return;}
  go(gv_pagina[0]?gv_pagina:"grade");focus=gv_pfoco;gv_pag_t=ov?1:0;
}
/* Novo filtro (chip ou lateral): a grade recomeca no primeiro canal. */
static void gv_filtrar(const char *grupo,const char *pagina){
  snprintf(gv_grupo,sizeof gv_grupo,"%s",grupo);
  gv_lin=gv_top=gv_off=gv_card=0;gv_card_scroll=0;gv_rol=0;gv_tdesl=0;gv_tfoc=0;gv_snap_force=1;gv_snapshot();
  gv_zone=gv_cartoes()&&!strcmp(pagina,"cartoes")?6:0;gv_side_open=0;
  gv_go(pagina);
}
/* Programa futuro: OK marca/desmarca o lembrete (como no guia de sempre). */
static int gv_lembrete(GVCh *c,EpgProg *p){
  if(!c||!p||p->ini<=time(NULL))return 0;
  char msg[200];
  int r=guia_lembrete_alternar(c->id,c->nome,p->titulo,c->base,p->ini,p->fim);
  if(r==1)snprintf(msg,sizeof msg,i18n("Lembrete marcado: %s"),p->titulo);
  else if(r==0)snprintf(msg,sizeof msg,"%s",i18n("Lembrete cancelado"));
  else snprintf(msg,sizeof msg,"%s",i18n("Não cabe mais lembrete"));
  glem_aviso_curto(msg);
  return 1;
}
static void gv_assistir(const char *id){
  if(!id||!id[0])return;
  if(player_mini_no_guia_ativo()&&!strcmp(guia_canal_grupo_id(id),guia_canal_grupo_id(player_id_canal())))guia_pedir_canal_id(id);
  else guia_pedir_preview_id(id);
}
static int gv_activate(void){
  GVCh *c;
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
      if(focus==1936){gv_voltar();return 1;}
      if(!gv_aj_sub&&focus==1937){g5ui_open("fontes-gerenciar");return 1;}
      if(!gv_aj_sub&&(focus==1942||focus==1944||focus==1947||focus==1949)){gv_aj_open(focus);return 1;}
      if(gv_aj_sub==1937||gv_aj_sub==1949){
        if(focus>=1985&&focus<1991){
          if(gv_aj_sub==1937){guia_fonte_alternar(gv_aj_source_off+focus-1985);gv_snap_force=1;}
          else epg_recarregar();
        }else if(focus==1955)g5ui_open("setup-tipo");
        else if(focus==1975)gv_go("epg-verificando");
        else if(focus==1956||focus==1957){gv_aj_source_off+=focus==1956?-6:6;focus=1985;gv_gfolha.ok=0;}
        return 1;
      }
      if(focus==1940)guia_definir_preview(!guia_preview_ligado());
      else if(focus==1942)ajustes_definir_guia_previa_grande(1);
      else if(focus==1941)ajustes_definir_guia_previa_grande(0);
      else if(focus==1943||focus==1944)ajustes_definir_guia_previa_destacar(focus==1943);
      else if(focus==1945)ajustes_definir_guia_previa_sem_som(!ajustes_guia_previa_sem_som());
      else if(focus==1946||focus==1947||focus==1958){ajustes_definir_guia_dias(focus==1958?7:focus==1947?3:1);gv_tdesl=0;gv_tfoc=0;gv_snap_force=1;epg_recarregar();}
      else if(focus==1948)ajustes_definir_vidro(!ajustes_vidro());
      else if(focus==1949)gv_go("epg-saude");
      else if(focus==1937){sources_options=0;g5ui_open("fontes-gerenciar");}
      else if(focus==1938)want_pin(1);
      else if(focus==1939)g5ui_open("multiview-2");
    }else if(!strcmp(screen,"pastas")){
      /* sem pastas ainda: nada a abrir */
    }else if(!strncmp(screen,"epg-",4)&&strcmp(screen,"epg-saude")){
      if(focus==1960)gv_voltar();else if(focus==1961)gv_go("epg-verificando");
    }else if(!strcmp(screen,"epg-saude")){
      if(focus==1975)gv_go("epg-verificando");
      else if(focus>=1970&&focus<1973){epg_recarregar();gv_voltar();}
    }
    return 1;}
  if(!strcmp(screen,"fontes-erro")){
    if(focus>=1990&&focus<1993)gv_go("epg-verificando");
    else if(focus==1710){guia_recarregar(0);gv_voltar();}
    else if(focus==1711)gv_go("erros");
    else gv_voltar();return 1;}
  if((!strcmp(screen,"vazio")||!gv_total_n)&&!gv_sports()&&focus>=1700){
    GuiaFonteSaude source;
    if(focus>=4000&&guia_fonte_saude(focus-4000,&source)&&!guia_fonte_ligada(focus-4000)){
      if(guia_fonte_alternar(focus-4000)){gv_snap_force=dirty=1;gv_zone=0;focus=-1;if(!strcmp(screen,"vazio"))gv_go("grade");}
    }else if(focus==1797||focus==1798){gv_empty_offset+=focus==1797?-4:4;focus=-1;}
    else if(focus==1710){guia_recarregar(0);gv_snap_force=1;}
    else g5ui_open("fontes-gerenciar");
    return 1;
  }
  int topo=gv_sports()&&((focus>=1100&&focus<1107)||(focus>=1200&&focus<1206)||(focus>=1302&&focus<1305)||(focus>=1320&&focus<1320+gv_cats));
  if(topo){
    if(focus<1107)gv_zone=5;else if(focus<1206){gv_zone=4;gv_chipi=focus-1200;}
    else{gv_zone=3;gv_sb=focus<1305?focus-1300:focus-1314;}
  }
  if(gv_sports()&&!topo)return gs_activate();
  if(gv_zone==5){
    int id=focus-1100;
    if(id==0)gv_go("grade");
    else if(id==1)gv_go("cartoes");
    else if(id==2)gv_go("esportes-ligas");
    else if(id==3)g5ui_open("vod-filmes");
    else if(id==4)gv_go("busca");
    else if(id==5)g5ui_open("fontes-gerenciar");
    else if(id==6)g5ui_open("multiview-2");
    return 1;}
  if(gv_zone==4){
    int source=gv_source_index(gv_chipi);GuiaFonteSaude f;
    if(source>=0&&guia_fonte_saude(source,&f)){
      snprintf(gv_fonte,sizeof gv_fonte,"%s",f.chave);
      gv_filtrar("",gv_cartoes()?"cartoes":"grade");gv_zone=4;return 1;
    }
    switch(gv_chipi){
      case 0:gv_fonte[0]=0;gv_filtrar("",gv_cartoes()?"cartoes":"grade");gv_zone=4;break;
      case 3:gv_tdesl=0;gv_tfoc=0;gv_snap_force=1;gv_go("catchup");gv_zone=0;break;
      case 4:g5ui_open("setup-tipo");break;
      case 5:gv_go("epg-saude");break;
    }return 1;}
  if(gv_zone==3){
    if(gv_overlay()){gv_voltar();gv_zone=3;}
    if(gv_sb>=6&&gv_sb-6<gv_cats)gv_filtrar(gv_cat[gv_sb-6],gv_cartoes()?"cartoes":"grade");
    else if(gv_sb==2)gv_filtrar(GV_FAV,gv_cartoes()?"cartoes":"grade");
    else if(gv_sb==3)gv_filtrar("",gv_cartoes()?"cartoes":"grade");
    else if(gv_sb==4)gv_go("pastas");
    return 1;}
  if(gv_zone==2){c=gv_hero_channel(gv_cartoes()?gv_canal_cartao():gv_focus());if(c)gv_assistir(c->id);return 1;}
  if(gv_zone==1){c=gv_canal_focado();if(c)gv_assistir(c->id);return 1;}
  if(gv_zone==6||gv_zone==0){c=gv_canal_focado();
    if(c){EpgProg *p=gv_prog(c);if(!gv_lembrete(c,p))gv_assistir(c->id);}
    return 1;}
  return 1;
}
/* OK segurado (NV_HOLD, segurar.h): opcoes do canal; no dock, desafixa. */
static Uint32 gv_ok_desde;static int gv_ok_longo;
static void gv_ok_segurado(void){
  if(gv_sports()&&focus>=2200&&focus<2206){
    int i=gs_page*3+focus-2200;if(i<gs_n){gs_selected=gs_games[i];gs_open("esportes-onde-assistir");}return;
  }
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
  if(alvo>now+(time_t)ajustes_guia_dias()*86400)return 0;
  gv_tfoc=alvo<=now&&now<alvo+60&&!gv_past()?0:alvo;
  if(!gv_past()&&alvo<=now){
    for(int i=0;i<c->np;i++)if(c->p[i].ini==alvo&&c->p[i].fim>now){gv_tfoc=0;break;}
  }
  time_t t0=gv_tbase();
  int min=(int)((gv_tempo()-t0)/60);
  while(min<gv_tdesl&&gv_tdesl>0)gv_tdesl-=30;
  while(min>=gv_tdesl+90)gv_tdesl+=30;
  if(gv_tdesl<0)gv_tdesl=0;
  int max_minutes=ajustes_guia_dias()*24*60;
  if(gv_tdesl>max_minutes)gv_tdesl=max_minutes;
  gv_snap_force=1;
  return 1;
}
static void gv_mover_linha(int d){
  int n=gv_match;if(n<1)return;
  int l=gv_lin+d;if(l<0)l=0;if(l>n-1)l=n-1;
  gv_lin=l;gv_janela();
}
static void gv_side_enter(void){
  gv_exit_armed=0;
  if(gv_zone!=3){gv_return_zone=gv_zone;gv_return_focus=focus;}
  gv_side_open=1;gv_zone=3;if(!gv_sb_tem(gv_sb))gv_sb=3;
}
static void gv_side_leave(void){gv_side_open=0;gv_zone=gv_return_zone;focus=gv_return_focus;}
/* Options has one vertical focus target per row, including each picker. */
static void gv_ajustes_move(SDL_Keycode k){
#ifdef NV_TOUCH_UI
  toquerol_limpar(&gv_aj_touch);
#endif
  int ids[12],n=0;
  if(!gv_aj_sub){memcpy(ids,gv_aj_ids,sizeof gv_aj_ids);n=sizeof gv_aj_ids/sizeof *gv_aj_ids;}
  else{
    ids[n++]=1936;
    if(gv_aj_sub==1942||gv_aj_sub==1944||gv_aj_sub==1947){
      int first=gv_aj_sub==1942?1941:gv_aj_sub==1944?1943:1946;ids[n++]=first;ids[n++]=first+1;
      if(gv_aj_sub==1947)ids[n++]=1958;
    }else{
      GuiaFonteSaude source;
      for(int i=0;i<6&&guia_fonte_saude(gv_aj_source_off+i,&source);i++)ids[n++]=1985+i;
      ids[n++]=gv_aj_sub==1937?1955:1975;
      if(gv_aj_source_off)ids[n++]=1956;
      if(guia_fonte_saude(gv_aj_source_off+6,&source))ids[n++]=1957;
    }
  }
  if(k==SDLK_LEFT||k==SDLK_RIGHT){
    if(k==SDLK_RIGHT&&focus==1956&&ids[n-1]==1957)focus=1957;
    else if(k==SDLK_LEFT&&focus==1957&&gv_aj_source_off)focus=1956;
    return;
  }
  for(int i=0;i<n;i++)if(ids[i]==focus){int next=i+(k==SDLK_UP?-1:1);if(next>=0&&next<n)focus=ids[next];return;}
}
static void gv_point_focus(int id){
  if(gv_overlay()||gv_sports())return;
  if(id>=1100&&id<=1106)gv_zone=5;
  else if(id>=1200&&id<1300&&gv_chip_tem(id-1200)){gv_zone=4;gv_chipi=id-1200;}
  else if(id==1314)gv_zone=2;
  else if(id>=1400&&id<1406){gv_zone=1;gv_pin=id-1400;}
  else if(!gv_cartoes()&&id>=1500&&id<1500+gv_match){gv_zone=0;gv_lin=id-1500;gv_janela();}
  else if(id>=1600&&id<1600+gv_match){gv_zone=6;gv_card=gv_lin=id-1600;}
}
static int gv_event(SDL_Keycode k,int repeat){
  int ok=k==SDLK_RETURN||k==SDLK_KP_ENTER;
  if(!strcmp(screen,"ajustes")&&gv_arrow(k)){gv_ajustes_move(k);return 1;}
  if(gv_side_open){
    if(k==SDLK_RIGHT||k==SDLK_ESCAPE||k==SDLK_AC_BACK||k==SDLK_BACKSPACE){gv_side_leave();gv_exit_armed=k!=SDLK_RIGHT;return 1;}
    if(k==SDLK_UP||k==SDLK_DOWN){int d=k==SDLK_UP?-1:1,s=gv_sb+d;while(s>=0&&s<GV_SB_N&&!gv_sb_tem(s))s+=d;if(s>=0&&s<GV_SB_N)gv_sb=s;return 1;}
    if(ok&&!repeat){if(!gv_is(screen))gv_go("grade");gv_zone=3;gv_activate();gv_side_open=0;return 1;}
    return 1;
  }
  if((k==SDLK_ESCAPE||k==SDLK_AC_BACK||k==SDLK_BACKSPACE)&&!repeat){
    gv_ok_desde=0;
    if(!strcmp(screen,"numero")&&gv_numero[0]){size_t n=strlen(gv_numero);gv_numero[n-1]=0;gv_numero_desde=SDL_GetTicks();if(n>1)return 1;}
    if(gv_sports()){gs_back();return 1;}
    if(gv_overlay()){gv_numero[0]=0;gv_voltar();return 1;}
    if(gv_sports()||gv_past()){gv_go(gv_cartoes()?"cartoes":"grade");if(gv_zone!=6)gv_zone=0;return 1;}
    /* Desktop: the native sidebar is always there, so Back leaves the page. */
    if(gv_exit_armed||layout_desktop_ativo()){guia_fechar();g5ui_close();gv_exit_armed=0;return 1;}
    if(gv_zone==0||gv_zone==6){gv_side_enter();return 1;}
    /* Retrace the vertical controls before leaving content. */
    if(gv_total_n){
      if(gv_zone==5){
        if(focus<1104){focus=1104;return 1;}
        gv_zone=gv_zona_ok(1)?1:gv_cartoes()?6:0;return 1;
      }
      if(gv_zone==4){gv_zone=gv_cartoes()?6:0;return 1;}
      if(gv_zone==2){gv_zone=gv_zona_ok(1)?1:gv_cartoes()?6:0;return 1;}
      if(gv_zone==1){gv_zone=gv_cartoes()?6:0;return 1;}
    }
    guia_fechar();g5ui_close();return 1;}
  if(gv_lista()){
    if(gv_sports()&&gv_arrow(k)&&gs_arrow(k,repeat))return 1;
    if(gv_zone==5&&k==SDLK_DOWN&&gv_return_focus>=1700){gv_zone=gv_return_zone;focus=gv_return_focus;return 1;}
    if(focus>=1100&&focus<=1106){
      if(k==SDLK_LEFT||k==SDLK_RIGHT){int next=focus+(k==SDLK_LEFT?-1:1);if(next<1100)gv_side_enter();else if(next<=1106)focus=next;return 1;}
      if(ok&&!repeat){activate(focus);return 1;}
    }
    if(k==SDLK_UP&&focus>=1200&&gv_total_n){gv_return_focus=focus;gv_return_zone=gv_zone;gv_zone=5;focus=1100+(gv_sports()?2:0);return 1;}
    if(k==SDLK_LEFT){
      int at=-1,left=0;for(int i=0;i<num_focus;i++)if(next_focus[i]==focus)at=i;
      if(at>=0)for(int i=0;i<num_focus;i++)if(i!=at&&next_focus[i]>=1700&&focus_rect[i].x+focus_rect[i].w<=focus_rect[at].x+1&&fabsf(focus_rect[i].y-focus_rect[at].y)<30)left=1;
      if(!left){gv_side_enter();return 1;}
    }
    if(k==SDLK_UP||k==SDLK_DOWN||k==SDLK_LEFT||k==SDLK_RIGHT){
      if(!nav_spatial(k)){
        if(k==SDLK_LEFT)gv_side_enter();
        else if(k==SDLK_UP){gv_zone=5;focus=1100+(gv_sports()?2:0);}
      }return 1;
    }
    if(ok&&!repeat){
      gv_ok_desde=0;
      if(gv_sports()&&focus>=2200&&focus<2206){gv_ok_desde=SDL_GetTicks();if(!gv_ok_desde)gv_ok_desde=1;gv_ok_longo=0;return 1;}
      return gv_activate();
    }
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
    if(gv_zone==6){gv_card+=d*10;if(gv_card<0)gv_card=0;if(gv_card>=gv_match)gv_card=gv_match?gv_match-1:0;gv_lin=gv_card;gv_janela();}
    else gv_mover_linha(d*gv_vis(GV_BASE));
    return 1;}
  /* Vertical chain: tabs, actions (zone 5), pins, channels, sources (zone 4). */
  int grid=gv_cartoes()?6:0;
  if(k==SDLK_UP){
    if(gv_zone==0&&gv_lin>0)gv_mover_linha(-1);
    else if(gv_zone==6&&gv_card>=5){gv_card-=5;gv_lin=gv_card;gv_janela();}
    else if(gv_zone==4){
      gv_zone=grid;
      if(grid==0){gv_lin=gv_top+gv_vis(GV_BASE)-1;if(gv_lin>=gv_match)gv_lin=gv_match-1;gv_janela();}
    }
    else if(gv_zone==0||gv_zone==6||gv_zone==1||gv_zone==2){
      if((gv_zone==0||gv_zone==6)&&gv_zona_ok(1))gv_zone=1;
      else{gv_zone=5;focus=1104;}
    }
    else if(gv_zone==5&&focus>=1104)focus=1100+(gv_cartoes()?1:0);
    else if(gv_zone==3){int s2=gv_sb-1;while(s2>=0&&!gv_sb_tem(s2))s2--;if(s2>=0)gv_sb=s2;else{gv_zone=4;gv_chipi=0;}}
    return 1;}
  if(k==SDLK_DOWN){
    if(gv_zone==5){if(focus<1104)focus=1104;else gv_zone=gv_zona_ok(1)?1:grid;}
    else if(gv_zone==2)gv_zone=gv_zona_ok(1)?1:grid;
    else if(gv_zone==1){gv_zone=grid;if(grid==0){gv_lin=gv_top;gv_janela();}}
    else if(gv_zone==0){if(gv_lin+1<gv_match)gv_mover_linha(1);else gv_zone=4;}
    else if(gv_zone==6){if(gv_card+5<gv_match){gv_card+=5;gv_lin=gv_card;gv_janela();}else gv_zone=4;}
    else if(gv_zone==3){int s2=gv_sb+1;while(s2<GV_SB_N&&!gv_sb_tem(s2))s2++;if(s2<GV_SB_N)gv_sb=s2;}
    if(gv_zone==1&&gv_pin>=gv_npins)gv_pin=gv_npins-1;
    return 1;}
  if(k==SDLK_LEFT){
    if(gv_zone==0){if(!gv_tempo_passo(-1))gv_side_enter();}
    else if(gv_zone==6){if(gv_card%5){gv_card--;gv_lin=gv_card;gv_janela();}else gv_side_enter();}
    else if(gv_zone==1&&gv_pin>0)gv_pin--;
    else if(gv_zone==2||(gv_zone==1&&gv_pin==0)||(gv_zone==5&&(focus==1100||focus==1104)))gv_side_enter();
    else if(gv_zone==5&&focus>1100&&focus!=1104)focus--;
    else if(gv_zone==4){
      if(gv_chipi==2)gv_chipi=0;
      else if(gv_chipi>=6)gv_chipi=gv_chipi==6?2:gv_chipi-1;
      else if(gv_chipi==3){gv_chipi=0;for(int i=2;gv_chip_tem(i==2?2:i+3);i++)gv_chipi=i==2?2:i+3;}
      else if(gv_chipi>3)gv_chipi--;
    }
    return 1;}
  if(k==SDLK_RIGHT){
    if(gv_zone==3)gv_side_leave();
    else if(gv_zone==0)gv_tempo_passo(1);
    else if(gv_zone==6&&gv_card%5<4&&gv_card+1<gv_match){gv_card++;gv_lin=gv_card;gv_janela();}
    else if(gv_zone==1&&gv_pin<gv_npins-1)gv_pin++;
    else if(gv_zone==2)gv_hbt=2;
    else if(gv_zone==5&&focus<(focus<1104?1103:1106))focus++;
    else if(gv_zone==4){
      if(gv_chipi==0)gv_chipi=gv_chip_tem(2)?2:3;
      else if(gv_chipi==2||gv_chipi>=6){int next=gv_chipi==2?6:gv_chipi+1;gv_chipi=gv_chip_tem(next)?next:3;}
      else if(gv_chipi<5)gv_chipi++;
    }
    return 1;}
  return 1;
}
static void gv_tecla_solta(SDL_Keycode k){
  if(gv_sports()&&gv_arrow(k)){if(gs_key==k)gs_key=0;return;}
  if(k!=SDLK_RETURN&&k!=SDLK_KP_ENTER)return;
  if(gv_ok_desde&&!gv_ok_longo){
    if(gv_side_open)return;
    if(!gv_sports()){
      if(gv_zone==3)focus=1300+gv_sb;
      if(gv_zone==4)focus=1200+gv_chipi;
    }
    gv_ok_desde=0;gv_activate();return;}
  gv_ok_desde=0;gv_ok_longo=0;
}
static void gv_tick(void){
  static char preview_focus[80];static Uint32 preview_since;
  GVCh *c=gv_focus();
  if(!gv_overlay()&&!gv_sports()&&(gv_zone==0||gv_zone==6)&&c&&guia_preview_ligado()&&ajustes_guia_previa_destacar()){
    if(strcmp(preview_focus,c->id)){snprintf(preview_focus,sizeof preview_focus,"%s",c->id);preview_since=SDL_GetTicks();}
    else if(SDL_GetTicks()-preview_since>=600&&strcmp(guia_preview_id(),c->id))guia_pedir_preview_id(c->id);
  }else{preview_focus[0]=0;preview_since=SDL_GetTicks();}
  if(gv_ok_desde&&!gv_ok_longo&&segurar_venceu(SDL_GetTicks(),gv_ok_desde,"guia")){gv_ok_longo=1;gv_ok_segurado();}
  if(gv_numero[0]&&gv_numero_desde&&SDL_GetTicks()-gv_numero_desde>2000){
    const char *id=NULL;
    gv_numero_desde=0;
    if(gv_numero_alvo(&id,NULL)>=0){gv_numero[0]=0;gv_assistir(id);}
    else{gv_numero[0]=0;if(!strcmp(screen,"numero"))gv_voltar();}
  }
}
