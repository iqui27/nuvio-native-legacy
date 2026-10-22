// DE ONDE VEM O INICIO DOS CREDITOS quando o TheIntroDB nao tem o episodio.
//
// Medido nos logs das TVs do dono: 34 pedidos ao api.theintrodb.org voltaram
// 404 {"error":"media not found for provided season/episode"} — lacuna de
// DADOS (a API esta no ar: Breaking Bad T1E1 devolve intro e credits), nao
// queda. Sem marcador o cartao do proximo episodio caia na estimativa dos 2
// minutos finais. Ordem de preferencia, a primeira que existir:
//
//   1. capitulo do Matroska chamado creditos/end credits (descreve ESTA copia);
//   2. marcador do TheIntroDB para este episodio;
//   3. marcador de um episodio VIZINHO da mesma temporada, convertido em
//      "quanto falta para o fim" e aplicado a duracao real deste;
//   4. o que o dono ensinou nesta serie (apertou Comecar agora: dur - pos);
//   5. nada: o player usa a estimativa de sempre.
#ifndef NV_CREDFONTE_H
#define NV_CREDFONTE_H

enum { CRED_FONTE_NENHUMA = 0, CRED_FONTE_CAPITULO, CRED_FONTE_INTRODB,
       CRED_FONTE_VIZINHO, CRED_FONTE_APRENDIDO, CRED_FONTE_ANISKIP };

typedef struct {
  double dur;                   // duracao REAL deste episodio, em s
  double capitulo;              // inicio do capitulo de creditos (0 = nao ha)
  double introdb;               // inicio do marcador do TheIntroDB deste episodio (0 = nao ha)
  double aniskip;               // idem, vindo do AniSkip (mesma prioridade do introdb)
  double vizInicio, vizDur;     // marcador do vizinho e a duracao dele (0 = nao ha / desconhecida)
  double aprendidoResto;        // quanto faltava quando o dono apertou Proximo (0 = nada)
} CredEntrada;

// A regra sozinha, sem estado e sem rede. Devolve o inicio em s (0 = nenhum) e
// a fonte. Marcadores derivados (3 e 4) so valem se cairem na metade final e a
// pelo menos 15 s do fim; os dois diretos (1 e 2) passam como vieram, porque
// quem decide se aceita e a janela do player.
double cred_escolher(const CredEntrada *e, int *fonte);
const char *cred_fonte_nome(int fonte);

// Cache dos marcadores de vizinhos, por serie e TEMPORADA: guarda o par
// (inicio, duracao do vizinho). Um episodio vizinho serve ao resto da
// temporada sem outro pedido.
void cred_viz_guardar(const char *imdb, int temporada, double inicio, double durViz);
int  cred_viz_ler(const char *imdb, int temporada, double *inicio, double *durViz);

// Cache de "o servidor nao conhece este episodio", valido pela sessao (30 min,
// para uma queda de rede passageira nao calar o episodio para sempre).
int  cred_404_visto(const char *imdb, int temporada, int episodio, long agoraS);
void cred_404_marcar(const char *imdb, int temporada, int episodio, long agoraS);
#endif
