// Rail lateral fixa do Nuvio 1.0.1 legacy, com overlay expansível para
// navegação por D-pad.
//
// Duas animacoes independentes, e a separacao e o que da o movimento certo:
//   - `desliza` tira a barra da borda esquerda (posicao);
//   - `expande` troca a largura de "so icone" para "icone + rotulo".
// No tvOS a barra recolhida mostra apenas os icones e so alarga quando ganha o
// foco. Aqui ela nasce fora da tela, entao os dois acontecem quase juntos — mas
// com molas de rigidez diferente, de forma que a largura ATRASA em relacao a
// entrada. E esse atraso que produz a leitura "entrou e entao se abriu"; com uma
// mola so, a barra aparece ja no tamanho final e o efeito some.
//
// Icones derivados dos SVGs originais do sidebar, com alpha e recortes reais.
#include "horafmt.h"
#include "menu.h"
#include "ilha.h"
#include "iconeapp.h"
#include "perfis.h"
#include "tex_cache.h"
#include "gfx.h"
#include "text.h"
#include "idioma.h"
#include "anim.h"
#include "layout.h"
#include "segurar.h"
#include "ajustes.h"
#include "botoes.h"
#include "ponteiro.h"
#include "home.h"
#include "colecoes.h"
#define NV_ESCALA_TELA   // o arquivo inteiro mede pela tela virtual (escala.h)
#include "escala.h"
#include <math.h>
#include <string.h>

// Escala propria do menu, independente do "Tamanho da interface". A Moderna
// amplia em 20% a ilha inteira (incluindo texto e alvos), mantendo a margem.
// Apple TV e Padrao conservam a escala de 90% e suas medidas anteriores.
// A tela virtual mede 1920/fator x1080/fator; desenho e ponteiro usam o mesmo
// fator. Quem conversa com o resto do app (menu_barra_borda,
// menu_pilula_rect) devolve px da tela REAL, ja com o fator.
// As medidas dos layouts classicos sao as do mockup NA TELA: cada uma entra
// aqui dividida pelo fator (MK), e o que o usuario ve e o px do CSS.
#define NV_MENU_ESCALA_BASE 0.9f
// Apple TV sidebar: 20% larger at UI scale <= 90%; 100% and up unchanged.
static float tvAumento(void) {
  return ajustes_home_layout() == HOME_LAYOUT_DINAMICA && gfx_escala_ui_base() <= 0.9f + 0.001f ? 1.2f : 1.0f;
}
static float menuEscala(void) {
  return NV_MENU_ESCALA_BASE * (ajustes_home_layout() == HOME_LAYOUT_MODERNA ? NV_MENU_MODERNA_AUMENTO : 1.0f) * tvAumento() * fminf(gfx_escala_ui_base(), 1.0f) * gfx_escala_txt();
}
#define NV_MENU_ESCALA (menuEscala())
#define MK(v) ((v) / NV_MENU_ESCALA_BASE)
#undef NV_TELA_W
#undef NV_TELA_H
#define NV_TELA_W (NV_LAYOUT_REAL_W / NV_MENU_ESCALA)
#define NV_TELA_H (NV_LAYOUT_REAL_H / NV_MENU_ESCALA)

// O MENU DOS LAYOUTS CLASSICOS (dono, 03/10, sobre o mockup "Glass UI — ilha",
// design/glass-ilha/glass-ilha.html, telas 1 e 2):
//   "a Moderna pode ser essa [rail recolhido + menu aberto] e a Normal pode
//    ser essa outra [o painel aberto], so que nao flutuante: ela saindo da
//    lateral, tambem do meio, centralizada."
//
// MODERNA: uma ilha flutuante em x48, centrada na altura, 20% maior.
// Fechada e a pilula vertical de icones (base88 de largura, raio44);
// com o foco ela CRESCE ate o painel de 320 (raio 32) com icone e rotulo, na
// mola subamortecida da ilha do relogio (o "pulo" da Dynamic Island).
// PADRAO: o MESMO painel, colado na borda esquerda (lado esquerdo reto, cantos
// da direita arredondados) e centrado na altura (altura = conteudo). Fechado,
// com a rail fixa, e a mesma pilula estreita colada na borda; abrindo, ela
// sai da lateral ate a largura do painel. Com a rail recolhida o painel inteiro
// desliza de fora da tela.
// O layout Dinamica (barra da Apple TV, mais abaixo) NAO usa nada disto.
//
// MEDIDAS DO MOCKUP (CSS .ilha, .it e os estilos inline do <nav>):
//   - painel: padding 14, linhas de 60 com 4 de vao; rail 88 / aberto 320;
//   - linha aberta: pilula de 292 (raio 30), icone de TRACO 26 (mt_*.png,
//     tools/icones-menu.sh) com 20 de recuo, 18 ate o rotulo Inter 22/500 em
//     (243,242,239) a 72 %; em foco branco cheio;
//   - rail: circulo de 60 centrado, icone no meio; o ativo branco cheio;
//   - foco = superficie clara (vidro: branco a 14 %; solido: #2b2d34 opaco);
//   - pagina ativa = ponto de 5 px no acento: aberta a 6 px da ponta esquerda
//     da linha, no meio da altura; na rail 8 px acima da base do circulo;
//   - fio antes do perfil: aberta 1 px a 8 % com 10 de margem e 14 de recuo;
//     na rail um traco de 40 a 10 % com 8 de margem;
//   - perfil: aberta linha de 72, avatar de 40, nome em --fg cheio e a acao
//     em cinza a 45 %; na rail so o avatar de 44.
#define NV_MENU_X           (48.0f / NV_MENU_ESCALA)
#define NV_MENU_PAD         MK(14.0f)
#define NV_MENU_W_RAIL      MK(88.0f)
#define NV_MENU_W_ABERTO   MK(320.0f)
#define NV_MENU_RAIO_RAIL   MK(44.0f)
#define NV_MENU_RAIO_ABERTO MK(32.0f)
#define NV_MENU_ITEM_H      MK(60.0f)
#define NV_MENU_ITEM_VAO     MK(4.0f)
#define NV_MENU_ICONE       MK(26.0f)
// Centro do icone a partir da borda do painel: na rail o meio do circulo de
// 60 (14 + 30); aberta, 14 de padding + 20 de recuo + meio icone. O icone anda
// 3 px quando o painel cresce, como no mockup.
#define NV_MENU_ICONE_RAIL  (NV_MENU_PAD + NV_MENU_ITEM_H * 0.5f)               // 44
#define NV_MENU_ICONE_ABRE  (NV_MENU_PAD + MK(20.0f) + NV_MENU_ICONE * 0.5f)        // 47
#define NV_MENU_ROTULO_X    (NV_MENU_PAD + MK(20.0f) + NV_MENU_ICONE + MK(18.0f))       // 78
#define NV_MENU_ROTULO_MAX  (NV_MENU_W_ABERTO - NV_MENU_ROTULO_X - NV_MENU_PAD - MK(20.0f))
#define NV_MENU_FIO_RAIL     MK(8.0f)
#define NV_MENU_FIO_ABRE    MK(10.0f)
// Cor do texto e dos icones em repouso: o --fg do mockup a 72 %.
#define NV_MENU_FG_R      (243.0f / 255.0f)
#define NV_MENU_FG_G      (242.0f / 255.0f)
#define NV_MENU_FG_B      (239.0f / 255.0f)
#define NV_MENU_INATIVO    0.72f
// O RESTO DA TELA ESCURECE POUCO: uma luz ESCURA (GFX_LUZ, com nv_dither)
// nascendo fora da tela a esquerda e morrendo antes do meio. Encosta no painel
// o bastante para ele se destacar do conteudo e some sem degrau. Nada de
// faixas de gfx_cor empilhadas: o painel de 8 bits da OLED mostra cada uma
// como um degrau.
#define NV_MENU_VEU        0.70f
#define NV_MENU_VEU_ALC  1700.0f
// TEMPO DO VEU, com relogio proprio (rampa reta, medida na folha de contexto
// da referencia: fechar ~150 ms, abrir ~230 ms). A FORMA do painel nao usa
// esta rampa: ela anda na mola da ilha do relogio (NV_MENU_MOLA_*).
#define NV_MOLA_MENU_DESFOCO 60.0f
#define NV_MENU_ABRIR_MS  230.0f
#define NV_MENU_FECHAR_MS 150.0f
// A MOLA DA ILHA (ilha.c, ILHA_MOLA_W / ILHA_MOLA_Z): subamortecida, passa um
// pouco do alvo e volta. Na Moderna o painel "pula" como a ilha do relogio; na
// Padrao ele e uma gaveta presa na borda e anda na mesma frequencia, criticamente
// amortecido (sem repique contra a borda).
#define NV_MENU_MOLA_W    10.0f
#define NV_MENU_MOLA_Z     0.72f

// "Inicio" sem acento era erro de portugues NA TELA. E "Busca", nao "Buscar":
// os outros tres sao substantivos (Biblioteca, Ajustes) e o verbo destoava.
// Rotulos e ordem conferidos na referencia.
static const char *ROTULOS[MENU_N] = { "Início", "Explorar", "Guia TV", "Esportes", "Busca", "Biblioteca", "Agenda", "Perfil e Stats", "Ajustes" };

// ITEM ESCONDIDO (#162): Explorar, Guia, Agenda e Perfil somem da barra quando
// desligados nos Ajustes. Inicio, Busca, Biblioteca e Ajustes ficam sempre —
// sem os Ajustes nao haveria como trazer os outros de volta. Esconder so tira
// a linha: desenho, alvos do ponteiro e setas pulam o item, e o painel encolhe.
static int mostra(int i) {
  switch (i) {
    case MENU_EXPLORAR: return ajustes_menu_explorar();
    case MENU_GUIA:     return ajustes_menu_guia();
    case MENU_AGENDA:   return ajustes_menu_agenda();
    case MENU_PERFIL:   return ajustes_menu_perfil();
    default:            return 1;
  }
}
static int visiveis(void) {
  int i, n = 0;
  for (i = 0; i < MENU_N; i++) n += mostra(i);
  return n;
}
// RODAPE: quem esta usando o app, e a porta para trocar. Ele e um item de
// FOCO a mais, no indice MENU_N — nao entrou no enum de proposito, porque
// trocar de perfil nao e uma aba do app e ninguem deve poder "navegar" para
// ela como destino.
#define NV_MENU_RODAPE_H    MK(72.0f)
#define NV_MENU_AVATAR      MK(40.0f)
#define NV_MENU_AVATAR_RAIL MK(44.0f)
#define NV_MENU_FOCOS      (MENU_N + 1)
#define MENU_RODAPE         MENU_N
// Layout Dinamica: as pastas de Streaming entram como focos depois do rodape
// (MENU_ST0 + k). So a barra da Apple TV usa; a rail classica nao as conhece.
#define NV_MENU_ST_MAX      24
#define MENU_ST0            NV_MENU_FOCOS
#define NV_MENU_FOCOS_TV   (NV_MENU_FOCOS + NV_MENU_ST_MAX)

static int   pediuTrocar = 0;
static int   aberto  = 0;
static int   destino = MENU_INICIO;
static int   linha   = MENU_INICIO;   // destaque; so vira destino ao escolher
static int   mudou   = 0;
// POR CIMA DE UMA CAMADA (a pagina do titulo, dono 03/10): a barra abre sobre
// ela sem fecha-la. Ai DIREITA so devolve o foco (nao escolhe o destaque), a
// rail fixa nao e desenhada (o painel nasce e some pela opacidade) e
// `escolheu` diz ao app que um destino foi escolhido — mesmo o atual — para
// ele fechar a camada. `sobre` dura ate a saida assentar (menu_sobre).
// `semRail`: a camada nao tem rail (a pagina do titulo); a barra nasce e some
// pela opacidade em vez de crescer da rail fixa.
static int   sobre   = 0, semRail = 0, escolheu = 0;
// `desliza`: rampa reta do veu (0 fechado .. 1 aberto). `expande`: a FORMA
// do painel (rail -> aberto), na mola da ilha; passa de 1 no repique.
static float desliza = 0.0f;
static float expande = 0.0f, expandeV = 0.0f;
static float animFoco[NV_MENU_FOCOS_TV];
static int   pediuColecao = -1;   // col_folder da pasta escolhida na barra
// SEGURAR OK EM "BUSCAR" abre o Spotlight (spotlight.h). E o caminho da LG,
// onde o microfone do Magic Remote e do sistema e nem todo controle tem a
// amarela. Por isso o OK em Buscar decide na SOLTURA: toque curto = a tela de
// Busca, como sempre; segurado NV_HOLD_MS = o Spotlight, com o dedo ainda no
// botao (menu_atualizar), como o menu do cartaz.
static int    buscaOk, buscaLongo, pediuSpot;
static Uint32 buscaDesde;
static void icone(int d, float cx, float cy, float s, float r, float g, float b, float a);
static void corAvatar(const char *hex, float *r, float *g, float *b);
static int  tvAtivo(void);
static int  tvOrdem(int *lista);
#ifdef NV_TOUCH_UI
static void tvToqueLimpar(void);
#endif
static void tvAtualizar(float dt);
static void tvDesenhar(void);
static void iconeTraco(int d, float cx, float cy, float s, float r, float g, float b, float a);
static void topoAtualizar(float dt);
static void topoDesenhar(void);
static void topoMover(int dir);

