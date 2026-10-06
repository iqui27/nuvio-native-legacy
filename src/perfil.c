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
// lista de mais vistos) e UM acento, o do TEMA: no streak e nos quadrados do
// ritmo (o violeta de dado proprio saiu com o mockup de out/2026). O que era terceira codificacao de um numero ja mostrado foi apagado, nao
// re-estilizado.
//
// Sem rolagem nao ha PF_DOC_H, scroll, velScroll, SECAO_Y nem visivel(): todas
// as coordenadas daqui sao a posicao final na tela de 1080.
// ---------------------------------------------------------------------------
// O ACABAMENTO DO MOCKUP (Glass UI "ilha", tela 11 "Perfil e Stats + Social";
// out/2026). O dono: "o mockup ta bem mais polido que a build, nao podemos
// errar". A tela continua UMA, sem rolagem, com as mesmas duas paradas de
// foco; o que mudou foi a forma:
//   - o cabecalho e a PESSOA: avatar de 120 com anel no acento, o nome em
//     52/800 e "@usuario · fontes" embaixo. O titulo "Perfil e Stats" so
//     aparece quando nao ha identidade (e nos estados vazio e carregando);
//   - os numeros viram CARTOES DE VIDRO numa grade de 4-5 (valor 44/800,
//     rotulo em caixa alta), o streak no acento;
//   - embaixo, tres cartoes: o ritmo do mes (quadrados de 34 com tres
//     intensidades do acento), os mais vistos (linhas com miniatura 84x48) e
//     os AMIGOS (rostos e quem esta vendo agora, de socialvis).
// O QUE O MOCKUP TEM E O APP NAO: o chip "Quem ve: Amigos" e o botao "Editar
// perfil" (nao ha fluxo de editar perfil nesta tela, e "Quem ve" nao tem chave
// de traducao), os contadores "voce mandou / recebeu / viraram favoritos" do
// cartao de amigos (o modelo social nao conta isso sem teto) e "recomendacoes
// TROCADAS" (so ha a contagem das RECEBIDAS: recomenda_n). Nada disso foi
// inventado: o que falta fica fora, e o quinto numero so aparece com o
// recomenda ativo.
// ---------------------------------------------------------------------------
// 2.0 (MOCKUP-perfil-stats.html, out/2026). Quadro 1: o cartao Amigos vira a
// TERCEIRA parada de foco (superficie clara, anel no rosto escolhido e "OK
// comparar com Fulano"), o periodo vira etiqueta de vidro e os cartoes entram
// com um fade curto em cascata. Variante A (duelo): OK no cartao abre "Voce e
// Fulano" na MESMA tela, com o seletor de amigo no cabecalho, dois duelos
// (horas e filmes do mes, os meus de PerfilDados e os dele de SvPerfil),
// "Vistos pelos dois" e "Match" de SvCmp, e os cartazes de "Gostou
// recentemente". FORA, porque nenhum dado do app os sustenta: "Series em
// curso" (o MEU numero nao existe em PerfilDados), os CARTAZES de "Vistos
// pelos dois" (o servidor so manda as contagens), streak e generos do amigo,
// e a variante B inteira (Mes/Ano: o snapshot e mensal). Ver o relatorio do
// commit.
#include "menu.h"
#include "perfil.h"
#include "idioma.h"
#include "idiomacod.h"
#include "anim.h"
#include "gfx.h"
#include "layout.h"
#include "text.h"
#include "tex_cache.h"
#include "ajustes.h"
#include "socialvis.h"
#include "svdesenho.h"
#include "recomenda.h"
#include "trakt.h"
#include "simkl.h"
#include "plrui.h"
#include "caca.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define PF_X             ajustes_conteudo_x()
#define PF_W            (NV_TELA_W-PF_X-NV_LEGACY_CONTENT_RIGHT)

// AS MEDIDAS DO MOCKUP (px de 1920x1080). Cabecalho a 56 do topo, avatar de
// 120 e 28 ate o nome; numeros a 36 do cabecalho, 120 de altura e 20 de vao;
// os tres cartoes de baixo a 24 deles, o primeiro com 400 de largura.
// LAYOUT DINAMICA: a pilula da barra ("‹ Perfil e Stats") ocupa o topo
// esquerdo (y 44..104), exatamente onde o avatar e o nome comecam. Quando ela
// esta la, o conteudo todo desce o que for preciso para ficar 20 abaixo dela
// (o mesmo vao de busca.c/buTopoEsq). Sem a pilula, PF_DY e 0 e nada muda.
static float pfDy(void);
#define PF_DY             pfDy()
#define PF_TOPO          (56.0f + PF_DY)
#define PF_AVATAR        120.0f
#define PF_NUM_Y        (212.0f + PF_DY)
#define PF_NUM_H         120.0f
#define PF_NUM_GAP        20.0f
#define PF_CARD_Y       (356.0f + PF_DY)
#define PF_CARD_MIN_H    324.0f
#define PF_CARD_GAP       24.0f
#define PF_CAL_W         400.0f
#define PF_PAD_X          26.0f   // .tile: 22 x 26
#define PF_PAD_Y          22.0f
#define PF_RAIO           26.0f
// Ritmo: quadrados de 34 com 8 de vao, sete por linha, a partir do primeiro dia
// do mes (o mockup nao alinha por dia da semana nem tem cabecalho de dias).
#define PF_CEL            34.0f
#define PF_CEL_GAP         8.0f
#define PF_CAL_LINHAS      6
// Mais vistos: linha de 10/12 de recuo, miniatura 84x48 e 8 entre linhas.
#define PF_LIN_H          68.0f
#define PF_LIN_GAP         8.0f
#define PF_MINI_W         84.0f
#define PF_MINI_H         48.0f
// Amigos: rostos de 62 com 16 de vao.
#define PF_ROSTO          62.0f
#define PF_ROSTOS_MAX      6

#define PF_AVISO_H        54.0f
#define PF_AVISO_Y       (NV_TELA_H-PF_AVISO_H-24.0f)
#define PF_CONTEUDO_H    (PF_AVISO_Y-6.0f)

#define PF_SECOES          3   // 0 = calendario, 1 = mais vistos, 2 = amigos
// O DUELO (variante A): cartoes de 150, cartazes 2:3 de 150x225.
#define PF_DUELO_H       150.0f
#define PF_PW            150.0f
#define PF_PH            225.0f
#define PF_PGAP           20.0f
#define PF_QUEM           48.0f

// PALETA DE TEXTO: tres niveis, e so.
#define PF_FORTE   244
#define PF_MEDIO   206
#define PF_FRACO   140
// Os cinzas do mockup sobre o cartao de vidro: rotulo em caixa alta a 48%,
// apoio a 45-55%.
#define PF_ROTULO  126
#define PF_APOIO   122

// CORPOS QUE O text.c NAO TEM (44, 52, 22, 19, 15...). Rasterizados no estilo
// mais proximo ACIMA, com o mesmo peso, e desenhados reduzidos (txtEsc) — um
// estilo novo em text.c seria conflito com quem mexe nele em paralelo. Os
// numeros sao os que dao, na Inter estatica daqui, a MESMA largura medida na
// captura do mockup (a Inter variavel do navegador e ~9% mais larga abaixo de
// 24 px; ver escTitulo em agendaui.c).
#define PF_ESC_NOME   (53.0f / NV_FT_TITULO2)    // 52/800
#define PF_ESC_BIG    (45.0f / NV_FT_TITULO3)    // 44/800
#define PF_ESC_CARD   (23.0f / NV_FT_ROW_TITULO) // 22/700
#define PF_ESC_LIN    (20.5f / NV_FT_ROW_TITULO) // 19/600
#define PF_ESC_APOIO  (16.5f / NV_FT_CAPTION2)   // 15/400
#define PF_ESC_AMIGO  (19.5f / NV_FT_CAPTION2)   // 18/400
#define PF_ESC_DUELO  (37.0f / NV_FT_TITULO3)    // 36/700
#define PF_ESC_COMUM  (29.0f / NV_FT_TITULO3)    // 28/700
#define PF_ESC_QUEM   (22.0f / NV_FT_ROW_TITULO) // 21/600

static float pfDy(void) {
  float px, py, pw, ph;
  if (menu_pilula_rect(&px, &py, &pw, &ph) && py + ph + 20.0f > 56.0f)
    return py + ph + 20.0f - 56.0f;
  return 0.0f;
}

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

