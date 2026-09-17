// Perfil e Stats: UMA TELA, sem rolagem.
//
// A versao anterior era um documento de 2180px com quatro secoes, ~49 numerais,
// ~43 cores e sete estilos de texto. A 3 m nada daquilo se lia: tres violetas
// quase iguais faziam tres trabalhos diferentes, o mesmo par de inteiros
// aparecia em tres codificacoes (tiles, barra de proporcao e porcentagem) e o
// unico texto de navegacao da tela estava em 15px, que text.c documenta como
// tamanho de SELO e nao de leitura.
//
// A regra desta tela agora e: ~7 fatos, duas paradas de foco (o calendario e a
// lista de mais vistos), UM acento de dado (o violeta do streak e das celulas
// acesas) e o anel de foco no acento do TEMA, como nas outras doze telas do
// app. O que era terceira codificacao de um numero ja mostrado foi apagado, nao
// re-estilizado.
//
// Sem rolagem nao ha PF_DOC_H, scroll, velScroll, SECAO_Y nem visivel(): todas
// as coordenadas daqui sao a posicao final na tela de 1080.
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
#define PF_W            (NV_TELA_W-PF_X-NV_LEGACY_CONTENT_RIGHT)

// BANDA 1: titulo da pagina, na margem de cima como em ajustes.c.
// BANDA 2: identidade a esquerda e os quatro numeros a direita.
#define PF_RESUMO_Y      196.0f
#define PF_AVATAR        108.0f
#define PF_NUM_N            4
// BANDA 3: dois cabecalhos de secao e, abaixo, as duas colunas de conteudo.
#define PF_SECAO_Y       348.0f
#define PF_CONT_Y        406.0f
#define PF_DIR_X        (PF_X+PF_W*0.37f)
#define PF_DIR_W        (PF_W*0.63f)

// Calendario: 42px de celula e 6 linhas cobrem 31 dias com qualquer primeiro
// dia da semana (31+6 = 37 celulas). Sem numeral dentro: a celula e cor.
#define PF_CEL            42.0f
#define PF_CEL_GAP        10.0f
#define PF_CAL_PAD        28.0f
#define PF_CAL_LINHAS      6
#define PF_CAL_TOPO       74.0f   // topo do painel ate a 1a linha de celulas
#define PF_CAL_W         (PF_CEL*7.0f+PF_CEL_GAP*6.0f+PF_CAL_PAD*2.0f)
#define PF_CAL_H         (PF_CAL_TOPO+PF_CEL*PF_CAL_LINHAS \
                          +PF_CEL_GAP*(PF_CAL_LINHAS-1)+PF_CAL_PAD)
#define PF_CAL_LEG_Y     (PF_CONT_Y+PF_CAL_H+18.0f)
#define PF_GEN_Y         902.0f
#define PF_GEN_VIS          4

#define PF_CARD_H        122.0f
#define PF_CARD_GAP       22.0f
#define PF_CARD_ARTE_W   160.0f
#define PF_CARD_ARTE_H    90.0f
#define PF_CARD_PAD       18.0f

#define PF_AVISO_H        54.0f
#define PF_AVISO_Y       (NV_TELA_H-PF_AVISO_H-12.0f)
// UMA linha de rodape, em posicao FIXA (ajustes.c:2612). Nao quatro dicas que
// teleportam para a secao em foco.
#define PF_RODAPE_Y      (PF_AVISO_Y-40.0f)
#define PF_CONTEUDO_H    (PF_RODAPE_Y-6.0f)

#define PF_SECOES          2   // 0 = calendario, 1 = mais vistos

