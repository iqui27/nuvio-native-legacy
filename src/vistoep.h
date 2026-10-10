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

// Um episodio, para os lotes dos gestos da tela.
typedef struct { short temporada, episodio; } VistoPar;

// ---- o que as FONTES dizem (Trakt, conta Nuvio, jornal da conta) ------------
//
// LEITOR DE REDE NAO CHAMA vistoep_definir: chama vistoep_fonte, que CONTA o
// que a fonte fez. O relato do Silo (tt14688458, 08/10/2026) era "desmarco e
// volta marcado" com tres fontes vinculadas, e o log so dizia "30 episodios no
// mapa (28 vistos)" — o total, sem dizer QUEM marcou. Agora cada leitura diz,
// por fonte, quantos marcou e quantos tentou marcar e foram barrados.
#define VE_FONTE_PARES 12
typedef struct {
  int vistos;       // a fonte disse "visto" e entrou no mapa
  int bloqueados;   // a fonte disse "visto" e uma desmarcacao da pessoa barrou
  int venceu;       // o visto da fonte era MAIS NOVO que a desmarcacao: ela caiu
  // Os primeiros barrados (temporada, episodio), para o log dizer QUAIS.
  VistoPar par[VE_FONTE_PARES];
} VistoFonte;

// Um episodio vindo de uma fonte. `visto` 0 ou 1; `remotoMs` e QUANDO a fonte
// diz que foi visto (0 = ela nao diz). `c` acumula e pode ser NULL. Devolve 1
// quando o episodio esta no mapa depois da chamada (barrado tambem: entra com
// 0, e o que foi barrado sai em `c`); 0 so para episodio invalido ou teto.
int  vistoep_fonte(const char *imdb, int temporada, int episodio, int visto,
                   long long remotoMs, VistoFonte *c);
// DESMARCAR GANHA (vistonao.h). app.c liga aqui o juiz das desmarcacoes:
//   barra(imdb, t, e, remotoMs): 1 = ha desmarcacao da pessoa e o visto da
//     fonte nao e mais novo que ela (barrado); 0 = nao ha; -1 = havia e o
//     remoto, mais novo, ganhou. Consultado por vistoep_fonte a cada "visto".
//   gesto(imdb, pares, n, visto): avisado dos gestos locais — vistoep_aplicar e
//     vistoep_marcar_lote (o lote inteiro) e vistoep_definir(..., 1).
//     Com `pares` NULL e visto 1: TODAS as desmarcacoes do titulo caem
//     (vistoep_titulo_gesto, "marcar a serie inteira").
// NULL desliga (e o estado dos testes que linkam so este arquivo).
void vistoep_lapides(int (*barra)(const char *imdb, int temporada, int episodio,
                                  long long remotoMs),
                     void (*gesto)(const char *imdb, const VistoPar *pares, int n,
                                   int visto));
// O gesto no TITULO inteiro (visto.c): marcar a serie solta as desmarcacoes
// dela; desmarcar guarda a de cada episodio que o mapa tem como visto.
void vistoep_titulo_gesto(const char *imdb, int visto);
// A LINHA DO LOG, uma por titulo por leitura (nunca por episodio):
//   [vistoep] tt14688458: trakt +24 (bloqueados 4: T2E7 T2E8 T2E9 T2E10; remoto mais novo 0)
void vistoep_fonte_log(const char *imdb, const char *fonte, const VistoFonte *c);

// Marca um lote de uma vez, LOCALMENTE. Quem fala com o servidor e o chamador:
// o efeito local tem de ser imediato (a lista redesenha no mesmo quadro) e a
// rede leva segundos. Devolve quantos mudaram de estado de fato.
int  vistoep_marcar_lote(const char *imdb, const VistoPar *pares, int n, int visto);

// APLICA UM GESTO: muda o local e devolve em `envio` SO os episodios cujo estado
// mudou de fato (o que se manda ao Trakt, Simkl e conta). Mandar o que ja estava
// visto cria plays duplicados "de agora" no Trakt (/sync/history sem watched_at).
// Episodio de estado DESCONHECIDO (-1) conta como mudou, para o mapa vazio
// continuar reparando o remoto. `*ja` = quantos ja estavam assim. Devolve
// quantos entraram em `envio` (cabe `n`).
// Vistos da serie = o que o Trakt disse (baseTrakt) mais o que o mapa mudou
// desde entao (contarAgora - contarBase), preso a [0, exibidos].
int  vistoep_ajustar_vistos(int baseTrakt, int contarBase, int contarAgora, int exibidos);
// Primeiro episodio NAO visto (temporada >= 1, ordem temporada/numero). 0 = nenhum.
int  vistoep_primeiro_nao_visto(const char *imdb, int *temporada, int *episodio);
// Entradas do mapa DESTA serie (vistoep_n e o total de todas as series).
int  vistoep_total(const char *imdb);
int  vistoep_aplicar(const char *imdb, const VistoPar *lote, int n, int visto,
                     VistoPar *envio, int *ja);

// Monta o lote "ate aqui": todo episodio do mapa DESTA serie em posicao menor
// ou igual a (temporada, episodio), em ordem. Devolve quantos couberam em
// `saida`; `max` limita. Sai do MAPA e nao do catalogo porque e o mapa que sabe
// quais episodios existem para o Trakt — um episodio que o catalogo tem e o
// Trakt nao conhece nao pode ser marcado la.
int  vistoep_ate_aqui(const char *imdb, int temporada, int episodio,
                      VistoPar *saida, int max);
// O mesmo para uma temporada inteira.
int  vistoep_temporada(const char *imdb, int temporada, VistoPar *saida, int max);

enum { VE_LOTE_TEMPORADA = 0, VE_LOTE_ATE, VE_LOTE_DAQUI };
// ponytail: mapa inteiro + CAT_EP_MAX (1200); acompanhar se o catalogo crescer.
#define VE_MAX 8000
#define VE_LOTE_MAX (VE_MAX + 1200)
// O LOTE DE UM GESTO (temporada, ate aqui ou daqui em diante INCLUSIVO), do
// MAPA e do CATALOGO juntos, sem repetir. O mapa sozinho so enumera a serie
// quando o Trakt respondeu; sem Trakt ele so tem o que ja foi visto, e a
// temporada inteira era "0 episodios". `cat` sao os episodios que o catalogo
// lista. (agT, agE) e o proximo episodio a ir ao ar (agenda do TMDB; 0 = nao
// se sabe): dele em diante nada entra. Temporada 0 do catalogo fica fora do
// "ate aqui" (especial so entra se o mapa trouxer). `saida` nula conta.
// Daqui em diante respeita a agenda tambem no mapa, para so desmarcar lancados.
// Teto VE_LOTE_MAX; quem envia divide conforme o limite do destino.
int  vistoep_lote(const char *imdb, int modo, int temporada, int episodio,
                  const VistoPar *cat, int nCat, int agT, int agE,
                  VistoPar *saida, int max);
// Sobe a cada episodio que muda de estado (ou entra no mapa) e no logout: quem
// resume o mapa (o grafico de temporadas) so recalcula quando ela muda.
unsigned vistoep_revisao(void);
int  vistoep_n(void);          // total de episodios no mapa, para log e teste
void vistoep_esquecer(void);   // logout

#endif