// O CARTAO AMIGOS (terceira parada) e o DUELO. `amigo` e o indice em socialvis
// do rosto com o anel (resumo) ou do amigo do duelo; no duelo quem manda e o
// id (dId), porque o modelo social reordena (quem esta ao vivo vem antes).
enum { PF_RESUMO = 0, PF_DUELO };
static int amigo, modo;
static float focoAmigos;
static int dLinha, dCol;             // duelo: 0 = seletor de amigo, 1 = cartazes
static char dId[96];
static SvPerfil dPerf;               // copia relida a cada segundo, nunca por quadro
static int dTem;
static unsigned dRev = ~0u;
static Uint32 dLido;
static float dFoco[SV_FILA_MAX];
static int pedirAmigo, dTemPedido;
static PerfilDestaque dPedido;
// ENTRADA (o .fd do mockup): segundos desde que a cena apareceu; os cartoes
// surgem em cascata de 70 ms e as barras do duelo crescem em 1 s. So alfa e
// largura: nenhuma camada a mais.
static float tCena, tBarra;
// A PORTA DA CACA (plano social-pessoal, 8.3): um botao no canto de cima,
// alcancado por CIMA de qualquer parada (secao PF_SEC_CACA). Existe mesmo sem
// Trakt: a Caca le o que o app ja sabe, nao o snapshot desta tela.
#define PF_SEC_CACA 3
static int pedirCaca, secaoAntes;
static float focoCaca;
static int rostosCabem(void);
static int cartazesCabem(void);

static int limitar(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }
static void texto(TxtEstilo e, const char *s, int cor, float x, float y, float a) {
  txt_desenhar_alpha(txt_linha(e, s ? s : "", cor, cor, cor, 255), x, y, a);
}
static void corta(TxtEstilo e,const char *s,int c,float x,float y,float w,float a) {
  txt_desenhar_alpha(txt_linha_corta(e,s,c,c,c,255,w),x,y,a);
}
// TEXTO REDUZIDO (ver PF_ESC_*): a textura do estilo-base desenhada em `esc`,
// com o canto encaixado no pixel como em txt_desenhar_alpha.
static void txtEsc(TxtLinha l, float x, float y, float esc, float a) {
  GfxRect r;
  if (!l.tex || a <= 0.001f) return;
  r.x = floorf(x + 0.5f); r.y = floorf(y + 0.5f);
  r.w = (float)l.w * esc; r.h = (float)l.h * esc;
  gfx_rect(r, l.tex, GFX_TEXTO, 0, 0, 0, 0.0f, 1, 1, 1, a);
}
// Linha cortada para caber em `larg` DEPOIS de reduzida, ja desenhada.
// Devolve a largura desenhada.
static float escrever(TxtEstilo e, const char *s, int c, float x, float y,
                      float larg, float esc, float a) {
  TxtLinha l = txt_linha_corta(e, s ? s : "", c, c, c, 255, larg / esc);
  txtEsc(l, x, y, esc, a);
  return (float)l.w * esc;
}
// O ROTULO EM CAIXA ALTA do mockup (.cap2: 15/600, espacado .06em): o TXT_MINI
// espacado, com a caixa alta de idiomacod.h (acento e cirilico inclusos).
static float rotuloCaixa(const char *s, int c, float x, float y, float a) {
  char up[160];
  idioma_maiusc_em(ajustes_idioma(), up, sizeof up, i18n(s));
  return txt_tracking(TXT_MINI, up, c, c, c, x, y, a, 0.9f);
}
static void acento(float *r, float *g, float *b) { ajustes_acento(r, g, b); }
// A altura da linha de apoio (15 no mockup) ja reduzida. "Hg" pela caixa com
// ascendente e descendente, como em agendaui.c.
static float altLinhaApoio(void) {
  return (float)txt_linha(TXT_CAPTION2, "Hg", PF_APOIO, PF_APOIO, PF_APOIO, 255).h * PF_ESC_APOIO;
}
static int acentoI(float v) { return (int)(v * 255.0f + 0.5f); }

// O BRILHO DO FOCO do botao unico do estado vazio: a mancha difusa no realce.
static void brilhoFoco(GfxRect r, float f, float a) {
  float ar, ag, ab;
  GfxRect luz;
  if (f <= 0.01f) return;
  acento(&ar, &ag, &ab);
  luz.x = r.x - r.h * 0.9f; luz.y = r.y - r.h * 0.9f;
  luz.w = r.w + r.h * 1.8f; luz.h = r.h * 2.8f;
  gfx_rect(luz, 0, GFX_SOMBRA, 1.0f, 0, 0, 0.5f,
           ar, ag, ab, 0.35f * f * a);
}

// O CARTAO DE VIDRO (.vid do mockup). Sobre a arte do mockup ele e
// rgba(14,15,18,.72); aqui atras da tela ha o fundo liso do app, e o vidro
// escuro sumiria nele. Entao o vidro sai como um veu frio que da, sobre o
// fundo, o valor medido no mockup (~#18191b). O solido e o do mockup, #15161a.
static void cartao(GfxRect r, float a) {
  float raio = PF_RAIO / r.h;
  if (ajustes_vidro()) gfx_cor(r, raio, .92f, .93f, 1.0f, .04f * a);
  else gfx_cor(r, raio, .082f, .086f, .102f, a);
}
// A LINHA EM FOCO dentro de um cartao (.row.foco): branco a 12% no vidro, o
// cinza opaco da regra 2 no solido. Sem aro e sem realce: o foco e superficie.
static void linhaFoco(GfxRect r, float raio, float f, float a) {
  if (f <= 0.01f) return;
  if (ajustes_vidro()) gfx_cor(r, raio, 1, 1, 1, .12f * f * a);
  else gfx_cor(r, raio, .169f, .176f, .204f, f * a);
}
static void numero(char *s, size_t n, int v) { snprintf(s, n, "%d", v < 0 ? 0 : v); }
static void tempo(char *s, size_t n, int minutos) {
  if (minutos < 0) minutos = 0;
  if (minutos < 60) snprintf(s, n, "%d min", minutos);
  else snprintf(s, n, "%dh %02dmin", minutos / 60, minutos % 60);
}
// Quantos destaques cabem: a coluna tem altura fixa e nao rola.
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
  amigo = modo = 0; focoAmigos = 0; dLinha = dCol = 0; dId[0] = 0;
  dTem = 0; dRev = ~0u; dLido = 0; memset(dFoco, 0, sizeof dFoco);
  pedirAmigo = dTemPedido = 0; tCena = tBarra = 0;
  pedirCaca = 0; focoCaca = 0; secaoAntes = 0;
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
  amigo = 0; modo = PF_RESUMO; pedirAmigo = dTemPedido = 0;
  tCena = tBarra = 0;
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
    secao=item=dia=0; modo=PF_RESUMO; amigo=0;
    escolhido=-1;erro[0]=0;estado=PERFIL_ESTADO_CARREGANDO;return;
  }
  // O CONTEUDO CHEGOU AGORA (antes era esqueleto ou vazio): a cascata de
  // entrada roda a partir daqui. Uma atualizacao por cima de dado que ja estava
  // na tela nao repete a animacao.
  if (!temDados) tCena = 0;
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
  if(!temDados && secao != PF_SEC_CACA)secao=0;
  if (item >= nCards()) item = nCards() ? nCards() - 1 : 0;
  // Uma parada so existe se tiver filho focavel. Sem calendario o foco cai nos
  // cards; sem cards ele volta para o calendario. (Na tela antiga as secoes 0 e
  // 3 nao tinham filho nenhum: focar nelas so mexia um ponto de 14px.)
  if (secao == 0 && dados.nDias == 0 && nCards() > 0) secao = 1;
  if (secao == 1 && nCards() == 0) secao = 0;
  if (secao == 0 && dados.nDias == 0 && nCards() == 0 && rostosCabem() > 0) secao = 2;
}

int perfil_item_selecionado(PerfilDestaque *saida) {
  if (dTemPedido) {
    dTemPedido = 0;
    if (saida) *saida = dPedido;
    return 1;
  }
  if (escolhido < 0 || escolhido >= dados.nDestaques) return 0;
  if (saida) *saida = dados.destaques[escolhido];
  escolhido = -1;
  return 1;
}

// --- o cartao Amigos e o duelo -----------------------------------------------