// PALETA DE TEXTO: tres niveis, e so. Os catorze cinzas anteriores colapsavam
// em cinco faixas perceptuais de qualquer jeito.
#define PF_FORTE   244
#define PF_MEDIO   206
#define PF_FRACO   162
// ACENTO DE DADO, um so: o numeral do streak e as celulas acesas do calendario.
// O anel de FOCO nao usa este violeta — usa ajustes_acento(), o acento que a
// pessoa escolheu, como nas outras telas.
#define PF_AC_R   0.745f
#define PF_AC_G   0.435f
#define PF_AC_B   0.878f

// Quatro matizes SEPARADOS entre si e do violeta da interface. A paleta antiga
// tinha oito, quatro deles na mesma familia do acento — as pastilhas de genero
// pareciam quatro vezes a mesma coisa.
static const uint32_t PALETA[PF_GEN_VIS] = {
  0x3C9FE8, 0xE8A33C, 0x4FC08D, 0xE8636F
};

// COLUNAS DE NUMERO, em fracao de PF_W: x de cada uma e a largura util dela.
// NAO sao iguais de proposito. Com passo uniforme "39h 44min" em 56px saia
// truncado como "39h…", e a ultima coluna cortava justamente o "no mês" que e a
// ressalva do streak. A ultima vai ate a margem direita.
static const float PF_NUM_FX[PF_NUM_N] = { 0.34f, 0.56f, 0.68f, 0.80f };
static const float PF_NUM_FW[PF_NUM_N] = { 0.21f, 0.11f, 0.11f, 0.20f };

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
static float entrada, focoCal, focoItem[PERFIL_MAX_DESTAQUES];

static int limitar(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }
static void rgb(uint32_t c, float *r, float *g, float *b) {
  *r = ((c >> 16) & 255) / 255.0f;
  *g = ((c >> 8) & 255) / 255.0f;
  *b = (c & 255) / 255.0f;
}
static void texto(TxtEstilo e, const char *s, int cor, float x, float y, float a) {
  txt_desenhar_alpha(txt_linha(e, s ? s : "", cor, cor, cor, 255), x, y, a);
}
static void corta(TxtEstilo e,const char *s,int c,float x,float y,float w,float a) {
  txt_desenhar_alpha(txt_linha_corta(e,s,c,c,c,255,w),x,y,a);
}
// Anel de foco. RAIO EXTERNO = raio da caixa + espessura do anel, senao o canto
// do anel fica mais quadrado que o da caixa (detail.c:3006). A cor vem do tema.
static void anel(GfxRect r, float raioPx, float a) {
  float ar, ag, ab;
  GfxRect e = { r.x-NV_ANEL_FOCO, r.y-NV_ANEL_FOCO,
                r.w+NV_ANEL_FOCO*2.0f, r.h+NV_ANEL_FOCO*2.0f };
  ajustes_acento(&ar, &ag, &ab);
  gfx_rect(e, 0, GFX_ANEL, 0, NV_ANEL_FOCO/e.h, 0,
           (raioPx+NV_ANEL_FOCO)/e.h, ar, ag, ab, a);
}
static void numero(char *s, size_t n, int v) { snprintf(s, n, "%d", v < 0 ? 0 : v); }
static void tempo(char *s, size_t n, int minutos) {
  if (minutos < 0) minutos = 0;
  if (minutos < 60) snprintf(s, n, "%d min", minutos);
  else snprintf(s, n, "%dh %02dmin", minutos / 60, minutos % 60);
}
// Quantos destaques cabem: a coluna da direita tem altura fixa e nao rola.
static int nCards(void) {
  return dados.nDestaques < PERFIL_MAX_DESTAQUES ? dados.nDestaques
                                                 : PERFIL_MAX_DESTAQUES;
}

