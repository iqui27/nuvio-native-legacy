// Continue Assistindo vindo do Trakt.
//
// Antes o historico era um retrato exportado do app web (watchProgressItems do
// localStorage) e congelava no momento da exportacao. O dono ja usa o Trakt
// como fonte de progresso (traktSettings.watchProgressSource = "trakt"), entao
// perguntar direto a ele e o que mantem a fileira viva — e o que faz o app
// nativo concordar com o app web sem os dois se escreverem.
//
// CREDENCIAIS ficam em art/trakt.txt ("token<TAB>clientId"), arquivo do dono:
// tratar como segredo, nao versionar. Sem o arquivo o modulo simplesmente nao
// faz nada e a fileira cai no que veio no pacote.
#include "vistoep.h"
#ifndef NV_TRAKT_H
#define NV_TRAKT_H
#include "catalogo.h"
#include "perfil.h"
#include "traktscrobble.h"
#include <stddef.h>
#include <stdint.h>

int  trakt_carregar(const char *dirArte);   // 1 quando ha credencial

// Monta os tres cabecalhos que TODO pedido ao Trakt exige (token, versao da
// api e a chave do aplicativo) em `cab`, que precisa ter 4 posicoes — a
// ultima recebe NULL. Devolve 0 quando nao ha credencial carregada.
//
// Existe porque este mesmo bloco estava copiado em cada funcao do trakt.c, e o
// extras.c seria a quarta copia. Os buffers `aut` e `chave` sao de quem chama:
// os cabecalhos apontam para eles e precisam viver ate o fim do pedido.
int  trakt_cabecalhos(const char **cab, char *aut, size_t nAut,
                      char *chave, size_t nChave);
// CABECALHOS SEM CONTA, para os endpoints do Trakt que respondem so com a
// chave do APLICATIVO. `cab` precisa de 3 posicoes (a ultima recebe NULL) e
// `chave` e o buffer de quem chama, como em trakt_cabecalhos.
//
// MEDIDO em 16/09/2026 contra a api de verdade, com `trakt-api-version: 2` e
// `trakt-api-key` e NENHUM Authorization:
//
//   /shows/tt0903747/stats                        -> 200
//   /shows/tt0903747/seasons/1/episodes/1/stats   -> 200
//   /shows/tt0903747/seasons?extended=episodes,full -> 200
//   sem a chave nenhuma                           -> 403
//
// Ou seja: o que separava os graficos de audiencia e as notas por episodio de
// quem NUNCA vinculou o Trakt nao era a api, era este modulo — a chave do
// aplicativo so saia por trakt_cabecalhos(), que exige token. A chave nao e
// segredo de ninguem: ela vem compilada no pacote (-DNV_TRAKT_CLIENT_ID) e e a
// mesma para todo mundo que instala. Devolve 0 quando nem o vinculo nem o
// pacote trazem chave — e ai quem chama esconde a secao em vez de tentar.
int  trakt_cabecalhos_publicos(const char **cab, char *chave, size_t nChave);
int  trakt_ativo(void);
int  trakt_aguardando(void);
// Outros workers (extras) capturam ambas as geracoes antes da rede. Troca do
// Trakt no mesmo perfil invalida resposta sem limpar provas da conta/local.
unsigned long long trakt_credencial_geracao(void);
int trakt_historico_aplicar(const char *imdb, const char *tipo, int visto,
                            unsigned long long mapa, unsigned long long credencial);
// 1 quando a ultima resposta do Trakt foi 401 para ESTE token. traktauth
// observa isto para disparar a renovacao por refresh token.
int  trakt_recusada(void);
// Sessao confirmada morta: para os pedidos autenticados ate trakt_definir/esquecer.
void trakt_sessao_morta(void);
int  trakt_sessao_e_morta(void);

// Credencial vinda da CONTA, no lugar do arquivo. O token sai de
// sync_pull_provider_credentials (provider "trakt"); o clientId e do
// APLICATIVO, nao da pessoa, e vem compilado (-DNV_TRAKT_CLIENT_ID, gerado de
// local.properties). Enquanto isto nao existia, o vinculo Trakt do app nativo
// era o do dono do pacote — para todo mundo que instalasse.
int  trakt_definir(const char *token, const char *clientId);
// 1 quando o Trakt ja esta ligado com EXATAMENTE este token e este client id.
// Quem recebe a credencial da conta a cada ciclo de sync usa isto para nao
// remontar a home inteira por uma credencial que nao mudou.
int  trakt_credencial_igual(const char *token, const char *clientId);

