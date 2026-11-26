// VAI TOCAR NESTA TV (4.2): coleta e envio do registro anonimo de
// compatibilidade. A regra do que pode sair e vtreg.h; aqui so se junta o que
// o player ja sabe e se manda, fora do fio de quadro, para
// NV_REC_URL/v1/vaitocar. Nada sai com o ajuste "Compartilhar compatibilidade"
// desligado (o padrao, ate a pessoa aceitar na tela de consentimento do Farol), em perfil infantil ou
// sem NV_REC_URL no pacote. Nao le resposta: o veredito por fonte fica para
// depois de haver dados.
#ifndef VTREGENVIO_H
#define VTREGENVIO_H

#include "vtreg.h"

// O resultado da fonte que esta no player AGORA (chamar antes de parar o
// pipeline). `resultado` e VtRegResultado; fora da faixa, nada acontece.
void vtregenvio_resultado(int resultado, int trocou);
// A falha vista por tentarProximaFonteVOD, ja com os sinais dela.
void vtregenvio_falha(int motivo, int abriu, int erroDecoder, int trocou);
// O nucleo, sem player: normaliza, confere consentimento, descarta repetido e
// manda. 1 = saiu um envio.
int  vtregenvio_enviar(const VtRegEntrada *e);
// Modelo e sistema deste aparelho, crus (vtreg.h filtra). "" = desconhecido.
void vtregenvio_aparelho(const char **plataforma, const char **modelo, const char **firmware);

#ifdef VTREG_TESTE
void vtregenvio_teste_postar(int (*f)(const char *url, const char *corpo));
void vtregenvio_teste_zerar(void);
#endif

#endif