int perfil_iniciar(void) {
  memset(&dados, 0, sizeof(dados));
  aberto = sair = temDados = temIdentidade = carregando = 0;
  estado = PF_CARREGANDO;
  secao = item = 0; escolhido = -1;
  dia = pedirAtualizar = 0; erro[0] = 0;
  entrada = focoCal = 0;
  memset(focoItem, 0, sizeof(focoItem));
  return 1;
}
void perfil_encerrar(void) { perfil_iniciar(); }
void perfil_abrir(void) {
  // Abre SEMPRE na primeira parada. Consultar `dados` aqui nao serve: a tela e
  // aberta antes de o snapshot chegar da worker, entao nDias ainda e 0 e o foco
  // nascia na coluna da direita. Quem corrige o estado impossivel e
  // perfil_definir_dados, que e quem sabe o que chegou.
  aberto = 1; sair = 0; escolhido = -1; item = 0; secao = 0;
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
    secao=item=dia=0;
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
  if(!temDados)secao=0;
  if (item >= nCards()) item = nCards() ? nCards() - 1 : 0;
  // Uma parada so existe se tiver filho focavel. Sem calendario o foco cai nos
  // cards; sem cards ele volta para o calendario. (Na tela antiga as secoes 0 e
  // 3 nao tinham filho nenhum: focar nelas so mexia um ponto de 14px.)
  if (secao == 0 && dados.nDias == 0 && nCards() > 0) secao = 1;
  if (secao == 1 && nCards() == 0) secao = 0;
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
  // DUAS paradas de verdade, lado a lado: o calendario a esquerda e a lista de
  // mais vistos a direita. Esquerda/direita troca de coluna quando a coluna
  // atual acaba; dentro do calendario as setas andam dia a dia, com
  // continuidade entre semanas.
  if (secao == 0 && dados.nDias > 0) {
    if (k==SDLK_LEFT)  { if (dia>0) { dia--; return; } perfil_fechar(); return; }
    if (k==SDLK_RIGHT) { if (dia+1<dados.nDias) { dia++; return; }
                         if (nCards()) secao=1; return; }
    if (k==SDLK_UP)    { if (dia>=7) dia-=7; return; }
    if (k==SDLK_DOWN)  { if (dia+7<dados.nDias) dia+=7; return; }
    return;
  }
  if (secao == 1) {
    if (k==SDLK_LEFT)  { if (dados.nDias>0) secao=0; else perfil_fechar(); return; }
    if (k==SDLK_UP)    { if (item>0) item--; return; }
    if (k==SDLK_DOWN)  { if (item+1<nCards()) item++; return; }
    if (k==SDLK_RETURN || k==SDLK_KP_ENTER || k==SDLK_SPACE) {
      if (nCards() > 0) escolhido = item;
      return;
    }
    return;
  }
  if (k == SDLK_LEFT) perfil_fechar();
}

void perfil_atualizar(float dt, Uint32 agora) {
  (void)agora;
  int reduzida=ajustes_animacoes_reduzidas();
  entrada = anim_mola(entrada, aberto ? 1.0f : 0.0f, dt, NV_MOLA_TELA);
  if (!aberto && entrada < 0.002f) entrada = 0;
  focoCal = anim_mola(focoCal, secao == 0 ? 1.0f : 0.0f, dt, NV_MOLA_FOCO);
  for (int i = 0; i < PERFIL_MAX_DESTAQUES; i++)
    focoItem[i] = anim_mola(focoItem[i], secao == 1 && i == item ? 1.0f : 0.0f,
                            dt, NV_MOLA_FOCO);
  if(reduzida) {
    entrada=aberto?1:0;
    focoCal=(secao==0);
    for(int i=0;i<PERFIL_MAX_DESTAQUES;i++)focoItem[i]=(secao==1&&i==item);
  }
}

// Cabecalho de SECAO no padrao da casa: TXT_HEADLINE, sem ponto violeta e sem
// dica de navegacao pendurada (ajustes.c:2578, detail.c:3115).
static void tituloSecao(const char *s, float x, float a) {
  texto(TXT_HEADLINE, s, PF_MEDIO, x, PF_SECAO_Y, a);
}