// Esquece a credencial. Chamado ao SAIR: um token de Trakt que sobrevive ao
// logout continua ESCREVENDO (trakt_marcar) na conta de quem saiu, com o que a
// proxima pessoa assistir.
void trakt_esquecer(void);
// Reset transient tracker state for every profile opening; saved auth stays.
void trakt_abrir_sessao(void);

// Completa itens que ja tem imdb, tipo e progresso com arte (metahub, sem
// rede), sinopse/meta/nota (Cinemeta so se ainda faltarem) e minutos
// restantes, em paralelo. Compacta quem ficou sem poster ou "a seguir"
// invalido — falha do Cinemeta sozinha NAO remove o item. Nao depende de
// credencial Trakt. BLOQUEIA — chamar do fio de descoberta.
int  trakt_enfeitar_lote(CatItem *saida, int n);
int trakt_enfeitar_lote_sessao(CatItem *saida, int n, uint64_t sessao);

// Preenche ate `max` itens do "continue assistindo", ja com arte resolvida.
// BLOQUEIA — chamar do fio de descoberta. Devolve quantos preencheu.
// Remove um item da barra de retomada do Trakt (DELETE /sync/playback/<id>).
// `imdb` e a chave COMPOSTA que trakt_continuar montou. Devolve 0 quando nao ha
// id conhecido — o item veio do progresso da conta, nao do Trakt.
typedef struct { uint64_t sessao; unsigned long long mapa, credencial; char aut[200], chave[140]; } TraktDestino;
int trakt_destino_capturar(TraktDestino *);
int trakt_titulo_marcar_destino(const TraktDestino *, const char *, const char *, int);
int trakt_episodios_marcar_destino(const TraktDestino *, const char *, const VistoPar *, int, int);
typedef struct { VistoPar par; int tvdbEp; } TraktEpisodioAlvo;
int trakt_episodios_marcar_alvos_destino(const TraktDestino *, const char *, const TraktEpisodioAlvo *, int, int);
#define TRAKT_REMOVER_MAX 64
typedef struct {
  TraktDestino destino;
  char imdb[64]; long long ids[TRAKT_REMOVER_MAX]; int n;
} TraktRemocao;
int trakt_remocao_capturar(TraktRemocao *, const char *imdb);
int trakt_remocao_executar(const TraktRemocao *);
int trakt_progresso_ocultar_destino(const TraktDestino *, const char *imdb, int ocultar);
int trakt_playback_remover(const char *imdb);
// Marca (visto=1) ou desmarca (0) um lote de episodios, numa requisicao so.
// SINCRONO: quem chamar do fio de desenho tem de mandar para um fio proprio.
int trakt_episodios_marcar(const char *imdb, const VistoPar *pares, int qtd,
                           int visto);
int  trakt_continuar(CatItem *saida, int max);
// 1 quando a ULTIMA trakt_continuar nao teve resposta (rede, HTTP 5xx): o 0
// que ela devolveu e "nao sei", e nao "nada em andamento" (#151).
int  trakt_continuar_falhou(void);
// O id ("tt:S:E") e um item "a seguir" da ultima leitura — proximo episodio de
// serie cujo ultimo visto terminou. Entra na fileira com progresso 0.
int trakt_e_a_seguir(const char *id);
// Historico lido confirmou o mesmo episodio depois do progresso informado.
int trakt_cw_concluiu(const char *id, long long progressoMs);
int trakt_cw_concluiu_tvdb(const char *id, int tvdbEp, long long progressoMs);
// Oculta/reexibe a serie no progresso do Trakt (POST /users/hidden/progress_watched).
// Sincrono, para fio de trabalho. 0 sem Trakt ou sem IMDb.
int trakt_progresso_ocultar(const char *imdb, int ocultar);

