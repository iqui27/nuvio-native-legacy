#ifndef NV_LEGENDA_H
#define NV_LEGENDA_H
#include <stddef.h>

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
  // Tudo aqui nasce ZERADO no caminho SRT/VTT, e o desenho trata zero como
  // "o arquivo nao disse nada" — e ai quem manda e a preferencia da pessoa.

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

/* Os dois parsers, expostos para o teste poder cobrir cada um sem depender da
 * heuristica de deteccao. */
int legenda_extrair_srt(const char *corpo, LegendaCue **saida);
int legenda_extrair_ass(const char *corpo, LegendaCue **saida);
/* 1 quando o corpo se parece com ASS/SSA. */
int legenda_eh_ass(const char *corpo);

#endif
