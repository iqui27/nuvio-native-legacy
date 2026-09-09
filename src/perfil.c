// Perfil e Stats: relatorio editorial escuro, pensado para leitura a 3 metros.
//
// A composicao evita uma grade de cards identicos. O resumo e tipografico, o
// streak e um calendario, generos viram uma faixa proporcional e os titulos
// mais vistos ganham paineis largos com arte. O acento violeta pertence aos
// dados, enquanto o foco continua branco como no restante do native legacy.
#include "perfil.h"
#include "idioma.h"
#include "anim.h"
#include "gfx.h"
#include "layout.h"
#include "text.h"
#include "tex_cache.h"
#include "ajustes.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define PF_X             ajustes_conteudo_x()
#define PF_W            (NV_TELA_W-PF_X-104.0f)
#define PF_TOPO           62.0f
#define PF_RESUMO_Y       92.0f
#define PF_DESTAQUES_Y   500.0f
#define PF_ATIV_Y       1120.0f
#define PF_GENEROS_Y    1660.0f
#define PF_DOC_H        2180.0f
#define PF_SECOES          4
#define PF_CARD_W        ((PF_W-PF_CARD_GAP)*.5f)
#define PF_CARD_H        112.0f
#define PF_CARD_GAP       28.0f
#define PF_AVISO_H        54.0f
#define PF_AVISO_Y       (NV_TELA_H-PF_AVISO_H-12.0f)
#define PF_CONTEUDO_H    (PF_AVISO_Y-12.0f)

static const float SECAO_Y[PF_SECOES] = {
  PF_RESUMO_Y, PF_DESTAQUES_Y, PF_ATIV_Y, PF_GENEROS_Y
};
static const uint32_t PALETA[PERFIL_MAX_GENEROS] = {
  0xA84BD6, 0x3C9FE8, 0xC57BE3, 0x5CB8F2,
  0x71338E, 0x235B79, 0xD6A1E8, 0x777780
};

static PerfilDados dados;
static int aberto, sair, carregando, temDados;
static int temIdentidade;
static int secao, item, escolhido = -1;
static int dia, pedirAtualizar;
static char erro[160];
#define PF_CARREGANDO PERFIL_ESTADO_CARREGANDO
#define PF_ATUALIZANDO PERFIL_ESTADO_ATUALIZANDO
#define PF_PRONTO PERFIL_ESTADO_PRONTO
#define PF_STALE PERFIL_ESTADO_STALE
#define PF_ERRO PERFIL_ESTADO_ERRO
static PerfilEstado estado=PERFIL_ESTADO_CARREGANDO;
static float entrada, scroll, scrollAlvo, velScroll;
static float focoSec[PF_SECOES], focoItem[PERFIL_MAX_DESTAQUES];

static int limitar(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }
static float fmax0(float v) { return v > 0.0f ? v : 0.0f; }
static void rgb(uint32_t c, float *r, float *g, float *b) {
  *r = ((c >> 16) & 255) / 255.0f;
  *g = ((c >> 8) & 255) / 255.0f;
  *b = (c & 255) / 255.0f;
}
static void texto(TxtEstilo e, const char *s, int cor, float x, float y, float a) {
  txt_desenhar_alpha(txt_linha(e, s ? s : "", cor, cor, cor, 255), x, y - scroll, a);
}
static void textoC(TxtEstilo e, const char *s, int r, int g, int b,
                   float x, float y, float a) {
  txt_desenhar_alpha(txt_linha(e, s ? s : "", r, g, b, 255), x, y - scroll, a);
}
static void numero(char *s, size_t n, int v) { snprintf(s, n, "%d", v < 0 ? 0 : v); }
static void tempo(char *s, size_t n, int minutos) {
  if (minutos < 0) minutos = 0;
  if (minutos < 60) snprintf(s, n, "%d min", minutos);
  else snprintf(s, n, "%dh %02dmin", minutos / 60, minutos % 60);
}
static void anel(GfxRect r, float raio, float a) {
  float menor = r.w < r.h ? r.w : r.h;
  gfx_rect(r, 0, GFX_ANEL, 0, NV_ANEL_FOCO/menor, 0, raio, 0.96f, 0.96f, 0.98f, a);
}
static int visivel(float y, float h) { return y+h-scroll>=0 && y-scroll<NV_TELA_H; }
static void corta(TxtEstilo e,const char *s,int c,float x,float y,float w,float a) {
  txt_desenhar_alpha(txt_linha_corta(e,s,c,c,c,255,w),x,y-scroll,a);
}

