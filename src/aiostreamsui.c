#include "aiostreamsui.h"
#include "addons.h"
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
#include <time.h>

enum { SEM, INSTANCIA, IMPORTAR, SERVICOS, PRONTO, PAINEL, CATALOGOS, FONTES, LINKS, LEGENDAS, FILTROS, SAUDE, TITULO };
static const char *const titulos[] = {"Conectar", "Escolha uma instância", "Importar configuração", "Serviços e preferências", "Pronto", "Tudo que o AIOStreams traz", "Catálogos", "Fontes e serviços", "Links", "Legendas", "Filtros e ordenação", "Saúde", "Inspeção do título"};
static const char *const icons[] = {"aj_layout-dashboard","aj_library","aj_layers","aj_link","aj_captions","aj_sliders-horizontal","aj_activity"};
static const char *const hosts[] = {"https://aiostreams.fortheweak.cloud","https://aiostreamsfortheweebsstable.midnightignite.me","https://aiostreams.elfhosted.com","https://aiostreams.viren070.me","https://aiostreams.stremio.ru","https://aio.atbphosting.com","https://aiostreams.12312023.xyz"};
static const char *const nomes[] = {"Yeb’s", "Midnight’s", "ElfHosted", "Viren’s", "Kuu’s", "ATBP Hosting", "Omni’s"};
static int aberto, pagina, foco, lateral, hostIdx, indice = -1, entrada, pendente, perfil;
static Uint32 proximaConsulta;
static int saindo, paginaAnterior=-1;
static float abertura, corpo=1, entradaTempo, scrollFoco, scrollVel;
static float focoAnim[2][AIO_MAX+40];
static float listaPos, deslocamento, addonsFoco[168], addonsScroll, addonsVel;
static Uint32 addonsTick;
static float janela(int manter,int total,int visiveis,int ativo) {
  return ativo ? anim_clamp(scrollFoco-manter,0,fmaxf(0,total-visiveis)) : 0;
}
static float focoDe(int i,int nav) {
  return i>=0 && i<AIO_MAX+40 ? focoAnim[nav!=0][i] : 0;
}
static void movimento(float dt) {
  abertura=t21_entrada(abertura,dt,!saindo);
  if(paginaAnterior!=pagina) {
    paginaAnterior=pagina; corpo=0; entradaTempo=0; scrollFoco=0; scrollVel=0;
    memset(focoAnim,0,sizeof focoAnim);
  }
  corpo=anim_rampa(corpo,1,dt,180);
  entradaTempo+=dt;
  scrollFoco=anim_mola2(&scrollVel,scrollFoco,lateral?0:foco,dt,NV_MOLA2_SCROLL);
  for(int nav=0;nav<2;nav++) for(int i=0;i<AIO_MAX+40;i++) {
    float target=!saindo && lateral==nav && foco==i;
    focoAnim[nav][i]=anim_mola(focoAnim[nav][i],target,dt,
      target>focoAnim[nav][i]?NV_MOLA_FOCO:NV_MOLA_DESFOCO);
  }
}

