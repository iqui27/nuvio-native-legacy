// REFERENCIA INDEPENDENTE PARA O AUTOSYNC (F05, 1.8).
//
// Le, por HTTP Range e em fio proprio, UMA faixa de legenda de TEXTO embutida
// num Matroska (S_TEXT/UTF8, S_TEXT/ASS, S_TEXT/SSA) ou num MP4/MOV (tx3g,
// wvtt, QuickTime text) e devolve um LegendaDocumento imutavel. Existe para a
// sincronizacao automatica ter contra o que comparar a legenda externa: a
// faixa embutida foi multiplexada com o video e, por construcao, ja acompanha
// a imagem.
//
// NAO E O MKVASS. Nao chama mkvass_iniciar, legenda_carregar nem
// assrender_carregar; nao troca a faixa da pessoa; nao entrega nada ao
// overlay. Estado, fio, orcamento e conexoes sao deste modulo. Uma fonte local
// opcional pode fornecer uma janela indexada completa sem abrir conexao.
//
// SO COM INDICE. MKV: Cues pelo SeekHead. SRT com CueDuration em todo ponto
// (ffmpeg e mkvmerge gravam) sai do PROPRIO indice, so tempos (2-4 Ranges);
// ASS (ou sem CueDuration) busca cada bloco pelo CueClusterPosition +
// CueRelativePosition, porque o texto separa letreiro de dialogo. Sem Cues da
// faixa, sem CueRelativePosition, sem BlockDuration, com lacing, ou com
// qualquer bloco que nao veio: o documento NAO e marcado completo e nao e
// entregue. Nao varre Clusters (custaria o arquivo inteiro).
// MP4: so o `moov` (as caixas de topo sao puladas pelo tamanho); tempos de
// stts/ctts/elst e amostra vazia por stsz. MP4 fragmentado: SEM_INDICE.
// Documento so de tempos: o texto de cada fala e um rotulo ("fala 0001").
//
// COMPLETUDE, sem exagero: "completo" quer dizer "todos os blocos que o
// indice aponta vieram e foram lidos". ffmpeg e mkvmerge escrevem um CuePoint
// por bloco de legenda (conferido nos dois em tests/legref.sh); um muxer que
// indexe so parte dos blocos passaria por completo. Os portoes do AutoSync
// (cobertura, segmentos) recusam esse par em vez de inventar correcao.
#ifndef NV_LEGREF_H
#define NV_LEGREF_H
#include "legenda.h"
#include <stdint.h>

typedef enum {
  LEGREF_OCIOSO = 0, LEGREF_LENDO, LEGREF_PRONTO, LEGREF_INDISPONIVEL, LEGREF_CANCELADO
} LegRefFase;
typedef enum {
  LEGREF_OK = 0, LEGREF_PLATAFORMA, LEGREF_SEM_RANGE, LEGREF_NAO_MKV,
  LEGREF_SEM_FAIXA, LEGREF_SEM_INDICE, LEGREF_SEM_DURACAO, LEGREF_REDE,
  LEGREF_ORCAMENTO, LEGREF_INCOMPLETO, LEGREF_MEMORIA, LEGREF_PARADO
} LegRefMotivo;

typedef struct {
  long long maxBytes;  // bytes de corpo somados nesta leitura
  int maxPedidos;      // Ranges nesta leitura
  int pedidosPorSeg;   // teto de ritmo (nao disputar a conexao do video)
} LegRefOrcamento;

typedef struct {
  LegRefFase fase;
  LegRefMotivo motivo;
  uint64_t pedido;
  int feitos, total, pedidos, faixa;
  int alternativas;    // modo so indice: outras faixas que tambem dariam referencia
  long long bytes;
  char idioma[24], codec[24];
} LegRefStatus;

// Leitor de [ini, ini+n). Devolve buffer malloc (*tam pode ser menor so no fim
// do arquivo) e o status HTTP (206 = Range respeitado). `parar` deve ser
// consultado durante a transferencia. NULL = falha.
typedef unsigned char *(*LegRefLer)(void *u, const char *url, long long ini, long n,
                                    long *tam, int *status,
                                    int (*parar)(void *), void *pu);