int perfil_iniciar(void) {
  memset(&dados, 0, sizeof(dados));
  aberto = sair = temDados = temIdentidade = carregando = 0;
  estado = PF_CARREGANDO;
  secao = item = 0; escolhido = -1;
  dia = pedirAtualizar = 0; erro[0] = 0;
  entrada = scroll = scrollAlvo = velScroll = 0;
  memset(focoSec, 0, sizeof(focoSec));
  memset(focoItem, 0, sizeof(focoItem));
  return 1;
}
void perfil_encerrar(void) { perfil_iniciar(); }
void perfil_abrir(void) {
  aberto = 1; sair = 0; escolhido = -1; secao = item = 0;
  scroll = scrollAlvo = velScroll = 0;
  pedirAtualizar = 0;
}
void perfil_fechar(void) { aberto = 0; sair = 1; }
int perfil_aberto(void) { return aberto; }
int perfil_quer_sair(void) { int q = sair; sair = 0; return q; }
void perfil_definir_carregando(int v) {
  carregando = !!v;
  if(carregando) estado=temDados?PF_ATUALIZANDO:PF_CARREGANDO;
}
void perfil_definir_erro(const char *m) {
  snprintf(erro,sizeof(erro),"%s",m&&m[0]?m:"Não foi possível atualizar o histórico.");
  carregando=0; estado=temDados?PF_STALE:PF_ERRO;
}
void perfil_definir_estado(PerfilEstado novo, const char *m) {
  if (m && m[0]) snprintf(erro, sizeof erro, "%s", m);
  else if (novo == PERFIL_ESTADO_PRONTO || novo == PERFIL_ESTADO_SEM_ATIVIDADE) erro[0] = 0;
  carregando = novo == PERFIL_ESTADO_CARREGANDO || novo == PERFIL_ESTADO_ATUALIZANDO;
  estado = novo;
  if ((novo == PERFIL_ESTADO_PRIVADO || novo == PERFIL_ESTADO_DESCONECTADO || novo == PERFIL_ESTADO_INDISPONIVEL) && temDados)
    estado = PERFIL_ESTADO_STALE;
}
PerfilEstado perfil_estado(void) { return estado; }
int perfil_pediu_atualizar(void) { int p=pedirAtualizar; pedirAtualizar=0; return p; }

void perfil_definir_dados(const PerfilDados *d) {
  if (!d) {
    memset(&dados,0,sizeof(dados));temDados=temIdentidade=carregando=0;
    secao=item=dia=0;scroll=scrollAlvo=velScroll=0;
    escolhido=-1;erro[0]=0;estado=PERFIL_ESTADO_CARREGANDO;return;
  }
  dados = *d;
  // O produtor pode preencher buffers fixos ate o ultimo byte. Fechar todos
  // aqui mantem as chamadas de texto e de textura seguras mesmo com payload
  // truncado vindo da rede.
  dados.nome[sizeof(dados.nome)-1] = 0;
  dados.usuario[sizeof(dados.usuario)-1] = 0;
  dados.avatar[sizeof(dados.avatar)-1] = 0;
  dados.periodo[sizeof(dados.periodo)-1] = 0;
  dados.aviso[sizeof(dados.aviso)-1] = 0;
  if(dados.minutos<0)dados.minutos=0;
  if(dados.plays<0)dados.plays=0;
  if(dados.filmes<0)dados.filmes=0;
  if(dados.episodios<0)dados.episodios=0;
  // Calendario mensal: no maximo 31 dias, mesmo que o array tenha 42 slots.
  dados.nDias = limitar(dados.nDias, 0, 31);
  dados.primeiroDiaSemana = limitar(dados.primeiroDiaSemana, 0, 6);
  dados.nGeneros = limitar(dados.nGeneros, 0, PERFIL_MAX_GENEROS);
  dados.nDestaques = limitar(dados.nDestaques, 0, PERFIL_MAX_DESTAQUES);
  for (int i=0; i<dados.nGeneros; i++) {
    dados.generos[i].nome[sizeof(dados.generos[i].nome)-1] = 0;
    if(dados.generos[i].quantidade<0)dados.generos[i].quantidade=0;
  }
  for (int i=0; i<dados.nDestaques; i++) {
    PerfilDestaque *p=&dados.destaques[i];
    p->id[sizeof(p->id)-1]=0; p->titulo[sizeof(p->titulo)-1]=0;
    p->detalhe[sizeof(p->detalhe)-1]=0; p->poster[sizeof(p->poster)-1]=0;
    p->backdrop[sizeof(p->backdrop)-1]=0;
  }
  temIdentidade = dados.nome[0] || dados.usuario[0] || dados.avatar[0];
  temDados = temIdentidade || dados.minutos > 0 || dados.plays > 0 ||
             dados.filmes > 0 || dados.episodios > 0 || dados.nDestaques > 0;
  carregando = 0;
  erro[0]=0; estado=temDados?(dados.plays||dados.nDestaques?PF_PRONTO:PERFIL_ESTADO_SEM_ATIVIDADE):PF_ERRO; escolhido=-1;
  dia=limitar(dia,0,dados.nDias?dados.nDias-1:0);
  if(!temDados){secao=0;scroll=scrollAlvo=velScroll=0;}
  if (item >= dados.nDestaques) item = dados.nDestaques ? dados.nDestaques - 1 : 0;
}

