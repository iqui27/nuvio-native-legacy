// TRAKT SCROBBLE (#179): a decisao de QUAL chamada mandar e o corpo dela, sem
// rede nem estado global — para o teste poder cobrir sem SDL.
//
// O Trakt so mostra "Now Watching" com /scrobble/start; /scrobble/pause guarda
// o ponto e /scrobble/stop com >= 90% (limiar deste cliente) fecha o historico.
// Ate a 1.5.3 o app so mandava pause/stop, uma vez, ao SAIR do player.
#ifndef NV_TRAKTSCROBBLE_H
#define NV_TRAKTSCROBBLE_H
#include <stddef.h>

enum { SCR_NADA = 0, SCR_START, SCR_PAUSE, SCR_STOP };
enum { SCR_EV_TOCANDO = 1, SCR_EV_PAUSOU, SCR_EV_SAIU, SCR_EV_PONTO };

typedef struct { char id[64]; int ativo; } ScrobbleEstado;

// Devolve a chamada a fazer (SCR_*) e avanca o estado.
//   TOCANDO: start, salvo se ja ha um start em pe para o MESMO id (sem spam).
//   PAUSOU : pause, so se ha start em pe para o id (pausar de novo e ruido).
//   SAIU   : sempre fala: stop a partir de SCR_STOP_PCT, senao pause.
int scrobble_decidir(ScrobbleEstado *s, int evento, const char *id, double pct);

// PARIDADE COM O NUVIOTV OFICIAL (PlayerRuntimeController*.kt, WatchProgress.kt):
//   - sair do player manda STOP; a partir de 80% e o stop de conclusao, que o
//     Trakt e o Simkl contam como assistido (emitStopScrobbleForCurrentProgress);
//   - o progresso LOCAL vira "visto" a partir de 90% (COMPLETED_THRESHOLD), pela
//     POSICAO — inclusive depois de um seek, sem exigir tempo tocado.
// Aqui o "fim" de Legacy (marcador de creditos / cartao A seguir) tambem conclui.
#define SCR_STOP_PCT 80.0
#define SCR_VISTO_PCT 90.0
int scrobble_concluiu(double posSeg, double durSeg, int fimDeLegacy);

// JSON do corpo. `id` e "tt123" (filme) ou "tt123:T:E" (episodio: id do SHOW
// mais temporada/numero, que o Trakt resolve sozinho). `ep` opcional traz ids
// do episodio quando o meta do addon ja os conhece (TVDB/TMDB/Trakt).
typedef struct {
  const char *tvdb;
  const char *tmdb;
  const char *trakt;
  char proprio[3][24];
} ScrobbleEpIds;
// Copies episode ids into proprio[] so the pointers outlive the meta parse.
void scrobble_ids_guardar(ScrobbleEpIds *out, const char *tvdb,
                          const char *tmdb, const char *trakt);
void scrobble_ids_limpar(ScrobbleEpIds *out);
void scrobble_corpo(char *dst, size_t n, const char *id, double pct);
void scrobble_corpo_ep(char *dst, size_t n, const char *id, double pct,
                       const ScrobbleEpIds *ep);
const char *scrobble_nome(int acao);
#endif
