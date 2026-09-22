// SIMKL COMO FONTE DE "CONTINUAR ASSISTINDO" E DESTINO DO "+" (issue #110).
//
// O relato: "consigo vincular o Simkl em Ajustes, mas ele nao e usado no
// Continuar assistindo nem na watchlist — so aparecem Nuvio e Trakt". Era
// verdade: simklauth.c guardava o token e o unico consumidor era a aba Listas
// da Biblioteca. Este modulo e o paralelo de trakt.c para as duas coisas que o
// relato pede, e so elas.
//
// ROTAS, TODAS CONFERIDAS NA DOCUMENTACAO OFICIAL em 22/09/2026
// (https://simkl.docs.apiary.io, congelada, e https://api.simkl.org, a nova):
//   GET  /sync/activities                 carimbos por lista; "chame primeiro"
//   GET  /sync/playback                   pausados (<80%) de todos os tipos —
//                                         "Useful for Continue Watching rails"
//   DEL  /sync/playback/{id}              apaga um pausado pelo id do registro
//   GET  /sync/all-items/shows/watching   series em "watching", com o campo
//                                         next_to_watch ("S01E05")
//   GET  /sync/all-items/{movies|shows}/plantowatch
//   POST /sync/add-to-list                {"movies":[{"to":"plantowatch",
//                                          "ids":{"imdb":".."}}]}
//   POST /sync/history                    marcar visto (conferido em 22/09 em
//                                         api.simkl.org/api-reference/simkl/
//                                         add-to-history e guides/mark-as-
//                                         watched): shows[{ids,seasons[{number,
//                                         episodes[{number}]}]}]; filme em
//                                         movies[{ids}]; serie so com ids marca
//                                         todos os episodios
//   POST /sync/history/remove             desmarcar, MESMA forma (remove-from-
//                                         history). Sem seasons tira o titulo
//                                         da biblioteca do Simkl;
//                                         a doc nova diz que e o caminho
//                                         canonico de "Remove from list" (o
//                                         `to:"remove"` do add-to-list existe
//                                         mas e NAO documentado — nao usado)
// Toda chamada leva client_id, app-name e app-version NA QUERY (a doc nova
// chama os tres de obrigatorios) e Authorization: Bearer. O `simkl-api-key` em
// cabecalho e alternativa documentada ao client_id da query; NAO e mandado
// porque na Samsung o pedido sai pelo fetch do navegador, e um cabecalho a
// mais e mais um nome que o CORS do servidor precisa aceitar. User-Agent sai
// de rede.c.
//
// O QUE FICOU DE FORA, e por que:
//   - anime em "watching": o next_to_watch de anime vem na numeracao do AniDB
//     ("E12", sem temporada), e o resto do app (Cinemeta, addons) fala
//     temporada/episodio do TVDB. Converter exigiria /anime/episodes por
//     titulo. Anime PAUSADO entra, porque /sync/playback manda tvdb_season e
//     tvdb_number junto.
//   - scrobble: assistir aqui nao escreve no Simkl. O Simkl so cria playback a
//     partir de /scrobble/pause|stop, entao o "continuar" do Simkl mostra o que
//     outros clientes Simkl pausaram. Escrever e outro trabalho (outro issue).
//
// MODELO DE FIOS: o de trakt.c. Leituras BLOQUEIAM e so sao chamadas do fio de
// descoberta (montarContinuar, montar); escritas do "+" e do "tirar da
// retomada" saem num fio proprio e o fio de desenho so le um estado.
#ifndef NV_SIMKL_H
#define NV_SIMKL_H
#include "catalogo.h"
#include "vistoep.h"
#include <stddef.h>

// 1 quando ha token do Simkl nesta TV (vinculo feito em Ajustes).
int simkl_ativo(void);

// A frase honesta para quando a opcao escolhida e o Simkl e nao ha vinculo.
// Chave de idioma_tab.h; quem desenha passa por i18n como qualquer texto.
#define SIMKL_VINCULE "Vincule o Simkl em Ajustes"
// SIMKL_VINCULE quando a opcao pede Simkl e nao ha token; NULL quando esta
// tudo certo (ou quando a opcao nao e Simkl — `querSimkl` 0).
const char *simkl_aviso_sem_vinculo(int querSimkl);

// --- leitores PUROS (sem rede), publicos para o teste -------------------------

// GET /sync/playback -> itens da fileira. `ids` recebe o id do REGISTRO de
// playback (o que o DELETE pede), na mesma posicao do item; pode ser NULL.
// Item sem ids.imdb nao entra: sem imdb nao ha arte, fonte nem retomada.
int simkl_ler_playback(const char *json, CatItem *dst, long long *ids, int max);