static void recarregarDuelo(void) {
  dTem = dId[0] ? socialvis_perfil(dId, &dPerf) : 0;
  dRev = socialvis_revisao();
  if (!dTem) dPerf.nGostou = 0;
  if (dCol >= cartazesCabem()) dCol = cartazesCabem() ? cartazesCabem() - 1 : 0;
  if (dLinha == 1 && cartazesCabem() == 0) dLinha = 0;
}
// Abre (ou troca) o amigo do duelo. O pedido ao servidor sai a cada abertura,
// como em amigoperfil_abrir: o numero do mes dele muda.
static void duelo(int i, int novo) {
  const SvAmigo *am = socialvis_amigo(i);
  if (!am || !am->id[0]) return;
  snprintf(dId, sizeof dId, "%s", am->id);
  amigo = i; modo = PF_DUELO;
  dLinha = 0; dCol = 0;
  memset(dFoco, 0, sizeof dFoco);
  socialvis_abrir_perfil(dId);
  recarregarDuelo();
  dLido = SDL_GetTicks();
  tBarra = 0;
  if (novo) tCena = 0;
}
static void sairDuelo(void) {
  int m = rostosCabem();
  modo = PF_RESUMO; secao = 2; tCena = 0;
  if (amigo >= m) amigo = m > 0 ? m - 1 : 0;
  if (m <= 0) secao = nCards() ? 1 : 0;
}
static void eventoDuelo(SDL_Keycode k, int ok) {
  int n = socialvis_n_amigos();
  if (dLinha == 0) {
    if (k == SDLK_LEFT)  { if (amigo > 0) duelo(amigo - 1, 0); return; }
    if (k == SDLK_RIGHT) { if (amigo + 1 < n) duelo(amigo + 1, 0); return; }
    if (k == SDLK_DOWN)  { if (cartazesCabem() > 0) dLinha = 1; return; }
    if (ok) pedirAmigo = 1;
    return;
  }
  if (k == SDLK_UP)    { dLinha = 0; return; }
  if (k == SDLK_LEFT)  { if (dCol > 0) dCol--; return; }
  if (k == SDLK_RIGHT) { if (dCol + 1 < cartazesCabem()) dCol++; return; }
  if (ok && dCol < dPerf.nGostou && dPerf.gostou[dCol].imdb[0]) {
    memset(&dPedido, 0, sizeof dPedido);
    snprintf(dPedido.id, sizeof dPedido.id, "%s", dPerf.gostou[dCol].imdb);
    snprintf(dPedido.titulo, sizeof dPedido.titulo, "%s", dPerf.gostou[dCol].titulo);
    dTemPedido = 1;
  }
}

// Volta do perfil do amigo (Voltar la): o duelo reabre com a mesma pessoa,
// e nao o resumo do comeco. Amigo que sumiu do modelo deixa o resumo.
void perfil_voltar_ao_duelo(void) {
  int k = dId[0] ? socialvis_amigo_indice(dId) : -1;
  if (k >= 0 && temDados) duelo(k, 1);
}

int perfil_pediu_caca(void) { int v = pedirCaca; pedirCaca = 0; return v; }

int perfil_pediu_amigo(char *id, size_t tam) {
  if (!pedirAmigo) return 0;
  pedirAmigo = 0;
  if (id && tam) snprintf(id, tam, "%s", dId);
  return dId[0] != 0;
}

void perfil_evento(const SDL_Event *e) {
  if (!aberto || !e || e->type != SDL_KEYDOWN) return;
  SDL_Keycode k = e->key.keysym.sym;
  int ok = k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE;
  if (k == SDLK_ESCAPE || k == SDLK_AC_BACK || k == SDLK_BACKSPACE || k == SDLK_DELETE) {
    // Voltar no duelo volta ao resumo, com o anel no mesmo amigo.
    if (modo == PF_DUELO) { sairDuelo(); return; }
    perfil_fechar(); return;
  }
  if (modo == PF_DUELO) { eventoDuelo(k, ok); return; }
  if (secao == PF_SEC_CACA) {
    if (ok) pedirCaca = 1;
    else if (k == SDLK_DOWN) secao = temDados ? secaoAntes : 0;
    else if (k == SDLK_LEFT) perfil_fechar();
    return;
  }
  // CIMA na borda de cima de qualquer parada sobe para o botao da Caca.
  if (k == SDLK_UP && (!temDados || (secao == 0 && dia < 7) || (secao == 1 && item == 0) ||
                       secao == 2)) {
    secaoAntes = secao; secao = PF_SEC_CACA; return;
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
                         if (nCards()) secao=1; else if (rostosCabem()>0) secao=2;
                         return; }
    if (k==SDLK_UP)    { if (dia>=7) dia-=7; return; }
    if (k==SDLK_DOWN)  { if (dia+7<dados.nDias) dia+=7; return; }
    return;
  }
  if (secao == 1) {
    if (k==SDLK_LEFT)  { if (dados.nDias>0) secao=0; else perfil_fechar(); return; }
    if (k==SDLK_RIGHT) { if (rostosCabem()>0) secao=2; return; }
    if (k==SDLK_UP)    { if (item>0) item--; return; }
    if (k==SDLK_DOWN)  { if (item+1<nCards()) item++; return; }
    if (k==SDLK_RETURN || k==SDLK_KP_ENTER || k==SDLK_SPACE) {
      if (nCards() > 0) escolhido = item;
      return;
    }
    return;
  }
  // A TERCEIRA PARADA: o cartao Amigos. Esquerda/direita andam pelos rostos
  // (o anel), OK abre o duelo com o rosto escolhido.
  if (secao == 2) {
    if (k==SDLK_LEFT) {
      if (amigo>0) amigo--;
      else if (nCards()) secao=1;
      else if (dados.nDias>0) secao=0;
      else perfil_fechar();
      return;
    }
    if (k==SDLK_RIGHT) { if (amigo+1<rostosCabem()) amigo++; return; }
    if (ok) { duelo(amigo, 1); return; }
    return;
  }
  if (k == SDLK_LEFT) perfil_fechar();
}

void perfil_atualizar(float dt, Uint32 agora) {
  int reduzida=ajustes_animacoes_reduzidas();
  if (aberto) { tCena += dt; tBarra += dt; }
  entrada = anim_mola(entrada, aberto ? 1.0f : 0.0f, dt, NV_MOLA_TELA);
  if (!aberto && entrada < 0.002f) entrada = 0;
  focoCal = anim_mola(focoCal, secao == 0 ? 1.0f : 0.0f, dt, NV_MOLA_FOCO);
  for (int i = 0; i < PERFIL_MAX_DESTAQUES; i++)
    focoItem[i] = anim_mola(focoItem[i], secao == 1 && i == item ? 1.0f : 0.0f,
                            dt, NV_MOLA_FOCO);
  focoAmigos = anim_mola(focoAmigos, modo == PF_RESUMO && secao == 2 ? 1.0f : 0.0f,
                         dt, NV_MOLA_FOCO);
  focoCaca = reduzida ? (secao == PF_SEC_CACA) : anim_mola(focoCaca, secao == PF_SEC_CACA ? 1.0f : 0.0f,
                                                           dt, NV_MOLA_FOCO);
  for (int i = 0; i < SV_FILA_MAX; i++)
    dFoco[i] = anim_mola(dFoco[i], modo == PF_DUELO && dLinha == 1 && i == dCol ? 1.0f : 0.0f,
                         dt, NV_MOLA_FOCO);
  if(reduzida) {
    entrada=aberto?1:0;
    focoCal=(secao==0);
    for(int i=0;i<PERFIL_MAX_DESTAQUES;i++)focoItem[i]=(secao==1&&i==item);
    focoAmigos = modo == PF_RESUMO && secao == 2;
    for (int i = 0; i < SV_FILA_MAX; i++) dFoco[i] = modo == PF_DUELO && dLinha == 1 && i == dCol;
  }
  // O cartao de amigos le o MESMO modelo da fileira e do painel Social. Barato
  // por quadro: so refaz quando a fonte muda (ver socialvis.h).
  if (!aberto) return;
  socialvis_atualizar();
  if (modo == PF_DUELO) {
    // O MODELO REORDENA (quem esta ao vivo sobe): o indice segue o id. Amigo
    // que sumiu do modelo encerra o duelo em vez de mostrar outra pessoa.
    int k = socialvis_amigo_indice(dId);
    if (k < 0) sairDuelo();
    else {
      amigo = k;
      // A resposta do servidor chega no fio sem mexer na revisao: rele a cada
      // segundo, como amigoperfil.c.
      if (dRev != socialvis_revisao() || agora - dLido > 1000u) { dLido = agora; recarregarDuelo(); }
    }
  } else if (secao == 2) {
    int m = rostosCabem();
    if (m <= 0) secao = nCards() ? 1 : 0;
    else if (amigo >= m) amigo = m - 1;
  }
}

// Titulo de pagina, so quando nao ha pessoa para por no cabecalho (carregando,
// vazio, sem identidade). 56/800 em y 64, como Agenda e Biblioteca.
static void tituloPagina(float a) {
  TxtLinha t = txt_linha(TXT_TITULO2, "Perfil e Stats", 245, 245, 243, 255);
  if (!menu_pilula_titulo()) txt_desenhar_alpha(t, PF_X, 64.0f, a);   // Dinamica: na pilula
}