int perfil_item_selecionado(PerfilDestaque *saida) {
  if (escolhido < 0 || escolhido >= dados.nDestaques) return 0;
  if (saida) *saida = dados.destaques[escolhido];
  escolhido = -1;
  return 1;
}

void perfil_evento(const SDL_Event *e) {
  if (!aberto || !e || e->type != SDL_KEYDOWN) return;
  SDL_Keycode k = e->key.keysym.sym;
  if (k == SDLK_ESCAPE || k == SDLK_AC_BACK || k == SDLK_BACKSPACE || k == SDLK_DELETE) {
    perfil_fechar(); return;
  }
  if(k==SDLK_r || ((!temDados || erro[0]) &&
     (k==SDLK_RETURN || k==SDLK_KP_ENTER))) {
    if(!carregando)pedirAtualizar=1;
    return;
  }
  if (!temDados) return;
  // O calendario recebe o D-pad real, com continuidade entre semanas. Sair
  // pela primeira/ultima semana devolve a navegacao para as secoes.
  if(secao==2 && dados.nDias>0) {
    if(k==SDLK_LEFT && dia>0){dia--;return;}
    if(k==SDLK_RIGHT && dia+1<dados.nDias){dia++;return;}
    if(k==SDLK_UP && dia>=7){dia-=7;return;}
    if(k==SDLK_DOWN && dia+7<dados.nDias){dia+=7;return;}
  }
  if(secao==1) {
    if(k==SDLK_LEFT && item%2){item--;return;}
    if(k==SDLK_RIGHT && !(item%2) && item+1<dados.nDestaques){item++;return;}
    if(k==SDLK_UP && item>=2){item-=2;return;}
    if(k==SDLK_DOWN && item+2<dados.nDestaques){item+=2;return;}
  }
  if (k == SDLK_LEFT && (secao!=1 || item==0)) { perfil_fechar(); return; }
  if (k == SDLK_UP && secao > 0) secao--;
  else if (k == SDLK_DOWN && secao < PF_SECOES - 1) {
    secao++;
    if(secao==2)dia=0;
  }
  else if (secao == 1 && (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE)) {
    if (dados.nDestaques > 0) escolhido = item;
  }
}

