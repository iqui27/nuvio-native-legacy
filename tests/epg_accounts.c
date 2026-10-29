#include "epg_core_fixture.h"
static atomic_int falhar, lenta, liberar;
void epg_shared_consumers(void);
static int conta(const char *url,int (*cb)(const char *,size_t,void *),void *u,int *st,long long *bytes) {
  *bytes=0;*st=503;if(atomic_load(&falhar) && !strstr(url,"shared"))return 0;
  if(strstr(url,"slow")) {
    atomic_store(&lenta,1);
    while(!atomic_load(&liberar) && cargaAtual()) {struct timespec t={0,1000000};nanosleep(&t,NULL);}
    if(!cargaAtual())return 0;
  }
  const char *titulo=strstr(url,"one")?"Account one":"Account two";
  char a[32],b[32],xml[1600];time_t now=time(NULL);tempo(a,now-600);tempo(b,now+3600);
  int n=snprintf(xml,sizeof xml,"<tv><channel id=\"news\"><display-name>News HD</display-name></channel>"
    "<channel id=\"private\"><display-name>Private News</display-name></channel>"
    "<programme channel=\"news\" start=\"%s\" stop=\"%s\"><title>%s</title></programme>"
    "<programme channel=\"private\" start=\"%s\" stop=\"%s\"><title>%s</title></programme></tv>",a,b,titulo,a,b,titulo);
  *st=200;for(int i=0;i<n;i+=17) {size_t k=(size_t)(n-i<17?n-i:17);if(!cb(xml+i,k,u))return 0;*bytes+=(long long)k;}
  return 1;
}
static int programa(const char *id,const char *titulo) {
  int idx=epg_match_canal(id,"news","News HD");EpgProg p;assert(idx>=0);
  assert(epg_agora(idx,time(NULL),&p) && !strcmp(p.titulo,titulo));return idx;
}
int main(int argc,char **argv) {
  assert(argc==2);pasta=argv[1];assert(!mkdir(pasta,0700));responder=conta;
  EpgContaFonte fs[]={{1,1,"https://epg.invalid/one"},{2,2,"https://epg.invalid/two"}};
  epg_abrir_sessao(1);epg_so_provedores(1);epg_contas_configurar(fs,2);epg_iniciar();esperar();
  int antigo=programa("xtream:@00000001:7","Account one");
  programa("nvm3u:@00000002:7","Account two");assert(pedidos==2);
  assert(epg_match_canal("xtream:@00000003:7","news","News HD")==-1);
  assert(epg_casar("xta00000003","private",NULL,"Private News",NULL)==-1);
  assert(epg_casar("xta00000003","missing",NULL,"Private News HD",NULL)==-1);
  unsigned rev=epg_revisao();epg_contas_configurar(fs+1,1);assert(epg_revisao()!=rev);
  EpgProg p;assert(!epg_agora(antigo,time(NULL),&p));
  assert(epg_match_canal("xtream:@00000001:7","news","News HD")==-1);
  programa("nvm3u:@00000002:7","Account two");epg_passo();esperar();
  puts("PASS: duplicate IDs/names stay account-scoped; missing/removed accounts cannot borrow another private schedule");
  epg_abrir_sessao(1);epg_so_provedores(1);epg_contas_configurar(fs,2);epg_iniciar();int antes=pedidos;esperar();assert(pedidos==antes);
  programa("xtream:@00000001:7","Account one");programa("nvm3u:@00000002:7","Account two");
  atomic_store(&falhar,1);antes=pedidos;epg_recarregar();esperar();assert(pedidos==antes+2);
  programa("xtream:@00000001:7","Account one");programa("nvm3u:@00000002:7","Account two");
  EpgFonteInfo infos[2];assert(epg_fontes_info(infos,2)==2 && infos[0].deCache && infos[1].deCache);
  puts("PASS: cold restart reads private binary caches; explicit refresh retries network and retains working fallback on failure");
  epg_abrir_sessao(2);epg_so_provedores(1);epg_contas_configurar(fs,2);epg_iniciar();esperar();assert(epg_estado()==EPG_FALHOU);
  assert(epg_match_canal("xtream:@00000001:7","news","News HD")==-1);
  epg_abrir_sessao(1);epg_so_provedores(1);strcpy(fs[0].url,"https://epg.invalid/changed");
  epg_contas_configurar(fs,1);epg_iniciar();esperar();assert(epg_estado()==EPG_FALHOU);
  puts("PASS: changed credential URLs and different profiles cannot read another private cache");
  atomic_store(&falhar,0);strcpy(fs[0].url,"https://epg.invalid/slow/one");
  epg_contas_configurar(fs,1);epg_iniciar();
  for(int i=0;i<5000 && !atomic_load(&lenta);i++){struct timespec t={0,1000000};nanosleep(&t,NULL);}
  assert(atomic_load(&lenta));epg_contas_configurar(fs+1,1);atomic_store(&liberar,1);esperar();
  assert(epg_match_canal("xtream:@00000001:7","news","News HD")==-1);
  programa("nvm3u:@00000002:7","Account two");epg_abrir_sessao(1);
  puts("PASS: replacing sources during a slow load cancels the stale publication and reloads the current list");
  EpgExtra shared[]={{.chave="x12345678",.url="https://epg.invalid/shared"},
                     {.chave="xta00000001",.url="https://epg.invalid/one"}};
  epg_so_provedores(1);epg_extras_definir(shared,1);epg_iniciar();esperar();
  int sharedNews=epg_match_id("news"),sharedPrivate=epg_match_id("private");
  assert(sharedNews>=0 && sharedPrivate>=0);
  const char *sources[]={"xt","xta00000003","st","sta00000003","ma00000003","m12345678","a12345678"};
  for(size_t i=0;i<sizeof sources/sizeof sources[0];i++) {
    int method;
    assert(epg_casar(sources[i],"news",NULL,NULL,&method)==sharedNews && method==EPG_M_ID);
    assert(epg_casar(sources[i],NULL,"News HD",NULL,&method)==sharedNews && method==EPG_M_NOME);
    assert(epg_casar(sources[i],NULL,NULL,"Private N",&method)==sharedPrivate && method==EPG_M_ALIAS);
    assert(epg_casar(sources[i],NULL,NULL,"Private News Local",&method)==sharedPrivate && method==EPG_M_ALIAS);
  }
  programa("xtream:@00000003:7","Account two");
  programa("stalker:@00000003:7","Account two");
  programa("nvm3u:@00000003:7","Account two");
  epg_extras_definir(shared,2);epg_iniciar();esperar();
  programa("xtream:@00000001:7","Account one");
  programa("xtream:@00000003:7","Account two");
  epg_abrir_sessao(1);
  puts("PASS: standalone XMLTV matches IDs, names and rules across sources; own account schedules retain priority");
  // Fresh profile: provider fails without cache, standalone XMLTV still works.
  epg_abrir_sessao(3);epg_so_provedores(1);atomic_store(&falhar,1);
  epg_extras_definir(shared,2);epg_iniciar();esperar();
  assert(epg_estado()==EPG_PRONTO);
  assert(epg_fonte_estado("xta00000001")==EPG_FALHOU);
  assert(epg_canal_estado("xtream:@00000001:7")==EPG_FALHOU);
  assert(epg_casar("xta00000001","news",NULL,"News HD",NULL)==0);
  programa("xtream:@00000001:7","Account two");
  epg_shared_consumers();
  epg_abrir_sessao(3);
  puts("PASS: failed provider does not hide shared programmes in guide, player or programme search; unmatched channels retain failure");
  return 0;
}
