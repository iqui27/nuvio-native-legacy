// Menu lateral (a "sidebar" do app Apple TV na LG).
//
// Ele nao e uma tela: e uma camada que aparece POR CIMA do conteudo e toma o
// D-pad enquanto esta visivel. Por isso a API foge do padrao de tela em dois
// pontos, de proposito:
//   - nao tem menu_encerrar: texto e icones usam os caches de text.c e gfx.c;
//     o modulo nao possui alocacoes independentes.
//   - nao tem menu_quer_sair: fechar o menu nunca fecha o app. O Back aqui so
//     devolve o foco ao conteudo, e quem decide sair continua sendo a home.
//
// O ciclo no aparelho: o foco esta na primeira coluna de uma fileira, o usuario
// aperta ESQUERDA, a barra desliza da borda e ganha o foco. DIREITA ou OK
// escolhe o destino e devolve o foco ao conteudo.
#ifndef NV_MENU_H
#define NV_MENU_H
#include <SDL2/SDL.h>

// Os destinos do app, na ordem em que aparecem na barra. MENU_N fecha o enum
// para quem quiser dimensionar vetor por destino sem repetir o numero 4.
// ORDEM DA REFERENCIA: Inicio primeiro, depois Explorar e os destinos de
// catalogo. A descoberta fica perto da Home porque e uma porta de entrada,
// enquanto Biblioteca e Perfil continuam mais abaixo, como no shell legado.
typedef enum {
  MENU_INICIO,
  MENU_EXPLORAR,
  MENU_GUIA,
  MENU_ESPORTES,
  MENU_BUSCAR,
  MENU_BIBLIOTECA,
  // AGENDA — o calendario das series acompanhadas. Fica DEPOIS da Biblioteca
  // e antes do Perfil de proposito: as duas falam do acervo da pessoa (o que
  // ela guardou, e quando o que ela guardou sai), e o Perfil e sobre ela.
  MENU_AGENDA,
  MENU_PERFIL,
  MENU_AJUSTES,
  MENU_N
} MenuDestino;

// Zera destino e animacao. So e necessario se o app reinicializar a UI; o
// estado inicial ja e valido sem chamar (destino = MENU_INICIO, barra fora).
int  menu_iniciar(void);

// Desliza a barra para dentro E entrega o foco a ela. Chamar com o menu ja
// aberto nao faz nada, entao e seguro ligar direto no ESQUERDA da home.
void menu_abrir(void);
// Fecha sem escolher: o destaque volta para o destino atual.
void menu_fechar(void);
// ABRE POR CIMA DE UMA CAMADA que continua viva embaixo (a pagina do titulo):
// DIREITA/Voltar so devolvem o foco a ela; OK num destino (mesmo o atual) fica
// em menu_escolheu para o app fechar a camada e navegar. `semRailFixa` 1 quando
// a camada nao mostra a rail (a pagina do titulo): o painel nasce sem ela.
void menu_abrir_sobre(int semRailFixa);
// 1 desde menu_abrir_sobre ate a saida da barra assentar: e o que o app usa
// para desenhar a barra por cima da camada (inclusive recolhendo).
int  menu_sobre(void);
// 1 uma vez quando a pessoa ESCOLHEU algo na barra (destino, rodape, pasta;
// o Spotlight nao, ele abre por cima) — ao contrario de Voltar/DIREITA por cima. Consome a flag.
int  menu_escolheu(void);

// 1 enquanto a barra e dona do D-pad. Vira 0 no instante da escolha, ainda com
// a animacao de saida rodando — e esse o sinal que o conteudo deve usar para
// voltar a responder as teclas, senao o D-pad fica morto durante o recolhimento.
int  menu_aberto(void);
// 1 enquanto ainda ha pixel para desenhar (inclui a saida). Serve para o loop
// decidir se vale sequer chamar menu_desenhar.
int  menu_visivel(void);

// Destino em vigor (um MenuDestino). menu_definir_destino existe para o app
// impor o estado inicial ou reagir a uma navegacao que nao veio da barra.
int  menu_destino(void);
void menu_definir_destino(int destino);
// 1 uma unica vez, no quadro em que o usuario escolheu um destino DIFERENTE do
// que estava em vigor. Consome a flag: quem le, trata. Sem isso o app teria que
// guardar o destino anterior so para descobrir que ele mudou.
int  menu_mudou_destino(void);

const char *menu_rotulo(int destino);

// 1 uma unica vez, no quadro em que o usuario escolheu o rodape ("trocar de
// usuario"). Consome a flag, como menu_mudou_destino. Nao e um MenuDestino de
// proposito: trocar de perfil nao e uma aba do app, e uma acao que devolve a
// pessoa a tela de escolha.
int  menu_pediu_trocar(void);
// Layout Dinamica: indice de col_folder da pasta de Streaming escolhida na
// barra (a fileira "Streaming" mora la nesse layout), uma vez; -1 sem pedido.
// Quem le abre a colecao (vertudo_colecao), como OK na pasta da home.
int  menu_pediu_colecao(void);
// OK SEGURADO em "Buscar": abrir o Spotlight. Consome a flag.
int  menu_pediu_spotlight(void);