// AS COLUNAS: numeros em ate cinco, cartoes de baixo em 400 | 1fr | 1fr.
static int nNumeros(void) { return recomenda_ativo() ? 5 : 4; }
static GfxRect rNumero(int i) {
  int n = nNumeros();
  float w = (PF_W - PF_NUM_GAP * (float)(n - 1)) / (float)n;
  return (GfxRect){ PF_X + (w + PF_NUM_GAP) * (float)i, PF_NUM_Y, w, PF_NUM_H };
}
static float alturaCartoes(void) {
  // A coluna dos mais vistos manda: titulo, as QUATRO linhas possiveis e o
  // recuo. Pelo teto e nao por nCards(): o esqueleto (sem dado) e a tela
  // pronta tem de cair na mesma caixa, e o cartao nao muda de tamanho quando
  // chegam tres destaques em vez de quatro.
  float h = PF_PAD_Y + 28.0f + 8.0f + (float)PERFIL_MAX_DESTAQUES * (PF_LIN_H + PF_LIN_GAP)
          - PF_LIN_GAP + PF_PAD_Y;
  return h > PF_CARD_MIN_H ? h : PF_CARD_MIN_H;
}
static GfxRect rCartao(int i) {
  float w = (PF_W - PF_CAL_W - PF_CARD_GAP * 2.0f) * 0.5f, h = alturaCartoes();
  if (i == 0) return (GfxRect){ PF_X, PF_CARD_Y, PF_CAL_W, h };
  return (GfxRect){ PF_X + PF_CAL_W + PF_CARD_GAP + (w + PF_CARD_GAP) * (float)(i - 1),
                    PF_CARD_Y, w, h };
}
// Quantos rostos o cartao Amigos desenha: cabem quantos a largura deixa, com o
// vao do mockup. A navegacao usa a MESMA conta, para o anel nunca cair num
// rosto que nao foi desenhado.
static int rostosCabem(void) {
  GfxRect r = rCartao(2);
  float w = r.w - PF_PAD_X * 2.0f;
  int n = socialvis_n_amigos(), m = n < PF_ROSTOS_MAX ? n : PF_ROSTOS_MAX;
  while (m > 1 && m * PF_ROSTO + (m - 1) * 16.0f > w - 8.0f) m--;
  return m;
}

// O DUELO: a linha "Voce ... Fulano", os quatro cartoes e o cartao de cartazes.
static float yQuem(void)   { return PF_NUM_Y; }
static GfxRect rDuelo(int i) {
  float w = (PF_W - PF_NUM_GAP * 3.0f) / 4.0f;
  return (GfxRect){ PF_X + (w + PF_NUM_GAP) * (float)i, yQuem() + PF_QUEM + 14.0f, w, PF_DUELO_H };
}
static GfxRect rCartazes(void) {
  float y = rDuelo(0).y + PF_DUELO_H + PF_CARD_GAP;
  return (GfxRect){ PF_X, y, PF_W, PF_PAD_Y + 28.0f + 16.0f + PF_PH + 10.0f + 22.0f + PF_PAD_Y };
}
static int cartazesCabem(void) {
  int n = dTem ? dPerf.nGostou : 0, cabe;
  if (n > SV_FILA_MAX) n = SV_FILA_MAX;
  cabe = (int)((PF_W - PF_PAD_X * 2.0f + PF_PGAP) / (PF_PW + PF_PGAP));
  return n < cabe ? n : cabe;
}

// A CASCATA DE ENTRADA: o cartao k surge 70 ms depois do k-1, em 350 ms.
static float surge(int k) {
  float t;
  if (ajustes_animacoes_reduzidas()) return 1.0f;
  t = (tCena - 0.07f * (float)k) / 0.35f;
  if (t <= 0.0f) return 0.0f;
  if (t >= 1.0f) return 1.0f;
  return t * t * (3.0f - 2.0f * t);
}
// As barras do duelo crescem em 1 s, comecando 300 ms depois do cartao.
static float cresce(void) {
  float t;
  if (ajustes_animacoes_reduzidas()) return 1.0f;
  t = (tBarra - 0.3f) / 1.0f;
  if (t <= 0.0f) return 0.0f;
  if (t >= 1.0f) return 1.0f;
  t = 1.0f - t;
  return 1.0f - t * t * t;
}
// Nome curto para o seletor e para a dica: corta em `max` bytes sem partir
// um caractere UTF-8 no meio, com reticencias.
static void nomeCurto(char *dst, size_t tam, const char *src, size_t max) {
  size_t n = strlen(src ? src : "");
  if (!src) src = "";
  if (n <= max || tam < 8) { snprintf(dst, tam, "%s", src); return; }
  while (max > 0 && ((unsigned char)src[max] & 0xc0) == 0x80) max--;
  if (max + 4 > tam) max = tam - 4;
  memcpy(dst, src, max);
  memcpy(dst + max, "\xe2\x80\xa6", 4);
}

// O titulo de um cartao (22/700), no recuo de cima. Devolve o y abaixo dele.
static float tituloCartao(GfxRect r, const char *s, float a) {
  TxtLinha l = txt_linha(TXT_ROW_TITULO, s, PF_FORTE, PF_FORTE, PF_FORTE, 255);
  txtEsc(l, r.x + PF_PAD_X, r.y + PF_PAD_Y, PF_ESC_CARD, a);
  return r.y + PF_PAD_Y + (float)l.h * PF_ESC_CARD;
}

// Esqueleto: os MESMOS retangulos, nas MESMAS coordenadas do conteudo real, em
// NV_COR_ESQUELETO. Sem isso a tela inteira salta quando o dado chega.
static void desenharLoading(Uint32 agora, float a) {
  float pulso = ajustes_animacoes_reduzidas()?.55f:.48f+.12f*sinf((float)agora*.004f);
  float p = pulso * a;
  #define PF_ESQ(r_,raio_) gfx_cor((r_),(raio_),NV_COR_ESQUELETO_R, \
                                   NV_COR_ESQUELETO_G,NV_COR_ESQUELETO_B,p)
  PF_ESQ(((GfxRect){PF_X,PF_TOPO,PF_AVATAR,PF_AVATAR}), .5f);
  PF_ESQ(((GfxRect){PF_X+PF_AVATAR+28,PF_TOPO+22,360,48}), NV_RAIO_BADGE);
  PF_ESQ(((GfxRect){PF_X+PF_AVATAR+28,PF_TOPO+82,260,20}), NV_RAIO_BADGE);
  for (int i=0;i<nNumeros();i++) PF_ESQ(rNumero(i), PF_RAIO/PF_NUM_H);
  for (int i=0;i<3;i++) { GfxRect r = rCartao(i); PF_ESQ(r, PF_RAIO/r.h); }
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
  corta(TXT_TITULO3, titulo, PF_FORTE, PF_X, 212, PF_W*.72f, a);
  txt_bloco(TXT_BODY, corpo, PF_MEDIO, PF_MEDIO, PF_MEDIO,
            PF_X, 296, 840, NV_LD_BODY, a, 3);
  // Botao unico da tela, sempre em foco: preenchido na cor de realce com
  // texto pela regra do tema, com brilho curto e sem anel.
  { float ar,ag,ab; acento(&ar,&ag,&ab);
    brilhoFoco(btn,1.0f,a);
    gfx_cor(btn,NV_RAIO_PILL,ar,ag,ab,a); }
  texto(TXT_DET_BOTAO,"OK · Tentar novamente",ajustes_tinta_foco(),PF_X+28,472,a);
}

// O AVATAR DO CABECALHO: 120 px com o anel do mockup (5 de vao escuro e 3 no
// acento: box-shadow 0 0 0 5px, 0 0 0 8px). Sem foto, a INICIAL sobre o
// violeta do mockup (#7c5cff); sem nome nenhum, o icone de perfil.
// `av` e o disco: o do cabecalho (com anel) ou o de 48 do "Voce" no duelo,
// para a mesma pessoa nao ter duas cores na mesma tela.
static void avatarEu(GfxRect av, int anel, float a) {
  float ar, ag, ab;
  GLuint tx = dados.avatar[0] ? tex_obter_larg(dados.avatar, av.w + 40.0f) : 0;
  acento(&ar, &ag, &ab);
  if (anel) gfx_anel_fora(av, 0.5f, 5.0f, 3.0f, ar, ag, ab, a);
  if (tx) {
    gfx_rect(av, 0, GFX_DISCO, 0, 0, 0, 0, .09f, .09f, .10f, a);
    gfx_tex_aspect_atual = tex_aspecto(dados.avatar);
    gfx_rect(av, tx, GFX_AVATAR, 0, 0, 0, 0, 1, 1, 1, a);
    gfx_tex_aspect_atual = 0;
    return;
  }
  gfx_rect(av, 0, GFX_DISCO, 0, 0, 0, 0, .486f, .361f, 1.0f, a);
  { const char *nm = dados.nome[0] ? dados.nome : dados.usuario;
    char ini[8] = "";
    if (nm[0]) {
      // O primeiro CARACTERE, nao o primeiro byte: "Élio" comeca com 2 bytes.
      size_t k = 1;
      while (nm[k] && ((unsigned char)nm[k] & 0xc0) == 0x80 && k < 4) k++;
      { char um[8]; memcpy(um, nm, k); um[k] = 0;
        idioma_maiusc_em(ajustes_idioma(), ini, sizeof ini, um); }
    }
    if (ini[0]) {
      float esc = av.w / PF_AVATAR;
      TxtLinha l = txt_linha(TXT_TITULO3, ini, 255, 255, 255, 255);
      txtEsc(l, av.x + (av.w - l.w * esc) * .5f, av.y + (av.h - l.h * esc) * .5f, esc, a);
    } else gfx_icone((GfxRect){ av.x + av.w * .25f, av.y + av.h * .25f, av.w * .5f, av.h * .5f },
                     "menu_profile", .92f, .92f, .95f, a); }
}
static void desenharAvatar(float a) {
  avatarEu((GfxRect){ PF_X, PF_TOPO, PF_AVATAR, PF_AVATAR }, 1, a);
}