void perfil_atualizar(float dt, Uint32 agora) {
  (void)agora;
  int reduzida=ajustes_animacoes_reduzidas();
  entrada = anim_mola(entrada, aberto ? 1.0f : 0.0f, dt, NV_MOLA_TELA);
  if (!aberto && entrada < 0.002f) entrada = 0;
  float alvo = SECAO_Y[secao] - (secao == 0 ? PF_RESUMO_Y : 120.0f);
  float maxScroll = fmax0(PF_DOC_H - NV_TELA_H + 54.0f);
  scrollAlvo = anim_clamp(alvo, 0, maxScroll);
  scroll = anim_mola2(&velScroll, scroll, scrollAlvo, dt, NV_MOLA2_SCROLL);
  if(reduzida) { entrada=aberto?1:0;scroll=scrollAlvo;velScroll=0; }
  for (int i = 0; i < PF_SECOES; i++)
    focoSec[i] = anim_mola(focoSec[i], i == secao ? 1.0f : 0.0f, dt, NV_MOLA_FOCO);
  for (int i = 0; i < PERFIL_MAX_DESTAQUES; i++)
    focoItem[i] = anim_mola(focoItem[i], secao == 1 && i == item ? 1.0f : 0.0f,
                            dt, NV_MOLA_FOCO);
  if(reduzida) {
    for(int i=0;i<PF_SECOES;i++)focoSec[i]=(i==secao);
    for(int i=0;i<PERFIL_MAX_DESTAQUES;i++)focoItem[i]=(secao==1&&i==item);
  }
}

static void tituloSecao(const char *s, int qual, float y, float a) {
  float f = focoSec[qual];
  GfxRect ponto = { PF_X, y + 14 - scroll, 14, 14 };
  gfx_cor(ponto, .5f, .54f + .12f*f, .24f + .08f*f, .72f + .16f*f, a);
  if (f > .02f)
    anel((GfxRect){ponto.x-5,ponto.y-5,ponto.w+10,ponto.h+10},.5f,a*f);
  texto(TXT_TITULO3, s, 244, PF_X + 28, y, a);
  if (f > .02f) {
    const char *ajuda=qual==1?"Setas: títulos  ·  OK: detalhes":
       qual==2?"Setas: dias  ·  Voltar: menu":"Cima/baixo: seções  ·  Voltar: menu";
    TxtLinha d = txt_linha(TXT_MINI, ajuda, 185,185,194,255);
    txt_desenhar_alpha(d, PF_X + PF_W - d.w, y + 11 - scroll, a * f * .72f);
  }
}

static void desenharLoading(Uint32 agora, float a) {
  float pulso = ajustes_animacoes_reduzidas()?.55f:.48f+.12f*sinf((float)agora*.004f);
  tituloSecao("Perfil e Stats", 0, PF_TOPO, a);
  gfx_cor((GfxRect){PF_X,144-scroll,510,40},.18f,.18f,.18f,.20f,pulso*a);
  gfx_cor((GfxRect){PF_X,214-scroll,930,92},.08f,.18f,.18f,.20f,pulso*a);
  for (int i=0;i<4;i++)
    gfx_cor((GfxRect){PF_X+i*274,342-scroll,230,58},.15f,.18f,.18f,.20f,pulso*a);
  gfx_cor((GfxRect){PF_X,PF_ATIV_Y+58-scroll,PF_W*.57f,432},.035f,.18f,.18f,.20f,pulso*a);
  gfx_cor((GfxRect){PF_X,PF_GENEROS_Y+58-scroll,PF_W,176},.05f,.18f,.18f,.20f,pulso*a);
  for (int i=0;i<PERFIL_MAX_DESTAQUES;i++) {
    int col=i%2,lin=i/2;
    gfx_cor((GfxRect){PF_X+col*(PF_CARD_W+PF_CARD_GAP),
                      PF_DESTAQUES_Y+62+lin*(PF_CARD_H+PF_CARD_GAP)-scroll,
                      PF_CARD_W,PF_CARD_H},.035f,.18f,.18f,.20f,pulso*a);
  }
}