static void tituloPagina(float a) {
  TxtLinha t = txt_linha(TXT_TITULO1, "Perfil e Stats", PF_FORTE, PF_FORTE, PF_FORTE, 255);
  txt_desenhar_alpha(t, PF_X, NV_MARGEM_Y, a);
  // O periodo fica AO LADO do titulo, alinhado pela base, como a linha de
  // contexto dos Ajustes: e rotulo do recorte, nao um dado a mais.
  if (dados.periodo[0]) {
    TxtLinha p = txt_linha(TXT_CAPTION, dados.periodo, PF_FRACO, PF_FRACO, PF_FRACO, 255);
    txt_desenhar_alpha(p, PF_X + PF_W - p.w,
                       NV_MARGEM_Y + t.h - p.h - 12.0f, a);
  }
}

// Esqueleto: os MESMOS retangulos, nas MESMAS coordenadas do conteudo real, em
// NV_COR_ESQUELETO. O anterior punha blocos em 144/214/342 e nada aparecia ali,
// entao a tela inteira saltava quando o dado chegava.
static void desenharLoading(Uint32 agora, float a) {
  float pulso = ajustes_animacoes_reduzidas()?.55f:.48f+.12f*sinf((float)agora*.004f);
  float p = pulso * a;
  #define PF_ESQ(r_,raio_) gfx_cor((r_),(raio_),NV_COR_ESQUELETO_R, \
                                   NV_COR_ESQUELETO_G,NV_COR_ESQUELETO_B,p)
  tituloPagina(a);
  PF_ESQ(((GfxRect){PF_X,PF_RESUMO_Y,PF_AVATAR,PF_AVATAR}), .5f);
  PF_ESQ(((GfxRect){PF_X+132,PF_RESUMO_Y+4,300,40}), NV_RAIO_BADGE);
  PF_ESQ(((GfxRect){PF_X+132,PF_RESUMO_Y+54,170,22}), NV_RAIO_BADGE);
  for (int i=0;i<PF_NUM_N;i++) {
    float x = PF_X + PF_W*PF_NUM_FX[i], w = PF_W*PF_NUM_FW[i];
    PF_ESQ(((GfxRect){x,PF_RESUMO_Y-6,w*.62f,58}), NV_RAIO_BADGE);
    PF_ESQ(((GfxRect){x,PF_RESUMO_Y+66,w*.80f,22}), NV_RAIO_BADGE);
  }
  PF_ESQ(((GfxRect){PF_X,PF_SECAO_Y+4,240,34}), NV_RAIO_BADGE);
  PF_ESQ(((GfxRect){PF_DIR_X,PF_SECAO_Y+4,240,34}), NV_RAIO_BADGE);
  PF_ESQ(((GfxRect){PF_X,PF_CONT_Y,PF_CAL_W,PF_CAL_H}), NV_RAIO_CARD);
  PF_ESQ(((GfxRect){PF_X,PF_CAL_LEG_Y,PF_CAL_W*.62f,22}), NV_RAIO_BADGE);
  PF_ESQ(((GfxRect){PF_X,PF_CAL_LEG_Y+NV_LD_CAPTION,PF_CAL_W*.78f,22}), NV_RAIO_BADGE);
  for (int i=0;i<PF_GEN_VIS;i++) {
    float gw = PF_W*0.36f*.5f;
    PF_ESQ(((GfxRect){PF_X+(i%2)*gw,PF_GEN_Y+(i/2)*NV_LD_CAPTION+6,14,14}), NV_RAIO_BADGE);
    PF_ESQ(((GfxRect){PF_X+(i%2)*gw+26,PF_GEN_Y+(i/2)*NV_LD_CAPTION,gw*.55f,22}), NV_RAIO_BADGE);
  }
  for (int i=0;i<PERFIL_MAX_DESTAQUES;i++)
    PF_ESQ(((GfxRect){PF_DIR_X,PF_CONT_Y+i*(PF_CARD_H+PF_CARD_GAP),
                      PF_DIR_W,PF_CARD_H}), NV_RAIO_CARD);
  #undef PF_ESQ
}

