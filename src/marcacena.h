// MARCAR CENA: regra pura (sem SDL, rede ou disco). O player marca o instante,
// a marca fica no aparelho e pode viajar a um amigo. Primeira etapa da frente
// 4.7; ver docs/plans/marcar-cena.md.
#ifndef NV_MARCACENA_H
#define NV_MARCACENA_H
#include <stddef.h>

#define MARCACENA_MAX 200       // marcas guardadas por perfil; a mais antiga sai
#define MARCACENA_JUNTA_SEG 5   // duas marcas a menos disso sao a mesma cena
#define MARCACENA_TEXTO_MAX 60  // TEXTO_MAX do servidor de recomendacoes

typedef struct {
  char id[24];              // imdb do titulo
  char tipo;                // 'm' filme, 's' serie
  short temporada, episodio;
  int seg;                  // instante marcado
  int durSeg;               // duracao da fonte de quem marcou (0 = desconhecida)
  long long quando;         // epoch em segundos
} MarcaCena;

// "1:02:03" ou "2:03". Devolve dst.
char *marcacena_tempo(int seg, char *dst, size_t tam);

// O texto que viaja como recomendacao de texto livre (modelo -1). So usa
// [a-z0-9 ], que e o que o servidor publicado deixa passar, e por isso nao
// exige servidor novo: "cena 1h02m03s", "cena 0h02m03s t2 e5 d7260".
// Devolve 0 se a marca for invalida ou nao couber.
int marcacena_texto(const MarcaCena *m, char *dst, size_t tam);
// O inverso. Devolve 1 e preenche seg/temporada/episodio/durSeg quando o texto
// e uma cena; qualquer outro texto livre devolve 0 e segue sendo so texto.
int marcacena_ler_texto(const char *texto, MarcaCena *m);

// nuvio-legacy://play/movie/tt0111161?t=3723 (series: /play/series/id/T/E?t=).
int marcacena_link(const MarcaCena *m, char *dst, size_t tam);

// Onde abrir na fonte LOCAL. Cortes e duracoes diferem entre fontes: com as duas
// duracoes conhecidas e diferenca acima de 3 %, `incerto` sai 1 (o instante e
// aproximado). Devolve -1 quando a marca cai fora da fonte local.
int marcacena_ancorar(int seg, int durOrigem, int durLocal, int *incerto);

// Insere na lista (ordem de chegada). Marca a menos de MARCACENA_JUNTA_SEG de
// outra do mesmo titulo/episodio substitui a antiga; lista cheia perde a mais
// velha. Devolve o indice onde ficou, ou -1 se a marca for invalida.
int marcacena_inserir(MarcaCena *v, int *n, int max, const MarcaCena *m);
// Uma marca por linha: "id tipo T E seg dur quando\n". Devolve bytes escritos.
size_t marcacena_serializar(const MarcaCena *v, int n, char *dst, size_t tam);
int marcacena_carregar(const char *txt, MarcaCena *v, int max);
int marcacena_valida(const MarcaCena *m);

#endif