// --- O MENU DOS LAYOUTS CLASSICOS -------------------------------------------
// MODERNA: uma ilha flutuante na margem da ilha do relogio (x NV_MENU_MODERNA_X,
// centrada na altura e ampliada em20%): fechada a pilula vertical de icones, aberta o painel com
// rotulos. PADRAO: o mesmo painel colado na borda esquerda e centrado na
// altura (fechado, a mesma pilula estreita na borda). Na Moderna a ilha do
// relogio se posiciona na margem do menu (ilha_posicionar). Devolve a borda
// direita do menu em px da tela REAL (0 sem menu na tela, ou no layout
// Dinamica, que tem a pilula abaixo). Abrindo ja devolve a largura aberta;
// fechando, a que ainda esta na tela. A largura que o CONTEUDO reserva para a
// rail fixa e ajustes_rail_largura_fixa (NV_MENU_RAIL_BORDA_* em layout.h).
// O menu usa escala propria (90%, ampliada20% na Moderna), nao o Tamanho da
// interface; tudo o que esta API devolve ja vem em px da tela REAL.
#define NV_MENU_MODERNA_X 48.0f   // px da tela REAL
float menu_barra_borda(void);
// Barra no topo: a geometria da tira como desenhada agora, em px da tela REAL
// (retangulos x, y, w, h). `sel`/`foc` sao as capsulas de selecao e de foco
// (x, w, alfa); `foco[k]` e quanto a de foco cobre a pilula k (a tinta do
// rotulo); `cortado` 1 se algum rotulo nao coube inteiro. So os testes leem.
#define MENU_TOPO_MAX 16
typedef struct {
  int n, cortado, busca[MENU_TOPO_MAX];
  float y, h, x[MENU_TOPO_MAX], w[MENU_TOPO_MAX], foco[MENU_TOPO_MAX];
  float trilha[4], avatar[4], sel[3], foc[3];
} MenuTopoGeo;
void menu_topo_geo(MenuTopoGeo *g);

// --- PILULA DO LAYOUT DINAMICA (barra estilo Apple TV) ----------------------
// No layout Dinamica da home nao ha rail: fechada, a barra e uma pilula
// "‹ (icone) Secao" no topo esquerdo, por cima do conteudo. Este canto e DELA.
// Quem quiser algo no topo (ex.: a pilula de relogio/avisos) se posiciona AO
// LADO (x >= x+w+NV_MENU_PILULA_VAO, mesmo y/h) ou ABAIXO (y >= y+h+vao).
//   - menu_pilula_rect: 1 e o retangulo atual (inclui a seta "‹"; a largura
//     muda com o rotulo da secao) quando o layout e Dinamica; 0 e tudo zero nos
//     outros layouts (la o topo esquerdo esta livre).
//   - menu_pilula_alfa: opacidade com que ela esta na tela neste quadro (0 com
//     a pagina rolada para baixo ou enquanto o painel aberto a substitui).
//   - menu_pilula_mostrar: quem e dono da tela diz se a pilula deve aparecer
//     (0..1; negativo = some na hora); a barra anima ate la. app.c chama a cada quadro: so na HOME, e
//     so com ela no topo (home_topo_fracao); 0 nas outras telas, que tem o
//     proprio titulo nesse canto. Ou seja: fora da home o canto esta livre.
#define NV_MENU_PILULA_X     40.0f
#define NV_MENU_PILULA_Y     44.0f
#define NV_MENU_PILULA_H     60.0f
#define NV_MENU_PILULA_SETA  26.0f   // largura da seta antes da pilula
#define NV_MENU_PILULA_VAO   16.0f
int   menu_pilula_rect(float *x, float *y, float *w, float *h);
// 1 no layout Dinamica: a pilula diz o nome da secao, entao as telas do menu
// (Explorar, Busca, Biblioteca, Agenda, Perfil, Ajustes) NAO desenham o
// proprio titulo grande — ele ficaria duplicado atras dela.
int   menu_pilula_titulo(void);
float menu_pilula_alfa(void);
void  menu_pilula_mostrar(float alvo);
// 0 enquanto a BARRA DA APPLE TV esta aberta: o cabecalho do painel ja mostra
// o relogio e a ilha do canto desenharia a mesma hora duas vezes (captura da
// TCL: 11:29 sobre 11:29). So o layout Dinamica; na rail classica a ilha
// continua ao lado (menu_aberto nao a esconde la).
int   menu_relogio_cabe(void);

void menu_evento(const SDL_Event *e);
void menu_atualizar(float dt, Uint32 agora);
// Desenhe por ULTIMO: o menu escurece e cobre tudo que veio antes.
void menu_desenhar(Uint32 agora);

#endif
