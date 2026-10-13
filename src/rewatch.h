// SESSAO DE REASSISTIR (2.1): "estou revendo esta serie do comeco".
//
// Prototipo aprovado em docs/prototypes/rewatch (branch agente/21-trakt-ideias).
// Decisoes do dono, 13/10/2026:
//   - a sessao vive neste aparelho E na Conta Nuvio (viaja entre TVs); Trakt e
//     Simkl nao tem sessao, nada dela sai para eles;
//   - cada episodio concluido na sessao e um play novo: o historico vai de novo
//     ao Simkl e ao Trakt mesmo com o episodio ja visto (player.c);
//   - o selo "2x" so existe DENTRO da sessao. Fora dela o card segue como era.
//
// UMA SESSAO POR PERFIL E OBRA. Guarda quando comecou, de onde (pagina do
// titulo ou Continuar assistindo), os episodios JA REVISTOS com quantas vezes
// cada um foi revisto na sessao, e o ponto parcial. Nunca apaga historico:
// encerrar so fecha a sessao.
//
// A CONTA leva a sessao num texto (rewatch_texto) dentro do blob de ajustes do
// perfil. Mesclagem por obra, vence a gravacao mais nova; encerrar vira
// lapide (30 dias) para a sessao encerrada aqui nao ressuscitar vinda de la.
#ifndef NV_REWATCH_H
#define NV_REWATCH_H

#include <stddef.h>
#include "catalogo.h"

#define RW_MAX   24     // sessoes (todos os perfis do usuario)
#define RW_EPS   800    // episodios revistos por sessao
#define RW_LAPIDE_MS (30LL * 86400000LL)

enum { RW_ORIGEM_TITULO = 0, RW_ORIGEM_CONTINUAR = 1 };

// Sessao ativa DO PERFIL ATIVO para este titulo ("tt123" ou "tt123:2:8").
int  rewatch_ativa(const char *imdb);
// Comeca. 1 = criou; 0 = ja havia uma ativa (nada muda), ou id invalido.
int  rewatch_iniciar(const char *imdb, int origem);
// Fecha. O que ja foi enviado aos trackers fica; nada sai ao encerrar.
void rewatch_encerrar(const char *imdb);
// Um episodio concluido na sessao. Devolve quantas vezes ele foi visto contando
// a de antes (>= 2), ou 0 sem sessao. Limpa o ponto parcial.
int  rewatch_registrar(const char *imdb, int temporada, int episodio);
// 0 = fora da sessao ou ainda nao revisto nela; senao 1 + vezes revistas (>= 2).
// E o numero do selo "Nx".
int  rewatch_vezes(const char *imdb, int temporada, int episodio);
// Primeiro episodio ainda nao revisto (ou parcial), sem especiais/futuros.
int rewatch_alvo(const char *imdb, const CatEp *eps, int n, int *t, int *e, int *pct);
int  rewatch_feitos(const char *imdb);           // episodios distintos revistos
long long rewatch_inicio_ms(const char *imdb);   // 0 sem sessao
int  rewatch_origem(const char *imdb);

// Ponto de retomada da sessao (parou a meio). pct 1..99; fora disso apaga.
void rewatch_parcial(const char *imdb, int temporada, int episodio, int pct);
int  rewatch_parcial_de(const char *imdb, int *temporada, int *episodio, int *pct);

// Listagem das ativas do perfil (o Continuar assistindo desenha um cartao cada).
int  rewatch_n_ativas(void);
int  rewatch_ativa_n(int i, char *imdb, size_t tam);

// ---- Conta Nuvio ---------------------------------------------------------
// Texto das sessoes (inclui lapides), malloc; free pelo chamador.
char *rewatch_texto(void);
// Funde o texto vindo da conta. Devolve quantas sessoes mudaram aqui.
int  rewatch_mesclar(const char *texto);
// Escreve `rewatchSessions` no blob de ajustes da conta (troca a chave se ja
// existe, senao acrescenta). malloc; NULL se `base` nao e um objeto.
char *rewatch_blob_costurar(const char *base);
// Le `rewatchSessions` do blob e funde. Devolve quantas mudaram.
int  rewatch_blob_ler(const char *blob);
// 1 uma vez quando algo local ainda nao subiu para a conta (e limpa).
int  rewatch_sujo(void);
void rewatch_sujar(void);

void rewatch_esquecer(void);        // logout: larga a memoria, arquivo fica
void rewatch_relogio(long long (*f)(void));   // teste

#endif