// GET /sync/all-items/shows/watching -> o PROXIMO episodio de cada serie
// (next_to_watch "S02E05"), com progresso 0 e o last_watched_at como instante.
// Serie sem next_to_watch (em dia com o que foi ao ar) nao entra.
int simkl_ler_assistindo(const char *json, CatItem *dst, int max);

// GET /sync/all-items/<movies|shows>/plantowatch -> itens com naLista = 1.
int simkl_ler_plantowatch(const char *json, int serie, CatItem *dst, int max);

// Corpo e rota do "+" (adicionar = 1) e do "-" (0). 1 quando montou; 0 para id
// que nao e um tt do IMDb (o corpo iria com lixo dentro).
int simkl_corpo_lista(char *dst, size_t tam, const char *imdb,
                      const char *tipo, int adicionar);
const char *simkl_rota_lista(int adicionar);

// --- leituras de rede: BLOQUEIAM, so do fio de descoberta ---------------------

// Pausados + "a seguir", mais recente primeiro, ja com arte. Devolve 0 sem
// vinculo. Pula a rede quando /sync/activities responde igual a ultima vez
// (a doc manda nao baixar all-items "on a timer").
int simkl_continuar(CatItem *saida, int max);
// O id ("tt:S:E") e um "a seguir" do Simkl da ultima leitura (progresso 0).
int simkl_e_a_seguir(const char *id);

// O Plan to Watch inteiro (filmes e series), com naLista = 1.
int simkl_plantowatch(CatItem *saida, int max);
// 1 quando `imdb` estava no Plan to Watch na ultima leitura (ou foi posto la
// por este app depois dela). E a GUARDA do "-": ver simkl_lista_tipo.
int simkl_na_plantowatch(const char *imdb);

// --- escritas: NAO bloqueiam --------------------------------------------------

// Estados de uma escrita, com os MESMOS numeros que ctxmenu.c usa para o
// Trakt (CTX_PENDENTE = 1, CONFIRMADA = 2, FALHA = 3).
enum { SMK_OP_NENHUMA = 0, SMK_OP_PENDENTE, SMK_OP_CONFIRMADA, SMK_OP_FALHA };

// Poe (1) ou tira (0) o titulo do Plan to Watch, num fio. 1 quando a escrita
// saiu; 0 sem vinculo, com outra escrita no ar, ou — no "-" — quando o titulo
// nao esta no Plan to Watch conhecido. Nesse ultimo caso nada e mandado DE
// PROPOSITO: /sync/history/remove sem temporadas apaga o titulo da biblioteca
// INTEIRA do Simkl, historico de episodios vistos junto. Um "-" num titulo que
// esta nos Salvos por causa do Trakt ou da lista local, e que no Simkl esta
// em "completed", apagaria o que a pessoa assistiu.
int simkl_lista_tipo(const char *imdb, const char *tipo, int adicionar);
int simkl_lista_estado(void);

// DELETE /sync/playback/<id> do item da fileira, num fio. `imdb` e a chave
// composta da fileira ("tt:S:E" ou "tt"). 0 quando nao ha id conhecido — o
// item nao veio do Simkl.
int simkl_playback_remover(const char *imdb);

// --- historico ("marcar como assistido") --------------------------------------

// Teto do lote por pedido. Uma temporada de anime passa de 64; 256 cobre
// qualquer temporada real e ainda cabe num corpo de ~5 KB.
#define SMK_LOTE_MAX 256

// Corpo de /sync/history (e /remove, mesma forma) para um lote de episodios,
// agrupado por temporada. 0 para id que nao e tt, lote vazio ou buffer curto.
int simkl_corpo_historico_eps(char *dst, size_t tam, const char *imdb,
                              const VistoPar *pares, int qtd);
// Corpo do titulo inteiro. Filme: movies[{ids}]. Serie marcada: shows[{ids}].
// Serie DESMARCADA precisa de `temporadas` (numeros), senao 0 — ver simkl.c.
int simkl_corpo_historico_titulo(char *dst, size_t tam, const char *imdb,
                                 const char *tipo, const int *temporadas,
                                 int nt, int visto);
const char *simkl_rota_historico(int visto);

// SINCRONAS, bloqueiam ate 20 s: so de um fio (visto.c). 1 = 2xx sem
// not_found; 0 sem vinculo, sem corpo ou recusado.
int simkl_episodios_marcar(const char *imdb, const VistoPar *pares, int qtd, int visto);
int simkl_titulo_marcar(const char *imdb, const char *tipo, const int *temporadas,
                        int nt, int visto);

// Esquece caches e tabelas (logout, desvincular). O token e de simklauth.c.
void simkl_esquecer(void);

#endif