static void desenharVazio(float a) {
  const char *titulo = "Nenhuma reprodução neste período";
  const char *corpo = "Conecte o Trakt e assista a um filme ou episódio. Seu resumo usa somente o histórico disponível.";
  if (estado == PERFIL_ESTADO_PRIVADO) {
    titulo = "Perfil privado ou histórico não compartilhado";
    corpo = "O Trakt não liberou um histórico público para esta conta.";
  } else if (estado == PERFIL_ESTADO_DESCONECTADO) {
    titulo = "Trakt desconectado";
    corpo = "Vincule o Trakt para carregar identidade, obras recentes e estatísticas.";
  } else if (estado == PERFIL_ESTADO_INDISPONIVEL || estado == PERFIL_ESTADO_ERRO) {
    titulo = "Perfil indisponível";
    corpo = erro[0] ? erro : "Não foi possível confirmar este resumo agora.";
  }
  tituloSecao("Perfil e Stats", 0, PF_TOPO, a);
  corta(TXT_TITULO2, titulo, 244, PF_X, 248, PF_W, a);
  txt_bloco(TXT_BODY,
    corpo,
    177,179,187, PF_X, 326-scroll, 780, NV_LD_BODY, a, 3);
  GfxRect btn={PF_X,452-scroll,330,72};
  gfx_cor(btn,.24f,NV_COR_FOCO_R,NV_COR_FOCO_G,NV_COR_FOCO_B,a);
  anel(btn,.24f,a);
  texto(TXT_DET_BOTAO,"OK · Tentar novamente",240,PF_X+24,472,a);
  texto(TXT_CAPTION,"Voltar retorna ao menu",182,PF_X,554,a);
}

static void desenharResumo(float a) {
  if(!visivel(PF_RESUMO_Y,400))return;
  char b[64], t[64];
  float identidadeX=PF_X+180.0f, identidadeW=PF_W*.40f;
  float periodoY = PF_RESUMO_Y + 64.0f;
  float consumoY = PF_RESUMO_Y + 102.0f;
  tituloSecao("Perfil e Stats", 0, PF_RESUMO_Y, a);
  { GfxRect av={PF_X,PF_RESUMO_Y+62,140,140};
    GLuint tx=dados.avatar[0]?tex_obter_larg(dados.avatar,180):0;
    gfx_cor(av,.5f,.14f,.16f,.19f,a);
    if(tx){gfx_tex_aspect_atual=tex_aspecto(dados.avatar);gfx_rect(av,tx,GFX_AVATAR,0,0,0,0,1,1,1,a);gfx_tex_aspect_atual=0;}
    else gfx_icone((GfxRect){PF_X+38,PF_RESUMO_Y+100,64,64},"menu_profile",.82f,.83f,.86f,a);
  }
  if (dados.nome[0]) {
    corta(TXT_TITULO2,dados.nome,246,identidadeX,PF_RESUMO_Y+62,identidadeW,a);
    if (dados.usuario[0]) {
      snprintf(b,sizeof(b),"@%s",dados.usuario);
      corta(TXT_CAPTION,b,183,identidadeX,PF_RESUMO_Y+122,identidadeW,a);
      periodoY = PF_RESUMO_Y + 170.0f;
    } else periodoY = PF_RESUMO_Y + 128.0f;
    consumoY = periodoY + 44.0f;
  } else if (dados.usuario[0]) {
    snprintf(b,sizeof(b),"@%s",dados.usuario);
    corta(TXT_TITULO2,b,246,identidadeX,PF_RESUMO_Y+62,identidadeW,a);
    periodoY = PF_RESUMO_Y + 128.0f;
    consumoY = periodoY + 44.0f;
  }
  textoC(TXT_CAPTION, dados.periodo[0] ? dados.periodo : "Período não informado",
         184,112,220, identidadeX, periodoY, a);
  if (dados.minutos > 0) {
    tempo(t,sizeof(t),dados.minutos);
    snprintf(b,sizeof(b),i18n("%s assistidos"),t);
  } else snprintf(b,sizeof(b),i18n("%d reproduções registradas"),dados.plays);
  corta(TXT_TITULO1,b,246,identidadeX,consumoY,identidadeW,a);
  if(dados.aviso[0])corta(TXT_MINI,dados.aviso,183,identidadeX,PF_RESUMO_Y+292,identidadeW,a);

  const char *rot[4]={"REPRODUÇÕES","FILMES","EPISÓDIOS","DIAS ATIVOS"};
  int val[4]={dados.plays,dados.filmes,dados.episodios,dados.diasAtivosMes};
  float x=PF_X+PF_W*.58f;
  for(int i=0;i<4;i++) {
    float xx=x+(i%2)*PF_W*.22f, yy=PF_RESUMO_Y+62+(i/2)*104;
    numero(b,sizeof(b),val[i]); texto(TXT_TITULO2,b,244,xx,yy,a);
    textoC(TXT_MINI,rot[i],166,168,178,xx,yy+62,a);
  }
  if(dados.plays>0 && dados.minutos>0) {
    tempo(t,sizeof(t),(int)((double)dados.minutos/dados.plays+.5));
    snprintf(b,sizeof(b),i18n("%s por reprodução"),t);
    corta(TXT_CAPTION,b,193,x,PF_RESUMO_Y+292,PF_W*.42f,a);
  }
  // As contagens representam eventos de reproducao, nao titulos unicos.
  double total=(double)dados.filmes+dados.episodios;
  if(total>0) {
    float w=PF_W*.50f, filmes=w*(float)(dados.filmes/total);
    gfx_cor((GfxRect){PF_X,PF_RESUMO_Y+316-scroll,w,10},.4f,.23f,.60f,.82f,a);
    if(filmes>0)gfx_cor((GfxRect){PF_X,PF_RESUMO_Y+316-scroll,filmes,10},.4f,.65f,.30f,.83f,a);
    snprintf(b,sizeof(b),i18n("Filmes %.0f%%  ·  Episódios %.0f%%"),100*dados.filmes/total,100*dados.episodios/total);
    corta(TXT_CAPTION,b,201,PF_X,PF_RESUMO_Y+340,w,a);
  }
}

