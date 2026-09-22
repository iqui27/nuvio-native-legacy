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

void menu_evento(const SDL_Event *e);
void menu_atualizar(float dt, Uint32 agora);
// Desenhe por ULTIMO: o menu escurece e cobre tudo que veio antes.
void menu_desenhar(Uint32 agora);

#endif