// O CABECALHO: avatar, nome em 52/800, e "@usuario · Trakt · Simkl" (as contas
// que de fato estao ligadas nesta TV) em 19 a 55%. O periodo do resumo fica a
// direita, como etiqueta (o lugar do chip "Quem ve" do mockup — ver o topo).
// O SELETOR DE AMIGO do duelo (plrui_seg, o .seg do mockup), alinhado a
// direita. Mostra uma janela de ate oito nomes que contem o escolhido e cabe
// em `maxW`. Devolve a largura desenhada.
static float seletorAmigos(float xDir, float cy, float maxW, float a) {
  static char nomes[8][28];
  const char *rot[8];
  int cont[8], n = socialvis_n_amigos(), m = n < 8 ? n : 8, ini, i;
  float w = 0.0f;
  for (; m >= 1; m--) {
    ini = amigo < m ? 0 : amigo - m + 1;
    for (i = 0; i < m; i++) {
      const SvAmigo *am = socialvis_amigo(ini + i);
      nomeCurto(nomes[i], sizeof nomes[i], am ? am->nome : "", 16);
      rot[i] = nomes[i]; cont[i] = -1;
    }
    w = plrui_seg(rot, cont, m, amigo - ini, dLinha == 0, -1.0f, 0, a);
    if (w <= maxW || m == 1) break;
  }
  if (m < 1) return 0.0f;
  plrui_seg(rot, cont, m, amigo - ini, dLinha == 0, xDir - w, cy - 27.0f, a);
  return w;
}

// O botao "Caca" no canto direito, centrado em `cy`. "Caca · 2/3": quantas
// cacadas estao quase fechadas, quando ha. Devolve a largura (0 = nao desenhou).
static float botaoCaca(float cy, float a) {
  char rot[64];
  int idx[3], ns = caca_sugestoes(idx, 3), n = caca_n();
  float w;
  if (modo == PF_DUELO) return 0.0f;
  if (ns > 0) snprintf(rot, sizeof rot, i18n("Caça · %d quase"), ns);
  else if (n > 0) snprintf(rot, sizeof rot, i18n("Caça · %d"), n);
  else snprintf(rot, sizeof rot, "%s", i18n("Caça"));
  w = plrui_botao_largura(rot, "aj_crosshair");
  plrui_botao(PF_X + PF_W - w, cy - 30.0f, rot, "aj_crosshair", focoCaca, a);
  return w;
}

static void desenharCabecalho(float a) {
  float tx = PF_X + PF_AVATAR + 28.0f, cy = PF_TOPO + PF_AVATAR * 0.5f;
  float wTxt = PF_W - PF_AVATAR - 28.0f - 520.0f;   // periodo + botao da Caca
  char sub[200];
  size_t k = 0;
  TxtLinha nome, l2;
  const char *nm = dados.nome[0] ? dados.nome : dados.usuario[0] ? dados.usuario : NULL;
  desenharAvatar(a);
  sub[0] = 0;
  // NO DUELO o subtitulo e "Voce e Fulano" e a direita e o seletor de amigo.
  if (modo == PF_DUELO) {
    const SvAmigo *am = socialvis_amigo(amigo);
    float ws = seletorAmigos(PF_X + PF_W, cy, PF_W * 0.5f, a);
    wTxt = PF_W - PF_AVATAR - 28.0f - ws - 40.0f;
    snprintf(sub, sizeof sub, i18n("Você e %s"), am ? am->nome : "");
    k = strlen(sub);
  } else if (dados.nome[0] && dados.usuario[0])
    k += (size_t)snprintf(sub + k, sizeof sub - k, "@%s", dados.usuario);
  if (modo != PF_DUELO && trakt_ativo() && k < sizeof sub)
    k += (size_t)snprintf(sub + k, sizeof sub - k, "%sTrakt", k ? " \xc2\xb7 " : "");
  if (modo != PF_DUELO && simkl_ativo() && k < sizeof sub)
    k += (size_t)snprintf(sub + k, sizeof sub - k, "%sSimkl", k ? " \xc2\xb7 " : "");
  nome = nm ? txt_linha_corta(TXT_TITULO2, nm, 245, 245, 243, 255, wTxt / PF_ESC_NOME)
            : txt_linha_corta(TXT_TITULO2, "Perfil e Stats", 245, 245, 243, 255, wTxt / PF_ESC_NOME);
  l2 = txt_linha_corta(TXT_CAPTION2, sub, PF_FRACO, PF_FRACO, PF_FRACO, 255, wTxt);
  { float hN = (float)nome.h * PF_ESC_NOME, h2 = sub[0] ? (float)l2.h + 4.0f : 0.0f;
    float y = cy - (hN + h2) * 0.5f;
    txtEsc(nome, tx, y, PF_ESC_NOME, a);
    if (sub[0]) txt_desenhar_alpha(l2, tx, y + hN + 4.0f, a); }
  // O PERIODO numa etiqueta de vidro (a .ilha do mockup: pilula, sem aro).
  { float wc = botaoCaca(cy, a);
  if (modo != PF_DUELO && dados.periodo[0]) {
    TxtLinha m = txt_linha(TXT_MINI, "Hg", 104, 104, 104, 255);
    float w = txt_tracking(TXT_HERO_SEC, dados.periodo, 104, 104, 104, -1.0f, 0, 0, 2.2f);
    GfxRect pr = { PF_X + PF_W - w - 40.0f - (wc > 0.0f ? wc + 16.0f : 0.0f), cy - 24.0f, w + 40.0f, 48.0f };
    if (ajustes_vidro()) gfx_cor(pr, 0.5f, .92f, .93f, 1.0f, .06f * a);
    else gfx_cor(pr, 0.5f, .082f, .086f, .102f, a);
    txt_tracking(TXT_HERO_SEC, dados.periodo, 140, 140, 140,
                 pr.x + 20.0f, cy - (float)m.h * 0.5f - 2.0f, a, 2.2f);
  } }
}

// OS NUMEROS: um cartao de vidro por fato, valor em 44/800 e o rotulo em caixa
// alta embaixo. O streak no acento — o unico numero colorido da tela.
static void desenharNumeros(float a) {
  char val[5][64];
  const char *rot[5];
  int n = nNumeros();
  float ar, ag, ab;
  acento(&ar, &ag, &ab);
  if (dados.minutos > 0) { tempo(val[0], sizeof val[0], dados.minutos); rot[0] = "assistidos"; }
  else { numero(val[0], sizeof val[0], dados.plays); rot[0] = "reproduções"; }
  numero(val[1], sizeof val[1], dados.filmes);    rot[1] = "filmes";
  numero(val[2], sizeof val[2], dados.episodios); rot[2] = "episódios";
  numero(val[3], sizeof val[3], dados.streakAtual);
  rot[3] = dados.streakCompleto ? "dias em sequência" : "dias em sequência no mês";
  numero(val[4], sizeof val[4], recomenda_n());   rot[4] = "recomendações";
  float a0 = a;
  for (int i = 0; i < n; i++) {
    GfxRect r = rNumero(i);
    a = a0 * surge(i);
    if (a <= 0.002f) continue;
    TxtLinha v = i == 3
      ? txt_linha(TXT_TITULO3, val[i], acentoI(ar), acentoI(ag), acentoI(ab), 255)
      : txt_linha(TXT_TITULO3, val[i], 245, 245, 243, 255);
    float vy = r.y + PF_PAD_Y - 2.0f;
    cartao(r, a);
    // Corta o valor na largura do cartao: "39h 44min" cabe em 5 colunas.
    if ((float)v.w * PF_ESC_BIG > r.w - PF_PAD_X * 2.0f)
      v = txt_linha_corta(TXT_TITULO3, val[i], 245, 245, 243, 255,
                          (r.w - PF_PAD_X * 2.0f) / PF_ESC_BIG);
    txtEsc(v, r.x + PF_PAD_X, vy, PF_ESC_BIG, a);
    rotuloCaixa(rot[i], PF_ROTULO, r.x + PF_PAD_X, vy + (float)v.h * PF_ESC_BIG + 2.0f, a);
  }
}

