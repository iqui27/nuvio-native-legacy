// TRAILER PELO IMDb, como arquivo MP4 direto.
//
// Por que existe (20/09/2026): na LG o app e nativo e nao tem onde embutir o
// player do YouTube, e resolver o YouTube por fora nao e viavel de graca — o
// yt-dlp num IP de datacenter (Oracle) responde "Sign in to confirm you're
// not a bot", TODAS as instancias publicas de Piped e Invidious testadas
// estavam mortas ou no mesmo bloqueio, e o cobalt publico exige chave. O
// IMDb, ao contrario, serve o trailer principal de cada titulo como MP4
// (720p/1080p) e HLS numa URL assinada da CloudFront, valida por ~7 dias e
// que toca de qualquer IP — verificado do Mac e de um VPS. A pergunta vai a
// API GraphQL publica deles (api.graphql.imdb.com), que responde a um GET com
// a consulta na URL desde que va com Accept: application/json e um Referer
// do imdb.com; sem o Referer e 403. Funciona de datacenter tambem.
//
// A URL fica em memoria por titulo e em disco (dados_dir()/trailer/<imdb>),
// com o Expires da propria assinatura como validade.
#ifndef NV_TRAILERIMDB_H
#define NV_TRAILERIMDB_H

// Pede (em fio proprio) o trailer de `imdb`. Idempotente: nao repete o pedido
// de um titulo que ja tem resposta valida ou que esta em voo.
void        trailerimdb_pedir(const char *imdb);
// URL do MP4 (a melhor ate 1080p) ou NULL enquanto nao ha. `nome` recebe o
// titulo do video ("Trailer", "Official Trailer 2") quando nao e NULL.
const char *trailerimdb_url(const char *imdb, const char **nome);
// 1 quando a resposta ja veio (com ou sem trailer): quem espera para de esperar.
int         trailerimdb_respondeu(const char *imdb);

#endif
