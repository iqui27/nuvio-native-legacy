#include "m3u.h"
#include "dados.h"
#include "rede.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>

static char pasta[512];
static int perfil=1, pedidos, http=200, trocar, bloquear, bloqueado;
static uint64_t sessao=1;
static pthread_mutex_t redeTrava=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t redeSinal=PTHREAD_COND_INITIALIZER;
static const char *resposta="#EXTM3U url-tvg=\"https://epg.invalid/auto.xml\"\n"
  "#EXTINF:-1 tvg-id=\"one\" group-title=\"News\",One\n"
  "#EXTVLCOPT:http-referrer=https://required.invalid/\nhttps://cdn.invalid/one.ts\n"
  "#EXTINF:-1 tvg-id=\"two\" group-title=\"Films\",Two\nhttps://cdn.invalid/two.m3u8\n";
int perfis_ativo(void){return perfil;}
uint64_t perfis_sessao_geracao(void){return sessao;}
int perfis_sessao_atual(uint64_t s){return s==sessao;}
int perfis_sessao_travar(uint64_t s){return s==sessao;}
void perfis_sessao_destravar(void){}
char *dados_caminho(char *out,unsigned n,const char *nome){int k=snprintf(out,n,"%s/%s",pasta,nome);return k>=0&&(size_t)k<n?out:NULL;}
char *dados_ler(const char *nome){char cam[1024];assert(dados_caminho(cam,sizeof cam,nome));FILE *f=fopen(cam,"rb");if(!f)return NULL;
  assert(!fseek(f,0,SEEK_END));long n=ftell(f);rewind(f);assert(n>=0);char *b=malloc((size_t)n+1);assert(b);assert(fread(b,1,(size_t)n,f)==(size_t)n);b[n]=0;fclose(f);return b;}
int dados_gravar(const char *nome,const char *body){char cam[1024];assert(dados_caminho(cam,sizeof cam,nome));FILE *f=fopen(cam,"wb");if(!f)return 0;size_t n=strlen(body);int ok=fwrite(body,1,n,f)==n;return fclose(f)==0&&ok;}
int dados_gravar_leve(const char *nome,const char *body){return dados_gravar(nome,body);}
int dados_apagar(const char *nome){char cam[1024];assert(dados_caminho(cam,sizeof cam,nome));return unlink(cam)==0;}
char *rede_baixar_bin_medido_controle(const char *url,int prazo,const char *const *cab,const RedeControle *ctl,long *bytes,RedeMedida *med){
  (void)url;(void)prazo;(void)cab;assert(ctl->max_bytes>0);pedidos++;
  pthread_mutex_lock(&redeTrava);bloqueado=1;pthread_cond_broadcast(&redeSinal);
  while(bloquear)pthread_cond_wait(&redeSinal,&redeTrava);
  pthread_mutex_unlock(&redeTrava);
  if(trocar){perfil=2;sessao++;trocar=0;}
  med->status=http;*bytes=(long)strlen(resposta);return strdup(resposta);
}
static M3uCanal canais[8];
static void *refresh(void *unused){(void)unused;assert(m3u_canais_sessao(canais,8,sessao)==2);return NULL;}
int main(int argc,char **argv){
  assert(argc==2 && strlen(argv[1])<sizeof pasta);snprintf(pasta,sizeof pasta,"%s",argv[1]);assert(!mkdir(pasta,0700));
  assert(!m3u_configurado());
  assert(!m3u_definir_lista("javascript:bad"));assert(!m3u_definir_lista("https://invalid/\nsecret"));
  assert(m3u_definir_lista("https://provider.invalid/list.m3u?token=private"));
  assert(m3u_definir_epg("https://epg.invalid/manual.xml"));
  assert(m3u_canais_sessao(canais,8,sessao)==2&&pedidos==1);
  char id[80],url[4096],headers[512];strcpy(id,canais[0].id);
  assert(m3u_e_id(id)&&!m3u_e_id("m3u:bad"));
  assert(m3u_resolver_sessao(id,url,sizeof url,sessao,0)&&!strcmp(url,"https://cdn.invalid/one.ts")&&pedidos==1);
  assert(m3u_cabecalhos(id,headers,sizeof headers)&&strstr(headers,"Referer: https://required.invalid/"));
  assert(m3u_url_epg(url,sizeof url)&&!strcmp(url,"https://epg.invalid/manual.xml"));
  // Resolution uses the working map while a background refresh is blocked.
  pthread_t t;m3u_recarregar();bloquear=1;bloqueado=0;assert(!pthread_create(&t,NULL,refresh,NULL));
  pthread_mutex_lock(&redeTrava);while(!bloqueado)pthread_cond_wait(&redeSinal,&redeTrava);pthread_mutex_unlock(&redeTrava);
  assert(m3u_resolver_sessao(id,url,sizeof url,sessao,0));
  pthread_mutex_lock(&redeTrava);bloquear=0;pthread_cond_broadcast(&redeSinal);pthread_mutex_unlock(&redeTrava);pthread_join(t,NULL);
  // Failed refresh retains the valid cached list.
  http=503;m3u_recarregar();assert(m3u_canais_sessao(canais,8,sessao)==2);http=200;
  assert(m3u_definir_epg(""));assert(m3u_canais_sessao(canais,8,sessao)==2);
  assert(m3u_url_epg(url,sizeof url)&&!strcmp(url,"https://epg.invalid/auto.xml"));
  // A response from a closed profile cannot publish into the new profile.
  m3u_recarregar();trocar=1;uint64_t antiga=sessao;assert(!m3u_canais_sessao(canais,8,antiga));
  assert(perfil==2&&!m3u_configurado());assert(!m3u_resolver_sessao(id,url,sizeof url,sessao,0));
  assert(m3u_definir_lista("https://second.invalid/list"));
  resposta="<html>Login required</html>";assert(!m3u_canais_sessao(canais,8,sessao));
  m3u_esquecer_todos();assert(!m3u_configurado());perfil=1;sessao++;assert(!m3u_configurado());
  DIR *d=opendir(pasta);assert(d);struct dirent *e;int restantes=0;while((e=readdir(d)))if(e->d_name[0]!='.')restantes++;closedir(d);assert(restantes==0);assert(!rmdir(pasta));
  puts("m3u backend: cached playback, headers, refresh failure, profile cancellation and logout cleanup PASS");
}