static int padrao(void) { return ajustes_home_layout() == HOME_LAYOUT_PADRAO; }
static int topo(void) { return ajustes_menu_topo(); }   // #461: barra no topo
static float limita01(float v) { return v < 0.0f ? 0.0f : v > 1.0f ? 1.0f : v; }

// A GEOMETRIA DO PAINEL numa largura `e` (0 = rail, 1 = aberto; o repique da
// mola passa um pouco de 1). Tudo medido a partir do canto (x, y) do painel.
typedef struct {
  float x, y, w, h, raio;
  float fioY, fioM;          // fio do rodape: y e margem
  float rodY, rodH;          // linha do rodape
} MenuGeo;
static float itensFim(void) {   // base da ultima linha, a partir do topo do painel
  return NV_MENU_PAD + visiveis() * (NV_MENU_ITEM_H + NV_MENU_ITEM_VAO) - NV_MENU_ITEM_VAO;
}
// A altura acompanha o repique da largura (`e` pode passar de 1): a ilha
// inteira "pula", nao so a largura.
static float alturaEm(float e) {
  float m = anim_mistura(NV_MENU_FIO_RAIL, NV_MENU_FIO_ABRE, e < 0.0f ? 0.0f : e);
  return itensFim() + NV_MENU_ITEM_VAO + 2.0f * m + MK(1.0f) + NV_MENU_ITEM_VAO
       + anim_mistura(NV_MENU_AVATAR_RAIL, NV_MENU_RODAPE_H, e < 0.0f ? 0.0f : e) + NV_MENU_PAD;
}
// `entrada` (0..1) so conta na Padrao com a rail recolhida: o painel desliza
// de fora da tela pela esquerda.
static MenuGeo geoEm(float e, float entrada) {
  MenuGeo g;
  float ec = limita01(e);
  g.w = anim_mistura(NV_MENU_W_RAIL, NV_MENU_W_ABERTO, e);
  g.h = alturaEm(e);
  g.raio = anim_mistura(NV_MENU_RAIO_RAIL, NV_MENU_RAIO_ABERTO, ec);
  if (padrao()) {
    // Centrado pela altura ABERTA: as linhas ficam no mesmo y nas duas
    // larguras (so o rodape cresce), entao nada anda na vertical ao abrir.
    g.x = -(1.0f - entrada) * g.w;
    g.y = floorf((NV_TELA_H - alturaEm(1.0f)) * 0.5f);
  } else {
    g.x = NV_MENU_X;
    g.y = (NV_TELA_H - g.h) * 0.5f;
  }
  g.fioM = anim_mistura(NV_MENU_FIO_RAIL, NV_MENU_FIO_ABRE, ec);
  g.fioY = g.y + itensFim() + NV_MENU_ITEM_VAO + g.fioM;
  g.rodY = g.fioY + MK(1.0f) + g.fioM + NV_MENU_ITEM_VAO;
  g.rodH = anim_mistura(NV_MENU_AVATAR_RAIL, NV_MENU_RODAPE_H, ec);
  return g;
}
static float linhaY(const MenuGeo *g, int k) {
  return g->y + NV_MENU_PAD + k * (NV_MENU_ITEM_H + NV_MENU_ITEM_VAO);
}
// A PILULA DA LINHA nas duas larguras: na rail o circulo de 60 (o mesmo x do
// padding), aberta a pilula de 292. O foco cresce do circulo para a pilula
// junto com o painel.
static GfxRect linhaRect(const MenuGeo *g, float y, float h, float e) {
  float lw = anim_mistura(h, g->w - 2.0f * NV_MENU_PAD, limita01(e));
  return (GfxRect){ g->x + NV_MENU_PAD, y, lw, h };
}

// FOCO DE LINHA = SUPERFICIE UM DEGRAU MAIS CLARA (regra 2 do Glass UI):
// branco a 14 % sobre o vidro (o .it.foco do mockup), #2b2d34 opaco sobre o
// solido. Sem anel, sem bloco cheio no acento — o acento fica para o ESTADO (o
// ponto da pagina ativa). Some por OPACIDADE (a mola `f`), nunca por cor:
// misturar duas cores com alpha cheio deixava a pilula que perdeu o foco como
// placa arrastada (25/09, C9).
// No SOLIDO o mockup ainda da a linha uma sombra curta (0 10 30 a 40 %) e um
// fio claro de 1 px por dentro do topo (inset 0 1px 0 a 6 %): a pilula clara
// por baixo e a opaca 1 px abaixo deixam so o fio, seguindo o arredondado.
static void focoLinha(GfxRect r, float f, float alpha) {
  if (f <= 0.01f || alpha <= 0.01f) return;
  if (ajustes_vidro()) { if (!gfx_foco_placa(r, 0.5f, f, alpha)) gfx_cor(r, 0.5f, 1, 1, 1, .14f * f * alpha); return; }
  { GfxRect s = { r.x - MK(30.0f), r.y + MK(10.0f - 30.0f), r.w + MK(60.0f), r.h + MK(60.0f) };
    gfx_rect(s, 0, GFX_SOMBRA, 1.0f, MK(42.0f) * gfx_escala(), 0, (r.h * 0.5f + MK(30.0f)) / s.h,
             0, 0, 0, .40f * f * alpha); }
  gfx_cor(r, 0.5f, .169f + .06f * (1.0f - .169f), .176f + .06f * (1.0f - .176f),
          .204f + .06f * (1.0f - .204f), f * alpha);
  gfx_cor((GfxRect){ r.x, r.y + MK(1.0f), r.w, r.h - MK(1.0f) }, 0.5f, .169f, .176f, .204f, f * alpha);
}

// PONTEIRO (#99). Passar por cima da rail ABRE o painel ja com o destaque na
// linha sob o cursor — o mesmo que ESQUERDA e depois cima/baixo. O clique e o
// OK de sempre (escolher). Com o painel aberto, clicar fora dele fecha, como o
// Voltar.
static void ponteiroLinha(int i, int b) {
  (void)b;
  if (i < 0 || i >= NV_MENU_FOCOS_TV) return;
#ifdef NV_TOUCH_UI
  tvToqueLimpar();
#endif
  if (!aberto) menu_abrir();
  linha = i;
}
static void ponteiroFora(int a, int b) { (void)a; (void)b; menu_fechar(); }
static void alvosDasLinhas(const MenuGeo *g, float x, float w) {
  int k = 0;
  if (!ponteiro_ativo()) return;
  for (int i = 0; i < MENU_N; i++) {
    if (!mostra(i)) continue;
    ponteiro_alvo(x, linhaY(g, k++), w, NV_MENU_ITEM_H, ponteiroLinha, NULL, i, 0);
  }
  ponteiro_alvo(x, g->rodY, w, g->rodH, ponteiroLinha, NULL, MENU_RODAPE, 0);
}

// O MATERIAL DA ILHA (CSS .ilha do mockup), o mesmo nas duas formas:
//   vidro:  miolo rgba(14,15,18,.80) — a arte aparece por ele — e a luz larga
//           e fraca do ::before (radial branco a 10 % centrado a 22 % da
//           largura, 40 % ACIMA do topo, morrendo a 60 % do raio de 80 % da
//           altura); sombra 0 14 40 a 36 %;
//   solido: #15161a opaco, sem a luz; sombra 0 10 30 a 45 %.
// Com "Vidro" e o vidro do PlxNative (gfx_vidro): a barra do topo no perfil
// de barra, a rail no de painel; obedece a "Opacidade do vidro", ao "Vidro
// fosco" e ao "Contorno do vidro" como as outras ilhas. Na Padrao o retangulo comeca
// FORA da tela a esquerda (o raio a mais), e os cantos da esquerda nunca
// aparecem: o lado colado na borda e reto.
static void desenhaMaterial(const MenuGeo *g, float a) {
  GfxRect r = { g->x, g->y, g->w, g->h };
  float raio;
  if (a <= 0.01f) return;
  if (padrao() && !topo()) { r.x -= g->raio; r.w += g->raio; }
  raio = g->raio / r.h;
  // O box-shadow do CSS: o retangulo deslocado `dy` e alargado `b` (o blur)
  // em cada lado, com a queda em PIXELS (GFX_SOMBRA com uPar.x): metade da
  // forca na borda do painel, como a gaussiana do navegador.
  { int vidro = ajustes_vidro();
    // The top chrome shares the island shadow: 0 10 30 at 34 %.
    float b = MK(vidro && !topo() ? 40.0f : 30.0f), dy = MK(vidro && !topo() ? 14.0f : 10.0f);
    GfxRect s = { r.x - b, r.y + dy - b, r.w + 2.0f * b, r.h + 2.0f * b };
    gfx_sombra_sob(s, 1.0f, b * 1.41f * gfx_escala(), (g->raio + b) / s.h,
                   0, 0, 0, (topo() ? 0.34f : vidro ? 0.36f : 0.45f) * a, r, g->raio, vidro ? 0.0f : a); }
  if (topo()) { gfx_vidro_camada = 1; gfx_chrome(r, raio, a); gfx_vidro_camada = 0; }
  else if (ajustes_vidro()) {
    // Fechada, os .80 do mockup (a rail fica sobre a arte do destaque).
    // Aberto, o painel cobre o TEXTO da home (titulo, sinopse, "Continuar
    // assistindo" em x 152..400), e a 80 % ele atravessava legivel entre os
    // rotulos (captura de tests/homelayouts_shot.sh): sobe a 86 %.
    float al = anim_mistura(.80f, .86f, limita01((g->w - NV_MENU_W_RAIL) / (NV_MENU_W_ABERTO - NV_MENU_W_RAIL)));
    gfx_vidro_camada = 1;   // frost over page glass too (detail pills, tabs, cards)
    gfx_vidro_material(r, raio, al, a);
    gfx_vidro_camada = 0;
  } else {
    gfx_cor(r, raio, 21.0f / 255.0f, 22.0f / 255.0f, 26.0f / 255.0f, a);
  }
}

static void desenhaRodape(const MenuGeo *g, float e, float alpha, float foco, float aTexto);

// O PAINEL nas duas larguras. `e` vai de 0 (rail: so icones) a 1 (aberto, com
// rotulos; passa um pouco no repique); `a` e a opacidade do painel todo;
// `focos` diz se as molas de foco valem (a rail parada nao tem foco: a
// primeira tecla abre o painel); `entrada` e o deslize da Padrao recolhida.
static void desenhaBarra(float e, float a, int focos, float entrada) {
  float ar, ag, ab;
  float ec = limita01(e);
  MenuGeo g = geoEm(e, entrada);
  // O rotulo entra com a largura, nao antes dela: `e` ao quadrado segura a
  // palavra ate o painel ter espaco de verdade, senao ela nasce espremida
  // contra o icone.
  float aRot = ec * ec * a;
  float icx = g.x + anim_mistura(NV_MENU_ICONE_RAIL, NV_MENU_ICONE_ABRE, ec);
  int k = 0;
  ajustes_acento(&ar, &ag, &ab);
  desenhaMaterial(&g, a);

  // Tudo daqui para baixo fica preso ao painel. Sem o recorte, o rotulo — que
  // e desenhado no x fixo do texto — vaza para o conteudo enquanto o painel
  // ainda esta estreito.
  gfx_recorte(g.x, g.y, g.w, g.h);
  for (int i = 0; i < MENU_N; i++) {
    if (!mostra(i)) continue;
    float y = linhaY(&g, k++);
    float f = focos ? animFoco[i] : 0.0f;
    float cy = y + NV_MENU_ITEM_H * 0.5f;
    GfxRect linhaR = linhaRect(&g, y, NV_MENU_ITEM_H, e);
    int atual = (i == destino);
    focoLinha(linhaR, f, a);
    // ONDE VOCE ESTA: um ponto de 5 px no acento (regra 4: acento so para
    // estado). Aberto, a 6 px da ponta esquerda da linha, no meio da altura
    // (.it.ativo::after); na rail, 8 px acima da base do circulo, centrado
    // (mockup 2). O ponto anda entre os dois lugares junto com a largura.
    if (atual) {
      float px = anim_mistura(icx - MK(2.5f), linhaR.x + MK(6.0f), ec);
      float py = anim_mistura(y + NV_MENU_ITEM_H - MK(8.0f + 5.0f), cy - MK(2.5f), ec);
      gfx_cor((GfxRect){ px, py, MK(5.0f), MK(5.0f) }, 0.5f, ar, ag, ab, a);
    }
    // TONS DO MOCKUP RENDERIZADO (o que o dono aprovou, nao so o CSS lido):
    //   - rail: icones no --fg a 72 % (estilo inline), o ativo branco cheio;
    //   - aberto: icone e rotulo no --fg CHEIO — no mockup a regra `.quadro a
    //     {color:inherit}` vence a `.it` de 72 %, entao toda linha sai em
    //     (243,242,239) a 100 %; a pagina ativa nao tem tom proprio, quem diz
    //     "voce esta aqui" e o ponto;
    //   - em foco: branco cheio (.it.foco).
    { float base = atual ? 1.0f : anim_mistura(NV_MENU_INATIVO, 1.0f, ec);
      float op = anim_mistura(base, 1.0f, f);
      float br = atual ? 1.0f - ec : 0.0f;   // o ativo da rail e branco puro
      float bw = br > f ? br : f;
      float tr = anim_mistura(NV_MENU_FG_R, 1.0f, bw), tg = anim_mistura(NV_MENU_FG_G, 1.0f, bw),
            tb = anim_mistura(NV_MENU_FG_B, 1.0f, bw);
      iconeTraco(i, icx, cy, NV_MENU_ICONE, tr, tg, tb, op * a);
      if (aRot > 0.01f) {
        // Texto ja rasterizado nao muda de cor: troca no meio da mola, e a
        // opacidade anda com ela.
        int emFoco = f > 0.5f;
        TxtLinha l = emFoco
          ? txt_linha_corta(TXT_BODY, ROTULOS[i], 255, 255, 255, 255, NV_MENU_ROTULO_MAX)
          : txt_linha_corta(TXT_BODY, ROTULOS[i], 243, 242, 239, 255, NV_MENU_ROTULO_MAX);
        txt_desenhar_alpha(l, g.x + NV_MENU_ROTULO_X, cy - l.h * 0.5f, aRot * op);
      } }
  }
  // O fio do rodape: na rail um traco de 40 centrado no icone (a 10 %);
  // aberto vai de 14 a 14 da pilula da linha (a 8 %).
  { float fw = anim_mistura(MK(40.0f), g.w - 2.0f * (NV_MENU_PAD + MK(14.0f)), ec);
    float fx = anim_mistura(icx - MK(20.0f), g.x + NV_MENU_PAD + MK(14.0f), ec);
    gfx_cor((GfxRect){ fx, g.fioY, fw, MK(1.0f) }, 0, 1, 1, 1, anim_mistura(.10f, .08f, ec) * a); }
  desenhaRodape(&g, e, a, focos ? animFoco[MENU_RODAPE] : 0.0f, aRot);
  gfx_sem_recorte();
}

