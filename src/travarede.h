// TRAVA DE SERIE — a ponte com o mundo: servidor (nuvio-recomendacoes, rotas
// /v1/trava*), conta Nuvio (modo Juntos marca o visto nos outros perfis, e a
// reconciliacao de hora em hora le os vistos dos membros), Trakt dos outros
// perfis desta TV, e os avisos da ilha. O modelo e trava.h; as telas, travaui.h.
//
// NUNCA BLOQUEIA O QUADRO: tudo que fala com a rede roda num fio proprio, um
// pedido por vez, e o resultado e aplicado no fio principal em travarede_passo.
// Sem NUVIO_REC_URL a trava e so local (funciona, nao viaja). Sem conta Nuvio
// (so Trakt) tambem: o servidor so aceita perfis da mesma conta.
//
// FIO PRINCIPAL em tudo, menos travarede_rev (fio do recomenda).
#ifndef NV_TRAVAREDE_H
#define NV_TRAVAREDE_H
#include <SDL2/SDL.h>
#include "vistoep.h"

// Uma vez, depois de dados_iniciar e perfis_carregar_ativo.
void travarede_iniciar(void);
// Uma vez por quadro: acompanha conta/perfis, anda a fila, aplica respostas,
// avisa "alcancou voce" na ilha.
void travarede_passo(Uint32 agora);
// recomenda.c, a cada GET /v1/rec com corpo: a revisao das travas desta pessoa.
// Qualquer fio.
void travarede_rev(long long rev);
// Pede agora a leitura do servidor e a reconciliacao pela conta (trava nova,
// tela de gerenciar aberta).
void travarede_atualizar_ja(void);
// A posicao do perfil ATIVO nesta serie pelo que o vistoep sabe (pagina do
// titulo aberta, trava criada). So avanca.
void travarede_posicao_local(const char *serie);

// O GESTO DE VISTO (visto.c chama, via visto_ao_marcar). Publico para o teste.
void travarede_visto(const char *imdb, const VistoPar *pares, int n, int visto);

// Desligar a escrita na conta dos outros membros no modo Juntos (plano B do
// STATUS-trava.md). 1 = escreve (padrao).
#define TRAVA_JUNTOS_NA_CONTA 1

#endif
