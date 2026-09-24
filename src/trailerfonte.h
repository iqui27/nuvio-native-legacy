// DE ONDE VEM O TRAILER: a regra, sem rede, sem tela e sem plataforma cravada.
//
// Pedido do dono (22/09/2026): "deixa o toggle no settings de qual o source do
// trailer". Ajustes › Detalhes › "Fonte do trailer" (AJ_TRAILER_FONTE):
//   0 Automatico  a ordem de sempre, Apple -> IMDb -> YouTube, pulando o que a
//                 plataforma nao toca (IMDb na Samsung so com o servico de
//                 recomendacoes na build, que faz a pergunta com o Referer que o
//                 navegador nao deixa por; YouTube so na Samsung: o app nativo
//                 da LG nao tem onde embutir o player dele);
//   1 Apple TV, 2 IMDb, 3 YouTube  SO aquela. Sem ela para o titulo (ou sem
//                 ela nesta TV), sem trailer — com uma linha de log dizendo
//                 por que, para "o trailer sumiu" nao virar adivinhacao.
//
// A escolha mora aqui, e nao duplicada em detail.c e home.c, porque sao dois
// consumidores (pagina de titulo e destaque) x dois alvos, e a mesma regra
// escrita quatro vezes diverge na primeira mudanca. tests/trailer-fonte.sh
// exercita estas funcoes com as duas plataformas no mesmo binario.
#ifndef NV_TRAILERFONTE_H
#define NV_TRAILERFONTE_H

// Valores do ajuste = indice gravado em ajustes.txt ("trailerFonteLocal N").
// NAO reordenar: o arquivo guarda o numero.
enum { TRF_AUTO = 0, TRF_APPLE = 1, TRF_IMDB = 2, TRF_YOUTUBE = 3 };

// O que cada fonte tem para o titulo agora. `*_respondeu` = a busca terminou
// (com ou sem trailer); enquanto 0, a fonte ainda pode chegar. YouTube nao tem
// "respondeu" (a lista do TMDB chega sem marca de fim): quem chama poe 1 quando
// ja desistiu de esperar.
typedef struct {
  const char *apple;   int appleRespondeu, appleFalhou;
  const char *imdb;    int imdbRespondeu;
  const char *youtube; int youtubeRespondeu;
} TrailerCandidatos;

typedef enum {
  TRF_ESPERA  = 0,   // a fonte da vez ainda pode chegar: nao abrir nada
  TRF_ABRE    = 1,   // *url/*qual preenchidos
  TRF_NENHUMA = 2    // acabou a lista: sem trailer (logar uma vez)
} TrailerDecisao;

// A ordem que o ajuste `ajuste` pede na plataforma (`tizen` 1 = Samsung).
// Escreve ate 3 fontes TRF_* em `ordem` e devolve quantas.
int  trailerfonte_ordem(int ajuste, int tizen, int ordem[3]);
// Percorre a ordem: a primeira fonte com trailer abre; uma que ainda nao
// respondeu segura a fila (senao a mais rapida ganharia sempre, e a Apple, que
// e a melhor, e a mais lenta); a que respondeu vazia (ou falhou) cede a vez.
TrailerDecisao trailerfonte_escolher(int ajuste, int tizen, const TrailerCandidatos *c,
                                     const char **url, int *qual);
// Proxima fonte da ordem depois de `qual`, ou 0: e o que decide se o prazo
// vencido "tenta a proxima" ou "fica a arte".
int  trailerfonte_depois(int ajuste, int tizen, int qual);
// "apple" / "imdb" / "youtube", para o log.
const char *trailerfonte_nome(int qual);
// 1 quando o trailer em tela cheia pode ter som. Samsung: NUNCA (decisao do
// dono, 22/09/2026, "trailer fica mudo"): a Apple la e uma variante so de
// video (trailerapple.c, varianteMidia) e trocar para o YouTube so por causa
// do som punha na tela o player que cai em "Video player configuration error".
int  trailerfonte_com_som(int tizen);

// 1 quando o IMDb toca na Samsung: a build tem o servico de recomendacoes, por
// onde passa a pergunta que exige Referer (#136). O definir e para os testes.
int  trailerfonte_imdb_tizen(void);
void trailerfonte_definir_imdb_tizen(int sim);

// Atalhos com o ajuste gravado e a plataforma deste build.
int  trailerfonte_ajuste(void);
int  trailerfonte_tizen(void);

#endif