// O RITMO DO MES: um quadrado de 34 por dia, sete por linha, em TRES
// intensidades do acento (25/50/90% do mockup) pela fracao do dia mais cheio;
// dia sem nada no branco a 6%. O dia em foco fica BRANCO CHEIO e cresce 6%.
static void desenharAtividade(float a) {
  GfxRect r = rCartao(0);
  float y0, x0 = r.x + PF_PAD_X, ar, ag, ab, yLeg;
  int max = 0;
  char b[200];
  acento(&ar, &ag, &ab);
  cartao(r, a);
  y0 = tituloCartao(r, "Ritmo de atividade", a) + 18.0f;
  for (int i = 0; i < dados.nDias; i++) if (dados.atividade[i] > max) max = dados.atividade[i];
  for (int i = 0; i < dados.nDias; i++) {
    int col = i % 7, lin = i / 7;
    GfxRect c = { x0 + col * (PF_CEL + PF_CEL_GAP), y0 + lin * (PF_CEL + PF_CEL_GAP), PF_CEL, PF_CEL };
    float f = i == dia ? focoCal : 0.0f, esc = 1.0f + 0.06f * f;
    GfxRect dc = { c.x - c.w * (esc - 1.0f) * 0.5f, c.y - c.h * (esc - 1.0f) * 0.5f,
                   c.w * esc, c.h * esc };
    float raio = 9.0f / dc.h;
    if (lin >= PF_CAL_LINHAS) break;
    if (f < 0.99f) {
      unsigned v = dados.atividade[i];
      float k = 1.0f - f;
      if (!v) gfx_cor(dc, raio, 1, 1, 1, .06f * k * a);
      else gfx_cor(dc, raio, ar, ag, ab,
                   ((max > 2 && v * 3 <= (unsigned)max) ? .25f
                    : (max > 1 && v * 3 <= (unsigned)max * 2) ? .50f : .90f) * k * a);
    }
    if (f > 0.01f) gfx_cor(dc, raio, .94f, .94f, .93f, f * a);
  }
  { int linhas = (dados.nDias + 6) / 7;
    if (linhas > PF_CAL_LINHAS) linhas = PF_CAL_LINHAS;
    yLeg = y0 + linhas * PF_CEL + (linhas - 1) * PF_CEL_GAP + 14.0f; }
  // A LEGENDA: dias ativos e os dois generos mais vistos, numa linha so (o
  // mockup). Com o foco no ritmo, a linha de cima diz o dia em foco.
  if (secao == 0 && dados.nDias) {
    snprintf(b, sizeof b, i18n("Dia %d: %u reproduções"), dia + 1, dados.atividade[dia]);
    escrever(TXT_CAPTION2, b, PF_MEDIO, x0, yLeg, r.w - PF_PAD_X * 2.0f, PF_ESC_APOIO, a);
    yLeg += 22.0f;
  }
  // Os generos entram SO INTEIROS: um a um enquanto a linha couber na largura
  // do cartao ("Drama 34 · …" cortado no meio nao diz nada). Em ingles e
  // alemao a frase dos dias ja ocupa quase tudo, e ai a linha fica so com ela.
  { char tenta[200];
    float larg = r.w - PF_PAD_X * 2.0f;
    size_t k = (size_t)snprintf(b, sizeof b, i18n("%d de %d dias ativos no mês"),
                                dados.diasAtivosMes, dados.nDias);
    for (int g = 0; g < dados.nGeneros && g < 2 && k < sizeof b; g++) {
      snprintf(tenta, sizeof tenta, "%s \xc2\xb7 %s %d", b,
               i18n(dados.generos[g].nome), dados.generos[g].quantidade);
      if ((float)txt_largura(TXT_CAPTION2, tenta) * PF_ESC_APOIO > larg) break;
      k = (size_t)snprintf(b, sizeof b, "%s", tenta);
    }
    escrever(TXT_CAPTION2, b, PF_APOIO, x0, yLeg, larg, PF_ESC_APOIO, a); }
}

// MAIS VISTOS: linhas com a miniatura 16:9 de 84x48 (canto 8), titulo 19/600 e
// "T1E6 · 9 reproducoes" em 15. A linha em foco ganha a superficie clara.
static void desenharDestaques(float a) {
  GfxRect r = rCartao(1);
  int n = nCards();
  float y;
  cartao(r, a);
  y = tituloCartao(r, "Mais vistos", a) + 8.0f;
  if (!n) { escrever(TXT_CAPTION2, "Nenhum destaque neste período.", PF_APOIO,
                     r.x + PF_PAD_X, y + 8.0f, r.w - PF_PAD_X * 2.0f, PF_ESC_AMIGO, a); return; }
  for (int i = 0; i < n; i++) {
    float f = focoItem[i];
    GfxRect lr = { r.x + PF_PAD_X, y, r.w - PF_PAD_X * 2.0f, PF_LIN_H };
    GfxRect mini = { lr.x + 12.0f, lr.y + (PF_LIN_H - PF_MINI_H) * .5f, PF_MINI_W, PF_MINI_H };
    float tx = mini.x + PF_MINI_W + 16.0f, tw = lr.x + lr.w - 12.0f - tx;
    const char *art = dados.destaques[i].backdrop[0] ? dados.destaques[i].backdrop
                                                     : dados.destaques[i].poster;
    GLuint tex = art[0] ? tex_obter_larg(art, mini.w) : 0;
    char linha[200];
    TxtLinha t;
    linhaFoco(lr, 14.0f / lr.h, f, a);
    if (tex) { gfx_tex_aspect_atual = tex_aspecto(art);
               gfx_rect(mini, tex, GFX_CARD, 0, 0, 0, 8.0f / mini.h, 1, 1, 1, a);
               gfx_tex_aspect_atual = 0; }
    else gfx_cor(mini, 8.0f / mini.h, NV_COR_ESQUELETO_R, NV_COR_ESQUELETO_G,
                 NV_COR_ESQUELETO_B, a);
    // O "#N" do ranking nao volta: a ordem da lista JA e a posicao.
    snprintf(linha, sizeof linha, i18n("%d reproduções"), dados.destaques[i].plays);
    if (dados.destaques[i].detalhe[0]) {
      char junto[240];
      snprintf(junto, sizeof junto, "%s \xc2\xb7 %s", dados.destaques[i].detalhe, linha);
      snprintf(linha, sizeof linha, "%s", junto);
    }
    t = txt_linha_corta(TXT_ROW_TITULO, dados.destaques[i].titulo, 245, 245, 243, 255, tw / PF_ESC_LIN);
    { float hT = (float)t.h * PF_ESC_LIN, hM = altLinhaApoio();
      float ty = lr.y + (PF_LIN_H - hT - 2.0f - hM) * .5f;
      txtEsc(t, tx, ty, PF_ESC_LIN, a);
      escrever(TXT_CAPTION2, linha, f > 0.5f ? 150 : PF_APOIO, tx, ty + hT + 2.0f, tw,
               PF_ESC_APOIO, a); }
    y += PF_LIN_H + PF_LIN_GAP;
  }
}

// AMIGOS: os rostos (o mesmo desenho da fileira "Amigos assistindo" e do
// painel Social, com o anel vermelho de quem esta ao vivo), quantos sao e
// quantos estao vendo agora, e QUEM esta vendo o que. Tudo de socialvis — o
// modelo que as outras telas sociais ja leem. Sem amigos, a frase-convite que o
// Social ja usa.
static void desenharAmigos(Uint32 agora, float a) {
  GfxRect r = rCartao(2);
  int n = socialvis_n_amigos(), vivos = socialvis_n_ao_vivo();
  float y, x = r.x + PF_PAD_X, w = r.w - PF_PAD_X * 2.0f;
  cartao(r, a);
  // EM FOCO o cartao inteiro vira a superficie clara (.gl.foco), sem aro.
  if (n > 0) plrui_linha_foco(r, PF_RAIO, focoAmigos * a);
  y = tituloCartao(r, "Amigos", a);
  if (n > 0) {
    char c[80];
    TxtLinha l;
    if (vivos > 0) snprintf(c, sizeof c, "%d \xc2\xb7 %d %s", n, vivos, i18n("assistindo agora"));
    else snprintf(c, sizeof c, "%d", n);
    l = txt_linha(TXT_CAPTION2, c, PF_APOIO, PF_APOIO, PF_APOIO, 255);
    txtEsc(l, x + w - (float)l.w * PF_ESC_APOIO,
           y - (float)l.h * PF_ESC_APOIO - 2.0f, PF_ESC_APOIO, a);
  }
  y += 16.0f;
  if (n <= 0) {
    txt_bloco(TXT_CAPTION2,
              "Descubra o que seus amigos estão vendo.\nUma nova recomendação pode começar aqui.",
              PF_APOIO, PF_APOIO, PF_APOIO, x, y, w, 28.0f, a, 3);
    return;
  }
  { int k, m = rostosCabem();
    // O anel de foco (no acento) so no rosto escolhido, e so com o cartao em foco.
    for (k = 0; k < m; k++)
      svd_rosto((GfxRect){ x + 4.0f + k * (PF_ROSTO + 16.0f), y + 4.0f, PF_ROSTO, PF_ROSTO },
                socialvis_amigo(k), k == amigo ? focoAmigos : 0.0f, a, agora); }
  y += PF_ROSTO + 8.0f + 16.0f;
  // QUEM ESTA VENDO AGORA: o primeiro amigo ao vivo e o titulo dele, "Marina
  // esta vendo O Urso · T3E4". Sem ninguem ao vivo, a linha nao existe.
  for (int k = 0; k < n; k++) {
    const SvAmigo *am = socialvis_amigo(k);
    if (am && am->agora && am->nTit > 0) {
      char rest[260], ep[48] = "";
      float wn;
      socialvis_ep(&am->tit[0], ep, sizeof ep);
      if (ep[0]) snprintf(rest, sizeof rest, "%s %s \xc2\xb7 %s", i18n("está vendo"), am->tit[0].titulo, ep);
      else snprintf(rest, sizeof rest, "%s %s", i18n("está vendo"), am->tit[0].titulo);
      { TxtLinha ln = txt_linha_corta(TXT_HERO_SEC, am->nome, 245, 245, 243, 255, w * 0.4f);
        txtEsc(ln, x, y, 1.0f, a);
        wn = (float)ln.w + 6.0f; }
      escrever(TXT_CAPTION2, rest, 150, x + wn, y + 1.0f, w - wn, PF_ESC_AMIGO, a);
      break;
    }
  }
  // A DICA DO CARTAO, no pe e depois de um filete: "OK  comparar com Fulano"
  // (o rosto com o anel). Mais apagada fora do foco: ela diz o que o OK faz
  // quando o foco chegar aqui.
  { const SvAmigo *am = socialvis_amigo(amigo < rostosCabem() ? amigo : 0);
    char nm[40], lab[120];
    const char *ks[1] = { "OK" }, *ls[1] = { lab };
    float yc = r.y + r.h - PF_PAD_Y - 15.0f;
    if (!am) return;
    nomeCurto(nm, sizeof nm, am->nome, 18);
    snprintf(lab, sizeof lab, i18n("comparar com %s"), nm);
    gfx_cor((GfxRect){ x, yc - 15.0f - 16.0f, w, 1.0f }, 0, 1, 1, 1, .08f * a);
    plrui_dicas(ks, ls, 1, x, yc, 0, a * (.55f + .45f * focoAmigos)); }
}

