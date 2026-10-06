// O COFRE DO DIARIO NA NUVEM (opcional, privado) — o cliente das rotas
// /v1/diario* do nuvio-recomendacoes (migracao 011).
//
// NADA SOBE SEM O "SIM" DA PESSOA. A pergunta e feita UMA vez (ilha), na
// primeira abertura de Perfil e Stats com o Diario nao vazio; depois, so em
// Ajustes. Ligar: POST /v1/diario/cofre {ligado:1} e o historico sobe em lotes
// de ate 100 (titulo incluso). Desligar: POST {ligado:0}, que APAGA no
// servidor, e a TV para de enviar. Restaurar (TV nova): na primeira vez que
// liga num perfil cujo Diario local esta vazio, GET /v1/diario?desde=0.
// Sem NUVIO_REC_URL no pacote ou sem login, tudo e no-op.
#ifndef NV_DIARIONUVEM_H
#define NV_DIARIONUVEM_H

// Laco principal: anda a fila (um fio por vez, com folga entre lotes).
void diarionuvem_passo(void);
// A tela de Perfil e Stats abriu: faz a pergunta de primeira vez, se couber.
void diarionuvem_perguntar_se_preciso(void);
// Botao do modal "diario-cofre" (ilhasinais.c). 1 Guardar, 2 Agora nao.
void diarionuvem_acao(int botao);
// Ajustes: 1 liga, 0 desliga (e apaga no servidor). Perfil ativo.
void diarionuvem_definir(int ligado);
int  diarionuvem_ligado(void);
// 1 quando o pacote tem o servico e ha sessao (a opcao faz sentido).
int  diarionuvem_disponivel(void);

// --- puro, para teste ---------------------------------------------------------
#include "diario.h"
// "tt123" ou "tt123_s2e5" (o formato da chave do servidor).
void diarionuvem_chave(const DiarioEntrada *e, char *dst, unsigned tam);
// Corpo de POST /v1/diario/lote com `n` entradas. Devolve o tamanho ou 0.
unsigned diarionuvem_json_lote(const DiarioEntrada *v, int n, char *dst, unsigned tam);
// Le a resposta de GET /v1/diario. Devolve quantas entradas, `mais` e `ate`.
int  diarionuvem_ler_itens(const char *json, DiarioEntrada *v, int max, int *mais, long long *ate);
#endif