int menu_iniciar(void) {
#ifdef NV_TOUCH_UI
  tvToqueLimpar();
#endif
  aberto = 0; destino = MENU_INICIO; linha = MENU_INICIO; mudou = 0;
  desliza = 0.0f; expande = 0.0f; expandeV = 0.0f;
  for (int i = 0; i < MENU_N; i++) animFoco[i] = 0.0f;
  return 1;
}

void menu_abrir(void) {
  if (aberto) return;
#ifdef NV_TOUCH_UI
  tvToqueLimpar();
#endif
  // O destaque comeca sempre no destino em vigor, nunca onde ficou da ultima
  // vez: a barra e um mapa de onde voce esta, e abrir com o destaque em outro
  // item faria o usuario ler que ja mudou de tela.
  linha = mostra(destino) ? destino : MENU_INICIO;
  aberto = 1;
  buscaOk = buscaLongo = 0;
  sobre = semRail = 0;   // abertura comum; menu_abrir_sobre liga depois
}
void menu_fechar(void) { aberto = 0; linha = destino; }
void menu_abrir_sobre(int semRailFixa) {
  if (aberto) return;
  menu_abrir();
  sobre = 1;
  semRail = semRailFixa;
}
static int assentado(void);
int menu_sobre(void) {
  if (sobre && !aberto && assentado()) sobre = semRail = 0;
  return sobre;
}
int menu_escolheu(void) { int v = escolheu; escolheu = 0; return v; }

int menu_aberto(void)  { return aberto; }
int menu_visivel(void) { return 1; }
int menu_destino(void) { return destino; }
void menu_definir_destino(int d) {
  if (d < 0 || d >= MENU_N) return;
  destino = d;
  if (!aberto) linha = d;
}
int menu_mudou_destino(void) { int m = mudou; mudou = 0; return m; }
const char *menu_rotulo(int d) {
  return (d >= 0 && d < MENU_N) ? ROTULOS[d] : "";
}

// Confirma o destaque e recolhe. DIREITA tambem passa por aqui: no aparelho a
// barra nao "cancela" ao sair pela direita — o item destacado e o que o usuario
// esta olhando, e desfazer a escolha no caminho de volta seria surpresa.
static int tvPastaDoFoco(int foco);
static int tvPastasTotal(const int **v);
static void escolher(void) {
  escolheu = 1;
  if (linha >= MENU_ST0) {
    // Pasta de Streaming (layout Dinamica): abre a colecao, sem trocar de aba.
    pediuColecao = tvPastaDoFoco(linha);
    aberto = 0;
    linha = destino;
    return;
  }
  if (linha == MENU_RODAPE) {
    // O rodape nao troca de destino: ele pede a tela de escolha de perfil.
    pediuTrocar = 1;
    aberto = 0;
    linha = destino;
    return;
  }
  if (linha != destino) { destino = linha; mudou = 1; }
  aberto = 0;
}

int menu_pediu_trocar(void) { int p = pediuTrocar; pediuTrocar = 0; return p; }
int menu_pediu_colecao(void) { int c = pediuColecao; pediuColecao = -1; return c; }
int menu_pediu_spotlight(void) { int p = pediuSpot; pediuSpot = 0; return p; }

void menu_evento(const SDL_Event *e) {
  if (!aberto) return;
  if (e->type == SDL_KEYUP && buscaOk &&
      (e->key.keysym.sym == SDLK_RETURN || e->key.keysym.sym == SDLK_KP_ENTER)) {
    int longo = buscaLongo;
    buscaOk = buscaLongo = 0;
    if (ponteiro_ok_longo() && linha == MENU_BUSCAR) {
      pediuSpot = 1; aberto = 0; linha = destino;
    } else if (!longo && linha == MENU_BUSCAR) escolher();
    return;
  }
  if (e->type != SDL_KEYDOWN) return;
#ifdef NV_TOUCH_UI
  tvToqueLimpar();
#endif
  SDL_Keycode k = e->key.keysym.sym;
  // OK em Buscar so arma; a repeticao do firmware (OK segurado manda KEYDOWNs
  // separados) nao rearma.
  if ((k == SDLK_RETURN || k == SDLK_KP_ENTER) && linha == MENU_BUSCAR) {
    if (!buscaOk) { buscaOk = 1; buscaLongo = 0; buscaDesde = SDL_GetTicks(); }
    return;
  }
  buscaOk = 0;

  // Mesmo conjunto de teclas de "voltar" que o detalhe aceita: no controle e o
  // Back, no teclado cada pessoa alcanca uma diferente.
  if (k == SDLK_AC_BACK || k == SDLK_ESCAPE || k == SDLK_BACKSPACE ||
      k == SDLK_DELETE) { menu_fechar(); return; }

  // BARRA NO TOPO (#461): ESQUERDA/DIREITA andam pelos itens (sao o CIMA/BAIXO
  // da rail), BAIXO devolve o foco ao conteudo, CIMA ja e a borda da tela.
  if (topo()) {
    if (k == SDLK_DOWN) { menu_fechar(); return; }
    if (k == SDLK_RETURN || k == SDLK_KP_ENTER) { escolher(); return; }
    if (k == SDLK_RIGHT || k == SDLK_LEFT) topoMover(k == SDLK_RIGHT ? 1 : -1);
    return;
  }

  // Por cima da pagina do titulo, DIREITA e o caminho de volta para ela (dono,
  // 03/10): escolher ali fecharia a pagina so por sair da barra.
  if (k == SDLK_RIGHT && sobre) { menu_fechar(); return; }
  if (k == SDLK_RIGHT || k == SDLK_RETURN || k == SDLK_KP_ENTER) { escolher(); return; }
  if (tvAtivo()) {
    // Ordem da barra da Apple TV: cabecalho (perfil), Buscar, Inicio, ...
    int lista[NV_MENU_FOCOS_TV], n = tvOrdem(lista), p = 0, i;
    for (i = 0; i < n; i++) if (lista[i] == linha) p = i;
    if (k == SDLK_DOWN && p + 1 < n) linha = lista[p + 1];
    else if (k == SDLK_DOWN && linha < MENU_ST0 && tvPastasTotal(NULL)) linha = MENU_ST0;   // opens Streaming
    else if (k == SDLK_UP && p > 0) linha = lista[p - 1];
    return;
  }
  // Sem rotacao nas pontas: a barra e curta e o usuario ve as quatro linhas de
  // uma vez, entao dar a volta no fim da lista le como falha, nao como atalho.
  if (k == SDLK_DOWN) {
    int j = linha + 1;
    while (j < MENU_N && !mostra(j)) j++;
    if (j < NV_MENU_FOCOS) linha = j;
  } else if (k == SDLK_UP) {
    int j = linha - 1;
    while (j >= 0 && !mostra(j)) j--;
    if (j >= 0) linha = j;
  }
  // ESQUERDA morre aqui de proposito: a barra ja e a borda da tela.
}

void menu_atualizar(float dt, Uint32 agora) {
  if (buscaOk && !buscaLongo && aberto && segurar_venceu(agora, buscaDesde ? buscaDesde : 1, "menu busca")) {
    buscaLongo = 1;
    pediuSpot = 1;   // o Spotlight abre por cima de tudo: nao fecha a camada
    aberto = 0;
    linha = destino;
  }
  if (!aberto) buscaOk = buscaLongo = 0;
  if (tvAtivo()) { tvAtualizar(dt); return; }
  if (topo()) topoAtualizar(dt);
  // Recolhido e assentado nao custa nada: nem mola, nem laco pelos destinos.
  // A forma (`expande`, mola subamortecida) e o veu (`desliza`, rampa reta)
  // assentam em tempos diferentes; zerar so "se desliza == 0" deixava resto de
  // `expande` e o nome do perfil apagado ao lado do avatar da rail (#210).
  // Zera os dois, sempre, e a velocidade junto.
  if (!aberto && desliza < 0.002f && fabsf(expande) < 0.004f && fabsf(expandeV) < 0.05f) {
    desliza = 0.0f; expande = 0.0f; expandeV = 0.0f;
    return;
  }
  float alvo = aberto ? 1.0f : 0.0f;
  float ms   = aberto ? NV_MENU_ABRIR_MS : NV_MENU_FECHAR_MS;
  desliza = anim_rampa(desliza, alvo, dt, ms);
  // A MOLA DA ILHA (ilha.c, molaIlhaWZ): quatro subpassos, dt no maximo 50 ms.
  // Com Animacoes reduzidas vai direto ao alvo.
  if (anim_politica_reduzida || ajustes_animacoes_reduzidas()) { expande = alvo; expandeV = 0.0f; }
  else {
    float z = padrao() ? 1.0f : NV_MENU_MOLA_Z, w = NV_MENU_MOLA_W, h;
    int k;
    if (dt > 0.05f) dt = 0.05f;
    h = dt * 0.25f;
    for (k = 0; k < 4; k++) {
      float ac = w * w * (alvo - expande) - 2.0f * z * w * expandeV;
      expandeV += ac * h;
      expande += expandeV * h;
    }
    // Fechando, a forma nao passa da rail para menos (encolher abaixo de 88
    // seria um defeito, nao um repique).
    if (!aberto && expande < 0.0f) { expande = 0.0f; if (expandeV < 0.0f) expandeV = 0.0f; }
  }
  for (int i = 0; i < NV_MENU_FOCOS; i++) {
    float a = (aberto && i == linha) ? 1.0f : 0.0f;
    // O ITEM QUE SAI APAGA EM ~50 ms, e nao nos 120 ms do NV_MOLA_DESFOCO.
    // A pilula aqui e SOLIDA na cor de realce, com luz em volta: descendo o
    // menu com o controle, os 120 ms deixavam duas ou tres pilulas acesas
    // atras do foco — o "rastro" que o dono viu (25/09, C9), com o FPS em 60.
    // Mesmo valor do painel de Salvos (SP_MOLA_DESFOCO).
    animFoco[i] = anim_mola(animFoco[i], a, dt,
                            a > animFoco[i] ? NV_MOLA_FOCO : NV_MOLA_MENU_DESFOCO);
  }
}

// Mesmos vetores do sidebar oficial, rasterizados no build e tintados pelo shader.
static void icone(int d, float cx, float cy, float s, float r, float g, float b, float a) {
  // `portal` ja e um SVG embarcado e le como entrada para uma descoberta;
  // manter o icone real evita inventar um glifo SDF e duplicar o de Busca.
  static const char *nomes[MENU_N] = {"menu_home", "portal", "menu_guide", "menu_sports", "menu_search", "menu_library", "menu_agenda", "menu_profile", "menu_settings"};
  if (d < 0 || d >= MENU_N) return;
  gfx_icone((GfxRect){cx-s*.5f, cy-s*.5f, s, s}, nomes[d], r, g, b, a);
}


