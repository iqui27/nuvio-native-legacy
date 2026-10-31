#include "aiostreamsui.h"
#include "aiopair.h"
#include "addons.h"
#include "addonstats.h"
#include "aionotice.h"
#include "ajustes.h"
#include "celular.h"
#include "celbotao.h"
#include "descoberta.h"
#include "fileiras.h"
#include "gfx.h"
#include "idioma.h"
#include "js.h"
#include "layout.h"
#include "tela21.h"
#include "revela.h"
#include "botoes.h"
#include "teclavoltar.h"
#include "perfis.h"
#include "ponteiro.h"
#include "focus.h"
#include "qr.h"
#include "rede.h"
#include "sessao.h"
#include "teclado.h"
#include "text.h"
#include "tex_cache.h"
#include "streams.h"
#include "addonsui.h"
#include "sync.h"
#include "contacache.h"
#include "jsw.h"
#include "linguas.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <time.h>

enum { SEM, INSTANCIA, IMPORTAR, SERVICOS, PRONTO, PAINEL, CATALOGOS, FONTES, LINKS, LEGENDAS, FILTROS, SAUDE, TITULO };
static const char *const titulos[] = {"Conectar", "Escolha uma instância", "Importar configuração", "Serviços e preferências", "Pronto", "Tudo que o AIOStreams traz", "Catálogos", "Fontes e serviços", "Links", "Legendas", "Filtros e ordenação", "Saúde", "Inspeção do título"};
static const char *const icons[] = {"aj_layout-dashboard","aj_library","aj_layers","aj_link","aj_captions","aj_sliders-horizontal","aj_activity"};
static const char *const hosts[] = {"https://aiostreams.fortheweak.cloud","https://aiostreamsfortheweebsstable.midnightignite.me","https://aiostreams.elfhosted.com","https://aiostreams.viren070.me","https://aiostreams.stremio.ru","https://aio.atbphosting.com","https://aiostreams.12312023.xyz"};
static const char *const nomes[] = {"Yeb’s", "Midnight’s", "ElfHosted", "Viren’s", "Kuu’s", "ATBP Hosting", "Omni’s"};
static int aberto, pagina, foco, lateral, hostIdx, indice = -1, entrada, pendente, perfil;
static Uint32 proximaConsulta;
static GfxRect navigationRects[256];
static int navigationIds[256], navigationCount;
static struct { int page, focus, side; } history[64];
static int historyCount;
static void navigationTarget(float x,float y,float w,float h,PonteiroFn focusFn,PonteiroFn activate,int a,int b) {
  if (focusFn && navigationCount < 256) {
    navigationRects[navigationCount]=(GfxRect){x,y,w,h};
    navigationIds[navigationCount++]=a+1000*b;
  }
  ponteiro_alvo(x,y,w,h,focusFn,activate,a,b);
}
#define ponteiro_alvo navigationTarget
static int saindo, paginaAnterior=-1, regraVisivel=-1, evidenciaVisivel;
static float sidebarOpen;
static int dashboardReturn, connectOpen, connectFocus;
#ifdef NV_SHOT_HOOKS
static int connectShot;
#endif
static char connectUrl[512];
static int connectState;
static void manualConnect(void);
static float abertura, corpo=1, entradaTempo, scrollFoco, scrollVel;
static float focoAnim[2][AIO_DETAIL_MAX+40];
static float listaPos, deslocamento, addonsFoco[168], addonsScroll, addonsVel;
static Uint32 addonsTick;
static float janela(int manter,int total,int visiveis,int ativo) {
  float pos=ativo ? anim_clamp(scrollFoco-manter,0,fmaxf(0,total-visiveis)) : 0;
  // The spring can settle just below an integer; truncation would hide the focused last row.
  return fabsf(pos-roundf(pos))<.001f ? roundf(pos) : pos;
}
// Row at r slots from the window top fades in/out across one slot, so a fractional offset never pops.
static float rolaAlfa(float r,int vis) { return anim_clamp(r+1,0,1)*anim_clamp(vis-r,0,1); }
static float focoDe(int i,int nav) {
  return i>=0 && i<AIO_DETAIL_MAX+40 ? focoAnim[nav!=0][i] : 0;
}
static int regraSelecionada=-1, evidenciaSelecionada;
static void dashboardMovimento(float dt);
static void movimento(float dt) {
  abertura=t21_entrada(abertura,dt,!saindo);
  dashboardMovimento(dt);
  sidebarOpen=anim_mola(sidebarOpen,lateral?1:0,dt,NV_MOLA_FOCO);
  if(paginaAnterior<0) {
    paginaAnterior=pagina; regraVisivel=regraSelecionada; evidenciaVisivel=evidenciaSelecionada;
    corpo=0; entradaTempo=0;
  }
  int mesma=paginaAnterior==pagina && regraVisivel==regraSelecionada && evidenciaVisivel==evidenciaSelecionada;
  corpo=anim_rampa(corpo,mesma,dt,90);
  if(!mesma && corpo<=0) {
    paginaAnterior=pagina; regraVisivel=regraSelecionada; evidenciaVisivel=evidenciaSelecionada;
    entradaTempo=0; scrollFoco=0; scrollVel=0;
    memset(focoAnim,0,sizeof focoAnim);
  }
  entradaTempo+=dt;
  scrollFoco=anim_mola2(&scrollVel,scrollFoco,lateral?0:foco,dt,NV_MOLA2_SCROLL);
  for(int nav=0;nav<2;nav++) for(int i=0;i<AIO_DETAIL_MAX+40;i++) {
    float target=!saindo && lateral==nav && foco==i;
    focoAnim[nav][i]=anim_mola(focoAnim[nav][i],target,dt,
      target>focoAnim[nav][i]?NV_MOLA_FOCO:NV_MOLA_DESFOCO);
  }
}

static unsigned versao, geracao;
static int catalogoSelecionado, linkSelecionado=-1, maisHosts;
static int soManifesto;   // Configuration source is distinct from API authorization.
static time_t ultimaAtualizacao;
static int dashboardLinha[6], dashboardFoco;
static float dashboardScroll[6], dashboardVel[6];
static int dashboardTotal(int card);
static void dashboard(void);
static void dashboardMovimento(float dt) {
  for(int i=0;i<6;i++) dashboardScroll[i]=anim_mola2(&dashboardVel[i],dashboardScroll[i],dashboardLinha[i],dt,NV_MOLA2_SCROLL);
}
static void trace(void);
static char arteFundo[700], arteLogo[700];
void aioui_arte(const char *dir) {
  snprintf(arteFundo,sizeof arteFundo,"%s/aiostreams/backdrop.jpg",dir);
  snprintf(arteLogo,sizeof arteLogo,"%s/aiostreams/logo.png",dir);
}
static char usuario[96], base[2048], host[256], alvo[128], tipo[32], mensagem[256];
static char configure[320], configAuth[2048], configUuid[128], configOrigin[256];
static void clearAuthorization(void) {
  memset(configAuth,0,sizeof configAuth); memset(configUuid,0,sizeof configUuid);
  configOrigin[0]=0;
  aiopair_close();
}
static void instanceHost(const char *origin) {
  if(strcasecmp(host,origin)) clearAuthorization();
  snprintf(host,sizeof host,"%s",origin);
}
static AioSnapshot snap;
static const char *const gruposNome[]={"Resolução e qualidade","Tamanho e bitrate","Idiomas","Cache e tipos de fonte","Semeadores","Regex e palavras-chave","Ordenação","Formatador","Proxy","Serviços e preferências"};
static int regraPertence(const AioRegra *r) {
  static const int grupos[]={0,1,2,3,3,4,5,6,7,8};
  int seed=strstr(r->nome,"Seeder")!=NULL || strstr(r->nome,"seeder")!=NULL;
  return regraSelecionada>=0 && regraSelecionada<10 && r->grupo==grupos[regraSelecionada] &&
    (r->grupo!=3 || seed==(regraSelecionada==4));
}
static int conectada(void) { return snap.valida && !soManifesto; }
static const char *configStatus(void) {
  return snap.configurada && snap.configEmbedded ? i18n("Configuração do manifesto") :
    snap.configurada && !soManifesto ? i18n("Configuração conectada") :
    snap.configAccess==AIO_RESTRICTED ? i18n("Configuração restrita; streams disponíveis") : i18n("Somente manifesto");
}
// Footer actions below the rows keep the viewport (no snap back to the top).
static float janelaCatalogos(void) { return janela(4,snap.catalogos,5,pagina==CATALOGOS); }
static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
static int ocupado, entregue;
static unsigned entregaGeracao;
static AioSnapshot entrega;
static int entregaInstalar;
static char entregaBase[2048];
typedef struct { unsigned geracao; int instalar, diagnostic; char base[2048], host[256], id[128], tipo[32], auth[2048], authOrigin[256]; } Pedido;

