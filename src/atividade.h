// ATIVIDADE DE REPRODUCAO PARA O SERVICO SOCIAL — o lado do app de
// POST /v1/atividade (servidor/recomendacoes/, dono: outro modulo).
//
// O QUE SAI: um evento por momento da reproducao, nunca por quadro.
//   inicio    a imagem comecou a andar de verdade (video pronto e tocando)
//   progresso a cada 5 min TOCANDO (pausado nao conta), e na parada entre
//             20% e o fim
//   fim       passou de 90% ou entrou nos creditos (player_regra_concluiu)
//   abandono  parou antes de 20%. "Nao voltou em 7 dias" e decisao do
//             SERVIDOR: daqui so sai a parada com o pct.
//   salvo     o "+" salvou em qualquer destino (Nuvio, Trakt, Simkl)
//   reacao    Gostei / Mais ou menos / Nao gostei (reacao.c)
// `seg` e o que foi ASSISTIDO (tocando) desde o evento anterior do mesmo
// titulo — somar os `seg` da o tempo de tela, sem contar pausa nem avanco.
//
// SEM URL COMPILADA NADA ACONTECE (recomenda_ativo, a mesma regra do resto do
// social). E COM URL, NADA SAI ENQUANTO atividade_permitido() FOR 0 — o padrao.
// Quem liga e o nivel de privacidade da pessoa (Ajustes/Social), por
// atividade_definir_permitido. Desligar APAGA a fila: nada do que a pessoa
// acabou de negar pode sair depois do gesto (a regra de recomenda_atividade_nivel).
//
// O QUE FICA LOCAL MESMO DESLIGADO: so o necessario para a reacao funcionar —
// de que recomendacao o titulo veio (atividade_origem). As reacoes em si moram
// em reacao.c, por perfil.
//
// FILA: em memoria (ATIV_FILA_MAX) e espelhada em atividade-fila-p<N>.txt, uma
// linha JSON por evento, para um envio que caiu com a TV desligada sair no
// arranque seguinte. Um fio por lote, que nasce quando ha o que mandar e morre
// quando a fila esvazia (ou na primeira falha de rede — tenta de novo no
// proximo evento ou no proximo arranque).
#ifndef NV_ATIVIDADE_H
#define NV_ATIVIDADE_H
#include "catalogo.h"
#include <stddef.h>

#define ATIV_FILA_MAX     32
#define ATIV_PROGRESSO_S  300.0    // 5 min tocando entre dois "progresso"
#define ATIV_ABANDONO_PCT 20       // parada abaixo disto e "abandono"
#define ATIV_FIM_PCT      90       // a partir disto e "fim", com ou sem creditos

typedef struct {
  char ev[12];          // "inicio" | "progresso" | "fim" | "abandono" | "reacao" | "salvo" | "junto"
  char imdb[24];        // sempre "tt..." puro, sem ":T:E"
  char midia[8];        // "movie" | "series"
  char titulo[160];
  char poster[512];
  int  temporada, episodio;   // 0 em filme
  int  pct;             // 0..100
  int  seg;             // segundos assistidos neste trecho
  int  reacao;          // 1 | 0 | -1, so em ev=reacao
  long long rec;        // id da recomendacao de origem, 0 = nenhuma
} AtivEvento;

// --- inicializacao e privacidade --------------------------------------------
// Le a fila do disco (perfil ativo). Chamar uma vez no arranque, depois de
// dados_iniciar; chamar de novo na troca de perfil e seguro.
void atividade_iniciar(void);
// 0 = nada sai (padrao). >0 = os eventos sao enfileirados e enviados.
void atividade_definir_permitido(int nivel);
int  atividade_permitido(void);
// 1 quando o pacote tem o servico E a pessoa permitiu. Quem desenha "Fulano vai
// ver sua resposta" pergunta isto: sem envio, a frase seria mentira.
int  atividade_envia(void);

// --- ganchos do player ------------------------------------------------------
// Por quadro, SO com titulo de verdade (video pronto, duracao >= 120 s, nao
// canal). `concluiu` e player_regra_concluiu na posicao corrente. Troca de
// titulo/episodio no meio fecha o trecho anterior como uma parada.
void atividade_player_passo(const CatItem *ci, int temporada, int episodio,
                            double posSeg, double durSeg, int tocando,
                            int concluiu, float dt);
// Na saida do player (fecharSessao). Fecha o trecho: fim, abandono ou
// progresso, conforme o pct. Sem `inicio` antes, nao manda nada.
void atividade_player_saiu(double posSeg, double durSeg, int concluiu);

// --- outros ganchos ---------------------------------------------------------
// O "+" salvou (salvar=1). Remover nao vira evento.
void atividade_salvo(const CatItem *ci, int salvar);
// Reacao respondida (reacao.c). `rec` = recomendacao de origem ou 0.
void atividade_reacao(const char *imdb, const char *midia, const char *titulo,
                      const char *poster, int reacao, long long rec);

// Sessao do Watch Together acabou (juntos.c). `seg` = duracao da sessao.
void atividade_junto(const char *imdb, const char *midia, const char *titulo, const char *poster,
                     int temporada, int episodio, int seg);

// --- de onde o titulo veio --------------------------------------------------
// O painel Social abriu o titulo a partir da recomendacao `rec` de `nome`.
// Vale para o proximo player deste imdb; um id explicito ganha da busca.
void atividade_marcar_origem(long long rec, const char *imdb, const char *nome);
// A recomendacao de origem de `imdb`: a marcada pelo painel, ou a mais nova
// recebida deste titulo (recomenda_item). 0 = nao veio de ninguem. `nome`
// (opcional) recebe quem mandou.
long long atividade_origem(const char *imdb, char *nome, size_t tamNome);

// --- puro, para o teste -----------------------------------------------------
// Monta o corpo JSON de `e`. Devolve o tamanho escrito, ou 0 se nao coube.
size_t atividade_json(const AtivEvento *e, char *dst, size_t tam);
// Quantos eventos esperam envio, e a linha `i` da fila (JSON). Para o teste.
int  atividade_fila_n(void);
int  atividade_fila_linha(int i, char *dst, size_t tam);
// "tt123:2:5" -> "tt123". `dst` pode ser o proprio `src`.
void atividade_id_puro(char *dst, size_t tam, const char *src);
#endif