// Os icones da barra CLASSICA: os de traco do mockup (tools/icones-menu.sh).
// A Dinamica continua chamando icone(), com os menu_*.png de sempre.
static void iconeTraco(int d, float cx, float cy, float s, float r, float g, float b, float a) {
  static const char *nomes[MENU_N] = {"mt_inicio", "mt_explorar", "mt_guia", "menu_sports", "mt_busca", "mt_biblioteca", "mt_agenda", "mt_perfil", "mt_ajustes"};
  if (d < 0 || d >= MENU_N) return;
  gfx_icone((GfxRect){cx-s*.5f, cy-s*.5f, s, s}, nomes[d], r, g, b, a);
}

// Cor do avatar a partir do "#RRGGBB" que a conta guarda. Sem cor legivel, o
// azul do padrao do web.
static void corAvatar(const char *hex, float *r, float *g, float *b) {
  unsigned v = 0;
  *r = 0.12f; *g = 0.53f; *b = 0.90f;
  if (!hex || hex[0] != '#' || strlen(hex) < 7) return;
  if (sscanf(hex + 1, "%6x", &v) != 1) return;
  *r = ((v >> 16) & 255) / 255.0f;
  *g = ((v >> 8) & 255) / 255.0f;
  *b = (v & 255) / 255.0f;
}

// A INICIAL do nome, respeitando UTF-8: um nome comecado por acento tem dois
// bytes, e cortar no primeiro desenha lixo.
static void inicialDe(const char *nome, char *dst, size_t tam) {
  if (tam < 3) { if (tam) dst[0] = 0; return; }
  dst[0] = (nome && nome[0]) ? nome[0] : '?';
  dst[1] = 0;
  if (nome && (unsigned char)nome[0] >= 0xC0 && nome[1]) { dst[1] = nome[1]; dst[2] = 0; }
}

// Rodape: quem esta usando, e a porta para trocar. Desenha nas DUAS larguras —
// na rail so o avatar (44, no centro do icone, mockup 2), aberto o avatar de
// 40 na linha de 72 com o nome e a acao (mockup 1). aTexto e a mesma rampa dos
// rotulos. Foco = a mesma superficie clara das linhas.
static void desenhaRodape(const MenuGeo *g, float e, float alpha, float foco, float aTexto) {
  const ContaPerfil *p = perfis_item_ativo();
  float ec = limita01(e);
  float cy = g->rodY + g->rodH * 0.5f;
  float cr, cg, cb;
  float tam = anim_mistura(NV_MENU_AVATAR_RAIL, NV_MENU_AVATAR, ec);
  // Aberto, o avatar comeca no mesmo recuo de 20 do icone (.it padding).
  float cx = g->x + anim_mistura(NV_MENU_ICONE_RAIL, NV_MENU_PAD + MK(20.0f) + NV_MENU_AVATAR * 0.5f, ec);
  float tx = g->x + NV_MENU_PAD + MK(20.0f) + NV_MENU_AVATAR + MK(18.0f);
  float txMax = NV_MENU_W_ABERTO - (NV_MENU_PAD + MK(20.0f) + NV_MENU_AVATAR + MK(18.0f)) - NV_MENU_PAD - MK(20.0f);
  char ini[4];
  GfxRect av;

  if (alpha <= 0.01f) return;
  focoLinha(linhaRect(g, g->rodY, g->rodH, e), foco, alpha);

  av.x = cx - tam * 0.5f;
  av.y = cy - tam * 0.5f;
  av.w = av.h = tam;

  // FOTO quando a conta tem uma; senao o circulo com a inicial, que e o mesmo
  // que o app web mostra quando `avatar_url` e nulo. A inicial e 19 Bold
  // (TXT_V2_KBD18) a 90 % = 17 na tela: o mockup usa 16/700 no avatar de 40
  // e 17/700 no de 44.
  // A foto e pedida pela largura com que desenha (tex_obter_larg): o teto
  // unico de 640 decodificava ate 1,6 MB de avatar para um circulo de 44.
  // Ver tests/artemenor.c.
  { GLuint tex = (p && p->avatarUrl[0]) ? tex_obter_larg(p->avatarUrl, av.w) : 0;
    if (tex) {
      // Aspecto real da foto (#388): com 1.0 uma foto retangular era esticada
      // no quadrado; GFX_AVATAR recorta em cover sem cair em contain.
      gfx_tex_aspect_atual = tex_aspecto(p->avatarUrl);
      gfx_rect(av, tex, GFX_AVATAR, 0, 0, 0, 0.5f, 0, 0, 0, alpha);
      gfx_tex_aspect_atual = 0.0f;
    } else {
      corAvatar(p ? p->corHex : NULL, &cr, &cg, &cb);
      gfx_cor(av, 0.5f, cr, cg, cb, alpha);
      inicialDe(p ? p->nome : NULL, ini, sizeof ini);
      { TxtLinha l = txt_linha(TXT_V2_KBD18, ini, 255, 255, 255, 255);
        txt_desenhar_alpha(l, av.x + (av.w - l.w) * 0.5f,
                           av.y + (av.h - l.h) * 0.5f, alpha); } } }

  if (aTexto > 0.01f) {
    // Nome no corpo dos rotulos (TXT_BODY 25/500 a 90 % = 22,5 na tela, o
    // 22/500 do mockup) em --fg cheio; a acao em cinza a 45 %. O mockup pede
    // 16 px para a acao; TXT_CAPTION2 (21) a 90 % da 19 na tela, o mais perto
    // dele que o piso de leitura do app a 3 m ainda aceita para uma frase.
    int emFoco = foco > 0.5f;
    int c = emFoco ? 255 : 243;
    TxtLinha nome = txt_linha_corta(TXT_BODY, p ? p->nome : "Sua conta", c, c - (emFoco ? 0 : 1),
                                    c - (emFoco ? 0 : 4), 255, txMax);
    TxtLinha acao = txt_linha_corta(TXT_CAPTION2, "Trocar de perfil", 243, 242, 239, 255, txMax);
    float alto = nome.h + MK(2.0f) + acao.h, y0 = cy - alto * 0.5f;
    txt_desenhar_alpha(nome, tx, y0, aTexto);
    txt_desenhar_alpha(acao, tx, y0 + nome.h + MK(2.0f), aTexto * (emFoco ? .62f : .45f));
  }
}

// O PAINEL ESTA NA TELA? E com que forma. Fechado e assentado, com a rail
// fixa, e a rail (e = 0); recolhida e fechada, nada.
static int railFixa(void) { return !topo() && !ajustes_rail_recolhida() && !(sobre && semRail); }

// Borda direita do menu classico, em px da tela REAL (o menu e camada
// ampliada, escala.h), para a ilha do relogio. Abrindo, devolve logo a largura
// aberta; fechando, a largura que ainda esta na tela. 0 sem menu na tela ou no
// layout Dinamica.
float menu_barra_borda(void) {
  MenuGeo g;
  float e;
  if (tvAtivo() || topo()) return 0.0f;
  if (aberto) e = 1.0f;
  else if (desliza >= 0.002f || expande > 0.004f) e = limita01(expande);
  else if (railFixa()) e = 0.0f;
  else return 0.0f;
  g = geoEm(e, railFixa() ? 1.0f : (aberto ? 1.0f : anim_suave(desliza)));
  return (g.x + g.w) * NV_MENU_ESCALA;
}

static void menu_desenharCorpo_(Uint32 agora);
// Camada ampliada no fator fixo do menu (NV_MENU_ESCALA): o corpo desenha na
// tela virtual dele.
void menu_desenhar(Uint32 agora) {
  float ant = gfx_escala();
  gfx_escala_sair(NV_MENU_ESCALA);   // fator FIXO, nao o do Tamanho da interface
  menu_desenharCorpo_(agora);
  gfx_escala_sair(ant);
}
static void menu_desenharCorpo_(Uint32 agora) {
#ifdef NV_TOUCH_UI
  if (layout_modo_mobile() && !aberto && !ponteiro_borda_registrada()) return;
#endif
  (void)agora;
  if (tvAtivo()) { tvDesenhar(); return; }
  if (topo()) { topoDesenhar(); return; }
  // Rail fixa sempre presente, como no shell legacy: a pilula estreita, so
  // icones. `collapseSidebar`: com a barra RECOLHIDA o web nao desenha rail
  // nenhuma; o painel so aparece quando ganha foco.
  int fixa = railFixa();
  if (!aberto && desliza < 0.002f && expande <= 0.004f) {
    if (fixa) {
      MenuGeo g = geoEm(0.0f, 1.0f);
      desenhaBarra(0.0f, 1.0f, 0, 1.0f);
      alvosDasLinhas(&g, g.x, g.w);
    } else {
      // Recolhida, a rail nao existe na tela; uma faixa na borda faz o papel
      // dela para o ponteiro, como o ESQUERDA na primeira coluna.
      MenuGeo g = geoEm(0.0f, 1.0f);
      alvosDasLinhas(&g, 0.0f, MK(28.0f));
    }
    return;
  }

  // O VEU usa a rampa CRUA: a medida da referencia e uma reta (ver
  // NV_MENU_ABRIR_MS). A FORMA anda na mola da ilha (`expande`).
  float entrada = anim_suave(desliza);
  float e = expande;
  // Luz ESCURA nascendo 600 px fora da tela, na altura do meio: e quase uma
  // rampa horizontal (o centro esta longe), zerada por volta de x = 1100. Um
  // quad do tamanho da area que ela alcanca, nao da tela inteira.
  gfx_luz_canto((GfxRect){ 0, 0, NV_MENU_VEU_ALC - 600.0f, NV_TELA_H }, 0.0f,
                -600.0f, NV_TELA_H * 0.5f, NV_MENU_VEU_ALC, 0, 0, 0, NV_MENU_VEU * desliza);
  // Com a rail fixa o painel ja esta na tela e so cresce; recolhida, na
  // Moderna ele nasce (e some) pela opacidade no mesmo lugar, na Padrao
  // desliza de fora da tela pela esquerda.
  { float a = fixa ? 1.0f : (padrao() ? 1.0f : entrada);
    float ent = (fixa || !padrao()) ? 1.0f : entrada;
    MenuGeo g = geoEm(e, ent);
    if (aberto) {
      ponteiro_alvo(0, 0, NV_TELA_W, NV_TELA_H, NULL, ponteiroFora, 0, 0);
      ponteiro_alvo(g.x, g.y, g.w, g.h, NULL, NULL, 0, 0);
      alvosDasLinhas(&g, g.x, g.w);
    }
    desenhaBarra(e, a, 1, ent); }
}

// ===========================================================================
// BARRA DA APPLE TV (so no layout Dinamica da home; os outros layouts usam a
// rail de cima, sem mudanca nenhuma).
//
// Referencia: fotos do app Apple TV (tvOS 26) na TV do dono, 01/10/2026.
//   FECHADA: nao ha barra lateral. No topo esquerdo, por cima do conteudo, so
//   uma pilula de vidro "‹ (icone) Inicio" com a secao atual.
//   ABERTA: a pilula CRESCE ate virar um painel flutuante arredondado, com
//   margem da borda e altura so ate o ultimo item. Cabecalho com o avatar, o
//   nome e o relogio; itens com o icone num circulo; o item ATUAL tem pilula
//   cinza translucida e o item EM FOCO pilula clara com texto escuro (aqui: a
//   cor de realce, com a tinta por contraste dela — branca no padrao).
//   O conteudo atras escurece so do lado esquerdo, sem desfoque.
//
// Custo: fechada, 3 quads SDF e 2 glifos; aberta, 1 quad de sombra radial
// (meia tela, nenhum veu de tela cheia), 1 painel e ~2 quads por item.
// ===========================================================================
#include <time.h>
#include <ctype.h>

// MEDIDAS DO ORIGINAL (print do app oficial no layout Apple TV, medido pelo
// coordenador em 2000 px e convertido x0,96 para 1920; dono, 01/10: "muito
// pesada", "a letra ta grande demais"; reaberto 14/10 contra a foto
// fotos1014/appletv-sidebar.jpg — margem, largura, chip e foco voltam ao medido):
//   painel x 38, topo 38, ~355 de largura, raio ~40, vidro escuro translucido
//   com aro fino; avatar 44, nome 26 Medium, relogio ~24 Regular; icone de
//   linha num CIRCULO translucido (46), rotulo 25 Regular, passo 79, foco =
//   pilula BRANCA cheia de 74, um pouco MAIOR (cresce 3 px por lado); item
//   atual sem foco = pilula clara translucida; "Streaming" 22 cinza medio;
//   logo da pasta em circulo de 48.
#define TV_PAINEL_X     38.0f
#define TV_PAINEL_Y     38.0f
#define TV_PAINEL_W    355.0f
#define TV_RAIO         40.0f
#define TV_CAB_H       120.0f    // cabecalho: avatar, nome, relogio
#define TV_LINHA_H      79.0f
#define TV_PILULA_H     74.0f
#define TV_PAD_X        18.0f    // pilula da linha por dentro do painel
#define TV_PAD_BASE     14.0f
#define TV_ICONE        28.0f
#define TV_COL_CX       32.0f    // centro da coluna de icones, a partir da pilula
#define TV_ROT_X        66.0f    // x do rotulo, a partir da pilula
#define TV_LOGO         48.0f    // circulo da pasta de Streaming
#define TV_AVATAR       44.0f
#define TV_ROTULO_H     78.0f    // rotulo da secao "Streaming" (vaio maior
                                // acima: separacao clara do item de cima)
