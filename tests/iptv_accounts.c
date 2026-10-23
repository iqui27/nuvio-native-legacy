#include "xtream.h"
#include "m3u.h"
#include "stalker.h"
#include "iptvfontes.h"
#include "iptvid.h"
#include "dados.h"
#include "rede.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static const char *pasta;
static int perfil=1, falhar;
static uint64_t sessao=1;
uint64_t perfis_sessao_geracao(void){return sessao;}
int perfis_sessao_atual(uint64_t s){return s==sessao;}
int perfis_sessao_travar(uint64_t s){return s==sessao;}
void perfis_sessao_destravar(void){}
int perfis_ativo(void){return perfil;}
int ajustes_livetv_formato(void){return 0;}
const char *dados_dir(void){return pasta;}
char *dados_caminho(char *out,unsigned n,const char *nome){int k=snprintf(out,n,"%s/%s",pasta,nome);return k>=0&&(unsigned)k<n?out:NULL;}
char *dados_ler(const char *nome){char cam[1024];assert(dados_caminho(cam,sizeof cam,nome));FILE *f=fopen(cam,"rb");if(!f)return NULL;
  assert(!fseek(f,0,SEEK_END));long n=ftell(f);assert(n>=0);rewind(f);char *b=malloc((size_t)n+1);assert(b);assert(fread(b,1,(size_t)n,f)==(size_t)n);b[n]=0;fclose(f);return b;}
int dados_gravar(const char *nome,const char *body){if(falhar)return 0;char cam[1024];assert(dados_caminho(cam,sizeof cam,nome));FILE *f=fopen(cam,"wb");assert(f);
  assert(fwrite(body,1,strlen(body),f)==strlen(body));return fclose(f)==0;}
int dados_gravar_leve(const char *nome,const char *body){return dados_gravar(nome,body);}
int dados_apagar(const char *nome){char cam[1024];assert(dados_caminho(cam,sizeof cam,nome));return unlink(cam)==0;}

