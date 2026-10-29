#include "epg_core_fixture.h"
static const char *resposta;static long tamanho;static int indisponivel;
static int transmitir(const char *url,int (*cb)(const char *,size_t,void *),void *u,int *st,long long *bytes) {
  (void)url;*bytes=0;*st=indisponivel?503:200;if(indisponivel)return 0;
  for(long i=0;i<tamanho;i+=513) {size_t n=(size_t)(tamanho-i<513?tamanho-i:513);if(!cb(resposta+i,n,u))return 0;*bytes+=(long long)n;}
  return 1;
}
static const char *cab="<?xml version=\"1.0\"?><tv><channel\n id=\"news\"><display-name>News HD</display-name></channel>";
static char prog[512];
static void comprimir(z_stream *z,const char *s,size_t n) {
  z->next_in=(Bytef *)s;z->avail_in=(uInt)n;
  while(z->avail_in)assert(deflate(z,Z_NO_FLUSH)==Z_OK && z->avail_out);
}
static char *grande(long *n) {
  const size_t cap=2*1024*1024;char *out=malloc(cap),bloco[65536];assert(out);memset(bloco,'x',sizeof bloco);
  z_stream z={0};assert(deflateInit2(&z,1,Z_DEFLATED,31,8,Z_DEFAULT_STRATEGY)==Z_OK);
  z.next_out=(Bytef *)out;z.avail_out=(uInt)cap;
  comprimir(&z,cab,strlen(cab));comprimir(&z,"<!--",4);
  for(int i=0;i<2560;i++)comprimir(&z,bloco,sizeof bloco);
  comprimir(&z,"-->",3);comprimir(&z,prog,strlen(prog));
  assert(deflate(&z,Z_FINISH)==Z_STREAM_END);*n=(long)z.total_out;deflateEnd(&z);return out;
}
static void conferir(const char *body,long n,size_t split) {
  EpgProc pr={.g=&W,.agora=time(NULL)};Xmltv x={0};assert(wAbrir(&W));
  x.canal=cbCanal;x.programa=cbPrograma;x.u=&pr;assert(xmltv_iniciar(&x));
  if(split) {assert(xmltv_alimentar(&x,body,split));assert(xmltv_alimentar(&x,body+split,(size_t)n-split));}
  else for(long i=0;i<n;i+=777)assert(xmltv_alimentar(&x,body+i,(size_t)(n-i<777?n-i:777)));
  xmltv_fim(&x);wFechar(&W);assert(W.nCanais==1 && W.nEvs==1);
  assert(!strcmp(W.arena+W.evs[0].tit,"News & weather"));gradeSoltar(&W);
}
int main(int argc,char **argv) {
  assert(argc==2);pasta=argv[1];assert(!mkdir(pasta,0700));responder=transmitir;
  char a[32],b[32];time_t now=time(NULL);tempo(a,now-600);tempo(b,now+3600);
  snprintf(prog,sizeof prog,"<programme\n channel=\"news\" start=\"%s\" stop=\"%s\"><title>News &amp; weather</title></programme></tv>",a,b);
  char *body=grande(&tamanho);conferir(body,tamanho,0);
  puts("PASS:160MiB expanded gzip consumes bounded parser windows, preserves split comments/tags and programme entities");
  char simples[1024];snprintf(simples,sizeof simples,"%s%s",cab,prog);
  for(size_t split=1;split<strlen(simples);split++)conferir(simples,(long)strlen(simples),split);
  size_t grandeN=strlen(cab)+XMLTV_ELEM*2+strlen(prog)+128;char *registro=malloc(grandeN);assert(registro);
  size_t off=(size_t)sprintf(registro,"%s<programme channel=\"news\"><desc>",cab);
  memset(registro+off,'x',XMLTV_ELEM*2);off+=XMLTV_ELEM*2;
  off+=(size_t)sprintf(registro+off,"</desc></programme>%s",prog);conferir(registro,(long)off,0);free(registro);
  puts("PASS: every plain-XML split boundary and oversized-record recovery preserve the following complete programme");
  resposta=body;epg_abrir_sessao(1);epg_so_provedores(1);
  const char *ids[]={"NEWS"};EpgExtra fonte={.chave="large",.url="https://guide.invalid/one",.ids=ids,.nIds=1};
  epg_extras_definir(&fonte,1);epg_iniciar();esperar();assert(epg_estado()==EPG_PRONTO && epg_n_canais()==1);
  int pedidosAntes=pedidos;epg_abrir_sessao(1);epg_so_provedores(1);epg_extras_definir(&fonte,1);epg_iniciar();esperar();assert(pedidos==pedidosAntes);
  indisponivel=1;epg_recarregar();esperar();assert(pedidos==pedidosAntes+1 && epg_estado()==EPG_PRONTO);
  int canal=epg_casar("large","NEWS",NULL,NULL,NULL);EpgProg p;assert(canal>=0 && epg_agora(canal,time(NULL),&p));
  // A successful HTTP response with no XMLTV must not replace a good cache.
  indisponivel=0;resposta="<html>Login</html>";tamanho=(long)strlen(resposta);epg_recarregar();esperar();
  canal=epg_casar("large","NEWS",NULL,NULL,NULL);assert(canal>=0 && epg_agora(canal,time(NULL),&p));
  epg_abrir_sessao(1);free(body);
  puts("PASS: large provider stream uses filtered binary cache on restart; failed/invalid refresh keeps the last working schedule");return 0;
}