#define TV_SECAO_TRK     1.5f    // tracking do cabecalho (caixa alta)
// O MIOLO DO PAINEL tem um piso: o "Opacidade do vidro" (78 %) deixava os
// titulos das fileiras da Home lerem ATRAVES do vidro na TCL (captura
// fotos1014/tcl-sidebar-now.png: "Continuar assistindo" por baixo dos
// rotulos). Sem frost o piso e o que garante legibilidade.
#define TV_MIOLO_MIN    0.88f
// O CHIP: na foto da referencia (appletv-sidebar.jpg) cada icone de linha mora
// num circulo translucido, quase apagado quando a pilula branca do foco assume
// e a tinta do icone/rotulo escurece.
#define TV_CHIP         46.0f
#define TV_CHIP_REPOUSO  0.16f    // circulo claro translucido em repouso
#define TV_CHIP_FOCO     0.07f    // quase apagado na pilula do foco
#define TV_FOCO_CRESC    3.0f     // a pilula em foco cresce 3 px por lado
// Pilula fechada: mais baixa e com a letra do item (25), nao a de titulo.
#define TV_PIL_CIRC     46.0f
#define TV_ALTURA_MAX  (NV_TELA_H - 2.0f * TV_PAINEL_Y)
#define TV_MOLA_ROLAR   14.0f
// Molas (anim_mola2, rad/s): abrir um pouco mais lento que fechar, como a
// barra do aparelho; com Animacoes reduzidas vai direto ao alvo.
#define TV_MOLA_ABRE    13.0f
#define TV_MOLA_FECHA   17.0f
#define TV_MOLA_PILULA  10.0f
// STREAMING HINT AND OPENING (dono, 19/10): collapsed, a quiet hint row ends
// the list under Settings; opening, the panel grows on an exponential ease-out
// and the label and services rise in one by one. Only alpha, a y offset and
// the drawn panel height move: tvLayout is final from the first frame.
#define TV_DICA_H       76.0f    // hint row: up to two lines of TXT_DET_META2
#define TV_ST_DUR       0.42f    // each row, s
#define TV_ST_PASSO     0.05f    // stagger between rows, s
#define TV_ST_DY        22.0f    // rows rise from 22 px below
#define TV_ST_ALTURA    10.0f    // panel height: e^(-10 t) approach
static float tvStT = 1e3f, tvAltura;   // s since Streaming opened/closed; drawn panel height
static int tvStAberta;
static float tvDicaY = -1.0f;          // hint row top (tvLayout), -1 = none
static float tvExpo(float t) { t = limita01(t); return t >= 1.0f ? 1.0f : 1.0f - powf(2.0f, -10.0f * t); }

static float tvAbre = 0.0f, tvAbreV = 0.0f;
// Nada da barra na tela e nenhuma mola andando (as duas versoes da barra).
static int assentado(void) {
  if (aberto) return 0;
  if (tvAtivo()) return tvAbre < 0.002f && tvAbreV == 0.0f;
  return desliza < 0.002f && expande <= 0.004f && fabsf(expandeV) < 0.05f;
}
static float tvRolar = 0.0f, tvRolarV = 0.0f;
#ifdef NV_TOUCH_UI
static float tvLayout(float *ys, float *yRotulo);
static int tvToqueLivre;
static void tvToqueLimpar(void) { tvToqueLivre = 0; }
static GfxRect tvToqueRegiao;
static float tvToqueEscala = 1.0f, tvToqueMaximo;
static int tvToqueRolar(const PonteiroRolagem *e) {
  if (!layout_modo_mobile()) { tvToqueLimpar(); return 0; }
  if (e->fase == PONT_ROL_INICIO) {
    if (!e->eixoY || !aberto || e->x < tvToqueRegiao.x || e->x >= tvToqueRegiao.x + tvToqueRegiao.w || e->y < tvToqueRegiao.y || e->y >= tvToqueRegiao.y + tvToqueRegiao.h) return 0;
    tvToqueLivre = 1; tvRolarV = 0.0f; buscaOk = buscaLongo = 0; return 1;
  }
  if (e->fase == PONT_ROL_MOVER || e->fase == PONT_ROL_INERCIA) {
    float antes = tvRolar, proxima = tvRolar - e->delta / tvToqueEscala;
    int expandir = proxima > tvToqueMaximo && linha < MENU_ST0 && tvPastasTotal(NULL);
    if (expandir) {
      float ys[NV_MENU_FOCOS_TV];
      linha = MENU_ST0;  // Swiping past Settings opens Streaming, like Down.
      tvToqueMaximo = fmaxf(0.0f, tvLayout(ys, NULL) - TV_ALTURA_MAX);
    }
    tvRolar = anim_clamp(proxima, 0.0f, tvToqueMaximo);
    return expandir || fabsf(tvRolar - antes) > 0.001f;
  }
  return 1;
}
#endif
static float tvPilAlfa = 1.0f, tvPilAlvo = 1.0f;

static int tvAtivo(void) { return ajustes_home_layout() == HOME_LAYOUT_DINAMICA; }

// Ordem do original: Inicio, Busca, Explorar... (o resto segue a ordem do app).
static const int TV_ORDEM[MENU_N] = {
  MENU_INICIO, MENU_BUSCAR, MENU_EXPLORAR, MENU_GUIA, MENU_ESPORTES, MENU_AGENDA,
  MENU_BIBLIOTECA, MENU_PERFIL, MENU_AJUSTES
};
static const char *tvRotulo(int d) { return menu_rotulo(d); }
// Focos na ordem de navegacao: cabecalho (trocar de usuario) e os visiveis.
// PASTAS DE STREAMING (home_streaming_barra): a fileira "Streaming" que o
// layout Dinamica tira da home. Indices de col_folder, na ordem da fileira.
static int tvPastasTotal(const int **v) {
  const int *t;
  int n = home_streaming_barra(&t);
  if (v) *v = t;
  return n > NV_MENU_ST_MAX ? NV_MENU_ST_MAX : (n < 0 ? 0 : n);
}
// Streaming stays collapsed until focus or a mobile swipe passes Settings.
static int tvPastas(const int **v) {
  int n = tvPastasTotal(v);
  return linha >= MENU_ST0 ? n : 0;
}
static int tvPastaDoFoco(int foco) {
  const int *v;
  int n = tvPastasTotal(&v), k = foco - MENU_ST0;
  return (k >= 0 && k < n) ? v[k] : -1;
}
static int tvOrdem(int *lista) {
  const int *v;
  int n = 0, i, np = tvPastas(&v);
  lista[n++] = MENU_RODAPE;
  for (i = 0; i < MENU_N; i++) if (mostra(TV_ORDEM[i])) lista[n++] = TV_ORDEM[i];
  for (i = 0; i < np; i++) lista[n++] = MENU_ST0 + i;
  return n;
}
// O conteudo do painel em coordenadas PROPRIAS (0 = topo do painel, antes da
// rolagem): `ys[foco]` e o topo de cada linha (-1 = nao esta na barra).
// Devolve a altura total; `yRotulo` recebe o topo do rotulo "Streaming".
static float tvLayout(float *ys, float *yRotulo) {
  const int *v;
  int i, np = tvPastas(&v);
  float y = TV_CAB_H;
  for (i = 0; i < NV_MENU_FOCOS_TV; i++) ys[i] = -1.0f;
  ys[MENU_RODAPE] = 0.0f;
  for (i = 0; i < MENU_N; i++)
    if (mostra(TV_ORDEM[i])) { ys[TV_ORDEM[i]] = y; y += TV_LINHA_H; }
  if (yRotulo) *yRotulo = -1.0f;
  if (np) {
    if (yRotulo) *yRotulo = y;
    y += TV_ROTULO_H;
    for (i = 0; i < np; i++) { ys[MENU_ST0 + i] = y; y += TV_LINHA_H; }
  }
  tvDicaY = -1.0f;
  if (!np && tvPastasTotal(NULL)) { tvDicaY = y; y += TV_DICA_H; }
  return y + TV_PAD_BASE;
}
// Altura na tela: ate o ultimo item, no maximo a tela menos as margens (dai
// para baixo a barra ROLA, com o foco sempre visivel).
static GfxRect tvPainel(void) {
  float ys[NV_MENU_FOCOS_TV], h = tvLayout(ys, NULL);
  GfxRect r = { TV_PAINEL_X, TV_PAINEL_Y, TV_PAINEL_W, h < TV_ALTURA_MAX ? h : TV_ALTURA_MAX };
  return r;
}

// Closed pill anchors stay fixed while its label follows text scale (#205).
#define TV_PIL_X (NV_MENU_PILULA_X / gfx_escala_txt())
#define TV_PIL_SETA (NV_MENU_PILULA_SETA / gfx_escala_txt())
// A PILULA FECHADA: "‹" + circulo + rotulo da secao atual.
static float tvPilulaLargura(void) {
  return 7.0f + TV_PIL_CIRC + 14.0f + (float)txt_largura(TXT_BODY, tvRotulo(destino)) + 24.0f;
}
static GfxRect tvPilula(void) {
  GfxRect r = { TV_PIL_X + TV_PIL_SETA, NV_MENU_PILULA_Y / gfx_escala_txt(),
                tvPilulaLargura(), NV_MENU_PILULA_H / gfx_escala_txt() };
  return r;
}

int menu_pilula_rect(float *x, float *y, float *w, float *h) {
  GfxRect r;
  if (!tvAtivo()) { if (x) *x = 0; if (y) *y = 0; if (w) *w = 0; if (h) *h = 0; return 0; }
  r = tvPilula();
  // O retangulo devolvido INCLUI a seta, que fica a esquerda da pilula. Em
  // pixels da tela REAL: a pilula e camada ampliada (escala.h), e quem pergunta
  // (Busca, Biblioteca, a ilha) converte para a tela dele.
  { float e = NV_MENU_ESCALA;
    if (x) *x = TV_PIL_X * e;
    if (y) *y = r.y * e;
    if (w) *w = (r.x + r.w - TV_PIL_X) * e;
    if (h) *h = r.h * e; }
  return 1;
}
int menu_pilula_titulo(void) { return tvAtivo(); }
float menu_pilula_alfa(void) {
  if (!tvAtivo()) return 0.0f;
  { float a = tvPilAlfa * (1.0f - tvAbre); return a < 0.0f ? 0.0f : a; }
}
// O cabecalho da barra da Apple TV tem relogio proprio: aberta, a ilha do
// canto duplicava a hora (11:29 + 11:29 na captura da TCL).
int menu_relogio_cabe(void) { return !(aberto && tvAtivo()); }
void menu_pilula_mostrar(float alvo) {
  // Negativo = some JA, sem mola: ao trocar para uma tela com titulo no canto
  // a pilula nao pode ficar 300 ms por cima dele.
  if (alvo < 0.0f) { tvPilAlvo = tvPilAlfa = 0.0f; return; }
  tvPilAlvo = alvo > 1.0f ? 1.0f : alvo;
}

static void tvAtualizar(float dt) {
#ifdef NV_TOUCH_UI
  if (!layout_modo_mobile()) tvToqueLimpar();
#endif
  int i;
  float alvo = aberto ? 1.0f : 0.0f;
  desliza = 0.0f; expande = 0.0f; expandeV = 0.0f;
  tvPilAlfa = anim_mola(tvPilAlfa, tvPilAlvo, dt, TV_MOLA_PILULA);
  if (!aberto && tvAbre < 0.002f && tvAbreV == 0.0f) {
    tvAbre = 0.0f; tvRolar = 0.0f; tvRolarV = 0.0f;
    tvStT = 1e3f; tvAltura = 0.0f; tvStAberta = tvPastas(NULL) > 0; return;
  }
  { int st = tvPastas(NULL) > 0;
    int pousa = anim_politica_reduzida || ajustes_animacoes_reduzidas() || gfx_efeitos_minimos();
    float h = tvPainel().h;
    if (st != tvStAberta) { tvStAberta = st; tvStT = 0.0f; }
    tvStT = pousa ? 1e3f : fminf(tvStT + dt, 1e3f);
    tvAltura = pousa || tvAltura <= 0.0f ? h : h + (tvAltura - h) * expf(-TV_ST_ALTURA * dt);
    if (fabsf(tvAltura - h) < 0.5f) tvAltura = h; }
  tvAbre = anim_mola2(&tvAbreV, tvAbre, alvo, dt, aberto ? TV_MOLA_ABRE : TV_MOLA_FECHA);
  if (!aberto && tvAbre < 0.002f) { tvAbre = 0.0f; tvAbreV = 0.0f; }
  // ROLAGEM: a linha em foco fica inteira dentro do painel, com folga de meia
  // linha (a seguinte aparece cortada, que e o aviso de que ha mais).
  { float ys[NV_MENU_FOCOS_TV], total = tvLayout(ys, NULL), alvoR = tvRolar;
    float vis = total < TV_ALTURA_MAX ? total : TV_ALTURA_MAX;
    float y0 = (linha >= 0 && linha < NV_MENU_FOCOS_TV) ? ys[linha] : 0.0f;
    float h0 = linha == MENU_RODAPE ? TV_CAB_H : TV_LINHA_H, folga = TV_LINHA_H * 0.5f;
    if (y0 >= 0.0f) {
      if (y0 - folga < alvoR) alvoR = y0 - folga;
      if (y0 + h0 + folga > alvoR + vis) alvoR = y0 + h0 + folga - vis;
    }
    if (alvoR > total - vis) alvoR = total - vis;
    if (alvoR < 0.0f) alvoR = 0.0f;
    if (!aberto && tvAbre <= 0.002f) { tvRolar = 0.0f; tvRolarV = 0.0f; }
    else {
#ifdef NV_TOUCH_UI
      if (tvToqueLivre) tvRolar = anim_clamp(tvRolar, 0.0f, total - vis);
      else
#endif
      tvRolar = anim_mola2(&tvRolarV, tvRolar, alvoR, dt, TV_MOLA_ROLAR);
    } }
  for (i = 0; i < NV_MENU_FOCOS_TV; i++) {
    float a = (aberto && i == linha) ? 1.0f : 0.0f;
    animFoco[i] = anim_mola(animFoco[i], a, dt,
                            a > animFoco[i] ? NV_MOLA_FOCO : NV_MOLA_MENU_DESFOCO);
  }
}

