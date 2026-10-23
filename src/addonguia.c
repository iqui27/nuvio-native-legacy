#include "addonguia.h"
#include "addons.h"
#include "addonsui.h"
#include "ajustes.h"
#include "botoes.h"
#include "celbotao.h"
#include "debrid.h"
#include "dados.h"
#include "gfx.h"
#include "idioma.h"
#include "p2p.h"
#include "p2pmotor.h"
#include "perfis.h"
#include "recomenda.h"
#include "idiomacod.h"
#include <time.h>
#include "ponteiro.h"
#include "player.h"
#include "ilha.h"
#include "menu.h"
#include "nuvem.h"
#include "sync.h"
#include "tex_cache.h"
#include "redesaude.h"
#include "sessao.h"
#include "streams.h"
#include "text.h"
#include "addonurl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int tela=-1, anterior=AG_MODO, indice, foco, pedido, detalhes, pct=-1, serv, acessoConfig;
static int modoSelecionado;
static uint64_t identidade;
static char nome[96], titulo[160], servico[32];
static const char *const MODOS[]={"P2P nesta TV","Serviço de debrid","Só links diretos"};
static const char *const ICONES[]={"aj_tv","aj_cloud","aj_link"};
static const char *const DESCR[]={"Grátis · pode demorar a começar.","Mais rápido se pronto · precisa de conta.","Sem P2P · torrents ficam ocultos."};
static const char *const SERVICOS[]={"Real-Debrid","TorBox","Premiumize","AllDebrid"};
static const char *const CHAVES[]={"realdebrid","torbox","premiumize","alldebrid"};
static struct { GfxRect r; int id; } alvos[20];
static int nAlvos;
#ifdef NV_SHOT_HOOKS
static int shot;
#endif
static int localDisponivel(void) {
#ifdef NV_SHOT_HOOKS
  if(shot)return 1;
#endif
  return p2pmotor_disponivel();
}
static int proxima(void) { return stream_proxima_vod(stream_atual())>=0; }
static void texto(TxtEstilo estilo,const char *s,float x,float y,float w,float a) {
  TxtLinha l=txt_linha_corta(estilo,s,243,242,239,255,w); txt_desenhar_alpha(l,x,y,a);
}
static void bloco(const char *s,float x,float y,float w,float a,int linhas) {
  txt_bloco(TXT_ILHA_CORPO,s,243,242,239,x,y,w,35.0f,a,linhas);
}
static void icone(const char *s,float x,float y,float lado,float a) {
  gfx_icone((GfxRect){x,y,lado,lado},s,0.95f,0.95f,0.94f,a);
}
static void painel(GfxRect r,float raio) {
  if(ajustes_vidro()) {
    gfx_vidro_painel(r,raio/r.h,0.94f,1);
    gfx_cor(r,raio/r.h,0.055f,0.059f,0.071f,0.74f);
  }
  else gfx_cor(r,raio/r.h,0.071f,0.075f,0.086f,0.98f);
}
static void apontar(int id,int unused) { (void)unused; foco=id;if(tela==AG_MODO&&id>=0&&id<3)modoSelecionado=id+1; }
static void alvo(GfxRect r,int id) {
  if(nAlvos<20) {alvos[nAlvos].r=r;alvos[nAlvos++].id=id;}
  if(tela>=0)ponteiro_alvo(r.x,r.y,r.w,r.h,apontar,NULL,id,0);
}
static float botaoEm(const char *rot,const char *ic,float x,float y,int id,int pequeno,int primario) {
  TxtEstilo estilo=pequeno?TXT_G18M:TXT_G20B;
  TxtLinha l=txt_linha(estilo,rot,243,242,239,255);
  float pad=pequeno?22:26,h=pequeno?52:60,w=l.w+pad*2+(ic?35:0);
  if(x<0)x=-x-w;
  GfxRect r={x,y,w,h};
  if(foco==id||primario)gfx_cor(r,0.5f,236/255.0f,64/255.0f,122/255.0f,1);
  else gfx_cor(r,0.5f,1,1,1,0.08f);
  if(ic)gfx_icone((GfxRect){x+pad,y+(h-23)/2,23,23},ic,0.95f,0.95f,0.94f,1);
  txt_desenhar(l,x+pad+(ic?35:0),y+(h-l.h)/2);
  if(id<50)alvo(r,id); return w;
}
static float botao(const char *rot,const char *ic,float x,float y,int id) {
  return botaoEm(rot,ic,x,y,id,0,0);
}
static void kicker(const char *s,float x,float y) {
  char up[240];idioma_maiusc_em(ajustes_idioma(),up,sizeof up,s);
  txt_tracking(TXT_MINI,up,243,242,239,x,y,0.46f,2.1f);
}
static float chip(const char *s,float x,float y,int pequeno) {
  TxtEstilo estilo=pequeno?TXT_ILHA_HORA:TXT_G18R;
  TxtLinha l=txt_linha(estilo,s,243,242,239,255);
  float pad=pequeno?10:15,h=pequeno?26:42;
  gfx_cor((GfxRect){x,y,l.w+pad*2,h},0.5f,1,1,1,pequeno?0.043f:0.07f);
  txt_desenhar_alpha(l,x+pad,y+(h-l.h)/2,pequeno?0.64f:0.8f);
  return l.w+pad*2+(pequeno?9:10);
}
static void capacidades(int i,float x,float y,int pequeno) {
  int d,t;addonmodo_fatos(addonmodo_id(addons_base(i)),NULL,NULL,&d,&t);
  const char *labels[]={"Fontes para tocar","Catálogos","Fichas","Legendas"};
  const int recursos[]={ADD_STREAM,ADD_CATALOGO,ADD_META,ADD_LEGENDA};
  if(!addons_sondado(i)){chip(i18n("conferindo o manifesto…"),x,y,pequeno);return;}
  for(int a=0;a<4;a++)if(addons_fornece(i,recursos[a]))x+=chip(i18n(labels[a]),x,y,pequeno);
  if(t)x+=chip(i18n("Torrent / P2P"),x,y,pequeno);
  if(d)chip(i18n("Links diretos"),x,y,pequeno);
}
static const char *papelIcone(int i,int falhou) {
  int d,t;addonmodo_fatos(addonmodo_id(addons_base(i)),NULL,NULL,&d,&t);
  return falhou?"aj_unplug":addons_sondado(i)&&!addons_fornece(i,ADD_STREAM)?"aj_layout-grid":d&&!t?"aj_cloud":"aj_puzzle";
}
static void linha(float x,float y,float w) {gfx_cor((GfxRect){x,y,w,1},0,1,1,1,0.08f);}
static void pergunta(char *s,size_t n) {snprintf(s,n,i18n("Como o %s vai tocar?"),nome);}
static void rotas(float x,float y,float w) {
  char s[180]; pergunta(s,sizeof s);
  linha(x,y,w); texto(TXT_ILHA_SECAO,s,x,y+22,w,1);
  TxtLinha estado=txt_linha(TXT_G18R,addonmodo_get(identidade)==AM_AUTO?i18n("Nenhum modo ativo · escolher"):i18n("Mudar o modo"),243,242,239,255);
  txt_desenhar_alpha(estado,x+w-estado.w-22,y+24,0.6f);
  icone("aj_chevron-right",x+w-16,y+28,16,0.6f);
  float cw=(w-24)/3;
  for(int i=0;i<3;i++) {
    GfxRect r={x+i*(cw+12),y+60,cw,92};
    gfx_cor(r,20/r.h,1,1,1,foco==10+i||addonmodo_get(identidade)==i+1?0.14f:0.035f);
    icone(ICONES[i],r.x+18,r.y+20,21,0.9f);
    texto(TXT_G20B,i18n(i==0&&ajustes_p2p_url()[0]?"Servidor P2P":MODOS[i]),r.x+48,r.y+16,cw-58,1);
    texto(TXT_G18R,i18n(DESCR[i]),r.x+18,r.y+50,cw-30,0.64f);
    if(addonmodo_get(identidade)==i+1) {
      float ar,ag,ab;ajustes_acento(&ar,&ag,&ab);
      gfx_cor((GfxRect){r.x+cw-18,r.y+12,6,6},0.5f,ar,ag,ab,1);
    }
    alvo(r,10+i);
  }
}
/* Public configurator only: never put installation credentials in a QR. */
static int configUrl(char *out,size_t n) {
  const char *base=addons_base(indice), *p=strstr(base,"://");
  if(!p || strncmp(base,"https://",8))return 0;
  const char *end=p+3+strcspn(p+3,"/?#");
  if(memchr(p+3,'@',(size_t)(end-p-3)))return 0;
  return snprintf(out,n,"%.*s/configure",(int)(end-base),base)<(int)n;
}
void addonguia_abrir(int t,int addon,const char *tit) {
  tela=t; indice=addon; foco=0;detalhes=0;anterior=AG_MODO;acessoConfig=0;
  modoSelecionado=addonmodo_get(addonmodo_id(addons_base(addon)));
  if(modoSelecionado<AM_P2P)modoSelecionado=AM_P2P;
  identidade=addonmodo_id(addons_base(addon));
  snprintf(nome,sizeof nome,"%s",addons_nome(addon));
  snprintf(titulo,sizeof titulo,"%s",tit&&*tit? tit:addons_nome(addon));
}
void addonguia_configurar(int addon) {
  addonguia_abrir(AG_NOVO_DIRETO,addon,NULL);acessoConfig=1;
}
int addonguia_aberto(void) {return tela>=0;}
void addonguia_fechar(void) {tela=-1;celb_fechar_dono(CELB_ADDON);}
int addonguia_pedido(void) {int p=pedido;pedido=0;return p;}
uint64_t addonguia_identidade(void) {return identidade;}
void addonguia_progresso(const char *s,int p) {snprintf(servico,sizeof servico,"%s",s?s:"");pct=p;}
static void entregar(int p) {pedido=p;addonguia_fechar();}
static int escolher(int modo) {
  if(modo==AM_P2P && !localDisponivel() && !ajustes_p2p_url()[0])return 0;
  if(!addonmodo_set(identidade,modo))return 0;
  if(modo==AM_P2P)ajustes_definir_p2p_ligado(1);
  return 1;
}
static void conectar(void) {anterior=tela;tela=AG_CONECTAR;foco=0;}
static void ativar(void) {
  if(foco==20){detalhes=!detalhes;return;}
  if(foco>=10 && foco<=12){tela=AG_MODO;modoSelecionado=foco-9;foco-=10;return;}
  if(foco==30){anterior=tela;tela=AG_NOVO_DIRETO;acessoConfig=1;foco=0;return;}
  if(tela==AG_MODO) {
    if(foco==41){conectar();return;}
    if(foco==42){if(escolher(AM_DIRETO))entregar(player_aberto()?AGP_FONTES:AGP_NADA);return;}
    if(foco==40)foco=modoSelecionado-1;
    if(foco<=2)modoSelecionado=foco+1;
    if(foco==1 && !debrid_ativo()){conectar();return;}
    if(foco<=2 && escolher(foco+1))entregar(AGP_REPETIR);
    return;
  }
  if(tela==AG_CONECTAR) {
    if(foco>=0 && foco<4){
      serv=foco;
      if(celb_disponivel())celb_abrir_embutido(CELB_ADDON,"Conectar pelo celular");
      else if(foco<3)sync_manual_addons();
    }
    else {tela=anterior;foco=0;celb_fechar_dono(CELB_ADDON);}
    return;
  }
  if(foco==1){entregar(tela>=AG_NOVO_TORRENT&&tela<=AG_NOVO_DIRETO?AGP_NADA:AGP_FONTES);return;}
  if(tela==AG_NOVO_CATALOGO){entregar(AGP_FILEIRAS);return;}
  if(tela==AG_NOVO_DIRETO){if(foco==0)entregar(AGP_NADA);return;}
  if(tela==AG_NOVO_TORRENT || tela==AG_P2P){
    if(tela==AG_NOVO_TORRENT&&!player_aberto()){tela=AG_MODO;foco=0;return;}
    if(escolher(AM_P2P))entregar(AGP_REPETIR);else conectar();return;
  }
  if(tela==AG_401){anterior=tela;tela=AG_NOVO_DIRETO;acessoConfig=1;foco=0;return;}
  if(tela==AG_INICIANDO){entregar(AGP_FONTES);return;}
  entregar(tela!=AG_VAZIO&&proxima()?AGP_PROXIMA:AGP_REPETIR);
}
void addonguia_evento(const SDL_Event *e) {
  if(tela<0||e->type!=SDL_KEYDOWN)return;
  SDL_Keycode k=e->key.keysym.sym;
  if(k==SDLK_ESCAPE||k==SDLK_AC_BACK||k==SDLK_BACKSPACE){
    if(tela==AG_CONECTAR){tela=anterior;foco=0;celb_fechar_dono(CELB_ADDON);}
    else addonguia_fechar();return;
  }
  if(k==SDLK_RETURN||k==SDLK_KP_ENTER){ativar();return;}
  int atual=-1;for(int i=0;i<nAlvos;i++)if(alvos[i].id==foco)atual=i;
  if(atual<0)return;
  GfxRect r=alvos[atual].r;float best=1e20f;int novo=foco;
  for(int i=0;i<nAlvos;i++) {
    float dx=alvos[i].r.x+alvos[i].r.w/2-r.x-r.w/2,dy=alvos[i].r.y+alvos[i].r.h/2-r.y-r.h/2;
    if((k==SDLK_LEFT&&dx>=-1)||(k==SDLK_RIGHT&&dx<=1)||(k==SDLK_UP&&dy>=-1)||(k==SDLK_DOWN&&dy<=1))continue;
    if(k!=SDLK_LEFT&&k!=SDLK_RIGHT&&k!=SDLK_UP&&k!=SDLK_DOWN)continue;
    float d=(k==SDLK_LEFT||k==SDLK_RIGHT)?dx*dx+dy*dy*6:dy*dy+dx*dx*6;
    if(d<best){best=d;novo=alvos[i].id;}
  }
  foco=novo;
  if(tela==AG_MODO&&foco>=0&&foco<3)modoSelecionado=foco+1;
}
void addonguia_atualizar(int permitir) {
  addonmodo_preparar(perfis_dono()[0]?perfis_dono():sessao_usuario(),perfis_ativo_addons());
  if(tela==AG_CONECTAR) {
    if(!celb_disponivel()&&debrid_ativo()) {tela=AG_MODO;foco=1;}
    char chave[256];
    if(celb_pegar(CELB_ADDON,chave,sizeof chave)) {
      /* Same local credential store as Settings; input comes only from phone. */
      int ok=ajustes_debrid_celular(CHAVES[serv],chave);
      memset(chave,0,sizeof chave);
      if(ok && addonmodo_set(identidade,AM_DEBRID)){tela=AG_MODO;foco=1;}
    }
  }
  if(tela>=0||!permitir)return;
  for(int i=0;i<addons_n();i++) {
    uint64_t id=addonmodo_id(addons_base(i));
    if(!addons_ativo(i)||!addons_sondado(i)||addonmodo_visto(id))continue;
    int h,v,d,t;addonmodo_fatos(id,&h,&v,&d,&t);
    int nova=!addons_fornece(i,ADD_STREAM)?AG_NOVO_CATALOGO:d&&!t?AG_NOVO_DIRETO:AG_NOVO_TORRENT;
    if(addonmodo_marcar(id))addonguia_abrir(nova,i,NULL);
    return;
  }
}
void addonguia_falha(void) {
  if(tela>=0)return;
  int i=stream_ordem_addon(stream_atual());if(i<0)i=0;
  const Stream *s=stream_item(stream_atual());
  if(s && s->addonIdentidade)for(int a=0;a<addons_n();a++)
    if(addonmodo_id(addons_base(a))==s->addonIdentidade){i=a;break;}
  int h=0,v=0,d=0,t=0,vazio=1,consultados=0,p=0;
  for(int a=0;a<addons_n();a++) {
    if(!addons_ativo(a)||!addons_fornece(a,ADD_STREAM))continue;
    int ah,av,ad,at;uint64_t id=addonmodo_id(addons_base(a));addonmodo_fatos(id,&ah,&av,&ad,&at);
    consultados++;
    if(!av||ad||at)vazio=0;
    if(at&&!p2p_ativo()&&!debrid_ativo()&&addonmodo_get(id)!=AM_DIRETO){p=1;i=a;}
    if(ah==401||ah==403){h=ah;i=a;}
  }
  addonmodo_fatos(addonmodo_id(addons_base(i)),NULL,&v,&d,&t);
  int hdr=s && stream_incompativel(stream_atual());
  int semResposta=!v&&!h&&!s;
  if(s && s->url[0] && !h && !hdr)semResposta=0;
  addonguia_abrir(addonguia_razao(h,semResposta,p,0,hdr,vazio&&consultados>0),i,NULL);
}
static void detalhe(float x,float y,float w) {
  icone("aj_info",x,y+4,19,0.6f);
  TxtLinha l=txt_linha(TXT_G18R,i18n("Ver detalhes"),243,242,239,255);
  texto(TXT_G18R,i18n("Ver detalhes"),x+28,y,w-58,foco==20?1:0.6f);
  icone(detalhes?"aj_chevron-up":"aj_chevron-down",x+36+l.w,y+4,18,0.6f);
  alvo((GfxRect){x,y,l.w+62,32},20);
  if(detalhes) {
    painel((GfxRect){x,y+38,w,92},20);
    texto(TXT_ILHA_SUB,i18n("A escolha do Nuvio não muda a configuração do addon."),x+18,y+52,w-36,0.7f);
    char s[180];int h,v,d,t;addonmodo_fatos(identidade,&h,&v,&d,&t);
    snprintf(s,sizeof s,i18n("HTTP %d · %d links diretos · %d torrents"),h,d,t);
    texto(TXT_G18R,s,x+18,y+84,w-36,0.6f);
  }
}
static void qrPequeno(const char *url,float x,float y,float lado) {
  gfx_transformar(x,y,lado/celb_qr_lado(),0,0);
  celb_qr_desenhar(url,x,y,1);
  gfx_sem_transformar();
}
static void configuracao(float x,float y,float w) {
  char u[512];painel((GfxRect){x,y,w,152},24);
  if(configUrl(u,sizeof u))qrPequeno(u,x+20,y+20,112);
  texto(TXT_ILHA_NOME,i18n("Seu addon já tem debrid configurado?"),x+156,y+24,w-180,1);
  texto(TXT_ILHA_TEXTO,i18n("Se ele entrega links prontos, o Nuvio toca direto. Não precisa escolher aqui."),x+156,y+65,w-180,0.64f);
  texto(TXT_ILHA_TEXTO,i18n("Para mudar o serviço, abra o site do próprio addon pelo QR."),x+156,y+96,w-180,0.64f);
}
static void moldura(int pagina) {
  if(pagina)gfx_cor((GfxRect){0,0,1920,1080},0,0.045f,0.047f,0.057f,1);
  time_t agora=time(NULL);struct tm hora;localtime_r(&agora,&hora);
  char relogio[8];strftime(relogio,sizeof relogio,"%H:%M",&hora);
  painel((GfxRect){48,36,170,56},28);
  texto(TXT_ILHA_NOME,relogio,72,48,86,1);
  gfx_cor((GfxRect){155,53,1,22},0,1,1,1,0.15f);
  const ContaPerfil *perfil=perfis_item_ativo();
  rec_avatar_estilo((GfxRect){172,47,34,34},perfil?perfil->avatarUrl:"",perfil?perfil->nome:"?",perfil?perfil->nome:"?",1,TXT_G16B);
  texto(TXT_ILHA_NOME,"nuvio",1775,44,85,0.64f);
  if(pagina) {
    painel((GfxRect){48,112,88,428},44);
    const char *icons[]={"aj_house","aj_compass","aj_search","aj_library","aj_user-round","aj_sliders-horizontal"};
    gfx_cor((GfxRect){62,466,60,60},0.5f,1,1,1,0.08f);
    for(int i=0;i<6;i++)icone(icons[i],80,140+i*68,24,i==5?1:0.64f);
    gfx_cor((GfxRect){89,512,5,5},0.5f,236/255.0f,64/255.0f,122/255.0f,1);
  }
  icone("aj_arrow-left",200,1014,20,0.46f);
  texto(TXT_G18R,i18n("Voltar"),230,1011,300,0.46f);
  const char *rodape=pagina==2?i18n("Estado da última consulta · mude o modo de tocar quando quiser"):i18n("A escolha do Nuvio não muda a configuração do addon.");
  TxtLinha l=txt_linha(TXT_G18R,rodape,243,242,239,255);
  txt_desenhar_alpha(l,1804-l.w,1011,0.46f);
}
static void modoDesenhar(void) {
  char s[180];pergunta(s,sizeof s);
  snprintf(s,sizeof s,"%s › %s",i18n("Meus addons"),nome);
  kicker(s,200,134);
  gfx_cor((GfxRect){1572,148,65,4},0.5f,236/255.0f,64/255.0f,122/255.0f,1);
  texto(TXT_G18R,i18n("Escolher como tocar"),1645,138,160,0.64f);
  pergunta(s,sizeof s);
  texto(TXT_LOG_N44,s,200,167,1600,1);
  bloco(i18n("O addon encontrou o vídeo. O Nuvio cuida de como ele chega até você."),200,232,1600,0.64f,2);
  texto(TXT_ILHA_TEXTO,i18n("Para estas fontes, escolha P2P nesta TV ou um serviço conectado ao Nuvio."),200,270,1600,0.64f);
  for(int i=0;i<3;i++) {
    GfxRect r={200+i*540,322,518,416};
    gfx_cor(r,28/r.h,1,1,1,foco==i||modoSelecionado==i+1?0.14f:0.032f);alvo(r,i);
    kicker(i18n(i==0?"Nesta TV":i==1?"Pela sua conta":"Outras fontes"),r.x+28,r.y+24);
    icone(ICONES[i],r.x+28,r.y+60,36,0.8f);
    texto(TXT_G28B,i18n(i==0&&ajustes_p2p_url()[0]?"Servidor P2P":MODOS[i]),r.x+28,r.y+116,460,1);
    const char *descricao[]={"Grátis. O começo pode demorar mais, conforme quem compartilha.","Mais rápido se o vídeo já estiver pronto. Precisa de conta e plano.","Sem P2P. Mantém apenas fontes com link pronto."};
    txt_bloco(TXT_ILHA_TEXTO,i18n(descricao[i]),243,242,239,r.x+28,r.y+164,390,28.0f,0.64f,3);
    float hy=r.y+236;
    if(addonmodo_get(identidade)==i+1) {
      gfx_icone((GfxRect){r.x+28,hy+2,22,22},"aj_check",236/255.0f,64/255.0f,122/255.0f,1);
      TxtLinha l=txt_linha(TXT_G18R,i18n("Em uso nesta TV"),236,64,122,255);
      txt_desenhar(l,r.x+60,hy);hy+=44;
    }
    if(i==0) {
      texto(TXT_G18R,i18n("Recebe o começo e continua durante o vídeo."),r.x+28,hy,460,0.46f);
      if(!localDisponivel()&&!ajustes_p2p_url()[0])texto(TXT_G18R,i18n("Indisponível nesta TV"),r.x+28,hy+38,440,0.46f);
    } else if(i==1) {
      texto(TXT_G18R,"Real-Debrid · TorBox",r.x+28,hy,460,0.46f);
      texto(TXT_G18R,"Premiumize · AllDebrid",r.x+28,hy+24,460,0.46f);
      if(addonmodo_get(identidade)!=AM_DEBRID)texto(TXT_G18R,i18n("Conecte em Ajustes pelo celular."),r.x+28,hy+62,460,0.46f);
      botaoEm(i18n("Conectar pelo celular"),"aj_smartphone",r.x+28,r.y+342,41,1,0);
    } else {
      snprintf(s,sizeof s,i18n("Estas fontes do %s ficam ocultas."),nome);
      texto(TXT_G18R,s,r.x+28,hy,460,0.46f);
      texto(TXT_G18R,i18n("Outros addons ainda podem trazer vídeos."),r.x+28,hy+24,460,0.46f);
      botaoEm(i18n("Ver links diretos"),NULL,r.x+28,r.y+(addonmodo_get(identidade)==AM_DIRETO?342:304),42,1,0);
    }
  }
  configuracao(200,760,1598);
  snprintf(s,sizeof s,i18n("Continuar com %s"),i18n(modoSelecionado==AM_P2P?"P2P":MODOS[modoSelecionado-1]));
  float bw=botaoEm(s,"aj_arrow-right",200,934,40,0,1);
  texto(TXT_G18R,i18n("Mude depois em Meus addons."),212+bw,951,700,0.46f);
  detalhe(1560,950,240);
}
static const char *falhaTitulo(int t) {
  switch(t) {
    case AG_P2P:return i18n("O vídeo precisa do P2P nesta TV");
    case AG_DEBRID:return i18n("O serviço ainda está preparando o vídeo");
    case AG_OFFLINE:return i18n("O addon não respondeu a tempo");
    case AG_401:return i18n("O addon pediu para entrar de novo");
    case AG_403:return i18n("O addon não liberou o acesso agora");
    case AG_HDR:return i18n("Esta versão não abre nesta TV");
    case AG_VAZIO:return i18n("Ainda não encontramos o vídeo");
    default:return i18n("Não foi possível abrir a fonte");
  }
}
static const char *falhaCorpo(int t) {
  switch(t) {
    case AG_P2P:return i18n("O addon encontrou o vídeo, mas o P2P está desligado.");
    case AG_DEBRID:return i18n("Este arquivo ainda não está pronto na sua conta de debrid.");
    case AG_OFFLINE:return i18n("O addon demorou para responder. Vamos tentar outra fonte.");
    case AG_401:return i18n("Abra a configuração do próprio addon pelo celular. Depois, sincronize o addon com sua conta.");
    case AG_403:return i18n("O addon recusou este pedido. Se acontecer de novo, confira o acesso no site do addon.");
    case AG_HDR:return i18n("A qualidade disponível depende das outras fontes.");
    case AG_VAZIO:return i18n("Os addons responderam, mas não trouxeram fontes para este título.");
    default:return i18n("Abra Fontes para escolher outra opção.");
  }
}
void addonguia_lista(int f) {
  nAlvos=0;
  moldura(2);
  char cab[240];snprintf(cab,sizeof cab,"%s › %s",i18n("Ajustes"),i18n("Fontes e addons"));
  kicker(cab,200,136);
  texto(TXT_LOG_N44,i18n("Meus addons"),200,169,1300,1);
  texto(TXT_ILHA_CORPO,i18n("O que cada um traz. E o próximo passo, se precisar."),200,232,1400,0.64f);
  int inicio=f>3?f-3:0;
  for(int i=inicio;i<addons_n()&&i<inicio+4;i++) {
    float y=294+(i-inicio)*118;
    if(i==f)gfx_cor((GfxRect){200,y,1600,118},24/118.0f,1,1,1,0.075f);
    ponteiro_alvo(200,y,1600,118,addonsui_ponteiro,NULL,i,0);
    gfx_cor((GfxRect){224,y+20,58,58},18/58.0f,1,1,1,0.06f);
    int h,v,d,t;uint64_t id=addonmodo_id(addons_base(i));addonmodo_fatos(id,&h,&v,&d,&t);
    int catalogo=addons_sondado(i)&&!addons_fornece(i,ADD_STREAM);
    int falhou=h<0||h>=400, precisa=t&&!addonmodo_permite(id,0,debrid_ativo(),p2p_ativo());
    int ok=catalogo||v, desativado=!addons_ativo(i);
    const char *estado=desativado?i18n("Desligado"):falhou?i18n("Não respondeu"):precisa?i18n("Precisa escolher como tocar"):ok?i18n("Tudo certo"):i18n("conferindo o manifesto…");
    icone(papelIcone(i,falhou),239,y+35,28,0.7f);
    TxtLinha tit=txt_linha_corta(TXT_G26B,addons_nome(i),243,242,239,255,580);
    txt_desenhar(tit,306,y+14);
    float sx=306+tit.w+16;
    gfx_cor((GfxRect){sx,y+28,8,8},0.5f,falhou?0.698f:precisa?0.878f:ok?0.584f:0.55f,falhou?0.506f:precisa?0.694f:ok?0.729f:0.55f,falhou?0.533f:precisa?0.431f:ok?0.624f:0.55f,1);
    texto(TXT_G18R,estado,sx+16,y+18,1380-sx,0.64f);
    capacidades(i,306,y+52,1);
    const char *dica=desativado?i18n("Desligado"):h==401?falhaTitulo(AG_401):h==403?falhaTitulo(AG_403):falhou?i18n("Última tentativa: demorou para responder"):catalogo?i18n("Organiza os títulos · outro addon fornece o vídeo"):precisa?i18n("P2P desligado · nenhuma conta de debrid conectada"):d&&!t?i18n("O addon já entrega links prontos."):addonmodo_get(id)==AM_DIRETO?i18n("Sem P2P · torrents ficam ocultos."):i18n("Mude depois em Meus addons.");
    char servicoHint[240];
    if(!desativado&&!falhou&&d&&!t)for(int a=0;a<4;a++)if(strstr(addons_nome(i),SERVICOS[a])) {
      snprintf(servicoHint,sizeof servicoHint,i18n("Já toca pelo %s configurado no próprio addon"),SERVICOS[a]);dica=servicoHint;break;
    }
    texto(TXT_G18R,dica,306,y+85,1060,0.64f);
    const char *acao=falhou?i18n("Tentar de novo"):catalogo?i18n("Escolher fileiras da Home"):d&&!t?i18n("Configurar pelo celular"):i18n("Escolher como tocar");
    int anteriorFoco=foco;foco=i==f?50+i:-1;
    botaoEm(acao,NULL,-1780,y+33,50+i,1,0);foco=anteriorFoco;
  }
  if(f==addons_n()) {
    botao(i18n("Sincronizar addons"),"aj_rotate-cw",228,820,0);
  }
  if(f<addons_n()) {
    indice=f;identidade=addonmodo_id(addons_base(f));snprintf(nome,sizeof nome,"%s",addons_nome(f));
    painel((GfxRect){200,800,1600,180},24);rotas(228,810,1544);
  }
}
void addonguia_desenhar(void) {
  if(tela<0)return;
  nAlvos=0;
  gfx_cor((GfxRect){0,0,1920,1080},0,0,0,0,0.30f);
  if(tela==AG_MODO){moldura(1);modoDesenhar();return;}
  if(tela==AG_LISTA){addonguia_lista(foco);return;}
  moldura(tela==AG_CONECTAR);
  if(tela==AG_CONECTAR) {
    painel((GfxRect){380,244,1180,640},32);
    texto(TXT_ILHA_HORA,i18n("Fontes e addons"),432,286,1076,0.46f);
    texto(TXT_ILHA_TITULO,i18n("Conecte seu serviço pelo celular"),432,396,1076,1);
    bloco(i18n("Use sua conta do serviço para preparar o vídeo fora da TV."),432,460,1076,0.64f,2);
    float x=432;
    for(int i=0;i<(celb_disponivel()?4:3);i++)x+=botao(SERVICOS[i],NULL,x,542,i)+12;
    if(celb_embutido(CELB_ADDON))celb_desenhar_em(CELB_ADDON,(GfxRect){432,612,1076,170},1);
    else if(!celb_disponivel()&&nuvem_base_login()[0]) {
      const char *base=nuvem_base_login(),*p=strstr(base,"://");
      if(p){char u[512];size_t len=(size_t)(p+3+strcspn(p+3,"/?#")-base);
        if(len<sizeof u){memcpy(u,base,len);u[len]=0;qrPequeno(u,432,620,126);}}
      bloco(i18n("Conecte seu serviço pelo celular"),590,642,918,0.64f,2);
      texto(TXT_ILHA_SUB,i18n("Sincronizar addons"),590,712,918,0.64f);
    }
    else bloco(i18n("Escolha seu serviço. Você não precisa digitar chaves nesta TV."),432,632,1076,0.64f,2);
    botao(i18n("Voltar à escolha"),"aj_arrow-left",432,790,4);return;
  }
  if(tela==AG_INICIANDO) {
    painel((GfxRect){380,130,1180,850},32);
    texto(TXT_ILHA_TEXTO,titulo,432,174,1076,0.64f);
    TxtLinha cab=txt_linha(TXT_ILHA_TITULO,i18n("Conectando a outras pessoas…"),243,242,239,255);
    txt_desenhar(cab,960-cab.w/2,310);
    bloco(i18n("A TV recebe os primeiros pedaços para começar. O restante chega enquanto você assiste."),548,372,824,0.64f,2);
    const char *nodes[]={"aj_users","aj_tv-minimal-play","aj_circle-play"};
    for(int a=0;a<3;a++) {
      float nx=704+a*180;gfx_cor((GfxRect){nx,210,74,74},24/74.0f,1,1,1,0.06f);
      icone(nodes[a],nx+21,231,32,0.8f);
      if(a<2){float ar,ag,ab;ajustes_acento(&ar,&ag,&ab);gfx_cor((GfxRect){nx+88,246,78,2},0,ar,ag,ab,0.7f);}
    }
    P2pmEstado e;p2pmotor_estado(&e);
#ifdef NV_SHOT_HOOKS
    if(shot){e.pares=8;e.buffer_pct=68;}
#endif
    char s[120];snprintf(s,sizeof s,i18n("%u pessoas conectadas"),e.pares);
    texto(TXT_ILHA_TEXTO,s,740,458,600,0.64f);
    texto(TXT_ILHA_TEXTO,i18n("Preparando o início do vídeo"),502,540,850,0.8f);
    if(e.buffer_pct>=0){snprintf(s,sizeof s,"%d%%",e.buffer_pct);texto(TXT_ILHA_TEXTO,s,1340,540,120,0.8f);}
    gfx_cor((GfxRect){502,578,936,7},0.5f,1,1,1,0.09f);
    if(e.buffer_pct>=0){float ar,ag,ab;ajustes_acento(&ar,&ag,&ab);gfx_cor((GfxRect){502,578,936*e.buffer_pct/100.0f,7},0.5f,ar,ag,ab,1);}
    texto(TXT_G18R,i18n("Não precisa esperar o filme inteiro para começar."),692,605,746,0.46f);
    botao(i18n("Voltar às fontes"),NULL,850,646,0);
    rotas(432,730,1076);detalhe(432,920,1076);return;
  }
  int novo=tela>=AG_NOVO_TORRENT&&tela<=AG_NOVO_DIRETO;
  int acesso=tela==AG_NOVO_DIRETO&&acessoConfig;
  int routes=tela==AG_P2P||tela==AG_DEBRID||tela==AG_NOVO_TORRENT;
  float y=tela==AG_NOVO_CATALOGO?238:tela==AG_NOVO_DIRETO?180:novo?140:175;
  float h=routes?610:416;
  if(tela==AG_DEBRID)h=694;
  if(tela==AG_401||tela==AG_403||tela==AG_VAZIO||tela==AG_HDR)h=490;
  if(tela==AG_NOVO_TORRENT)h=834;
  if(tela==AG_NOVO_CATALOGO)h=636;
  if(tela==AG_NOVO_DIRETO)h=650;
  if(tela==AG_INICIANDO)h=780;
  painel((GfxRect){380,y,1180,h},32);
  float x=432,cy=y+38;
  if(novo) {
    gfx_cor((GfxRect){x,cy,58,58},18/58.0f,1,1,1,0.06f);
    icone(papelIcone(indice,0),x+15,cy+15,28,0.64f);
    texto(TXT_ILHA_TEXTO,i18n("Novo addon · adicionado à sua conta"),x+76,cy+14,1000,0.64f);
    cy+=78;
    kicker(tela==AG_NOVO_CATALOGO?i18n("Mais coisas para descobrir"):tela==AG_NOVO_DIRETO?i18n("Pronto para assistir"):i18n("Vamos deixar tudo pronto"),x,cy);cy+=30;
  } else {
    texto(TXT_ILHA_HORA,i18n("Resolver um problema › O vídeo não começa"),x,cy,1076,0.46f);cy+=34;
    gfx_cor((GfxRect){x,cy,58,58},18/58.0f,1,1,1,0.06f);
    icone("aj_circle-play",x+15,cy+15,28,0.64f);
    texto(TXT_ILHA_TEXTO,titulo,x+76,cy+14,1000,0.64f);cy+=70;
  }
  char s[240];
  if(tela==AG_INICIANDO)snprintf(s,sizeof s,"%s",i18n("Conectando a outras pessoas…"));
  else if(acesso)snprintf(s,sizeof s,"%s",i18n("Rever acesso no celular"));
  else if(tela==AG_NOVO_DIRETO)snprintf(s,sizeof s,"%s",i18n("Seu addon já entrega links prontos"));
  else if(novo)snprintf(s,sizeof s,i18n("O que o %s traz"),nome);
  else if(tela==AG_DEBRID&&servico[0])snprintf(s,sizeof s,i18n("O %s ainda está preparando o vídeo"),servico);
  else snprintf(s,sizeof s,"%s",falhaTitulo(tela));
  texto(TXT_ILHA_TITULO,s,x,cy,1076,1);cy+=62;
  const char *corpo=acesso?i18n("Abra a configuração do próprio addon no celular."):tela==AG_NOVO_CATALOGO?i18n("Catálogos para encontrar o próximo filme ou série. Outro addon traz o vídeo."):tela==AG_NOVO_DIRETO?i18n("Você pode assistir sem ligar o P2P nesta TV."):tela==AG_NOVO_TORRENT?i18n("O addon traz fontes para assistir. Torrents precisam de P2P ou de uma conta de debrid."):tela==AG_INICIANDO?i18n("A TV recebe os primeiros pedaços para começar. O restante chega enquanto você assiste."):falhaCorpo(tela);
  bloco(corpo,x,cy,novo?980:1076,0.64f,3);cy+=novo?104:66;
  if(novo && !acesso) {
    capacidades(indice,x,cy-8,0);cy+=52;
    if(tela==AG_NOVO_TORRENT) {
      linha(x,cy+4,1076);icone("aj_power",x,cy+36,26,0.64f);
      char estadoP2p[120];snprintf(estadoP2p,sizeof estadoP2p,"%s",ajustes_p2p_ligado()?i18n("Em uso nesta TV"):i18n("O P2P está desligado."));
      texto(TXT_ILHA_TEXTO,estadoP2p,x+44,cy+28,1000,1);
      texto(TXT_ILHA_TEXTO,i18n("Grátis · pode demorar a começar."),x+44,cy+62,1000,0.64f);
      cy+=136;
    }
  }
  if(tela==AG_INICIANDO||tela==AG_DEBRID) {
    P2pmEstado e;p2pmotor_estado(&e);
    int progresso=tela==AG_DEBRID?pct:e.buffer_pct;
#ifdef NV_SHOT_HOOKS
    if(shot){e.pares=8;progresso=tela==AG_DEBRID?42:68;}
#endif
    if(tela==AG_INICIANDO){snprintf(s,sizeof s,i18n("%u pessoas conectadas"),e.pares);texto(TXT_ILHA_TEXTO,s,x,cy,1076,0.64f);cy+=40;}
    texto(TXT_ILHA_TEXTO,tela==AG_DEBRID?i18n("O serviço ainda está preparando o vídeo"):i18n("Preparando o início do vídeo"),x,cy,900,0.8f);
    if(progresso>=0){snprintf(s,sizeof s,"%d%%",progresso);texto(TXT_ILHA_TEXTO,s,x+980,cy,96,0.8f);}
    gfx_cor((GfxRect){x,cy+36,1076,7},0.5f,1,1,1,0.09f);
    if(progresso>=0){float ar,ag,ab;ajustes_acento(&ar,&ag,&ab);gfx_cor((GfxRect){x,cy+36,1076*progresso/100.0f,7},0.5f,ar,ag,ab,1);}
    cy+=64;
  }
  if(tela==AG_NOVO_CATALOGO) {
    linha(x,cy,1076);icone("aj_check",x,cy+28,24,0.64f);
    texto(TXT_ILHA_TEXTO,i18n("Não precisa ligar o P2P para usar estes catálogos."),x+44,cy+22,1032,0.64f);
    texto(TXT_ILHA_TEXTO,i18n("Você pode escolher quais fileiras aparecem na Home."),x+44,cy+56,1032,0.64f);cy+=116;
  }
  const char *acao=tela==AG_NOVO_CATALOGO?i18n("Escolher fileiras da Home"):tela==AG_NOVO_DIRETO?i18n("Continuar"):tela==AG_INICIANDO?i18n("Voltar às fontes"):tela==AG_401?i18n("Rever acesso no celular"):tela==AG_P2P||tela==AG_NOVO_TORRENT?(localDisponivel()||ajustes_p2p_url()[0]?i18n("Ligar P2P e tentar"):i18n("Conectar pelo celular")):tela!=AG_VAZIO&&proxima()?i18n("Tentar a próxima fonte"):i18n("Buscar fontes de novo");
  if(tela==AG_NOVO_TORRENT&&!player_aberto())acao=i18n("Escolher como tocar");
  float w=botao(acao,tela==AG_P2P||tela==AG_NOVO_TORRENT?"aj_power":NULL,x,cy,0);
  if(tela!=AG_INICIANDO)botao(novo?i18n("Agora não"):i18n("Voltar às fontes"),NULL,x+w+12,cy,1);
  cy+=84;
  if(routes){rotas(x,cy,1076);cy+=188;}
  if(tela==AG_NOVO_DIRETO){
    char url[512];if(configUrl(url,sizeof url)){qrPequeno(url,x+930,cy-6,126);}
    texto(TXT_ILHA_NOME,acesso?i18n("Rever acesso no celular"):i18n("Quer mudar o serviço?"),x,cy,740,1);
    bloco(i18n("Abra a configuração do próprio addon no celular."),x,cy+40,730,0.64f,2);
    cy+=112;
  }
  detalhe(x,cy,1076);
}
#ifdef NV_SHOT_HOOKS
int addonguia_shot_tela(void) {return tela;}
void addonguia_shot(int t) {
  shot=1;addonguia_abrir(t,0,"Os Relojoeiros · T2:E5");
  snprintf(nome,sizeof nome,"Torrentio");snprintf(servico,sizeof servico,"TorBox");pct=42;
}
#endif