// --- o duelo (variante A do mockup) --------------------------------------------

// Barra do duelo: trilho branco 10%, a parte de quem lidera no acento e a do
// outro em branco 50%. `frac` 0..1 ja multiplicada pelo crescimento.
static void barraDuelo(GfxRect r, float frac, int lider, float a) {
  float ar, ag, ab;
  GfxRect f = r;
  gfx_cor(r, 0.5f, 1, 1, 1, .10f * a);
  if (frac <= 0.0f) return;
  f.w = r.w * (frac > 1.0f ? 1.0f : frac);
  if (f.w < r.h) f.w = r.h;
  acento(&ar, &ag, &ab);
  if (lider) gfx_cor(f, 0.5f, ar, ag, ab, a);
  else gfx_cor(f, 0.5f, 1, 1, 1, .50f * a);
}

// UM DUELO: rotulo em caixa alta, o meu numero a esquerda e o dele a direita
// (quem lidera no acento), e as duas barras. Valor < 0 = nao ha dado: sai "—",
// sem barra e sem lider — comparar com um numero ausente seria inventar.
static void cartaoDuelo(GfxRect r, const char *rot, int eu, int ele, int horas, float a) {
  char sa[32], sb[32];
  int lider = (eu >= 0 && ele >= 0 && eu != ele) ? (eu > ele ? 0 : 1) : -1;
  float ar, ag, ab, x = r.x + PF_PAD_X, w = r.w - PF_PAD_X * 2.0f, y;
  TxtLinha la, lb;
  acento(&ar, &ag, &ab);
  cartao(r, a);
  rotuloCaixa(rot, PF_ROTULO, x, r.y + PF_PAD_Y, a);
  if (eu < 0) snprintf(sa, sizeof sa, "\xe2\x80\x94");
  else if (horas) snprintf(sa, sizeof sa, i18n("%d h"), eu / 60);
  else snprintf(sa, sizeof sa, "%d", eu);
  if (ele < 0) snprintf(sb, sizeof sb, "\xe2\x80\x94");
  else if (horas) snprintf(sb, sizeof sb, i18n("%d h"), ele / 60);
  else snprintf(sb, sizeof sb, "%d", ele);
  la = lider == 0 ? txt_linha(TXT_TITULO3, sa, acentoI(ar), acentoI(ag), acentoI(ab), 255)
                  : txt_linha(TXT_TITULO3, sa, 245, 245, 243, 255);
  lb = lider == 1 ? txt_linha(TXT_TITULO3, sb, acentoI(ar), acentoI(ag), acentoI(ab), 255)
       : ele < 0  ? txt_linha(TXT_TITULO3, sb, PF_FRACO, PF_FRACO, PF_FRACO, 255)
                  : txt_linha(TXT_TITULO3, sb, 245, 245, 243, 255);
  y = r.y + PF_PAD_Y + 26.0f;
  txtEsc(la, x, y, PF_ESC_DUELO, a);
  txtEsc(lb, x + w - (float)lb.w * PF_ESC_DUELO, y, PF_ESC_DUELO, a);
  y += (float)la.h * PF_ESC_DUELO + 10.0f;
  if (eu >= 0 && ele >= 0) {
    int max = eu > ele ? eu : ele;
    float g = cresce();
    barraDuelo((GfxRect){ x, y, w, 10.0f }, max > 0 ? g * (float)eu / (float)max : 0.0f, lider == 0, a);
    barraDuelo((GfxRect){ x, y + 16.0f, w, 10.0f }, max > 0 ? g * (float)ele / (float)max : 0.0f, lider == 1, a);
  }
}

// UM CARTAO DE COMPARACAO (SvCmp): o valor grande quando ha dado, ou "—" e o
// MOTIVO (privado, poucos pares, carregando...) — o mesmo texto do perfil do
// amigo, socialvis_cmp_texto.
static void cartaoCmp(GfxRect r, int qual, float a) {
  const SvCmp *c = &dPerf.cmp[qual];
  char val[96], sub[96];
  int ok = 0;
  float ar, ag, ab, x = r.x + PF_PAD_X, w = r.w - PF_PAD_X * 2.0f, y = r.y + PF_PAD_Y + 26.0f;
  acento(&ar, &ag, &ab);
  cartao(r, a);
  rotuloCaixa(qual == SV_CMP_MATCH ? "Match" : "Vistos pelos dois", PF_ROTULO, x, r.y + PF_PAD_Y, a);
  socialvis_cmp_texto(qual, c, val, sizeof val, &ok);
  if (!ok || !dTem) {
    TxtLinha l = txt_linha(TXT_TITULO3, "\xe2\x80\x94", PF_FRACO, PF_FRACO, PF_FRACO, 255);
    txtEsc(l, x, y, PF_ESC_DUELO, a);
    if (!dTem) snprintf(val, sizeof val, "%s", i18n("Perfil indisponível"));
    escrever(TXT_CAPTION2, val, PF_APOIO, x, y + (float)l.h * PF_ESC_DUELO + 8.0f, w, PF_ESC_AMIGO, a);
    return;
  }
  if (qual == SV_CMP_MATCH) {
    TxtLinha l;
    char pct[16];
    snprintf(pct, sizeof pct, "%d%%", c->pct);
    l = txt_linha(TXT_TITULO3, pct, acentoI(ar), acentoI(ag), acentoI(ab), 255);
    txtEsc(l, x, y, PF_ESC_DUELO, a);
    snprintf(sub, sizeof sub, i18n("%d de %d reações iguais"), c->iguais, c->total);
    escrever(TXT_CAPTION2, sub, PF_APOIO, x, y + (float)l.h * PF_ESC_DUELO + 8.0f, w, PF_ESC_AMIGO, a);
    return;
  }
  // "Vistos pelos dois": "14 filmes · 3 séries", ja traduzido.
  { TxtLinha l = txt_linha_corta(TXT_TITULO3, val, 245, 245, 243, 255, w / PF_ESC_COMUM);
    txtEsc(l, x, y + 4.0f, PF_ESC_COMUM, a); }
}

// O MOTIVO de nao haver numero do amigo, numa frase (os textos sao os do
// perfil do amigo). NULL quando os numeros estao la.
static const char *motivoDuelo(char *b, size_t tam) {
  if (!dTem) return "Perfil indisponível";
  if (dPerf.minutosMes >= 0 || dPerf.filmesMes >= 0) return NULL;
  if (dPerf.compartilha == 0) return "Esta pessoa não compartilha sua atividade.";
  if (dPerf.estado == SV_PERFIL_INDO) return "Carregando atividade…";
  if (dPerf.estado == SV_PERFIL_NAO_ACHOU && dPerf.porOnde != SV_FONTE_NUVIO) {
    snprintf(b, tam, i18n("Aqui só aparece o que %s compartilha."), socialvis_fonte_nome(dPerf.porOnde));
    return b;
  }
  if (dPerf.estado == SV_PERFIL_FALHA) return "Não foi possível atualizar. Tente novamente.";
  // Sem resposta do servidor (recomenda desligado, ou amigo so do tracker):
  // dizer que nao ha perfil, e nao deixar dois "—" sem explicacao.
  return "Perfil indisponível";
}