// Iniciais como na referencia ("HR"): a primeira letra das duas primeiras
// palavras do nome, respeitando UTF-8. Nome de uma palavra so: uma letra.
static void tvIniciais(const char *nome, char *dst, size_t tam) {
  size_t n = 0;
  int palavras = 0;
  const char *p = nome && nome[0] ? nome : "?";
  while (*p && palavras < 2 && n + 5 < tam) {
    while (*p == ' ') p++;
    if (!*p) break;
    { size_t len = 1;
      unsigned char c = (unsigned char)*p;
      if (c >= 0xF0) len = 4; else if (c >= 0xE0) len = 3; else if (c >= 0xC0) len = 2;
      if (len == 1) dst[n++] = (char)toupper(c);
      else { size_t k; for (k = 0; k < len && p[k]; k++) dst[n++] = p[k]; } }
    palavras++;
    while (*p && *p != ' ') p++;
  }
  dst[n] = 0;
}

static void tvRelogio(char *buf, size_t tam) {
  time_t t = time(NULL);
  struct tm tmv;
#ifdef _WIN32
  localtime_s(&tmv, &t);
#else
  localtime_r(&t, &tmv);
#endif
  hora_tela(buf, tam, &tmv);
}

// A PILULA DO FOCO E UM POUCO MAIOR (referencia: o item em foco cresce sobre
// os vizinhos). O repique da mola passa de 1; a geometria aceita um pouco.
static GfxRect tvPillFoco(GfxRect p, float f) {
  if (f < 0.0f) f = 0.0f;
  if (f > 1.15f) f = 1.15f;
  return (GfxRect){ p.x - TV_FOCO_CRESC * f, p.y - TV_FOCO_CRESC * f,
                    p.w + 2.0f * TV_FOCO_CRESC * f, p.h + 2.0f * TV_FOCO_CRESC * f };
}

// O CHIP do icone da linha: circulo translucido em repouso, quase apagado (e
// escuro) quando a pilula branca do foco assume.
static void tvChip(float cx, float cy, float foco, float alfa) {
  float c = 1.0f - limita01(foco);   // branco no repouso, escuro no foco
  float a = anim_mistura(TV_CHIP_REPOUSO, TV_CHIP_FOCO, limita01(foco));
  gfx_cor((GfxRect){ cx - TV_CHIP * 0.5f, cy - TV_CHIP * 0.5f, TV_CHIP, TV_CHIP },
          0.5f, c, c, c, a * alfa);
}

// Icone de linha, sem bolha atras: branco a 85% em repouso, a tinta escura
// sobre a pilula branca do foco.
static void tvIcone(int d, float cx, float cy, float foco, float alfa) {
  float lum = foco > 0.5f ? 0.08f : 1.0f;
  icone(d, cx, cy, TV_ICONE, lum, lum, lum, alfa * (foco > 0.5f ? 1.0f : 0.85f));
}

// Circulo da PASTA de Streaming: a capa dela recortada em circulo (cover).
// Sem capa ainda (rede), a cor da pasta com a inicial do nome.
static void tvCirculoPasta(const ColFolder *pf, float cx, float cy, float alfa) {
  GfxRect c = { cx - TV_LOGO * 0.5f, cy - TV_LOGO * 0.5f, TV_LOGO, TV_LOGO };
  const char *capa = pf ? col_capa(pf) : NULL;
  // Pedida pela largura do circulo (TV_LOGO=48, cap 128 pelo piso), nao pelo
  // 640 unico — ver tests/artemenor.c.
  GLuint tex = (capa && capa[0]) ? tex_obter_larg(capa, TV_LOGO) : 0;
  if (tex) {
    float asp = tex_aspecto(capa);
    gfx_tex_aspect_atual = asp > 0.0f ? asp : 1.0f;
    gfx_card_forcar_cover_atual = 1.0f;
    gfx_rect(c, tex, GFX_CARD, 0, 0, 0, 0.5f, 0, 0, 0, alfa);
    gfx_card_forcar_cover_atual = 0.0f;
  } else {
    float r = 0.3f, g = 0.3f, b = 0.34f;
    char ini[8];
    if (pf) col_cor(pf, &r, &g, &b);
    gfx_cor(c, 0.5f, r, g, b, alfa);
    inicialDe(pf ? pf->title : NULL, ini, sizeof ini);
    { TxtLinha l = txt_linha(TXT_CAPTION, ini, 255, 255, 255, 255);
      txt_desenhar_alpha(l, c.x + (c.w - l.w) * 0.5f, c.y + (c.h - l.h) * 0.5f, alfa); }
  }
}

static void tvAvatar(GfxRect av, float alfa) {
  const ContaPerfil *p = perfis_item_ativo();
  // Como o avatar do menu: pela largura com que desenha (tex_obter_larg).
  GLuint tex = (p && p->avatarUrl[0]) ? tex_obter_larg(p->avatarUrl, av.w) : 0;
  if (tex) {
    gfx_tex_aspect_atual = tex_aspecto(p->avatarUrl);
    gfx_rect(av, tex, GFX_AVATAR, 0, 0, 0, 0.5f, 0, 0, 0, alfa);
    gfx_tex_aspect_atual = 0.0f;
  } else {
    float cr, cg, cb;
    char ini[16];
    corAvatar(p ? p->corHex : NULL, &cr, &cg, &cb);
    gfx_cor(av, 0.5f, cr, cg, cb, alfa);
    tvIniciais(p ? p->nome : NULL, ini, sizeof ini);
    { TxtLinha l = txt_linha(TXT_CAPTION2, ini, 255, 255, 255, 255);
      txt_desenhar_alpha(l, av.x + (av.w - l.w) * 0.5f, av.y + (av.h - l.h) * 0.5f, alfa); }
  }
}

// Escurece SO o lado esquerdo: faixas verticais de cor chapada com o alfa
// caindo em degraus pequenos ate sumir em x ~ 900. Cada pixel e pintado UMA
// vez e pelo shader mais barato (GFX_COR). MEDIDO na C9 (01/10): com uma
// sombra radial (GFX_LUZ, 1000x1080) + aro no painel a barra aberta ficava em
// 47-49 fps contra 60 fechada.
static void tvSombra(float a) {
  int i;
  float x = 0.0f;
  if (a <= 0.01f) return;
  // Veu SUAVE atras do painel (o original nao escurece a tela): 0,22 ate o
  // fim do painel e caindo a zero em ~300 px.
  // Degraus de 20 px e ~0,014 de alfa: com 50 px os degraus apareciam como
  // faixas verticais sobre ceu claro (captura no Mac).
  float edge = TV_PAINEL_X + TV_PAINEL_W + 12.0f;
  gfx_cor((GfxRect){ 0, 0, edge, NV_TELA_H }, 0.0f, 0, 0, 0, 0.22f * a);
  x = edge;
  for (i = 1; i <= 15; i++, x += 20.0f)
    gfx_cor((GfxRect){ x, 0, 20, NV_TELA_H }, 0.0f, 0, 0, 0, 0.22f * a * (1.0f - i / 16.0f));
}

static void tvPonteiroPilula(int a, int b) {
  (void)a; (void)b;
#ifdef NV_TOUCH_UI
  if (layout_modo_mobile()) ponteiro_borda_ativar();
  else menu_abrir();
#else
  menu_abrir();
#endif
}

// The hint row: chevron in the icon column, up to two lines of secondary text.
static void tvDica(GfxRect Q, float y, float a) {
  float x = Q.x + TV_PAD_X + TV_ROT_X, larg = Q.w - TV_PAD_X * 2.0f - TV_ROT_X - 16.0f;
  float cy = y + TV_DICA_H * 0.5f, ic = TV_ICONE * 0.8f;
  float h = txt_bloco_corta(TXT_DET_META2, "Role para baixo para os streamings", 200, 202, 208, 0, 0, larg, 28.0f, 0.0f, 2);
  if (a <= 0.01f) return;
  gfx_icone((GfxRect){ Q.x + TV_PAD_X + TV_COL_CX - ic * 0.5f, cy - ic * 0.5f, ic, ic }, "aj_chevrons-down",
            .78f, .79f, .82f, a * 0.85f);
  txt_bloco_corta(TXT_DET_META2, "Role para baixo para os streamings", 200, 202, 208, x, cy - h * 0.5f, larg, 28.0f, a, 2);
}