typedef struct LegRef LegRef;
// 1 quando a plataforma tem pedido HTTP cancelavel (rede_pedir): .tpk 4/5 e 6+,
// LG, Android. O WGT/AVPlay fica de fora.
int legref_disponivel(void);
// ler NULL = HTTP Range via rede_pedir. Cria o fio; nao faz rede.
LegRef *legref_criar(LegRefLer ler, void *u);
void legref_destruir(LegRef *r);          // cancela e faz join
// Comeca a ler a melhor faixa de texto que nao esteja em `excluidas`
// (TrackNumber), preferindo `idioma`. Substitui o pedido anterior. 0 = recusado.
uint64_t legref_pedir(LegRef *r, const char *url, uint64_t sessao, const char *idioma,
                      const int *excluidas, int nExcluidas, const LegRefOrcamento *orc);
void legref_cancelar(LegRef *r);
// Cabecalhos da fonte ("Nome: valor" por linha), lidos a cada legref_pedir na
// thread de quem pede. Sem eles o CDN que exige Referer/Authorization responde
// erro ao Range e a referencia fica "rede". NULL = nenhum.
void legref_fonte_cabecalhos(const char *(*f)(void));
// Fonte local opcional (mkvass): consultada no worker ANTES de qualquer Range.
// Devolve snapshot retido/de quem chama ou NULL; nunca toca o overlay.
void legref_fonte_local(LegendaDocumento *(*f)(const char *, uint64_t, const int *, int));
// Pausa entre Ranges (seek, buffer curto). Nao cancela.
void legref_pausar(LegRef *r, int pausar);
LegRefStatus legref_status(LegRef *r);
// Entrega o documento do pedido `pedido` (de quem chama, liberar). Uma vez so.
LegendaDocumento *legref_tomar(LegRef *r, uint64_t pedido);
const char *legref_motivo(LegRefMotivo m);   // ingles estavel, para log

// MODO SO INDICE (.tpk 6+, ligado por padrao so la; o mesmo desenho do
// EmbeddedSubtitleTimelineLoader do NuvioTV). A referencia sai INTEIRA do
// indice Cues do MKV, sem ler um bloco de legenda sequer: 2-4 Ranges no total,
// nada correndo durante o filme. Serve SRT sem CueDuration (fim estimado pelo
// proximo inicio, teto 4 s, documento marcado LEGENDA_DOC_FIM_ESTIMADO), ASS
// (os tempos filtrados para o dialogo: sem rajada de karaoke, sem evento curto
// ou longo demais) e PGS (exibe + limpa viram um intervalo, so quando a
// contagem das faixas de texto confirma o par). Android/LG/web seguem lendo os
// blocos como antes (modo desligado). Testes ligam/desligam por aqui.
void legref_modo_indice(int ligado);
int legref_modo_indice_ativo(void);

// Parsers puros, expostos para o teste.
typedef struct { long long pos; int rel; double inicio, dur; } LegRefPonto;
int legref_cues(const unsigned char *p, long n, int faixa, double escalaSeg,
                LegRefPonto **saida, int *semRel);
// Todos os pontos da faixa no indice, com ou sem posicao de bloco (o modo so
// indice nao busca blocos). dur <= 0 = ponto sem CueDuration. -1 = sem memoria
// ou mais de `max` pontos.
int legref_cues_tempos(const unsigned char *p, long n, int faixa, double escalaSeg,
                       int max, LegRefPonto **saida);
// A linha do tempo de UMA faixa a partir dos pontos do indice (em qualquer
// ordem). tipo: LEGREF_IDX_*. pares = mediana das contagens das faixas de
// texto do arquivo (0 = desconhecida), so para decidir o par do PGS.
// *estimado = 1 quando algum fim saiu estimado. Devolve o numero de falas
// (ordenadas) ou -1 sem memoria.
enum { LEGREF_IDX_TEXTO = 0, LEGREF_IDX_ASS, LEGREF_IDX_PGS };
typedef struct { double inicio, fim; } LegRefFala;
int legref_indice_linha(const LegRefPonto *v, int n, int tipo, int pares,
                        LegRefFala **saida, int *estimado, int *filtradas);

#endif
