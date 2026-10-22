// TRAVA DE SERIE na mesma TV/conta (plano social-pessoal, secao 5).
//
// Uma trava junta 2..8 perfis DA MESMA CONTA numa serie:
//   JUNTOS    um episodio que conta como visto e marcado para todos os membros
//             (travajuntos.c faz isso); nao ha espera.
//   SEPARADOS cada um assiste no proprio perfil. O LIMITE de um membro e o
//             episodio mais adiantado que TODOS OS OUTROS ja viram, mais um:
//             da para ver o proximo, nao o seguinte. Membro que nao comecou
//             segura todos no T1E1. Especiais (temporada 0) ficam fora.
// A posicao de cada membro vem do Diario dele (diario_ultimo_episodio): nao ha
// copia, entao o que o player, a conta ou o Trakt gravarem ja conta.
//
// ARQUIVO: travas.txt, marcado com a conta dona (hash), como o Diario. Outra
// conta nao le. Teto de TRAVA_MAX travas. Seguro de qualquer fio (mutex).
#ifndef NV_TRAVA_H
#define NV_TRAVA_H
#include "diario.h"
#include "perfis.h"

#define TRAVA_MAX      30
#define TRAVA_MEMBROS  CONTA_PERFIL_MAX
#define TRAVA_JUNTOS      0
#define TRAVA_SEPARADOS   1

typedef struct {
  int  id;                          // estavel, > 0
  char serie[DIARIO_ID_MAX];        // id do titulo, sem ":T:E"
  char titulo[64];                  // nome para as telas (pode estar vazio)
  int  modo;                        // TRAVA_JUNTOS | TRAVA_SEPARADOS
  int  pin;                         // "Assistir mesmo assim" pede PIN
  int  pausada;                     // congela as esperas
  int  n;                           // membros
  unsigned char perfil[TRAVA_MEMBROS];  // indices de perfil (1..8)
} Trava;

typedef struct { int perfil, t, e; } TravaPos;   // 0,0 = nao comecou

// Estado de um membro. `minT/minE`: o menos adiantado dos OUTROS; `quem`: bits
// (bit i = perfil i+1) de quem esta nesse ponto; `esperando`: o membro ja viu
// alem do que os outros alcancaram.
typedef struct {
  int minT, minE;
  unsigned quem;
  int esperando;
  int atras;                        // episodios (mesma temporada) que o menos adiantado esta atras; -1 = outra temporada
} TravaEstado;

// --- ciclo de vida ---------------------------------------------------------
void trava_dono(const char *conta);   // como diario_dono
void trava_iniciar(void);
void trava_esquecer(void);            // logout: solta a memoria (arquivo fica)
unsigned trava_geracao(void);

// --- gestao (devolvem 1 se fez; criar devolve o id ou -1) -------------------
int  trava_criar(const char *serie, const char *titulo, int modo, int pin,
                 const int *perfis, int n);
int  trava_apagar(int id);
int  trava_definir_modo(int id, int modo);
int  trava_definir_pin(int id, int pin);
int  trava_definir_pausada(int id, int pausada);
int  trava_adicionar_membro(int id, int perfil);
// Com menos de 2 membros a trava se desfaz (devolve 2 nesse caso).
int  trava_remover_membro(int id, int perfil);

// --- leitura ---------------------------------------------------------------
int  trava_n(void);
int  trava_item(int i, Trava *saida);                 // i em [0, trava_n)
int  trava_obter(int id, Trava *saida);
// Travas desta serie em que `perfil` esta (0 = qualquer perfil). Devolve quantas.
int  trava_da_serie(const char *serie, int perfil, int *ids, int max);
int  trava_tem_membro(const Trava *t, int perfil);

// --- regra -------------------------------------------------------------------
int  trava_posicoes(const Trava *t, TravaPos *v);     // n posicoes, na ordem dos membros
int  trava_estado(const Trava *t, int perfil, TravaEstado *s);
// `perfil` pode ver (te)? `epsNaTemp` = quantos episodios tem a temporada do
// menos adiantado (0 = nao se sabe: so vale E+1 na mesma temporada).
// Pausada, Juntos, especiais e quem esta sozinho: sempre 1.
int  trava_liberado(const Trava *t, int perfil, int te, int ee, int epsNaTemp);
// Quem segura `perfil` (bits) em (te,ee); 0 se esta liberado.
unsigned trava_quem_segura(const Trava *t, int perfil, int te, int ee, int epsNaTemp);

// --- Juntos ------------------------------------------------------------------
// Perfis a marcar quando `perfil` termina `serie`: membros (menos `perfil`) de
// toda trava JUNTOS, nao pausada, que o inclui. Sem repeticao. Devolve quantos.
int  trava_juntos_alvos(const char *serie, int perfil, int *alvos, int max);

// Teste: relogio e mutex nao precisam de gancho; so ha o arquivo.
#endif