static void tvDesenhar(void) {
  float s = tvAbre < 0.0f ? 0.0f : (tvAbre > 1.0f ? 1.0f : tvAbre);
  GfxRect P = tvPilula(), Q = tvPainel(), R;
  float qh = tvAltura > 0.0f ? tvAltura : Q.h;
  float pa = tvPilAlfa;
  float A, raioPx;
  float ar, ag, ab;
  ajustes_acento(&ar, &ag, &ab);

  // Fechada e com a pilula escondida (pagina rolada): nada na tela. O clique
  // na area da pilula continua abrindo, como o ESQUERDA na primeira coluna.
  if (s <= 0.001f) {
    // Pilula visivel: clicar nela abre. Escondida, o alvo dela cobriria o que
    // a tela tem no canto (o campo da Busca); fica so a faixa da borda, que
    // abre ao passar, como a rail recolhida.
    if (ponteiro_ativo()) {
      if (pa > 0.5f)
        ponteiro_alvo(TV_PIL_X, P.y, P.x + P.w - TV_PIL_X, P.h,
                      NULL, tvPonteiroPilula, 0, 0);
      else
#ifdef NV_TOUCH_UI
        ponteiro_alvo(0, 0, 28.0f, NV_TELA_H,
                      layout_modo_mobile() ? NULL : tvPonteiroPilula,
                      layout_modo_mobile() ? tvPonteiroPilula : NULL, 0, 0);
#else
        ponteiro_alvo(0, 0, 28.0f, NV_TELA_H, tvPonteiroPilula, NULL, 0, 0);
#endif
    }
    if (pa <= 0.01f) return;
  }

  tvSombra(s);

  // O painel NASCE da pilula: o retangulo e o raio vao de um ao outro na mola.
  R.x = anim_mistura(P.x, Q.x, s);
  R.y = anim_mistura(P.y, Q.y, s);
  R.w = anim_mistura(P.w, Q.w, s);
  R.h = anim_mistura(P.h, qh, s);
  raioPx = anim_mistura(P.h * 0.5f, TV_RAIO, s);
  A = pa + (1.0f - pa) * (s * 3.0f > 1.0f ? 1.0f : s * 3.0f);

  if (aberto) {
    ponteiro_alvo(0, 0, NV_TELA_W, NV_TELA_H, NULL, ponteiroFora, 0, 0);
    ponteiro_alvo(Q.x, Q.y, Q.w, Q.h, NULL, NULL, 0, 0);
#ifdef NV_TOUCH_UI
    { float ys[NV_MENU_FOCOS_TV];
      tvToqueEscala = gfx_escala();
      tvToqueRegiao = (GfxRect){Q.x * tvToqueEscala, Q.y * tvToqueEscala, Q.w * tvToqueEscala, Q.h * tvToqueEscala};
      tvToqueMaximo = fmaxf(0.0f, tvLayout(ys, NULL) - Q.h);
      ponteiro_rolagem(tvToqueRolar);
    }
#endif
  }

  // Keep the polished Apple TV shape and focus. THE MATERIAL IS ALWAYS
  // GLASS (fotos1014/appletv-sidebar.jpg): this layout mimics the tvOS
  // sidebar, and the reference panel is translucent tinted glass regardless
  // of the global "Interface de vidro" switch; gfx_vidro_folha still honors
  // the Opacity/Frost/effect settings internally.
  // The shared material at TV_MIOLO_MIN (88 % at the default "Opacity"): at
  // 78 % the Home row titles read through the panel (TCL print). It follows
  // the slider like every other glass surface.
  gfx_vidro_camada = 1;   // frost over page glass too (detail pills, tabs, cards)
  gfx_vidro_material(R, raioPx / R.h, TV_MIOLO_MIN, A);
  gfx_vidro_camada = 0;
  gfx_vidro_matiz(R, raioPx / R.h, A);

  // Conteudo da pilula fechada: some no comeco da abertura.
  { float ap = A * (1.0f - s * 3.0f);
    if (ap > 0.01f) {
      // A seta "‹" fica solta sobre a arte: uma sombra escura de 1 px a
      // mantem legivel quando o fundo e claro (ceu, neve).
      TxtLinha seta = txt_linha(TXT_HEADLINE, "\xE2\x80\xB9", 245, 245, 248, 255);
      TxtLinha sombra = txt_linha(TXT_HEADLINE, "\xE2\x80\xB9", 0, 0, 0, 255);
      float cy = P.y + P.h * 0.5f;
      float sx = TV_PIL_X + (TV_PIL_SETA - seta.w) * 0.5f - 3.0f;
      float sy = cy - seta.h * 0.5f - 2.0f;
      txt_desenhar_alpha(sombra, sx + 1.0f, sy + 2.0f, ap * 0.35f);
      txt_desenhar_alpha(seta, sx, sy, ap * 0.90f);
      { GfxRect c = { P.x + 7.0f, cy - TV_PIL_CIRC * 0.5f, TV_PIL_CIRC, TV_PIL_CIRC };
        gfx_cor(c, 0.5f, 1.0f, 1.0f, 1.0f, 0.22f * ap);
        icone(destino, c.x + TV_PIL_CIRC * 0.5f, cy, 24.0f, 0.97f, 0.97f, 0.98f, ap); }
      { TxtLinha l = txt_linha(TXT_BODY, tvRotulo(destino), 245, 245, 248, 255);
        txt_desenhar_alpha(l, P.x + 7.0f + TV_PIL_CIRC + 14.0f, cy - l.h * 0.5f, ap); }
    } }

  // Conteudo do painel aberto, preso ao retangulo que cresce e deslocado pela
  // rolagem (tvRolar). Linha fora do painel nao desenha nem vira alvo.
  { float ac = (s - 0.22f) / 0.70f;
    float ys[NV_MENU_FOCOS_TV], yRot, f, topo;
    const int *pastas;
    int i, np = tvPastas(&pastas);
    // Foco: pilula BRANCA cheia e um pouco maior, texto e icone escuros
    // (como o original; a foto mostra branco-azulado, nao cinza).
    const float FR = 0.97f, FG = 0.97f, FB = 1.00f;
    const int TINTA_FOCO = 22;
    if (ac <= 0.01f) return;
    if (ac > 1.0f) ac = 1.0f;
    tvLayout(ys, &yRot);
    topo = Q.y - tvRolar;
    gfx_recorte(R.x, R.y, R.w, R.h);

    // Cabecalho: avatar 44, nome 26 Medium, relogio 23 Regular cinza claro.
    // Focavel: e o "trocar de usuario".
    { float cyCab = topo + 50.0f;
      int emFoco, c, cRel;
      const ContaPerfil *p = perfis_item_ativo();
      char hora[12];
      GfxRect av = { Q.x + TV_PAD_X + TV_COL_CX - TV_AVATAR * 0.5f, cyCab - TV_AVATAR * 0.5f,
                     TV_AVATAR, TV_AVATAR };
      TxtLinha rel, nome;
      f = animFoco[MENU_RODAPE];
      if (f > 0.01f) {
        GfxRect pill = { Q.x + TV_PAD_X, cyCab - TV_PILULA_H * 0.5f, Q.w - TV_PAD_X * 2.0f, TV_PILULA_H };
        gfx_cor(tvPillFoco(pill, f), 0.5f, FR, FG, FB, f * ac);
      }
      emFoco = f > 0.5f;
      c = emFoco ? TINTA_FOCO : 240;
      cRel = emFoco ? 70 : 200;
      tvAvatar(av, ac);
      tvRelogio(hora, sizeof hora);
      rel = txt_linha(TXT_DET_META2, hora, cRel, cRel, cRel, 255);
      nome = txt_linha_corta(TXT_PG_RELOGIO, p ? p->nome : "Sua conta", c, c, c, 255,
                             Q.x + Q.w - 24.0f - rel.w - 16.0f - (Q.x + TV_PAD_X + TV_ROT_X));
      txt_desenhar_alpha(nome, Q.x + TV_PAD_X + TV_ROT_X, cyCab - nome.h * 0.5f, ac);
      txt_desenhar_alpha(rel, Q.x + Q.w - 24.0f - rel.w, cyCab - rel.h * 0.5f, ac);
      if (aberto && ponteiro_ativo() && cyCab - TV_PILULA_H * 0.5f >= Q.y - 1.0f)
        ponteiro_alvo(Q.x, cyCab - TV_PILULA_H * 0.5f, Q.w, TV_PILULA_H, ponteiroLinha, NULL, MENU_RODAPE, 0); }

    // Rotulo da secao de Streaming, mais marcante (dono, 14/10): um corpo
    // acima do rotulo, Bold, em CAIXA ALTA com tracking — o padrao das secoes
    // do tvOS — e mais espaco acima (TV_ROTULO_H) para separar de Ajustes.
    if (np && yRot >= 0.0f) {
      float e = tvStAberta ? tvExpo((tvStT - TV_ST_PASSO) / TV_ST_DUR) : 1.0f;
      TxtLinha l = txt_linha(TXT_TV_SECAO, "STREAMING", 235, 237, 243, 255);
      // The hint hands over to the label: it fades where the label rises.
      if (tvStT < TV_ST_DUR) tvDica(Q, topo + yRot, ac * (1.0f - tvExpo(tvStT / 0.10f)));
      txt_tracking(TXT_TV_SECAO, "STREAMING", 235, 237, 243,
                   Q.x + TV_PAD_X + TV_COL_CX - TV_ICONE * 0.5f,
                   topo + yRot + TV_ROTULO_H - l.h - 6.0f + TV_ST_DY * (1.0f - e), ac * e, TV_SECAO_TRK);
    }
    if (tvDicaY >= 0.0f) {
      float e = tvExpo(tvStT / TV_ST_DUR);
      tvDica(Q, topo + tvDicaY + TV_ST_DY * 0.5f * (1.0f - e), ac * e);
    }

    for (i = 0; i < MENU_N + np; i++) {
      int d = i < MENU_N ? TV_ORDEM[i] : MENU_ST0 + (i - MENU_N);
      int atual = (d == destino);
      float y, cy, ac0 = ac;
      GfxRect pill;
      if (ys[d] < 0.0f) continue;
      y = topo + ys[d];
      if (y + TV_LINHA_H < Q.y || y > Q.y + Q.h) continue;
      cy = y + TV_LINHA_H * 0.5f;
      if (d >= MENU_ST0 && tvStAberta) {   // staggered rise, label first
        float e = tvExpo((tvStT - (d - MENU_ST0 + 2) * TV_ST_PASSO) / TV_ST_DUR);
        cy += TV_ST_DY * (1.0f - e); ac = ac0 * e;
      }
      pill = (GfxRect){ Q.x + TV_PAD_X, cy - TV_PILULA_H * 0.5f, Q.w - TV_PAD_X * 2.0f, TV_PILULA_H };
      f = animFoco[d];
      // ATUAL sem foco: pilula clara translucida, sutil (a "barra" cinza da
      // foto), nada de pilula pesada.
      if (atual && f < 0.99f) gfx_cor(pill, 0.5f, 1.0f, 1.0f, 1.0f, 0.12f * (1.0f - f) * ac);
      if (f > 0.01f) gfx_cor(tvPillFoco(pill, f), 0.5f, FR, FG, FB, f * ac);
      { int emFoco = f > 0.5f;
        int c = emFoco ? TINTA_FOCO : 235;
        const char *rot;
        float ccx = pill.x + TV_COL_CX;
        if (d < MENU_ST0) {
          tvChip(ccx, cy, f, ac);
          tvIcone(d, ccx, cy, f, ac);
          rot = tvRotulo(d);
        } else {
          const ColFolder *pf = col_folder(pastas[d - MENU_ST0]);
          tvCirculoPasta(pf, ccx, cy, ac);
          rot = pf ? pf->title : "";
        }
        { TxtLinha l = txt_linha_corta(TXT_TV_ROTULO, rot, c, c, c, 255,
                                       pill.w - TV_ROT_X - 16.0f);
          txt_desenhar_alpha(l, pill.x + TV_ROT_X, cy - l.h * 0.5f, ac); } }
      ac = ac0;
      if (aberto && ponteiro_ativo() && y >= Q.y - 1.0f && y + TV_LINHA_H <= Q.y + Q.h + 1.0f)
        ponteiro_alvo(Q.x, y, Q.w, TV_LINHA_H, ponteiroLinha, NULL, d, 0);
    }
    gfx_sem_recorte();
  }
}


// ===========================================================================
// BARRA NO TOPO (#461, ajustes_menu_topo; layouts Moderna e Padrao): a tira
// de abas do PlxNative (rust-modules/ui/src/widgets.rs, draw_tab_row,
// TabStrip, Capsule; GPL-3.0-or-later, (c) 2026 Gleb Linnik e contribuidores,
// commit 9d2777fe; ver docs/CREDITS.md). O PERFIL e um circulo avulso no canto
// esquerdo, a TRILHA de vidro fica no centro abracando as pilulas (rotulo em
// texto puro; a Busca e uma pilula quadrada so com o icone, na ponta direita)
// e a ilha do relogio, no canto direito, divide com os dois a mesma linha de
// centro. Fechada ela so aparece na HOME com o foco no destaque
// (home_topo_fracao, via menu_pilula_mostrar): nas outras telas o canto e do
// titulo delas e a barra abre por cima. Sem rail fixa o conteudo usa a largura toda.
// ===========================================================================
// Medidas deles (tela de 1920, a nossa) a 0,9: o corpo do rotulo deles e 28 e
// o nosso TXT_BODY 25. TK leva px da tela REAL para a tela da barra.
#define TK(v)          ((v) / NV_MENU_ESCALA)
#define TP(v)          TK((v) * 0.9f)
// A27: the track, the avatar chip and the clock island are one 60 px line
// (NV_CHROME_*), the tab capsules 48 inside a 6 px inset, pad 22, gap 2.
#define TOPO_X         TK(NV_CHROME_EDGE)
#define TOPO_CY        TK(NV_CHROME_TOP + NV_CHROME_H * 0.5f)   // linha de centro da ilha
#define TOPO_PILULA_H  TK(NV_CHROME_H - 2.0f * NV_CHROME_INSET)
#define TOPO_PAD       TK(22.0f)    // de cada lado do rotulo
#define TOPO_VAO       TK(2.0f)     // entre duas pilulas
#define TOPO_TRILHA    TK(NV_CHROME_INSET)   // trilha -> pilula, igual nos 4 lados
#define TOPO_ICONE     TK(26.0f)    // a pilula da Busca e quadrada
#define TOPO_H         (TOPO_PILULA_H + 2.0f * TOPO_TRILHA)
#define TOPO_Y         (TOPO_CY - TOPO_H * 0.5f)
// Sem espaco ate a ilha do relogio, aperta primeiro o vao, depois o respiro;
// so abaixo dos dois minimos o rotulo seria cortado.
#define TOPO_VAO_MIN   TOPO_VAO
#define TOPO_PAD_MIN   TP(10.0f)
#define TOPO_TXT       TXT_BODY     // o corpo dos rotulos da rail
#define TOPO_RELOGIO   ((NV_ILHA_MARGEM_D * gfx_escala_ui_base() + ilha_relogio_largura() + 16.0f) / NV_MENU_ESCALA)

// AS CAPSULAS VIAJAM entre as pilulas (Capsule::step deles): mola criticamente
// amortecida na borda esquerda E na largura (uma pilula larga vira estreita
// sem saltar) e outra no alfa, que e como ela entra e sai da fileira. A de
// SELECAO marca a pagina (destino), a de FOCO segue o controle. Rigidez 240
// (K_TAB_CAP = K_TAB_SCROLL) e 300 (K_APPEAR); a mola deles e w = sqrt(k).
// Abaixo de CAP_LAND_A ela nao esta na tela, entao POUSA na pilula nova em vez
// de atravessar a fileira. Animacoes reduzidas: sempre pousa.
#define TOPO_K_CAP     15.4919f     // sqrt(240)
#define TOPO_K_CAP_A   17.3205f     // sqrt(300)
#define TOPO_POUSA_A   0.02f
typedef struct { float x, vx, w, vw, a, va; int em; } TopoCapsula;
static TopoCapsula topoSel = { .em = -1 }, topoFoc = { .em = -1 };

// Itens da pilula, da esquerda para a direita: os destinos na ordem do enum e a
// Busca por ultimo. O perfil nao entra: e o circulo avulso.
static int topoLista(int *ordem) {
  int n = 0, i;
  for (i = 0; i < MENU_N; i++) if (i != MENU_BUSCAR && mostra(i)) ordem[n++] = i;
  if (mostra(MENU_BUSCAR)) ordem[n++] = MENU_BUSCAR;
  return n;
}