// Atividade recente dos AMIGOS do dono. Usa o feed social oficial do Trakt
// (/users/me/friends/activities), mantendo no CatItem o titulo/arte normais e
// os dados sociais nos campos de apresentacao: `pais` = nome do amigo,
// `provNome` = acao e `direcao` = contexto do episodio. BLOQUEIA.
int  trakt_social(CatItem *saida, int max);

// Snapshot mensal usado pela tela Perfil e Stats. Faz as chamadas no fio de
// trabalho do app e nunca deve ser executado no laco de desenho.
int  trakt_perfil(PerfilDados *saida);

// Informa onde o dono parou. `imdb` pode trazer episodio ("tt123:4:9").
// Ate agora o app so LIA o Trakt; sem isto, assistir aqui nao mexia no
// "continue assistindo" dos outros aparelhos dele. Nao bloqueia: sai num fio.
void trakt_marcar(const char *imdb, double posSeg, double durSeg, double assistidoSeg, int assistiu);
// Scrobble durante a reproducao (#179): /scrobble/start ("Now Watching") ao
// tocar, /scrobble/pause ao pausar. `evento` e SCR_EV_* de traktscrobble.h.
// Nao bloqueia; repeticoes (start ja em pe, pause sem start) sao descartadas.
// `assistidoSeg`: midia realmente tocada nesta fonte principal (seek nao
// conta); `assistiu`: a decisao do player (player_regra_visto), sem ela nao
// sai stop. Saida recusada tambem encerra o start em pe.
int  trakt_scrobble(int evento, const char *imdb, double posSeg, double durSeg, double assistidoSeg,
                    int assistiu);
int  trakt_scrobble_alvo(int evento, const TraktScrobbleAlvo *alvo, double posSeg,
                         double durSeg, double assistidoSeg, int assistiu);

// Watchlist ("Minha Lista") e colecao ("Comprados") do dono. `qual` e
// "watchlist" ou "collection". BLOQUEIA — chamar do fio de descoberta.
int  trakt_lista(const char *qual, CatItem *saida, int max);
// A MESMA LEITURA, mas o vetor cresce por realloc (devolve em *saida, liberar
// com free). Evita reservar TRAKT_LISTA_MAX * sizeof(CatItem) (16 KB cada) de
// antemao. Pagina a watchlist; para no teto.
int  trakt_lista_cresc(const char *qual, CatItem **saida, int max);
// Teto de itens POR LISTA (watchlist, colecao), filmes + series.
#define TRAKT_LISTA_MAX 400

// Acrescenta ou tira o titulo da WATCHLIST do dono. Nao bloqueia. O estado de
// leitura ja vem em CatItem.naLista, preenchido por trakt_lista na descoberta —
// o que faltava era escrever de volta: o botao "+" so mexia num vetor local e a
// lista nos outros aparelhos nunca sabia.
void trakt_watchlist(const char *imdb, int adicionar);

// Marca (ou desmarca) o titulo como ASSISTIDO em /sync/history.
//
// NAO confundir com trakt_marcar, que e /scrobble/pause ("parei aqui") e serve
// ao player. O botao do olho quer dizer "ja vi este", e usava trakt_marcar com
// duracao 1.0 — valor que a propria guarda daquela funcao rejeita, entao nada
// chegava ao Trakt.
void trakt_assistido(const char *imdb, int marcar);

// Le o corpo de /sync/watched/movies e marca cada filme como visto no
// historico do catalogo (cat_visto). Devolve quantos entraram; -1 sem corpo,
// sem array completo ou sem memoria, sem publicar parcialmente nesses casos.
// O ciclo de trakt_continuar baixa TODAS as paginas antes de aplicar (#212).
int trakt_ler_filmes_vistos(const char *corpo);

// Nota de 1 a 10 em /sync/ratings (a reacao dos creditos, reacao.c). Nao
// bloqueia; 0 quando o Trakt esta desligado ou o id nao e IMDb.
int trakt_avaliar(const char *imdb, const char *tipo, int nota);
// Pure payload builder: title or series IMDb:season:episode; zero removes.
int trakt_nota_corpo(char *dst, size_t tam, const char *imdb, const char *tipo, int nota);
#endif
