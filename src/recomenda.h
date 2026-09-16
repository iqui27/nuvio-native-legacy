// RECOMENDAR UM TITULO A UM AMIGO — o lado cliente.
//
// POR QUE EXISTE E POR QUE CONTRA UM SERVIDOR PROPRIO: o Supabase do Nuvio nao
// tem nada social (medido em 15/09/2026 no indice OpenAPI do PostgREST: 13
// tabelas, 52 RPC, nenhuma de amigo, recomendacao ou aviso) e este repositorio
// e um fork nao oficial — criar tabela la nao e uma decisao nossa. O servico
// vive em servidor/recomendacoes/ e nao toca em nada do Nuvio. Ver
// PLANO-SOCIAL-RECOMENDACOES.md.
//
// A DISCIPLINA E A DE atualizacao.c: um fio proprio para a rede, todo o estado
// atras de um mutex, e o laco de desenho so LE. Nenhuma funcao daqui bloqueia,
// com uma excecao anotada (recomenda_enviar apenas ENFILEIRA).
//
// SEM URL COMPILADA O MODULO INTEIRO NAO EXISTE. recomenda_ativo() devolve 0,
// nenhuma funcao abre conexao, e quem desenha esconde a aba e o item de menu.
// O dono publica builds sem NUVIO_REC_URL em local.properties, e um botao que
// so dá erro e pior que botao nenhum.
//
// REALTIME AQUI E SONDAGEM. Este app so tem HTTP (rede.c carrega a libcurl do
// aparelho por dlopen; nao ha WebSocket nem SSE, e no Tizen o mesmo codigo roda
// em WebAssembly). A sondagem e de 60 s com o app aberto, e o caso comum custa
// um 304 sem corpo porque o cliente devolve o ETag que recebeu.
#ifndef NV_RECOMENDA_H
#define NV_RECOMENDA_H

#include "catalogo.h"
#include <SDL2/SDL.h>
#include <stddef.h>

// Teto da lista guardada. 60 x ~900 bytes = ~54 KB estaticos, pela mesma razao
// de SALVOS_MAX: esta lista e percorrida por quadro com o painel aberto, e o
// alvo Tizen ja bateu no teto de memoria do WebAssembly (TIZEN-MEMORIA.md).
// Sessenta recomendacoes sao mais do que qualquer pessoa real recebe antes de
// a retencao de 90 dias do servidor apagar as velhas.
#define REC_MAX           60
#define REC_CONTATOS_MAX  40
// Quantos modelos prontos existem. Texto livre (modelo -1) e etapa posterior.
#define REC_MODELOS        6

typedef struct {
  long long id;        // id no servidor; e tambem o cursor
  long long criado;    // epoch em segundos
  char de[96];         // "trakt:<slug>" | "nuvio:<sub>"
  char deNome[64];     // nome de exibicao de quem mandou
  char imdb[24];
  char tipo[8];        // "movie" | "series"
  char titulo[160];
  char poster[512];
  char ano[16];
  int  modelo;         // indice do template; -1 = texto livre
  char texto[72];      // so quando modelo == -1
  int  visto;          // 0 = ainda conta para o selo
} RecItem;

typedef struct {
  char id[96];
  char nome[64];
  char origem[8];      // "trakt" | "nuvio"
} RecContato;

// 0 quando o pacote saiu SEM NUVIO_REC_URL. Nesse caso nada mais aqui faz
// coisa alguma, e quem desenha deve esconder a superficie inteira.
int  recomenda_ativo(void);

// Le cursor e cache do disco. Chamar UMA vez no arranque, depois de
// dados_iniciar. Nao abre conexao.
void recomenda_iniciar(void);