static void desenharAtividade(float a) {
  if(!visivel(PF_ATIV_Y,500))return;
  tituloSecao("Ritmo de atividade",2,PF_ATIV_Y,a);
  GfxRect painel={PF_X,PF_ATIV_Y+58-scroll,PF_W*.57f,432};
  gfx_cor(painel,.045f,.075f,.073f,.085f,.98f*a);
  static const char *dias[7]={"Dom","Seg","Ter","Qua","Qui","Sex","Sáb"};
  float x0=PF_X+32, y0=PF_ATIV_Y+110-scroll, cel=44, gap=10;
  for(int c=0;c<7;c++) {
    TxtLinha l=txt_linha(TXT_MINI,dias[c],145,147,156,255);
    txt_desenhar_alpha(l,x0+c*(cel+gap)+(cel-l.w)*.5f,y0-28,a);
  }
  int max=0,melhor=0,ultimo=-1;
  for(int i=0;i<dados.nDias;i++) {
    if(dados.atividade[i]) ultimo=i;
    if(dados.atividade[i]>max){max=dados.atividade[i];melhor=i;}
  }
  for(int i=0;i<dados.nDias;i++) {
    int p=dados.primeiroDiaSemana+i, col=p%7, lin=p/7;
    float q=max?(float)dados.atividade[i]/max:0.0f;
    GfxRect c={x0+col*(cel+gap),y0+lin*(cel+gap),cel,cel};
    if(q<=0) gfx_cor(c,.16f,.12f,.12f,.14f,.78f*a);
    else gfx_cor(c,.16f,.38f+.23f*q,.13f+.13f*q,.52f+.31f*q,a);
    char nd[8];snprintf(nd,sizeof(nd),"%d",i+1);
    TxtLinha l=txt_linha(TXT_MINI,nd,242,242,246,255);
    txt_desenhar_alpha(l,c.x+(c.w-l.w)*.5f,c.y+(c.h-l.h)*.5f,a);
    if(secao==2 && i==dia)anel((GfxRect){c.x-3,c.y-3,c.w+6,c.h+6},.20f,a);
  }
  char b[96];
  if(dados.nDias)snprintf(b,sizeof(b),i18n("Dia %d: %u reproduções"),dia+1,dados.atividade[dia]);
  else snprintf(b,sizeof(b),"Atividade diária indisponível");
  corta(TXT_CAPTION,b,226,PF_X+32,PF_ATIV_Y+450,painel.w-64,a);
  // Legenda separada do calendario: leitura numerica continua acessivel sem
  // depender de distinguir cinco tons de violeta na tela.
  float lx=PF_X+454;
  texto(TXT_CAPTION,"Reproduções",205,lx,PF_ATIV_Y+106,a);
  for(int i=0;i<5;i++) {
    float q=i/4.0f;
    gfx_cor((GfxRect){lx+i*38,PF_ATIV_Y+150-scroll,28,28},.18f,
             i?.38f+.23f*q:.12f,i?.13f+.13f*q:.12f,i?.52f+.31f*q:.14f,a);
  }
  snprintf(b,sizeof(b),"0 a %d por dia",max);
  corta(TXT_MINI,b,184,lx,PF_ATIV_Y+196,painel.w-480,a);
  if(dados.atividade[melhor]){
    snprintf(b,sizeof(b),i18n("Pico: %d reproduções no dia %d"),max,melhor+1);
    corta(TXT_CAPTION,b,219,lx,PF_ATIV_Y+276,painel.w-480,a);
  }
  if(ultimo>=0) {
    snprintf(b,sizeof(b),i18n("Última atividade: dia %d"),ultimo+1);
    corta(TXT_CAPTION,b,193,lx,PF_ATIV_Y+326,painel.w-480,a);
  }
  float rx=PF_X+PF_W*.63f;
  numero(b,sizeof(b),dados.streakAtual); textoC(TXT_TITULO1,b,190,111,224,rx,PF_ATIV_Y+76,a);
  corta(TXT_CALLOUT,dados.streakCompleto?"dias em sequência":"dias em sequência no mês",220,rx,PF_ATIV_Y+148,PF_W*.37f,a);
  snprintf(b,sizeof(b),i18n("%d de %d dias ativos no mês"),dados.diasAtivosMes,dados.nDias);
  corta(TXT_BODY,b,203,rx,PF_ATIV_Y+218,PF_W*.37f,a);
  if(dados.anoCompleto){
    snprintf(b,sizeof(b),i18n("%d dias ativos no ano"),dados.diasAtivosAno);
    corta(TXT_CAPTION,b,185,rx,PF_ATIV_Y+278,PF_W*.37f,a);
  } else corta(TXT_CAPTION,"Cobertura: período selecionado",185,rx,PF_ATIV_Y+278,PF_W*.37f,a);
}