static void desenharVazio(float a) {
  const char *titulo = "Nenhuma reprodução neste período";
  const char *corpo = "Conecte o Trakt e assista a um filme ou episódio. Seu resumo usa somente o histórico disponível.";
  GfxRect btn={PF_X,452,330,72};
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
  tituloPagina(a);
  corta(TXT_TITULO2, titulo, PF_FORTE, PF_X, 248, PF_W*.72f, a);
  txt_bloco(TXT_BODY, corpo, PF_MEDIO, PF_MEDIO, PF_MEDIO,
            PF_X, 340, 840, NV_LD_BODY, a, 3);
  // Botao unico da tela, sempre em foco: preenchido na cor de realce com
  // texto escuro (a regra de NV_COR_FOCO, layout.h), sem anel.
  { float ar,ag,ab; ajustes_acento(&ar,&ag,&ab);
    gfx_cor(btn,NV_RAIO_PILL,ar,ag,ab,a); }
  texto(TXT_DET_BOTAO,"OK · Tentar novamente",20,PF_X+28,472,a);
}

// Banda 2: quem e (esquerda) e os QUATRO numeros que sobraram (direita).
// Todos no MESMO estilo — antes o total assistido usava TXT_TITULO1 (76), o
// streak usava TXT_TITULO1 tambem e os contadores TXT_TITULO2 (56), maior que
// os cabecalhos de secao.
static void desenharResumo(float a) {
  char b[64], t[64];
  const char *rot[PF_NUM_N];
  char val[PF_NUM_N][64];
  float ident = PF_X + PF_AVATAR + 24.0f;

  { GfxRect av={PF_X,PF_RESUMO_Y,PF_AVATAR,PF_AVATAR};
    GLuint tx=dados.avatar[0]?tex_obter_larg(dados.avatar,PF_AVATAR+40.0f):0;
    gfx_cor(av,.5f,NV_COR_ESQUELETO_R,NV_COR_ESQUELETO_G,NV_COR_ESQUELETO_B,a);
    if(tx){gfx_tex_aspect_atual=tex_aspecto(dados.avatar);
           gfx_rect(av,tx,GFX_AVATAR,0,0,0,0,1,1,1,a);gfx_tex_aspect_atual=0;}
    else gfx_icone((GfxRect){PF_X+24,PF_RESUMO_Y+24,60,60},"menu_profile",
                   .82f,.83f,.86f,a);
  }
  if (dados.nome[0] || dados.usuario[0]) {
    float w = PF_X + PF_W*PF_NUM_FX[0] - ident - 24.0f;
    if (dados.nome[0]) {
      corta(TXT_HEADLINE,dados.nome,PF_FORTE,ident,PF_RESUMO_Y+16,w,a);
      if (dados.usuario[0]) {
        snprintf(b,sizeof(b),"@%s",dados.usuario);
        corta(TXT_CAPTION,b,PF_FRACO,ident,PF_RESUMO_Y+16+NV_LD_HEADLINE,w,a);
      }
    } else {
      snprintf(b,sizeof(b),"@%s",dados.usuario);
      corta(TXT_HEADLINE,b,PF_FORTE,ident,PF_RESUMO_Y+16,w,a);
    }
  }

  if (dados.minutos > 0) { tempo(t,sizeof t,dados.minutos);
                           snprintf(val[0],sizeof val[0],"%s",t);
                           rot[0]="assistidos"; }
  else { numero(val[0],sizeof val[0],dados.plays); rot[0]="reproduções"; }
  numero(val[1],sizeof val[1],dados.filmes);     rot[1]="filmes";
  numero(val[2],sizeof val[2],dados.episodios);  rot[2]="episódios";
  numero(val[3],sizeof val[3],dados.streakAtual);
  rot[3]=dados.streakCompleto?"dias em sequência":"dias em sequência no mês";

  for (int i=0;i<PF_NUM_N;i++) {
    float x = PF_X + PF_W*PF_NUM_FX[i], w = PF_W*PF_NUM_FW[i];
    // O UNICO numeral com acento e o streak — a promessa de "um violeta, e so
    // em dado" se cumpre aqui e nas celulas acesas do calendario.
    if (i==3) txt_desenhar_alpha(
        txt_linha_corta(TXT_TITULO2,val[i],(int)(PF_AC_R*255),(int)(PF_AC_G*255),
                        (int)(PF_AC_B*255),255,w),x,PF_RESUMO_Y,a);
    else corta(TXT_TITULO2,val[i],PF_FORTE,x,PF_RESUMO_Y,w,a);
    corta(TXT_CAPTION,rot[i],PF_FRACO,x,PF_RESUMO_Y+NV_LD_TITULO2,w,a);
  }
}

