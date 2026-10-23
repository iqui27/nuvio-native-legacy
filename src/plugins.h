// Plugins Nuvio (#134, F09): repositorios de scrapers JS (o formato do app
// oficial) como MAIS UMA ORIGEM de fontes, ao lado dos addons Stremio.
//
// Um repositorio e um manifest.json com scrapers[] ({id, name, filename,
// supportedTypes, enabled}); cada scraper e um JS com getStreams(tmdbId,
// mediaType, temporada, episodio). Quem roda o JS e pluginjs.c (QuickJS); este
// modulo cuida do resto: a lista de repositorios (da conta, tabela `plugins`,
// como no web e no Android), manifesto e codigo em cache (6 h), quem roda para
// qual titulo, a conversao do resultado em Stream com o nome do scraper, e a
// GERACAO de conta/perfil: trocar de perfil ou de conta avanca o grupo de rede
// (rede_grupo_avancar), interrompe os scrapers e fetches em voo e descarta o
// que chegar atrasado.
//
// DESLIGADO POR PADRAO: o liga/desliga e deste aparelho, por perfil. Em alvo
// sem motor/transporte (plugins_disponivel() == 0) nada roda e a tela diz
// "indisponivel neste aparelho".
#ifndef NV_PLUGINS_H
#define NV_PLUGINS_H
#include "streams.h"

#define PLUG_REPOS_MAX     24
#define PLUG_SCRAPERS_MAX  160   // = ADD_EXTRA_MAX em addons.c (vagas na folha)

void plugins_iniciar(void);              // le o estado do perfil ativo
void plugins_perfil_mudou(void);         // relê para o perfil novo (avanca a geracao)
int  plugins_disponivel(void);           // motor + transporte neste alvo
int  plugins_ligado(void);
void plugins_definir_ligado(int ligado);
unsigned plugins_geracao(void);          // muda a cada troca de conta/perfil
// Current target's last foreground round. Cut counts include unstarted scrapers.
typedef struct { int enfileirados, terminados, cortados; } PlugResumo;
void plugins_resumo(const char *id, const char *tipo, PlugResumo *saida);

// Repositorios (para a tela e o sync).
typedef struct { char url[600]; char nome[96]; int ativo; char tipo[24]; } PlugRepo;
int  plugins_n_repos(void);
int  plugins_repo(int i, PlugRepo *saida);
int  plugins_repo_scrapers(int i, int *ligados);   // total; *ligados os que rodam
int  plugins_repo_estado(int i);    // 0 = manifesto nao lido, 1 = ok, -1 = falhou, 2 = nao roda aqui
int  plugins_alternar_repo(int i);  // devolve o estado novo
typedef struct { char id[64]; char nome[96]; int filme, serie, ativo; } PlugScraper;
int  plugins_scraper(int i, int j, PlugScraper *saida);
int  plugins_alternar_scraper(int i, int j);   // estado novo; -1 nao existe
int  plugins_remover_repo(int i);
// Acrescenta pela URL digitada (com ou sem /manifest.json). 1 = entrou;
// 0 = invalida; -1 = ja existe; -2 = lista cheia.
int  plugins_adicionar_repo(const char *url);
// Le os manifestos de novo (fio proprio). Volta na hora.
void plugins_atualizar(void);
int  plugins_atualizando(void);

// SYNC COM ACK. A lista local tem uma revisao (`rev`) que cada edicao
// (adicionar/remover/ligar repositorio) avanca e marca PENDENTE, gravada em
// disco — remover o ULTIMO repositorio tambem e uma edicao e sobe como lista
// vazia. O sync tira um retrato (plugins_retrato: lista, n >= 0, rev,
// geracao), empurra, e so com 2xx chama plugins_confirmar com o mesmo rev e
// geracao: uma edicao feita durante o push (rev novo) ou uma troca de perfil
// (geracao nova) mantem a pendencia. A leitura da conta so substitui a lista
// local sem pendencia e na mesma geracao; um 200 com array VAZIO e uma lista
// vazia valida (a conta removeu tudo), e resposta que nao e array nao e lista.
typedef struct {
  PlugRepo lista[PLUG_REPOS_MAX];
  int n;
  unsigned rev, geracao;
  int pendente;
} PlugRetrato;
void plugins_retrato(PlugRetrato *s);
int  plugins_confirmar(unsigned rev, unsigned geracao);       // 1 = pendencia limpa
int  plugins_definir_da_conta(const PlugRepo *lista, int n, unsigned geracao);  // 1 = mudou
int  plugins_pendente(void);
// Le a resposta da tabela `plugins` (array JSON do PostgREST). -1 quando NAO
// e um array (erro/HTML/lixo nao e "lista vazia"); senao a quantidade lida.
int  plugins_ler_conta(const char *json, PlugRepo *saida, int max);
void plugins_esquecer(void);         // saiu da conta (avanca a geracao)

// As fontes dos plugins para `id` ("tt1234567", "tt1234567:1:2" ou
// "tmdb:603[:t:e]") e `tipo` ("movie" / "series"). BLOQUEIA — chamar de fio
// proprio; roda os scrapers em paralelo, cada um com o seu prazo. Devolve a
// quantidade e o vetor em *saida (free pelo chamador); 0 com plugins
// desligados ou sem scraper para o tipo. -1 cancelado ou geracao trocada.
// `cancelado` devolvendo 2 e CORTE: para de esperar e fica com o que terminou.
// `aviso` (pode ser NULL; ver OrigemAviso em addons.h) ouve cada scraper.
int  plugins_consultar(const char *id, const char *tipo,
                       int (*cancelado)(void *), void *ctx,
                       void (*aviso)(void *u, int k, const char *nome, int estado,
                                     const void *fontes, int n),
                       void *avisoU, Stream **saida);
// Capturar na UI: scrapers, ordem, chave TMDB e credenciais pertencem ao
// retrato. A consulta independente nunca relê a configuracao do perfil novo
// nem publica fontes/progresso. 0 = vazio valido; -1 = incompleta/cancelada.
typedef struct PluginsConsulta PluginsConsulta;
PluginsConsulta *plugins_consulta_capturar(void);
int plugins_consulta_atual(const PluginsConsulta *consulta);
int plugins_consulta_consultar(const PluginsConsulta *consulta, const char *id, const char *tipo,
                              int (*cancelado)(void *), void *ctx,
                              void (*aviso)(void *u, int k, const char *nome, int estado,
                                            const void *fontes, int n),
                              void *avisoU, Stream **saida);
void plugins_consulta_liberar(PluginsConsulta *consulta);
// Pluga plugins_consultar na busca de fontes dos addons. Uma vez, no arranque.
void plugins_ligar_aos_addons(void);

#endif
