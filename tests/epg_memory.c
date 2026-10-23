#include "epg_core_fixture.h"
static void programa(char *xml,size_t cap,size_t *n,time_t ini,time_t fim,const char *nome) {
  char a[32],b[32];tempo(a,ini);tempo(b,fim);
  int k=snprintf(xml+*n,cap-*n,"<programme channel=\"news\" start=\"%s\" stop=\"%s\"><title>%s</title></programme>",a,b,nome);
  assert(k>0 && (size_t)k<cap-*n);*n+=(size_t)k;
}
static const char *resposta;
static int transmitir(const char *url,int (*cb)(const char *,size_t,void *),void *u,int *st,long long *bytes) {
  (void)url;*st=200;*bytes=(long long)strlen(resposta);return cb(resposta,strlen(resposta),u);
}
int main(int argc,char **argv) {
  assert(argc==2);pasta=argv[1];assert(!mkdir(pasta,0700));responder=transmitir;
  epg_memoria_definir(EPG_MEMORIA_AUTO,1024);assert(epgMemoriaBaixa);
  epg_memoria_definir(EPG_MEMORIA_AUTO,0);assert(epgMemoriaBaixa);
  epg_memoria_definir(EPG_MEMORIA_AUTO,8192);assert(!epgMemoriaBaixa);
  epg_memoria_definir(EPG_MEMORIA_COMPLETA,512);assert(!epgMemoriaBaixa);
  epg_memoria_definir(EPG_MEMORIA_BAIXA,8192);assert(epgMemoriaBaixa);
  epg_memoria_definir(EPG_MEMORIA_COMPLETA,8192);
  carga.memoriaBaixa=0;fioVivo=1;fontesMudaram=0;reiniciarPend=0;
  epg_memoria_definir(EPG_MEMORIA_BAIXA,8192);
  assert(carga.cancelada && reiniciarPend && fontesMudaram && !carga.memoriaBaixa);
  fioVivo=0;carga.cancelada=0;reiniciarPend=0;
  char xml[4096];size_t n=(size_t)snprintf(xml,sizeof xml,"<tv><channel id=\"news\"><display-name>News</display-name></channel>");
  time_t now=time(NULL);programa(xml,sizeof xml,&n,now-7200,now,"Expired");
  programa(xml,sizeof xml,&n,now-7200,now+900,"Current");
  programa(xml,sizeof xml,&n,now+900,now+3600,"Next");
  programa(xml,sizeof xml,&n,now+86399,now+90000,"Boundary");
  programa(xml,sizeof xml,&n,now+86400,now+90000,"Tomorrow");
  programa(xml,sizeof xml,&n,now+172800,now+180000,"Later");
  snprintf(xml+n,sizeof xml-n,"</tv>");
  EpgProc pr={.g=&W,.agora=now};
  carga.memoriaBaixa=0;assert(wAbrir(&W));assert(processarBuf(&pr,xml,(long)strlen(xml)));wFechar(&W);
  assert(W.nCanais==1 && W.nEvs==6);gradeSoltar(&W);
  pr=(EpgProc){.g=&W,.agora=now};carga.memoriaBaixa=1;assert(wAbrir(&W));
  assert(processarBuf(&pr,xml,(long)strlen(xml)));wFechar(&W);assert(W.nCanais==1 && W.nEvs==3);
  assert(!strcmp(W.arena+W.evs[0].tit,"Current"));assert(!strcmp(W.arena+W.evs[2].tit,"Boundary"));
  gradeSoltar(&W);
  puts("PASS: automatic/explicit device memory policy, immutable worker snapshot, current programme and strict24-hour boundaries retain all channel metadata");
  resposta=xml;EpgExtra fonte={.chave="memory",.url="https://guide.invalid/memory"};
  epg_abrir_sessao(1);epg_so_provedores(1);epg_memoria_definir(EPG_MEMORIA_COMPLETA,8192);
  epg_extras_definir(&fonte,1);epg_iniciar();esperar();assert(P.nCanais==1 && P.nEvs==6);
  epg_memoria_definir(EPG_MEMORIA_BAIXA,8192);epg_passo();esperar();assert(P.nCanais==1 && P.nEvs==3);
  assert(!strcmp(P.arena+P.evs[0].tit,"Current"));
  epg_memoria_definir(EPG_MEMORIA_COMPLETA,8192);epg_passo();esperar();assert(P.nCanais==1 && P.nEvs==6);
  assert(pedidos==3);epg_abrir_sessao(1);
  puts("PASS: mode changes rebuild filtered binary caches; returning to Full retrieves later programmes rather than reusing a shortened24-hour schedule");
  return 0;
}