static void desenharAtividade(float a) {
  static const char *diasSem[7]={"Dom","Seg","Ter","Qua","Qui","Sex","Sáb"};
  GfxRect painel={PF_X,PF_CONT_Y,PF_CAL_W,PF_CAL_H};
  float x0=PF_X+PF_CAL_PAD, y0=PF_CONT_Y+PF_CAL_TOPO;
  int max=0;
  char b[96];

  tituloSecao("Ritmo de atividade", PF_X, a);
  gfx_cor(painel,NV_RAIO_CARD,NV_COR_FOCO_R,NV_COR_FOCO_G,NV_COR_FOCO_B,.34f*a);
  for(int c=0;c<7;c++) {
    TxtLinha l=txt_linha(TXT_CAPTION,diasSem[c],PF_FRACO,PF_FRACO,PF_FRACO,255);
    txt_desenhar_alpha(l,x0+c*(PF_CEL+PF_CEL_GAP)+(PF_CEL-l.w)*.5f,
                       PF_CONT_Y+PF_CAL_PAD+2.0f,a);
  }
  for(int i=0;i<dados.nDias;i++) if(dados.atividade[i]>max) max=dados.atividade[i];
  for(int i=0;i<dados.nDias;i++) {
    int p=dados.primeiroDiaSemana+i, col=p%7, lin=p/7;
    GfxRect c={x0+col*(PF_CEL+PF_CEL_GAP),y0+lin*(PF_CEL+PF_CEL_GAP),PF_CEL,PF_CEL};
    if(lin>=PF_CAL_LINHAS) break;
    // DUAS intensidades, nao cinco. O comentario da versao anterior ja admitia
    // que cinco tons de violeta nao se distinguem a 3 m — e mesmo assim havia
    // uma legenda de cinco quadradinhos para explica-los.
    if(!dados.atividade[i])
      gfx_cor(c,NV_RAIO_BADGE,NV_COR_FOCO_R,NV_COR_FOCO_G,NV_COR_FOCO_B,.80f*a);
    else if(max>1 && dados.atividade[i]*2<=max)
      gfx_cor(c,NV_RAIO_BADGE,PF_AC_R,PF_AC_G,PF_AC_B,.52f*a);
    else
      gfx_cor(c,NV_RAIO_BADGE,PF_AC_R,PF_AC_G,PF_AC_B,a);
    if(i==dia && focoCal>.02f) anel(c,PF_CEL*NV_RAIO_BADGE,a*focoCal);
  }
  if(dados.nDias)snprintf(b,sizeof(b),i18n("Dia %d: %u reproduções"),dia+1,dados.atividade[dia]);
  else snprintf(b,sizeof(b),"Atividade diária indisponível");
  corta(TXT_CAPTION,b,PF_MEDIO,PF_X,PF_CAL_LEG_Y,PF_CAL_W,a);
  snprintf(b,sizeof(b),i18n("%d de %d dias ativos no mês"),dados.diasAtivosMes,dados.nDias);
  corta(TXT_CAPTION,b,PF_FRACO,PF_X,PF_CAL_LEG_Y+NV_LD_CAPTION,PF_CAL_W,a);
}

