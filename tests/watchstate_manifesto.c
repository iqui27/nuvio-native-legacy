// watch_state v2, passo 1: o manifesto do addon e o filtro de eventos.
// Spec: .mercado/aiostreams-watch-state.md. NAO precisa de rede.
//
//   bash tests/watchstate_manifesto.sh
#include <stdio.h>
#include <string.h>
#include "watchstate.h"

static int falhas;
#define OK(c) do { if (!(c)) { printf("FALHOU %d: %s\n", __LINE__, #c); falhas++; } } while (0)

static const char *SPEC =
  "{\"id\":\"com.example.tracker\",\"version\":\"1.2.0\",\"name\":\"T\","
  "\"types\":[\"movie\",\"series\"],"
  "\"resources\":[\"meta\",{\"name\":\"watch_state\",\"types\":[\"movie\",\"series\"],"
  "\"idPrefixes\":[\"tt\",\"tmdb:\",\"kitsu:\"]}],"
  "\"watchState\":{\"version\":2,\"push\":{\"events\":[\"stop\",\"played\",\"unplayed\","
  "\"watchlisted\",\"rated\"],\"bulk\":true},\"pull\":{\"items\":true,\"ttlSeconds\":300}}}";

int main(void) {
  WsDecl d;
  // manifesto do spec: so stop/played/unplayed (nomes desconhecidos ignorados)
  ws_decl_ler(SPEC, &d);
  OK(d.push);
  OK(d.eventos == (WS_STOP | WS_PLAYED | WS_UNPLAYED));
  OK(ws_decl_aceita(&d, WS_STOP, "series", "tt0903747", "tt0903747:3:7"));
  OK(!ws_decl_aceita(&d, WS_START, "series", "tt0903747", "tt0903747:3:7"));
  OK(!ws_decl_aceita(&d, WS_PAUSE, "movie", "tt1", "tt1"));
  // tipo fora de types
  OK(!ws_decl_aceita(&d, WS_STOP, "tv", "tt1", "tt1"));
  OK(!ws_decl_aceita(&d, WS_STOP, "", "tt1", "tt1"));
  // prefixo casa com o meta OU com o video; nenhum casa = fora
  OK(ws_decl_aceita(&d, WS_STOP, "series", "tt13293588", "kitsu:42323:7"));
  OK(ws_decl_aceita(&d, WS_STOP, "series", "mal:1", "kitsu:42323:7"));
  OK(!ws_decl_aceita(&d, WS_STOP, "series", "mal:1", "anidb:2:1"));

  // events omitido = todo evento de reproducao
  ws_decl_ler("{\"resources\":[{\"name\":\"watch_state\",\"types\":[\"movie\"]}],"
              "\"watchState\":{\"version\":2,\"push\":{}}}", &d);
  OK(d.push && d.eventos == WS_TODOS);
  OK(ws_decl_aceita(&d, WS_PAUSE, "movie", "tt1", "tt1"));   // sem idPrefixes = qualquer id
  OK(!ws_decl_aceita(&d, WS_PAUSE, "series", "tt1", "tt1:1:1"));

  // tipos/prefixos da raiz quando o resource nao traz; forma curta do resource
  ws_decl_ler("{\"types\":[\"series\"],\"idPrefixes\":[\"kitsu:\"],"
              "\"resources\":[\"catalog\",\"watch_state\"],"
              "\"watchState\":{\"version\":2,\"push\":{\"events\":[\"start\"]}}}", &d);
  OK(d.push && d.eventos == WS_START);
  OK(ws_decl_aceita(&d, WS_START, "series", "kitsu:1", "kitsu:1:2"));
  OK(!ws_decl_aceita(&d, WS_START, "series", "tt1", "tt1:1:1"));

  // nao participa: sem resource, v1 (playback), versao 1, sem push, so pull
  ws_decl_ler("{\"resources\":[\"meta\"],\"watchState\":{\"version\":2,\"push\":{}}}", &d);
  OK(!d.push);
  ws_decl_ler("{\"resources\":[{\"name\":\"playback\"}],\"playback\":{\"events\":[\"stop\"]}}", &d);
  OK(!d.push);
  ws_decl_ler("{\"resources\":[\"watch_state\"],\"watchState\":{\"version\":1,\"push\":{}}}", &d);
  OK(!d.push);
  ws_decl_ler("{\"resources\":[\"watch_state\"],\"watchState\":{\"version\":2,\"pull\":{\"items\":true}}}", &d);
  OK(!d.push);
  ws_decl_ler("{\"resources\":[\"watch_state\"]}", &d);
  OK(!d.push);
  // so eventos que nao sao de reproducao: nada a receber
  ws_decl_ler("{\"resources\":[\"watch_state\"],\"watchState\":{\"version\":2,"
              "\"push\":{\"events\":[\"rated\",\"dropped\"]}}}", &d);
  OK(!d.push);
  // viewers:true: sem vinculo de pessoa (W8) nao enviamos
  ws_decl_ler("{\"resources\":[\"watch_state\"],\"watchState\":{\"version\":2,\"viewers\":true,\"push\":{}}}", &d);
  OK(d.viewers && !ws_decl_aceita(&d, WS_STOP, "movie", "tt1", "tt1"));
  // "watch_state" so como valor de outra chave nao conta
  ws_decl_ler("{\"name\":\"watch_state\",\"resources\":[\"meta\"],"
              "\"watchState\":{\"version\":2,\"push\":{}}}", &d);
  OK(!d.push);
  ws_decl_ler(NULL, &d);
  OK(!d.push);

  if (falhas) return 1;
  printf("watchstate_manifesto: ok\n");
  return 0;
}
