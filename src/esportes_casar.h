// ESPORTE CASADO COM O CANAL, a parte que fala com o guia.
//
// LEMBRETE DE JOGO: ate aqui o lembrete de um jogo (ficha do jogo, time ou jogo
// seguido) vencia como aviso curto, sem acao — a pessoa ainda tinha de achar o
// canal. Agora, nos minutos antes do apito, o app procura nos canais da pessoa
// quem transmite (grade + emissora da agenda, regra em esportes_canal.h) e o
// lembrete vira o cartao "começa agora no <canal>" com Assistir.
//
// Sem canal certo, tudo fica como era (aviso curto). Desliga em Esportes >
// Ajustes > "Jogo no canal certo" (por perfil, ligado de fabrica).
#ifndef NV_ESPORTES_CASAR_H
#define NV_ESPORTES_CASAR_H
#include "lembrete.h"
#include <stddef.h>
#include <time.h>

int  espcasar_ligado(void);
int  espcasar_definir(int ligado);   // 1 = gravou
// Fio de desenho, por quadro. Custa nada sem lembrete de jogo a menos de
// ESPCASAR_ANTES_S do inicio; com um, varre os canais em fatias de 2 ms.
#define ESPCASAR_ANTES_S 300
void espcasar_passo(time_t agora);
// O canal achado para este lembrete: 1 e preenche id/nome/base, ou 0.
int  espcasar_canal(const Lembrete *l, char *id, size_t nId, char *nome, size_t nNome,
                    char *base, size_t nBase);
#endif
