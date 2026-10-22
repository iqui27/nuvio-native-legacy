// O QUE A PESSOA DESMARCOU NA TV, e que nenhuma fonte pode re-marcar.
//
// O relato (Silo, tt14688458, TCL, 08/10/2026): "desmarco os episodios e eles
// voltam marcados". O mapa de vistos (vistoep.c) e reescrito por leitores de
// rede, e o gesto de desmarcar so vivia em dois lugares frageis:
//   - no proprio mapa, que e memoria: a leitura seguinte do Trakt
//     (/shows/<id>/progress/watched) escreve `completed` por cima;
//   - no jornal da conta (contapend.c), que so existe com conta Nuvio logada,
//     so e consultado pelo pull da CONTA, e e podado assim que ela confirma o
//     delete (contapend_podar) — dali em diante nada lembrava do gesto.
// Bastava UMA fonte continuar dizendo "visto" (remove recusado ou perdido sem
// rede, corrida entre o POST do gesto e a leitura, um servico espelhando o
// outro) para a marca voltar.
//
// A REGRA: DESMARCAR GANHA. Cada episodio desmarcado vira uma entrada com o
// instante do gesto, POR PERFIL, em disco. Toda fonte que quer escrever "visto"
// passa por vistoep_fonte, que pergunta aqui:
//   - a fonte nao diz quando foi visto, ou diz um instante que nao e mais novo
//     que o gesto: BARRADO, o episodio fica desmarcado;
//   - a fonte diz um instante MAIS NOVO que o gesto (visto de novo em outro
//     aparelho depois de desmarcar aqui): o remoto ganha e a entrada cai. E a
//     mesma regra "ultima mudanca vence" de oculto() em contapend.c.
//
// QUANTO DURA. Nao ha prazo: "nao vi" vale ate a pessoa dizer outra coisa. A
// entrada sai quando (1) ela marca ou assiste o episodio de novo nesta TV,
// (2) uma fonte traz um visto mais novo, ou (3) o teto estoura e a mais velha
// da lugar. Prazo em dias foi descartado de proposito: o caso que originou
// isto e uma fonte que NUNCA aplica o remove, e com prazo a marca voltaria
// sozinha meses depois. O teto (VISTONAO_MAX, ~40 bytes por entrada em disco)
// e o que impede o arquivo de crescer sem limite.
//
// O ARQUIVO e por usuario da conta (vistonao-<sub>.txt; "anon" sem conta) com
// o perfil em cada linha, como o jornal: so ids, episodio e horario, nenhum
// token. Existe SEM conta Nuvio — quem so tem Trakt tambem desmarca.
//
// vistoep.c NAO depende deste modulo: app.c liga os dois por
// vistoep_lapides(vistonao_barra, vistonao_gesto), e os testes que linkam so
// vistoep.c continuam como eram.
#ifndef NV_VISTONAO_H
#define NV_VISTONAO_H

#include "vistoep.h"

#define VISTONAO_MAX 2000
// FOLGA DE RELOGIO. O instante do gesto e do relogio da TV; o do visto e do
// servidor da fonte (o Trakt carimba "agora" DELE num /sync/history sem
// watched_at). Marcar e desmarcar em seguida com a TV 30 s atrasada dava um
// "visto mais novo que o gesto" de mentira. Um visto so ganha se for mais novo
// que o gesto por mais que isto.
#define VISTONAO_FOLGA_MS 120000LL

// O GESTO (fio principal). `visto` 0: a pessoa desmarcou — nasce/renova a
// entrada de cada par, com o instante de agora. `visto` 1: marcou ou assistiu
// de novo — a entrada cai; com `pares` NULL caem TODAS as do titulo ("marcar a
// serie inteira"). Perfil = perfis_ativo() no momento.
void vistonao_gesto(const char *imdb, const VistoPar *pares, int n, int visto);

// A PERGUNTA DAS FONTES (qualquer fio). `remotoMs` e quando a fonte diz que o
// episodio foi visto (0 = nao diz).
//   1  barrado: ha desmarcacao e o remoto nao e mais novo que ela
//   0  nao ha desmarcacao para este episodio neste perfil
//  -1  havia, o remoto e mais novo: ele ganha e a entrada caiu
int  vistonao_barra(const char *imdb, int temporada, int episodio, long long remotoMs);

// O PRIMEIRO EPISODIO DESMARCADO ate (temporada, episodio) inclusive — o "a
// seguir" do Continuar assistindo recua para ele quando o remoto esta adiante
// do que a pessoa deixou como visto (Silo: remoto T2E10, desmarcados T2E7..E10
// -> T2E7). So conta entrada que ainda vale contra `remotoMs` (mesma regra e
// mesma folga de vistonao_barra); nao mexe em nada. Temporada 0 (extras) nao
// puxa. 1 e *pt/*pe preenchidos, ou 0.
int  vistonao_primeira(const char *imdb, int temporada, int episodio, long long remotoMs,
                       int *pt, int *pe);

int  vistonao_n(void);          // entradas em memoria (todos os perfis do usuario)
void vistonao_esquecer(void);   // larga a memoria; o arquivo fica
// Teste: relogio em ms.
void vistonao_relogio(long long (*f)(void));

#endif
