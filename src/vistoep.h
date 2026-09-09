// QUAIS EPISODIOS FORAM VISTOS — o dado que o app nunca teve.
//
// O historico de catalogo.c e por TITULO: id_base() trunca o id no ':' de
// proposito, entao "tt123:2:8" e "tt123" sao a mesma coisa para ele. Isso basta
// para "marcar a serie como assistida" e nao basta para nada por episodio.
//
// POR QUE UM MODULO PROPRIO, e nao um campo em CatEp: a lista de episodios e
// carregada SOB DEMANDA e descartada ao trocar de titulo (cat_definir_tudo zera
// nEps), enquanto o que foi visto vale para a sessao inteira e vem da rede
// antes de qualquer lista existir. Guardar no episodio faria o dado nascer e
// morrer junto com a tela que o mostra.
//
// DUAS FONTES, e as duas escrevem aqui:
//   - a conta Nuvio, por sync_pull_watched_items, que ja devolve `season` e
//     `episode` em cada linha (PLANO-CONTA-SYNC.md secao 1.5) e cujo episodio
//     era lido e jogado fora;
//   - o Trakt, por /sync/watched/shows, que devolve o mapa COMPLETO de
//     temporadas e episodios. Nao serve /sync/history: ela e paginada nas
//     ultimas reproducoes e responde "o que foi visto recentemente", nao "o que
//     esta visto".
//
// A CHAVE E O ID DO TITULO, sempre truncado no ':' — quem chama pode passar
// "tt123" ou "tt123:2:8", e o segundo e o formato que CatItem.imdb carrega num
// item de "Continuar assistindo".
#ifndef NV_VISTOEP_H
#define NV_VISTOEP_H

// -1 nao se sabe (nunca foi consultado, ou o titulo nao esta no mapa)
//  0 sabe-se que NAO foi visto
//  1 visto
int  vistoep_estado(const char *imdb, int temporada, int episodio);

// Marca um episodio. `visto` 0 ou 1. Chamado tanto pela leitura da rede quanto
// pela acao da pessoa na TV — o efeito local e imediato, e quem fala com o
// servidor e o chamador.
void vistoep_definir(const char *imdb, int temporada, int episodio, int visto);

// Quantos episodios de um titulo estao marcados como vistos. Serve ao rotulo
// da lista ("12 de 20") sem obrigar a tela a varrer o mapa.
int  vistoep_contar(const char *imdb);

// O titulo TEM mapa? Distingue "serie sem episodio visto" de "nunca soubemos
// nada desta serie", que e a diferenca entre desenhar zero e nao desenhar nada.
int  vistoep_conhecido(const char *imdb);

// Le o corpo de /shows/<id>/progress/watched do Trakt, que enumera a serie
// INTEIRA com `completed` por episodio — entao este leitor escreve 0 tambem, e
// nao so 1. Devolve quantos episodios entraram, ou -1 em corpo invalido.
int  vistoep_ler_progresso(const char *imdb, const char *json);

// Um episodio, para os lotes. Os tres gestos que a tela oferece — este
// episodio, ate aqui, a temporada inteira — sao o MESMO lote com tamanhos
// diferentes, e por isso ha uma funcao so em vez de tres.
typedef struct { short temporada, episodio; } VistoPar;

// Marca um lote de uma vez, LOCALMENTE. Quem fala com o servidor e o chamador:
// o efeito local tem de ser imediato (a lista redesenha no mesmo quadro) e a
// rede leva segundos. Devolve quantos mudaram de estado de fato.
int  vistoep_marcar_lote(const char *imdb, const VistoPar *pares, int n, int visto);

// Monta o lote "ate aqui": todo episodio do mapa DESTA serie em posicao menor
// ou igual a (temporada, episodio), em ordem. Devolve quantos couberam em
// `saida`; `max` limita. Sai do MAPA e nao do catalogo porque e o mapa que sabe
// quais episodios existem para o Trakt — um episodio que o catalogo tem e o
// Trakt nao conhece nao pode ser marcado la.
int  vistoep_ate_aqui(const char *imdb, int temporada, int episodio,
                      VistoPar *saida, int max);
// O mesmo para uma temporada inteira.
int  vistoep_temporada(const char *imdb, int temporada, VistoPar *saida, int max);

int  vistoep_n(void);          // total de episodios no mapa, para log e teste
void vistoep_esquecer(void);   // logout

#endif
