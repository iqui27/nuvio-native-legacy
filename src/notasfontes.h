// FONTES DE NOTA: a tabela do que o app sabe mostrar, a conversao de cada
// escala nativa para 0..100, as cores e o encaixe da linha do titulo.
//
// PURO, de proposito: nada aqui abre janela, textura ou rede. A tela de titulo
// (notasui.c) so desenha o que estas funcoes decidem, e por isso a regra pode
// ser testada sem GL (tests/notasfontes.c).
//
// O VALOR de entrada e o de extras_nota(): o numero NATIVO da fonte vezes 10
// (imdb 7.8 -> 78, tomatoes 87% -> 870, letterboxd 3.9 de 5 -> 39). A escala
// nativa varia por fonte e e a que o site dono da nota usa — e por isso que a
// linha do titulo mostra "87%" ao lado de "7.8" e nao tudo em 0..10.
#ifndef NV_NOTASFONTES_H
#define NV_NOTASFONTES_H
#include <stddef.h>
#include "extras.h"

// Quem avalia: o comparativo "critica x publico" so faz sentido com os dois
// lados, e a nota agregada do MDBList nao e nenhum deles.
typedef enum { NF_PUBLICO, NF_CRITICA, NF_AGREGADA } NfGrupo;

// Quantas fontes cabem na linha do titulo (todas as do enum, menos as que o
// titulo nunca mostra — hoje nenhuma).
#define NF_MAX EX_NFONTES

// Posicao FIXA da fonte na linha (0 = primeira da esquerda). A ordem e a de
// leitura de quem escolhe: IMDb e a critica primeiro, o publico depois, os
// nichos por ultimo. Trakt antes do TMDB porque assim a linha de hoje
// (IMDb, Rotten Tomatoes, Trakt) continua na mesma ordem quando as duas outras
// se ligam.
int  nf_posicao(int fonte);
// Fonte que ocupa a posicao `pos` da linha (0..EX_NFONTES-1), ou -1.
int  nf_na_posicao(int pos);

// Igual a nf_na_posicao, mas com `letterFirst` o Letterboxd abre a linha e os
// demais seguem na ordem de sempre.
int  nf_na_posicao_ord(int pos, int letterFirst);
// Letterboxd survives late IMDb/RT loads regardless of display order.
// The legacy order argument is retained for callers testing both presets.
int  nf_prioridade_ord(int fonte, int letterFirst);

// PRIORIDADE quando a linha nao cabe: 0 sai por ultimo. Nao e a ordem da linha
// — Metacritic sobrevive ao Popcornmeter num aperto mesmo aparecendo depois.
int  nf_prioridade(int fonte);

// A fonte entra na linha de FABRICA? So o que a linha ja mostrava antes de
// existir a escolha: IMDb, Rotten Tomatoes (critica) e Trakt.
int  nf_padrao_titulo(int fonte);

// 1 quando a nota so existe com a chave do MDBList (tudo menos IMDb, que vem
// do catalogo, e Trakt, que o app consulta direto).
int  nf_precisa_mdblist(int fonte);

NfGrupo nf_grupo(int fonte);

// Nome de exibicao (proprio de cada marca, nao traduzido) e o rotulo do grupo.
const char *nf_nome(int fonte);

// Maior valor da escala nativa (10, 100, 5, 4).
int  nf_escala_max(int fonte);

// Nota 0..100 comparavel entre fontes. `cru` e o de extras_nota() (nativo x10);
// 0 = sem nota. Um valor ACIMA da escala nativa e lido como porcentagem — a
// api ja devolveu o mesmo provedor nas duas formas, e cravar a escala faria o
// letterboxd de 78 virar 1560.
int  nf_norm100(int fonte, int cru);

// Texto da nota na escala NATIVA. `longo` acrescenta o "/5" de quem nao e
// obvio (letterboxd, ebert) — o heatmap usa, a linha do titulo nao. `virgula`
// troca o separador decimal (portugues). Metacritic e a nota do MDBList saem
// como inteiro puro, sem "%" nem "/100": e assim que o site escreve.
int nf_cru(int fonte, double v);
void nf_texto(int fonte, int cru, int virgula, int longo, char *dst, size_t cap);

// --- cores (0..1) ------------------------------------------------------------
// Rampa viridis, perceptualmente uniforme e segura para daltonismo. t 0..1.
void nf_viridis(float t, float *r, float *g, float *b);
// Cor da celula para uma nota 0..100: viridis sobre a faixa util
// [piso..100] — abaixo do piso trava no escuro, porque a diferenca entre 12 e
// 30 nao informa ninguem. Devolve em `tinta` 0 (texto escuro) ou 1 (claro) que
// legibilizam o numero sobre a celula.
void nf_cor_nota(int norm100, int piso, float *r, float *g, float *b, float *tinta);
// Cor do quadrado do Metacritic (faixas do proprio site para filmes: verde
// 61+, amarelo 40-60, vermelho abaixo).
void nf_cor_metacritic(int score, float *r, float *g, float *b);
// Celula da grade de episodios: nota em DECIMOS (72 = 7.2), 0 = sem nota
// (devolve 0 e nao pinta). Faixa 5.0..10.0: episodio de serie quase nunca fica
// abaixo de 5, e uma faixa mais larga gastaria metade da rampa onde nada mora.
int  nf_cor_episodio(int decimos, float *r, float *g, float *b);

// --- resumo -------------------------------------------------------------------
typedef struct {
  int n;                // fontes que entraram na conta (sem a agregada)
  int media;            // 0..100
  int min, max;         // 0..100
  int fonteMin, fonteMax;
  int criticos;         // media 0..100 das fontes de critica; -1 sem nenhuma
  int publico;          // idem
  int diff;             // publico - criticos, so com os dois lados (senao 0)
  int temDiff;
} NfResumo;
// `fontes[i]` e `norm[i]` (0..100) descrevem as notas presentes.
void nf_resumo(const int *fontes, const int *norm, int n, NfResumo *r);

// --- encaixe da linha do titulo -----------------------------------------------
// `larg[i]` e a largura de cada item (ja com o vao que o precede), `prio[i]` a
// prioridade dele (nf_prioridade). Tira, um a um, o de MENOR prioridade (maior
// numero; no empate o mais a direita) ate somar <= `disp`. `manter[i]` sai 1 ou
// 0. Devolve quantos ficaram. Nunca estoura `disp`: com nada cabendo devolve 0.
int  nf_encaixar(const float *larg, const int *prio, int n, float disp,
                 unsigned char *manter);

// --- grade de episodios --------------------------------------------------------
// Tamanho da celula para `nt` temporadas x `maxEps` colunas dentro de
// dispW x dispH. A celula e sempre um retangulo mais largo que alto, com teto
// e piso; `linhaH` e o passo vertical. Devolve 0 se nem o piso cabe.
int  nf_grade_celula(int nt, int maxEps, float dispW, float dispH,
                     float *cw, float *ch);

#endif
