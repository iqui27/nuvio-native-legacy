// SALVAR NA ILHA DO RELOGIO (pedido do dono, 03/10/2026).
//
//   1. "colocar animacao na ilha do relogio quando eu adiciono algo na lista":
//      toda gravacao na lista (o "+" do detalhe, "Salvar" do menu do cartaz, o
//      "Salvar" da recomendacao na ilha) termina em ilhasalvar_aviso: a capa
//      voa ate a pilula e ela diz "Salvo em <destino>"; tirar da lista diz
//      "Removido da lista", sem voo (ilha_salvar, ilha.h).
//   2. "a primeira vez que usar, perguntar na ilha onde quer salvar": a PRIMEIRA
//      vez que alguem poe um titulo na lista, ANTES de salvar, a pilula cresce
//      ate o modal "Onde o + salva?" com as opcoes de Ajustes > "Onde o + salva"
//      (Lista do Nuvio e, so se a conta estiver ligada, Trakt e Simkl). A
//      resposta vai para o MESMO valor de Ajustes, a marca e gravada e o salvar
//      segue, ja com a animacao.
//
// NAO HA SEGUNDA PERGUNTA ("como quer salvar?"): o app so tem UMA lista e nao
// ha categoria, pasta nem modo de exibir que se escolha ao salvar. O "como" que
// existe de verdade e o destino, e ele e esta pergunta.
//
// A marca e salvar-perguntado.txt. Quem ja respondeu o explicador antigo
// (salvos-intro.txt) ou ja tem titulos salvos nao e perguntado de novo.
#ifndef NV_ILHASALVAR_H
#define NV_ILHASALVAR_H
#include "catalogo.h"
#include <SDL2/SDL.h>

// A PRIMEIRA vez: devolve 1 = a pergunta esta na ilha e este salvar FICA
// ADIAMENTO — quem chamou desiste agora (nao grava nada); a resposta (ou o
// Voltar, que mantem o padrao) grava o titulo por ilhasalvar_executar. 0 =
// pode salvar ja. So pergunta ao ENTRAR na lista; tirar nunca pergunta.
int  ilhasalvar_perguntar(const CatItem *ci, int entrar);

// O aviso e o voo da capa, para depois de gravar. `entrou` 1 = entrou, 0 = saiu.
void ilhasalvar_aviso(const CatItem *ci, int entrou);

// Botao de um modal de chave "salvar:..." (ilhasinais.c entrega).
void ilhasalvar_acao(const char *chave, int botao);

// Uma vez por quadro: detecta o Voltar da pergunta.
void ilhasalvar_passo(Uint32 agora);

// Grava como o "+" do detalhe faz (lista local, Trakt/Simkl conforme Ajustes,
// marca do catalogo) e anima. E o caminho do salvar adiado.
void ilhasalvar_executar(const CatItem *ci, int entrar);

// 1 enquanto a pergunta esta na tela (testes).
int  ilhasalvar_pergunta_aberta(void);
// Troca de perfil: abandona a escolha de destino ainda nao confirmada.
void ilhasalvar_abrir_sessao(void);
// Rotulo do destino em vigor, ja traduzido ("Lista do Nuvio" ...).
const char *ilhasalvar_destino(void);

#endif