// Generos: SO as pastilhas, em duas linhas de duas. A faixa proporcional de
// oito cores que ficava acima delas dizia a mesma coisa pior, e quatro das oito
// cores eram o mesmo violeta da interface.
static void desenharGeneros(float a) {
  int n = dados.nGeneros < PF_GEN_VIS ? dados.nGeneros : PF_GEN_VIS;
  float w = PF_W*0.36f*.5f;
  if (!n) return;
  for (int i=0;i<n;i++) {
    float r,g,bl;
    float xx=PF_X+(i%2)*w, yy=PF_GEN_Y+(i/2)*NV_LD_CAPTION;
    char rot[80];
    rgb(dados.generos[i].cor?dados.generos[i].cor:PALETA[i],&r,&g,&bl);
    gfx_cor((GfxRect){xx,yy+6,14,14},NV_RAIO_BADGE,r,g,bl,a);
    // i18n NO NOME, ANTES DE COMPOR. A tradução acontece dentro de txt_linha,
    // sobre a string INTEIRA — e "Comédia · 32" nunca casa com chave nenhuma.
    // Era o defeito da foto 06-profile.png: a tela toda em inglês com
    // "Comédia", "Mistério" e "Terror" no meio. Mesma família do commit 81216de.
    snprintf(rot,sizeof(rot),"%s · %d",i18n(dados.generos[i].nome),
             dados.generos[i].quantidade);
    corta(TXT_CAPTION,rot,PF_MEDIO,xx+26,yy,w-46,a);
  }
}

static void desenharDestaques(float a) {
  int n = nCards();
  tituloSecao("Mais vistos", PF_DIR_X, a);
  if(!n) { texto(TXT_BODY,"Nenhum destaque neste período.",PF_FRACO,
                 PF_DIR_X,PF_CONT_Y+8,a); return; }
  for(int i=0;i<n;i++) {
    float f=focoItem[i];
    GfxRect r={PF_DIR_X,PF_CONT_Y+i*(PF_CARD_H+PF_CARD_GAP),PF_DIR_W,PF_CARD_H};
    float raio=NV_RAIO_CARD*(r.w<r.h?r.w:r.h);
    float tx0=r.x+PF_CARD_PAD+PF_CARD_ARTE_W+NV_HOME_TEXT_GUTTER;
    float tw=r.x+r.w-PF_CARD_PAD-tx0;
    GfxRect mini={r.x+PF_CARD_PAD,r.y+(PF_CARD_H-PF_CARD_ARTE_H)*.5f,
                  PF_CARD_ARTE_W,PF_CARD_ARTE_H};
    const char *art=dados.destaques[i].backdrop[0]?dados.destaques[i].backdrop
                                                  :dados.destaques[i].poster;
    GLuint tex=art[0]?tex_obter_larg(art,mini.w):0;
    char linha[200];
    // FOCO EM SUPERFICIE: a pilula escura acende e o anel entra JUNTO, os dois
    // pelo mesmo f. Antes o card subia numa mola e o anel aparecia de uma vez.
    gfx_cor(r,NV_RAIO_CARD,NV_COR_FOCO_R,NV_COR_FOCO_G,NV_COR_FOCO_B,(.34f+.66f*f)*a);
    if(f>.02f) anel(r,raio,a*f);
    if(tex){gfx_tex_aspect_atual=tex_aspecto(art);
            gfx_rect(mini,tex,GFX_CARD,f,0,0,NV_RAIO_CARD,1,1,1,a);
            gfx_tex_aspect_atual=0;}
    else gfx_cor(mini,NV_RAIO_CARD,NV_COR_ESQUELETO_R,NV_COR_ESQUELETO_G,
                 NV_COR_ESQUELETO_B,a);
    corta(TXT_CALLOUT,dados.destaques[i].titulo,PF_FORTE,tx0,r.y+30,tw,a);
    // UMA linha de apoio, nao duas encavaladas (as duas anteriores ficavam a 25px
    // uma da outra contra NV_LD_CAPTION 29 — entrelinha NEGATIVA — e a de baixo
    // ainda estourava o card). O "#N" do ranking saiu: a ordem da lista JA e a
    // posicao. A duracao saiu junto: ela e plays x runtime, o mesmo numero ao
    // lado outra vez, e era o que fazia a linha precisar de corte.
    snprintf(linha,sizeof linha,i18n("%d reproduções"),dados.destaques[i].plays);
    if(dados.destaques[i].detalhe[0]) {
      char junto[240];
      snprintf(junto,sizeof junto,"%s  ·  %s",dados.destaques[i].detalhe,linha);
      corta(TXT_CAPTION,junto,PF_FRACO,tx0,r.y+30+NV_LD_CALLOUT,tw,a);
    } else corta(TXT_CAPTION,linha,PF_FRACO,tx0,r.y+30+NV_LD_CALLOUT,tw,a);
  }
}