static unsigned versao, geracao;
static int catalogoSelecionado, regraSelecionada=-1, linkSelecionado=-1, evidenciaSelecionada, maisHosts;
static void trace(void);
static char arteFundo[700], arteLogo[700];
void aioui_arte(const char *dir) {
  snprintf(arteFundo,sizeof arteFundo,"%s/aiostreams/backdrop.jpg",dir);
  snprintf(arteLogo,sizeof arteLogo,"%s/aiostreams/logo.png",dir);
}
static char usuario[96], base[2048], host[256], alvo[128], tipo[32], mensagem[256];
static char configure[320];
static AioSnapshot snap;
static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
static int ocupado, entregue;
static unsigned entregaGeracao;
static AioSnapshot entrega;
static int entregaInstalar;
static char entregaBase[2048];
typedef struct { unsigned geracao; int instalar; char base[2048], host[256], id[128], tipo[32]; } Pedido;

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
  RedeResposta r = {0}; const char *h[] = {"User-Agent: AIO-NuvioLegacy",auth,NULL};
  RedePedido p = {.url=u,.cabecalhos=h,.prazo_ms=12000,.max_bytes=1024*1024,.seguir=0};
  rede_pedir(&p,&r); s->http[k]=r.status; s->ms[k]=(int)r.ms; s->retry[k]=r.retry_after_s;
  if(r.erro==REDE_OK && r.status>=200 && r.status<300) { *corpo=r.corpo; r.corpo=NULL; }
  rede_resposta_limpar(&r);
}
static void *worker(void *arg) {
  Pedido *p=arg; AioSnapshot *s=calloc(1,sizeof *s); char *body=NULL, url[3072], auth[4096];
  if(s) {
    snprintf(s->host,sizeof s->host,"%s",p->host);
    buscar(s,p->base,NULL,0,&body);
    if(body) { aio_manifesto(s,body); free(body); body=NULL; }
    snprintf(url,sizeof url,"%s/api/v1/status",p->host); buscar(s,url,NULL,1,&body);
    // Status version is not evidence of authenticated configuration access.
    if(body) { aio_status(s,body); free(body); body=NULL; }
    if(s->valida && aio_autorizacao(p->base,auth,sizeof auth) && !strncmp(p->host,"https://",8)) {
      snprintf(url,sizeof url,"%s/api/v1/user",p->host); buscar(s,url,auth,2,&body);
      if(body) { aio_configuracao(s,body); memset(body,0,strlen(body)); free(body); body=NULL; }
      if(p->id[0] && s->searchAtiva) {
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
  saindo=1; geracao++; pendente=0; celular_fechar();
}
static void encerrar(void) {
  aberto=0;
  memset(&snap,0,sizeof snap); memset(base,0,sizeof base); alvo[0]=mensagem[0]=0;
}
void aioui_abrir(int i) {
  abertura=0; saindo=0; paginaAnterior=-1;
  aberto=1; geracao++; foco=0; lateral=0; entrada=pendente=0; indice=i; alvo[0]=0; proximaConsulta=0;
  regraSelecionada=linkSelecionado=-1; evidenciaSelecionada=0;
  memset(&snap,0,sizeof snap); mensagem[0]=0;
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
static void mudar(int p) { pagina=p; foco=0; lateral=0; mensagem[0]=0; regraSelecionada=-1; linkSelecionado=-1; evidenciaSelecionada=0; celular_fechar(); }
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
  if(pagina==SERVICOS) return 2;
  if(pagina==IMPORTAR) return 2;
  if(pagina==PRONTO) return 2;
  if(pagina==PAINEL) return 9;
  if(pagina==CATALOGOS) return snap.catalogos+4;
  if(pagina==FONTES) return snap.addons-nLegendas()+snap.servicos+4;
  if(pagina==LEGENDAS) return nLegendas()+snap.legendas+2;
  if(pagina==LINKS) return snap.links+3;
  if(pagina==FILTROS) {
    if(regraSelecionada>=0) { int n=1; for(int i=0;i<snap.detalhes;i++) if(snap.detalhe[i].grupo==regraSelecionada) n++; return n; }
    return 9;
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
static void acao(void) {
  if(lateral) { mudar(PAINEL+foco); lateral=1; foco=pagina-PAINEL; return; }
  if(pagina>=CATALOGOS && pagina<=SAUDE && foco>=nLinhas()-2) {
    if(foco==nLinhas()-1) { geracao++; atualizarRede(0); }
    else mudar(SERVICOS);
    return;
  }
  if(pagina==SEM) { int f=foco; mudar(f==2?IMPORTAR:INSTANCIA); if(f==1) campo(1); }
  else if(pagina==INSTANCIA) {
    if(foco<7) { hostIdx=foco; snprintf(host,sizeof host,"%s",hosts[hostIdx]); qrAtual(); }
    else if(foco==7) campo(1); else if(foco==8) mudar(IMPORTAR);
    else if(foco==10) { maisHosts=!maisHosts; foco=maisHosts?4:0; }
    else mudar(SERVICOS);
  } else if(pagina==SERVICOS) { if(foco==0) mudar(IMPORTAR); else mudar(INSTANCIA); }
  else if(pagina==IMPORTAR) {
    if(foco==0) {
      if(celular_disponivel() && celular_abrir(i18n("Link de instalação"))) entrada=3;
      else campo(3);
    } else campo(3);
  } else if(pagina==PRONTO) mudar(foco?CATALOGOS:PAINEL);
  else if(pagina==PAINEL) {
    if(foco<6) mudar(CATALOGOS+foco);
    else if(foco==6) { campo(2); }
    else if(foco==7) { geracao++; atualizarRede(0); }
    else mudar(IMPORTAR);
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
    if(regraSelecionada<0 && foco<8) { regraSelecionada=foco; foco=0; }
    else mudar(SERVICOS);
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
void aioui_evento(const SDL_Event *e) {
  if(!aberto || saindo) return;
  if(teclado_aberto()) { teclado_evento(e); return; }
  if(e->type!=SDL_KEYDOWN) return;
  SDL_Keycode k=e->key.keysym.sym;
  if(nv_tecla_voltar(e)) {
    if(entrada==3 || linkSelecionado>=0) { celular_fechar(); entrada=0; linkSelecionado=-1; return; }
    if(pagina==FILTROS && regraSelecionada>=0) { foco=regraSelecionada; regraSelecionada=-1; return; }
    if(pagina==SEM || pagina==PAINEL) fechar();
    else mudar(snap.valida?PAINEL:SEM);
    return;
  }
  if(pagina==CATALOGOS && !lateral && foco<snap.catalogos && (k==SDLK_PAGEUP || k==SDLK_PAGEDOWN)) {
    int f=fileira(foco);
    if(f>=0) { fil_mover(f,k==SDLK_PAGEUP?-1:1); desc_remontar_fileiras(); }
    return;
  }
  if(pagina==PAINEL && !lateral && foco<6 && (k==SDLK_UP || k==SDLK_DOWN)) {
    if(k==SDLK_UP && foco>=2) foco-=2;
    else if(k==SDLK_DOWN) foco=foco<4?foco+2:6;
    return;
  }
  if(pagina==PAINEL && !lateral && foco<6 && (k==SDLK_LEFT || k==SDLK_RIGHT)) {
    if(k==SDLK_RIGHT && !(foco%2)) foco++;
    else if(k==SDLK_LEFT && foco%2) foco--;
    else if(k==SDLK_LEFT) { lateral=1; foco=0; }
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
  else if(k==SDLK_RETURN || k==SDLK_KP_ENTER) acao();
}
static void receber(const char *texto) {
  char origem[256];
  if(entrada==1) {
    if(aio_url(texto,origem,sizeof origem) && !strcmp(texto,origem)) {
      snprintf(host,sizeof host,"%s",origem); qrAtual(); mudar(SERVICOS);
    } else snprintf(mensagem,sizeof mensagem,"%s",i18n("URL inválida. Use apenas a origem HTTP ou HTTPS, sem credenciais."));
  } else if(entrada==2) {
    if(!texto[0] || strspn(texto,"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789:_-.")!=strlen(texto))
      snprintf(mensagem,sizeof mensagem,"%s",i18n("ID do título inválido."));
    else { snprintf(alvo,sizeof alvo,"%s",texto); strcpy(tipo,strchr(texto,':')?"series":"movie"); mudar(TITULO); geracao++; atualizarRede(0); }
  } else if(aio_url(texto,origem,sizeof origem) && !strchr(texto,'#') &&
      strstr(texto,"/manifest.json") &&
      (strstr(texto,"/manifest.json")[14]==0 || strstr(texto,"/manifest.json")[14]=='?') && strlen(texto)<sizeof base) {
    snprintf(base,sizeof base,"%s",texto); snprintf(host,sizeof host,"%s",origem); qrAtual(); geracao++; atualizarRede(1);
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
    else if(r==TECLADO_CANCELOU) entrada=0;
  }
  if(entrada==3) { char texto[2048]; if(celular_pegar(texto,sizeof texto)) { receber(texto); memset(texto,0,sizeof texto); } }
  pthread_mutex_lock(&trava);
  if(entregue) {
    if(entregaGeracao==geracao) {
      snap=entrega; trace();
      for(int k=0;k<6;k++) if(snap.retry[k]>0) {
        unsigned wait=(unsigned)(snap.retry[k]>3600?3600:snap.retry[k]);
        proximaConsulta=agora+wait*1000u;
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
  TXT_INI();
  TxtLinha t=txt_linha_corta(e,s,cor,cor,cor,255,w/escala);
  txt_desenhar(t,x/escala,y/escala);
  TXT_FIM();
}
static void bloco(const char *s,float x,float y,float w,int linhas) {
  float scale=gfx_escala_txt();
  TXT_INI();
  txt_bloco_corta(TXT_ILHA_SUB,s,170,179,185,x/scale,y/scale,w/scale,28,1,linhas);
  TXT_FIM();
}
static void painel(float x,float y,float w,float h) {
  GfxRect r={x,y,w,h};
  if(w<h)plrui_material(r,T21_R_MODAL,0,1);
  else t21_ilha(r,T21_R_MODAL,1);
}
static void pointer(int i,int b) { lateral=b; foco=i; }
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
static void linha(int i,const char *icon,const char *nome,const char *sub,float x,float y,float w) {
  float h=pagina==SEM?145:94, ty=pagina==SEM?38:17;
  if(y+h<260 || y>920) return;
  float alpha=gfx_opacidade_grupo;
  float edge=anim_clamp(fminf((y+h-260)/24,(920-y)/24),0,1);
  float entry=anim_saida(anim_atraso(entradaTempo,i,38,300));
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
static void botao(int i,const char *nome,float x,float y,float w) {
  float scale=gfx_escala_txt();
  GfxRect r={x/scale,y/scale,fmaxf(w/scale,botao_largura(nome,NULL,0)),T21_CTRL_H};
  TXT_INI();
  botao_pilula(r,nome,NULL,focoDe(i,0),0,0,1);
  TXT_FIM();
  ponteiro_alvo(x,y,r.w*scale,r.h*scale,pointer,click,i,0);
}
static void qrDesenhar(const char *url,float x,float y,float size) {
  // Shared QR texture: one draw instead of hundreds of modules each frame.
  float scale=size/celb_qr_lado();
  gfx_transformar(0,0,scale,x,y);
  celb_qr_desenhar(url,0,0,1);
  gfx_sem_transformar();
  if(deslocamento) gfx_transformar(0,0,1,0,deslocamento);
}
static void aside(void) {
  gfx_cor((GfxRect){1466,285,1,545},0,.8f,.84f,.88f,.08f);
  plrui_kicker(i18n(titulos[pagina]),1497,289,243,242,239,.65f);
  txt_bloco_corta(TXT_ILHA_SECAO,i18n(pagina==CATALOGOS?"Gerenciar no celular":pagina==FONTES?"Serviços no celular":pagina==LEGENDAS?"Gerenciar legendas no celular":pagina==SAUDE?"Status da instância":"Edite pelo celular"),238,238,238,1497,320,302,28,1,2);
  bloco(i18n("Regras avançadas ficam no AIOStreams. Salve lá e atualize esta TV."),1497,378,290,3);
  qrDesenhar(configure,1497,483,152);
  bloco(i18n("Página oficial de configuração. Entre com sua própria conta."),1497,650,290,3);
  botao(nLinhas()-2,i18n("Configuração no celular"),1497,708,308);
  botao(nLinhas()-1,i18n("Atualizar"),1497,776,308);
  texto(TXT_ILHA_HORA,i18n("Credenciais ocultas"),1497,877,290,142);
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
  float storyHeight=txt_bloco(TXT_V2_A3TIT,i18n(story[pagina]),240,241,242,124,345,490,56,1,4);
  bloco(i18n("Reúna addons, catálogos e legendas. Configure os detalhes no celular e acompanhe tudo nesta TV."),124,345+storyHeight+34,485,5);
  const char *steps[]={i18n("Escolha sua instância"),i18n("Conecte e configure"),i18n("Valide nesta TV")};
  for(int i=0;i<3;i++) {
    int done=i<step;
    gfx_cor((GfxRect){124,800+i*49,30,30},.5f,.35f,.45f,.47f,i==step?.6f:.22f);
    if(done) gfx_icone((GfxRect){129,805+i*49,20,20},"aj_check",.7f,.76f,.79f,1);
    else { char number[8]; snprintf(number,sizeof number,"%d",i+1); texto(TXT_ILHA_NUM,number,134,806+i*49,20,200); }
    texto(TXT_ILHA_SUB,steps[i],169,800+i*49,450,195);
  }
  if(pagina!=PRONTO) texto(TXT_ILHA_PERGUNTA,pagina==SEM?i18n("Como deseja conectar?"):i18n(titulos[pagina]),716,204,1080,240);
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
    for(int i=0;i<4;i++) {
      float y=263+i*88;
      gfx_cor((GfxRect){738,y+16,52,52},.3f,.21f,.225f,.24f,.45f);
      gfx_icone((GfxRect){738,y+26,28,28},ico[i],.8f,.85f,.87f,1);
      texto(TXT_ILHA_NOME,check[i],810,y+13,920,238);
      texto(TXT_ILHA_SUB,sub[i],810,y+49,920,174);
    }
    qrDesenhar(configure,716,640,141); bloco(i18n("Salvar e instalar no AIOStreams. Depois copie o link do manifesto."),882,666,820,4);
    botao(0,i18n("Tenho meu link de instalação"),716,808,480); botao(1,i18n("Instância"),1216,808,300);
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
    ponteiro_alvo(716,655,1078,66,pointer,click,1,0);
    botao(0,i18n("Enviar pelo celular"),716,842,380); botao(1,i18n("Colar link"),1116,842,350);
  } else {
    gfx_cor((GfxRect){716,195,84,84},.5f,.25f,.32f,.34f,.55f);
    gfx_icone((GfxRect){736,215,44,44},"aj_check",.65f,.84f,.83f,1);
    texto(TXT_ILHA_PERGUNTA,i18n("AIOStreams adicionado"),716,307,1040,240);
    texto(TXT_ILHA_SUB,host,716,361,1040,174);
    const char *checks[]={i18n("Manifesto"),snap.configurada?i18n("Configuração conectada"):i18n("Somente manifesto"),i18n("Credenciais ocultas")};
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
  static char result[768]; result[0]=0;
  char copy[256]; snprintf(copy,sizeof copy,"%s",value);
  char *save=NULL;
  for(char *p=strtok_r(copy,",",&save);p;p=strtok_r(NULL,",",&save)) {
    while(*p==' ') p++;
    const char *label=!strcmp(p,"true")?i18n("Sim"):!strcmp(p,"false")?i18n("Não"):!strcmp(p,"Portuguese")?i18n("Português"):!strcmp(p,"English")?i18n("Inglês"):!strcmp(p,"cached")?i18n("Em cache"):!strcmp(p,"uncached")?i18n("Fora do cache"):!strcmp(p,"resolution")?i18n("Resolução"):!strcmp(p,"size")?i18n("Tamanho"):!strcmp(p,"language")?i18n("Idiomas"):p;
    size_t n=strlen(result); snprintf(result+n,sizeof result-n,"%s%s",n?", ":"",label);
  }
  return result;
}
static const char *regraNome(const AioRegra *r) {
  static char name[192]; const char *s=r->nome, *mode="";
  const char *prefix[]={"required","included","excluded","preferred"};
  const char *labels[]={"Obrigatório","Incluído","Excluído","Preferido"};
  for(int i=0;i<4;i++) if(!strncmp(s,prefix[i],strlen(prefix[i]))) { mode=i18n(labels[i]); s+=strlen(prefix[i]); break; }
  const char *label=strstr(s,"resultLimits")?i18n("Resultados"):strstr(s,"Encodes")?i18n("Codec · HDR"):strstr(s,"AudioChannels")?i18n("Áudio"):strstr(s,"ReleaseGroups")?i18n("Grupo"):strstr(s,"Resolutions")?i18n("Resolução e qualidade"):strstr(s,"Languages")||strstr(s,"Subtitles")?i18n("Idiomas"):strstr(s,"Qualities")||strstr(s,"Tags")?i18n("Qualidade"):strstr(s,"StreamTypes")?i18n("Cache e tipos de fonte"):strstr(s,"Seeder")||strstr(s,"seeder")?i18n("Semeadores"):strstr(s,"size")?i18n("Tamanho"):strstr(s,"bitrate")?i18n("Bitrate"):strstr(s,"sort.")?i18n("Ordenação"):strstr(s,"proxy.")?i18n("Proxy"):strstr(s,"template.")?i18n("Modelo"):strstr(s,"formatter.")?i18n("Formatador"):r->grupo==4?i18n("Regex e palavras-chave"):i18n("Cache e tipos de fonte");
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
      AioEvidencia *e=&snap.evidencia[i]; float y=380+(i-listaPos)*98;
      plrui_linha_foco((GfxRect){108,y,1290,90},22,focoDe(i+2,0));
      gfx_icone((GfxRect){125,y+18,24,24},e->erro?"aj_info":"aj_activity",.7f,.8f,.82f,1);
      texto(TXT_ILHA_NOME,e->titulo,168,y+12,1200,239);
      texto(TXT_ILHA_SUB,e->descricao,168,y+48,1200,174);
      ponteiro_alvo(108,y,1290,90,pointer,click,i+2,0);
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
    float y=389+(i-listaPos)*60; AioResultado *r=&snap.resultado[i];
    plrui_linha_foco((GfxRect){108,y,1290,59},22,focoDe(i+2,0));
    char num[24]; snprintf(num,sizeof num,"%d",i+1); texto(TXT_ILHA_SUB,num,125,y+20,50,150);
    texto(TXT_ILHA_SECAO,r->nome[0]?r->nome:i18n("Fonte sem nome"),193,y+7,810,238);
    char sub[256]; snprintf(sub,sizeof sub,"%s · %s",i18n(r->tipo),r->cache<0?i18n("Cache desconhecido"):r->cache?i18n("Em cache"):i18n("Fora do cache"));
    texto(TXT_ILHA_NUM,sub,193,y+35,810,174);
    snprintf(num,sizeof num,r->rank?"%d":"—",r->rank); texto(TXT_ILHA_SUB,num,1034,y+20,120,222);
    texto(TXT_ILHA_NUM,decisao(r),1160,y+22,230,184);
    ponteiro_alvo(108,y,1290,60,pointer,click,i+2,0);
  }
  texto(TXT_ILHA_NUM,i18n("Ordem recebida; rejeições anteriores não são fornecidas pelo manifesto."),124,890,1270,150);
  int selected=foco>=2?foco-2:0;
  texto(TXT_ILHA_HORA,i18n("Decisão Nuvio"),1490,184,326,174);
  if(selected<snap.resultados) {
    AioResultado *r=&snap.resultado[selected];
    txt_bloco_corta(TXT_ILHA_NOME,r->nome,240,240,238,1490,217,326,34,1,3);
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
static void workspace(void) {
  if(pagina==TITULO) { tituloDesenhar(); return; }
  painel(72,148,315,824); painel(411,148,1437,824);
  texto(TXT_ILHA_NOME,"AIOStreams",160,181,200,240);
  texto(TXT_ILHA_NUM,i18n("Seu addon conectado"),160,218,200,163);
  GLuint logo=tex_obter(arteLogo); if(logo) gfx_rect((GfxRect){110,190,26,26},logo,GFX_MARCA,0,0,0,0,1,1,1,1);
  const char *nav[]={i18n("Visão geral"),i18n("Catálogos"),i18n("Fontes e serviços"),i18n("Links"),i18n("Legendas"),i18n("Filtros e ordenação"),i18n("Saúde")};
  for(int i=0;i<7;i++) {
    float y=265+i*70;
    plrui_linha_foco((GfxRect){91,y,277,64},20,fmaxf(focoDe(i,1),pagina==PAINEL+i?.35f:0));
    gfx_icone((GfxRect){111,y+19,25,25},icons[i],.77f,.82f,.84f,1);
    texto(TXT_ILHA_ITEM,nav[i],150,y+19,210,220);
    if(pagina==PAINEL+i) gfx_cor((GfxRect){97,y+30,5,5},.5f,.72f,.86f,.89f,1);
    ponteiro_alvo(91,y,277,64,pointer,click,i,1);
  }
  gfx_cor((GfxRect){91,830,277,1},0,1,1,1,.08f);
  gfx_cor((GfxRect){108,850,254,34},.2f,.25f,.32f,.34f,.55f);
  texto(TXT_ILHA_HORA,snap.configurada?i18n("Configuração conectada"):i18n("Somente manifesto"),118,860,234,192);
  texto(TXT_ILHA_HORA,host,108,891,252,134);
  texto(TXT_ILHA_TITULO,i18n(titulos[pagina]),453,184,1290,243);
  const char *intro=pagina==CATALOGOS?"CH+ / CH-: mover na Home":pagina==FONTES?"Addons, serviços, cache e P2P":pagina==LINKS?"Destinos externos e instalação":pagina==LEGENDAS?"Separadas das faixas dentro do vídeo":pagina==FILTROS?"Resumo somente leitura · Edite pelo celular":pagina==SAUDE?"Instância, latência e erros observados":"Sua configuração, com cada recurso no seu lugar.";
  bloco(i18n(intro),453,245,1240,1);
  if(pagina==PAINEL) {
    gfx_icone((GfxRect){473,318,25,25},"aj_shield-check",.8f,.86f,.88f,1);
    const char *displayHost=host;
    for(int i=0;i<7;i++) if(!strcmp(host,hosts[i])) displayHost=nomes[i];
    texto(TXT_ILHA_NOME,displayHost,518,306,1160,236);
    gfx_cor((GfxRect){453,374,1353,1},0,.8f,.84f,.88f,.08f);
    texto(TXT_ILHA_SUB,snap.configurada?i18n("Configuração conectada"):i18n("Somente manifesto"),518,350,1100,170);
    if(snap.valida) {
      gfx_cor((GfxRect){1690,313,116,34},.2f,.25f,.32f,.34f,.55f);
      gfx_icone((GfxRect){1700,322,16,16},"aj_check",.7f,.84f,.86f,1);
      texto(TXT_ILHA_NUM,i18n("Pronto"),1722,322,76,214);
    }
    // Per-card counts as in the approved mock, straight from snapshot data.
    int noInicio=0;
    for(int i=0;i<snap.catalogos;i++) { int f=fileira(i); if(f>=0&&!fil_linha_oculta(f)) noInicio++; }
    char card[6][256];
    snprintf(card[0],sizeof card[0],i18n("%d disponíveis · %d no Início"),snap.catalogos,noInicio);
    const char *serv=serviceAtivo();
    if(snap.addons-nLegendas()>0&&serv) snprintf(card[1],sizeof card[1],i18n("%d complementos · %s conectado"),snap.addons-nLegendas(),serv);
    else snprintf(card[1],sizeof card[1],"%s",i18n("Addons, serviços, cache e P2P"));
    snprintf(card[2],sizeof card[2],"%s",i18n("Destinos externos e instalação"));
    if(nLegendas()) snprintf(card[3],sizeof card[3],i18n("%d complementos · Sem faixas embutidas"),nLegendas());
    else snprintf(card[3],sizeof card[3],"%s",i18n("Separadas das faixas dentro do vídeo"));
    snprintf(card[4],sizeof card[4],"%s",i18n("Resumo só leitura · Edite no celular"));
    snprintf(card[5],sizeof card[5],"%s",i18n("Instância, latência e erros de fontes"));
    const char *sub[]={card[0],card[1],card[2],card[3],card[4],card[5]};
    for(int i=0;i<6;i++) {
      float x=453+(i%2)*727, w=i%2?626:702, y=389+(i/2)*112;
      linha(i,icons[i+1],nav[i+1],sub[i],x,y,w);
      texto(TXT_ILHA_NUM,snap.configurada?i18n("Configuração conectada"):i18n("Somente manifesto"),x+w-240,y+22,190,158);
    }
    gfx_cor((GfxRect){453,741,1353,1},0,.8f,.84f,.88f,.08f);
    texto(TXT_ILHA_NOME,i18n("Inspeção do título"),453,770,900,238);
    texto(TXT_ILHA_SUB,i18n("Todos os resultados recebidos, na ordem do AIOStreams."),453,804,980,174);
    botao(6,i18n("Inspecionar título"),1446,767,360); botao(7,i18n("Atualizar"),453,842,300); botao(8,i18n("Reconectar"),775,842,350);
    return;
  }
  if(pagina!=TITULO) aside();
  listaPos=janela(4,snap.catalogos,5,pagina==CATALOGOS && foco<snap.catalogos); int start=(int)listaPos;

  if(pagina==CATALOGOS) {
    texto(TXT_ILHA_SECAO,i18n("Catálogos publicados pelo manifesto"),453,285,690,220);
    texto(TXT_ILHA_HORA,i18n("Na Home"),1285,343,140,160);
    texto(TXT_ILHA_HORA,i18n("Catálogos"),520,343,690,160);
    texto(TXT_ILHA_HORA,i18n("Ativado"),1188,343,90,160);
    gfx_cor((GfxRect){453,321,980,1},0,1,1,1,.08f);
    for(int i=start;i<snap.catalogos && i<start+5;i++) {
      int f=fileira(i); char sub[192];
      snprintf(sub,sizeof sub,"%s · %s",origemCatalogo(i),!strcmp(snap.catalogo[i].tipo,"movie")?i18n("Filmes"):!strcmp(snap.catalogo[i].tipo,"series")?i18n("Séries"):snap.catalogo[i].tipo);
      float y=370+(i-listaPos)*88;
      plrui_linha_foco((GfxRect){453,y,980,86},22,focoDe(i,0));
      char rank[16]; snprintf(rank,sizeof rank,"%d",i+1); texto(TXT_ILHA_ITEM,rank,470,y+30,40,150);
      texto(TXT_ILHA_NOME,snap.catalogo[i].nome,520,y+16,640,239);
      texto(TXT_ILHA_SUB,sub,520,y+49,640,174);
      gfx_cor((GfxRect){1188,y+26,100,34},.2f,.3f,.39f,.41f,.55f);
      texto(TXT_ILHA_NUM,i18n("Ativado"),1199,y+36,80,200);
      int on=f>=0&&!fil_linha_oculta(f); float r,g,b; ajustes_acento(&r,&g,&b);
      gfx_cor((GfxRect){1305,y+30,40,24},.5f,.25f,.27f,.29f,1);
      gfx_cor((GfxRect){1308+(on?15:0),y+33,18,18},.5f,on?r:.67f,on?g:.7f,on?b:.71f,1);
      texto(TXT_ILHA_NUM,on?i18n("Ligado"):i18n("Desligado"),1354,y+33,78,180);
      ponteiro_alvo(453,y,980,86,pointer,click,i,0);
    }
    botao(snap.catalogos,i18n("Mover para cima"),453,743,240); botao(snap.catalogos+1,i18n("Mover para baixo"),705,743,270);
    texto(TXT_ILHA_HORA,i18n("CH+ / CH-: mover na Home"),473,822,890,148);
    botao(snap.catalogos+2,i18n("Atualizar catálogos"),453,900,450); botao(snap.catalogos+3,i18n("Gerenciar no celular"),925,900,470);
  } else if(pagina==FONTES) {
    int n=snap.addons-nLegendas()+snap.servicos, row=0;
    listaPos=janela(3,n,4,1); int first=(int)listaPos;

    for(int i=0;i<snap.addons+snap.servicos;i++) {
      AioItem *item=i<snap.addons?&snap.addon[i]:&snap.servico[i-snap.addons];
      if(i<snap.addons && item->legenda) continue;
      if(row>=first && row<first+4) {
        float y=330+(row-listaPos)*88;
        char sub[256]; int total=0,cached=0,uncached=0;
        for(int k=0;k<snap.resultados;k++) if(!strcmp(snap.resultado[k].provedor,item->nome)) {
          total++; cached+=snap.resultado[k].cache==1; uncached+=snap.resultado[k].cache==0;
        }
        const char *serv=serviceAtivo();
        if(total&&serv&&cached) snprintf(sub,sizeof sub,uncached?i18n("%s · %d em cache / %d sem cache"):i18n("%s · %d em cache"),serv,cached,uncached);
        else if(total) snprintf(sub,sizeof sub,"%s",i18n("HTTP direto · Cache não se aplica"));
        else snprintf(sub,sizeof sub,"%s",item->ativo?i18n("Ativado"):i18n("Desativado"));
        linha(row,i<snap.addons?"aj_layers":"aj_cloud",item->nome,sub,453,y,980);
        if(total) { char count[32]; snprintf(count,sizeof count,i18n("%d resultados"),total); texto(TXT_ILHA_SUB,count,1216,y+30,168,222); }
      }
      row++;
    }
    if(!snap.addons) bloco(i18n("Conecte a configuração para ver addons e serviços. O manifesto não revela suas chaves."),473,326,880,3);
    gfx_cor((GfxRect){453,700,980,1},0,1,1,1,.08f);
    // Aggregated debrid strip: one enabled service plus per-title cache evidence.
    const char *ativoServico=serviceAtivo();
    gfx_icone((GfxRect){473,722,26,26},"aj_cloud",.8f,.85f,.87f,1);
    if(ativoServico) {
      char titulo[128]; snprintf(titulo,sizeof titulo,i18n("%s conectado"),ativoServico);
      texto(TXT_ILHA_NOME,titulo,512,714,700,238);
      int emCache=0,semCache=0,httpDireto=0;
      for(int i=0;i<snap.resultados;i++) {
        if(snap.resultado[i].cache==1) emCache++;
        else if(snap.resultado[i].cache==0) semCache++;
        if(!strcmp(snap.resultado[i].tipo,"HTTP") && snap.resultado[i].cache<0) httpDireto++;
      }
      char sub[128]; snprintf(sub,sizeof sub,i18n("%d em cache · %d sem cache · %d HTTP direto"),emCache,semCache,httpDireto);
      texto(TXT_ILHA_NUM,sub,512,748,700,178);
    } else {
      texto(TXT_ILHA_NOME,i18n("Acesso ao serviço indisponível"),512,714,700,238);
      texto(TXT_ILHA_NUM,i18n("Conecte a configuração para ver o inventário de serviços"),512,748,700,178);
    }
    // P2P policy and Usenet sit side by side, as in the approved mock.
    float mx[2]={453,955}; int mw[2]={478,451};
    const char *mNome[2]={i18n("P2P / torrents"),i18n("Usenet")};
    const char *mSub[2]={ajustes_p2p_ligado()?i18n("Política local Nuvio: ligado"):i18n("Política local Nuvio: desligado"),i18n("Compatível · Não configurado")};
    const char *mIcone[2]={"aj_layers","aj_info"};
    for(int i=0;i<2;i++) {
      plrui_linha_foco((GfxRect){mx[i],784,mw[i],80},20,focoDe(n+i,0));
      gfx_icone((GfxRect){mx[i]+14,806,22,22},mIcone[i],.77f,.82f,.84f,1);
      texto(TXT_ILHA_NOME,mNome[i],mx[i]+48,796,mw[i]-92,239);
      texto(TXT_ILHA_NUM,mSub[i],mx[i]+48,828,mw[i]-92,174);
      gfx_icone((GfxRect){mx[i]+mw[i]-34,813,20,20},"aj_chevron-right",.68f,.73f,.76f,1);
      ponteiro_alvo(mx[i],784,mw[i],80,pointer,click,n+i,0);
    }
    texto(TXT_ILHA_NUM,i18n("Debrid por torrent usa HTTPS. A regra local não altera restrições da instância."),473,870,930,150);
    botao(n+2,i18n("Inspecionar título"),453,900,430); botao(n+3,i18n("Serviços no celular"),905,900,490);
  } else if(pagina==FILTROS && regraSelecionada>=0) {
    listaPos=janela(4,snap.detalhes,5,1); int row=0, first=(int)listaPos;
    for(int i=0;i<snap.detalhes;i++) if(snap.detalhe[i].grupo==regraSelecionada) {
      if(row>=first && row<first+5) linha(row,"aj_lock",regraNome(&snap.detalhe[i]),valorRegra(snap.detalhe[i].valor),453,310+(row-listaPos)*104,980);
      row++;
    }
    if(!row) bloco(i18n("Não informado"),473,345,900,2);
    botao(row,i18n("Editar no celular"),453,866,440);
  } else if(pagina==FILTROS) {
    const char *names[]={i18n("Resolução e qualidade"),i18n("Tamanho e bitrate"),i18n("Idiomas"),i18n("Cache e tipos de fonte"),i18n("Regex e palavras-chave"),i18n("Ordenação"),i18n("Formatador"),i18n("Proxy")};
    for(int i=0;i<8;i++) {
      char sub[128];
      if(!snap.configurada) snprintf(sub,sizeof sub,"%s",i18n("Conecte a configuração para ver esta regra"));
      else if(i==0) snprintf(sub,sizeof sub,i18n("Exigidas: %s · Excluídas: %s"),snap.resolucaoObrigatoria[0]?snap.resolucaoObrigatoria:i18n("Não informado"),snap.resolucaoExcluida[0]?snap.resolucaoExcluida:i18n("Não informado"));
      else if(i==3) snprintf(sub,sizeof sub,i18n("Excluir cache: %s · Excluir sem cache: %s"),snap.regraCache<0?i18n("Não informado"):snap.regraCache?i18n("Sim"):i18n("Não"),snap.regraUncached<0?i18n("Não informado"):snap.regraUncached?i18n("Sim"):i18n("Não"));
      else if(snap.resumo[i][0]) snprintf(sub,sizeof sub,"%s",valorRegra(snap.resumo[i]));
      else snprintf(sub,sizeof sub,i18n("%d campos informados · Somente leitura"),snap.regras[i]);
      float x=453+(i%2)*504, y=330+(i/2)*124;
      plrui_linha_foco((GfxRect){x,y,478,100},22,focoDe(i,0));
      texto(TXT_ILHA_NOME,names[i],x+12,y+20,412,239);
      texto(TXT_ILHA_SUB,sub,x+12,y+57,418,174);
      gfx_icone((GfxRect){x+440,y+38,20,20},"aj_lock",.6f,.65f,.68f,1);
      ponteiro_alvo(x,y,478,100,pointer,click,i,0);
    }
    botao(8,i18n("Editar no celular"),453,866,440);
  } else if(pagina==LEGENDAS) {
    texto(TXT_ILHA_SECAO,i18n("Addons"),453,285,650,220);
    listaPos=janela(2,nLegendas(),2,foco<=nLegendas()); int row=0, first=(int)listaPos;
    for(int i=0;i<snap.addons;i++) if(snap.addon[i].legenda) {
      if(row>=first && row<first+2) linha(row+1,"aj_captions",snap.addon[i].nome,snap.addon[i].ativo?i18n("Ativado"):i18n("Desativado"),453,330+(row-listaPos)*100,980);
      row++;
    }
    gfx_cor((GfxRect){453,540,980,1},0,1,1,1,.08f);
    bloco(i18n("Addons de legenda e faixas embutidas são diferentes. Resultados variam por título, arquivo e hash do vídeo."),453,560,980,2);
    texto(TXT_ILHA_SECAO,i18n("Legendas"),453,638,650,220);
    listaPos=janela(nLegendas()+2,snap.legendas,2,foco<=nLegendas()+snap.legendas); first=(int)listaPos;
    for(int i=first;i<snap.legendas && i<first+2;i++) {
      char flags[192];
      snprintf(flags,sizeof flags,"%s · %s: %s · SDH: %s",snap.legenda[i].idioma,i18n("Forçada"),snap.legenda[i].forcada<0?i18n("Não informado"):snap.legenda[i].forcada?i18n("Sim"):i18n("Não"),snap.legenda[i].sdh<0?i18n("Não informado"):snap.legenda[i].sdh?i18n("Sim"):i18n("Não"));
      linha(nLegendas()+i+1,"aj_captions",i18n(ling_nome(snap.legenda[i].idioma)),flags,453,674+(i-first)*80,980);
    }
    char languages[320]; snprintf(languages,sizeof languages,i18n("Principal: %s · Secundária: %s"),ling_legenda()[0]?i18n(ling_nome(ling_legenda())):i18n("Não informado"),ling_legenda2()[0]?i18n(ling_nome(ling_legenda2())):i18n("Não informado"));
    texto(TXT_ILHA_SUB,languages,453,852,980,180);
    botao(0,i18n("Inspecionar título"),453,900,420);
    botao(nConteudo()-1,i18n("Gerenciar no celular"),895,900,538);
  } else if(pagina==SAUDE) {
    char state[192]; snprintf(state,sizeof state,"%s · %s · %d ms",snap.versaoInstancia[0]?snap.versaoInstancia:i18n("Não informado"),snap.canal,snap.ms[1]);
    gfx_cor((GfxRect){453,286,980,112},.25f,.21f,.27f,.29f,.35f);
    gfx_icone((GfxRect){475,325,28,28},"aj_circle-check",.65f,.84f,.83f,1);
    texto(TXT_G30B,i18n("Status da instância"),550,310,670,236);
    texto(TXT_G20M,state,550,350,670,174);
    char total[24]; snprintf(total,sizeof total,"%d",snap.erros);
    texto(TXT_G30B,total,1320,309,100,236);
    texto(TXT_ILHA_NUM,i18n("Saúde"),1320,366,100,174);
    const char *names[]={i18n("Manifesto"),i18n("Status da instância"),i18n("Configuração"),i18n("Consulta de fontes"),i18n("Legendas"),i18n("Inspeção do título")};
    for(int i=0;i<6;i++) {
      float y=445+i*49; char status[80];
      texto(TXT_G20M,names[i],473,y,530,220);
      snprintf(status,sizeof status,snap.http[i]?"HTTP %d · %d ms":"—",snap.http[i],snap.ms[i]); texto(TXT_G20M,status,1060,y,360,174);
      gfx_cor((GfxRect){453,y+39,980,1},0,.8f,.84f,.88f,.08f);
    }
    char stats[192]; snprintf(stats,sizeof stats,i18n("%d estatísticas · %d erros · %d sem campos obrigatórios"),snap.estatisticas,snap.erros,snap.filtrados);
    texto(TXT_ILHA_SUB,snap.search?stats:i18n("Não informado"),473,765,950,174);
    bloco(i18n("Cota restante não informada. Nenhuma sondagem de reprodução. Em HTTP 429, aguarde antes de atualizar."),473,805,880,2);
    botao(0,i18n("Atualizar"),453,866,350); botao(1,i18n("Configuração no celular"),825,866,530);
  } else {
    int row=0, first=foco>3?foco-3:0;
    for(int i=0;i<snap.resultados;i++) if(strcmp(snap.resultado[i].tipo,"HTTP") && strcmp(snap.resultado[i].tipo,"P2P") && strcmp(snap.resultado[i].tipo,"Desconhecido")) {
      if(row>=first && row<first+3) linha(row,"aj_smartphone",snap.resultado[i].nome[0]?snap.resultado[i].nome:i18n(snap.resultado[i].tipo),i18n("Abrir no celular"),453,310+(row-first)*104,980);
      row++;
    }
    if(!row) bloco(i18n("Links externos, YouTube e NZB não são vídeos HTTP reproduzíveis. Abra e gerencie pelo celular."),473,340,850,4);
    if(linkSelecionado>=0) { qrDesenhar(celular_url(),1185,575,176); texto(TXT_ILHA_HORA,i18n("Credenciais ocultas"),1185,765,220,142); }
    gfx_cor((GfxRect){453,622,980,1},0,1,1,1,.08f);
    texto(TXT_ILHA_NOME,i18n("Link de instalação"),453,648,720,239);
    bloco(i18n("Links pessoais permanecem ocultos. Exporte ou importe arquivos JSON pelo celular."),453,690,700,2);
    botao(snap.links+1,i18n("Reconectar"),453,756,300);
    gfx_cor((GfxRect){453,830,980,1},0,1,1,1,.08f);
    botao(snap.links,i18n("Inspecionar título"),453,866,400);
    botao(snap.links+2,i18n("Fontes e serviços"),875,866,480);
  }
}
static void addonPointer(int i,int b) { (void)b; addonsui_ponteiro(i,0); }
static void addonClick(int i,int b) {
  addonPointer(i,b); SDL_Event e={0}; e.type=SDL_KEYDOWN; e.key.keysym.sym=SDLK_RETURN; addonsui_evento(&e);
}
static void addonsPainel(float x,float y,float w,float h) { painel(x,y,w,h); }
void aioui_addons_desenhar(int f) {
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
    texto(TXT_ILHA_NOME,addons_nome(i),535,y+17,1190,242);
    texto(TXT_ILHA_SUB,aioui_detectar(i)?i18n("Abrir painel"):addons_ativo(i)?i18n("Ativado"):i18n("Desativado"),535,y+53,1190,174);
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
  texto(TXT_ILHA_SUB,i18n("Setas: navegar    OK: abrir    Voltar: retornar"),92,1020,1450,160);
}
void aioui_desenhar(Uint32 agora) {
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
  if(pagina<PAINEL) setup(); else workspace();
  if(deslocamento) gfx_sem_transformar();
  deslocamento=0;
  gfx_opacidade_grupo=alpha*entry;
  const char *keys[]={"↑ ↓", "OK", "Voltar"};
  const char *labels[]={i18n("Navegar"),i18n("Abrir"),i18n("Voltar")};
  plrui_dicas(keys,labels,3,92,1020,0,.7f);
  if(mensagem[0]) texto(TXT_ILHA_SUB,mensagem,453,934,1340,215);
  gfx_opacidade_grupo=alpha;
  if(teclado_aberto()) teclado_desenhar(agora);
}
#ifdef NV_SHOT_HOOKS
void aioui_shot(int p,const AioSnapshot *s) {
  abertura=0; saindo=0; paginaAnterior=-1;
  perfil=perfis_ativo(); snprintf(usuario,sizeof usuario,"%s",sessao_usuario());
  indice=-1; pendente=0;
  aberto=1; pagina=p; snap=*s; foco=p==TITULO?2:0; lateral=0; regraSelecionada=linkSelecionado=-1; evidenciaSelecionada=0;
  snprintf(host,sizeof host,"%s",hosts[0]); qrAtual();
}
#endif
