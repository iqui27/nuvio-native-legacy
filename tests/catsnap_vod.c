#include <assert.h>
#include "../src/guiav5ui.c"

static struct { char name[240]; char *body; } files[32];
static int nfiles, requests, fail_request, playlist_requests, empty_playlist;
char *dados_ler(const char *name){
  for(int i=0;i<nfiles;i++)if(!strcmp(files[i].name,name))return strdup(files[i].body);
  return NULL;
}
int dados_gravar_leve(const char *name,const char *body){
  int i=0;while(i<nfiles&&strcmp(files[i].name,name))i++;
  if(i==nfiles){assert(nfiles<32);snprintf(files[nfiles++].name,240,"%s",name);files[i].body=NULL;}
  free(files[i].body);files[i].body=strdup(body);assert(files[i].body);return 1;
}
int dados_gravar(const char *name,const char *body){return dados_gravar_leve(name,body);}
const char *i18n(const char *s){return s;}
Uint32 SDL_GetTicks(void){return 100;}
void admin_sha256_hex(const unsigned char *body,unsigned long n,char *hex){
  unsigned long long hash=5381;for(unsigned long i=0;i<n;i++)hash=hash*33+body[i];
  snprintf(hex,65,"%064llx",hash);
}
int rede_pedir(const RedePedido *request,RedeResposta *response){
  playlist_requests++;response->status=fail_request?503:200;
  const char *body=empty_playlist?"#EXTM3U\n":"#EXTM3U\n#EXTINF:-1,Movie\nhttps://example.invalid/movie.mp4\n";
  if(!fail_request)assert(request->receber_corpo(body,strlen(body),request->corpo_usuario));
  return REDE_OK;
}
void rede_resposta_limpar(RedeResposta *response){(void)response;}
char *xtream_consultar(const char *server,const char *user,const char *password,const char *request,int seconds,int *status){
  (void)server;(void)user;(void)password;(void)seconds;requests++;
  *status=fail_request?503:200;if(fail_request)return NULL;
  if(!strcmp(request,"get_vod_categories"))return strdup("[{\"category_id\":\"1\",\"category_name\":\"Movies\"}]");
  if(!strcmp(request,"get_series_categories"))return strdup("[{\"category_id\":\"2\",\"category_name\":\"Series\"}]");
  if(strstr(request,"category_id=3"))return strdup("[]");
  if(strstr(request,"get_vod_streams"))return strdup("[{\"stream_id\":7,\"name\":\"Movie\",\"category_id\":\"1\"}]");
  assert(strstr(request,"get_series"));return strdup("[{\"series_id\":8,\"name\":\"Series\",\"category_id\":\"2\"}]");
}
static void run(int op,int kind,const char *category){
  operation=op;vod_request_kind=kind;snprintf(vod_request_id,sizeof vod_request_id,"%s",category);
  worker_categories_missing=0;vod_load_worker();
}
int main(void){
  snprintf(vod_work_key,sizeof vod_work_key,"identity-a");
  snprintf(work_draft.user,sizeof work_draft.user,"fixture-user");
  snprintf(work_draft.password,sizeof work_draft.password,"fixture-password");
  work_draft.movies=work_draft.series=1;
  run(6,0,"");assert(worker_ok&&requests==2);
  run(7,0,"1");run(7,1,"2");assert(worker_ok&&requests==4&&vod_pending.count==2);
  vod_free(&vod_pending);run(8,0,"");
  assert(worker_ok&&!worker_categories_missing&&requests==4&&vod_pending.count==2);
  run(6,0,"");run(7,0,"1");run(7,1,"2");assert(requests==4&&vod_pending.count==2);
  vod_work_stamp++;
  run(8,0,"");assert(worker_categories_missing&&vod_pending.count==2);
  run(6,0,"");assert(requests==6);
  char *old=strdup(vod_pending.json[0]);assert(old);
  fail_request=1;run(7,0,"1");assert(!worker_ok&&requests==7&&!strcmp(old,vod_pending.json[0]));free(old);
  fail_request=0;run(7,0,"1");assert(worker_ok&&requests==8);
  run(7,0,"3");assert(worker_ok&&requests==9);
  run(7,0,"3");assert(worker_ok&&requests==9);
  dados_gravar_leve("guiav5-vod-identity-a-0-item-1.json","broken snapshot");
  run(7,0,"1");assert(worker_ok&&requests==10);
  snprintf(vod_work_key,sizeof vod_work_key,"identity-b");
  vod_free(&vod_pending);run(8,0,"");assert(worker_categories_missing&&!vod_pending.count);
  run(6,0,"");assert(worker_ok&&requests==12);
  vod_free(&vod_pending);
  G5Item out[2];work_draft.type=1;vod_work_stamp=0;
  snprintf(work_draft.playlist,sizeof work_draft.playlist,"https://example.invalid/list");
  assert(load_playlist(out,2)==1&&playlist_requests==1);
  assert(load_playlist(out,2)==1&&playlist_requests==1);
  vod_work_stamp=1;fail_request=1;
  assert(load_playlist(out,2)==-1&&playlist_requests==2);
  vod_work_stamp=0;assert(load_playlist(out,2)==1&&playlist_requests==2);
  vod_work_stamp=1;fail_request=0;empty_playlist=1;
  assert(load_playlist(out,2)==0&&playlist_requests==3);
  assert(load_playlist(out,2)==0&&playlist_requests==3);
  snprintf(work_draft.playlist,sizeof work_draft.playlist,"https://other.invalid/list");
  assert(load_playlist(out,2)==0&&playlist_requests==4);
  for(int i=0;i<nfiles;i++)free(files[i].body);
  puts("catsnap VOD: fresh restart has zero requests; refresh, identity, empty, corrupt and failure retention OK");
  puts("catsnap playlist: fresh snapshot has zero requests; refresh, identity, empty and failed refresh retention OK");
  return 0;
}