char *rede_baixar_st_retry(const char *url,int s,const char *const *h,int *st,int *retry){
  (void)s;(void)h;*st=200;*retry=0;int b=strstr(url,"b.invalid")!=NULL;
  if(strstr(url,"get_live_categories"))return strdup("[{\"category_id\":1,\"category_name\":\"News\"}]");
  if(strstr(url,"get_live_streams"))return strdup("[{\"stream_id\":7,\"name\":\"News HD\",\"category_id\":1,\"epg_channel_id\":\"news\"}]");
  if(strstr(url,"get_short_epg"))return strdup(b?"{\"epg_listings\":[{\"title\":\"Qg==\",\"start_timestamp\":100,\"stop_timestamp\":200}]}":"{\"epg_listings\":[{\"title\":\"QQ==\",\"start_timestamp\":100,\"stop_timestamp\":200}]}");
  return strdup(b?"{\"user_info\":{\"auth\":1,\"status\":\"Active\",\"allowed_output_formats\":[\"m3u8\"]}}":"{\"user_info\":{\"auth\":1,\"status\":\"Active\",\"allowed_output_formats\":[\"ts\"]}}");
}
char *rede_baixar_bin_medido_controle(const char *url,int s,const char *const *h,const RedeControle *ctl,long *n,RedeMedida *med){
  (void)s;(void)h;(void)ctl;med->status=200;int b=strstr(url,"b.invalid")!=NULL;
  const char *body=b?"#EXTM3U url-tvg=\"https://b.invalid/epg\"\n#EXTINF:-1 tvg-id=\"news\",News HD\n#EXTVLCOPT:http-referrer=https://b.invalid/\nhttps://b.invalid/play\n":"#EXTM3U url-tvg=\"https://a.invalid/epg\"\n#EXTINF:-1 tvg-id=\"news\",News HD\n#EXTVLCOPT:http-referrer=https://a.invalid/\nhttps://a.invalid/play\n";
  *n=(long)strlen(body);return strdup(body);
}
char *rede_baixar_com(const char *url,int s,const char *const *h){
  (void)s;(void)h;int b=strstr(url,"b.invalid")!=NULL;
  if(strstr(url,"action=handshake"))return strdup("{\"js\":1,\"token\":\"TOKEN\"}");
  if(strstr(url,"action=get_profile"))return strdup("{\"js\":{}}");
  if(strstr(url,"action=get_genres"))return strdup("{\"js\":[{\"id\":\"1\",\"title\":\"News\"}]}");
  if(strstr(url,"action=get_ordered_list"))return strdup(strstr(url,"&p=1")?"{\"js\":{\"data\":[{\"id\":\"7\",\"name\":\"News\",\"cmd\":\"ffmpeg http://localhost/ch/7\",\"tv_genre_id\":\"1\"}]}}":"{\"js\":{\"data\":[]}}");
  if(strstr(url,"action=create_link"))return strdup(b?"{\"js\":{\"cmd\":\"ffmpeg https://b.invalid/stalker/play\"}}":"{\"js\":{\"cmd\":\"ffmpeg https://a.invalid/stalker/play\"}}");
  return NULL;
}
static void xtConfig(unsigned id,const char *server,const char *user){xtream_fonte_selecionar(id);xtream_definir_servidor(server);xtream_definir_usuario(user);xtream_definir_senha("dummy");}
static void stConfig(unsigned id,const char *server){stalker_fonte_selecionar(id);stalker_definir_portal(server);stalker_definir_mac("00:1a:79:00:00:01");}
int main(int argc,char **argv){
  assert(argc==2);pasta=argv[1];assert(!mkdir(pasta,0700));
  xtConfig(0,"https://a.invalid","alice");m3u_fonte_selecionar(0);assert(m3u_definir_lista("https://a.invalid/list"));stConfig(0,"a.invalid");
  IptvConta c[32],selected;assert(iptv_contas(c,32)==3);for(int i=0;i<3;i++)assert(c[i].id==0&&c[i].ativa);
  assert(iptv_conta_adicionar(IPTV_XTREAM));assert(iptv_conta_selecionada(&selected));unsigned xb=selected.id;assert(xb);
  xtConfig(xb,"https://b.invalid","bob");
  XtreamCanal xa[2],xbch[2];assert(xtream_fonte_canais(0,xa,2,sessao)==1);assert(xtream_fonte_canais(xb,xbch,2,sessao)==1);
  assert(!strcmp(xa[0].id,"xtream:7")&&strcmp(xa[0].id,xbch[0].id));
  char url[4096];assert(xtream_url_formato(xa[0].id,"ts",url,sizeof url)&&strstr(url,"a.invalid/live/alice/dummy/7.ts"));
  assert(xtream_url_formato(xbch[0].id,"m3u8",url,sizeof url)&&strstr(url,"b.invalid/live/bob/dummy/7.m3u8"));
  const char *ext[2];assert(xtream_formatos_canal(xa[0].id,ext)==1&&!strcmp(ext[0],"ts"));
  assert(xtream_formatos_canal(xbch[0].id,ext)==1&&!strcmp(ext[0],"m3u8"));
  XtreamProg p[2];int status;assert(xtream_epg_curto(xa[0].id,p,2,&status)==1&&!strcmp(p[0].titulo,"A"));
  assert(xtream_epg_curto(xbch[0].id,p,2,&status)==1&&!strcmp(p[0].titulo,"B"));
  puts("PASS: two Xtream accounts with overlapping stream/EPG IDs, formats and editing selection");

  assert(iptv_conta_adicionar(IPTV_M3U));assert(iptv_conta_selecionada(&selected));unsigned mb=selected.id;
  m3u_fonte_selecionar(mb);assert(m3u_definir_lista("https://b.invalid/list"));
  M3uCanal ma[2],mbch[2];assert(m3u_fonte_canais(0,ma,2,sessao)==1);assert(m3u_fonte_canais(mb,mbch,2,sessao)==1);
  assert(strlen(ma[0].id)==22&&strcmp(ma[0].id,mbch[0].id));
  assert(m3u_resolver_sessao(ma[0].id,url,sizeof url,sessao,0)&&!strcmp(url,"https://a.invalid/play"));
  assert(m3u_resolver_sessao(mbch[0].id,url,sizeof url,sessao,0)&&!strcmp(url,"https://b.invalid/play"));
  assert(m3u_cabecalhos(ma[0].id,url,sizeof url)&&strstr(url,"a.invalid"));
  assert(m3u_cabecalhos(mbch[0].id,url,sizeof url)&&strstr(url,"b.invalid"));
  assert(m3u_fonte_epg(0,url,sizeof url)&&strstr(url,"a.invalid/epg"));
  assert(m3u_fonte_epg(mb,url,sizeof url)&&strstr(url,"b.invalid/epg"));
  puts("PASS: two M3U accounts retain separate URLs, headers, caches and automatic EPG");

  assert(iptv_conta_adicionar(IPTV_STALKER));assert(iptv_conta_selecionada(&selected));unsigned sb=selected.id;stConfig(sb,"b.invalid");
  StalkerCanal sa[2],sbch[2];assert(stalker_fonte_canais(0,sa,2,sessao)==1);assert(stalker_fonte_canais(sb,sbch,2,sessao)==1);
  assert(!strcmp(sa[0].id,"stalker:7")&&strcmp(sa[0].id,sbch[0].id));
  assert(stalker_resolver_sessao(sa[0].id,url,sizeof url,sessao)&&strstr(url,"a.invalid/stalker/play"));
  assert(stalker_resolver_sessao(sbch[0].id,url,sizeof url,sessao)&&strstr(url,"b.invalid/stalker/play"));
  puts("PASS: two Stalker accounts keep command tables, handshakes and playback separate");

  int n=iptv_contas(c,32);assert(n==6);assert(iptv_conta_renomear("Second portal"));assert(iptv_conta_ativar(0));
  assert(iptv_conta_selecionada(&selected)&&!selected.ativa&&!strcmp(selected.nome,"Second portal"));
  assert(!iptv_conta_renomear("bad\nname"));falhar=1;assert(!iptv_conta_adicionar(IPTV_XTREAM)&&iptv_contas(NULL,0)==n);falhar=0;
  IptvConta antesOrdem[IPTV_MAX_CONTAS];assert(iptv_contas(antesOrdem,IPTV_MAX_CONTAS)==n);
  assert(iptv_conta_mover(n-1,-1));assert(iptv_contas(c,32)==n && c[n-2].id==sb);
  assert(iptv_conta_selecionada(&selected) && selected.id==sb && !selected.ativa);
  falhar=1;assert(!iptv_conta_mover(n-2,-1));falhar=0;
  assert(iptv_contas(c,32)==n && c[n-2].id==sb);
  assert(!iptv_conta_mover(-1,1) && !iptv_conta_mover(2147483647,1) && !iptv_conta_mover(0,0));
  sessao++;assert(iptv_contas(c,32)==n && c[n-2].id==sb);
  assert(iptv_conta_mover(n-2,1));assert(iptv_contas(c,32)==n);
  for(int i=0;i<n;i++)assert(c[i].id==antesOrdem[i].id && c[i].tipo==antesOrdem[i].tipo && c[i].ativa==antesOrdem[i].ativa && !strcmp(c[i].nome,antesOrdem[i].nome));
  puts("PASS: account reorder persists without changing identities/selected account, failed writes roll back, invalid indices are rejected");
  perfil=2;sessao++;assert(!iptv_contas(NULL,0));assert(!xtream_fonte_configurado(xb));assert(!m3u_fonte_configurado(mb));assert(!stalker_fonte_configurado(sb));
  perfil=1;sessao++;assert(iptv_contas(c,32)==n);assert(iptv_conta_selecionada(&selected)&&selected.id==sb&&!selected.ativa);
  assert(iptv_conta_remover(&selected)&&selected.id==sb);stalker_fonte_esquecer(sb);
  assert(iptv_conta_adicionar(IPTV_STALKER)&&iptv_conta_selecionada(&selected)&&selected.id>sb);
  assert(dados_gravar("keep.txt","keep"));
  // Source-keyed guide files can contain private playback URLs. Account IDs
  // restart after logout; neither those caches nor URL configurations survive.
  const char *privateGuide[]={"ltv-xta00000001-p1.bin","ltv-st-p2.bin","ltv-sta00000005-p2.bin",
    "ltv-ma00000003-p1.bin","ltv-m12345678-p2.bin","epg-x-ma00000003-p1.bin",
    "epg-x-xta00000001-p1.bin","ltv-urls-p2.txt","ltv-fontes-p1.txt","ltv-indice-p1.txt"};
  for(unsigned i=0;i<sizeof privateGuide/sizeof *privateGuide;i++)assert(dados_gravar(privateGuide[i],"private-fixture"));
  assert(dados_gravar("ltv-a12345678-p1.bin","public-addon-cache"));
  iptv_contas_esquecer_todas();
  for(unsigned i=0;i<sizeof privateGuide/sizeof *privateGuide;i++)assert(!dados_ler(privateGuide[i]));
  char *publicCache=dados_ler("ltv-a12345678-p1.bin");assert(publicCache && !strcmp(publicCache,"public-addon-cache"));free(publicCache);
  xtream_fontes_invalidar();m3u_fontes_invalidar();stalker_fontes_invalidar();
  assert(!iptv_contas(NULL,0));assert(!xtream_fonte_configurado(0)&&!m3u_fonte_configurado(0)&&!stalker_fonte_configurado(0));
  char *keep=dados_ler("keep.txt");assert(keep&&!strcmp(keep,"keep"));free(keep);
  puts("PASS: legacy migration, metadata persistence, failed-write rollback, profile isolation, stable deletion IDs and logout cleanup");
  xtream_fonte_selecionar(0);assert(xtream_definir_servidor("https://kept.invalid"));
  stalker_fonte_selecionar(0);assert(stalker_definir_portal("kept.invalid"));
  falhar=1;
  assert(!xtream_definir_servidor("https://lost.invalid"));assert(strstr(xtream_servidor_curto(),"kept.invalid"));
  assert(!stalker_definir_portal("lost.invalid"));assert(strstr(stalker_portal_curto(),"kept.invalid"));
  falhar=0;
  puts("PASS: failed first-field writes are reported and preserve existing Xtream/Stalker addresses");
  assert(xtream_conta_definir("https://kept.invalid","saved-user","saved-password"));
  assert(stalker_conta_definir("kept.invalid","00:1a:79:00:00:02"));
  m3u_fonte_selecionar(0);assert(m3u_conta_definir("https://kept.invalid/list","https://kept.invalid/epg"));
  falhar=1;
  assert(!xtream_conta_definir("https://lost.invalid","lost-user","lost-password"));
  assert(strstr(xtream_servidor_curto(),"kept.invalid") && !strcmp(xtream_usuario(),"saved-user"));
  assert(!stalker_conta_definir("lost.invalid","00:1a:79:00:00:03"));assert(strstr(stalker_portal_curto(),"kept.invalid"));
  assert(!m3u_conta_definir("https://lost.invalid/list","https://lost.invalid/epg"));
  assert(!strcmp(m3u_lista(),"https://kept.invalid/list") && !strcmp(m3u_epg(),"https://kept.invalid/epg"));
  falhar=0;
  assert(!xtream_conta_definir("", "saved-user", "saved-password"));
  assert(!xtream_conta_definir("kept.invalid", "bad\nuser", "saved-password"));
  assert(!stalker_conta_definir("kept.invalid","invalid"));
  assert(!m3u_conta_definir("ftp://invalid/list",""));
  sessao++; // Reload from disk: failed writes must not change persisted values either.
  assert(!strcmp(xtream_usuario(),"saved-user"));
  assert(!strcmp(m3u_epg(),"https://kept.invalid/epg"));
  puts("PASS: complete-account Save commits all fields together; failed or invalid saves preserve previous credentials in memory and on disk");
  perfil=3;sessao++;
  assert(dados_gravar("xtream-p3.txt","# credential\nservidor\t\nusuario\t\nsenha\t\n"));
  assert(dados_gravar("m3u-p3.txt","lista\t\nepg\t\n"));
  assert(dados_gravar("stalker-p3.txt","# credential\nportal\t\nmac\t\ndevice\t\nserial\t\n"));
  assert(!iptv_contas(NULL,0));
  assert(dados_gravar("m3u-p3-s9.txt","lista\thttps://kept.invalid/list\nepg\thttps://kept.invalid/epg\n"));
  assert(dados_gravar("stalker-p3-s10.txt","portal\thttp://partial.invalid\nmac\t\n"));
  assert(dados_gravar("livetv-contas-p3.txt","next\t11\nselected\t3\naccount\t1\t1\t1\tXtream 1\naccount\t2\t2\t1\tM3U 1\naccount\t9\t2\t1\tSaved playlist\naccount\t10\t3\t1\tPartial portal\n"));
  sessao++;assert(iptv_contas(c,32)==2 && c[0].id==9 && c[1].id==10);
  assert(iptv_conta_selecionada(&selected) && selected.id==10);
  char *saved=dados_ler("m3u-p3-s9.txt");assert(saved && strstr(saved,"https://kept.invalid/list"));free(saved);
  saved=dados_ler("stalker-p3-s10.txt");assert(saved && strstr(saved,"http://partial.invalid"));free(saved);
  sessao++;assert(iptv_contas(NULL,0)==2 && iptv_conta_selecionada(&selected) && selected.id==10);
  assert(iptv_conta_adicionar(IPTV_M3U) && iptv_conta_selecionada(&selected) && selected.id==11);
  puts("PASS: empty legacy files do not create slots; empty preview metadata is removed without touching saved or partial credentials, selection or monotonic IDs");
  return 0;
}
