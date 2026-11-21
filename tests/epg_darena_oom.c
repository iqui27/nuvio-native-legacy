// (#776) Falha de alocacao da arena de sinopses nao pode derrubar a grade:
// a sinopse e opcional. Sem a darena (inicial ou crescimento), todos os
// programas continuam publicados, so sem descricao. --wrap=malloc injeta a
// falha exatamente nos 64 KiB iniciais (EPG_DARENA_INI).
#include "epg_core_fixture.h"
static int falhar_darena, darena_falhou;
void *__real_malloc(size_t n);
void *__wrap_malloc(size_t n) {
  // So a primeira alocacao de 64 KiB — a da arena de sinopses em wAbrir —
  // falha; qualquer outro malloc desse tamanho segue normal.
  if (falhar_darena && !darena_falhou && n == (size_t)EPG_DARENA_INI) {
    darena_falhou = 1;
    return NULL;
  }
  return __real_malloc(n);
}
static const char *resposta;
static int transmitir(const char *url,int (*cb)(const char *,size_t,void *),void *u,int *st,long long *bytes) {
  (void)url;*st=200;*bytes=(long long)strlen(resposta);return cb(resposta,strlen(resposta),u);
}
int main(int argc,char **argv) {
  assert(argc==2);pasta=argv[1];assert(!mkdir(pasta,0700));responder=transmitir;
  char xml[8192];size_t n=(size_t)snprintf(xml,sizeof xml,"<tv><channel id=\"news\"><display-name>News</display-name></channel>");
  time_t now=time(NULL);
  for(int i=0;i<4;i++){
    char a[32],b[32];tempo(a,now+i*3600);tempo(b,now+i*3600+1800);
    int k=snprintf(xml+n,sizeof xml-n,"<programme channel=\"news\" start=\"%s\" stop=\"%s\"><title>Prog %d</title><desc>Sinopse %d</desc></programme>",a,b,i,i);
    assert(k>0 && (size_t)k<sizeof xml-n);n+=(size_t)k;
  }
  snprintf(xml+n,sizeof xml-n,"</tv>");resposta=xml;
  EpgExtra fonte={.chave="oom",.url="https://guide.invalid/oom"};
  falhar_darena=1;
  epg_abrir_sessao(1);epg_so_provedores(1);epg_memoria_definir(EPG_MEMORIA_COMPLETA,8192);
  epg_extras_definir(&fonte,1);epg_iniciar();esperar();
  assert(P.nCanais==1 && P.nEvs==4);   // os quatro programas continuam
  for(int i=0;i<P.nEvs;i++){EpgProg p;pubProg(i,&p);}
  puts("PASS: falha de alocacao da arena de sinopses nao derruba a grade; programas sobem sem descricao");
  epg_abrir_sessao(1);
  return 0;
}
