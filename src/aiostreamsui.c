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
static unsigned versao, geracao;
static int catalogoSelecionado, regraSelecionada=-1, linkSelecionado=-1;
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
  aberto=0; geracao++; pendente=0; celular_fechar();
  memset(&snap,0,sizeof snap); memset(base,0,sizeof base); alvo[0]=mensagem[0]=0;
}
void aioui_abrir(int i) {
  aberto=1; geracao++; foco=0; lateral=0; entrada=pendente=0; indice=i; alvo[0]=0; proximaConsulta=0;
  regraSelecionada=linkSelecionado=-1;
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
static void mudar(int p) { pagina=p; foco=0; lateral=0; mensagem[0]=0; regraSelecionada=-1; linkSelecionado=-1; celular_fechar(); }
static void campo(int qual) {
  entrada=qual;
  teclado_abrir_com(qual==1?i18n("URL da instância"):qual==2?i18n("ID do título"):i18n("Link de instalação"),
    i18n("Cole pelo celular. O link pessoal permanece oculto."),qual==3?TECLADO_LONGO:TECLADO_MAX,
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789:/?&=._-%+@!~#",NULL);
  teclado_mascarar(qual==3);
}
static int nLinhas(void) {
  if(pagina==SEM) return 3;
  if(pagina==INSTANCIA) return 10;
  if(pagina==SERVICOS) return 2;
  if(pagina==IMPORTAR) return 2;
  if(pagina==PRONTO) return 2;
  if(pagina==PAINEL) return 9;
  if(pagina==CATALOGOS) return snap.catalogos+4;
  if(pagina==FONTES) return snap.addons+snap.servicos+3;
  if(pagina==LEGENDAS) return snap.legendas+2;
  if(pagina==LINKS) return snap.links+3;
  if(pagina==FILTROS) {
    if(regraSelecionada>=0) { int n=1; for(int i=0;i<snap.detalhes;i++) if(snap.detalhe[i].grupo==regraSelecionada) n++; return n; }
    return 9;
  }
  if(pagina==TITULO) return snap.resultados+2;
  return 2;
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
  if(pagina==SEM) { int f=foco; mudar(f==2?IMPORTAR:INSTANCIA); if(f==1) campo(1); }
  else if(pagina==INSTANCIA) {
    if(foco<7) { hostIdx=foco; snprintf(host,sizeof host,"%s",hosts[hostIdx]); qrAtual(); }
    else if(foco==7) campo(1); else if(foco==8) mudar(IMPORTAR); else mudar(SERVICOS);
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
    if(foco==snap.addons+snap.servicos) ajustes_definir_p2p_ligado(!ajustes_p2p_ligado());
    else if(foco==snap.addons+snap.servicos+1) campo(2);
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
  } else if(pagina==LEGENDAS) { if(foco==0) campo(2); else if(foco>snap.legendas) mudar(SERVICOS); }
  else if(pagina==TITULO) {
    if(foco==0) campo(2); else if(foco==1) { geracao++; atualizarRede(0); }
    else if(foco>=2 && foco-2<snap.resultados && snap.resultado[foco-2].destino[0]) {
      if(celular_abrir_link(i18n("Abrir no celular"),snap.resultado[foco-2].destino)) linkSelecionado=foco-2;
    }
  } else if(pagina==SAUDE && foco==0) { geracao++; atualizarRede(0); }
  else mudar(SERVICOS);
}
void aioui_evento(const SDL_Event *e) {
  if(!aberto) return;
  if(teclado_aberto()) { teclado_evento(e); return; }
  if(e->type!=SDL_KEYDOWN) return;
  SDL_Keycode k=e->key.keysym.sym;
  if(k==SDLK_ESCAPE || k==SDLK_AC_BACK || k==SDLK_BACKSPACE) {
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
  TxtLinha t=txt_linha_corta(e,s,cor,cor,cor,255,w); txt_desenhar(t,x,y);
}
static void bloco(const char *s,float x,float y,float w,int linhas) {
  txt_bloco_corta(TXT_ILHA_SUB,s,170,179,185,x,y,w,28,1,linhas);
}
static void painel(float x,float y,float w,float h) {
  GfxRect r={x,y,w,h};
  if(ajustes_vidro()) gfx_vidro_folha(r,32/h,1);
  else gfx_cor(r,32/h,.071f,.075f,.086f,.98f);
}
static void pointer(int i,int b) { lateral=b; foco=i; }
static void click(int i,int b) { pointer(i,b); acao(); }
static void voltarClick(int a,int b) {
  (void)a; (void)b; SDL_Event e={0}; e.type=SDL_KEYDOWN; e.key.keysym.sym=SDLK_ESCAPE;
  aioui_evento(&e);
}
static void linha(int i,const char *icon,const char *nome,const char *sub,float x,float y,float w) {
  if(y<295 || y>855) return;
  float h=pagina==SEM?145:94, ty=pagina==SEM?38:17;
  if(!lateral && foco==i) gfx_cor((GfxRect){x,y,w,h},22/h,.19f,.205f,.22f,.9f);
  gfx_cor((GfxRect){x+20,y+(h-52)/2,52,52},.3f,.21f,.225f,.24f,.45f);
  gfx_icone((GfxRect){x+32,y+(h-28)/2,28,28},icon,.85f,.88f,.89f,1);
  texto(pagina==SEM?TXT_G28B:TXT_ILHA_NOME,nome,x+94,y+ty,w-152,241);
  texto(TXT_G20M,sub,x+94,y+ty+36,w-152,174);
  gfx_icone((GfxRect){x+w-44,y+(h-22)/2,22,22},"aj_chevron-right",.68f,.73f,.76f,1);
  ponteiro_alvo(x,y,w,h,pointer,click,i,0);
}
static void botao(int i,const char *nome,float x,float y,float w) {
  float r,g,b; ajustes_acento(&r,&g,&b); int f=!lateral && foco==i;
  gfx_cor((GfxRect){x,y,w,56},.5f,f?r:.14f,f?g:.15f,f?b:.16f,1);
  texto(TXT_ILHA_SUB,nome,x+24,y+16,w-48,f?ajustes_tinta_foco():235);
  ponteiro_alvo(x,y,w,56,pointer,click,i,0);
}
static void qrDesenhar(const char *url,float x,float y,float size) {
  // Shared QR texture: one draw instead of hundreds of modules each frame.
  float scale=size/celb_qr_lado();
  gfx_transformar(0,0,scale,x,y);
  celb_qr_desenhar(url,0,0,1);
  gfx_sem_transformar();
}
static void aside(void) {
  gfx_cor((GfxRect){1466,285,1,545},0,.8f,.84f,.88f,.08f);
  texto(TXT_ILHA_HORA,"AIOStreams",1497,289,302,174);
  texto(TXT_ILHA_SECAO,i18n("Edite pelo celular"),1497,324,302,238);
  bloco(i18n("Regras avançadas ficam no AIOStreams. Salve lá e atualize esta TV."),1497,378,290,3);
  qrDesenhar(configure,1497,483,152);
  bloco(i18n("Página oficial de configuração. Entre com sua própria conta."),1497,650,290,3);
  texto(TXT_ILHA_HORA,i18n("Credenciais ocultas"),1497,798,290,142);
}
static void setup(void) {
  painel(72,148,1776,824);
  GLuint logo=tex_obter(arteLogo);
  if(logo) gfx_rect((GfxRect){124,216,48,48},logo,GFX_MARCA,0,0,0,0,1,1,1,1);
  texto(TXT_G30B,"AIOStreams",199,206,440,242);
  texto(TXT_ILHA_SUB,i18n("Um addon, todas as suas fontes"),199,248,440,174);
  gfx_cor((GfxRect){661,156,1,815},0,.8f,.84f,.88f,.08f);
  static const char *const story[]={"Um lugar.\nTodas as fontes.","Escolha onde\nele roda.","Sua configuração.\nNesta TV.","Configure uma vez.\nAssista do seu jeito.","Conectado.\nPronto para explorar."};
  texto(TXT_ILHA_HORA,i18n(titulos[pagina]),124,312,490,174);
  float storyHeight=txt_bloco(TXT_V2_A3TIT,i18n(story[pagina]),240,241,242,124,345,490,56,1,4);
  bloco(i18n("Reúna addons, catálogos e legendas. Configure os detalhes no celular e acompanhe tudo nesta TV."),124,345+storyHeight+34,485,5);
  const char *steps[]={i18n("Escolha sua instância"),i18n("Conecte e configure"),i18n("Valide nesta TV")};
  for(int i=0;i<3;i++) {
    gfx_icone((GfxRect){125,800+i*49,24,24},i==2&&pagina==PRONTO?"aj_check":"aj_circle-dot",.7f,.76f,.79f,1);
    texto(TXT_ILHA_SUB,steps[i],169,800+i*49,450,195);
  }
  if(pagina!=PRONTO) texto(TXT_ILHA_PERGUNTA,pagina==SEM?i18n("Como deseja conectar?"):i18n(titulos[pagina]),716,204,1080,240);
  if(pagina==SEM) {
    linha(0,"aj_globe",i18n("Usar uma instância pública"),i18n("Escolha um servidor listado na documentação."),716,310,1078);
    linha(1,"aj_server",i18n("Conectar meu servidor"),i18n("Informe a origem do seu AIOStreams."),716,468,1078);
    linha(2,"aj_smartphone",i18n("Importar configuração existente"),i18n("Traga seu link de instalação."),716,626,1078);
    bloco(i18n("Debrid é opcional. HTTP, P2P e canais dependem da instância e desta TV."),716,774,1040,4);
  } else if(pagina==INSTANCIA) {
    botao(7,i18n("Meu servidor"),716,263,320); botao(8,i18n("Já tenho o link"),1056,263,350);
    int start=foco<7 && foco>3?foco-3:0;
    for(int i=start;i<7 && i<start+4;i++) linha(i,"aj_globe",nomes[i],i==2?i18n("Pública: sem P2P, HTTP ou canais ao vivo"):i==3?i18n("Versão de desenvolvimento"):i18n("Estável · Servidor comunitário"),716,345+(i-start)*94,1078);
    bloco(i18n("Debrid é opcional. HTTP, P2P e canais dependem da instância e desta TV."),738,772,1040,2);
    botao(9,i18n("Continuar"),716,877,360);
  } else if(pagina==SERVICOS) {
    const char *check[]={i18n("Serviços"),i18n("Addons"),i18n("Filtros e ordenação"),i18n("Formatador")};
    const char *sub[]={i18n("1. Serviços: debrid ou Usenet, opcionais"),i18n("2. Addons: escolha suas fontes e catálogos"),i18n("3. Filtros e ordenação: revise suas regras"),i18n("4. Formatador: nomes e descrições das fontes")};
    const char *ico[]={"aj_cloud","aj_puzzle","aj_sliders-horizontal","aj_file-text"};
    for(int i=0;i<4;i++) {
      float y=290+i*96;
      gfx_icone((GfxRect){738,y+26,28,28},ico[i],.8f,.85f,.87f,1);
      texto(TXT_ILHA_NOME,check[i],810,y+13,920,238);
      texto(TXT_G20M,sub[i],810,y+49,920,174);
    }
    qrDesenhar(configure,716,706,141); bloco(i18n("Salvar e instalar no AIOStreams. Depois copie o link do manifesto."),882,722,820,4);
    botao(0,i18n("Tenho meu link de instalação"),716,877,480); botao(1,i18n("Instância"),1216,877,300);
  } else if(pagina==IMPORTAR) {
    if(entrada!=3) qrDesenhar(configure,716,287,206);
    bloco(i18n("Abra a configuração oficial, salve e copie o link de instalação. Envie pelo pareamento já usado pelo Nuvio."),970,314,790,5);
    texto(TXT_G20M,i18n("1. Serviços: debrid ou Usenet, opcionais"),716,530,1040,210);
    texto(TXT_G20M,i18n("2. Addons: escolha suas fontes e catálogos"),716,571,1040,210);
    gfx_cor((GfxRect){716,655,1078,66},.25f,.17f,.185f,.20f,.7f);
    texto(TXT_G22M,i18n("Link de instalação"),738,676,1034,220);
    bloco(i18n("Links pessoais permanecem ocultos. Exporte ou importe arquivos JSON pelo celular."),716,745,1040,2);
    if(entrada==3) qrDesenhar(celular_url(),716,305,210);
    ponteiro_alvo(716,655,1078,66,pointer,click,1,0);
    botao(0,i18n("Enviar pelo celular"),716,842,380); botao(1,i18n("Colar link"),1116,842,350);
  } else {
    gfx_icone((GfxRect){716,207,84,84},"aj_circle-check",.65f,.84f,.83f,1);
    texto(TXT_ILHA_PERGUNTA,i18n("AIOStreams adicionado"),716,319,1040,240);
    texto(TXT_G20M,host,716,371,1040,174);
    const char *checks[]={i18n("Manifesto"),snap.configurada?i18n("Configuração conectada"):i18n("Somente manifesto"),i18n("Credenciais ocultas")};
    for(int i=0;i<3;i++) {
      gfx_icone((GfxRect){716,425+i*48,24,24},"aj_check",.65f,.84f,.83f,1);
      texto(TXT_G20M,checks[i],756,425+i*48,1000,220);
    }
    const char *labels[]={i18n("Catálogos"),i18n("Addons"),i18n("Serviços")};
    int counts[]={snap.catalogos,snap.addons,snap.servicos};
    for(int i=0;i<3;i++) { char count[24]; snprintf(count,sizeof count,"%d",counts[i]);
      texto(TXT_G30B,count,716+i*270,601,240,240); texto(TXT_ILHA_SUB,labels[i],716+i*270,644,240,174);
    }
    bloco(i18n("Manifesto reconhecido. A validação não garante a reprodução de cada fonte."),716,711,1040,3);
    botao(0,i18n("Abrir painel"),716,842,370); botao(1,i18n("Escolher catálogos"),1106,842,410);
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
  const char *label=strstr(s,"Resolutions")?i18n("Resolução e qualidade"):strstr(s,"Languages")||strstr(s,"Subtitles")?i18n("Idiomas"):strstr(s,"Qualities")||strstr(s,"Tags")?i18n("Qualidade"):strstr(s,"StreamTypes")?i18n("Cache e tipos de fonte"):strstr(s,"Seeder")||strstr(s,"seeder")?i18n("Semeadores"):strstr(s,"size")?i18n("Tamanho"):strstr(s,"bitrate")?i18n("Bitrate"):strstr(s,"sort.")?i18n("Ordenação"):strstr(s,"proxy.")?i18n("Proxy"):strstr(s,"template.")?i18n("Modelo"):strstr(s,"formatter.")?i18n("Formatador"):r->grupo==4?i18n("Regex e palavras-chave"):i18n("Cache e tipos de fonte");
  const char *media=strstr(s,"movies")?i18n("Filmes"):strstr(s,"series")?i18n("Séries"):strstr(s,"anime")?"Anime":"";
  snprintf(name,sizeof name,"%s%s%s%s%s",label,mode[0]?" · ":"",mode,media[0]?" · ":"",media);
  return name;
}
static const char *decisao(const AioResultado *r) {
  return r->decisao==1?i18n("Destino externo: fora do player da TV."):r->decisao==2?i18n("Oculto pelos filtros locais"):r->decisao==3?i18n("Fora do cache: escolha manual."):r->decisao==4?i18n("Mantido na lista da TV"):i18n("Resultado recebido. Compatibilidade ainda não verificada.");
}
static void tituloDesenhar(void) {
  painel(72,148,1362,824); painel(1458,148,390,824);
  texto(TXT_ILHA_TITULO,i18n("Inspeção do título"),108,184,1080,242);
  texto(TXT_G20M,alvo,108,245,1040,174);
  botao(0,i18n("Escolher título"),108,283,340); botao(1,i18n("Atualizar"),470,283,240);
  texto(TXT_ILHA_HORA,i18n("AIO # / Fonte recebida"),125,363,800,148);
  texto(TXT_ILHA_HORA,i18n("Posição na TV"),1025,363,150,148);
  texto(TXT_ILHA_HORA,i18n("Decisão Nuvio"),1175,363,220,148);
  int first=foco>9?foco-9:0;
  for(int i=first;i<snap.resultados && i<first+8;i++) {
    float y=389+(i-first)*60; AioResultado *r=&snap.resultado[i];
    if(!lateral && foco==i+2) gfx_cor((GfxRect){108,y,1290,59},.36f,.19f,.205f,.22f,.9f);
    char num[24]; snprintf(num,sizeof num,"%d",i+1); texto(TXT_G20M,num,125,y+20,50,150);
    texto(TXT_ILHA_SECAO,r->nome[0]?r->nome:i18n("Fonte sem nome"),193,y+7,810,238);
    char sub[256]; snprintf(sub,sizeof sub,"%s · %s",i18n(r->tipo),r->cache<0?i18n("Cache desconhecido"):r->cache?i18n("Em cache"):i18n("Fora do cache"));
    texto(TXT_ILHA_NUM,sub,193,y+35,810,174);
    snprintf(num,sizeof num,r->rank?"%d":"—",r->rank); texto(TXT_G20M,num,1034,y+20,120,222);
    texto(TXT_ILHA_NUM,decisao(r),1160,y+22,230,184);
    ponteiro_alvo(108,y,1290,60,pointer,click,i+2,0);
  }
  texto(TXT_ILHA_NUM,i18n("Ordem recebida; rejeições anteriores não são fornecidas pelo manifesto."),124,890,1270,150);
  int selected=foco>=2?foco-2:0;
  texto(TXT_ILHA_HORA,i18n("Decisão Nuvio"),1490,184,326,174);
  if(selected<snap.resultados) {
    AioResultado *r=&snap.resultado[selected];
    txt_bloco_corta(TXT_ILHA_NOME,r->nome,240,240,238,1490,217,326,34,1,3);
    texto(TXT_G20M,i18n("Posição na TV"),1490,340,230,174);
    char rank[32]; snprintf(rank,sizeof rank,r->rank?"%d":"—",r->rank); texto(TXT_G20M,rank,1765,340,50,236);
    bloco(decisao(r),1490,454,326,5);
  }
  char stats[192]; snprintf(stats,sizeof stats,i18n("%d estatísticas · %d erros · %d sem campos obrigatórios"),snap.estatisticas,snap.erros,snap.filtrados);
  bloco(snap.search?stats:i18n("Não informado"),1490,650,326,4);
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
    if((lateral && foco==i) || pagina==PAINEL+i) gfx_cor((GfxRect){91,y,277,64},.32f,.15f,.17f,.18f,1);
    gfx_icone((GfxRect){111,y+19,25,25},icons[i],.77f,.82f,.84f,1);
    texto(TXT_G22M,nav[i],150,y+19,210,220);
    if(pagina==PAINEL+i) gfx_cor((GfxRect){97,y+30,5,5},.5f,.72f,.86f,.89f,1);
    ponteiro_alvo(91,y,277,64,pointer,click,i,1);
  }
  texto(TXT_ILHA_HORA,snap.configurada?i18n("Configuração conectada"):i18n("Somente manifesto"),108,850,256,175);
  texto(TXT_ILHA_HORA,host,108,891,252,134);
  texto(TXT_ILHA_TITULO,i18n(titulos[pagina]),453,184,1290,243);
  bloco(pagina==TITULO?i18n("Todos os resultados recebidos, na ordem do AIOStreams."):i18n("Sua configuração, com cada recurso no seu lugar."),453,245,1240,1);
  if(pagina==PAINEL) {
    gfx_icone((GfxRect){473,318,25,25},"aj_shield-check",.8f,.86f,.88f,1);
    const char *displayHost=host;
    for(int i=0;i<7;i++) if(!strcmp(host,hosts[i])) displayHost=nomes[i];
    texto(TXT_ILHA_NOME,displayHost,518,306,1160,236);
    gfx_cor((GfxRect){453,374,1353,1},0,.8f,.84f,.88f,.08f);
    texto(TXT_ILHA_SUB,snap.configurada?i18n("Configuração conectada"):i18n("Somente manifesto"),518,350,1160,170);
    const char *sub[]={i18n("Catálogos publicados pelo manifesto"),i18n("Addons, serviços, cache e P2P"),i18n("Destinos externos e instalação"),i18n("Separadas das faixas dentro do vídeo"),i18n("Resumo somente leitura · Edite pelo celular"),i18n("Instância, latência e erros observados")};
    for(int i=0;i<6;i++) linha(i,icons[i+1],nav[i+1],sub[i],453+(i%2)*727,389+(i/2)*112,i%2?626:702);
    gfx_cor((GfxRect){453,741,1353,1},0,.8f,.84f,.88f,.08f);
    texto(TXT_ILHA_NOME,i18n("Inspeção do título"),453,770,900,238);
    texto(TXT_G20M,i18n("Todos os resultados recebidos, na ordem do AIOStreams."),453,804,980,174);
    botao(6,i18n("Inspecionar título"),1446,767,360); botao(7,i18n("Atualizar"),453,842,300); botao(8,i18n("Reconectar"),775,842,350);
    return;
  }
  if(pagina!=TITULO) aside();
  int start=foco>4?foco-4:0;
  if(pagina==CATALOGOS) {
    texto(TXT_ILHA_HORA,i18n("CH+ / CH-: mover na Home"),473,285,890,148);
    for(int i=start;i<snap.catalogos && i<start+5;i++) {
      int f=fileira(i); char sub[128];
      snprintf(sub,sizeof sub,"%s · %s",!strcmp(snap.catalogo[i].tipo,"movie")?i18n("Filmes"):!strcmp(snap.catalogo[i].tipo,"series")?i18n("Séries"):snap.catalogo[i].tipo,f>=0&&!fil_linha_oculta(f)?i18n("Na Home"):i18n("Fora da Home"));
      linha(i,"aj_library",snap.catalogo[i].nome,sub,453,310+(i-start)*104,950);
    }
    botao(snap.catalogos,i18n("Mover para cima"),453,832,450); botao(snap.catalogos+1,i18n("Mover para baixo"),925,832,470);
    botao(snap.catalogos+2,i18n("Atualizar catálogos"),453,900,450); botao(snap.catalogos+3,i18n("Gerenciar no celular"),925,900,470);
  } else if(pagina==FONTES) {
    int n=snap.addons+snap.servicos;
    int first=foco>2?foco-2:0;
    if(first>=n && n>0) first=n>3?n-3:0;
    for(int i=first;i<n && i<first+3;i++) {
      AioItem *item=i<snap.addons?&snap.addon[i]:&snap.servico[i-snap.addons];
      linha(i,i<snap.addons?"aj_layers":"aj_cloud",item->nome,item->ativo?i18n("Ativado"):i18n("Desativado"),453,310+(i-first)*104,950);
    }
    if(!snap.addons) bloco(i18n("Conecte a configuração para ver addons e serviços. O manifesto não revela suas chaves."),473,326,880,3);
    char resumo[160]; snprintf(resumo,sizeof resumo,i18n("%d serviços · %d resultados · %d links · %d P2P"),snap.servicos,snap.resultados,snap.links,snap.p2p);
    texto(TXT_ILHA_SUB,resumo,473,748,880,178);
    linha(n,"aj_layers",i18n("P2P / torrents"),ajustes_p2p_ligado()?i18n("Política local Nuvio: ligado"):i18n("Política local Nuvio: desligado"),453,650,950);
    botao(n+1,i18n("Inspecionar título"),453,866,430); botao(n+2,i18n("Serviços no celular"),905,866,490);
  } else if(pagina==FILTROS && regraSelecionada>=0) {
    int row=0, first=foco>4?foco-4:0;
    for(int i=0;i<snap.detalhes;i++) if(snap.detalhe[i].grupo==regraSelecionada) {
      if(row>=first && row<first+5) linha(row,"aj_lock",regraNome(&snap.detalhe[i]),valorRegra(snap.detalhe[i].valor),453,310+(row-first)*104,980);
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
      if(!lateral && foco==i) gfx_cor((GfxRect){x,y,478,100},.22f,.19f,.205f,.22f,.9f);
      texto(TXT_ILHA_NOME,names[i],x+12,y+20,412,239);
      texto(TXT_ILHA_SUB,sub,x+12,y+57,418,174);
      gfx_icone((GfxRect){x+440,y+38,20,20},"aj_lock",.6f,.65f,.68f,1);
      ponteiro_alvo(x,y,478,100,pointer,click,i,0);
    }
    botao(8,i18n("Editar no celular"),453,866,440);
  } else if(pagina==LEGENDAS) {
    botao(0,i18n("Inspecionar título"),453,295,380);
    int y=375;
    for(int i=0;i<snap.addons && y<560;i++) if(snap.addon[i].legenda) {
      linha(snap.legendas+1,"aj_captions",snap.addon[i].nome,snap.addon[i].ativo?i18n("Ativado"):i18n("Desativado"),453,y,980); y+=94;
    }
    int first=foco>3?foco-3:0;
    for(int i=first;i<snap.legendas && i<first+3;i++) {
      char flags[192];
      snprintf(flags,sizeof flags,"%s · %s: %s · SDH: %s",snap.legenda[i].idioma,i18n("Forçada"),snap.legenda[i].forcada<0?i18n("Não informado"):snap.legenda[i].forcada?i18n("Sim"):i18n("Não"),snap.legenda[i].sdh<0?i18n("Não informado"):snap.legenda[i].sdh?i18n("Sim"):i18n("Não"));
      linha(i+1,"aj_captions",i18n(ling_nome(snap.legenda[i].idioma)),flags,453,560+(i-first)*88,980);
    }
    texto(TXT_ILHA_NUM,snap.recursos&AIO_LEGENDA?i18n("Recurso de legendas publicado"):i18n("Recurso de legendas não publicado"),473,352,920,170);
    if(!snap.legendas) bloco(i18n("Addons de legenda e faixas embutidas são diferentes. Resultados variam por título, arquivo e hash do vídeo."),473,600,870,4);
    char languages[320]; snprintf(languages,sizeof languages,i18n("Principal: %s · Secundária: %s"),ling_legenda()[0]?i18n(ling_nome(ling_legenda())):i18n("Não informado"),ling_legenda2()[0]?i18n(ling_nome(ling_legenda2())):i18n("Não informado"));
    texto(TXT_ILHA_SUB,languages,473,760,870,180);
    botao(snap.legendas+1,i18n("Gerenciar legendas no celular"),453,866,540);
  } else if(pagina==SAUDE) {
    char state[192]; snprintf(state,sizeof state,"%s · %s · %d ms",snap.versaoInstancia[0]?snap.versaoInstancia:i18n("Não informado"),snap.canal,snap.ms[1]);
    texto(TXT_ILHA_NOME,i18n("Status da instância"),473,305,940,236);
    texto(TXT_G20M,state,473,343,940,174);
    const char *names[]={i18n("Manifesto"),i18n("Status da instância"),i18n("Configuração"),i18n("Consulta de fontes"),i18n("Legendas"),i18n("Inspeção do título")};
    for(int i=0;i<6;i++) {
      float y=407+i*49; char status[80];
      texto(TXT_G20M,names[i],473,y,530,220);
      snprintf(status,sizeof status,snap.http[i]?"HTTP %d · %d ms":"—",snap.http[i],snap.ms[i]); texto(TXT_G20M,status,1060,y,360,174);
      gfx_cor((GfxRect){453,y+39,980,1},0,.8f,.84f,.88f,.08f);
    }
    char stats[192]; snprintf(stats,sizeof stats,i18n("%d estatísticas · %d erros · %d sem campos obrigatórios"),snap.estatisticas,snap.erros,snap.filtrados);
    texto(TXT_ILHA_SUB,snap.search?stats:i18n("Não informado"),473,722,950,174);
    bloco(i18n("Cota restante não informada. Nenhuma sondagem de reprodução. Em HTTP 429, aguarde antes de atualizar."),473,764,880,3);
    botao(0,i18n("Atualizar"),453,866,350); botao(1,i18n("Configuração no celular"),825,866,530);
  } else {
    int row=0, first=foco>3?foco-3:0;
    for(int i=0;i<snap.resultados;i++) if(strcmp(snap.resultado[i].tipo,"HTTP") && strcmp(snap.resultado[i].tipo,"P2P") && strcmp(snap.resultado[i].tipo,"Desconhecido")) {
      if(row>=first && row<first+3) linha(row,"aj_smartphone",snap.resultado[i].nome[0]?snap.resultado[i].nome:i18n(snap.resultado[i].tipo),i18n("Abrir no celular"),453,310+(row-first)*104,980);
      row++;
    }
    if(!row) bloco(i18n("Links externos, YouTube e NZB não são vídeos HTTP reproduzíveis. Abra e gerencie pelo celular."),473,340,850,4);
    if(linkSelecionado>=0) { qrDesenhar(celular_url(),473,640,176); bloco(i18n("Credenciais ocultas"),690,670,700,2); }
    botao(snap.links,i18n("Inspecionar título"),453,842,400);
    botao(snap.links+1,i18n("Reconectar"),875,842,350);
    botao(snap.links+2,i18n("Fontes e serviços"),453,906,400);
  }
}
static void addonPointer(int i,int b) { (void)b; addonsui_ponteiro(i,0); }
static void addonClick(int i,int b) {
  addonPointer(i,b); SDL_Event e={0}; e.type=SDL_KEYDOWN; e.key.keysym.sym=SDLK_RETURN; addonsui_evento(&e);
}
static void addonsPainel(float x,float y,float w,float h) {
  GfxRect r={x,y,w,h}; if(ajustes_vidro()) gfx_vidro_folha(r,32/h,1); else gfx_cor(r,32/h,.071f,.075f,.086f,.98f);
}
void aioui_addons_desenhar(int f) {
  gfx_cor((GfxRect){0,0,1920,1080},0,.035f,.065f,.077f,1);
  GLuint fundo=tex_obter_hero(arteFundo); if(fundo) gfx_textura((GfxRect){0,0,1920,1080},fundo);
  addonsPainel(72,148,315,824); addonsPainel(411,148,1437,824);
  texto(TXT_ILHA_SUB,i18n("Ajustes / Addons / AIOStreams"),150,65,1400,196);
  texto(TXT_ILHA_SECAO,i18n("Ajustes"),111,190,240,180);
  texto(TXT_ILHA_NOME,i18n("Addons"),111,395,240,240);
  texto(TXT_ILHA_HORA,i18n("Ligados nesta TV"),453,199,1200,174);
  texto(TXT_ILHA_TITULO,i18n("Addons"),453,230,1200,240);
  texto(TXT_ILHA_SUB,i18n("Ligados nesta TV"),453,290,1200,170);
  int n=addons_n(), start=f>4?f-4:0;
  for(int i=start;i<n && i<start+5;i++) {
    float y=360+(i-start)*105;
    if(i==f) gfx_cor((GfxRect){453,y,1340,94},.23f,.18f,.20f,.21f,1);
    if(aioui_detectar(i)) {
      GLuint logo=tex_obter(arteLogo); if(logo) gfx_rect((GfxRect){477,y+24,43,43},logo,GFX_MARCA,0,0,0,0,1,1,1,1);
    } else gfx_icone((GfxRect){477,y+28,28,28},"aj_puzzle",.85f,.88f,.9f,1);
    texto(TXT_ILHA_NOME,addons_nome(i),535,y+17,1190,242);
    texto(TXT_ILHA_SUB,aioui_detectar(i)?i18n("Abrir painel"):addons_ativo(i)?i18n("Ativado"):i18n("Desativado"),535,y+53,1190,174);
    ponteiro_alvo(453,y,1340,94,addonPointer,addonClick,i,0);
  }
  GfxRect r={453,866,550,56};
  gfx_cor(r,.5f,f==n?.26f:.14f,.17f,.19f,1);
  texto(TXT_ILHA_SUB,sync_manual_addons_rodando()?i18n("Sincronizando…"):i18n("Sincronizar addons"),477,882,500,234);
  ponteiro_alvo(r.x,r.y,r.w,r.h,addonPointer,addonClick,n,0);
  r.x=1030; r.w=750; gfx_cor(r,.5f,f==n+1?.26f:.14f,.17f,.19f,1);
  texto(TXT_ILHA_SUB,i18n("Conectar ou importar sua configuração"),1054,882,700,234);
  ponteiro_alvo(r.x,r.y,r.w,r.h,addonPointer,addonClick,n+1,0);
  texto(TXT_ILHA_SUB,i18n("Setas: navegar    OK: abrir    Voltar: retornar"),92,1020,1450,160);
}
void aioui_desenhar(Uint32 agora) {
  if(!aberto) return;
  (void)agora;
  gfx_cor((GfxRect){0,0,1920,1080},0,.035f,.065f,.077f,1);
  GLuint fundo=tex_obter_hero(arteFundo);
  if(fundo) gfx_textura((GfxRect){0,0,1920,1080},fundo);
  texto(TXT_ILHA_SUB,i18n("Ajustes / Addons / AIOStreams"),150,65,1400,196);
  time_t now=time(NULL); struct tm clock; localtime_r(&now,&clock); char hhmm[16]; strftime(hhmm,sizeof hhmm,"%H:%M",&clock);
  painel(1710,46,138,56); texto(TXT_ILHA_NOME,hhmm,1734,61,100,235);
  pthread_mutex_lock(&trava); int busy=ocupado; pthread_mutex_unlock(&trava);
  if(pendente || busy) texto(TXT_ILHA_SUB,i18n("Conferindo…"),1590,65,250,180);
  painel(72,46,56,56);
  gfx_icone((GfxRect){87,62,24,24},"pl_chevron-left",.85f,.88f,.9f,1);
  ponteiro_alvo(72,46,56,56,NULL,voltarClick,0,0);
  if(pagina<PAINEL) setup(); else workspace();
  texto(TXT_ILHA_SUB,i18n("Setas: navegar    OK: abrir    Voltar: retornar"),92,1020,1450,160);
  if(mensagem[0]) texto(TXT_ILHA_SUB,mensagem,453,934,1340,215);
  if(teclado_aberto()) teclado_desenhar(agora);
}
#ifdef NV_SHOT_HOOKS
void aioui_shot(int p,const AioSnapshot *s) {
  aberto=1; pagina=p; snap=*s; foco=p==TITULO?2:0; lateral=0; regraSelecionada=linkSelecionado=-1;
  snprintf(host,sizeof host,"%s",hosts[0]); qrAtual();
}
#endif
