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
static int catalogoSelecionado;
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
  RedeResposta r = {0}; const char *h[] = {auth,NULL};
  RedePedido p = {.url=u,.cabecalhos=auth?h:NULL,.prazo_ms=12000,.max_bytes=1024*1024,.seguir=0};
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
    if(body) { free(body); body=NULL; }
    if(s->valida && aio_autorizacao(p->base,auth,sizeof auth) && !strncmp(p->host,"https://",8)) {
      snprintf(url,sizeof url,"%s/api/v1/user",p->host); buscar(s,url,auth,2,&body);
      if(body) { aio_configuracao(s,body); memset(body,0,strlen(body)); free(body); body=NULL; }
      memset(auth,0,sizeof auth);
    }
    if(s->valida && p->id[0]) {
      char b[2048], encoded[512]; nv_addon_base(p->base,b,sizeof b);
      if(nv_addon_id(encoded,sizeof encoded,p->id) && NV_COUBE(nv_addon_url(url,sizeof url,b,"/stream/%s/%s.json",p->tipo,encoded),url)) {
        buscar(s,url,NULL,3,&body);
        if(body) { aio_resultados(s,body); memset(body,0,strlen(body)); free(body); }
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
  if(body && sessao_usuario()[0]) contacache_gravar(CC_ADDONS_APLICADOS,perfis_ativo_addons(),sessao_usuario(),body);
  jsw_livre(&w); memset(lista,0,sizeof lista);
}
static void fechar(void) {
  aberto=0; geracao++; pendente=0; celular_fechar();
  memset(&snap,0,sizeof snap); memset(base,0,sizeof base); alvo[0]=mensagem[0]=0;
}
void aioui_abrir(int i) {
  aberto=1; geracao++; foco=0; lateral=0; entrada=pendente=0; indice=i; alvo[0]=0; proximaConsulta=0;
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
static void mudar(int p) { pagina=p; foco=0; lateral=0; mensagem[0]=0; }
static void campo(int qual) {
  entrada=qual;
  teclado_abrir_com(qual==1?i18n("URL da instância"):qual==2?i18n("ID do título"):i18n("Link de instalação"),
    i18n("Cole pelo celular. O link pessoal permanece oculto."),qual==3?TECLADO_LONGO:TECLADO_MAX,
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789:/?&=._-%+@!~#",NULL);
  teclado_mascarar(qual==3);
}
static int nLinhas(void) {
  if(pagina==SEM) return 3;
  if(pagina==INSTANCIA) return 9;
  if(pagina==SERVICOS) return 2;
  if(pagina==IMPORTAR) return 2;
  if(pagina==PRONTO) return 2;
  if(pagina==PAINEL) return 9;
  if(pagina==CATALOGOS) return snap.catalogos+4;
  if(pagina==FONTES) return snap.addons+snap.servicos+3;
  if(pagina==LEGENDAS || pagina==LINKS) return 1;
  if(pagina==FILTROS) return 9;
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
    if(foco<7) { hostIdx=foco; snprintf(host,sizeof host,"%s",hosts[hostIdx]); qrAtual(); mudar(SERVICOS); }
    else if(foco==7) campo(1); else mudar(IMPORTAR);
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
  } else if(pagina==TITULO) {
    if(foco==0) campo(2); else if(foco==1) { geracao++; atualizarRede(0); }
    else snprintf(mensagem,sizeof mensagem,"%s",i18n("Ordem recebida; rejeições anteriores não são fornecidas pelo manifesto."));
  } else if(pagina==SAUDE && foco==0) { geracao++; atualizarRede(0); }
  else mudar(SERVICOS);
}
void aioui_evento(const SDL_Event *e) {
  if(!aberto) return;
  if(teclado_aberto()) { teclado_evento(e); return; }
  if(e->type!=SDL_KEYDOWN) return;
  SDL_Keycode k=e->key.keysym.sym;
  if(k==SDLK_ESCAPE || k==SDLK_AC_BACK || k==SDLK_BACKSPACE) {
    if(entrada==3) { celular_fechar(); entrada=0; return; }
    if(pagina==SEM || pagina==PAINEL) fechar();
    else mudar(snap.valida?PAINEL:SEM);
    return;
  }
  if(pagina==CATALOGOS && !lateral && foco<snap.catalogos && (k==SDLK_PAGEUP || k==SDLK_PAGEDOWN)) {
    int f=fileira(foco);
    if(f>=0) { fil_mover(f,k==SDLK_PAGEUP?-1:1); desc_remontar_fileiras(); }
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
      snap=entrega;
      for(int k=0;k<4;k++) if(snap.retry[k]>0) {
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
  else gfx_cor(r,32/h,.055f,.063f,.071f,.98f);
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
  texto(TXT_G28B,nome,x+94,y+ty,w-152,241);
  texto(TXT_G22M,sub,x+94,y+ty+38,w-152,174);
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
  painel(1440,302,350,570);
  texto(TXT_ILHA_SECAO,i18n("Edite pelo celular"),1464,330,302,238);
  bloco(i18n("Regras avançadas ficam no AIOStreams. Salve lá e atualize esta TV."),1464,378,290,3);
  qrDesenhar(configure,1464,483,190);
  bloco(i18n("Página oficial de configuração. Entre com sua própria conta."),1464,701,290,3);
  texto(TXT_ILHA_HORA,i18n("Credenciais ocultas"),1464,820,290,142);
}
static void setup(void) {
  painel(72,148,1776,824);
  GLuint logo=tex_obter(arteLogo);
  if(logo) gfx_rect((GfxRect){124,216,48,48},logo,GFX_MARCA,0,0,0,0,1,1,1,1);
  texto(TXT_G30B,"AIOStreams",199,206,440,242);
  texto(TXT_ILHA_SUB,i18n("Um addon, todas as suas fontes"),199,248,440,174);
  gfx_cor((GfxRect){661,156,1,815},0,.8f,.84f,.88f,.08f);
  static const char *const story[]={"Um lugar.\nTodas as fontes.","Escolha onde\nele roda.","Sua configuração.\nNesta TV.","Configure uma vez.\nAssista do seu jeito.","Conectado.\nPronto para explorar."};
  float storyHeight=txt_bloco(TXT_V2_A3TIT,i18n(story[pagina]),240,241,242,124,345,490,56,1,4);
  bloco(i18n("Reúna addons, catálogos e legendas. Configure os detalhes no celular e acompanhe tudo nesta TV."),124,345+storyHeight+34,485,5);
  const char *steps[]={i18n("Escolha sua instância"),i18n("Conecte e configure"),i18n("Valide nesta TV")};
  for(int i=0;i<3;i++) {
    gfx_icone((GfxRect){125,800+i*49,24,24},i==2&&pagina==PRONTO?"aj_check":"aj_circle-dot",.7f,.76f,.79f,1);
    texto(TXT_ILHA_SUB,steps[i],169,800+i*49,450,195);
  }
  texto(TXT_ILHA_PERGUNTA,pagina==SEM?i18n("Como deseja conectar?"):i18n(titulos[pagina]),716,204,1080,240);
  if(pagina==SEM) {
    linha(0,"aj_globe",i18n("Usar uma instância pública"),i18n("Escolha um servidor listado na documentação."),716,310,1078);
    linha(1,"aj_server",i18n("Conectar meu servidor"),i18n("Informe a origem do seu AIOStreams."),716,468,1078);
    linha(2,"aj_smartphone",i18n("Importar configuração existente"),i18n("Traga seu link de instalação."),716,626,1078);
    bloco(i18n("Debrid é opcional. HTTP, P2P e canais dependem da instância e desta TV."),716,774,1040,4);
  } else if(pagina==INSTANCIA) {
    int start=foco>4?foco-4:0;
    for(int i=start;i<7 && i<start+5;i++) linha(i,"aj_globe",nomes[i],i==2?i18n("Pública: sem P2P, HTTP ou canais ao vivo"):i==3?i18n("Versão de desenvolvimento"):i18n("Estável · Servidor comunitário"),716,310+(i-start)*100,1078);
    botao(7,i18n("Meu servidor"),940,866,320); botao(8,i18n("Já tenho o link"),1280,866,350);
  } else if(pagina==SERVICOS) {
    const char *check[]={i18n("1. Serviços: debrid ou Usenet, opcionais"),i18n("2. Addons: escolha suas fontes e catálogos"),i18n("3. Filtros e ordenação: revise suas regras"),i18n("4. Formatador: nomes e descrições das fontes")};
    for(int i=0;i<4;i++) texto(TXT_ILHA_NOME,check[i],716,310+i*70,1050,230);
    qrDesenhar(configure,716,612,176); bloco(i18n("Salvar e instalar no AIOStreams. Depois copie o link do manifesto."),920,627,820,4);
    botao(0,i18n("Tenho meu link de instalação"),940,842,480); botao(1,i18n("Instância"),1440,842,300);
  } else if(pagina==IMPORTAR) {
    if(entrada!=3) qrDesenhar(configure,716,305,210);
    bloco(i18n("Abra a configuração oficial, salve e copie o link de instalação. Envie pelo pareamento já usado pelo Nuvio."),970,314,790,5);
    bloco(i18n("Links pessoais permanecem ocultos. Exporte ou importe arquivos JSON pelo celular."),716,563,1040,3);
    if(entrada==3) qrDesenhar(celular_url(),716,305,210);
    botao(0,i18n("Enviar pelo celular"),940,842,380); botao(1,i18n("Colar link"),1340,842,350);
  } else {
    gfx_icone((GfxRect){940,312,68,68},"aj_circle-check",.65f,.84f,.83f,1);
    texto(TXT_G30B,i18n("AIOStreams adicionado"),940,410,780,240);
    bloco(i18n("Manifesto reconhecido. A validação não garante a reprodução de cada fonte."),940,485,790,3);
    bloco(i18n("Instalação local nesta TV. A sincronização da conta pode substituir a lista."),940,608,790,3);
    botao(0,i18n("Abrir painel"),940,842,370); botao(1,i18n("Escolher catálogos"),1330,842,410);
  }
}
static void workspace(void) {
  painel(72,148,315,824); painel(411,148,1437,824);
  texto(TXT_ILHA_NOME,"AIOStreams",110,185,250,240);
  texto(TXT_ILHA_SUB,i18n("Seu addon conectado"),110,222,250,163);
  const char *nav[]={i18n("Visão geral"),i18n("Catálogos"),i18n("Fontes e serviços"),i18n("Links"),i18n("Legendas"),i18n("Filtros e ordenação"),i18n("Saúde")};
  for(int i=0;i<7;i++) {
    float y=265+i*70;
    if((lateral && foco==i) || pagina==PAINEL+i) gfx_cor((GfxRect){91,y,277,64},.32f,.15f,.17f,.18f,1);
    gfx_icone((GfxRect){111,y+19,25,25},icons[i],.77f,.82f,.84f,1);
    texto(TXT_ILHA_SUB,nav[i],150,y+19,210,220);
    ponteiro_alvo(91,y,277,64,pointer,click,i,1);
  }
  texto(TXT_ILHA_HORA,snap.configurada?i18n("Configuração conectada"):i18n("Somente manifesto"),108,850,256,175);
  texto(TXT_ILHA_HORA,host,108,891,252,134);
  texto(TXT_ILHA_TITULO,i18n(titulos[pagina]),453,184,1290,243);
  bloco(pagina==TITULO?i18n("Todos os resultados recebidos, na ordem do AIOStreams."):i18n("Sua configuração, com cada recurso no seu lugar."),453,245,1240,1);
  if(pagina==PAINEL) {
    texto(TXT_ILHA_NOME,host,518,310,1160,236);
    texto(TXT_ILHA_SUB,snap.configurada?i18n("Configuração conectada"):i18n("Somente manifesto"),518,350,1160,170);
    const char *sub[]={i18n("Catálogos publicados pelo manifesto"),i18n("Addons, serviços, cache e P2P"),i18n("Destinos externos e instalação"),i18n("Separadas das faixas dentro do vídeo"),i18n("Resumo somente leitura · Edite pelo celular"),i18n("Instância, latência e erros observados")};
    for(int i=0;i<6;i++) linha(i,icons[i+1],nav[i+1],sub[i],453+(i%2)*700,398+(i/2)*113,680);
    botao(6,i18n("Inspecionar título"),453,800,420); botao(7,i18n("Atualizar"),895,800,300); botao(8,i18n("Reconectar"),1217,800,350);
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
  } else if(pagina==FILTROS) {
    const char *names[]={i18n("Resolução e qualidade"),i18n("Tamanho e bitrate"),i18n("Idiomas"),i18n("Cache e tipos de fonte"),i18n("Regex e palavras-chave"),i18n("Ordenação"),i18n("Formatador"),i18n("Proxy")};
    for(int i=start;i<8 && i<start+5;i++) {
      char sub[128];
      if(!snap.configurada) snprintf(sub,sizeof sub,"%s",i18n("Conecte a configuração para ver esta regra"));
      else if(i==0) snprintf(sub,sizeof sub,i18n("Exigidas: %s · Excluídas: %s"),snap.resolucaoObrigatoria[0]?snap.resolucaoObrigatoria:i18n("Não informado"),snap.resolucaoExcluida[0]?snap.resolucaoExcluida:i18n("Não informado"));
      else if(i==3) snprintf(sub,sizeof sub,i18n("Excluir cache: %s · Excluir sem cache: %s"),snap.regraCache<0?i18n("Não informado"):snap.regraCache?i18n("Sim"):i18n("Não"),snap.regraUncached<0?i18n("Não informado"):snap.regraUncached?i18n("Sim"):i18n("Não"));
      else if(snap.resumo[i][0]) snprintf(sub,sizeof sub,"%s",snap.resumo[i]);
      else snprintf(sub,sizeof sub,i18n("%d campos informados · Somente leitura"),snap.regras[i]);
      linha(i,"aj_sliders-horizontal",names[i],sub,453,310+(i-start)*104,950);
    }
    botao(8,i18n("Editar no celular"),453,866,440);
  } else if(pagina==LEGENDAS) {
    texto(TXT_ILHA_NOME,snap.recursos&AIO_LEGENDA?i18n("Recurso de legendas publicado"):i18n("Recurso de legendas não publicado"),473,324,920,233);
    bloco(i18n("Addons de legenda e faixas embutidas são diferentes. Resultados variam por título, arquivo e hash do vídeo."),473,399,870,4);
    int y=535;
    for(int i=0;i<snap.addons && y<720;i++) if(snap.addon[i].legenda) {
      texto(TXT_ILHA_NOME,snap.addon[i].nome,473,y,870,230); y+=42;
    }
    char languages[320]; snprintf(languages,sizeof languages,i18n("Principal: %s · Secundária: %s"),i18n(ling_nome(ling_legenda())),i18n(ling_nome(ling_legenda2())));
    texto(TXT_ILHA_SUB,languages,473,760,870,180);
    botao(0,i18n("Gerenciar legendas no celular"),453,866,540);
  } else if(pagina==SAUDE) {
    const char *names[]={i18n("Manifesto"),i18n("Status da instância"),i18n("Configuração"),i18n("Consulta de fontes")};
    for(int i=0;i<4;i++) { char sub[160]; snprintf(sub,sizeof sub,"HTTP %d · %d ms",snap.http[i],snap.ms[i]); linha(i,"aj_activity",names[i],sub,453,310+i*110,950); }
    bloco(i18n("Cota restante não informada. Nenhuma sondagem de reprodução. Em HTTP 429, aguarde antes de atualizar."),473,764,880,3);
    botao(0,i18n("Atualizar"),453,866,350); botao(1,i18n("Configuração no celular"),825,866,530);
  } else if(pagina==TITULO) {
    botao(0,i18n("Escolher título"),453,302,400); botao(1,i18n("Atualizar"),875,302,340);
    texto(TXT_ILHA_HORA,i18n("AIO # / Fonte recebida"),473,376,900,148);
    int first=foco>9?foco-9:0;
    for(int i=first;i<snap.resultados && i<first+8;i++) {
      float y=411+(i-first)*58;
      if(!lateral && foco==i+2) gfx_cor((GfxRect){453,y,960,56},.3f,.18f,.2f,.21f,1);
      char nome[224],sub[160]; snprintf(nome,sizeof nome,"%d. %s",i+1,snap.resultado[i].nome[0]?snap.resultado[i].nome:i18n("Fonte sem nome"));
      snprintf(sub,sizeof sub,"%s · %s",i18n(snap.resultado[i].tipo),snap.resultado[i].cache<0?i18n("Cache desconhecido"):snap.resultado[i].cache?i18n("Em cache"):i18n("Fora do cache"));
      texto(TXT_ILHA_SECAO,nome,473,y+2,920,239); texto(TXT_ILHA_NUM,sub,505,y+31,880,165);
      ponteiro_alvo(453,y,960,56,pointer,click,i+2,0);
    }
    painel(1440,302,350,570);
    texto(TXT_ILHA_SECAO,i18n("Decisão Nuvio"),1464,330,300,235);
    bloco(i18n("Ordem e regras dependem desta TV. A inspeção não abre links para verificar reprodução."),1464,383,296,5);
    int selected=foco>=2?foco-2:0;
    if(selected<snap.resultados) {
      AioResultado *r=&snap.resultado[selected];
      bloco(!strcmp(r->tipo,"Externo")||!strcmp(r->tipo,"NZB")||!strcmp(r->tipo,"YouTube")?i18n("Destino externo: fora do player da TV."):r->cache==0?i18n("Fora do cache: escolha manual."):!strcmp(r->tipo,"P2P")&&!ajustes_p2p_ligado()?i18n("P2P desligado nesta TV."):i18n("Resultado recebido. Compatibilidade ainda não verificada."),1464,560,296,5);
    }
    if(!snap.resultados) bloco(i18n("Escolha um ID de título para consultar as fontes. Nenhum link de reprodução será aberto."),473,420,850,4);
    bloco(i18n("Ordem recebida; rejeições anteriores não são fornecidas pelo manifesto."),473,900,900,2);
  } else {
    texto(TXT_ILHA_NOME,i18n("Links externos e instalação"),473,324,900,235);
    bloco(i18n("Links externos, YouTube e NZB não são vídeos HTTP reproduzíveis. Abra e gerencie pelo celular."),473,397,850,4);
    char sub[128]; snprintf(sub,sizeof sub,i18n("%d links na última consulta de título"),snap.links); texto(TXT_ILHA_SUB,sub,473,570,900,180);
    botao(0,i18n("Gerenciar no celular"),453,866,480);
  }
}
static void addonPointer(int i,int b) { (void)b; addonsui_ponteiro(i,0); }
static void addonClick(int i,int b) {
  addonPointer(i,b); SDL_Event e={0}; e.type=SDL_KEYDOWN; e.key.keysym.sym=SDLK_RETURN; addonsui_evento(&e);
}
void aioui_addons_desenhar(int f) {
  gfx_cor((GfxRect){0,0,1920,1080},0,.035f,.065f,.077f,1);
  GLuint fundo=tex_obter_hero(arteFundo); if(fundo) gfx_textura((GfxRect){0,0,1920,1080},fundo);
  painel(72,148,315,824); painel(411,148,1437,824);
  texto(TXT_ILHA_SUB,i18n("Ajustes / Addons / AIOStreams"),150,65,1400,196);
  texto(TXT_ILHA_SECAO,i18n("Ajustes"),111,190,240,180);
  texto(TXT_ILHA_NOME,i18n("Addons"),111,395,240,240);
  texto(TXT_ILHA_TITULO,i18n("Addons"),453,230,1200,240);
  texto(TXT_ILHA_SUB,i18n("Ligados nesta TV"),453,290,1200,170);
  int n=addons_n(), start=f>4?f-4:0;
  for(int i=start;i<n && i<start+5;i++) {
    float y=360+(i-start)*105;
    if(i==f) gfx_cor((GfxRect){453,y,1340,94},.23f,.18f,.20f,.21f,1);
    gfx_icone((GfxRect){477,y+28,28,28},aioui_detectar(i)?"aj_layers":"aj_puzzle",.85f,.88f,.9f,1);
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
  aberto=1; pagina=p; snap=*s; foco=0; lateral=0;
  snprintf(host,sizeof host,"%s",hosts[0]); qrAtual();
}
#endif
