// Biblioteca e vistos DA CONTA — as duas superficies que o sync baixava e
// jogava fora.
//
// O DEFEITO QUE ISTO CONSERTA (issue do @Haylefal, LG webOS 4): "Library is
// empty and not carrying over from other instances running on other devices.
// Top right of library says 'Local'." Estava certo: `puxarSoLeitura` em sync.c
// chamava `sync_pull_library` e `sync_pull_watched_items`, CONTAVA as linhas e
// liberava o corpo. A conta respondia, o numero entrava no resumo dos ajustes,
// e a tela de Biblioteca continuava mostrando so o que o Trakt tivesse trazido.
// Sem Trakt vinculado ela ficava vazia com o selo "LOCAL" — que era a unica
// parte honesta da tela.
//
// O CONTRATO, LIDO NO APP WEB e nao suposto (NuvioWeb-0.3.38-beta,
// js/core/profile/savedLibrarySyncService.js e watchedItemsSyncService.js):
//
//   biblioteca: RPC `sync_pull_library`, parametros
//               { p_profile_id, p_limit, p_offset } — paginada, o web usa
//               p_limit=500 e soma paginas ate uma vir incompleta.
//               Linha: { content_id, content_type, name, poster, poster_shape,
//               background, description, release_info, imdb_rating, genres,
//               addon_base_url, added_at }.
//   vistos:     RPC `sync_pull_watched_items`, parametros
//               { p_profile_id, p_page, p_page_size } — p_page comeca em 1.
//               Linha: { content_id, content_type, title, season, episode,
//               watched_at }.
//
// E A CORRECAO MAIS IMPORTANTE DE TODAS: `sync_pull_saved_library` NAO EXISTE,
// e nao e so "este servidor nao tem" — a funcao nao existe em lugar nenhum. O
// servico do web que se chama savedLibrarySyncService chama `sync_pull_library`.
// A secao 1.4 do PLANO-CONTA-SYNC.md lista "Biblioteca" e "Biblioteca salva"
// como duas superficies; sao a MESMA, e os parametros paginados que a tabela
// atribui a segunda (`p_limit`/`p_offset`) sao os da primeira. O codigo antigo
// tinha as duas metades separadas e as duas erradas: chamava
// `sync_pull_library` SEM p_limit/p_offset e `sync_pull_saved_library` (que
// responde PGRST202) COM eles.
//
// ONDE ESTE MODULO PARA: ele le e guarda, e sabe aplicar no catalogo. Ele NAO
// fala com a rede — quem busca e sync.c, no fio dele, e entrega o corpo cru
// aqui no fio principal (mesma disciplina do colBlob/catHomeBlob). Assim este
// arquivo compila e e testado sem SDL, sem rede e sem descoberta.
#ifndef NV_CONTALIB_H
#define NV_CONTALIB_H

// Teto de itens guardados. NAO e arbitrario: cada item abaixo custa ~1,5 KB, e
// esta TV ja bateu no teto de 128 MiB do WebAssembly com o catalogo publicado
// (TIZEN-MEMORIA.md). 200 itens sao ~300 KB, alocados SO quando existe conta.
// O log diz quando a lista foi cortada — lista cortada em silencio e o mesmo
// defeito de origem que este modulo conserta.
#define CONTALIB_MAX        200
// Vistos custam 48 bytes cada; o teto aqui pode ser folgado.
#define CONTALIB_VISTO_MAX  2000

typedef struct {
  char id[24];        // content_id, "tt0111161"
  char tipo[8];       // "movie" | "series", ja normalizado
  char titulo[160];
  char poster[512];
  char backdrop[512];
  char meta[96];      // release_info ("1994", "2008–2013")
  char genero[160];   // "Filme · Drama · Crime", ja composto
  long long addedMs;  // added_at, para a ordem "Ordem da lista"
  int  nota;          // imdb_rating em porcentagem; 0 = desconhecida
} ContaLibItem;

typedef struct {
  char id[24];
  char tipo[8];
  int  temporada, episodio;   // 0 quando a linha e do TITULO inteiro
  long long vistoMs;
} ContaVisto;

// Le a resposta de `sync_pull_library`. Devolve quantos itens entraram.
//
// -1 QUANDO A RESPOSTA FOI RECUSADA, e recusar faz parte do contrato: a regra 1
// da secao 1.6 do PLANO-CONTA-SYNC.md ("lista remota vazia nao e delecao") esta
// implementada aqui. Resposta que nao e array, ou array vazio com itens ja
// guardados, NAO substitui nada — uma resposta vazia pode ser perfil errado,
// 401 mal tratado ou o servidor fora do ar, e o web faz exatamente isto
// (`if (!remoteItems.length && localItems.length) return localItems;`).
int contalib_ler_biblioteca(const char *json);

// Idem para `sync_pull_watched_items`, com a mesma recusa de lista vazia.
int contalib_ler_vistos(const char *json);

int contalib_n(void);
const ContaLibItem *contalib_item(int i);   // NULL fora da faixa
int contalib_n_vistos(void);
const ContaVisto *contalib_visto(int i);

// 1 quando a CONTA ja respondeu com pelo menos um item de biblioteca. E o que
// decide o selo de origem da tela de Biblioteca: sem isto o selo diria "CONTA"
// enquanto a conta ainda nao respondeu, que e a mesma familia de mentira do
// "NUVIO" cravado que ele tinha antes.
int contalib_tem_conta(void);

// Marca os itens da conta no catalogo (naLista) e acrescenta os que faltam.
// Devolve quantos itens do catalogo ficaram marcados. FIO PRINCIPAL apenas —
// mexe no vetor que o desenho le.
//
// NUNCA DESMARCA. Ela so liga `naLista`; a lista da conta vazia nao apaga o que
// o Trakt marcou, e o inverso tambem vale. Desmarcar exigiria saber que a
// resposta foi completa e correta, e uma resposta vazia nunca prova isso.
int contalib_aplicar_catalogo(void);

// Reaplica se o catalogo foi TROCADO desde a ultima aplicacao. Chamar por
// quadro; custa uma comparacao de inteiro e um strcmp de 16 bytes no caso
// comum.
//
// POR QUE PRECISA EXISTIR. A descoberta republica o catalogo inteiro
// (cat_definir_tudo) varias vezes por ciclo, e cada republicacao apaga tudo que
// nao veio dela — inclusive os itens da conta que este modulo acrescentou. Sem
// reconciliacao a biblioteca funcionava por ~20 s no arranque e voltava a ficar
// vazia quando a descoberta terminasse, que e o pior dos defeitos possiveis:
// intermitente e sem log.
void contalib_reconciliar(void);

// Alimenta o historico do catalogo (cat_historico_definir_id) com os vistos da
// conta. Devolve quantos titulos foram marcados. FIO PRINCIPAL apenas.
//
// SO LINHAS DE TITULO INTEIRO, sem season/episode. E a mesma regra que
// trakt.c ja aplica ao ler /sync/history (ele pula a linha que tem "episode"):
// o historico daqui responde "esta obra esta marcada como vista?", e um
// episodio nao responde isso por uma serie de oito temporadas.
int contalib_aplicar_vistos(void);

void contalib_esquecer(void);

#endif
