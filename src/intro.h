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
// Igual a intro_pedir, mas diz a duracao (s) dos episodios E-1 e E+1 quando o
// catalogo sabe (0 = desconhecida): sem marcador deste episodio, o marcador de
// um vizinho vira "quanto falta para o fim". Ver credfonte.h.
void intro_pedir_vizinhos(const char *imdb,int temporada,int episodio,double durAnt,double durProx);
void intro_desligar(void);
int  intro_ativo(double posSeg,double *fim,int *tipo);
// Segundo em que os creditos comecam, ou 0 quando nao ha marcador. Serve ao
// posplay.c, que precisa do INSTANTE e nao de "estou dentro".
double intro_creditos_seg(void);
// Duracao real da midia (0 = desconhecida) e se e filme: base da guarda de janela.
void intro_definir_duracao(double dur,int filme);
// Janela aceitavel? Creditos <= 900 s, abertura/resumo <= 180 s, creditos de
// filme so a partir de 50% da duracao. `motivo` (opcional) diz o porque.
int  intro_janela_ok(int tipo,double ini,double fim,double dur,int filme,const char **motivo);
// Botao de pular com tempo: some em INTRO_BOTAO_SEG sem foco, uma aparicao
// automatica por trecho, volta so com os controles (osd) de pe.
#define INTRO_BOTAO_SEG 10.0
int  intro_botao(double posSeg,double agora,int osd,int focado,double *fim,int *tipo);
int  intro_botao_visivel(double *fim,int *tipo);
int  intro_extrair(const char *json,IntroTrecho *saida,int max);
// Copia ate `max` trechos conhecidos; devolve quantos.
int  intro_trechos(IntroTrecho *saida,int max);
// Copia apenas creditos com inicio/fim numericos explicitos e verificados na
// resposta atual. Sem fim, vizinhos, capitulos e estimativas nao entram.
int  intro_creditos_limitados(IntroTrecho *saida,int max);
#ifdef NV_SHOT_HOOKS
void intro_shot_definir(const IntroTrecho *v,int n);   // capturas: trechos fixos
#endif
#endif