static void desenharGeneros(float a) {
  if(!visivel(PF_GENEROS_Y,280))return;
  tituloSecao("Seu mapa de gêneros",3,PF_GENEROS_Y,a);
  if(!dados.nGeneros) { texto(TXT_BODY,"Os gêneros aparecem conforme o histórico cresce.",170,
                              PF_X,PF_GENEROS_Y+78,a); return; }
  double total=0; for(int i=0;i<dados.nGeneros;i++) total+=dados.generos[i].quantidade;
  if(total<1) total=1;
  float x=PF_X,y=PF_GENEROS_Y+92-scroll;
  for(int i=0;i<dados.nGeneros;i++) {
    float w=PF_W*((float)dados.generos[i].quantidade/total),r,g,b;
    rgb(dados.generos[i].cor?dados.generos[i].cor:PALETA[i],&r,&g,&b);
    if(w>2)gfx_cor((GfxRect){x,y,w-2,30},.12f,r,g,b,a); x+=w;
  }
  x=PF_X;
  for(int i=0;i<dados.nGeneros;i++) {
    float w=PF_W/4,xx=PF_X+(i%4)*w,yy=PF_GENEROS_Y+148+(i/4)*52;
    float r,g,b;rgb(dados.generos[i].cor?dados.generos[i].cor:PALETA[i],&r,&g,&b);
    gfx_cor((GfxRect){xx,yy+7-scroll,14,14},.5f,r,g,b,a);
    char rot[64];snprintf(rot,sizeof(rot),"%s · %d",dados.generos[i].nome,dados.generos[i].quantidade);
    corta(TXT_CAPTION,rot,219,xx+26,yy,w-42,a);
  }
}