static void desenharDuelo(Uint32 agora, float a) {
  const SvAmigo *am = socialvis_amigo(amigo);
  float y = yQuem(), cy = y + PF_QUEM * 0.5f;
  char mb[200];
  const char *motivo;
  (void)agora;
  if (!am) return;
  // QUEM: eu a esquerda, o amigo a direita (a ordem dos numeros embaixo).
  { float a1 = a * surge(0);
    TxtLinha voce = txt_linha(TXT_ROW_TITULO, "Você", 245, 245, 243, 255);
    TxtLinha ele = txt_linha_corta(TXT_ROW_TITULO, am->nome, 245, 245, 243, 255, (PF_W * 0.3f) / PF_ESC_QUEM);
    float we = (float)ele.w * PF_ESC_QUEM;
    avatarEu((GfxRect){ PF_X, y, PF_QUEM, PF_QUEM }, 0, a1);
    txtEsc(voce, PF_X + PF_QUEM + 12.0f, cy - (float)voce.h * PF_ESC_QUEM * 0.5f, PF_ESC_QUEM, a1);
    svd_avatar((GfxRect){ PF_X + PF_W - PF_QUEM, y, PF_QUEM, PF_QUEM }, am->avatar, am->nome, am->id, a1);
    txtEsc(ele, PF_X + PF_W - PF_QUEM - 12.0f - we, cy - (float)ele.h * PF_ESC_QUEM * 0.5f,
           PF_ESC_QUEM, a1);
    motivo = motivoDuelo(mb, sizeof mb);
    if (motivo) {
      TxtLinha l = txt_linha_corta(TXT_CAPTION2, motivo, PF_APOIO, PF_APOIO, PF_APOIO, 255,
                                   (PF_W * 0.4f) / PF_ESC_AMIGO);
      txtEsc(l, PF_X + (PF_W - (float)l.w * PF_ESC_AMIGO) * 0.5f,
             cy - (float)l.h * PF_ESC_AMIGO * 0.5f, PF_ESC_AMIGO, a1);
    } }
  // OS QUATRO CARTOES. Os meus numeros sao os do snapshot do mes (PerfilDados);
  // os dele, os do mes que o servidor manda (SvPerfil). "Series em curso" do
  // mockup NAO entra: o meu lado nao existe em PerfilDados.
  // Sem minutos mas com reproducoes, a duracao e desconhecida: "—", nao "0 h".
  { int ele = dTem ? dPerf.minutosMes : -1;
    if (surge(1) > 0.002f)
      cartaoDuelo(rDuelo(0), "assistidas neste mês", dados.minutos > 0 ? dados.minutos : dados.plays > 0 ? -1 : 0, ele, 1, a * surge(1)); }
  if (surge(2) > 0.002f)
    cartaoDuelo(rDuelo(1), "filmes vistos", dados.filmes, dTem ? dPerf.filmesMes : -1, 0, a * surge(2));
  if (surge(3) > 0.002f) cartaoCmp(rDuelo(2), SV_CMP_COMUM, a * surge(3));
  if (surge(4) > 0.002f) cartaoCmp(rDuelo(3), SV_CMP_MATCH, a * surge(4));
  // GOSTOU RECENTEMENTE: os cartazes do que o amigo marcou como gostei.
  { GfxRect r = rCartazes();
    float a5 = a * surge(5), x = r.x + PF_PAD_X, yy;
    int n = cartazesCabem(), c;
    if (a5 <= 0.002f) return;
    cartao(r, a5);
    yy = tituloCartao(r, "Gostou recentemente", a5) + 16.0f;
    if (n == 0) {
      const char *st = !dTem ? "Perfil indisponível"
                     : dPerf.estado == SV_PERFIL_INDO ? "Carregando atividade…"
                     : dPerf.compartilha == 0 ? "Esta pessoa não compartilha sua atividade."
                     : "Nada compartilhado por aqui ainda";
      escrever(TXT_CAPTION2, st, PF_APOIO, x, yy + 8.0f, r.w - PF_PAD_X * 2.0f, PF_ESC_AMIGO, a5);
      return;
    }
    for (c = 0; c < n; c++) {
      const SvEvento *e = &dPerf.gostou[c];
      GfxRect pr = { x + (float)c * (PF_PW + PF_PGAP), yy, PF_PW, PF_PH };
      float f = dFoco[c], raio = 14.0f / PF_PH;
      // O foco no cartaz e o aro branco do mockup (3 px a 75%), sem halo.
      if (f > 0.01f) gfx_anel_fora(pr, raio, 0.0f, 3.0f, 1, 1, 1, .75f * f * a5);
      svd_poster(pr, e->poster[0] ? e->poster : e->arte, raio, a5);
      escrever(TXT_CAPTION2, e->titulo, f > 0.5f ? PF_MEDIO : PF_APOIO, pr.x, pr.y + PF_PH + 10.0f,
               PF_PW, PF_ESC_APOIO, a5);
    }
  }
}

void perfil_desenhar(Uint32 agora) {
  if (entrada <= .002f) return;
  float a=entrada;
  gfx_cor((GfxRect){0,0,NV_TELA_W,NV_TELA_H},0,
          NV_COR_FUNDO_R,NV_COR_FUNDO_G,NV_COR_FUNDO_B,a);
  // UM recorte, aberto aqui e fechado aqui: gfx_sem_recorte() DESLIGA a
  // tesoura, nao devolve a de fora.
  gfx_recorte(0,0,NV_TELA_W,PF_CONTEUDO_H);
  if(carregando && !temDados) { desenharLoading(agora,a); botaoCaca(64.0f + 34.0f, a); }
  else if(!temDados) { desenharVazio(a); botaoCaca(64.0f + 34.0f, a); }
  else if (modo == PF_DUELO) {
    desenharCabecalho(a);
    desenharDuelo(agora, a);
  } else {
    if (temIdentidade) desenharCabecalho(a);
    else { tituloPagina(a); botaoCaca(64.0f + 34.0f, a); }
    desenharNumeros(a);
    if (surge(5) > 0.002f) desenharAtividade(a * surge(5));
    if (surge(6) > 0.002f) desenharDestaques(a * surge(6));
    if (surge(7) > 0.002f) desenharAmigos(agora, a * surge(7));
  }
  gfx_sem_recorte();

  // NO DUELO o pe e a linha de dicas do mockup, no lugar do aviso.
  if (modo == PF_DUELO && temDados) {
    static const char *const ks0[3] = { "\xe2\x86\x90 \xe2\x86\x92", "OK", "Voltar" };
    static const char *const ls0[3] = { "Trocar de amigo", "Ver perfil", "Perfil" };
    static const char *const ks1[2] = { "OK", "Voltar" };
    static const char *const ls1[2] = { "Abrir", "Perfil" };
    if (dLinha == 1) plrui_dicas(ks1, ls1, 2, PF_X, PF_AVISO_Y + PF_AVISO_H * 0.5f, 0, a);
    else plrui_dicas(ks0, ls0, 3, PF_X, PF_AVISO_Y + PF_AVISO_H * 0.5f, 0, a);
    return;
  }

  // O AVISO de dado parcial/erro, numa pilula de vidro no pe. A linha de dicas
  // de navegacao ("Setas: navegar...") saiu com o mockup, como na Agenda.
  char avisoBuf[320];
  const char *aviso=NULL;
  if(erro[0]){
    if(estado==PF_STALE)snprintf(avisoBuf,sizeof avisoBuf,i18n("Atualização indisponível · mostrando o último resumo recebido. %s"),erro);
    else snprintf(avisoBuf,sizeof avisoBuf,"%s",erro);
    aviso=avisoBuf;
  } else if(carregando)aviso="Atualizando histórico sem interromper o conteúdo anterior…";
  else aviso=dados.aviso[0]?dados.aviso:dados.parcial?"Histórico parcial: os totais consideram somente os registros carregados.":NULL;
  if(aviso){
    GfxRect r = { PF_X, PF_AVISO_Y, PF_W, PF_AVISO_H };
    if (ajustes_vidro()) gfx_cor(r, .5f, .92f, .93f, 1.0f, .04f * a);
    else gfx_cor(r, .5f, .082f, .086f, .102f, a);
    { float esc = PF_ESC_APOIO;
      TxtLinha l = txt_linha_corta(TXT_CAPTION2, aviso, PF_APOIO, PF_APOIO, PF_APOIO, 255,
                                   (PF_W - 48.0f) / esc);
      txtEsc(l, PF_X + 24.0f, PF_AVISO_Y + (PF_AVISO_H - (float)l.h * esc) * .5f, esc, a); }
  }
}
