// Links externos (docs/deeplink.md): nuvio-legacy://, stremio:// e imdb.com
// viram um pedido para o app abrir titulo, tocar ou buscar. O parser e puro
// (testado em tests/deeplink.c); postar/pegar levam o pedido do fio da
// plataforma (Intent do Android, parametros de lancamento do webOS) ao laco
// principal.
#ifndef NV_DEEPLINK_H
#define NV_DEEPLINK_H

#define DL_URL_MAX 512

typedef enum { DL_NENHUM = 0, DL_TITULO, DL_TOCAR, DL_BUSCA } DlTipo;

typedef struct {
  DlTipo tipo;
  char imdb[24];        // "tt0111161"; vazio se o link trouxe id do TMDB
  long tmdb;            // > 0 se o link trouxe id do TMDB
  int serie;            // 1 = series, 0 = movie
  int temporada, episodio;  // so em DL_TOCAR; 0 = nao pedido
  char consulta[96];    // so em DL_BUSCA (UTF-8, sem controles)
} DeepLink;

// 1 = URL valida e `out` preenchido; 0 = lixo (out zerado). Nunca escreve
// fora de `out` e le no maximo DL_URL_MAX bytes.
int  deeplink_parse(const char *url, DeepLink *out);

// Qualquer fio: valida e guarda o ultimo pedido (o mais novo vence).
void deeplink_postar(const char *url);
// Laco principal: 1 e entrega o pedido pendente, uma vez so.
int  deeplink_pegar(DeepLink *out);
#endif
