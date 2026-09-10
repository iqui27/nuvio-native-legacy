// MARCADORES DE ABERTURA, RESUMO E CREDITOS.
//
// A fonte e o TheIntroDB (api.theintrodb.org/v3/media), e a troca foi feita por
// uma razao concreta: o servico anterior (api.introdb.app) e indexado POR
// EPISODIO — sem `season` e `episode` ele nao responde. Filme, portanto, nao
// tinha marcador nenhum e caia na estimativa proporcional do posplay.c. O app
// web tem a mesma limitacao, pelo mesmo motivo (skipIntroRepository.js exige os
// dois numeros antes de chamar).
//
// O TheIntroDB aceita `imdb_id` sozinho e devolve marcador de FILME. Conferido
// ao vivo antes de escrever este arquivo:
//
//   GET /v3/media?imdb_id=tt0111161
//   {"tmdb_id":278,"type":"movie","credits":[{"start_ms":8300000,"end_ms":null}]}
//
// (Shawshank: creditos aos 8300 s de um filme de 8520 s.) Para serie, com
// season/episode, vem tambem o `intro`:
//
//   {"type":"tv","intro":[{"start_ms":272500,"end_ms":366000}],
//    "credits":[{"start_ms":3503000,"end_ms":null}]}
//
// SEM CHAVE e sem cadastro. Titulo desconhecido responde 404 com
// {"error":"media not found"}, que aqui vira "zero marcadores".
#ifndef NV_INTRO_H
#define NV_INTRO_H
// `fim` ZERO QUER DIZER "ATE O FIM DA MIDIA", que e como a API representa o
// `end_ms: null` dos creditos. Quem consome tem de tratar esse caso — ver
// intro_ativo.
typedef struct { double inicio,fim; int tipo; } IntroTrecho;
enum { INTRO_ABERTURA=1, INTRO_RESUMO=2, INTRO_CREDITOS=3 };
// `temporada` e `episodio` ZERO = filme: a consulta sai so com o imdb.
void intro_pedir(const char *imdb,int temporada,int episodio);
void intro_desligar(void);
int  intro_ativo(double posSeg,double *fim,int *tipo);
// Segundo em que os creditos comecam, ou 0 quando nao ha marcador. Serve ao
// posplay.c, que precisa do INSTANTE e nao de "estou dentro".
double intro_creditos_seg(void);
int  intro_extrair(const char *json,IntroTrecho *saida,int max);
#endif
