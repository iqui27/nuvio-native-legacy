// O GRAFICO DE TEMPORADAS da pagina de uma serie (dono, 06/10/2026: "um
// grafico bonitinho de barra para saber quanto de cada temporada voce ja viu,
// e que amigos estao na sua frente").
//
// Uma coluna por temporada, cheia na proporcao do que voce viu dos episodios
// que JA FORAM AO AR; o que ainda vai estrear aparece tracejado no alto da
// coluna. Temporada completa fica na cor de realce. Os amigos NAO entram nas
// barras (dono: "menos poluido"): um cartao separado ao lado diz "Ana e mais
// 2 estão na sua frente" e lista ate tres (rosto, nome, frente/atras, T3E2).
//
// NADA E PEDIDO DAQUI. Tudo ja esta na memoria quando a pagina abre:
//   - os episodios e as temporadas: a lista do meta (catalogo.h);
//   - o que voce viu: o mapa do vistoep (Trakt/conta Nuvio), tri-estado;
//   - o que ainda nao foi ao ar: a agenda do TMDB (extras_agenda_*), a mesma
//     regra de epNaoExibido em detail.c;
//   - os amigos: amigostitulo.h, que so tem o que cada um publicou dentro do
//     alcance dele.
// A soma e refeita so quando uma dessas fontes muda de revisao.
//
// SEM SPOILER: nenhum nome de episodio aparece aqui, so a posicao (T3E2).
//
// detail.c e dono do quando e do onde (fileira propria entre os episodios e as
// Notas); este modulo so monta os numeros e desenha o bloco.
#ifndef NV_TEMPORADAS_GRAFICO_H
#define NV_TEMPORADAS_GRAFICO_H
#include "amigostitulo.h"
#include <SDL2/SDL.h>

#define TG_TEMP_MAX 64

// Um episodio da lista do meta. `visto`: -1 nao se sabe, 0 nao, 1 sim.
typedef struct { short temporada, episodio; signed char visto; } TgEp;

typedef struct {
  int numero;      // numero da temporada (0 = especiais)
  int total;       // episodios na lista
  int exibidos;    // destes, os que ja foram ao ar
  int vistos;      // vistos ENTRE os exibidos
  int completa;    // vistos == exibidos > 0
  int ultVisto;    // o maior episodio visto nesta temporada (0 = nenhum)
} TgTemp;

typedef struct {
  char id[96], nome[64], avatar[256];
  int temporada, episodio;
  int reacao, nota, agora;
  int frente;      // 1 = adiante de voce
  int col;         // indice em TgDados.t (-1 = temporada fora do grafico)
} TgAmigo;

typedef struct {
  char imdb[24];
  int n;                     // temporadas no grafico
  TgTemp t[TG_TEMP_MAX];
  int sabe;                  // ha mapa do vistoep desta serie
  int vistos, exibidos, total, completas;
  int meuT, meuE;            // o episodio visto mais adiante (0 = nenhum)
  int nAmg, nFrente, nAtras; // amigos com posicao conhecida
  int totalAmg;              // amigos no titulo (pode passar de AMT_MAX)
  TgAmigo amg[AMT_MAX];      // os da frente primeiro, cada grupo do mais adiantado ao menos
} TgDados;

// PURO (sem GL, sem catalogo): o que os testes usam. (agT, agE) e o proximo
// episodio a ir ao ar (0 = nao se sabe: tudo conta como exibido). Especiais
// (temporada 0) so entram com `especiais`. `amg` pode ser NULL. Devolve d->n.
int  tgraf_montar(TgDados *d, const TgEp *eps, int n, int sabe, int agT, int agE,
                  int especiais, const AmigosTitulo *amg);

// O bloco existe? Precisa de temporada e de algo a dizer: voce viu ao menos
// um episodio, ou ha amigo com posicao conhecida.
int  tgraf_existe(const TgDados *d);

// Os dados da serie `idxCatalogo` aberta na pagina, refeitos so quando o
// catalogo, o vistoep, os amigos ou a agenda mudam. Nunca NULL.
const TgDados *tgraf_dados(int idxCatalogo);

// Indice da coluna da temporada `numero` (-1 = nao esta no grafico).
int  tgraf_coluna(const TgDados *d, int numero);

// Frase do cartao dos amigos: "Ana e mais 2 estão na sua frente". "" sem amigos.
void tgraf_frase_amigos(const TgDados *d, char *dst, size_t tam);

float tgraf_altura(void);
float tgraf_altura_dados(const TgDados *d);
// Desenha o bloco em (x, y), largura `w`. `foco` = coluna focada (-1 = a
// fileira nao tem o foco); `selNumero` = temporada escolhida na pagina.
void tgraf_desenhar(const TgDados *d, float x, float y, float w, int foco,
                    int selNumero, float a, Uint32 agora);
// Nova pagina: a animacao de crescer volta a tocar na proxima vez.
void tgraf_reiniciar(void);
#endif