static void desenharDestaques(float a) {
  if(!visivel(PF_DESTAQUES_Y,520))return;
  tituloSecao("Mais vistos",1,PF_DESTAQUES_Y,a);
  if(!dados.nDestaques) { texto(TXT_BODY,"Nenhum destaque neste período.",170,
                                PF_X,PF_DESTAQUES_Y+78,a); return; }
  gfx_recorte(PF_X-12,0,PF_W+24,PF_CONTEUDO_H);
  for(int i=0;i<dados.nDestaques;i++) {
    int col=i%2,lin=i/2;
    float f=focoItem[i], x=PF_X+col*(PF_CARD_W+PF_CARD_GAP);
    float docY=PF_DESTAQUES_Y+62+lin*(PF_CARD_H+PF_CARD_GAP);
    float y=docY-scroll;
    if(y+PF_CARD_H<0 || y>PF_CONTEUDO_H) continue;
    float lift=f*NV_FOCO_LIFT;
    GfxRect r={x,y-lift,PF_CARD_W,PF_CARD_H};
    if(f>.02f) {
      GfxRect s={r.x-18,r.y+10,r.w+36,r.h+26};
      gfx_rect(s,0,GFX_SOMBRA,f,0,0,.05f,0,0,0,NV_SOMBRA_ALFA*a*f);
    }
    gfx_cor(r,.045f,.045f,.075f,.085f,a);
    const char *art=dados.destaques[i].backdrop[0]?dados.destaques[i].backdrop:dados.destaques[i].poster;
    GfxRect mini={r.x+18,r.y+11,168,90};
    GLuint tx=art[0]?tex_obter_larg(art,mini.w):0;
    if(tx){gfx_tex_aspect_atual=tex_aspecto(art);gfx_rect(mini,tx,GFX_CARD,f,0,0,.045f,1,1,1,a);gfx_tex_aspect_atual=0;}
    else gfx_cor(mini,.045f,.14f,.14f,.16f,a);
    char rank[12],meta[80],linha[112]; snprintf(rank,sizeof(rank),"#%d",i+1);
    textoC(TXT_CALLOUT,rank,189,112,220,r.x+204,docY-lift+15,a*.90f);
    corta(TXT_TITULO3,dados.destaques[i].titulo,246,r.x+250,docY-lift+8,r.w-270,a);
    corta(TXT_CAPTION,dados.destaques[i].detalhe,216,r.x+204,docY-lift+66,r.w-222,a);
    if(dados.destaques[i].minutos>0) {
      tempo(meta,sizeof(meta),dados.destaques[i].minutos);
      snprintf(linha,sizeof(linha),i18n("%d reproduções  ·  %s"),dados.destaques[i].plays,meta);
    } else snprintf(linha,sizeof(linha),i18n("%d reproduções"),dados.destaques[i].plays);
    corta(TXT_CAPTION,linha,194,r.x+204,docY-lift+91,r.w-222,a);
    if(secao==1 && item==i)anel((GfxRect){r.x-NV_ANEL_FOCO,r.y-NV_ANEL_FOCO,
                              r.w+NV_ANEL_FOCO*2,r.h+NV_ANEL_FOCO*2},.047f,a);
  }
  gfx_sem_recorte();
}

void perfil_desenhar(Uint32 agora) {
  if (entrada <= .002f) return;
  float a=entrada;
  gfx_cor((GfxRect){0,0,NV_TELA_W,NV_TELA_H},0,
          NV_COR_FUNDO_R,NV_COR_FUNDO_G,NV_COR_FUNDO_B,a);
  gfx_recorte(0,0,NV_TELA_W,PF_CONTEUDO_H);
  if(carregando && !temDados) desenharLoading(agora,a);
  else if(!temDados) desenharVazio(a);
  else {
    desenharResumo(a); desenharAtividade(a); desenharGeneros(a); desenharDestaques(a);
  }
  gfx_sem_recorte();
  char avisoBuf[320];
  const char *aviso=NULL;
  if(erro[0]){
    if(estado==PF_STALE)snprintf(avisoBuf,sizeof avisoBuf,i18n("Atualização indisponível · mostrando o último resumo recebido. %s"),erro);
    else snprintf(avisoBuf,sizeof avisoBuf,"%s",erro);
    aviso=avisoBuf;
  } else if(carregando)aviso="Atualizando histórico sem interromper o conteúdo anterior…";
  else aviso=dados.aviso[0]?dados.aviso:dados.parcial?"Histórico parcial: os totais consideram somente os registros carregados.":NULL;
  if(aviso){
    gfx_cor((GfxRect){PF_X,PF_AVISO_Y,PF_W,PF_AVISO_H},.15f,.13f,.11f,.16f,.97f*a);
    TxtLinha l=txt_linha_corta(TXT_CAPTION,aviso,226,217,235,255,PF_W-40);
    txt_desenhar_alpha(l,PF_X+20,PF_AVISO_Y+14,a);
  }
}