static int topoIndice(const int *ordem, int n, int id) {
  int k;
  for (k = 0; k < n; k++) if (ordem[k] == id) return k;
  return -1;
}

// ESQUERDA/DIREITA: o circulo do perfil e o primeiro da fila, a pilula vem depois.
static void topoMover(int dir) {
  int ordem[NV_MENU_FOCOS + 1], n = 0, k, p = 0;
  ordem[n++] = MENU_RODAPE;
  n += topoLista(ordem + 1);
  for (k = 0; k < n; k++) if (ordem[k] == linha) p = k;
  p += dir;
  if (p >= 0 && p < n) linha = ordem[p];
}

static const char *topoRotulo(int i) {
  // "Perfil e Stats" nao cabe na pilula do topo em pt-BR; na barra vale so "Perfil".
  return i == MENU_PERFIL ? i18n("Perfil") : ROTULOS[i];
}

// A tira medida: larguras das pilulas (rotulo + 2 x respiro; a Busca quadrada),
// o vao entre elas e a largura da area das pilulas (sem a trilha), tudo em px
// da tela da barra. tab_metrics_from + tab_content_w deles, com o aperto no
// lugar da rolagem: a nossa tira nunca rola.
typedef struct { int n, ordem[NV_MENU_FOCOS], cortado; float ws[NV_MENU_FOCOS], vao, pad, rotMax, larg; } TopoTira;

static void topoMedir(TopoTira *t) {
  int k, nl = 0, ni = 0;
  float txt = 0.0f, disp, fixo;
  t->n = topoLista(t->ordem);
  for (k = 0; k < t->n; k++) {
    if (t->ordem[k] == MENU_BUSCAR) ni++;
    else { txt += (float)txt_largura(TOPO_TXT, topoRotulo(t->ordem[k])); nl++; }
  }
  disp = NV_TELA_W - 2.0f * TOPO_RELOGIO - 2.0f * TOPO_TRILHA;
  fixo = txt + ni * TOPO_PILULA_H;
  t->vao = TOPO_VAO; t->pad = TOPO_PAD; t->rotMax = 1e6f; t->cortado = 0;
  if (t->n > 1 && fixo + 2.0f * nl * t->pad + (t->n - 1) * t->vao > disp)
    t->vao = fmaxf(TOPO_VAO_MIN, (disp - fixo - 2.0f * nl * t->pad) / (t->n - 1));
  if (nl && fixo + 2.0f * nl * t->pad + (t->n - 1) * t->vao > disp)
    t->pad = fmaxf(TOPO_PAD_MIN, (disp - fixo - (t->n - 1) * t->vao) / (2.0f * nl));
  if (nl && fixo + 2.0f * nl * t->pad + (t->n - 1) * t->vao > disp) {
    t->rotMax = (disp - ni * TOPO_PILULA_H - (t->n - 1) * t->vao - 2.0f * nl * t->pad) / nl;
    t->cortado = 1;
  }
  t->larg = t->n > 1 ? (t->n - 1) * t->vao : 0.0f;
  for (k = 0; k < t->n; k++) {
    t->ws[k] = t->ordem[k] == MENU_BUSCAR ? TOPO_PILULA_H
             : 2.0f * t->pad + txt_linha_corta(TOPO_TXT, topoRotulo(t->ordem[k]), 255, 255, 255, 255, t->rotMax).w;
    t->larg += t->ws[k];
  }
}

// x da pilula k na tira (0 = inicio dela): tab_pill_x deles.
static float topoPilulaX(const TopoTira *t, int k) {
  float x = 0.0f;
  int i;
  for (i = 0; i < k && i < t->n; i++) x += t->ws[i] + t->vao;
  return x;
}

static void topoCapsulaPasso(TopoCapsula *c, const TopoTira *t, int k, int pousa, float dt) {
  if (k >= 0) {
    float x = topoPilulaX(t, k), w = t->ws[k];
    if (pousa || c->em < 0 || c->a < TOPO_POUSA_A) { c->x = x; c->w = w; c->vx = c->vw = 0.0f; }
    c->em = k;
    c->x = anim_mola2(&c->vx, c->x, x, dt, TOPO_K_CAP);
    c->w = anim_mola2(&c->vw, c->w, w, dt, TOPO_K_CAP);
    c->a = anim_mola2(&c->va, c->a, 1.0f, dt, TOPO_K_CAP_A);
  } else {
    // Nada a marcar: SEGURA a posicao e apaga, para sair e voltar a mesma
    // pilula ser um esmaecer e nao uma ida e volta.
    c->em = -1;
    c->a = anim_mola2(&c->va, c->a, 0.0f, dt, TOPO_K_CAP_A);
  }
}

// Quanto da pilula (x, w) a capsula cobre, 0..1 (cap_cover deles): a UNICA
// regra de onde sai a tinta do rotulo, entao ela nunca discorda do fundo que
// desliza por baixo.
static float topoCobre(float px, float pw, const TopoCapsula *c) {
  float lo, hi;
  if (pw <= 0.0f) return 0.0f;
  lo = fmaxf(px, c->x);
  hi = fminf(px + pw, c->x + c->w);
  return limita01((hi - lo) / pw);
}

static void topoAtualizar(float dt) {
  TopoTira t;
  int pousa = anim_politica_reduzida || ajustes_animacoes_reduzidas();
  tvPilAlfa = anim_mola(tvPilAlfa, tvPilAlvo, dt, TV_MOLA_PILULA);
  topoMedir(&t);
  topoCapsulaPasso(&topoSel, &t, topoIndice(t.ordem, t.n, destino), pousa, dt);
  topoCapsulaPasso(&topoFoc, &t, aberto ? topoIndice(t.ordem, t.n, linha) : -1, pousa, dt);
}

void menu_topo_geo(MenuTopoGeo *o) {
  TopoTira t;
  float e = NV_MENU_ESCALA, x0;
  int k;
  topoMedir(&t);
  memset(o, 0, sizeof *o);
  x0 = floorf((NV_TELA_W - t.larg) * 0.5f);
  o->n = t.n; o->cortado = t.cortado;
  o->y = (TOPO_CY - TOPO_PILULA_H * 0.5f) * e; o->h = TOPO_PILULA_H * e;
  for (k = 0; k < t.n && k < MENU_TOPO_MAX; k++) {
    o->x[k] = (x0 + topoPilulaX(&t, k)) * e; o->w[k] = t.ws[k] * e;
    o->busca[k] = t.ordem[k] == MENU_BUSCAR;
    o->foco[k] = topoFoc.a * topoCobre(topoPilulaX(&t, k), t.ws[k], &topoFoc);
  }
  o->trilha[0] = (x0 - TOPO_TRILHA) * e; o->trilha[1] = TOPO_Y * e;
  o->trilha[2] = (t.larg + 2.0f * TOPO_TRILHA) * e; o->trilha[3] = TOPO_H * e;
  o->avatar[0] = TOPO_X * e; o->avatar[1] = TOPO_Y * e; o->avatar[2] = o->avatar[3] = TOPO_H * e;
  o->sel[0] = (x0 + topoSel.x) * e; o->sel[1] = topoSel.w * e; o->sel[2] = limita01(topoSel.a);
  o->foc[0] = (x0 + topoFoc.x) * e; o->foc[1] = topoFoc.w * e; o->foc[2] = limita01(topoFoc.a);
}

// Uma capsula (A27): a de selecao e so um preenchimento #F3F2EF a 18 %, sem
// aro, blur nem sombra; a de foco, #F3F2EF opaco, a excecao ao material.
static void topoCapsula(const TopoCapsula *c, float x0, float y, int foco, float a) {
  GfxRect r = { x0 + c->x, y, c->w, TOPO_PILULA_H };
  float ca = limita01(c->a) * a;
  if (ca <= 0.004f || c->w <= 0.5f) return;
  gfx_cor(r, 0.5f, 0xf3 / 255.0f, 0xf2 / 255.0f, 0xef / 255.0f, (foco ? 1.0f : .18f) * ca);
}

static void topoDesenhar(void) {
  TopoTira t;
  int k;
  float a, x0, y, fAv;
  int focos = aberto || desliza >= 0.002f || expande > 0.004f;
  MenuGeo g = { 0 }, gAv = { 0 };
  topoMedir(&t);
  x0 = floorf((NV_TELA_W - t.larg) * 0.5f);   // compacta e centrada na tela
  y = TOPO_CY - TOPO_PILULA_H * 0.5f;
  g.x = x0 - TOPO_TRILHA; g.y = TOPO_Y; g.w = t.larg + 2.0f * TOPO_TRILHA; g.h = TOPO_H; g.raio = TOPO_H * 0.5f;
  gAv.x = TOPO_X; gAv.y = TOPO_Y; gAv.w = gAv.h = TOPO_H; gAv.raio = TOPO_H * 0.5f;
  // Fechada vale a opacidade que a tela pediu (0 fora da home); abrindo ela
  // nasce pela rampa do veu, e fechando volta para la.
  a = fmaxf(tvPilAlfa, anim_suave(desliza));
  if (a <= 0.01f) {
    // Escondida: uma faixa na borda de cima abre ao passar, como a rail recolhida.
    if (ponteiro_ativo()) ponteiro_alvo(0, 0, NV_TELA_W, MK(20.0f), ponteiroLinha, NULL, destino, 0);
    return;
  }
  // O veu desce do alto: luz ESCURA centrada acima da barra, morrendo no meio da tela.
  if (desliza > 0.002f)
    gfx_luz_canto((GfxRect){ 0, 0, NV_TELA_W, 760.0f }, 0.0f, g.x + g.w * 0.5f, -700.0f, 1400.0f,
                  0, 0, 0, NV_MENU_VEU * desliza);
  if (aberto) {
    ponteiro_alvo(0, 0, NV_TELA_W, NV_TELA_H, NULL, ponteiroFora, 0, 0);
    ponteiro_alvo(g.x, g.y, g.w, g.h, NULL, NULL, 0, 0);
  }
  desenhaMaterial(&g, a);
  desenhaMaterial(&gAv, a);
  // O perfil: o avatar e do tamanho de uma pilula, dentro do seu circulo de
  // trilha (CHIP_D = TAB_PILL_H); em foco ganha o aro claro.
  fAv = focos ? animFoco[MENU_RODAPE] : 0.0f;
  tvAvatar((GfxRect){ gAv.x + TOPO_TRILHA, gAv.y + TOPO_TRILHA, TOPO_PILULA_H, TOPO_PILULA_H }, a);
  if (fAv > 0.01f) gfx_vidro_borda((GfxRect){ gAv.x, gAv.y, gAv.w, gAv.h }, 0.5f, .75f * fAv * a);
  if (ponteiro_ativo() && a > 0.5f) ponteiro_alvo(gAv.x, gAv.y, gAv.w, gAv.h, ponteiroLinha, NULL, MENU_RODAPE, 0);
  gfx_recorte(g.x, g.y, g.w, g.h);
  // As capsulas por BAIXO dos rotulos: selecao, e o foco por cima dela.
  topoCapsula(&topoSel, x0, y, 0, a);
  if (focos) topoCapsula(&topoFoc, x0, y, 1, a);
  for (k = 0; k < t.n; k++) {
    GfxRect r = { x0 + topoPilulaX(&t, k), y, t.ws[k], TOPO_PILULA_H };
    int i = t.ordem[k];
    float px = topoPilulaX(&t, k), fm, sm, tr, tg, tb, ta;
    // A27 ink: #F3F2EF at .74 idle (.80 on the opaque fallback) -> 1.0 as the
    // selection arrives, then #121316 as the focus capsule covers the pill.
    fm = focos ? limita01(topoFoc.a) * topoCobre(px, t.ws[k], &topoFoc) : 0.0f;
    sm = limita01(topoSel.a) * topoCobre(px, t.ws[k], &topoSel);
    tr = anim_mistura(0xf3, 0x12, fm) / 255.0f;
    tg = anim_mistura(0xf2, 0x13, fm) / 255.0f;
    tb = anim_mistura(0xef, 0x16, fm) / 255.0f;
    ta = a * anim_mistura(ajustes_vidro() && !gfx_efeitos_minimos() ? .74f : .80f, 1.0f, fmaxf(sm, fm));
    if (ponteiro_ativo() && a > 0.5f) ponteiro_alvo(r.x, r.y, r.w, r.h, ponteiroLinha, NULL, i, 0);
    if (i == MENU_BUSCAR) iconeTraco(i, r.x + r.w * 0.5f, y + TOPO_PILULA_H * 0.5f, TOPO_ICONE, tr, tg, tb, ta);
    else {
      TxtLinha l = txt_linha_corta(TOPO_TXT, topoRotulo(i), 255, 255, 255, 255, t.rotMax);
      txt_desenhar_tom(l, r.x + (r.w - l.w) * 0.5f, y + (TOPO_PILULA_H - l.h) * 0.5f, tr, tg, tb, ta);
    }
  }
  gfx_sem_recorte();
}