// Liga a sondagem. A PRIMEIRA chamada cria o fio e faz um ciclo imediato; as
// seguintes NAO FAZEM NADA.
//
// Ela e idempotente porque quem a chama e o bloco `homePronta` de app.c, que
// roda UMA VEZ POR QUADRO — a mesma posicao de atualizacao_verificar(), e pela
// mesma razao. Sem essa garantia a sondagem de 60 s viraria uma a cada 200 ms,
// que e o passo do laco do fio. Para pedir um ciclo fora de hora, use
// recomenda_pedir_agora.
void recomenda_verificar(void);

// Pede um ciclo AGORA, mesmo com o fio ja de pe: ao abrir a aba Social (para
// nao mostrar lista velha) e ao entrar na tela de escolher o amigo. Nao
// bloqueia — so acorda o fio, que responde em ate 200 ms.
void recomenda_pedir_agora(void);

// Quantas recomendacoes ainda nao vistas — o numero do selo.
int  recomenda_n_novas(void);

int  recomenda_n(void);
// Copia da linha `i` (mais nova primeiro) para `saida`. 1 quando copiou.
//
// COPIA E NAO PONTEIRO de proposito: a lista vive atras de um mutex que o fio
// de rede reescreve, e devolver ponteiro para dentro dela seria entregar ao
// desenho memoria que muda debaixo dele — o mesmo defeito que derrubou
// salvospainel.c quando ele apontava para dentro do CatItem.
int  recomenda_item(int i, RecItem *saida);

// Marca TODAS como vistas: apaga o selo na hora, grava, e enfileira o aviso ao
// servidor para os outros aparelhos concordarem. Chamar quando a aba Social
// abre.
void recomenda_marcar_vistas(void);

// Enfileira um envio; o fio de rede o despacha. 1 quando entrou na fila (ha
// espaco e os dados sao suficientes), 0 quando nem isso. O resultado de
// verdade sai em recomenda_envio_estado().
int  recomenda_enviar(const CatItem *ci, const char *paraId, int modelo,
                      const char *texto);

enum { REC_ENVIO_NADA = 0, REC_ENVIO_INDO, REC_ENVIO_OK, REC_ENVIO_FALHA };
int  recomenda_envio_estado(void);
void recomenda_envio_limpar(void);

// Copia ate `max` contatos. Devolve quantos copiou.
int  recomenda_contatos(RecContato *saida, int max);

// Frase do modelo `i` em portugues (a chave de i18n). NULL fora da faixa.
const char *recomenda_modelo(int i);

// Frase JA PRONTA de uma recomendacao: o modelo traduzido, ou o texto livre
// como veio. Nunca NULL. Compartilhada com quem desenha a aba Social, para as
// duas superficies nunca divergirem na regra de "modelo -1 = texto livre".
const char *rec_frase(const RecItem *r);

// "há 2 h" — a frase INTEIRA passa por i18n como formato, nao montada de
// pedacos (mesma regra do "Salvo há 2 horas" de salvospainel.c).
void rec_quando_texto(char *dst, size_t tam, long long quandoS);

// Apaga cache, cursor e marca do cartao do aparelho. Chamar de
// sync_esquecer_usuario: recomendacao e tao pessoal quanto a lista de salvos.
void recomenda_esquecer(void);

// --- CARTAO DE ABERTURA ------------------------------------------------------
//
// Mesmo formato e mesmas regras do cartao de atualizacao.c: abre uma vez por
// recomendacao, so com a home de pe, e NUNCA por cima de quem esta assistindo.
void recomenda_mostrar_se_houver(void);
int  recomenda_aberta(void);
void recomenda_evento(const SDL_Event *e);
void recomenda_atualizar(float dt, Uint32 agora);
void recomenda_desenhar(Uint32 agora);

// IMDb que o dono pediu para abrir (OK no cartao), ou NULL. Consumido uma vez.
// Quem resolve o id e abre o detalhe e o roteador (app.c) — este modulo nao
// conhece detail.c, pela mesma razao que salvospainel.c nao conhece.
const char *recomenda_pediu_abrir(void);

#endif
