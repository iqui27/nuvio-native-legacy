#ifndef NV_LEGENDA_H
#define NV_LEGENDA_H

// Manual delay accepted by the player controls and stored preferences (#425).
#define LEG_ATRASO_MANUAL_MAX 60000
#include <stddef.h>
#include <stdint.h>

// Quantos blocos podem estar no ar AO MESMO TEMPO.
//
// SRT nunca precisou disto: um bloco por vez, e o parser antigo devolvia so
// um. ASS de fansub precisa — a fala embaixo e o letreiro traduzido no alto
// sao dois eventos SIMULTANEOS, e mostrar so um e exatamente a "metade das
// falas que some" do relato da issue #92.
//
// TRES e nao mais: cada bloco custa ate quatro rasterizacoes de linha, e
// text.c so rasteriza TXT_POR_QUADRO (4) por quadro. Com tres blocos o pior
// caso ja leva tres quadros para assentar; com oito, a legenda apareceria
// visivelmente em pedacos.
#define LEGENDA_SIMULTANEAS 3

typedef struct {
  double inicio, fim;
  char texto[768];

  // --- O QUE O ARQUIVO ASS DIZ SOBRE ESTE BLOCO -----------------------------
  //
  // SRT/VTT also supplies bold/italic emphasis; other style fields stay
  // unset. Zero means the file did not specify a value: use user settings.

  // Ancora ASS 1..9 (1 = base-esquerda, 5 = meio-centro, 9 = topo-direita).
  // 0 = o arquivo nao declarou; usa a posicao escolhida na folha de faixas.
  short an;
  short negrito, italico;
  // 0xRRGGBB, ou -1 quando o arquivo nao traz cor propria.
  int   cor;
  // \pos(x,y) em unidades de `resX`/`resY`. Negativo = sem \pos.
  float posX, posY;
  // PlayResX/PlayResY do cabecalho (0 quando ausente). E a regra de tres que
  // transforma o \pos do arquivo em pixel de tela: um fansub feito em 1280x720
  // posiciona em 1280x720, e desenhar isso em 1920 sem converter joga o texto
  // para a esquerda da tela.
  float resX, resY;
  // Ordem de leitura no arquivo. Sobrevive a ordenacao por tempo e serve de
  // criterio de desempate: dois eventos no mesmo instante empilham na ordem em
  // que o fansub os escreveu, que e a ordem em que ele pensou a tela.
  int   ordem;
} LegendaCue;

/* OpenSubtitles e desenhado pela UI, acima do plano de video. */
void legenda_carregar(const char *url);
/* O mesmo, avisando `pronto` no fio do download (corpo UTF-8 ou NULL, a
 * geracao do pedido e se ela ainda e a dona do overlay). `pronto` e chamado
 * exatamente uma vez, inclusive em falha, e nao pode tocar a UI. */
typedef void (*LegendaBaixada)(const char *corpo, unsigned geracao, int vigente, void *u);
unsigned legenda_carregar_com(const char *url, LegendaBaixada pronto, void *u);
unsigned legenda_carregar_idioma(const char *url, const char *idioma, LegendaBaixada pronto, void *u);
// Um candidato por play; carregar_com adota pelo link, no mesmo fio/callback.
void legenda_pre_baixar(const char *url);
void legenda_pre_descartar(const char *motivo);
void legenda_pre_reiniciar(void);
void legenda_desligar(void);
// Liga com um corpo ja em memoria (parser sincrono). Ver a nota em legenda.c.
void legenda_definir_corpo(const char *corpo);
/* O mesmo, para o lote seguinte da mesma faixa: sem apagar o quadro. */
void legenda_atualizar_corpo(const char *corpo);
/* GERACAO DA FAIXA ATIVA. Toda troca de dono da legenda (desligar, carregar,
 * definir) a incrementa. Quem entrega em segundo plano (o mkvass) guarda a
 * geracao que recebeu e entrega COM ela: se a legenda mudou de dono no meio
 * (outra faixa, externa, desligada), o lote e descartado em vez de religar a
 * faixa antiga. definir_se devolve a geracao nova (0 = descartado);
 * atualizar_se devolve 1 quando publicou (mesma geracao, legenda ligada). */
unsigned legenda_geracao(void);
int      legenda_ligada_em(unsigned geracao);
unsigned legenda_definir_corpo_se(const char *corpo, unsigned geracao);
int      legenda_atualizar_corpo_se(const char *corpo, unsigned geracao);
int  legenda_texto(double posSeg, int atrasoMs, char *dst, size_t tam);

