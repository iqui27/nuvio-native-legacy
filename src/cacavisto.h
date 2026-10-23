// O QUE FOI VISTO, para a Caca e as conquistas — o ADAPTADOR.
//
// O plano manda as conquistas lerem o DIARIO (diario.h, frente feat/sp-diario).
// O Diario ainda nao existe nesta arvore, entao este modulo responde com o que
// o app ja sabe HOJE, nesta ordem:
//   1. o historico do catalogo (cat_historico_estado_id): Trakt
//      /sync/watched/movies, os itens assistidos da conta Nuvio e o gesto
//      "marcar como assistido" desta TV, ja fundidos por catalogo.c;
//   2. o progresso local (progresso.c): filme com >= 90% conta como visto, e e
//      a unica fonte de hoje que diz QUANDO (lastWatchedMs).
//
// TROCAR PELO DIARIO E MEXER SO AQUI: cacavisto_filme passa a perguntar ao
// diario (primeira vez, sem revisoes) e o resto da Caca nao muda. A data so
// importa para "Em ordem"; sem ela o degrau nao sai (conquista.h).
#ifndef NV_CACAVISTO_H
#define NV_CACAVISTO_H

// 1 visto, 0 nao (ou nao se sabe). `quando`, se nao NULL, recebe o epoch da
// primeira vez quando a fonte sabe; 0 quando nao sabe. `imdb` "tt..."; `tmdb`
// e a reserva para quando o Diario guardar o id do TMDB (hoje ignorado).
int cacavisto_filme(const char *imdb, long tmdb, long long *quando);

// Um numero que muda quando o que foi visto PODE ter mudado (geracao do
// historico do catalogo). A Caca recalcula quando ele anda.
unsigned long long cacavisto_geracao(void);

#endif
