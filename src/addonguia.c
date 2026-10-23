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
#include "ponteiro.h"
#include "player.h"
#include "redesaude.h"
#include "sessao.h"
#include "streams.h"
#include "text.h"
#include "addonurl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int tela=-1, anterior=AG_MODO, indice, foco, pedido, detalhes, pct=-1, serv, acessoConfig;
static uint64_t identidade;
static char nome[96], titulo[160], servico[32];
static const char *const MODOS[]={"P2P nesta TV","Serviço de debrid","Só links diretos"};
static const char *const ICONES[]={"aj_tv-minimal-play","aj_cloud","aj_link"};
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
  txt_bloco(TXT_ILHA_CORPO,s,243,242,239,x,y,w,1.45f,a,linhas);
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
static void apontar(int id,int unused) { (void)unused; foco=id; }
static void alvo(GfxRect r,int id) {
  if(nAlvos<20) {alvos[nAlvos].r=r;alvos[nAlvos++].id=id;}
  if(tela>=0)ponteiro_alvo(r.x,r.y,r.w,r.h,apontar,NULL,id,0);
}
static float botao(const char *rot,const char *ic,float x,float y,int id) {
  TxtLinha l=txt_linha(TXT_G20B,rot,243,242,239,255);
  float w=l.w+52+(ic?35:0), tinta=1;
  GfxRect r={x,y,w,60};
  if(foco==id){float ar,ag,ab;tinta=ajustes_acento_tinta(&ar,&ag,&ab);gfx_cor(r,0.5f,ar,ag,ab,1);}
  else gfx_cor(r,0.5f,1,1,1,0.08f);
  l=txt_linha(TXT_G20B,rot,(int)(243*tinta),(int)(242*tinta),(int)(239*tinta),255);
  if(ic)gfx_icone((GfxRect){x+26,y+18,23,23},ic,tinta,tinta,tinta,1);
  txt_desenhar(l,x+26+(ic?35:0),y+(60-l.h)/2);
  alvo(r,id); return w;
}
static void linha(float x,float y,float w) {gfx_cor((GfxRect){x,y,w,1},0,1,1,1,0.08f);}
static void pergunta(char *s,size_t n) {snprintf(s,n,i18n("Como o %s vai tocar?"),nome);}
static void rotas(float x,float y,float w) {
  char s[180]; pergunta(s,sizeof s);
  linha(x,y,w); texto(TXT_ILHA_SECAO,s,x,y+22,w,1);
  texto(TXT_ILHA_SUB,addonmodo_get(identidade)==AM_AUTO?i18n("Nenhum modo ativo · escolher"):i18n("Mudar o modo"),x+w-280,y+24,280,0.6f);
  float cw=(w-24)/3;
  for(int i=0;i<3;i++) {
    GfxRect r={x+i*(cw+12),y+60,cw,108};
    gfx_cor(r,20/r.h,1,1,1,foco==10+i?0.14f:0.035f);
    icone(ICONES[i],r.x+18,r.y+20,21,0.9f);
    texto(TXT_G20B,i18n(i==0&&ajustes_p2p_url()[0]?"Servidor P2P":MODOS[i]),r.x+48,r.y+16,cw-58,1);
    texto(TXT_G18R,i18n(DESCR[i]),r.x+18,r.y+58,cw-30,0.64f);
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
  identidade=addonmodo_id(addons_base(addon));
  snprintf(nome,sizeof nome,"%s",addons_nome(addon));
  snprintf(titulo,sizeof titulo,"%s",tit&&*tit? tit:addons_nome(addon));
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
  if(foco>=10 && foco<=12){tela=AG_MODO;foco-=10;return;}
  if(foco==30){anterior=tela;tela=AG_NOVO_DIRETO;acessoConfig=1;foco=0;return;}
  if(tela==AG_MODO) {
    if(foco==40)foco=addonmodo_get(identidade)>=AM_P2P?addonmodo_get(identidade)-1:0;
    if(foco==1 && !debrid_ativo()){conectar();return;}
    if(foco<=2 && escolher(foco+1))entregar(AGP_REPETIR);
    return;
  }
  if(tela==AG_CONECTAR) {
    if(foco>=0 && foco<4){serv=foco;celb_abrir_embutido(CELB_ADDON,"Conectar pelo celular");}
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
}
void addonguia_atualizar(int permitir) {
  addonmodo_preparar(perfis_dono()[0]?perfis_dono():sessao_usuario(),perfis_ativo_addons());
  if(tela==AG_CONECTAR) {
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
  texto(TXT_G18R,i18n("Ver detalhes"),x+28,y,w-28,foco==20?1:0.6f);
  alvo((GfxRect){x,y,220,32},20);
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
  texto(TXT_ILHA_TEXTO,i18n("Se ele entrega links prontos, o Nuvio toca direto."),x+156,y+65,w-180,0.64f);
  texto(TXT_ILHA_TEXTO,i18n("Para mudar o serviço, abra o site do próprio addon pelo QR."),x+156,y+96,w-180,0.64f);
}
static void modoDesenhar(void) {
  char s[180];pergunta(s,sizeof s);
  texto(TXT_ILHA_HORA,i18n("Meus addons"),200,134,1000,0.46f);
  texto(TXT_LOG_N44,s,200,167,1600,1);
  bloco(i18n("O addon encontrou o vídeo. O Nuvio cuida de como ele chega até você."),200,232,1600,0.64f,2);
  texto(TXT_ILHA_TEXTO,i18n("Para estas fontes, escolha P2P nesta TV ou um serviço conectado ao Nuvio."),200,270,1600,0.64f);
  for(int i=0;i<3;i++) {
    GfxRect r={200+i*540,322,518,350};
    painel(r,28);gfx_cor(r,28/r.h,1,1,1,foco==i?0.09f:0.015f);alvo(r,i);
    icone(ICONES[i],r.x+28,r.y+52,36,0.8f);
    texto(TXT_G28B,i18n(i==0&&ajustes_p2p_url()[0]?"Servidor P2P":MODOS[i]),r.x+28,r.y+108,460,1);
    txt_bloco(TXT_ILHA_TEXTO,i18n(DESCR[i]),243,242,239,r.x+28,r.y+158,450,1.4f,0.64f,3);
    texto(TXT_G18R,i==0?i18n("Recebe o começo e continua durante o vídeo."):i==1?"Real-Debrid · TorBox · Premiumize · AllDebrid":i18n("Outros addons ainda podem trazer vídeos."),r.x+28,r.y+244,460,0.46f);
    if(i==0&&!localDisponivel()&&!ajustes_p2p_url()[0])texto(TXT_G18R,i18n("Indisponível nesta TV"),r.x+28,r.y+286,440,0.46f);
    else texto(TXT_G20B,addonmodo_get(identidade)==i+1?i18n("Em uso nesta TV"):i==1&&!debrid_ativo()?i18n("Conectar pelo celular"):i18n("Confirmar"),r.x+28,r.y+286,460,1);
  }
  configuracao(200,694,1598);
  botao(i18n("Continuar"),"aj_arrow-right",200,870,40);
  texto(TXT_G18R,i18n("Mude depois em Meus addons."),470,887,700,0.46f);
  detalhe(1560,886,240);
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
  texto(TXT_ILHA_HORA,i18n("Fontes e addons"),200,136,1300,0.46f);
  texto(TXT_LOG_N44,i18n("Meus addons"),200,169,1300,1);
  texto(TXT_ILHA_CORPO,i18n("O que cada um traz. E o próximo passo, se precisar."),200,232,1400,0.64f);
  int inicio=f>3?f-3:0;
  for(int i=inicio;i<addons_n()&&i<inicio+4;i++) {
    float y=294+(i-inicio)*118;
    if(i==f)gfx_cor((GfxRect){200,y,1600,118},24/118.0f,1,1,1,0.075f);
    ponteiro_alvo(200,y,1600,118,addonsui_ponteiro,NULL,i,0);
    icone("aj_puzzle",224,y+30,34,0.7f);
    texto(TXT_G28B,addons_nome(i),306,y+17,800,1);
    int h,v,d,t;uint64_t id=addonmodo_id(addons_base(i));addonmodo_fatos(id,&h,&v,&d,&t);
    const char *estado=!addons_sondado(i)?i18n("conferindo o manifesto…"):h==401?falhaTitulo(AG_401):h==403?falhaTitulo(AG_403):t&&!debrid_ativo()&&!p2p_ativo()?i18n("Escolher como tocar"):!v&&h?i18n("Última tentativa: demorou para responder"):!v?i18n("Nenhuma consulta recente"):i18n("Tudo certo");
    texto(TXT_G18R,estado,800,y+25,550,0.6f);
    const char *papel=!addons_sondado(i)?i18n("conferindo o manifesto…"):addons_fornece(i,ADD_STREAM)?i18n("Fontes para tocar"):i18n("Catálogos");
    texto(TXT_G18R,papel,306,y+58,700,0.64f);
    texto(TXT_G18R,addons_ativo(i)?i18n("Ligado"):i18n("Desligado"),1100,y+58,240,0.6f);
    const char *acao=addons_sondado(i)&&!addons_fornece(i,ADD_STREAM)?i18n("Escolher fileiras da Home"):d&&!t?i18n("Rever acesso no celular"):i18n("Escolher como tocar");
    texto(TXT_G20B,acao,1400,y+40,370,1);
  }
  if(f==addons_n()) {
    botao(i18n("Sincronizar addons"),"aj_rotate-cw",228,820,0);
  }
  texto(TXT_G18R,i18n("Ligar ou desligar"),1400,1000,360,0.46f);
  if(f<addons_n()) {
    indice=f;identidade=addonmodo_id(addons_base(f));snprintf(nome,sizeof nome,"%s",addons_nome(f));
    painel((GfxRect){200,800,1600,180},24);rotas(228,810,1544);
  }
}
void addonguia_desenhar(void) {
  if(tela<0)return;
  nAlvos=0;
  gfx_cor((GfxRect){0,0,1920,1080},0,0,0,0,0.30f);
  if(tela==AG_MODO){modoDesenhar();return;}
  if(tela==AG_LISTA){addonguia_lista(foco);return;}
  if(tela==AG_CONECTAR) {
    painel((GfxRect){380,180,1180,700},32);
    texto(TXT_ILHA_HORA,i18n("Fontes e addons"),432,222,1076,0.46f);
    texto(TXT_ILHA_TITULO,i18n("Conecte seu serviço pelo celular"),432,276,1076,1);
    bloco(i18n("Use sua conta do serviço para preparar o vídeo fora da TV."),432,340,1076,0.64f,2);
    float x=432;
    for(int i=0;i<4;i++)x+=botao(SERVICOS[i],NULL,x,442,i)+12;
    if(celb_embutido(CELB_ADDON))celb_desenhar_em(CELB_ADDON,(GfxRect){432,530,1076,240},1);
    else bloco(i18n("Escolha seu serviço. Você não precisa digitar chaves nesta TV."),432,550,1076,0.64f,2);
    botao(i18n("Voltar à escolha"),"aj_arrow-left",432,788,4);return;
  }
  if(tela==AG_INICIANDO) {
    painel((GfxRect){380,130,1180,850},32);
    texto(TXT_ILHA_TEXTO,titulo,432,174,1076,0.64f);
    TxtLinha cab=txt_linha(TXT_ILHA_TITULO,i18n("Conectando a outras pessoas…"),243,242,239,255);
    txt_desenhar(cab,960-cab.w/2,230);
    bloco(i18n("A TV recebe os primeiros pedaços para começar. O restante chega enquanto você assiste."),548,294,824,0.64f,2);
    const char *nodes[]={"aj_users","aj_tv-minimal-play","aj_circle-play"};
    for(int a=0;a<3;a++) {
      float nx=704+a*180;gfx_cor((GfxRect){nx,390,74,74},24/74.0f,1,1,1,0.06f);
      icone(nodes[a],nx+21,411,32,0.8f);
      if(a<2){float ar,ag,ab;ajustes_acento(&ar,&ag,&ab);gfx_cor((GfxRect){nx+88,426,78,2},0,ar,ag,ab,0.7f);}
    }
    P2pmEstado e;p2pmotor_estado(&e);
#ifdef NV_SHOT_HOOKS
    if(shot){e.pares=8;e.buffer_pct=68;}
#endif
    char s[120];snprintf(s,sizeof s,i18n("%u pessoas conectadas"),e.pares);
    texto(TXT_ILHA_TEXTO,s,740,494,600,0.64f);
    texto(TXT_ILHA_TEXTO,i18n("Preparando o início do vídeo"),502,540,850,0.8f);
    if(e.buffer_pct>=0){snprintf(s,sizeof s,"%d%%",e.buffer_pct);texto(TXT_ILHA_TEXTO,s,1340,540,120,0.8f);}
    gfx_cor((GfxRect){502,578,936,7},0.5f,1,1,1,0.09f);
    if(e.buffer_pct>=0){float ar,ag,ab;ajustes_acento(&ar,&ag,&ab);gfx_cor((GfxRect){502,578,936*e.buffer_pct/100.0f,7},0.5f,ar,ag,ab,1);}
    texto(TXT_G18R,i18n("Não precisa esperar o filme inteiro para começar."),692,605,746,0.46f);
    botao(i18n("Voltar às fontes"),NULL,850,646,0);
    rotas(432,730,1076);detalhe(432,900,1076);return;
  }
  int novo=tela>=AG_NOVO_TORRENT&&tela<=AG_NOVO_DIRETO;
  int acesso=tela==AG_NOVO_DIRETO&&acessoConfig;
  int routes=tela==AG_P2P||tela==AG_DEBRID||tela==AG_NOVO_TORRENT||tela==AG_INICIANDO;
  float y=tela==AG_INICIANDO?130:novo?140:175;
  float h=routes?610:416;
  if(tela==AG_DEBRID)h=694;
  if(tela==AG_401||tela==AG_403||tela==AG_VAZIO||tela==AG_HDR)h=490;
  if(tela==AG_NOVO_TORRENT)h=834;
  if(tela==AG_NOVO_CATALOGO)h=650;
  if(tela==AG_NOVO_DIRETO)h=650;
  if(tela==AG_INICIANDO)h=780;
  painel((GfxRect){380,y,1180,h},32);
  float x=432,cy=y+38;
  if(novo) {
    gfx_cor((GfxRect){x,cy,58,58},18/58.0f,1,1,1,0.06f);
    icone("aj_puzzle",x+15,cy+15,28,0.64f);
    texto(TXT_ILHA_TEXTO,i18n("Novo addon · adicionado à sua conta"),x+76,cy+14,1000,0.64f);
    cy+=78;
    texto(TXT_ILHA_HORA,tela==AG_NOVO_CATALOGO?i18n("Mais coisas para descobrir"):tela==AG_NOVO_DIRETO?i18n("Pronto para assistir"):i18n("Vamos deixar tudo pronto"),x,cy,1076,0.46f);cy+=30;
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
    char caps[200]="";
    if(addons_sondado(indice)) {
      const char *labels[]={"Catálogos","Fontes para tocar","Legendas","Fichas dos títulos"};
      for(int a=0;a<4;a++)if(addons_fornece(indice,a)) {
        if(caps[0])strncat(caps," · ",sizeof caps-strlen(caps)-1);
        strncat(caps,i18n(labels[a]),sizeof caps-strlen(caps)-1);
      }
    }
    int http=0,valida=0,diretos=0,torrents=0;
    addonmodo_fatos(identidade,&http,&valida,&diretos,&torrents);
    if(torrents || (tela==AG_NOVO_TORRENT
#ifdef NV_SHOT_HOOKS
       && shot
#else
       && 0
#endif
       )) {
      strncat(caps," · ",sizeof caps-strlen(caps)-1);
      strncat(caps,i18n("Torrent / P2P"),sizeof caps-strlen(caps)-1);
    }
    if(tela==AG_NOVO_DIRETO&&!acesso) {
      strncat(caps," · ",sizeof caps-strlen(caps)-1);
      strncat(caps,i18n("Links diretos"),sizeof caps-strlen(caps)-1);
    }
    TxtLinha cap=txt_linha(TXT_G18R,caps,243,242,239,255);
    gfx_cor((GfxRect){x,cy-8,cap.w+30,42},0.5f,1,1,1,0.07f);
    texto(TXT_G18R,caps,x+15,cy,1046,0.8f);cy+=52;
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
  const char *acao=tela==AG_NOVO_CATALOGO?i18n("Escolher fileiras da Home"):tela==AG_NOVO_DIRETO?i18n("Continuar"):tela==AG_INICIANDO?i18n("Voltar às fontes"):tela==AG_401?i18n("Rever acesso no celular"):tela==AG_P2P||tela==AG_NOVO_TORRENT?(localDisponivel()||ajustes_p2p_url()[0]?i18n("Ligar P2P e tentar"):i18n("Conectar pelo celular")):tela!=AG_VAZIO&&proxima()?i18n("Tentar a próxima fonte"):i18n("Buscar fontes de novo");
  if(tela==AG_NOVO_TORRENT&&!player_aberto())acao=i18n("Escolher como tocar");
  float w=botao(acao,tela==AG_P2P||tela==AG_NOVO_TORRENT?"aj_power":NULL,x,cy,0);
  if(tela!=AG_INICIANDO)botao(novo?i18n("Agora não"):i18n("Voltar às fontes"),NULL,x+w+12,cy,1);
  cy+=84;
  if(routes){rotas(x,cy,1076);cy+=188;}
  if(tela==AG_NOVO_CATALOGO) {
    texto(TXT_ILHA_TEXTO,i18n("Não precisa ligar o P2P para usar estes catálogos."),x,cy,1076,0.64f);cy+=46;
    texto(TXT_ILHA_TEXTO,i18n("Você pode escolher quais fileiras aparecem na Home."),x,cy,1076,0.64f);cy+=46;
  }
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