void perfil_desenhar(Uint32 agora) {
  if (entrada <= .002f) return;
  float a=entrada;
  gfx_cor((GfxRect){0,0,NV_TELA_W,NV_TELA_H},0,
          NV_COR_FUNDO_R,NV_COR_FUNDO_G,NV_COR_FUNDO_B,a);
  // UM recorte, aberto aqui e fechado aqui. A versao anterior chamava
  // gfx_sem_recorte() no fim dos destaques, o que DESLIGA a tesoura em vez de
  // devolver o recorte externo — so nao dava defeito porque era o ultimo
  // desenho da tela.
  gfx_recorte(0,0,NV_TELA_W,PF_CONTEUDO_H);
  if(carregando && !temDados) desenharLoading(agora,a);
  else if(!temDados) desenharVazio(a);
  else {
    tituloPagina(a);
    desenharResumo(a);
    desenharAtividade(a);
    desenharGeneros(a);
    desenharDestaques(a);
  }
  gfx_sem_recorte();

  if (temDados)
    texto(TXT_CAPTION,"Setas: navegar  ·  OK: abrir  ·  Voltar: menu",
          PF_FRACO,PF_X,PF_RODAPE_Y,a);
  else
    texto(TXT_CAPTION,"OK: tentar de novo  ·  Voltar: menu",
          PF_FRACO,PF_X,PF_RODAPE_Y,a);

  char avisoBuf[320];
  const char *aviso=NULL;
  if(erro[0]){
    if(estado==PF_STALE)snprintf(avisoBuf,sizeof avisoBuf,i18n("Atualização indisponível · mostrando o último resumo recebido. %s"),erro);
    else snprintf(avisoBuf,sizeof avisoBuf,"%s",erro);
    aviso=avisoBuf;
  } else if(carregando)aviso="Atualizando histórico sem interromper o conteúdo anterior…";
  else aviso=dados.aviso[0]?dados.aviso:dados.parcial?"Histórico parcial: os totais consideram somente os registros carregados.":NULL;
  if(aviso){
    gfx_cor((GfxRect){PF_X,PF_AVISO_Y,PF_W,PF_AVISO_H},NV_RAIO_CARD,
            NV_COR_FOCO_R,NV_COR_FOCO_G,NV_COR_FOCO_B,.92f*a);
    TxtLinha l=txt_linha_corta(TXT_CAPTION,aviso,PF_MEDIO,PF_MEDIO,PF_MEDIO,255,PF_W-40);
    txt_desenhar_alpha(l,PF_X+20,PF_AVISO_Y+(PF_AVISO_H-l.h)*.5f,a);
  }
}