// Todos os blocos vivos em `posSeg`, ate `max`. Devolve quantos preencheu.
// Ordem: a do arquivo (ver `ordem`), que e a ordem de empilhamento na tela.
int  legenda_cues(double posSeg, int atrasoMs, LegendaCue *dst, int max);

/* Parser puro, tambem usado pela regressao. O chamador libera *saida.
 * RECONHECE O FORMATO SOZINHO: SRT/VTT pelo "-->" e ASS/SSA pelo cabecalho.
 * Era aqui que todo arquivo .ass morria em silencio — a varredura procurava
 * "-->" e um `Dialogue:` nao tem nenhum, entao um ASS perfeitamente valido
 * devolvia ZERO blocos e a tela ficava sem legenda sem dizer por que. */
int legenda_extrair(const char *corpo, LegendaCue **saida);
// Subtitle bytes -> NUL-terminated UTF-8 (malloc; caller frees).
// UTF-16 with BOM, UTF-8 with/without BOM, otherwise the legacy Windows
// 1252/1251/1256 detector. Optional `origem` reports the detected encoding.
// Returns NULL for invalid arguments or allocation failure.
char *legenda_utf8(const char *bytes, long n, const char **origem);
// Declared Greek/Turkish selects 1253/1254 only after BOM/UTF-8 handling.
char *legenda_utf8_idioma(const char *bytes, long n, const char *idioma,
                         const char *arquivo, const char **origem);
// Explicit metadata first; otherwise bounded .el./.gre./.ell./.tr./.tur. tags.
const char *legenda_idioma(const char *idioma, const char *arquivo);

/* Os dois parsers, expostos para o teste poder cobrir cada um sem depender da
 * heuristica de deteccao. */
int legenda_extrair_srt(const char *corpo, LegendaCue **saida);
int legenda_extrair_ass(const char *corpo, LegendaCue **saida);
/* 1 quando o corpo se parece com ASS/SSA. */
int legenda_eh_ass(const char *corpo);
/* 1 while the subtitle on (this generation) is an ASS/SSA document. */
int legenda_ass_ativa(void);

/* Immutable subtitle documents, independent of the currently drawn overlay.
 * Build once on a worker, retain while analysing/drawing, release when done.
 * `identidade` must identify the subtitle track/file, never contain a signed
 * URL or credentials. A partial mkvass batch MUST NOT set COMPLETO for the
 * whole track. JANELA + COMPLETO proves only its explicit indexed interval. */
typedef struct LegendaDocumento LegendaDocumento;
enum {
  LEGENDA_DOC_COMPLETO = 1u << 0,
  LEGENDA_DOC_FORCED = 1u << 1,
  LEGENDA_DOC_SINAIS = 1u << 2,
  LEGENDA_DOC_JANELA = 1u << 3 /* COMPLETO somente entre janelaInicio/Fim */
};
typedef struct {
  char idioma[24], origem[96], identidade[128];
  uint64_t sessao;
  unsigned flags;
  double duracaoSeg; /* media duration when known; zero = unknown */
  double janelaInicio, janelaFim; /* intervalo indexado integralmente colhido */
} LegendaDocumentoInfo;
LegendaDocumento *legenda_documento_criar(const char *corpo,
                                        const LegendaDocumentoInfo *info);
/* Uses the existing UTF-8/UTF-16/Windows charset conversion before parsing. */
LegendaDocumento *legenda_documento_bytes(const char *bytes, long n,
                                         const LegendaDocumentoInfo *info);
LegendaDocumento *legenda_documento_de_cues(const LegendaCue *v, int n,
                                           const LegendaDocumentoInfo *info);
/* Copy the current overlay only if `geracao` still owns it. Does not change
 * selection/libass. Caller supplies accurate source/language/completeness. */
LegendaDocumento *legenda_documento_ativo(unsigned geracao,
                                         const LegendaDocumentoInfo *info);
LegendaDocumento *legenda_documento_reter(LegendaDocumento *doc);
void legenda_documento_liberar(LegendaDocumento *doc);
const LegendaDocumentoInfo *legenda_documento_info(const LegendaDocumento *doc);
const LegendaCue *legenda_documento_dados(const LegendaDocumento *doc, int *n);
uint64_t legenda_documento_hash(const LegendaDocumento *doc);
int legenda_documento_cues(const LegendaDocumento *doc, double posSeg,
                          int atrasoMs, LegendaCue *dst, int max);

#endif
