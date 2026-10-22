// CONFERIR VARIAS FONTES AO MESMO TEMPO (2.0.2, ligado de fabrica, so para fontes ja prontas no debrid).
//
// A verificacao do automatico e em SERIE de proposito (issue #130, fonteauto.h):
// conferir uma fonte de debrid faz o servico adicionar o arquivo ao painel, e
// uma por vez e o que garante que so a fonte que vai tocar vira arquivo. Com
// "Conferir varias fontes ao mesmo tempo" (ligado) so as que ja estao em cache no
// debrid — conferir uma delas nao baixa nada — saem da serie por velocidade: as
// primeiras `k` da fila (todas em cache) sao conferidas JUNTAS, e a escolhida e a primeira que
// serve NA ORDEM DA FILA — a resposta mais rapida de uma fonte pior nunca passa
// na frente de uma melhor que ainda esta conferindo. Isto so muda quanto se
// espera, e nunca a qualidade escolhida.
//
// Sem rede e sem a lista de streams: quem confere e o callback (o mesmo
// FonteVerificar de fonteauto.h, que aqui roda em varios fios e tem de ser seguro
// para isso).
#ifndef NV_FONTEPARALELA_H
#define NV_FONTEPARALELA_H
#include "fonteauto.h"

#define FONTEPARALELA_MAX 4

// Confere fila[0..k-1] em paralelo (k limitado a n e a FONTEPARALELA_MAX) e
// devolve o primeiro, na ordem da fila, que serve — assim que ele e conhecido,
// sem esperar as que vem depois dele —, ou -1 se nenhuma das k serve.
// falhou(), se nao NULL, e chamada (num fio da conferencia) para cada uma que
// nao serviu. Fios que ainda conferem quando a funcao volta terminam sozinhos.
// *tocadas (se nao NULL) recebe quantas conferencias comecaram.
// `prazoMs` > 0 trata o que nao respondeu ate la como "nao serviu".
int fonteparalela(const int *fila, int n, int k, FonteVerificar verificar,
                  FonteFalhou falhou, void *u, int *tocadas, unsigned prazoMs);

// A MESMA, para quando `u` tem de viver ate o ULTIMO fio terminar. Os fios que
// ainda conferem quando a funcao volta continuam usando `u`; com `u` na pilha de
// quem chamou, eles liam e escreviam num quadro ja desfeito (o ASAN pegou no
// Mac: SEGV em verificarOuParar lendo a Conferencia de stream_primeira_boa,
// cujo fio ja tinha acabado). soltarU(u), se nao NULL, e chamada UMA vez, no
// fio que soltar a ultima referencia, depois que nenhum fio usa mais `u`.
int fonteparalela_soltando(const int *fila, int n, int k, FonteVerificar verificar,
                           FonteFalhou falhou, void *u, int *tocadas, unsigned prazoMs,
                           void (*soltarU)(void *u));

// Quantas das primeiras da fila entram na conferencia conjunta: o PREFIXO de
// fila[] (ate `max`) em que pronta(fila[i]) e verdadeiro — fontes ja em cache no
// debrid. A primeira que nao esta pronta corta a corrida: dai em diante tudo e
// conferido em serie, uma por vez (#130), e nenhum download extra comeca.
int fonteparalela_prefixo(const int *fila, int n, int max, int (*pronta)(int i, void *u), void *u);

#endif
