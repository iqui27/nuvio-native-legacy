// A ILHA DO RELOGIO DENTRO DO PLAYER (Glass UI, mockup aprovado em 03/10).
//
// Fora do player a ilha e ilha.c. Dentro dele ela era proibida (o player
// tinha relogio proprio no canto e um toast de acento no meio da tela); o dono
// aprovou o contrario: a hora vira a PILULA da ilha (hora · "termina as"),
// no canto escolhido em Ajustes > Posicao do relogio, e tudo o que e pequeno
// no player NASCE DELA, com a mola da ilha (ILHA_MOLA de ilha.c, a mesma do
// Spotlight): os avisos (proporcao, reconexao, "Fonte 2 de 3") abrem a pilula
// para 64 com icone + frase; as listas de Audio e Legendas, o estilo da
// legenda, o carregamento e o erro da fonte fazem a pilula CRESCER ate o
// corpo, com a linha da pilula como cabecalho. Animacoes reduzidas: nasce
// pronta.
//
// COMO SE USA. Cada quadro, quem quer a ilha faz um pedido (plrilha_pedir);
// o pedido vale so para aquele quadro. Sem pedido, a ilha mostra a hora
// enquanto o OSD estiver de pe (plrilha_relogio) e some com ele. O CORPO e
// desenhado por quem pediu, pela funcao `corpo`, dentro do retangulo que a
// ilha da — e a ilha continua chamando o ultimo corpo enquanto encolhe, com o
// alfa caindo, para ele sair junto com a forma.
//
// app.c desenha a ilha depois de todas as camadas do player (plrilha_desenhar).
#ifndef NV_PLRILHA_H
#define NV_PLRILHA_H
#include "gfx.h"
#include <SDL2/SDL.h>

typedef void (*PlrIlhaCorpo)(GfxRect corpo, float a, void *u);

typedef struct {
  const char *icone;        // art/icones (pl_*, aj_*); NULL = nenhum
  int corIcone;             // 0 = branco, 1 = ambar (aviso), 2 = acento
  const char *texto;        // frase antes da hora (ja traduzida); NULL = so a hora
  int semFim;               // 1 = sem "termina as"
  int pontos, ponto;        // > 0: N pontos (o ciclo da proporcao), `ponto` aceso
  const char *direita;      // ponta direita do cabecalho do corpo (ja traduzido)
  float w, h;               // o CORPO (abaixo do cabecalho de 64); 0 = so a pilula
  PlrIlhaCorpo corpo;
  void *u;
  int aberta;               // 1 = pilula aberta de 64 (aviso), 0 = 56
  int modal;                // 1 = miolo do modal (.86)
  int video;                // 1 = video vivo atras (botao/isla translucidos, nunca opacos)
  int ancoraTopo;           // 1 = (centro) o corpo acompanha o TOPO da forma enquanto ela cresce ou encolhe, em vez de ficar centrado no tamanho final
  int centro;               // 1 = cartao SOZINHO no meio da tela (sem relogio nem medidor); so o corpo
  int baixa;                // 1 = prioridade baixa: cede a qualquer outro pedido do quadro (a guia parental)
  int respira;              // 1 = o ponto que respira antes da frase (atividade: o canal sintonizando)
  int voltaRelogio;         // 1 = ao acabar, o corpo vira a pilula da HORA, segura um instante e so entao sai (guia parental). Relogio desligado: encolhe e some no lugar
  // PONTEIRO (#99): a pilula SEM corpo e clicavel. Vira o `focar` de um alvo do
  // tamanho da pilula assentada; o clique entrega OK a quem trata a tecla dela
  // (a de falha da legenda: faixas_pilula_tecla). NULL = so informativa.
  void (*ponteiroOk)(int a, int b);
  float insetEsq;           // recuo esquerdo do cabecalho do corpo (o titulo); 0 = 28
  int semSombra;            // 1 = material sem a sombra caida (Fontes)
  int tituloLeve;           // 1 = titulo delicado (PLR_TIT_*): Medium um degrau abaixo, cinza polido
} PlrIlhaPedido;

// Titulo delicado (#205, "Central de controle"): #A8ADB5 no vidro escuro (~8:1).
#define PLR_TIT_ESTILO TXT_G20M
#define PLR_TIT_R 168
#define PLR_TIT_G 173
#define PLR_TIT_B 181

void plrilha_pedir(const PlrIlhaPedido *p);
// A pilula da hora acompanhando o OSD: `a` e a opacidade dos controles.
// `falta` em segundos ate o fim do titulo (< 0 = sem "termina as": canal).
void plrilha_relogio(float a, double falta);
// #202: a velocidade da reproducao em centesimos (velocidade.h). Fora de
// 100 a pilula mostra "1,5x" depois do "termina as". Vale ate a proxima chamada.
void plrilha_velocidade(int cent);
// O titulo aberto: com um painel de pe, vai a esquerda da linha do relogio
// (dono, 11/10). NULL/"" = nada. Vale ate a proxima chamada.
void plrilha_titulo(const char *t);
// 1 = ancorada a direita (Posicao do relogio), para o OSD por as marcas de
// formato no outro canto.
int  plrilha_direita(void);
// Esconde a ilha neste quadro (a folha de Fontes por cima de tudo).
void plrilha_esconder(void);
void plrilha_desenhar(Uint32 agora);
// O retangulo desenhado no ultimo quadro; 0 = fora da tela.
int  plrilha_rect(GfxRect *r);
// O pedido deste quadro tem corpo e ele ja esta assentado (para quem
// precisa saber se o corpo esta de pe, como o foco do erro).
float plrilha_corpo_alfa(void);

#ifdef NV_SHOT_HOOKS
// Capturas: hora fixa (0 = a do relogio).
void plrilha_shot_hora(time_t t);
int plrilha_shot_centro(GfxRect *r);
// A linha do ultimo quadro: x do texto, x do fim da linha, se tinha "termina as".
void plrilha_shot_linha(float *txtX, float *fimX, int *temFim);
#endif

#endif