int aioui_detectar(int i) {
  return i >= 0 && (!strncasecmp(addons_nome(i),"AIOStreams",10) ||
    !strncasecmp(addons_id_manifesto(i),"aiostreams",10) ||
    !strncasecmp(addons_id_manifesto(i),AIO_ID_PREFIX,sizeof AIO_ID_PREFIX-1));
}
static int primeiro(void) { for(int i=0;i<addons_n();i++) if(aioui_detectar(i)) return i; return -1; }
static void qrAtual(void) {
  snprintf(configure,sizeof configure,"%s/stremio/configure",host);
}
static void buscar(AioSnapshot *s, const char *u, const char *auth, int k, char **corpo) {
  RedeResposta r = {0}; const char *h[] = {"User-Agent: AIO-NuvioLegacy","Cache-Control: no-cache, no-store",auth,NULL};
  RedePedido p = {.url=u,.cabecalhos=h,.prazo_ms=12000,.max_bytes=1024*1024,.seguir=0};
  rede_pedir(&p,&r); s->http[k]=r.status; s->ms[k]=(int)r.ms; s->retry[k]=r.status==429 && r.retry_after_s<=0?60:r.retry_after_s;
  AioCapability *cap=k==0?&s->manifestAccess:k==2?&s->configAccess:k==5?&s->searchAccess:NULL;
  if(cap) aio_capability(cap,r.status);
  const char *kind[]={"manifest","status","config","stream","subtitles","search"};
  printf("[aio] request=%s identity=%s url=REDACTED status=%d bytes=%zu transport=%d shape=%s\n",
    kind[k],auth?"basic":"manifest",r.status,r.corpo?strlen(r.corpo):0,r.erro,
    r.corpo && *r.corpo=='{'?"object":r.corpo && *r.corpo=='['?"array":"other");
  fflush(stdout);
  if(r.erro==REDE_OK && r.status>=200 && r.status<300) { *corpo=r.corpo; r.corpo=NULL; }
  rede_resposta_limpar(&r);
}
// Embedded installation configs are base64url JSON. Expand the shared long-URL
// alias first; never interpret an encrypted credential token as configuration.
static int manifestConfig(AioSnapshot *s,const char *manifest) {
  char *expanded=nv_longa_expandir(manifest);
  const char *u=expanded?expanded:manifest, *path=strstr(u,"://");
  int ok=0;
  if(path) {
    path=strchr(path+3,'/');
    if(path) {
      const char *last=strstr(path,"/manifest.json");
      const char *begin=last;
      if(begin) { while(begin>path && begin[-1]!='/') begin--; }
      size_t len=begin && last?(size_t)(last-begin):0;
      if(len && len<=1024*1024) {
        char *raw=calloc(1,len+1); size_t n=0; unsigned bits=0; int count=0,valid=1;
        if(raw) {
          const char alphabet[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
          for(size_t i=0;i<len;i++) {
            unsigned char c=begin[i];
            if(c=='%' && i+2<len && isxdigit((unsigned char)begin[i+1]) && isxdigit((unsigned char)begin[i+2])) {
              char hex[3]={begin[i+1],begin[i+2],0}; c=(unsigned char)strtoul(hex,NULL,16); i+=2;
            }
            if(c=='=') break;
            if(c=='-') c='+'; else if(c=='_') c='/';
            const char *v=c?strchr(alphabet,c):NULL;
            if(!v) { valid=0; break; }
            bits=(bits<<6)|(unsigned)(v-alphabet); count+=6;
            if(count>=8) { count-=8; raw[n++]=(char)(bits>>count); }
          }
          if(valid && n && aio_json_valid(raw) && *raw=='{') {
            aio_configuracao(s,raw); ok=s->configurada;
            if(ok) { s->configAccess=AIO_UNKNOWN; s->configEmbedded=1; }
          }
          memset(raw,0,len+1); free(raw);
        }
      }
    }
  }
  if(expanded) { memset(expanded,0,strlen(expanded)); free(expanded); }
  return ok;
}
static void *worker(void *arg) {
  Pedido *p=arg; AioSnapshot *s=calloc(1,sizeof *s); char *body=NULL, url[3072], auth[4096];
  if(s) {
    s->estatisticas=s->erros=s->filtrados=-1;
    snprintf(s->host,sizeof s->host,"%s",p->host);
    buscar(s,p->base,NULL,0,&body);
    if(body) { aio_manifesto(s,body); free(body); body=NULL; }
    int embedded=s->valida && manifestConfig(s,p->base);
    snprintf(url,sizeof url,"%s/api/v1/status",p->host); buscar(s,url,NULL,1,&body);
    // Status version is not evidence of authenticated configuration access.
    if(body) { aio_status(s,body); free(body); body=NULL; }
    char manifestOrigin[256];
    int authorized=0;
    if(p->auth[0] && !strcasecmp(p->host,p->authOrigin))
      authorized=!!snprintf(auth,sizeof auth,"%s",p->auth);
    else if(aio_url(p->base,manifestOrigin,sizeof manifestOrigin) && !strcasecmp(p->host,manifestOrigin))
      authorized=aio_autorizacao(p->base,auth,sizeof auth);
    if(s->valida && authorized && !strncmp(p->host,"https://",8)) {
      snprintf(url,sizeof url,"%s/api/v1/user",p->host); buscar(s,url,auth,2,&body);
      if(body) {
        aio_configuracao(s,body); memset(body,0,strlen(body)); free(body); body=NULL;
        if(!s->configurada && embedded) { manifestConfig(s,p->base); s->configAccess=AIO_FAILED; }
      }
      if(p->diagnostic && p->id[0] && s->searchAtiva) {
        char encoded[512];
        if(nv_addon_id(encoded,sizeof encoded,p->id)) {
          snprintf(url,sizeof url,"%s/api/v1/search?type=%s&id=%s&format=true",p->host,p->tipo,encoded);
          buscar(s,url,auth,5,&body);
          if(body) { aio_search(s,body); memset(body,0,strlen(body)); free(body); body=NULL; }
        }
      }
      memset(auth,0,sizeof auth);
    }
    if(s->valida && p->id[0]) {
      char b[2048], encoded[512]; nv_addon_base(p->base,b,sizeof b);
      if(nv_addon_id(encoded,sizeof encoded,p->id) && NV_COUBE(nv_addon_url(url,sizeof url,b,"/stream/%s/%s.json",p->tipo,encoded),url)) {
        buscar(s,url,NULL,3,&body);
        if(body) { aio_resultados(s,body); memset(body,0,strlen(body)); free(body); body=NULL; }
      }
      if((s->recursos&AIO_LEGENDA) && nv_addon_id(encoded,sizeof encoded,p->id) &&
          NV_COUBE(nv_addon_url(url,sizeof url,b,"/subtitles/%s/%s.json",p->tipo,encoded),url)) {
        buscar(s,url,NULL,4,&body);
        if(body) { aio_legendas(s,body); memset(body,0,strlen(body)); free(body); body=NULL; }
      }
    }
  }
  if(s) { printf("[aio] sections manifest=%d config=%d embedded=%d addons=%d services=%d filters=%d catalogs=%d statistics=%d errors=%d\n",
    s->valida,s->configurada,s->configurada && s->configAccess!=AIO_AVAILABLE,s->addons,s->servicos,s->detalhes,s->catalogos,s->estatisticas,s->erros); fflush(stdout); }
  pthread_mutex_lock(&trava);
  if(s) entrega=*s; else memset(&entrega,0,sizeof entrega);
  entregaGeracao=p->geracao; entregaInstalar=p->instalar;
  snprintf(entregaBase,sizeof entregaBase,"%s",p->base); entregue=1; ocupado=0;
  pthread_mutex_unlock(&trava);
  free(s); memset(p,0,sizeof *p); free(p); return NULL;
}
static void trace(void) {
  int ranks[AIO_MAX], count=stream_n();
  if(count>AIO_MAX) count=AIO_MAX;
  int current=alvo[0] && stream_lista_do_alvo(alvo);
  if(current) stream_inspecao_ordem(ranks,count);
  for(int i=0;i<snap.resultados;i++) {
    AioResultado *r=&snap.resultado[i]; r->rank=0; r->decisao=r->destino[0]?1:0;
    if(!current || !r->identidade || r->decisao) continue;
    for(int j=0;j<count;j++) {
      const Stream *v=stream_item(j);
      if(v && r->identidade==aio_identidade(v->url[0]?v->url:v->infoHash)) {
        r->rank=ranks[j]; r->decisao=!r->rank?2:(v->foraCache||v->naoVideo)?3:4; break;
      }
    }
  }
}
static void atualizarRede(int install) { pendente=install+1; }
static void iniciarRede(void) {
  int install=pendente-1;
  if(!pendente || (proximaConsulta && !SDL_TICKS_PASSED(SDL_GetTicks(),proximaConsulta))) return;
  pthread_mutex_lock(&trava);
  if(ocupado) { pthread_mutex_unlock(&trava); return; }
  Pedido *p=calloc(1,sizeof *p);
  if(!p) { pthread_mutex_unlock(&trava); return; }
  p->geracao=++geracao; p->instalar=install; pendente=0;
  snprintf(p->base,sizeof p->base,"%s",base); snprintf(p->host,sizeof p->host,"%s",host);
  snprintf(p->id,sizeof p->id,"%s",alvo); snprintf(p->tipo,sizeof p->tipo,"%s",tipo);
  snprintf(p->auth,sizeof p->auth,"%s",configAuth);
  snprintf(p->authOrigin,sizeof p->authOrigin,"%s",configOrigin);
  pthread_t t; ocupado=1; entregue=0;
  if(pthread_create(&t,NULL,worker,p)) { ocupado=0; free(p); }
  else pthread_detach(t);
  pthread_mutex_unlock(&trava);
}
static void guardarInstalacao(void) {
  // Reuse the same account/profile snapshot restored by sync at startup.
  // Never upload the complete list to the account.
  static AddonRemoto lista[128];
  int n=addons_exportar(lista,128); Jsw w; jsw_iniciar(&w); jsw_arr_ini(&w);
  for(int i=0;i<n;i++) {
    jsw_obj_ini(&w); jsw_cs(&w,"url",lista[i].url); jsw_cs(&w,"name",lista[i].nome);
    jsw_cb(&w,"enabled",lista[i].ativo); jsw_obj_fim(&w);
  }
  jsw_arr_fim(&w);
  const char *body=jsw_texto_final(&w);
  if(body) contacache_gravar(CC_ADDONS_APLICADOS,perfis_ativo_addons(),sessao_usuario()[0]?sessao_usuario():"@local",body);
  jsw_livre(&w); memset(lista,0,sizeof lista);
}
static void fechar(void) {
  saindo=1; geracao++; pendente=0; celular_fechar(); aiopair_close(); connectOpen=0;
}
static void encerrar(void) {
  aberto=0;
  clearAuthorization();
  memset(&snap,0,sizeof snap); memset(base,0,sizeof base); alvo[0]=mensagem[0]=0;
}
void aioui_abrir(int i) {
  abertura=0; saindo=0; paginaAnterior=-1; historyCount=0; navigationCount=0;
  sidebarOpen=0; dashboardReturn=connectOpen=dashboardFoco=0;
  clearAuthorization();
  aberto=1; geracao++; foco=0; lateral=0; entrada=pendente=0; indice=i; alvo[0]=0; proximaConsulta=0;
  regraSelecionada=linkSelecionado=-1; evidenciaSelecionada=0; soManifesto=0; ultimaAtualizacao=0;
  memset(dashboardLinha,0,sizeof dashboardLinha);
  memset(dashboardScroll,0,sizeof dashboardScroll); memset(dashboardVel,0,sizeof dashboardVel);
  memset(&snap,0,sizeof snap); snap.estatisticas=snap.erros=snap.filtrados=-1; mensagem[0]=0;
  perfil=perfis_ativo(); versao=addons_versao(); snprintf(usuario,sizeof usuario,"%s",sessao_usuario());
  snprintf(host,sizeof host,"%s",hosts[hostIdx]); base[0]=0;
  if(i>=0) {
    nv_addon_url(base,sizeof base,addons_base(i),"/manifest.json");
    aio_url(base,host,sizeof host); pagina=PAINEL; atualizarRede(0);
  } else pagina=SEM;
  qrAtual();
}
void aioui_inspecionar(const char *id,const char *t) {
  aioui_abrir(primeiro());
  if(indice>=0) {
    snprintf(alvo,sizeof alvo,"%s",id?id:""); snprintf(tipo,sizeof tipo,"%s",t?t:"movie");
    pagina=TITULO;
    atualizarRede(0);
  }
}
int aioui_aberto(void) { return aberto; }
int aioui_opaco(void) { return aberto && abertura>=1 && !saindo; }
static void mudar(int p) {
  if (p != pagina && historyCount < 64) {
    history[historyCount].page=pagina; history[historyCount].focus=foco;
    history[historyCount++].side=lateral;
  }
  pagina=p; foco=0; lateral=0; navigationCount=0; mensagem[0]=0;
  regraSelecionada=-1; linkSelecionado=-1; evidenciaSelecionada=0; celular_fechar();
}
static void campo(int qual) {
  entrada=qual;
  teclado_abrir_com(qual==1?i18n("URL da instância"):qual==2?i18n("ID do título"):i18n("Link de instalação"),
    i18n("Cole pelo celular. O link pessoal permanece oculto."),qual==3?TECLADO_LONGO:TECLADO_MAX,
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789:/?&=._-%+@!~#",NULL);
  teclado_mascarar(qual==3);
}
static int nLegendas(void) {
  int n=0; for(int i=0;i<snap.addons;i++) if(snap.addon[i].legenda) n++;
  return n;
}
static const char *serviceAtivo(void) {
  for(int i=0;i<snap.servicos;i++) if(snap.servico[i].ativo) return snap.servico[i].nome;
  return NULL;
}
static int nConteudo(void) {
  if(pagina==SEM) return 3;
  if(pagina==INSTANCIA) return 11;
  if(pagina==SERVICOS) return 6;
  if(pagina==IMPORTAR) return 2;
  if(pagina==PRONTO) return 2;
  if(pagina==PAINEL) return 11;
  if(pagina==CATALOGOS) return snap.catalogos+4;
  if(pagina==FONTES) return snap.addons-nLegendas()+snap.servicos+4;
  if(pagina==LEGENDAS) return nLegendas()+snap.legendas+2;
  if(pagina==LINKS) return snap.links+3;
  if(pagina==FILTROS) {
    if(regraSelecionada>=0) { int n=1; for(int i=0;i<snap.detalhes;i++) if(regraPertence(&snap.detalhe[i])) n++; return n; }
    return 10;
  }
  if(pagina==TITULO) return (evidenciaSelecionada?snap.evidencias:snap.resultados)+3;
  return 2;
}
static int nLinhas(void) {
  return nConteudo()+(pagina>=CATALOGOS && pagina<=SAUDE?2:0);
}
static int fileira(int c) {
  char chave[512];
  if(c<0 || c>=snap.catalogos || indice<0) return -1;
  snprintf(chave,sizeof chave,"%s_%s_%s",addons_id_manifesto(indice),snap.catalogo[c].tipo,snap.catalogo[c].id);
  for(int i=0;i<fil_n();i++) if(!strcmp(fil_chave(i),chave)) return i;
  return -1;
}
static void previa(int i);
static void acao(void) {
  if(lateral) {
    if(PAINEL+foco!=pagina) previa(foco);
    lateral=0; foco=pagina==PAINEL?dashboardFoco:0; navigationCount=0;
    return;
  }
  if(pagina>=CATALOGOS && pagina<=SAUDE && foco>=nLinhas()-2) {
    if(foco==nLinhas()-1) { geracao++; atualizarRede(0); }
    else mudar(SERVICOS);
    return;
  }
  if(pagina==SEM) { int f=foco; mudar(f==2?IMPORTAR:INSTANCIA); if(f==1) campo(1); }
  else if(pagina==INSTANCIA) {
    if(foco<7) { hostIdx=foco; instanceHost(hosts[hostIdx]); qrAtual(); }
    else if(foco==7) campo(1); else if(foco==8) mudar(IMPORTAR);
    else if(foco==10) { maisHosts=!maisHosts; foco=maisHosts?4:0; }
    else mudar(SERVICOS);
  } else if(pagina==SERVICOS) {
    if(foco<4) snprintf(mensagem,sizeof mensagem,"%s",i18n("Abra a configuração oficial, salve e copie o link de instalação. Envie pelo pareamento já usado pelo Nuvio."));
    else mudar(foco==4?IMPORTAR:PRONTO);
  }
  else if(pagina==IMPORTAR) {
    if(foco==0) {
      if(celular_disponivel() && celular_abrir(i18n("Link de instalação"))) entrada=3;
      else campo(3);
    } else campo(3);
  } else if(pagina==PRONTO) mudar(snap.valida?(foco?CATALOGOS:PAINEL):(foco?SERVICOS:IMPORTAR));
  else if(pagina==PAINEL) {
    if(foco<6 && !snap.valida) { geracao++; atualizarRede(0); }
    else if(foco<6 && !snap.configurada && foco<4) { foco=9; acao(); }
    else if(foco<6) {
      const int pages[]={FONTES,FONTES,FILTROS,FILTROS,CATALOGOS,SAUDE};
      int card=foco; mudar(pages[card]);
      if(card==3) regraSelecionada=7;
    }
    else if(foco==6) campo(2);
    else if(foco==7) mudar(FONTES);
    else if(foco==8) fechar();   // back to Complementos, where this screen opens from
    else if(foco==9) {
      connectOpen=1; connectFocus=0; connectUrl[0]=0;
      aiopair_start(host); connectState=aiopair_state(connectUrl,sizeof connectUrl);
    }
    else { geracao++; atualizarRede(0); }
  } else if(pagina==CATALOGOS) {
    if(foco<snap.catalogos) {
      catalogoSelecionado=foco;
      int f=fileira(foco);
      if(f>=0) { fil_alternar(f); desc_remontar_fileiras(); }
      else snprintf(mensagem,sizeof mensagem,"%s",i18n("Atualize os catálogos para escolher na Home."));
    } else if(foco==snap.catalogos || foco==snap.catalogos+1) {
      int f=fileira(catalogoSelecionado);
      if(f>=0) { fil_mover(f,foco==snap.catalogos?-1:1); desc_remontar_fileiras(); }
    } else if(foco==snap.catalogos+2) { desc_repetir(); geracao++; atualizarRede(0); }
    else mudar(SERVICOS);
  } else if(pagina==FONTES) {
    int n=snap.addons-nLegendas()+snap.servicos;
    if(foco==n) ajustes_definir_p2p_ligado(!ajustes_p2p_ligado());
    else if(foco==n+1) snprintf(mensagem,sizeof mensagem,"%s",i18n("O AIOStreams suporta Usenet por provedores compatíveis. Reproduzir NZB direto exige integração."));
    else if(foco==n+2) campo(2);
    else mudar(SERVICOS);
  } else if(pagina==FILTROS) {
    if(regraSelecionada<0 && foco<10) { regraSelecionada=foco; foco=0; }
    else if(foco==nConteudo()-1) mudar(SERVICOS);
  } else if(pagina==LINKS) {
    if(foco<snap.links) {
      int k=0;
      for(int i=0;i<snap.resultados;i++) if(strcmp(snap.resultado[i].tipo,"HTTP") && strcmp(snap.resultado[i].tipo,"P2P") && strcmp(snap.resultado[i].tipo,"Desconhecido")) {
        if(k++==foco) { if(celular_abrir_link(i18n("Abrir no celular"),snap.resultado[i].destino)) linkSelecionado=i; break; }
      }
    } else if(foco==snap.links) campo(2);
    else mudar(foco==snap.links+1?IMPORTAR:FONTES);
  } else if(pagina==LEGENDAS) {
    if(foco==0) campo(2);
    else if(foco<=nLegendas() || foco==nConteudo()-1) mudar(SERVICOS);
  }
  else if(pagina==TITULO) {
    if(foco==nLinhas()-1) { evidenciaSelecionada=!evidenciaSelecionada; foco=2; }
    else if(evidenciaSelecionada && foco>=2) return;
    else if(foco==0) campo(2); else if(foco==1) { geracao++; atualizarRede(0); }
    else if(foco>=2 && foco-2<snap.resultados && snap.resultado[foco-2].destino[0]) {
      if(celular_abrir_link(i18n("Abrir no celular"),snap.resultado[foco-2].destino)) linkSelecionado=foco-2;
    }
  } else if(pagina==SAUDE && foco==0) { geracao++; atualizarRede(0); }
  else mudar(SERVICOS);
}
static void manualConnect(void) {
  aiopair_close(); connectOpen=0; entrada=4;
  teclado_abrir_com(i18n("UUID do AIOStreams"),i18n("Acesso somente leitura à configuração"),128,
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_",NULL);
  teclado_mascarar(1);
}
static void irRail(void) { dashboardFoco=foco; lateral=1; foco=0; navigationCount=0; }
// Rail preview: swap the section without stacking history; Back from any section returns to the overview.
static void previa(int i) {
  int target=PAINEL+i;
  if(target==pagina) return;
  historyCount=0;
  if(target!=PAINEL) { history[0].page=PAINEL; history[0].focus=dashboardFoco; history[0].side=0; historyCount=1; }
  pagina=target; dashboardReturn=0; mensagem[0]=0;
  regraSelecionada=-1; linkSelecionado=-1; evidenciaSelecionada=0; celular_fechar();
}
static int dashboardCompleto(void);
static int cabecalhoIds(int *ids);
static int dashboardCta(void);
static int dashboardVis[6]={3,3,3,3,3,3};
// D-pad follows the drawn grid: header actions, then the banner or the stat row, then cards.
static void dashboardTecla(SDL_Keycode k) {
  int ids[3],n=cabecalhoIds(ids),hi=-1,cta=dashboardCta(),completo=dashboardCompleto();
  for(int i=0;i<n;i++) if(ids[i]==foco) hi=i;
  int up=k==SDLK_UP, down=k==SDLK_DOWN, left=k==SDLK_LEFT, right=k==SDLK_RIGHT;
  if(hi>=0) {
    if(left) { if(hi) foco=ids[hi-1]; else irRail(); }
    else if(right) { if(hi<n-1) foco=ids[hi+1]; }
    else if(down) foco=completo?(hi<2?hi:2):cta;
    return;
  }
  if(foco==cta) {
    if(up) foco=ids[n-1]; else if(down) foco=0; else if(left) irRail();
    return;
  }
  if(foco>=6) { foco=completo?0:cta; return; }
  int rolavel=completo || foco>=4;
  int total=dashboardTotal(foco), vis=dashboardVis[foco], step=down-up;
  if(rolavel && step && dashboardLinha[foco]+step>=0 && dashboardLinha[foco]+step<=total-vis) { dashboardLinha[foco]+=step; return; }
  if(completo) {
    if(up) foco=foco>=3?foco-3:ids[foco<n?foco:n-1];
    else if(down) { if(foco<3) foco+=3; }
    else if(left) { if(foco%3) foco--; else irRail(); }
    else if(right && foco%3<2) foco++;
  } else if(foco<4) {
    if(left) { if(foco) foco--; else irRail(); }
    else if(right) { if(foco<3) foco++; }
    else if(up) foco=cta;
    else if(down) foco=foco<2?4:5;
  } else {
    if(left) { if(foco==5) foco=4; else irRail(); }
    else if(right) foco=5;
    else if(up) foco=foco==4?0:2;
  }
}
void aioui_evento(const SDL_Event *e) {
  if(!aberto) return;
  if(saindo) return;
  if(teclado_aberto()) { teclado_evento(e); return; }
  if(e->type!=SDL_KEYDOWN) return;
  SDL_Keycode k=e->key.keysym.sym;
  if(connectOpen) {
    if(nv_tecla_voltar(e)) { aiopair_close(); connectOpen=0; }
    else if(k==SDLK_LEFT || k==SDLK_UP) connectFocus=0;
    else if(k==SDLK_RIGHT || k==SDLK_DOWN) connectFocus=1;
    else if((k==SDLK_RETURN || k==SDLK_KP_ENTER) && !e->key.repeat) {
      if(connectFocus) manualConnect();
      else if(connectState==AIOPAIR_SYNCED) { connectOpen=0; aiopair_close(); }
      else aiopair_start(host);
    }
    return;
  }
  if(dashboardReturn && k==SDLK_RIGHT) {
    pagina=PAINEL; lateral=0; foco=dashboardFoco; dashboardReturn=0;
    if(historyCount && history[historyCount-1].page==PAINEL) historyCount--;
    navigationCount=0; return;
  }
  if(nv_tecla_voltar(e)) {
    if(pagina==PAINEL && lateral) { lateral=0; foco=dashboardFoco; navigationCount=0; return; }
    if(dashboardReturn) {
      pagina=PAINEL; lateral=0; foco=dashboardFoco; dashboardReturn=0;
      if(historyCount && history[historyCount-1].page==PAINEL) historyCount--;
      navigationCount=0; return;
    }
    if(entrada==3 || linkSelecionado>=0) { celular_fechar(); entrada=0; linkSelecionado=-1; return; }
    if(pagina==FILTROS && regraSelecionada>=0) { foco=regraSelecionada; lateral=0; regraSelecionada=-1; return; }
    if(historyCount) {
      historyCount--; pagina=history[historyCount].page;
      foco=history[historyCount].focus; lateral=history[historyCount].side;
      navigationCount=0; celular_fechar();
      if(pagina==PAINEL) dashboardReturn=0;
    } else fechar();
    return;
  }
  if(pagina==PAINEL && !lateral && (k==SDLK_UP || k==SDLK_DOWN || k==SDLK_LEFT || k==SDLK_RIGHT)) {
    dashboardTecla(k); return;
  }
  // Moving along the rail previews each section at once, so the rail visibly opens pages.
  if(lateral && pagina>=PAINEL && pagina<=SAUDE && (k==SDLK_UP || k==SDLK_DOWN)) {
    int next=foco+(k==SDLK_DOWN?1:-1);
    if(next>=0 && next<7) { foco=next; previa(next); }
    return;
  }
  if(lateral && pagina>=PAINEL && pagina<=SAUDE && k==SDLK_RIGHT) {
    lateral=0; foco=pagina==PAINEL?dashboardFoco:0; navigationCount=0; return;
  }
  if(pagina==PAINEL && lateral && k==SDLK_RIGHT) { lateral=0; foco=dashboardFoco; navigationCount=0; return; }
  if(k==SDLK_UP || k==SDLK_DOWN || k==SDLK_LEFT || k==SDLK_RIGHT) {
    int first=0,total=0,step=(k==SDLK_DOWN)-(k==SDLK_UP);
    if(pagina==CATALOGOS) total=snap.catalogos;
    else if(pagina==TITULO) { first=2;total=evidenciaSelecionada?snap.evidencias:snap.resultados; }
    else if(pagina==FONTES) total=snap.addons-nLegendas()+snap.servicos+2;
    else if(pagina==LINKS) total=snap.links;
    else if(pagina==FILTROS) total=regraSelecionada>=0?nConteudo()-1:10;
    else if(pagina==LEGENDAS) total=nConteudo()-2;
    int adjacent=foco+step,visible=0;
    for(int i=0;i<navigationCount;i++) if(navigationIds[i]==adjacent) visible=1;
    // Reveal an adjacent hidden row before geometry can select a footer.
    if(!lateral && step && foco>=first && foco<first+total && adjacent>=first && adjacent<first+total && !visible) { foco=adjacent;return; }
    int current=-1;
    for(int i=0;i<navigationCount;i++) if(navigationIds[i]==foco+1000*lateral) current=i;
    int next=focus_rect_nearest(navigationRects,navigationCount,current,
      (k==SDLK_RIGHT)-(k==SDLK_LEFT),(k==SDLK_DOWN)-(k==SDLK_UP));
    if(next>=0) { foco=navigationIds[next]%1000; lateral=navigationIds[next]/1000; return; }
  }
  if(pagina==CATALOGOS && !lateral && foco<snap.catalogos && (k==SDLK_PAGEUP || k==SDLK_PAGEDOWN)) {
    int f=fileira(foco);
    if(f>=0) { fil_mover(f,k==SDLK_PAGEUP?-1:1); desc_remontar_fileiras(); }
    return;
  }
  if(pagina==INSTANCIA && (k==SDLK_UP || k==SDLK_DOWN)) {
    const int ordem[]={0,1,2,3,10,7,8,9}, outras[]={4,5,6,10,7,8,9};
    const int *v=maisHosts?outras:ordem; int n=maisHosts?7:8;
    for(int i=0;i<n;i++) if(v[i]==foco) {
      int j=i+(k==SDLK_UP?-1:1); if(j>=0 && j<n) foco=v[j]; return;
    }
  }
  if(k==SDLK_LEFT && pagina>=PAINEL) { lateral=1; foco=pagina<=SAUDE?pagina-PAINEL:0; }
  else if(k==SDLK_RIGHT && lateral) { lateral=0; foco=0; }
  else if(k==SDLK_UP && foco>0) foco--;
  else if(k==SDLK_DOWN && foco<(lateral?7:nLinhas())-1) foco++;
  else if((k==SDLK_RETURN || k==SDLK_KP_ENTER) && !e->key.repeat) acao();
}
static void receber(const char *texto) {
  char origem[256];
  if(entrada==4) {
    if(!texto[0] || strlen(texto)>=sizeof configUuid || strspn(texto,"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_")!=strlen(texto)) return;
    snprintf(configUuid,sizeof configUuid,"%s",texto);
    entrada=5;
    teclado_abrir_com(i18n("Senha do AIOStreams"),i18n("Credenciais ocultas"),768,
      "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789:/?&=._-%+@!~#",NULL);
    teclado_mascarar(1); return;
  } else if(entrada==5) {
    char credentials[1024];
    snprintf(credentials,sizeof credentials,"%s:%s",configUuid,texto);
    if(texto[0] && !strncmp(host,"https://",8) && aio_basic(credentials,configAuth,sizeof configAuth)) {
      snprintf(configOrigin,sizeof configOrigin,"%s",host); geracao++; atualizarRede(0);
    }
    memset(credentials,0,sizeof credentials); memset(configUuid,0,sizeof configUuid);
  } else if(entrada==1) {
    if(aio_url(texto,origem,sizeof origem) && !strcmp(texto,origem)) {
      instanceHost(origem); qrAtual(); mudar(SERVICOS);
    } else snprintf(mensagem,sizeof mensagem,"%s",i18n("URL inválida. Use apenas a origem HTTP ou HTTPS, sem credenciais."));
  } else if(entrada==2) {
    if(!texto[0] || strspn(texto,"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789:_-.")!=strlen(texto))
      snprintf(mensagem,sizeof mensagem,"%s",i18n("ID do título inválido."));
    else { snprintf(alvo,sizeof alvo,"%s",texto); strcpy(tipo,strchr(texto,':')?"series":"movie"); mudar(TITULO); geracao++; atualizarRede(0); }
  } else if(aio_url(texto,origem,sizeof origem) && !strchr(texto,'#') &&
      strstr(texto,"/manifest.json") &&
      (strstr(texto,"/manifest.json")[14]==0 || strstr(texto,"/manifest.json")[14]=='?') && strlen(texto)<sizeof base) {
    if(strcmp(base,texto)) clearAuthorization();
    snprintf(base,sizeof base,"%s",texto); instanceHost(origem); qrAtual(); geracao++; atualizarRede(1);
  } else snprintf(mensagem,sizeof mensagem,"%s",i18n("Link inválido. Copie o manifesto em Salvar e instalar no AIOStreams."));
  entrada=0;
}
void aioui_atualizar(float dt,Uint32 agora) {
  if(!aberto) return;
  movimento(dt);
  if(saindo) { if(abertura<=0) encerrar(); return; }
  if(perfil!=perfis_ativo() || strcmp(usuario,sessao_usuario()) || (indice>=0 && versao!=addons_versao())) { fechar(); return; }
  if(teclado_aberto()) teclado_atualizar(dt,agora);
  if(entrada) {
    int r=teclado_resultado();
    if(r==TECLADO_PRONTO) { receber(teclado_texto()); teclado_esquecer(); }
    else if(r==TECLADO_CANCELOU) { entrada=0; memset(configUuid,0,sizeof configUuid); }
  }
  if(connectOpen) {
#ifdef NV_SHOT_HOOKS
    if(!connectShot)
#endif
    connectState=aiopair_state(connectUrl,sizeof connectUrl);
    char auth[2048],origin[256];
    if(aiopair_take(auth,sizeof auth,origin,sizeof origin)) {
      if(!strcmp(host,origin)) {
        snprintf(configAuth,sizeof configAuth,"%s",auth);
        snprintf(configOrigin,sizeof configOrigin,"%s",origin); geracao++; atualizarRede(0);
      }
      memset(auth,0,sizeof auth); memset(origin,0,sizeof origin);
    }
  }
  if(entrada==3) { char texto[2048]; if(celular_pegar(texto,sizeof texto)) { receber(texto); memset(texto,0,sizeof texto); } }
  pthread_mutex_lock(&trava);
  if(entregue) {
    if(entregaGeracao==geracao) {
      snap=entrega; ultimaAtualizacao=time(NULL); trace();
      // Last real stream response is installation/profile scoped, no synthetic probe.
      if(!alvo[0] && indice>=0) {
        AddonAioRequest *movie=calloc(1,sizeof *movie), *series=calloc(1,sizeof *series);
        int m=movie?addons_aio_history(indice,"movie",movie,1):0;
        int t=series?addons_aio_history(indice,"series",series,1):0;
        AddonAioRequest *last=t && (!m || series->completed>movie->completed)?series:m?movie:NULL;
        if(last) {
          snap.http[3]=last->http; snap.ms[3]=(int)last->clientMs;
          snap.estatisticas=last->statistics; snap.erros=last->errors; snap.playable=last->playable;
          snap.evidencias=last->evidenceCount; snap.evidenceOmitted=last->omitted;
          memcpy(snap.evidencia,last->evidence,sizeof snap.evidencia);
          printf("[aio] history present=1 status=%d statistics=%d errors=%d evidence=%d\n",last->http,last->statistics,last->errors,last->evidenceCount); fflush(stdout);
        }
        free(movie); free(series);
      }
      for(int k=0;k<6;k++) if(snap.retry[k]>0) {
        unsigned wait=(unsigned)(snap.retry[k]>3600?3600:snap.retry[k]);
        proximaConsulta=agora+wait*1000u;
      }
      if(configAuth[0] && snap.configAccess==AIO_RESTRICTED) {
        clearAuthorization();
        snprintf(mensagem,sizeof mensagem,"%s",i18n("Acesso recusado. Confira UUID e senha."));
      }
      if(!snap.valida) snprintf(mensagem,sizeof mensagem,"%s",i18n("Manifesto indisponível, inválido ou ainda exige configuração."));
      else if(entregaInstalar) {
        if(addons_adicionar("AIOStreams",entregaBase)) {
          indice=addons_n()-1; versao=addons_versao(); guardarInstalacao(); desc_repetir(); mudar(PRONTO);
        } else snprintf(mensagem,sizeof mensagem,"%s",i18n("AIOStreams já instalado ou lista cheia."));
      }
    }
    memset(&entrega,0,sizeof entrega); memset(entregaBase,0,sizeof entregaBase); entregue=0;
  }
  pthread_mutex_unlock(&trava);
  iniciarRede();
}

// Rendering is fixed-cost per visible page: no network, JSON, regex or allocation.
static void texto(TxtEstilo e,const char *s,float x,float y,float w,int cor) {
  float escala=gfx_escala_txt();
  if(e==TXT_ILHA_SUB)e=TXT_ILHA_SUB_L;
  TXT_INI();
  TxtLinha t=txt_linha_corta(e,s,cor,cor,cor,255,w/escala);
  txt_desenhar(t,x/escala,y/escala);
  TXT_FIM();
}
static float blocoEstilo(TxtEstilo estilo,const char *s,float x,float y,float w,float leading,int linhas) {
  float scale=gfx_escala_txt();
  TXT_INI();
  float h=txt_bloco_corta(estilo,s,238,238,238,x/scale,y/scale,w/scale,leading,1,linhas)*scale;
  TXT_FIM();
  return h;
}
static void bloco(const char *s,float x,float y,float w,int linhas) {
  float alpha=gfx_opacidade_grupo; gfx_opacidade_grupo=alpha*.72f;
  blocoEstilo(TXT_ILHA_SUB,s,x,y,w,28,linhas);
  gfx_opacidade_grupo=alpha;
}
static void painel(float x,float y,float w,float h) {
  GfxRect r={x,y,w,h};
  t21_ilha_folha(r,T21_R_MODAL,1);
}
static void pointer(int i,int b) {
  if(pagina==PAINEL && b && !lateral) dashboardFoco=foco;
  lateral=b; foco=i;
}
// Upstream wrapper attribution: a catalog id carries the wrapper addon when it
// embeds its name. Match is on normalized tokens; without a derivable match the
// row shows the wrapping addon itself, as the approved mock does.
static const char *origemCatalogo(int c) {
  static char out[192];
  char id[128]; size_t j=0;
  for(size_t i=0;j<sizeof id-1 && snap.catalogo[c].id[i];i++) {
    char ch=(char)tolower((unsigned char)snap.catalogo[c].id[i]);
    id[j++]=(ch=='-'||ch=='_'||ch=='.')?' ':ch;
  }
  id[j]=0;
  for(int a=0;a<snap.addons;a++) {
    char nome[96]; size_t k=0;
    for(size_t i=0;k<sizeof nome-1 && snap.addon[a].nome[i];i++) {
      char ch=(char)tolower((unsigned char)snap.addon[a].nome[i]);
      nome[k++]=(ch=='-'||ch=='_'||ch=='.')?' ':ch;
    }
    nome[k]=0;
    if(k>2 && strstr(id,nome)) { snprintf(out,sizeof out,"%s",snap.addon[a].nome); return out; }
  }
  snprintf(out,sizeof out,"AIOStreams");
  return out;
}
static void click(int i,int b) { pointer(i,b); acao(); }
static void voltarClick(int a,int b) {
  (void)a; (void)b; SDL_Event e={0}; e.type=SDL_KEYDOWN; e.key.keysym.sym=SDLK_ESCAPE;
  aioui_evento(&e);
}
static float entradaDe(int coluna,int fila) {
  return ajustes_animacoes_reduzidas_usuario()?1:revela_entra(1,revela_onda_atraso(coluna,fila),1+(Uint32)(entradaTempo*1000));
}
static float itemInicio(int coluna,int fila,float *y) {
  float alpha=gfx_opacidade_grupo, entry=entradaDe(coluna,fila);
  gfx_opacidade_grupo=alpha*entry;
  *y+=(1-entry)*NV_ENTRA_DY;
  return alpha;
}
static void linha(int i,const char *icon,const char *nome,const char *sub,float x,float y,float w) {
  float h=pagina==SEM?145:94, ty=pagina==SEM?38:17;
  if(y+h<260 || y>920) return;
  float alpha=gfx_opacidade_grupo;
  float edge=anim_clamp(fminf((y+h-260)/24,(920-y)/24),0,1);
  float entry=entradaDe(pagina==PAINEL?i%2:0,pagina==PAINEL?i/2:(int)((y-263)/94));
  gfx_opacidade_grupo=alpha*edge*entry;
  y+=(1-entry)*NV_ENTRA_DY;
  plrui_linha_foco((GfxRect){x,y,w,h},22,focoDe(i,0));
  plrui_material((GfxRect){x+20,y+(h-52)/2,52,52},16,0,.45f);
  gfx_icone((GfxRect){x+32,y+(h-28)/2,28,28},icon,.85f,.88f,.89f,1);
  texto(pagina==SEM?TXT_ILHA_NOME_L:TXT_ILHA_NOME,nome,x+94,y+ty,w-152,241);
  texto(TXT_ILHA_SUB,sub,x+94,y+ty+36,w-152,174);
  gfx_icone((GfxRect){x+w-44,y+(h-22)/2,22,22},"aj_chevron-right",.68f,.73f,.76f,1);
  ponteiro_alvo(x,y,w,h,pointer,click,i,0);
  gfx_opacidade_grupo=alpha;
}
static void botaoIc(int i,const char *nome,const char *ic,float x,float y,float w) {
  float scale=gfx_escala_txt();
  GfxRect r={x/scale,y/scale,fmaxf(w/scale,botao_largura(nome,ic,0)),T21_CTRL_H};
  TXT_INI();
  botao_pilula(r,nome,ic,focoDe(i,0),0,0,1);
  TXT_FIM();
  ponteiro_alvo(x,y,r.w*scale,r.h*scale,pointer,click,i,0);
}
static void botao(int i,const char *nome,float x,float y,float w) { botaoIc(i,nome,NULL,x,y,w); }
static float botaoW(int i,const char *nome,const char *ic,float x,float y) {
  botaoIc(i,nome,ic,x,y,0); return botao_largura(nome,ic,0)*gfx_escala_txt();
}
static void qrDesenhar(const char *url,float x,float y,float size) {
  // Shared QR texture: one draw instead of hundreds of modules each frame.
  float scale=size/celb_qr_lado();
  gfx_transformar(0,0,scale,x,y);
  celb_qr_desenhar(url,0,0,1);
  gfx_sem_transformar();
  if(deslocamento) gfx_transformar(0,0,1,0,deslocamento);
}
static void setup(void) {
  painel(72,148,1776,824);
  GLuint logo=tex_obter(arteLogo);
  if(logo) gfx_rect((GfxRect){124,216,48,48},logo,GFX_MARCA,0,0,0,0,1,1,1,1);
  texto(TXT_V2_GRUPO,"AIOStreams",199,206,440,242);
  texto(TXT_ILHA_SUB,i18n("Um addon, todas as suas fontes"),199,248,440,174);
  gfx_cor((GfxRect){661,156,1,815},0,.8f,.84f,.88f,.08f);
  static const char *const story[]={"Um lugar.\nTodas as fontes.","Escolha onde\nele roda.","Sua configuração.\nNesta TV.","Configure uma vez.\nAssista do seu jeito.","Conectado.\nPronto para explorar."};
  int step=pagina==PRONTO?2:pagina==IMPORTAR||pagina==SERVICOS?1:0;
  char kicker[128]; snprintf(kicker,sizeof kicker,"%s · %d / 3",i18n(titulos[pagina]),step+1);
  texto(TXT_ILHA_HORA,kicker,124,312,490,174);
  float storyHeight=blocoEstilo(TXT_V2_A3TIT,i18n(story[pagina]),124,345,490,56,4);
  bloco(i18n("Reúna addons, catálogos e legendas. Configure os detalhes no celular e acompanhe tudo nesta TV."),124,345+storyHeight+34,485,5);
  const char *steps[]={i18n("Escolha sua instância"),i18n("Conecte e configure"),i18n("Valide nesta TV")};
  for(int i=0;i<3;i++) {
    int done=i<step;
    gfx_cor((GfxRect){124,800+i*49,30,30},.5f,.35f,.45f,.47f,i==step?.6f:.22f);
    if(done) gfx_icone((GfxRect){129,805+i*49,20,20},"aj_check",.7f,.76f,.79f,1);
    else { char number[8]; snprintf(number,sizeof number,"%d",i+1); texto(TXT_ILHA_NUM,number,134,806+i*49,20,200); }
    texto(TXT_ILHA_SUB,steps[i],169,800+i*49,450,195);
  }
  if(pagina!=PRONTO) texto(TXT_ILHA_PERGUNTA,pagina==SEM?i18n("Como deseja conectar?"):pagina==SERVICOS?i18n("Etapas da configuração"):i18n(titulos[pagina]),716,204,pagina==SERVICOS?810:1080,240);
  if(pagina==SEM) {
    linha(0,"aj_globe",i18n("Usar uma instância pública"),i18n("Escolha um servidor listado na documentação."),716,263,1078);
    linha(1,"aj_server",i18n("Conectar meu servidor"),i18n("Informe a origem do seu AIOStreams."),716,421,1078);
    linha(2,"aj_smartphone",i18n("Importar configuração existente"),i18n("Traga seu link de instalação."),716,579,1078);
    gfx_cor((GfxRect){716,745,1078,1},0,1,1,1,.08f);
    bloco(i18n("Debrid é opcional. HTTP, P2P e canais dependem da instância e desta TV."),716,774,1040,4);
  } else if(pagina==INSTANCIA) {
    botao(7,i18n("Meu servidor"),716,263,320); botao(8,i18n("Já tenho o link"),1056,263,350);
    for(int i=0;i<3;i++) gfx_cor((GfxRect){1678+i*42,214,34,4},.5f,.65f,.84f,.86f,i==0?1:.18f);
    int start=maisHosts?4:0;
    for(int i=start;i<7 && i<start+4;i++) {
      float y=345+(i-start)*94;
      linha(i,i==3?"aj_server":"aj_globe",nomes[i],i==2?i18n("Pública · Sem P2P, HTTP ou ao vivo"):i==3?i18n("Nightly · Instância de desenvolvedor"):i18n("Estável · Hospedagem comunitária"),716,y,1078);
      const char *badge=i==hostIdx?i18n("Selecionada"):i==2?i18n("Restrita"):i==3?i18n("Nightly"):NULL;
      if(badge) {
        gfx_cor((GfxRect){1620,y+29,126,32},.2f,i==2?.36f:.25f,i==2?.29f:.32f,i==2?.18f:.34f,.65f);
        texto(TXT_ILHA_NUM,badge,1630,y+38,106,216);
      }
    }
    plrui_linha_foco((GfxRect){716,727,1078,60},20,focoDe(10,0));
    texto(TXT_ILHA_SUB,i18n(maisHosts?"Voltar às instâncias principais":"Também na documentação: Kuu’s, ATBP Hosting, Omni’s"),738,745,986,174);
    gfx_icone((GfxRect){1750,745,22,22},"aj_chevron-right",.68f,.73f,.76f,1);
    ponteiro_alvo(716,727,1078,60,pointer,click,10,0);
    bloco(i18n("Cada instância define limites e trata os dados de acesso nela informados. A lista não indica testes realizados."),738,803,1040,2);
    botao(9,i18n("Continuar"),716,877,360);
  } else if(pagina==SERVICOS) {
    const char *check[]={i18n("Serviços"),i18n("Addons"),i18n("Filtros e ordenação"),i18n("Formatador")};
    const char *sub[]={i18n("1. Serviços: debrid ou Usenet, opcionais"),i18n("2. Addons: escolha suas fontes e catálogos"),i18n("3. Filtros e ordenação: revise suas regras"),i18n("4. Formatador: nomes e descrições das fontes")};
    const char *ico[]={"aj_cloud","aj_puzzle","aj_sliders-horizontal","aj_file-text"};
    plrui_material((GfxRect){1668,205,126,34},12,0,.45f);
    gfx_icone((GfxRect){1678,213,18,18},"aj_smartphone",.8f,.85f,.87f,1);
    texto(TXT_ILHA_HORA,i18n("No celular"),1702,214,84,195);
    for(int i=0;i<4;i++) {
      float y=263+i*88, alpha=itemInicio(0,i,&y);
      plrui_linha_foco((GfxRect){716,y,1078,84},22,focoDe(i,0));
      plrui_material((GfxRect){738,y+16,52,52},16,0,.45f);
      gfx_icone((GfxRect){750,y+28,28,28},ico[i],.8f,.85f,.87f,1);
      char numero[160];snprintf(numero,sizeof numero,"%d. %s",i+1,check[i]);
      texto(TXT_ILHA_NOME,numero,810,y+13,920,238);
      const char *descricao=strchr(sub[i],':');
      texto(TXT_ILHA_SUB,descricao?descricao+2:sub[i],810,y+49,920,174);
      gfx_icone((GfxRect){1750,y+31,22,22},"aj_chevron-right",.68f,.73f,.76f,1);
      ponteiro_alvo(716,y,1078,84,pointer,click,i,0);
      gfx_opacidade_grupo=alpha;
    }
    qrDesenhar(configure,716,640,141);
    texto(TXT_ILHA_NOME,i18n("Salvar e instalar no AIOStreams"),882,660,900,238);
    bloco(i18n("Conclua a instalação no celular e volte com o link do manifesto."),882,700,620,2);
    float x=716+botaoW(4,i18n("Tenho meu link de instalação"),"aj_arrow-right",716,808)+16;
    botaoW(5,i18n("Ver exemplo de conclusão"),"aj_check",x,808);
  } else if(pagina==IMPORTAR) {
    if(entrada!=3) qrDesenhar(configure,716,287,206);
    bloco(i18n("Abra a configuração oficial, salve e copie o link de instalação. Envie pelo pareamento já usado pelo Nuvio."),970,314,790,5);
    texto(TXT_ILHA_SUB,i18n("1. Serviços: debrid ou Usenet, opcionais"),716,530,1040,210);
    texto(TXT_ILHA_SUB,i18n("2. Addons: escolha suas fontes e catálogos"),716,571,1040,210);
    plrui_material((GfxRect){716,655,1078,66},20,0,.7f);
    plrui_linha_foco((GfxRect){716,655,1078,66},20,focoDe(1,0));
    texto(TXT_ILHA_ITEM,i18n("Link de instalação"),738,676,1034,220);
    bloco(i18n("Links pessoais permanecem ocultos. Exporte ou importe arquivos JSON pelo celular."),716,745,1040,2);
    if(entrada==3) qrDesenhar(celular_url(),716,305,210);
    ponteiro_alvo(716,655,1078,66,pointer,click,2,0);
    botao(0,i18n("Enviar pelo celular"),716,842,380); botao(1,i18n("Colar link"),1116,842,350);
  } else {
    if(!snap.valida) {
      texto(TXT_ILHA_PERGUNTA,i18n("Ver exemplo de conclusão"),716,307,1040,240);
      texto(TXT_ILHA_SUB,i18n("Exemplo"),716,365,1040,174);
      bloco(i18n("Abra a configuração oficial, salve e copie o link de instalação. Envie pelo pareamento já usado pelo Nuvio."),716,420,1040,3);
      bloco(i18n("Manifesto reconhecido. A validação não garante a reprodução de cada fonte."),716,555,1040,2);
      botao(0,i18n("Tenho meu link de instalação"),716,728,550);
      botao(1,i18n("Voltar"),1286,728,300);
      return;
    }
    gfx_cor((GfxRect){716,195,84,84},.5f,.25f,.32f,.34f,.55f);
    gfx_icone((GfxRect){736,215,44,44},"aj_check",.65f,.84f,.83f,1);
    texto(TXT_ILHA_PERGUNTA,i18n("AIOStreams adicionado"),716,307,1040,240);
    texto(TXT_ILHA_SUB,snap.nome[0]?snap.nome:"AIOStreams",716,361,1040,174);
    const char *checks[]={i18n("Manifesto"),configStatus(),i18n("Credenciais ocultas")};
    for(int i=0;i<3;i++) {
      gfx_icone((GfxRect){716,411+i*43,24,24},"aj_check",.65f,.84f,.83f,1);
      texto(TXT_ILHA_SUB,checks[i],756,411+i*43,1000,220);
    }
    const char *labels[]={i18n("Catálogos"),i18n("Fontes e serviços"),i18n("Legendas")};
    int counts[]={snap.catalogos,snap.addons-nLegendas(),nLegendas()};
    gfx_cor((GfxRect){716,548,1078,1},0,1,1,1,.08f);
    for(int i=0;i<3;i++) { char count[24]; snprintf(count,sizeof count,"%d",counts[i]);
      texto(TXT_V2_GRUPO,count,716+i*180,576,170,240); texto(TXT_ILHA_SUB,labels[i],716+i*180,618,170,174);
    }
    gfx_cor((GfxRect){716,655,1078,1},0,1,1,1,.08f);
    bloco(i18n("Manifesto reconhecido. A validação não garante a reprodução de cada fonte."),716,679,1040,2);
    botao(0,i18n("Abrir painel"),716,728,370); botao(1,i18n("Escolher catálogos"),1106,728,410);
  }
}
static const char *valorRegra(const char *value) {
  // Translate complete values only: splitting regexes/keywords on commas changes their meaning.
  if(!strcmp(value,"true")) return i18n("Sim");
  if(!strcmp(value,"false")) return i18n("Não");
  if(!strcmp(value,"Portuguese")) return i18n("Português");
  if(!strcmp(value,"English")) return i18n("Inglês");
  if(!strcmp(value,"cached")) return i18n("Em cache");
  if(!strcmp(value,"uncached")) return i18n("Fora do cache");
  if(!strcmp(value,"resolution")) return i18n("Resolução");
  if(!strcmp(value,"size")) return i18n("Tamanho");
  if(!strcmp(value,"language")) return i18n("Idiomas");
  return value;
}
static const char *regraNome(const AioRegra *r) {
  static char name[192]; const char *s=r->nome, *mode="";
  const char *prefix[]={"required","included","excluded","preferred"};
  const char *labels[]={"Obrigatório","Incluído","Excluído","Preferido"};
  for(int i=0;i<4;i++) if(!strncmp(s,prefix[i],strlen(prefix[i]))) { mode=i18n(labels[i]); s+=strlen(prefix[i]); break; }
  const char *label=strstr(s,"resultLimits")?i18n("Resultados"):strstr(s,"Encodes")?i18n("Codec · HDR"):strstr(s,"AudioChannels")?i18n("Áudio"):strstr(s,"ReleaseGroups")?i18n("Grupo"):strstr(s,"Resolutions")?i18n("Resolução e qualidade"):strstr(s,"Languages")||strstr(s,"Subtitles")?i18n("Idiomas"):strstr(s,"Qualities")||strstr(s,"Tags")?i18n("Qualidade"):strstr(s,"StreamTypes")?i18n("Cache e tipos de fonte"):strstr(s,"Seeder")||strstr(s,"seeder")?i18n("Semeadores"):strstr(s,"size")?i18n("Tamanho"):strstr(s,"bitrate")?i18n("Bitrate"):strstr(s,"sortCriteria.")?i18n("Ordenação"):strstr(s,"proxy.")?i18n("Proxy"):strstr(s,"template.")?i18n("Modelo"):strstr(s,"formatter.")?i18n("Formatador"):r->grupo==4?i18n("Regex e palavras-chave"):r->grupo==2?i18n("Idiomas"):r->grupo==8?i18n("Serviços e preferências"):i18n("Cache e tipos de fonte");
  const char *media=strstr(s,"movies")?i18n("Filmes"):strstr(s,"series")?i18n("Séries"):strstr(s,"anime")?"Anime":"";
  const char *scope=strstr(s,".cached")?i18n("Em cache"):strstr(s,".uncached")?i18n("Fora do cache"):strstr(s,".version")?i18n("Versão"):strstr(s,".enabled")?i18n("Ativado"):strstr(s,".custom")?i18n("Modelo próprio"):strstr(s,".proxiedServices")?i18n("Serviços"):strstr(s,".proxiedAddons")?i18n("Addons"):"";
  snprintf(name,sizeof name,"%s%s%s%s%s%s%s",label,mode[0]?" · ":"",mode,media[0]?" · ":"",media,scope[0]?" · ":"",scope);
  return name;
}
static const char *decisao(const AioResultado *r) {
  return r->decisao==1?i18n("Destino externo: fora do player da TV."):r->decisao==2?i18n("Oculto pelos filtros locais"):r->decisao==3?i18n("Fora do cache: escolha manual."):r->decisao==4?i18n("Mantido na lista da TV"):i18n("Resultado recebido. Compatibilidade ainda não verificada.");
}
// Mock-style why: playback clause, video facts, then the decision tail. Every
// clause comes from data the snapshot really carries; no rank means no claim.
static const char *motivo(const AioResultado *r) {
  static char b[512];
  if(r->decisao==1) return i18n("Destino externo: o Nuvio entrega ao celular; nada é reproduzido na TV.");
  if(r->decisao==2) return i18n("Os filtros locais do Nuvio ocultam esta fonte da lista automática.");
  const char *modo=r->cache==1?i18n("Reprodução em cache no debrid."):r->cache==0?i18n("Reprodução sem cache no debrid."):i18n("HTTP direto, cache não se aplica.");
  char video[128];
  if(r->altura>0&&strstr(r->nome,"MP4")) snprintf(video,sizeof video,i18n("%dp conhecido; MP4 compatível com o player desta TV."),r->altura);
  else if(r->altura>0) snprintf(video,sizeof video,i18n("%dp conhecido."),r->altura);
  else snprintf(video,sizeof video,"%s",i18n("Resolução desconhecida"));
  const char *fim=r->decisao==3?i18n("Fora da escolha automática; fica para escolha manual."):r->decisao==4?i18n("Mantida na lista com a posição calculada pelo Nuvio."):i18n("Sem lista de reprodução carregada, a posição não é calculada.");
  snprintf(b,sizeof b,i18n("%s %s %s"),modo,video,fim);
  return b;
}
static void tituloDesenhar(void) {
  painel(72,148,1362,824); painel(1458,148,390,824);
  texto(TXT_ILHA_TITULO,i18n("Inspeção do título"),108,184,1080,242);
  texto(TXT_ILHA_SUB,alvo,108,245,1040,174);
  botao(0,i18n("Escolher título"),108,283,340); botao(1,i18n("Atualizar"),470,283,240);
  botao(nLinhas()-1,i18n(evidenciaSelecionada?"Consulta de fontes":"Estatísticas"),730,283,360);
  if(evidenciaSelecionada) {
    listaPos=janela(6,snap.evidencias,5,1); int first=(int)listaPos;
    for(int i=first;i<snap.evidencias && i<first+5;i++) {
      AioEvidencia *e=&snap.evidencia[i]; float y=380+(i-listaPos)*98, alpha=itemInicio(0,i-first,&y);
      plrui_linha_foco((GfxRect){108,y,1290,90},22,focoDe(i+2,0));
      gfx_icone((GfxRect){125,y+18,24,24},e->erro?"aj_info":"aj_activity",.7f,.8f,.82f,1);
      texto(TXT_ILHA_NOME,e->titulo,168,y+12,1200,239);
      texto(TXT_ILHA_SUB,e->descricao,168,y+48,1200,174);
      ponteiro_alvo(108,y,1290,90,pointer,click,i+2,0);
      gfx_opacidade_grupo=alpha;
    }
    if(!snap.evidencias) bloco(i18n("Não informado"),125,390,1100,2);
    texto(TXT_ILHA_SECAO,i18n("Estatísticas"),1490,184,326,238);
    bloco(i18n("Ordem recebida; rejeições anteriores não são fornecidas pelo manifesto."),1490,270,326,5);
    return;
  }
  texto(TXT_ILHA_HORA,i18n("AIO # / Fonte recebida"),125,363,800,148);
  texto(TXT_ILHA_HORA,i18n("Posição na TV"),1025,363,150,148);
  texto(TXT_ILHA_HORA,i18n("Decisão Nuvio"),1175,363,220,148);
  listaPos=janela(9,snap.resultados,8,1); int first=(int)listaPos;
  for(int i=first;i<snap.resultados && i<first+8;i++) {
    float y=389+(i-listaPos)*60, alpha=itemInicio(0,i-first,&y); AioResultado *r=&snap.resultado[i];
    plrui_linha_foco((GfxRect){108,y,1290,59},22,focoDe(i+2,0));
    char num[24]; snprintf(num,sizeof num,"%d",i+1); texto(TXT_ILHA_SUB,num,125,y+20,50,150);
    texto(TXT_ILHA_SECAO,r->nome[0]?r->nome:i18n("Fonte sem nome"),193,y+7,810,238);
    char sub[256]; snprintf(sub,sizeof sub,"%s · %s",i18n(r->tipo),r->cache<0?i18n("Cache desconhecido"):r->cache?i18n("Em cache"):i18n("Fora do cache"));
    texto(TXT_ILHA_NUM,sub,193,y+35,810,174);
    snprintf(num,sizeof num,r->rank?"%d":"—",r->rank); texto(TXT_ILHA_SUB,num,1034,y+20,120,222);
    texto(TXT_ILHA_NUM,decisao(r),1160,y+22,230,184);
    ponteiro_alvo(108,y,1290,60,pointer,click,i+2,0);
    gfx_opacidade_grupo=alpha;
  }
  texto(TXT_ILHA_NUM,i18n("Ordem recebida; rejeições anteriores não são fornecidas pelo manifesto."),124,890,1270,150);
  int selected=foco>=2?foco-2:0;
  texto(TXT_ILHA_HORA,i18n("Decisão Nuvio"),1490,184,326,174);
  if(selected<snap.resultados) {
    AioResultado *r=&snap.resultado[selected];
    blocoEstilo(TXT_ILHA_NOME,r->nome,1490,217,326,34,3);
    static const char *const regras[]={"Equilíbrio","Qualidade máxima","Começar rápido"};
    int prio=ajustes_fonte_prioridade(); if(prio<0||prio>2) prio=0;
    char posicao[24]; snprintf(posicao,sizeof posicao,r->rank?"%d":"—",r->rank);
    const char *chaves[]={i18n("AIOStreams"),i18n("Posição Nuvio"),i18n("Regra automática")};
    const char *valores[]={i18n("Recebido"),posicao,i18n(regras[prio])};
    for(int i=0;i<3;i++) {
      texto(TXT_ILHA_SUB,chaves[i],1490,290+i*34,180,220);
      texto(TXT_ILHA_SUB,valores[i],1672,290+i*34,114,236);
    }
    texto(TXT_ILHA_HORA,i18n("Motivo da decisão"),1490,412,326,174);
    bloco(motivo(r),1490,436,326,6);
  }
  char stats[192]; snprintf(stats,sizeof stats,i18n("%d estatísticas · %d erros · %d sem campos obrigatórios"),snap.estatisticas,snap.erros,snap.filtrados);
  bloco(snap.search?stats:i18n("Não informado"),1490,706,326,2);
  bloco(i18n("Ordem e regras dependem desta TV. A inspeção não abre links para verificar reprodução."),1490,780,326,5);
  if(linkSelecionado>=0) qrDesenhar(celular_url(),1490,580,176);
  if(!snap.resultados) bloco(i18n("Escolha um ID de título para consultar as fontes. Nenhum link de reprodução será aberto."),124,420,1100,4);
}
static int dashboardDetail(int card,int index) {
  for(int i=0;i<snap.detalhes;i++) {
    int g=snap.detalhe[i].grupo;
    if((card==2 && g<=4) || (card==3 && (g==5 || g==6))) if(!index--) return i;
  }
  return -1;
}
static int dashboardTotal(int card) {
  if(card==0) return snap.addons;
  if(card==1) return snap.servicos+1;
  if(card==4) return snap.catalogos;
  if(card==5) return 4+snap.evidencias;
  int n=0; for(int i=0;i<snap.detalhes;i++) {
    int g=snap.detalhe[i].grupo;
    if((card==2 && g<=4) || (card==3 && (g==5 || g==6))) n++;
  }
  return n;
}
static void dashboardLabel(const char *key,char *out,size_t n) {
  const char *prefix[]={"excluded","required","included","preferred"};
  const char *verb[]={"Excluir","Exigir","Incluir","Preferir"};
  const char *suffix[]={"Resolutions","Qualities","VisualTags","AudioTags","Languages","Subtitles","StreamTypes","Encodes","AudioChannels","ReleaseGroups","RegexPatterns","Keywords","StreamExpressions"};
  const char *label[]={"resolução","qualidade","vídeo","áudio","idiomas","legendas","tipo de fonte","codec","canais de áudio","grupo de lançamento","regex","palavras-chave","expressão"};
  for(int i=0;i<4;i++) for(int j=0;j<13;j++) {
    char field[96]; snprintf(field,sizeof field,"%s%s",prefix[i],suffix[j]);
    if(!strncmp(key,field,strlen(field))) { snprintf(out,n,"%s · %s",i18n(verb[i]),i18n(label[j])); return; }
  }
  if(!strncmp(key,"sortCriteria.",13)) snprintf(out,n,"%s · %s",i18n("Ordenação"),key+13);
  else if(!strncmp(key,"formatter.",10)) snprintf(out,n,"%s · %s",i18n("Formatador"),key+10);
  else if(!strncmp(key,"resultLimits",12)) snprintf(out,n,"%s · %s",i18n("Limites"),key+12);
  else snprintf(out,n,"%s",key);
}
static void dashboardRow(int card,int row,char *title,size_t nt,char *value,size_t nv) {
  title[0]=value[0]=0;
  if(card==0 || (card==1 && row<snap.servicos)) {
    AioItem *item=card?&snap.servico[row]:&snap.addon[row];
    snprintf(title,nt,"%s",item->nome);
    snprintf(value,nv,"%s",i18n(item->ativo?"Ativado":"Desligado"));
  } else if(card==1) {
    snprintf(title,nt,"%s",i18n("Status de debrid"));
    snprintf(value,nv,"%s",i18n("Cota e assinatura não informadas"));
  } else if(card==2 || card==3) {
    int i=dashboardDetail(card,row); if(i<0) return;
    dashboardLabel(snap.detalhe[i].nome,title,nt);
    snprintf(value,nv,"%s",!strcmp(snap.detalhe[i].valor,"true")?i18n("Ativado"):!strcmp(snap.detalhe[i].valor,"false")?i18n("Desligado"):snap.detalhe[i].valor);
  } else if(card==4) {
    snprintf(title,nt,"%s",snap.catalogo[row].nome);
    snprintf(value,nv,"%s",!strcmp(snap.catalogo[row].tipo,"movie")?i18n("Filmes"):!strcmp(snap.catalogo[row].tipo,"series")?i18n("Séries"):snap.catalogo[row].tipo);
  } else if(row==0) {
    snprintf(title,nt,"%s",i18n("Manifesto"));
    snprintf(value,nv,"HTTP %d · %d ms",snap.http[0],snap.ms[0]);
  } else if(row==1) {
    snprintf(title,nt,"%s",i18n("Status da instância"));
    snprintf(value,nv,"HTTP %d · %d ms",snap.http[1],snap.ms[1]);
  } else if(row==2) {
    snprintf(title,nt,"%s",i18n("Última consulta de fontes"));
    if(snap.http[3]) snprintf(value,nv,"HTTP %d · %d ms",snap.http[3],snap.ms[3]);
    else snprintf(value,nv,"%s",i18n("Ainda não medida · Inspecionar título"));
  } else if(row==3) {
    snprintf(title,nt,"%s",i18n("Estatísticas e erros"));
    if(snap.estatisticas>=0 && snap.erros>=0) snprintf(value,nv,"%d / %d",snap.estatisticas,snap.erros);
    else snprintf(value,nv,"%s",i18n("Não informados pela última consulta"));
  } else {
    AioEvidencia *e=&snap.evidencia[row-4];
    snprintf(title,nt,"%s",e->titulo); snprintf(value,nv,"%s",e->descricao);
  }
}
// Dashboard palette (DESIGN.md §3): values primary, labels a step down; the
// accent marks state and the key action; green/amber/red only say healthy,
// partial or failed. Measured on the solid card: 14.9:1 and 7.5:1.
static const float aioTexto[3]={243/255.f,242/255.f,239/255.f};
static const float aioOk[3]={.373f,.827f,.553f}, aioErr[3]={1,.482f,.447f}, aioWarn[3]={.949f,.757f,.306f};
static float txtCor(TxtEstilo e,const char *s,float x,float y,float w,const float *c,float a,int direita) {
  float scale=gfx_escala_txt(); TXT_INI();
  TxtLinha l=txt_linha_corta(e,s,(int)(c[0]*255),(int)(c[1]*255),(int)(c[2]*255),(int)(255*a),w/scale);
  float lw=l.w*scale;
  txt_desenhar(l,(direita?x-lw:x)/scale,y/scale); TXT_FIM();
  return lw;
}
static float largura(TxtEstilo e,const char *s) { return t21_texto_linha(e,s,255,255,255,255,4000).w; }
// One surface per card, never a panel behind them; focus lifts it a step lighter, no ring.
static void superficie(GfxRect r,float raio,float claro,float foco) {
  float g=6*foco; r=(GfxRect){r.x-g,r.y-g,r.w+2*g,r.h+2*g};
  plrui_material(r,raio,0,1);
  gfx_cor(r,raio/r.h,1,1,1,claro+.075f*foco);
}
// State pill: tinted fill, dot and label in the state colour; off is plain grey.
static float selo(float x,float y,const char *s,const float *c,int ligado,int direita) {
  float tw=largura(TXT_AJ_CHIP,s), w=tw+(ligado?40:28);
  if(direita) x-=w;
  gfx_cor((GfxRect){x,y,w,32},.5f,c[0],c[1],c[2],ligado?.16f:.08f);
  if(ligado) gfx_cor((GfxRect){x+14,y+11.5f,9,9},.5f,c[0],c[1],c[2],1);
  txtCor(TXT_AJ_CHIP,s,x+(ligado?29:14),y+6,tw+4,c,ligado?1:.72f,0);
  return w;
}
static void monograma(const char *nome,float x,float y,float d) {
  char m[3]={0}; int n=0, novo=1;
  for(const unsigned char *p=(const unsigned char *)nome;*p && n<2;p++) {
    if(isalnum(*p)) { if(novo || isupper(*p)) m[n++]=(char)toupper(*p); novo=0; } else novo=1;
  }
  gfx_cor((GfxRect){x,y,d,d},.5f,1,1,1,.09f);
  txtCor(TXT_G16B,m,x+(d-largura(TXT_G16B,m))/2,y+d/2-10,d,aioTexto,1,0);
}
static void disco(const char *icone,float x,float y,float d) {
  gfx_cor((GfxRect){x,y,d,d},.5f,1,1,1,.09f);
  gfx_icone((GfxRect){x+d*.25f,y+d*.25f,d*.5f,d*.5f},icone,.953f,.949f,.937f,.9f);
}
static int dashboardCompleto(void) { return snap.valida && snap.configurada; }
static int cabecalhoIds(int *ids) {
  int n=0; if(snap.valida) ids[n++]=10;
  ids[n++]=6; if(dashboardCompleto()) ids[n++]=9;
  return n;
}
static int dashboardCta(void) { return dashboardCompleto()?-1:snap.valida?9:10; }
static const char *botaoNome(int id) {
  return i18n(id==6?"Inspecionar título":id==9?(snap.configAccess==AIO_AVAILABLE?"Reconectar":"Conectar pelo celular"):!snap.valida?"Tentar de novo":"Atualizar");
}
static const char *botaoIcone(int id) { return id==6?"aj_funnel":id==9?"aj_smartphone":"aj_rotate-cw"; }
static void botaoPrimario(int i,float xd,float y) {
  const char *nome=botaoNome(i), *ic=botaoIcone(i);
  float scale=gfx_escala_txt(), w=botao_largura(nome,ic,1);
  GfxRect r={xd/scale-w,y/scale,w,T21_CTRL_H};
  // At rest the key action keeps an accent tint, so it reads as the way forward before focus.
  float ar,ag,ab; ajustes_acento(&ar,&ag,&ab);
  gfx_cor((GfxRect){r.x*scale,r.y*scale,r.w*scale,r.h*scale},.5f,ar,ag,ab,.24f*(1-focoDe(i,0)));
  TXT_INI(); botao_pilula(r,nome,ic,focoDe(i,0),1,0,1); TXT_FIM();
  ponteiro_alvo(r.x*scale,r.y*scale,r.w*scale,r.h*scale,pointer,click,i,0);
}
static void dashboardLinhaDesenhar(int c,int row,float x,float y,float w) {
  char t[192],v[384]; dashboardRow(c,row,t,sizeof t,v,sizeof v);
  float xl=x+22, xr=x+w-22, cy=y+36, room=w-44;
  if(c==0 || (c==1 && row<snap.servicos)) {
    AioItem *it=c?&snap.servico[row]:&snap.addon[row];
    monograma(it->nome,xl,cy-21,42);
    float sw=selo(xr,cy-16,i18n(it->ativo?"Ativado":"Desligado"),it->ativo?aioOk:aioTexto,it->ativo,1);
    txtCor(TXT_G22M,it->nome,xl+56,cy-14,room-70-sw,aioTexto,1,0);
  } else if(c==1) {
    disco("aj_cloud",xl,cy-21,42);
    float vw=txtCor(TXT_G18R,v,xr,cy-11,room*.5f,aioTexto,.68f,1);
    txtCor(TXT_G22M,t,xl+56,cy-14,room-70-vw,aioTexto,1,0);
  } else if(c==2) {
    float tw=fminf(largura(TXT_AJ_CHIP,v),room*.55f)+28;
    gfx_cor((GfxRect){xr-tw,cy-16,tw,32},.5f,1,1,1,.1f);
    txtCor(TXT_AJ_CHIP,v,xr-tw+14,cy-10,tw-20,aioTexto,1,0);
    txtCor(TXT_G20M,t,xl,cy-13,room-tw-16,aioTexto,.68f,0);
  } else if(c==3) {
    int i=dashboardDetail(3,row); char n[8], chave[64]; const char *rumo="";
    snprintf(n,sizeof n,"%d",row+1);
    gfx_cor((GfxRect){xl,cy-21,42,42},.5f,1,1,1,.09f);
    txtCor(TXT_G16B,n,xl+(42-largura(TXT_G16B,n))/2,cy-10,42,aioTexto,1,0);
    if(i>=0 && !strncmp(snap.detalhe[i].nome,"sortCriteria.",13)) {
      size_t k=0; const char *s=snap.detalhe[i].valor;
      while(s[k] && s[k]!=' ' && s[k]!='(' && k<sizeof chave-1) { chave[k]=s[k]; k++; }
      chave[k]=0; snprintf(t,sizeof t,"%s",valorRegra(chave));
      rumo=strstr(s,"desc")?i18n("Maior primeiro"):strstr(s,"asc")?i18n("Menor primeiro"):"";
    } else rumo=v;
    float vw=txtCor(TXT_G18R,rumo,xr,cy-11,room*.45f,aioTexto,.68f,1);
    txtCor(TXT_G22M,t,xl+56,cy-14,room-70-vw,aioTexto,1,0);
  } else if(c==4) {
    disco(!strcmp(snap.catalogo[row].tipo,"series")?"aj_tv":"aj_film",xl,cy-21,42);
    float vw=txtCor(TXT_G18R,v,xr,cy-11,room*.4f,aioTexto,.68f,1);
    txtCor(TXT_G22M,t,xl+56,cy-14,room-70-vw,aioTexto,1,0);
  } else if(row<3) {
    int k=row==2?3:row, code=snap.http[k], ok=code>=200 && code<300;
    const float *cor=!code?aioTexto:ok?aioOk:aioErr;
    gfx_cor((GfxRect){xl+4,cy-5,10,10},.5f,cor[0],cor[1],cor[2],code?1:.4f);
    float vw;
    if(ok) {
      char ms[24],http[24]; snprintf(ms,sizeof ms,"%d ms",snap.ms[k]); snprintf(http,sizeof http,"HTTP %d",code);
      vw=txtCor(TXT_G20B,ms,xr,cy-12,140,aioTexto,1,1);
      vw+=14+txtCor(TXT_G18R,http,xr-vw-14,cy-11,140,aioTexto,.68f,1);
    } else if(code) {
      char http[24]; snprintf(http,sizeof http,"HTTP %d",code);
      vw=txtCor(TXT_G20B,http,xr,cy-12,160,aioErr,1,1);
    } else vw=txtCor(TXT_G18R,v,xr,cy-11,room*.5f,aioTexto,.68f,1);
    txtCor(TXT_G22M,t,xl+30,cy-14,room-44-vw,aioTexto,1,0);
  } else {
    float vw=txtCor(TXT_G18R,v,xr,cy-11,room*.5f,aioTexto,.68f,1);
    txtCor(TXT_G22M,t,xl,cy-14,room-14-vw,aioTexto,1,0);
  }
}
static void dashboardCartao(int c,GfxRect r,int compacto) {
  float f=focoDe(c,0), y=r.y, alpha=itemInicio(c%3,c/3,&y); r.y=y;
  superficie(r,26,.035f,f);
  float ar,ag,ab, ink=ajustes_acento_tinta(&ar,&ag,&ab);
  GfxRect d={r.x+20,r.y+18,48,48};
  gfx_cor(d,15/48.f,1,1,1,.08f*(1-f)); gfx_cor(d,15/48.f,ar,ag,ab,f);
  float ic=.953f*(1-f)+ink*f;
  const char *ico[]={"aj_puzzle","aj_shield-check","aj_sliders-horizontal","aj_layers","aj_library","aj_activity"};
  const char *titles[]={"Add-ons e presets","Serviços","Filtros e limites","Ordenação e formato","Catálogos","Saúde"};
  gfx_icone((GfxRect){d.x+11,d.y+11,26,26},ico[c],ic,ic,ic,1);
  int total=dashboardTotal(c), travado=(!snap.configurada && c<4) || (!snap.valida && c<5);
  if(compacto) {
    texto(TXT_ILHA_NOME,i18n(titles[c]),r.x+82,r.y+28,r.w-104,243);
    gfx_icone((GfxRect){r.x+24,r.y+76,22,22},"aj_lock",.953f,.949f,.937f,.68f);
    txtCor(TXT_ILHA_SUB_L,i18n(snap.valida?"Acesso à configuração necessário":"Instância indisponível"),r.x+58,r.y+74,r.w-80,aioTexto,.68f,0);
  } else {
    char count[16]; if(travado) strcpy(count,"—"); else snprintf(count,sizeof count,"%d",c==1?snap.servicos:total);
    float cw=largura(TXT_AJ_CHIP,count)+26;
    gfx_cor((GfxRect){r.x+r.w-20-cw,r.y+28,cw,30},.5f,1,1,1,.07f);
    txtCor(TXT_AJ_CHIP,count,r.x+r.w-20-cw+13,r.y+33,cw,aioTexto,.72f,0);
    texto(TXT_ILHA_NOME,i18n(titles[c]),r.x+82,r.y+28,r.w-124-cw,243);
    int vis=(int)((r.h-88)/72); dashboardVis[c]=vis<1?1:vis;
    if(travado) {
      gfx_icone((GfxRect){r.x+24,r.y+92,24,24},"aj_server-crash",.953f,.949f,.937f,.68f);
      txtCor(TXT_ILHA_SUB_L,i18n("Manifesto indisponível. Atualize para tentar novamente."),r.x+60,r.y+91,r.w-84,aioTexto,.68f,0);
    } else if(!total) {
      bloco(i18n(c==4?"Nenhum catálogo publicado pelo manifesto.":c==0?"Nenhum preset declarado nesta configuração.":"Sem regras personalizadas nesta seção."),r.x+24,r.y+92,r.w-48,3);
    } else {
      vis=dashboardVis[c];
      dashboardLinha[c]=(int)anim_clamp(dashboardLinha[c],0,fmaxf(0,total-vis));
      GfxRecorteAnt clip=gfx_recorte_dentro(r.x,r.y+76,r.w,r.h-84);
      float scroll=anim_clamp(dashboardScroll[c],0,fmaxf(0,total-vis));
      for(int row=(int)scroll;row<total && row<=(int)scroll+vis;row++)
        dashboardLinhaDesenhar(c,row,r.x,r.y+80+(row-scroll)*72,r.w);
      gfx_recorte_voltar(clip);
      if(total>vis) {
        float th=r.h-104, h=fmaxf(24,th*vis/total), sy=r.y+86+(th-h)*scroll/(total-vis);
        gfx_cor((GfxRect){r.x+r.w-11,r.y+86,4,th},.5f,1,1,1,.07f*f);
        gfx_cor((GfxRect){r.x+r.w-11,sy,4,h},.5f,ar,ag,ab,f);
      }
    }
  }
  if(!lateral) ponteiro_alvo(r.x,r.y,r.w,r.h,pointer,click,c,0);
  gfx_opacidade_grupo=alpha;
}
static void dashboard(void) {
  float L=72+88+28+232*anim_clamp(sidebarOpen,0,1), W=1848-L;
  int completo=dashboardCompleto();
  int ids[3],n=cabecalhoIds(ids); float bx=1848, scale=gfx_escala_txt();
  for(int i=n-1;i>=0;i--) {
    bx-=botao_largura(botaoNome(ids[i]),botaoIcone(ids[i]),0)*scale;
    botaoIc(ids[i],botaoNome(ids[i]),botaoIcone(ids[i]),bx,160,0); bx-=16;
  }
  texto(TXT_ILHA_TITULO,snap.nome[0]?snap.nome:"AIOStreams",L,146,bx-L-24,243);
  char updated[32]="", status[320];
  if(ultimaAtualizacao) { struct tm t; localtime_r(&ultimaAtualizacao,&t); strftime(updated,sizeof updated,"%H:%M:%S",&t); }
  const float *cor=!snap.valida?aioErr:snap.configurada?aioOk:aioWarn;
  float pw=selo(L,202,i18n(!snap.valida?"Indisponível":snap.configurada?"Conectado":"Só streams"),cor,1,0);
  if(snap.valida) snprintf(status,sizeof status,"%s%s%s · %s %s%s%s",snap.versaoInstancia[0]?snap.versaoInstancia:snap.versao[0]?snap.versao:i18n("Não informado"),
    snap.canal[0]?" · ":"",snap.canal,i18n("Atualizado"),updated[0]?updated:i18n("Ainda não"),snap.detalhesOmitidos?" · ":"",snap.detalhesOmitidos?i18n("Configuração no celular"):"");
  else snprintf(status,sizeof status,"HTTP %d · %s %s",snap.http[0],i18n("Atualizado"),updated[0]?updated:i18n("Ainda não"));
  txtCor(TXT_ILHA_SUB_L,status,L+pw+14,205,bx-L-pw-30,aioTexto,.68f,0);
  if(completo) {
    const char *labels[]={"Add-ons","Serviços","Regras e preferências","Catálogos"};
    const char *tileIc[]={"aj_puzzle","aj_shield-check","aj_sliders-horizontal","aj_library"};
    int on[2]={0,0}, filmes=0;
    for(int i=0;i<snap.addons;i++) on[0]+=snap.addon[i].ativo;
    for(int i=0;i<snap.servicos;i++) on[1]+=snap.servico[i].ativo;
    for(int i=0;i<snap.catalogos;i++) filmes+=!strcmp(snap.catalogo[i].tipo,"movie");
    int numbers[]={snap.addons,snap.servicos,snap.detalhes,snap.catalogos};
    float tw=(W-60)/4;
    for(int i=0;i<4;i++) {
      float x=L+i*(tw+20), y=248, alpha=itemInicio(i,0,&y);
      superficie((GfxRect){x,y,tw,132},24,.035f,0);
      gfx_icone((GfxRect){x+tw-54,y+22,30,30},tileIc[i],.953f,.949f,.937f,.42f);
      char num[16], sub[96]; snprintf(num,sizeof num,"%d",numbers[i]);
      if(i<2) snprintf(sub,sizeof sub,i18n("%d de %d ativos"),on[i],numbers[i]);
      else if(i==2) snprintf(sub,sizeof sub,i18n("%d filtros · %d ordenação"),dashboardTotal(2),dashboardTotal(3));
      else snprintf(sub,sizeof sub,i18n("%d filmes · %d séries"),filmes,snap.catalogos-filmes);
      texto(TXT_AJ_NUM58,num,x+24,y+12,tw-90,243);
      txtCor(TXT_G20B,i18n(labels[i]),x+24,y+76,tw-48,aioTexto,1,0);
      txtCor(TXT_ILHA_GENERO,sub,x+24,y+101,tw-48,aioTexto,.68f,0);
      gfx_opacidade_grupo=alpha;
    }
    float cw=(W-40)/3;
    for(int c=0;c<6;c++) dashboardCartao(c,(GfxRect){L+c%3*(cw+20),400+c/3*326,cw,306},0);
  } else {
    float ar,ag,ab; ajustes_acento(&ar,&ag,&ab);
    float bc[3]={snap.valida?ar:aioErr[0],snap.valida?ag:aioErr[1],snap.valida?ab:aioErr[2]};
    superficie((GfxRect){L,248,W,132},26,.075f,0);
    gfx_cor((GfxRect){L+30,278,72,72},22/72.f,bc[0],bc[1],bc[2],.2f);
    gfx_icone((GfxRect){L+47,295,38,38},snap.valida?"aj_smartphone":"aj_server-crash",bc[0]*.6f+.4f,bc[1]*.6f+.4f,bc[2]*.6f+.4f,1);
    int cta=dashboardCta();
    float cw=botao_largura(botaoNome(cta),botaoIcone(cta),1)*scale, tx=L+W-30-cw;
    botaoPrimario(cta,L+W-30,286);
    if(snap.valida) {
      char num[16]; snprintf(num,sizeof num,"%d",snap.catalogos);
      txtCor(TXT_LOG_N44,num,tx-30,270,200,aioTexto,1,1);
      float lw=largura(TXT_ILHA_GENERO,i18n("Catálogos"));
      txtCor(TXT_ILHA_GENERO,i18n("Catálogos"),tx-30-lw,324,lw+4,aioTexto,.68f,0);
      tx-=40+fmaxf(lw,60);
    }
    char sub[256];
    if(snap.valida) snprintf(sub,sizeof sub,"%s",i18n("Add-ons, serviços e regras aparecem depois de entrar pelo celular. Os streams já funcionam."));
    else snprintf(sub,sizeof sub,i18n("O servidor devolveu HTTP %d. Em HTTP 429, aguarde antes de tentar de novo."),snap.http[0]);
    txtCor(TXT_G30B,i18n(snap.valida?"Veja sua configuração completa":"A instância não respondeu"),L+128,278,tx-L-150,aioTexto,1,0);
    txtCor(TXT_ILHA_TEXTO,sub,L+128,322,tx-L-150,aioTexto,.68f,0);
    float tw=(W-60)/4, gw=(W-20)/2;
    for(int c=0;c<4;c++) dashboardCartao(c,(GfxRect){L+c*(tw+20),400,tw,110},1);
    for(int c=4;c<6;c++) dashboardCartao(c,(GfxRect){L+(c-4)*(gw+20),530,gw,502},0);
  }
}
// The rail is its own island: lighter than the cards and tinted toward the
// accent, so it never reads as page. Focus opens it and pushes the page.
static void sidebar(void) {
  float open=anim_clamp(sidebarOpen,0,1), w=88+232*open, h=884, raio=44-12*open;
  GfxRect r={72,148,w,h};
  float ar,ag,ab, ink=ajustes_acento_tinta(&ar,&ag,&ab);
  plrui_material(r,raio,1,1);
  gfx_cor(r,raio/h,1,1,1,.06f); gfx_cor(r,raio/h,ar,ag,ab,.09f);
  GfxRecorteAnt clip=gfx_recorte_dentro(72,148,w,h);
  float base=gfx_opacidade_grupo, rot=open*open;
  gfx_cor((GfxRect){86,164,60,60},.5f,1,1,1,.08f);
  GLuint logo=tex_obter(arteLogo); if(logo) gfx_rect((GfxRect){100,178,32,32},logo,GFX_MARCA,0,0,0,0,1,1,1,1);
  gfx_opacidade_grupo=base*rot;
  texto(TXT_ILHA_NOME,"AIOStreams",164,170,200,243);
  texto(TXT_ILHA_NUM,i18n("Seu addon conectado"),164,204,200,174);
  gfx_opacidade_grupo=base;
  const char *nav[]={i18n("Visão geral"),i18n("Catálogos"),i18n("Fontes e serviços"),i18n("Links"),i18n("Legendas"),i18n("Filtros e ordenação"),i18n("Saúde")};
  for(int i=0;i<7;i++) {
    GfxRect it={84,248+i*76,w-24,64};
    int ativa=pagina==PAINEL+i; float f=focoDe(i,1);
    if(ativa) gfx_cor(it,.5f,ar,ag,ab,.24f*(1-f));
    if(f>.002f) gfx_cor(it,.5f,ar,ag,ab,f);
    float c[3]; for(int k=0;k<3;k++) { float b=ativa?(k==0?ar:k==1?ag:ab)*.6f+.4f:.953f; c[k]=b*(1-f)+ink*f; }
    gfx_icone((GfxRect){it.x+19,it.y+19,26,26},icons[i],c[0],c[1],c[2],ativa||f>.5f?1:.72f);
    gfx_opacidade_grupo=base*rot;
    txtCor(TXT_ILHA_ITEM,nav[i],it.x+64,it.y+20,it.w-80,c,1,0);
    gfx_opacidade_grupo=base;
    ponteiro_alvo(it.x,it.y,it.w,it.h,pointer,click,i,1);
  }
  float by=148+h-66;
  gfx_cor((GfxRect){94,by,44,44},.5f,1,1,1,.1f);
  gfx_icone((GfxRect){105,by+11,22,22},open>.5f?"pl_chevron-left":"aj_chevron-right",.953f,.949f,.937f,1);
  const float *cor=!snap.valida?aioErr:snap.configurada?aioOk:aioWarn;
  gfx_opacidade_grupo=base*rot;
  gfx_cor((GfxRect){156,by+17,10,10},.5f,cor[0],cor[1],cor[2],1);
  txtCor(TXT_ILHA_SUB_L,i18n(!snap.valida?"Indisponível":snap.configurada?"Conectado":"Só streams"),176,by+10,w-120,aioTexto,.72f,0);
  gfx_opacidade_grupo=base;
  gfx_recorte_voltar(clip);
}
// Section pages: a list island and a facts island that reflow with the rail.
// Rows are collected first so the list can scroll by pixels across group headers.
enum { SV_NADA, SV_TEXTO, SV_SELO, SV_BARRA, SV_SETA };
typedef struct SecLinha {
  int id,tipo,on,ponto,arg; float y,frac;
  const char *grupo,*ic,*mono;
  char nome[160],sub[224],val[64];
  void (*preenche)(struct SecLinha *);
} SecLinha;
static SecLinha secItem[AIO_CATALOG_MAX+48];
static int secN;
static float secL,secLW,secFX,secFY;
static SecLinha *secAdd(int id,const char *grupo,const char *ic,const char *mono,const char *nome,const char *sub) {
  static SecLinha sobra;
  SecLinha *s=secN<(int)(sizeof secItem/sizeof *secItem)?&secItem[secN++]:&sobra;
  s->id=id; s->tipo=SV_NADA; s->on=s->ponto=s->arg=0; s->frac=0; s->preenche=NULL;
  s->grupo=grupo; s->ic=ic; s->mono=mono; s->val[0]=0;
  snprintf(s->nome,sizeof s->nome,"%s",nome); snprintf(s->sub,sizeof s->sub,"%s",sub?sub:"");
  return s;
}
static void secMoldura(void) {
  secL=72+88+28+232*anim_clamp(sidebarOpen,0,1); secLW=1848-secL-460; secFX=1408; secFY=276; secN=0;
  plrui_material((GfxRect){secL,248,secLW,784},28,0,1);
  superficie((GfxRect){secFX,248,440,784},28,.035f,0);
}
static float secTexto(const char *s,float y,int linhas) {
  float a=gfx_opacidade_grupo; gfx_opacidade_grupo=a*.72f;
  float h=blocoEstilo(TXT_ILHA_SUB,s,secFX+28,y,384,28,linhas);
  gfx_opacidade_grupo=a; return h;
}
static void secTopo(const char *kicker,const char *grande,const float *cor,int numero,const char *txt) {
  float s=gfx_escala_txt();
  plrui_kicker(kicker,secFX+28,secFY,243,242,239,.55f);
  secFY+=28;
  if(grande) { txtCor(numero?TXT_AJ_NUM58:TXT_ILHA_TITULO,grande,secFX+28,secFY,384,cor?cor:aioTexto,1,0); secFY+=(numero?70:58)*s; }
  if(txt && *txt) secFY+=12+secTexto(txt,secFY+12,4);
  secFY+=26;
}
static void secFato(const char *a,const char *b,const float *cor) {
  float s=gfx_escala_txt();
  gfx_cor((GfxRect){secFX+28,secFY,384,1},0,1,1,1,.08f);
  float bw=txtCor(TXT_G20B,b,secFX+412,secFY+15,250,cor?cor:aioTexto,1,1);
  txtCor(TXT_G20M,a,secFX+28,secFY+15,384-bw-16,aioTexto,.68f,0);
  secFY+=26*s+30;
}
// Facts island footer: phone setup and refresh, stacked full width.
static void secBotoesFatos(int config,int atualizar) {
  float s=gfx_escala_txt(), ch=T21_CTRL_H*s, y=1032-28-ch;
  botaoIc(atualizar,i18n("Atualizar"),"aj_rotate-cw",secFX+28,y,384);
  botaoIc(config,i18n("Configuração no celular"),"aj_smartphone",secFX+28,y-12-ch,384);
}
// List island footer; wraps to a second line before it leaves the island.
static float secBotoes(const int *ids,const char *const *nomes,int n) {
  float s=gfx_escala_txt(), ch=T21_CTRL_H*s, x0=secL+24, xmax=secL+secLW-24, x=x0;
  int linhas=1;
  for(int i=0;i<n;i++) { float bw=botao_largura(nomes[i],NULL,0)*s; if(x>x0 && x+bw>xmax) { linhas++; x=x0; } x+=bw+14; }
  float y=1032-24-linhas*ch-(linhas-1)*12, topo=y; x=x0;
  for(int i=0;i<n;i++) {
    float bw=botao_largura(nomes[i],NULL,0)*s;
    if(x>x0 && x+bw>xmax) { x=x0; y+=ch+12; }
    botaoIc(ids[i],nomes[i],NULL,x,y,0); x+=bw+14;
  }
  return topo-16;
}
static void secLista(float fim) {
  float top=266, x=secL+16, w=secLW-32, y=0, viewH=fim-top;
  const char *grupo=NULL;
  for(int k=0;k<secN;k++) {
    SecLinha *e=&secItem[k];
    if(e->grupo && (!grupo || strcmp(grupo,e->grupo))) y+=(k?20:0)+36;
    grupo=e->grupo; e->y=y; y+=100;
  }
  float total=y-8, ys=0; int prev=-1;
  // Follow the focus spring between rows so scrolling glides like the other lists.
  for(int k=0;k<secN;k++) if(secItem[k].id>=0 && secItem[k].id<=scrollFoco) prev=k;
  if(prev>=0) {
    int next=-1; for(int k=prev+1;k<secN && next<0;k++) if(secItem[k].id>secItem[prev].id) next=k;
    float t=scrollFoco-secItem[prev].id;
    ys=next<0?secItem[prev].y+t*100:secItem[prev].y+(secItem[next].y-secItem[prev].y)*anim_clamp(t/(secItem[next].id-secItem[prev].id),0,1);
  }
  float off=anim_clamp(ys-fminf(218,fmaxf(0,viewH-100)),0,fmaxf(0,total-viewH));
  GfxRecorteAnt clip=gfx_recorte_dentro(secL,248,secLW,fim-248);
  grupo=NULL;
  for(int k=0;k<secN;k++) {
    SecLinha *e=&secItem[k];
    float yy=top+e->y-off;
    int cabeca=e->grupo && (!grupo || strcmp(grupo,e->grupo)); grupo=e->grupo;
    if(yy+92<248 || yy-36>fim) continue;
    float alpha=gfx_opacidade_grupo, edge=anim_clamp(fminf((yy+92-248)/24,(fim-yy)/24),0,1);
    float entry=entradaDe(0,k<8?k:8);
    gfx_opacidade_grupo=alpha*edge*entry; yy+=(1-entry)*NV_ENTRA_DY;
    if(cabeca) plrui_kicker(e->grupo,x+22,yy-30,243,242,239,.55f);
    if(e->preenche) e->preenche(e);
    if(e->id>=0) plrui_linha_foco((GfxRect){x,yy,w,92},22,focoDe(e->id,0));
    if(e->ponto) {
      const float *c=e->ponto==1?aioOk:e->ponto==2?aioErr:aioTexto;
      gfx_cor((GfxRect){x+22,yy+20,52,52},.5f,1,1,1,.09f);
      gfx_cor((GfxRect){x+41,yy+39,14,14},.5f,c[0],c[1],c[2],e->ponto==3?.4f:1);
    } else if(e->mono) monograma(e->mono,x+22,yy+20,52);
    else if(e->ic) disco(e->ic,x+22,yy+20,52);
    float dir=x+w-24;
    if(e->tipo==SV_TEXTO && e->val[0]) dir-=txtCor(TXT_G20B,e->val,dir,yy+31,300,aioTexto,1,1)+20;
    else if(e->tipo==SV_SELO) dir-=selo(dir,yy+30,e->val,e->on?aioOk:aioTexto,e->on,1)+20;
    else if(e->tipo==SV_SETA) { gfx_icone((GfxRect){dir-22,yy+35,22,22},"aj_chevron-right",.953f,.949f,.937f,.6f); dir-=46; }
    else if(e->tipo==SV_BARRA) {
      if(e->val[0]) txtCor(TXT_G20B,e->val,dir,yy+31,110,aioTexto,1,1);
      float bw=fminf(240,w*.24f), bx=dir-130-bw;
      gfx_cor((GfxRect){bx,yy+41,bw,10},.5f,1,1,1,.08f);
      const float *c=e->on?aioOk:aioErr;
      if(e->frac>0) gfx_cor((GfxRect){bx,yy+41,fmaxf(10,bw*e->frac),10},.5f,c[0],c[1],c[2],1);
      dir=bx-24;
    }
    txtCor(TXT_ILHA_NOME,e->nome,x+94,yy+16,dir-x-94,aioTexto,1,0);
    txtCor(TXT_ILHA_SUB,e->sub,x+94,yy+50,dir-x-94,aioTexto,.68f,0);
    if(e->id>=0 && yy>=247 && yy+92<=fim+1) ponteiro_alvo(x,yy,w,92,pointer,click,e->id,0);
    gfx_opacidade_grupo=alpha;
  }
  gfx_recorte_voltar(clip);
}
static void catalogoPreenche(SecLinha *e) {
  int i=e->arg, f=fileira(i);
  e->on=f>=0 && !fil_linha_oculta(f); e->tipo=SV_SELO;
  snprintf(e->val,sizeof e->val,"%s",i18n(e->on?"Na Home":"Oculto"));
  snprintf(e->sub,sizeof e->sub,"%s · %s",origemCatalogo(i),!strcmp(snap.catalogo[i].tipo,"movie")?i18n("Filmes"):!strcmp(snap.catalogo[i].tipo,"series")?i18n("Séries"):snap.catalogo[i].tipo);
}
static void secoes(void) {
  secMoldura();
  char tmp[256];
  if(pagina==CATALOGOS) {
    int filmes=0;
    for(int i=0;i<snap.catalogos;i++) {
      int serie=!strcmp(snap.catalogo[i].tipo,"series"); filmes+=!strcmp(snap.catalogo[i].tipo,"movie");
      SecLinha *e=secAdd(i,NULL,serie?"aj_tv":"aj_film",NULL,snap.catalogo[i].nome,NULL);
      e->arg=i; e->preenche=catalogoPreenche;
    }
    int ids[]={snap.catalogos,snap.catalogos+1,snap.catalogos+2,snap.catalogos+3};
    const char *nomes[]={i18n("Mover para cima"),i18n("Mover para baixo"),i18n("Atualizar catálogos"),i18n("Gerenciar no celular")};
    secLista(secBotoes(ids,nomes,4));
    snprintf(tmp,sizeof tmp,"%d",snap.catalogos);
    char inv[128]; snprintf(inv,sizeof inv,i18n("%d publicados · %d indisponíveis"),snap.catalogTotal,snap.catalogOmitted);
    secTopo(i18n("Catálogos"),tmp,NULL,1,inv);
    char n[16]; snprintf(n,sizeof n,"%d",filmes); secFato(i18n("Filmes"),n,NULL);
    snprintf(n,sizeof n,"%d",snap.catalogos-filmes); secFato(i18n("Séries"),n,NULL);
    secFY+=10; secTexto(i18n("CH+ / CH-: mover na Home"),secFY,2);
    secBotoesFatos(nLinhas()-2,nLinhas()-1);
  } else if(pagina==FONTES) {
    int row=0, emCache=0, semCache=0, httpDireto=0;
    const char *serv=serviceAtivo();
    for(int i=0;i<snap.resultados;i++) {
      emCache+=snap.resultado[i].cache==1; semCache+=snap.resultado[i].cache==0;
      httpDireto+=!strcmp(snap.resultado[i].tipo,"HTTP") && snap.resultado[i].cache<0;
    }
    for(int i=0;i<snap.addons+snap.servicos;i++) {
      int addon=i<snap.addons;
      AioItem *item=addon?&snap.addon[i]:&snap.servico[i-snap.addons];
      if(addon && item->legenda) continue;
      int total=0,cached=0,uncached=0;
      for(int k=0;k<snap.resultados;k++) if(!strcmp(snap.resultado[k].provedor,item->nome)) {
        total++; cached+=snap.resultado[k].cache==1; uncached+=snap.resultado[k].cache==0;
      }
      if(total&&serv&&cached) snprintf(tmp,sizeof tmp,uncached?i18n("%s · %d em cache / %d sem cache"):i18n("%s · %d em cache"),serv,cached,uncached);
      else if(total) snprintf(tmp,sizeof tmp,"%s",i18n("HTTP direto · Cache não se aplica"));
      else snprintf(tmp,sizeof tmp,"%s",addon?i18n(item->ativo?"Ativado":"Desativado"):i18n("Cota e assinatura não informadas"));
      SecLinha *e=secAdd(row++,i18n(addon?"Add-ons":"Serviços"),NULL,item->nome,item->nome,tmp);
      if(addon && total) { e->tipo=SV_TEXTO; snprintf(e->val,sizeof e->val,i18n("%d resultados"),total); }
      else if(!addon) { e->tipo=SV_SELO; e->on=item->ativo; snprintf(e->val,sizeof e->val,"%s",i18n(item->ativo?"Ativado":"Desligado")); }
    }
    int p2p=ajustes_p2p_ligado();
    SecLinha *e=secAdd(row,i18n("Serviços"),"aj_layers",NULL,i18n("P2P / torrents"),i18n(p2p?"Política local Nuvio: ligado":"Política local Nuvio: desligado"));
    e->tipo=SV_SELO; e->on=p2p; snprintf(e->val,sizeof e->val,"%s",i18n(p2p?"Ligado":"Desligado"));
    e=secAdd(row+1,i18n("Serviços"),"aj_info",NULL,i18n("Usenet"),i18n("Compatível · Não configurado"));
    e->tipo=SV_SETA;
    int ids[]={row+2,row+3}; const char *nomes[]={i18n("Inspecionar título"),i18n("Serviços no celular")};
    float fim=secBotoes(ids,nomes,2);
    secLista(fim);
    if(!snap.addons) bloco(i18n("Conecte a configuração para ver addons e serviços. O manifesto não revela suas chaves."),secL+40,fim-130,secLW-80,3);
    secTopo(serv?serv:i18n("Serviços"),i18n(serv?"Conectado":"Não configurado"),serv?aioOk:NULL,0,i18n("Debrid por torrent usa HTTPS. A regra local não altera restrições da instância."));
    char n[16];
    snprintf(n,sizeof n,"%d",emCache); secFato(i18n("Em cache"),n,NULL);
    snprintf(n,sizeof n,"%d",semCache); secFato(i18n("Sem cache"),n,NULL);
    snprintf(n,sizeof n,"%d",httpDireto); secFato(i18n("HTTP direto"),n,NULL);
    secBotoesFatos(nLinhas()-2,nLinhas()-1);
  } else if(pagina==LINKS) {
    int row=0;
    for(int i=0;i<snap.resultados;i++) if(strcmp(snap.resultado[i].tipo,"HTTP") && strcmp(snap.resultado[i].tipo,"P2P") && strcmp(snap.resultado[i].tipo,"Desconhecido")) {
      SecLinha *e=secAdd(row++,NULL,"aj_smartphone",NULL,snap.resultado[i].nome[0]?snap.resultado[i].nome:i18n(snap.resultado[i].tipo),i18n("Abrir no celular"));
      e->tipo=SV_SETA;
    }
    SecLinha *e=secAdd(snap.links+1,NULL,"aj_link",NULL,i18n("Link de instalação"),i18n("Links pessoais permanecem ocultos. Exporte ou importe arquivos JSON pelo celular."));
    e->tipo=SV_TEXTO; snprintf(e->val,sizeof e->val,"%s",i18n("Reconectar"));
    int ids[]={snap.links,snap.links+2}; const char *nomes[]={i18n("Inspecionar título"),i18n("Fontes e serviços")};
    secLista(secBotoes(ids,nomes,2));
    secTopo(i18n("No celular"),NULL,NULL,0,i18n("Links externos, YouTube e NZB não são vídeos HTTP reproduzíveis. Abra e gerencie pelo celular."));
    if(linkSelecionado>=0) {
      qrDesenhar(celular_url(),secFX+28,secFY,176);
      texto(TXT_ILHA_HORA,i18n("Credenciais ocultas"),secFX+28,secFY+190,384,142);
    }
    secBotoesFatos(nLinhas()-2,nLinhas()-1);
  } else if(pagina==LEGENDAS) {
    int row=0;
    for(int i=0;i<snap.addons;i++) if(snap.addon[i].legenda) {
      SecLinha *e=secAdd(++row,i18n("Addons"),NULL,snap.addon[i].nome,snap.addon[i].nome,i18n("Legendas"));
      e->tipo=SV_SELO; e->on=snap.addon[i].ativo; snprintf(e->val,sizeof e->val,"%s",i18n(e->on?"Ativado":"Desligado"));
    }
    for(int i=0;i<snap.legendas;i++) {
      snprintf(tmp,sizeof tmp,"%s · %s: %s · SDH: %s",snap.legenda[i].idioma,i18n("Forçada"),snap.legenda[i].forcada<0?i18n("Não informado"):snap.legenda[i].forcada?i18n("Sim"):i18n("Não"),snap.legenda[i].sdh<0?i18n("Não informado"):snap.legenda[i].sdh?i18n("Sim"):i18n("Não"));
      secAdd(nLegendas()+i+1,i18n("Legendas"),"aj_languages",NULL,i18n(ling_nome(snap.legenda[i].idioma)),tmp);
    }
    int ids[]={0,nConteudo()-1}; const char *nomes[]={i18n("Inspecionar título"),i18n("Gerenciar no celular")};
    secLista(secBotoes(ids,nomes,2));
    secTopo(i18n("Legendas"),NULL,NULL,0,i18n("Addons de legenda e faixas embutidas são diferentes. Resultados variam por título, arquivo e hash do vídeo."));
    secFato(i18n("Principal"),ling_legenda()[0]?i18n(ling_nome(ling_legenda())):i18n("Não informado"),NULL);
    secFato(i18n("Secundária"),ling_legenda2()[0]?i18n(ling_nome(ling_legenda2())):i18n("Não informado"),NULL);
    secBotoesFatos(nLinhas()-2,nLinhas()-1);
  } else if(pagina==FILTROS && regraSelecionada>=0) {
    int row=0;
    for(int i=0;i<snap.detalhes;i++) if(regraPertence(&snap.detalhe[i])) {
      const AioRegra *r=&snap.detalhe[i];
      secAdd(row++,i18n(gruposNome[regraSelecionada]),NULL,NULL,regraNome(r),!strcmp(r->valor,"hidden")?i18n("Oculto"):valorRegra(r->valor));
    }
    int ids[]={row}; const char *nomes[]={i18n("Editar no celular")};
    float fim=secBotoes(ids,nomes,1);
    secLista(fim);
    if(!row) bloco(i18n("Não informado"),secL+40,300,secLW-80,2);
    snprintf(tmp,sizeof tmp,"%d",row);
    secTopo(i18n(gruposNome[regraSelecionada]),tmp,NULL,1,i18n("Resumo somente leitura · Edite pelo celular"));
    if(snap.detalhesOmitidos) { snprintf(tmp,sizeof tmp,i18n("Ocultos · %d"),snap.detalhesOmitidos); secTexto(tmp,secFY,1); }
    secBotoesFatos(nLinhas()-2,nLinhas()-1);
  } else if(pagina==FILTROS) {
    static const char *const ic[]={"aj_monitor","aj_gauge","aj_languages","aj_database","aj_users","aj_type","aj_list-ordered","aj_file-text","aj_network","aj_settings-2"};
    for(int i=0;i<10;i++) {
      int saved=regraSelecionada,count=0; regraSelecionada=i;
      for(int k=0;k<snap.detalhes;k++) if(regraPertence(&snap.detalhe[k])) count++;
      regraSelecionada=saved;
      snprintf(tmp,sizeof tmp,i18n("%d campos informados · Somente leitura"),count);
      SecLinha *e=secAdd(i,NULL,ic[i],NULL,i18n(gruposNome[i]),snap.configurada && !soManifesto?tmp:i18n("Conecte a configuração para ver esta regra"));
      e->tipo=SV_SETA;
    }
    secLista(1008);
    snprintf(tmp,sizeof tmp,"%d",snap.detalhes);
    secTopo(i18n("Regras"),tmp,NULL,1,i18n("Regras avançadas ficam no AIOStreams. Salve lá e atualize esta TV."));
    char n[16];
    snprintf(n,sizeof n,"%d",dashboardTotal(2)); secFato(i18n("Filtros"),n,NULL);
    snprintf(n,sizeof n,"%d",dashboardTotal(3)); secFato(i18n("Ordenação"),n,NULL);
    if(snap.detalhesOmitidos) { snprintf(tmp,sizeof tmp,i18n("Ocultos · %d"),snap.detalhesOmitidos); secTexto(tmp,secFY+10,1); }
    secBotoesFatos(10,11);
  } else if(pagina==SAUDE) {
    const char *names[]={i18n("Manifesto"),i18n("Status da instância"),i18n("Configuração"),i18n("Consulta de fontes"),i18n("Legendas"),i18n("Inspeção do título")};
    int mx=1, falhas=0;
    for(int i=0;i<6;i++) { if(snap.ms[i]>mx) mx=snap.ms[i]; falhas+=snap.http[i]>=400; }
    for(int i=0;i<6;i++) {
      if(snap.http[i]) snprintf(tmp,sizeof tmp,"HTTP %d",snap.http[i]); else snprintf(tmp,sizeof tmp,"%s",i18n("Não informado"));
      SecLinha *e=secAdd(-1,NULL,NULL,NULL,names[i],tmp);
      int ok=snap.http[i]>=200 && snap.http[i]<400;
      e->ponto=!snap.http[i]?3:ok?1:2; e->tipo=SV_BARRA; e->on=ok;
      e->frac=snap.http[i]?anim_clamp((float)snap.ms[i]/mx,0,1):0;
      if(snap.http[i]) snprintf(e->val,sizeof e->val,"%d ms",snap.ms[i]);
    }
    secLista(1008);
    int estado=!snap.valida?2:falhas?1:0;
    const float *cor=estado==2?aioErr:estado==1?aioWarn:aioOk;
    secTopo(i18n("Instância"),i18n(estado==2?"Indisponível":estado==1?"Com falhas":"Saudável"),cor,0,i18n("Cota restante não informada. Nenhuma sondagem de reprodução. Em HTTP 429, aguarde antes de atualizar."));
    snprintf(tmp,sizeof tmp,"%s%s%s",snap.versaoInstancia[0]?snap.versaoInstancia:i18n("Não informado"),snap.canal[0]?" · ":"",snap.canal);
    secFato(i18n("Versão"),tmp,NULL);
    char n[16];
    if(snap.estatisticas<0) snprintf(n,sizeof n,"—"); else snprintf(n,sizeof n,"%d",snap.estatisticas);
    secFato(i18n("Estatísticas"),n,NULL);
    if(snap.erros<0) snprintf(n,sizeof n,"—"); else snprintf(n,sizeof n,"%d",snap.erros);
    secFato(i18n("Erros"),n,snap.erros>0?aioErr:NULL);
    secBotoesFatos(1,0);
  }
}
static void workspace(void) {
  if(pagina==PAINEL) {
    dashboard();
    if(lateral) navigationCount=0;
    sidebar();
    return;
  }
  if(pagina==TITULO) { tituloDesenhar(); return; }
  secoes();
  float L=secL;
  texto(TXT_ILHA_TITULO,i18n(titulos[pagina]),L,146,1848-L,243);
  const char *intro=pagina==CATALOGOS?"Catálogos publicados pelo manifesto":pagina==FONTES?"Addons, serviços, cache e P2P":pagina==LINKS?"Destinos externos e instalação":pagina==LEGENDAS?"Separadas das faixas dentro do vídeo":pagina==FILTROS?"Resumo somente leitura · Edite pelo celular":"Instância, latência e erros observados";
  txtCor(TXT_ILHA_SUB_L,i18n(intro),L,204,1848-L,aioTexto,.68f,0);
  sidebar();
}
int aioui_addons_mover(int f,int dx,int dy) {
  if(f<addons_n() && dy && (dy<0?f>0:f+1<addons_n())) return f+dy;
  for(int i=0;i<navigationCount;i++)if(navigationIds[i]==f) {
    int next=focus_rect_nearest(navigationRects,navigationCount,i,dx,dy);
    if(next!=i)return navigationIds[next];
  }
  // Rows outside the rendered window still page through the same list.
  if(f<addons_n() && dy)return dy<0?(f>0?f-1:f):f+1;
  return f;
}
static void addonPointer(int i,int b) { (void)b; addonsui_ponteiro(i,0); }
static void addonClick(int i,int b) {
  addonPointer(i,b); SDL_Event e={0}; e.type=SDL_KEYDOWN; e.key.keysym.sym=SDLK_RETURN; addonsui_evento(&e);
}
static void addonsPainel(float x,float y,float w,float h) { painel(x,y,w,h); }
static void addonsDesenhar(int f);
void aioui_addons_desenhar(int f) {
  navigationCount=0;
  int semAro=gfx_vidro_sem_aro; gfx_vidro_sem_aro=1;
  addonsDesenhar(f);
  gfx_vidro_sem_aro=semAro;
}
static void addonsDesenhar(int f) {
  Uint32 now=SDL_GetTicks(); float dt=addonsTick?fminf((now-addonsTick)/1000.f,.05f):0;
  addonsTick=now;
  for(int i=0;i<168;i++) addonsFoco[i]=anim_mola(addonsFoco[i],i==f,dt,NV_MOLA_FOCO);
  addonsScroll=anim_mola2(&addonsVel,addonsScroll,fmaxf(0,f-4),dt,NV_MOLA2_SCROLL);
  gfx_cor((GfxRect){0,0,1920,1080},0,.035f,.065f,.077f,1);
  GLuint fundo=tex_obter_hero(arteFundo); if(fundo) gfx_textura((GfxRect){0,0,1920,1080},fundo);
  addonsPainel(72,148,315,824); addonsPainel(411,148,1437,824);
  texto(TXT_ILHA_SUB,i18n("Ajustes / Addons / AIOStreams"),150,65,1400,196);
  texto(TXT_ILHA_SECAO,i18n("Ajustes"),111,190,240,180);
  texto(TXT_ILHA_NOME,i18n("Addons"),111,395,240,240);
  texto(TXT_ILHA_HORA,i18n("Ligados nesta TV"),453,199,1200,174);
  texto(TXT_ILHA_TITULO,i18n("Addons"),453,230,1200,240);
  texto(TXT_ILHA_SUB,i18n("Ligados nesta TV"),453,290,1200,170);
  int n=addons_n();
  float scroll=anim_clamp(addonsScroll,0,fmaxf(0,n-5)); int start=(int)scroll;
  for(int i=start;i<n && i<start+5;i++) {
    float y=360+(i-scroll)*105;
    plrui_linha_foco((GfxRect){453,y,1340,94},22,addonsFoco[i]);
    if(aioui_detectar(i)) {
      GLuint logo=tex_obter(arteLogo); if(logo) gfx_rect((GfxRect){477,y+24,43,43},logo,GFX_MARCA,0,0,0,0,1,1,1,1);
    } else gfx_icone((GfxRect){477,y+28,28,28},"aj_puzzle",.85f,.88f,.9f,1);
    texto(TXT_ILHA_NOME_L,addons_nome(i),535,y+17,1190,242);
    texto(TXT_ILHA_SUB_L,aioui_detectar(i)?i18n("Abrir painel"):addons_ativo(i)?i18n("Ativado"):i18n("Desativado"),535,y+53,1190,174);
    ponteiro_alvo(453,y,1340,94,addonPointer,addonClick,i,0);
  }
  GfxRect r={453,866,350,56};
  botao_pilula(r,sync_manual_addons_rodando()?i18n("Sincronizando…"):i18n("Sincronizar addons"),NULL,addonsFoco[n],0,0,1);
  ponteiro_alvo(r.x,r.y,r.w,r.h,addonPointer,addonClick,n,0);
  r.x=820; r.w=350;
  botao_pilula(r,i18n("TV ao vivo"),NULL,addonsFoco[n+1],0,0,1);
  ponteiro_alvo(r.x,r.y,r.w,r.h,addonPointer,addonClick,n+1,0);
  r.x=1187; r.w=606;
  botao_pilula(r,i18n("Conectar ou importar sua configuração"),NULL,addonsFoco[n+2],0,0,1);
  ponteiro_alvo(r.x,r.y,r.w,r.h,addonPointer,addonClick,n+2,0);

}
static void connectClick(int i,int unused) {
  (void)unused; connectFocus=i;
  SDL_Event e={0}; e.type=SDL_KEYDOWN; e.key.keysym.sym=SDLK_RETURN; aioui_evento(&e);
}
static void connectPointer(int i,int unused) { (void)unused; connectFocus=i; }
static void connectDraw(Uint32 agora) {
  gfx_cor((GfxRect){0,0,1920,1080},0,0,0,0,T21_VEIL);
  GfxRect folha={330,150,1260,780};
  plrui_material(folha,34,1,1);
  gfx_cor(folha,34/780.f,1,1,1,.035f);
  plrui_kicker("AIOStreams",382,196,243,242,239,.55f);
  texto(TXT_ILHA_TITULO,i18n("Conectar pelo celular"),382,226,1150,243);
  int synced=connectState==AIOPAIR_SYNCED;
  if(connectUrl[0] && !synced) qrDesenhar(connectUrl,382,326,400);
  else {
    gfx_cor((GfxRect){382,326,400,400},26/400.f,1,1,1,.06f);
    gfx_icone((GfxRect){542,486,80,80},synced?"aj_check":"aj_smartphone",.953f,.949f,.937f,.9f);
  }
  // Steps follow the real pairing states: scan, validate on the instance, deliver once.
  const char *titulos3[]={"Aponte a câmera para o código","Entre com UUID e senha","A TV sincroniza sozinha"};
  const char *subs3[]={"O celular abre a página de conexão, em HTTPS.","O celular confere direto na sua instância.","Entrega única, em caixa temporária de 5 minutos."};
  int atual=synced?3:connectState==AIOPAIR_CHECKING?1:0;
  float ar,ag,ab, ink=ajustes_acento_tinta(&ar,&ag,&ab);
  for(int i=0;i<3;i++) {
    float y=330+i*86;
    if(i<atual) {
      gfx_cor((GfxRect){840,y,44,44},.5f,aioOk[0],aioOk[1],aioOk[2],.22f);
      gfx_icone((GfxRect){851,y+11,22,22},"aj_check",aioOk[0],aioOk[1],aioOk[2],1);
    } else {
      float c[3]={i==atual?ink:.953f,i==atual?ink:.949f,i==atual?ink:.937f};
      if(i==atual) gfx_cor((GfxRect){840,y,44,44},.5f,ar,ag,ab,1);
      else gfx_cor((GfxRect){840,y,44,44},.5f,1,1,1,.1f);
      char n[4]; snprintf(n,sizeof n,"%d",i+1);
      txtCor(TXT_G20B,n,862-largura(TXT_G20B,n)/2,y+10,30,c,1,0);
    }
    txtCor(TXT_G23B,i18n(titulos3[i]),904,y-2,640,aioTexto,1,0);
    txtCor(TXT_ILHA_SUB_L,i18n(subs3[i]),904,y+30,640,aioTexto,.68f,0);
  }
  const char *description=synced?"Sincronizado":
    connectState==AIOPAIR_CHECKING?"Validando sua instância…":
    connectState==AIOPAIR_ERROR?"Acesso recusado. Confira UUID e senha.":
    connectState==AIOPAIR_EXPIRED?"Pareamento expirado. Gere outro QR.":
    connectState==AIOPAIR_UNAVAILABLE?"Pareamento indisponível. Tente novamente ou use a TV.":
    connectState==AIOPAIR_STARTING?"Preparando conexão segura…":"Aguardando o celular";
  int falhou=connectState==AIOPAIR_ERROR || connectState==AIOPAIR_EXPIRED || connectState==AIOPAIR_UNAVAILABLE;
  const float *cor=falhou?aioErr:synced?aioOk:NULL;
  float pulso=.55f+.45f*sinf(agora/260.f);
  if(connectState==AIOPAIR_STARTING || connectState==AIOPAIR_CHECKING) plrui_anel(851,602,22,1,agora,1);
  else gfx_cor((GfxRect){845,596,12,12},.5f,cor?cor[0]:ar,cor?cor[1]:ag,cor?cor[2]:ab,falhou||synced?1:pulso);
  txtCor(TXT_G20M,i18n(description),872,588,660,cor?cor:aioTexto,1,0);
  if(connectUrl[0] && !synced) {
    txtCor(TXT_ILHA_HORA_L,i18n("Ou abra este endereço no celular"),840,648,700,aioTexto,.6f,0);
    txtCor(TXT_ILHA_SUB_L,connectUrl,840,676,700,aioTexto,.82f,0);
  }
  const char *labels[]={i18n(synced?"Concluir":"Gerar novo QR"),i18n("Digitar na TV")};
  const char *ic[]={synced?"aj_check":"aj_rotate-cw","aj_keyboard"};
  float x=382, scale=gfx_escala_txt();
  for(int i=0;i<2;i++) {
    float w=botao_largura(labels[i],ic[i],0);
    GfxRect rect={x/scale,830/scale,w,T21_CTRL_H};
    TXT_INI(); botao_pilula(rect,labels[i],ic[i],connectFocus==i,0,0,1); TXT_FIM();
    ponteiro_alvo(x,830,w*scale,T21_CTRL_H*scale,connectPointer,connectClick,i,0);
    x+=w*scale+16;
  }
}
static void desenhar(Uint32 agora);
void aioui_desenhar(Uint32 agora) {
  navigationCount=0;
  int semAro=gfx_vidro_sem_aro; gfx_vidro_sem_aro=1;
  desenhar(agora);
  gfx_vidro_sem_aro=semAro;
}
static void desenhar(Uint32 agora) {
  if(!aberto) return;
  float alpha=gfx_opacidade_grupo;
  float entry=anim_suave(abertura);
  gfx_opacidade_grupo=alpha*entry;
  gfx_cor((GfxRect){0,0,1920,1080},0,.035f,.065f,.077f,1);
  GLuint fundo=tex_obter_hero(arteFundo);
  if(fundo) gfx_textura((GfxRect){0,0,1920,1080},fundo);
  texto(TXT_ILHA_SUB,i18n("Ajustes / Addons / AIOStreams"),150,65,1400,196);
  time_t now=time(NULL); struct tm clock; localtime_r(&now,&clock); char hhmm[16]; strftime(hhmm,sizeof hhmm,"%H:%M",&clock);
  painel(1710,46,138,56); texto(TXT_ILHA_NOME,hhmm,1734,61,100,235);
  pthread_mutex_lock(&trava); int busy=ocupado; pthread_mutex_unlock(&trava);
  if(pendente || busy) {
    plrui_anel(1564,74,28,1,agora,1);
    texto(TXT_ILHA_SUB,i18n("Conferindo…"),1590,65,120,180);
  }
  painel(72,46,56,56);
  gfx_icone((GfxRect){87,62,24,24},"pl_chevron-left",.85f,.88f,.9f,1);
  ponteiro_alvo(72,46,56,56,NULL,voltarClick,0,0);
  gfx_opacidade_grupo=alpha*entry*anim_suave(corpo);
  deslocamento=(1-entry)*NV_ENTRA_DY;
  if(deslocamento) gfx_transformar(0,0,1,0,deslocamento);
  int destino=pagina, regra=regraSelecionada, evidencia=evidenciaSelecionada;
  pagina=paginaAnterior<0?pagina:paginaAnterior;
  regraSelecionada=regraVisivel; evidenciaSelecionada=evidenciaVisivel;
  if(pagina<PAINEL) setup(); else workspace();
  pagina=destino; regraSelecionada=regra; evidenciaSelecionada=evidencia;
  if(deslocamento) gfx_sem_transformar();
  deslocamento=0;
  gfx_opacidade_grupo=alpha*entry;

  if(mensagem[0]) texto(TXT_ILHA_SUB,mensagem,453,934,1340,215);
  gfx_opacidade_grupo=alpha;
  if(connectOpen) { navigationCount=0; connectDraw(agora); }
  if(teclado_aberto()) teclado_desenhar(agora);
}
#ifdef NV_SHOT_HOOKS
void aioui_shot_connect(int state) {
  connectOpen=connectShot=1; connectState=state; connectFocus=0;
  snprintf(connectUrl,sizeof connectUrl,"https://pair.example.invalid/aio/#2222222222224222a222222222222222");
}
void aioui_shot(int p,const AioSnapshot *s) {
  abertura=0; saindo=0; paginaAnterior=-1; historyCount=0; navigationCount=0;
  sidebarOpen=0; dashboardReturn=connectOpen=connectShot=0;
  perfil=perfis_ativo(); snprintf(usuario,sizeof usuario,"%s",sessao_usuario());
  indice=-1; pendente=0;
  memset(dashboardLinha,0,sizeof dashboardLinha); memset(dashboardScroll,0,sizeof dashboardScroll); memset(dashboardVel,0,sizeof dashboardVel);
  ultimaAtualizacao=time(NULL);
  aberto=1; pagina=p; snap=*s; foco=p==TITULO?2:0; lateral=0; regraSelecionada=linkSelecionado=-1; evidenciaSelecionada=0;
  snprintf(host,sizeof host,"%s",hosts[0]); qrAtual();
}
#endif
