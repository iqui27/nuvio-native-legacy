// FAROL — ACOES DE FONTE (issue #15, PLANO 2.4 / 5 item 15, ADENDO 2.1 C).
//
// O Farol conserta, nao so informa: as quatro acoes de fonte APLICAM a troca.
//   FAROL_A_PROXIMA_FONTE  a proxima da fila que ainda nao foi tentada
//   FAROL_A_FONTE_SEM_DV   a primeira sem Dolby Vision
//   FAROL_A_FONTE_SEM_DTS  a primeira sem DTS
//   FAROL_A_FONTE_MENOR    a primeira menor que a atual (tamanho, depois
//                          bitrate, depois altura)
//
// REGRAS DE ACEITE
//   - O filtro das acoes (sem DV / sem DTS) e TEMPORARIO: vive so em memoria,
//     vale ate a reproducao acabar ou a lista de fontes mudar, e NUNCA grava
//     fontefalha.txt nem ajuste algum. A unica excecao e a escolha EXPLICITA
//     "esconder DV nesta TV" (persistir=1 em SEM_DV), que mostra a acao feita
//     na ilha com Desfazer.
//   - Sem candidata elegivel a acao nao e oferecida (FarolFatos.tem* == 0) e
//     o executor devolve 0 sem tocar em nada.
//   - Fonte ja tentada nesta reproducao (ou a atual) nunca volta.
//   - Toda execucao registra farol_registrar_acao e farol_registrar_resultado.
//
// FIO: o executor roda no fio principal (a troca e consumida por app.c no
// quadro seguinte). As funcoes puras e o estado temporario tomam so travas
// folha proprias; nenhuma outra funcao e chamada com elas tomadas.
#ifndef NV_FAROLACAO_FONTE_H
#define NV_FAROLACAO_FONTE_H

#include <stdint.h>
#include "fontefiltro.h"

// Linhas que o snapshot de streams.c entrega: sempre em ORDEM DE PREFERENCIA.
#define FAROL_FONTE_MAX 256

typedef struct {
  int indice;           // posicao na lista de streams.c
  int altura;           // 0 = desconhecida
  int kbps;             // 0 = desconhecido
  uint64_t bytes;       // 0 = desconhecido
  unsigned char atual;  // 1 = a fonte que esta tocando
  unsigned char ok;     // 1 = o automatico poderia tocar (fora do auto = 0)
  unsigned char dv;
  unsigned char dts;
} FarolFonteLinha;

typedef struct { int outra, menor, semDv, semDts; } FarolFonteElegib;

// --- Parte pura (sem streams, sem rede, sem disco) -------------------------
// Preenche os 4 fatos (0/1) de FarolFatos. `geracao` e a da lista: a tentada
// de outra lista nao conta.
void farolacao_fonte_elegibilidade(const FarolFonteLinha *l, int n,
                                   unsigned geracao, FarolFonteElegib *out);
// Indice (campo `indice` da linha) da candidata da acao, ou -1.
int farolacao_fonte_escolher(int acao, const FarolFonteLinha *l, int n,
                             unsigned geracao);
// 1 se `acao` e uma das quatro desta unidade.
int farolacao_fonte_acao_valida(int acao);

// --- Fontes ja tentadas (por reproducao: zeram com a lista) ----------------
void farolacao_fonte_tentada_marcar(unsigned geracao, int indice);
int  farolacao_fonte_tentada(unsigned geracao, int indice);

// --- Filtro temporario so desta reproducao ---------------------------------
// 1 e copia em *out quando ha filtro temporario ativo. Barato (atomico) quando
// nao ha. Chamavel com verTrava tomada.
int  farolacao_fonte_filtro_temporario(FonteFiltroConfig *out);
// NULL desliga. 0 se a configuracao e invalida.
int  farolacao_fonte_filtro_temporario_definir(const FonteFiltroConfig *cfg);
// Reproducao/lista encerrada: esquece o filtro temporario e as tentadas.
void farolacao_fonte_encerrar(void);

// --- Executor ---------------------------------------------------------------
// Executa a acao. persistir=1 so vale em SEM_DV ("esconder DV nesta TV").
// Devolve 1 se a troca foi pedida; 0 se nao ha candidata (ou a lista mudou) —
// sem efeito algum alem do registro.
int farolacao_fonte_executar(int acao, int persistir);
// Desfaz o "esconder DV nesta TV" mais recente (o que a ilha oferece). 1 se
// havia o que desfazer.
int farolacao_fonte_desfazer_dv(void);

#endif
